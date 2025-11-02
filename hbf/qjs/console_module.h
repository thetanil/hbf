/* Console module for QuickJS - binds console.log/warn/error to HBF logger */
#ifndef HBF_QJS_CONSOLE_MODULE_H
#define HBF_QJS_CONSOLE_MODULE_H

#include "quickjs.h"

/* Initialize console module (console.log, console.warn, console.error)
 * Registers global 'console' object with logging methods
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_init_console_module(JSContext *ctx);

#endif /* HBF_QJS_CONSOLE_MODULE_H */
