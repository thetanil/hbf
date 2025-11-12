# Pod Content Pipeline: Build Host to Runtime Database

This document explains how pod content (JavaScript files and static assets) flows from the build host through asset packing, schema application, and into the runtime versioned filesystem.

---
## Overview

HBF uses a deterministic build pipeline to embed pod content into the binary:

1. **Build Host**: Pod directory with `hbf/*.js` and `static/**` files
2. **Asset Packing**: Files packed into compressed binary format via `asset_packer`
3. **C Array**: Asset bundle embedded as C array in binary
4. **Runtime**: Database created and schema applied
5. **Migration**: Asset bundle decompressed and migrated into versioned filesystem
6. **Access**: Content accessible via versioned filesystem API

At runtime, a SQLite database is created (in-memory or on-disk), the schema is applied, and the embedded asset bundle is migrated into the **overlay filesystem** (`hbf/db/overlay_fs.c`), which provides versioned file tracking.

---
## Build-Time Pipeline

### Stage 1: Pod Directory Structure

Each pod follows a standard layout:

```
pods/
  base/                    # Example pod
    BUILD.bazel            # Must contain packed_assets target
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

### Stage 2: Asset Packing

The `packed_assets` rule in each pod's `BUILD.bazel` packs files into a compressed binary format:

```python
# pods/base/BUILD.bazel
genrule(
    name = "packed_assets",
    srcs = glob(["hbf/**/*", "static/**/*"]),
    outs = ["assets_blob.c", "assets_blob.h"],
    cmd = """
        $(location //tools:asset_packer) \
            --output-source $(location assets_blob.c) \
            --output-header $(location assets_blob.h) \
            --symbol-name assets_blob \
            $(SRCS)
    """,
    tools = ["//tools:asset_packer"],
)
```

**What happens**:
- Bazel collects all matching files from `hbf/` and `static/` directories
- `asset_packer` sorts files lexicographically for determinism
- Packs files into binary format:
  ```
  [num_entries:u32]
  repeat num_entries times:
    [name_len:u32]
    [name:bytes]
    [data_len:u32]
    [data:bytes]
  ```
- Compresses with zlib
- Generates C source/header with `assets_blob[]` and `assets_blob_len`

**Output**: `assets_blob.c` and `assets_blob.h` with embedded compressed bundle

### Stage 3: C Array Embedding

**Generated C code**:
```c
/* assets_blob.c */
const unsigned char assets_blob[] = {
    0x78, 0x9c, 0xed, 0x5d, /* zlib header */
    /* ... compressed asset data ... */
};
const size_t assets_blob_len = 45678;
```

**Linked into binary**:
```python
cc_library(
    name = "embedded_assets",
    srcs = ["assets_blob.c"],
    hdrs = ["assets_blob.h"],
    alwayslink = True,  # Force linker to include symbols
)
```

**Output**: Final binary at `bazel-bin/bin/hbf` with embedded asset bundle

---
## Runtime: Database Initialization and Asset Migration

### Database Initialization

At startup (`hbf/db/db.c`):

```c
/* Open or create database */
sqlite3 *db = NULL;
hbf_db_init(inmem, &db);
```

**What happens**:
1. Opens SQLite database (`:memory:` or `./hbf.db`)
2. Applies overlay_fs schema (tables, indexes, views, triggers)
3. Migrates asset bundle: `overlay_fs_migrate_assets(db, assets_blob, assets_blob_len)`
4. Sets global handle: `overlay_fs_init_global(db)`

The database is configured with:
- `PRAGMA journal_mode=WAL` - Write-ahead logging for concurrency
- `PRAGMA foreign_keys=ON` - Enforce referential integrity

### Asset Bundle Migration

**C Code** (`hbf/db/overlay_fs.c`):
```c
migrate_status_t overlay_fs_migrate_assets(
    sqlite3 *db,
    const uint8_t *bundle_blob,
    size_t bundle_len
);
```

**What happens**:
1. Computes `bundle_id = SHA256(bundle_blob)` for idempotency
2. Checks if already migrated: `SELECT 1 FROM migrations WHERE bundle_id = ?`
3. If already migrated, returns `MIGRATE_ERR_ALREADY_APPLIED`
4. Decompresses bundle with zlib `uncompress()`
5. Parses binary format: reads `num_entries`, then for each entry reads `name_len`, `name`, `data_len`, `data`
6. Begins transaction: `BEGIN IMMEDIATE`
7. For each file: calls `overlay_fs_write(db, name, data, data_len)`
8. Records migration: `INSERT INTO migrations (bundle_id, applied_at, entries) VALUES (...)`
9. Commits transaction

**Versioned filesystem schema** (from `hbf/db/overlay_schema.sql`):
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
  data BLOB NOT NULL,
  PRIMARY KEY (file_id, version_number)
) WITHOUT ROWID;

CREATE VIEW latest_files AS
  SELECT fv.data, fi.path, fv.mtime, fv.size
  FROM file_versions fv
  JOIN file_ids fi ON fv.file_id = fi.file_id
  WHERE (fv.file_id, fv.version_number) IN (
    SELECT file_id, MAX(version_number)
    FROM file_versions
    GROUP BY file_id
  );

CREATE TABLE migrations (
  bundle_id TEXT PRIMARY KEY,  -- SHA256 of compressed bundle
  applied_at INTEGER NOT NULL,
  entries INTEGER NOT NULL
);
```

**Why versioned filesystem?**
- Immutable file history (every write creates a new version)
- Support for live editing with full audit trail (will be gated by JWT auth in future)
- Fast indexed reads using `file_id` + `version_number`

---
## Runtime: Reading and Writing Files

### Reading Files (Static Handler)

**C Code** (`hbf/http/server.c:93`):
```c
/* Read file from versioned filesystem */
unsigned char *data = NULL;
size_t size;
int ret = overlay_fs_read_file(path, 1, &data, &size);
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

### Writing Files (Versioned Filesystem)

The versioned filesystem supports programmatic file writes through the overlay API. File editing endpoints have been removed; JWT-based authentication will gate future editor APIs.

**Overlay FS Write Example** (using C API):
```c
// Write new version of a file
overlay_fs_write(db, "static/style.css", data, size);
```

**What happens internally**:
1. Creates or retrieves `file_id` for the path
2. Calculates next version number
3. Inserts new version with timestamp
4. Next read returns the latest version
5. All previous versions preserved for history
6. All operations use SQLite transactions (atomic)

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

- Asset packing: `pods/base/hbf/lib/auth.js` → packed into binary format
- Compression: Bundle compressed with zlib
- C array: `assets_blob` includes compressed auth.js bytes
- Binary: `bazel-bin/bin/hbf` contains embedded asset bundle

**3. Runtime migration**:
- Database created and schema applied
- Asset bundle decompressed
- auth.js migrated to `file_versions` as version 1
- Available for import

**4. Runtime import**:
```javascript
// In hbf/server.js
import { checkAuth } from "./lib/auth.js";

app.handle = function(req, res) {
    if (checkAuth(req.headers.Authorization)) {
        // ...
    }
};
```

**5. Dev mode edit**:
```bash
curl -X PUT 'http://localhost:5309/__dev/api/file?name=hbf/lib/auth.js' \
  --data 'export function checkAuth(token) { return token === "newsecret"; }'
```

- Creates version 2 in `file_versions` table
- Next request loads version 2
- Original version 1 preserved for rollback

**6. Version history query**:
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

/* Migrate asset bundle to versioned FS */
migrate_status_t overlay_fs_migrate_assets(
    sqlite3 *db,
    const uint8_t *bundle_blob,
    size_t bundle_len
);
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
- Base pod: ~45 KB compressed asset bundle
- Each version stores full file content (no delta compression)
- `WITHOUT ROWID` optimization for `file_versions` table
- Materialized `latest_files_meta` view for fast listings

---
## Troubleshooting

### Files not found at runtime

**Symptom**: `404 Not Found` for files that exist in pod directory

**Check**:
1. Verify file is in `hbf/` or `static/` directory
2. Check `glob()` pattern in `packed_assets` rule includes file
3. Rebuild: `bazel build //:hbf`
4. Check logs for migration errors

### Module import fails

**Symptom**: `ReferenceError: could not load module 'X'`

**Check**:
1. Use relative paths: `import foo from "./lib/bar.js"` (not `"lib/bar.js"`)
2. Verify module file exists in database (check migration logs)
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
1. Check compressed asset bundle size in build output
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
- Compressed assets keep binary size small
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
- `DOCS/asset_mgmt_migration.md` - Asset packer migration guide
