# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

HBF is a single, statically linked C99 web compute environment built with Bazel 8.x using bzlmod that embeds:
- **SQLite** as the application/data store (with WAL + FTS5)
- **CivetWeb** for HTTP and SSE (NO SSL, NO TLS, NO IPV6, NO FILES)
- **QuickJS-NG** for runtime extensibility and scripting

**Architecture**: Single static binary that serves a pod (embedded SQLite database with content + JavaScript). Each pod defines its own routing, data model, and features. HBF core provides the runtime; pods provide the application logic.

## Directory Structure

```
hbf/
  shell/      - Server CLI, main, logging, config
  db/         - SQLite wrapper and schema
  http/       - CivetWeb server wrapper
  qjs/        - QuickJS engine and bindings

pods/
  base/       - Base pod with example content
    hbf/      - JavaScript files (server.js)
    static/   - Static assets (HTML, CSS, etc.)

third_party/  - Vendored dependencies (CivetWeb, QuickJS)
tools/        - Build scripts (lint, db_to_c, etc.)
DOCS/         - Documentation
```

## Build System

Uses **Bazel 8 with bzlmod** (MODULE.bazel, no WORKSPACE).

### Build Commands

```bash
# Build default binary
bazel build //:hbf

# Build a specific pod binary (example: test pod)
bazel build //:hbf_test

# Run all tests
bazel test //...

# Run lint checks
bazel run //:lint

# Run specific test with output
bazel test //hbf/shell:config_test --test_output=all
```

**Binary output**:
- `bazel-bin/bin/hbf` (base pod binary)
- Additional pods appear under `bazel-bin/bin/hbf_<pod>` (e.g., `hbf_test`)

Build modes (recommended):
- Release (stripped): `--compilation_mode=opt --strip=always`
- Debug (symbols): `--compilation_mode=dbg`

Tip: set `.bazelrc` so `bazel run` defaults to debug for a better dev experience.

### Bazel Configuration
- `.bazelrc` contains build settings
- **musl toolchain only** (100% static linking)
- Single output layout: all binaries under `bazel-bin/bin/`
- Build-mode driven stripping (no manual strip genrule)

## Language & Standards

- **Strict C99**: No C++, no language extensions
- Compiler: `-std=c99 -Wall -Wextra -Werror -Wpedantic` + 20+ additional warning flags
- Style: Linux kernel coding style (tabs, 8-space indent)
- Linting: clang-tidy with CERT C rules

### Licensing Constraints
**No GPL dependencies**. Only MIT, BSD, Apache-2.0, or Public Domain:
- SQLite (Public Domain)
- CivetWeb (MIT)
- QuickJS (MIT)

## SQLite Configuration

Critical compile-time flags:
```c
-DSQLITE_THREADSAFE=1
-DSQLITE_ENABLE_FTS5
-DSQLITE_ENABLE_SQLAR
-DSQLITE_OMIT_LOAD_EXTENSION
-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
-DSQLITE_ENABLE_JSON1
```

Runtime pragmas:
```sql
PRAGMA foreign_keys=ON;
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

## QuickJS Integration

- **Engine**: QuickJS-NG (MIT license)
- **Memory limits**: 64 MB per context
- **Execution timeout**: 5000ms default
- **Host modules**: `hbf:db`, `hbf:http` (request/response objects), `hbf:console`
- **Content loading**: server.js loaded from embedded SQLAR archive
- No npm at runtime; pure JS modules only

## CLI Arguments

```bash
hbf [options]

Options:
  --port <num>         HTTP server port (default: 5309)
  --log_level <level>  Log level: debug, info, warn, error (default: info)
  --dev                Enable development mode
  --help, -h           Show this help message
```

## Compiler Warnings (Enforced)

```
-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion
-Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition
-Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings
-Wcast-align -Wundef -Wswitch-enum -Wswitch-default -Wbad-function-cast
```

## Testing

All tests use minunit-style C macros with assert.h:
- `hbf/shell:hash_test` - Hash function tests
- `hbf/shell:config_test` - CLI parsing tests
- `hbf/db:db_test` - SQLite wrapper tests
- `hbf/db:overlay_test` - Schema overlay tests
- `hbf/qjs:engine_test` - QuickJS engine tests
- `pods/base:fs_build_test` - SQLAR build tests

```bash
# Run all tests
$ bazel test //...
✅ 6 test targets, all passing
```

## Current Implementation

**Core Components**:
- ✅ `hbf/shell/` - Logging, config, CLI, main
- ✅ `hbf/db/` - SQLite wrapper with overlay schema support
- ✅ `hbf/http/` - CivetWeb HTTP server with request routing
- ✅ `hbf/qjs/` - QuickJS engine with host bindings (db, console, request/response)
- ✅ `pods/base/` - Base pod with SQLAR build system

**Pod Build System**:
- Each pod directory contains static files (hbf/*.js, static/*.html, etc.)
- BUILD.bazel creates SQLAR archive (SQLite archive format)
- SQLAR archive embedded as an object file via objcopy and linked into the binary (faster compiles than large C arrays)
- Overlay schema applied at build time for pod-specific tables

**Binary**:
- Size: 5.3 MB stripped (statically linked with SQLite + QuickJS)
- 100% static linking with musl libc 1.2.3
- Zero runtime dependencies
- Compiles with `-Werror` and 30+ warning flags

## Key Design Decisions

1. **Single binary deployment**: All dependencies statically linked
2. **Pod-based architecture**: Each pod is self-contained (DB + JS + static files)
3. **No HTTPS in binary**: Use reverse proxy (nginx, Traefik, Caddy) for TLS
4. **SQLite as universal store**: Content, data, and static files in one database
5. **QuickJS sandboxing**: Memory limits, execution timeouts, no host FS/network access

## Known Issues & Workarounds

### Bazel Build Output Layout
All binaries are emitted under `bazel-bin/bin/` (symlinks to configuration-specific artifacts), e.g.:
```bash
bazel-bin/bin/hbf
bazel-bin/bin/hbf_test
```

## References

- **Development setup**: `DOCS/development-setup.md`
- **Coding standards**: `DOCS/coding-standards.md`

## Discovering Pod Targets

```bash
# List all hbf pod binaries by name convention
bazel query 'kind("cc_binary", //:*)' | grep '^//:hbf'

# If pod binaries are tagged with "hbf_pod"
bazel query 'attr(tags, "hbf_pod", kind("cc_binary", //...))'
```

## Reproducible Builds and Version Stamping

- Use a `workspace_status_command` (e.g., `tools/status.sh`) with `stamp = True` to embed version/commit metadata.
- For bit-for-bit artifacts, set `SOURCE_DATE_EPOCH` and add `-ffile-prefix-map`/`-fdebug-prefix-map` flags.
- Validate determinism by building twice with a fixed `SOURCE_DATE_EPOCH` and comparing `sha256sum` of `bazel-bin/bin/hbf`.
