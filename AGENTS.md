# AGENTS.md

A concise, agent-focused guide to working inside the HBF repository. Think of this as a README for AI coding agents: what the project is, how to build/test/lint, coding standards, safety rules, and implementation facts you can rely on. Keep output small, accurate, and incremental. Prefer precise file paths.

---
## Project Overview
HBF is a single, statically linked C99 web compute environment built with Bazel 8 (bzlmod) that embeds:
- SQLite (WAL, JSON1, FTS5, SQLAR) as unified store for data + static assets
- CivetWeb for HTTP (WebSocket enabled; IPv4 only; no TLS/SSL/CGI/FILES; terminate TLS behind reverse proxy)
- QuickJS-NG for per-request sandboxed JavaScript (fresh runtime/context each request; configurable mem/timeout limits)
- Pod-based architecture: each pod supplies `hbf/server.js` and `static/**` assets, embedded into the final binary via SQLAR → C array pipeline.
- Versioned filesystem: all file writes create immutable versions with full audit trail

Single binary outputs under `bazel-bin/bin/` (e.g. `hbf`, `hbf_test`). All HTTP requests enter CivetWeb; `/static/**` served directly from embedded DB via versioned filesystem; all other routes handled by evaluating `hbf/server.js` for the active pod.

---
## Directory Hints
```
BUILD.bazel                # Registers pod binaries (hbf, hbf_test)
MODULE.bazel               # Bazel 8 module deps (CivetWeb, QuickJS-NG, SQLite, zlib)
hbf/shell/                 # main.c, CLI, logging, config
hbf/http/                  # server.c (static handler), handler.c (QuickJS request), handler.h
hbf/db/                    # db.c (SQLite init + SQLAR read), overlay_fs.* (versioned FS)
hbf/qjs/                   # engine.c, module_loader.*, bindings/{request,response}.c
pods/base/, pods/test/     # Example pods: JavaScript + static assets
tools/                     # Helper scripts (lint.sh, db_to_c.sh, sql_to_c.sh, npm_vendor.sh)
DOCS/                      # Specifications (hbf_spec.md, coding-standards.md, plans)
CLAUDE.md                  # Detailed architecture & build pipeline
js_import_router.md        # Router integration plan (find-my-way)
```

---
## Build & Run Commands
Prefer file/pod-scoped builds and tests; avoid full-repo commands unless necessary.
```
# Build default (base) pod binary
bazel build //:hbf
# Build test pod
bazel build //:hbf_test
# Run binary with flags
bazel run //:hbf -- --port 5309 --log_level debug
# Run all tests (C99 unit + pod build tests)
bazel test //...
# Run a single test with full output
bazel test //hbf/shell:config_test --test_output=all
# Lint (alias to //tools:lint target)
bazel run //:lint
```
File-scoped rebuild is implicit: modifying a file and re-running the related bazel target reuses cache.

---
## Testing Instructions
Test targets (C99 + build integration) you can rely on:
```
//hbf/shell:hash_test
//hbf/shell:config_test
//hbf/db:db_test
//hbf/db:overlay_test
//hbf/db:overlay_fs_test
//hbf/qjs:engine_test
//pods/base:fs_build_test
//pods/test:fs_build_test
```
Run everything before committing: `bazel test //...`. Use `--test_output=all` for debugging a failing target.

---
## Coding Standards (C99)
Strict C99 only, zero warnings:
Flags enforced (see DOCS/coding-standards.md):
```
-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion 
-Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes 
-Wmissing-declarations -Wcast-qual -Wwrite-strings -Wcast-align -Wundef 
-Wswitch-enum -Wswitch-default -Wbad-function-cast
```
Style: Linux kernel (tabs, 8 width). SPDX header every source file: `/* SPDX-License-Identifier: MIT */`.
No GPL dependencies; only MIT / BSD / Apache-2.0 / Public Domain. Current third_party: CivetWeb (MIT), QuickJS-NG (MIT), SQLite (PD), zlib.

---
## JavaScript Runtime Facts
- Entry: `hbf/server.js` in each pod, loaded from versioned filesystem (`latest_files` view) for every non-static request
- Fresh QuickJS runtime + context per request (no pooling, guaranteed via mutex serialization in `hbf/http/handler.c:56-351`)
- Memory/timeout limits: Runtime-configurable via `hbf_qjs_init(mem_limit_mb, timeout_ms)` in `hbf/qjs/engine.c:58`
  - Timeout enforced via `JS_SetInterruptHandler` with `CLOCK_MONOTONIC` timer (engine.c:35-55, 120)
- Host bindings:
  - **request/response**: `bindings/request.c`, `bindings/response.c` (provides `req.path`, `req.method`, `req.headers`, etc.)
  - **console**: Basic logging via `console_module.c`
  - **db**: Direct SQLite access via `db_module.c`
    - `db.query(sql, [params])` → Returns array of row objects
    - `db.execute(sql, [params])` → For INSERT/UPDATE/DELETE
- ESM imports: ACTIVE for relative paths only (e.g., `import foo from "./lib/bar.js"`)
  - Module loader: `hbf/qjs/module_loader.c` loads from `latest_files` view
  - NO bare specifier support yet (e.g., `import 'lodash'` will not work)
  - Both static and dynamic imports working (see `pods/test/hbf/server.js:4,34`)
- Static assets resolved under `/static/*` directly in C via `overlay_fs_read_file()` (no JS involvement)
- Router: Manual path conditionals currently (find-my-way integration planned, see `js_import_router.md`)

---
## Versioned / Embedded Filesystem
**Build-time embedding** (SQLAR → C array pipeline):
1. Collect pod `hbf/*.js` + `static/**`
2. Create SQLAR (sqlite3 -A)
3. Apply overlay schema (`hbf/db/overlay_schema.sql`)
4. VACUUM optimize (removes dead space)
5. Convert DB to C (`db_to_c.sh` → `*_fs_embedded.c` with `fs_db_data`, `fs_db_len`)
6. Link via `pod_binary()` macro (see `tools/pod_binary.bzl`)

**Runtime overlay filesystem** (ACTIVE, fully integrated):
- All reads via `overlay_fs_read_file(path, enable_overlay, **data, *size)` from `latest_files` view
- Static handler (`hbf/http/server.c`) uses overlay for `/static/*` routes
- Cache headers: `Cache-Control: public, max-age=3600` for static assets
- Versioned writes create immutable history in `file_versions` table
- Public API (see `hbf/db/overlay_fs.h`):
  - `overlay_fs_read(db, path, ...)` - Read latest version
  - `overlay_fs_write(db, path, ...)` - Write new version
  - `overlay_fs_exists(db, path)` - Check existence
  - `overlay_fs_version_count(db, path)` - Get version history count
  - `overlay_fs_migrate_sqlar(db)` - Convert SQLAR to versioned FS

**Complete pipeline documentation**: See `DOCS/custom_content.md` for detailed explanation of how files flow from build host → SQLAR → overlay_fs → runtime, including:
- Step-by-step build pipeline with code examples
- Runtime read/write operations with file references
- File lifecycle examples (adding, importing, editing)
- Overlay filesystem API reference (C and JavaScript)
- Performance characteristics and troubleshooting

---
## Safety & Permissions
Agents may perform WITHOUT asking:
- Read/list files
- Run targeted builds/tests: `bazel build //:hbf`, `bazel test //hbf/db:overlay_fs_test`
- Run lint: `bazel run //:lint`
- Read documentation in `DOCS/` and `CLAUDE.md`
Must ASK before:
- Adding new external dependencies (respect license policy)
- Large refactors across many directories
- Changing build macros in `tools/pod_binary.bzl`
- Deleting files or altering licensing headers
- Introducing new pods (requires manual registration + CI changes)

---
## Do / Don't
### Do
- Keep diffs small and pod-scoped.
- Maintain C99 warning-free builds (treat new warnings as failures).
- Re-run affected tests after edits.
- Use existing helper scripts in `tools/` instead of re-implementing.
- Prefer updating `server.js` or pod assets through documented pipeline.

### Don't
- Introduce GPL or copyleft dependencies.
- Pool or persist QuickJS contexts (architectural constraint: per-request fresh sandbox).
- Bypass Bazel macros for embedding pods.
- Hard-code file lists into genrules (use globs as existing system does).

---
## Project Structure Pointers
- Pod registration: root `BUILD.bazel` via `pod_binary(...)` calls.
- QuickJS engine: `hbf/qjs/engine.c`, module loading in `module_loader.c`.
- HTTP static handler: `hbf/http/server.c` (`/static/*`).
- Dynamic request handling: `hbf/http/handler.c` invoking JS.
- DB + SQLAR access: `hbf/db/db.c`, overlays in `overlay_fs.*`.
- Tests enumerate behaviors: search `*_test.c` under `hbf/` and `pods/*`.

---
## API & Behavior Guarantees (Current Code)
**HTTP Endpoints (C handlers)**:
- `GET /health` → `{"status":"ok"}` (health_handler in server.c)
- `/static/**` → File content from versioned FS (static_handler in server.c)

**Request Routing**:
- All non-static, non-health routes invoke QuickJS `app.handle(req, res)` from pod `server.js`
- Each request: fresh QuickJS runtime/context created, then destroyed (handler.c:65, 343)
- Static files served in parallel (no mutex); dynamic routes serialized via `handler_mutex` (handler.c:56)

**Build Outputs**:
- Static binaries at `bazel-bin/bin/hbf*` (musl libc, 100% static, no dynamic libs)
- CivetWeb compiled with (see third_party/civetweb/civetweb.BUILD:13-28):
  - `-DNO_SSL` - No TLS/SSL (use reverse proxy)
  - `-DNO_CGI` - No CGI support
  - `-DNO_FILES` - No built-in file serving (handled by overlay_fs)
  - `-DUSE_IPV6=0` - IPv4 only
  - `-DUSE_WEBSOCKET` - WebSocket support enabled

---
## PR / Commit Checklist
Before finishing a task:
- All modified tests passing: `bazel test //...`
- No new warnings (C) or lint failures: `bazel run //:lint`
- Diff small & focused; include summary rationale in commit message.
- No license violations; new files have SPDX header.

---
## When Stuck
If a required symbol or behavior isn’t obvious:
1. Search path with Bazel query or grep (e.g. `handler.c`, `engine.c`).
2. Re-read `CLAUDE.md` for pipeline details.
3. Propose a short plan (bullet list) in discussion before large changes.

---
## Extensibility Notes & Known Limitations
**Implemented**:
- ✅ Overlay filesystem with live editing
- ✅ Dev mode API endpoints
- ✅ ESM module imports (relative paths only)
- ✅ Versioned file history tracking

**Not Yet Implemented** (see `agent_help.md` for tracking):
- ❌ find-my-way router (currently manual path conditionals in `server.js`)
- ❌ SSE endpoints (CivetWeb has capability, no app-level handlers yet)
- ❌ Bare module specifiers (e.g., `import 'lodash'`)
- ❌ Import maps or vendor resolution

**Verified Compile-Time Flags** (see `third_party/civetweb/civetweb.BUILD`):
- ✅ `-DNO_SSL` (line 16) - SSL/TLS disabled
- ✅ `-DNO_FILES` (line 18) - File serving disabled
- ✅ `-DUSE_IPV6=0` (line 19) - IPv6 disabled (IPv4 only)
- ✅ `-DUSE_WEBSOCKET` (line 20) - WebSocket enabled for future SSE/WS features

Record new unknowns or unverifiable claims in `agent_help.md`.

---
## FAQ (Repo-Specific)
Q: Where do I add a new pod?
A: Create `pods/<name>/` with `BUILD.bazel`, then add `pod_binary()` invocation in root `BUILD.bazel`.

Q: How do I read embedded static assets?
A: Served automatically over `/static/*` by C via `overlay_fs_read_file()`; dynamic content via `server.js`.

Q: How to adjust JS memory/timeout limits?
A: Limits are runtime-configurable via `hbf_qjs_init(mem_limit_mb, timeout_ms)` in `hbf/qjs/engine.c:58`. Check `hbf/shell/main.c` for current defaults.

Q: How to run single test verbosely?
A: `bazel test //hbf/db:overlay_fs_test --test_output=all`

Q: Can I use npm packages in server.js?
A: Not yet. Only relative ESM imports work (e.g., `import './lib/foo.js'`). Bare specifiers like `import 'lodash'` are not supported. See `js_import_router.md` for vendor plan.

Q: How do I edit files at runtime?
A: File editing endpoints have been removed. Future versions will provide JWT-gated editor APIs.

Q: Are SSE (Server-Sent Events) endpoints available?
A: No. CivetWeb supports SSE, but no application-level SSE handlers are implemented yet.

---
## Commands (Copyable Reference)
```
bazel build //:hbf
bazel build //:hbf_test
bazel run //:hbf -- --port 5309 --log_level debug
bazel test //...
bazel test //hbf/qjs:engine_test --test_output=all
bazel run //:lint
```

---
## License Summary
Project: MIT. Allowed dependency licenses: MIT, BSD, Apache-2.0, Public Domain. Source files should keep original third_party license notices.

---
## Maintainability Principles
- Prefer incremental enhancement over rewrites.
- Centralize embedding logic: modify `tools/pod_binary.bzl` only with review.
- Keep `AGENTS.md` concise; move deep explanations to `CLAUDE.md` / `DOCS/`.
- Update this file when adding build/test commands or structural changes.

---
(End of AGENTS.md)
