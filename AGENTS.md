# AGENTS.md

A concise, agent-focused guide to working inside the HBF repository. Think of this as a README for AI coding agents: what the project is, how to build/test/lint, coding standards, safety rules, and implementation facts you can rely on. Keep output small, accurate, and incremental. Prefer precise file paths.

---
## Project Overview
HBF is a single, statically linked C99 web compute environment built with Bazel 8 (bzlmod) that embeds:
- SQLite (WAL, JSON1, FTS5, SQLAR) as unified store for data + static assets
- CivetWeb for HTTP and SSE (no TLS/SSL in binary; terminate behind reverse proxy)
- QuickJS-NG for per-request sandboxed JavaScript (fresh runtime/context each request; ~64MB mem cap; ~5000ms timeout)
- Pod-based architecture: each pod supplies `hbf/server.js` and `static/**` assets, embedded into the final binary via SQLAR → C array pipeline.

Single binary outputs under `bazel-bin/bin/` (e.g. `hbf`, `hbf_test`). All HTTP requests enter CivetWeb; `/static/**` served directly from embedded DB; all other routes handled by evaluating `hbf/server.js` for the active pod.

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
bazel run //:hbf -- --port 5309 --log_level debug --dev
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
- Entry: `hbf/server.js` in each pod, loaded from embedded SQLAR for every non-static request.
- Fresh QuickJS runtime + context per request (no pooling).
- Host bindings: request/response (`bindings/request.c`, `bindings/response.c`), console, db (queries against main SQLite).
- Static assets resolved under `/static/*` directly in C (no JS involvement).
- Planned router upgrade (see `js_import_router.md`): integrate ESM find-my-way; current implementation uses manual path conditional logic (verify in pods/test or base `server.js`).

---
## Versioned / Embedded Filesystem
Embedded pod content packed into SQLAR at build via pipeline:
1. Collect pod `hbf/*.js` + `static/**`.
2. Create SQLAR (sqlite3 -A).
3. VACUUM optimize.
4. Convert DB to C (`db_to_c.sh` → `*_fs_embedded.c` with `fs_db_data`, `fs_db_len`).
5. Link via `pod_binary()` macro (see `tools/pod_binary.bzl`).
Runtime: Reads from main SQLite DB (WAL) using functions in `hbf/db/db.c` and overlay code (`overlay_fs.c`) for version tracking.

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
- Health endpoint: `GET /health` returns `{"status":"ok"}` (C handler).
- Routes not under `/static/` invoke QuickJS `app.handle(req,res)` from pod `server.js`.
- Static files: paths like `/static/style.css` served directly from SQLite SQLAR.
- Each request: new QuickJS runtime/context, then destroyed.
- Build outputs: static binaries at `bazel-bin/bin/hbf*` (musl, no dynamic libs).

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
## Extensibility Notes (Forward-Looking)
Documentation references future overlay dev endpoints and ESM router integration (see `phase2-overlay-plan.md`, `js_import_router.md`). Only implement features documented in code; speculative items belong in a follow-up proposal. Record unknowns or unverifiable claims in `agent_help.md`.

---
## FAQ (Repo-Specific)
Q: Where do I add a new pod?  A: Create `pods/<name>/` with `BUILD.bazel`, then add `pod_binary()` invocation in root `BUILD.bazel`.
Q: How do I read embedded static assets?  A: Served automatically over `/static/*` by C; dynamic JS via `server.js`.
Q: How to adjust JS limits?  A: See `hbf/qjs/engine.c` (memory/timeout definitions).
Q: How to run single test verbosely?  A: `bazel test //hbf/db:overlay_fs_test --test_output=all`.

---
## Commands (Copyable Reference)
```
bazel build //:hbf
bazel build //:hbf_test
bazel run //:hbf -- --port 5309 --log_level debug --dev
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
