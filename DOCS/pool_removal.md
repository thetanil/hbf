# Migration Plan: Remove QuickJS Context Pool and Use Single Context with Exclusive DB Access

Date: 2025-10-22

## Background

Currently, HBF uses a pool of QuickJS contexts (one JSRuntime+JSContext per
thread/request) to handle concurrent HTTP requests. Each context is initialized
independently and does not share state. Scripts and static assets are loaded
from the SQLite database, but the DB pointer is not directly accessible from JS,
and SQLite is not thread-safe for shared connections.

QuickJS-NG documentation recommends using a single JSRuntime per process for
simplicity and safety, especially when embedding. Given our single-instance
deployment and the need for DB-backed routing/static serving, we can simplify by
removing the pool and using one QuickJS context with exclusive access to the
database.

## Goals
- Remove the context pool and mutex/condvar logic.
- Use a single JSRuntime+JSContext for the entire process.
- Give the JS environment direct (but safe) access to the SQLite database pointer.
- Ensure all DB access from JS is serialized and thread-safe (single-threaded model).
- Simplify request handling and resource management.

## Migration Steps

### 1. Remove Pool Implementation
- Delete `internal/qjs/pool.[ch]` and all references to pool functions (`hbf_qjs_pool_init`, `hbf_qjs_pool_acquire`, `hbf_qjs_pool_release`, `hbf_qjs_pool_shutdown`, etc.).
- Update HTTP handler (`internal/http/handler.c`) to use a single global context instead of acquiring/releasing from the pool.
- Remove pool tests and related code.

### 2. Create a Global QuickJS Context
- In the main application startup (e.g., `main.c` or server init), create a single JSRuntime and JSContext.
- Store the SQLite DB pointer as a global (or in a struct alongside the JS context).
- Initialize the context with `router.js` and `server.js` from the database at startup.
- Expose the DB pointer to JS via a host module or global binding (see below).

### 3. Expose Database Access to JS

**Status: Complete ✅**

- The C binding for DB access (`hbf:db` module) is implemented and exposes `db.query(sql, params)` and `db.execute(sql, params)` to JS.
- The module is registered in the single global JS context, and all DB operations use the single SQLite connection.
- Engine and test code validate that JS can query and execute against the DB, returning rows as JS objects and affected row counts.
- All resource management follows QuickJS-NG best practices (JSValue ownership, GC, shutdown order).
- **Fixed:** Previously failing QuickJS assertion was caused by:
  1. Missing `JS_FreeValue()` call on `JS_GetGlobalObject()` return value in `db_module.c`
  2. Missing `JS_SetContextOpaque()` call to make engine context accessible to modules
- All tests now pass with no memory leaks or GC issues.

### 4. Update Router and Server Scripts

**Status: Complete**

- `router.js` remains unchanged - it's pure JS routing logic with no DB dependencies.
- `server.js` updated with DB API examples:
  - `GET /api/nodes` - List nodes using `db.query()`
  - `GET /api/nodes/:id` - Get node by ID with parameter binding
  - `POST /api/nodes` - Create node using `db.execute()`
- All routes properly use the global `db` module with error handling.
- Integration test added to `engine_test.c` verifying router.js + server.js work with DB access.
- No per-request context isolation assumptions - all routes share the single global context and database connection.

### 5. Update HTTP Handler

**Status: Complete ✅**

- HTTP handler (`internal/http/handler.c`) updated to use global `g_qjs_ctx`.
- **Engine modifications**:
  - Added `hbf_qjs_ctx_create_with_db(sqlite3 *db)` to accept external database pointer.
  - Internal refactoring: `hbf_qjs_ctx_create_internal()` handles both owned and external DB.
  - Added `own_db` flag to context struct - only closes DB if owned.
  - `hbf_qjs_ctx_destroy()` respects `own_db` flag.
- **Main.c startup sequence**:
  1. Opens main database (`{storage_dir}/{hash(timestamp)}/index.db` - unique per invocation)
  2. Initializes database schema
  3. Creates global QuickJS context with external DB via `hbf_qjs_ctx_create_with_db()`
  4. Loads `router.js` and `server.js` from database `nodes` table
  5. Starts HTTP server
- **Thread safety**:
  - Added global `pthread_mutex_t g_qjs_mutex` in `main.c`.
  - HTTP handler locks mutex before `JS_Call()` and unlocks after.
  - Single-threaded JS execution model - requests are serialized.
- **Request flow**:
  1. HTTP request arrives → handler acquires mutex
  2. Creates JS `req` and `res` objects
  3. Calls `app.handle(req, res)` in shared context
  4. Releases mutex
  5. Sends HTTP response
- All tests passing (6 suites, 14 functions total).

### 6. Remove Pool-Related Configuration

**Status: Complete ✅** (Nothing to Remove)

- **No pool-related config options were ever implemented** - they were only documented as "Planned (Later Phases)" in CLAUDE.md.
- Verified: No `--qjs_pool_size`, `--qjs_mem_mb`, or `--qjs_timeout_ms` options exist in code.
- **Global limits are hardcoded in main.c**:
  - `hbf_qjs_init(64, 5000)` - 64MB memory limit, 5000ms timeout
  - Applied to single global context (no per-context configuration needed)
- **Updated CLAUDE.md**: Removed planned pool-related options, documented current hardcoded settings.

### 7. Testing and Validation

**Status: Complete ✅**

- All tests for DB access from JS (including error handling and edge cases) are implemented and passing.
- Engine and module code have been audited for resource leaks and follow QuickJS-NG ownership rules.
- Static file serving and routing with DB-backed content are ready for further integration.
- No thread-safety issues observed; SQLite is used in serialized mode.
- **Console module** added and tested - binds console.log/warn/error/debug to HBF logger.
- **Router integration** tested - verified that router.js + server.js can load and execute with DB access.
- All 6 test suites passing with 14 test functions total.

## Considerations
- **Thread Safety**: SQLite must be opened in serialized mode (default for our
  build flags). All DB access must occur on the main thread.
- **Performance**: Single context may limit concurrency, but simplifies resource
  management and avoids subtle bugs.
- **State Management**: All JS globals are shared across requests. Avoid leaking
  sensitive/request-specific data between requests.
- **Security**: Restrict DB API as needed to prevent unsafe queries from JS.

## Rollback Plan
- If performance or scalability issues arise, consider reintroducing a pool with
  stricter DB access patterns (e.g., per-context DB handles, request queueing).

## References
- [QuickJS-NG Developer Guide: Runtime and Contexts](https://quickjs-ng.github.io/quickjs/developer-guide/intro/#runtime-and-contexts)
- [SQLite Threading Modes](https://www.sqlite.org/threadsafe.html)
- `CLAUDE.md` and `hbf_impl.md` for project architecture

---

This migration will simplify the codebase, improve safety, and make DB-backed routing and static serving easier to implement and maintain.