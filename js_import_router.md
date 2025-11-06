# Find-My-Way Router Integration Plan

## Overview

This plan details the integration of the `find-my-way` HTTP router library into HBF's JavaScript layer (pods/test/hbf/server.js). The router will replace the current manual `if/else` routing logic with a more maintainable, performant radix-tree-based router.

**Key constraints:**
- HTTP server is handled by CivetWeb in C; we only need the router instance in JavaScript
- Use ESM imports (`import`) instead of CommonJS (`require`)
- ES modules are loaded per request from the embedded SQLite database
- Focus on the test pod for this integration

## Goals

1. ✅ **Confirm ES module imports work in server.js** - Validate that `import` statements function correctly within the QuickJS runtime loading from the embedded database
2. ✅ **Vendor find-my-way package** - Use existing `tools/npm_vendor.sh` to bring the MIT-licensed router into the repo
3. ✅ **Replace manual routing** - Refactor `pods/test/hbf/server.js` to use find-my-way instead of manual path matching
4. ✅ **Maintain compatibility** - Ensure all existing routes continue to work
5. ✅ **Document patterns** - Establish router patterns for future pods

## Background

### Find-My-Way Overview

**License:** MIT (compatible with HBF's licensing constraints)

**Key Features:**
- Fast radix tree (compact prefix tree) based routing
- Framework-independent
- Supports parametric routes (`/user/:id`), wildcards, and regex
- Lightweight and performant

**API Highlights:**
```javascript
import createRouter from 'find-my-way'

const router = createRouter()

// Register routes
router.on('GET', '/', (req, res, params) => {
  res.end('hello world')
})

router.on('GET', '/user/:id', (req, res, params) => {
  res.end(`User ${params.id}`)
})

// Lookup and execute
router.lookup(req, res)
```

**HBF Adaptation:**
- We don't create the HTTP server (CivetWeb does that)
- We don't call `router.lookup()` directly (C handler calls `app.handle()`)
- We need to bridge HBF's `req`/`res` objects to the router

### Current Routing Implementation

The test pod's `server.js` currently uses manual routing:

```javascript
app.handle = function (req, res) {
  const { method, path } = req;

  if (path === "/" && method === "GET") {
    // Home page handler
  }
  else if (path === "/hello" && method === "GET") {
    // Hello handler
  }
  else if (path.startsWith("/user/") && method === "GET") {
    // User handler with manual param extraction
  }
  // ... more routes
}
```

**Problems with current approach:**
- ❌ Manual path matching is error-prone
- ❌ No automatic parameter extraction
- ❌ Difficult to maintain as routes grow
- ❌ No route introspection or debugging
- ❌ Hard to add middleware or route grouping

## Implementation Plan

### Phase 1: Validate ES Module Support in Server.js

**Goal:** Confirm that ES module imports work within the QuickJS runtime when loading from the embedded database.

#### Step 1.1: Create Test Module

Create a simple test module to validate import functionality:

**File:** `pods/test/hbf/lib/hello.js`
```javascript
// Simple module to test ES imports
export function greet(name) {
  return `Hello, ${name}!`;
}

export const version = "1.0.0";
```

#### Step 1.2: Update server.js to Import Test Module

**File:** `pods/test/hbf/server.js` (add at top)
```javascript
import { greet, version } from "./lib/hello.js";

// Test the import in a route
app.handle = function (req, res) {
  // ... existing code ...

  // Add new test route
  if (path === "/test-import" && method === "GET") {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({
      message: greet("HBF"),
      version: version
    }));
    return;
  }

  // ... rest of routes ...
}
```

#### Step 1.3: Build and Test

```bash
# Build test pod
bazel build //:hbf_test

# Run test pod
./bazel-bin/bin/hbf_test --port 5309 --dev

# Test the import
curl http://localhost:5309/test-import
# Expected: {"message":"Hello, HBF!","version":"1.0.0"}
```

**Success Criteria:**
- ✅ Build completes without errors
- ✅ Server starts successfully
- ✅ `/test-import` route returns expected JSON
- ✅ No QuickJS module loading errors in logs

**If this fails:** We need to revisit the ES module loader implementation in `hbf/qjs/module_loader.c` per the `js_import_impl.md` plan before proceeding.

### Phase 2: Vendor Find-My-Way Package

**Goal:** Bring the find-my-way router into the repo using the existing npm vendoring workflow.

#### Step 2.1: Research Package Details

Check the latest stable version and verify ESM support:

```bash
# Check package info
npm view find-my-way versions
npm view find-my-way dist.module

# Verify MIT license
npm view find-my-way license
```

As of this writing, find-my-way@8.x supports ESM exports.

#### Step 2.2: Vendor the Package

Use the existing `tools/npm_vendor.sh` script:

```bash
# Vendor find-my-way for test pod
tools/npm_vendor.sh --pod test find-my-way@8.2.2
```

**Expected outputs:**
```
third_party/npm/
  find-my-way@8.2.2/
    pkg/                          # Extracted npm package
      package.json
      index.js
      lib/
      ...
    LICENSE                       # MIT license

pods/test/
  static/
    vendor/
      esm/
        find-my-way.js            # Bundled ESM
        find-my-way.js.map        # Source map
    importmap.json                # Updated with find-my-way entry
```

**Verify import map:**
```bash
cat pods/test/static/importmap.json
```

Expected:
```json
{
  "imports": {
    "find-my-way": "/static/vendor/esm/find-my-way.js"
  }
}
```

#### Step 2.3: Test Browser Import (Optional)

Create a quick browser test to verify the vendored module loads correctly:

**File:** `pods/test/static/test-router.html`
```html
<!DOCTYPE html>
<html>
<head>
  <title>Router Test</title>
  <script type="importmap" src="/static/importmap.json"></script>
</head>
<body>
  <h1>Find-My-Way Browser Test</h1>
  <div id="status">Testing import...</div>

  <script type="module">
    import createRouter from "find-my-way";

    const statusEl = document.getElementById("status");

    try {
      const router = createRouter();
      statusEl.textContent = "✓ Successfully imported find-my-way!";
      statusEl.style.color = "green";
      console.log("Router instance:", router);
    } catch (err) {
      statusEl.textContent = `✗ Import failed: ${err.message}`;
      statusEl.style.color = "red";
    }
  </script>
</body>
</html>
```

Test in browser:
```bash
./bazel-bin/bin/hbf_test --port 5309 --dev
# Visit: http://localhost:5309/static/test-router.html
```

**Success Criteria:**
- ✅ Package vendored successfully
- ✅ License file copied
- ✅ ESM bundle created
- ✅ Import map updated
- ✅ (Optional) Browser test shows green success

### Phase 3: Server-Side Import Test

**Goal:** Confirm find-my-way can be imported and instantiated in the server.js QuickJS context.

#### Step 3.1: Add Import Statement

Since we're working server-side (not browser), we need to import from the embedded database path, NOT from the import map. The import map is only for browser `<script type="module">` contexts.

**Important:** Server-side imports use relative or absolute paths in the virtual filesystem:

**File:** `pods/test/hbf/server.js` (top of file)
```javascript
// NOTE: Server-side (QuickJS) imports use filesystem paths, not import maps
// Import maps are for browser-side <script type="module"> only

// Test: Can we load find-my-way from static/vendor/esm/?
import createRouter from "/static/vendor/esm/find-my-way.js";

console.log("Router constructor loaded:", typeof createRouter);
```

#### Step 3.2: Create Router Instance in Handler

Add router instantiation and test route:

```javascript
app.handle = function (req, res) {
  // Create router instance per request (for now)
  const router = createRouter();

  // Register test route
  router.on('GET', '/router-test', (req, res, params) => {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({
      message: "Router working!",
      params: params
    }));
  });

  // Try to lookup route
  const found = router.find(req.method, req.path);

  if (found) {
    // Route found, execute handler
    found.handler(req, res, found.params);
    return;
  }

  // Fallback to existing manual routes
  // ... existing if/else chain ...
}
```

#### Step 3.3: Test

```bash
# Rebuild
bazel build //:hbf_test

# Run
./bazel-bin/bin/hbf_test --port 5309

# Test router
curl http://localhost:5309/router-test
# Expected: {"message":"Router working!","params":{}}
```

**Success Criteria:**
- ✅ Import succeeds without QuickJS errors
- ✅ Router instantiates successfully
- ✅ Test route returns expected response
- ✅ Existing routes still work

**If this fails:**
- Check QuickJS logs for module loading errors
- Verify the ESM bundle is valid JavaScript (not CommonJS)
- Check that the bundled file is in the embedded database

### Phase 4: Full Router Migration

**Goal:** Replace all manual routing with find-my-way, maintaining 100% compatibility.

#### Step 4.1: Design Router Architecture

**Key decisions:**

1. **Router lifecycle:** Create router instance once per request or share?
   - **Recommendation:** Create once at module load, reuse for all requests
   - **Rationale:** Radix tree is read-only after route registration, safe to share

2. **HBF req/res bridge:** Find-my-way expects standard Node.js req/res
   - **Solution:** Wrap HBF's req/res objects or pass directly if compatible
   - **HBF's req object:** `{ method, path, query, body, dev, headers }`
   - **HBF's res object:** `{ status(), set(), send() }`
   - **Find-my-way expects:** `req.method`, `req.url` (path), callback `(req, res, params)`

3. **Parameter extraction:** Router provides `params` object
   - **Current:** Manual string splitting for `/user/:id`
   - **After:** `params.id` provided by router

4. **Query string handling:** Router doesn't parse query strings
   - **Keep existing:** `parseQuery()` helper function

#### Step 4.2: Refactor server.js

**File:** `pods/test/hbf/server.js`

```javascript
// Import router at module level
import createRouter from "/static/vendor/esm/find-my-way.js";

// Create router instance (shared across requests)
const router = createRouter({
  defaultRoute: (req, res) => {
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
  }
});

// Helper: Parse query string into object
function parseQuery(queryString) {
  const params = {};
  if (!queryString) return params;
  queryString.split('&').forEach(function (pair) {
    const parts = pair.split('=');
    if (parts[0]) {
      params[decodeURIComponent(parts[0])] = parts[1] ?
        decodeURIComponent(parts[1]) : '';
    }
  });
  return params;
}

// Register routes
// Route: Home page
router.on('GET', '/', (req, res, params) => {
  res.set("Content-Type", "text/html");
  res.send(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HBF Test Pod</title>
    <link rel="stylesheet" href="/static/style.css">
</head>
<body>
    <h1>HBF Test Pod</h1>
    <p>Find-My-Way Router Integration</p>
    <h2>Available Routes</h2>
    <ul>
        <li><a href="/hello">/hello</a> - Hello world</li>
        <li><a href="/user/42">/user/42</a> - User endpoint (parametric)</li>
        <li><a href="/echo">/echo</a> - Echo request</li>
        <li><a href="/__dev/">/__dev/</a> - Dev editor (dev mode only)</li>
    </ul>
</body>
</html>`);
});

// Route: Hello world
router.on('GET', '/hello', (req, res, params) => {
  res.set("Content-Type", "application/json");
  res.send(JSON.stringify({ message: "Hello, World!" }));
});

// Route: Parametric user route
router.on('GET', '/user/:id', (req, res, params) => {
  res.set("Content-Type", "application/json");
  res.send(JSON.stringify({ userId: params.id }));
});

// Route: Echo
router.on('GET', '/echo', (req, res, params) => {
  res.set("Content-Type", "application/json");
  res.send(JSON.stringify({
    method: req.method,
    url: req.path
  }));
});

// Route: Dev UI
router.on('GET', '/__dev/', (req, res, params) => {
  if (!req.dev) {
    res.status(403);
    res.set("Content-Type", "text/plain");
    res.send("Dev mode not enabled");
    return;
  }

  const result = db.query(
    "SELECT data AS content FROM latest_files WHERE path = ?",
    ["static/dev/index.html"]
  );

  if (result.length === 0) {
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Dev UI not found");
    return;
  }

  res.set("Content-Type", "text/html");
  res.send(result[0].content);
});

// Route: Dev API - Read file
router.on('GET', '/__dev/api/file', (req, res, params) => {
  if (!req.dev) {
    res.status(403);
    res.set("Content-Type", "text/plain");
    res.send("Dev mode not enabled");
    return;
  }

  const query = parseQuery(req.query);
  const fileName = query.name;

  if (!fileName || (!fileName.startsWith("static/") && !fileName.startsWith("hbf/"))) {
    res.status(400);
    res.set("Content-Type", "text/plain");
    res.send("Invalid file name (must start with static/ or hbf/)");
    return;
  }

  const result = db.query(
    "SELECT data AS content FROM latest_files WHERE path = ?",
    [fileName]
  );

  if (result.length === 0) {
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("File not found");
    return;
  }

  const ext = fileName.split(".").pop();
  const contentTypes = {
    "html": "text/html",
    "css": "text/css",
    "js": "application/javascript",
    "json": "application/json",
    "md": "text/markdown",
    "txt": "text/plain"
  };
  const contentType = contentTypes[ext] || "text/plain";

  res.set("Content-Type", contentType);
  res.send(result[0].content);
});

// Route: Dev API - Write file
router.on('PUT', '/__dev/api/file', (req, res, params) => {
  if (!req.dev) {
    res.status(403);
    res.set("Content-Type", "text/plain");
    res.send("Dev mode not enabled");
    return;
  }

  const query = parseQuery(req.query);
  const fileName = query.name;

  if (!fileName || (!fileName.startsWith("static/") && !fileName.startsWith("hbf/"))) {
    res.status(400);
    res.set("Content-Type", "text/plain");
    res.send("Invalid file name (must start with static/ or hbf/)");
    return;
  }

  const content = req.body;
  const mtime = Math.floor(Date.now() / 1000);

  // Write to versioned filesystem
  db.execute(
    "INSERT OR IGNORE INTO file_ids (path) VALUES (?)",
    [fileName]
  );

  const fileIdResult = db.query(
    "SELECT file_id FROM file_ids WHERE path = ?",
    [fileName]
  );

  if (fileIdResult.length === 0) {
    res.status(500);
    res.send("Failed to get file_id");
    return;
  }

  const fileId = fileIdResult[0].file_id;

  const versionResult = db.query(
    "SELECT COALESCE(MAX(version_number), 0) + 1 AS next_version FROM file_versions WHERE file_id = ?",
    [fileId]
  );

  const nextVersion = versionResult[0].next_version;
  const size = content ? content.length : 0;

  db.execute(
    "INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) VALUES (?, ?, ?, ?, ?, ?)",
    [fileId, fileName, nextVersion, mtime, size, content]
  );

  res.status(200);
  res.set("Content-Type", "text/plain");
  res.send("OK");
});

// Route: Dev API - Delete file
router.on('DELETE', '/__dev/api/file', (req, res, params) => {
  if (!req.dev) {
    res.status(403);
    res.set("Content-Type", "text/plain");
    res.send("Dev mode not enabled");
    return;
  }

  const query = parseQuery(req.query);
  const fileName = query.name;

  if (!fileName) {
    res.status(400);
    res.set("Content-Type", "text/plain");
    res.send("Missing file name");
    return;
  }

  const fileIdResult = db.query(
    "SELECT file_id FROM file_ids WHERE path = ?",
    [fileName]
  );

  if (fileIdResult.length === 0) {
    res.status(404);
    res.send("File not found");
    return;
  }

  const fileId = fileIdResult[0].file_id;

  db.execute(
    "DELETE FROM file_versions WHERE file_id = ?",
    [fileId]
  );

  db.execute(
    "DELETE FROM file_ids WHERE file_id = ?",
    [fileId]
  );

  res.status(200);
  res.set("Content-Type", "text/plain");
  res.send("OK");
});

// Export app.handle for C handler
// Note: globalThis not needed here since we're not using ES modules in strict mode yet
app = {};

app.handle = function (req, res) {
  // Add common middleware
  res.set("X-Powered-By", "HBF");

  // Find and execute route
  // Note: find-my-way expects req.url but HBF provides req.path
  // We'll use router.find() instead of router.lookup() for more control
  const found = router.find(req.method, req.path);

  if (found) {
    // Execute route handler with params
    found.handler(req, res, found.params || {});
  }
  // If no route found, defaultRoute (404) is NOT called by find()
  // We need to call it manually or use lookup()
  else {
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
  }
};
```

#### Step 4.3: Testing Strategy

Create comprehensive test suite:

**File:** `test_routes.sh`
```bash
#!/bin/bash
# Test all routes in the migrated router

set -e

BASE_URL="http://localhost:5309"

echo "Testing router migration..."

# Test 1: Home page
echo -n "Test 1: GET / ... "
RESPONSE=$(curl -s "$BASE_URL/")
if [[ "$RESPONSE" == *"HBF Test Pod"* ]]; then
  echo "✓"
else
  echo "✗ Failed"
  exit 1
fi

# Test 2: Hello route
echo -n "Test 2: GET /hello ... "
RESPONSE=$(curl -s "$BASE_URL/hello")
if [[ "$RESPONSE" == *'"message":"Hello, World!"'* ]]; then
  echo "✓"
else
  echo "✗ Failed"
  exit 1
fi

# Test 3: Parametric route
echo -n "Test 3: GET /user/42 ... "
RESPONSE=$(curl -s "$BASE_URL/user/42")
if [[ "$RESPONSE" == *'"userId":"42"'* ]]; then
  echo "✓"
else
  echo "✗ Failed"
  exit 1
fi

# Test 4: Echo route
echo -n "Test 4: GET /echo ... "
RESPONSE=$(curl -s "$BASE_URL/echo")
if [[ "$RESPONSE" == *'"method":"GET"'* ]]; then
  echo "✓"
else
  echo "✗ Failed"
  exit 1
fi

# Test 5: 404 handling
echo -n "Test 5: GET /nonexistent ... "
STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/nonexistent")
if [[ "$STATUS" == "404" ]]; then
  echo "✓"
else
  echo "✗ Failed (got $STATUS)"
  exit 1
fi

echo ""
echo "✅ All router tests passed!"
```

Run tests:
```bash
# Build and run server in background
bazel build //:hbf_test
./bazel-bin/bin/hbf_test --port 5309 &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Run tests
chmod +x test_routes.sh
./test_routes.sh

# Kill server
kill $SERVER_PID
```

**Success Criteria:**
- ✅ All existing routes work identically
- ✅ Parametric routes extract params correctly
- ✅ 404 handling works
- ✅ Dev mode routes work with `--dev` flag
- ✅ No regression in functionality

### Phase 5: Optimization and Polish

#### Step 5.1: Router Debugging

Add debug route to inspect registered routes:

```javascript
// Debug route (dev mode only)
router.on('GET', '/__dev/routes', (req, res, params) => {
  if (!req.dev) {
    res.status(403);
    res.send("Dev mode not enabled");
    return;
  }

  // Pretty print router tree
  const routes = router.prettyPrint();

  res.set("Content-Type", "text/plain");
  res.send(routes);
});
```

Test:
```bash
./bazel-bin/bin/hbf_test --dev
curl http://localhost:5309/__dev/routes
```

#### Step 5.2: Performance Comparison

Use the existing benchmark skill to compare performance:

```bash
# Benchmark before router migration (current manual routing)
bazel build //:hbf_test
./bazel-bin/bin/hbf_test --port 5309 &
ab -n 10000 -c 100 http://localhost:5309/hello
kill $!

# Benchmark after router migration (find-my-way)
# ... rebuild with new router code ...
./bazel-bin/bin/hbf_test --port 5309 &
ab -n 10000 -c 100 http://localhost:5309/hello
kill $!
```

**Expected:** Find-my-way should be equal or faster due to radix tree O(k) lookup vs O(n) if/else.

#### Step 5.3: Documentation

Update pod documentation:

**File:** `pods/test/README.md`
```markdown
# HBF Test Pod

Test pod demonstrating find-my-way router integration.

## Router Features

- **Parametric routes:** `/user/:id` automatically extracts parameters
- **Radix tree matching:** O(k) lookup time (k = path length)
- **Route introspection:** `GET /__dev/routes` shows route tree (dev mode)

## Adding New Routes

```javascript
// Simple route
router.on('GET', '/api/health', (req, res, params) => {
  res.send(JSON.stringify({ status: 'ok' }));
});

// Parametric route
router.on('GET', '/posts/:postId/comments/:commentId', (req, res, params) => {
  const { postId, commentId } = params;
  res.send(JSON.stringify({ postId, commentId }));
});

// Wildcard route
router.on('GET', '/static/*', (req, res, params) => {
  res.send(`Wildcard matched: ${params['*']}`);
});
```

## Testing

Run integration tests:
```bash
bash test_routes.sh
```
```

### Phase 6: Migrate Base Pod (Future)

**Note:** This phase is out of scope for the current task but documented for completeness.

Once the test pod integration is stable:

1. Vendor find-my-way for base pod:
   ```bash
   tools/npm_vendor.sh --pod base find-my-way@8.2.2
   ```

2. Refactor `pods/base/hbf/server.js` using same patterns

3. Update base pod tests

4. Document as canonical pattern in `CLAUDE.md`

## Success Criteria

### Phase 1: ES Module Validation
- ✅ Simple module import works in server.js
- ✅ `/test-import` route returns expected response
- ✅ No QuickJS errors

### Phase 2: Package Vendoring
- ✅ find-my-way vendored to `third_party/npm/`
- ✅ ESM bundle in `pods/test/static/vendor/esm/`
- ✅ MIT license file copied
- ✅ Import map updated

### Phase 3: Server Import
- ✅ Router imports successfully in QuickJS
- ✅ Router instance created
- ✅ Test route works

### Phase 4: Full Migration
- ✅ All manual routes replaced with router
- ✅ Parametric routes work (`/user/:id`)
- ✅ All existing tests pass
- ✅ No functionality regression

### Phase 5: Polish
- ✅ Debug route added
- ✅ Performance benchmarks equal or better
- ✅ Documentation updated

## Implementation Checklist

- [ ] **Phase 1.1:** Create test module `pods/test/hbf/lib/hello.js`
- [ ] **Phase 1.2:** Add import to server.js
- [ ] **Phase 1.3:** Build and test `/test-import` route
- [ ] **Phase 2.1:** Research find-my-way version and license
- [ ] **Phase 2.2:** Run `tools/npm_vendor.sh --pod test find-my-way@8.2.2`
- [ ] **Phase 2.3:** Verify vendored files
- [ ] **Phase 3.1:** Import find-my-way in server.js
- [ ] **Phase 3.2:** Create router instance and test route
- [ ] **Phase 3.3:** Test `/router-test` route
- [ ] **Phase 4.1:** Design router architecture
- [ ] **Phase 4.2:** Refactor all routes to use router
- [ ] **Phase 4.3:** Create and run test suite
- [ ] **Phase 5.1:** Add debug route
- [ ] **Phase 5.2:** Run performance benchmarks
- [ ] **Phase 5.3:** Update documentation
- [ ] **Commit:** All changes to git

## Risks and Mitigations

### Risk 1: ES Module Loading Fails
**Impact:** High - Blocks entire integration
**Mitigation:** Phase 1 validates imports work before proceeding
**Fallback:** Implement module loader per `js_import_impl.md` if needed

### Risk 2: Bundle Incompatibility
**Impact:** Medium - Router doesn't work in QuickJS
**Mitigation:** Test server-side import in Phase 3 before full migration
**Fallback:** Bundle with different esbuild settings or use different router

### Risk 3: Performance Regression
**Impact:** Medium - Slower routing affects all requests
**Mitigation:** Benchmark in Phase 5
**Fallback:** Keep router instance per-request vs shared if needed

### Risk 4: HBF req/res Incompatibility
**Impact:** Medium - Router can't use HBF's objects
**Mitigation:** Use `router.find()` instead of `router.lookup()` for control
**Fallback:** Create adapter layer to wrap req/res

## Future Enhancements

- **Middleware support:** Add pre/post route middleware
- **Route grouping:** Group routes by prefix (e.g., `/api/*`)
- **Versioned APIs:** Use constraints for API versioning
- **Route validation:** Schema validation for params and body
- **OpenAPI generation:** Auto-generate API docs from routes
- **Hot reload:** Reload routes without restarting server (dev mode)

## References

- **find-my-way repository:** https://github.com/delvedor/find-my-way
- **find-my-way documentation:** https://github.com/delvedor/find-my-way#find-my-way
- **HBF ES module plan:** `js_import_plan.md`
- **HBF ES module implementation:** `js_import_impl.md`
- **npm vendoring script:** `tools/npm_vendor.sh`
- **HBF architecture:** `CLAUDE.md`
