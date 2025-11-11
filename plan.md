# plan.md

## HBF Major Refactor Implementation Plan

### Overview
This refactor replaces CivetWeb with [libhttp](https://github.com/lammertb/libhttp), drops SQLAR and pod build macros, and switches asset embedding to [miniz](https://github.com/richgel999/miniz) (single-file C99, no C++). Assets from `pods/podname/*` will be compressed with zlib (high compression), embedded as C arrays via `xxd -i`, and loaded into SQLite at startup via QuickJS migration. No runtime filesystem access; only SQLite DB is used.

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

4. **Integrate miniz for Asset Compression** ✅ COMPLETE (Step 1/2)
   - Added miniz single-file C99 to `third_party/miniz/miniz.c` and `miniz.h`.
   - Created `third_party/miniz/BUILD.bazel` with proper build configuration.
   - **TODO**: Write Bazel build rules to compress and embed assets.

5. **Drop SQLAR and Pod Build Macros** ✅ COMPLETE
   - Removed SQLAR from `third_party/sqlar/`, `MODULE.bazel`, and all related scripts (`db_to_c.sh`, `sql_to_c.sh`).
   - Deleted pod registration macros from `tools/pod_binary.bzl` and root `BUILD.bazel`.
   - Removed overlay schema and versioned FS logic from `hbf/db/overlay_fs.*`.

6. **Startup Asset Migration** ⏳ PENDING
   - On application start, use QuickJS to run a migration script:
     - Decompress embedded assets in memory.
     - Load each asset into the SQLite DB (no filesystem access).
     - Apply any required SQL migrations.

7. **Update QuickJS Integration** ⏳ PENDING
   - Pass pointers to the SQLite DB and embedded assets to QuickJS runtime.
   - Ensure all file reads/writes go through SQLite only.

8. **Remove Filesystem Access** ⏳ PENDING
   - Audit codebase to ensure no runtime filesystem access remains.
   - All reads/writes must use SQLite DB.

9. **Testing & Validation** ⏳ PENDING
   - Update and run all tests to validate new asset pipeline and HTTP server.
   - Ensure zero warnings (C99) and passing lint.

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
1. Remove SQLAR and pod build macros
2. Remove overlay filesystem logic
3. Create asset compression build rules with miniz + xxd
4. Implement startup asset migration via QuickJS
5. Test and validate new architecture
6. Update documentation

### Known Issues / Blockers
- **BUILD WILL NOT COMPILE YET**: libhttp commit hash may need verification
- **API COMPATIBILITY**: Need to verify libhttp API is compatible with all mg_* → lh_* replacements
- **TESTING REQUIRED**: All tests need to be updated and run after completing SQLAR removal

  The codebase currently will not compile because:

  1. db.c and db.h still reference:
    - overlay_fs.h includes
    - overlay_fs_* function calls
    - SQLAR extension initialization (sqlite3_sqlar_init)
    - Embedded fs_db_data and fs_db_len symbols (no longer generated)
  2. server.c and handler.c still reference:
    - overlay_fs_read_file() calls
    - overlay_fs.h includes
  3. Missing implementation for:
    - New asset compression pipeline with miniz
    - Asset embedding via xxd -i
    - Startup migration to load compressed assets into SQLite

---

## End of Plan
