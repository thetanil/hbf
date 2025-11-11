# plan.md

## HBF Major Refactor Implementation Plan

### Overview
This refactor replaces CivetWeb with [EmbeddableWebServer (EWS)](https://github.com/hellerf/EmbeddableWebServer), drops SQLAR and pod build macros, and switches asset embedding to [miniz](https://github.com/richgel999/miniz) (single-file C99, no C++). Assets from `pods/podname/*` will be compressed with zlib (high compression), embedded as C arrays via `xxd -i`, and loaded into SQLite at startup via QuickJS migration. No runtime filesystem access; only SQLite DB is used.

**HTTP Server Choice:** EWS was selected over srv (standalone binary, not a library) and libmicrohttpd (LGPL license violation). EWS is BSD-licensed, single-header, and embeddable.

---

### Implementation Status

#### ✅ Completed

1. **Branch Setup**
   - On `migrate-srv` branch

2. **EWS Integration - Phase 1**
   - ✅ Downloaded `EmbeddableWebServer.h` to `third_party/ews/`
   - ✅ Created `third_party/ews/BUILD.bazel` with pthread linking
   - ✅ Rewrote `hbf/http/server.h` - Changed from `mg_context` to `struct Server*`
   - ✅ Rewrote `hbf/http/server.c` - Implemented EWS callback model:
     - Global server instance for callback access (EWS limitation)
     - `createResponseForRequest()` main router
     - Static file handler using overlay_fs
     - Health check handler
     - IPv4 socket setup with `acceptConnectionsUntilStopped()`
   - ✅ Updated `hbf/http/handler.h` - Changed signature to EWS types

#### 🚧 In Progress

3. **EWS Integration - Phase 2**
   - 🚧 Rewrite `hbf/http/handler.c` - Adapt QuickJS handler from CivetWeb to EWS Request/Response structures
   - ⏳ Update `hbf/qjs/bindings/request.c` - Port from `mg_connection` to EWS `Request` struct
   - ⏳ Update `hbf/qjs/bindings/response.c` - Port from `mg_connection` to EWS `Response` struct
   - ⏳ Update `hbf/http/BUILD.bazel` - Replace `@civetweb` with `//third_party/ews`
   - ⏳ Update `hbf/shell/main.c` - Handle EWS threading (blocking `acceptConnectionsUntilStopped`)

#### ⏳ Remaining Steps

4. **Remove CivetWeb**
   - Delete CivetWeb from `third_party/civetweb/` and update `MODULE.bazel`.
   - Remove all CivetWeb references from Bazel files.

5. **Drop SQLAR and Pod Build Macros**
   - Remove SQLAR from `third_party/sqlar/`, `MODULE.bazel`, and all related scripts (`db_to_c.sh`, `sql_to_c.sh`).
   - Delete pod registration macros from `tools/pod_binary.bzl` and root `BUILD.bazel`.
   - Remove overlay schema and versioned FS logic from `hbf/db/overlay_fs.*`.

6. **Integrate miniz for Asset Compression**
   - Add miniz single-file C99 to `third_party/miniz/miniz.c` and `miniz.h`.
   - Write a Bazel build rule or script to:
     - Compress each asset in `pods/podname/*` with zlib (max compression).
     - Generate C header for each asset: `xxd -i asset.bin.deflate > asset_deflate.h`.
     - Include generated headers in the build.

7. **Startup Asset Migration**
   - On application start, use QuickJS to run a migration script:
     - Decompress embedded assets in memory.
     - Load each asset into the SQLite DB (no filesystem access).
     - Apply any required SQL migrations.

8. **Update QuickJS Integration**
   - Pass pointers to the SQLite DB and embedded assets to QuickJS runtime.
   - Ensure all file reads/writes go through SQLite only.

9. **Remove Filesystem Access**
   - Audit codebase to ensure no runtime filesystem access remains.
   - All reads/writes must use SQLite DB.

10. **Testing & Validation**
   - Update and run all tests to validate new asset pipeline and HTTP server.
   - Ensure zero warnings (C99) and passing lint.

11. **Documentation**
   - Update `AGENTS.md`, `README.md`, and `DOCS/` to reflect new architecture and build pipeline.

---

### Key Design Decisions & Technical Notes

#### HTTP Server Migration (CivetWeb → EWS)

**Why EWS?**
- srv: Rejected - standalone binary, not embeddable library
- libmicrohttpd: Rejected - LGPL license (copyleft, violates HBF policy)
- EWS: Selected - BSD 2-clause, single header, embeddable, no dependencies

**EWS Architectural Constraints:**
1. **Blocking Server Start**: `acceptConnectionsUntilStopped()` blocks the calling thread
   - Solution: main.c will need to call this in main thread or spawn background thread
2. **Single Callback Model**: Only one `createResponseForRequest()` callback for all routes
   - Solution: Implemented manual routing inside callback (path prefix matching)
3. **No Built-in Context Passing**: Callback doesn't support user data pointer
   - Solution: Using global `g_server_instance` with mutex protection
4. **Thread-per-Connection**: EWS spawns thread for each connection
   - Implication: QuickJS mutex in handler.c remains critical for malloc contention

**EWS Response Model:**
- Must return `struct Response*` allocated with EWS functions (`responseAlloc()`, etc.)
- Cannot return NULL except for manual connection handling
- Response body uses EWS's `heapString` type (auto-managed memory)

**Files Modified:**
- `hbf/http/server.h`: Changed `struct mg_context *ctx` → `struct Server *ews_server`
- `hbf/http/server.c`: Complete rewrite (~280 lines)
- `hbf/http/handler.h`: Changed signature from `mg_connection` → `EWS Request/Response`
- `hbf/http/handler.c`: **IN PROGRESS** - needs complete port to EWS types

**Still TODO for EWS Integration:**
- Port QuickJS bindings (`request.c`, `response.c`) from CivetWeb to EWS structs
- Update BUILD.bazel dependencies
- Handle EWS threading model in main.c
- Test with existing pod JavaScript code

---

### Notes
- Only MIT/BSD/Apache-2.0/Public Domain dependencies allowed.
- No C++ code from miniz; use C99 single-file only.
- No runtime filesystem access; SQLite DB is the sole data store.
- All asset embedding and migration must be automated in Bazel build.

---

## End of Plan
