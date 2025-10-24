read internal/db/BUILD.bazel

not done: DOCS/revised_phase3_plan.md

make instructions for claude about how the static content build works
there are files in static/
they should be appended to the schema.sql 
the file object names are json with the name property being the path relative to ./static/)
schema is translated into c code, so that it is compiled into the bin (bazel)
when the bin is started, it looks for a database if one is given on the cli
otherwise, it opens the last database written to in ./henvs
otherwise it exits

i would like to make a migration from the current database file handling and sql
schema, to something new. anything which is there now related to the schema, how
it is used in the application, how it is queried, and how it is compiled into
the application should be flagged for removal. the new system will be designed
as such. write a plan called db_plan.md which includes all the relevant things
in the current implementation to be flagged for removal, and detailed
implementation plan for the following:

files under fs/ will be archived in a sqlite db using sqlar using a command like
- sqlite3 fs.db -Ac fs/*
- https://sqlite.org/sqlar.html
- https://sqlite.org/src/file/ext/misc/sqlar.c
- the database should be vaccumed and shrunk before creating the archive to
  reduce size of the binary if possible
- this is done at build time by bazel
- this could be a genrule which creates fs.db

the binary database file (fs.db) is converted to fs.c
- this is another genrule in bazel
- use something like xxd -i fs.db > fs.c
- fs.c gets compiled into the hbf binary

this can be run in memory mode like:
```c
sqlite3 *db;
sqlite3_open("file:memdb1?mode=memory&cache=shared", &db);
sqlite3_exec(db, (char*)dump_sql, NULL, NULL, NULL);
```

or it can be written to the filesystem and used persistently. the default is
persistent mode. tests should set a flag to use inmem database. a cli flag inmem
can be used for easy script testing.

the layout of the fs/ directory is notable

fs/static/{some_path}/{some_filename}
- i expect, but do not know how the archive works, that the name/id in the sqlar
  table will be static/{some_path}/{some_filename}
- this will be a handler for civetweb

the http static file server handler receives a request for
/{some_path}/{some_filename} and it prepends /static/ to the requested url and try to
serve it if it is availble with a query like 

```sql
SELECT name, mode, datetime(mtime,'unixepoch'), sqlar_uncompress(data,sz)
  FROM sqlar
 WHERE ...;
```
if the file is found in the sqlar table, civetweb creates the response and no
quickjs js data needs be created.

fs/hbf/ 
contains files which are needed for hbf itself, primarily the server.js
which is loaded into the quickjs context as the http handler. this can be edited
by the user via REST CRUD, but is not served as a static file






> no do not manually run the inject_content.sh script. this is handled in db internal/db/BUILD.bazel

● Summary

  I've successfully diagnosed and fixed all the issues you requested:

  ✅ Completed Tasks:

  1. Removed hardcoded /tmp/hbf_test.db path - No more references to /tmp filesystem
  2. Implemented proper database path generation - Uses ./henvs/{random_id}.db pattern with DNS-safe hash
  3. Added --db_path CLI argument - Database path can be specified from command line
  4. Uncommented and fixed schema initialization - hbf_db_init_schema() now runs on startup
  5. Added detailed error logging in pool.c - Script loading failures now show the actual JavaScript error and fail fast with clear messages
  6. Created standalone test (internal/qjs/standalone_test.c) - Proves that context reuse is safe and global variables persist correctly across multiple JS_Eval() calls

  🔍 Root Cause Analysis:

  The original error "app is undefined in global context" was caused by:
  - Schema initialization was commented out in main.c
  - Database had no nodes table
  - Script loading failed silently with "no such table: nodes"
  - Contexts were initialized but app was never defined
  - Requests failed with 503 because app was undefined

  📊 Current Status:

  BEFORE: 503 Service Unavailable (app undefined)NOW: Scripts load successfully! ✅REMAINING: "Maximum call stack size exceeded" error (re-entrancy bug in handler)

run tools/integration_test.sh and see the following: The "Maximum call stack
size exceeded" is a separate issue related to how app.handle() is being called
from the C code, which matches the git commit messages you saw earlier about
handling re-entrancy.


# HBF - Web Compute Environment

A single, statically linked C99 web compute environment embedding SQLite, CivetWeb, and QuickJS-NG for runtime extensibility. HBF provides isolated user pods with programmable routing, document storage, and full-text search—all in one self-contained binary with zero runtime dependencies.

## 🚀 Project Status

**Current Phase**: Phase 2b Complete ✅ (Multi-Tenancy Ready)

- ✅ **Phase 0**: Foundation, Bazel setup, musl toolchain, coding standards
- ✅ **Phase 1**: HTTP server with CivetWeb, logging, CLI parsing, signal handling
- ✅ **Phase 2a**: SQLite integration, database schema (document-graph + system tables, FTS5)
- ✅ **Phase 2b**: User pod & connection management (multi-tenancy infrastructure)
- 🔄 **Phase 3**: Routing & Authentication (next: Argon2id, JWT HS256, `/new` and `/login` endpoints)

See completion reports in [DOCS/](DOCS/) for detailed phase documentation.

## 📋 What is HBF?

HBF (abbreviation intentionally minimal) is a **multi-tenant web compute environment** designed for:

- **Self-hosted web applications** with isolated user environments
- **Programmable APIs** via user-owned JavaScript routers (QuickJS-NG)
- **Document-centric storage** with SQLite + FTS5 full-text search
- **Edge deployment** with a single ~1MB static binary
- **Zero-dependency operation** - no npm, no Node.js, no external databases

**Key Design Philosophy**: One binary, zero configuration, maximum capability.

## ✨ Core Features

### Currently Implemented (Phase 0-2b)

#### Multi-Tenancy Infrastructure
- ✅ **User Pod Isolation** - Each user gets their own directory and SQLite database
- ✅ **Connection Caching** - LRU cache with 100 connection limit, thread-safe
- ✅ **Automatic Schema Init** - New pods get full database schema on creation
- ✅ **Secure Permissions** - Directories 0700, databases 0600
- ✅ **Hash-Based Naming** - DNS-safe 8-character user identifiers

#### Database Layer
- ✅ **SQLite 3.50.4** - WAL mode, FTS5 full-text search, JSON1 support
- ✅ **Document-Graph Schema** - Nodes, edges, tags for flexible data modeling
- ✅ **System Tables** - Users, sessions, permissions, policies, audit logs
- ✅ **Full-Text Search** - FTS5 with porter stemming, external content tables
- ✅ **Transaction Support** - ACID guarantees, foreign key constraints

#### HTTP Server
- ✅ **CivetWeb v1.16** - Embedded HTTP server (no HTTPS, use reverse proxy)
- ✅ **Graceful Shutdown** - Clean signal handling (SIGINT, SIGTERM)
- ✅ **Health Endpoint** - `GET /health` with uptime and status
- ✅ **Structured Logging** - Levels (debug/info/warn/error) with UTC timestamps

#### Build & Quality
- ✅ **100% Static Linking** - musl toolchain, 1.1 MB stripped binary
- ✅ **Zero Dependencies** - No glibc, no runtime libraries
- ✅ **53+ Test Cases** - Comprehensive coverage across 5 test suites
- ✅ **CERT C Compliance** - clang-tidy checks, strict C99
- ✅ **Warnings as Errors** - 30+ compiler flags enforced

### Coming Soon (Phase 3+)

- 🔄 **QuickJS-NG Runtime** - Embedded JavaScript engine with memory/timeout limits
- 🔄 **Express.js-Style Routing** - Programmable routes via `server.js` from database
- 🔄 **Static File Serving** - Content stored in SQLite, served via JavaScript middleware
- 🔄 **Authentication** - Argon2id password hashing, JWT HS256 tokens
- 🔄 **Authorization** - Table-level permissions, row-level security policies
- 🔄 **Document API** - CRUD operations with FTS5 search
- 🔄 **EJS Templates** - Server-side rendering in QuickJS
- 🔄 **WebSocket Support** - Real-time communication channels

## 🏗️ Architecture

### Design Constraints

**Language**: Strict C99 (no C++, no extensions)
**Style**: Linux kernel coding style + CERT C Coding Standard
**Licenses**: MIT/BSD/Apache-2.0/Public Domain only (no GPL)
**Linking**: 100% static with musl (no glibc)
**Security**: No HTTPS in binary (use Traefik/Caddy/nginx as reverse proxy)

### Binary Characteristics

```
Size:         1.1 MB (stripped, statically linked)
Toolchain:    musl 1.2.3
Dependencies: ZERO runtime libraries
Target:       x86_64 Linux (easily portable)
```

### Directory Structure

```
/third_party/              # Vendored dependencies (no git submodules)
  civetweb/                # ✅ MIT - HTTP server (v1.16)
  sqlite3/                 # ✅ Public Domain - Database (v3.50.4)
  quickjs-ng/              # 🔄 MIT - JS engine (Phase 3)
  simple_graph/            # 🔄 MIT - Document store (Phase 5)
  argon2/                  # 🔄 Apache-2.0 - Password hashing (Phase 4)
  sha256_hmac/             # 🔄 MIT - JWT signing (Phase 4)

/internal/                 # Core implementation (all C99)
  core/                    # ✅ Logging, config, CLI, hash, main
  http/                    # ✅ CivetWeb server wrapper
  henv/                    # ✅ User pod management + connection cache
  db/                      # ✅ SQLite wrapper, schema, transactions
  qjs/                     # 🔄 QuickJS engine, bindings (Phase 3)
  auth/                    # 🔄 Argon2id, JWT, sessions (Phase 4)
  authz/                   # 🔄 Permissions, row policies (Phase 4)
  document/                # 🔄 Document store + FTS5 (Phase 5)
  templates/               # 🔄 EJS rendering (Phase 6.1)
  ws/                      # 🔄 WebSocket handlers (Phase 8)
  api/                     # 🔄 REST endpoints (Phase 7)

/static/                   # Build-time content (injected into DB)
  server.js                # 🔄 Express-style routing script (Phase 3)
  lib/                     # 🔄 router.js, static.js middleware (Phase 3)
  www/                     # 🔄 index.html, CSS, client assets (Phase 3)

/tools/                    # Build tools
  inject_content.sh        # 🔄 Static content → SQL (Phase 3)
  sql_to_c.sh              # ✅ Schema → C byte array (Phase 2a)

/DOCS/                     # Documentation
  coding-standards.md      # C99 style guide
  development-setup.md     # Devcontainer setup
  phase*-completion.md     # Completion reports
  phase3.md                # Phase 3 detailed plan
  schema_doc_graph.md      # Database schema design
```

**Legend**: ✅ Implemented | 🔄 Planned

## 🚦 Quick Start

### Prerequisites

- Bazel 8.4.2+
- clang-tidy-18 (for linting)
- Linux x86_64 (devcontainer available)

### Build

```bash
# Build the static binary
bazel build //:hbf

# Run all tests
bazel test //...

# Run lint checks
bazel run //:lint
```

### Run

```bash
# Start server (default port 5309)
./bazel-bin/hbf

# Custom configuration
./bazel-bin/hbf --port 8080 --storage_dir ./data --log_level debug

# Test health endpoint
curl http://localhost:5309/health
# {"status":"ok","version":"0.1.0","uptime_seconds":5}

# Graceful shutdown
# Ctrl+C or kill -SIGTERM <pid>
```

### CLI Options

```bash
hbf [options]

Options:
  --port <num>         HTTP server port (default: 5309)
  --storage_dir <path> Directory for user pod storage (default: ./henvs)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show this help message
```

## 🧪 Testing

```bash
# Run all test suites
bazel test //...

# Run specific test with output
bazel test //internal/henv:manager_test --test_output=all

# Run tests in a specific package
bazel test //internal/db:all
```

**Current Test Coverage** (53+ test cases):
- ✅ Hash generator (5 tests)
- ✅ Config parser (25 tests)
- ✅ Database wrapper (7 tests)
- ✅ Schema initialization (9 tests)
- ✅ User pod manager (12 tests)

All tests pass with `-Werror` and CERT C checks enabled.

## 📐 Database Schema

### Document-Graph Model

HBF uses a flexible document-graph model for data storage:

```sql
nodes      -- Atomic/composite entities (JSON body)
  ├── id, type, body, name, version
  ├── created_at, modified_at
  └── Virtual columns: name, version (from JSON)

edges      -- Directed relationships
  ├── src, dst, rel (relationship type)
  └── props (JSON properties)

tags       -- Hierarchical labels
  ├── tag, node_id, parent_tag_id
  └── order_num (sibling ordering)

nodes_fts  -- FTS5 full-text search (porter stemming)
```

### System Tables (Prefix: `_hbf_`)

```
_hbf_users              -- User accounts + Argon2id hashes
_hbf_sessions           -- JWT session tracking
_hbf_table_permissions  -- Table-level ACLs
_hbf_row_policies       -- Row-level security (SQL conditions)
_hbf_config             -- Per-pod configuration
_hbf_audit_log          -- Admin action audit trail
_hbf_schema_version     -- Schema versioning
```

**Total**: 11 tables, all with foreign keys, indexes, and FTS5 integration.

See [DOCS/schema_doc_graph.md](DOCS/schema_doc_graph.md) for detailed schema documentation.

## 📊 Quality Metrics

### Code Quality

```bash
$ bazel run //:lint
✅ 8 source files checked, 0 issues
   - All CERT C checks passing
   - Cognitive complexity < 25
   - No unsafe casts or conversions
```

### Compiler Flags (Enforced)

```c
-std=c99 -Wall -Wextra -Werror -Wpedantic
-Wconversion -Wdouble-promotion -Wshadow -Wformat=2
-Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes
-Wmissing-declarations -Wcast-qual -Wwrite-strings -Wcast-align
-Wundef -Wswitch-enum -Wswitch-default -Wbad-function-cast
```

### Build Results

```bash
$ bazel build //:hbf && bazel test //...
✅ Build: SUCCESS (1.1 MB binary)
✅ Tests: 5/5 test suites (53+ cases)
✅ Lint:  8/8 source files
```

## 🗺️ Roadmap

### Phase 3: JavaScript Runtime & Express-Style Routing (Next)
- QuickJS-NG integration with memory/timeout limits
- Express.js-compatible API (`app.get()`, `app.post()`, etc.)
- Load `server.js` from database at startup
- Static content serving from database (nodes table)
- Build-time content injection (`tools/inject_content.sh`)
- Context pooling for performance (16 contexts × 64 MB)

### Phase 4: Authentication & Authorization
- Argon2id password hashing
- JWT HS256 token generation
- Session management
- Table-level permission system
- Row-level security policies
- Query rewriting for RLS

### Phase 5: Document Store
- CRUD operations via REST API
- Hierarchical document IDs (`pages/home`, `api/users`)
- FTS5 full-text search with BM25 ranking
- Binary content support
- Content-Type handling

### Phase 6.1: Template Rendering
- EJS-compatible engine in QuickJS
- Template storage in SQLite
- HTML auto-escaping

### Phase 6.2: Node Compatibility
- CommonJS module loader
- `fs` shim backed by SQLite
- Minimal Node.js API shims
- Pure JS modules only (no native addons)

### Phase 7: HTTP API Surface
- Complete REST API endpoints
- Admin UI with Monaco editor
- Document management
- User management

### Phase 8-10: Polish & Deploy
- WebSocket support
- Packaging & optimization
- Performance tuning & hardening
- Production deployment guides

See [hbf_impl.md](hbf_impl.md) for complete phase-by-phase implementation details.

## 🔒 Security Model

### Current (Phase 2b)
- ✅ File permissions: directories 0700, databases 0600
- ✅ Hash-based user isolation (8-char DNS-safe)
- ✅ Thread-safe connection caching
- ✅ Foreign key constraints enforced
- ✅ Transaction support with rollback

### Planned (Phase 3+)
- 🔄 Argon2id password hashing (constant-time compare)
- 🔄 JWT HS256 with session revocation
- 🔄 Table-level permissions
- 🔄 Row-level security policies
- 🔄 QuickJS sandboxing (memory limits, timeouts)
- 🔄 Audit logging for admin actions

**Note**: HTTPS is out of scope—use a reverse proxy (Traefik, Caddy, nginx) for TLS termination.

## 🧑‍💻 Development

### Devcontainer

This project includes a custom devcontainer with:
- Bazel 8.4.2
- clang-tidy-18 (LLVM 18)
- musl toolchain 1.2.3
- Go, Python (uv), Hugo

See [DOCS/development-setup.md](DOCS/development-setup.md) for details.

### Coding Standards

HBF follows strict C99 with:
- **Linux kernel coding style** (tabs, K&R braces, 100-col limit)
- **CERT C Coding Standard** (security-focused rules)
- **musl libc principles** (minimalism, defined behavior)

See [DOCS/coding-standards.md](DOCS/coding-standards.md) for complete guidelines.

### Contributing

This is currently a solo project in active development. The implementation follows a strict phased approach with comprehensive testing and quality gates at each stage.

If you're interested in contributing, please:
1. Read the phase completion reports in [DOCS/](DOCS/)
2. Review [hbf_impl.md](hbf_impl.md) for the implementation plan
3. Check [CLAUDE.md](CLAUDE.md) for AI assistant guidance
4. Ensure all tests pass and lint checks are clean

## 📚 Documentation

### Implementation Guides
- [hbf_impl.md](hbf_impl.md) - Complete 10-phase implementation plan
- [CLAUDE.md](CLAUDE.md) - AI assistant guidance for working with this codebase

### Completion Reports
- [DOCS/phase0-completion.md](DOCS/phase0-completion.md) - Foundation & Bazel setup
- [DOCS/phase1-completion.md](DOCS/phase1-completion.md) - HTTP server with CivetWeb
- [DOCS/phase2b-completion.md](DOCS/phase2b-completion.md) - User pod & connection management

### Reference
- [DOCS/coding-standards.md](DOCS/coding-standards.md) - C99 style guide
- [DOCS/development-setup.md](DOCS/development-setup.md) - Devcontainer setup
- [DOCS/schema_doc_graph.md](DOCS/schema_doc_graph.md) - Database schema design

## 📦 Dependencies

All dependencies are vendored (no git submodules) and have permissive licenses:

| Dependency | License | Version | Purpose |
|------------|---------|---------|---------|
| **SQLite** | Public Domain | 3.50.4 | Embedded database |
| **CivetWeb** | MIT | 1.16 | HTTP server |
| **QuickJS-NG** | MIT | (Phase 6) | JavaScript engine |
| **Simple-graph** | MIT | (Phase 5) | Document store |
| **Argon2** | Apache-2.0 | (Phase 3) | Password hashing |

In-house implementations (MIT):
- SHA-256 + HMAC for JWT signing
- Base64url encoding/decoding
- Node.js API shims

**Zero GPL code** - all dependencies are MIT/BSD/Apache-2.0/Public Domain.

## 🎯 Design Principles

1. **Single Binary Deployment** - Everything in one static executable
2. **Multi-Tenancy First** - Isolated user pods with separate databases
3. **Programmable by Default** - User-owned JavaScript routing
4. **SQLite as Universal Store** - Documents, code, config, all in one DB
5. **Zero External Dependencies** - No npm, Node, or package managers at runtime
6. **Security Through Simplicity** - Minimal attack surface, clear trust boundaries
7. **C99 Strictness** - No UB, no compiler extensions, no shortcuts

## 📜 License

MIT License - See individual source files for SPDX headers.

Third-party dependencies retain their original licenses:
- SQLite: Public Domain
- CivetWeb: MIT
- QuickJS-NG: MIT
- Simple-graph: MIT
- Argon2: Apache-2.0

## 🔗 References

- [SQLite](https://sqlite.org/) - Embedded database engine
- [CivetWeb](https://github.com/civetweb/civetweb) - Embedded HTTP server
- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) - Compact JavaScript engine
- [Simple-graph](https://github.com/dpapathanasiou/simple-graph) - Document storage abstraction
- [musl libc](https://musl.libc.org/) - Lightweight C library for static linking
- [Bazel](https://bazel.build/) - Build system
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard) - Security guidelines

---

**Status**: Active development • **Phase**: 2b/10 Complete • **Binary Size**: 1.1 MB • **Test Coverage**: 53+ cases
