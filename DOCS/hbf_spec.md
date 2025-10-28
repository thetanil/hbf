# HBF Specification (current implementation)

This document describes the behavior and architecture of HBF as implemented in
this repository today. It is based on the actual code paths and build pipeline
and highlights any divergences from prior plans.

## Overview

HBF is a single-binary web runtime that embeds:
- An HTTP server (CivetWeb)
- A QuickJS JavaScript engine for per-request handlers
- SQLite for persistent application data ("main" database)
- An embedded, read-only SQLAR SQLite database (fs_db) used only inside the DB
  module at startup to hydrate the main database with baseline assets (including
  `hbf/server.js` and `static/*`)

Endpoints are served by:
- A built-in health check (C)
- A static asset handler for `/static/**` (C, served from the main database’s
  SQLAR)
- A JavaScript handler for all other routes (QuickJS evaluating `hbf/server.js`
  stored in the main database’s SQLAR on each request)

## Build and Embed Pipeline

Bazel drives a pipeline that turns the contents of `fs/` into an embedded SQLite
archive included in the binary:

- Rule: `//fs:fs_archive` creates `fs.db` via `sqlite3 -A -cf` including:
  - `fs/hbf/server.js`
  - `fs/static/index.html` and other files under `fs/static/`
- Rule: `//fs:fs_db_gen` converts `fs.db` into C sources (`fs_embedded.c`, `fs_embedded.h`)
- Library: `//fs:fs_embedded` is linked into the final binary

References:
- `fs/BUILD.bazel`

At build time, `fs.db` is generated and converted to C sources. At runtime, the
DB module can deserialize the embedded `fs.db` internally only when creating the
main DB (e.g., when the DB file is missing or when using `:memory:`). It does
not modify an existing database from the template. The `fs_db` pointer is not
exposed outside the DB module.

## Runtime Databases

HBF maintains a single long-lived SQLite connection for all application
activity: the main database. An embedded, read-only template database (fs_db) is
compiled into the binary and used only inside the DB module during
initialization.

- Main DB (runtime, writable)
  - Path: `./hbf.db` (or `:memory:` with `--inmem`)
  - Configured with `PRAGMA journal_mode=WAL` and `PRAGMA foreign_keys=ON`
  - Contains the SQLAR table with application assets (including `hbf/server.js` and `static/*`). All runtime reads of assets use the main database’s SQLAR.

- Template DB (fs_db, read-only, embedded)
  - Deserialized internally from compiled-in `fs_embedded` data
  - Used only to create/hydrate the main database when creating it (e.g., when `hbf.db` is missing or when using `:memory:`). It is not used to modify existing databases that already exist on disk.
  - Its connection and pointer never leave the DB module; after initialization, only the main DB handle is used throughout the system

Initialization contract (DB module):
- On startup, the DB module opens/creates the main DB.
- If the main DB file does not exist, the DB module creates it from the embedded
  `fs_db` template (or deserializes it for `:memory:`). If the main DB exists
  but is missing the `sqlar` table or baseline assets, startup fails with a
  fatal error; the module does not rehydrate or modify an existing DB from the
  template.
- The DB module returns only the main DB connection to callers. No component
  outside the DB module receives or uses an fs_db handle.

References:
- `internal/db/db.c`, `internal/db/db.h`

Status: aligned — fs_db is now private to the DB module; hydration occurs only
when creating the main DB (not for existing DBs); runtime asset reads use main
DB SQLAR via `hbf_db_read_file_from_main` and related helpers; public APIs
expose only the main DB. If an existing DB is missing `sqlar` or baseline
assets, startup fails (no automatic rehydration).

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
  - If needed (DB missing or in-memory mode), create/hydrate the main DB from
    the embedded `fs_db` template (inside the DB module)
  - Return only the main DB connection to the caller (fs_db remains private and is not exposed)
- Initialize QuickJS engine (memory limit 64 MB, timeout 5000 ms)
- Create and start HTTP server
- Log: `HTTP server listening at http://localhost:<port>/`

Status: aligned — `hbf_server_create(int port, sqlite3 *db)` accepts only the
main DB; server state no longer contains `fs_db`.

Signals SIGINT/SIGTERM cause a clean shutdown.

## HTTP Server and Routing

`internal/http/server.c` uses CivetWeb to register handlers in this order:
- `/health` → built-in C handler returning `{ "status": "ok" }`
- `/static/**` → static file handler reading from the main database’s SQLAR
- `**` (catch-all) → QuickJS request handler

Status: aligned — the static handler reads assets via `hbf_db_read_file_from_main(server->db, ...)`.

### Static Handler

- Serves only under `/static/**` by reading from the main DB’s SQLAR
- MIME types are inferred by extension
- Returns 404 if the asset is not found in SQLAR

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

Status: aligned — the JS handler loads `hbf/server.js` using `hbf_db_read_file_from_main(server->db, ...)`.

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

Dependencies provenance (Bazel excerpts):

```
bazel_dep(name = "sqlite3", version = "3.50.4")
bazel_dep(name = "zlib", version = "1.3.1.bcr.7")

git_repository(
  name = "civetweb",
  remote = "https://github.com/civetweb/civetweb.git",
  tag = "v1.16",
  build_file = "//third_party/civetweb:civetweb.BUILD",
)

git_repository(
  name = "quickjs-ng",
  remote = "https://github.com/quickjs-ng/quickjs.git",
  tag = "v0.8.0",
  build_file = "//third_party/quickjs-ng:quickjs.BUILD",
)
```

## Known Limitations (current state)

- No module `import`/`require` support in QuickJS environment; all server code must live in `hbf/server.js`
- Body parsing for POST/PUT is not implemented
- The embedded template (fs_db) is read-only and used only during initialization; all runtime asset reads use the main DB’s SQLAR
- The reason phrase in HTTP status line is fixed to `OK`
- Static handler serves only under `/static/**`; homepage `/` is served by JS
- No automatic rehydration of existing databases: if the main DB exists but is
  missing the `sqlar` table or baseline assets, startup fails (fatal) rather
  than modifying the DB from the embedded template

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
  - `internal/db/db.*` — init, close, main-DB SQLAR access (fs_db is internal/template-only)
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

This spec reflects the repository's behavior as of the current main branch and
will be updated as future plan items are implemented.
