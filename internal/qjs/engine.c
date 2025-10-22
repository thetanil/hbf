/* QuickJS engine wrapper implementation */
#include "internal/qjs/engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "internal/core/log.h"
#include "quickjs.h"

/* Global engine configuration */
static struct {
	size_t mem_limit_bytes;
	int timeout_ms;
	int initialized;
} g_qjs_config = {0};

/* Context wrapper structure */
struct hbf_qjs_ctx {
	JSRuntime *rt;
	JSContext *ctx;
	char error_buf[512];
	int64_t start_time_ms;
};

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

/* Create QuickJS context */

/* C binding for console.log */
static JSValue hbf_console_log(JSContext *ctx, JSValueConst this_val,
							  int argc, JSValueConst *argv)
{
	int i;
	char buf[1024];
	size_t offset = 0;
	for (i = 0; i < argc; i++) {
		const char *str = JS_ToCString(ctx, argv[i]);
		if (str) {
			size_t len = strlen(str);
			if (offset + len + 2 < sizeof(buf)) {
				memcpy(buf + offset, str, len);
				offset += len;
				buf[offset++] = ' ';
			}
			JS_FreeCString(ctx, str);
		}
	}
	if (offset > 0 && buf[offset - 1] == ' ') {
		buf[offset - 1] = '\0';
	} else {
		buf[offset] = '\0';
	}
	hbf_log_info("[JS] %s", buf);
	return JS_UNDEFINED;
}

hbf_qjs_ctx_t *hbf_qjs_ctx_create(void)
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

	/* Create runtime */
	rt = JS_NewRuntime();
	if (!rt) {
		hbf_log_error("Failed to create QuickJS runtime");
		free(ctx);
		return NULL;
	}

	/* Set memory limit */
	if (g_qjs_config.mem_limit_bytes > 0) {
		JS_SetMemoryLimit(rt, g_qjs_config.mem_limit_bytes);
	}

	/* Set interrupt handler for timeout */
	if (g_qjs_config.timeout_ms > 0) {
		JS_SetInterruptHandler(rt, hbf_qjs_interrupt_handler, ctx);
	}

	/* Create context */
	js_ctx = JS_NewContext(rt);
	if (!js_ctx) {
		hbf_log_error("Failed to create QuickJS context");
		JS_FreeRuntime(rt);
		free(ctx);
		return NULL;
	}

	ctx->rt = rt;
	ctx->ctx = js_ctx;
	ctx->start_time_ms = hbf_qjs_get_time_ms();
	ctx->error_buf[0] = '\0';

	/* Register console.log binding */
	JSValue console = JS_NewObject(js_ctx);
	JS_SetPropertyStr(js_ctx, console, "log",
					  JS_NewCFunction(js_ctx, hbf_console_log, "log", 1));
	JS_SetPropertyStr(js_ctx, JS_GetGlobalObject(js_ctx), "console", console);

	hbf_log_debug("QuickJS context created");
	return ctx;
}

/* Destroy QuickJS context */
void hbf_qjs_ctx_destroy(hbf_qjs_ctx_t *ctx)
{
	if (!ctx)
		return;

	if (ctx->ctx) {
		JS_FreeContext(ctx->ctx);
		ctx->ctx = NULL;
	}

	if (ctx->rt) {
		JS_FreeRuntime(ctx->rt);
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
