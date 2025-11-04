# JavaScript Module Import Implementation Plan

## Analysis of Provided Code

The provided C code demonstrates a standalone QuickJS ES module loader with filesystem-based loading. Key components:

**What it does well:**
- ✅ Uses `JS_SetModuleLoaderFunc()` to register custom module loader
- ✅ Evaluates entry module with `JS_EVAL_TYPE_MODULE` flag
- ✅ Implements path resolution for relative (`./`, `../`) and absolute (`/`) paths
- ✅ Compiles modules with `JS_EVAL_FLAG_COMPILE_ONLY` before returning `JSModuleDef`
- ✅ Proper error handling with `JS_ThrowReferenceError`

**What needs adaptation for HBF:**
- ❌ Uses `fopen`/`fread` for disk I/O → Must use `overlay_fs_read_file()` for SQLAR
- ❌ Uses `realpath()` and `dirname()` → Must resolve paths in virtual filesystem context
- ❌ Standalone program → Must integrate into existing `hbf/qjs/engine.c` and `hbf/qjs/handler.c`
- ❌ No context about base paths → Must understand `hbf/` and `static/` virtual directories
- ❌ Memory management doesn't fit HBF patterns → Must follow HBF's allocation patterns

## Comparison to js_import_plan.md

| Requirement | Example Code | HBF Integration Needed |
|-------------|--------------|------------------------|
| ES Module evaluation | ✅ `JS_EVAL_TYPE_MODULE` | Adapt entry point in `engine.c` |
| Module loader registration | ✅ `JS_SetModuleLoaderFunc()` | Add to engine initialization |
| Relative path resolution | ✅ `./` and `../` | Adapt for virtual FS paths |
| Absolute path support | ✅ `/` paths | Map to `static/` in SQLAR |
| Read from SQLAR | ❌ Uses `fopen` | Use `overlay_fs_read_file()` |
| Bare specifiers | ❌ Not supported | OK per plan (not needed server-side) |
| Module-global scope | ⚠️ Not addressed | Need `globalThis.app` pattern |

**Verdict:** The example provides a solid foundation for the module loader logic, but requires significant adaptation for HBF's SQLAR-backed virtual filesystem architecture.

## Implementation Strategy

We'll integrate ES module support into HBF by modifying existing QuickJS engine code and adding a module loader that reads from the embedded SQLAR database.

### Architecture Overview

```
Entry point (hbf/server.js)
    ↓ [JS_EvalFile with JS_EVAL_TYPE_MODULE]
QuickJS Runtime
    ↓ [import "./modules/foo.js"]
Module Loader (new: hbf/qjs/module_loader.c)
    ↓ [resolve path relative to base]
Overlay FS (hbf/db/overlay_fs.c)
    ↓ [overlay_fs_read_file()]
SQLAR Database (embedded)
```

### File Changes Required

1. **New file: `hbf/qjs/module_loader.c`** - Module loader implementation
2. **New file: `hbf/qjs/module_loader.h`** - Module loader header
3. **Modify: `hbf/qjs/engine.c`** - Switch to module evaluation, register loader
4. **Modify: `hbf/qjs/engine.h`** - Expose module loader init function
5. **Modify: `hbf/qjs/BUILD.bazel`** - Add module_loader sources
6. **Modify: `pods/base/hbf/server.js`** - Use `globalThis.app` pattern
7. **New test: `hbf/qjs:module_loader_test`** - Unit tests for path resolution
8. **New test files: `pods/test/hbf/modules/*.js`** - Test modules

### Detailed Implementation Steps

## Step 1: Add Module Loader Implementation

**File: `hbf/qjs/module_loader.h`**

```c
/* SPDX-License-Identifier: MIT */
/*
 * QuickJS ES Module Loader for HBF
 * Resolves module imports from SQLAR-backed virtual filesystem
 */

#ifndef HBF_QJS_MODULE_LOADER_H
#define HBF_QJS_MODULE_LOADER_H

#include "quickjs.h"
#include "../db/db.h"

/*
 * Initialize module loader for a QuickJS runtime
 * Must be called after runtime creation, before context creation
 */
void hbf_qjs_module_loader_init(JSRuntime *rt, struct hbf_db *db);

/*
 * Module loader function (internal, exposed for testing)
 * Resolves module_name and loads from SQLAR via overlay_fs
 */
JSModuleDef *hbf_qjs_module_loader(JSContext *ctx,
                                    const char *module_name,
                                    void *opaque);

#endif /* HBF_QJS_MODULE_LOADER_H */
```

**File: `hbf/qjs/module_loader.c`**

```c
/* SPDX-License-Identifier: MIT */

#include "module_loader.h"
#include "../db/overlay_fs.h"
#include "../shell/log.h"
#include <string.h>
#include <stdlib.h>

/* Maximum path length for module resolution */
#define MAX_MODULE_PATH 1024

/*
 * Normalize a path by resolving "." and ".." components
 * Input: relative or absolute path with possible . and .. components
 * Output: normalized path without . or .. (modifies buf in-place)
 * Returns 0 on success, -1 on error (path traversal outside root)
 */
static int normalize_path(char *buf, size_t buf_size)
{
	char *segments[64];
	int segment_count = 0;
	char *token;
	char *rest = buf;
	char temp[MAX_MODULE_PATH];

	/* Tokenize by '/' */
	while ((token = strtok_r(rest, "/", &rest))) {
		if (strcmp(token, ".") == 0) {
			/* Skip "." */
			continue;
		} else if (strcmp(token, "..") == 0) {
			/* Go up one level */
			if (segment_count > 0) {
				segment_count--;
			} else {
				/* Trying to go above root */
				return -1;
			}
		} else {
			/* Regular segment */
			if (segment_count >= 64) {
				return -1; /* Too many segments */
			}
			segments[segment_count++] = strdup(token);
		}
	}

	/* Rebuild path */
	temp[0] = '\0';
	for (int i = 0; i < segment_count; i++) {
		if (i > 0) {
			strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
		}
		strncat(temp, segments[i], sizeof(temp) - strlen(temp) - 1);
		free(segments[i]);
	}

	strncpy(buf, temp, buf_size);
	return 0;
}

/*
 * Resolve module specifier to virtual filesystem path
 * Handles:
 *   - Relative: "./foo.js" or "../bar.js" (relative to base_name)
 *   - Absolute: "/static/vendor/lib.js" (maps to "static/...")
 *   - Bare: "lodash" (NOT SUPPORTED, returns error)
 */
static int resolve_module_path(const char *base_name,
                                const char *module_name,
                                char *resolved,
                                size_t resolved_size)
{
	char temp[MAX_MODULE_PATH];

	/* Case 1: Relative path (./ or ../) */
	if (strncmp(module_name, "./", 2) == 0 ||
	    strncmp(module_name, "../", 3) == 0) {
		/* Extract directory of base_name */
		strncpy(temp, base_name, sizeof(temp));
		char *last_slash = strrchr(temp, '/');
		if (last_slash) {
			*last_slash = '\0'; /* Trim to directory */
		} else {
			temp[0] = '\0'; /* No directory, use empty */
		}

		/* Append module_name */
		if (temp[0] != '\0') {
			strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
		}
		strncat(temp, module_name, sizeof(temp) - strlen(temp) - 1);

		/* Normalize */
		if (normalize_path(temp, sizeof(temp)) < 0) {
			return -1;
		}

		strncpy(resolved, temp, resolved_size);
		return 0;
	}

	/* Case 2: Absolute path (/) */
	if (module_name[0] == '/') {
		/* Strip leading '/' and use as-is in virtual FS */
		strncpy(temp, module_name + 1, sizeof(temp));
		if (normalize_path(temp, sizeof(temp)) < 0) {
			return -1;
		}
		strncpy(resolved, temp, resolved_size);
		return 0;
	}

	/* Case 3: Bare specifier (not supported) */
	return -1;
}

/*
 * Module loader function called by QuickJS
 * opaque is a pointer to struct hbf_db
 */
JSModuleDef *hbf_qjs_module_loader(JSContext *ctx,
                                    const char *module_name,
                                    void *opaque)
{
	struct hbf_db *db = (struct hbf_db *)opaque;
	char resolved[MAX_MODULE_PATH];
	char *module_source = NULL;
	size_t module_len = 0;
	JSModuleDef *module = NULL;

	/* Get the importing module's name (base for relative resolution) */
	JSAtom base_atom = JS_GetScriptOrModuleName(ctx, 0);
	const char *base_name = JS_AtomToCString(ctx, base_atom);
	JS_FreeAtom(ctx, base_atom);

	if (!base_name) {
		base_name = "hbf/server.js"; /* Default for entry module */
	}

	/* Resolve module path */
	if (resolve_module_path(base_name, module_name, resolved,
	                        sizeof(resolved)) < 0) {
		JS_ThrowReferenceError(ctx, "Cannot resolve module '%s' from '%s'",
		                       module_name, base_name);
		goto cleanup;
	}

	hbf_log_debug("Module loader: resolving '%s' from '%s' -> '%s'",
	              module_name, base_name, resolved);

	/* Read module source from SQLAR */
	if (overlay_fs_read_file(db->db, resolved, &module_source,
	                          &module_len) != 0) {
		JS_ThrowReferenceError(ctx, "Cannot load module '%s' (resolved: %s)",
		                       module_name, resolved);
		goto cleanup;
	}

	/* Compile module */
	JSValue func_val = JS_Eval(ctx, module_source, module_len, resolved,
	                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

	if (JS_IsException(func_val)) {
		goto cleanup;
	}

	/* Extract JSModuleDef from compiled module */
	module = (JSModuleDef *)JS_VALUE_GET_PTR(func_val);
	JS_FreeValue(ctx, func_val);

cleanup:
	if (base_name && strcmp(base_name, "hbf/server.js") != 0) {
		JS_FreeCString(ctx, base_name);
	}
	free(module_source);
	return module;
}

/*
 * Initialize module loader for a runtime
 */
void hbf_qjs_module_loader_init(JSRuntime *rt, struct hbf_db *db)
{
	JS_SetModuleLoaderFunc(rt, NULL, hbf_qjs_module_loader, db);
	hbf_log_debug("QuickJS module loader initialized");
}
```

**Key Design Decisions:**

1. **Path Resolution**: Implements relative (`./`, `../`) and absolute (`/`) path resolution without `realpath()` syscalls. Uses pure string manipulation and normalization.

2. **Base Path Context**: Uses `JS_GetScriptOrModuleName()` to get the importing module's path for relative resolution. Falls back to `"hbf/server.js"` for the entry module.

3. **Virtual FS Integration**: Uses `overlay_fs_read_file()` to load module sources from SQLAR, consistent with HBF's architecture.

4. **Error Handling**: Throws JavaScript `ReferenceError` for module not found, consistent with ECMAScript spec.

5. **No Bare Specifiers**: Explicitly rejects bare specifiers like `"lodash"` per plan (server-side uses relative imports only).

## Step 2: Modify QuickJS Engine

**File: `hbf/qjs/engine.c`**

Find the entry point evaluation (likely in `hbf_qjs_engine_load_script` or similar) and change:

```c
// OLD: Global script evaluation
JSValue result = JS_Eval(ctx, source, source_len, "hbf/server.js",
                         JS_EVAL_TYPE_GLOBAL);

// NEW: ES Module evaluation
JSValue result = JS_Eval(ctx, source, source_len, "hbf/server.js",
                         JS_EVAL_TYPE_MODULE);
```

Add module loader initialization in engine creation:

```c
#include "module_loader.h"

struct hbf_qjs_engine *hbf_qjs_engine_create(struct hbf_db *db)
{
	struct hbf_qjs_engine *engine = calloc(1, sizeof(*engine));
	if (!engine) {
		return NULL;
	}

	engine->rt = JS_NewRuntime();
	if (!engine->rt) {
		free(engine);
		return NULL;
	}

	/* Initialize module loader BEFORE creating context */
	hbf_qjs_module_loader_init(engine->rt, db);

	engine->ctx = JS_NewContext(engine->rt);
	if (!engine->ctx) {
		JS_FreeRuntime(engine->rt);
		free(engine);
		return NULL;
	}

	/* Rest of initialization... */
	return engine;
}
```

**Critical Change:** Module loader must be registered on the runtime BEFORE creating the context, as per QuickJS API requirements.

## Step 3: Update BUILD.bazel

**File: `hbf/qjs/BUILD.bazel`**

Add module_loader sources to the qjs library:

```python
cc_library(
    name = "qjs",
    srcs = [
        "bindings.c",
        "console.c",
        "engine.c",
        "module_loader.c",  # NEW
        # ... other sources
    ],
    hdrs = [
        "bindings.h",
        "console.h",
        "engine.h",
        "module_loader.h",  # NEW
        # ... other headers
    ],
    # ... rest of rule
)
```

## Step 4: Update Pod JavaScript to Use globalThis

**File: `pods/base/hbf/server.js`**

Change from global assignment to `globalThis`:

```javascript
// OLD (implicit global in non-strict mode):
// const app = { handle(req, res) { ... } };

// NEW (explicit globalThis for module scope):
const app = {
	handle(req, res) {
		// Handle request
		const path = req.url;

		if (path === '/') {
			res.status(200);
			res.header('Content-Type', 'text/html');
			res.send('<h1>HBF Base Pod</h1>');
			return;
		}

		res.status(404);
		res.send('Not found');
	}
};

// Export to global scope for C handler
globalThis.app = app;
```

**Rationale:** ES modules run in strict mode and have their own scope. Variables declared with `const`/`let`/`var` are module-local. To make `app` accessible from C code via `JS_GetGlobalObject()`, it must be explicitly assigned to `globalThis`.

## Step 5: Create Test Modules

**File: `pods/test/hbf/modules/square.js`**

```javascript
// Simple module for testing imports
export const name = "square";

export function draw(size) {
	return {
		shape: "square",
		size: size,
		area: size * size
	};
}
```

**File: `pods/test/hbf/server.js`**

```javascript
import { name, draw } from "./modules/square.js";

const app = {
	handle(req, res) {
		if (req.url === '/test-import') {
			const result = draw(10);
			res.status(200);
			res.header('Content-Type', 'application/json');
			res.send(JSON.stringify({
				module: name,
				result: result
			}));
			return;
		}

		res.status(404);
		res.send('Not found');
	}
};

globalThis.app = app;
```

## Step 6: Add Unit Tests

**File: `hbf/qjs/module_loader_test.c`**

```c
/* SPDX-License-Identifier: MIT */

#include "module_loader.h"
#include "../db/db.h"
#include "../db/overlay_fs.h"
#include <assert.h>
#include <string.h>

/* Expose internal function for testing */
extern int resolve_module_path(const char *base_name,
                                const char *module_name,
                                char *resolved,
                                size_t resolved_size);

void test_resolve_relative(void)
{
	char resolved[1024];
	int ret;

	/* Test: ./foo.js from hbf/server.js */
	ret = resolve_module_path("hbf/server.js", "./foo.js",
	                          resolved, sizeof(resolved));
	assert(ret == 0);
	assert(strcmp(resolved, "hbf/foo.js") == 0);

	/* Test: ../bar.js from hbf/modules/square.js */
	ret = resolve_module_path("hbf/modules/square.js", "../bar.js",
	                          resolved, sizeof(resolved));
	assert(ret == 0);
	assert(strcmp(resolved, "hbf/bar.js") == 0);

	/* Test: ./nested/deep.js from hbf/server.js */
	ret = resolve_module_path("hbf/server.js", "./nested/deep.js",
	                          resolved, sizeof(resolved));
	assert(ret == 0);
	assert(strcmp(resolved, "hbf/nested/deep.js") == 0);
}

void test_resolve_absolute(void)
{
	char resolved[1024];
	int ret;

	/* Test: /static/vendor/lib.js */
	ret = resolve_module_path("hbf/server.js", "/static/vendor/lib.js",
	                          resolved, sizeof(resolved));
	assert(ret == 0);
	assert(strcmp(resolved, "static/vendor/lib.js") == 0);
}

void test_resolve_path_traversal(void)
{
	char resolved[1024];
	int ret;

	/* Test: ../../escape.js from hbf/server.js (should fail) */
	ret = resolve_module_path("hbf/server.js", "../../escape.js",
	                          resolved, sizeof(resolved));
	assert(ret == -1);
}

void test_resolve_bare_specifier(void)
{
	char resolved[1024];
	int ret;

	/* Test: bare specifier should fail */
	ret = resolve_module_path("hbf/server.js", "lodash",
	                          resolved, sizeof(resolved));
	assert(ret == -1);
}

int main(void)
{
	test_resolve_relative();
	test_resolve_absolute();
	test_resolve_path_traversal();
	test_resolve_bare_specifier();

	printf("All module_loader tests passed\n");
	return 0;
}
```

**Add to `hbf/qjs/BUILD.bazel`:**

```python
cc_test(
    name = "module_loader_test",
    srcs = [
        "module_loader_test.c",
        "module_loader.c",  # Include implementation for internal function access
    ],
    deps = [
        "//hbf/db",
        "//hbf/shell:log",
        "//third_party/quickjs",
    ],
)
```

## Step 7: Add Integration Test

**File: `hbf/qjs/module_import_integration_test.c`**

This test requires embedding a test database with modules:

```c
/* SPDX-License-Identifier: MIT */

#include "engine.h"
#include "module_loader.h"
#include "../db/db.h"
#include <assert.h>

/* Embedded test database symbols */
extern const unsigned char fs_db_data[];
extern const unsigned long fs_db_len;

void test_import_module(void)
{
	struct hbf_db *db = hbf_db_open_from_memory(fs_db_data, fs_db_len);
	assert(db != NULL);

	struct hbf_qjs_engine *engine = hbf_qjs_engine_create(db);
	assert(engine != NULL);

	/* Entry module should import ./modules/square.js and work */
	int ret = hbf_qjs_engine_run_request(engine, "/test-import", NULL);
	assert(ret == 0);

	hbf_qjs_engine_destroy(engine);
	hbf_db_close(db);
}

int main(void)
{
	test_import_module();
	printf("Module import integration test passed\n");
	return 0;
}
```

This test depends on the `test` pod embedded library and validates end-to-end module loading.

## Step 8: Update MIME Type Support

**File: `hbf/http/server.c`**

Add `.mjs` and `.map` to the MIME type table:

```c
static const struct {
	const char *ext;
	const char *mime;
} mime_types[] = {
	{ ".html", "text/html" },
	{ ".css", "text/css" },
	{ ".js", "application/javascript" },
	{ ".mjs", "application/javascript" },  /* NEW: ES module files */
	{ ".json", "application/json" },
	{ ".map", "application/json" },        /* NEW: Source maps */
	{ ".png", "image/png" },
	{ ".jpg", "image/jpeg" },
	{ ".svg", "image/svg+xml" },
	{ ".wasm", "application/wasm" },
	{ NULL, NULL }
};
```

## Step 9: NPM Package Vendoring Tooling

This section implements the browser-side package vendoring system that allows importing npm packages via import maps without CDNs.

### Overview

The vendoring tool:
1. Downloads npm packages with `npm pack` (no node_modules)
2. Extracts to `third_party/npm/<package>@<version>/`
3. Bundles for browser with esbuild → single ESM file
4. Generates/updates import map in `pods/<pod>/static/importmap.json`
5. All artifacts committed to git for reproducible builds

### File: `tools/npm_vendor.sh`

```bash
#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Vendor npm packages for HBF pods
# Usage: tools/npm_vendor.sh --pod <base|test> <package@version> [package@version...]
#
# Example:
#   tools/npm_vendor.sh --pod base @codemirror/view@6.25.0 lodash-es@4.17.21

set -euo pipefail

# Parse arguments
POD=""
PACKAGES=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --pod)
            POD="$2"
            shift 2
            ;;
        *)
            PACKAGES+=("$1")
            shift
            ;;
    esac
done

# Validate inputs
if [[ -z "$POD" ]]; then
    echo "Error: --pod required"
    echo "Usage: $0 --pod <base|test> <package@version> [...]"
    exit 1
fi

if [[ ${#PACKAGES[@]} -eq 0 ]]; then
    echo "Error: at least one package required"
    echo "Usage: $0 --pod <base|test> <package@version> [...]"
    exit 1
fi

# Paths
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POD_DIR="$REPO_ROOT/pods/$POD"
VENDOR_DIR="$POD_DIR/static/vendor"
VENDOR_ESM_DIR="$VENDOR_DIR/esm"
THIRD_PARTY_NPM="$REPO_ROOT/third_party/npm"
IMPORTMAP_FILE="$POD_DIR/static/importmap.json"

# Validate pod exists
if [[ ! -d "$POD_DIR" ]]; then
    echo "Error: Pod '$POD' not found at $POD_DIR"
    exit 1
fi

# Create directories
mkdir -p "$VENDOR_ESM_DIR"
mkdir -p "$THIRD_PARTY_NPM"

# Check for required tools
for cmd in npm npx jq; do
    if ! command -v "$cmd" &> /dev/null; then
        echo "Error: $cmd is required but not installed"
        exit 1
    fi
done

# Initialize or load import map
if [[ -f "$IMPORTMAP_FILE" ]]; then
    IMPORT_MAP=$(cat "$IMPORTMAP_FILE")
else
    IMPORT_MAP='{"imports":{}}'
fi

# Process each package
for PACKAGE_SPEC in "${PACKAGES[@]}"; do
    echo "Processing $PACKAGE_SPEC..."

    # Parse package@version
    if [[ "$PACKAGE_SPEC" =~ ^(@?[^@]+)@(.+)$ ]]; then
        PACKAGE_NAME="${BASH_REMATCH[1]}"
        PACKAGE_VERSION="${BASH_REMATCH[2]}"
    else
        echo "Error: Invalid package spec '$PACKAGE_SPEC' (expected format: package@version)"
        exit 1
    fi

    # Safe name for filesystem (replace / and @ with __)
    SAFE_NAME="${PACKAGE_NAME//@/__}"
    SAFE_NAME="${SAFE_NAME//\//__}"

    # Package directory in third_party
    PACKAGE_DIR="$THIRD_PARTY_NPM/${SAFE_NAME}@${PACKAGE_VERSION}"
    PACKAGE_PKG_DIR="$PACKAGE_DIR/pkg"

    # Check if already vendored
    if [[ -d "$PACKAGE_PKG_DIR" ]]; then
        echo "  Already vendored to $PACKAGE_DIR, skipping download"
    else
        echo "  Downloading $PACKAGE_SPEC..."

        # Create temp directory
        TEMP_DIR=$(mktemp -d)
        trap "rm -rf $TEMP_DIR" EXIT

        # Download with npm pack
        cd "$TEMP_DIR"
        npm pack "$PACKAGE_SPEC" --silent

        # Extract tarball
        TARBALL=$(ls *.tgz)
        mkdir -p "$PACKAGE_PKG_DIR"
        tar -xzf "$TARBALL" -C "$PACKAGE_PKG_DIR" --strip-components=1

        echo "  Extracted to $PACKAGE_PKG_DIR"
    fi

    # Read package.json to find entry point
    PACKAGE_JSON="$PACKAGE_PKG_DIR/package.json"

    # Determine ESM entry point (prefer "module" field, fallback to "main")
    if jq -e '.module' "$PACKAGE_JSON" > /dev/null 2>&1; then
        ENTRY=$(jq -r '.module' "$PACKAGE_JSON")
    elif jq -e '.exports["."].import' "$PACKAGE_JSON" > /dev/null 2>&1; then
        ENTRY=$(jq -r '.exports["."].import' "$PACKAGE_JSON")
    elif jq -e '.main' "$PACKAGE_JSON" > /dev/null 2>&1; then
        ENTRY=$(jq -r '.main' "$PACKAGE_JSON")
    else
        ENTRY="index.js"
    fi

    # Resolve entry path
    ENTRY_PATH="$PACKAGE_PKG_DIR/$ENTRY"

    if [[ ! -f "$ENTRY_PATH" ]]; then
        echo "  Warning: Entry point $ENTRY not found, trying index.js"
        ENTRY_PATH="$PACKAGE_PKG_DIR/index.js"
    fi

    if [[ ! -f "$ENTRY_PATH" ]]; then
        echo "  Error: Cannot find entry point for $PACKAGE_NAME"
        exit 1
    fi

    echo "  Entry point: $ENTRY"

    # Bundle with esbuild
    OUTPUT_FILE="$VENDOR_ESM_DIR/${SAFE_NAME}.js"
    SOURCEMAP_FILE="$OUTPUT_FILE.map"

    echo "  Bundling to $OUTPUT_FILE..."

    npx esbuild "$ENTRY_PATH" \
        --bundle \
        --format=esm \
        --outfile="$OUTPUT_FILE" \
        --sourcemap \
        --platform=browser \
        --target=es2020

    echo "  ✓ Bundled: $OUTPUT_FILE"
    echo "  ✓ Sourcemap: $SOURCEMAP_FILE"

    # Copy LICENSE
    if [[ -f "$PACKAGE_PKG_DIR/LICENSE" ]]; then
        cp "$PACKAGE_PKG_DIR/LICENSE" "$PACKAGE_DIR/LICENSE"
    elif [[ -f "$PACKAGE_PKG_DIR/LICENSE.md" ]]; then
        cp "$PACKAGE_PKG_DIR/LICENSE.md" "$PACKAGE_DIR/LICENSE.md"
    else
        echo "  Warning: No LICENSE file found for $PACKAGE_NAME"
    fi

    # Update import map
    IMPORT_PATH="/static/vendor/esm/${SAFE_NAME}.js"
    IMPORT_MAP=$(echo "$IMPORT_MAP" | jq --arg key "$PACKAGE_NAME" --arg val "$IMPORT_PATH" \
        '.imports[$key] = $val')

    echo "  ✓ Import map: \"$PACKAGE_NAME\" -> \"$IMPORT_PATH\""
    echo ""
done

# Write updated import map
echo "$IMPORT_MAP" | jq '.' > "$IMPORTMAP_FILE"
echo "✓ Updated import map: $IMPORTMAP_FILE"

# Generate vendor manifest
VENDOR_MANIFEST="$VENDOR_DIR/vendor.json"
jq -n --argjson imports "$(echo "$IMPORT_MAP" | jq '.imports')" \
    '{
        "generated": (now | strftime("%Y-%m-%dT%H:%M:%SZ")),
        "packages": $imports
    }' > "$VENDOR_MANIFEST"

echo "✓ Generated vendor manifest: $VENDOR_MANIFEST"
echo ""
echo "Done! Vendored ${#PACKAGES[@]} package(s) for pod '$POD'"
echo ""
echo "Next steps:"
echo "  1. Review generated files in $VENDOR_DIR"
echo "  2. Commit changes to git"
echo "  3. Use in HTML:"
echo "     <script type=\"importmap\" src=\"/static/importmap.json\"></script>"
echo "     <script type=\"module\">"
echo "       import { ... } from \"$PACKAGE_NAME\";"
echo "     </script>"
```

**Make executable:**
```bash
chmod +x tools/npm_vendor.sh
```

### File: `tools/npm_vendor_test.sh`

Test script to validate vendoring:

```bash
#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Test npm vendoring tool

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "Testing npm_vendor.sh..."

# Test 1: Vendor a simple package
echo "Test 1: Vendor lodash-es@4.17.21 to test pod"
tools/npm_vendor.sh --pod test lodash-es@4.17.21

# Verify outputs
if [[ ! -f "pods/test/static/vendor/esm/lodash-es.js" ]]; then
    echo "FAIL: Bundle not created"
    exit 1
fi

if [[ ! -f "pods/test/static/importmap.json" ]]; then
    echo "FAIL: Import map not created"
    exit 1
fi

if ! grep -q "lodash-es" "pods/test/static/importmap.json"; then
    echo "FAIL: Import map doesn't contain lodash-es"
    exit 1
fi

echo "✓ Test 1 passed"

# Test 2: Vendor scoped package
echo "Test 2: Vendor @codemirror/view@6.25.0 to test pod"
tools/npm_vendor.sh --pod test @codemirror/view@6.25.0

if [[ ! -f "pods/test/static/vendor/esm/__codemirror__view.js" ]]; then
    echo "FAIL: Scoped package bundle not created"
    exit 1
fi

if ! grep -q "@codemirror/view" "pods/test/static/importmap.json"; then
    echo "FAIL: Import map doesn't contain @codemirror/view"
    exit 1
fi

echo "✓ Test 2 passed"

# Test 3: Verify import map structure
echo "Test 3: Verify import map structure"
IMPORTS=$(jq '.imports' pods/test/static/importmap.json)

if [[ "$IMPORTS" == "null" ]]; then
    echo "FAIL: Import map missing .imports"
    exit 1
fi

echo "✓ Test 3 passed"

echo ""
echo "All npm_vendor tests passed!"
```

### Example Usage

**Vendor a package:**
```bash
# Vendor CodeMirror for base pod
tools/npm_vendor.sh --pod base @codemirror/view@6.25.0

# Vendor multiple packages at once
tools/npm_vendor.sh --pod base \
    lodash-es@4.17.21 \
    @codemirror/view@6.25.0 \
    @codemirror/state@6.4.0
```

**Use in browser HTML:**

`pods/base/static/dev/index.html`:
```html
<!DOCTYPE html>
<html>
<head>
    <title>HBF Dev</title>
    <!-- Import map MUST come before any module scripts -->
    <script type="importmap" src="/static/importmap.json"></script>
</head>
<body>
    <div id="editor"></div>

    <script type="module">
        import { EditorView, keymap } from "@codemirror/view";
        import { defaultKeymap } from "@codemirror/commands";

        // Use the imported modules
        const view = new EditorView({
            parent: document.getElementById('editor'),
            extensions: [keymap.of(defaultKeymap)]
        });
    </script>
</body>
</html>
```

### Generated Files Structure

After running vendoring:

```
third_party/npm/
  lodash-es@4.17.21/
    pkg/                    # Extracted npm package
      package.json
      index.js
      ...
    LICENSE                 # Copied from package
  __codemirror__view@6.25.0/
    pkg/
      package.json
      ...
    LICENSE

pods/base/
  static/
    vendor/
      esm/
        lodash-es.js        # Bundled ESM
        lodash-es.js.map    # Source map
        __codemirror__view.js
        __codemirror__view.js.map
      vendor.json           # Manifest (for audit)
    importmap.json          # Import map for browser
```

### Import Map Format

`pods/base/static/importmap.json`:
```json
{
  "imports": {
    "lodash-es": "/static/vendor/esm/lodash-es.js",
    "@codemirror/view": "/static/vendor/esm/__codemirror__view.js",
    "@codemirror/state": "/static/vendor/esm/__codemirror__state.js"
  }
}
```

### Vendor Manifest Format

`pods/base/static/vendor/vendor.json`:
```json
{
  "generated": "2025-11-04T10:30:00Z",
  "packages": {
    "lodash-es": "/static/vendor/esm/lodash-es.js",
    "@codemirror/view": "/static/vendor/esm/__codemirror__view.js"
  }
}
```

### Integration with Pod Build

No Bazel changes needed! The existing `pod_db` rule already globs `static/**/*`:

```python
# pods/base/BUILD.bazel (unchanged)
sqlar(
    name = "pod_db",
    srcs = glob([
        "hbf/**/*",
        "static/**/*",  # Picks up vendor/**, importmap.json automatically
    ]),
)
```

The vendored files are automatically included in the pod SQLAR and served by the HTTP server.

### Reproducibility

All vendored artifacts are committed to git:
- Source packages in `third_party/npm/`
- Bundled ESM in `pods/*/static/vendor/esm/`
- Import maps in `pods/*/static/importmap.json`

This ensures:
- ✅ Offline builds (no npm registry required)
- ✅ Reproducible builds (same inputs → same outputs)
- ✅ Version pinning (explicit package@version)
- ✅ License compliance (LICENSE files tracked)

### CI/CD Integration

Add vendoring validation to CI:

`.github/workflows/pr.yml`:
```yaml
- name: Validate vendored packages
  run: |
    # Ensure no uncommitted vendored files
    git status --porcelain pods/*/static/vendor/ third_party/npm/
    if [[ -n "$(git status --porcelain pods/*/static/vendor/ third_party/npm/)" ]]; then
      echo "Error: Uncommitted vendored files detected"
      exit 1
    fi

    # Verify import maps are valid JSON
    for map in pods/*/static/importmap.json; do
      jq empty "$map" || exit 1
    done
```

## Implementation Phases

### Phase 1: Core Module Loader (Week 1)
- [ ] Implement `hbf/qjs/module_loader.{c,h}`
- [ ] Add unit tests for path resolution (`module_loader_test`)
- [ ] Update BUILD.bazel for new sources
- [ ] Verify tests pass: `bazel test //hbf/qjs:module_loader_test`

### Phase 2: Engine Integration (Week 1)
- [ ] Modify `hbf/qjs/engine.c` to use `JS_EVAL_TYPE_MODULE`
- [ ] Register module loader in engine creation
- [ ] Update `pods/base/hbf/server.js` to use `globalThis.app`
- [ ] Verify base pod still works: `bazel run //:hbf`

### Phase 3: Test Pod & Validation (Week 2)
- [ ] Create `pods/test/hbf/modules/square.js`
- [ ] Update `pods/test/hbf/server.js` to import square module
- [ ] Add integration test with embedded test pod
- [ ] Verify end-to-end: `bazel test //hbf/qjs:module_import_integration_test`

### Phase 4: NPM Vendoring Tool (Week 2)
- [ ] Implement `tools/npm_vendor.sh` script
- [ ] Create `tools/npm_vendor_test.sh` validation script
- [ ] Test vendoring lodash-es: `tools/npm_vendor.sh --pod test lodash-es@4.17.21`
- [ ] Test vendoring scoped package: `tools/npm_vendor.sh --pod test @codemirror/view@6.25.0`
- [ ] Verify generated files: import map, bundled ESM, source maps
- [ ] Run validation: `bash tools/npm_vendor_test.sh`

### Phase 5: MIME Support & Browser Integration (Week 2-3)
- [ ] Add `.mjs` and `.map` MIME types to `hbf/http/server.c`
- [ ] Add HTTP tests for new MIME types
- [ ] Create example HTML in `pods/base/static/dev/index.html` with import map
- [ ] Test browser module loading with vendored package
- [ ] Verify source maps work in browser devtools

### Phase 6: Documentation & CI (Week 3)
- [ ] Document module loader in `DOCS/js_modules.md`
- [ ] Update `CLAUDE.md` with module import examples
- [ ] Add developer guide for vendoring npm packages
- [ ] Add CI validation for vendored packages (`.github/workflows/pr.yml`)
- [ ] Document browser import map usage

## Testing Strategy

### Unit Tests
- [x] Path resolution logic (relative, absolute, normalization)
- [x] Path traversal prevention (`../../escape.js` should fail)
- [x] Bare specifier rejection

### Integration Tests
- [ ] Import module from SQLAR-backed virtual FS
- [ ] Multi-level imports (server.js → square.js → utils.js)
- [ ] Error handling (module not found, syntax errors)
- [ ] globalThis.app accessibility from C handler

### HTTP Tests
- [ ] Serve `.js` with correct MIME
- [ ] Serve `.mjs` with correct MIME
- [ ] Serve `.map` with correct MIME
- [ ] Cache headers in dev vs prod mode

### Browser Tests (Manual)
- [ ] Load import map from `/static/importmap.json`
- [ ] Import vendored package via bare specifier
- [ ] Source map support in browser devtools
- [ ] Multi-package imports (e.g., CodeMirror + lodash-es)
- [ ] Verify no CORS errors for same-origin module loads

### Vendoring Tests
- [ ] Vendor simple package (lodash-es)
- [ ] Vendor scoped package (@codemirror/view)
- [ ] Vendor multiple packages in single command
- [ ] Re-run vendoring (idempotent check)
- [ ] Import map merging (add package to existing map)
- [ ] LICENSE file copying
- [ ] Vendor manifest generation
- [ ] Invalid package spec handling
- [ ] Non-existent pod handling
- [ ] ESM entry point detection (module field, exports field, main field)

## Risk Mitigation

### Risk: Breaking existing pods
**Mitigation:**
- Keep base pod simple during initial implementation
- Add `globalThis.app` in parallel with legacy global pattern
- Test both patterns in CI

### Risk: Path traversal vulnerabilities
**Mitigation:**
- Normalize all paths before filesystem access
- Reject paths that traverse above root
- Add fuzzing tests for path resolver

### Risk: Performance regression
**Mitigation:**
- Module compilation is one-time per request (not cached initially)
- Profile with Apache Bench before/after
- Consider adding module cache if needed (future optimization)

### Risk: QuickJS API changes
**Mitigation:**
- Pin QuickJS version in third_party
- Document QuickJS API version requirements
- Add CI check for QuickJS version

## Success Criteria

✅ **Server-side imports work:**
```javascript
import { name, draw } from "./modules/square.js";
```

✅ **globalThis.app pattern works:**
```javascript
globalThis.app = { handle(req, res) { ... } };
```

✅ **All tests pass:**
```bash
bazel test //hbf/qjs:module_loader_test
bazel test //hbf/qjs:module_import_integration_test
bazel test //...
```

✅ **Binary size unchanged:**
- Stripped binary still ~5.3 MB
- Module loader adds <10 KB

✅ **No regressions:**
- Existing pods work without changes (after globalThis migration)
- HTTP serving performance unchanged
- Build time impact <5%

## Future Enhancements (Out of Scope)

- Module caching across requests (currently recompiles per request)
- Dynamic `import()` support for lazy loading
- Import map support for server-side bare specifiers
- Circular dependency detection
- Source map support for server-side debugging
- Hot module reloading in dev mode

## References

- **js_import_plan.md** - Original high-level plan
- **QuickJS documentation** - `JS_SetModuleLoaderFunc` API
- **MDN: JavaScript modules** - ES module semantics
- **HBF architecture** - `CLAUDE.md` and `DOCS/`
