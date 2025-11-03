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
  db/      # SQLite wrapper and overlay schema
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
1) Create SQLAR from the pod’s `hbf/*.js` and `static/*`
2) Apply overlay schema and `VACUUM INTO` to optimize the DB
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
- `//hbf/shell:hash_test`
- `//hbf/shell:config_test`
- `//hbf/db:db_test`
- `//hbf/db:overlay_test`
- `//hbf/qjs:engine_test`
- `//pods/base:fs_build_test`
- `//pods/test:fs_build_test`

Run everything:

```bash
bazel test //...
```

## Licensing

MIT for HBF. Third‑party components retain their original licenses. No GPL dependencies; only MIT/BSD/Apache‑2.0/Public Domain are allowed.

## References

- Spec and behavior: `DOCS/hbf_spec.md`
- Coding standards: `DOCS/coding-standards.md`
- Developer guide: `CLAUDE.md`

## Next Steps, future dev

- make the layered fs api / database tables which allows per file readthrough to
  sqlar table as base.

- dev (hbf_dev) includes monaco for jsfiddle / shadertoy like glsl fiddle examples

- remove monaco from base, base (hbf) is just a static site with a lite editor
  for blog and js edit. 

- new pod bootstrap(hbf-bootstrap)

- v1 requires base to have local edit and sync with hosting working

- some way to do db migrations at some point

- what if repo template for own pod which can deploy to your hosted site
