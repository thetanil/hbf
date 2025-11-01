# Phase 0 & 1: Static Asset Packaging and Live Dev Environment

**Status**: Ready for implementation
**Prerequisites**: Current codebase builds and runs (✅ verified)

## Current State (Baseline Established)

### Architecture Understanding
- **Build-time**: `fs.db` is generated from `fs/` directory and embedded as C byte array (`fs_embedded.c`)
- **Runtime**:
  - On first run (or `--inmem`), main DB (`hbf.db` or `:memory:`) is hydrated from embedded `fs.db`
  - All asset reads use main DB's `sqlar` table via `hbf_db_read_file_from_main()`
  - Main DB is **writable** - this is the key for dev mode overlay
  - Static files served from `/static/**` via CivetWeb handler (`internal/http/server.c:50`)
  - All other routes handled by QuickJS loading `hbf/server.js` from main DB

### Current Files in Archive (2 total)
```
hbf/server.js         ✅ Packaged
static/index.html     ✅ Packaged
static/style.css      ❌ Missing (404 error at runtime)
static/favicon.ico    ❌ Missing
static/assets/        ❌ Empty directory (excluded by Bazel)
```

### Current Build System Issues
**Problem**: `fs/BUILD.bazel` lines 15-17 hardcode only 2 files:
```bash
cp $(location hbf/server.js) hbf/server.js
cp $(location static/index.html) static/index.html
sqlite3 -A -cf $(location fs.db) hbf/server.js static/index.html
```

**User Requirement**: Dropping a file in `fs/static/` or `fs/hbf/` should automatically include it (no manual Bazel edits).

**Root Cause**: genrule `srcs` uses glob but `cmd` hardcodes specific files.

### Dev Mode Flag Available
- ✅ `--dev` flag exists in `config.h`/`config.c`
- ✅ Parsed and logged in `main.c:36`
- ❌ **Not passed to server** - `hbf_server_create()` only takes `port` and `db`
- ❌ Not accessible to QuickJS (no env shim or header injection yet)

## Phase 0: Fix Static Asset Packaging

**Goal**: Make `fs.db` include all files in `fs/` automatically using Bazel glob.

### Tasks

#### 0.1 Fix `fs/BUILD.bazel` genrule to auto-include all files

**Current Code** (lines 13-19):
```starlark
cmd = """
    mkdir -p hbf static
    cp $(location hbf/server.js) hbf/server.js
    cp $(location static/index.html) static/index.html
    sqlite3 -A -cf $(location fs.db) hbf/server.js static/index.html
    rm -rf hbf static
""",
```

**New Code**:
```starlark
cmd = """
    # Create staging directory structure
    mkdir -p staging

    # Copy all globbed sources preserving directory structure
    for src in $(SRCS); do
        # Get relative path from fs/ directory
        rel_path=$${src#fs/}
        # Create parent directory
        mkdir -p staging/$$(dirname $$rel_path)
        # Copy file
        cp $$src staging/$$rel_path
    done

    # Create SQLAR archive from staging directory
    cd staging && sqlite3 -A -cf ../$(location fs.db) hbf static

    # Cleanup
    cd .. && rm -rf staging
""",
```

**Why this works**:
- `$(SRCS)` expands to all files matched by glob (Bazel provides full paths)
- Preserves directory structure by creating parent dirs
- `sqlite3 -A` archives entire `hbf/` and `static/` directory trees
- No hardcoded filenames - fully automatic

#### 0.2 Update `fs_build_test.sh` to verify all expected files

Add checks after line 37:
```bash
# Test 5: Verify style.css exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/style.css'" | grep -q 1; then
    echo "FAIL: static/style.css not found in archive"
    exit 1
fi

# Test 6: Verify favicon.ico exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/favicon.ico'" | grep -q 1; then
    echo "FAIL: static/favicon.ico not found in archive"
    exit 1
fi
```

#### 0.3 Add missing MIME types to `internal/http/server.c`

Add to `get_mime_type()` after line 44:
```c
if (strcmp(ext, ".woff") == 0) {
    return "font/woff";
}
if (strcmp(ext, ".woff2") == 0) {
    return "font/woff2";
}
if (strcmp(ext, ".wasm") == 0) {
    return "application/wasm";
}
if (strcmp(ext, ".md") == 0) {
    return "text/markdown";
}
```

**Why**: Monaco Editor will need font files; WASM may be used later; markdown for docs.

#### 0.4 Conditional cache headers for dev mode

**Requirement**: Pass `config.dev` to server so it can set `Cache-Control: no-store` in dev mode.

**Changes**:

1. Update `internal/http/server.h` line 11-15:
```c
typedef struct hbf_server {
    int port;
    int dev;           /* Development mode flag */
    sqlite3 *db;       /* Main database (contains SQLAR) */
    struct mg_context *ctx;
} hbf_server_t;
```

2. Update `hbf_server_create()` signature in `server.h` line 24:
```c
hbf_server_t *hbf_server_create(int port, int dev, sqlite3 *db);
```

3. Update `internal/http/server.c` line 124-143:
```c
hbf_server_t *hbf_server_create(int port, int dev, sqlite3 *db)
{
    hbf_server_t *server;

    if (!db) {
        hbf_log_error("NULL database handle");
        return NULL;
    }

    server = calloc(1, sizeof(hbf_server_t));
    if (!server) {
        hbf_log_error("Failed to allocate server");
        return NULL;
    }

    server->port = port;
    server->dev = dev;  /* Store dev flag */
    server->db = db;
    server->ctx = NULL;

    return server;
}
```

4. Update `static_handler()` in `server.c` line 90-97 (cache header logic):
```c
/* Set cache headers based on dev mode */
const char *cache_header = server->dev
    ? "Cache-Control: no-store\r\n"
    : "Cache-Control: public, max-age=3600\r\n";

/* Send response */
mg_printf(conn,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: %s\r\n"
          "Content-Length: %zu\r\n"
          "%s"
          "Connection: close\r\n"
          "\r\n",
          mime_type, size, cache_header);
```

5. Update `main.c` line 55:
```c
server = hbf_server_create(config.port, config.dev, db);
```

### Acceptance Criteria for Phase 0

```bash
# Build succeeds
bazel build //:hbf

# Tests pass
bazel test //fs:fs_build_test --test_output=all
# Should show: "PASS: Archive contains 4 files" (server.js, index.html, style.css, favicon.ico)

# Verify archive contents
sqlite3 bazel-bin/fs/fs.db "SELECT name FROM sqlar ORDER BY name"
# Should output:
#   hbf/server.js
#   static/favicon.ico
#   static/index.html
#   static/style.css

# Runtime test
rm -f hbf.db && bazel-bin/hbf --inmem &
sleep 1
curl -s http://localhost:5309/static/style.css  # Should return CSS content, not 404
curl -I http://localhost:5309/static/index.html | grep "Cache-Control"  # Should be "public, max-age=3600"
pkill hbf

# Dev mode test
bazel-bin/hbf --inmem --dev &
sleep 1
curl -I http://localhost:5309/static/index.html | grep "Cache-Control"  # Should be "no-store"
pkill hbf
```

---

## Phase 1: Add HTMX and Monaco Editor Assets

**Goal**: Fetch HTMX and Monaco via Bazel `http_archive`, stage them into `fs.db` at build time.

### 1.1 Add `http_archive` to MODULE.bazel

Add after line 48 (after quickjs-ng):
```starlark
# HTTP archive support for fetching third-party web assets
http_archive = use_repo_rule("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# HTMX - Hypermedia-driven web framework
# Version: 2.0.3 (latest stable as of implementation)
http_archive(
    name = "htmx",
    url = "https://unpkg.com/htmx.org@2.0.3/dist/htmx.min.js",
    sha256 = "TBD",  # Run: curl -sL <url> | sha256sum
    downloaded_file_path = "htmx.min.js",
    build_file_content = """
filegroup(
    name = "htmx_min",
    srcs = ["htmx.min.js"],
    visibility = ["//visibility:public"],
)
    """,
)

# Monaco Editor - VS Code's text editor
# Version: 0.52.0 (latest stable)
# Note: We only fetch the pre-built 'min' distribution (~5 MB compressed)
http_archive(
    name = "monaco_editor",
    url = "https://registry.npmjs.org/monaco-editor/-/monaco-editor-0.52.0.tgz",
    sha256 = "TBD",  # Run: curl -sL <url> | sha256sum
    strip_prefix = "package",
    build_file_content = """
filegroup(
    name = "min_vs",
    srcs = glob(["min/vs/**/*"]),
    visibility = ["//visibility:public"],
)
    """,
)
```

**Why npm tarball for Monaco**:
- GitHub releases don't include pre-built `min/` directory
- npm tarball has `package/min/vs/**` already minified and ready
- `strip_prefix = "package"` normalizes paths

**TODO before implementation**:
1. Download both URLs and compute actual `sha256` hashes
2. Verify tarball structure matches `strip_prefix`

### 1.2 Update `fs/BUILD.bazel` to stage external assets

Replace the entire `fs_archive` genrule (lines 4-20) with:
```starlark
genrule(
    name = "fs_archive",
    srcs = glob(
        ["hbf/**/*", "static/**/*"],
        exclude = ["BUILD.bazel"],
        exclude_directories = 1,
    ) + [
        "@htmx//:htmx_min",
        "@monaco_editor//:min_vs",
    ],
    outs = ["fs.db"],
    cmd = """
        # Create staging directory structure
        mkdir -p staging/hbf staging/static/vendor

        # Copy local fs/ files preserving structure
        for src in $(locations hbf/**/*) $(locations static/**/*); do
            # Skip BUILD.bazel if it somehow got included
            [[ $$src == */BUILD.bazel ]] && continue

            # Determine relative path
            if [[ $$src == */hbf/* ]]; then
                rel_path=$${src#*hbf/}
                mkdir -p staging/hbf/$$(dirname $$rel_path)
                cp $$src staging/hbf/$$rel_path
            elif [[ $$src == */static/* ]]; then
                rel_path=$${src#*static/}
                mkdir -p staging/static/$$(dirname $$rel_path)
                cp $$src staging/static/$$rel_path
            fi
        done

        # Stage HTMX
        mkdir -p staging/static/vendor
        cp $(location @htmx//:htmx_min) staging/static/vendor/htmx.min.js

        # Stage Monaco Editor (preserving min/vs/** structure)
        mkdir -p staging/static/monaco
        for monaco_file in $(locations @monaco_editor//:min_vs); do
            # Extract path after 'min/' (e.g., min/vs/loader.js -> vs/loader.js)
            rel_path=$${monaco_file#*/min/}
            mkdir -p staging/static/monaco/$$(dirname $$rel_path)
            cp $$monaco_file staging/static/monaco/$$rel_path
        done

        # Create SQLAR archive
        cd staging && sqlite3 -A -cf ../$(location fs.db) hbf static

        # Cleanup
        cd .. && rm -rf staging
    """,
)
```

**Why this structure**:
- HTMX served from `/static/vendor/htmx.min.js` (single file)
- Monaco served from `/static/monaco/vs/loader.js` and `/static/monaco/vs/**/*`
- Preserves Monaco's internal directory structure (`vs/base/`, `vs/editor/`, etc.)

### 1.3 Update `fs_build_test.sh` to verify vendor assets

Add after the favicon test:
```bash
# Test 7: Verify HTMX exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/vendor/htmx.min.js'" | grep -q 1; then
    echo "FAIL: static/vendor/htmx.min.js not found in archive"
    exit 1
fi

# Test 8: Verify Monaco loader exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/monaco/vs/loader.js'" | grep -q 1; then
    echo "FAIL: static/monaco/vs/loader.js not found in archive"
    exit 1
fi

# Test 9: Verify Monaco has multiple files (should be 100+)
MONACO_COUNT=$(sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar WHERE name LIKE 'static/monaco/vs/%'")
if [ "$MONACO_COUNT" -lt 50 ]; then
    echo "FAIL: Monaco incomplete ($MONACO_COUNT files, expected 50+)"
    exit 1
fi

echo "PASS: Archive contains $FILE_COUNT files (Monaco files: $MONACO_COUNT)"
```

### 1.4 Test Monaco and HTMX serving

**Manual verification**:
```bash
bazel build //:hbf
rm -f hbf.db && bazel-bin/hbf --inmem &
sleep 2

# Test HTMX
curl -I http://localhost:5309/static/vendor/htmx.min.js
# Should return: 200 OK, Content-Type: application/javascript

# Test Monaco loader
curl -I http://localhost:5309/static/monaco/vs/loader.js
# Should return: 200 OK, Content-Type: application/javascript

# Test Monaco base module
curl -I http://localhost:5309/static/monaco/vs/base/common/platform.js
# Should return: 200 OK

pkill hbf
```

### Acceptance Criteria for Phase 1

```bash
# Build succeeds with external dependencies
bazel build //:hbf

# Tests pass
bazel test //fs:fs_build_test --test_output=all
# Should show: "PASS: Archive contains 100+ files (Monaco files: 50+)"

# Archive inspection
sqlite3 bazel-bin/fs/fs.db "SELECT name FROM sqlar WHERE name LIKE 'static/vendor/%'"
# Should output: static/vendor/htmx.min.js

sqlite3 bazel-bin/fs/fs.db "SELECT COUNT(*) FROM sqlar WHERE name LIKE 'static/monaco/%'"
# Should output: 50+ (exact count depends on Monaco version)

# Binary size check
ls -lh bazel-bin/hbf
# Expected: ~5-6 MB stripped (was ~1 MB, Monaco adds ~4 MB)

# Runtime serving test
rm -f hbf.db && bazel-bin/hbf --inmem --dev &
sleep 2
curl -s http://localhost:5309/static/vendor/htmx.min.js | head -c 50
# Should start with: (function(){...
curl -s http://localhost:5309/static/monaco/vs/loader.js | head -c 50
# Should start with AMD loader code
pkill hbf
```

---

## Implementation Order

1. **Phase 0.1**: Fix BUILD.bazel glob packaging (30 min)
2. **Phase 0.2**: Update build test (10 min)
3. **Phase 0.3**: Add MIME types (5 min)
4. **Phase 0.4**: Thread dev flag to server (20 min)
5. **Verify Phase 0**: Run all acceptance tests (10 min)
6. **Phase 1.1**: Add http_archive to MODULE.bazel with actual sha256 (20 min)
7. **Phase 1.2**: Update fs/BUILD.bazel for vendor staging (40 min)
8. **Phase 1.3**: Update build test for vendor assets (15 min)
9. **Phase 1.4**: Manual serving tests (15 min)
10. **Verify Phase 1**: Run all acceptance tests (10 min)

**Total estimated time**: ~2.5 hours

---

## Out of Scope (Future Phases)

- Dev overlay tables in SQLite (Phase 2)
- Dev API endpoints (`GET/PUT /__dev/api/file`) (Phase 2)
- Monaco editor UI page (Phase 3)
- HTMX live reload with SSE (Phase 4)
- Monaco Vim mode (optional enhancement)

---

## Key Architectural Clarifications

### Main DB is Writable ✅
- `hbf.db` (or `:memory:`) is opened with default (read-write) mode
- No `SQLITE_OPEN_READONLY` flag used
- **This enables overlay pattern**: Write new layers to main DB at runtime

### No Separate Dev DB ❌
- Only one runtime database: main DB
- Overlay tables will live in main DB alongside `sqlar` (Phase 2)
- Dev mode toggle controls:
  - Which SQL view to query (`sqlar` vs `latest_fs`)
  - Cache headers
  - Dev API endpoint access

### Filesystem Layer Design (Preview for Phase 2)
```sql
-- Base layer (never modified at runtime)
CREATE TABLE sqlar (
    name TEXT PRIMARY KEY,
    mode INT,
    mtime INT,
    sz INT,
    data BLOB
);

-- Overlay layers (written by dev API)
CREATE TABLE fs_layer (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    layer_id INT NOT NULL DEFAULT 1,  -- All dev edits use layer_id=1
    name TEXT NOT NULL,
    op TEXT CHECK(op IN ('add', 'modify', 'delete')),
    mtime INT NOT NULL,
    sz INT NOT NULL,
    data BLOB
);

-- Materialized current state (maintained by triggers)
CREATE TABLE overlay_sqlar (
    name TEXT PRIMARY KEY,
    mtime INT NOT NULL,
    sz INT NOT NULL,
    data BLOB,
    deleted INT DEFAULT 0
);

-- View that merges base + overlay
CREATE VIEW latest_fs AS
SELECT name, mtime, sz, data FROM overlay_sqlar WHERE deleted = 0
UNION ALL
SELECT name, mtime, sz, data FROM sqlar
WHERE name NOT IN (SELECT name FROM overlay_sqlar);
```

**Read path in C** (Phase 2):
```c
const char *table = server->dev ? "latest_fs" : "sqlar";
sql = "SELECT sqlar_uncompress(data, sz) FROM %s WHERE name = ?";
snprintf(query, sizeof(query), sql, table);
```

---

## Dependencies and Risks

### External Dependencies
- ✅ HTMX stable release (15 KB) - Low risk
- ⚠️ Monaco npm tarball (4-5 MB) - Medium risk
  - **Risk**: npm registry availability during build
  - **Mitigation**: Bazel caches downloads; offline builds work after first fetch
  - **Risk**: Monaco version breaking changes
  - **Mitigation**: Pin to exact version via sha256

### Build System Risks
- ⚠️ Complex bash in genrule `cmd`
  - **Risk**: Path manipulation fragile across Bazel sandbox environments
  - **Mitigation**: Test on Linux (devcontainer) and validate with extensive logging
  - **Fallback**: Simplify to Python script if bash becomes unmaintainable

### Binary Size Growth
- Before: ~1 MB (SQLite + CivetWeb + QuickJS)
- After: ~5-6 MB (adds Monaco ~4 MB + HTMX 15 KB)
- **Acceptable**: Still single-file deployment, gzip compression for network transfer

### Performance Impact
- Archive read time: Negligible (SQLAR indexed by name)
- First request: QuickJS creates fresh context (~5 ms overhead)
- Dev mode overhead: One additional `if (server->dev)` check per static request

---

## Success Metrics

**Phase 0 Success**:
- [ ] All existing tests pass
- [ ] `style.css` loads at runtime (no 404)
- [ ] `bazel test //fs:fs_build_test` shows 4+ files in archive
- [ ] Dev mode sets `Cache-Control: no-store`
- [ ] Prod mode sets `Cache-Control: public, max-age=3600`

**Phase 1 Success**:
- [ ] All Phase 0 criteria met
- [ ] HTMX serves from `/static/vendor/htmx.min.js`
- [ ] Monaco loader serves from `/static/monaco/vs/loader.js`
- [ ] Monaco includes 50+ support files (`vs/base/`, `vs/editor/`, etc.)
- [ ] Binary size < 10 MB
- [ ] No build warnings or errors
- [ ] No CDN dependencies (100% self-contained)

---

## Notes for Implementation

### Glob Pattern Gotchas in Bazel
- `glob(["static/**/*"])` matches files recursively but excludes directories
- Empty `static/assets/` won't appear in `$(SRCS)` (by design)
- Build will succeed even with empty directory (creates staging structure, archives what exists)

### SQLAR Archive Format
- `sqlite3 -A` creates `sqlar` table with schema:
  ```sql
  CREATE TABLE sqlar(
      name TEXT PRIMARY KEY,  -- Full path (e.g., "static/vendor/htmx.min.js")
      mode INT,               -- File permissions (33188 = -rw-r--r--)
      mtime INT,              -- Modification time (Unix timestamp)
      sz INT,                 -- Uncompressed size
      data BLOB               -- Compressed content (zlib) OR raw if sz == length(data)
  );
  ```
- Read with: `SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?`
- Already implemented in `internal/db/db.c:251`

### Cache Header Best Practices
- Dev mode `no-store`: Forces browser to always revalidate (essential for live edit UX)
- Prod mode `public, max-age=3600`: 1-hour cache (balance between performance and updates)
- Future enhancement: ETag support using `mtime` from sqlar table

### MIME Type Coverage
Current types supported (after Phase 0.3):
- HTML, CSS, JS, JSON ✅
- PNG, JPEG, SVG, ICO ✅
- WOFF, WOFF2 ✅
- WASM, Markdown ✅
- Fallback: `application/octet-stream` ✅

Missing (can add later if needed):
- `.ttf`, `.otf` (if Monaco needs legacy fonts)
- `.map` (source maps - serve as `application/json`)
- `.xml`, `.txt` (rare in modern web apps)
