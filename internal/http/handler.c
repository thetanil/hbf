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
				// pool removed: no release needed
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	res = hbf_qjs_create_response(ctx, &response);
	if (JS_IsException(res) || JS_IsNull(res)) {
		hbf_log_error("Failed to create response object");
		JS_FreeValue(ctx, req);
	// pool removed: no release needed
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Get global.app.handle */
	hbf_log_debug("Getting global object");
	global = JS_GetGlobalObject(ctx);
	if (JS_IsException(global) || JS_IsNull(global) || JS_IsUndefined(global)) {
		hbf_log_error("Failed to get global object!");
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Debug: check what's in global */
	hbf_log_debug("Getting 'app' from global object");

	app = JS_GetPropertyStr(ctx, global, "app");

	if (JS_IsUndefined(app)) {
		hbf_log_error("app is undefined in global context");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
	// pool removed: no release needed
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	if (JS_IsNull(app)) {
		hbf_log_error("app is null in global context");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
	// pool removed: no release needed
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	hbf_log_debug("Getting 'handle' property from app");
	handle_func = JS_GetPropertyStr(ctx, app, "handle");

	if (JS_IsUndefined(handle_func)) {
		hbf_log_error("app.handle is undefined");
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
	// pool removed: no release needed
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	hbf_log_debug("Checking if handle is a function");
	if (!JS_IsFunction(ctx, handle_func)) {
		hbf_log_error("app.handle is not a function (type check failed)");
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
	// pool removed: no release needed
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Call app.handle(req, res) with mutex protection */
	{
		JSValue args[2];
		args[0] = req;
		args[1] = res;

		hbf_log_debug("About to call app.handle()");

		/* Check if req and res are valid */
		hbf_log_debug("Validating req and res objects");
		if (JS_IsNull(req) || JS_IsUndefined(req)) {
			hbf_log_error("req is null or undefined!");
		}
		if (JS_IsNull(res) || JS_IsUndefined(res)) {
			hbf_log_error("res is null or undefined!");
		}

		JSValue test_method = JS_GetPropertyStr(ctx, req, "method");
		hbf_log_debug("req has method: %d", !JS_IsUndefined(test_method));
		JS_FreeValue(ctx, test_method);

		JSValue test_path = JS_GetPropertyStr(ctx, req, "path");
		hbf_log_debug("req has path: %d", !JS_IsUndefined(test_path));
		JS_FreeValue(ctx, test_path);

		/* Lock mutex to serialize JS execution (single-threaded model) */
		extern pthread_mutex_t g_qjs_mutex;
		hbf_log_debug("Locking mutex");
		pthread_mutex_lock(&g_qjs_mutex);

		/* Reset execution timeout timer before JS entry point */
		hbf_log_debug("Resetting exec timer");
		hbf_qjs_begin_exec(qjs_ctx);

		hbf_log_debug("Calling JS_Call (handle_func=%p, app=%p, argc=2)...", (void*)handle_func.u.ptr, (void*)app.u.ptr);
		result = JS_Call(ctx, handle_func, app, 2, args);
		hbf_log_debug("JS_Call returned (result=%p)", (void*)result.u.ptr);

		/* Unlock mutex */
		pthread_mutex_unlock(&g_qjs_mutex);
	}

	/* Check for JavaScript error */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(ctx);
		const char *err_str = JS_ToCString(ctx, exception);
		JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
		const char *stack_str = NULL;
		JSValue message = JS_GetPropertyStr(ctx, exception, "message");
		const char *message_str = NULL;

		if (!JS_IsUndefined(message)) {
			message_str = JS_ToCString(ctx, message);
		}

		if (!JS_IsUndefined(stack)) {
			stack_str = JS_ToCString(ctx, stack);
		}

		hbf_log_error("JavaScript error in request handler: %s",
			      err_str ? err_str : "unknown");

		if (message_str && message_str[0] != '\0') {
			hbf_log_error("Error message: %s", message_str);
		}

		if (stack_str && stack_str[0] != '\0') {
			hbf_log_error("Stack trace: %s", stack_str);
		} else {
			hbf_log_error("Stack trace: (empty - possible native code error)");
		}

		JS_FreeValue(ctx, message);
		JS_FreeValue(ctx, stack);
		if (message_str) {
			JS_FreeCString(ctx, message_str);
		}
		if (stack_str) {
			JS_FreeCString(ctx, stack_str);
		}
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
	// pool removed: no release needed

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
	// pool removed: no release needed

	return status;
}
