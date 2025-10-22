/* Response object binding implementation */
#include "internal/qjs/bindings/response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/core/log.h"

/* Helper: Get response_t from JS object */
static hbf_response_t *get_response_data(JSContext *ctx, JSValueConst this_val)
{
	JSValue ptr_val;
	int64_t ptr_int;

	ptr_val = JS_GetPropertyStr(ctx, this_val, "__hbf_response_ptr__");
	if (JS_IsException(ptr_val) || JS_IsUndefined(ptr_val)) {
		JS_FreeValue(ctx, ptr_val);
		hbf_log_error("Failed to get response pointer from object");
		return NULL;
	}

	if (JS_ToInt64(ctx, &ptr_int, ptr_val) < 0) {
		JS_FreeValue(ctx, ptr_val);
		hbf_log_error("Failed to convert response pointer to integer");
		return NULL;
	}

	JS_FreeValue(ctx, ptr_val);
	return (hbf_response_t *)((uintptr_t)ptr_int);
}

/* res.status(code) - Set HTTP status code */
static JSValue js_res_status(JSContext *ctx, JSValueConst this_val,
			      int argc, JSValueConst *argv)
{
	hbf_response_t *res;
	int status;

	(void)argc;

	res = get_response_data(ctx, this_val);
	if (!res) {
		return JS_EXCEPTION;
	}

	if (JS_ToInt32(ctx, &status, argv[0]) < 0) {
		return JS_EXCEPTION;
	}

	res->status_code = status;

	/* Return this for chaining */
	return JS_DupValue(ctx, this_val);
}

/* res.send(body) - Send text response */
static JSValue js_res_send(JSContext *ctx, JSValueConst this_val,
			    int argc, JSValueConst *argv)
{
	hbf_response_t *res;
	const char *body;
	size_t len;

	(void)argc;

	res = get_response_data(ctx, this_val);
	if (!res) {
		return JS_EXCEPTION;
	}

	if (res->sent) {
		hbf_log_warn("Response already sent");
		return JS_UNDEFINED;
	}

	body = JS_ToCStringLen(ctx, &len, argv[0]);
	if (!body) {
		return JS_EXCEPTION;
	}

	res->body = (char *)malloc(len + 1);
	if (res->body) {
		memcpy(res->body, body, len);
		res->body[len] = '\0';
		res->body_len = len;
		res->sent = 1;
	}

	JS_FreeCString(ctx, body);

	return JS_UNDEFINED;
}

/* res.json(obj) - Send JSON response */
static JSValue js_res_json(JSContext *ctx, JSValueConst this_val,
			    int argc, JSValueConst *argv)
{
	hbf_response_t *res;
	JSValue json_str;
	const char *str;
	size_t len;

	(void)argc;

	res = get_response_data(ctx, this_val);
	if (!res) {
		return JS_EXCEPTION;
	}

	if (res->sent) {
		hbf_log_warn("Response already sent");
		return JS_UNDEFINED;
	}

	/* Use JSON.stringify */
	json_str = JS_JSONStringify(ctx, argv[0], JS_NULL, JS_NULL);
	if (JS_IsException(json_str)) {
		return JS_EXCEPTION;
	}

	str = JS_ToCStringLen(ctx, &len, json_str);
	if (str) {
		res->body = (char *)malloc(len + 1);
		if (res->body) {
			memcpy(res->body, str, len);
			res->body[len] = '\0';
			res->body_len = len;
			res->sent = 1;

			/* Set Content-Type header */
			if (res->header_count < 32) {
				res->headers[res->header_count++] =
					strdup("Content-Type: application/json");
			}
		}
		JS_FreeCString(ctx, str);
	}

	JS_FreeValue(ctx, json_str);

	return JS_UNDEFINED;
}

/* res.set(name, value) - Set response header */
static JSValue js_res_set(JSContext *ctx, JSValueConst this_val,
			   int argc, JSValueConst *argv)
{
	hbf_response_t *res;
	const char *name;
	const char *value;
	char *header;
	size_t header_len;

	(void)argc;

	res = get_response_data(ctx, this_val);
	if (!res) {
		return JS_EXCEPTION;
	}

	if (res->header_count >= 32) {
		hbf_log_warn("Maximum header count reached");
		return JS_UNDEFINED;
	}

	name = JS_ToCString(ctx, argv[0]);
	value = JS_ToCString(ctx, argv[1]);

	if (name && value) {
		/* Format: "Name: Value" */
		header_len = strlen(name) + strlen(value) + 3;
		header = (char *)malloc(header_len);
		if (header) {
			snprintf(header, header_len, "%s: %s", name, value);
			res->headers[res->header_count++] = header;
		}
	}

	if (name)
		JS_FreeCString(ctx, name);
	if (value)
		JS_FreeCString(ctx, value);

	return JS_UNDEFINED;
}

/* Create JavaScript response object */
JSValue hbf_qjs_create_response(JSContext *ctx, hbf_response_t *res_data)
{
	JSValue res;
	JSValue ptr_val;

	if (!ctx || !res_data) {
		hbf_log_error("Invalid arguments to hbf_qjs_create_response");
		return JS_NULL;
	}

	/* Initialize response data */
	res_data->status_code = 200;
	res_data->header_count = 0;
	res_data->body = NULL;
	res_data->body_len = 0;
	res_data->sent = 0;

	/* Create response object */
	res = JS_NewObject(ctx);
	if (JS_IsException(res)) {
		return res;
	}

	/* Store response pointer as hidden property (as int64) */
	ptr_val = JS_NewInt64(ctx, (int64_t)((uintptr_t)res_data));
	JS_SetPropertyStr(ctx, res, "__hbf_response_ptr__", ptr_val);

	/* Bind methods */
	JS_SetPropertyStr(ctx, res, "status",
			  JS_NewCFunction(ctx, js_res_status, "status", 1));
	JS_SetPropertyStr(ctx, res, "send",
			  JS_NewCFunction(ctx, js_res_send, "send", 1));
	JS_SetPropertyStr(ctx, res, "json",
			  JS_NewCFunction(ctx, js_res_json, "json", 1));
	JS_SetPropertyStr(ctx, res, "set",
			  JS_NewCFunction(ctx, js_res_set, "set", 2));

	return res;
}

/* Send accumulated response to CivetWeb */
void hbf_send_response(struct mg_connection *conn, hbf_response_t *response)
{
	int i;

	if (!conn || !response) {
		return;
	}

	/* Send status line */
	mg_printf(conn, "HTTP/1.1 %d OK\r\n", response->status_code);

	/* Send custom headers */
	for (i = 0; i < response->header_count; i++) {
		mg_printf(conn, "%s\r\n", response->headers[i]);
	}

	/* Send body */
	if (response->body && response->body_len > 0) {
		mg_printf(conn, "Content-Length: %zu\r\n\r\n",
			  response->body_len);
		mg_write(conn, response->body, response->body_len);
	} else {
		mg_printf(conn, "Content-Length: 0\r\n\r\n");
	}
}

/* Free response data */
void hbf_response_free(hbf_response_t *response)
{
	int i;

	if (!response) {
		return;
	}

	/* Free headers */
	for (i = 0; i < response->header_count; i++) {
		free(response->headers[i]);
		response->headers[i] = NULL;
	}

	/* Free body */
	if (response->body) {
		free(response->body);
		response->body = NULL;
	}

	response->header_count = 0;
	response->body_len = 0;
	response->sent = 0;
}
