/* QuickJS engine wrapper - runtime initialization and context management
 * Provides memory limits, execution timeouts, and error handling.
 */
#ifndef HBF_QJS_ENGINE_H
#define HBF_QJS_ENGINE_H

#include <stddef.h>

/* Opaque context handle */
typedef struct hbf_qjs_ctx hbf_qjs_ctx_t;

/* Initialize QuickJS engine with global settings
 * mem_limit_mb: Memory limit per context in MB (0 = unlimited)
 * timeout_ms: Execution timeout in milliseconds (0 = unlimited)
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_init(size_t mem_limit_mb, int timeout_ms);

/* Shutdown QuickJS engine and free resources */
void hbf_qjs_shutdown(void);

/* Create a new QuickJS context
 * Returns context handle or NULL on error
 */
hbf_qjs_ctx_t *hbf_qjs_ctx_create(void);

/* Destroy a QuickJS context */
void hbf_qjs_ctx_destroy(hbf_qjs_ctx_t *ctx);

/* Evaluate JavaScript code in context
 * Returns 0 on success, -1 on error
 * Error details available via hbf_qjs_get_error()
 */
int hbf_qjs_eval(hbf_qjs_ctx_t *ctx, const char *code, size_t len);

/* Get last error message from context
 * Returns error string (valid until next call) or NULL if no error
 */
const char *hbf_qjs_get_error(hbf_qjs_ctx_t *ctx);

/* Get internal JSContext pointer (for bindings)
 * WARNING: Use with care, violates abstraction
 */
void *hbf_qjs_get_js_context(hbf_qjs_ctx_t *ctx);

/* Get internal JSRuntime pointer (for bindings)
 * WARNING: Use with care, violates abstraction
 */
void *hbf_qjs_get_js_runtime(hbf_qjs_ctx_t *ctx);

/* Mark the beginning of JS execution (resets timeout timer)
 * Call this before any JS entry point (JS_Call, JS_Eval, etc.)
 * to ensure accurate timeout measurement
 */
void hbf_qjs_begin_exec(hbf_qjs_ctx_t *ctx);

#endif /* HBF_QJS_ENGINE_H */
