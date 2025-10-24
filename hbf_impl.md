# HBF Implementation Guide (C99 + Bazel)

**Last Updated**: 2025-10-24 **Current Status**: Database Migration to SQLAR Complete ✅

This document provides a pragmatic, phased plan to build HBF: a single,
statically linked C99 web compute environment that embeds SQLite as the
application/data store, CivetWeb for HTTP + WebSockets, and QuickJS-NG for
runtime extensibility.

**Design Philosophy**: Single-instance deployment (localhost or single k3s pod)
with programmable routing via JavaScript. All HTTP requests handled by
user-defined `server.js` loaded from SQLite database.

**Recent Major Change**: Migrated from multi-tenant pod system to single-user
SQLAR-based architecture. Static files embedded in SQLite Archive at build time.

## Core Architecture

- **Language**: Strict C99 (no C++)
- **Binary**: Single statically linked executable with musl toolchain (100%
  static, no glibc)
- **HTTP**: CivetWeb (HTTP only, no HTTPS in binary), WebSockets supported
- **DB**: Two SQLite databases - `fs.db` (embedded SQLAR archive, read-only) and
  `hbf.db` (runtime data, read-write)
- **Scripting**: QuickJS-NG embedded, sandboxed (memory/timeout limits)
- **Programmable Routing**: Express.js-compatible API in JavaScript (`server.js`
  from fs.db archive)
- **Per-Request Isolation**: Each HTTP request creates fresh JSRuntime +
  JSContext (no reuse)
- **Static Content**: Embedded in fs.db SQLAR archive at build time, served via
  sqlite3_deserialize API
- **Build**: Bazel 8 with bzlmod, vendored third_party sources
- **Licensing**: MIT/BSD/Apache-2/PD only (NO GPL)

## Implementation Phases Overview

- ✅ **Phase 0**: Foundation (Bazel setup, musl toolchain, coding standards,
  DNS-safe hash)
- ✅ **Phase 1**: HTTP Server Bootstrap (CivetWeb, logging, CLI, signal handling)
- ✅ **Phase 2a**: SQLite Integration & Database Schema (document-graph, system
  tables, FTS5) - DEPRECATED, replaced by SQLAR
- ✅ **Phase 2b**: User Pod & Connection Management (connection caching, LRU
  eviction) - DEPRECATED, removed multi-tenancy
- ✅ **Phase 2c**: **Database Migration to SQLAR** (db_plan.md implementation)
  - Removed multi-user pod system (henv/)
  - Implemented SQLAR-based filesystem embedding
  - Two-database architecture (fs.db embedded + hbf.db runtime)
  - Fixed segfault in sqlite3_deserialize cleanup
- ✅ **Phase 3.0**: **Per-Request Context Isolation Fix** (Critical QuickJS
  runtime fix)
- 🔄 **Phase 3.1**: Request/Response Bindings (NEXT - 2-3 days)
- 🔄 **Phase 3.2**: Enhanced Router & Static Content (2-3 days)
- 🔄 **Phase 4**: Authentication & Authorization (1 week)
- 🔄 **Phase 5**: Document Store & Search (3-4 days)
- 🔄 **Phase 6**: Templates & Node Compatibility (1 week)
- 🔄 **Phase 7**: REST API & Admin UI (1 week)
- 🔄 **Phase 8**: WebSockets (3-4 days)
- 🔄 **Phase 9**: Packaging & Optimization (1 week)
- 🔄 **Phase 10**: Hardening & Performance (1 week)

---

## Phase 0: Foundation & Constraints ✅ COMPLETE

### Status
**Completed**: 2025-10-15 **Binary Size**: 1.1 MB (stripped, statically linked)

### Deliverables
- ✅ Directory structure established
- ✅ DNS-safe hash generator (`internal/core/hash.c|h`) - SHA-256 → base36 (8
  chars)
- ✅ MODULE.bazel (bzlmod, Bazel 8) configured
- ✅ musl toolchain setup (100% static linking)
- ✅ Coding standards documented (`DOCS/coding-standards.md`)
- ✅ clang-format and clang-tidy configurations

### Directory Structure
```
/third_party/
  civetweb/          ✅ MIT - CivetWeb v1.16
  sqlite3/           ✅ Public Domain - SQLite 3.50.4
  quickjs-ng/        ✅ MIT - QuickJS-NG (embedded)

/internal/
  core/              ✅ Logging, config, CLI, hash generator, main
  http/              ✅ CivetWeb server wrapper, simplified single-user handler
  db/                ✅ SQLite wrapper, SQLAR file reading (fs.db + hbf.db)
  qjs/               ✅ QuickJS-NG engine, bindings, modules
    bindings/        ✅ Request, response, db, console modules

/fs/                 ✅ Build-time filesystem (embedded into fs.db SQLAR)
  hbf/               ✅ JavaScript runtime files
    server.js        ✅ Express-style routing script (placeholder)
  static/            ✅ Static web content
    index.html       ✅ Default landing page
    style.css        ✅ Basic CSS

/tools/
  lint.sh            ✅ clang-tidy wrapper
  create_fs_archive.sh ✅ Convert fs/ to SQLite SQLAR archive
  db_to_c.sh         ✅ SQLite database to C byte array converter

/DOCS/               ✅ Documentation
  coding-standards.md
  development-setup.md
  phase0-completion.md
  phase1-completion.md
  phase2b-completion.md
  revised_phase3_plan.md
```

### Tests
- ✅ `internal/core/hash_test.c` - 5 test cases
- ✅ All tests passing

---

## Phase 1: HTTP Server Bootstrap ✅ COMPLETE

### Status
**Completed**: 2025-10-16

### Deliverables
- ✅ `internal/core/config.c|h` - CLI parsing (port, log_level, storage_dir, dev
mode) - ✅ `internal/core/log.c|h` - Logging with levels (DEBUG, INFO, WARN,
ERROR) and UTC timestamps - ✅ `internal/http/server.c|h` - CivetWeb lifecycle
and callbacks
- ✅ `GET /health` - Returns JSON health status
- ✅ Signal handling (SIGINT, SIGTERM)

### CLI Arguments
```bash
hbf [options]
  --port <num>         HTTP server port (default: 5309)
  --storage_dir <path> Directory for user pod storage (default: ./henvs)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show help message
```

### Tests
- ✅ `internal/core/config_test.c` - 25 test cases

---

## Phase 2a: SQLite Integration & Database Schema ✅ COMPLETE

### Status
**Completed**: 2025-10-18

### Deliverables
- ✅ SQLite3 integration from Bazel Central Registry (v3.50.4)
- ✅ `internal/db/db.c|h` - Database wrapper (open, close, execute, query) - ✅
`internal/db/schema.c|h` - Schema initialization with embedded SQL
- ✅ `internal/db/schema.sql` - Complete schema definition

### Database Schema (11 Tables)

**Document-Graph Core Tables**:
- ✅ `nodes` - Document-graph nodes with JSON body (type, name, content)
- ✅ `edges` - Directed relationships between nodes
- ✅ `tags` - Hierarchical labeling system
- ✅ `nodes_fts` - FTS5 full-text search with porter stemming

**System Tables** (`_hbf_*` prefix):
- ✅ `_hbf_users` - User accounts with Argon2id password hashes (schema ready)
- ✅ `_hbf_sessions` - JWT session tracking with token hashes
- ✅ `_hbf_table_permissions` - Table-level access control
- ✅ `_hbf_row_policies` - Row-level security policies
- ✅ `_hbf_config` - Per-environment configuration
- ✅ `_hbf_audit_log` - Audit trail for admin actions
- ✅ `_hbf_schema_version` - Schema versioning

### SQLite Configuration
**Compile-time flags**:
```c
-DSQLITE_THREADSAFE=0
-DSQLITE_ENABLE_FTS5
-DSQLITE_OMIT_LOAD_EXTENSION
-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
-DSQLITE_ENABLE_JSON1
```

**Runtime pragmas**:
```sql
PRAGMA foreign_keys=ON;
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

### Tests
- ✅ `internal/db/db_test.c` - 7 test cases
- ✅ `internal/db/schema_test.c` - 9 test cases

### Deprecation Note
**Phase 2a schema has been superseded by Phase 2c (SQLAR migration)**. The
document-graph schema and system tables have been removed. Future phases will
reintroduce necessary tables as needed for the single-user architecture.

---

## Phase 2b: User Pod & Connection Management ✅ COMPLETE

### Status
**Completed**: 2025-10-19

### Deliverables
- ✅ `internal/henv/manager.c|h` - User pod lifecycle management
- ✅ Connection caching with LRU eviction (max 100 connections)
- ✅ Thread-safe connection pool (mutex-protected)
- ✅ Automatic schema initialization on pod creation
- ✅ Secure file permissions (directories 0700, databases 0600)
- ✅ Hash collision detection

### Architecture
- **Storage**: `{storage_dir}/{hash}/index.db`
- **Default DB**: Time-based hash `{storage_dir}/{hash(timestamp)}/index.db`
  (unique per invocation)
- **Connection Limit**: 100 cached connections (hardcoded in Phase 2b,
  configurable in Phase 9)
- **Eviction**: LRU (Least Recently Used)

### Tests
- ✅ `internal/henv/manager_test.c` - 12 test cases

### Deprecation Note
**Phase 2b has been superseded by Phase 2c (SQLAR migration)**. The multi-tenant
pod system has been removed in favor of a simpler single-user architecture.

---

## Phase 2c: Database Migration to SQLAR ✅ COMPLETE

### Status
**Completed**: 2025-10-24 **Major Refactor**: Migrated from schema-based
multi-tenant system to SQLAR-based single-user architecture

### Problem Statement
The previous architecture (Phase 2a/2b) implemented a complex multi-tenant pod
system with per-user database isolation. This added unnecessary complexity for
the initial single-instance deployment model. The db_plan.md proposed a
simplified architecture using SQLite Archive (SQLAR) for embedded static files.

### Architecture Change

#### Before (Phase 2a/2b)
- Multi-user pod system (`internal/henv/`)
- Schema-based database with nodes/edges/tags tables
- Per-user database isolation with connection caching
- Complex LRU eviction logic
- Static content planned for database nodes table

#### After (Phase 2c)
- Single-user architecture
- Two-database model:
  - `fs.db` - Embedded SQLAR archive (read-only, in-memory via
    sqlite3_deserialize)
  - `hbf.db` or `:memory:` - Runtime application data (read-write)
- Static files embedded at build time
- Simplified HTTP server (182 lines, down from 329)
- Simplified main.c (80 lines, down from 202)

### Deliverables

#### 1. Filesystem Build System (`/fs/`)
- ✅ `fs/hbf/server.js` - JavaScript routing script (placeholder)
- ✅ `fs/static/index.html` - Default landing page
- ✅ `fs/static/style.css` - Basic CSS
- ✅ `fs/BUILD.bazel` - Bazel rules for SQLAR generation
- ✅ `fs/fs_build_test.sh` - Verification script

#### 2. Build Tools (`/tools/`)
- ✅ `tools/create_fs_archive.sh` - Converts fs/ directory to SQLite SQLAR
  archive
- ✅ `tools/db_to_c.sh` - Converts SQLite database to C byte array (fs_embedded.c/h)

#### 3. Database API (`internal/db/`)
- ✅ Complete rewrite of `db.h` and `db.c`
- ✅ `hbf_db_init(inmem, &db, &fs_db)` - Initialize both databases
- ✅ `hbf_db_close(db, fs_db)` - Clean shutdown
- ✅ `hbf_db_read_file(fs_db, path, &data, &size)` - Read from SQLAR archive
- ✅ `hbf_db_file_exists(fs_db, path)` - Check file existence
- ✅ Uses `sqlite3_deserialize()` API for in-memory fs.db
- ✅ Fixed segfault: Use `sqlite3_malloc64()` for FREEONCLOSE compatibility

#### 4. HTTP Server Simplification (`internal/http/`)
- ✅ Removed thread-local storage complexity
- ✅ Removed henv dependencies
- ✅ Direct SQLAR file serving
- ✅ Simplified from 329 to 182 lines (44% reduction)

#### 5. Configuration Updates (`internal/core/`)
- ✅ Removed `--storage_dir` CLI flag
- ✅ Added `--inmem` flag for in-memory database mode
- ✅ Simplified config struct (removed storage_dir field)
- ✅ Updated main.c (202 → 80 lines, 60% reduction)

#### 6. QuickJS Engine Updates (`internal/qjs/`)
- ✅ Removed dependency on old `hbf_db_open()` function
- ✅ Engine uses `sqlite3_open()` directly for test databases
- ✅ Engine only receives runtime db handle (no fs_db knowledge)

### Critical Bug Fixes

#### Segfault in sqlite3_deserialize Cleanup
**Symptom**: Core dump on Ctrl+C shutdown
**Root Cause**: Memory allocated with `malloc()` was freed by SQLite's internal
allocator when using `SQLITE_DESERIALIZE_FREEONCLOSE` flag
**Fix**: Changed to use SQLite's memory allocator
```c
// internal/db/db.c lines 70-100
fs_data_copy = sqlite3_malloc64(fs_db_len);  // Was: malloc()
// ... use memory ...
sqlite3_free(fs_data_copy);  // Was: free()
```

### Removed Components
- ❌ `internal/henv/` - Entire directory (multi-user pod management)
- ❌ `internal/db/schema.sql` - Schema-based tables
- ❌ `internal/db/schema.c|h` - Schema initialization
- ❌ `internal/db/schema_test.c` - Schema tests
- ❌ `--storage_dir` CLI flag

### Tests
- ✅ `//fs:fs_build_test` - SQLAR archive verification (8 files)
- ✅ `//internal/db:db_test` - Database operations (5 test cases, no segfault)
- ✅ `//internal/core:config_test` - Updated for new CLI flags (7 test cases)
- ✅ Manual shutdown test - No core dumps on SIGINT

### Build Verification
```bash
bazel build //:hbf           # ✅ PASSED
bazel test //...             # ✅ 6/6 tests PASSED
bazel-bin/hbf --inmem        # ✅ Runs without segfault
# Ctrl+C shutdown             # ✅ Clean shutdown, no core dump
```

### Binary Size
- **Current**: 1.1 MB (stripped, statically linked)
- **Embedded fs.db**: 3,584 bytes (8 files)

---

## Phase 3.0: Per-Request Context Isolation Fix ✅ COMPLETE

### Status
**Completed**: 2025-10-23 **Critical Fix**: Resolved persistent "Maximum call
stack size exceeded" error

### Problem Identified
1. **Stack Overflow**: Global context reuse caused stack buildup across requests
2. **Segfault**: Static class ID reuse across different JSRuntimes caused
   crashes after 2-3 requests

### Root Cause
- **Issue 1**: Global `JSContext` was shared across all HTTP requests, causing
  stack frames to accumulate
- **Issue 2**: `hbf_response_class_id` (static global) was initialized once for
  first runtime, then reused for subsequent runtimes where it was invalid

### Solution Implemented

#### 1. Per-Request Runtime + Context (`internal/http/handler.c`)
```c
/* CRITICAL: Create a fresh JSRuntime + JSContext for each request.
 * This prevents stack overflow errors and ensures complete isolation.
 * DO NOT reuse contexts between requests - this is a QuickJS best practice.
 */
qjs_ctx = hbf_qjs_ctx_create_with_db(g_default_db);
// ... handle request ...
hbf_qjs_ctx_destroy(qjs_ctx);  // Destroy after each request
```

**Key Changes**:
- Each HTTP request creates new `JSRuntime` + `JSContext`
- Loads `server.js` from database into fresh context
- Handles request with complete isolation
- Destroys both runtime and context after response
- Removed mutex locking (no longer needed with isolated contexts)

#### 2. Removed Global Context (`internal/core/main.c`)
```c
// Before: Created global context at startup
// g_qjs_ctx = hbf_qjs_ctx_create_with_db(g_default_db);

// After: Only verify server.js exists, load per-request
hbf_log_info("Found server.js (%d bytes) - will load per-request", code_len);
```

**Key Changes**:
- Removed `g_qjs_ctx` global variable
- Removed `g_qjs_mutex` global mutex
- Engine initialization only sets global config (memory limit, timeout)
- `server.js` verified at startup but loaded per-request

#### 3. Fixed Class ID Registration (`internal/qjs/bindings/response.c`)
```c
/*
 * Initialize response class for a new JSRuntime.
 * IMPORTANT: Must be called for EACH new runtime when using per-request runtimes.
 * Class IDs are runtime-specific and cannot be shared across runtimes.
 */
void hbf_qjs_init_response_class(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* Always reset and re-register the class for the new runtime */
    hbf_response_class_id = 0;
    JS_NewClassID(rt, &hbf_response_class_id);

    JSClassDef response_class_def = {
        .class_name = "HbfResponse",
        .finalizer = NULL,
    };

    JS_NewClass(rt, hbf_response_class_id, &response_class_def);
}
```

**Key Changes**:
- Always reset `hbf_response_class_id = 0` for each new runtime
- Re-register class for every new runtime (class IDs are runtime-specific)
- Removed conditional check that prevented re-initialization
- Added extensive documentation

### Testing Results
✅ **All issues resolved**:
- No stack overflow errors under load
- No segfaults after multiple requests
- Successfully handles 6+ sequential requests
- Both C endpoints (`/health`) and JS endpoints (`/hellojs`, `/`) work
- All unit tests pass (7/7 test suites)

### Core Dump Analysis
Used GDB with unstripped binary (`bazel-bin/internal/core/hbf`) to identify:
```
#0  JS_SetPropertyInternal2()      <- QuickJS internal (crash point)
#1  JS_SetPropertyStr()             <- QuickJS API
#2  hbf_qjs_create_response()      <- Our code (using stale class ID)
#3  hbf_qjs_request_handler()      <- HTTP handler
```

**Diagnosis**: Stale class ID from destroyed runtime was being used in new
runtime, causing invalid memory access.

### Documentation Added
- Code comments in `handler.c` explaining per-request isolation requirement
- Code comments in `response.c` explaining class ID registration requirement
- Warning comments to prevent future regressions

### Deliverables
- ✅ `internal/http/handler.c` - Per-request context creation
- ✅ `internal/core/main.c` - Removed global context
- ✅ `internal/qjs/bindings/response.c` - Fixed class registration
- ✅ `internal/http/BUILD.bazel` - Added SQLite dependency
- ✅ All tests passing
- ✅ Core dump analysis documented

### Performance Notes
- **Per-request overhead**: ~0.5-1ms for runtime/context creation (acceptable
  for web requests)
- **Memory**: Each runtime uses ~64MB limit (configurable)
- **Concurrency**: CivetWeb handles multi-threading; each thread gets isolated
  JS execution
- **Future optimization**: Could cache compiled bytecode if needed (Phase 10)

---

## Phase 3.1: Request/Response Bindings (NEXT - 2-3 Days) 🔄

### Status
**In Progress**: Partially implemented **Target Completion**: 2-3 days

### Current State
- ✅ `internal/qjs/bindings/request.c|h` - Basic request object (method, path,
headers) - ✅ `internal/qjs/bindings/response.c|h` - Basic response object
(status, send, json, set)
- ✅ Request/response objects created and passed to JS
- ⚠️ Missing: Request body parsing (POST/PUT)
- ⚠️ Missing: Response redirect
- ⚠️ Missing: Comprehensive tests

### Goals
- Complete POST/PUT body parsing (JSON only initially)
- Add `res.redirect(url)` support
- Add query parameter parsing to request object
- Write comprehensive unit tests for bindings
- End-to-end integration tests

### Tasks Breakdown

#### Day 1: Request Body Parsing
1. **Implement POST/PUT body reading** (`internal/qjs/bindings/request.c`):
   - Use CivetWeb's `mg_read()` API to read request body
   - Parse JSON body (use QuickJS JSON parser)
   - Set `req.body` property with parsed object
   - Handle parse errors gracefully (return 400 Bad Request)
   - Add Content-Length validation

2. **Add query parameter parsing**:
   - Parse URL query string (`?key=value&foo=bar`)
   - Populate `req.query` object
   - Handle URL decoding
   - Support array parameters (`?id=1&id=2` → `req.query.id = [1, 2]`)

3. **Write tests**:
   - Test JSON body parsing (valid/invalid JSON)
   - Test query parameter parsing
   - Test Content-Length handling
   - Test request with no body

#### Day 2: Response Enhancements
1. **Implement `res.redirect()`** (`internal/qjs/bindings/response.c`):
   - Set Location header
   - Set status code (302 by default, or custom)
   - Mark response as sent
   - Example: `res.redirect('/login')` or `res.redirect(301, '/new-url')`

2. **Add response status text**:
   - Map status codes to standard text (200 → "OK", 404 → "Not Found")
   - Include in HTTP response line

3. **Add Content-Type helpers**:
   - `res.type(type)` - Set Content-Type header
   - Common shortcuts: `res.json()`, `res.html()`, `res.text()`

4. **Write tests**:
   - Test redirect with various status codes
   - Test Content-Type setting
   - Test response chaining
   - Test error cases (sending twice)

#### Day 3: Integration & Testing
1. **Integration tests** (`internal/qjs/engine_test.c` or new file):
   - End-to-end request → JS → response cycle
   - Test with actual server.js
   - Test error handling (JS exceptions → 500)
   - Test timeout handling

2. **HTTP integration tests**:
   - Create test script that starts server and makes real HTTP requests
   - Test POST with JSON body
   - Test query parameters
   - Test redirects
   - Test error responses

3. **Documentation**:
   - Update code comments
   - Document request/response API
   - Add examples to DOCS/

### Success Criteria
- ✅ POST/PUT requests can send JSON body
- ✅ Request body is accessible as `req.body` in JavaScript
- ✅ Query parameters accessible as `req.query`
- ✅ `res.redirect()` works correctly
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ No memory leaks (verify with valgrind if needed)

### Deliverables
- Enhanced `internal/qjs/bindings/request.c` with body parsing
- Enhanced `internal/qjs/bindings/response.c` with redirect support
- Comprehensive test suite
- Documentation updates

---

## Phase 3.2: Enhanced Router & Static Content (2-3 Days) 🔄

### Status
**Not Started** **Target Completion**: 2-3 days after Phase 3.1

### Current State
- ✅ `static/server.js` - Minimal routing script in database
- ✅ `tools/inject_content.sh` - Static content injection tool
- ⚠️ Router is minimal (only handles `/hellojs`)
- ⚠️ No static file serving
- ⚠️ No middleware chain

### Goals
- Create full Express.js-compatible router in `static/lib/router.js`
- Implement static file serving middleware
- Support middleware chains with `next()`
- Error handling middleware
- Load all static content from database

### Tasks Breakdown

#### Day 1: Router Implementation
1. **Create `static/lib/router.js`**:
   - Express.js-compatible routing API
   - Route registration: `app.get()`, `app.post()`, `app.put()`, `app.delete()`
   - Middleware support: `app.use()`
   - Parameter extraction: `/user/:id` → `req.params.id`
   - Route matching with wildcards
   - Middleware chain with `next()` function

2. **Implement route matching**:
   - Pattern matching for paths (`/user/:id`, `/post/:slug`)
   - Support for wildcards (`/api/*`)
   - Route priority (specific routes before wildcards)
   - Case-insensitive matching (optional)

3. **Test router**:
   - Test route registration
   - Test parameter extraction
   - Test middleware chain
   - Test route priority

#### Day 2: Static File Serving
1. **Create static file middleware**:
   - Query database for static files
   - Content-Type detection based on file extension
   - Caching headers (ETag, Last-Modified)
   - 404 handling for missing files

2. **Update `tools/inject_content.sh`**:
   - Scan `static/www/` directory
   - Generate SQL INSERT statements
   - Proper JSON encoding
   - File type detection
   - MIME type mapping

3. **Build integration**:
   - Update `internal/db/BUILD.bazel` to run inject_content.sh
   - Inject static content at build time
   - Include in schema initialization

4. **Test static serving**:
   - Test serving HTML files
   - Test serving CSS files
   - Test serving JavaScript files
   - Test 404 for missing files
   - Test Content-Type headers

#### Day 3: Error Handling & Middleware
1. **Implement error handling**:
   - Error middleware: `app.use((err, req, res, next) => {})`
   - Default 500 error handler
   - JavaScript exception → HTTP 500
   - Stack trace in development mode

2. **Middleware enhancements**:
   - Route-specific middleware: `app.get('/path', middleware, handler)`
   - Multiple middleware per route
   - Middleware can modify req/res
   - Early response termination

3. **Update `server.js`**:
   - Use router.js for routing
   - Add static file middleware
   - Add error handling middleware
   - Example routes for demonstration

4. **Integration testing**:
   - Test full request/response cycle
   - Test error handling
   - Test middleware chain
   - Test static file serving

### Success Criteria
- ✅ Express.js-compatible router works
- ✅ Parameter extraction works (`/user/:id`)
- ✅ Middleware chains work with `next()`
- ✅ Static files served from database
- ✅ Error handling catches JS exceptions
- ✅ All tests pass

### Deliverables
- `static/lib/router.js` - Full router implementation
- Updated `static/server.js` - Using router and middleware
- Updated `tools/inject_content.sh` - Working content injection
- Static file serving middleware
- Error handling middleware
- Comprehensive tests
- Documentation

---

## Phase 4: Authentication & Authorization (1 Week) 🔄

### Status
**Not Started** **Target Completion**: 1 week after Phase 3.2

### Subtasks

#### Phase 4.1: Password Hashing & JWT (2-3 Days)
1. **Vendor Argon2** (`third_party/argon2/`):
   - Reference implementation (Apache-2.0)
   - Bazel build integration
   - C wrapper: `hbf_argon2_hash()`, `hbf_argon2_verify()`
   - Constant-time comparison

2. **JWT Implementation** (in-house, no external libs):
   - SHA-256 HMAC (vendor single-file MIT implementation)
   - Base64url encoding/decoding
   - JWT structure: header.payload.signature
   - Functions: `hbf_jwt_sign()`, `hbf_jwt_verify()`
   - Token expiry validation

3. **Session Management** (`internal/auth/sessions.c|h`):
   - Session creation on login
   - Session lookup by token hash (SHA-256)
   - Session expiry/cleanup
   - Revocation support

4. **API Endpoints**:
   - `POST /register` - Create user with Argon2id hash
   - `POST /login` - Verify password, return JWT
   - `POST /logout` - Invalidate session
   - `GET /me` - Get current user info (requires auth)

5. **Tests**:
   - Test Argon2 hashing/verification
   - Test JWT signing/verification
   - Test session management
   - Test authentication endpoints

#### Phase 4.2: Authorization (2-3 Days)
1. **Table-level permissions** (`internal/authz/table_perms.c|h`):
   - Read `_hbf_table_permissions` table
   - Check permissions before query execution
   - Admin-only endpoints
   - User-based access control

2. **Row-level security** (`internal/authz/row_policies.c|h`):
   - Read `_hbf_row_policies` table
   - Inject WHERE clauses into queries
   - Per-user data isolation
   - Policy evaluation

3. **Auth Middleware** (JavaScript):
   - Check JWT in Authorization header
   - Validate token
   - Set `req.user` from token payload
   - Protect routes with middleware

4. **Tests**:
   - Test table permissions
   - Test row policies
   - Test auth middleware
   - Test unauthorized access

### Deliverables
- `internal/auth/argon2.c|h` - Password hashing - `internal/auth/jwt.c|h` - JWT
implementation - `internal/auth/sessions.c|h` - Session management -
`internal/authz/table_perms.c|h` - Table permissions -
`internal/authz/row_policies.c|h` - Row-level security
- Auth/authz middleware in JavaScript
- Authentication endpoints
- Comprehensive tests
- Documentation

---

## Phase 5: Document Store & Search (3-4 Days) 🔄

### Status
**Not Started**

### Tasks
1. **Vendor simple-graph** (`third_party/simple_graph/`):
   - MIT-licensed graph library
   - Bazel integration
   - Wrapper for document CRUD

2. **Document API** (`internal/document/`):
   - `GET /api/documents` - List documents
   - `GET /api/documents/:id` - Get document
   - `POST /api/documents` - Create document
   - `PUT /api/documents/:id` - Update document
   - `DELETE /api/documents/:id` - Delete document

3. **FTS5 Search**:
   - Index documents in `nodes_fts` table
   - Search API: `GET /api/documents/search?q=query`
   - Ranking and snippet generation
   - Tag-based filtering

4. **Tests**:
   - Test document CRUD operations
   - Test full-text search
   - Test tag management
   - Test search ranking

### Deliverables
- Document API endpoints
- Full-text search implementation
- Tag management
- Comprehensive tests
- Documentation

---

## Phase 6: Templates & Node Compatibility (1 Week) 🔄

### Status
**Not Started**

### Subtasks

#### Phase 6.1: EJS Templates (2-3 Days)
1. **Vendor EJS or implement minimal version**
2. **Template rendering** (`internal/templates/`):
   - Load templates from database
   - Render with data
   - Caching for performance
3. **JavaScript integration**:
   - `res.render(template, data)` function
   - Template helpers

#### Phase 6.2: Node.js Compatibility (2-3 Days)
1. **CommonJS module loader**:
   - `require()` function
   - Load modules from database
   - Module caching
2. **Node.js API shims**:
   - `process` global
   - `Buffer` class
   - `fs` module (backed by SQLite)
   - Minimal path/util modules
3. **NPM module vendoring strategy**

### Deliverables
- EJS template rendering
- CommonJS module system
- Node.js API shims
- Documentation

---

## Phase 7: REST API & Admin UI (1 Week) 🔄

### Status
**Not Started**

### Tasks
1. **Admin API endpoints**:
   - `GET /_api/users` - List users (admin only)
   - `POST /_api/users` - Create user (admin only)
   - `GET /_api/permissions` - List permissions
   - `POST /_api/permissions` - Update permissions
   - `GET /_api/audit` - Audit log

2. **Monaco Editor UI**:
   - `GET /_admin` - Admin dashboard
   - Code editor for JavaScript
   - Database browser
   - Log viewer

3. **System metrics**:
   - `GET /_api/metrics` - System stats
   - Connection pool status
   - Memory usage
   - Request statistics

### Deliverables
- Admin API endpoints
- Admin web UI
- System metrics
- Documentation

---

## Phase 8: WebSockets (3-4 Days) 🔄

### Status
**Not Started**

### Tasks
1. **WebSocket handler** (`internal/ws/`):
   - CivetWeb WebSocket integration
   - Connection management
   - Message routing

2. **JavaScript API**:
   - `ws.send()` function
   - `ws.on('message', callback)`
   - Connection events

3. **Channel support**:
   - `/ws/:channel` routing
   - Broadcast messages
   - Private messages

### Deliverables
- WebSocket handler
- JavaScript WebSocket API
- Channel support
- Tests and documentation

---

## Phase 9: Packaging & Optimization (1 Week) 🔄

### Status
**Not Started**

### Tasks
1. **Make connection limit configurable**:
   - Add `--db_max_open` CLI argument
   - Currently hardcoded to 100 in Phase 2b

2. **Binary optimization**:
   - Strip unused symbols
   - Dead code elimination
   - Size optimization

3. **Configuration options**:
   - Environment variable support
   - Configuration file support
   - Runtime configuration API

4. **Deployment packaging**:
   - Docker container (optional)
   - Systemd service file
   - Installation script

### Deliverables
- Optimized binary
- Configuration system
- Deployment packages
- Installation documentation

---

## Phase 10: Hardening & Performance (1 Week) 🔄

### Status
**Not Started**

### Tasks
1. **Security hardening**:
   - Input validation
   - SQL injection prevention
   - XSS prevention
   - Rate limiting

2. **Performance tuning**:
   - Connection pool optimization
   - Query optimization
   - Caching strategies
   - Benchmarking

3. **Memory leak detection**:
   - Valgrind analysis
   - AddressSanitizer (ASan)
   - Fix any leaks

4. **Load testing**:
   - Stress testing with many concurrent requests
   - Memory usage under load
   - Performance profiling

### Deliverables
- Hardened security
- Performance optimizations
- Memory leak fixes
- Load testing results
- Performance documentation

---

## Current Implementation Status

### Completed Components ✅

**Core Infrastructure**:
- ✅ Bazel 8 build system with bzlmod
- ✅ musl toolchain (100% static linking)
- ✅ Coding standards (CERT C, Linux kernel style)
- ✅ DNS-safe hash generator (SHA-256 → base36)
- ✅ Logging system (4 levels, UTC timestamps)
- ✅ CLI argument parsing
- ✅ Signal handling (graceful shutdown)

**HTTP Server**:
- ✅ CivetWeb integration
- ✅ Request routing
- ✅ `GET /health` endpoint
- ✅ Multi-threaded request handling
- ✅ Per-request JS context isolation (Phase 3.0 fix)

**Database**:
- ✅ SQLite3 integration (v3.50.4)
- ✅ Complete schema (11 tables)
- ✅ WAL mode + FTS5
- ✅ Connection caching (LRU, max 100)
- ✅ Thread-safe access
- ✅ User pod management

**JavaScript Runtime**:
- ✅ QuickJS-NG integration
- ✅ Per-request runtime + context creation
- ✅ Memory limits (64 MB default)
- ✅ Execution timeouts (5000ms default)
- ✅ Database module (`db.query()`, `db.execute()`)
- ✅ Console module (`console.log/warn/error/debug`)
- ✅ Request/response bindings (basic)
- ✅ server.js loaded from database

**Static Content**:
- ✅ `tools/inject_content.sh` - Content injection
- ✅ `static/server.js` - Minimal routing script
- ✅ Content stored in database

**Tests**:
- ✅ 7 test suites (53+ test cases total)
- ✅ All tests passing
- ✅ Build verification (no warnings with `-Werror`)
- ✅ Lint checks (clang-tidy CERT rules)

**Binary**:
- ✅ Size: 1.1 MB (stripped, statically linked)
- ✅ Zero runtime dependencies
- ✅ Fully static with musl libc 1.2.3

### Next Steps (Immediate Focus)

**Week 1-2: Phase 3.1 - Request/Response Bindings**
- Day 1-2: POST/PUT body parsing
- Day 3-4: Response enhancements (redirect, etc.)
- Day 5-6: Integration testing

**Week 3-4: Phase 3.2 - Enhanced Router & Static Content**
- Day 1-2: Router implementation
- Day 3-4: Static file serving
- Day 5-6: Error handling and middleware

**Week 5-6: Phase 4 - Authentication & Authorization**
- Week 5: Password hashing, JWT, sessions
- Week 6: Table/row permissions, auth middleware

### Quality Gates (All Passing) ✅

```bash
# Build
$ bazel build //:hbf
✅ Build completed successfully (1.1 MB binary)

# Test
$ bazel test //...
✅ 7 test suites, 53+ test cases, all passing

# Lint
$ bazel run //:lint
✅ 8 source files checked, 0 issues
```

---

## Critical Design Decisions

### 1. Per-Request Context Isolation (Phase 3.0)
**Decision**: Create fresh JSRuntime + JSContext for each HTTP request.

**Rationale**:
- Prevents stack overflow from context reuse
- Ensures complete isolation between requests
- Matches QuickJS best practices
- Prevents memory/state leaks between requests
- Class IDs are runtime-specific and cannot be shared

**Trade-offs**:
- ~0.5-1ms overhead per request (acceptable)
- Higher memory usage during concurrent requests
- Cannot share compiled bytecode between requests (could optimize later)

### 2. Single-Instance Deployment
**Decision**: Design for single-instance deployment initially (localhost or
single k3s pod).

**Rationale**:
- Simpler architecture
- No distributed state management
- Easier to debug and maintain
- Can scale later with multiple instances + load balancer

**Trade-offs**:
- Limited horizontal scalability initially
- Single point of failure (mitigated by container orchestration)

### 3. Database-Backed Routing
**Decision**: Load `server.js` from SQLite database, not filesystem.

**Rationale**:
- Dynamic routing without recompilation
- Version control in database
- Hot-reload capability (future)
- Single source of truth

**Trade-offs**:
- Slightly slower startup (acceptable)
- Database becomes critical dependency

### 4. No HTTPS in Binary
**Decision**: Expect reverse proxy (nginx, Traefik, Caddy) for TLS termination.

**Rationale**:
- Smaller binary
- Simpler code
- Standard practice for backend services
- Certificate management handled by proxy

### 5. Static Content in Database
**Decision**: Inject static files into SQLite at build time.

**Rationale**:
- Single deployment artifact
- Version control for all content
- Simplified deployment
- No filesystem dependencies

**Trade-offs**:
- Larger database file
- Build-time injection required
- Binary content needs Base64 encoding

---

## References

- **Implementation Guide**: `hbf_impl.md` (this file)
- **Coding Standards**: `DOCS/coding-standards.md`
- **Development Setup**: `DOCS/development-setup.md`
- **Completion Reports**:
  - `DOCS/phase0-completion.md`
  - `DOCS/phase1-completion.md`
  - `DOCS/phase2b-completion.md`
- **Phase Plans**: `DOCS/revised_phase3_plan.md`
- **Project Instructions**: `CLAUDE.md`

---

## Change Log

- **2025-10-24**: Phase 2c complete - Database migration to SQLAR (major
  refactor)
  - Removed multi-tenant pod system (Phase 2a/2b deprecated)
  - Implemented two-database architecture (fs.db SQLAR + hbf.db runtime)
  - Fixed critical segfault in sqlite3_deserialize cleanup
  - Simplified codebase (HTTP server -44%, main.c -60%)
- **2025-10-23**: Phase 3.0 complete - Per-request context isolation fix (stack
  overflow + segfault resolved)
- **2025-10-19**: Phase 2b complete - User pod & connection management (now
  deprecated)
- **2025-10-18**: Phase 2a complete - SQLite integration & schema (now
  deprecated)
- **2025-10-16**: Phase 1 complete - HTTP server bootstrap
- **2025-10-15**: Phase 0 complete - Foundation established
