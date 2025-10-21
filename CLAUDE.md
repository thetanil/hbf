# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HBF is a single, statically linked C99 web compute environment that embeds:
- **SQLite** as the application/data store (with WAL + FTS5)
- **CivetWeb** for HTTP and WebSockets
- **QuickJS-NG** for runtime extensibility and scripting

**Current Status**: Phase 2b Complete ✅
- ✅ Phase 0: Foundation, Bazel setup, musl toolchain, coding standards
- ✅ Phase 1: HTTP server with CivetWeb, logging, CLI parsing, signal handling
- ✅ Phase 2a: SQLite integration, database schema (document-graph + system tables, FTS5)
- ✅ Phase 2b: User pod & connection management (multi-tenancy, connection caching)
- 🔄 Phase 3: Routing & Authentication (next)

The comprehensive implementation plan is in `hbf_impl.md`. See `DOCS/phase0-completion.md`, `DOCS/phase1-completion.md`, and `DOCS/phase2b-completion.md` for completion reports.

## Build System

This project uses **Bazel 8 with bzlmod** (MODULE.bazel, no WORKSPACE).

### Build Commands
```bash
# Build the static binary (musl toolchain)
bazel build //:hbf

# Run all tests (hash_test, config_test)
bazel test //...

# Run lint checks (clang-tidy with CERT rules)
bazel run //:lint

# Run specific test with output
bazel test //internal/core:config_test --test_output=all
```

**Binary output**: `bazel-bin/hbf` (1.1 MB stripped, statically linked with SQLite)

### Bazel Configuration
- `.bazelrc` contains build settings including bzlmod enablement
- Target toolchain: **musl only** (100% static linking, no glibc builds)
- Single output: fully static `hbf` binary

## Architecture & Design Constraints

### Language & Standards
- **Strict C99**: No C++, no language extensions
- Compiler flags enforce warnings-as-errors: `-std=c99 -Wall -Wextra -Werror -Wpedantic`
- Follow CERT C Coding Standard and Linux kernel coding style (approximated via `.clang-format`)
- See `hbf_impl.md` Phase 0 for complete compiler flag list

### Licensing Constraints
**No GPL dependencies allowed**. Only MIT, BSD, Apache-2.0, or Public Domain:
- SQLite (Public Domain)
- CivetWeb (MIT)
- QuickJS (MIT)
- Argon2 (Apache-2.0)
- All other vendored code must be MIT/BSD/Apache-2/PD

### Static Linking Requirements
- Use musl toolchain exclusively
- Link flags: `-static -Wl,--gc-sections`
- Output: single fully static binary with no runtime dependencies
- Strip symbols for release builds

## Directory Structure

```
/third_party/       # Vendored dependencies (no git submodules)
  civetweb/         # ✅ MIT - Fetched from Git (v1.16)
  sqlite3/          # ✅ Public Domain - From Bazel Central Registry (v3.50.4)
  simple_graph/     # 🔄 MIT (Phase 5)
  quickjs-ng/       # 🔄 MIT (Phase 6)
  argon2/           # 🔄 Apache-2.0 (Phase 3)
  sha256_hmac/      # 🔄 MIT single-file implementation (Phase 3)

/internal/          # Core implementation (all C99)
  core/             # ✅ Logging, config, CLI, hash generator, main
  http/             # ✅ CivetWeb server wrapper
  henv/             # ✅ User pod management with connection caching (Phase 2b)
  db/               # ✅ SQLite wrapper, schema, embedded schema.sql (Phase 2a)
  auth/             # 🔄 Argon2id password hashing, JWT HS256 (Phase 3)
  authz/            # 🔄 Table permissions, row policies (Phase 4)
  document/         # 🔄 Document store with FTS5 search (Phase 5)
  qjs/              # 🔄 QuickJS-NG engine, module loader (Phase 6)
  templates/        # 🔄 EJS template integration (Phase 6.1)
  ws/               # 🔄 WebSocket handlers (Phase 8)
  api/              # 🔄 REST endpoint implementations (Phase 7)

/tools/
  lint.sh           # ✅ clang-tidy wrapper script
  sql_to_c.sh       # ✅ SQL to C byte array converter
  pack_js/          # 🔄 JS bundler (Phase 6.2)

/DOCS/              # ✅ Documentation
  coding-standards.md
  development-setup.md
  phase0-completion.md
  phase1-completion.md
  phase2b-completion.md
```

**Legend**: ✅ Implemented | 🔄 Planned

## Security & Crypto Implementation

- **Password hashing**: Argon2id (constant-time compare)
- **JWT**: HS256 using in-house SHA-256 + HMAC (no external crypto libs)
- **Base64url**: Pure C implementation (no dependencies)
- **Session storage**: DB-backed with SHA-256 hashed tokens
- **HTTPS**: Out of scope (use reverse proxy if needed)
- **QuickJS sandboxing**: Memory limits, execution timeouts, no host FS/network access except via explicit shims

## SQLite Configuration

Critical compile-time flags:
```c
-DSQLITE_THREADSAFE=1
-DSQLITE_ENABLE_FTS5
-DSQLITE_OMIT_LOAD_EXTENSION
-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
-DSQLITE_ENABLE_JSON1
```

Runtime pragmas on connection open:
```sql
PRAGMA foreign_keys=ON;
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

## QuickJS Integration

- **Per-cenv runtime/context** (or pooled)
- **Memory limits**: 32-128 MB configurable
- **Execution timeout**: Interrupt handler enforces `_hbf_config.qjs_timeout_ms`
- **Host modules**:
  - `hbf:db` - query/execute with prepared statements
  - `hbf:http` - request/response objects
  - `hbf:util` - JSON, base64, time utilities
- **Node compatibility**: CommonJS wrapper + minimal shims (fs backed by SQLite, path, buffer, util, events, timers)
- **No npm at runtime**: Pure JS modules only, vendored and packed via Bazel

## System Tables (Prefix: `_hbf_`)

All system tables use `_hbf_` prefix:
- `_hbf_users` - User accounts with Argon2id password hashes
- `_hbf_sessions` - JWT session tracking with token hashes
- `_hbf_table_permissions` - Table-level access control
- `_hbf_row_policies` - Row-level security policies (SQL conditions)
- `_hbf_config` - Per-cenv configuration key-value store
- `_hbf_audit_log` - Audit trail for admin actions
- `_hbf_documents` - Document store with binary support
- `_hbf_document_tags` - Document tagging system
- `_hbf_document_search` - FTS5 virtual table for full-text search
- `_hbf_endpoints` - Dynamic QuickJS-powered HTTP endpoints

## Implementation Phases

The project follows a 10-phase implementation plan (see `hbf_impl.md` for details):

1. ✅ **Phase 0**: Foundation (Bazel setup, musl toolchain, directory structure, DNS-safe hash)
2. ✅ **Phase 1**: HTTP Server Bootstrap (CivetWeb, logging, CLI parsing, signal handling)
3. ✅ **Phase 2a**: SQLite Integration & Database Schema (document-graph model, system tables, FTS5)
4. 🔄 **Phase 2b**: User Pod & Connection Management (multi-tenancy, connection caching)
5. 🔄 **Phase 3**: Routing & Authentication (host/path routing, Argon2id, JWT HS256)
6. 🔄 **Phase 4**: Authorization & row-level policies
7. 🔄 **Phase 5**: Document store + FTS5 search (simple-graph integration)
8. 🔄 **Phase 6**: QuickJS-NG embedding + user router.js
9. 🔄 **Phase 6.1**: EJS template rendering
10. 🔄 **Phase 6.2**: Node-compatible module system (CommonJS + shims)
11. 🔄 **Phase 7**: REST API surface
12. 🔄 **Phase 8**: WebSocket support
13. 🔄 **Phase 9**: Packaging & static linking optimization
14. 🔄 **Phase 10**: Hardening & performance tuning

Each phase has specific deliverables, tests, and acceptance criteria. See completion reports in `DOCS/` for finished phases.

## Testing Strategy

- **Unit tests**: minunit-style C macros with assert.h
- **Integration tests**: Black-box HTTP endpoint testing
- **Build verification**: All tests must pass with `-Werror`
- **Linting**: clang-tidy with CERT checks enabled
- **Formatting**: clang-format (Linux kernel style approximation)
- **CI**: lint → build → test pipeline
- **Load testing**: Phase 10 includes benchmarking and memory leak checks (ASan/Valgrind)

## Code Quality Requirements

### Compiler Warnings (Enforced)
```
-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion
-Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition
-Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings
-Wcast-align -Wundef -Wswitch-enum -Wswitch-default -Wbad-function-cast
```

### Style Guidelines
- Linux kernel coding style (tabs, 8-space indent, 80-100 column limit)
- musl libc style principles (minimalism, no UB, simple macros)
- CERT C Coding Standard (applicable C99 rules)
- `.clang-format` and `.clang-tidy` configurations enforce these rules

## HTTP API Endpoints

### Current (Phase 1)
- ✅ `GET /health` - Health check with uptime
- ✅ `404` - JSON error response for unmatched routes

### Planned

**System** (apex domain):
- 🔄 `POST /new` - Create new user pod + owner (Phase 3)

**User Pod** (at {user-hash}.domain):
- 🔄 `POST /login` - User login, returns JWT (Phase 3)
- 🔄 `GET /_admin` - Monaco editor UI (Phase 6)

**Admin API** (owner/admin only):
- 🔄 `GET/POST/DELETE /_api/permissions` (Phase 4)
- 🔄 `GET/POST /_api/policies` (Phase 4)
- 🔄 `GET/POST/PUT/DELETE /_api/documents/{docID}` (Phase 5)
- 🔄 `GET /_api/documents/search?q=...` - FTS5 search (Phase 5)
- 🔄 `GET /_api/templates` - List templates (Phase 6.1)
- 🔄 `POST /_api/templates/preview` - Render with data (Phase 6.1)
- 🔄 `GET /_api/metrics` - Per-henv stats (Phase 10)

**User Routes** (via router.js):
- 🔄 All other paths handled by user's router.js (Phase 6)
- 🔄 Default: serve from document database by path (Phase 7)

**WebSockets**:
- 🔄 `/{user-hash}/ws/{channel}?token=...` (Phase 8)

## CLI Arguments

### Current (Phase 2b)
```bash
hbf [options]

Options:
  --port <num>         HTTP server port (default: 5309)
  --storage_dir <path> Directory for user pod storage (default: ./henvs)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show this help message
```

### Planned (Later Phases)
```bash
  --base_domain <domain>    Base domain for routing (default: ipsaw.com) (Phase 3)
  --qjs_mem_mb <num>        QuickJS memory limit in MB (Phase 6)
  --qjs_timeout_ms <num>    QuickJS execution timeout in ms (Phase 6)
```

**Note**: Connection limit is hardcoded to 100 in Phase 2b; may become configurable in Phase 10.

## Development Environment

- **Container**: Custom devcontainer with Bazel 8.4.2, Go, Python (uv), Hugo
- **Tools**: clang-tidy-18 (LLVM 18), musl toolchain 1.2.3
- **Ports**: 80 (web services), 8080 (Traefik dashboard)
- **VS Code extensions**: Python, Black formatter, JSON, Markdown, reStructuredText

See [DOCS/development-setup.md](DOCS/development-setup.md) for detailed setup instructions.

## Key Design Decisions

1. **Single binary deployment**: All dependencies statically linked, no installation required
2. **Multi-tenancy via cenvs**: Each compute environment is an isolated SQLite database
3. **No HTTPS in binary**: Expect reverse proxy (nginx, Traefik) for TLS termination
4. **Pure JS modules only**: No native Node addons, no npm at runtime
5. **SQLite as universal store**: App data, documents, FTS index, module cache, config all in one DB
6. **Stateless + stateful auth**: JWT for stateless auth, DB sessions for revocation
7. **Query rewriting for authz**: Row-level security via SQL WHERE clause injection

## Current Implementation Status

### Completed (Phase 0, 1, 2a, & 2b)

**Core Components**:
- ✅ `internal/core/hash.c|h` - DNS-safe hash generator (SHA-256 → base36, 8 chars)
- ✅ `internal/core/log.c|h` - Logging with levels (DEBUG, INFO, WARN, ERROR) and UTC timestamps
- ✅ `internal/core/config.c|h` - CLI argument parsing with validation (port, storage_dir, log_level, dev)
- ✅ `internal/core/main.c` - Application lifecycle, signal handling, henv manager integration
- ✅ `internal/http/server.c|h` - CivetWeb wrapper with graceful shutdown
- ✅ `internal/db/db.c|h` - SQLite wrapper with WAL, foreign keys, transactions
- ✅ `internal/db/schema.c|h` - Schema initialization with embedded SQL
- ✅ `internal/db/schema.sql` - Document-graph + system tables + FTS5
- ✅ `internal/henv/manager.c|h` - User pod management with LRU connection caching

**Database Schema** (11 tables):
- ✅ `nodes` - Document-graph nodes with JSON body
- ✅ `edges` - Directed relationships
- ✅ `tags` - Hierarchical labeling
- ✅ `nodes_fts` - FTS5 full-text search with porter stemming
- ✅ `_hbf_users`, `_hbf_sessions`, `_hbf_table_permissions`, `_hbf_row_policies`
- ✅ `_hbf_config`, `_hbf_audit_log`, `_hbf_schema_version`

**User Pod Management**:
- ✅ Multi-tenancy infrastructure with isolated directories
- ✅ LRU connection cache (max 100 connections, thread-safe)
- ✅ Automatic schema initialization on pod creation
- ✅ Hash collision detection
- ✅ Secure file permissions (directories 0700, databases 0600)

**Tests**:
- ✅ `internal/core/hash_test.c` - 5 test cases
- ✅ `internal/core/config_test.c` - 25 test cases
- ✅ `internal/db/db_test.c` - 7 test cases (pragmas, transactions, etc.)
- ✅ `internal/db/schema_test.c` - 9 test cases (schema init, FTS5, triggers, graph queries)
- ✅ `internal/henv/manager_test.c` - 12 test cases (pod creation, caching, permissions)

**Binary**:
- ✅ Size: 1.1 MB (stripped, statically linked with SQLite)
- ✅ 100% static linking with musl libc 1.2.3
- ✅ Zero runtime dependencies
- ✅ All code passes clang-tidy CERT checks
- ✅ Compiles with `-Werror` and 30+ warning flags

**Endpoints**:
- ✅ `GET /health` - Returns JSON with status, version, uptime
- ✅ `404 Not Found` - JSON error response for unmatched routes

### Quality Gates (All Passing)

```bash
# Build
$ bazel build //:hbf
✅ Build completed successfully

# Test
$ bazel test //...
✅ 5 test targets, 53+ test cases total, all passing
   - hash_test: 5 tests
   - config_test: 25 tests
   - db_test: 7 tests
   - schema_test: 9 tests
   - manager_test: 12 tests

# Lint
$ bazel run //:lint
✅ 8 source files checked, 0 issues
```

## References

- **Primary spec**: `hbf_impl.md` (comprehensive phase-by-phase implementation guide)
- **Completion reports**: `DOCS/phase0-completion.md`, `DOCS/phase1-completion.md`, `DOCS/phase2b-completion.md`
- **Coding standards**: `DOCS/coding-standards.md`
- **Development setup**: `DOCS/development-setup.md`
