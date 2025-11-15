# Hypermedia Implementation: Phase 1 Plan

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
    *   Delete unused scripts from `tools/` like `npm_vendor.sh`, `js_to_c.sh`, `asset_packer.c`.

5.  **Verification:**
    *   After these changes, the project should build successfully with `bazel build //:hbf`.
    *   The resulting binary should start without errors, but will have very limited functionality (e.g., only `/health` might work).

---

## Phase 1: Hypermedia API and Homepage

With the simplified foundation, this phase implements the core hypermedia API endpoints and a basic homepage, as described in `DOCS/hypermedia-fragments.md`.

### Task Breakdown:

1.  **Database Schema Setup:**
    *   Create a new file `hbf/db/hypermedia_schema.sql`.
    *   Add the SQL `CREATE TABLE` statements for `nodes`, `edges`, and `view_type_templates` to this file.
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
