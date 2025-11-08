/* Initialize the JS DB module (db.query, db.execute) */

#ifndef HBF_QJS_ENGINE_H
#define HBF_QJS_ENGINE_H

#include <stddef.h>
#include <quickjs.h>
#include <sqlite3.h>
#include <stdint.h>

/* Opaque context handle */
typedef struct hbf_qjs_ctx hbf_qjs_ctx_t;
struct hbf_qjs_ctx {
	JSRuntime *rt;
	JSContext *ctx;
	char error_buf[512];
	int64_t start_time_ms;
	sqlite3 *db;
	int own_db; /* 1 if we own the DB and should close it */
};

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
 * NOTE: This creates an in-memory database for testing
 */
hbf_qjs_ctx_t *hbf_qjs_ctx_create(void);

/* Create a new QuickJS context with external database
 * db: Existing SQLite database connection (caller owns, must remain valid)
 * Returns context handle or NULL on error
 */
hbf_qjs_ctx_t *hbf_qjs_ctx_create_with_db(sqlite3 *db);

/* Destroy a QuickJS context */
void hbf_qjs_ctx_destroy(hbf_qjs_ctx_t *ctx);

/* Evaluate JavaScript code in context
 * filename: Source name for error messages (e.g., "server.js" or "<eval>")
 * Returns 0 on success, -1 on error
 * Error details available via hbf_qjs_get_error()
 */
int hbf_qjs_eval(hbf_qjs_ctx_t *ctx, const char *code, size_t len,
		 const char *filename);

/* Evaluate JavaScript code as an ES module
 * Supports static import/export statements at the top level
 * Executes the job queue to resolve module promises
 * filename: Source name for error messages (e.g., "server.js")
 * Returns 0 on success, -1 on error
 * Error details available via hbf_qjs_get_error()
 */
int hbf_qjs_eval_module(hbf_qjs_ctx_t *ctx, const char *code, size_t len,
			const char *filename);

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
