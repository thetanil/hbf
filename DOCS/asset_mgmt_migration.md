# Asset Management Migration Guide (Main Branch)

This guide explains how to migrate HBF’s asset pipeline on the main branch from the legacy SQLAR-based embedding to a C-only, deterministic pack-and-migrate flow using the existing zlib dependency and a small C tool (`asset_packer`). It preserves the existing HTTP stack (CivetWeb) and QuickJS runtime while removing SQLAR and pod macros from the build.

The migration has four parts:
- Use zlib (already a Bazel dependency)
- Introduce the `asset_packer` tool (Bazel target)
- Wire a C-only startup migration into SQLite (no JS at boot)
- Remove SQLAR (code and build references)

---

## 1) Use zlib (already present)

zlib is already declared via Bazel Central Registry in `MODULE.bazel` and linked into existing targets.

- Confirmation in `MODULE.bazel`:

```starlark
# zlib compression library (from Bazel Central Registry)
bazel_dep(name = "zlib", version = "1.3.1.bcr.7")
```

No additional setup is required.

---

## 2) Asset packer tool

Create a small C tool that:
- Reads input files, sorts lexicographically
- Packs into a deterministic binary format
- Compresses with zlib
- Emits a C source/header pair with `assets_blob[]` and `assets_blob_len`

Binary format (uncompressed buffer):
```
[num_entries:u32]
repeat num_entries times:
  [name_len:u32]
  [name:bytes]
  [data_len:u32]
  [data:bytes]
```

Add a Bazel target in `tools/BUILD.bazel`:

```starlark
cc_binary(
  name = "asset_packer",
  srcs = ["asset_packer.c"],
  deps = ["@zlib"],
    copts = [
        "-std=c99",
        "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-Wconversion",
        "-Wdouble-promotion", "-Wshadow", "-Wformat=2", "-Wstrict-prototypes",
        "-Wold-style-definition", "-Wmissing-prototypes", "-Wmissing-declarations",
        "-Wcast-qual", "-Wwrite-strings", "-Wcast-align", "-Wundef",
        "-Wswitch-enum", "-Wswitch-default", "-Wbad-function-cast",
    ],
    visibility = ["//visibility:public"],
)
```

Usage:

```bash
bazel build //tools:asset_packer
bazel-bin/tools/asset_packer \
  --output-source assets_blob.c \
  --output-header assets_blob.h \
  --symbol-name assets_blob \
  pods/base/hbf/server.js pods/base/static/style.css
```

This creates C artifacts you can compile and link into the binary. For CI hermeticity, wrap in a genrule/macro that globs the pod’s assets and feeds them to `asset_packer`.

Optional deterministic test (good to keep):

```starlark
sh_test(
    name = "asset_packer_deterministic_test",
    srcs = ["asset_packer_deterministic_test.sh"],
    data = [":asset_packer"],
    args = ["$(location :asset_packer)"],
    tags = ["local"],
)
```

The script runs the tool twice and ensures identical output hashes.

---

## 3) C-only startup migration into SQLite

At process start, before HTTP serves requests:
1) Open SQLite (on-disk file or `:memory:`)
2) Ensure schema exists (overlay tables + migrations table)
3) Call `overlay_fs_migrate_assets(db, assets_blob, assets_blob_len)`
4) Continue normal boot (CivetWeb + QuickJS)

Recommended API (header):

```c
#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MIGRATE_OK = 0,
    MIGRATE_ERR_DECOMPRESS = -1,
    MIGRATE_ERR_DB = -2,
    MIGRATE_ERR_ALREADY_APPLIED = -3,
    MIGRATE_ERR_CORRUPT = -4,
} migrate_status_t;

migrate_status_t overlay_fs_migrate_assets(
    sqlite3 *db,
    const uint8_t *bundle_blob,
    size_t bundle_len
);
```

Implementation outline:
- Compute `bundle_id = SHA256(bundle_blob)` (hash compressed bytes)
- Idempotency: `SELECT 1 FROM migrations WHERE bundle_id = ? LIMIT 1` ⇒ return `MIGRATE_ERR_ALREADY_APPLIED`
- Decompress with `uncompress()` (zlib)
- Parse buffer strictly:
  - read `num_entries:u32`
  - for each entry: bounds-check `name_len`/`data_len`, then insert
- Transactional write:
  - `BEGIN IMMEDIATE`
  - `INSERT OR REPLACE INTO overlay (path, content, mtime) VALUES (?, ?, strftime('%s','now'))`
  - `INSERT INTO migrations (bundle_id, applied_at, entries) VALUES (?, strftime('%s','now'), num_entries)`
  - `COMMIT`; on error `ROLLBACK`

Schema notes (overlay):
- Keep your existing overlay tables and `latest_files` view
- Add `migrations(bundle_id TEXT PRIMARY KEY, applied_at INTEGER, entries INTEGER)`

Where to call:
- In `hbf/shell/main.c` after DB init and schema creation, before starting CivetWeb

---

## 4) Remove SQLAR

Remove build-time and runtime SQLAR usage:

Build system:
- Delete or stop invoking `db_to_c.sh`, `sql_to_c.sh` and any SQLAR-specific genrules/macros
- Remove SQLAR third_party registration from `MODULE.bazel`
- Remove pod macros that build SQLAR, and replace with the asset-packer macro/genrule

Runtime code:
- Remove `sqlite3_sqlar_init` references
- Remove `overlay_fs_migrate_sqlar` (keep the name as a stub if API stability is needed, returning a clear error)
- Replace embedded DB symbols (`fs_db_data`, `fs_db_len`) with `assets_blob`, `assets_blob_len`
- Ensure all reads use `overlay_fs_read_file()` against SQLite; no direct FS reads

Tests:
- Replace SQLAR migration tests with asset migration tests:
  - Idempotency (second run returns `MIGRATE_ERR_ALREADY_APPLIED`)
  - Corrupt bundle handling (no partial writes)
  - Static and dynamic routes load from DB after migration

---

## 5) Bazel wiring patterns

Minimal wiring to compile the generated assets:

```starlark
cc_library(
    name = "embedded_assets",
    srcs = ["assets_blob.c"],
    hdrs = ["assets_blob.h"],
    visibility = ["//visibility:public"],
)
```

Link the above into your main binary (or a pod-specific binary) and reference `assets_blob` at boot.

For hermetic packing inside Bazel, prefer a macro that:
- Globs `pods/<name>/hbf/**/*.js` and `pods/<name>/static/**`
- Sorts input
- Runs `asset_packer` to produce `assets_blob.c/.h` under `bazel-out`
- Exposes a `cc_library` for the resulting sources

---

## 6) Commands and expected outcomes

Build the packer:

```bash
bazel build //tools:asset_packer
```

Run deterministic test (if present):

```bash
bazel test //tools:asset_packer_deterministic_test
```

Generate assets for a pod:

```bash
bazel-bin/tools/asset_packer \
  --output-source bazel-out/assets_blob.c \
  --output-header bazel-out/assets_blob.h \
  --symbol-name assets_blob \
  pods/base/hbf/server.js pods/base/static/style.css
```

Build and run project (example):

```bash
bazel build //:hbf
bazel run //:hbf -- --port 5309 --log_level debug
```

Expected behavior:
- On first boot, migration writes assets into SQLite and records the bundle
- Subsequent boots skip migration (idempotent)
- `/static/**` served from DB via overlay
- QuickJS imports resolve from `latest_files` (unchanged)

---

## 7) Notes and gotchas

- Keep bundle parsing strict: reject truncated names/data; avoid integer overflows
- Asset paths are stored as provided; normalize consistently during packing to avoid duplicates
- For very large bundles, consider a streaming packer; current approach packs into RAM once
- Ensure `assets_placeholder.c` (if used) decompresses to `num_entries=0` (4 zero bytes)
- Licenses: zlib is permissive; maintain SPDX headers in all sources

---

## 8) Checklist

- [ ] Use existing zlib dependency (already present)
- [ ] Add `tools:asset_packer` and build it
- [ ] Implement `overlay_fs_migrate_assets` and call it at boot
- [ ] Remove SQLAR references from build and code
- [ ] Add deterministic test and migration tests
- [ ] Verify zero warnings (`-Wall -Wextra -Werror`) and `bazel test //...` passes
