# HBF big idea

A tiny, batteries‑included server‑side HTML platform that pairs: (1) a per‑request JavaScript sandbox (QuickJS) for logic, (2) SQLite for data and content (SQL + SQLAR), and (3) htmx + SSE for hypermedia interactivity—shipped as one binary—with a live, in‑browser dev loop (save → see) that never requires a rebuild while you iterate.

## What this is (mental model)

Think “FoxPro/Access forms + HyperCard’s live, user‑modifiable stacks + Smalltalk’s live tools,” but for the Web: HTML fragments and htmx drive UX; JavaScript runs server‑side in a fresh QuickJS context per request; SQLite is the source of truth for data and assets; and a dev overlay lets you edit and preview instantly.

- Display surface: the browser (HTML first; fragments and partials for SSR)
- Programmable environment: per‑request JavaScript (QuickJS), safe time/memory limits
- Data store: SQLite for relational data; SQLAR for app code/static assets
- Interactivity: htmx requests + SSE/WebSocket broadcast for realtime
- Dev ergonomics: an overlay on the main DB so saves go live without rebuilds

## Design influences → HBF primitives

- HyperCard (stacks/cards, HyperTalk, built‑in persistence, RAD)
  - Borrow: one tool to browse/edit/run; live edits persist; object/event model; plug‑ins (XCMD/XFCN → JS modules)
  - Map: HTML cards (pages) + partials; server‑side JS handlers are the scripts; stack = database image; live editing via DB overlay
  - Avoid: closed, single‑user file format; instead use SQLite + plain HTML/JS

- Microsoft Access (forms/reports, macros/VBA, split front/back end)
  - Borrow: end‑user approachable CRUD (forms/tables/queries), templates, split content (dev/stage/prod)
  - Map: htmx forms + helpers; SQL migrations/seeds; staging → promote in DB

- FoxPro/Visual FoxPro (xBase table‑centric, integrated language+DB, reports)
  - Borrow: data‑centric commands, indexes, fast prototyping on local DB; packaged runtime
  - Map: thin JS helpers that feel “data‑first” (query, render, commit) and ship everything in one binary

- Smalltalk (image‑based persistence, live IDE with browser/inspector/debugger, messaging)
  - Borrow: live editing while system runs; inspectors; continue‑after‑error; MVC discipline around HTML views and domain models
  - Map: dev UI with console/inspector; hot‑swap code with version pins; SSR fragments = “views”; data/models in SQLite

References: Wikipedia articles on HyperCard, Microsoft Access, Visual FoxPro, and Smalltalk (last accessed Oct 28, 2025).

## What’s next (immediate steps)

1) Minimal dev overlay (no rebuild) for `fs/` content
- Contract: Saving an asset (server JS or static) writes to an overlay table in the main DB; read path prefers overlay → base SQLAR; delete in overlay masks base.
- Success: Edit `fs/hbf/server.js` in the dev UI and see requests use the new code without `bazel build` or restarting (within safe reload semantics).

2) htmx helper in `server.js` (SSR contract)
- Contract: Request helpers expose HX‑* conventions and partial rendering:
  - `res.hx({ location|redirect|refresh|trigger|pushUrl })`
  - `render(view, data, { partial: true|false, layout })` with fragment slots
  - Implicit `Vary`/cache headers for htmx vs full HTML
- Success: htmx requests get partials; full GET gets layout‑wrapped page.

3) Simple dev route with Monaco editor and live preview
- Route: `/__dev` serves a static Monaco scaffold (no framework)
- Left: editor bound to files in the overlay; Right: iframe preview of app
- Auto‑refresh: SSE channel that reloads the iframe on save
- Success: “save → live” loop confirmed against the main DB overlay.

## Feature map (ranked by leverage → enables)

Tier 0 — Prove the loop
- Dev overlay read‑through with mask‑over semantics (enables: all dev UX)
- htmx helpers + partial rendering (enables: Todo, Blog, CRUD screens)
- Monaco dev route + SSE auto‑refresh (enables: Editor, ShaderToy preview)

Tier 1 — Core SSR + content
- Minimal templating helpers (layout, fragment slots, includes)
- Routing conveniences (params, 404/redirect, content negotiation)
- Static cache control and ETag helpers

Tier 2 — Write path and versioning
- Public write API to SQLAR (add/update/delete/list) with content hashes
- Staging vs production spaces; transactional promote/rollback
- Safe hot‑reload policy (pin current version per request; next request sees new)

Tier 3 — Live tools
- Server console → browser bridge; structured logs per request
- Inspector: last N requests, timing, DB queries (lite) in dev UI
- Error page with “view source” of SSR fragment and continue‑after‑fix

Tier 4 — Realtime and collaboration
- SSE helpers (`publish(channel, data)`) and subscriptions
- Basic WebSocket broadcast for multi‑user previews and chat

Tier 5 — Security & multi‑app
- CSP + secure headers by default; CSRF aligned to htmx
- Sessions and pluggable authZ hooks; per‑app namespaces in one binary

Tier 6 — Tooling & DX
- CLI: `hbf dev|open|pack|push|logs`
- Test harness: in‑proc server, HTML/partial snapshots, htmx request simulation

https://chatgpt.com/share/6901594d-ae70-8005-af57-4cf804b11f5a
## App archetypes → minimum feature sets

- Blog
  - Needs: SSR + layout/partials, routing, markdown render, static assets, basic cache headers
  - Nice: draft → publish staging, pagination helpers
  - Status: Tier 1 covers core; Tier 2 staging improves workflow

- Todo (htmx CRUD)
  - Needs: htmx helpers, partial rendering, form validation, DB CRUD helpers
  - Nice: optimistic updates over SSE, per‑user sessions
  - Status: Tier 0 + Tier 1 unlocks MVP

- Editor (live code/content)
  - Needs: dev route (Monaco), overlay write APIs, safe hot‑reload, console
  - Nice: diff/rollback, gist‑like share/export
  - Status: Tier 0 proves loop; Tier 2 solidifies write path

- ShaderToy‑style (no social)
  - Needs: live code editing, WebGL canvas in static page, preview iframe + SSE reload, asset serving
  - Nice: throttle/permits, snapshot share/export
  - Status: Tier 0 + Tier 1 sufficient; optimization later

Common prerequisites bubble to the top: (a) SSR contract + htmx helpers, (b) dev overlay + Monaco, (c) write APIs + hot‑reload safety.

## How the platform ideas shape HBF

- Live everywhere (Smalltalk/HyperCard): edits change the running world; HBF achieves this via DB overlay, per‑request VM instances, and version‑pinned swaps.
- Data‑centric (FoxPro/Access): CRUD is first‑class; provide tiny helpers that reduce boilerplate between SQL, HTML, and htmx forms.
- Hypermedia UX (HyperCard → Web): events are clicks/inputs; handlers return HTML fragments or signals (HX‑*). Keep the contract tight and boring.

## Near‑term acceptance criteria (engineering contracts)

- Overlay
  - Read path: overlay entry if present; tombstone entry masks base
  - Write path: upsert by logical path; record content hash and mtime
  - Access: will be gated by JWT-based authentication in future
- SSR helpers
  - `render(view, data)` finds `/views/{view}.html` in DB; partial iff htmx
  - `res.hx()` sets HX‑* headers and sensible cache/Vary
- Dev route
  - `/__dev` static app reads file list from DB; save writes overlay; SSE `event: reload` on save

## Later

- Staging → promote flow with integrity checks
- Inspector and request timeline (inspired by Smalltalk browser/inspector)
- Tiny components kit for forms/lists built on htmx patterns
- CLI for packaging and pushing content

---

If this document fully captures the README’s FUTURE, that section can live here going forward to keep the README focused on the Quickstart.
