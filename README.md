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

- 🔄 **Host-Based Routing** - `{user-hash}.domain.com` → user pod
- 🔄 **Authentication** - Argon2id password hashing, JWT HS256 tokens
- 🔄 **User Pod Creation** - `POST /new` endpoint for self-service signup
- 🔄 **Authorization** - Table-level permissions, row-level security policies
- 🔄 **QuickJS-NG Runtime** - Sandboxed JavaScript execution for user routes
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
  simple_graph/            # 🔄 MIT - Document store (Phase 5)
  quickjs-ng/              # 🔄 MIT - JS engine (Phase 6)
  argon2/                  # 🔄 Apache-2.0 - Password hashing (Phase 3)
  sha256_hmac/             # 🔄 MIT - JWT signing (Phase 3)

/internal/                 # Core implementation (all C99)
  core/                    # ✅ Logging, config, CLI, hash, main
  http/                    # ✅ CivetWeb server wrapper
  henv/                    # ✅ User pod management + connection cache
  db/                      # ✅ SQLite wrapper, schema, transactions
  auth/                    # 🔄 Argon2id, JWT, sessions (Phase 3)
  authz/                   # 🔄 Permissions, row policies (Phase 4)
  document/                # 🔄 Document store + FTS5 (Phase 5)
  qjs/                     # 🔄 QuickJS engine, router (Phase 6)
  templates/               # 🔄 EJS rendering (Phase 6.1)
  ws/                      # 🔄 WebSocket handlers (Phase 8)
  api/                     # 🔄 REST endpoints (Phase 7)

/DOCS/                     # Documentation
  coding-standards.md      # C99 style guide
  development-setup.md     # Devcontainer setup
  phase*-completion.md     # Completion reports
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

### Phase 3: Routing & Authentication (Next)
- Host-based routing: `{user-hash}.domain.com`
- User pod creation: `POST /new`
- Login endpoint: `POST /login`
- Argon2id password hashing
- JWT HS256 token generation
- Session management

### Phase 4: Authorization
- Table-level permission system
- Row-level security policies
- Query rewriting for RLS
- Audit logging

### Phase 5: Document Store
- CRUD operations via REST API
- Hierarchical document IDs (`pages/home`, `api/users`)
- FTS5 full-text search with BM25 ranking
- Binary content support
- Content-Type handling

### Phase 6: Programmable Runtime
- QuickJS-NG embedded engine
- User-owned `router.js` for custom routing
- Host modules: `hbf:db`, `hbf:http`, `hbf:router`
- Memory limits and execution timeouts
- Monaco editor for in-browser development

### Phase 6.1: Template Rendering
- EJS-compatible engine in QuickJS
- Template storage in SQLite
- HTML auto-escaping

### Phase 6.2: Node Compatibility
- CommonJS module loader
- `fs` shim backed by SQLite
- Minimal Node.js API shims
- Pure JS modules only (no native addons)

### Phase 7-10: Polish & Deploy
- Complete REST API surface
- WebSocket support
- Packaging & optimization
- Performance tuning & hardening

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
