/* QuickJS HTTP request handler implementation */
#include "internal/http/handler.h"
#include "internal/http/server.h"
#include <stdlib.h>

#include "internal/core/log.h"
#include "internal/db/db.h"
#include "internal/qjs/bindings/request.h"
#include "internal/qjs/bindings/response.h"
#include "internal/qjs/engine.h"
#include "quickjs.h"
#include "sqlite3.h"

/* QuickJS request handler */
int hbf_qjs_request_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *ri;
	hbf_qjs_ctx_t *qjs_ctx;
	JSContext *ctx;
	JSValue global, app, handle_func, req, res, result;
	hbf_response_t response;
	int status;
	/* cbdata is expected to be hbf_server_t* for access to fs_db */
	hbf_server_t *server = (hbf_server_t *)cbdata;


	ri = mg_get_request_info(conn);
	if (!ri) {
		hbf_log_error("Failed to get request info");
		return 500;
	}

	hbf_log_debug("QuickJS handler: %s %s", ri->request_method, ri->local_uri);

	/*
	 * CRITICAL: Create a fresh JSRuntime + JSContext for each request.
	 * This prevents stack overflow errors and ensures complete isolation.
	 * DO NOT reuse contexts between requests - this is a QuickJS best practice.
	 */
	qjs_ctx = hbf_qjs_ctx_create_with_db(server ? server->db : NULL);
	if (!qjs_ctx) {
		hbf_log_error("Failed to create QuickJS context for request");
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	ctx = (JSContext *)hbf_qjs_get_js_context(qjs_ctx);
	if (!ctx) {
		hbf_log_error("Failed to get JS context");
		hbf_qjs_ctx_destroy(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

		/* Load hbf/server.js from SQLAR archive using fs_db */
		unsigned char *js_data = NULL;
		size_t js_size = 0;
		int ret = hbf_db_read_file(server ? server->fs_db : NULL, "hbf/server.js", &js_data, &js_size);
		if (ret != 0 || !js_data || js_size == 0) {
			hbf_log_error("hbf/server.js not found in SQLAR archive");
			hbf_qjs_ctx_destroy(qjs_ctx);
			mg_send_http_error(conn, 503, "Service Unavailable");
			return 503;
		}
		ret = hbf_qjs_eval(qjs_ctx, (const char *)js_data, js_size, "hbf/server.js");
		free(js_data);
		if (ret != 0) {
			hbf_log_error("Failed to load hbf/server.js: %s", hbf_qjs_get_error(qjs_ctx));
			hbf_qjs_ctx_destroy(qjs_ctx);
			mg_send_http_error(conn, 500, "Internal Server Error");
			return 500;
		}

	/* Create request and response objects */
	req = hbf_qjs_create_request(ctx, conn);
	if (JS_IsException(req) || JS_IsNull(req)) {
		hbf_log_error("Failed to create request object");
		hbf_qjs_ctx_destroy(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	res = hbf_qjs_create_response(ctx, &response);
	if (JS_IsException(res) || JS_IsNull(res)) {
		hbf_log_error("Failed to create response object");
		JS_FreeValue(ctx, req);
		hbf_qjs_ctx_destroy(qjs_ctx);
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
		hbf_response_free(&response);
		hbf_qjs_ctx_destroy(qjs_ctx);
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
		hbf_response_free(&response);
		hbf_qjs_ctx_destroy(qjs_ctx);
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	if (JS_IsNull(app)) {
		hbf_log_error("app is null in global context");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_response_free(&response);
		hbf_qjs_ctx_destroy(qjs_ctx);
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
		hbf_response_free(&response);
		hbf_qjs_ctx_destroy(qjs_ctx);
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
		hbf_response_free(&response);
		hbf_qjs_ctx_destroy(qjs_ctx);
		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Call app.handle(req, res) - no mutex needed (isolated context) */
	{
		JSValue args[2];
		args[0] = req;
		args[1] = res;

		hbf_log_debug("About to call app.handle()");

		/* Reset execution timeout timer before JS entry point */
		hbf_log_debug("Resetting exec timer");
		hbf_qjs_begin_exec(qjs_ctx);

		hbf_log_debug("Calling JS_Call (handle_func=%p, app=%p, argc=2)...", (void*)handle_func.u.ptr, (void*)app.u.ptr);
		result = JS_Call(ctx, handle_func, app, 2, args);
		hbf_log_debug("JS_Call returned (result=%p)", (void*)result.u.ptr);
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

		JS_FreeValue(ctx, exception);
		JS_FreeValue(ctx, result);
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_response_free(&response);

		/* Destroy context after error */
		hbf_qjs_ctx_destroy(qjs_ctx);

		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Send response */
	status = response.status_code;
	hbf_send_response(conn, &response);

	/* Cleanup - free all JS values before destroying context */
	JS_FreeValue(ctx, result);
	JS_FreeValue(ctx, handle_func);
	JS_FreeValue(ctx, app);
	JS_FreeValue(ctx, res);
	JS_FreeValue(ctx, req);
	JS_FreeValue(ctx, global);
	hbf_response_free(&response);

	/* Destroy the context and runtime (per-request isolation) */
	hbf_qjs_ctx_destroy(qjs_ctx);

	return status;
}
