# Anchor DB – Single-Node LSM Database

## Overview

**Anchor DB** is a lightweight, single-node, relational database built with governance and privacy by design. It supports:

- Row versioning (time travel, audit logs)
- TTL / retention policies
- Role-based access and masking
- Column projection for memory efficiency
- Memtable → immutable memtable → SSTable flush
- Planned LSM-based storage engine

This version is a **single-file, interactive CLI demo** showing core LSM functionality and row-level access control.

---

## Features

1. **Users & Roles**
   - Create users
   - Assign multiple roles
   - Role-based masking (admin vs non-admin)

2. **Tables & Columns**
   - Create tables and add columns
   - Set TTL for rows (seconds)
   - Column-projected storage

3. **Memtable**
   - Active memtable receives writes
   - Immutable memtables for rotation
   - Automatic rotation when full

4. **LSM / SSTables**
   - Flush immutable memtables to disk as SSTables
   - Tombstone support for deletes
   - Placeholder `COMPACT` command for future LSM merge

5. **CLI Commands**
   - `CREATE USER`, `LOGIN`, `GRANT`
   - `CREATE TABLE`, `ADD`, `INSERT`, `DELETE`
   - `SELECT <table> ASOF <version>` for time-travel queries
   - `FLUSH ALL`, `COMPACT`, `SHOW MEMTABLES`, `EXIT`

---

## Demo

Below is a step-by-step CLI demo illustrating all features.

### **User & Role Management**

```text
anchor> CREATE USER alice
User created: alice
anchor> CREATE USER bob
User created: bob
anchor> LOGIN alice
Logged in as alice
anchor> GRANT alice admin
Granted role admin to alice
anchor> LOGIN bob
Logged in as bob
anchor> GRANT bob support
Granted role support to bob
```

**Explanation:**
- Creates two users: `alice` and `bob`
- Assigns roles `admin` and `support`
- Role determines visibility of masked columns

---

### **Table & Column Management**

```text
anchor> CREATE TABLE users
Table created: users
anchor> ADD users email
Added column email to users
anchor> ADD users name
Added column name to users
```

**Explanation:**
- Creates `users` table
- Adds `email` and `name` columns

---

### **Insert Rows & TTL**

```text
anchor> LOGIN alice
Logged in as alice
anchor> INSERT users alice@mail.com Alice
Inserted row v1
anchor> INSERT users bob@mail.com Bob
Inserted row v2
```

**Explanation:**
- Inserts two rows
- Version numbers increment globally (`v1`, `v2`)
- TTL can be set per table (future enhancement)

---

### **Select & Masking**

```text
anchor> SELECT users
email=alice@mail.com name=Alice (v1)
email=bob@mail.com name=Bob (v2)
anchor> LOGIN bob
Logged in as bob
anchor> SELECT users
email=**** name=**** (v1)
email=**** name=**** (v2)
```

**Explanation:**
- Admins see full values
- Non-admins see masked columns

---

### **Delete & Tombstones**

```text
anchor> LOGIN alice
Logged in as alice
anchor> DELETE users
Deleted row in users (tombstone)
anchor> SELECT users
email=alice@mail.com name=Alice (v1)
email=bob@mail.com name=Bob (v2)
```

**Explanation:**
- `DELETE` adds tombstone row
- Future flush / compaction will remove these logically deleted rows
- For simplicity, tombstones are versioned and respected in reads

---

### **Memtable Rotation & Flush**

```text
anchor> SHOW MEMTABLES
Active memtable buckets: 1
Immutable memtables: 0
anchor> FLUSH ALL
→ Flushed SSTable sst_users_0.dat
anchor> SHOW MEMTABLES
Active memtable buckets: 1
Immutable memtables: 0
```

**Explanation:**
- Active memtable can be rotated to immutable
- `FLUSH ALL` writes immutable memtables to disk as SSTables
- Each SSTable is named with `sst_<table>_<gen>.dat`

---

### **Compaction (LSM Merge Placeholder)**

```text
anchor> COMPACT
→ Compacting SSTables (placeholder for LSM merge)
```

**Explanation:**
- Prepares for future LSM merge / compaction
- Currently just prints a placeholder

---

### **Time-Travel Queries**

```text
anchor> SELECT users ASOF 1
email=alice@mail.com name=Alice (v1)
```

**Explanation:**
- `ASOF <version>` allows reading table state at a past version
- Works across active and immutable memtables

---

### **Exiting CLI**

```text
anchor> EXIT
```

---

## File Structure

- **Single-file C program**: `anchor.c`
- SSTables flushed to disk as `sst_<table>_<gen>.dat`

---

## Future Roadmap

1. LSM merge / compaction with multiple SSTables
2. Column projection in flush / merge
3. TTL enforcement on reads and background cleanup
4. Multi-scope, group-aware access control
5. Streaming read API
6. Multi-node / distributed extension

