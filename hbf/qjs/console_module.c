/* Console module implementation - binds console.log to HBF logger */
#include "hbf/qjs/console_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hbf/shell/log.h"
#include "quickjs.h"

/* Helper to convert JSValue to string for logging */
static char *js_value_to_string(JSContext *ctx, JSValueConst val)
{
	const char *str;
	char *result;
	size_t len;

	/* Try to convert to string */
	str = JS_ToCString(ctx, val);
	if (!str) {
		return strdup("[object]");
	}

	len = strlen(str);
	result = (char *)malloc(len + 1);
	if (result) {
		memcpy(result, str, len + 1);
	}

	JS_FreeCString(ctx, str);
	return result;
}

/* Helper to concatenate multiple arguments into a single string */
static char *concat_args(JSContext *ctx, int argc, JSValueConst *argv)
{
	char *result;
	char *temp;
	size_t total_len;
	size_t offset;
	int i;

	if (argc == 0) {
		return strdup("");
	}

	/* Calculate total length needed */
	total_len = 0;
	for (i = 0; i < argc; i++) {
		temp = js_value_to_string(ctx, argv[i]);
		total_len += strlen(temp);
		if (i < argc - 1) {
			total_len += 1; /* Space between args */
		}
		free(temp);
	}

	/* Allocate result buffer */
	result = (char *)malloc(total_len + 1);
	if (!result) {
		return strdup("[OOM]");
	}

	/* Concatenate all arguments */
	offset = 0;
	for (i = 0; i < argc; i++) {
		size_t arg_len;

		temp = js_value_to_string(ctx, argv[i]);
		arg_len = strlen(temp);
		memcpy(result + offset, temp, arg_len);
		offset += arg_len;
		free(temp);

		if (i < argc - 1) {
			result[offset++] = ' ';
		}
	}
	result[offset] = '\0';

	return result;
}

/* console.log(...args) - logs at INFO level */
static JSValue js_console_log(JSContext *ctx,
			      JSValueConst this_val __attribute__((unused)),
			      int argc, JSValueConst *argv)
{
	char *msg = concat_args(ctx, argc, argv);

	hbf_log_info("%s", msg);
	free(msg);

	return JS_UNDEFINED;
}

/* console.warn(...args) - logs at WARN level */
static JSValue js_console_warn(JSContext *ctx,
			       JSValueConst this_val __attribute__((unused)),
			       int argc, JSValueConst *argv)
{
	char *msg = concat_args(ctx, argc, argv);

	hbf_log_warn("%s", msg);
	free(msg);

	return JS_UNDEFINED;
}

/* console.error(...args) - logs at ERROR level */
static JSValue js_console_error(JSContext *ctx,
				JSValueConst this_val __attribute__((unused)),
				int argc, JSValueConst *argv)
{
	char *msg = concat_args(ctx, argc, argv);

	hbf_log_error("%s", msg);
	free(msg);

	return JS_UNDEFINED;
}

/* console.debug(...args) - logs at DEBUG level */
static JSValue js_console_debug(JSContext *ctx,
				JSValueConst this_val __attribute__((unused)),
				int argc, JSValueConst *argv)
{
	char *msg = concat_args(ctx, argc, argv);

	hbf_log_debug("%s", msg);
	free(msg);

	return JS_UNDEFINED;
}

/* Console module function list */
static const JSCFunctionListEntry console_funcs[] = {
	JS_CFUNC_DEF("log", 0, js_console_log),
	JS_CFUNC_DEF("warn", 0, js_console_warn),
	JS_CFUNC_DEF("error", 0, js_console_error),
	JS_CFUNC_DEF("debug", 0, js_console_debug),
};

/* Initialize console module */
int hbf_qjs_init_console_module(JSContext *ctx)
{
	JSValue global_obj;
	JSValue console_obj;

	console_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, console_obj, console_funcs,
				   sizeof(console_funcs) /
					   sizeof(JSCFunctionListEntry));

	global_obj = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global_obj, "console", console_obj);
	JS_FreeValue(ctx, global_obj);

	return 0;
}
