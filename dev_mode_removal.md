# Dev Mode Removal Plan

Objective: Remove the `--dev` flag, the `req.dev` concept, and all `__dev`-prefixed routes / documentation so that the server operates in a single always-on editable mode that will later be guarded by authentication/authorization (JWT) in a future change.

This document enumerates every occurrence found in the repository that references dev mode and proposes the smallest safe removal or refactor steps. After implementing these, follow-up work will introduce auth-based gating; until then, the formerly dev-only endpoints become standard admin/editor endpoints (or are temporarily disabled if not needed immediately).

---
## Guiding Principles
- Minimize churn: delete or simplify instead of large rewrites.
- Preserve versioned filesystem write capability (keep overlay FS APIs).
- Avoid breaking unrelated tests; update or remove tests tied only to `--dev` flag parsing.
- Maintain C99 warning-free build. Remove dead code branches (cache headers, 403 checks).
- Document changed behavior in `README.md` and `AGENTS.md` (no dev flag; endpoints either removed or renamed).
- DO NOT add auth now; just eliminate dev mode scaffolding cleanly.

---
## Categorized Occurrences & Actions

### 1. CLI / Config Parsing
Files:
- `hbf/shell/config.c` (prints `--dev`, parses it)
- `hbf/shell/config_test.c` (tests enabling dev mode)
- `README.md` (lists `--dev` usage)
- `AGENTS.md` (run command examples with `--dev`)

Actions:
- Remove `--dev` option and associated boolean in config struct (if present).
- Delete usage output line.
- Update tests: remove cases constructing argv with `--dev`; adjust expectations.
- Update README and AGENTS run examples (drop `--dev`).

### 2. Request Object Binding / QuickJS
Files:
- `hbf/qjs/bindings/request.h` (mentions dev flag comment)
- `hbf/qjs/bindings/request.c` (sets `req.dev` property)

Actions:
- Remove `req.dev` property entirely from JS request object.
- Update comments reflecting no dev mode.
- Search pods' JavaScript for `req.dev` guards and remove them (see Section 5).

### 3. HTTP Server Logic (C)
Files:
- `hbf/http/server.c`:
  - Cache header conditional (no-store vs max-age=3600).
  - X-HBF-Dev response header logic.
  - `dev_files_list_handler` for `/__dev/api/files` with dev-only 403 check.
- `hbf/http/handler.c` (comment referencing overlay support in dev mode).
- `hbf/db/db.h` & `hbf/db/overlay_fs.h` comments referencing dev mode reads.

Actions:
- Remove conditionals keyed on dev flag (always use production caching policy OR decide new default: choose `no-store` only for modified writes? For now: use `Cache-Control: max-age=3600` and drop dev variability).
- Remove `X-HBF-Dev` header entirely.
- Remove or rename `/__dev/api/files` handler:
  - Option A: Keep endpoint but rename to `/__fs/api/files` (future admin) without gating.
  - Option B: Remove endpoint now if not needed. (Recommend keep under new path for continuity.)
- Update comments in handler.c & db headers.

### 4. Tooling / Integration Scripts
Files:
- `tools/integration_endpoints_403.txt` (expects 403 when not in dev mode)
- `tools/integration_endpoints_200.txt` (DEV endpoints list)
- Benchmarking assets in `.claude/skills/benchmark/` referencing dev mode (shell scripts and markdown: `benchmark.sh`, `SKILL.md`, `README.md`, `SUMMARY.md`, `lib/server.sh`).

Actions:
- Remove lines describing dev-only expectations; delete 403 file or repurpose to track future auth-required endpoints (rename to `integration_endpoints_auth.txt` placeholder?).
- Adjust 200 endpoints file: remove DEV prefix lines or rename endpoints if changed.
- Update benchmark scripts to run single server instance without `--dev`; prune category 3 description referencing dev mode.
- Keep performance tests for filesystem writes using new always-available endpoints.

### 5. Pod JavaScript & Static Editor Assets
Files:
- `pods/base/hbf/server.js`
- `pods/test/hbf/server.js`
- `pods/test/static/dev/index.html` (editor UI)

Occurrences:
- Conditional checks: `if (!req.dev) { ... 'Dev mode not enabled' }`
- Link in index page `/__dev/` labeled dev editor.
- Routes: `/__dev/`, `/__dev/api/file`, `/__dev/api/files` (GET/PUT/DELETE).

Actions:
- Remove all `req.dev` checks; routes become always available.
- Optional: Rename route prefix from `__dev` to `__edit` or `__fs` for forward compatibility. (If rename chosen, coordinate with C handler rename plan in Section 3.)
- Update UI text removing "dev mode" phrasing.
- Ensure dynamic file list previously served in JS still aligns with C handler path if renamed.

### 6. Documentation
Files:
- `AGENTS.md`, `README.md`, `DOCS/custom_content.md`, `DOCS/big_idea.md`, `DOCS/hbf_spec.md`
- Multiple references to dev mode enabling, examples with `--dev`, troubleshooting sections, cache header explanation.

Actions:
- Systematically remove `--dev` mentions; replace with description of always-on editor endpoints.
- Clarify future gating by JWT-based auth (placeholder statement).
- Remove troubleshooting sections specific to dev mode visibility (rewrite focusing on caching semantics if still relevant).
- Update examples: drop `bazel run //:hbf -- --dev`; just `bazel run //:hbf`.

### 7. Tests
Files:
- `hbf/shell/config_test.c` uses `--dev` in argv arrays.
- Possibly integration tests referencing 403 responses when not in dev (`integration_endpoints_403.txt`).

Actions:
- Modify config tests to remove dev flag cases (adjust expected option count if necessary).
- Remove 403 expectation tests (since no gating until auth arrives).
- Add new test (optional) ensuring filesystem endpoints reachable without flag.

### 8. Headers & Comments
Files:
- Comments in `server.c`, `handler.c`, `db.h`, `overlay_fs.h`, request bindings referencing dev mode.

Actions:
- Rewrite comments to remove dev references; describe overlay filesystem as universally active.

### 9. Build / Run Examples in Scripts
Files:
- Any script appending `--dev` (e.g., `.claude/skills/benchmark/lib/server.sh`).

Actions:
- Remove parameter logic; single invocation style.

---
## Stepwise Implementation Strategy
1. Remove CLI parsing & tests (Section 1) – simplest, isolates flag.
2. Eliminate `req.dev` property (QuickJS bindings) and patch pod JS to drop checks (Sections 2 & 5).
3. Adjust server C logic: headers, handler rename or removal (Section 3). Decide route naming (choose `__fs` for clarity). Provide compatibility alias (`/__dev/api/files` -> 301 or mirror) temporarily if desired.
4. Update tooling scripts & integration endpoint lists (Section 4).
5. Update docs and README/AGENTS concurrently (Section 6) to avoid inconsistency.
6. Run full test suite; fix breakages (Section 7). Add small test for new always-on endpoint.
7. Cleanup comments & minor references (Section 8 & 9).
8. Final lint & tests; commit.

---
## Potential Risks & Mitigations
- Missing a reference leading to stale docs: Mitigate with a second grep after edits for `--dev` and `__dev`.
- Third-party performance assumptions around cache headers: choose conservative caching (keep `max-age=3600`) until auth introduces secure editing; editors can manually refresh.
- Route rename churn: If stability required, keep `__dev` for now but drop gating; document future migration to `__fs` when auth added.

---
## Decision Points To Confirm Before Code Changes
- Keep route prefix as `__dev` (always-on) vs rename now? (Default: keep to minimize diff.)
- Retain `/__dev/api/files` C handler or remove entirely? (Default: retain; it’s useful.)
- Cache header policy: Always `max-age=3600` or always `no-store` for editor endpoints? (Default: `max-age=3600` for static assets; editor API endpoints can send `no-store`.)

Indicate choices; then proceed with implementation.

decision: remove all the __dev related routes and handlers. use defaults suggested.

---
## Post-Removal Verification Checklist
- grep `--dev` returns zero matches (excluding historical logs / benchmarks if retained).
- grep `req.dev` returns zero matches.
- Tests: `bazel test //...` passes.
- Lint: `bazel run //:lint` passes.
- Editor endpoints accessible without flag; PUT creates new file versions.

---
## Future Auth Integration Placeholder
Add JWT-based middleware once implemented:
- Validate token before allowing write/delete endpoints.
- Possibly add role-based header `X-HBF-Role` instead of former `X-HBF-Dev`.

---
(End of dev_mode_removal.md)
