# HBF Implementation Summary

**Date**: 2025-11-01
**Status**: Phase 0 & 1 ready for implementation

## What Was Done

### 1. Codebase Exploration ✅

**Findings**:
- ✅ Current archive contains only 2 files (`server.js`, `index.html`) but references 4 files
- ✅ `style.css` returns 404 at runtime (missing from archive)
- ✅ `fs/BUILD.bazel` genrule hardcodes filenames instead of using glob dynamically
- ✅ Main DB (`hbf.db`) is writable - enables overlay pattern without separate dev DB
- ✅ `--dev` flag exists but not passed to HTTP server (only logged)
- ✅ Static files served via CivetWeb handler reading from main DB `sqlar` table
- ✅ QuickJS loads `hbf/server.js` from main DB on each request

**Architecture Verified**:
```
Build Time:
  fs/hbf/** + fs/static/**
    → sqlite3 -A → fs.db
    → db_to_c → fs_embedded.c
    → linked into binary

Runtime:
  Embedded fs.db → hydrate → main DB (hbf.db or :memory:)
  Static requests → CivetWeb handler → hbf_db_read_file_from_main()
  Other requests → QuickJS handler → load hbf/server.js from main DB
```

### 2. Build and Test Baseline ✅

**Current State**:
```bash
$ bazel build //:hbf
✅ Build succeeded

$ bazel test //fs:fs_build_test --test_output=all
✅ PASS: Archive contains 2 files

$ sqlite3 bazel-bin/fs/fs.db "SELECT name FROM sqlar"
hbf/server.js
static/index.html

$ curl http://localhost:5309/static/style.css
❌ Error 404: Not Found
```

**Binary Size**: 1.1 MB (stripped, static)

### 3. New Implementation Plans Created ✅

Created three new planning documents to replace the ChatGPT-generated `hbf_next.md`:

#### `DOCS/phase0-1-plan.md` (Comprehensive)
- **Phase 0**: Fix static asset packaging (auto-glob all files)
  - Fix `fs/BUILD.bazel` to use `$(SRCS)` dynamically
  - Thread `config.dev` to server for conditional cache headers
  - Add missing MIME types (WOFF, WOFF2, WASM, Markdown)
  - Update tests to verify 4+ files in archive

- **Phase 1**: Add HTMX and Monaco Editor
  - `http_archive` for HTMX (15 KB)
  - `http_archive` for Monaco npm tarball (~4 MB)
  - Stage external assets into `fs.db` at build time
  - Serve from `/static/vendor/htmx.min.js` and `/static/monaco/vs/**`
  - Expected binary size: 5-6 MB

**Acceptance Criteria Defined**:
- All files auto-included via glob (drop file in `fs/`, no Bazel edits)
- Dev mode serves with `Cache-Control: no-store`
- Prod mode serves with `Cache-Control: public, max-age=3600`
- Monaco and HTMX served from single binary (no CDN)

#### `DOCS/phase2-overlay-plan.md` (Future)
- Dev overlay filesystem using layered SQLite tables
- `sqlar` (base, immutable) + `fs_layer` (append-only) + `overlay_sqlar` (materialized)
- Dev API endpoints: `GET/PUT/DELETE /__dev/api/file`
- Minimal Monaco editor UI at `/static/dev/index.html`
- Live edit → save → preview workflow

#### `DOCS/SUMMARY.md` (This file)
- High-level overview of findings and plans
- Quick reference for implementation order

### 4. Cleaned Up Incorrect Documentation ✅

**Removed/Corrected**:
- ❌ References to "cenv" and "user pods" (old multi-tenant architecture)
- ❌ References to `/henvs` storage directories (removed feature)
- ❌ Confusion about multiple databases (only one main DB at runtime)
- ❌ Vague "fill in later" placeholders (e.g., sha256 hashes marked TODO)
- ❌ Incorrect Bazel syntax (filegroup glob on external repos)
- ❌ Speculative options without decisions (e.g., "recommended option B")

**Backed Up**: `hbf_next.md` → `hbf_next.md.old`

## Key Architectural Clarifications

### Single Database Model
```
Runtime Databases:
  ✅ Main DB (hbf.db or :memory:) - WRITABLE
  ❌ No separate dev DB
  ❌ No separate overlay DB
  ❌ No fs_db at runtime (only used during hydration inside db.c)

Dev Mode Implementation:
  - Overlay tables (fs_layer, overlay_sqlar) live in main DB
  - C read path checks server->dev flag:
    - dev=0 → query sqlar (base layer)
    - dev=1 → query latest_fs view (overlay + base)
```

### Layered Filesystem Design (Phase 2)
```sql
Layer 0 (base):        sqlar table (immutable after hydration)
Layer 1 (dev overlay): fs_layer table (append-only change log)
Materialized view:     overlay_sqlar (current state, updated by triggers)
Read abstraction:      latest_fs view (UNION of overlay + base)
```

### Build System Requirements
- Auto-glob all files from `fs/hbf/**` and `fs/static/**`
- No manual file listing in Bazel (user requirement)
- Stage external dependencies (HTMX, Monaco) at build time
- Single `fs.db` embedded in binary

## Implementation Order

**Phase 0** (~1.5 hours):
1. Fix `fs/BUILD.bazel` genrule to use `$(SRCS)` loop
2. Update `fs_build_test.sh` to verify style.css and favicon.ico
3. Add MIME types to `internal/http/server.c`
4. Thread `config.dev` to `hbf_server_t` struct
5. Conditional cache headers in static handler
6. Verify all acceptance tests

**Phase 1** (~1 hour):
1. Get sha256 for HTMX and Monaco URLs
2. Add `http_archive` to `MODULE.bazel`
3. Update `fs/BUILD.bazel` to stage vendor assets
4. Update build test to verify vendor files
5. Test Monaco and HTMX serving at runtime
6. Verify binary size < 10 MB

**Phase 2** (Future):
1. Add overlay schema to `internal/db/schema.sql`
2. Implement `hbf_db_read_file()` with overlay support
3. Add dev API endpoints to `server.js`
4. Create minimal dev UI with Monaco
5. Test live edit workflow

## Next Steps

### Immediate (Ready to Implement)
1. **Get actual sha256 hashes** for HTMX and Monaco:
   ```bash
   curl -sL https://unpkg.com/htmx.org@2.0.3/dist/htmx.min.js | sha256sum
   curl -sL https://registry.npmjs.org/monaco-editor/-/monaco-editor-0.52.0.tgz | sha256sum
   ```

2. **Implement Phase 0** (see `phase0-1-plan.md` for detailed steps)

3. **Verify Phase 0 acceptance criteria** before moving to Phase 1

### Future (After Phase 0 & 1)
1. Implement Phase 2 overlay filesystem
2. Build dev UI with Monaco editor
3. Add SSE for live reload
4. Consider Monaco Vim mode (optional)

## Files Modified/Created

### Created
- ✅ `DOCS/phase0-1-plan.md` - Detailed Phase 0 & 1 implementation guide
- ✅ `DOCS/phase2-overlay-plan.md` - Phase 2 overlay design and dev API
- ✅ `DOCS/SUMMARY.md` - This summary document

### Renamed
- ✅ `DOCS/hbf_next.md` → `DOCS/hbf_next.md.old` - Backup of ChatGPT document

### To Be Modified (Phase 0)
- `fs/BUILD.bazel` - Fix genrule to use $(SRCS) loop
- `fs/fs_build_test.sh` - Add tests for style.css and favicon.ico
- `internal/http/server.h` - Add `dev` field to `hbf_server_t`
- `internal/http/server.c` - Thread dev flag, conditional cache headers, MIME types
- `internal/core/main.c` - Pass `config.dev` to `hbf_server_create()`

### To Be Modified (Phase 1)
- `MODULE.bazel` - Add `http_archive` for HTMX and Monaco
- `fs/BUILD.bazel` - Stage vendor assets from external repos
- `fs/fs_build_test.sh` - Verify vendor files in archive

## Questions Answered

**Q**: How many runtime databases are there?
**A**: One - main DB (`hbf.db` or `:memory:`). Embedded `fs.db` is private to `internal/db/db.c` and only used during hydration.

**Q**: Can we write to main DB at runtime?
**A**: Yes! Main DB is opened read-write. This enables overlay pattern without separate dev DB.

**Q**: How do we add files without editing Bazel every time?
**A**: Fix `fs/BUILD.bazel` genrule to loop over `$(SRCS)` instead of hardcoding filenames. Glob already captures files, just need to process them dynamically.

**Q**: Where does `--dev` flag go after parsing?
**A**: Currently only logged. Phase 0 will thread it to `hbf_server_t` so static handler can use it.

**Q**: How big will binary be with Monaco?
**A**: ~5-6 MB (currently 1.1 MB + Monaco ~4 MB + HTMX 15 KB).

**Q**: Do we need CDN for Monaco/HTMX?
**A**: No! Bazel `http_archive` fetches at build time and embeds in binary. 100% offline capable.

## Success Metrics

**Phase 0 Success**:
- [ ] Archive contains 4+ files (server.js, index.html, style.css, favicon.ico)
- [ ] `curl http://localhost:5309/static/style.css` returns CSS (not 404)
- [ ] Dev mode: `Cache-Control: no-store`
- [ ] Prod mode: `Cache-Control: public, max-age=3600`
- [ ] All tests pass

**Phase 1 Success**:
- [ ] HTMX serves from `/static/vendor/htmx.min.js`
- [ ] Monaco serves from `/static/monaco/vs/loader.js`
- [ ] Archive contains 100+ files
- [ ] Binary size < 10 MB
- [ ] No CDN dependencies
- [ ] All tests pass

**Phase 2 Success** (Future):
- [ ] Dev API endpoints work (`GET/PUT/DELETE /__dev/api/file`)
- [ ] Overlay tables created in main DB
- [ ] Base `sqlar` unchanged after edits
- [ ] Prod mode ignores overlay
- [ ] Dev UI loads and edits files

## Critical Insights

1. **Main DB is writable** - This is the key insight that enables overlay pattern without architectural changes. We can add tables to the same DB that holds `sqlar`.

2. **Glob works, genrule doesn't use it** - The `srcs = glob(...)` correctly identifies files, but `cmd` hardcodes only 2 files. Fix: iterate over `$(SRCS)`.

3. **No multi-tenancy** - Old hbf_next.md assumed user pods and multiple databases. Current architecture is single-instance (localhost or single k3s pod).

4. **Dev flag exists but unused** - Parse logic is there, just needs to be threaded through to server struct.

5. **Monaco needs npm tarball** - GitHub releases don't include pre-built `min/` directory. npm registry has `monaco-editor-x.y.z.tgz` with `package/min/vs/**` ready to use.

## Risks and Mitigations

**Risk**: Bazel genrule bash too complex
**Mitigation**: Test in devcontainer (Linux), add verbose logging, fallback to Python script if needed

**Risk**: Monaco binary size too large
**Mitigation**: Acceptable for dev tool; can optimize later by cherry-picking Monaco modules

**Risk**: Overlay corrupts main DB
**Mitigation**: Base `sqlar` never modified; can always restart from embedded `fs.db`

**Risk**: External http_archive fails during build
**Mitigation**: Bazel caches downloads; pin sha256 for reproducibility; offline builds work after first fetch
