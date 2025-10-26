# HBF Simple: Minimal QuickJS HTTP Server

## Purpose

A stripped-down variant of HBF that removes SQLite entirely to isolate and debug the QuickJS runtime integration, specifically the persistent "stack size exceeded" error. This serves as a minimal reproduction case for QuickJS context/binding issues.

## Architecture

```
CivetWeb HTTP Request
  ↓
Copy mg_request_info → qjs_request struct
  ↓
Use single global QuickJS context (created at startup)
  ↓
Load server.js from embedded C string (once at startup)
  ↓
Call handleRequest(request) in JS
  ↓
Extract qjs_response from JS return value
  ↓
Transform to CivetWeb response (mg_printf)
  ↓
Send HTTP response
```

## Key Simplifications

1. **No SQLite**: hbf_simple does not link against `internal/db/` or `internal/henv/` (these remain in repo for main hbf build)
2. **No multi-tenancy**: Single global server.js (embedded at build time)
3. **Embedded server.js**: Compile `static/server.js` into C byte array (like schema.sql)
4. **Minimal bindings**: Only `hbf:http` module (request/response objects)
5. **No authentication**: Remove all `_hbf_users`, sessions, JWT
6. **Single global context**: One QuickJS runtime + context (no pooling)
7. **Memory-only**: No disk I/O except logging

## Directory Structure

```
/hbf_simple/
  main.c              # Entry point, CivetWeb setup, request handler
  qjs_runner.c|h      # QuickJS context management, JS execution
  http_bridge.c|h     # CivetWeb ↔ QuickJS struct conversion
  server_js.c|h       # Embedded server.js (generated from static/server.js)

  BUILD.bazel         # Target: hbf_simple binary

/static/
  server.js           # Shared routing script (used by both hbf and hbf_simple)
```

## Implementation Plan

### Step 1: Core Structure (30 min)

**Files to create**:
- `hbf_simple/BUILD.bazel` - Bazel target for `hbf_simple` binary
- `hbf_simple/main.c` - CivetWeb initialization, signal handling, request handler stub
- `hbf_simple/qjs_runner.h` - Interface: `qjs_init()`, `qjs_handle_request()`, `qjs_cleanup()`
- `hbf_simple/http_bridge.h` - Structs: `qjs_request`, `qjs_response`

**Dependencies**:
- CivetWeb (already vendored)
- QuickJS-NG (already vendored)
- internal/core/log.c|h (reuse existing)

**CLI args** (minimal):
```bash
hbf_simple [--port 5309] [--log_level info]
```

### Step 2: HTTP Bridge Structs (20 min)

**File**: `hbf_simple/http_bridge.h|c`

```c
typedef struct {
    const char *method;        // "GET", "POST", etc.
    const char *uri;           // "/path?query"
    const char *query_string;  // "foo=bar&baz=qux"
    const char *http_version;  // "1.1"

    // Headers (simplified: fixed array)
    struct {
        const char *name;
        const char *value;
    } headers[32];
    int num_headers;

    // Body
    const char *body;
    size_t body_len;

    // Parsed params (optional, for debugging)
    // struct { char *key; char *value; } params[16];
} qjs_request;

typedef struct {
    int status_code;           // 200, 404, etc.

    // Headers
    struct {
        const char *name;
        const char *value;
    } headers[32];
    int num_headers;

    // Body
    char *body;
    size_t body_len;
} qjs_response;
```

**Functions**:
```c
// Convert CivetWeb request to qjs_request
void bridge_civetweb_to_qjs(const struct mg_request_info *mg_req,
                             const char *body, size_t body_len,
                             qjs_request *out);

// Send qjs_response via CivetWeb connection
void bridge_qjs_to_civetweb(struct mg_connection *conn,
                             const qjs_response *resp);

// Free response body (allocated by JS)
void bridge_free_response(qjs_response *resp);
```

### Step 3: Embed server.js (15 min)

**Tool**: Reuse `tools/sql_to_c.sh` pattern

Create `tools/js_to_c.sh`:
```bash
#!/bin/bash
# Convert static/server.js to C byte array

input="$1"
output="$2"

echo "/* Auto-generated from $input */" > "$output"
echo "#include <stddef.h>" >> "$output"
echo "const char server_js_source[] = {" >> "$output"
xxd -i < "$input" >> "$output"
echo ", 0x00 };" >> "$output"
echo "const size_t server_js_source_len = sizeof(server_js_source) - 1;" >> "$output"
```

**Bazel rule**:
```python
genrule(
    name = "embed_server_js",
    srcs = ["//static:server.js"],
    outs = ["server_js.c"],
    cmd = "$(location //tools:js_to_c) $< $@",
    tools = ["//tools:js_to_c"],
)
```

**Usage**:
```c
// In hbf_simple/server_js.h
extern const char server_js_source[];
extern const size_t server_js_source_len;
```

### Step 4: QuickJS Runner (60 min)

**File**: `hbf_simple/qjs_runner.c|h`

**Interface**:
```c
// Initialize QuickJS runtime
int qjs_init(void);

// Process HTTP request through JS
// Returns 0 on success, -1 on error
int qjs_handle_request(const qjs_request *req, qjs_response *resp);

// Cleanup runtime
void qjs_cleanup(void);
```

**Implementation approach**:

1. **Single global context** (no pooling):
   ```c
   static JSRuntime *rt = NULL;
   static JSContext *ctx = NULL;
   ```

2. **Initialization** (called once at startup):
   - Create runtime with memory limit (64 MB)
   - Create single context
   - Register `hbf:http` module (minimal request/response bindings)
   - Evaluate `server_js_source` (loads app.get/post handlers from static/server.js)
   - Store global `handleRequest` function reference

3. **Request handling** (reuses global context):
   - Create JS request object from `qjs_request` struct
   - Call `handleRequest(request)` from server.js
   - Extract response properties (status, headers, body)
   - Populate `qjs_response` struct
   - Handle exceptions (log + return 500)

4. **Error handling**:
   - Catch stack overflow explicitly
   - Log full exception details (file, line, stack trace)
   - Return 500 Internal Server Error on JS exception

**Key questions to debug**:
- Does stack overflow happen during module load or request handling?
- Is it related to recursion in router matching?
- Does it happen on first request or subsequent requests?
- Does using the same server.js expose the same issue?

### Step 5: Minimal hbf:http Bindings (45 min)

**File**: `hbf_simple/qjs_runner.c` (inline for simplicity)

**Module exports**:
```javascript
// hbf:http module
export const Request = class {
    constructor(native_ptr) {
        this.method = ...;
        this.uri = ...;
        this.query = ...;
        this.headers = { ... };
        this.body = ...;
    }

    // Optional: param extraction
    param(key) { ... }
};

export const Response = class {
    constructor() {
        this.status = 200;
        this.headers = {};
        this._body = "";
    }

    status(code) { this.status = code; return this; }
    header(name, value) { this.headers[name] = value; return this; }
    send(body) { this._body = String(body); return this; }
    json(obj) {
        this.headers["Content-Type"] = "application/json";
        this._body = JSON.stringify(obj);
        return this;
    }
};
```

**C implementation**:
- Manually construct JS objects (no fancy macros yet)
- Use `JS_NewObject()`, `JS_SetPropertyStr()` for request
- Extract properties via `JS_GetPropertyStr()` for response
- **Keep it minimal**: No streaming, no advanced features

### Step 6: CivetWeb Request Handler (30 min)

**File**: `hbf_simple/main.c`

```c
static int http_handler(struct mg_connection *conn, void *cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);

    // Read request body
    char body[8192] = {0};
    int body_len = mg_read(conn, body, sizeof(body) - 1);
    if (body_len < 0) body_len = 0;

    // Convert to qjs_request
    qjs_request req = {0};
    bridge_civetweb_to_qjs(req_info, body, (size_t)body_len, &req);

    // Process through QuickJS
    qjs_response resp = {0};
    int rc = qjs_handle_request(&req, &resp);

    if (rc != 0) {
        // JS error - send 500
        mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
        mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
        mg_printf(conn, "JavaScript execution failed\n");
        return 500;
    }

    // Send response
    bridge_qjs_to_civetweb(conn, &resp);
    bridge_free_response(&resp);

    return resp.status_code;
}
```

### Step 7: Use Existing server.js (5 min)

**File**: `static/server.js` (already exists, shared between hbf and hbf_simple)

**No changes needed** - hbf_simple will use the same server.js as the main hbf build. This ensures we're testing with the exact same routing logic that may be causing stack overflow issues in the full build.

The existing server.js already exports `handleRequest(native_req)` and includes:
- Express-style routing with `app.get()`, `app.post()`, etc.
- Request/Response classes from `hbf:http`
- Route matching logic

**Note**: By using the same server.js, we can determine if the stack overflow is caused by the routing logic itself or by the SQLite/database integration in the full hbf build.

## Testing Strategy

### Manual Tests

1. **Build and run**:
   ```bash
   bazel build //hbf_simple:hbf_simple
   ./bazel-bin/hbf_simple/hbf_simple --port 5309 --log_level debug
   ```

2. **Basic requests**:
   ```bash
   curl http://localhost:5309/
   # Expected: "Hello from HBF Simple!"

   curl http://localhost:5309/health
   # Expected: {"status":"ok","runtime":"quickjs"}

   curl -X POST http://localhost:5309/echo -d '{"test":123}'
   # Expected: {"received":"{\"test\":123}"}
   ```

3. **Stress test**:
   ```bash
   # Run 1000 requests to see if stack overflow appears
   for i in {1..1000}; do curl -s http://localhost:5309/health > /dev/null; done
   ```

4. **Memory test**:
   ```bash
   # Check for leaks (optional, Phase 10 tool)
   valgrind --leak-check=full ./bazel-bin/hbf_simple/hbf_simple
   ```

### Debugging Focus

**If stack overflow still occurs**:
1. Add debug logging at every QuickJS call boundary
2. Print stack depth in `handleRequest`
3. Check if it's during module initialization or request handling
4. Inspect QuickJS runtime stack limit configuration
5. The issue is in QuickJS integration or server.js itself

**If stack overflow disappears**:
- The issue is related to SQLite/database integration
- May be thread safety issues with context pooling
- Database calls from JS may be exhausting stack

## Success Criteria

- [✓] Binary builds successfully with Bazel
- [✓] HTTP server responds on configured port
- [✓] `GET /` returns expected text response
- [✓] `GET /health` returns valid JSON
- [✓] `POST /echo` echoes request body
- [✓] No stack overflow errors in logs
- [✓] 1000 sequential requests complete without errors
- [✓] Response headers and status codes work correctly

## Future Enhancements (Optional)

Once debugging is complete:

1. **Add database back**: If issue is isolated to non-DB code, gradually add SQLite
2. **Context pooling**: Test if pooling vs. global context matters
3. **Static file serving**: Serve from embedded tarball (no DB)
4. **WebSocket echo**: Minimal WS handler for testing

## File Checklist

**New files**:
- [ ] `hbf_simple/BUILD.bazel`
- [ ] `hbf_simple/main.c`
- [ ] `hbf_simple/qjs_runner.h`
- [ ] `hbf_simple/qjs_runner.c`
- [ ] `hbf_simple/http_bridge.h`
- [ ] `hbf_simple/http_bridge.c`
- [ ] `hbf_simple/server_js.h` (generated)
- [ ] `tools/js_to_c.sh`

**Reused files** (no changes):
- `internal/core/log.c|h` (linked by hbf_simple)
- `static/server.js` (embedded by hbf_simple)
- `third_party/civetweb/`
- `third_party/quickjs-ng/`

**Not used by hbf_simple** (remain in repo for main hbf):
- `internal/db/` (not linked)
- `internal/henv/` (not linked)

**Modified files**:
- `BUILD.bazel` (add filegroup for tools/js_to_c.sh)

## Estimated Timeline

- Step 1 (Core structure): 30 min
- Step 2 (HTTP bridge): 20 min
- Step 3 (Embed server.js): 15 min
- Step 4 (QuickJS runner): 60 min
- Step 5 (Bindings): 45 min
- Step 6 (Request handler): 30 min
- Step 7 (Use existing server.js): 5 min
- **Total**: ~3 hours

## Key Differences from Full HBF

| Feature | Full HBF | HBF Simple |
|---------|----------|------------|
| Database | SQLite with WAL | None |
| Multi-tenancy | User pods + hashes | Single global server |
| server.js source | Database `nodes` table | Embedded C byte array |
| Authentication | JWT + sessions | None |
| Bindings | 5+ modules | `hbf:http` only |
| Context model | Pool (planned) | Single global context |
| Static files | Database injection | Not implemented |
| Routing | Full Express API (via server.js) | Same server.js |
| Build deps | internal/db, internal/henv | Neither (core + http only) |

## Debugging Insights

This minimal version will help answer:

1. **Is the stack overflow QuickJS-related or SQLite-related?**
2. **Does it happen during module load or request processing?**
3. **Is it triggered by complex routing logic or binding code?**
4. **Does context reuse vs. recreation matter?**

Once isolated, the fix can be applied to the full HBF implementation.
