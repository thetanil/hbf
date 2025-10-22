/* Built-in module registration implementation */
#include "internal/qjs/modules.h"

#include <string.h>

#include "internal/core/log.h"
#include "internal/qjs/loader.h"

/* Load router.js module */
int hbf_qjs_load_router_module(hbf_qjs_ctx_t *ctx, sqlite3 *db)
{
	int ret;

	if (!ctx || !db) {
		hbf_log_error("Invalid arguments to hbf_qjs_load_router_module");
		return -1;
	}

	/* Load router.js from database */
	ret = hbf_qjs_load_script(ctx, db, "router.js");
	if (ret != 0) {
		hbf_log_warn("Failed to load router.js, using fallback");
		/* TODO: Could load embedded fallback router here */
		return -1;
	}

	hbf_log_debug("Router module loaded successfully");
	return 0;
}
