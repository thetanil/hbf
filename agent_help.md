# agent_help.md

Unresolved or uncertain items found while drafting and verifying `AGENTS.md`. These either reference planned features, ambiguous documentation, or concepts not fully implemented in the current codebase. Agents should treat these as questions or future work, not established facts.

## 1. Overlay Filesystem Runtime Usage
Documentation (CLAUDE.md, overlay plans) describes a dev overlay (`latest_fs`, `fs_layer`, `overlay_sqlar`) and live edit endpoints (`/__dev/api/file`, etc.). Current codebase includes `overlay_fs.c` and `overlay_fs_test.c`, but:
- No confirmed production wiring of overlay read path into static handler (need to inspect `hbf/http/server.c` for dev flag usage; not yet read in this verification pass).
- Dev API endpoints for file read/write (`/__dev/api/file`, `__dev/api/files`) appear in benchmark docs but may not exist in current `pods/*/hbf/server.js`.
Action: Inspect `hbf/http/server.c`, `pods/base/hbf/server.js`, `pods/test/hbf/server.js` for actual dev endpoints and cache header logic; update AGENTS.md if implemented.

## 2. Router Integration (find-my-way)
`js_import_router.md` outlines integration of the `find-my-way` ESM router. Current pod `server.js` references manual routing (`app.handle` with conditionals). Need confirmation whether router has been imported yet.
Action: If not integrated, AGENTS.md correctly marks as "planned". Agents should not assume router APIs until `import createRouter` appears in pod code.

## 3. Memory/Timeout Constants
AGENTS.md cites ~64MB memory limit and ~5000ms timeout from docs. Verification requires viewing `hbf/qjs/engine.c` to confirm constants or macros. Not read yet in verification step.
Action: Open `hbf/qjs/engine.c` and verify values; adjust if different.

## 4. Static Asset Serving vs Overlay
Statement: "Static assets resolved under `/static/*` directly in C" is correct, but whether overlay modifications can affect these reads (dev mode) is not confirmed without source review for overlay integration.
Action: Confirm if `overlay_fs` functions or alternate view (`latest_files`) are used; if not, clarify static path is base SQLAR only.

## 5. VACUUM Optimization in Pod Build Pipeline
AGENTS.md mentions VACUUM optimization. Verified in pod BUILD files: both `pods/base/BUILD.bazel` and `pods/test/BUILD.bazel` run `VACUUM INTO`. ✅ No action.

## 6. SSE Support Mention
CLAUDE.md references CivetWeb for HTTP and SSE. Need confirmation of any actual SSE endpoint implementation in `hbf/http/server.c`. Not verified yet.
Action: Inspect `server.c` for Server-Sent Events handlers before claiming SSE availability.

## 7. Engine Timeout Enforcement
Need to confirm that QuickJS timeout (~5000ms) is enforced (likely via interrupt handler). Requires reading `engine.c` or `handler.c`.
Action: Read and verify actual mechanism (e.g., `JS_SetInterruptHandler`).

## 8. Per-Request Runtime Guarantee
Assumption: fresh runtime/context each request. Need to confirm no pooling or reuse in `handler.c`.
Action: Inspect `hbf/http/handler.c` for lifecycle (create → execute → free). Adjust wording if reused.

## 9. Benchmark Claims
Benchmark skill documents throughput numbers (e.g., 20k+ req/sec) which may depend on environment. AGENTS.md omits raw performance figures to avoid environment-specific claims. ✅ Intentional.

## 10. Absence of Module Import Support
Docs mention planned ESM module support. Need to verify if `module_loader.c` currently resolves bare specifiers or only simple relative imports. If partial support exists, refine guidance.
Action: Review `hbf/qjs/module_loader.c`; update AGENTS.md if import map or vendor logic implemented.

## 11. Dev Flag Cache Headers
AGENTS.md notes planned conditional cache headers. Must verify if `server.c` currently sets different headers when `--dev` is passed. If not implemented, mark as future.

## 12. Database Access From JS
AGENTS.md states DB queries via host-provided `db` module. Need to confirm exported API surface (e.g., `query`, `execute` naming) from `db_module.c`.
Action: Inspect `hbf/qjs/db_module.c` (not yet read) and list exact functions for future refinement.

## 13. Security Constraints (No TLS/SSL/IPV6/FILES)
CLAUDE.md lists limitations (NO SSL/TLS/IPV6/FILES). Need confirmation these are compile-time options (e.g., civetweb build flags) present in `third_party/civetweb.BUILD`.
Action: Review civetweb build config.

## 14. Pod Binary Size Figures
CLAUDE.md mentions sizes (e.g., 5.3MB stripped, 13MB debug). Not re-verified in current build artifacts. AGENTS.md intentionally omits exact size to avoid drift.

## 15. Unstripped Alias Behavior
Macro creates `<name>_unstripped` alias pointing to same target; if a truly unstripped binary is desired, additional logic may be needed. AGENTS.md references alias generically. Confirm if this could mislead; consider clarifying in future revision.

## 16. Overlay FS API Coverage
AGENTS.md lightly describes versioned FS. Overlay code may expose more granular APIs (e.g., migrate SQLAR). Not exhaustively documented yet.
Action: Enumerate public functions from `overlay_fs.h` for future agent hints.

---
## Proposed Next Verification Steps (ordered)
1. Read `hbf/http/server.c` and `handler.c` (dev flag usage, SSE, per-request lifecycle).
2. Read `hbf/qjs/engine.c` (memory limit + timeout enforcement).
3. Read `hbf/qjs/module_loader.c` (module import status).
4. Read `hbf/qjs/db_module.c` (JS DB API surface).
5. Read `hbf/db/overlay_fs.h` (public API list).
6. Inspect `third_party/civetweb/civetweb.BUILD` for disabled features.
7. Adjust AGENTS.md with any confirmed discrepancies; append new open items here if not resolvable.

---
## Guidance for Agents
Treat items above as questions. Do not assume planned features (router, dev overlay endpoints, SSE) are active unless code confirms. When needed, perform focused file reads before relying on a feature.

(End of agent_help.md)
