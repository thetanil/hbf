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

#endif /* HBF_QJS_LOADER_H */
