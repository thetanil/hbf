/* QuickJS engine wrapper implementation */
#include "internal/qjs/engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "internal/core/log.h"
#include "quickjs.h"

#include "internal/db/db.h"
#include "internal/qjs/db_module.h"
#include "internal/qjs/console_module.h"

/* Global engine configuration */
static struct {
	size_t mem_limit_bytes;
	int timeout_ms;
	int initialized;
} g_qjs_config = {0};


/* Get current time in milliseconds */
static int64_t hbf_qjs_get_time_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Interrupt handler for execution timeout */
static int hbf_qjs_interrupt_handler(JSRuntime *rt, void *opaque)
{
	hbf_qjs_ctx_t *ctx = (hbf_qjs_ctx_t *)opaque;
	int64_t elapsed;

	(void)rt; /* Unused parameter */

	if (g_qjs_config.timeout_ms == 0)
		return 0; /* No timeout */

	elapsed = hbf_qjs_get_time_ms() - ctx->start_time_ms;

	if (elapsed > g_qjs_config.timeout_ms) {
		hbf_log_warn("QuickJS execution timeout after %lld ms",
			     (long long)elapsed);
		return 1; /* Interrupt execution */
	}

	return 0; /* Continue */
}

/* Initialize QuickJS engine */
int hbf_qjs_init(size_t mem_limit_mb, int timeout_ms)
{
	if (g_qjs_config.initialized) {
		hbf_log_warn("QuickJS engine already initialized");
		return 0;
	}

	g_qjs_config.mem_limit_bytes = mem_limit_mb * 1024 * 1024;
	g_qjs_config.timeout_ms = timeout_ms;
	g_qjs_config.initialized = 1;

	hbf_log_info("QuickJS engine initialized (mem_limit=%zu MB, timeout=%d ms)",
		     mem_limit_mb, timeout_ms);

	return 0;
}

/* Shutdown QuickJS engine */
void hbf_qjs_shutdown(void)
{
	if (!g_qjs_config.initialized)
		return;

	g_qjs_config.initialized = 0;
	hbf_log_info("QuickJS engine shutdown");
}

/* Create QuickJS context (internal implementation) */
static hbf_qjs_ctx_t *hbf_qjs_ctx_create_internal(sqlite3 *db, int own_db)
{
	hbf_qjs_ctx_t *ctx;
	JSRuntime *rt;
	JSContext *js_ctx;

	if (!g_qjs_config.initialized) {
		hbf_log_error("QuickJS engine not initialized");
		return NULL;
	}

	/* Allocate context wrapper */
	ctx = (hbf_qjs_ctx_t *)calloc(1, sizeof(hbf_qjs_ctx_t));
	if (!ctx) {
		hbf_log_error("Failed to allocate QuickJS context");
		return NULL;
	}

	hbf_log_debug("Creating QuickJS runtime");
	/* Create runtime */
	rt = JS_NewRuntime();
	if (!rt) {
		hbf_log_error("Failed to create QuickJS runtime");
		free(ctx);
		return NULL;
	}

	hbf_log_debug("Setting memory limit");
	/* Set memory limit */
	if (g_qjs_config.mem_limit_bytes > 0) {
		JS_SetMemoryLimit(rt, g_qjs_config.mem_limit_bytes);
	}

	hbf_log_debug("Setting interrupt handler");
	/* Set interrupt handler for timeout */
	if (g_qjs_config.timeout_ms > 0) {
		JS_SetInterruptHandler(rt, hbf_qjs_interrupt_handler, ctx);
	}

	hbf_log_debug("Creating QuickJS context");
	/* Create context with standard library */
	js_ctx = JS_NewContext(rt);
	if (!js_ctx) {
		hbf_log_error("Failed to create QuickJS context");
		JS_FreeRuntime(rt);
		free(ctx);
		return NULL;
	}
	hbf_log_debug("QuickJS context created, now setting up");

	ctx->rt = rt;
	ctx->ctx = js_ctx;
	ctx->start_time_ms = hbf_qjs_get_time_ms();
	ctx->error_buf[0] = '\0';
	ctx->db = NULL;

	/* Handle database setup */
	if (db) {
		/* Use provided database (external) */
		ctx->db = db;
		ctx->own_db = 0; /* Don't close on destroy */
	} else if (own_db) {
		/* Open own database (for testing) */
		if (hbf_db_open(":memory:", &ctx->db) != 0) {
			hbf_log_error("Failed to open SQLite DB");
			JS_FreeContext(js_ctx);
			JS_FreeRuntime(rt);
			free(ctx);
			return NULL;
		}
		ctx->own_db = 1; /* Close on destroy */
	}

	hbf_log_debug("Setting context opaque");
	/* Set context opaque to allow DB access from JS modules */
	JS_SetContextOpaque(js_ctx, ctx);

	hbf_log_debug("Adding intrinsics - BaseObjects");
	/* Initialize standard builtin objects */
	/* IMPORTANT: Add intrinsics AFTER setting up context but BEFORE registering modules */
	JS_AddIntrinsicBaseObjects(js_ctx);  /* Object, Function, Array, etc. */

	hbf_log_debug("Adding intrinsics - Date");
	JS_AddIntrinsicDate(js_ctx);         /* Date.now() needed by server.js */

	hbf_log_debug("Adding intrinsics - RegExp");
	JS_AddIntrinsicRegExp(js_ctx);       /* RegExp needed for router.js path matching */

	hbf_log_debug("Adding intrinsics - JSON");
	JS_AddIntrinsicJSON(js_ctx);         /* JSON.stringify() needed for res.json() */

	hbf_log_debug("Registering db module");
	/* Register custom modules */
	hbf_qjs_init_db_module(js_ctx);

	hbf_log_debug("Registering console module");
	hbf_qjs_init_console_module(js_ctx);

	hbf_log_debug("QuickJS context created successfully");
	return ctx;
}

/* Create QuickJS context with in-memory database (for testing) */
hbf_qjs_ctx_t *hbf_qjs_ctx_create(void)
{
	return hbf_qjs_ctx_create_internal(NULL, 1);
}

/* Create QuickJS context with external database */
hbf_qjs_ctx_t *hbf_qjs_ctx_create_with_db(sqlite3 *db)
{
	if (!db) {
		hbf_log_error("External database pointer is NULL");
		return NULL;
	}
	return hbf_qjs_ctx_create_internal(db, 0);
}

/* Destroy QuickJS context */
void hbf_qjs_ctx_destroy(hbf_qjs_ctx_t *ctx)
{
	if (!ctx)
		return;

	/* Only close database if we own it */
	if (ctx->db && ctx->own_db) {
		sqlite3_close(ctx->db);
		ctx->db = NULL;
	}

	if (ctx->ctx && ctx->rt) {
		JS_RunGC(ctx->rt);
		JS_FreeContext(ctx->ctx);
		JS_FreeRuntime(ctx->rt);
		ctx->ctx = NULL;
		ctx->rt = NULL;
	}

	free(ctx);
	hbf_log_debug("QuickJS context destroyed");
}

/* Evaluate JavaScript code */
int hbf_qjs_eval(hbf_qjs_ctx_t *ctx, const char *code, size_t len,
		 const char *filename)
{
	JSValue result;

	if (!ctx || !ctx->ctx || !code) {
		hbf_log_error("Invalid arguments to hbf_qjs_eval");
		return -1;
	}

	/* Use default filename if not provided */
	if (!filename) {
		filename = "<eval>";
	}

	/* Reset start time for timeout tracking */
	ctx->start_time_ms = hbf_qjs_get_time_ms();

	/* Evaluate code */
	result = JS_Eval(ctx->ctx, code, len, filename,
			 JS_EVAL_TYPE_GLOBAL);

	/* Check for exception */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(ctx->ctx);
		const char *str = JS_ToCString(ctx->ctx, exception);

		if (str) {
			snprintf(ctx->error_buf, sizeof(ctx->error_buf),
				 "%s", str);
			JS_FreeCString(ctx->ctx, str);
		} else {
			snprintf(ctx->error_buf, sizeof(ctx->error_buf),
				 "Unknown JavaScript error");
		}

		JS_FreeValue(ctx->ctx, exception);
		JS_FreeValue(ctx->ctx, result);

		hbf_log_warn("JavaScript evaluation error: %s",
			     ctx->error_buf);
		return -1;
	}

	JS_FreeValue(ctx->ctx, result);
	ctx->error_buf[0] = '\0';
	return 0;
}

/* Get last error message */
const char *hbf_qjs_get_error(hbf_qjs_ctx_t *ctx)
{
	if (!ctx || ctx->error_buf[0] == '\0')
		return NULL;

	return ctx->error_buf;
}

/* Get internal JSContext pointer */
void *hbf_qjs_get_js_context(hbf_qjs_ctx_t *ctx)
{
	return ctx ? ctx->ctx : NULL;
}

/* Get internal JSRuntime pointer */
void *hbf_qjs_get_js_runtime(hbf_qjs_ctx_t *ctx)
{
	return ctx ? ctx->rt : NULL;
}

/* Mark the beginning of JS execution */
void hbf_qjs_begin_exec(hbf_qjs_ctx_t *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->start_time_ms = hbf_qjs_get_time_ms();
}
