# Adding Custom Initial Content to HBF (Bazel-driven schema injection)

This project bakes initial static content and scripts into the database schema at build time. When the application starts for the first time, it initializes the database using that schema, which already contains your content.

This flow is Bazel-native and QuickJS-based. It is not Node.js: npm packages won’t run directly in HBF without bundling/conversion to plain JavaScript that QuickJS can execute.


## How it works

At a high level, the build wires your static files into SQL INSERT statements and merges them into the DB schema:

- The rule `//internal/db:static_content_sql_gen` copies a curated set of static files into a temp directory and runs `tools/inject_content.sh` to convert them into SQL inserts.
- The rule `//internal/db:schema_with_content_gen` concatenates your base `schema.sql` with the generated static content SQL.
- The rule `//internal/db:schema_sql_gen` runs `tools/sql_to_c.sh` to convert the merged SQL into a C array embedded into the binary.
- The `//internal/db:schema` library links that C array so the DB can be initialized with both structure and content on first start.

Key files:
- `internal/db/BUILD.bazel` – contains the genrules that generate and combine SQL.
- `tools/inject_content.sh` – walks a static directory and emits SQL `INSERT INTO nodes(type, body)` rows. It writes JSON into `body` of shape `{ "name": string, "content": string }`, and escapes quotes for SQL.
- `tools/sql_to_c.sh` – converts the merged SQL to a C byte array (`hbf_schema_sql`) to be linked into the binary.
- `static/` – holds the default content:
  - `static/server.js` (script)
  - `static/lib/router.js` (script)
  - `static/www/index.html`, `static/www/style.css` (static assets)

Type mapping in `inject_content.sh`:
- Files ending with `.js` are inserted with `type = 'script'`.
- All other files are inserted with `type = 'static'`.
- Names are normalized as follows:
  - `server.js` → name `"server.js"`
  - `lib/router.js` → name `"router.js"`
  - everything else → name equals the relative path with a leading slash, for example `static/www/index.html` becomes `"/www/index.html"`.


## Add your own content

There are two straightforward ways to include custom content in your builds.

### Option A: Extend the default static set

1) Add your files under the `static/` tree, for example:
- `static/www/about.html`
- `static/lib/util.js`

2) Export them in the appropriate `BUILD.bazel` files so Bazel can reference them:
- In `static/www/BUILD.bazel` add `"about.html"` to `exports_files`.
- In `static/lib/BUILD.bazel` add `"util.js"` to `exports_files`.

3) Update `internal/db/BUILD.bazel` to include your new files in `static_content_sql_gen.srcs`:
```
# internal/db/BUILD.bazel (snippet)
genrule(
    name = "static_content_sql_gen",
    srcs = [
        "//static:server.js",
        "//static/lib:router.js",
        "//static/www:index.html",
        "//static/www:style.css",
        # Add your files below
        "//static/www:about.html",
        "//static/lib:util.js",
    ],
    outs = ["static_content.sql"],
    cmd = """
      TMPDIR=$$(mktemp -d)
      trap "rm -rf $$TMPDIR" EXIT
      mkdir -p $$TMPDIR/static/lib $$TMPDIR/static/www

      # Copy known files
      cp $(location //static:server.js) $$TMPDIR/static/server.js
      cp $(location //static/lib:router.js) $$TMPDIR/static/lib/router.js
      cp $(location //static/www:index.html) $$TMPDIR/static/www/index.html
      cp $(location //static/www:style.css) $$TMPDIR/static/www/style.css

      # Copy your files
      cp $(location //static/www:about.html) $$TMPDIR/static/www/about.html
      cp $(location //static/lib:util.js) $$TMPDIR/static/lib/util.js

      $(location //tools:inject_content) $$TMPDIR/static $(location static_content.sql)
    """,
    tools = ["//tools:inject_content"],
)
```

The specific `cp` lines ensure the temp directory matches the `static/` layout so names in the DB are predictable.

4) Build and test:
```bash
bazel build //internal/db:schema
bazel test //internal/db:schema_test
```

Your content will be included in `schema_with_content.sql` and then in the compiled `schema_sql.c` that the app uses to initialize the DB.


### Option B: Provide a separate static tree

If you want to keep the default static content intact but add a separate bundle, you can:
- Create a new Bazel package with your own `BUILD.bazel` exporting files (e.g., `//my_static/...`).
- Create your own genrule (mirroring `static_content_sql_gen`) that copies your files into a temp `static/` directory and runs `//tools:inject_content` on it.
- Either concatenate your generated SQL with the base `schema.sql` as an additional layer or replace the default `static_content_sql_gen` in your build overlay.

This approach keeps your custom content isolated and makes it easy to swap in different content sets for different builds.


## About JavaScript and QuickJS (no npm at runtime)

HBF does not embed Node.js.
- Scripts are executed by QuickJS.
- You cannot import npm packages at runtime.
- If you need code from npm, bundle it into a single self-contained JS file that does not use Node-specific APIs (e.g., `fs`, `net`). Tools such as esbuild/rollup can produce a browser-compatible or no-`require` bundle, which you then place under `static/` and list in `static_content_sql_gen.srcs`.

Guidelines for JS compatibility:
- Avoid Node globals and built-ins; stick to standard JS.
- Ensure any module format is ESM or IIFE that works under QuickJS.
- Keep bundle sizes reasonable; everything is converted into SQL and then into a C array.


## Verifying what gets injected

You can inspect the generated files during a build:
- `bazel-bin/internal/db/static_content.sql` – SQL inserts produced by your static files.
- `bazel-bin/internal/db/schema_with_content.sql` – base schema plus your content.
- `bazel-bin/internal/db/schema_sql.c` – C array containing the merged SQL.

Optionally, you can run the injection script locally to preview the SQL:
```bash
./tools/inject_content.sh static out.sql
sed -n '1,80p' out.sql
```


## Data model inserted

By default the injector writes into a `nodes` table with two columns:
- `type`: `'script'` for `.js` files, `'static'` for others.
- `body`: JSON string with fields:
  - `name`: a normalized name ("server.js", "router.js", or "/path/under/static").
  - `content`: the file contents.

Example row emitted by the injector (shown as JSON inside the SQL):
```sql
INSERT INTO nodes (type, body)
VALUES (
  'static',
  '{"name":"/www/index.html","content":"<html>..."}'
);
```


## Troubleshooting

- Missing labels in `srcs`: make sure every file you reference is exported via an `exports_files([...])` in its `BUILD.bazel` and added to `static_content_sql_gen.srcs`.
- Path/name mismatch: the injector expects a `static/` directory with subfolders `lib` and `www`. Ensure the temp copy or your own genrule matches that shape.
- Large files: the SQL is turned into a C byte array; very large assets will increase the binary size. Prefer minimal assets or serve larger assets by another channel if needed.
- Special-cased names: only `server.js` and `lib/router.js` are normalized to simple names; everything else is stored with a leading slash path.


## Quick reference

- Add files under `static/`.
- Export them in the nearest `BUILD.bazel` via `exports_files([...])`.
- Add them to `internal/db:static_content_sql_gen.srcs` and update its copy commands.
- Build `//internal/db:schema` to regenerate the merged schema.
- Remember: no npm at runtime. Bundle or rewrite as plain JS compatible with QuickJS.
