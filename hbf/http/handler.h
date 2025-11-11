/* QuickJS HTTP request handler */
#ifndef HBF_HTTP_HANDLER_H
#define HBF_HTTP_HANDLER_H

#include <libhttp.h>

/* QuickJS request handler (delegates to JavaScript)
 * This handler:
 * 1. Creates fresh QuickJS context per request
 * 2. Creates request/response objects
 * 3. Calls app.handle(req, res) in JavaScript
 * 4. Sends the response back to libhttp
 * 5. Destroys context after request
 *
 * Returns: HTTP status code
 */
int hbf_qjs_request_handler(struct lh_con_t *conn, void *cbdata);

#endif /* HBF_HTTP_HANDLER_H */
