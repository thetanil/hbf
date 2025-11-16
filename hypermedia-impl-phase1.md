# Hypermedia Implementation: Phase 1 Plan (Revised Addendum Pending)

This document outlines the plan to refactor HBF and implement the first phase of the hypermedia system. The primary goal is to remove the JavaScript runtime and pod-based architecture, replacing it with a pure C-based hypermedia API for managing and serving content fragments from a SQLite database.

---

## Phase 0: Core Simplification (Removing JS Runtime)

The goal of this phase is to strip the application down to its C foundation, removing all components related to QuickJS, pod generation, and the overlay filesystem. This provides a clean slate for building the hypermedia features.

### Task Breakdown:

1.  **Delete JS-related Directories and Files:**
    *   Delete `hbf/qjs/` directory.
    *   Delete `pods/` directory.
    *   Delete `third_party/quickjs-ng/` and `third_party/find-my-way/`.

2.  **Update Build System (Bazel):**
    *   **Root `BUILD.bazel`**: Remove the `pod_binary` invocations for `:hbf` and `:hbf_test`. Remove the `:lint` alias that points to JS linting.
    *   **`MODULE.bazel`**: Remove the `bazel_dep` for `quickjs-ng`.
    *   **`hbf/http/BUILD.bazel`**: Remove dependencies on `//hbf/qjs`. The `http_handler` target will be removed.
    *   **`tools/BUILD.bazel`**: Remove targets for `js_to_c.sh`, `npm_vendor.sh`.
    *   **`tools/pod_binary.bzl`**: Delete this file.

3.  **Refactor C Source Code:**
    *   **`hbf/http/handler.c` and `handler.h`**: Delete these files. They are the bridge to the QuickJS runtime.
    *   **`hbf/http/server.c`**:
        *   Remove the include for `handler.h`.
        *   Remove the `handler_mutex`.
        *   In `main_request_handler`, remove the logic that delegates to `hbf_http_handle_request`. This function will become the primary router.
    *   **`hbf/shell/main.c`**:
        *   Remove calls to `hbf_qjs_init` and related configuration.
        *   Remove any logic related to pod selection.
    *   **`hbf/db/db.c`**:
        *   Modify `hbf_db_init` to create a new, empty database file on each startup (e.g., `hbf.db`). The existing logic for WAL mode and settings can be kept.
        *   Remove all functions and logic related to `sqlar` and the overlay filesystem (`overlay_fs_*`).
    *   **`hbf/db/overlay_fs.c` and `overlay_fs.h`**: Delete these files.
    *   **`hbf/http/static_handler.c`**: This file will be removed as static file serving will be handled differently or not at all in phase 1.

4.  **Cleanup Tooling:**
    *   Delete unused scripts from `tools/` like `npm_vendor.sh`, `js_to_c.sh`.
    *   Retain `asset_packer.c` (will be repurposed in Phase 1 to embed core runtime assets—htmx, internal JS/CSS—into the binary for in‑memory serving).

5.  **Verification:**
    *   After these changes, the project should build successfully with `bazel build //:hbf`.
    *   The resulting binary should start without errors, but will have very limited functionality (e.g., only `/health` might work).

---

## Phase 1: Hypermedia API and Homepage

With the simplified foundation, this phase implements the core hypermedia API endpoints and a basic homepage, as described in `DOCS/hypermedia-fragments.md`.

### Task Breakdown:

1.  **Database Schema Setup:**
    *   Create a new file `hbf/db/hypermedia_schema.sql`.
    *   Add the SQL `CREATE TABLE` statements for `nodes`, `edges`, and `view_type_templates` (baseline approach) to this file.
    *   (Alternative later) Allow templates to be represented as nodes with `view_type='template'` and remove dedicated table; see new "Template-as-Node Option" section below.
    *   Modify `hbf/db/db.c` to execute the contents of `hypermedia_schema.sql` immediately after the database connection is established, ensuring the tables exist on every run.

2.  **Implement C-based HTTP Handlers:**
    *   **Routing**: In `hbf/http/server.c`, enhance `main_request_handler` to act as a simple router. It will parse the request URI and method and delegate to new handler functions.
    *   **Create New Handler Files**: Create `hbf/http/api_handlers.c` and `hbf/http/api_handlers.h` to house the logic for the new API.
    *   **Implement API Endpoints** in `api_handlers.c`:
        *   `handle_create_fragment` (`POST /fragment`): Inserts a new row into the `nodes` table.
        *   `handle_get_fragment` (`GET /fragment/:id`): Retrieves a node, renders it using a template, and returns HTML.
        *   `handle_create_template` (`POST /template`): Inserts a new row into the `view_type_templates` table.
        *   `handle_create_edge` (`POST /link`): Inserts a new row into the `edges` table.

3.  **Implement C-based Template Renderer:**
    *   Create `hbf/http/renderer.c` and `hbf/http/renderer.h`.
    *   Implement the `render_template` function as specified in `DOCS/hypermedia-fragments.md`. This function will perform simple key-value substitution.

4.  **Create the Homepage:**
    *   **Homepage Handler**: Create a function `handle_get_homepage` (`GET /`) in `api_handlers.c`.
    *   **Seed Data**: In `hbf/shell/main.c` or `hbf/db/db.c`, after initializing the schema, insert some default data:
        *   A "page" template in `view_type_templates`.
        *   A "link-list" template in `view_type_templates`.
        *   A root "home" node in the `nodes` table.
        *   Several "link" nodes that are children of the "home" node.
    *   **Homepage Logic**: The `handle_get_homepage` function will:
        1.  Fetch the root "home" node.
        2.  Fetch the "page" template.
        3.  Render the page, including an HTMX `hx-get` attribute to lazy-load the list of links.
    *   **Lazy-load Handler**: Create a handler `handle_get_link_list` (`GET /links`) that queries the database for all children of the "home" node, renders them using the "link-list" template, and returns the concatenated HTML.

5.  **Build System Updates:**
    *   Update `hbf/http/BUILD.bazel` to include the new source files (`api_handlers.c`, `renderer.c`).
    *   Update `hbf/db/BUILD.bazel` to embed `hypermedia_schema.sql`.

6.  **Verification:**
    *   Build the project and run the server.
    *   Use `curl` to test the API endpoints for creating and linking fragments.
    *   Open a browser to `http://localhost:PORT/` and verify that a basic page loads.
    *   Verify that the page makes a subsequent HTMX request to `/links` to load the list of content.

---
## Addendum: Revised Minimal Hypermedia Plan

Instead of immediately deleting JavaScript & pod infrastructure, we first stand up a minimal hypermedia server using only C99 + SQLite + htmx, sharing one process‑lifetime `sqlite3*` across requests. Dynamic behaviors (rendering, traversal, linkage) are encoded in SQL (schema, triggers, recursive CTEs) plus a tiny C template substitution function—no embedded scripting runtime.

### Phase 0 (Minimal Core)
Goal: binary opens `hbf.db`, enables WAL, starts CivetWeb, serves `/health` and a homepage `/` containing:
```html
<script src="https://unpkg.com/htmx.org@1.9.10"></script>
<div id="link-list" hx-get="/links" hx-trigger="load"></div>
```
Tasks:
1. Provide `sqlite3* hbf_db_get(void);` accessor.
2. Inline routing in `server.c` for `/`, `/links`, `/fragment/{id}`, `/fragment` (POST).
3. Stub `/links` returns simple `<ul><li>` list html.
4. Keep QuickJS/pod code dormant (deletion deferred).

### Phase 1 (Graph + Templates)
Add `hypermedia_schema.sql` (tables: `view_type_templates`, `nodes`, `edges`; optional later `closure`). Execute after DB open. Implement:
* `renderer.c` – `{{key}}` substitution.
* `api_handlers.c` – CRUD endpoints for fragments/templates/links.
* Seed templates (`page`, `link-list`, `link`) + root `home` node + child link nodes.
* HATEOAS: each HTML response includes anchors/forms describing next possible transitions.

### Phase 1 Asset Embedding & Core Routes
Reuse `asset_packer.c` pipeline to bundle selected third-party or internal assets (initially htmx minified JS) into a compressed blob compiled into the binary. Source for htmx comes from Bazel `http_file` repo `@htmx` (label usually `@htmx//:htmx.min.js`); configure asset packer inputs to consume that file directly (no CDN). At startup:
1. Decompress blob into memory (single allocation) and index files by virtual path.
2. Register CivetWeb handlers for core asset routes under `/_hbf/` prefix (e.g., `/_hbf/htmx/htmx_2.0.3.min.js`).
3. Serve assets directly from memory (no filesystem or DB lookup) with appropriate `Content-Type` and cache headers (`Cache-Control: public, max-age=3600`).
4. Homepage references htmx via the in‑memory route instead of external CDN once stable.

Benefits: deterministic build, fully offline, leverages Bazel sha256 verification, keeps binary self-contained, aligns with future embedding of additional core UI libraries.

### Dynamic SQL Execution
* Recursive CTE for later `/assemble/{id}`.
* Optional triggers for closure maintenance & versioning.
* Potential `html_cache` column for rendered fragment memoization.

### Verification
```bash
bazel build //:hbf
bazel run //:hbf -- --port 5309
curl http://localhost:5309/health
curl http://localhost:5309/
curl http://localhost:5309/links
curl -X POST -d 'view_type=card&content=Hello' http://localhost:5309/fragment
curl http://localhost:5309/fragment/1
```

### Future
* Recursive assembly endpoint `/assemble/{id}`.
* Markdown parsing (small C lib) for fragment content.
* Closure table + triggers.
* Immutable versioning & content addressability.
* Dev UI (CodeMirror) and removal of unused QuickJS/pod assets.

End Addendum.
