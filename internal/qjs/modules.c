/* Built-in module registration implementation */
#include "internal/qjs/modules.h"

#include <string.h>

#include "internal/core/log.h"

/* Load router.js module */
int hbf_qjs_load_router_module(hbf_qjs_ctx_t *ctx, sqlite3 *db)
{
	(void)ctx;
	(void)db;
	// No longer loads router.js at startup; script loading is now per request
	return 0;
}
