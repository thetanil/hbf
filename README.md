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

## Dev endpoints ownership

To remove ambiguity and large copies, dev API endpoints are owned as follows:

- Native C (fast path):
  - GET `/__dev/api/files` — implemented in `hbf/http/server.c` (see `dev_files_list_handler`).
    - Builds JSON in SQLite (JSON1) and streams directly, avoiding QuickJS and BLOB reads.
    - Only enabled in `--dev` mode.
- JavaScript (pod):
  - `/__dev/` (Dev UI page) and single‑file APIs `/__dev/api/file` (GET/PUT/DELETE) remain in `server.js`.

The native handler for `/__dev/api/files` is registered before the catch‑all JS request handler, so JS must not implement this route. Both base and test pods have had their JS versions removed.

## Schema sourcing (authoritative)

The authoritative database schema for the versioned filesystem lives in `hbf/db/overlay_schema.sql` and is applied at pod build time (see the `pod_db` rules in each pod's `BUILD.bazel`). Every embedded pod database ships with the correct tables, indexes, views, the materialized `latest_files_meta`, and triggers.

At runtime, we do not create or migrate the schema in code. `overlay_fs.c` requires the schema to already exist and fails fast if it is missing. This avoids divergence and ensures a single source of truth.

### Using the schema in C tests

To apply the same authoritative schema in unit tests without duplicating SQL in C files, a Bazel genrule converts `overlay_schema.sql` into a small C translation unit that exposes the schema as a `const char*`:

- Target: `//hbf/db:overlay_schema_c`
- Output: `overlay_schema_gen.c` (auto-generated from `overlay_schema.sql` via `//tools:sql_to_c`)
- Usage in a test target:
  - Add `:overlay_schema_gen.c` to the test's `srcs`
  - Declare and use the generated symbols in your test:
    - `extern const char * const hbf_schema_sql_ptr;`
    - `extern const unsigned long hbf_schema_sql_len;`
    - `sqlite3_exec(db, hbf_schema_sql_ptr, NULL, NULL, &errmsg);`

This keeps `overlay_schema.sql` as the single source for both pods and tests.

## Next Steps, future dev

- benchmark is using base pod, not test pod

- need js_db_test.md

- need to 'include' or whatever for qjs to overlay_fs

- each pod's server.js and i guess http module for static file access all have
  their own methods for reading and writing new content to the latest_fs tables.
  i would like an api which is in db which is reused in all of those cases

- dev (hbf_dev) includes monaco for jsfiddle / shadertoy like glsl fiddle examples

- remove monaco from base, base (hbf) is just a static site with a lite editor
  for blog and js edit. 

- new pod bootstrap(hbf-bootstrap)

- v1 requires base to have local edit and sync with hosting working

- some way to do db migrations at some point

- what if repo template for own pod which can deploy to your hosted site

- try to add ffmpeg to statically compiled musl bin, but this might be tough
  given the number of deps ffmpeg has


