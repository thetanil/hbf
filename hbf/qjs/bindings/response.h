/* Response object binding - C to JavaScript bridge */
#ifndef HBF_QJS_BINDINGS_RESPONSE_H
#define HBF_QJS_BINDINGS_RESPONSE_H

#include <libhttp.h>

#include "quickjs.h"

/* Response data structure (accumulates response in C) */
typedef struct {
	int status_code;
	char *headers[32];  /* Array of "Key: Value" strings */
	int header_count;
	char *body;
	size_t body_len;
	int sent;  /* Flag to prevent double-send */
} hbf_response_t;

/* Initialize response class (call once at startup before creating responses) */
void hbf_qjs_init_response_class(JSContext *ctx);

/* Create a JavaScript response object
 * Methods created:
 *   - res.status(code): Set HTTP status code
 *   - res.send(body): Send text response
 *   - res.json(obj): Send JSON response
 *   - res.set(name, value): Set response header
 *
 * Returns: JSValue response object (must be freed with JS_FreeValue)
 */
JSValue hbf_qjs_create_response(JSContext *ctx, hbf_response_t *res_data);

/* Send accumulated response to libhttp connection */
void hbf_send_response(struct lh_con_t *conn, hbf_response_t *response);

/* Free response data */
void hbf_response_free(hbf_response_t *response);

#endif /* HBF_QJS_BINDINGS_RESPONSE_H */
