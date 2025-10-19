# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HBF is a single, statically linked C99 web compute environment that embeds:
- **SQLite** as the application/data store (with WAL + FTS5)
- **CivetWeb** for HTTP and WebSockets
- **QuickJS** for runtime extensibility and scripting

This is a **planning-stage repository**. The comprehensive implementation plan is in `hbf_impl.md`, but actual source code has not yet been written.

## Build System

This project uses **Bazel 8 with bzlmod** (MODULE.bazel, no WORKSPACE).

### Expected Build Commands (when implemented)
```bash
# Build the static binary (musl toolchain)
bazel build //:hbf

# Run all tests
bazel test //...

# Run lint checks
bazel build //:lint
```

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

## Planned Directory Structure

```
/third_party/       # Vendored dependencies (no git submodules)
  quickjs/          # MIT
  sqlite/           # Public Domain amalgamation
  civetweb/         # MIT
  argon2/           # Apache-2.0
  sha256_hmac/      # MIT single-file implementation

/internal/          # Core implementation (all C99)
  core/             # Logging, config, CLI argument parsing
  http/             # CivetWeb server, routing, request handling
  cenv/             # Compute environment manager (SQLite DB per cenv)
  db/               # SQLite wrapper, schema, prepared statements
  auth/             # Argon2id password hashing, JWT HS256, sessions
  authz/            # Table permissions, row policies, query rewriting
  document/         # Document store with FTS5 full-text search
  qjs/              # QuickJS engine, module loader, DB bindings
  templates/        # EJS template integration
  ws/               # WebSocket handlers
  api/              # REST endpoint implementations

/tools/
  pack_js/          # Bazel helper to bundle JS into C arrays or SQLite blobs
```

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

1. **Phase 0**: Foundation (Bazel setup, musl toolchain, directory structure)
2. **Phase 1-1.3**: HTTP server + cenv routing + SQLite DB management
3. **Phase 2**: Schema & system table initialization
4. **Phase 3**: Authentication (Argon2id + JWT HS256)
5. **Phase 4**: Authorization & row-level policies
6. **Phase 5**: Document store + FTS5 search
7. **Phase 6**: QuickJS embedding + dynamic endpoints
8. **Phase 6.1**: EJS template rendering in QuickJS
9. **Phase 6.2**: Node-compatible module system (CommonJS + shims)
10. **Phase 7**: REST API surface (parity with WCE reference)
11. **Phase 8**: WebSocket support
12. **Phase 9**: Packaging & static linking optimization
13. **Phase 10**: Hardening & performance tuning

Each phase has specific deliverables, tests, and acceptance criteria.

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

## HTTP API Endpoints (Planned)

### System
- `GET /health` - Health check
- `POST /new` - Create new cenv + owner user

### Authentication
- `POST /{cenv}/login` - User login, returns JWT

### Admin (owner/admin only)
- `GET/POST/DELETE /{cenv}/admin/permissions`
- `GET/POST /{cenv}/admin/policies`
- `GET/POST/DELETE /{cenv}/admin/endpoints`
- `GET /{cenv}/admin/metrics` (Phase 10)

### Documents
- `GET /{cenv}/documents` - List documents
- `POST /{cenv}/documents` - Create document
- `GET/PUT/DELETE /{cenv}/documents/{docID}`
- `GET /{cenv}/documents/search?q=...` - FTS5 search

### Templates
- `GET /{cenv}/templates` - List template documents
- `POST /{cenv}/templates/preview` - Render with data

### Dynamic Endpoints
- `/{cenv}/js/{path}` - Execute QuickJS script from `_hbf_endpoints`

### WebSockets
- `/{cenv}/ws/{channel}?token=...` - WebSocket connection with auth

## CLI Arguments (Planned)

```bash
hbf [options]

Options:
  --port <num>              HTTP server port (default: 5309)
  --storage_dir <path>      Directory for cenv SQLite DBs (default: ./cenvs)
  --log_level <level>       Log level: debug, info, warn, error
  --dev                     Development mode
  --db_max_open <num>       Max open SQLite connections
  --qjs_mem_mb <num>        QuickJS memory limit in MB
  --qjs_timeout_ms <num>    QuickJS execution timeout in ms
```

## Development Environment

- **Container**: Custom devcontainer with Bazel, Go, Python (uv), Hugo
- **Ports**: 80 (web services), 8080 (Traefik dashboard)
- **VS Code extensions**: Python, Black formatter, JSON, Markdown, reStructuredText

## Key Design Decisions

1. **Single binary deployment**: All dependencies statically linked, no installation required
2. **Multi-tenancy via cenvs**: Each compute environment is an isolated SQLite database
3. **No HTTPS in binary**: Expect reverse proxy (nginx, Traefik) for TLS termination
4. **Pure JS modules only**: No native Node addons, no npm at runtime
5. **SQLite as universal store**: App data, documents, FTS index, module cache, config all in one DB
6. **Stateless + stateful auth**: JWT for stateless auth, DB sessions for revocation
7. **Query rewriting for authz**: Row-level security via SQL WHERE clause injection

## References

- **Primary spec**: `hbf_impl.md` (comprehensive phase-by-phase implementation guide)
- **WCE reference**: Design mirrors WCE's implementation approach, adapted to C99
