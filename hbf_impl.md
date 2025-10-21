# HBF Implementation Guide (C99 + Bazel)

This document proposes a pragmatic, phased plan to build HBF: a single,
statically linked C99 web compute environment that embeds SQLite as the
application/data store, CivetWeb for HTTP + WebSockets, and QuickJS-NG for runtime
extensibility. 

- Language: Strict C99 (no C++)
- Binary: Single statically linked executable with musl toolchain (100% static, no glibc)
- HTTP: CivetWeb (HTTP, no HTTPS in scope), WebSockets supported
- DB: SQLite3 amalgamation with WAL + FTS5; Simple-graph for document store
- Scripting: QuickJS-NG embedded, sandboxed
- User Routing: Express-like router.js per user pod
- Templates: EJS-compatible renderer running inside QuickJS
- Modules: Node-compatible JS module shim (CommonJS + minimal ESM), fs shim backed by SQLite
- Build: Bazel (cc_binary), vendored third_party sources; no GPL dependencies (MIT/BSD/Apache-2/PD only)


## Phase 0: Foundation & Constraints

Goals
- Freeze licensing, build, and dependency constraints
- Establish project layout and Bazel scaffolding

Decisions & Constraints
- Licenses: SQLite (public domain), CivetWeb (MIT), QuickJS-NG (MIT), Simple-graph (MIT),
  EJS (MIT), Argon2 (Apache-2.0), in-house JWT/HMAC (MIT), in-house shims (MIT). No GPL.
- Static linking: Use musl toolchain exclusively to produce a 100% static
  binary; no glibc builds.
- C99 only: compile with `-std=c99`, no extensions; keep warnings-as-errors
  later.
- Security posture: No HTTPS, but WebSockets allowed; strong password hashing
  (Argon2id) and signed JWT (HS256) with small self-contained crypto.

Deliverables
- Directory structure
- DNS-safe hash generator for usernames
- MODULE.bazel (bzlmod, Bazel 8) and Bazel `BUILD` files that compile a hello-world `hbf` cc_binary

Structure
```
/third_party/
  quickjs-ng/        (vendored, MIT - https://quickjs-ng.github.io/quickjs/)
  sqlite/            (amalgamation or pinned source, PD)
  simple_graph/      (vendored, MIT - https://github.com/dpapathanasiou/simple-graph)
  civetweb/          (vendored, MIT)
  argon2/            (ref impl, Apache-2)
  sha256_hmac/       (single-file SHA-256 + HMAC, MIT – Brad Conte style)
/internal/
  core/              (logging, config, cli, hash generator)
  http/              (civetweb server, routing, websockets)
  henv/              (henv manager, user pods, db files)
  db/                (sqlite glue, schema, statements)
  auth/              (argon2, jwt, sessions, middleware)
  authz/             (table perms, row policies, query rewrite)
  document/          (simple-graph integration + FTS5)
  qjs/               (quickjs-ng engine, module loader, db bindings, router bindings)
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


## Phase 1: HTTP Server Bootstrap (CivetWeb Only)

Goal
- Minimal HTTP server with CivetWeb, process lifecycle, config, and basic routing. **NO database work in this phase.**

Tasks
- Vendor CivetWeb source in `third_party/civetweb/`
- CLI/config: `--port` (default 5309), `--log_level`, `--dev`
- CivetWeb integration: start server, request logger, timeouts
- Signal handling for graceful shutdown (SIGINT, SIGTERM)
- Register handlers for:
  - `GET /health` - returns JSON health status
  - Placeholder 404 handler for all other routes
- Logging infrastructure (`internal/core/log.c|h`) with levels (debug, info, warn, error)

Deliverables
- `internal/core/config.c|h` - CLI parsing and config struct
- `internal/core/log.c|h` - Logging with levels and timestamps
- `internal/http/server.c|h` - CivetWeb server lifecycle and callbacks
- `third_party/civetweb/BUILD.bazel` - CivetWeb cc_library
- `GET /health` returns 200 JSON: `{"status": "ok", "version": "0.1.0", "uptime_seconds": N}`

Tests
- Start/stop server without errors
- `/health` endpoint returns expected JSON
- Signal handling (graceful shutdown on SIGINT)
- Invalid port handling
- Concurrent request handling (10+ simultaneous requests)

Acceptance
- `bazel build //:hbf` compiles with CivetWeb linked
- `bazel test //...` passes all tests
- `bazel run //:lint` passes with no warnings
- Server starts, responds to `/health`, shuts down cleanly


## Phase 2a: SQLite Integration & Database Schema

Goal
- Integrate SQLite3 with Bazel and define complete database schema based on document-graph model.

Note: This phase focuses solely on SQLite integration and schema definition. User pod management is deferred to Phase 2b.

Tasks
- Integrate SQLite3 from Bazel registry (or vendored amalgamation if not available)
- SQLite database wrapper (`internal/db/db.c|h`):
  - Open/close database connections
  - Set pragmas: `foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`
  - Execute SQL statements with prepared statements
  - Basic error handling and result mapping
- Schema initialization (`internal/db/schema.c|h`):
  - Document-graph core tables (nodes, edges, tags) from `DOCS/schema_doc_graph.md`
  - System tables for HBF (`_hbf_*` prefix)
  - FTS5 virtual table for full-text search
  - Indexes for performance
  - Schema versioning

### Database Schema (Detailed)

**Document-Graph Core Tables** (based on `DOCS/schema_doc_graph.md`):

```sql
-- Nodes: atomic or composite entities (functions, modules, scenes, documents, etc.)
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,                    -- e.g., "function", "module", "scene", "document"
    body TEXT NOT NULL,                    -- JSON content and metadata
    name TEXT GENERATED ALWAYS AS (json_extract(body, '$.name')) VIRTUAL,
    version TEXT GENERATED ALWAYS AS (json_extract(body, '$.version')) VIRTUAL,
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_nodes_type ON nodes(type);
CREATE INDEX idx_nodes_name ON nodes(name);
CREATE INDEX idx_nodes_created ON nodes(created_at);

-- Edges: directed relationships between nodes
CREATE TABLE edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    src INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    dst INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    rel TEXT NOT NULL,                     -- relationship type: "includes", "calls", "depends-on", "follows", "branch-to", etc.
    props TEXT,                            -- JSON properties: ordering, weights, conditions, etc.
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_edges_src ON edges(src);
CREATE INDEX idx_edges_dst ON edges(dst);
CREATE INDEX idx_edges_rel ON edges(rel);
CREATE INDEX idx_edges_src_rel ON edges(src, rel);

-- Tags: hierarchical labeling for composable documents
CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tag TEXT NOT NULL,                     -- tag identifier
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    parent_tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    order_num INTEGER NOT NULL DEFAULT 0,  -- ordering among siblings
    metadata TEXT,                         -- JSON metadata
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_tags_tag ON tags(tag);
CREATE INDEX idx_tags_node ON tags(node_id);
CREATE INDEX idx_tags_parent ON tags(parent_tag_id);
CREATE INDEX idx_tags_parent_order ON tags(parent_tag_id, order_num);
```

**HBF System Tables**:

```sql
-- User accounts with Argon2id password hashes
CREATE TABLE _hbf_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,           -- Argon2id PHC string format
    email TEXT,
    role TEXT NOT NULL DEFAULT 'user',     -- 'owner', 'admin', 'user'
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_hbf_users_username ON _hbf_users(username);
CREATE INDEX idx_hbf_users_role ON _hbf_users(role);

-- JWT session tracking with token hashes
CREATE TABLE _hbf_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES _hbf_users(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL UNIQUE,       -- SHA-256 hash of JWT
    expires_at INTEGER NOT NULL,
    last_used INTEGER NOT NULL DEFAULT (unixepoch()),
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_hbf_sessions_user ON _hbf_sessions(user_id);
CREATE INDEX idx_hbf_sessions_token ON _hbf_sessions(token_hash);
CREATE INDEX idx_hbf_sessions_expires ON _hbf_sessions(expires_at);

-- Table-level access control
CREATE TABLE _hbf_table_permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES _hbf_users(id) ON DELETE CASCADE,
    table_name TEXT NOT NULL,
    can_select INTEGER NOT NULL DEFAULT 0,
    can_insert INTEGER NOT NULL DEFAULT 0,
    can_update INTEGER NOT NULL DEFAULT 0,
    can_delete INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE UNIQUE INDEX idx_hbf_perms_user_table ON _hbf_table_permissions(user_id, table_name);

-- Row-level security policies (SQL conditions)
CREATE TABLE _hbf_row_policies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    policy_name TEXT NOT NULL,
    role TEXT NOT NULL,                    -- 'owner', 'admin', 'user'
    operation TEXT NOT NULL,               -- 'SELECT', 'INSERT', 'UPDATE', 'DELETE'
    condition TEXT NOT NULL,               -- SQL WHERE clause (e.g., "user_id = $user_id")
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_hbf_policies_table ON _hbf_row_policies(table_name);
CREATE UNIQUE INDEX idx_hbf_policies_name ON _hbf_row_policies(table_name, policy_name);

-- Per-henv configuration key-value store
CREATE TABLE _hbf_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,                   -- JSON value
    description TEXT,
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

-- Audit trail for admin actions
CREATE TABLE _hbf_audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER REFERENCES _hbf_users(id) ON DELETE SET NULL,
    action TEXT NOT NULL,                  -- 'CREATE_USER', 'GRANT_PERMISSION', etc.
    table_name TEXT,
    record_id INTEGER,
    details TEXT,                          -- JSON details
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX idx_hbf_audit_user ON _hbf_audit_log(user_id);
CREATE INDEX idx_hbf_audit_created ON _hbf_audit_log(created_at);
CREATE INDEX idx_hbf_audit_action ON _hbf_audit_log(action);

-- Schema version tracking
CREATE TABLE _hbf_schema_version (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL DEFAULT (unixepoch()),
    description TEXT
);
```

**Full-Text Search**:

```sql
-- FTS5 virtual table for searching node content
CREATE VIRTUAL TABLE nodes_fts USING fts5(
    node_id UNINDEXED,
    type UNINDEXED,
    content,
    content=nodes,
    content_rowid=id,
    tokenize='porter unicode61'
);

-- Triggers to keep FTS index synchronized
CREATE TRIGGER nodes_fts_insert AFTER INSERT ON nodes BEGIN
    INSERT INTO nodes_fts(node_id, type, content)
    VALUES (new.id, new.type, json_extract(new.body, '$.content'));
END;

CREATE TRIGGER nodes_fts_update AFTER UPDATE ON nodes BEGIN
    UPDATE nodes_fts SET content = json_extract(new.body, '$.content')
    WHERE node_id = new.id;
END;

CREATE TRIGGER nodes_fts_delete AFTER DELETE ON nodes BEGIN
    DELETE FROM nodes_fts WHERE node_id = old.id;
END;
```

**Default Configuration Values**:

```sql
-- Insert default config values
INSERT INTO _hbf_config (key, value, description) VALUES
    ('qjs_mem_mb', '64', 'QuickJS memory limit in MB'),
    ('qjs_timeout_ms', '5000', 'QuickJS execution timeout in milliseconds'),
    ('session_lifetime_hours', '168', 'Session lifetime in hours (default: 7 days)'),
    ('max_upload_size_mb', '10', 'Maximum upload size in MB'),
    ('enable_audit_log', 'true', 'Enable audit logging for admin actions');

-- Insert schema version
INSERT INTO _hbf_schema_version (version, description) VALUES
    (1, 'Initial schema: document-graph + HBF system tables');
```

Deliverables
- ✅ SQLite3 from Bazel Central Registry (version 3.50.4)
- ✅ `internal/db/db.c|h` - SQLite wrapper with connection management
- ✅ `internal/db/schema.c|h` - Complete schema DDL and initialization
- ✅ `internal/db/schema.sql` - SQL schema file (embedded as C byte array via tools/sql_to_c.sh)
- ✅ `tools/sql_to_c.sh` - Script to convert SQL to C byte array (avoids C99 string literal limits)

Tests (All Passing ✅)
- ✅ Open database and set pragmas correctly (WAL, foreign_keys, synchronous)
- ✅ Schema initialization creates all tables (11 tables: nodes, edges, tags, _hbf_*)
- ✅ Foreign key constraints enforced
- ✅ WAL mode enabled and verified
- ✅ FTS5 virtual table created and functional with external content
- ✅ Config defaults inserted correctly (5 default values)
- ✅ Schema version tracking works (version 1)
- ✅ Transaction management (begin, commit, rollback)
- ✅ Last insert ID and row change count
- ✅ FTS5 triggers (insert, update, delete) maintain index synchronization
- ✅ Document-graph operations (insert nodes, create edges, add tags, traverse graph)
- ✅ FTS5 full-text search with porter stemming

Acceptance (All Met ✅)
- ✅ `bazel build //internal/db:schema` succeeds with SQLite linked
- ✅ `bazel test //...` passes all tests (4 test targets, 41+ test cases)
  - hash_test: 5 tests
  - config_test: 25 tests
  - db_test: 7 tests
  - schema_test: 9 tests
- ✅ `bazel run //:lint` passes (all 7 C source files clean)
- ✅ Schema creates successfully in empty database
- ✅ All indexes and triggers created
- ✅ Document-graph queries work (nodes, edges, tags verified)
- ✅ FTS5 search functional with content synchronization

**Status: Phase 2a COMPLETE** ✅

Notes:
- SQLite compile-time options configured in `.bazelrc`: SQLITE_THREADSAFE=1, SQLITE_ENABLE_FTS5, SQLITE_OMIT_LOAD_EXTENSION, SQLITE_DEFAULT_WAL_SYNCHRONOUS=1, SQLITE_ENABLE_JSON1
- FTS5 external content table uses `content=nodes, content_rowid=id` with special delete syntax in triggers
- Schema SQL embedded as byte array to avoid C99 4095-char string literal limit
- Database libraries available but not yet integrated into main binary (will be used in Phase 2b for user pod management)


## Phase 2b: User Pod & Connection Management

Goal
- Implement user pod directory management and database connection caching.

Note: This phase builds on Phase 2a's database schema and focuses on multi-tenancy infrastructure.

Tasks
- User pod manager (`internal/henv/manager.c|h`):
  - `henv_create_user_pod(user_hash)` - Create directory and initialize DB
  - `henv_user_exists(user_hash)` - Check if user pod exists
  - `henv_open(user_hash)` - Open database with connection caching
  - `henv_close_all()` - Close all cached connections
  - Hash collision detection
- File structure:
  - User directory: `storage_dir/{user-hash}/` mode 0700
  - Default henv: `storage_dir/{user-hash}/index.db` mode 0600
- Database connection cache (`internal/db/db.c|h`):
  - LRU cache or simple hash map for open connections
  - Mutex protection for thread safety
  - Idle connection timeout
  - Max connections limit
- CLI argument: `--storage_dir <path>` (default: ./henvs)

Deliverables
- `internal/henv/manager.c|h` - User pod management
- Connection caching in `internal/db/db.c|h`
- Tests for user pod creation and management

Tests
- Create user pod → directory created with correct permissions (0700)
- Create index.db → file created mode 0600, schema initialized
- Open existing henv → connection returned from cache
- Open same henv twice → returns cached connection
- Hash collision → error returned
- Close all connections → cache cleared
- Concurrent access to different henvs works

Acceptance
- User pod creation and database initialization works
- Connection caching functional and thread-safe
- Multiple henvs can be opened simultaneously
- All tests pass


## Phase 3: Host + Path Routing & Authentication Endpoints

Goal
- Parse Host header for user-hash routing and implement `/new` and `/login` endpoints with Argon2id + JWT.

Note: This phase combines routing (previously "Phase 1.2") with authentication (previously "Phase 3") since routing is needed for auth endpoints.

### Phase 3.1: Host + Path Routing

Tasks
- Router wrapper (`internal/http/router.c|h`):
  - Parse Host header to extract user-hash (e.g., `a1b2c3d4.ipsaw.com` → `a1b2c3d4`)
  - Handle apex domain (`ipsaw.com`) for system endpoints
  - Hash validation: 8 characters, [0-9a-z] only
  - Provide `hbf_route_context` structure with user_hash, subpath, hostname
- Integration with CivetWeb server for request routing

Deliverables
- `internal/http/router.c|h` with `hbf_route_extract()` and `hbf_validate_hash()`

Tests
- Valid user-hash extraction: `a1b2c3d4.ipsaw.com` → user_hash="a1b2c3d4"
- Apex domain handling: `ipsaw.com/new` → system handler
- Invalid hash → 404
- Hash validation (reject non-alphanumeric, wrong length)

### Phase 3.2: Authentication (Argon2id + JWT HS256)

Tasks
- Vendor Argon2 reference implementation in `third_party/argon2/`
- Vendor SHA-256 + HMAC single-file (Brad Conte style, MIT) in `third_party/sha256_hmac/`
- Password hashing: Argon2id (Apache-2.0 ref impl) with parameters tuned for target box; encode hash with PHC string format.
- JWT: Implement HS256 using MIT SHA-256 + HMAC (single-file, Brad Conte style) and base64url encode/decode (pure C, no external deps).
  - Use HMAC_SHA256 for the HS256 signature over `base64url(header).base64url(payload)`.
  - Implement base64url encoder/decoder in-house (pure C).
  - JWT payload includes `user_hash` to scope token to originating henv.
- Session store: insert session on login with token hash (SHA-256 of the full JWT), expiry from config, rolling `last_used` and expiry updates.
- Endpoints:
  - POST `/new` {username, password, email}:
    - Generate user_hash from username via `hbf_dns_safe_hash()`
    - Check for hash collision (directory exists)
    - Create user pod directory: `{storage_dir}/{user-hash}/`
    - Create `index.db` with initial schema
    - Bootstrap owner user with Argon2id password hash
    - Return `{url: "https://{user-hash}.ipsaw.com", user_hash: "...", admin: {...}}`
  - POST `/{user-hash}.ipsaw.com/login` {username, password}:
    - Verify credentials against henv's `_hbf_users` table
    - Return `{token, expires_at, user}`
- Request auth middleware: extract Bearer token, verify signature + expiry, check session exists, verify user_hash matches request.

Deliverables
- `internal/auth/argon2.c|h`, `internal/auth/jwt.c|h`, `internal/auth/session.c|h`
- `third_party/sha256_hmac/` vendored single-file (MIT) and `internal/crypto/sha256_hmac.c|h` thin wrappers
- HTTP handlers in `internal/api/auth.c` for `/new` and `/login`
- `internal/core/hash.c|h` for `hbf_dns_safe_hash()` (from Phase 0)

Tests
- Hash/verify cycles, JWT sign/verify including expiry
- `/new` creates user pod with correct structure
- Hash collision detection
- Login success/failure, disabled user handling
- JWT scoping to user_hash


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


## Phase 5: Document Store + Search (Simple-graph Integration)

Goal
- CRUD documents using simple-graph, with optional FTS5 search; support binary content and Monaco editor serving.

Tasks
- Integrate simple-graph API for document operations
- REST endpoints: list/create/get/update/delete; hierarchical IDs like `pages/home`, `api/users`
- FTS5 search with BM25 ranking (extend simple-graph if needed)
- Content types: `application/json`, `text/markdown`, `text/html`, `text/ejs`, `text/javascript`, `text/css`, `application/octet-stream` (binary as blob or base64)
- Special document IDs with `_system/` prefix for Monaco editor and admin UI:
  - `_system/monaco/editor.main.js`
  - `_system/monaco/editor.main.css`
  - `_system/admin.html`
  - `_system/router.js` (user's custom router)
- Serve documents with appropriate Content-Type headers

Deliverables
- `internal/document/document.c|h` wrapping simple-graph API
- `internal/api/documents.c` for REST endpoints
- Document serving handler (GET /path → document lookup)
- Monaco editor files embedded as SQL inserts or C arrays

Tests
- CRUD roundtrip, hierarchical ID support
- Search relevance (if FTS5 integrated)
- Binary content path (blob storage)
- Serve Monaco editor files from database
- Content-Type header mapping


## Phase 6: QuickJS-NG Embedding + User Router

Goal
- Sandboxed JS runtime with DB access, HTTP context, and user-owned router.js for custom routing.

Tasks
- Initialize QuickJS-NG runtime/context per request (or a pool) with:
  - Memory limit (e.g., 32–128MB) from `_hbf_config.qjs_mem_mb`
  - Interrupt handler for timeout (from `_hbf_config.qjs_timeout_ms`)
  - Disable dangerous host bindings; no raw file or network by default
- Built-in host modules:
  - `hbf:db` with `query(sql, params)` and `execute(sql, params)` using prepared statements; converts SQLite types to JS
  - `hbf:http` request object (method, path, query, headers, body JSON or raw), and `response(status, headers, body)` builder
  - `hbf:util` JSON helpers, base64, time
  - `hbf:router` Express-like routing API:
    - `router.get(path, handler)`
    - `router.post(path, handler)`
    - `router.put(path, handler)`
    - `router.delete(path, handler)`
    - `router.use(handler)` for middleware/default handler
    - Parameter extraction (e.g., `/posts/:id`)
- User router.js:
  - Load `router.js` from documents (ID: `_system/router.js`)
  - Execute in QuickJS-NG context on each request
  - If no router.js exists, use default handler (serve from document database by path)
  - Errors surfaced as 500 with sanitized message; include `line:col` when possible
- Admin editor endpoint `/_admin` for editing router.js and other documents

Deliverables
- `internal/qjs/engine.c|h` for QuickJS-NG initialization
- `internal/qjs/modules/db.c`, `http.c`, `util.c`, `router.c` for host modules
- `internal/http/router_handler.c` for router.js execution
- Admin UI page (`_system/admin.html`) with Monaco editor

Tests
- Simple router: `router.get('/hello', () => res.json({msg: 'hi'}))`
- DB access from router handler
- Parameter extraction: `/posts/:id`
- Timeout enforced (long-running script aborted)
- Default handler fallback (no router.js → serve documents)
- Monaco editor loads and displays in admin UI


## Phase 6.1: EJS Template Support (in QuickJS-NG)

Goal
- Render EJS templates inside QuickJS-NG, loading templates from SQLite.

Tasks
- Bundle a minimal EJS-compatible engine (MIT) adapted for QuickJS-NG and no Node APIs. If using upstream, vendor a small pure-JS subset; otherwise implement a compact EJS renderer supporting:
  - `<% %>` script, `<%= %>` escape, `<%- %>` raw, includes (`<% include file %>`), locals, partials.
- `templates.render(id | source, data, loader)` where loader reads from simple-graph documents by id or provided source.
- Auto-escape HTML by default; allow raw with `<%- %>`.
- Map `text/ejs` (or `text/html+ejs`) documents to renderer.
- Integrate with router.js: allow templates to be served via router handlers.

Deliverables
- `internal/qjs/modules/ejs.js` (vendored), `internal/templates/ejs.c|h` (glue), DB-backed loader.
- `hbf:templates` module for template rendering from JS

Tests
- Render variables, loops/conditionals, include parent; error messages with line numbers.
- Template rendering from router.js handler


## Phase 6.2: Node-Compatible Module System (No npm/node at runtime)

Goal
- Load pure JS "Node modules" as QuickJS-NG modules with shims and a DB-backed filesystem.

Tasks
- Module loader strategy:
  - Prefer ES modules when available.
  - Provide CommonJS compatibility by wrapping source: `(function(exports, require, module, __filename, __dirname){ ... })` inside QuickJS-NG; maintain module cache.
- `fs` shim backed by SQLite: virtual filesystem that resolves `require('pkg/file')` by reading simple-graph documents under `modules/{pkg}/...` or a packaged module table; also support in-memory C-embedded JS for core shims.
- Provide minimal shims: `fs` (readFileSync, existsSync, stat), `path`, `buffer` (basic), `util`, `events`, `timers` (setTimeout/clearTimeout mapped to QuickJS job queue), `process` (env, cwd, versions minimal).
- Bazel packer: a rule that takes a list of JS sources (from vendor or project) and produces either:
  - a C array (compiled into the binary) addressable by virtual paths, or
  - a bootstrap SQLite image inserted into documents on first init.
- Policy: Only pure JS modules that do not require native addons or Node runtime.

Deliverables
- `internal/qjs/node_compat.js` (require, module cache, shims)
- `tools/pack_js` Bazel rule + small packer (C or Python) to emit C array/SQLite blob
- Integration with simple-graph for module storage

Tests
- Load a simple npm-like package (vendored source) without npm
- Require graph and caching
- fs shim loads from DB (simple-graph documents)
- CommonJS and ES module interop


## Phase 7: HTTP API Surface

Goal
- Implement REST endpoints for document and user management, with router.js handling custom routes.

Endpoints
- System (apex domain: ipsaw.com)
  - GET `/health`
  - POST `/new` (create user pod + owner)
- User Pod (at {user-hash}.ipsaw.com)
  - POST `/login` (authenticate to henv)
  - GET `/_admin` (Monaco editor UI)
  - Admin API (owner/admin only):
    - `GET/POST/DELETE /_api/permissions` (+ list by user)
    - `GET/POST /_api/policies`
    - `GET/POST/PUT/DELETE /_api/documents/{docID...}` (document CRUD)
    - `GET /_api/documents/search?q=...` (FTS5 search)
  - Templates:
    - `GET /_api/templates` (list template docs)
    - `POST /_api/templates/preview` (render with provided data)
- User Routes (via router.js)
  - All other paths handled by user's router.js
  - If no router.js: default handler serves from documents by path

Deliverables
- `internal/api/*.c` handlers, shared JSON utilities, request parsing, uniform error responses
- System endpoint handlers (health, new, login)
- Admin API handlers (documents, permissions, policies)
- Router.js integration (all requests → router.js → response)

Tests
- Black-box HTTP tests for happy paths and auth failures
- System endpoints (health, /new, login)
- Admin API (document CRUD, search)
- Router.js handling (custom routes + default fallback)
- Monaco editor serving


## Phase 8: WebSockets

Goal
- Provide WS endpoint(s) with CivetWeb, dispatch messages into QuickJS-NG handlers, and optional auth.

Tasks
- Route: `/{user-hash}.ipsaw.com/ws/{channel}` with token query `?token=...` or Authorization during upgrade.
- On open/close/message events, call JS handler from router.js or dedicated WebSocket handler.
- Broadcast helper and per-channel subscriptions in C, with backpressure.
- `hbf:ws` module for server-initiated sends from JS.

Deliverables
- `internal/ws/ws.c|h` for WebSocket management
- QuickJS-NG glue `hbf:ws` for server-initiated sends
- WebSocket integration with router.js

Tests
- Connect, auth, echo, broadcast to N clients, resilience on disconnect
- WebSocket handlers in router.js


## Phase 9: Packaging, Static Linking, and Ops

Goal
- Produce small, statically linked binary with reproducible builds.

Tasks
- Bazel configs:
  - `:hbf` (musl 100% static) target; ship a single fully static binary
  - Strip symbols; embed build info/version
- CLI flags:
  - `--port` (default: 5309)
  - `--storage_dir` (default: ./henvs)
  - `--base_domain` (default: ipsaw.com)
  - `--log_level` (default: info)
  - `--dev` (development mode)
  - `--db_max_open` (default: 100)
  - `--qjs_mem_mb` (default: 64)
  - `--qjs_timeout_ms` (default: 5000)
- Logging: levels, structured JSON logs option.
- Default security headers (except HTTPS); CORS toggle.

Deliverables
- `//:hbf` (musl 100% static) Bazel target
- Release docs with size expectations
- Deployment guide (Litestream configuration, k3s manifests, self-hosting instructions)

Tests
- Smoke test binary on a clean container; verify no runtime deps when using musl
- Binary size check (target: <20MB)
- Startup time measurement


## Phase 10: Hardening & Perf

Goal
- Stability, safety, and throughput improvements.

Tasks
- Connection cache/LRU for henv SQLite handles; close idle DBs.
- Rate limiting and request body size limits; template and JS execution quotas.
- Precompiled SQL statements for hot paths; WAL checkpointing.
- Optional per-henv thread pools; CivetWeb tuning (num threads, queue len).
- Per-henv metrics collection (requests, DB queries, JS execution times).

Deliverables
- Config toggles and metrics endpoints (`/_api/metrics` for per-henv stats).
- LRU cache for henv database connections
- Rate limiting middleware

Tests
- Load tests (target: 1000 req/s on moderate hardware)
- Memory leak checks (ASan/Valgrind)
- Fuzz parsers (URLs, JSON, EJS, router.js)
- Concurrent request handling (100+ simultaneous connections)
- Database connection pooling under load


## QuickJS-NG Integration Details (Reference)

- Runtime/context lifecycle: create per-request context or a pool keyed by user-hash/henv-hash; set interrupt handler to abort long scripts.
- Module loader hook: implement `JSModuleLoaderFunc` to resolve `import` from DB (simple-graph documents) or embedded C arrays. For CommonJS, use a bootstrap `require` implemented in JS using `eval` in a new function scope.
- Host bindings should validate SQL and parameters; always use sqlite prepared statements; convert NULL/int64/double/text/blob properly.
- Error reporting: surface JS stack traces to logs; client sees sanitized messages.
- Router.js execution: load script from `_system/router.js` document, execute in sandboxed context per request, return response.


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
    "//internal/henv:manager",
    "//internal/db:schema",
    "//internal/auth:auth",
    "//internal/authz:authz",
    "//internal/document:document",
    "//internal/qjs:engine",
    "//third_party/sqlite:sqlite3",
    "//third_party/simple_graph:simple_graph",
    "//third_party/civetweb:civetweb",
    "//third_party/quickjs-ng:quickjs-ng",
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
    "-DSQLITE_ENABLE_JSON1",
  ],
)

cc_library(
  name = "simple_graph",
  # Integrate simple-graph C code if available, or SQL schema only
  hdrs = ["simple_graph.h"],
)

cc_library(
  name = "civetweb",
  srcs = glob(["src/*.c"]),
  copts = ["-DUSE_WEBSOCKET"],
)

cc_library(
  name = "quickjs-ng",
  srcs = ["quickjs.c", "libregexp.c", "libunicode.c", "cutils.c"],
  copts = ["-DCONFIG_BIGNUM"],
)
```

JS packer rule (concept)
```
# Takes js sources and produces a C source with unsigned char[] blob
# to embed into the binary; loader reads from this map by virtual path.
# Used for Monaco editor, EJS templates, Node shims, etc.
```


## Security Notes

- HTTPS is out of scope; recommend reverse proxy termination (Traefik, Caddy, nginx).
- Use Argon2id for password hashing; configurable params; constant-time compare.
- JWT: Base64url encode/decode; sign with HMAC_SHA256; store only SHA256(JWT) in sessions for revocation; secrets per process (env override); rotate via restart; scope to user-hash.
- Strict query parameterization; reject raw string interpolation.
- Sandbox QuickJS-NG: memory/time limits, no host FS/network except explicit shims.
- Per-henv isolation: each henv is completely independent (separate SQLite DB, no shared state).
- Router.js sandboxing: user code runs in isolated QuickJS context with no access to host system.


## Milestone Cheatsheet

- M1: Server + host/path routing + user pod manager + schema init (Phases 1–2)
- M2: User pod creation, auth (Argon2id, JWT), sessions (Phase 3–4)
- M3: Simple-graph integration + FTS5 search + Monaco editor (Phase 5)
- M4: QuickJS-NG runtime + router.js + host modules (Phase 6)
- M5: EJS templates + admin UI (Phase 6.1)
- M6: Node-compat loader + fs shim backed by documents (Phase 6.2)
- M7: Full REST API + router.js routing (Phase 7)
- M8: WebSockets (Phase 8)
- M9: Static packaging + hardening (Phases 9–10)


## Acceptance & Verification per Phase

For each phase:
- Build: PASS – Bazel builds `//:hbf` (musl toolchain, fully static)
- Lint/Type: PASS – compile with `-Wall -Wextra` later phases
- Tests: PASS – unit + integration suites for the touched components using a minunit-style macro harness


## License Inventory (No GPL)

- SQLite amalgamation – Public Domain
- Simple-graph – MIT
- CivetWeb – MIT
- QuickJS-NG – MIT
- EJS (vendored, trimmed) – MIT
- Argon2 reference – Apache-2.0
- cJSON (if used) – MIT
- In-house code – MIT (recommended)
- SHA-256/HMAC (single-file) – MIT (Brad Conte style)


## Appendix: Minimal Contracts

User Pod Creation
- Endpoint: POST `/new`
- Inputs: username, password, email
- Outputs: `{url: "https://{user-hash}.ipsaw.com", user_hash: "...", admin: {...}}`
- Errors: username taken (hash collision), invalid input

Authentication
- Endpoint: POST `/{user-hash}.ipsaw.com/login`
- Inputs: username, password
- Outputs: JWT (HS256), expiry, user info
- Errors: invalid credentials, disabled user, henv not found

DB API (QuickJS-NG via `hbf:db`)
- `query(sql:string, params:array)` → array<object>
- `execute(sql:string, params:array)` → { rows_affected:int, last_insert_id:int|null }
- Errors: SQL error (message suppressed/trimmed), permission denied

Router API (QuickJS-NG via `hbf:router`)
- `router.get(path, handler)` → register GET route
- `router.post(path, handler)` → register POST route
- `router.use(handler)` → register middleware/default
- `req.params` → path parameters (e.g., `/posts/:id`)
- `res.json(obj)`, `res.send(string)`, `res.status(code)` → response builders

EJS (QuickJS-NG via `hbf:templates`)
- `render(id|string, data, loader?)` → html string
- Errors: parse/render error with location

WebSockets
- Connect `/{user-hash}.ipsaw.com/ws/{channel}?token=...`
- JS handlers: `on_open(ctx)`, `on_message(ctx, data)`, `on_close(ctx)`


---

That's the end-to-end plan to deliver HBF: a self-contained web compute environment with user-owned routing, in-browser development, and complete application isolation—implemented in C99, statically linked, and built with Bazel.
