/* Database loader implementation */
#include "internal/qjs/loader.h"

#include <string.h>

#include "internal/core/log.h"

/* Load script from database by name */
int hbf_qjs_load_script(hbf_qjs_ctx_t *ctx, sqlite3 *db, const char *name)
{
	sqlite3_stmt *stmt = NULL;
	const char *sql;
	int rc;
	const char *content;
	size_t content_len;

	if (!ctx || !db || !name) {
		hbf_log_error("Invalid arguments to hbf_qjs_load_script");
		return -1;
	}

	/* Query for script content */
	sql = "SELECT json_extract(body, '$.content') "
	      "FROM nodes "
	      "WHERE json_extract(body, '$.name') = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare script query: %s",
			      sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind script name: %s",
			      sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		if (rc == SQLITE_DONE) {
			hbf_log_warn("Script '%s' not found in database", name);
		} else {
			hbf_log_error("Failed to query script: %s",
				      sqlite3_errmsg(db));
		}
		sqlite3_finalize(stmt);
		return -1;
	}

	/* Get script content */
	content = (const char *)sqlite3_column_text(stmt, 0);
	if (!content) {
		hbf_log_error("Script '%s' has no content", name);
		sqlite3_finalize(stmt);
		return -1;
	}

	content_len = (size_t)sqlite3_column_bytes(stmt, 0);

	/* Evaluate script */
	hbf_log_info("Loading script '%s' (%zu bytes)", name, content_len);
	rc = hbf_qjs_eval(ctx, content, content_len);

	sqlite3_finalize(stmt);

	if (rc != 0) {
		const char *error = hbf_qjs_get_error(ctx);
		hbf_log_error("Failed to evaluate script '%s': %s",
			      name, error ? error : "unknown error");
		return -1;
	}

	hbf_log_info("Script '%s' loaded successfully", name);
	return 0;
}

/* Load server.js specifically */
int hbf_qjs_load_server_js(hbf_qjs_ctx_t *ctx, sqlite3 *db)
{
	return hbf_qjs_load_script(ctx, db, "server.js");
}

/* Initialize context with router.js and server.js from database */
int hbf_qjs_ctx_init_with_scripts(hbf_qjs_ctx_t *ctx, sqlite3 *db)
{
	int rc;

	if (!ctx || !db) {
		hbf_log_error("Invalid arguments to hbf_qjs_ctx_init_with_scripts");
		return -1;
	}

	/* Load router.js first (provides Express-style API) */
	rc = hbf_qjs_load_script(ctx, db, "router.js");
	if (rc != 0) {
		hbf_log_error("Failed to load router.js into context");
		return -1;
	}

	/* Load server.js into context */
	rc = hbf_qjs_load_server_js(ctx, db);
	if (rc != 0) {
		hbf_log_error("Failed to load server.js into context");
		return -1;
	}

	hbf_log_debug("QuickJS context initialized with router.js and server.js");
	return 0;
}
