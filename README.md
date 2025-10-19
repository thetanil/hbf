# HBF - Web Compute Environment

A single, statically linked C99 web compute environment embedding SQLite, CivetWeb, and QuickJS-NG for runtime extensibility.

## Project Status

**Current Phase**: Phase 1 Complete ✅

- ✅ **Phase 0**: Foundation, Bazel setup, musl toolchain, coding standards
- ✅ **Phase 1**: HTTP server with CivetWeb, logging, CLI parsing, signal handling
- 🔄 **Phase 2**: User pod & database management (next)

See [DOCS/phase0-completion.md](DOCS/phase0-completion.md) and [DOCS/phase1-completion.md](DOCS/phase1-completion.md) for detailed completion reports.

## Goals

Build a self-contained web server that provides:

- **Single binary deployment** - 100% static linking with musl (no runtime dependencies)
- **Multi-tenancy** - Isolated user pods with separate SQLite databases
- **Programmable routing** - User-owned JavaScript router with QuickJS-NG
- **Document storage** - SQLite-backed documents with full-text search (FTS5)
- **Template rendering** - EJS templates running in QuickJS
- **WebSocket support** - Real-time communication via CivetWeb
- **Strong security** - Argon2id password hashing, JWT (HS256), row-level policies

## Design Constraints

### Language & Standards
- **Strict C99** - No C++, no compiler extensions
- **Linux kernel coding style** - Tabs, K&R braces, 100-column limit
- **CERT C Coding Standard** - Security-focused rules enforced via clang-tidy
- **Warnings as errors** - All code compiles with `-Werror` and 30+ warning flags

### Licensing
**No GPL dependencies**. Only MIT, BSD-2, Apache-2.0, or Public Domain:
- SQLite (Public Domain)
- CivetWeb (MIT)
- QuickJS-NG (MIT)
- Simple-graph (MIT)
- Argon2 (Apache-2.0)

### Binary Characteristics
- **Static linking only** - musl toolchain, no glibc builds
- **Small footprint** - Current binary: 170 KB (stripped)
- **Zero dependencies** - No runtime libraries required
- **Security** - No HTTPS in binary (use reverse proxy)

## Build System

**Bazel 8 with bzlmod** (MODULE.bazel, no WORKSPACE)

```bash
# Build the static binary (musl toolchain)
bazel build //:hbf

# Run all tests
bazel test //...

# Run lint checks (clang-tidy with CERT rules)
bazel run //:lint
```

**Binary output**: `bazel-bin/internal/core/hbf` (170 KB, fully static)

## Quick Start

```bash
# Build
bazel build //:hbf

# Run server
./bazel-bin/internal/core/hbf --port 8080

# Test health endpoint
curl http://localhost:8080/health
# {"status":"ok","version":"0.1.0","uptime_seconds":5}

# Shutdown with Ctrl+C (graceful)
```

## Current Features (Phase 1)

### HTTP Server
- CivetWeb v1.16 integration (MIT license)
- Signal handling (SIGINT, SIGTERM) for graceful shutdown
- Request logging with timestamps
- Keep-alive connections
- 4 worker threads, 10-second timeout

### Endpoints
- `GET /health` - Health check with uptime
- `404` - JSON error response for unmatched routes

### Configuration
```bash
hbf [options]

Options:
  --port <num>         HTTP server port (default: 5309)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show this help message
```

### Logging
Structured logging with levels and UTC timestamps:
```
[2025-10-19 19:39:07] [INFO] HBF v0.1.0 starting
[2025-10-19 19:39:07] [INFO] HTTP server started on port 5309
```

## Testing

All phases include comprehensive testing:

```bash
# Run all tests (currently: hash_test, config_test)
bazel test //...

# Run specific test with output
bazel test //internal/core:config_test --test_output=all
```

**Current test coverage**:
- Hash generator (5 test cases)
- Config parser (25 test cases)

## Code Quality

### Compiler Flags (All Enforced)
```
-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion
-Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition
-Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings
-Wcast-align -Wundef -Wswitch-enum -Wswitch-default -Wbad-function-cast
```

### Static Analysis
- **clang-tidy** with CERT C checks enabled
- **Warnings as errors** across all targets
- **clang-format** for consistent style

```bash
# Run all quality checks
bazel run //:lint
```

## Architecture

### Directory Structure
```
/third_party/          # Vendored dependencies
  civetweb/            # HTTP server (fetched from Git)
  sqlite/              # (Phase 2)
  simple_graph/        # (Phase 5)
  quickjs-ng/          # (Phase 6)
  argon2/              # (Phase 3)

/internal/             # Core implementation
  core/                # Logging, config, CLI, hash generator
  http/                # CivetWeb server wrapper
  henv/                # User pod management (Phase 2)
  db/                  # SQLite wrapper (Phase 2)
  auth/                # Authentication (Phase 3)
  authz/               # Authorization (Phase 4)
  document/            # Document store (Phase 5)
  qjs/                 # QuickJS engine (Phase 6)
  templates/           # EJS templates (Phase 6.1)
  ws/                  # WebSockets (Phase 8)
  api/                 # REST API (Phase 7)

/tools/                # Build tools
  lint.sh              # clang-tidy wrapper
```

### Current Components

**Phase 0 (Foundation)**:
- `internal/core/hash.c|h` - DNS-safe hash generator (SHA-256 → base36)
- Bazel build system with musl toolchain
- Coding standards documentation
- Quality enforcement (.clang-format, .clang-tidy)

**Phase 1 (HTTP Server)**:
- `internal/core/config.c|h` - CLI argument parsing
- `internal/core/log.c|h` - Logging with levels
- `internal/core/main.c` - Application lifecycle
- `internal/http/server.c|h` - CivetWeb wrapper
- `third_party/civetweb/` - Git repository integration

## Planned Features

See [hbf_impl.md](hbf_impl.md) for the complete 10-phase implementation plan:

- **Phase 2**: User pods & SQLite database management
- **Phase 3**: Routing & authentication (Argon2id, JWT HS256)
- **Phase 4**: Authorization & row-level security policies
- **Phase 5**: Document store with FTS5 full-text search
- **Phase 6**: QuickJS-NG embedding & user-owned router.js
- **Phase 6.1**: EJS template rendering
- **Phase 6.2**: Node-compatible module system (CommonJS, fs shim)
- **Phase 7**: Complete REST API surface
- **Phase 8**: WebSocket support
- **Phase 9**: Packaging & optimization
- **Phase 10**: Hardening & performance tuning

## Documentation

- [hbf_impl.md](hbf_impl.md) - Complete implementation guide
- [CLAUDE.md](CLAUDE.md) - AI assistant guidance
- [DOCS/coding-standards.md](DOCS/coding-standards.md) - C99 coding standards
- [DOCS/phase0-completion.md](DOCS/phase0-completion.md) - Phase 0 completion report
- [DOCS/phase1-completion.md](DOCS/phase1-completion.md) - Phase 1 completion report
- [DOCS/development-setup.md](DOCS/development-setup.md) - Development environment setup

## Development Environment

This project uses a custom devcontainer with:
- Bazel 8.4.2
- clang-tidy-18 (LLVM 18)
- musl toolchain 1.2.3
- Go, Python (uv), Hugo

## License

MIT License - See individual files for SPDX headers.

Third-party dependencies:
- SQLite: Public Domain
- CivetWeb: MIT
- QuickJS-NG: MIT
- Simple-graph: MIT
- Argon2: Apache-2.0

## Contributing

This is a solo project in active development. The implementation follows a strict phased approach with comprehensive testing and quality gates at each stage.

## References

- [CivetWeb](https://github.com/civetweb/civetweb) - Embedded HTTP server
- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) - JavaScript engine
- [Simple-graph](https://github.com/dpapathanasiou/simple-graph) - Document storage
- [SQLite](https://sqlite.org/) - Embedded database
- [musl libc](https://musl.libc.org/) - Lightweight C library for static linking
