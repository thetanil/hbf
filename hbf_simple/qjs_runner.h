/* QuickJS Runner: JavaScript execution engine */

#ifndef HBF_SIMPLE_QJS_RUNNER_H
#define HBF_SIMPLE_QJS_RUNNER_H

#include "hbf_simple/http_bridge.h"

/* Initialize QuickJS runtime and load server.js */
int qjs_init(void);

/* Process HTTP request through JavaScript
 * Returns 0 on success, -1 on error */
int qjs_handle_request(const qjs_request *req, qjs_response *resp);

/* Cleanup QuickJS runtime */
void qjs_cleanup(void);

#endif /* HBF_SIMPLE_QJS_RUNNER_H */
