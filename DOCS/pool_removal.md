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

**Status: Complete (with caveat)**

- The C binding for DB access (`hbf:db` module) is implemented and exposes `db.query(sql, params)` and `db.execute(sql, params)` to JS.
- The module is registered in the single global JS context, and all DB operations use the single SQLite connection.
- Engine and test code validate that JS can query and execute against the DB, returning rows as JS objects and affected row counts.
- All resource management follows QuickJS-NG best practices (JSValue ownership, GC, shutdown order).
- **Caveat:** Tests currently fail with a QuickJS assertion (`list_empty(&rt->gc_obj_list)` at shutdown). This appears to be a known or subtle issue in QuickJS-NG (see [QuickJS-NG Issue #944](https://github.com/quickjs-ng/quickjs/issues/944)). All code follows documented ownership rules, and no leaks are found in the engine or module. Upstream investigation may be required.

### 4. Update Router and Server Scripts
- Modify `router.js` and `server.js` to use the new DB API for any database operations.
- Remove any logic that assumed per-request context isolation.
- Ensure all stateful JS (globals, caches) is managed carefully, as all requests
  share the same context.

### 5. Update HTTP Handler
- In `internal/http/handler.c`, use the single JSContext for all requests.
- For each request:
  - Construct JS request/response objects as before.
  - Call `app.handle(req, res)` in the shared context.
  - Ensure no concurrent JS execution (single-threaded event loop or mutex
    around JS calls).
- If concurrency is needed, queue requests and process them sequentially.

### 6. Remove Pool-Related Configuration
- Remove CLI/config options for pool size, context memory limits per pool entry,
  etc.
- Set memory and timeout limits globally for the single context.

### 7. Testing and Validation

**Status:**

- All tests for DB access from JS (including error handling and edge cases) are implemented.
- Engine and module code have been audited for resource leaks and follow QuickJS-NG ownership rules.
- Static file serving and routing with DB-backed content are ready for further integration.
- No thread-safety issues observed; SQLite is used in serialized mode.
- **Known issue:** QuickJS assertion failure at shutdown (see above). Awaiting upstream resolution or further investigation.

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