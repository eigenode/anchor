#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* ================= CONFIG ================= */

#define MAX_USERS 16
#define MAX_ROLES 8
#define MAX_TABLES 8
#define MAX_COLUMNS 8
#define MAX_ROWS 128
#define MAX_VERSIONS 8
#define MAX_POLICIES 8

/* ================= USERS ================= */

typedef struct {
    char name[32];
} role_t;

typedef struct {
    char name[32];
    role_t roles[MAX_ROLES];
    size_t role_count;
} user_t;

user_t users[MAX_USERS];
size_t user_count = 0;
user_t *current_user = NULL;

/* ================= SCHEMA ================= */

typedef enum {
    CLASS_PUBLIC,
    CLASS_PII
} classification_t;

typedef struct {
    char name[32];
    classification_t classification;
} column_t;

typedef struct {
    column_t columns[MAX_COLUMNS];
    size_t column_count;
} schema_t;

/* ================= POLICY ================= */

typedef struct {
    int ttl_seconds;
} table_policy_t;

typedef struct {
    uint64_t version;
    time_t created_at;
    char created_by[32];
    table_policy_t table_policy;
} policy_version_t;

typedef struct {
    policy_version_t versions[MAX_POLICIES];
    size_t count;
    uint64_t active;
} policy_chain_t;

/* ================= STORAGE ================= */

typedef struct {
    uint64_t version;
    time_t ts;
    char owner[32];
    char values[MAX_COLUMNS][64];
} row_version_t;

typedef struct {
    row_version_t versions[MAX_VERSIONS];
    size_t count;
} row_t;

typedef struct {
    char name[32];
    schema_t schema;
    policy_chain_t policies;
    row_t rows[MAX_ROWS];
    size_t row_count;
} table_t;

table_t tables[MAX_TABLES];
size_t table_count = 0;

uint64_t global_version = 1;

/* ================= EXECUTION ================= */

typedef struct {
    uint64_t version;
    time_t ts;
    char columns[MAX_COLUMNS][64];
    size_t column_count;
} logical_row_t;

typedef struct {
    user_t *user;
    policy_version_t *policy;
    time_t now;
} exec_ctx_t;

typedef void (*row_consumer_fn)(logical_row_t *, void *);

/* ================= UTILS ================= */

void trim(char *s) {
    s[strcspn(s, "\n")] = 0;
}

int has_role(const char *r) {
    if (!current_user) return 0;
    for (size_t i = 0; i < current_user->role_count; i++)
        if (strcmp(current_user->roles[i].name, r) == 0)
            return 1;
    return 0;
}

table_t *find_table(const char *name) {
    for (size_t i = 0; i < table_count; i++)
        if (strcmp(tables[i].name, name) == 0)
            return &tables[i];
    return NULL;
}

policy_version_t *active_policy(table_t *t) {
    if (t->policies.count == 0) return NULL;
    return &t->policies.versions[t->policies.count - 1];
}

/* ================= EXECUTION CORE ================= */

int materialize_row(
    exec_ctx_t *ctx,
    table_t *table,
    row_version_t *rv,
    size_t *proj,
    size_t proj_count,
    logical_row_t *out
) {
    if (ctx->policy &&
        ctx->policy->table_policy.ttl_seconds &&
        ctx->now - rv->ts > ctx->policy->table_policy.ttl_seconds)
        return 0;

    out->version = rv->version;
    out->ts = rv->ts;
    out->column_count = proj_count;

    for (size_t i = 0; i < proj_count; i++) {
        size_t c = proj[i];
        column_t *col = &table->schema.columns[c];

        if (col->classification == CLASS_PII &&
            has_role("support")) {
            strcpy(out->columns[i], "****");
        } else {
            strcpy(out->columns[i], rv->values[c]);
        }
    }
    return 1;
}

void exec_select(
    table_t *table,
    exec_ctx_t *ctx,
    size_t *proj,
    size_t proj_count,
    row_consumer_fn consume,
    void *arg
) {
    logical_row_t row;

    for (size_t i = 0; i < table->row_count; i++) {
        row_t *r = &table->rows[i];
        for (int v = (int)r->count - 1; v >= 0; v--) {
            if (materialize_row(
                    ctx, table, &r->versions[v],
                    proj, proj_count, &row)) {
                consume(&row, arg);
                break;
            }
        }
    }
}

/* ================= COMMANDS ================= */

void create_user(const char *name) {
    user_t *u = &users[user_count++];
    memset(u, 0, sizeof(*u));
    strcpy(u->name, name);
    printf("User created: %s\n", name);
}

void login(const char *name) {
    for (size_t i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, name) == 0) {
            current_user = &users[i];
            printf("Logged in as %s\n", name);
            return;
        }
    }
    printf("User not found\n");
}

void grant_role(const char *user, const char *role) {
    for (size_t i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, user) == 0) {
            strcpy(users[i].roles[users[i].role_count++].name, role);
            printf("Granted role %s to %s\n", role, user);
            return;
        }
    }
}

void create_table(const char *name) {
    table_t *t = &tables[table_count++];
    memset(t, 0, sizeof(*t));
    strcpy(t->name, name);
    printf("Table created: %s\n", name);
}

void add_column(const char *table, const char *col) {
    table_t *t = find_table(table);
    if (!t) return;

    column_t *c = &t->schema.columns[t->schema.column_count++];
    strcpy(c->name, col);
    c->classification = CLASS_PII;
    printf("Added column %s\n", col);
}

policy_version_t *new_policy(table_t *t) {
    policy_version_t *p = &t->policies.versions[t->policies.count++];
    memset(p, 0, sizeof(*p));
    p->version = global_version++;
    p->created_at = time(NULL);
    strcpy(p->created_by, current_user->name);
    t->policies.active = p->version;
    return p;
}

void set_ttl(const char *table, int ttl) {
    table_t *t = find_table(table);
    if (!t) return;
    policy_version_t *p = new_policy(t);
    p->table_policy.ttl_seconds = ttl;
    printf("TTL set to %d seconds\n", ttl);
}

void insert_row(const char *table, char vals[][64]) {
    table_t *t = find_table(table);
    if (!t) return;

    row_t *r = &t->rows[t->row_count++];
    row_version_t *v = &r->versions[r->count++];
    v->version = global_version++;
    v->ts = time(NULL);
    strcpy(v->owner, current_user->name);

    for (size_t i = 0; i < t->schema.column_count; i++)
        strcpy(v->values[i], vals[i]);

    printf("Inserted row v%llu\n", v->version);
}

/* ================= OUTPUT ================= */

void print_row(logical_row_t *r, void *arg) {
    (void)arg;
    for (size_t i = 0; i < r->column_count; i++)
        printf("%s ", r->columns[i]);
    printf("\n");
}

/* ================= CLI ================= */

void repl() {
    char line[256];

    while (1) {
        printf("anchor> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        trim(line);

        char *tok = strtok(line, " ");
        if (!tok) continue;

        if (!strcmp(tok, "CREATE")) {
            tok = strtok(NULL, " ");
            if (!strcmp(tok, "USER"))
                create_user(strtok(NULL, " "));
            else if (!strcmp(tok, "TABLE"))
                create_table(strtok(NULL, " "));
        }
        else if (!strcmp(tok, "LOGIN"))
            login(strtok(NULL, " "));
        else if (!strcmp(tok, "GRANT"))
            grant_role(strtok(NULL, " "), strtok(NULL, " "));
        else if (!strcmp(tok, "ADD"))
            add_column(strtok(NULL, " "), strtok(NULL, " "));
        else if (!strcmp(tok, "SET"))
            set_ttl(strtok(NULL, " "), atoi(strtok(NULL, " ")));
        else if (!strcmp(tok, "INSERT")) {
            char *table = strtok(NULL, " ");
            table_t *t = find_table(table);
            if (!t) continue;

            char vals[MAX_COLUMNS][64] = {0};
            for (size_t i = 0; i < t->schema.column_count; i++) {
                char *v = strtok(NULL, " ");
                if (!v) break;
                strcpy(vals[i], v);
            }
            insert_row(table, vals);
        }
        else if (!strcmp(tok, "SELECT")) {
            table_t *t = find_table(strtok(NULL, " "));
            if (!t) continue;

            exec_ctx_t ctx = {
                .user = current_user,
                .policy = active_policy(t),
                .now = time(NULL)
            };

            size_t proj[MAX_COLUMNS];
            for (size_t i = 0; i < t->schema.column_count; i++)
                proj[i] = i;

            exec_select(t, &ctx, proj, t->schema.column_count,
                        print_row, NULL);
        }
        else if (!strcmp(tok, "QUIT"))
            break;
    }
}

int main() {
    printf("Anchor DB (clean execution layer)\n");
    repl();
    return 0;
}

