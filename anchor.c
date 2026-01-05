/* =========================
 * Anchor DB – LSM Vertical Slice v2
 * ========================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ===================== CONFIG ===================== */

#define MAX_TABLES     8
#define MAX_COLUMNS    8
#define MAX_VALUE      64
#define MAX_USERS      8
#define MAX_ROLES      4
#define MAX_ROWS       5
#define MAX_IMMUTABLE  8

/* ===================== USERS ===================== */

typedef struct {
    char name[32];
    char roles[MAX_ROLES][32];
    size_t role_count;
} user_t;

static user_t users[MAX_USERS];
static size_t user_count;
static user_t *current_user;

/* ===================== TABLE ===================== */

typedef struct {
    char name[32];
    char columns[MAX_COLUMNS][32];
    size_t column_count;
    time_t ttl;
} table_t;

static table_t tables[MAX_TABLES];
static size_t table_count;

/* ===================== ROW ===================== */

typedef struct {
    uint64_t version;
    time_t ts;
    bool tombstone;
    char values[MAX_COLUMNS][MAX_VALUE];
} row_t;

/* ===================== MEMTABLE ===================== */

typedef struct {
    char table[32];
    row_t rows[MAX_ROWS];
    size_t count;
} bucket_t;

typedef struct {
    bucket_t buckets[MAX_TABLES];
    size_t bucket_count;
} memtable_t;

/* ===================== LSM STATE ===================== */

static memtable_t active;
static memtable_t immutables[MAX_IMMUTABLE];
static size_t immutable_count;
static uint64_t global_version;
static uint64_t sstable_gen;

/* ===================== HELPERS ===================== */

static table_t *find_table(const char *name) {
    for (size_t i = 0; i < table_count; i++)
        if (!strcmp(tables[i].name, name))
            return &tables[i];
    return NULL;
}

static bucket_t *find_bucket(memtable_t *mt, const char *table) {
    for (size_t i = 0; i < mt->bucket_count; i++)
        if (!strcmp(mt->buckets[i].table, table))
            return &mt->buckets[i];
    return NULL;
}

static bool has_role(const char *r) {
    if (!current_user) return false;
    for (size_t i = 0; i < current_user->role_count; i++)
        if (!strcmp(current_user->roles[i], r))
            return true;
    return false;
}

static void memtable_init(memtable_t *mt) {
    memset(mt, 0, sizeof(*mt));
}

/* ===================== ROTATION ===================== */

static void rotate_memtable(void) {
    if (immutable_count >= MAX_IMMUTABLE) {
        printf("⚠ Immutable memtables full! Flush required.\n");
        return;
    }
    immutables[immutable_count++] = active;
    memtable_init(&active);
    printf("→ Memtable rotated (immutables=%zu)\n", immutable_count);
}

/* ===================== SSTABLE ===================== */

static void flush_immutable(memtable_t *mt) {
    for (size_t b = 0; b < mt->bucket_count; b++) {
        bucket_t *bk = &mt->buckets[b];
        char fn[64];
        snprintf(fn, sizeof(fn), "sst_%s_%llu.dat",
                 bk->table, (unsigned long long)sstable_gen++);
        FILE *f = fopen(fn, "w");
        if (!f) continue;

        for (size_t i = 0; i < bk->count; i++) {
            row_t *r = &bk->rows[i];
            fprintf(f, "%llu %ld %d",
                (unsigned long long)r->version,
                r->ts,
                r->tombstone ? 1 : 0);
            for (size_t c = 0; c < MAX_COLUMNS; c++)
                fprintf(f, " %s", r->values[c]);
            fprintf(f, "\n");
        }
        fclose(f);
        printf("→ Flushed SSTable %s\n", fn);
    }
}

static void flush_all_immutables(void) {
    while (immutable_count) {
        flush_immutable(&immutables[0]);
        memmove(&immutables[0], &immutables[1],
                sizeof(memtable_t) * --immutable_count);
    }
}

/* ===================== INSERT / DELETE ===================== */

static void insert_row(const char *table, char vals[][MAX_VALUE], bool tomb) {
    table_t *t = find_table(table);
    if (!t || !current_user) return;

    bucket_t *b = find_bucket(&active, table);
    if (!b) {
        if (active.bucket_count >= MAX_TABLES) {
            rotate_memtable();
            b = &active.buckets[active.bucket_count++];
        } else {
            b = &active.buckets[active.bucket_count++];
        }
        memset(b, 0, sizeof(*b));
        strcpy(b->table, table);
    }

    if (b->count >= MAX_ROWS) {
        rotate_memtable();
        b = &active.buckets[active.bucket_count++];
        memset(b, 0, sizeof(*b));
        strcpy(b->table, table);
    }

    row_t *r = &b->rows[b->count++];
    r->version = ++global_version;
    r->ts = time(NULL);
    r->tombstone = tomb;

    for (size_t i = 0; i < t->column_count; i++)
        strcpy(r->values[i], vals ? vals[i] : "");
}

/* ===================== READ ===================== */

typedef struct {
    table_t *table;
    uint64_t asof;
} read_ctx_t;

static void emit_row(row_t *r, read_ctx_t *ctx) {
    if (r->version > ctx->asof) return;
    if (r->tombstone) return;

    bool mask = !has_role("admin");
    for (size_t i = 0; i < ctx->table->column_count; i++)
        printf("%s=%s ",
            ctx->table->columns[i],
            mask ? "****" : r->values[i]);

    printf("(v%llu)\n", (unsigned long long)r->version);
}

static void select_table(const char *name, uint64_t asof) {
    table_t *t = find_table(name);
    if (!t) return;

    read_ctx_t ctx = { t, asof ? asof : UINT64_MAX };

    for (size_t i = 0; i < active.bucket_count; i++)
        if (!strcmp(active.buckets[i].table, name))
            for (size_t r = 0; r < active.buckets[i].count; r++)
                emit_row(&active.buckets[i].rows[r], &ctx);

    for (ssize_t m = immutable_count - 1; m >= 0; m--)
        for (size_t b = 0; b < immutables[m].bucket_count; b++)
            if (!strcmp(immutables[m].buckets[b].table, name))
                for (size_t r = 0; r < immutables[m].buckets[b].count; r++)
                    emit_row(&immutables[m].buckets[b].rows[r], &ctx);
}

/* ===================== COMPACTION ===================== */

static void compact_sstables(void) {
    printf("→ Compacting SSTables (placeholder for LSM merge)\n");
}

/* ===================== DEBUG ===================== */

static void show_memtables(void) {
    printf("Active memtable buckets: %zu\n", active.bucket_count);
    printf("Immutable memtables: %zu\n", immutable_count);
}

/* ===================== CLI ===================== */

static void repl(void) {
    char cmd[256];
    printf("Anchor DB – LSM Demo v2\n");

    while (1) {
        printf("anchor> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;

        if (!strncmp(cmd, "CREATE USER", 11)) {
            sscanf(cmd, "CREATE USER %31s", users[user_count].name);
            printf("User created: %s\n", users[user_count].name);
            user_count++;
        }
        else if (!strncmp(cmd, "LOGIN", 5)) {
            char u[32]; sscanf(cmd, "LOGIN %31s", u);
            for (size_t i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, u)) current_user = &users[i];
            if (current_user) printf("Logged in as %s\n", current_user->name);
        }
        else if (!strncmp(cmd, "GRANT", 5)) {
            char u[32], r[32]; sscanf(cmd, "GRANT %31s %31s", u, r);
            for (size_t i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, u))
                    strcpy(users[i].roles[users[i].role_count++], r);
            printf("Granted role %s to %s\n", r, u);
        }
        else if (!strncmp(cmd, "CREATE TABLE", 12)) {
            sscanf(cmd, "CREATE TABLE %31s", tables[table_count].name);
            printf("Table created: %s\n", tables[table_count].name);
            table_count++;
        }
        else if (!strncmp(cmd, "ADD", 3)) {
            char t[32], c[32];
            sscanf(cmd, "ADD %31s %31s", t, c);
            table_t *tb = find_table(t);
            strcpy(tb->columns[tb->column_count++], c);
            printf("Added column %s to %s\n", c, t);
        }
        else if (!strncmp(cmd, "INSERT", 6)) {
            char t[32], v1[64], v2[64];
            sscanf(cmd, "INSERT %31s %63s %63s", t, v1, v2);
            char vals[2][MAX_VALUE] = {0};
            strcpy(vals[0], v1); strcpy(vals[1], v2);
            insert_row(t, vals, false);
            printf("Inserted row v%llu\n", (unsigned long long)global_version);
        }
        else if (!strncmp(cmd, "DELETE", 6)) {
            char t[32]; sscanf(cmd, "DELETE %31s", t);
            insert_row(t, NULL, true);
            printf("Deleted row in %s (tombstone)\n", t);
        }
        else if (!strncmp(cmd, "SELECT", 6)) {
            char t[32]; uint64_t v = 0;
            sscanf(cmd, "SELECT %31s ASOF %llu", t, &v);
            select_table(t, v);
        }
        else if (!strncmp(cmd, "FLUSH ALL", 9))
            flush_all_immutables();
        else if (!strncmp(cmd, "COMPACT", 7))
            compact_sstables();
        else if (!strncmp(cmd, "SHOW MEMTABLES", 13))
            show_memtables();
        else if (!strncmp(cmd, "EXIT", 4))
            break;
    }
}

/* ===================== MAIN ===================== */

int main(void) {
    memtable_init(&active);
    repl();
    return 0;
}

