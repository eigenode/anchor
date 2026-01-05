# Anchor DB

## Overview

Anchor is a **single-node relational database designed for governance, privacy, and modern compliance needs**.  

> ⚠️ **Note:** Single-node mode is temporary — for this first proof-of-concept (POC). The architecture is designed to eventually scale to multi-node distributed operation.  

Unlike traditional databases, Anchor focuses on **privacy by design, versioned rows, and flexible access control**, making it ideal for products where **data protection, auditability, and governance** are critical.

### Key Product Features

- **Governance & compliance:** every row is versioned, allowing audit trails, time-travel queries, and retention policies.
- **Privacy by design:** built-in column masking and role-based access ensures sensitive information is never exposed accidentally.
- **Time-travel & audit logs:** query your data at any previous version for auditing or debugging.
- **TTL & retention policies:** automatically hide or expire old data to support compliance.
- **Efficient memory usage:** column projections reduce memory footprint for large tables.
- **Future-proof storage:** current in-memory LSM structure can flush to disk with SSTables for scalability.

---

## Developer Guide

### Compiling

Compile Anchor with C99:

```bash
gcc -std=c99 -O2 -o anchor anchor.c
```

### Running the CLI

Run the interactive demo:

```bash
./anchor
```

Prompt:

```
Anchor DB – LSM Demo v2
anchor>
```

---

## Demo

This demo illustrates **users, roles, masking, inserts, deletes, flush, and time-travel queries**.

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
Immutable memtables: 1
anchor> FLUSH ALL
→ Memtable rotated (immutables=2)
→ Flushed SSTable sst_users_0.dat
→ Flushed SSTable sst_users_1.dat
anchor> SHOW MEMTABLES
Active memtable buckets: 1
Immutable memtables: 2
anchor> COMPACT
→ Compacting SSTables (placeholder for LSM merge)
```

### Notes

- **Deletes:** tombstones hide historical rows; current SELECT returns nothing after deletion.
- **Time-travel queries:** `ASOF <version>` returns state of data before deletes/updates.
- **Flush:** writes SSTables to disk but keeps immutables in memory for demo purposes.
- **Masking:** non-admin users never see sensitive data.
- **Single-node:** this is a POC mode. Multi-node scaling is planned for future development.

