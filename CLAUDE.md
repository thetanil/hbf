# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HBF is a single, statically linked C99 web compute environment that embeds:
- **SQLite** as the application/data store (with WAL + FTS5)
- **CivetWeb** for HTTP and WebSockets
- **QuickJS-NG** for runtime extensibility and scripting

**Current Status**: Phase 1 Complete ✅
- ✅ Phase 0: Foundation, Bazel setup, musl toolchain, coding standards
- ✅ Phase 1: HTTP server with CivetWeb, logging, CLI parsing, signal handling
- 🔄 Phase 2: User pod & database management (next)

The comprehensive implementation plan is in `hbf_impl.md`. See `DOCS/phase0-completion.md` and `DOCS/phase1-completion.md` for completion reports.

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

**Binary output**: `bazel-bin/internal/core/hbf` (170 KB stripped, 205 KB unstripped)

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
  sqlite/           # 🔄 Public Domain amalgamation (Phase 2)
  simple_graph/     # 🔄 MIT (Phase 5)
  quickjs-ng/       # 🔄 MIT (Phase 6)
  argon2/           # 🔄 Apache-2.0 (Phase 3)
  sha256_hmac/      # 🔄 MIT single-file implementation (Phase 3)

/internal/          # Core implementation (all C99)
  core/             # ✅ Logging, config, CLI, hash generator, main
  http/             # ✅ CivetWeb server wrapper
  henv/             # 🔄 User pod management (Phase 2)
  db/               # 🔄 SQLite wrapper, schema, prepared statements (Phase 2)
  auth/             # 🔄 Argon2id password hashing, JWT HS256 (Phase 3)
  authz/            # 🔄 Table permissions, row policies (Phase 4)
  document/         # 🔄 Document store with FTS5 search (Phase 5)
  qjs/              # 🔄 QuickJS-NG engine, module loader (Phase 6)
  templates/        # 🔄 EJS template integration (Phase 6.1)
  ws/               # 🔄 WebSocket handlers (Phase 8)
  api/              # 🔄 REST endpoint implementations (Phase 7)

/tools/
  lint.sh           # ✅ clang-tidy wrapper script
  pack_js/          # 🔄 JS bundler (Phase 6.2)

/DOCS/              # ✅ Documentation
  coding-standards.md
  development-setup.md
  phase0-completion.md
  phase1-completion.md
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
3. 🔄 **Phase 2**: User Pod & Database Management (SQLite, simple-graph, schema init)
4. 🔄 **Phase 3**: Routing & Authentication (host/path routing, Argon2id, JWT HS256)
5. 🔄 **Phase 4**: Authorization & row-level policies
6. 🔄 **Phase 5**: Document store + FTS5 search (simple-graph integration)
7. 🔄 **Phase 6**: QuickJS-NG embedding + user router.js
8. 🔄 **Phase 6.1**: EJS template rendering
9. 🔄 **Phase 6.2**: Node-compatible module system (CommonJS + shims)
10. 🔄 **Phase 7**: REST API surface
11. 🔄 **Phase 8**: WebSocket support
12. 🔄 **Phase 9**: Packaging & static linking optimization
13. 🔄 **Phase 10**: Hardening & performance tuning

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

### Current (Phase 1)
```bash
hbf [options]

Options:
  --port <num>         HTTP server port (default: 5309)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show this help message
```

### Planned (Later Phases)
```bash
  --storage_dir <path>      Directory for henv SQLite DBs (default: ./henvs)
  --base_domain <domain>    Base domain for routing (default: ipsaw.com)
  --db_max_open <num>       Max open SQLite connections (Phase 2)
  --qjs_mem_mb <num>        QuickJS memory limit in MB (Phase 6)
  --qjs_timeout_ms <num>    QuickJS execution timeout in ms (Phase 6)
```

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

### Completed (Phase 0 & 1)

**Core Components**:
- ✅ `internal/core/hash.c|h` - DNS-safe hash generator (SHA-256 → base36, 8 chars)
- ✅ `internal/core/log.c|h` - Logging with levels (DEBUG, INFO, WARN, ERROR) and UTC timestamps
- ✅ `internal/core/config.c|h` - CLI argument parsing with validation
- ✅ `internal/core/main.c` - Application lifecycle and signal handling
- ✅ `internal/http/server.c|h` - CivetWeb wrapper with graceful shutdown

**Tests**:
- ✅ `internal/core/hash_test.c` - 5 test cases (determinism, DNS-safe chars, collisions, NULL handling)
- ✅ `internal/core/config_test.c` - 25 test cases (all CLI args, edge cases, error handling)

**Binary**:
- ✅ Size: 170 KB (stripped), 205 KB (unstripped)
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
✅ 2 test targets, 30 test cases total, all passing

# Lint
$ bazel run //:lint
✅ 5 source files checked, 0 issues
```

## References

- **Primary spec**: `hbf_impl.md` (comprehensive phase-by-phase implementation guide)
- **Completion reports**: `DOCS/phase0-completion.md`, `DOCS/phase1-completion.md`
- **Coding standards**: `DOCS/coding-standards.md`
- **Development setup**: `DOCS/development-setup.md`
