# HBF Specification (current implementation)

This document describes the behavior and architecture of HBF as implemented in this repository today. It is based on the actual code paths and build pipeline and highlights any divergences from prior plans.

## Overview

HBF is a single-binary web runtime that embeds:
- An HTTP server (CivetWeb)
- A QuickJS JavaScript engine for per-request handlers
- SQLite for persistent application data ("main" database)
- An embedded, read-only SQLAR SQLite database (fs_db) for app assets and the server-side handler script

Endpoints are served by:
- A built-in health check (C)
- A static asset handler for `/static/**` (C, served from the main database’s SQLAR)
- A JavaScript handler for all other routes (QuickJS evaluating `hbf/server.js` stored in the main database’s SQLAR on each request)

## Build and Embed Pipeline

Bazel drives a pipeline that turns the contents of `fs/` into an embedded SQLite archive included in the binary:

- Rule: `//fs:fs_archive` creates `fs.db` via `sqlite3 -A -cf` including:
  - `fs/hbf/server.js`
  - `fs/static/index.html` and other files under `fs/static/`
- Rule: `//fs:fs_db_gen` converts `fs.db` into C sources (`fs_embedded.c`, `fs_embedded.h`)
- Library: `//fs:fs_embedded` is linked into the final binary

References:
- `fs/BUILD.bazel`

At runtime, `internal/db/db.c` deserializes the embedded `fs.db` into an in-memory SQLite connection (`fs_db`) and initializes SQLAR functions (`sqlite3_sqlar_init`).

## Runtime Databases

HBF maintains a single long-lived SQLite connection for all application activity: the main database. An embedded, read-only template database (fs_db) is compiled into the binary and used only inside the DB module during initialization.

- Main DB (runtime, writable)
  - Path: `./hbf.db` (or `:memory:` with `--inmem`)
  - Configured with `PRAGMA journal_mode=WAL` and `PRAGMA foreign_keys=ON`
  - Contains the SQLAR table with application assets (including `hbf/server.js` and `static/*`). All runtime reads of assets use the main database’s SQLAR.

- Template DB (fs_db, read-only, embedded)
  - Deserialized internally from compiled-in `fs_embedded` data
  - Used only to (re)create or hydrate the main database on startup when required (e.g., when `hbf.db` is missing or lacks the expected SQLAR content)
  - Its connection and pointer never leave the DB module; after initialization, only the main DB handle is used throughout the system

Initialization contract (DB module):
- On startup, the DB module opens/creates the main DB.
- If the main DB is missing required structures/content (e.g., no SQLAR table or missing baseline assets), the DB module clones from the embedded fs_db into the main DB.
- The DB module returns only the main DB connection to callers. No component outside the DB module receives or uses an fs_db handle.

References:
- `internal/db/db.c`, `internal/db/db.h`

Implementation status vs spec (delta):
- Current code paths in the HTTP layer still pass and use an `fs_db` pointer directly (e.g., static/JS handlers reading from `server->fs_db`). This violates the spec’s constraint that `fs_db` is private to the DB module.
- Migration steps:
  1) Keep `fs_db` internal to the DB module. Perform main DB hydration there and return only the main DB handle.
  2) Provide DB accessors that read assets from the main DB’s SQLAR (e.g., `hbf_db_read_file_from_main(db, path, ...)`).
  3) Remove `fs_db` from public headers and from `hbf_server_t`. Update call sites to use the main DB handle exclusively.
  4) Add a startup integrity check in the DB module (ensure main DB has required SQLAR entries, else rehydrate from embedded template).

## Configuration and Startup

Command line options (`--help`):
- `--port PORT` (default 5309)
- `--log-level LEVEL` (debug|info|warn|error; default info)
- `--dev` (development mode flag)
- `--inmem` (use in-memory main DB)

References:
- `internal/core/config.c`, `internal/core/config.h`

Startup sequence (`internal/core/main.c`):
- Parse config and initialize logging
- Initialize databases via the DB module:
  - Open or create the main `hbf.db`
  - If needed, (re)create/hydrate the main DB from the embedded fs_db template (inside the DB module)
  - Return only the main DB connection to the caller (fs_db remains private and is not exposed)
- Initialize QuickJS engine (memory limit 64 MB, timeout 5000 ms)
- Create and start HTTP server
- Log: `HTTP server listening at http://localhost:<port>/`

Implementation status vs spec (delta):
- `hbf_server_create` currently accepts both `db` and `fs_db`, and the server struct retains both pointers. To align with the spec:
  - Change `hbf_server_create` to accept only the main `db`.
  - Ensure the DB module completes any fs_db→main hydration before returning.
  - Remove all references to `fs_db` from HTTP/server code.

Signals SIGINT/SIGTERM cause a clean shutdown.

## HTTP Server and Routing

`internal/http/server.c` uses CivetWeb to register handlers in this order:
- `/health` → built-in C handler returning `{ "status": "ok" }`
- `/static/**` → static file handler reading from the main database’s SQLAR
- `**` (catch-all) → QuickJS request handler

Implementation status vs spec (delta):
- The static handler currently reads files via `server->fs_db`. Migrate to use main DB accessors from the DB module (e.g., `hbf_db_read_file_from_main(server->db, path, ...)`).

### Static Handler

- Maps `/` to `static/index.html` only if handled by JS; static handler explicitly serves under `/static/**`
- For `/static/**`, file path is `static<URI>` (e.g., `/static/style.css` → `static/static/style.css` would be wrong; current code maps `/static/**` by handler path and uses `static%s`, so URIs should not include an extra `static` prefix; avoid conflicting routes at root)
- MIME types are inferred by extension
- 404 if file not found in the embedded SQLAR

References:
- `internal/http/server.c` (static handler)

### JavaScript Handler (QuickJS)

Per request, `internal/http/handler.c`:
1. Creates a fresh QuickJS runtime and context (`hbf_qjs_ctx_create_with_db(server->db)`) — contexts are NOT reused between requests
2. Reads and evaluates `hbf/server.js` from the main database’s SQLAR (via DB module accessors)
3. Constructs `req` and `res` JS objects and calls `app.handle(req, res)` from the evaluated script
4. Sends the accumulated response

If evaluation of `hbf/server.js` fails or `app.handle` is missing/not a function, a 5xx is returned.

References:
- `internal/http/handler.c`
- `internal/qjs/engine.c`
- `internal/db/db.h` (asset read helpers targeting the main DB’s SQLAR)

Implementation status vs spec (delta):
- The handler presently calls into `hbf_db_read_file(server->fs_db, "hbf/server.js", ...)`. Update it to read from the main DB (e.g., `hbf_db_read_file_from_main(server->db, ...)`) after removing `fs_db` from the server struct.
- Remove all references to `fs_db` from quickjs module and handlers

## JavaScript Request/Response Contracts

### Request (`req`)
Constructed in `internal/qjs/bindings/request.c`:
- `method`: HTTP method (e.g., GET)
- `path`: `ri->local_uri` (e.g., `/hello`)
- `query`: raw query string (or "")
- `headers`: object with incoming headers
- `params`: empty object (reserved for future routing parameterization)

Note: There is currently no parsed body support for POST/PUT.

### Response (`res`)
Constructed and bound in `internal/qjs/bindings/response.c`:
- Methods:
  - `res.status(code)`
  - `res.set(name, value)`
  - `res.send(body)`
  - `res.json(obj)` (sets `Content-Type: application/json` automatically)
- Notes:
  - Headers array is limited to 32 entries
  - `hbf_send_response` writes `HTTP/1.1 <status> OK` regardless of the specific reason phrase
  - `Content-Length` is set automatically based on the body

## Default JavaScript App

`fs/hbf/server.js` (embedded at build time and stored in the main database’s SQLAR) defines a global `app` with `handle(req, res)` and includes:
- Middleware: sets `X-Powered-By: HBF`
- Routes:
  - `GET /` → HTML homepage with an "Available Routes" section
  - `GET /hello` → `{ "message": "Hello, World!" }`
  - `GET /user/:id` (e.g., `/user/42`) → `{ "userId": "42" }`
  - `GET /echo` → `{ "method": <method>, "url": <path> }`
  - All other routes → 404 "Not Found"

References:
- `fs/hbf/server.js`

## Health Check

`GET /health` returns HTTP 200 with body:
```json
{"status":"ok"}
```
Reference: `internal/http/server.c`.

## Logging

- Logging is configured at startup based on `--log-level`
- Examples during startup:
  - `HBF starting (port=<port>, inmem=<0/1>, dev=<0/1>)`
  - `Opened main database: ./hbf.db`
  - `Loaded embedded filesystem database (<n> bytes)`
  - `QuickJS engine initialized (mem_limit=64 MB, timeout=5000 ms)`
  - `HTTP server listening at http://localhost:<port>/`

References:
- `internal/core/log.*`, log usage throughout modules

## CLI and Running

With Bazel:
- Build: `bazel build //:hbf`
- Run: `bazel run //:hbf -- --port 8080 --log-level debug`

Integration test: `./tools/integration_test.sh` verifies the endpoints listed above.

References: `tools/integration_test.sh`

## QuickJS Engine Settings

- Memory limit: 64 MB
- Execution timeout: 5000 ms (interrupt handler)
- Fresh runtime and context per request (no pooling)

Reference: `internal/qjs/engine.c`

## Dependencies

- CivetWeb (HTTP server)
- SQLite (core DB)
- SQLAR extension (archive within SQLite)
- QuickJS-NG (JavaScript engine)
- zlib (compression dependency used by SQLAR)

References: `third_party/*`, `MODULE.bazel` (zlib fetched via Bazel Central Registry)

## Known Limitations (current state)

- No module `import`/`require` support in QuickJS environment; all server code must live in `hbf/server.js`
- Body parsing for POST/PUT is not implemented
- The `fs_db` embedded archive is read-only at runtime
- The reason phrase in HTTP status line is fixed to `OK`
- Static handler serves only under `/static/**`; homepage `/` is served by JS

## Future Alignment with Design Notes

Planned adjustments from `DOCS/design_next.md`:
- Keep `fs_db` pointer private to the DB module; use it solely as a template to
  (re)create the main database when needed
- If the main database lacks the `sqlar` table or a required content set,
  rehydrate it from `fs_db`
- Provide REST APIs for reading/writing SQLAR documents to enable live content
  updates
- Introduce a Monaco editor under `fs/static/monaco` and markdown rendering in
  responses
- Optimize direct CivetWeb → `/static/*` fast path (already partially in place
  via `/static/**` handler)
- Default route handler in JS (implemented) with fallback to serving
  `index.html` from SQLAR (JS currently generates the homepage)

These items describe intended behavior and may differ from the current implementation documented above.

## File Index (selected)

- Entry points
  - `internal/core/main.c` — startup, shutdown
  - `internal/core/config.*` — CLI parsing
- Databases
  - `internal/db/db.*` — init, close, read file from fs_db
  - `fs/BUILD.bazel` — fs_db build pipeline
- HTTP
  - `internal/http/server.*` — CivetWeb setup and route registration
  - `internal/http/handler.*` — QuickJS per-request handler
- QuickJS
  - `internal/qjs/engine.*` — engine init and per-request context
  - `internal/qjs/bindings/request.*` — request object
  - `internal/qjs/bindings/response.*` — response object
- App content (embedded)
  - `fs/hbf/server.js` — default JS handler
  - `fs/static/**` — static assets (served under `/static/...`)
- Tooling
  - `tools/integration_test.sh` — endpoint checks

---

This spec reflects the repository's behavior as of the current main branch and will be updated as future plan items are implemented.
