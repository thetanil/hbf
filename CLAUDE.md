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
  db/         - SQLite wrapper, versioned filesystem, and schema
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
- `hbf/db:overlay_test` - Versioned filesystem integration tests
- `hbf/db:overlay_fs_test` - Versioned filesystem unit tests
- `hbf/qjs:engine_test` - QuickJS engine tests (includes db module integration)
- `pods/base:fs_build_test` - SQLAR build tests for base pod
- `pods/test:fs_build_test` - SQLAR build tests for test pod

```bash
# Run all tests
$ bazel test //...
✅ 8 test targets, all passing
```

## Threading and concurrency model

- HTTP server: CivetWeb runs with 4 worker threads (see `hbf/http/server.c`, option `num_threads=4`).
- QuickJS requests: Serialized with a global `handler_mutex` in `hbf/http/handler.c` that protects the entire lifecycle of per-request QuickJS runtime/context creation, execution, and destruction. This eliminates malloc contention and the prior crash under load. Only one JavaScript request executes at a time.
- Static files: Served in parallel (no mutex) via `overlay_fs_read_file()` in `hbf/http/server.c`.
- Database handle: A single SQLite handle is shared across threads. It is registered globally for filesystem ops via `overlay_fs_init_global(db)` (in `hbf/db/db.c`), and is also passed to each QuickJS context (`ctx->db`) for JS `db.query()/db.execute()`.
- SQLite safety: Built with `SQLITE_THREADSAFE=1` and `PRAGMA journal_mode=WAL`, so concurrent access is safe and internally serialized by SQLite; readers and writers generally do not block each other in WAL mode.
- Dev mode: `server->dev` toggles overlay behavior and cache headers; does not affect locking semantics.

Implications:
- Static routes remain parallel; dynamic (QuickJS) routes are serialized. If higher dynamic throughput is needed later, narrow the mutex scope to just context create/destroy.

## Current Implementation

**Core Components**:
- ✅ `hbf/shell/` - Logging, config, CLI, main
- ✅ `hbf/db/` - SQLite wrapper with versioned filesystem support
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
- ✅ Versioned filesystem schema applied at build time for immutable file history
- ✅ All binaries emit to unified `bazel-bin/bin/` directory
- ✅ GitHub Actions matrix builds for multiple pods in parallel
- ✅ CI produces both stripped (5.3MB) and unstripped (13MB) release artifacts

**Versioned Filesystem** (Integrated):
- ✅ Immutable version history for all file writes
- ✅ Fast indexed reads using `file_id` and `version_number`
- ✅ Schema: `file_ids` (path mapping), `file_versions` (versioned storage)
- ✅ View: `latest_files` for accessing current versions
- ✅ Migration support: Convert SQLAR archives to versioned storage
- ✅ Full backward compatibility with existing SQLAR-based pod loading

**Binary**:
- Size: 5.3 MB stripped (statically linked with SQLite + QuickJS + CivetWeb)
- Size: 13 MB unstripped (includes full debug symbols for gdb/lldb)
- Embedded database: ~3.2 MB (VACUUMed, no dead space)
- 100% static linking with musl libc 1.2.3
- Zero runtime dependencies
- Compiles with `-Werror` and 30+ warning flags

**Testing**:
- ✅ All 8 test targets passing
- ✅ Both base and test pod binaries build successfully
- ✅ Pod discovery via Bazel query works
- ✅ Versioned filesystem unit tests (7 subtests)
- ✅ Versioned filesystem integration tests (4 subtests)

## Versioned Filesystem

Located in `hbf/db/overlay_fs.{c,h}`. Provides immutable file history tracking using SQLite.

**Features**:
- Append-only versioning (every write creates new version)
- Indexed reads via `file_id` + `version_number DESC`
- `WITHOUT ROWID` optimization for space efficiency
- SQLAR migration support via `overlay_fs_migrate_sqlar()`
- Full backward compatibility with existing pod system

**Performance** (in-memory, 1000 files, 10 versions):
- Write: 28,000+ files/sec (initial), 37,000+ writes/sec (multi-version)
- Read: 142,000+ reads/sec (latest version)

## Key Design Decisions

1. **Single binary deployment**: All dependencies statically linked
2. **Pod-based architecture**: Each pod is self-contained (DB + JS + static files)
3. **No HTTPS in binary**: Use reverse proxy (nginx, Traefik, Caddy) for TLS
4. **SQLite as universal store**: Content, data, and static files in one database
5. **QuickJS sandboxing**: Memory limits, execution timeouts, no host FS/network access
6. **Versioned filesystem**: Immutable file history with full audit trail for all changes

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
- use pods/test/hbf/server.js test pod for tests

## Dev Endpoints

Development mode API endpoints are split between native C (for performance) and JavaScript (for flexibility):

**Native C endpoints** (`hbf/http/server.c`):
- `GET /__dev/api/files` - File listing API (see `dev_files_list_handler`)
  - Builds JSON directly in SQLite using JSON1 extension
  - Streams output without QuickJS overhead or BLOB reads
  - Only enabled with `--dev` flag
  - Registered before JavaScript catch-all handler

**JavaScript endpoints** (`server.js` in each pod):
- `GET /__dev/` - Dev UI page
- `GET /__dev/api/file?name=<path>` - Read single file
- `PUT /__dev/api/file?name=<path>` - Write file version
- `DELETE /__dev/api/file?name=<path>` - Delete file

This split optimizes the file listing endpoint (high volume, read-heavy) while keeping single-file operations in JavaScript for pod customization.

## Schema Management

**Single Source of Truth**: The authoritative database schema lives in `hbf/db/overlay_schema.sql` and is applied at pod build time.

### Build-time Schema Application

Every pod embeds the complete schema:
- Applied during pod build via `pod_db` rules in each pod's `BUILD.bazel`
- Embedded database includes tables, indexes, views, materialized `latest_files_meta`, and triggers
- No runtime schema creation or migration in `overlay_fs.c`
- Fails fast if schema is missing at runtime

This approach prevents schema drift and ensures consistency across all pods.

### Schema in C Tests

Tests use the same authoritative schema via Bazel code generation:

**Generated artifacts**:
- Target: `//hbf/db:overlay_schema_c`
- Tool: `//tools:sql_to_c` converts SQL → C source
- Output: `overlay_schema_gen.c` with embedded schema string

**Usage in test code**:
```c
// Add overlay_schema_gen.c to test's srcs in BUILD.bazel
extern const char * const hbf_schema_sql_ptr;
extern const unsigned long hbf_schema_sql_len;

// Apply schema in test
sqlite3_exec(db, hbf_schema_sql_ptr, NULL, NULL, &errmsg);
```

This eliminates SQL duplication in C files and maintains `overlay_schema.sql` as the sole source for both production and tests.