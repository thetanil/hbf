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

Build modes:
- **Release (stripped)**: `--compilation_mode=opt --strip=always` → 5.3MB binary
- **Debug (symbols)**: `--compilation_mode=dbg` → 13MB binary with debug info
- **Local development**: Use `bazel run //:hbf` (defaults per `.bazelrc`)

**Important**: After building with `--strip=always`, manually run `strip` on the binary to ensure symbols are removed (Bazel's `--strip` flag may not work with all toolchains). This is handled automatically in CI/CD.

Tip: set `.bazelrc` so `bazel run` defaults to debug for a better dev experience.

### Bazel Configuration
- `.bazelrc` contains build settings
- **musl toolchain only** (100% static linking)
- Single output layout: all binaries under `bazel-bin/bin/`
- Build-mode driven stripping (no manual strip genrule)

### Multi-Pod Build System

HBF uses a macro-based multi-pod build system that allows multiple independent pods to be built from a single codebase. Pods are **manually registered** in the root BUILD.bazel file, providing explicit control over what gets built in CI/CD.

**Pod Structure**: Each pod follows a standard directory layout:
```
pods/
  <podname>/
    BUILD.bazel          # Must contain pod_db target
    hbf/                 # JavaScript source files
      server.js
    static/              # Static assets
      *.html, *.css
```

**Build Targets**: The `pod_binary()` macro in `tools/pod_binary.bzl` generates binary targets for each pod:
- `//:hbf` - Base pod (default)
- `//:hbf_<podname>` - Other pods (e.g., `//:hbf_test`)
- `//:hbf_<podname>_unstripped` - Alias for convenience (points to same target)

**How Pod Embedding Works**:

The `pod_binary()` macro orchestrates a build pipeline:

1. **SQLAR Creation**: Pod directory (hbf/*.js, static/*) → SQLite SQLAR archive (temp.db)
2. **Schema Application**: Overlay schema SQL applied to temp.db
3. **VACUUM Optimization**: `VACUUM INTO` removes dead space, creating final optimized .db file
4. **C Array Generation**: Optimized SQLAR binary → C source files via `db_to_c` tool
   - Generates `<name>_fs_embedded.c` and `<name>_fs_embedded.h`
   - Provides symbols: `fs_db_data` (byte array) and `fs_db_len` (size)
5. **Library Creation**: `cc_library` with `alwayslink = True` ensures symbols are included
6. **Binary Linking**: `cc_binary` links embedded library + core HBF libraries
7. **Output**: Final binary copied to `bazel-bin/bin/<name>`

Each pod gets its own embedded library (e.g., `//:hbf_embedded`, `//:hbf_test_embedded`) that provides the `fs_db_data`/`fs_db_len` symbols. Tests that need database access must depend on one of these embedded libraries.

**Adding a New Pod**:
1. Create `pods/<name>/` directory with BUILD.bazel containing `pod_db` target
2. Add `pod_binary(name = "hbf_<name>", pod = "//pods/<name>", tags = ["hbf_pod"])` to root BUILD.bazel
3. Add `"<name>"` to matrix pod list in `.github/workflows/pr.yml` for CI builds
4. (Optional) Add `"<name>"` to `.github/workflows/release.yml` for release artifacts

**Why Manual Pod Registration?**
- **Explicit control**: No surprises about what's being built
- **Deliberate releases**: Only explicitly listed pods are released
- **Bazel alignment**: Same manual approach in BUILD.bazel and GitHub Actions
- **Simple and predictable**: Easy to understand what will be built

**Discovering Pods**:
```bash
# List all hbf pod binaries by name convention
bazel query 'kind("cc_binary", //:*)' | grep '^//:hbf'

# List pods tagged with hbf_pod
bazel query 'attr(tags, "hbf_pod", kind("cc_binary", //...))'
```

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
- `hbf/qjs:engine_test` - QuickJS engine tests (includes db module integration)
- `pods/base:fs_build_test` - SQLAR build tests for base pod
- `pods/test:fs_build_test` - SQLAR build tests for test pod

```bash
# Run all tests
$ bazel test //...
✅ 7 test targets, all passing
```

## Current Implementation

**Core Components**:
- ✅ `hbf/shell/` - Logging, config, CLI, main
- ✅ `hbf/db/` - SQLite wrapper with overlay schema support
- ✅ `hbf/http/` - CivetWeb HTTP server with request routing
- ✅ `hbf/qjs/` - QuickJS engine with host bindings (db, console, request/response)
- ✅ `pods/base/` - Base pod with example content
- ✅ `pods/test/` - Test pod for validation

**Multi-Pod Build System** (Completed):
- ✅ Macro-based build system via `pod_binary()` in `tools/pod_binary.bzl`
- ✅ Each pod directory contains static files (hbf/*.js, static/*.html, etc.)
- ✅ SQLAR archive generated per-pod and embedded via `db_to_c` conversion to C arrays
- ✅ `VACUUM INTO` optimization removes all dead space from embedded databases
- ✅ Per-pod embedded libraries with `alwayslink = True` for symbol resolution
- ✅ Overlay schema applied at build time for pod-specific tables
- ✅ All binaries emit to unified `bazel-bin/bin/` directory
- ✅ GitHub Actions matrix builds for multiple pods in parallel
- ✅ CI produces both stripped (5.3MB) and unstripped (13MB) release artifacts

**Binary**:
- Size: 5.3 MB stripped (statically linked with SQLite + QuickJS + CivetWeb)
- Size: 13 MB unstripped (includes full debug symbols for gdb/lldb)
- Embedded database: ~3.2 MB (VACUUMed, no dead space)
- 100% static linking with musl libc 1.2.3
- Zero runtime dependencies
- Compiles with `-Werror` and 30+ warning flags

**Testing**:
- ✅ All 7 test targets passing
- ✅ Both base and test pod binaries build successfully
- ✅ Pod discovery via Bazel query works

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

## Reproducible Builds and Version Stamping

- Use a `workspace_status_command` (e.g., `tools/status.sh`) with `stamp = True` to embed version/commit metadata.
- For bit-for-bit artifacts, set `SOURCE_DATE_EPOCH` and add `-ffile-prefix-map`/`-fdebug-prefix-map` flags.
- Validate determinism by building twice with a fixed `SOURCE_DATE_EPOCH` and comparing `sha256sum` of `bazel-bin/bin/hbf`.
