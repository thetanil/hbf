/* QuickJS HTTP request handler */
#ifndef HBF_HTTP_HANDLER_H
#define HBF_HTTP_HANDLER_H

/* Forward declarations */
struct Request;
struct Response;
struct hbf_server;

/* EWS-compatible QuickJS request handler
 * This handler:
 * 1. Creates a fresh QuickJS context for isolation
 * 2. Loads hbf/server.js from database
 * 3. Creates request/response objects
 * 4. Calls app.handle(req, res) in JavaScript
 * 5. Returns EWS Response structure
 * 6. Destroys context after request
 *
 * Returns: EWS Response pointer or NULL on error
 */
struct Response *hbf_qjs_handle_ews_request(const struct Request *request,
                                            struct hbf_server *server);

#endif /* HBF_HTTP_HANDLER_H */
