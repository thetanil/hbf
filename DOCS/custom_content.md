# Pod Content Pipeline: Build Host to Runtime Database

This document explains how pod content (JavaScript files and static assets) flows from the build host through SQLAR archiving, schema overlay, and into the runtime versioned filesystem.

---
## Overview

HBF uses a multi-stage build pipeline to embed pod content into the binary:

1. **Build Host**: Pod directory with `hbf/*.js` and `static/**` files
2. **SQLAR Archive**: Files packed into SQLite archive format
3. **Schema Overlay**: Versioned filesystem schema applied
4. **VACUUM**: Database optimized to remove dead space
5. **C Array**: Database converted to embedded C array
6. **Runtime**: Content accessible via versioned filesystem API

At runtime, the embedded database is opened in memory or on disk, and all file operations go through the **overlay filesystem** (`hbf/db/overlay_fs.c`), which provides versioned file tracking.

---
## Build-Time Pipeline

### Stage 1: Pod Directory Structure

Each pod follows a standard layout:

```
pods/
  base/                    # Example pod
    BUILD.bazel            # Must contain pod_db target
    hbf/
      server.js            # Entry point (required)
      lib/
        util.js            # Additional modules (optional)
    static/
      index.html           # Static assets
      style.css
      vendor/
        htmx.min.js
```

**Key points**:
- `hbf/server.js` is the required entry point that exports `globalThis.app.handle(req, res)`
- ESM imports work for relative paths: `import foo from "./lib/util.js"`
- Static files are served directly via `/static/*` routes
- All files are embedded at build time

### Stage 2: SQLAR Archive Creation

The `pod_db` rule in each pod's `BUILD.bazel` creates a SQLAR archive:

```python
# pods/base/BUILD.bazel
sqlar(
    name = "pod_db",
    srcs = glob([
        "hbf/**/*.js",
        "static/**/*",
    ]),
    compress = True,
)
```

**What happens**:
- Bazel collects all matching files from `hbf/` and `static/` directories
- SQLite's SQLAR format packs files into a database with this schema:
  ```sql
  CREATE TABLE sqlar(
    name TEXT PRIMARY KEY,  -- File path
    mode INT,               -- File permissions
    mtime INT,              -- Modification time
    sz INT,                 -- Uncompressed size
    data BLOB               -- File content (possibly compressed)
  );
  ```
- Compression reduces binary size (e.g., 359 KB for base pod)

**Output**: `pods/base/pod.db` (SQLAR archive)

### Stage 3: Schema Overlay Application

The `pod_binary()` macro applies the versioned filesystem schema:

```python
# From tools/pod_binary.bzl (simplified)
overlay_sqlar(
    name = name + "_overlay",
    src = pod + ":pod_db",
    schema = "//hbf/db:overlay_schema",
)
```

**What happens**:
- Reads the authoritative schema from `hbf/db/overlay_schema.sql`
- Creates tables for versioned filesystem:
  ```sql
  CREATE TABLE file_ids (
    file_id INTEGER PRIMARY KEY,
    path TEXT UNIQUE NOT NULL
  );

  CREATE TABLE file_versions (
    file_id INTEGER NOT NULL,
    version_number INTEGER NOT NULL,
    mtime INTEGER NOT NULL,
    size INTEGER NOT NULL,
    data BLOB,
    PRIMARY KEY (file_id, version_number)
  ) WITHOUT ROWID;

  CREATE VIEW latest_files AS
    SELECT fv.data, fi.path
    FROM file_versions fv
    JOIN file_ids fi ON fv.file_id = fi.file_id
    WHERE (fv.file_id, fv.version_number) IN (
      SELECT file_id, MAX(version_number)
      FROM file_versions
      GROUP BY file_id
    );
  ```
- Migrates SQLAR data to versioned filesystem (all files become version 1)
- Creates materialized view `latest_files_meta` for fast file listings
- Drops original `sqlar` table

**Why versioned filesystem?**
- Immutable file history (every write creates a new version)
- Dev mode live editing with full audit trail
- Fast indexed reads using `file_id` + `version_number`

### Stage 4: VACUUM Optimization

```bash
sqlite3 temp.db "VACUUM INTO optimized.db"
```

**What happens**:
- Copies database to new file, removing all dead space
- Rewrites pages for optimal layout
- Reduces binary size significantly
- Result: production-ready database with no wasted bytes

**Output**: Optimized database file

### Stage 5: C Array Embedding

The `db_to_c` tool converts the database to C source:

```bash
tools/db_to_c.sh optimized.db hbf_base_fs_embedded.c hbf_base_fs_embedded.h
```

**Generated C code**:
```c
/* hbf_base_fs_embedded.c */
const unsigned char fs_db_data[] = {
    0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, /* SQLite header */
    /* ... ~359 KB of database bytes ... */
};
const unsigned long fs_db_len = 367616;
```

**Linked into binary**:
```python
cc_library(
    name = "hbf_base_embedded",
    srcs = ["hbf_base_fs_embedded.c"],
    hdrs = ["hbf_base_fs_embedded.h"],
    alwayslink = True,  # Force linker to include symbols
)
```

**Output**: Final binary at `bazel-bin/bin/hbf` with embedded database

---
## Runtime: Reading and Writing Files

### Database Initialization

At startup (`hbf/shell/main.c`):

```c
/* Open embedded database */
sqlite3 *db = NULL;
hbf_db_open_from_embedded(fs_db_data, fs_db_len, &db);

/* Register global handle for overlay_fs operations */
overlay_fs_init_global(db);
```

The embedded database is copied to memory (or disk) and opened with:
- `PRAGMA journal_mode=WAL` - Write-ahead logging for concurrency
- `PRAGMA foreign_keys=ON` - Enforce referential integrity
- `PRAGMA synchronous=NORMAL` - Balance durability and performance

### Reading Files (Static Handler)

**C Code** (`hbf/http/server.c:93`):
```c
/* Read file from versioned filesystem */
unsigned char *data = NULL;
size_t size;
int ret = overlay_fs_read_file(path, server->dev, &data, &size);
```

**What happens**:
1. `overlay_fs_read_file()` queries `latest_files` view:
   ```sql
   SELECT data FROM latest_files WHERE path = ?
   ```
2. Returns latest version of file (highest `version_number`)
3. Works for both base content (version 1) and overlay edits (version 2+)
4. Static files served in parallel (no mutex, SQLite handles concurrency)

**File path mapping**:
- Request: `GET /static/style.css`
- Handler strips `/`: `static/style.css`
- Database path: `static/style.css` (matches embedded path)

### Writing Files (Dev Mode API)

**JavaScript Code** (`pods/base/hbf/server.js:189-247`):
```javascript
// PUT /__dev/api/file?name=static/style.css
const fileName = query.name;
const content = req.body;

// 1. Get or create file_id
db.execute("INSERT OR IGNORE INTO file_ids (path) VALUES (?)", [fileName]);
const fileIdResult = db.query("SELECT file_id FROM file_ids WHERE path = ?", [fileName]);
const fileId = fileIdResult[0].file_id;

// 2. Get next version number
const versionResult = db.query(
    "SELECT COALESCE(MAX(version_number), 0) + 1 AS next_version FROM file_versions WHERE file_id = ?",
    [fileId]
);
const nextVersion = versionResult[0].next_version;

// 3. Insert new version
db.execute(
    "INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) VALUES (?, ?, ?, ?, ?, ?)",
    [fileId, fileName, nextVersion, mtime, size, content]
);
```

**What happens**:
1. Dev mode PUT creates version 2 of file
2. Next read returns version 2 (latest)
3. Version 1 (build-time) preserved for history
4. All operations use SQLite transactions (atomic)

**Version history example**:
```
file_versions table:
file_id | path              | version_number | mtime      | data
--------|-------------------|----------------|------------|----------
1       | static/style.css  | 1              | 1735862400 | (original)
1       | static/style.css  | 2              | 1735948800 | (edited)
1       | static/style.css  | 3              | 1736035200 | (edited)
```

### Dynamic Module Loading

**JavaScript Code** (`pods/test/hbf/server.js:4`):
```javascript
import { hello } from "./lib/esm_test.js";
```

**What happens** (`hbf/qjs/module_loader.c:134-170`):
1. QuickJS normalizes path: `hbf/server.js` + `./lib/esm_test.js` → `hbf/lib/esm_test.js`
2. Module loader queries database:
   ```sql
   SELECT data FROM latest_files WHERE path = 'hbf/lib/esm_test.js'
   ```
3. Compiles module with `JS_Eval(ctx, src, len, path, JS_EVAL_TYPE_MODULE)`
4. Returns module definition to QuickJS

**Import resolution**:
- ✅ Relative paths: `./lib/foo.js`, `../util.js`
- ❌ Bare specifiers: `import 'lodash'` (not supported yet)
- ❌ URL imports: `import 'https://...'` (not supported)

---
## File Lifecycle: Build Host to Runtime

### Example: Adding `hbf/lib/auth.js`

**1. Create file on build host**:
```bash
# pods/base/hbf/lib/auth.js
export function checkAuth(token) {
    return token === "secret";
}
```

**2. Build pipeline**:
```bash
bazel build //:hbf
```

- SQLAR: `pods/base/hbf/lib/auth.js` → row in `sqlar` table
- Schema: Migrated to `file_versions` as version 1
- VACUUM: Database optimized
- C array: `fs_db_data` includes auth.js bytes
- Binary: `bazel-bin/bin/hbf` contains embedded file

**3. Runtime import**:
```javascript
// In hbf/server.js
import { checkAuth } from "./lib/auth.js";

app.handle = function(req, res) {
    if (checkAuth(req.headers.Authorization)) {
        // ...
    }
};
```

**4. Dev mode edit**:
```bash
curl -X PUT 'http://localhost:5309/__dev/api/file?name=hbf/lib/auth.js' \
  --data 'export function checkAuth(token) { return token === "newsecret"; }'
```

- Creates version 2 in `file_versions` table
- Next request loads version 2
- Original version 1 preserved for rollback

**5. Version history query**:
```sql
SELECT version_number, mtime, size
FROM file_versions
WHERE file_id = (SELECT file_id FROM file_ids WHERE path = 'hbf/lib/auth.js')
ORDER BY version_number DESC;

-- Results:
-- version_number | mtime      | size
-- 2              | 1735948800 | 67
-- 1              | 1735862400 | 54
```

---
## Overlay Filesystem API

### C API (`hbf/db/overlay_fs.h`)

**Global handle functions** (used by runtime):
```c
/* Set global database handle */
void overlay_fs_init_global(sqlite3 *db);

/* Read latest version (uses global handle) */
int overlay_fs_read_file(const char *path, int dev,
                         unsigned char **data, size_t *size);

/* Write new version (uses global handle) */
int overlay_fs_write_file(const char *path,
                          const unsigned char *data, size_t size);
```

**Explicit handle functions** (used by tests):
```c
/* Read latest version */
int overlay_fs_read(sqlite3 *db, const char *path,
                   unsigned char **data, size_t *size);

/* Write new version */
int overlay_fs_write(sqlite3 *db, const char *path,
                    const unsigned char *data, size_t size);

/* Check file existence */
int overlay_fs_exists(sqlite3 *db, const char *path);

/* Get version count */
int overlay_fs_version_count(sqlite3 *db, const char *path);

/* Migrate SQLAR to versioned FS */
int overlay_fs_migrate_sqlar(sqlite3 *db);
```

### JavaScript API (via `db` module)

**Query (read-only)**:
```javascript
const results = db.query(
    "SELECT data FROM latest_files WHERE path = ?",
    ["static/style.css"]
);
```

**Execute (write)**:
```javascript
db.execute(
    "INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) VALUES (?, ?, ?, ?, ?, ?)",
    [fileId, path, version, Date.now() / 1000, content.length, content]
);
```

**Transaction example**:
```javascript
// JavaScript doesn't have explicit transaction API,
// but each db.execute() runs in a transaction
db.execute("BEGIN IMMEDIATE");
db.execute("INSERT INTO file_ids (path) VALUES (?)", [path1]);
db.execute("INSERT INTO file_ids (path) VALUES (?)", [path2]);
db.execute("COMMIT");
```

---
## Performance Characteristics

### Read Performance
- Indexed query on `(file_id, version_number DESC)`
- 142,000+ reads/sec (in-memory benchmark, 1000 files)
- No mutex for static file serving (parallel reads)
- SQLite WAL mode: readers don't block writers

### Write Performance
- Append-only (no UPDATE, only INSERT)
- 37,000+ writes/sec (multi-version benchmark)
- Each write creates new version (immutable history)
- Dev mode writes serialized via SQLite transactions

### Storage Characteristics
- Base pod: ~359 KB embedded (VACUUMed)
- Each version stores full file content (no delta compression)
- `WITHOUT ROWID` optimization for `file_versions` table
- Materialized `latest_files_meta` view for fast listings

---
## Troubleshooting

### Files not found at runtime

**Symptom**: `404 Not Found` for files that exist in pod directory

**Check**:
1. Verify file is in `hbf/` or `static/` directory
2. Check `glob()` pattern in `pod_db` rule includes file
3. Rebuild: `bazel build //:hbf`
4. Inspect embedded database:
   ```bash
   # Extract database from binary (if needed)
   strings bazel-bin/bin/hbf | grep -A 5 "SQLite format"

   # Or check via runtime:
   bazel run //:hbf -- --dev
   curl http://localhost:5309/__dev/api/files | jq
   ```

### Module import fails

**Symptom**: `ReferenceError: could not load module 'X'`

**Check**:
1. Use relative paths: `import foo from "./lib/bar.js"` (not `"lib/bar.js"`)
2. Verify module file exists in database (see above)
3. Check module path resolution:
   - Base: `hbf/server.js`
   - Import: `./lib/util.js`
   - Resolved: `hbf/lib/util.js` ✅

### Dev mode edits not visible

**Symptom**: Changes via `PUT /__dev/api/file` don't appear

**Check**:
1. Cache headers: Dev mode should return `Cache-Control: no-store`
2. Verify write succeeded: Check HTTP response status
3. Query version count:
   ```javascript
   const count = db.query(
       "SELECT COUNT(*) as c FROM file_versions WHERE file_id = (SELECT file_id FROM file_ids WHERE path = ?)",
       ["static/style.css"]
   )[0].c;
   console.log(`Versions: ${count}`);
   ```

### Large binary size

**Symptom**: Binary size unexpectedly large

**Check**:
1. Verify VACUUM was applied (check `pod_binary.bzl`)
2. Remove unused files from `glob()` pattern
3. Consider vendoring large libraries as separate static files
4. Check for duplicate files in multiple pods

---
## Best Practices

### File Organization
- Keep `hbf/server.js` minimal (import from `hbf/lib/`)
- Group related modules: `hbf/lib/auth.js`, `hbf/lib/db.js`
- Use meaningful directory names: `static/vendor/`, `static/images/`

### ESM Imports
- Always use relative paths: `./lib/foo.js`
- Include `.js` extension
- Avoid circular dependencies
- Prefer static imports for better optimization

### Dev Mode
- Use `PUT /__dev/api/file` for quick edits
- Check version history before deploying changes
- Test edits thoroughly before rebuilding binary

### Production
- Minimize embedded file count
- Bundle npm packages if needed (esbuild/rollup)
- Use compression for large text files
- Profile binary size after adding content

---
## Quick Reference

### Add file to pod
1. Create file in `pods/base/hbf/` or `pods/base/static/`
2. Build: `bazel build //:hbf`
3. File automatically included via `glob()` pattern
4. Import in JS: `import foo from "./path/to/file.js"`

### Edit file in dev mode
```bash
# Read current version
curl http://localhost:5309/__dev/api/file?name=static/style.css

# Write new version
curl -X PUT http://localhost:5309/__dev/api/file?name=static/style.css \
  --data-binary @new-style.css

# Verify change
curl http://localhost:5309/static/style.css
```

### Check embedded files
```bash
bazel run //:hbf -- --dev
curl http://localhost:5309/__dev/api/files | jq '.[] | .name'
```

### View version history
```javascript
// In server.js or dev console
const versions = db.query(
    "SELECT version_number, mtime, size FROM file_versions WHERE file_id = (SELECT file_id FROM file_ids WHERE path = ?)",
    ["static/style.css"]
);
```

---
## Related Documentation
- `AGENTS.md` - Build commands and agent workflow
- `CLAUDE.md` - Detailed architecture and pipeline
- `hbf/db/overlay_schema.sql` - Authoritative database schema
- `tools/pod_binary.bzl` - Pod build macro implementation
- `agent_help.md` - Verified implementation details
