# plan.md

## HBF Major Refactor Implementation Plan

### Overview
This refactor replaces CivetWeb with [libhttp](https://github.com/lammertb/libhttp), drops SQLAR and pod build macros, and switches asset embedding to [miniz](https://github.com/richgel999/miniz) (single-file C99, no C++). Assets from `pods/podname/*` will be compressed deterministically via a C packer tool (built with miniz), embedded as C arrays, and loaded into SQLite at startup via **C-only migration** (no JS involved). SQLite DB defaults to on-disk (`./hbf.db`) with idempotent migration checking; `--inmem` flag enables in-memory mode for testing.

---

### Architecture & Design Decisions

#### Asset Embedding Pipeline
- **Hermetic build**: C packer tool (`tools/asset_packer`) built with miniz under Bazel
- **Deterministic output**: Stable file ordering, zeroed mtimes, `TZ=UTC`
- **Format**: Custom binary format: `[name_len:u32][name:bytes][data_len:u32][data:bytes]...`
- **Compression**: High-level zlib compression via miniz
- **Output**: `assets_blob.c` and `assets_blob.h` with symbols `assets_blob[]` and `assets_blob_len`

#### Migration & Idempotency
- **C-only**: No JS involved in asset loading; reduces boot dependencies
- **Idempotent**: `migrations` table tracks applied bundles by SHA256 hash
- **Transaction safety**: Rollback on error; no partial writes
- **Error modes**:
  - `MIGRATE_OK`: Success
  - `MIGRATE_ERR_ALREADY_APPLIED`: Bundle already loaded (not an error)
  - `MIGRATE_ERR_DECOMPRESS`: Corrupt or invalid compressed data
  - `MIGRATE_ERR_DB`: SQLite transaction failed

#### Database Persistence
- **Default**: On-disk SQLite file at `./hbf.db` (or `HBF_DB_PATH` env var)
- **Fast restart**: Migration skips if bundle already applied (checks hash)
- **Testing**: `--inmem` CLI flag enables in-memory DB (ephemeral, re-migrates each boot)
- **No runtime FS access**: All assets read from SQLite `overlay` table

#### Boot Sequence
1. Parse CLI args (`--inmem`, `--db-path`, `--port`)
2. Init SQLite (on-disk or `:memory:` based on `--inmem`)
3. Create schema (`overlay`, `migrations` tables)
4. Call `overlay_fs_migrate_assets(db, assets_blob, assets_blob_len)` (idempotent)
5. Start libhttp server
6. Init QuickJS runtime (reads from SQLite via `overlay_fs_read_file()`)
7. Wait for shutdown signal

#### Risk Mitigations
- **JS-driven migration risk**: Original plan used QuickJS for migration, creating boot dependency and needing temporary JS bindings for miniz. **Mitigation**: C-only migration eliminates JS from critical path.
- **Hermeticity risk**: Relying on host `xxd` tool is non-hermetic and flaky. **Mitigation**: C packer tool built under Bazel with miniz ensures reproducible builds.
- **Double-loading risk**: Without idempotency checks, restarts could duplicate assets. **Mitigation**: `migrations` table with SHA256-based bundle tracking prevents re-application.
- **Partial load risk**: Errors during migration could leave DB in inconsistent state. **Mitigation**: SQLite transaction with rollback ensures atomicity.
- **libhttp threading**: Dynamic routes use QuickJS with per-request fresh context; ensure libhttp handler model preserves this. **Mitigation**: Keep existing mutex serialization for dynamic routes.

---

### Step-by-Step Plan

1. **Branch Setup** ✅ COMPLETE
   - Working on `migrate` branch.

2. **Remove CivetWeb** ✅ COMPLETE
   - Deleted CivetWeb from `third_party/civetweb/` and updated `MODULE.bazel`.
   - Removed all CivetWeb-specific build flags and code from Bazel and C sources.
   - Replaced HTTP server logic in `hbf/http/` with libhttp equivalents.

3. **Integrate libhttp** ✅ COMPLETE
   - Added `libhttp` as a third_party dependency (MIT license) in `MODULE.bazel`.
   - Created `third_party/ews/libhttp.BUILD` with proper build configuration.
   - Updated HTTP handlers to use libhttp API:
     - `hbf/http/server.h` - Updated to use `struct lh_ctx_t` and `struct lh_con_t`
     - `hbf/http/server.c` - Replaced all `mg_*` functions with `lh_*` equivalents
     - `hbf/http/handler.h` - Updated handler signature
     - `hbf/http/handler.c` - Updated to use libhttp types and functions
     - `hbf/qjs/bindings/request.h/c` - Updated to use `struct lh_con_t` and `lh_*` functions
     - `hbf/qjs/bindings/response.h/c` - Updated to send responses via libhttp
     - `hbf/http/BUILD.bazel` - Updated dependency from `@civetweb` to `@libhttp`

4. **Integrate miniz for Asset Compression** ⏳ IN PROGRESS
   - ✅ Added miniz single-file C99 to `third_party/miniz/miniz.c` and `miniz.h`.
   - ✅ Created `third_party/miniz/BUILD.bazel` with proper build configuration.
   - 🔄 **NEXT**: Build C asset packer tool (`tools/asset_packer.c`) that:
     - Takes pod asset files as input (sorted deterministically)
     - Compresses with miniz (high compression level)
     - Outputs `.c` and `.h` files with named symbols (`assets_blob`, `assets_blob_len`)
     - Ensures reproducibility (zeroed mtimes, stable order, TZ=UTC)
   - 🔄 **THEN**: Create Bazel genrule to invoke packer and embed assets hermetically.

5. **Drop SQLAR and Pod Build Macros** ✅ COMPLETE
   - Removed SQLAR from `third_party/sqlar/`, `MODULE.bazel`, and all related scripts (`db_to_c.sh`, `sql_to_c.sh`).
   - Deleted pod registration macros from `tools/pod_binary.bzl` and root `BUILD.bazel`.
   - Removed SQLAR initialization and related code from overlay_fs, but **overlay_fs API remains** (used elsewhere in the application). Stub or refactor as needed, but do NOT remove overlay_fs files.

6. **Startup Asset Migration** ⏳ PENDING
   - Implement C function `overlay_fs_migrate_assets(db, bundle_blob, bundle_len)`:
     - Compute SHA256 hash of bundle → `bundle_id`
     - Check `migrations` table; skip if already applied (idempotent)
     - Decompress bundle using miniz (`mz_uncompress`)
     - Parse entries and insert into `overlay` table via transaction
     - Record migration in `migrations` table with `bundle_id` and timestamp
   - Call from `main.c` boot sequence (after SQLite init, before libhttp start)
   - Support both on-disk DB (default: `./hbf.db`) and `--inmem` flag for testing
   - **No QuickJS involvement** in migration; keeps boot simple and deterministic.

7. **Update QuickJS Integration** ⏳ PENDING
   - Pass pointers to the SQLite DB and embedded assets to QuickJS runtime.
   - Ensure all file reads/writes go through SQLite only.

8. **Remove Filesystem Access** ⏳ PENDING
   - Audit codebase to ensure no runtime filesystem access remains except for overlay_fs API, which is retained for compatibility. All reads/writes should use SQLite DB, with overlay_fs providing the required API surface.

9. **Testing & Validation** ⏳ PENDING
   - **Migration tests**:
     - Idempotency: Run `overlay_fs_migrate_assets()` twice; verify no duplicates and second call returns `MIGRATE_ERR_ALREADY_APPLIED`
     - Corruption: Pass invalid/truncated blob; verify error return and no partial DB writes
     - Empty bundle: Pass zero-length blob; verify graceful handling
     - In-memory mode: Test with `--inmem` flag; verify ephemeral behavior
   - **Integration tests**:
     - Static route: GET `/static/file.js` reads from DB after migration
     - Dynamic route: GET `/api/health` executes `server.js` ESM module from DB
     - Missing module: Import non-existent file returns 404 with clear error
   - **Regression tests**:
     - Update existing tests to use new asset pipeline (remove SQLAR references)
     - Verify `overlay_fs_read_file()` API still works (compatibility layer)
   - **Build validation**:
     - Zero warnings under `-Wall -Wextra -Werror`
     - Passing `//...` test suite
     - Lint passes (`//:lint` target)

10. **Documentation** ⏳ PENDING
   - Update `AGENTS.md`, `README.md`, and `DOCS/` to reflect new architecture and build pipeline.

---

### Notes
- Only MIT/BSD/Apache-2.0/Public Domain dependencies allowed.
- No C++ code from miniz; use C99 single-file only.
- No runtime filesystem access; SQLite DB is the sole data store.
- All asset embedding and migration must be automated in Bazel build.

---

## Current Status Summary

### Completed ✅
- Branch setup on `migrate`
- CivetWeb completely removed and replaced with libhttp
- All HTTP server code updated to use libhttp API (mg_* → lh_*)
- miniz added as dependency for future asset compression
- Build system updated to use new dependencies

### Files Modified
- `MODULE.bazel` - Replaced CivetWeb git_repository with libhttp
- `third_party/ews/BUILD.bazel` - Created (wrapper for libhttp)
- `third_party/ews/libhttp.BUILD` - Created (external build file)
- `third_party/miniz/BUILD.bazel` - Created
- `third_party/miniz/miniz.c` - Downloaded
- `third_party/miniz/miniz.h` - Downloaded
- `hbf/http/server.h` - Updated to libhttp types
- `hbf/http/server.c` - Updated all mg_* calls to lh_*
- `hbf/http/handler.h` - Updated to libhttp types
- `hbf/http/handler.c` - Updated to libhttp types and functions
- `hbf/http/BUILD.bazel` - Updated dependency
- `hbf/qjs/bindings/request.h` - Updated to libhttp types
- `hbf/qjs/bindings/request.c` - Updated mg_* calls to lh_*
- `hbf/qjs/bindings/response.h` - Updated to libhttp types
- `hbf/qjs/bindings/response.c` - Updated mg_* calls to lh_*

### Removed
- `third_party/civetweb/` directory and all contents

### Next Steps 🔄

#### Immediate (Make it Compile)
1. ✅ Remove SQLAR and pod build macros
2. 🔄 **IN PROGRESS**: Build C asset packer tool (`tools/asset_packer.c`)
3. Stub `overlay_fs_migrate_assets()` with no-op return
4. Remove SQLAR symbols from `db.c` (`sqlite3_sqlar_init`, `fs_db_data`, `fs_db_len`)
5. Add placeholder `assets_blob[]` to `main.c` and wire migration call
6. Fix compilation errors

#### Next (Real Implementation)
7. Implement real migration logic in `overlay_fs_migrate_assets()`:
   - SHA256 hash computation
   - Idempotency check via `migrations` table
   - miniz decompression
   - SQLite transaction + insert
8. Create Bazel genrule to invoke packer and embed assets
9. Wire packer output into binary build

#### Final (Test & Ship)
10. Add migration tests (idempotency, corruption, --inmem mode)
11. Update all tests to use new asset pipeline
12. Run `//...` test suite, fix failures
13. Update `AGENTS.md` to remove CivetWeb/SQLAR references
14. Ensure zero warnings and passing lint

### Known Issues / Blockers

#### Compilation Issues (Immediate)
The codebase currently will not compile because:

1. **SQLAR symbols still referenced** in `db.c` and `db.h`:
   - `sqlite3_sqlar_init()` calls (remove)
   - `fs_db_data` and `fs_db_len` symbols (replace with `assets_blob`, `assets_blob_len`)

2. **Missing migration function**:
   - `overlay_fs_migrate_assets()` not yet implemented (needs stub first)

3. **Missing asset packer tool**:
   - `tools/asset_packer.c` doesn't exist yet
   - Bazel genrule to invoke it not created

#### Post-Compilation Issues
- **libhttp API verification**: Need to runtime-test all `mg_*` → `lh_*` replacements work correctly
- **QuickJS module loading**: Verify ESM imports work after migration from SQLAR to new format
- **Static file serving**: Test that large files decompress and serve correctly via libhttp
- **Test suite**: Many tests reference SQLAR functions and will need updates

---

## Implementation Reference

### C Migration Function Contract

```c
// hbf/overlay_fs/migrate.h

#include <sqlite3.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    MIGRATE_OK = 0,
    MIGRATE_ERR_DECOMPRESS = -1,
    MIGRATE_ERR_DB = -2,
    MIGRATE_ERR_ALREADY_APPLIED = -3,  // Not an error; indicates no-op
    MIGRATE_ERR_CORRUPT = -4
} migrate_status_t;

/**
 * Migrates embedded asset bundle into SQLite DB.
 * Idempotent: skips if bundle_id (SHA256 hash) already applied.
 *
 * @param db Open SQLite connection
 * @param bundle_blob Embedded compressed asset bundle
 * @param bundle_len Size of bundle in bytes
 * @return migrate_status_t status code
 */
migrate_status_t overlay_fs_migrate_assets(
    sqlite3* db,
    const uint8_t* bundle_blob,
    size_t bundle_len
);
```

**Implementation steps:**
1. Compute `bundle_id = SHA256(bundle_blob)`
2. Check: `SELECT COUNT(*) FROM migrations WHERE bundle_id = ?`
   - If found → return `MIGRATE_ERR_ALREADY_APPLIED`
3. Decompress using `miniz`: `mz_uncompress()`
4. Parse binary format: `[name_len:u32][name:bytes][data_len:u32][data:bytes]...`
5. `BEGIN TRANSACTION`
6. For each entry: `INSERT OR REPLACE INTO overlay (path, content, mtime) VALUES (?, ?, unixepoch())`
7. `INSERT INTO migrations (bundle_id, applied_at) VALUES (?, unixepoch())`
8. `COMMIT`
9. On error: `ROLLBACK` and return error code

---

### Asset Packer Tool Specification

**Binary**: `tools/asset_packer`

**Usage**:
```bash
asset_packer \
  --output-source output.c \
  --output-header output.h \
  --symbol-name assets_blob \
  file1.js file2.html ...
```

**Behavior**:
1. Read all input files (preserve order from args)
2. For each file:
   - Store relative path (deterministic, sorted lexicographically)
   - Read content into memory
3. Pack into binary format:
   ```
   [num_entries:u32]
   [name_len:u32][name:bytes][data_len:u32][data:bytes]
   [name_len:u32][name:bytes][data_len:u32][data:bytes]
   ...
   ```
4. Compress entire blob with miniz: `mz_compress2()` at level 9 (best compression)
5. Write `.c` file:
   ```c
   #include "output.h"
   const unsigned char assets_blob[] = { 0x1f, 0x8b, ... };
   const size_t assets_blob_len = 12345;
   ```
6. Write `.h` file:
   ```c
   #ifndef ASSETS_BLOB_H
   #define ASSETS_BLOB_H
   #include <stddef.h>
   extern const unsigned char assets_blob[];
   extern const size_t assets_blob_len;
   #endif
   ```

**Reproducibility requirements**:
- Stable input ordering (caller's responsibility via sorted args)
- Zeroed/deterministic timestamps (not stored in format)
- `TZ=UTC` environment variable set during build

---

### Bazel Integration Example

```python
# tools/BUILD.bazel

cc_binary(
    name = "asset_packer",
    srcs = ["asset_packer.c"],
    deps = ["//third_party/miniz"],
    copts = ["-std=c99", "-Wall", "-Wextra", "-Werror"],
)

# hbf/assets/BUILD.bazel

load("//tools:embed_assets.bzl", "embed_assets")

embed_assets(
    name = "embedded_assets",
    srcs = glob([
        "pods/**/*.js",
        "pods/**/*.html",
        "pods/**/*.css",
    ]),
    out_header = "assets_blob.h",
    out_source = "assets_blob.c",
)

cc_library(
    name = "assets",
    srcs = ["assets_blob.c"],
    hdrs = ["assets_blob.h"],
    visibility = ["//visibility:public"],
)
```

---

## End of Plan
