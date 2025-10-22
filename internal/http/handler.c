/* QuickJS HTTP request handler implementation */
#include "internal/http/handler.h"

#include "internal/core/log.h"
#include "internal/qjs/bindings/request.h"
#include "internal/qjs/bindings/response.h"
#include "internal/qjs/pool.h"
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

	/* Acquire QuickJS context from pool */
	qjs_ctx = hbf_qjs_pool_acquire();
	if (!qjs_ctx) {
		hbf_log_error("Failed to acquire QuickJS context");
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	ctx = (JSContext *)hbf_qjs_get_js_context(qjs_ctx);
	if (!ctx) {
		hbf_log_error("Failed to get JS context");
		hbf_qjs_pool_release(qjs_ctx);
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
	app = JS_GetPropertyStr(ctx, global, "app");

	if (JS_IsUndefined(app) || JS_IsNull(app)) {
		hbf_log_warn("app object not found - server.js not loaded?");
		JS_FreeValue(ctx, res);
		JS_FreeValue(ctx, req);
		JS_FreeValue(ctx, global);
		hbf_qjs_pool_release(qjs_ctx);
		mg_send_http_error(conn, 503, "Service Unavailable");
		return 503;
	}

	handle_func = JS_GetPropertyStr(ctx, app, "handle");

	if (!JS_IsFunction(ctx, handle_func)) {
		hbf_log_error("app.handle is not a function");
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

		result = JS_Call(ctx, handle_func, app, 2, args);
	}

	/* Check for JavaScript error */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(ctx);
		const char *err_str = JS_ToCString(ctx, exception);

		hbf_log_error("JavaScript error in request handler: %s",
			      err_str ? err_str : "unknown");

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
		hbf_qjs_pool_release(qjs_ctx);

		mg_send_http_error(conn, 500, "Internal Server Error");
		return 500;
	}

	/* Send response */
	status = response.status_code;
	hbf_send_response(conn, &response);

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
