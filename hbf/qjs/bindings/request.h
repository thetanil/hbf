/* Request object binding - C to JavaScript bridge */
#ifndef HBF_QJS_BINDINGS_REQUEST_H
#define HBF_QJS_BINDINGS_REQUEST_H

#include <civetweb.h>

#include "quickjs.h"

/* Create a JavaScript request object from CivetWeb request
 * Properties created:
 *   - method: HTTP method (GET, POST, etc.)
 *   - path: Request path
 *   - query: Query string (raw)
 *   - headers: Object with header key-value pairs
 *   - params: Object for route parameters (populated by router)
 *   - dev: Boolean indicating if dev mode is enabled
 *
 * Returns: JSValue request object (must be freed with JS_FreeValue)
 */
JSValue hbf_qjs_create_request(JSContext *ctx, struct mg_connection *conn, int dev);

#endif /* HBF_QJS_BINDINGS_REQUEST_H */
