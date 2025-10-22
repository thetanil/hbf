# QuickJS Engine and Integration Analysis

Date: 2025-10-22

This document reviews the current QuickJS (quickjs-ng) embedding, database-backed script loading, HTTP routing, and proposes improvements to align with quickjs-ng recommendations and our project’s “no filesystem” constraint.

## Summary

- The embedding uses one JSRuntime+JSContext per pooled context, with memory limits and an interrupt-based timeout — aligned with quickjs-ng guidance.
- All JavaScript (router.js, server.js) and static assets are stored and loaded from the SQLite database. No filesystem access is used at runtime — consistent with our design goals.
- The HTTP handler bridges CivetWeb requests to JS via request/response bindings and calls `app.handle(req, res)` defined by router.js — functioning correctly in happy paths.
- Gaps identified:
  - Execution timeout “start time” isn’t reset before per-request JS calls — can cause immediate timeouts long after initialization.
  - Use of a process-global `JSAtom` for response binding is unsafe across runtimes.
  - Minor JSValue lifecycle issues (a missing `JS_FreeValue` on an early return path); passing `"<eval>"` as filename hinders error stack usefulness.
  - No DB-backed static file serving route is implemented yet.
  - SQL for script loading doesn’t leverage the generated/indexed `name` virtual column.

## What’s implemented now

Components reviewed:
- `internal/qjs/engine.[ch]`
  - Creates JSRuntime/JSContext per pooled context.
  - Sets memory limit via `JS_SetMemoryLimit`.
  - Sets interrupt handler via `JS_SetInterruptHandler` using a monotonic timestamp to enforce timeouts during execution.
  - Evaluates code with `JS_Eval(..., JS_EVAL_TYPE_GLOBAL)` and collects exceptions into a per-context error buffer.
- `internal/qjs/loader.[ch]`
  - Loads JS from the database (`nodes` table, JSON `body` with `{ name, content }`).
  - Loads `router.js` then `server.js` into a context.
- `internal/qjs/modules.[ch]`
  - Thin helper around loading `router.js` from the DB.
- `internal/qjs/pool.[ch]`
  - Pre-creates a fixed-size pool of contexts. Each context is initialized with `router.js` and `server.js` from the DB.
  - Threads acquire/release contexts with mutex/condvar. No JS objects cross context boundaries.
- `internal/qjs/bindings/{request,response}.[ch]`
  - Request: creates a JS object with method, path, query, headers, and an empty params object.
  - Response: accumulates status, headers, and body in C; exposes JS methods `status`, `send`, `json`, `set`; then flushes to CivetWeb.
- `internal/http/{server,handler}.[ch]`
  - CivetWeb server registers `**` to the QuickJS handler.
  - Handler acquires a context, constructs `req` and `res`, then calls `app.handle(req, res)` exported by `router.js`.
- `static/lib/router.js` and `static/server.js`
  - `router.js` defines a small Express-like router with `get/post/put/delete/use` and `next()` middleware, mounting a global `var app = new Router();`.
  - `server.js` adds simple routes and middleware. No static-file serving yet.
- `internal/db/schema.sql` and DOCS
  - Bazel injects `router.js`, `server.js`, and static files into the DB at build time (`tools/inject_content.sh`). Static assets are stored with `type='static'` and names like `"/www/index.html"`.

## Conformance with quickjs-ng documentation

- Runtimes and contexts: Using one JSRuntime per context and never sharing JSValues across runtimes is correct. QuickJS doesn’t support multithreading within a single runtime; the pool approach avoids that.
- Memory and time limits: Using `JS_SetMemoryLimit` and `JS_SetInterruptHandler` matches the recommended APIs for resource control.
- Script evaluation: Using `JS_Eval(..., JS_EVAL_TYPE_GLOBAL)` for classic scripts is appropriate for our `router.js` / `server.js` model. We are not using ES Modules yet — acceptable. If ES Modules are desired later, we should implement a custom module loader via `JS_SetModuleLoaderFunc` (see Improvements).
- No quickjs-libc: We don’t register `quickjs-libc` (std/os), which is good since we want no OS/fs access.

## Issues and improvements

1) Execution timeout start time is not reset for per-request calls
- Where: `internal/qjs/engine.c` sets `ctx->start_time_ms` in `hbf_qjs_eval()` and on context creation, but not before `JS_Call` in the HTTP handler.
- Effect: The interrupt handler measures since last set; if the context sits idle longer than the configured timeout, the next request execution may be immediately interrupted.
- Fix: Provide an API to mark execution start before any JS entry point: either
  - Add `hbf_qjs_begin_exec(hbf_qjs_ctx_t*)` to set `start_time_ms` and call it in `hbf_qjs_request_handler` just before `JS_Call`, or
  - Add a wrapper `hbf_qjs_call(...)` that sets the time and then calls `JS_Call`.

2) Response binding uses a global JSAtom across runtimes
- Where: `internal/qjs/bindings/response.c` keeps `static JSAtom response_ptr_atom`.
- Issue: JS atoms are per-runtime; reusing an atom value across contexts (different runtimes) is invalid and can crash or misbehave.
- Fix options:
  - Use property string variants: replace `JS_{Get,Set}Property(ctx, obj, atom, ...)` with `JS_{Get,Set}PropertyStr(ctx, obj, "__hbf_response_ptr__", ...)`. This ensures proper atom handling per runtime.
  - Or define a dedicated JS class with `JS_NewClassID/JS_NewClass` and store the C pointer via `JS_SetOpaque` on objects of that class (cleaner and safer). Add a finalizer if needed.

3) Minor JSValue lifecycle issues in HTTP handler
- Where: `internal/http/handler.c`
- When `app` is `undefined`, the code returns early without freeing `app`. Values returned by `JS_GetPropertyStr` should be freed even if undefined.
- Fix: Ensure every live JSValue from `JS_GetGlobalObject`/`JS_GetPropertyStr` is freed on all code paths.

4) Improve error context by passing source names to JS_Eval
- Where: `internal/qjs/loader.c`
- Currently passes `"<eval>"` as the filename for all scripts; stack traces and error messages would be more useful if we pass the script’s `name` (e.g., `"router.js"`, `"server.js"`).
- Fix: Use `JS_Eval(ctx, content, len, name, JS_EVAL_TYPE_GLOBAL)`.

5) Consider setting a maximum system stack size
- Where: `internal/qjs/engine.c`
- QuickJS provides `JS_SetMaxStackSize` to limit C stack usage from JS recursion. We already have middleware recursion guards in JS; a stack limit adds defense-in-depth.

6) ES Module support (future)
- If/when needed, implement a DB-backed module loader via `JS_SetModuleLoaderFunc` so `import` can resolve names to DB rows. This keeps the no-filesystem invariant.

7) Script-loading SQL should leverage indexed virtual columns
- Where: `internal/qjs/loader.c`
- Query currently uses `json_extract(body, '$.name') = ?`. The schema defines a GENERATED VIRTUAL column `name` with an index (`idx_nodes_name`).
- Fix: Change to `SELECT json_extract(body, '$.content') FROM nodes WHERE type='script' AND name = ?` to use the index and filter by type.

8) Per-request memory/pressure considerations
- Contexts are long-lived and reused; JS globals accrue state across requests. That’s OK for our current model, but if per-request isolation becomes necessary, we can:
  - Reset/refresh contexts periodically, or
  - Use a different pool strategy (short-lived contexts), at some perf cost.

## Static file server method (DB-backed)

Requirement: provide a route handler to serve static assets from the database instead of the filesystem.

Current state:
- Static files are injected into the DB with `type='static'` and a normalized name (e.g., `"/www/index.html"`).
- There is no static serving route or binding yet.

Proposed minimal design (phaseable):

- Data contract
  - Input: HTTP GET to a path (e.g., `/` mapping to `/www/index.html`, or `/style.css` mapping to `/www/style.css`).
  - DB lookup: `SELECT json_extract(body,'$.content') FROM nodes WHERE type='static' AND name = ?`.
  - Output: 200 with the content and appropriate `Content-Type`; otherwise 404.

- Option A: Implement in C with a JS-visible helper
  - Add a C helper exposed to JS (e.g., `__hbf_read_static(path)`), which:
    - Queries the DB for `type='static'` and normalized name.
    - Returns `null` on miss or `{ body: string, mime: string }` on hit.
  - In `router.js`, add `app.get("/*", ...)` or `app.static(prefix)` that calls the helper and responds via `res.set()/res.send()`.
  - Pros: simple; keeps DB code in C; JS remains small.

- Option B: Implement in C fully as a fallback handler
  - In `hbf_qjs_request_handler`, if `app.handle` didn’t send a response (e.g., leave a flag), perform a DB lookup for static and serve directly.
  - Pros: no new JS APIs; faster path for static; Cons: diverges from “route handler in JS”.

- MIME type mapping
  - Minimal table by extension in C (`.html`, `.css`, `.js`, `.png`, etc.).
  - Or expose a JS helper to compute MIME if we keep logic in JS.

- Normalization
  - Map `/` to `/www/index.html` by convention (already used in our static tree).
  - Keep names consistent with `tools/inject_content.sh` scheme.

Suggested first step: Option A (JS route using a small C helper) — it meets the “route handler” requirement and keeps flexibility in JS.

## Actionable recommendations

Short-term fixes:
- Reset timeout start before `JS_Call` per request.
- Replace global `JSAtom` with `JS_{Get,Set}PropertyStr` or switch to a JS class + `JS_SetOpaque`.
- Free all JSValues on all early-return paths in `internal/http/handler.c`.
- Pass the actual script name to `JS_Eval` in `loader.c`.
- Update loader SQL to use `type='script' AND name = ?`.

Next features:
- Add a DB-backed static route as described (Option A).
- Optionally set `JS_SetMaxStackSize`.
- Add a tiny MIME map.
- Add tests for static serving, including 200/404 and content-type checks.

Future (if needed):
- Implement `JS_SetModuleLoaderFunc` for ES Modules from DB.
- Consider periodic context recycling if memory growth across requests becomes a concern.

## Notes on testing

- There are good tests for engine, loader, pool, and router behavior. Add:
  - A test to ensure per-request timeouts work (after waiting longer than the timeout, the next `JS_Call` should still execute when we reset the start timestamp).
  - Static serving tests that populate a static row (e.g., `/www/index.html`) and ensure `GET /` returns it with `Content-Type: text/html`.

## Conclusion

The current QuickJS embedding is solid and follows quickjs-ng’s core guidance for runtime/context, memory limits, and timeouts. With a handful of correctness/perf fixes and a simple DB-backed static route, we’ll fully align with the “no filesystem” architecture and improve robustness and debuggability.