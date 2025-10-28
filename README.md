# HBF

a single-binary web runtime (C + SQLite + CivetWeb + QuickJS)

HBF is a single executable that serves HTTP, runs a JavaScript handler per request (QuickJS), and stores both your app’s code and static files inside a SQLite database. The current behavior and interfaces are documented in DOCS/hbf_spec.md — this README is a concise Quickstart aligned with that spec.

## What you get today

- CivetWeb HTTP server with three routes out of the box:
  - GET /health → {"status":"ok"}
  - GET /static/** → files loaded from the main DB’s SQLAR
  - Catch‑all → executes hbf/server.js (from the main DB’s SQLAR) with a QuickJS context per request
- QuickJS sandbox per request (fresh runtime/context, 64 MB limit, 5s timeout)
- Embedded read‑only template DB (fs.db) compiled into the binary and used only to create a new main DB

See full details in DOCS/hbf_spec.md.

## Build

Prerequisites:
- Linux x86_64
- Bazel (bzlmod enabled). All C libraries (SQLite, CivetWeb, QuickJS‑NG, zlib) are fetched by Bazel; no system packages required.

Commands:

```bash
# Build stripped binary
bazel build //:hbf

# Optionally, build unstripped binary for debugging
bazel build //:hbf_unstripped

# Run tests and lint
bazel test //...
bazel run //:lint
```

Artifacts:
- Stripped binary: bazel-bin/hbf
- Unstripped binary: bazel-bin/internal/core/hbf (via //:hbf_unstripped)

## Run

```bash
# Default run (port 5309, persistent DB file ./hbf.db)
./bazel-bin/hbf

# Custom port and verbose logging
./bazel-bin/hbf --port 8080 --log-level debug

# In‑memory database (discard on exit)
./bazel-bin/hbf --inmem

# Or run through Bazel
bazel run //:hbf -- --port 8080 --log-level info
```

Then:

```bash
curl http://localhost:5309/health
# {"status":"ok"}
```

## CLI options

```text
--port <num>        HTTP port (default: 5309)
--log-level <lvl>   debug | info | warn | error (default: info)
--dev               development mode flag
--inmem             use an in‑memory main database (not persisted)
--help, -h          show help
```

## Default app and routes

At build time, fs/ is packed into an embedded SQLite archive (SQLAR) and linked into the binary. On first run (or with --inmem), the DB module creates the main database (./hbf.db) from that template and stores assets there. At runtime, all reads go to the main DB.

Included content today:
- JavaScript handler: fs/hbf/server.js (stored under hbf/server.js in SQLAR)
- Static files: fs/static/index.html (served under /static/index.html)

The default server.js implements:
- GET / → HTML page listing sample routes
- GET /hello → {"message":"Hello, World!"}
- GET /user/:id → echoes userId
- GET /echo → echoes method and URL
- otherwise 404 Not Found

## Customize your app

Edit these files and rebuild:
- fs/hbf/server.js → your request handler (global app.handle(req, res))
- fs/static/** → any static assets served under /static/**

Then rebuild to regenerate the embedded template and binary:

```bash
bazel build //:hbf
```

Notes:
- The embedded template DB is used only to create a new main DB. If ./hbf.db already exists, it is not modified. Delete ./hbf.db (or use --inmem) to pick up new embedded content.

## How the embedded content pipeline works

Implemented in fs/BUILD.bazel:
1) //fs:fs_archive → creates fs.db (SQLAR) from fs/hbf/server.js and fs/static/index.html
2) //fs:fs_db_gen → converts fs.db into C sources (fs_embedded.c/.h)
3) //fs:fs_embedded → linked into the final binary

On startup, the DB module will create the main DB from that embedded template if needed; thereafter, only the main DB is used for reads.

## End‑to‑end check

Run the integration script (starts the server on a random free port and exercises endpoints):

```bash
./tools/integration_test.sh
```

## Troubleshooting

- Health returns 200 but other routes 5xx
  - Ensure hbf/server.js evaluates to a global app with app.handle(req, res)
- Static file 404
  - Static files are served only under /static/**; confirm the path and rebuild
- Changed fs/ content isn’t showing up
  - Delete ./hbf.db (or use --inmem) so the main DB is recreated from the embedded template

## License

MIT. Third‑party components retain their original licenses. See DOCS/hbf_spec.md for implementation details and limitations.

## FUTURE

A small, batteries-included platform for server‑side rendered apps that lean on htmx and hypermedia—plus a live, in‑browser development experience that can target a local or remotely hosted HBF server.

### Server‑side rendering with htmx

- First‑class HTML and partial responses (hx‑request aware handlers).
- Helpers: `render(template, data)`, simple `layout()` with slots, and fragment helpers for partials.
- Response helpers for htmx: HX‑Redirect, HX‑Location, HX‑Refresh, HX‑Trigger, HX‑Push‑Url, plus caching/Vary conventions.
- Streaming: SSE and incremental chunking to support hx‑swap‑oob and progressive rendering.

### Live development in the browser

- Built‑in editor (Monaco) served under a dev UI; edits are stored in the main DB (SQLAR) or a dev overlay.
- Save → instant availability: code/assets become live without a full rebuild; per‑request QuickJS sandbox ensures isolation.
- Preview pane with auto‑refresh driven by htmx/WebSocket; keyboard palette for common actions (restart, clear DB, seed).
- Server‑side console bridged to the browser for logs, stack traces, and minimal REPL eval in a safe context.
- Safe hot‑reload for server modules with version pinning and one‑click rollback.

### Remote or local app server

- Pair a browser dev session with a local process or a remote HBF via secure WebSocket tunnel + token.
- Live tail of logs, request traces, and DB metrics exposed in the dev UI.
- Content sync protocol (chunked, content‑addressed blobs) with dry‑run diffs before push.

### Hypermedia contract and UX

- Opinionated conventions for links and forms (method overrides, validation, error presentation) that map cleanly to htmx.
- A small HTML partials/components kit (forms, lists, tables) built on hx‑boost, hx‑swap, and out‑of‑band swaps.
- Action discovery via link rels and `data-hbf-action`, powering a keyboardable command palette.

### Database and content write path

- Public write APIs to SQLAR (add/update/delete assets and server code) with content‑addressed versions and history.
- Staging vs production content spaces; transactional “promote” with integrity checks.
- Built‑in migrations (SQL) and seed data applied on first run and promotion.

### Realtime

- SSE and WebSocket helpers; broadcast channels; per‑user subscriptions.
- htmx SSE extension compatibility; simple `publish()` API from QuickJS.

### Security and multi‑tenancy

- Sensible defaults: CSP, secure headers, CSRF integration with htmx.
- AuthN/AuthZ hooks in JS (sessions, roles/claims), per‑route guards.
- App namespaces in a single binary (per‑app DB/schema, per‑request CPU/mem timeouts).

### Tooling

- CLI: `hbf dev`, `hbf open`, `hbf pack`, `hbf push/pull`, `hbf logs`.
- Inspector: request timeline, DB query log, slow route profiler.
- Test harness: in‑proc server, snapshot HTML/partials, simulate htmx requests.

### Examples

- Todo (htmx), chat (SSE/WebSocket), live Markdown preview, admin CRUD.

### maybe

- Add a “How to update content without rebuild” section once write APIs for SQLAR are available.
- Wire a short dev‑loop note (e.g., using `--inmem` + dev overlay) into the README.
- Include Monaco for the in‑browser editor.
- Update content live without full rebuild; document safe hot‑reload semantics.