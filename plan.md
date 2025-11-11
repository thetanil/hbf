# plan.md

## HBF Major Refactor Implementation Plan

### Overview
This refactor replaces CivetWeb with [libhttp](https://github.com/lammertb/libhttp), drops SQLAR and pod build macros, and switches asset embedding to [miniz](https://github.com/richgel999/miniz) (single-file C99, no C++). Assets from `pods/podname/*` will be compressed with zlib (high compression), embedded as C arrays via `xxd -i`, and loaded into SQLite at startup via QuickJS migration. No runtime filesystem access; only SQLite DB is used.

---

### Step-by-Step Plan

1. **Branch Setup**
   - Create a new branch for the refactor.

2. **Remove CivetWeb**
   - Delete CivetWeb from `third_party/civetweb/` and update `MODULE.bazel`.
   - Remove all CivetWeb-specific build flags and code from Bazel and C sources.
   - Replace HTTP server logic in `hbf/http/` with libhttp equivalents.

3. **Integrate libhttp**
   - Add `libhttp` as a third_party dependency (MIT license).
   - Implement HTTP handlers using libhttp API in C.
   - Use the embedded server pattern as shown in [embedded_c.c](https://github.com/lammertb/libhttp/blob/master/examples/embedded_c/embedded_c.c) to run the HTTP server inside the HBF binary.
   - Update entrypoint and request routing to use libhttp.

4. **Drop SQLAR and Pod Build Macros**
   - Remove SQLAR from `third_party/sqlar/`, `MODULE.bazel`, and all related scripts (`db_to_c.sh`, `sql_to_c.sh`).
   - Delete pod registration macros from `tools/pod_binary.bzl` and root `BUILD.bazel`.
   - Remove overlay schema and versioned FS logic from `hbf/db/overlay_fs.*`.

5. **Integrate miniz for Asset Compression**
   - Add miniz single-file C99 to `third_party/miniz/miniz.c` and `miniz.h`.
   - Write a Bazel build rule or script to:
     - Compress each asset in `pods/podname/*` with zlib (max compression).
     - Generate C header for each asset: `xxd -i asset.bin.deflate > asset_deflate.h`.
     - Include generated headers in the build.

6. **Startup Asset Migration**
   - On application start, use QuickJS to run a migration script:
     - Decompress embedded assets in memory.
     - Load each asset into the SQLite DB (no filesystem access).
     - Apply any required SQL migrations.

7. **Update QuickJS Integration**
   - Pass pointers to the SQLite DB and embedded assets to QuickJS runtime.
   - Ensure all file reads/writes go through SQLite only.

8. **Remove Filesystem Access**
   - Audit codebase to ensure no runtime filesystem access remains.
   - All reads/writes must use SQLite DB.

9. **Testing & Validation**
   - Update and run all tests to validate new asset pipeline and HTTP server.
   - Ensure zero warnings (C99) and passing lint.

10. **Documentation**
   - Update `AGENTS.md`, `README.md`, and `DOCS/` to reflect new architecture and build pipeline.

---

### Notes
- Only MIT/BSD/Apache-2.0/Public Domain dependencies allowed.
- No C++ code from miniz; use C99 single-file only.
- No runtime filesystem access; SQLite DB is the sole data store.
- All asset embedding and migration must be automated in Bazel build.

---

## End of Plan
