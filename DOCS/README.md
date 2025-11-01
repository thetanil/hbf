# HBF Documentation

This directory contains the planning and specification documents for HBF (web compute environment).

## Quick Navigation

### Current Status
- **Completed**: Phase 0 (Foundation), Phase 1 (HTTP Server), Phase 2a (SQLite Integration), Phase 2b (User Pod Management - deprecated)
- **Next**: Phase 0 & 1 (Static Assets + Monaco/HTMX) - see below
- **Current Binary**: 1.1 MB, fully static, working HTTP server with QuickJS

### Start Here
1. **[SUMMARY.md](SUMMARY.md)** - High-level overview of what was done, findings, and next steps
2. **[phase0-1-plan.md](phase0-1-plan.md)** - Detailed implementation plan for next phases (READY TO IMPLEMENT)
3. **[phase2-overlay-plan.md](phase2-overlay-plan.md)** - Future dev overlay filesystem design

## Document Index

### Primary Specifications
- **[hbf_spec.md](hbf_spec.md)** - Current implementation specification (runtime behavior)
- **[coding-standards.md](coding-standards.md)** - C99 coding standards and style guide
- **[development-setup.md](development-setup.md)** - Devcontainer setup and build instructions

### Implementation Plans (Active)
- **[phase0-1-plan.md](phase0-1-plan.md)** ⭐ **START HERE FOR NEXT IMPLEMENTATION**
  - Phase 0: Fix static asset packaging (auto-glob)
  - Phase 1: Add HTMX and Monaco Editor
  - Estimated time: 2.5 hours
  - All acceptance criteria defined
  - Bazel code examples provided

- **[phase2-overlay-plan.md](phase2-overlay-plan.md)** (Future)
  - Dev overlay filesystem design
  - Live edit API endpoints
  - Monaco editor UI
  - SQL schema and C API changes

### Completion Reports (Historical)
- **[phase0-completion.md](phase0-completion.md)** - Foundation (Bazel, musl, directory structure)
- **[phase1-completion.md](phase1-completion.md)** - HTTP server with CivetWeb
- **[phase2b-completion.md](phase2b-completion.md)** - User pod management (deprecated - multi-tenancy removed)

### Archived/Superseded
- **[hbf_next.md.old](hbf_next.md.old)** - Original ChatGPT-generated plan (archived)
  - Contains incorrect assumptions (multi-tenancy, separate dev DB, cenv references)
  - Superseded by `phase0-1-plan.md` and `phase2-overlay-plan.md`

## Architecture Quick Reference

### Current State (Verified 2025-11-01)
```
Build Time:
  fs/hbf/**/* + fs/static/**/*
    → sqlite3 -A -cf fs.db
    → db_to_c → fs_embedded.c
    → linked into hbf binary

Runtime:
  Embedded fs.db template → hydrate once → main DB (hbf.db or :memory:)

  HTTP Requests:
    /health        → C handler (health check)
    /static/**     → C handler → read from main DB sqlar table
    /**            → QuickJS handler → load hbf/server.js from main DB sqlar
```

### Architecture Principles
- ✅ **Single database**: Only `hbf.db` at runtime (no separate fs_db, no dev DB)
- ✅ **Writable main DB**: Enables overlay pattern in same database
- ✅ **Single binary**: All dependencies statically linked with musl
- ✅ **No multi-tenancy**: Single-instance deployment (was removed from architecture)
- ✅ **No CDN dependencies**: HTMX and Monaco embedded in binary

### Key Files
```
/workspaces/hbf/
  ├── fs/
  │   ├── BUILD.bazel          ← NEEDS FIX: Hardcodes 2 files instead of using glob
  │   ├── hbf/
  │   │   └── server.js        ← QuickJS request handler
  │   └── static/
  │       ├── index.html       ← Root page
  │       ├── style.css        ← MISSING FROM ARCHIVE (404 at runtime)
  │       └── favicon.ico      ← MISSING FROM ARCHIVE
  │
  ├── internal/
  │   ├── db/
  │   │   ├── db.c             ← hbf_db_read_file_from_main() - reads from sqlar
  │   │   └── db.h
  │   ├── http/
  │   │   ├── server.c         ← Static file handler, needs dev flag
  │   │   ├── server.h         ← hbf_server_t struct, needs dev field
  │   │   └── handler.c        ← QuickJS request handler
  │   └── core/
  │       ├── config.c         ← Parses --dev flag (already done)
  │       └── main.c           ← Needs to pass config.dev to server
  │
  └── DOCS/
      ├── README.md            ← This file
      ├── SUMMARY.md           ← Review findings and next steps
      └── phase0-1-plan.md     ← Implementation guide (start here)
```

## Common Tasks

### Build and Run
```bash
# Build binary
bazel build //:hbf

# Run tests
bazel test //...

# Run server (dev mode)
rm -f hbf.db && bazel-bin/hbf --dev --inmem

# Inspect archive
sqlite3 bazel-bin/fs/fs.db "SELECT name FROM sqlar"
```

### Development Workflow
```bash
# 1. Read the plan
cat DOCS/phase0-1-plan.md

# 2. Make changes to fs/BUILD.bazel, etc.
# (see phase0-1-plan.md for exact code)

# 3. Rebuild
bazel build //:hbf

# 4. Test
bazel test //fs:fs_build_test --test_output=all

# 5. Runtime verification
rm -f hbf.db && bazel-bin/hbf --inmem --dev &
curl http://localhost:5309/static/style.css  # Should NOT be 404
pkill hbf
```

### Verify Current Issues
```bash
# Issue 1: Only 2 files in archive (should be 4+)
sqlite3 bazel-bin/fs/fs.db "SELECT COUNT(*) FROM sqlar"
# Output: 2 (WRONG - missing style.css and favicon.ico)

# Issue 2: style.css returns 404
bazel-bin/hbf --inmem &
curl http://localhost:5309/static/style.css
# Output: Error 404: Not Found (WRONG)
pkill hbf

# Issue 3: Dev flag not used
bazel-bin/hbf --dev --inmem &
curl -I http://localhost:5309/static/index.html | grep Cache-Control
# Output: Cache-Control: public, max-age=3600 (WRONG - should be no-store in dev mode)
pkill hbf
```

## Implementation Roadmap

### Phase 0: Fix Static Asset Packaging (~1.5 hours)
**Goal**: All files in `fs/` auto-included in archive, dev mode cache headers work

**Tasks**:
1. Fix `fs/BUILD.bazel` genrule to loop over `$(SRCS)` ✅ Code provided
2. Update `fs_build_test.sh` to verify 4 files ✅ Code provided
3. Add MIME types (WOFF, WOFF2, WASM, MD) ✅ Code provided
4. Thread `config.dev` to `hbf_server_t` ✅ Code provided
5. Conditional cache headers based on dev flag ✅ Code provided

**Acceptance**:
- Archive has 4 files
- `style.css` serves without 404
- Dev mode: `Cache-Control: no-store`
- Prod mode: `Cache-Control: public, max-age=3600`

### Phase 1: Add HTMX and Monaco (~1 hour)
**Goal**: Serve Monaco and HTMX from embedded binary (no CDN)

**Tasks**:
1. Get sha256 for HTMX and Monaco URLs ⚠️ TODO
2. Add `http_archive` to `MODULE.bazel` ✅ Code provided
3. Update `fs/BUILD.bazel` to stage vendor assets ✅ Code provided
4. Update build test to verify vendor files ✅ Code provided
5. Test serving at runtime ✅ Commands provided

**Acceptance**:
- HTMX serves from `/static/vendor/htmx.min.js`
- Monaco serves from `/static/monaco/vs/loader.js`
- Archive has 100+ files
- Binary size < 10 MB
- No CDN dependencies

### Phase 2: Dev Overlay Filesystem (Future)
**Goal**: Live edit files in browser with Monaco

**Tasks**:
1. Add overlay schema (sqlar + fs_layer + overlay_sqlar)
2. Implement `hbf_db_read_file()` with overlay support
3. Add dev API endpoints (GET/PUT/DELETE /__dev/api/file)
4. Create minimal Monaco UI at /static/dev/index.html
5. Test live edit → save → preview workflow

**See**: [phase2-overlay-plan.md](phase2-overlay-plan.md)

## Troubleshooting

### "Archive is empty" or "File count is 2"
**Cause**: `fs/BUILD.bazel` genrule hardcodes only `server.js` and `index.html`
**Fix**: Implement Phase 0.1 to use `$(SRCS)` loop

### "404 Not Found" for style.css
**Cause**: File exists in `fs/static/` but not included in archive
**Fix**: Same as above (Phase 0.1)

### "Dev mode still shows max-age=3600"
**Cause**: `config.dev` not passed to `hbf_server_t`
**Fix**: Implement Phase 0.4 to thread dev flag

### "Monaco files not found in archive"
**Cause**: Phase 1 not implemented yet
**Fix**: Must complete Phase 0 first, then implement Phase 1

### "Build fails with missing sha256"
**Cause**: `http_archive` requires integrity hash
**Fix**: Download URLs and compute sha256 (see phase0-1-plan.md Section 1.1)

## Key Insights from Code Review

1. **Glob works, genrule doesn't** - `srcs = glob(["static/**/*"])` correctly finds files, but `cmd = "... index.html"` only archives 2. Fix: Loop over `$(SRCS)`.

2. **Main DB is writable** - No `SQLITE_OPEN_READONLY` flag means we can add overlay tables to same DB. No need for separate dev DB.

3. **Dev flag exists but unused** - Parsing works in `config.c`, just needs to be passed to `server->dev` field.

4. **Static handler already has DB access** - `server->db` is available, just need to check `server->dev` to choose view.

5. **QuickJS creates fresh context per request** - No global state, safe to implement dev API in JavaScript.

## Questions? Issues?

**Before implementing**:
1. Read [SUMMARY.md](SUMMARY.md) for context
2. Read [phase0-1-plan.md](phase0-1-plan.md) for detailed steps
3. Verify current issues with commands in "Verify Current Issues" section

**During implementation**:
1. Follow acceptance criteria in phase0-1-plan.md
2. Run tests after each phase: `bazel test //...`
3. Verify runtime behavior, don't just trust builds

**After implementation**:
1. Update this README if architecture changes
2. Create completion report (e.g., `phase0-1-completion.md`)
3. Move to next phase

---

**Last Updated**: 2025-11-01
**Current Phase**: Ready to implement Phase 0 & 1
**See Also**: [SUMMARY.md](SUMMARY.md), [phase0-1-plan.md](phase0-1-plan.md)
