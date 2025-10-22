/* Database loader for server.js and other scripts */
#ifndef HBF_QJS_LOADER_H
#define HBF_QJS_LOADER_H

#include <sqlite3.h>

#include "internal/qjs/engine.h"

/* Load and execute server.js from database
 * Looks for a node with name='server.js' in the database
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_load_server_js(hbf_qjs_ctx_t *ctx, sqlite3 *db);

/* Load and execute arbitrary script from database by name
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_load_script(hbf_qjs_ctx_t *ctx, sqlite3 *db, const char *name);

/* Initialize context with router.js and server.js from database
 * This function loads the JavaScript environment into the context.
 * Should be called after hbf_qjs_ctx_create() and before using the context.
 *
 * This is a convenience function that calls:
 *   1. hbf_qjs_load_script(ctx, db, "router.js")
 *   2. hbf_qjs_load_server_js(ctx, db)
 *
 * ctx: QuickJS context to initialize
 * db: SQLite database handle containing router.js and server.js
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_ctx_init_with_scripts(hbf_qjs_ctx_t *ctx, sqlite3 *db);

#endif /* HBF_QJS_LOADER_H */
