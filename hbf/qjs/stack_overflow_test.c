/* Standalone test to reproduce QuickJS stack overflow when calling app.handle() */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hbf/qjs/engine.h"
#include "hbf/shell/log.h"
#include "quickjs.h"

/* Read file into memory */
static char *read_file(const char *path, size_t *len)
{
	FILE *f;
	char *buf;
	size_t size;

	f = fopen(path, "r");
	if (!f) {
		perror("fopen");
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = (char *)malloc(size + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	if (fread(buf, 1, size, f) != size) {
		free(buf);
		fclose(f);
		return NULL;
	}

	buf[size] = '\0';
	*len = size;
	fclose(f);
	return buf;
}

int main(void)
{
	hbf_qjs_ctx_t *qjs_ctx;
	JSContext *ctx;
	char *server_code;
	size_t server_len;
	int ret;
	const char *server_js_path = "static/server.js";
	JSValue global, app, handle_func, req, res, result;

	/* Initialize logging */
	hbf_log_set_level(HBF_LOG_DEBUG);

	printf("QuickJS Stack Overflow Test\n");
	printf("============================\n\n");

	/* Read server.js */
	printf("Reading %s...\n", server_js_path);
	server_code = read_file(server_js_path, &server_len);
	if (!server_code) {
		fprintf(stderr, "Failed to read %s\n", server_js_path);
		return 1;
	}
	printf("Loaded %zu bytes\n\n", server_len);

	/* Initialize engine */
	printf("Initializing QuickJS engine...\n");
	hbf_qjs_init(64, 5000);

	/* Create context */
	printf("Creating QuickJS context...\n");
	qjs_ctx = hbf_qjs_ctx_create();
	if (!qjs_ctx) {
		fprintf(stderr, "Failed to create context\n");
		hbf_qjs_shutdown();
		free(server_code);
		return 1;
	}

	ctx = (JSContext *)hbf_qjs_get_js_context(qjs_ctx);
	printf("Context created successfully\n\n");

	/* Load server.js */
	printf("Evaluating server.js...\n");
	ret = hbf_qjs_eval(qjs_ctx, server_code, server_len, server_js_path);

	if (ret != 0) {
		fprintf(stderr, "Failed to load server.js: %s\n", hbf_qjs_get_error(qjs_ctx));
		hbf_qjs_ctx_destroy(qjs_ctx);
		hbf_qjs_shutdown();
		free(server_code);
		return 1;
	}
	printf("server.js loaded successfully\n\n");

	/* Get global.app.handle */
	printf("Getting global.app.handle...\n");
	global = JS_GetGlobalObject(ctx);
	app = JS_GetPropertyStr(ctx, global, "app");

	if (JS_IsUndefined(app) || JS_IsNull(app)) {
		fprintf(stderr, "app is not defined in global\n");
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, global);
		hbf_qjs_ctx_destroy(qjs_ctx);
		hbf_qjs_shutdown();
		free(server_code);
		return 1;
	}

	handle_func = JS_GetPropertyStr(ctx, app, "handle");

	if (JS_IsUndefined(handle_func) || !JS_IsFunction(ctx, handle_func)) {
		fprintf(stderr, "app.handle is not a function\n");
		JS_FreeValue(ctx, handle_func);
		JS_FreeValue(ctx, app);
		JS_FreeValue(ctx, global);
		hbf_qjs_ctx_destroy(qjs_ctx);
		hbf_qjs_shutdown();
		free(server_code);
		return 1;
	}

	printf("Creating mock req and res objects...\n");

	/* Create simple mock objects for req and res */
	req = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, req, "method", JS_NewString(ctx, "GET"));
	JS_SetPropertyStr(ctx, req, "path", JS_NewString(ctx, "/hellojs"));

	res = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, res, "statusCode", JS_NewInt32(ctx, 200));

	/* Create status() and send() functions for res */
	const char *res_methods =
		"(function(res) {\n"
		"  res.status = function(code) { this.statusCode = code; return this; };\n"
		"  res.send = function(body) { this.body = body; return this; };\n"
		"  return res;\n"
		"})";

	JSValue res_func = JS_Eval(ctx, res_methods, strlen(res_methods), "<res-setup>", JS_EVAL_TYPE_GLOBAL);
	if (!JS_IsException(res_func) && JS_IsFunction(ctx, res_func)) {
		JSValue args[] = { res };
		JSValue new_res = JS_Call(ctx, res_func, JS_UNDEFINED, 1, args);
		if (!JS_IsException(new_res)) {
			JS_FreeValue(ctx, res);
			res = new_res;
		}
		JS_FreeValue(ctx, res_func);
	}

	printf("Calling app.handle(req, res)...\n\n");

	/* Call app.handle(req, res) */
	JSValue call_args[2];
	call_args[0] = req;
	call_args[1] = res;

	result = JS_Call(ctx, handle_func, app, 2, call_args);

	/* Check for error */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(ctx);
		const char *err_str = JS_ToCString(ctx, exception);
		JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
		const char *stack_str = JS_ToCString(ctx, stack);

		printf("\n*** EXCEPTION CAUGHT ***\n");
		printf("Error: %s\n", err_str ? err_str : "(unknown)");
		if (stack_str && stack_str[0] != '\0') {
			printf("Stack: %s\n", stack_str);
		}

		if (err_str && (strstr(err_str, "stack") || strstr(err_str, "Maximum call stack"))) {
			printf("\n✓ SUCCESS: Reproduced stack overflow!\n");
		} else {
			printf("\n✗ Got an error, but not a stack overflow\n");
		}

		if (err_str) JS_FreeCString(ctx, err_str);
		if (stack_str) JS_FreeCString(ctx, stack_str);
		JS_FreeValue(ctx, stack);
		JS_FreeValue(ctx, exception);
	} else {
		printf("app.handle() completed successfully (no error)\n");
		printf("\n✓ Test passed - no stack overflow\n");
	}

	/* Cleanup */
	JS_FreeValue(ctx, result);
	JS_FreeValue(ctx, handle_func);
	JS_FreeValue(ctx, app);
	JS_FreeValue(ctx, res);
	JS_FreeValue(ctx, req);
	JS_FreeValue(ctx, global);
	hbf_qjs_ctx_destroy(qjs_ctx);
	hbf_qjs_shutdown();
	free(server_code);

	return 0;
}
