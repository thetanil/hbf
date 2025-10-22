/* QuickJS context pool - pre-created contexts for request handling */
#ifndef HBF_QJS_POOL_H
#define HBF_QJS_POOL_H

#include <sqlite3.h>

#include "internal/qjs/engine.h"

/* Initialize context pool
 * pool_size: Number of contexts to pre-create (default: 16)
 * mem_limit_mb: Memory limit per context in MB
 * timeout_ms: Execution timeout in milliseconds
 * db: Database handle to load server.js from
 * Returns 0 on success, -1 on error
 */
int hbf_qjs_pool_init(int pool_size, size_t mem_limit_mb, int timeout_ms,
		      sqlite3 *db);

/* Shutdown context pool and free all resources */
void hbf_qjs_pool_shutdown(void);

/* Acquire a context from the pool (blocking if none available)
 * Returns context handle or NULL on error
 */
hbf_qjs_ctx_t *hbf_qjs_pool_acquire(void);

/* Release a context back to the pool */
void hbf_qjs_pool_release(hbf_qjs_ctx_t *ctx);

/* Get pool statistics (for monitoring/debugging) */
void hbf_qjs_pool_stats(int *total, int *available, int *in_use);

#endif /* HBF_QJS_POOL_H */
