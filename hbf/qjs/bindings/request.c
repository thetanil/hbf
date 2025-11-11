/* Request object binding implementation */
#include "hbf/qjs/bindings/request.h"

#include <stdlib.h>
#include <string.h>

#include "hbf/shell/log.h"

/* Create JavaScript request object from libhttp request */
JSValue hbf_qjs_create_request(JSContext *ctx, struct lh_con_t *conn)
{
	const struct lh_rqi_t *ri;
	JSValue req;
	JSValue headers;
	JSValue params;
	int i;

	if (!ctx || !conn) {
		hbf_log_error("Invalid arguments to hbf_qjs_create_request");
		return JS_NULL;
	}

	ri = lh_get_request_info(conn);
	if (!ri) {
		hbf_log_error("Failed to get request info from connection");
		return JS_NULL;
	}

	/* Create request object */
	req = JS_NewObject(ctx);
	if (JS_IsException(req)) {
		return req;
	}

	/* Set method */
	JS_SetPropertyStr(ctx, req, "method",
			  JS_NewString(ctx, ri->request_method ? ri->request_method : "GET"));

	/* Set path (local_uri) */
	JS_SetPropertyStr(ctx, req, "path",
			  JS_NewString(ctx, ri->local_uri ? ri->local_uri : "/"));

	/* Set query string */
	JS_SetPropertyStr(ctx, req, "query",
			  JS_NewString(ctx, ri->query_string ? ri->query_string : ""));

	/* Create headers object */
	headers = JS_NewObject(ctx);
	if (!JS_IsException(headers)) {
		for (i = 0; i < ri->num_headers; i++) {
			JS_SetPropertyStr(ctx, headers,
					  ri->http_headers[i].name,
					  JS_NewString(ctx, ri->http_headers[i].value));
		}
		JS_SetPropertyStr(ctx, req, "headers", headers);
	}

	/* Create empty params object (will be populated by router) */
	params = JS_NewObject(ctx);
	if (!JS_IsException(params)) {
		JS_SetPropertyStr(ctx, req, "params", params);
	}

	/* Read request body for POST/PUT/DELETE requests */
	if (ri->content_length > 0) {
		char *body_buf = malloc((size_t)ri->content_length + 1);
		if (body_buf) {
			int bytes_read = lh_read(conn, body_buf, (size_t)ri->content_length);
			if (bytes_read > 0) {
				body_buf[bytes_read] = '\0';
				JS_SetPropertyStr(ctx, req, "body",
				                  JS_NewStringLen(ctx, body_buf, (size_t)bytes_read));
			} else {
				JS_SetPropertyStr(ctx, req, "body", JS_NewString(ctx, ""));
			}
			free(body_buf);
		} else {
			JS_SetPropertyStr(ctx, req, "body", JS_NewString(ctx, ""));
		}
	} else {
		JS_SetPropertyStr(ctx, req, "body", JS_NewString(ctx, ""));
	}

	return req;
}
