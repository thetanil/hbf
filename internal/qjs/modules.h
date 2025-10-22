/* Built-in module registration for QuickJS */
#ifndef HBF_QJS_MODULES_H
#define HBF_QJS_MODULES_H

#include <sqlite3.h>

#include "internal/qjs/engine.h"

/* Load router.js into QuickJS context
 * This must be called before loading server.js
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_load_router_module(hbf_qjs_ctx_t *ctx, sqlite3 *db);

#endif /* HBF_QJS_MODULES_H */
