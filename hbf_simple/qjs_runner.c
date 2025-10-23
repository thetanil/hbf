/* QuickJS Runner: JavaScript execution engine */

#include "hbf_simple/qjs_runner.h"
#include "hbf_simple/server_js.h"
#include "internal/core/log.h"
#include "quickjs.h"

#include <stdlib.h>
#include <string.h>

/* Global QuickJS context (single instance, no pooling) */
static JSRuntime *rt = NULL;
static JSContext *ctx = NULL;
static JSValue handle_request_func;

/* Helper: Log JS exception */
static void log_js_exception(JSContext *local_ctx)
{
       JSValue exception = JS_GetException(local_ctx);
       const char *str = JS_ToCString(local_ctx, exception);
       if (str) {
			   hbf_log_error("JavaScript exception: %s", str);
	       JS_FreeCString(local_ctx, str);
       }

       /* Check for stack trace */
       JSValue stack = JS_GetPropertyStr(local_ctx, exception, "stack");
       if (!JS_IsUndefined(stack)) {
	       const char *stack_str = JS_ToCString(local_ctx, stack);
	       if (stack_str) {
					   hbf_log_error("Stack trace:\n%s", stack_str);
		       JS_FreeCString(local_ctx, stack_str);
	       }
       }
       JS_FreeValue(local_ctx, stack);
       JS_FreeValue(local_ctx, exception);
}

/* Helper: Create JS request object from qjs_request */
static JSValue create_request_object(JSContext *local_ctx, const qjs_request *req)
{
	JSValue obj = JS_NewObject(local_ctx);

	/* Basic properties */
	JS_SetPropertyStr(local_ctx, obj, "method",
			    JS_NewString(local_ctx, req->method));
	JS_SetPropertyStr(local_ctx, obj, "uri",
			    JS_NewString(local_ctx, req->uri));
	JS_SetPropertyStr(local_ctx, obj, "query",
			    req->query_string ? JS_NewString(local_ctx, req->query_string) : JS_NULL);
	JS_SetPropertyStr(local_ctx, obj, "httpVersion",
			    JS_NewString(local_ctx, req->http_version));

	/* Headers object */
       JSValue headers = JS_NewObject(local_ctx);
       for (int i = 0; i < req->num_headers; i++) {
	       JS_SetPropertyStr(local_ctx, headers, req->headers[i].name,
				 JS_NewString(local_ctx, req->headers[i].value));
       }
       JS_SetPropertyStr(local_ctx, obj, "headers", headers);

	/* Body */
       if (req->body && req->body_len > 0) {
	       JS_SetPropertyStr(local_ctx, obj, "body",
				 JS_NewStringLen(local_ctx, req->body, req->body_len));
       } else {
	       JS_SetPropertyStr(local_ctx, obj, "body", JS_NULL);
       }

	return obj;
}

/* Helper: Extract response from JS object */
static int extract_response_object(JSContext *local_ctx, JSValue resp_val,
									qjs_response *resp)
{
	memset(resp, 0, sizeof(*resp));

	/* Default status */
	resp->status_code = 200;

	/* Get status */
	JSValue status = JS_GetPropertyStr(local_ctx, resp_val, "status");
	if (JS_IsNumber(status)) {
		int32_t code;
		JS_ToInt32(ctx, &code, status);
		resp->status_code = code;
	}
	JS_FreeValue(local_ctx, status);

	/* Get headers */
	JSValue headers = JS_GetPropertyStr(local_ctx, resp_val, "headers");
       if (JS_IsObject(headers)) {
	       JSPropertyEnum *props;
	       uint32_t prop_count;
	       if (JS_GetOwnPropertyNames(local_ctx, &props, &prop_count, headers,
					 JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
		       for (uint32_t i = 0; i < prop_count && i < 32; i++) {
			       JSValue key = JS_AtomToString(local_ctx, props[i].atom);
			       JSValue val = JS_GetProperty(local_ctx, headers, props[i].atom);

			       const char *key_str = JS_ToCString(local_ctx, key);
			       const char *val_str = JS_ToCString(local_ctx, val);

			       if (key_str && val_str) {
				       /* Allocate and copy header strings */
				       resp->headers[i].name = strdup(key_str);
				       resp->headers[i].value = strdup(val_str);
				       resp->num_headers++;
			       }

			       if (key_str) JS_FreeCString(local_ctx, key_str);
			       if (val_str) JS_FreeCString(local_ctx, val_str);

			       JS_FreeValue(local_ctx, key);
			       JS_FreeValue(local_ctx, val);
		       }
		       js_free(local_ctx, props);
	       }
       }
       JS_FreeValue(local_ctx, headers);

	/* Get body */
	JSValue body = JS_GetPropertyStr(local_ctx, resp_val, "body");
       if (JS_IsString(body)) {
	       size_t len;
	       const char *str = JS_ToCStringLen(local_ctx, &len, body);
	       if (str) {
		       resp->body = malloc(len + 1);
		       if (resp->body) {
			       memcpy(resp->body, str, len);
			       resp->body[len] = '\0';
			       resp->body_len = len;
		       }
		       JS_FreeCString(local_ctx, str);
	       }
       }
       JS_FreeValue(local_ctx, body);

	return 0;
}

/* hbf:http module initialization */
static int js_http_module_init(JSContext *local_ctx, JSModuleDef *m)
{
	/* Set module exports to undefined (Request/Response are pure JS classes) */
	JS_SetModuleExport(local_ctx, m, "Request", JS_UNDEFINED);
	JS_SetModuleExport(local_ctx, m, "Response", JS_UNDEFINED);
	return 0;
}

int qjs_init(void)
{
	hbf_log_info("Initializing QuickJS runtime");

	/* Create runtime with 64 MB memory limit */
	rt = JS_NewRuntime();
	if (!rt) {
			   hbf_log_error("Failed to create QuickJS runtime");
		return -1;
	}

	JS_SetMemoryLimit(rt, 64 * 1024 * 1024);  /* 64 MB */

	/* Create context */
	ctx = JS_NewContext(rt);
	if (!ctx) {
			   hbf_log_error("Failed to create QuickJS context");
		JS_FreeRuntime(rt);
		rt = NULL;
		return -1;
	}

       /* Register hbf:http module */
       JSModuleDef *m = JS_NewCModule(ctx, "hbf:http", js_http_module_init);
       if (!m) {
			   hbf_log_error("Failed to register hbf:http module");
	       goto error;
       }
       JS_AddModuleExport(ctx, m, "Request");
       JS_AddModuleExport(ctx, m, "Response");
       handle_request_func = JS_UNDEFINED;

	/* Load server.js */
	hbf_log_info("Loading server.js (%zu bytes)", server_js_source_len);
	JSValue result = JS_Eval(ctx, server_js_source, server_js_source_len,
	                         "server.js", JS_EVAL_TYPE_MODULE);

	if (JS_IsException(result)) {
			   hbf_log_error("Failed to load server.js");
		log_js_exception(ctx);
		JS_FreeValue(ctx, result);
		goto error;
	}

	JS_FreeValue(ctx, result);

	/* Get handleRequest function from global scope */
	JSValue global = JS_GetGlobalObject(ctx);
	handle_request_func = JS_GetPropertyStr(ctx, global, "handleRequest");
	JS_FreeValue(ctx, global);

	if (!JS_IsFunction(ctx, handle_request_func)) {
			   hbf_log_error("handleRequest is not a function in server.js");
		goto error;
	}

	hbf_log_info("QuickJS runtime initialized successfully");
	return 0;

error:
	if (ctx) {
		JS_FreeContext(ctx);
		ctx = NULL;
	}
	if (rt) {
		JS_FreeRuntime(rt);
		rt = NULL;
	}
	return -1;
}

int qjs_handle_request(const qjs_request *req, qjs_response *resp)
{
	if (!ctx || !req || !resp) {
		return -1;
	}

	/* Create request object */
	JSValue req_obj = create_request_object(ctx, req);

	/* Call handleRequest(req) */
	JSValue result = JS_Call(ctx, handle_request_func, JS_UNDEFINED, 1, &req_obj);
	JS_FreeValue(ctx, req_obj);

	if (JS_IsException(result)) {
			   hbf_log_error("JavaScript execution failed in handleRequest");
		log_js_exception(ctx);
		JS_FreeValue(ctx, result);

		/* Return 500 error */
		memset(resp, 0, sizeof(*resp));
		resp->status_code = 500;
		resp->body = strdup("Internal Server Error: JavaScript execution failed");
		resp->body_len = strlen(resp->body);
		return -1;
	}

	/* Extract response object */
	int rc = extract_response_object(ctx, result, resp);
	JS_FreeValue(ctx, result);

	return rc;
}

void qjs_cleanup(void)
{
	hbf_log_info("Cleaning up QuickJS runtime");

	if (!JS_IsUndefined(handle_request_func)) {
		JS_FreeValue(ctx, handle_request_func);
		handle_request_func = JS_UNDEFINED;
	}

	if (ctx) {
		JS_FreeContext(ctx);
		ctx = NULL;
	}

	if (rt) {
		JS_FreeRuntime(rt);
		rt = NULL;
	}
}
