# ES Module Import Playbook for HBF QuickJS

## Overview

This playbook documents the minimal steps required to enable and validate ES module (`import`/`export`) support in the HBF QuickJS runtime, focusing on the test pod (`pods/test/hbf/server.js`). The goal is to ensure that a simple ES module can be imported and used according to ESM standards, before attempting more complex integrations (e.g., third-party routers).

## Motivation

The current routing integration plan assumes ES module imports already work, but they do not. This playbook precedes router integration and focuses on making ESM imports function at all in the QuickJS context.

## Steps

### 1. Create Minimal ES Module

**File:** `pods/test/hbf/lib/esm_test.js`
```javascript
// Minimal ES module for import testing
export function hello(name) {
  return `Hello, ${name}!`;
}

export const value = 42;
```

### 2. Import Module in server.js

**File:** `pods/test/hbf/server.js`
```javascript
import { hello, value } from "./lib/esm_test.js";

app.handle = function (req, res) {
  if (req.path === "/esm-test" && req.method === "GET") {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({
      message: hello("ESM"),
      value: value
    }));
    return;
  }
  // ...existing code...
}
```

### 3. Build and Run

```bash
bazel build //:hbf_test
./bazel-bin/bin/hbf_test --port 5309 --dev
```

### 4. Test Import

```bash
curl http://localhost:5309/esm-test
# Expected: {"message":"Hello, ESM!","value":42}
```

### 5. Debugging

If import fails:
- Check QuickJS logs for module loader errors
- Ensure `esm_test.js` is present in the embedded database
- Review `hbf/qjs/module_loader.c` for ESM loader implementation
- Compare with ESM spec: https://tc39.es/ecma262/#sec-modules

## Success Criteria
- ✅ Server builds and runs
- ✅ `/esm-test` route returns expected JSON
- ✅ No QuickJS import errors
- ✅ Module loader follows ESM semantics (static analysis, live bindings, etc.)

## Next Steps
- Once minimal ESM import works, proceed to more complex modules (e.g., find-my-way router)
- Document loader implementation details in `js_import_impl.md`
- Update integration plans to assume working ESM imports

## References
- [ECMAScript Modules Spec](https://tc39.es/ecma262/#sec-modules)
- [QuickJS Module Loader](https://bellard.org/quickjs/)
- `js_import_impl.md` (HBF loader implementation)

## Implementation Notes (2025-11-07)

### Loader Implementation
- The ES module loader is implemented in C in `hbf/qjs/module_loader.c`.
- It uses a custom function `db_get_module_source` to fetch module source from the pod's SQLite overlay filesystem.
- The loader expects module paths as they appear in the database (e.g., `hbf/lib/esm_test.js`).
- Path normalization is handled by the loader; the import path in JS must match the database path.
- The loader is registered with QuickJS using `JS_SetModuleLoaderFunc`.
- After calling any async JS (e.g., dynamic import), the C host must run the QuickJS job queue (`JS_ExecutePendingJob`) until all jobs are resolved, or promises will not complete.

### References
- QuickJS module loader: https://bellard.org/quickjs/
- QuickJS-Emscripten async job handling: https://git.dailysh.it/public/malta-slides/raw/commit/2b1994622ae3b615d7cd3e84369faa6ace95c73f/node_modules/@tootallnate/quickjs-emscripten/c/interface.c
- Pod build/embedding: `.claude/skills/add-pod/SKILL.md`

### Bugs Fixed
- Ensured integration test uses the test pod binary (`hbf_test`) and not the base binary.
- Added QuickJS job queue execution after each request to resolve promises and dynamic imports.
- Fixed variable shadowing in C handler when running job queue.
- Verified module presence in the pod database using SQLite queries.
- Corrected import path in JS to match database (`hbf/lib/esm_test.js`).

### Current Issue & Reproduction
- The `/esm-test` route fails with `ReferenceError: could not load module 'hbf/lib/esm_test.js'` (HTTP 500).
- The module is present in the database at `hbf/lib/esm_test.js` (verified via SQLite).
- The loader and import path appear correct, but QuickJS cannot resolve the module at runtime.
- Possible causes: loader path normalization, QuickJS configuration, or subtle database/embedding issue.

#### To Reproduce:
1. Build and run the test pod:
   ```bash
   rm -f ./hbf.db ./hbf.db-shm ./hbf.db-wal
   bazel build //:hbf_test
   bazel run //:hbf_test
   ```
2. Query the database for the module:
   ```bash
   sqlite3 ./hbf.db "SELECT path FROM latest_files_meta WHERE path LIKE '%esm_test.js%'"
   # Should show: hbf/lib/esm_test.js
   ```
3. Run the integration test:
   ```bash
   tools/integration_test.sh
   # /esm-test route returns 500 error, module not found
   ```

#### Resolution (2025-11-08)
✅ **ES module imports are now working!**

**Implementation:**
1. **Created module_loader.c** - C99-compliant ES module loader that:
   - Fetches module source from `latest_files` table in the versioned filesystem
   - Compiles modules using `JS_Eval` with `JS_EVAL_TYPE_MODULE` flag
   - Handles path normalization (currently absolute paths only)
   - Proper error handling with HBF logging

2. **Created module_loader.h** - Public API for initializing the loader

3. **Updated BUILD.bazel** - Added module_loader.c/h to engine library

4. **Integrated into engine.c** - Loader initialized after JSRuntime creation:
   ```c
   hbf_qjs_module_loader_init(rt, ctx->db);
   ```

**Testing:**
- Integration test passes: `Testing GET /esm-test (ESM loader)... PASS`
- Dynamic imports work correctly in server.js
- Module exports accessible as expected

**Files Changed:**
- `hbf/qjs/module_loader.c` (new)
- `hbf/qjs/module_loader.h` (new)
- `hbf/qjs/BUILD.bazel` (added module_loader to engine)
- `hbf/qjs/engine.c` (initialize loader)
