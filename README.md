# HBF

Single, statically linked C99 web compute environment (SQLite + CivetWeb + QuickJS‑NG) built with Bazel 8 and bzlmod.

HBF is a single binary that serves HTTP, runs a JavaScript handler per request in QuickJS, and embeds your pod’s code and static files inside a SQLite database. This README is a concise quickstart; see `DOCS/hbf_spec.md` for full details.

## Highlights

- CivetWeb HTTP server with health and static file handling
- Request handling via QuickJS with per‑request sandbox
  - Fresh runtime/context per request
  - 64 MB memory limit and 5s execution timeout
- Pod‑based architecture: each pod provides its own `server.js` and static assets
- Single, fully static binary (musl) with zero runtime dependencies

## Directory layout

```
hbf/
  shell/   # CLI, main, logging, config
  db/      # SQLite wrapper, versioned filesystem, and schema
  http/    # CivetWeb server wrapper and routing
  qjs/     # QuickJS engine and host bindings

pods/
  base/    # Base pod example (JS + static assets)
  test/    # Test pod used by CI/tests

third_party/  # Vendored deps (CivetWeb, QuickJS, SQLite, etc.)
tools/        # Build scripts and helper tools
DOCS/         # Documentation (specs, plans, standards)
```

## Build

Prerequisites:
- Linux x86_64
- Bazel 8.x with bzlmod (no WORKSPACE)

Commands:

```bash
# Build default base pod binary
bazel build //:hbf

# Build a specific pod binary (example: test pod)
bazel build //:hbf_test

# Run all tests
bazel test //...

# Lint (clang-tidy and checks)
bazel run //:lint
```

Binary output layout (symlinked):
- `bazel-bin/bin/hbf`
- `bazel-bin/bin/hbf_test`

## Run

```bash
# Easiest for local dev
bazel run //:hbf -- --port 5309 --log_level debug

# Or run the built binary directly
./bazel-bin/bin/hbf --port 8080 --log_level info
```

Health check:

```bash
curl http://localhost:5309/health
# {"status":"ok"}
```

## CLI

```text
--port <num>         HTTP server port (default: 5309)
--log_level <level>  debug | info | warn | error (default: info)
--dev                Enable development mode
--help, -h           Show help
```

## Pods and embedding

HBF builds a separate static binary per pod using a macro‑based pipeline. Each pod includes JavaScript (`pods/<name>/hbf/server.js`) and static assets (`pods/<name>/static/**`), which are packed into a SQLAR SQLite database and embedded into the binary.

How it works at build time:
1) Create SQLAR from the pod's `hbf/*.js` and `static/*`
2) Apply versioned filesystem schema and `VACUUM INTO` to optimize the DB
3) Convert the optimized DB into C sources providing `fs_db_data`/`fs_db_len`
4) Link the embedded DB library into the final `cc_binary`
5) Copy output to `bazel-bin/bin/<name>`

Add a new pod:
1) Create `pods/<name>/` with a `BUILD.bazel` that defines a `pod_db` target
2) Register it in the root `BUILD.bazel` via `pod_binary(name = "hbf_<name>", pod = "//pods/<name>")`
3) Build it with `bazel build //:hbf_<name>`

## QuickJS integration

- Engine: QuickJS‑NG
- Limits: 64 MB/context, 5000 ms execution timeout
- Host modules: `hbf:db`, `hbf:http` (request/response bindings), `hbf:console`
- Content: `server.js` loaded from the embedded SQLAR archive

## SQLite configuration

Compile‑time flags:

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

## Testing

All tests are C99 and run via Bazel:
- `//hbf/shell:hash_test` - Hash function tests
- `//hbf/shell:config_test` - CLI parsing tests
- `//hbf/db:db_test` - SQLite wrapper tests
- `//hbf/db:overlay_test` - Versioned filesystem integration tests
- `//hbf/db:overlay_fs_test` - Versioned filesystem unit tests
- `//hbf/qjs:engine_test` - QuickJS engine tests
- `//pods/base:fs_build_test` - Base pod build tests
- `//pods/test:fs_build_test` - Test pod build tests

Run everything:

```bash
bazel test //...
# ✅ 8 test targets, all passing
```

## Licensing

MIT for HBF. Third‑party components retain their original licenses. No GPL dependencies; only MIT/BSD/Apache‑2.0/Public Domain are allowed.

## Versioned Filesystem

HBF includes a versioned filesystem (`hbf/db/overlay_fs.{c,h}`) that provides immutable file history tracking using SQLite.

### Features

- **Immutable history**: Every write creates a new version (append-only)
- **Fast reads**: Optimized queries with indexed `file_id` + `version_number DESC`
- **Version tracking**: Full audit trail of all file changes
- **Migration support**: Convert SQLAR archives to versioned storage via `overlay_fs_migrate_sqlar()`
- **Backward compatible**: Works alongside existing SQLAR-based pod system

### API

```c
int overlay_fs_init(const char *db_path, sqlite3 **db);
int overlay_fs_write(sqlite3 *db, const char *path, const unsigned char *data, size_t size);
int overlay_fs_read(sqlite3 *db, const char *path, unsigned char **data, size_t *size);
int overlay_fs_exists(sqlite3 *db, const char *path);
int overlay_fs_version_count(sqlite3 *db, const char *path);
int overlay_fs_migrate_sqlar(sqlite3 *db);
```

### Performance

Benchmark results (in-memory, 1000 files, 10 versions each):
- Write: 28,000+ files/sec (initial), 37,000+ writes/sec (multi-version)
- Read: 142,000+ reads/sec (latest version)

## References

- Spec and behavior: `DOCS/hbf_spec.md`
- Coding standards: `DOCS/coding-standards.md`
- Developer guide: `CLAUDE.md`

## Next Steps, future dev

- each pod's server.js and i guess http module for static file access all have
  their own methods for reading and writing new content to the latest_fs tables.
  i would like an api which is in db which is reused in all of those cases

- split db and fs module so that http and qjs use fs module and the db pointer
  is protected and handles locking (related to memory corruption bug under heavy
  write contention)

- dev (hbf_dev) includes monaco for jsfiddle / shadertoy like glsl fiddle examples

- remove monaco from base, base (hbf) is just a static site with a lite editor
  for blog and js edit. 

- new pod bootstrap(hbf-bootstrap)

- v1 requires base to have local edit and sync with hosting working

- some way to do db migrations at some point

- what if repo template for own pod which can deploy to your hosted site

- try to add ffmpeg to statically compiled musl bin, but this might be tough
  given the number of deps ffmpeg has

## memory corruption

 Root Cause: Heap Corruption from Malloc Contention Under High Concurrency

  The Crash

  Location: Thread 1 (LWP 26911) crashed in QuickJS's JS_SetPropertyInternal2() at hbf/http/handler.c:85 while creating a response object.

  Memory corruption: The crash occurred when trying to dereference a corrupted pointer value 0x8a42d22900012400 (clearly invalid - not a real memory address).

  Why It Happened

  1. Multi-threaded Architecture
  - CivetWeb configured with 4 worker threads (hbf/http/server.c:173)
  - Each thread handles requests concurrently
  - All threads share the same SQLite database handle (server->db)

  2. Benchmark Load (from .claude/skills/benchmark/benchmark.sh)
  - Default: 10,000 requests at 10 concurrent connections
  - Server running with --inmem --dev --log-level error flags
  - Testing QuickJS runtime routes (/hello, /user/42, /echo) and filesystem writes

  3. The Fatal Pattern
  Looking at the thread states in the core dump:
  - Thread 1: Crashed in JS_SetPropertyInternal2() during response creation
  - Thread 2: Running JavaScript JSON stringification
  - Threads 3 & 4: Both stuck in __lock() inside __libc_malloc_impl() ← Key evidence!

  This shows severe malloc contention. Each request:
  1. Creates a fresh QuickJS runtime + context (allocates ~MB of memory)
  2. Loads and executes hbf/server.js
  3. Processes the request (more allocations)
  4. Destroys the context (frees all memory)

  With 4 threads doing this simultaneously under benchmark load, musl's global malloc lock becomes a bottleneck.

  4. Heap Corruption
  The musl allocator uses a single lock for all malloc/free operations. Under extreme contention:
  - Race conditions can corrupt heap metadata
  - When Thread 1 tried to access a QuickJS object, it read corrupted heap data
  - The pointer at offset 0x20 contained garbage: 0x8a42d22900012400
  - Attempting to dereference 0x30(%rax) → SIGSEGV

  Evidence Chain

  Assembly at crash point:
  0x42c820: mov    0x20(%rax),%rax      ; Load pointer from structure
  0x42c824: test   %rax,%rax            ; Check if null
  0x42c827: je     0x42c6d8             ; Jump if null
  0x42c82d: mov    0x30(%rax),%r10      ; CRASH - dereference bad pointer

  The corrupted value 0x8a42d22900012400 passed the null check but was invalid memory, causing the segfault.

  Contributing Factors

  1. Per-request context creation (hbf/http/handler.c:41): While correct for isolation, it creates massive memory churn under load
  2. No SQLite connection pooling: Single shared connection being accessed by 4 threads
  3. Musl's single-lock allocator: Not optimized for high-concurrency workloads
  4. In-memory database (--inmem): Increases memory pressure

  Recommendations

  Immediate fixes:
  1. Add per-thread context pooling - Reuse QuickJS contexts instead of create/destroy per request
  2. Enable SQLite FULLMUTEX mode - Explicitly configure thread-safe mode:
  sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
  3. Reduce CivetWeb threads - Test with 2 threads instead of 4 to reduce contention
  4. Consider alternative allocators - jemalloc or tcmalloc have better concurrency characteristics than musl

  Longer-term:
  1. Connection pooling - One SQLite connection per worker thread
  2. Shared QuickJS runtime with per-thread contexts (requires locking)
  3. Performance profiling - Benchmark with different thread counts and allocators

  The crash is not a QuickJS bug but rather heap corruption from malloc contention under the specific combination of per-request context creation + 4 worker threads + high benchmark load.