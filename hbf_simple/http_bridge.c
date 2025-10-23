/* HTTP Bridge: CivetWeb ↔ QuickJS struct conversion */

#include "hbf_simple/http_bridge.h"
#include "internal/core/log.h"
#include "civetweb.h"

#include <stdlib.h>
#include <string.h>

void bridge_civetweb_to_qjs(const struct mg_request_info *mg_req,
                             const char *body, size_t body_len,
                             qjs_request *out)
{
	if (!mg_req || !out) {
		return;
	}

	memset(out, 0, sizeof(*out));

	/* Copy basic request info */
	out->method = mg_req->request_method ? mg_req->request_method : "GET";
	out->uri = mg_req->request_uri ? mg_req->request_uri : "/";
	out->query_string = mg_req->query_string;  /* May be NULL */
	out->http_version = mg_req->http_version ? mg_req->http_version : "1.1";

	/* Copy headers (up to 32) */
	out->num_headers = 0;
	for (int i = 0; i < mg_req->num_headers && i < 32; i++) {
		out->headers[i].name = mg_req->http_headers[i].name;
		out->headers[i].value = mg_req->http_headers[i].value;
		out->num_headers++;
	}

	/* Copy body */
	out->body = body;
	out->body_len = body_len;
}

void bridge_qjs_to_civetweb(struct mg_connection *conn,
                             const qjs_response *resp)
{
	if (!conn || !resp) {
		return;
	}

	/* Send status line */
	mg_printf(conn, "HTTP/1.1 %d OK\r\n", resp->status_code);

	/* Send headers */
	for (int i = 0; i < resp->num_headers; i++) {
		mg_printf(conn, "%s: %s\r\n",
		          resp->headers[i].name,
		          resp->headers[i].value);
	}

	/* Content-Length header */
	mg_printf(conn, "Content-Length: %zu\r\n", resp->body_len);

	/* End headers */
	mg_printf(conn, "\r\n");

	/* Send body */
	if (resp->body && resp->body_len > 0) {
		mg_write(conn, resp->body, resp->body_len);
	}
}

void bridge_free_response(qjs_response *resp)
{
	if (!resp) {
		return;
	}

	if (resp->body) {
		free(resp->body);
		resp->body = NULL;
	}

	resp->body_len = 0;
}
