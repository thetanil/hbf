# Revised Phase 3 Implementation Plan

**Date**: 2025-10-22
**Status**: Pool removal complete, moving forward with Phase 3 completion

## Context: What We've Accomplished

### Pool Removal Migration (Complete ✅)

We've successfully migrated from the planned "context pool" architecture to a simpler **single global context** model:

**Completed Work**:
1. ✅ Single `JSRuntime` + `JSContext` created at startup (no pool)
2. ✅ Direct database access from JS via `db.query()` and `db.execute()`
3. ✅ Console module: `console.log/warn/error/debug` → HBF logger
4. ✅ HTTP handler uses global context with mutex for thread safety
5. ✅ Router.js and server.js infrastructure in place
6. ✅ Database: time-based hash `{storage_dir}/{hash(timestamp)}/index.db`
7. ✅ All tests passing (6 suites, 14 functions)

**Key Architecture Decisions Made**:
- Single-threaded JS execution (mutex-protected)
- Single SQLite connection shared between C and JS
- No context pooling (simpler, recommended by QuickJS-NG docs)
- DB-backed routing via server.js loaded from database

## What's Left: Phase 3 Completion

### Phase 3.1: Request/Response Bindings (1-2 days)

**Status**: Partially implemented
**Goal**: Complete HTTP request/response object bindings for JavaScript

**What Exists**:
- `internal/qjs/bindings/request.h` (interface defined)
- `internal/qjs/bindings/response.h` (interface defined)
- `internal/http/handler.c` references these (but stubs only)

**Tasks**:
1. **Implement request object** (`internal/qjs/bindings/request.c`):
   - [x] Parse method, path, query string from `mg_request_info`
   - [x] Extract headers into JS object
   - [x] Parse query parameters into `req.query` object
   - [x] Extract route parameters into `req.params` (set by router)
   - [ ] Body parsing for POST/PUT (JSON only for now)
   - [ ] Tests for request object creation

2. **Implement response object** (`internal/qjs/bindings/response.c`):
   - [x] `res.status(code)` - Set HTTP status code
   - [x] `res.set(name, value)` - Set response header
   - [x] `res.json(obj)` - Send JSON response
   - [x] `res.send(str)` - Send text response
   - [ ] `res.redirect(url)` - HTTP redirect
   - [ ] Tests for response object

3. **Integration**:
   - [ ] Update HTTP handler to create real req/res objects
   - [ ] Test end-to-end: HTTP request → JS handler → HTTP response
   - [ ] Handle errors gracefully (JS exceptions → 500 errors)

**Deliverables**:
- Working `internal/qjs/bindings/request.c`
- Working `internal/qjs/bindings/response.c`
- Integration tests in `internal/qjs/engine_test.c`
- End-to-end HTTP test (curl → server → JS → response)

### Phase 3.2: Static Content Injection (1 day)

**Status**: Tool exists but not integrated
**Goal**: Build-time injection of router.js, server.js, and static files into database

**What Exists**:
- `tools/inject_content.sh` (may be placeholder)
- `static/lib/router.js` ✅
- `static/server.js` ✅
- `static/www/` directory structure

**Tasks**:
1. **Content injection tool**:
   - [ ] Verify `tools/inject_content.sh` works correctly
   - [ ] Generates SQL INSERT statements for `nodes` table
   - [ ] Proper JSON encoding in `body` column
   - [ ] Sets correct `type` field ('js', 'html', 'css', etc.)
   - [ ] Sets correct `name` field (e.g., 'router.js', 'server.js')

2. **Build integration**:
   - [ ] Update schema to include static content on init
   - [ ] OR: Load static content at startup from filesystem initially
   - [ ] Document where static content lives in production

3. **Verification**:
   - [ ] Query database for router.js → verify it loads
   - [ ] Query database for server.js → verify it loads
   - [ ] Query database for index.html → verify it exists

**Deliverables**:
- Functional `tools/inject_content.sh`
- Static content available in database at startup
- Documentation on adding new static files

### Phase 3.3: Static File Serving (1 day)

**Status**: Not implemented
**Goal**: Serve static files (HTML, CSS, JS) from database via JS middleware

**Tasks**:
1. **Static middleware** (in `static/lib/static.js` or inline in server.js):
   ```javascript
   app.use(function(req, res, next) {
     // Try to find file in database
     var rows = db.query('SELECT body, type FROM nodes WHERE name = ?', [req.path]);
     if (rows.length > 0) {
       var contentType = getContentType(rows[0].type);
       res.set('Content-Type', contentType);
       res.send(rows[0].body);
     } else {
       next(); // Continue to next handler
     }
   });
   ```

2. **Content-Type mapping**:
   - `.html` → `text/html`
   - `.css` → `text/css`
   - `.js` → `application/javascript`
   - `.json` → `application/json`
   - Default → `application/octet-stream`

3. **Testing**:
   - [ ] `GET /` → serves index.html from database
   - [ ] `GET /style.css` → serves CSS from database
   - [ ] `GET /nonexistent` → 404 from final handler

**Deliverables**:
- Static file middleware in server.js
- Files served from database
- Proper Content-Type headers

### Phase 3.4: Enhanced Router Features (1-2 days)

**Status**: Basic routing works, needs enhancements
**Goal**: Production-ready Express.js-compatible routing

**What Exists**:
- `static/lib/router.js` with GET/POST/PUT/DELETE/USE
- Parameter extraction (`:id` syntax)
- Middleware chain with `next()`

**Tasks**:
1. **Error handling**:
   - [ ] Catch errors in handlers
   - [ ] Error middleware: `app.use(function(err, req, res, next) {})`
   - [ ] Default 500 error handler if user doesn't provide one

2. **Request body parsing**:
   - [ ] Parse JSON body for POST/PUT requests
   - [ ] Set `req.body` with parsed JSON
   - [ ] Handle parse errors gracefully

3. **Query string parsing**:
   - [ ] Already in router? Verify and test
   - [ ] `req.query.name` should work for `?name=value`

4. **Middleware improvements**:
   - [ ] Route-specific middleware: `app.get('/path', middleware, handler)`
   - [ ] Middleware can modify req/res
   - [ ] Test middleware chain order

**Deliverables**:
- Enhanced router.js with error handling
- Request body parsing
- Comprehensive router tests

## Phase 4: Authentication & Authorization (Future)

**Not started - roughly 1 week of work**

### Phase 4.1: Password Hashing & JWT (2-3 days)

**Tasks**:
1. **Vendor Argon2** (`third_party/argon2/`):
   - [ ] Bazel build for Argon2id
   - [ ] Wrapper: `hbf_argon2_hash(password, hash_out)`
   - [ ] Wrapper: `hbf_argon2_verify(password, hash)`

2. **JWT Implementation** (no external libs):
   - [ ] SHA-256 HMAC from scratch or vendor single-file impl
   - [ ] Base64url encoding
   - [ ] JWT generation: `hbf_jwt_sign(payload_json, secret, token_out)`
   - [ ] JWT verification: `hbf_jwt_verify(token, secret, payload_out)`

3. **Session Management**:
   - [ ] `_hbf_sessions` table (already in schema ✅)
   - [ ] Session creation on login
   - [ ] Session lookup by token hash
   - [ ] Session expiry/cleanup

4. **API Endpoints**:
   - [ ] `POST /register` - Create user with Argon2 hash
   - [ ] `POST /login` - Verify password, return JWT
   - [ ] `POST /logout` - Invalidate session

**Deliverables**:
- Argon2 password hashing working
- JWT signing/verification
- Login/register endpoints functional

### Phase 4.2: Authorization (2-3 days)

**Tasks**:
1. **Table-level permissions**:
   - [ ] `_hbf_table_permissions` enforcement
   - [ ] Query rewriting to check permissions
   - [ ] Admin-only endpoints

2. **Row-level security**:
   - [ ] `_hbf_row_policies` table
   - [ ] SQL injection of WHERE clauses
   - [ ] Per-user data isolation

3. **Middleware**:
   - [ ] Auth middleware for Express routes
   - [ ] Check JWT in Authorization header
   - [ ] Set `req.user` from validated token

**Deliverables**:
- Table permissions working
- Row policies enforced
- Auth middleware protecting routes

## Phase 5: Document Store & Search (Future, 3-4 days)

**Not started**

**Tasks**:
1. **simple-graph integration**:
   - [ ] Vendor simple-graph library
   - [ ] Document CRUD via graph operations

2. **FTS5 Search**:
   - [ ] `_hbf_document_search` table (already in schema ✅)
   - [ ] Indexing trigger
   - [ ] Search API: `GET /_api/documents/search?q=...`

3. **Document API**:
   - [ ] `GET/POST/PUT/DELETE /_api/documents/{id}`
   - [ ] Tag management
   - [ ] Metadata queries

**Deliverables**:
- Document CRUD working
- Full-text search operational
- Tag-based filtering

## Phase 6: Templates & Node Compatibility (Future, 1 week)

### Phase 6.1: EJS Templates (2-3 days)

**Tasks**:
- [ ] Vendor or implement minimal EJS
- [ ] Template rendering from database
- [ ] Server-side rendering for HTML responses

### Phase 6.2: Node.js Compatibility (2-3 days)

**Tasks**:
- [ ] CommonJS module loader
- [ ] `require()` shim backed by database
- [ ] Minimal Node API shims (process, Buffer, etc.)
- [ ] NPM module vendoring strategy

## Immediate Next Steps (This Week)

### Priority 1: Complete Phase 3.1 (Request/Response Bindings)

**Day 1**:
1. Implement `request.c` - parse HTTP request into JS object
2. Implement `response.c` - build HTTP response from JS calls
3. Write unit tests for both

**Day 2**:
1. Integrate with HTTP handler
2. End-to-end test: curl request → JS handler → HTTP response
3. Handle errors/exceptions properly

### Priority 2: Static Content (Day 3)

1. Verify `tools/inject_content.sh` works
2. Load router.js and server.js from database (not inline code)
3. Add static file middleware to server.js

### Priority 3: Testing & Documentation (Day 4)

1. Integration tests for full request/response cycle
2. Document how to add routes to server.js
3. Document how to add static files
4. Update CLAUDE.md with Phase 3 completion status

## Success Criteria

**Phase 3 Complete When**:
- ✅ HTTP requests create proper `req` object in JS
- ✅ JavaScript can respond via `res.json()`, `res.send()`, etc.
- ✅ Static files served from database
- ✅ router.js and server.js loaded from database (not hardcoded)
- ✅ Error handling works (JS exceptions → HTTP 500)
- ✅ All existing tests pass + new integration tests
- ✅ Documentation updated

## Risk Mitigation

**Known Challenges**:
1. **Request body parsing**: POST/PUT body reading from CivetWeb
   - *Solution*: Use `mg_read()` API, parse JSON in C or JS

2. **Binary content**: Serving images, fonts, etc.
   - *Solution*: Base64 encode in database, decode on serve

3. **Performance**: Single-threaded JS might be slow
   - *Solution*: Profile first, optimize later (async I/O future)

**Fallback Plans**:
- If DB loading is slow: Cache compiled scripts in memory
- If static serving is complex: Filesystem fallback initially
- If req/res bindings are tricky: Minimal subset first (just JSON API)

## Timeline Estimate

**Aggressive (Full-time work)**:
- Phase 3 completion: 1 week
- Phase 4 (Auth): 1 week
- Phase 5 (Documents): 3-4 days
- Phase 6 (Templates/Node): 1 week
- **Total**: ~1 month

**Realistic (Part-time work)**:
- Phase 3: 2 weeks
- Phase 4: 2 weeks
- Phase 5: 1 week
- Phase 6: 2 weeks
- **Total**: 2 months

## Conclusion

The pool removal was a major simplification that aligns with QuickJS-NG best practices. Moving forward, Phase 3 completion is within reach - the foundation is solid, we just need to finish request/response bindings and static content serving.

**Next commit should be**: Request/Response bindings implementation ✅
