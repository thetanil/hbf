# JavaScript module support plan (server + browser)

This plan enables first-class ES Modules across HBF: from pod creation, through SQLAR packaging, to serving and consumption by both the server (QuickJS) and the browser. It relies on standards-based ES modules and import maps, with vendored npm packages committed to source control.

## Goals

- Server JS can statically import local modules, e.g. `import { name, draw } from "./modules/square.js"`.
- Browser code can import bare packages, e.g. `import { EditorView, keymap } from "@codemirror/view"`.
- A user can specify an npm package; we vendor a reproducible build into the repo and include it in pod SQLAR.
- Static HTTP server serves modules with correct MIME and cache headers; works with modern browsers without extra tools.

## Summary of the approach

- Use ES Modules everywhere; rely on MDN guidance for imports/exports and import maps.
- Vendor npm packages into source control and pre-bundle to browser-ready ESM to avoid Node resolution in browsers.
- Generate an `importmap.json` per pod to resolve bare specifiers to vendored URLs.
- Keep server-side module imports relative; add a QuickJS module loader that resolves `./` and `/` paths to the SQLAR-backed virtual FS.
- Package everything into SQLAR under `hbf/` and `static/` as we do today; static files are then served by `hbf/http/server.c`.

## Browser module support

- Import maps are required for bare specifiers in browsers (MDN: “Importing modules using import maps”). We will:
  - Generate `pods/<pod>/static/importmap.json` mapping package names (and optional prefixes) to vendored URLs.
  - In HTML, include:
    - `<script type="importmap" src="/static/importmap.json"></script>`
    - Then load app entry with `<script type="module" src="/static/app.js"></script>` (or inline module script).
- Vendored modules are served under `/static/vendor/...`:
  - For simple, self-contained libraries: copy minified ESM provided by the package.
  - For complex packages (e.g., CodeMirror, lodash-es, three, etc.): pre-bundle to a single ESM file with esbuild to avoid Node “exports” resolution and deep dependency trees in the browser.
  - Example import map entries:
```json
{
  "imports": {
    "@codemirror/view": "/static/vendor/esm/codemirror-view.js"
  }
}
```
- MIME types: ensure `.js` and `.mjs` are served with a JavaScript MIME type (MDN recommends `text/javascript`). Our server currently returns `application/javascript` for `.js`; we will add `.mjs` and source maps (`.map` as `application/json`).

## Server (QuickJS) module support

- Switch the server entrypoint to be evaluated as an ES module so static `import` works.
  - Change QuickJS evaluation from `JS_EVAL_TYPE_GLOBAL` to `JS_EVAL_TYPE_MODULE` for the entry file.
  - Install a module loader with `JS_SetModuleLoaderFunc` that resolves module specifiers to bytes read via `overlay_fs_read_file()`:
    - Relative imports `./foo.js` and `../bar.js`: resolve relative to the importing module’s URL (e.g., base `hbf/`), then read the file from the SQLAR-backed FS.
    - Absolute-like `/static/...` can be supported by mapping to `static/...` inside SQLAR.
    - Bare specifiers will not be supported on the server initially (keep server imports relative, per requirement). We can add an import-map-like resolver later if needed.
- Module-global vs global object:
  - ES modules run in strict mode and don’t implicitly create globals. Server code should explicitly attach to `globalThis` (e.g., `globalThis.app = { handle(req,res){...} }`) so the C handler can access it. We will update docs and example pod to use `globalThis.app`.
- File layout for server modules:
  - `pods/<pod>/hbf/server.js` (or `server.mjs`) is the entry module.
  - Additional server modules live under `pods/<pod>/hbf/modules/...` and are imported with `./modules/...`.

## Vendoring npm packages (reproducible, committed)

We’ll add a simple tooling pipeline that fetches and builds npm packages into vendored ESM artifacts committed to the repo.

- New tool: `tools/npm_vendor.sh` (and optional `tools/npm_vendor.mjs` wrapper)
  - Inputs: package spec(s) like `@codemirror/view@6.25.0` or `lodash-es@4.17.21`.
  - Steps:
    1. Create a temp working dir; run `npm pack <spec>` to download the tarball without creating a node_modules tree.
    2. Extract tarball into `third_party/npm/<name>@<version>/pkg/`.
    3. For browser consumption, run esbuild to produce a single-file ESM bundle:
       - Entry: package’s ESM entry (“module” field in package.json, falling back to `exports` or default index).
       - Format: `esm` (no minify in dev; minify in release), sourcemap alongside file.
       - External: none (bundle deps by default for simpler import maps). Optionally allow `--external:` flags to control bundling.
       - Output: `pods/<pod>/static/vendor/esm/<safe-name>.js` and `.js.map`.
    4. Generate/augment `pods/<pod>/static/importmap.json` to map the bare specifier to the vendored file.
    5. Copy LICENSE and NOTICE from the package into `third_party/npm/<name>@<version>/` and keep metadata for compliance.
  - The script is idempotent and safe to run for multiple pods by passing `--pod <base|test|...>`.
  - All produced files are checked into git to ensure offline, reproducible builds.

- For packages already shipping browser-ready ESM (e.g., `htmx`, some `lodash-es` builds), we can skip esbuild and map directly to published ESM; the tool can detect this and copy the file.

- Optional: a small manifest `pods/<pod>/static/vendor/vendor.json` listing vendored packages for auditability.

## Pod build integration (SQLAR)

- Our existing `genrule` already globs `static/**/*` and `hbf/**/*` and packs them into SQLAR.
- By placing vendored outputs under `pods/<pod>/static/vendor/...` and `pods/<pod>/static/importmap.json`, they’re automatically included in the pod DB; no Bazel rule changes required.
- We will update the example pods’ `index.html` under `static/dev/` to demonstrate import maps and ESM entry.

## HTTP static serving

- Update `hbf/http/server.c` MIME mapping:
  - Add `.mjs` -> `text/javascript` (or continue using `application/javascript`).
  - Add `.map` -> `application/json`.
  - Keep `.js` as-is. MDN notes that the MIME must be a JavaScript type; both `text/javascript` and `application/javascript` work in practice.
- Caching: keep `no-store` in dev and `public, max-age=3600` in prod as implemented.

## Developer flow (DX)

1. Vendor a package for a pod:
   - Run `tools/npm_vendor.sh --pod base @codemirror/view@6.25.0`.
   - This creates/updates:
     - `third_party/npm/@codemirror__view@6.25.0/**` (source + LICENSE)
     - `pods/base/static/vendor/esm/codemirror-view.js` (+ sourcemap)
     - `pods/base/static/importmap.json` (maps "@codemirror/view" to the ESM file)
   - Commit changes.
2. Reference it in browser code:
   - In `pods/base/static/dev/index.html` (or your app HTML):
```html
<script type="importmap" src="/static/importmap.json"></script>
<script type="module">
  import {EditorView, keymap} from "@codemirror/view";
  // ...use it...
</script>
```
3. Use modules in server code:
   - In `pods/base/hbf/server.js` (module-friendly):
```js
import { name, draw } from "./modules/square.js";

globalThis.app = {
  handle(req, res) {
    // use imported code
    const s = draw(/*...*/);
    // respond
  }
};
```
4. Build pod and run integration tests as usual (Bazel targets already pack `static/` and `hbf/`).

## Tests and validation

Add the following low-risk tests to ensure the pipeline works end to end:

- Static server MIME tests (C unit/integration):
  - Request `/static/vendor/esm/codemirror-view.js` -> status 200, `Content-Type` is JavaScript MIME.
  - Request `/static/vendor/esm/codemirror-view.js.map` -> `Content-Type: application/json`.
  - Request `/static/index.html` -> `text/html`.

- SQLAR packaging test (shell/Bazel):
  - After building `:pod_db`, query the DB to confirm presence of `static/importmap.json` and the vendored ESM file in `latest_files`.

- QuickJS module import test (C or JS-driven):
  - Provide a small `hbf/modules/square.js` and import it from `hbf/server.js`.
  - Run a request hitting code that uses the imported symbol; assert 200.

- Browser smoke test (optional):
  - Serve `static/dev/index.html` that imports `@codemirror/view` via the import map and asserts basic usage (can be a tiny headless fetch + simple HTML check or a CI doc link for manual validation).

## Edge cases and notes

- Packages using CommonJS only: must be bundled with esbuild with `format=esm`. Many modern libs ship ESM; prefer ESM variants (e.g., `lodash-es`).
- Deep dependency trees: bundle into a single ESM to avoid maintaining huge import maps. For very large libs, consider chunk splitting later.
- Versioning: import map can support multiple versions using `scopes` if we need coexisting versions on a page.
- File extensions: prefer `.js` for portability (MDN); support `.mjs` if authors want the clarity—ensure MIME is correct.
- Source maps: include `.map` alongside ESM for DX; not required in production.
- Licensing: store package LICENSE files in `third_party/npm/...` and maintain a `THIRD_PARTY_NOTICES.md` entry.

## Incremental implementation steps

1. Add MIME entries in `hbf/http/server.c` for `.mjs` and `.map`.
2. Implement QuickJS module loader + switch entry eval to `JS_EVAL_TYPE_MODULE`.
   - Keep backward compatibility: if `hbf/server.js` assignment to `app` fails under strict mode, document and update example to use `globalThis.app`.
3. Create `tools/npm_vendor.sh` to pack and bundle npm packages into `pods/<pod>/static/vendor/esm/` and generate `importmap.json`.
4. Update example pod HTML (`pods/*/static/dev/index.html`) to include an import map and a small ESM snippet; add a minimal `hbf/modules/square.js` demo for server.
5. Add the tests listed above.
6. Document the workflow in `DOCS/custom_content.md` (or a new `DOCS/js_modules.md`).

## Acceptance criteria

- Server: `import` from `hbf/server.js` to `./modules/...` works, request path exercises imported code, responses are correct.
- Browser: Importing `@codemirror/view` via `<script type="importmap">` and ESM works on a modern browser, without CDNs.
- Vendoring: Running the vendor tool produces committed artifacts and a working import map; pod build includes them; HTTP server serves them with correct MIME.

## References

- MDN JavaScript modules guide: import/export, import maps, MIME requirements, dynamic `import()` and module semantics.
