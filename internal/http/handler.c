/* QuickJS HTTP request handler implementation */
#include "internal/http/handler.h"

#include "internal/core/log.h"
#include "internal/qjs/bindings/request.h"
#include "internal/qjs/bindings/response.h"
#include "internal/qjs/engine.h"
// #include "internal/qjs/pool.h" // pool removed
#include "quickjs.h"

/* QuickJS request handler */
int hbf_qjs_request_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *ri;
	hbf_qjs_ctx_t *qjs_ctx;
	JSContext *ctx;
	JSValue global, app, handle_func, req, res, result;
	hbf_response_t response;
	int status;

	(void)cbdata;

	ri = mg_get_request_info(conn);
	if (!ri) {
		hbf_log_error("Failed to get request info");
		return 500;
	}

	hbf_log_debug("QuickJS handler: %s %s", ri->request_method, ri->local_uri);

	// Pool removed: use global QuickJS context
	extern hbf_qjs_ctx_t *g_qjs_ctx; // TODO: define global context in main
	qjs_ctx = g_qjs_ctx;
	if (!qjs_ctx) {
		hbf_log_error("Global QuickJS context not initialized");
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	ctx = (JSContext *)hbf_qjs_get_js_context(qjs_ctx);
	if (!ctx) {
		hbf_log_error("Failed to get JS context");
		// Pool removed: no release needed for global context
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Create request and response objects */
	req = hbf_qjs_create_request(ctx, conn);
	if (JS_IsException(req) || JS_IsNull(req)) {
		hbf_log_error("Failed to create request object");
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	res = hbf_qjs_create_response(ctx, &response);
	if (JS_IsException(res) || JS_IsNull(res)) {
		hbf_log_error("Failed to create response object");
		JS_FreeValue(ctx, req);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Get global.app.handle */
	global = JS_GetGlobalObject(ctx);

	/* Debug: check what's in global */
	hbf_log_debug("Getting 'app' from global object");

	app = JS_GetPropertyStr(ctx, global, "app");

	if (JS_IsUndefined(app)) {
		hbf_log_error("app is undefined in global context");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	if (JS_IsNull(app)) {
		hbf_log_error("app is null in global context");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	handle_func = JS_GetPropertyStr(ctx, app, "handle");

	if (JS_IsUndefined(handle_func)) {
		hbf_log_error("app.handle is undefined");
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	if (!JS_IsFunction(ctx, handle_func)) {
		hbf_log_error("app.handle is not a function (type check failed)");
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Call app.handle(req, res) */
	{
		JSValue args[2];
		args[0] = req;
		args[1] = res;

		hbf_log_debug("About to call app.handle()");

		/* Check if req and res are valid */
		if (JS_IsNull(req) || JS_IsUndefined(req)) {
			hbf_log_error("req is null or undefined!");
		}
		if (JS_IsNull(res) || JS_IsUndefined(res)) {
			hbf_log_error("res is null or undefined!");
		}

		/* Reset execution timeout timer before JS entry point */
		hbf_qjs_begin_exec(qjs_ctx);

		hbf_log_debug("Calling JS_Call...");
		result = JS_Call(ctx, handle_func, app, 2, args);
		hbf_log_debug("JS_Call returned");
	}

	/* Check for JavaScript error */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(ctx);
		const char *err_str = JS_ToCString(ctx, exception);
		JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
		const char *stack_str = NULL;

		if (!JS_IsUndefined(stack)) {
			stack_str = JS_ToCString(ctx, stack);
		}

		hbf_log_error("JavaScript error in request handler: %s",
			      err_str ? err_str : "unknown");

		if (stack_str) {
			hbf_log_error("Stack trace: %s", stack_str);
			JS_FreeCString(ctx, stack_str);
		}

		JS_FreeValue(ctx, stack);
		if (err_str) {
			JS_FreeCString(ctx, err_str);
		}

		/* Reset re-entrancy flag before releasing context back to pool */
		{
			JSValue false_val = JS_FALSE;
			JS_SetPropertyStr(ctx, app, "_inHandle", false_val);
		}

		JS_FreeValue(ctx, exception);
		JS_FreeValue(ctx, result);
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_response_free(&response);
		hbf_qjs_pool_release(qjs_ctx);

		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Send response */
	status = response.status_code;
	hbf_send_response(conn, &response);

	/* Reset re-entrancy flag before releasing context back to pool */
	{
		JSValue false_val = JS_FALSE;
		JS_SetPropertyStr(ctx, app, "_inHandle", false_val);
	}

	/* Cleanup */
	JS_FreeValue(ctx, result);
	JS_FreeValue(ctx, handle_func);
	JS_FreeValue(ctx, app);
	JS_FreeValue(ctx, res);
	JS_FreeValue(ctx, req);
	JS_FreeValue(ctx, global);
	hbf_response_free(&response);

	/* Release context back to pool */
	hbf_qjs_pool_release(qjs_ctx);

	return status;
}
