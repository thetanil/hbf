/* HTTP Bridge: CivetWeb ↔ QuickJS struct conversion */

#ifndef HBF_SIMPLE_HTTP_BRIDGE_H
#define HBF_SIMPLE_HTTP_BRIDGE_H

#include <stddef.h>

struct mg_connection;
struct mg_request_info;

/* Request struct passed to QuickJS */
typedef struct {
	const char *method;        /* "GET", "POST", etc. */
	const char *uri;           /* "/path?query" */
	const char *query_string;  /* "foo=bar&baz=qux" or NULL */
	const char *http_version;  /* "1.1" */

	/* Headers (fixed array for simplicity) */
	struct {
		const char *name;
		const char *value;
	} headers[32];
	int num_headers;

	/* Body */
	const char *body;
	size_t body_len;
} qjs_request;

/* Response struct returned from QuickJS */
typedef struct {
	int status_code;           /* 200, 404, etc. */

	/* Headers */
	struct {
		const char *name;
		const char *value;
	} headers[32];
	int num_headers;

	/* Body (allocated by JS, must be freed) */
	char *body;
	size_t body_len;
} qjs_response;

/* Convert CivetWeb request to qjs_request */
void bridge_civetweb_to_qjs(const struct mg_request_info *mg_req,
                             const char *body, size_t body_len,
                             qjs_request *out);

/* Send qjs_response via CivetWeb connection */
void bridge_qjs_to_civetweb(struct mg_connection *conn,
                             const qjs_response *resp);

/* Free response body (allocated by JS) */
void bridge_free_response(qjs_response *resp);

#endif /* HBF_SIMPLE_HTTP_BRIDGE_H */
