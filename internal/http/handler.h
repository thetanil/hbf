/* QuickJS HTTP request handler */
#ifndef HBF_HTTP_HANDLER_H
#define HBF_HTTP_HANDLER_H

#include <civetweb.h>

/* QuickJS request handler (delegates to JavaScript)
 * This handler:
 * 1. Uses a global QuickJS context
 * 2. Creates request/response objects
 * 3. Calls app.handle(req, res) in JavaScript
 * 4. Sends the response back to CivetWeb
 * 5. No release needed for global context
 *
 * Returns: HTTP status code
 */
int hbf_qjs_request_handler(struct mg_connection *conn, void *cbdata);

#endif /* HBF_HTTP_HANDLER_H */
