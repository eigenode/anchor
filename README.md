# Anchor DB

## Overview

Anchor is a single-node relational database designed for governance, privacy, and modern compliance needs.

> Note: Single-node mode is temporary. This first proof-of-concept (POC) runs entirely in memory. The architecture is designed to eventually scale to multi-node distributed operation.

Anchor focuses on privacy by design, versioned rows, and flexible access control, making it ideal for products where data protection, auditability, and governance are critical.

### Key Product Features

- Governance and compliance: every row is versioned, allowing audit trails, time-travel queries, and retention policies.
- Privacy by design: built-in column masking and role-based access ensures sensitive information is never exposed accidentally.
- Time-travel and audit logs: query your data at any previous version for auditing or debugging.
- TTL and retention policies: automatically hide or expire old data to support compliance.
- Efficient memory usage: column projections reduce memory footprint for large tables.
- Future-proof storage: current in-memory LSM structure can flush to disk with SSTables for scalability.

---

## Developer Guide

### Compiling

Compile Anchor with C99:

```bash
gcc -std=c99 -O2 -o anchor anchor.c
```

Alternatively, use the included Makefile:

```bash
make
```

This will generate the `anchor` executable.

### Running the CLI

Run the interactive demo:

```bash
./anchor
```

Prompt:

```
Anchor DB – LSM Demo v3
anchor>
```

---

## Demo

This demo illustrates users, roles, masking, inserts, deletes, flush, and time-travel queries.

```text
anchor> CREATE USER alice
User created: alice
anchor> CREATE USER bob
User created: bob
anchor> GRANT alice admin
Granted role admin to alice
anchor> LOGIN bob
Logged in as bob
anchor> GRANT bob support
Granted role support to bob
anchor> CREATE TABLE users
Table created: users
anchor> ADD users email
Added column email to users
anchor> ADD users name
Added column name to users
anchor> LOGIN alice
Logged in as alice
anchor> INSERT users alice@mail.com Alice
Inserted row v1
anchor> INSERT users bob@mail.com Bob
Inserted row v2
anchor> SELECT users
email=alice@mail.com name=Alice (v1)
email=bob@mail.com name=Bob (v2)
anchor> LOGIN bob
Logged in as bob
anchor> SELECT users
email=**** name=**** (v1)
email=**** name=**** (v2)
anchor> LOGIN alice
Logged in as alice
anchor> DELETE users
Deleted row in users (tombstone)
anchor> SELECT users
# no rows returned because table deleted
anchor> SELECT users ASOF 1
email=alice@mail.com name=Alice (v1)
anchor> SELECT users ASOF 2
email=alice@mail.com name=Alice (v1)
email=bob@mail.com name=Bob (v2)
anchor> SHOW MEMTABLES
Active memtable buckets: 1
Immutable memtables: 0
anchor> FLUSH ALL
→ Memtable rotated (immutables=1)
→ Flushed SSTable sst_users_0.dat
anchor> SHOW MEMTABLES
Active memtable buckets: 1
Immutable memtables: 1
anchor> COMPACT
→ Compacting SSTables (placeholder for LSM merge)
```

---

## CLI Cheat Sheet

| Command | Description | Example / Expected Output |
|---------|-------------|--------------------------|
| `CREATE USER <name>` | Create a new user | `CREATE USER alice` → `User created: alice` |
| `LOGIN <name>` | Switch current session to a user | `LOGIN alice` → `Logged in as alice` |
| `GRANT <user> <role>` | Assign role to user | `GRANT alice admin` → `Granted role admin to alice` |
| `CREATE TABLE <name>` | Create a new table | `CREATE TABLE users` → `Table created: users` |
| `ADD <table> <column>` | Add column to table | `ADD users email` → `Added column email to users` |
| `INSERT <table> <val1> <val2> ...` | Insert a row | `INSERT users alice@mail.com Alice` → `Inserted row v1` |
| `DELETE <table>` | Mark all rows in table with tombstone | `DELETE users` → `Deleted row in users (tombstone)` |
| `SELECT <table>` | Read current table state | After delete → no rows returned |
| `SELECT <table> ASOF <version>` | Read table state at specific version | `SELECT users ASOF 1` → `email=alice@mail.com name=Alice (v1)` |
| `FLUSH ALL` | Flush active and immutable memtables to SSTables | `FLUSH ALL` → `→ Memtable rotated (immutables=N)` + `→ Flushed SSTable sst_users_X.dat` |
| `SHOW MEMTABLES` | Display active and immutable memtable counts | `Active memtable buckets: 1`<br>`Immutable memtables: 1` |
| `COMPACT` | Placeholder for future LSM merge | `→ Compacting SSTables (placeholder for LSM merge)` |
| `EXIT` | Exit CLI | — |

---

## Notes on Behavior

- Versioning: Each insert or delete increases `vN`.
- Deletes / Tombstones: Hide rows for all `SELECT` queries after deletion; historical `ASOF` queries still show pre-delete rows.
- Masking: Non-admin users see `****` for all columns.
- Flush: Writes SSTables but preserves an active memtable to allow new inserts.
- ASOF Queries: Always return the table state at the requested version, even across deletes.

---

## Future Roadmap

Anchor is designed to grow from a POC to a production-grade database. Planned enhancements:

1. Full LSM Storage Engine
   - Merge multiple immutable memtables into disk-backed SSTables.
   - Implement tombstone propagation across flushes for consistent deletes.
   - Column projections to reduce memory and disk usage.
   - Background compaction threads to merge SSTables automatically.

2. SSTable Read API
   - Queries automatically read from disk if data is not in-memory.
   - Support ASOF queries across flushed data.
   - Efficient caching of frequently accessed tables.

3. Extended Access Control and Policies
   - Per-column masking and TTL.
   - Multi-scope and group-aware roles.
   - Configurable policy DSL for governance rules.

4. Multi-Node and Distributed Scaling
   - Eventually shard tables across nodes.
   - Use consensus/replication for durability.
   - Provide Spanner-like consistency guarantees.

5. Product-Oriented Enhancements
   - Logging and audit dashboards for compliance.
   - Metrics on row versions, TTL expirations, and role-based access.
   - Integration hooks for governance and privacy tools.

Anchor evolves from a developer-friendly POC to a fully-featured governance-oriented relational database.

