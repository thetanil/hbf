# HBF Implementation Guide (C99 + Bazel)

This document proposes a pragmatic, phased plan to build HBF: a single,
statically linked C99 web compute environment that embeds SQLite as the
application/data store, CivetWeb for HTTP + WebSockets, and QuickJS-NG for runtime
extensibility.

**Design Philosophy**: Single-instance deployment (localhost or single k3s pod) with programmable routing via JavaScript. All HTTP requests handled by user-defined `server.js` loaded from SQLite database.

- Language: Strict C99 (no C++)
- Binary: Single statically linked executable with musl toolchain (100% static, no glibc)
- HTTP: CivetWeb (HTTP, no HTTPS in scope), WebSockets supported
- DB: SQLite3 amalgamation with WAL + FTS5; Simple-graph for document store
- Scripting: QuickJS-NG embedded, sandboxed (memory/timeout limits)
- Programmable Routing: Express.js-compatible API in JavaScript (`server.js` from database)
- Static Content: Served from SQLite nodes table (injected at build time)
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
  http/              (civetweb server, request handler, websockets)
  henv/              (henv manager, user pods, db files)
  db/                (sqlite glue, schema, statements)
  auth/              (argon2, jwt, sessions, middleware)
  authz/             (table perms, row policies, query rewrite)
  document/          (simple-graph integration + FTS5)
  qjs/               (quickjs-ng engine, module loader, context pool, bindings/)
    bindings/        (request, response, db, ws host modules)
  templates/         (ejs integration helpers)
  ws/                (websocket handlers)
  api/               (REST endpoints, admin endpoints)
/static/             (build-time content injected into database)
  server.js          (Express-style routing script)
  lib/               (router.js, static.js middleware)
  www/               (index.html, style.css, client-side assets)
/tools/
  inject_content.sh  (static/ -> SQL INSERT statements)
  sql_to_c.sh        (schema.sql -> C byte array)
  pack_js/           (Bazel helper to bundle JS -> C array/SQLite blob)

hbf_impl.md
CLAUDE.md
README.md
MODULE.bazel
BUILD.bazel (root)
.bazelrc
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
  - `hbf_henv_create_user_pod(user_hash)` - Create directory and initialize DB
  - `hbf_henv_user_exists(user_hash)` - Check if user pod exists
  - `hbf_henv_open(user_hash)` - Open database with connection caching
  - `hbf_henv_close_all()` - Close all cached connections
  - Hash collision detection
- File structure:
  - User directory: `storage_dir/{user-hash}/` mode 0700
  - Default henv: `storage_dir/{user-hash}/index.db` mode 0600
- Database connection cache (in `internal/henv/manager.c`):
  - LRU cache for open connections
  - Mutex protection for thread safety
  - Max connections limit (configurable, default: 100)
- CLI argument: `--storage_dir <path>` (default: ./henvs)

Deliverables
- ✅ `internal/henv/manager.c|h` - User pod management (378 lines)
- ✅ `internal/henv/manager_test.c` - Test suite (12 tests, 372 lines)
- ✅ `internal/henv/BUILD.bazel` - Build configuration
- ✅ CLI integration in `internal/core/config.c|h` and `internal/core/main.c`

Tests (All Passing ✅)
- ✅ Initialize and shutdown henv manager
- ✅ Create user pod with correct directory/file permissions (0700/0600)
- ✅ Hash collision detection
- ✅ User existence checking
- ✅ Open user pod database
- ✅ Connection caching (same handle returned for repeated opens)
- ✅ Open non-existent user pod (error handling)
- ✅ Multiple user pods simultaneously
- ✅ Close all connections
- ✅ Invalid user hash length validation
- ✅ Get database path formatting
- ✅ Schema initialization verification

Acceptance (All Met ✅)
- ✅ `bazel build //:hbf` succeeds (binary size: 1.1 MB statically linked)
- ✅ `bazel test //...` passes all tests (5 test targets, 53+ test cases)
  - hash_test: 5 tests
  - config_test: 25 tests
  - db_test: 7 tests
  - schema_test: 9 tests
  - manager_test: 12 tests
- ✅ `bazel run //:lint` passes (8 source files, 0 issues)
- ✅ User pod creation and database initialization works
- ✅ Connection caching functional and thread-safe
- ✅ Multiple henvs can be opened simultaneously
- ✅ All tests pass with proper cleanup

**Status: Phase 2b COMPLETE** ✅

Notes:
- Connection cache uses linked list with LRU eviction policy
- Thread safety provided by pthread mutex around all cache operations
- Automatic schema initialization via `hbf_db_init_schema()` on pod creation
- File permissions enforced: directories 0700, database files 0600
- Binary size increased from 205 KB to 1.1 MB due to SQLite integration (expected)
- Manager integrated into main application lifecycle (init on startup, shutdown on exit)
- Storage directory created automatically if not exists
- See `DOCS/phase2b-completion.md` for detailed completion report


## Phase 3: JavaScript Runtime & Express-Style Routing

**Status**: 🔄 Planning
**Dependencies**: Phase 2b (User pod & connection management)
**Goal**: Embed QuickJS-NG and implement Express.js-style routing for dynamic request handling

### Overview

Phase 3 introduces programmable request handling via a JavaScript runtime. Instead of static routes, HBF loads a `server.js` from the database and uses it to handle all HTTP requests through an Express.js-compatible API.

**Key Architectural Shift**:
- **Original Plan**: Authentication first, then routing, then QuickJS (Phases 3→6)
- **Revised Plan**: QuickJS runtime first, with routing and static content serving
- **Reasoning**: Single-instance deployment model initially (localhost or single k3s pod), user-programmable routing is core value proposition, static content database-backed from start

**Single-Instance Model**:
```
HTTP Request → CivetWeb → HBF Request Handler → QuickJS (server.js) → Response
                                                      ↓
                                              Document Database
                                              (nodes table)
```

**Key Components**:
1. **Default Database**: `{storage_dir}/henvs/index.db` (loaded at startup)
2. **Server Script**: Node with `name='server.js'` (loaded into QuickJS)
3. **Static Files**: Nodes with paths like `/www/index.html`
4. **Express.js Clone**: Minimal routing API compatible with QuickJS

### Phase 3.1: QuickJS Integration & Basic Runtime

**Goal**: Embed QuickJS-NG, load `server.js` from database, handle basic requests

Tasks
- Vendor QuickJS-NG in `third_party/quickjs-ng/`
  - Static linking
  - No filesystem access (we provide shims)
  - No network access (we provide shims)
  - Configurable memory limit (default: 64 MB)
  - Interrupt handler for timeout (default: 5000 ms)
- QuickJS engine wrapper (`internal/qjs/engine.c|h`):
  - `hbf_qjs_init(mem_limit_mb, timeout_ms)` - Initialize QuickJS engine
  - `hbf_qjs_shutdown()` - Shutdown engine
  - `hbf_qjs_ctx_create()` - Create runtime context (per-request or pooled)
  - `hbf_qjs_ctx_destroy(ctx)` - Destroy context
  - `hbf_qjs_eval(ctx, code, len)` - Evaluate JavaScript code
  - `hbf_qjs_call_function(ctx, func_name, args_json, result_json)` - Call JS function
  - `hbf_qjs_load_server_js(ctx, db)` - Load server.js from database
- Database loader (`internal/qjs/loader.c|h`):
  - Load server.js from `nodes` table where `json_extract(body, '$.name') = 'server.js'`
  - Cache compiled script for subsequent requests
- Build-time static content injection:
  - Tool: `tools/inject_content.sh` (similar to `tools/sql_to_c.sh`)
  - Generate SQL INSERT statements from `static/` directory
  - Inject into database during schema initialization
  - Directory structure: `static/server.js`, `static/www/index.html`, etc.
- Context pooling (`internal/qjs/pool.c|h`):
  - Pre-create contexts at startup (default: 16 contexts)
  - Acquire/release for request handling
  - Thread-safe via mutex

Deliverables
- `third_party/quickjs-ng/BUILD.bazel` - QuickJS build configuration
- `internal/qjs/engine.c|h` - Runtime wrapper with memory limits
- `internal/qjs/loader.c|h` - Database loader for server.js
- `internal/qjs/pool.c|h` - Context pool for request handling
- `tools/inject_content.sh` - Static content to SQL converter
- `static/server.js` - Minimal server script (hello world)
- `static/www/index.html` - Default homepage
- Build system integration (genrules for content injection)
- Main.c integration (load server.js at startup)

Tests
- QuickJS init/shutdown
- Evaluate simple expression
- Memory limit enforcement (context creation fails at limit)
- Timeout enforcement (long-running scripts interrupted)
- Error handling (JavaScript exceptions caught)
- Load server.js from database
- Load missing server.js (error handling)
- Cache server.js (subsequent loads use cached version)
- Context pool acquire/release

Acceptance
- QuickJS-NG compiles and links statically
- Memory limit enforced (context creation fails at limit)
- Timeout enforced (long-running scripts interrupted)
- server.js loads from `index.db` at startup
- Static content injected into database at build time
- All tests pass (10+ test cases)

### Phase 3.2: Express.js-Compatible Router

**Goal**: Implement minimal Express.js API for route handling in QuickJS

Tasks
- C-to-JS bindings (`internal/qjs/bindings/`):
  - Request object (`bindings/request.c|h`):
    - Create JavaScript request object from CivetWeb request
    - Properties: `method`, `path`, `query`, `params`, `headers`, `body`
    - Parse query string into `req.query` object
    - Parse headers into `req.headers` object
  - Response object (`bindings/response.c|h`):
    - Create JavaScript response object
    - Methods: `res.status(code)`, `res.send(body)`, `res.json(obj)`, `res.set(name, value)`
    - Accumulate response in C struct (`hbf_response_t`)
    - Send to CivetWeb after handler completes
- Router module (`static/lib/router.js`):
  - Express-style routing API in pure JavaScript
  - Methods: `app.get(path, handler)`, `app.post(path, handler)`, `app.put(path, handler)`, `app.delete(path, handler)`, `app.use(handler)`
  - Parameter extraction: `/user/:id` → `req.params.id`
  - Route matching with regex compilation
  - Middleware support for fallback handlers
- Built-in module registration (`internal/qjs/modules.c|h`):
  - Register `hbf:router` as built-in module
  - Load router.js into QuickJS context
  - Export as CommonJS module
- CivetWeb handler integration (`internal/http/handler.c`):
  - Main HTTP handler that delegates to QuickJS
  - Create request/response objects
  - Call `app.handle(req, res)` in JavaScript
  - Handle JavaScript exceptions (500 error)
  - Send response to CivetWeb

Deliverables
- `internal/qjs/bindings/request.c|h` - Request object creation
- `internal/qjs/bindings/response.c|h` - Response object creation
- `static/lib/router.js` - Express-style router implementation
- `internal/qjs/modules.c|h` - Built-in module registration
- `internal/http/handler.c|h` - CivetWeb→QuickJS bridge
- Example `static/server.js` with `/hello` endpoint
- Tests for routing, parameters, middleware

Tests
- Create request object
- Create response object
- Response status/send/json methods
- Route matching: `GET /hello` → handler
- Route parameters: `/user/123` → `req.params.id === "123"`
- Middleware fallback
- 404 handling

Acceptance
- `GET /hello` returns "Hello, World!"
- Route parameters work (`/user/123` → `req.params.id === "123"`)
- Response methods work (`res.status(404).send("Not Found")`)
- Middleware fallback works
- All tests pass (15+ test cases)

### Phase 3.3: Static File Serving from Database

**Goal**: Serve static files from `nodes` table with path-based lookup

Tasks
- Database binding (`internal/qjs/bindings/db.c|h`):
  - `hbf:db` module for database access from JavaScript
  - `db.query(sql, params)` → array of objects
  - `db.execute(sql, params)` → `{rows_affected, last_insert_id}`
  - Convert SQLite types to JavaScript types
  - Prepared statements for SQL injection prevention
- Static file middleware (`static/lib/static.js`):
  - Query database for static files by path
  - Content-Type detection from file extension
  - 404 handling for missing files
  - Integration with router as fallback handler
- Example server.js with static files:
  - Dynamic routes (e.g., `/hello`, `/api/user/:id`)
  - Static file fallback (serves from `/www/*` in database)
  - 404 handler

Deliverables
- `internal/qjs/bindings/db.c|h` - Database query from JS
- `static/lib/static.js` - Static file middleware
- `static/www/index.html` - Example homepage
- `static/www/style.css` - Example stylesheet
- Complete `static/server.js` with all features
- Tests for static file serving

Tests
- Serve static HTML
- Serve static CSS with correct Content-Type
- Static file 404
- Database query from JS
- Content-Type headers

Acceptance
- `GET /` serves `index.html` from database
- `GET /style.css` serves CSS with correct Content-Type
- `GET /nonexistent` returns 404
- Database queries work from JavaScript (`hbf:db`)
- All tests pass (10+ test cases)

**Status: Phase 3 READY FOR IMPLEMENTATION** 🔄

Notes:
- QuickJS context pooling: 16 contexts × 64 MB = 1 GB max (configurable)
- Binary size expected: < 2 MB (QuickJS adds ~800 KB)
- Request latency target: < 5ms for simple routes
- Memory limit enforced per-context via `JS_NewRuntime()`
- Timeout enforced via `JS_SetInterruptHandler()`
- Security: No filesystem access, no network access (only `hbf:db` and request/response)
- Authentication and multi-tenant routing deferred to Phase 4


## Phase 4: Authentication & Authorization

Goal
- Implement authentication (Argon2id + JWT HS256) and authorization (table/row-level policies).

Note: Authentication moved from original Phase 3, now builds on top of Phase 3's QuickJS runtime.

### Phase 4.1: Authentication (Argon2id + JWT HS256)

Tasks
- Vendor Argon2 reference implementation in `third_party/argon2/`
- Vendor SHA-256 + HMAC single-file (Brad Conte style, MIT) in `third_party/sha256_hmac/`
- Password hashing: Argon2id (Apache-2.0 ref impl) with parameters tuned for target box; encode hash with PHC string format
- JWT: Implement HS256 using MIT SHA-256 + HMAC (single-file, Brad Conte style) and base64url encode/decode (pure C, no external deps)
  - Use HMAC_SHA256 for the HS256 signature over `base64url(header).base64url(payload)`
  - Implement base64url encoder/decoder in-house (pure C)
  - JWT payload includes `user_id` (not user_hash, since we're single-instance)
- Session store: insert session on login with token hash (SHA-256 of the full JWT), expiry from config, rolling `last_used` and expiry updates
- Endpoints (exposed via QuickJS server.js):
  - POST `/register` {username, password, email}:
    - Create new user in `_hbf_users` table with Argon2id password hash
    - Return `{user_id, username, role}`
  - POST `/login` {username, password}:
    - Verify credentials against `_hbf_users` table
    - Return `{token, expires_at, user}`
- Request auth middleware (C layer before QuickJS):
  - Extract Bearer token from Authorization header
  - Verify signature + expiry
  - Check session exists in `_hbf_sessions`
  - Inject `req.user` into JavaScript request object

Deliverables
- `internal/auth/argon2.c|h` - Argon2id password hashing wrapper
- `internal/auth/jwt.c|h` - JWT HS256 signing and verification
- `internal/auth/session.c|h` - Session management
- `internal/auth/base64url.c|h` - Base64url encoding/decoding
- `third_party/argon2/BUILD.bazel` - Argon2 build configuration
- `third_party/sha256_hmac/` - Vendored single-file SHA-256 + HMAC (MIT)
- HTTP handlers in `internal/api/auth.c` for `/register` and `/login`
- Auth middleware in `internal/http/auth_middleware.c|h`
- Update `static/server.js` example with auth routes

Tests
- Argon2id hash/verify cycles
- JWT sign/verify including expiry
- Base64url encode/decode round-trip
- `/register` creates user with hashed password
- `/login` success/failure, disabled user handling
- Auth middleware extracts and verifies JWT
- Session management (create, validate, expire)

Acceptance
- Password hashing works with Argon2id PHC format
- JWT tokens signed and verified correctly
- `/register` endpoint creates users
- `/login` endpoint returns valid JWT
- Auth middleware validates tokens before passing to QuickJS
- Sessions tracked in database
- All tests pass (20+ test cases)

### Phase 4.2: Authorization & Row Policies

Goal
- Enforce table-level permissions and row-level policies by query rewriting.

Tasks
- `authz_check_*` helpers (owner/admin implicit full access)
- Store table permissions and policies in DB; CRUD admin endpoints
- Query rewriter: append WHERE fragments for SELECT/UPDATE/DELETE using policies; substitute `$user_id` with caller
- Protect database queries from JavaScript (`hbf:db` module) with permission checks
- Admin endpoints (exposed via QuickJS server.js):
  - `GET/POST/DELETE /admin/permissions` - Manage table permissions
  - `GET/POST /admin/policies` - Manage row-level policies

Deliverables
- `internal/authz/authz.c|h` - Permission checking
- `internal/authz/query_rewrite.c|h` - Query rewriting for row policies
- Admin endpoints in `internal/api/authz.c`
- Integration with `hbf:db` module (permission checks before query execution)
- Update `static/server.js` example with admin routes

Tests
- Unit tests for query rewrite (with existing WHERE/ORDER/LIMIT)
- Permission matrix (user roles vs. table operations)
- Row policy enforcement (query rewriting)
- Admin endpoints (grant/revoke permissions, create policies)


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
- Implement REST endpoints for document and user management, with server.js handling custom routes.

Note: All endpoints exposed via QuickJS server.js (no multi-tenant routing).

Endpoints
- System:
  - GET `/health` - Health check (already implemented in Phase 1)
  - POST `/register` - User registration (from Phase 4.1)
  - POST `/login` - User authentication (from Phase 4.1)
- Admin API (owner/admin only):
  - `GET/POST/DELETE /_api/permissions` - Manage table permissions
  - `GET/POST /_api/policies` - Manage row-level policies
  - `GET/POST/PUT/DELETE /_api/documents/{docID...}` - Document CRUD
  - `GET /_api/documents/search?q=...` - FTS5 search
  - GET `/_api/templates` - List template docs
  - POST `/_api/templates/preview` - Render with provided data
- Developer UI:
  - GET `/_admin` - Monaco editor UI for editing server.js
- User Routes (via server.js):
  - All other paths handled by user's server.js
  - Default handler serves from documents by path

Deliverables
- `internal/api/*.c` handlers, shared JSON utilities, request parsing, uniform error responses
- Admin API handlers (documents, permissions, policies, templates)
- Monaco editor UI (HTML + JavaScript embedded in database)
- server.js integration (all requests → server.js → response)

Tests
- Black-box HTTP tests for happy paths and auth failures
- System endpoints (health, register, login)
- Admin API (document CRUD, search, permissions, policies)
- server.js handling (custom routes + default fallback)
- Monaco editor serving and editing


## Phase 8: WebSockets

Goal
- Provide WS endpoint(s) with CivetWeb, dispatch messages into QuickJS-NG handlers, and optional auth.

Tasks
- Route: `/ws/{channel}` with token query `?token=...` or Authorization header during upgrade
- On open/close/message events, call JS handler from server.js or dedicated WebSocket handler
- Broadcast helper and per-channel subscriptions in C, with backpressure
- `hbf:ws` module for server-initiated sends from JS
- Auth integration: verify JWT before accepting WebSocket connection

Deliverables
- `internal/ws/ws.c|h` for WebSocket management
- QuickJS-NG glue `hbf:ws` for server-initiated sends from JavaScript
- WebSocket integration with server.js (route handlers for WebSocket events)
- Auth middleware for WebSocket upgrade

Tests
- Connect with valid JWT, auth failure with invalid/missing token
- Echo test (client sends message, receives same message back)
- Broadcast to N clients (message sent to all connected clients in channel)
- Resilience on disconnect (cleanup, no memory leaks)
- WebSocket handlers in server.js


## Phase 9: Packaging, Static Linking, and Ops

Goal
- Produce small, statically linked binary with reproducible builds.

Tasks
- Bazel configs:
  - `:hbf` (musl 100% static) target; ship a single fully static binary
  - Strip symbols; embed build info/version
- CLI flags (update from Phase 1):
  - `--port` (default: 5309)
  - `--storage_dir` (default: ./henvs)
  - `--log_level` (default: info)
  - `--dev` (development mode)
  - `--db_max_open` (default: 100) - Max database connections
  - `--qjs_pool_size` (default: 16) - QuickJS context pool size
  - `--qjs_mem_mb` (default: 64) - QuickJS memory limit per context
  - `--qjs_timeout_ms` (default: 5000) - QuickJS execution timeout
- Logging: levels, structured JSON logs option
- Default security headers (except HTTPS); CORS toggle
- Environment variables for secrets (JWT secret, etc.)

Deliverables
- `//:hbf` (musl 100% static) Bazel target
- Release docs with size expectations
- Deployment guide (systemd unit, Docker container, k3s manifests, self-hosting instructions)
- Litestream configuration for SQLite replication

Tests
- Smoke test binary on a clean container; verify no runtime deps when using musl
- Binary size check (target: <5MB including QuickJS)
- Startup time measurement (target: <500ms)
- Memory footprint check


## Phase 10: Hardening & Perf

Goal
- Stability, safety, and throughput improvements.

Tasks
- Connection cache/LRU for SQLite handles; close idle DBs (already implemented in Phase 2b, tune parameters)
- Rate limiting and request body size limits; template and JS execution quotas
- Precompiled SQL statements for hot paths; WAL checkpointing tuning
- CivetWeb tuning (num threads, queue len, keep-alive settings)
- Metrics collection (requests, DB queries, JS execution times, memory usage)
- Security hardening:
  - Input validation (SQL injection, XSS, path traversal)
  - Resource limits (max request size, max connections, max query time)
  - Error message sanitization
- Performance tuning:
  - QuickJS bytecode caching
  - Prepared statement caching
  - Response caching for static content

Deliverables
- Config toggles and metrics endpoint (`/_api/metrics` for system stats)
- Rate limiting middleware (requests per IP, per user)
- Resource quota enforcement
- Performance benchmarks and tuning guide
- Security audit and hardening checklist

Tests
- Load tests (target: 1000 req/s on moderate hardware)
- Memory leak checks (ASan/Valgrind)
- Fuzz parsers (URLs, JSON, EJS, server.js)
- Concurrent request handling (100+ simultaneous connections)
- Database connection pooling under load
- Rate limiting enforcement (block after threshold)
- SQL injection attempts (all blocked)
- XSS attempts (all sanitized)


## QuickJS-NG Integration Details (Reference)

- Runtime/context lifecycle: pre-create context pool at startup; acquire context per request, release after response sent; set interrupt handler to abort long scripts
- Module loader hook: implement `JSModuleLoaderFunc` to resolve `import` from DB (simple-graph documents) or embedded C arrays. For CommonJS, use a bootstrap `require` implemented in JS using `eval` in a new function scope
- Host bindings should validate SQL and parameters; always use sqlite prepared statements; convert NULL/int64/double/text/blob properly
- Error reporting: surface JS stack traces to logs; client sees sanitized messages (no stack traces exposed)
- server.js execution: load script from nodes table (name='server.js'), compile once at startup, execute route handlers in sandboxed context per request, return response


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

- HTTPS is out of scope; recommend reverse proxy termination (Traefik, Caddy, nginx)
- Use Argon2id for password hashing; configurable params; constant-time compare
- JWT: Base64url encode/decode; sign with HMAC_SHA256; store only SHA256(JWT) in sessions for revocation; secrets per process (env override); rotate via restart
- Strict query parameterization; reject raw string interpolation (always use prepared statements)
- Sandbox QuickJS-NG: memory/time limits, no host FS/network except explicit shims (`hbf:db`, `hbf:http`, etc.)
- server.js sandboxing: user code runs in isolated QuickJS context with no access to host system
- Single database instance: `index.db` in storage directory (multi-tenancy not currently supported)


## Milestone Cheatsheet

- M1: Foundation + HTTP server + schema init (Phases 0–2a)
- M2: User pod manager + connection caching (Phase 2b)
- M3: QuickJS-NG runtime + Express.js router + static files from DB (Phase 3)
- M4: Authentication (Argon2id, JWT) + authorization (table/row policies) (Phase 4)
- M5: Simple-graph integration + FTS5 search + Monaco editor (Phase 5)
- M6: EJS templates + admin UI (Phase 6.1)
- M7: Node-compat loader + fs shim backed by documents (Phase 6.2)
- M8: Full REST API + server.js routing (Phase 7)
- M9: WebSockets (Phase 8)
- M10: Static packaging + hardening + performance tuning (Phases 9–10)


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

User Registration
- Endpoint: POST `/register`
- Inputs: username, password, email
- Outputs: `{user_id, username, role}`
- Errors: username taken, invalid input

Authentication
- Endpoint: POST `/login`
- Inputs: username, password
- Outputs: `{token, expires_at, user: {user_id, username, role}}`
- Errors: invalid credentials, disabled user

DB API (QuickJS-NG via `hbf:db`)
- `query(sql:string, params:array)` → array<object>
- `execute(sql:string, params:array)` → {rows_affected:int, last_insert_id:int|null}
- Errors: SQL error (message suppressed/trimmed), permission denied

Router API (QuickJS-NG via `hbf:router`)
- `app.get(path, handler)` → register GET route
- `app.post(path, handler)` → register POST route
- `app.put(path, handler)` → register PUT route
- `app.delete(path, handler)` → register DELETE route
- `app.use(handler)` → register middleware/default handler
- `req.method`, `req.path`, `req.query`, `req.params`, `req.headers`, `req.body` → request properties
- `res.json(obj)`, `res.send(string)`, `res.status(code)`, `res.set(name, value)` → response builders

EJS (QuickJS-NG via `hbf:templates`)
- `render(id|string, data, loader?)` → html string
- Errors: parse/render error with location

WebSockets
- Connect `/ws/{channel}?token=...`
- JS handlers: `on_open(ctx)`, `on_message(ctx, data)`, `on_close(ctx)`


---

That's the end-to-end plan to deliver HBF: a self-contained web compute environment with programmable routing via JavaScript, in-browser development with Monaco editor, and database-backed content storage—implemented in strict C99, statically linked with musl, and built with Bazel.
