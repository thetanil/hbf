/* Request object binding - C to JavaScript bridge */
#ifndef HBF_QJS_BINDINGS_REQUEST_H
#define HBF_QJS_BINDINGS_REQUEST_H

#include <libhttp.h>

#include "quickjs.h"

/* Create a JavaScript request object from libhttp request
 * Properties created:
 *   - method: HTTP method (GET, POST, etc.)
 *   - path: Request path
 *   - query: Query string (raw)
 *   - headers: Object with header key-value pairs
 *   - params: Object for route parameters (populated by router)
 *   - body: Request body (for POST/PUT/DELETE)
 *
 * Returns: JSValue request object (must be freed with JS_FreeValue)
 */
JSValue hbf_qjs_create_request(JSContext *ctx, struct lh_ctx_t *http_ctx, struct lh_con_t *conn);

#endif /* HBF_QJS_BINDINGS_REQUEST_H */
