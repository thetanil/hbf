/*
 * SPDX-License-Identifier: MIT
 * HBF QuickJS ES Module Loader
 * Loads JavaScript modules from the embedded SQLite database.
 */

#include "hbf/qjs/module_loader.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "hbf/shell/log.h"
#include "quickjs.h"

/*
 * Fetch module source from the versioned filesystem.
 * Returns malloc'd string or NULL on error. Caller must free.
 */
static char *hbf_qjs_get_module_source(sqlite3 *db, const char *module_path)
{
	sqlite3_stmt *stmt = NULL;
	char *src = NULL;
	int rc;
	const char *query = "SELECT data FROM latest_files WHERE path = ?";

	if (!db || !module_path) {
		return NULL;
	}

	rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare module query: %s",
			      sqlite3_errmsg(db));
		return NULL;
	}

	rc = sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind module path: %s",
			      sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return NULL;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const unsigned char *data = sqlite3_column_text(stmt, 0);
		if (data) {
			src = strdup((const char *)data);
			if (!src) {
				hbf_log_error("Failed to allocate module source");
			}
		}
	} else if (rc != SQLITE_DONE) {
		hbf_log_error("Module query error: %s", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return src;
}

/*
 * Module normalizer callback.
 * Resolves relative module paths against the base module.
 */
static char *hbf_qjs_module_normalize(JSContext *ctx, const char *base_name,
				      const char *module_name, void *opaque)
{
	char *normalized;

	(void)opaque; /* Database handle, unused for normalization */

	/* For now, return module_name as-is (absolute paths only) */
	/* TODO: Implement relative path resolution if needed */
	normalized = js_strdup(ctx, module_name);
	if (!normalized) {
		hbf_log_error("Failed to normalize module: %s (base: %s)",
			      module_name, base_name ? base_name : "none");
	}

	return normalized;
}

/*
 * Module loader callback.
 * Loads and compiles a JavaScript module from the database.
 */
static JSModuleDef *hbf_qjs_module_loader(JSContext *ctx,
					  const char *module_name, void *opaque)
{
	sqlite3 *db = (sqlite3 *)opaque;
	char *src = NULL;
	JSValue func_val;
	JSModuleDef *module = NULL;

	if (!db) {
		hbf_log_error("Module loader: no database handle");
		return NULL;
	}

	/* Fetch module source from database */
	src = hbf_qjs_get_module_source(db, module_name);
	if (!src) {
		hbf_log_error("Module not found: %s", module_name);
		JS_ThrowReferenceError(ctx, "could not load module '%s'",
				       module_name);
		return NULL;
	}

	/* Compile module */
	func_val = JS_Eval(ctx, src, strlen(src), module_name,
			   JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	free(src);

	if (JS_IsException(func_val)) {
		hbf_log_error("Failed to compile module: %s", module_name);
		return NULL;
	}

	/* Get module definition from the compiled module */
	module = (JSModuleDef *)JS_VALUE_GET_PTR(func_val);

	return module;
}

/*
 * Initialize the ES module loader for a QuickJS runtime.
 * Must be called after JS_NewRuntime() and before loading any modules.
 */
void hbf_qjs_module_loader_init(JSRuntime *rt, sqlite3 *db)
{
	if (!rt) {
		hbf_log_error("Module loader init: null runtime");
		return;
	}

	if (!db) {
		hbf_log_warn("Module loader init: null database (imports will fail)");
	}

	JS_SetModuleLoaderFunc(rt, hbf_qjs_module_normalize,
			       hbf_qjs_module_loader, db);

	hbf_log_debug("ES module loader initialized");
}
