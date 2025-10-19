# HBF Implementation Guide (C99 + Bazel)

This document proposes a pragmatic, phased plan to build HBF: a single,
statically linked C99 web compute environment that embeds SQLite as the
application/data store, CivetWeb for HTTP + WebSockets, and QuickJS for runtime
extensibility. Design and order of operations mirror WCE’s `IMPL.md`, adapted to
C, QuickJS, and Bazel.

- Language: Strict C99 (no C++)
- Binary: Single statically linked executable with musl toolchain (100% static, no glibc)
- HTTP: CivetWeb (HTTP, no HTTPS in scope), WebSockets supported
- DB: SQLite3 amalgamation with WAL + FTS5
- Scripting: QuickJS embedded, sandboxed
- Templates: EJS-compatible renderer running inside QuickJS
- Modules: Node-compatible JS module shim (CommonJS + minimal ESM), fs shim backed by SQLite
- Build: Bazel (cc_binary), vendored third_party sources; no GPL dependencies (MIT/BSD/Apache-2/PD only)


## Phase 0: Foundation & Constraints

Goals
- Freeze licensing, build, and dependency constraints
- Establish project layout and Bazel scaffolding

Decisions & Constraints
- Licenses: SQLite (public domain), CivetWeb (MIT), QuickJS (MIT), EJS (MIT),
  Argon2 (Apache-2.0), in-house JWT/HMAC (MIT), in-house shims (MIT). No GPL.
- Static linking: Use musl toolchain exclusively to produce a 100% static
  binary; no glibc builds.
- C99 only: compile with `-std=c99`, no extensions; keep warnings-as-errors
  later.
- Security posture: No HTTPS, but WebSockets allowed; strong password hashing
  (Argon2id) and signed JWT (HS256) with small self-contained crypto.

Deliverables
- Directory structure
- MODULE.bazel (bzlmod, Bazel 8) and Bazel `BUILD` files that compile a hello-world `hbf` cc_binary

Structure
```
/third_party/
  quickjs/           (vendored, MIT)
  sqlite/            (amalgamation or pinned source, PD)
  civetweb/          (vendored, MIT)
  argon2/            (ref impl, Apache-2)
  sha256_hmac/       (single-file SHA-256 + HMAC, MIT – Brad Conte style; or vendor SHA-256 from B-Con/crypto-algorithms)
/internal/
  core/              (logging, config, cli)
  http/              (civetweb server, routing, websockets)
  cenv/              (cenv manager, db files)
  db/                (sqlite glue, schema, statements)
  auth/              (argon2, jwt, sessions, middleware)
  authz/             (table perms, row policies, query rewrite)
  document/          (document store + FTS5)
  qjs/               (quickjs engine, module loader, db bindings, ejs)
  templates/         (ejs integration helpers)
  ws/                (websocket adapters to JS)
  api/               (REST endpoints, admin endpoints)
/tools/
  pack_js/           (Bazel helper to bundle JS -> C array/SQLite blob)

hbf_impl.md
README.md (later)
MODULE.bazel
BUILD.bazel (root)
```

Bazel Notes (Bazel 8, bzlmod)
- Use `cc_library` for each third_party and subsystem; `cc_binary(name = "hbf")` linking fully static against musl.
- musl toolchain is the only supported toolchain; glibc builds are not produced.
- Suggested flags:
  - copts: `-std=c99 -O2 -fdata-sections -ffunction-sections -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_FTS5 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1`
  - linkopts: `-static -Wl,--gc-sections`
- Provide a Bazel C/C++ toolchain that targets musl and set it as default via `MODULE.bazel` (bzlmod).
 - WarningsAsErrors: '*' – treat all warnings as errors across all targets.

Test/CI
- Bazel `bazel test //...` with a consistent minunit-style C test harness (assert.h-based macros) and black-box HTTP tests.

### Coding Standards & Quality Gates (always-on)

- Standards to follow (documented in `DOCS/coding-standards.md`):
  - CERT C Coding Standard (applicable rules for C99)
  - Linux kernel coding style (tabs, 8-space indent, 80–100 cols) – approximated via .clang-format
  - musl/libc style principles (minimalism, defined behavior, no UB, simple macros)
- Enforcement (Bazel + tooling):
  - Compiler flags (both clang and gcc compatible where possible):
    - `-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion -Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings -Wcast-align -Wundef -Wswitch-enum -Wswitch-default -Wbad-function-cast`
  - `.clang-tidy` configured to enable CERT checks and core C checks (example):
    - Checks: `cert-*,bugprone-*,portability-*,readability-*,clang-analyzer-*,performance-*,-cppcoreguidelines-*,-modernize-*`
    - Language: C (no C++)
  - `.clang-format` approximating Linux kernel style (tabs, indent width 8, continuation indent 8, column limit 100, keep simple blocks on one line disabled).
  - Bazel lint target `//:lint` running clang-tidy over all C sources and failing on any warning.
  - Pre-commit hook to run `clang-format --dry-run --Werror` and `clang-tidy` on staged files.
  - CI: run `//:lint`, then build with `-Werror`, then tests.


## Phase 1: Bootstrap (HTTP + Storage) 

Goal
- Minimal HTTP server with CivetWeb, process lifecycle, config, routing frame; storage directory and cenv-id path extraction.

Tasks
- CLI/config: `--port` (default 5309), `--storage_dir` (default ./cenvs), `--log_level`, `--dev`.
- CivetWeb integration: start server, request logger, timeouts; register handlers for:
  - GET `/health`
  - `/{cenv-id}/{path...}` dispatch (no logic yet)
- Signal handling for graceful shutdown.
- UUID v4 generator (C, crypto-safe) to allocate new cenvs later.

Deliverables
- `internal/core/config.c|h`, `internal/http/server.c|h` with CivetWeb callbacks
- `GET /health` returns 200 JSON `{ok:true}`

Tests
- Start/stop server, concurrency sanity, simple bench (<100 req/s local).


## Phase 1.2: Cenv Routing & Validation

Goal
- Extract `{cenv-id}` from path, validate UUID, prepare per-cenv dispatch.

Tasks
- Router wrapper on top of CivetWeb: parse path into `{cenv-id}` and `subpath`.
- UUID validation via regex or parse function, 404 for invalid.
- Provide a `cenv_context` structure attached to request.

Deliverables
- `internal/http/router.c|h` and integration with server.


## Phase 1.3: Database File Management

Goal
- Map cenv-id to SQLite DB file and open/initialize as needed.

Tasks
- `Manager` in C: `cenv_exists`, `cenv_create`, `cenv_open`, `cenv_close_all`.
- File path: `storage_dir/{uuid}.db` mode 0600; directory 0700.
- SQLite open with pragmas: `foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`.
- One open connection per cenv in a hash map guarded by mutex; reference counting or LRU eviction in Phase 10.

Deliverables
- `internal/cenv/manager.c|h` with open/create/init hooks.

Tests
- Create and open DB; permissions; WAL active; corruption errors propagate.


## Phase 2: Schema & Initialization

Goal
- Create `_hbf_*` system tables and indexes at first open.

Tables (prefix `_hbf_`)
- `_hbf_users` (id, username unique, password_hash, role, email?, created_at, last_login, enabled)
- `_hbf_sessions` (session_id, user_id, token_hash, created_at, expires_at, last_used, ip, ua)
- `_hbf_table_permissions` (table_name, user_id, can_read, can_write, can_delete, can_grant)
- `_hbf_row_policies` (table_name, user_id?, policy_type, sql_condition, created_at, created_by)
- `_hbf_config` (key, value, updated_at, updated_by)
- `_hbf_audit_log` (timestamp, user_id, username, action, resource_type, resource_id, details, ip, ua)
- `_hbf_documents` (id, content, content_type, is_binary, searchable, created_at, modified_at, created_by, modified_by, version)
- `_hbf_document_tags` (document_id, tag)
- `_hbf_document_search` (FTS5 virtual table, triggers)
- `_hbf_endpoints` (path, method, script, description, enabled, created/modified metadata)

Tasks
- Place schema as const char* in `internal/db/schema.c` and execute inside a transaction.
- Insert default `_hbf_config` values: `session_timeout_hours=24`, `allow_registration=false`, `max_users=10`, `max_document_size_mb=10`, `qjs_timeout_ms=5000`.

Deliverables
- `internal/db/schema.c|h` + initialization from `cenv_manager`.

Tests
- Fresh DB contains all tables; indexes present; triggers maintain FTS.


## Phase 3: Authentication (Argon2id + JWT HS256)

Goal
- User registration and login with secure password hashing and stateless tokens paired with DB sessions.

Tasks
- Password hashing: Argon2id (Apache-2.0 ref impl) with parameters tuned for target box; encode hash with PHC string format.
- JWT: Implement HS256 using MIT SHA-256 + HMAC (single-file, Brad Conte style) and base64url encode/decode (pure C, no external deps).
  - Use HMAC_SHA256 for the HS256 signature over `base64url(header).base64url(payload)`.
  - Implement base64url encoder/decoder in-house (pure C).
- Session store: insert session on login with token hash (SHA-256 of the full JWT), expiry from config, rolling `last_used` and expiry updates.
- Endpoints:
  - POST `/new` {username, password, email?}: create cenv DB, bootstrap owner user
  - POST `/{cenv}/login` {username, password}: return `{token, expires_at, user}`
- Request auth middleware: extract Bearer token, verify signature + expiry, check session exists, scope to `{cenv}`.

Deliverables
- `internal/auth/argon2.c|h`, `internal/auth/jwt.c|h`, `internal/auth/session.c|h`
- `third_party/sha256_hmac/` vendored single-file (MIT) and `internal/crypto/sha256_hmac.c|h` thin wrappers
- HTTP handlers in `internal/api/auth.c`

Tests
- Hash/verify cycles, JWT sign/verify including expiry, login success/failure, disabled user handling.


## Phase 4: Authorization & Row Policies

Goal
- Enforce table-level permissions and row-level policies by query rewriting.

Tasks
- `authz_check_*` helpers (owner/admin implicit full access).
- Store table permissions and policies in DB; CRUD admin endpoints.
- Query rewriter: append WHERE fragments for SELECT/UPDATE/DELETE using policies; substitute `$user_id` with caller.
- Protect document and endpoint APIs with checks.

Deliverables
- `internal/authz/authz.c|h`, `internal/authz/query_rewrite.c|h`
- Admin endpoints: grant/revoke/list permissions; create/list row policies.

Tests
- Unit tests for rewrite (with existing WHERE/ORDER/LIMIT), permission matrix.


## Phase 5: Document Store + Search

Goal
- CRUD documents with optional FTS5 search and tagging; binary support via base64.

Tasks
- REST endpoints: list/create/get/update/delete, search with BM25; hierarchical IDs like `pages/home`, `api/users`.
- Triggers maintain FTS external content table; `searchable` flag toggles indexing.
- Content types: `application/json`, `text/markdown`, `text/html`, `text/ejs`, `application/octet-stream` (base64 encoded).

Deliverables
- `internal/document/document.c|h`, `internal/api/documents.c`

Tests
- CRUD roundtrip, tag queries, search relevance, binary content path.


## Phase 6: QuickJS Embedding (Runtime)

Goal
- Sandboxed JS runtime with DB access, HTTP context, timeouts and memory limits; dynamic endpoints defined in DB.

Tasks
- Initialize QuickJS runtime/context per request (or a pool) with:
  - Memory limit (e.g., 32–128MB) and interrupt handler for timeout (from `_hbf_config.qjs_timeout_ms`).
  - Disable dangerous host bindings; no raw file or network by default.
- Built-in host modules:
  - `hbf:db` with `query(sql, params)` and `execute(sql, params)` using prepared statements; converts SQLite types to JS.
  - `hbf:http` request object (method, path, query, headers, body JSON or raw), and `response(status, headers, body)` builder.
  - `hbf:util` JSON helpers, base64, time.
- Dynamic endpoints:
  - HTTP: `/{cenv}/js/{path}` resolves to `_hbf_endpoints` by (method, path or wildcard); execute QuickJS script owned by the cenv and return result. Endpoint definitions are stored per-cenv and are creatable/editable by the cenv owner (and admin per policy).
  - Errors surfaced as 500 with sanitized message; include `line:col` when possible.

Deliverables
- `internal/qjs/engine.c|h`, `internal/server/js_endpoints.c` (QuickJS HTTP endpoints), `internal/api/endpoints.c`

Tests
- Simple endpoint returns JSON; db access from JS; timeout enforced; disabled endpoints skipped.


## Phase 6.1: EJS Template Support (in QuickJS)

Goal
- Render EJS templates inside QuickJS, loading templates from SQLite.

Tasks
- Bundle a minimal EJS-compatible engine (MIT) adapted for QuickJS and no Node APIs. If using upstream, vendor a small pure-JS subset; otherwise implement a compact EJS renderer supporting:
  - `<% %>` script, `<%= %>` escape, `<%- %>` raw, includes (`<% include file %>`), locals, partials.
- `templates.render(id | source, data, loader)` where loader reads from `_hbf_documents` by id or provided source.
- Auto-escape HTML by default; allow raw with `<%- %>`.
- Map `text/ejs` (or `text/html+ejs`) documents to renderer.

Deliverables
- `internal/qjs/modules/ejs.js` (vendored), `internal/templates/ejs.c|h` (glue), DB-backed loader.

Tests
- Render variables, loops/conditionals, include parent; error messages with line numbers.


## Phase 6.2: Node-Compatible Module System (No npm/node at runtime)

Goal
- Load pure JS “Node modules” as QuickJS modules with shims and a DB-backed filesystem.

Tasks
- Module loader strategy:
  - Prefer ES modules when available.
  - Provide CommonJS compatibility by wrapping source: `(function(exports, require, module, __filename, __dirname){ ... })` inside QuickJS; maintain module cache.
- `fs` shim backed by SQLite: virtual filesystem that resolves `require('pkg/file')` by reading `_hbf_documents` rows under `modules/{pkg}/...` or a packaged module table; also support in-memory C-embedded JS for core shims.
- Provide minimal shims: `fs` (readFileSync, existsSync, stat), `path`, `buffer` (basic), `util`, `events`, `timers` (setTimeout/clearTimeout mapped to QuickJS job queue), `process` (env, cwd, versions minimal).
- Bazel packer: a rule that takes a list of JS sources (from vendor or project) and produces either:
  - a C array (compiled into the binary) addressable by virtual paths, or
  - a bootstrap SQLite image inserted into `_hbf_modules` on first init.
- Policy: Only pure JS modules that do not require native addons or Node runtime.

Deliverables
- `internal/qjs/node_compat.js` (require, module cache, shims)
- `tools/pack_js` Bazel rule + small packer (C or Python) to emit C array/SQLite blob

Tests
- Load a simple npm-like package (vendored source) without npm; require graph and caching; fs shim loads from DB.


## Phase 7: HTTP API Surface (Parity with WCE)

Goal
- Implement REST endpoints equivalent to WCE with C handlers delegating to auth/authz/doc/qjs layers.

Endpoints
- System
  - GET `/health`
  - POST `/new` (create cenv + owner)
  - POST `/{cenv}/login`
- Admin (owner/admin only)
  - `GET/POST/DELETE /{cenv}/admin/permissions` (+ list by user)
  - `GET/POST /{cenv}/admin/policies`
  - `GET/POST/GET(id)/DELETE /{cenv}/admin/endpoints`
- Documents
  - `GET /{cenv}/documents` (list)
  - `POST /{cenv}/documents`
  - `GET/PUT/DELETE /{cenv}/documents/{docID...}`
  - `GET /{cenv}/documents/search?q=...`
- Templates
  - `GET /{cenv}/templates` (list template docs)
  - `POST /{cenv}/templates/preview` (render with provided data)
- JS Endpoints
  - `/{cenv}/js/{path...}` execute QuickJS script from `_hbf_endpoints`

Deliverables
- `internal/api/*.c` handlers, shared JSON utilities, request parsing, uniform error responses.

Tests
- Black-box HTTP tests for happy paths and auth failures.


## Phase 8: WebSockets

Goal
- Provide WS endpoint(s) with CivetWeb, dispatch messages into QuickJS handlers, and optional auth.

Tasks
- Route: `/{cenv}/ws/{channel}` with token query `?token=...` or Authorization during upgrade.
- On open/close/message events, call JS handler exported from a script or configured channel handler in `_hbf_endpoints`.
- Broadcast helper and per-channel subscriptions in C, with backpressure.

Deliverables
- `internal/ws/ws.c|h` and QuickJS glue `hbf:ws` for server-initiated sends.

Tests
- Connect, auth, echo, broadcast to N clients, resilience on disconnect.


## Phase 9: Packaging, Static Linking, and Ops

Goal
- Produce small, statically linked binary with reproducible builds.

Tasks
- Bazel configs:
  - `:hbf` (musl 100% static) target; ship a single fully static binary
  - Strip symbols; embed build info/version
- CLI flags:
  - `--port`, `--storage_dir`, `--dev`, `--db_max_open`, `--qjs_mem_mb`, `--qjs_timeout_ms`
- Logging: levels, structured JSON logs option.
- Default security headers (except HTTPS); CORS toggle.

Deliverables
- `//:hbf` (musl 100% static) Bazel target
- Release docs with size expectations

Tests
- Smoke test binary on a clean container; verify no runtime deps when using musl.


## Phase 10: Hardening & Perf

Goal
- Stability, safety, and throughput improvements.

Tasks
- Connection cache/LRU for cenv SQLite handles; close idle DBs.
- Rate limiting and request body size limits; template and JS execution quotas.
- Precompiled SQL statements for hot paths; WAL checkpointing.
- Optional per-cenv thread pools; CivetWeb tuning (num threads, queue len).
- Audit log coverage for admin actions.

Deliverables
- Config toggles and metrics endpoints (`/{cenv}/admin/metrics`).

Tests
- Load tests; memory leak checks (ASan/Valgrind); fuzz parsers (URLs, JSON, EJS).


## QuickJS Integration Details (Reference)

- Runtime/context lifecycle: create per-request context or a pool keyed by cenv; set interrupt handler to abort long scripts.
- Module loader hook: implement `JSModuleLoaderFunc` to resolve `import` from DB or embedded C arrays. For CommonJS, use a bootstrap `require` implemented in JS using `eval` in a new function scope.
- Host bindings should validate SQL and parameters; always use sqlite prepared statements; convert NULL/int64/double/text/blob properly.
- Error reporting: surface JS stack traces to logs; client sees sanitized messages.


## SQLite Build Flags

- `-DSQLITE_THREADSAFE=1`
- `-DSQLITE_OMIT_LOAD_EXTENSION`
- `-DSQLITE_ENABLE_FTS5`
- `-DSQLITE_ENABLE_JSON1` (optional)
- `-DSQLITE_DEFAULT_SYNCHRONOUS=1`
- `-DSQLITE_DEFAULT_CACHE_SIZE=-16000` (optional tuning)


## CivetWeb Configuration

- Disable TLS (HTTPS out of scope)
- Set `num_threads`, `request_timeout_ms`, `websocket_timeout_ms`, `enable_keep_alive`
- Register request handlers via `mg_set_request_handler`
- WebSockets via `mg_set_websocket_handler`


## JSON Utilities

- Use a small permissive JSON library (e.g., cJSON under MIT) or write a thin wrapper around QuickJS JSON.parse/stringify for request/response processing to minimize code size.


## Bazel Overview (Sketch)

Root `BUILD.bazel`
```
# Bazel 8 with bzlmod (MODULE.bazel), no WORKSPACE
cc_binary(
  name = "hbf",
  srcs = [
    "internal/core/main.c",
    # ... other src groups
  ],
  deps = [
    "//internal/http:server",
    "//internal/cenv:manager",
    "//internal/db:schema",
    "//internal/auth:auth",
    "//internal/authz:authz",
    "//internal/document:document",
    "//internal/qjs:engine",
    "//third_party/sqlite:sqlite3",
    "//third_party/civetweb:civetweb",
    "//third_party/quickjs:quickjs",
    "//third_party/argon2:argon2",
    "//third_party/sha256_hmac:sha256_hmac",
  ],
  copts = ["-std=c99", "-O2", "-fdata-sections", "-ffunction-sections"],
  linkopts = ["-static", "-Wl,--gc-sections"],
  visibility = ["//visibility:public"],
)
```

MODULE.bazel (sketch)
```
module(name = "hbf", version = "0.1.0")

# Select a musl toolchain module or a custom toolchain defined in-repo
# bazel_dep(name = "rules_cc", version = "...")
# bazel_dep(name = "rules_musl", version = "...")

# use_extension(...) to register the musl toolchain
# use_repo(...) to expose toolchain repos if needed
```

Third-party (examples)
```
cc_library(
  name = "sqlite3",
  srcs = ["sqlite3.c"],
  hdrs = ["sqlite3.h"],
  copts = [
    "-DSQLITE_THREADSAFE=1",
    "-DSQLITE_ENABLE_FTS5",
    "-DSQLITE_OMIT_LOAD_EXTENSION",
  ],
)

cc_library(
  name = "civetweb",
  srcs = glob(["src/*.c"]),
  copts = ["-DUSE_WEBSOCKET"],
)

cc_library(
  name = "quickjs",
  srcs = ["quickjs.c", "libregexp.c", "libunicode.c", "cutils.c", "quickjs-libc.c"],
  copts = ["-DCONFIG_BIGNUM"],
)
```

JS packer rule (concept)
```
# Takes js sources and produces a C source with unsigned char[] blob
# to embed into the binary; loader reads from this map by virtual path.
```


## Security Notes

- HTTPS is out of scope; recommend reverse proxy termination if needed.
- Use Argon2id for password hashing; configurable params; constant-time compare.
- JWT: Base64url encode/decode; sign with HMAC_SHA256; store only SHA256(JWT) in sessions for revocation; secrets per process (env override); rotate via restart.
- Strict query parameterization; reject raw string interpolation.
- Sandbox QuickJS: memory/time limits, no host FS/network except explicit shims.


## Milestone Cheatsheet

- M1: Server + routing + cenv manager + schema init (Phases 1–2)
- M2: Auth (Argon2id, JWT), sessions, admin endpoints (Phase 3–4)
- M3: Documents + search (Phase 5)
- M4: QuickJS runtime + DB API + dynamic endpoints (Phase 6)
- M5: EJS + templates + preview (Phase 6.1)
- M6: Node-compat loader + fs shim + module packer (Phase 6.2)
- M7: WebSockets (Phase 8)
- M8: Static packaging + hardening (Phases 9–10)


## Acceptance & Verification per Phase

For each phase:
- Build: PASS – Bazel builds `//:hbf` (musl toolchain, fully static)
- Lint/Type: PASS – compile with `-Wall -Wextra` later phases
- Tests: PASS – unit + integration suites for the touched components using a minunit-style macro harness


## License Inventory (No GPL)

- SQLite amalgamation – Public Domain
- CivetWeb – MIT
- QuickJS – MIT
- EJS (vendored, trimmed) – MIT
- Argon2 reference – Apache-2.0
- cJSON (if used) – MIT
- In-house code – MIT (recommended)
 - SHA-256/HMAC (single-file) – MIT (Brad Conte style)


## Appendix: Minimal Contracts

Authentication
- Inputs: username, password
- Outputs: JWT (HS256), expiry, user info
- Errors: invalid credentials, disabled user, cenv not found

DB API (QuickJS)
- `query(sql:string, params:array)` → array<object>
- `execute(sql:string, params:array)` → { rows_affected:int, last_insert_id:int|null }
- Errors: SQL error (message suppressed/trimmed), permission denied

EJS
- `render(id|string, data, loader?)` → html string
- Errors: parse/render error with location

WebSockets
- Connect `/{cenv}/ws/{channel}?token=...`
- JS handlers: `on_open(ctx)`, `on_message(ctx, data)`, `on_close(ctx)`


---

That’s the end-to-end plan to deliver HBF with a similar feature set to WCE, implemented in C99, embeddable, and built with Bazel—prioritizing static linking, safety, and a clean path for pure-JS module reuse without Node at runtime.
