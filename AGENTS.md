# AGENTS.md

A concise, agent-focused guide to working inside the HBF repository. Think of this as a README for AI coding agents: what the project is, how to build/test/lint, coding standards, safety rules, and implementation facts you can rely on. Keep output small, accurate, and incremental. Prefer precise file paths.

---
## Project Overview

**IMPORTANT: HBF has completed Phase 0 of hypermedia migration. See "Current Migration Status" below.**

HBF is a minimal, statically linked C99 hypermedia server built with Bazel 8 (bzlmod) that embeds:
- SQLite (WAL, JSON1, FTS5) as unified store
- CivetWeb for HTTP (WebSocket enabled; IPv4 only; no TLS/SSL/CGI/FILES; terminate TLS behind reverse proxy)
- Phase 0: Core hypermedia endpoints (/, /health, /links, /fragment/*)
- Phase 1 (pending): Full hypermedia API with templates, nodes, edges, and graph traversal

### Current Migration Status (2025-11-16)
**Target Architecture:** Pure C99 hypermedia server (SQLite + htmx) without JavaScript runtime
**Migration Plan:** See `hypermedia-impl-phase1.md` (revised addendum at lines 99-154)
**Status:** ✅ Phase 0 COMPLETE - QuickJS/pod infrastructure removed

**Phase 0 Task Status (Core Simplification):**
- ✅ COMPLETE: Deleted `hbf/qjs/`, `pods/`, `third_party/quickjs-ng/`, `third_party/find-my-way/`
- ✅ COMPLETE: Updated Bazel files (BUILD.bazel, MODULE.bazel, hbf/http/BUILD.bazel, tools/BUILD.bazel)
- ✅ COMPLETE: Refactored C source (removed handler.c/h, overlay_fs.c/h, static_handler.c)
- ✅ COMPLETE: Cleanup tooling (deleted js_to_c.sh, npm_vendor.sh, pod_binary.bzl)
- ✅ COMPLETE: Verification (binary builds cleanly at 2.2MB, runs with hypermedia endpoints)

**Phase 1 Status (Pending):**
- Hypermedia schema (nodes, edges, view_type_templates tables)
- C-based template renderer with {{key}} substitution
- CRUD API endpoints (/fragment, /template, /link)
- Homepage with seed data and htmx lazy-loading
- Asset embedding for htmx.min.js

**Current Working Branch:** `hyper`
**Main Branch:** `main`

Binary output: `bazel-bin/bin/hbf` (2.2MB static binary). All HTTP requests enter CivetWeb and are routed to C handlers in [server.c](hbf/http/server.c:184-188).

---
## Directory Hints
```
BUILD.bazel                # Root build (alias to //bin:hbf)
bin/BUILD.bazel            # Binary target for hbf executable
MODULE.bazel               # Bazel 8 module deps (CivetWeb, SQLite, zlib, htmx)
hbf/shell/                 # main.c, CLI, logging, config
hbf/http/                  # server.c (hypermedia handlers), server.h
hbf/db/                    # db.c (SQLite init with WAL), db.h
tools/                     # Helper scripts (lint.sh, sql_to_c.sh, asset_packer.c)
DOCS/                      # Specifications (coding-standards.md, hypermedia plans)
hypermedia-impl-phase1.md  # Phase 0 & 1 implementation plan
```

---
## Build & Run Commands
```
# Build hypermedia binary
bazel build //:hbf
# Run binary with flags
bazel run //:hbf -- --port 5309 --log_level debug
# Or run directly
bazel-bin/bin/hbf --port 5309
# Run all tests
bazel test //...
# Run a single test with full output
bazel test //hbf/shell:config_test --test_output=all
# Lint
bazel run //:lint
```

---
## Testing Instructions
Active test targets (Phase 0):
```
//hbf/shell:hash_test
//hbf/shell:config_test
//hbf/db:db_test
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
No GPL dependencies; only MIT / BSD / Apache-2.0 / Public Domain. Current third_party: CivetWeb (MIT), SQLite (PD), zlib.

---
## Hypermedia API (Phase 0)
Core endpoints implemented in [server.c](hbf/http/server.c):
- `GET /health` → JSON health check: `{"status":"ok"}`
- `GET /` → Homepage with htmx integration, lazy-loads link list
- `GET /links` → HTML list stub (hardcoded for Phase 0)
- `GET /fragment/:id` → Fragment stub (will query DB in Phase 1)
- `POST /fragment` → Fragment creation stub (will insert to DB in Phase 1)

Database:
- SQLite with WAL mode enabled for concurrency
- Global handle via `hbf_db_get()` in [db.c](hbf/db/db.c:78)
- Phase 1 will add hypermedia schema (nodes, edges, view_type_templates)

Routing:
- Direct C handlers registered in [server.c:184-188](hbf/http/server.c:184-188)
- No JavaScript runtime or dynamic routing
- htmx loads from CDN (Phase 1 will embed locally)

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
- Deleting files or altering licensing headers

---
## Do / Don't
### Do
- Keep diffs small and module-scoped
- Maintain C99 warning-free builds (treat new warnings as failures)
- Re-run affected tests after edits
- Use existing helper scripts in `tools/` instead of re-implementing
- Follow hypermedia principles: return HTML, use htmx for dynamic behavior

### Don't
- Introduce GPL or copyleft dependencies
- Add JavaScript runtime or complex template engines (keep it simple: C + SQL + htmx)
- Hard-code file lists into genrules (use globs as existing system does)

---
## Project Structure Pointers
- Binary target: `bin/BUILD.bazel`
- HTTP handlers: `hbf/http/server.c` (all endpoint logic)
- Database: `hbf/db/db.c` (SQLite init + global handle)
- Main entry: `hbf/shell/main.c`
- Tests enumerate behaviors: search `*_test.c` under `hbf/`

---
## API & Behavior Guarantees (Current Code)
**HTTP Endpoints (C handlers)**:
- `GET /health` → `{"status":"ok"}` ([server.c:13](hbf/http/server.c:13))
- `GET /` → Homepage with htmx ([server.c:139](hbf/http/server.c:139))
- `GET /links` → HTML list stub ([server.c:170](hbf/http/server.c:170))
- `GET /fragment/*` → Fragment stub ([server.c:194](hbf/http/server.c:194))
- `POST /fragment` → Fragment creation stub ([server.c:226](hbf/http/server.c:226))

**Request Routing**:
- All routes handled by direct C functions registered in `hbf_server_start()` ([server.c:184-188](hbf/http/server.c:184-188))
- No middleware, no dynamic routing, no JavaScript - pure C handlers
- CivetWeb thread pool handles concurrency (4 threads by default)

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

**Implemented (Phase 0):**
- Core hypermedia endpoints (/, /health, /links, /fragment/*)
- SQLite with WAL mode for concurrency
- Clean C99 architecture without JavaScript runtime
- Direct C handler routing via CivetWeb

**Not Yet Implemented (Phase 1):**
- Hypermedia database schema (nodes, edges, templates)
- Template renderer with {{key}} substitution
- CRUD API for fragments/templates/links
- Local htmx embedding (currently loads from CDN)
- SSE/WebSocket endpoints for real-time updates

**Verified Compile-Time Flags** (see `third_party/civetweb/civetweb.BUILD`):
- ✅ `-DNO_SSL` (line 16) - SSL/TLS disabled
- ✅ `-DNO_FILES` (line 18) - File serving disabled
- ✅ `-DUSE_IPV6=0` (line 19) - IPv4 only
- ✅ `-DUSE_WEBSOCKET` (line 20) - WebSocket enabled for future use

---
## Hypermedia Migration Roadmap

**Reference:** `hypermedia-impl-phase1.md` (revised addendum lines 99-154)

**Goal:** Pure C99 hypermedia server using SQLite + htmx

**Phase 0: Core Simplification (✅ COMPLETE - 2025-11-16)**
Result: Minimal C99 server (2.2MB binary) serving `/health`, `/`, `/links`, `/fragment/*` endpoints
- ✅ Deleted QuickJS, pods, and JS infrastructure
- ✅ Updated build system to remove pod_binary macro
- ✅ Simplified C source (removed handler.c, overlay_fs.c)
- ✅ Verification: Binary builds and runs cleanly

**Phase 1: Hypermedia API (NEXT)**
Tasks:
1. Database schema: `hbf/db/hypermedia_schema.sql` with `nodes`, `edges`, `view_type_templates` tables
2. C-based HTTP handlers: `hbf/http/api_handlers.c` for CRUD endpoints
3. Template renderer: `hbf/http/renderer.c` for `{{key}}` substitution
4. Homepage: Seed data + lazy-loading with htmx
5. Asset embedding: Bundle htmx.min.js from Bazel `@htmx` repo
6. Core routes: Serve embedded assets under `/_hbf/` prefix from memory

---
## FAQ (Repo-Specific)
Q: How do I add a new HTTP endpoint?
A: Add a C handler function in `hbf/http/server.c` and register it in `hbf_server_start()` using `mg_set_request_handler()`.

Q: How do I query the database from a handler?
A: Use `sqlite3 *db = hbf_db_get();` to get the global database handle, then use standard SQLite3 C API.

Q: How to run a single test verbosely?
A: `bazel test //hbf/db:db_test --test_output=all`

Q: Can I use JavaScript/TypeScript?
A: No. Phase 0 removed all JavaScript runtime support. Use C + SQL + htmx for dynamic behavior.

Q: How do I serve static assets?
A: Phase 1 will add asset embedding. For now, use htmx from CDN in HTML responses.

---
## Commands (Copyable Reference)
```
# Build
bazel build //:hbf
# Run
bazel run //:hbf -- --port 5309 --log_level debug
# Or run directly
bazel-bin/bin/hbf --port 5309
# Test
bazel test //...
bazel test //hbf/db:db_test --test_output=all
# Lint
bazel run //:lint
```

---
## License Summary
Project: MIT. Allowed dependency licenses: MIT, BSD, Apache-2.0, Public Domain. Source files should keep original third_party license notices.

---
## Maintainability Principles
- Prefer incremental enhancement over rewrites
- Keep `AGENTS.md` concise; move deep explanations to `DOCS/`
- Update this file when adding build/test commands or structural changes
- Follow hypermedia principles: HTML over the wire, htmx for interactivity

---
(End of AGENTS.md)
