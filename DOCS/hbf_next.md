# HBF Next Steps: Live Dev Environment, HTMX Template, and Monaco Editor

This document outlines a detailed, step‑by‑step plan to stand up a live development environment in HBF that supports editing assets stored in the SQLite SQLAR archive (e.g., `static/index.html`) and viewing changes live in a preview iframe. It also covers how to package and serve HTMX and the Monaco editor entirely from the embedded filesystem, including Bazel strategies for pulling third‑party assets.

## Starting point (current state)

From `DOCS/hbf_spec.md` and the codebase:
- Static assets under `/static/**` are served from the main DB’s SQLAR via CivetWeb (`internal/http/server.c`). Root `/` maps to `static/index.html`.
- Catch‑all requests are handled by QuickJS (`internal/http/handler.c`), loading and executing `hbf/server.js` from the main DB’s SQLAR on each request.
- The main DB is created from an embedded `fs.db` template at first run or in `--inmem` mode; runtime reads always use the main DB.
- QuickJS exposes `db.query(sql, params)` and `db.execute(sql, params)` for direct SQL access inside `hbf/server.js`.
- Packaging: `//fs:fs_archive` currently only inserts `fs/hbf/server.js` and `fs/static/index.html` into `fs.db`.

Assumptions for the plan:
- Dev endpoints are acceptable to live under `/{__dev|_admin}` and be gated by a `--dev` flag.
- For the fastest path, we can initially update rows directly in the `sqlar` table (no separate overlay), then iterate to a true overlay.

## Goals

- Include HTMX and Monaco in the embedded filesystem (no CDN) so the single binary serves all assets.
- Provide a dev UI with Monaco that edits `static/index.html` (and other files) in the DB and shows updates live in a preview iframe.
- Define the evolution path from “direct write” to a proper dev overlay with read‑through and tombstone semantics.
- Keep Bazel fetches deterministic and pinned; prefer hermetic `http_archive`/`http_file` over network at build time.

## Phase 0 — Baseline build and server hygiene

Dependencies: none

- Expand `//fs:fs_archive` to package all of `fs/static/**` and `fs/hbf/**`:
  - Use the existing `srcs = glob(["hbf/**/*", "static/**/*"], exclude=["BUILD.bazel"], exclude_directories=1)` to feed `sqlite3 -A -c -f $@ $(SRCS)` so every tracked static file is archived.
  - Add `style.css`, `assets/`, and future vendor folders.
- MIME types: add a few that Monaco commonly needs if not already present: `text/css`, `application/javascript`, `application/json`, `image/svg+xml`, `font/woff`, `font/woff2`, `application/wasm`, `text/plain`, `text/markdown`, `application/octet-stream` fallback. Extend `internal/http/server.c:get_mime_type` as needed.
- Dev caching: when `--dev` is set, serve `/static/**` with `Cache-Control: no-store` to avoid stale previews. In prod keep a public cache header.

Deliverable: broader `fs.db` coverage, correct MIME, predictable dev caching.

## Phase 1 — Bundle HTMX and Monaco assets (pinned http_archive)

Dependencies: Phase 0

Principle: Do not vendor assets in this repo; fetch pinned, hermetic archives with Bazel and stage them into the embedded filesystem at build time.

Steps:

1) In `MODULE.bazel` enable `http_archive` and pin versions:

   - Add: `http_archive = use_repo_rule("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")`.
   - Add pinned archives (choose versions and fill in integrity/sha256):

   - Monaco Editor
     - Example:
       - name: `monaco_editor`
       - url: `https://github.com/microsoft/monaco-editor/archive/refs/tags/v0.49.0.tar.gz`
       - strip_prefix: `monaco-editor-0.49.0`
       - build_file_content: expose a filegroup `min_vs = glob(["min/vs/**"])`

   - HTMX
     - Prefer `http_archive` pointing at a release tag zip/tarball (e.g., `https://github.com/bigskysoftware/htmx/archive/refs/tags/1.9.12.zip` or newer), with `build_file_content` that exposes `dist/htmx.min.js` as a target (e.g., `:htmx_min`).

   Note: Set `integrity` (or `sha256`) for both archives once versions are selected to ensure reproducibility.

2) Expose targets from the archives (via `build_file_content`):

   - For Monaco:
     - `filegroup(name = "min_vs", srcs = glob(["min/vs/**"]))`
   - For HTMX:
     - `filegroup(name = "htmx_min", srcs = ["dist/htmx.min.js"])` (adjust path if the release layout differs)

3) In `fs/BUILD.bazel`, stage these external files into the archive before invoking `sqlite3 -A -cf`:

   - Amend the `:fs_archive` genrule to:
     - Create a local staging tree `static/monaco/min/vs/**` and `static/vendor/htmx.min.js` inside the genrule’s sandbox.
     - Copy from `$(locations @monaco_editor//:min_vs)` into `static/monaco/min/vs/` preserving structure.
     - Copy from `$(location @htmx//:htmx_min)` to `static/vendor/htmx.min.js`.
     - Then build `fs.db` from `hbf/**` + `static/**` so these assets are included in SQLAR.

4) Confirm by test:
   - Extend `fs/fs_build_test.sh` to check presence of `static/monaco/min/vs/loader.js` and `static/vendor/htmx.min.js` in the archive.

Why http_archive (not vendored):
- Keeps repo lean, upgrades explicit, and ensures hermetic, pinned builds without npm/node.
- `build_file_content` avoids needing upstream Bazel files; we create minimal `filegroup` targets inside the external repo context.

Deliverable: `GET /static/monaco/min/vs/loader.js` and `GET /static/vendor/htmx.min.js` both work from the single binary, with versions pinned via Bazel module configuration.

### Monaco Vim (Vim mode) — pinned http_archive, same-origin serving

Goal: Enable Vim keybindings in Monaco using `monaco-vim`, served entirely from our origin to satisfy strict CSP and offline builds.

Options considered:
- A) Use CDN (unpkg) at runtime — rejected due to CSP and offline constraints.
- B) Use `http_archive` to fetch the `monaco-vim` npm tarball or GitHub release, and serve `dist/monaco-vim.js` from `/static/vendor/monaco-vim/` — recommended.
- C) Build from source (rollup/webpack) — possible but introduces a JS toolchain; our current plan avoids node/npm for hermeticity and simplicity.

Recommended (B) steps:
1) In `MODULE.bazel` add a pinned `http_archive` for monaco-vim:
   - Source suggestion: npm tarball
     - `https://registry.npmjs.org/monaco-vim/-/monaco-vim-<VERSION>.tgz` with `strip_prefix = "package"`
     - or GitHub release archive `https://github.com/brijeshb42/monaco-vim/archive/refs/tags/v<VERSION>.tar.gz`
   - Provide `build_file_content` that exposes `filegroup(name = "dist", srcs = ["dist/monaco-vim.js"])` (adjust if path differs).
   - Pin `sha256`/`integrity`.

2) In `fs/BUILD.bazel` `:fs_archive` genrule, copy `@monaco_vim//:dist` into `static/vendor/monaco-vim/monaco-vim.js` before archiving, alongside Monaco and HTMX.

3) Dev UI wiring (no bundler):
```
<script src="/static/monaco/min/vs/loader.js"></script>
<script src="/static/vendor/monaco-vim/monaco-vim.js"></script>
<script>
  require.config({ paths: { 'vs': '/static/monaco/min/vs' } });
  require(['vs/editor/editor.main'], function () {
    const editor = monaco.editor.create(document.getElementById('editor'), {
      value: '<h1>Hello Vim</h1>',
      language: 'html'
    });
    const status = document.getElementById('vim-status');
    // UMD global provided by dist build
    const vim = MonacoVim.initVimMode(editor, status);
  });
<\/script>
```

CSP notes:
- Same-origin: All scripts served from `/static/**` on our domain; no cross-origin fetches.
- Monaco AMD loader often requires `'unsafe-eval'` due to dynamic function wrappers. For strict CSP:
  - Dev mode: allow `script-src 'self' 'unsafe-eval'`; allow workers via `worker-src 'self' blob:`; allow fonts/images `font-src/img-src 'self'`.
  - Future strict mode: consider switching to Monaco ESM build and a bundling step to remove `unsafe-eval` needs (trade-off: introduces a JS bundler into the toolchain).
- Avoid inline script where possible; or emit nonces if we must inline.

Validation:
- Extend `fs/fs_build_test.sh` to assert presence of `static/vendor/monaco-vim/monaco-vim.js`.
- Manual: open dev UI, verify `MonacoVim.initVimMode` works and keystrokes map to Vim commands.

## Phase 2 — Minimal live edit loop (direct sqlar writes)

Dependencies: Phases 0–1

Objective: Prove “edit → save → preview” without changing C code.

- Dev UI route in `fs/static/dev/index.html` (or `/_ _dev` served via JS):
  - Left: Monaco editor; Right: `<iframe src="/"></iframe>` or `/static/index.html`.
  - Load current file content via `GET /__dev/api/file?name=static/index.html`.
  - On save, `PUT /__dev/api/file?name=static/index.html` with raw body.
  - After save returns 200, reload the iframe (`iframe.contentWindow.location.reload()`); no SSE needed initially.
- Implement dev API in `fs/hbf/server.js` using `db.query/execute`:
  - `GET /__dev/api/file?name=...` → `SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?` and return bytes as text/plain (or detected content-type).
  - `PUT /__dev/api/file?name=...` → `INSERT OR REPLACE INTO sqlar(name,mode,mtime,sz,data) VALUES(?,?,?,?,?)` with:
    - `mode=33188` (regular file, 0644),
    - `mtime = strftime('%s','now')`,
    - `sz = length(?)`, `data = ?` — uncompressed is OK since reads call `sqlar_uncompress(data, sz)` and return `data` when `sz == length(data)`.
  - Optional: validate the `name` is under `static/` or `hbf/` to avoid arbitrary writes.
- Gate these dev endpoints behind a simple check (only when `--dev`). Short‑term options:
  - Add a custom response header (e.g., `X-HBF-Dev: 1`) from C when `--dev` is on, and in JS read `req.headers['x-hbf-dev']`.
  - Or restrict the route by a secret query (`/__dev?token=...`) for now.

Deliverable: Open `/__dev`, edit `index.html`, press save, iframe reload shows changes.

## Phase 3 — HTMX template in `static/index.html`

Dependencies: Phase 2

- Update `static/index.html` to include HTMX from `/static/vendor/htmx.min.js`.
- Demonstrate:
  - A partial area that responds to `hx-get="/echo"` with JSON pretty‑print.
  - A small “live snippet” area where editing a fragment in the editor saves to `static/fragment.html` and the page includes it via `<div hx-get="/static/fragment.html" hx-trigger="load, every 2s" hx-swap="outerHTML">` for a simple live view (until SSE lands).
- Stretch: Introduce a light `res.hx()` helper in `server.js` to set HX‑* headers.

Deliverable: `index.html` demonstrates htmx interactions and plays nicely with the dev editor.

## Phase 4 — Monaco wiring details

Dependencies: Phase 1

- Serve Monaco from `/static/monaco/min/vs/` and configure loader:
  - `<script>require.config({ paths: { 'vs': '/static/monaco/min/vs' } });</script>`
  - `<script>require(['vs/editor/editor.main'], function() { /* create editor */ });</script>`
- Instantiate the editor in HTML mode to edit `static/index.html`:
  - Fetch file content via the dev API; populate the model.
  - Save button: `fetch('/__dev/api/file?name=static/index.html', { method:'PUT', body:editor.getValue() })` then reload preview iframe.
- Usability nice‑to‑haves: dirty indicator, keyboard shortcut `Ctrl/Cmd+S`, status toast.

Deliverable: Editor is fast, no CDN, and preview reload works reliably.

## Phase 5 — True dev overlay (read‑through mask)

Dependencies: Phase 2 (can run in parallel with Phase 3–4 once API proved)

Motivation: Don’t mutate base `sqlar`; allow “mask over” edits and easy revert.

- Schema (materialized overlay + history):
  - `overlay_sqlar(name TEXT PRIMARY KEY, mtime INT, sz INT, data BLOB, deleted INT DEFAULT 0)` — fast "current view" used on hot paths.
  - `fs_layer(id INTEGER PRIMARY KEY AUTOINCREMENT, layer_id INT, name TEXT, op TEXT CHECK(op IN ('add','modify','delete')), mtime INT, sz INT, data BLOB)` — append‑only change log that records each update.
  - Triggers: on `INSERT` into `fs_layer`, update `overlay_sqlar` to reflect the latest op for `name` (set `deleted=1` on delete; upsert `data/sz/mtime` on add/modify).
- Read path (dev mode only):
  - Hot path (C static handler and JS loader): read from `overlay_sqlar` first; if not present or `deleted=1`, fall back to base `sqlar`.
  - Admin/debug paths can query `fs_layer` history.
- Write path (dev API): append an op row to `fs_layer`; triggers maintain `overlay_sqlar`. Optional: allow a direct upsert into `overlay_sqlar` when history is not needed.
- Integrations:
  - Add `hbf_db_read_file_dev(sqlite3 *db, const char *name, ...)` that encapsulates overlay‑first logic; use in C static handler.
  - In QuickJS request handler, when loading `hbf/server.js`, prefer overlay in dev mode as well (enables live server code edits). Guard with safety: pin the evaluated source per request; crash‑safe fallbacks.
- Garbage/revert semantics: add `DELETE`/`undelete` endpoints; add a small list endpoint to view overlay vs base; occasional compaction that materializes current overlay into base when promoting.

Deliverable: Edits don’t touch base `sqlar`; toggling dev mode immediately switches back to base.

### Overlay design options: layered ops vs materialized overlay

There are two compatible designs for the overlay, and we can combine them:

- A) Materialized overlay (simple, fast reads)
  - Table: `overlay_sqlar(name PRIMARY KEY, mtime, sz, data, deleted)`
  - Reads: O(1) lookup in overlay; if missing, read base `sqlar`.
  - Writes: upsert row; set `deleted=1` for tombstones.
  - Pros: minimal SQL, fastest hot path (static handler/JS loader).
  - Cons: no history unless separately logged.

- B) Layered ops (append‑only history; supports multiple layers)
  - Table: `fs_layer(layer_id, name, op {'add','modify','delete'}, data, mtime, sz)`
  - Reads: either a window function over base+layers to pick the topmost op, or maintain a materialized view/table for current state via triggers.
  - Example read (your idea) to resolve the latest op for a given file name:
    - Union base `sqlar` (as layer 0) with `fs_layer`, then `ROW_NUMBER() OVER (PARTITION BY name ORDER BY layer_id DESC)` and select rank 1 with `op != 'delete'`.
  - Pros: full audit/history, supports multiple concurrent layers (e.g., dev/stage).
  - Cons: more complex reads if not materialized; per‑request window queries add overhead.

Recommended hybrid:
- Keep `fs_layer` for append‑only history and multi‑layer semantics.
- Maintain `overlay_sqlar` as the materialized "current" view via triggers, so hot paths do a single key lookup.
- For admin tools and diffing, query `fs_layer` directly or with window functions.

SQL sketches:

- Create tables:
```
CREATE TABLE IF NOT EXISTS overlay_sqlar (
  name TEXT PRIMARY KEY,
  mtime INT NOT NULL,
  sz INT NOT NULL,
  data BLOB,
  deleted INT NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS fs_layer (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  layer_id INT NOT NULL,
  name TEXT NOT NULL,
  op TEXT NOT NULL CHECK(op IN ('add','modify','delete')),
  mtime INT NOT NULL,
  sz INT NOT NULL,
  data BLOB
);
```

- Trigger (simplified) to keep overlay in sync:
```
CREATE TRIGGER fs_layer_apply AFTER INSERT ON fs_layer BEGIN
  CASE NEW.op
    WHEN 'delete' THEN
      INSERT INTO overlay_sqlar(name, mtime, sz, data, deleted)
      VALUES(NEW.name, NEW.mtime, 0, NULL, 1)
      ON CONFLICT(name) DO UPDATE SET mtime=excluded.mtime, sz=0, data=NULL, deleted=1;
    ELSE
      INSERT INTO overlay_sqlar(name, mtime, sz, data, deleted)
      VALUES(NEW.name, NEW.mtime, NEW.sz, NEW.data, 0)
      ON CONFLICT(name) DO UPDATE SET mtime=excluded.mtime, sz=excluded.sz, data=excluded.data, deleted=0;
  END;
END;
```

- REST read (JS) using a window function (no materialized overlay):
```
SELECT data FROM (
  WITH all_files AS (
    SELECT 0 AS layer_id, name, data, 'add' AS op FROM sqlar
    UNION ALL
    SELECT layer_id, name, data, op FROM fs_layer
  ),
  ranked AS (
    SELECT name, op, data,
           ROW_NUMBER() OVER (PARTITION BY name ORDER BY layer_id DESC) AS rnk
    FROM all_files
    WHERE name = ?
  )
  SELECT data FROM ranked WHERE rnk = 1 AND op != 'delete'
);
```

Streaming edits vs final snapshot:
- Start with snapshot‑on‑save (simple, robust for small/medium files edited in Monaco).
- Optionally record granular "edit" events (e.g., diff hunks or Monaco operations) into `fs_layer` for history/collab; add periodic snapshot checkpoints to avoid expensive replays.
- Reads should never reconstruct from diffs on hot paths; always hit `overlay_sqlar`.

## Phase 6 — Better live reload (SSE or polling)

Dependencies: Phase 2

- Initial: client‑side reload after successful save is sufficient.
- SSE later: Implement a small SSE broadcaster in C (CivetWeb keeps the connection; QuickJS per‑request model isn’t suited to holding SSE streams):
  - `GET /__dev/events` registers a connection; `POST /__dev/events/broadcast` from dev API triggers a send.
  - Or reuse WebSocket if preferred; CivetWeb supports both.
- For now, polling fallback with `hx-trigger="every 2s"` is acceptable.

Deliverable: Rock‑solid live reload without over‑complexity.

## Phase 7 — Security, flags, and DX polish

Dependencies: prior phases

- Respect `--dev` flag: expose to JS (e.g., set header or an env shim) and hard‑gate `/__dev` and dev APIs.
- Add ETag/Last‑Modified for static in prod; `no-store` in dev.
- Add tiny test coverage:
  - Build `//:hbf` and run a test that `PUT` to dev API changes `index.html` and `GET /` reflects change.
- Documentation: quickstart for the live dev loop.

## Order of operations (dependency‑aware)

1) Phase 0: Expand fs packaging + MIME + dev cache header.
2) Phase 1: Bring in assets (vendor HTMX; `http_archive` Monaco) and confirm they serve from `/static/...`.
3) Phase 2: Implement dev API in `server.js` and the basic dev UI page (`/__dev` or `/static/dev/index.html`). Prove edit/save/reload.
4) Phase 3: Enhance `static/index.html` to showcase HTMX and live fragment refresh; keep UI minimal.
5) Phase 4: Monaco ergonomics (keybindings, status, resize, HTML language mode).
6) Phase 5: Introduce `overlay_sqlar` and switch read/write paths in dev mode.
7) Phase 6–7: Optional SSE + security polish + tests/docs.

## Bazel specifics for assets

- `MODULE.bazel` additions:
  - `http_archive` for `monaco-editor` (pin version); optional `http_file` for `htmx.min.js` or just vendor it in `fs/static/vendor/`.
- `third_party/monaco/BUILD.bazel`:
  - `filegroup(name = "min_vs", srcs = glob(["@monaco_editor//min/vs/**"]))` (adjust paths as needed with `strip_prefix`).
- `fs/BUILD.bazel` changes:
  - Update `:fs_archive` to include all `srcs` and to stage `static/monaco/min/vs/**` (from external) into the build workspace directory that `sqlite3 -A` will archive.
  - Keep tests (e.g., extend `fs_build_test.sh`) to validate the presence of key files (loader.js) in the archive.

## Risks and mitigations

- Monaco size: pulling `min/vs/**` increases the binary size by a few MB. Mitigate by pinning a specific version and considering on‑demand subset later.
- SSE complexity: defer until basic reload loop is proven; polling or client‑side reload after save is fine.
- Overlay complexity: ship direct `sqlar` writes first, cut over to overlay when stable.
- Dev gating: short‑term soft checks; later wire `--dev` into both C and JS layers.

## Acceptance criteria

- `bazel run //:hbf -- --dev` serves:
  - `/static/vendor/htmx.min.js` and `/static/monaco/min/vs/loader.js` directly from the binary.
  - `/__dev` with a Monaco editor that loads `static/index.html`, saves via `PUT` API, and reloads the preview iframe on save.
  - `/` reflects the edited `index.html` after save.
- Optional: overlay phase passes basic mask‑over tests (serve overlay version when present; tombstone hides base).

## Follow‑ups

- Add minimal `res.hx()` helper and SSR partial rendering contract.
- Add list/diff view in dev UI; local persistence of unsaved changes.
- Consider a tiny component kit for htmx forms.
