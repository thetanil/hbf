/* QuickJS context pool implementation */
#include "internal/qjs/pool.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "internal/core/log.h"
#include "internal/qjs/loader.h"

#define MAX_POOL_SIZE 64

/* Pool entry */
typedef struct pool_entry {
	hbf_qjs_ctx_t *ctx;
	int available; /* 1 if available, 0 if in use */
} pool_entry_t;

/* Global context pool */
static struct {
	pool_entry_t entries[MAX_POOL_SIZE];
	int pool_size;
	pthread_mutex_t mutex;
	pthread_cond_t cond; /* Signal when context becomes available */
	int initialized;
} g_pool = {0};

/* Initialize context pool */
int hbf_qjs_pool_init(int pool_size, size_t mem_limit_mb, int timeout_ms,
		      sqlite3 *db)
{
	int i;
	int rc;

	if (g_pool.initialized) {
		hbf_log_warn("QuickJS pool already initialized");
		return 0;
	}

	if (pool_size <= 0 || pool_size > MAX_POOL_SIZE) {
		hbf_log_error("Invalid pool size: %d (max: %d)",
			      pool_size, MAX_POOL_SIZE);
		return -1;
	}

	/* Initialize engine */
	rc = hbf_qjs_init(mem_limit_mb, timeout_ms);
	if (rc != 0) {
		hbf_log_error("Failed to initialize QuickJS engine");
		return -1;
	}

	/* Initialize mutex and condition variable */
	pthread_mutex_init(&g_pool.mutex, NULL);
	pthread_cond_init(&g_pool.cond, NULL);

	/* Pre-create contexts */
	hbf_log_info("Creating QuickJS context pool (size=%d)", pool_size);

	for (i = 0; i < pool_size; i++) {
		hbf_qjs_ctx_t *ctx = hbf_qjs_ctx_create();
		if (!ctx) {
			hbf_log_error("Failed to create context %d", i);
			/* Clean up partial pool */
			while (i > 0) {
				i--;
				hbf_qjs_ctx_destroy(g_pool.entries[i].ctx);
			}
			pthread_mutex_destroy(&g_pool.mutex);
			pthread_cond_destroy(&g_pool.cond);
			hbf_qjs_shutdown();
			return -1;
		}

		/* Load server.js into context */
		if (db) {
			rc = hbf_qjs_load_server_js(ctx, db);
			if (rc != 0) {
				hbf_log_warn("Failed to load server.js into context %d (will retry per-request)", i);
				/* Continue anyway - server.js might not exist yet */
			}
		}

		g_pool.entries[i].ctx = ctx;
		g_pool.entries[i].available = 1;
	}

	g_pool.pool_size = pool_size;
	g_pool.initialized = 1;

	hbf_log_info("QuickJS context pool initialized (%d contexts)", pool_size);
	return 0;
}

/* Shutdown context pool */
void hbf_qjs_pool_shutdown(void)
{
	int i;

	if (!g_pool.initialized)
		return;

	hbf_log_info("Shutting down QuickJS context pool");

	pthread_mutex_lock(&g_pool.mutex);

	/* Destroy all contexts */
	for (i = 0; i < g_pool.pool_size; i++) {
		if (g_pool.entries[i].ctx) {
			hbf_qjs_ctx_destroy(g_pool.entries[i].ctx);
			g_pool.entries[i].ctx = NULL;
		}
	}

	g_pool.pool_size = 0;
	g_pool.initialized = 0;

	pthread_mutex_unlock(&g_pool.mutex);

	pthread_mutex_destroy(&g_pool.mutex);
	pthread_cond_destroy(&g_pool.cond);

	hbf_qjs_shutdown();
}

/* Acquire context from pool (blocking) */
hbf_qjs_ctx_t *hbf_qjs_pool_acquire(void)
{
	int i;
	hbf_qjs_ctx_t *ctx = NULL;

	if (!g_pool.initialized) {
		hbf_log_error("QuickJS pool not initialized");
		return NULL;
	}

	pthread_mutex_lock(&g_pool.mutex);

	/* Wait for available context */
	while (1) {
		/* Find available context */
		for (i = 0; i < g_pool.pool_size; i++) {
			if (g_pool.entries[i].available) {
				g_pool.entries[i].available = 0;
				ctx = g_pool.entries[i].ctx;
				break;
			}
		}

		if (ctx) {
			break; /* Got one */
		}

		/* Wait for signal */
		hbf_log_debug("Waiting for available QuickJS context");
		pthread_cond_wait(&g_pool.cond, &g_pool.mutex);
	}

	pthread_mutex_unlock(&g_pool.mutex);

	hbf_log_debug("Acquired QuickJS context from pool");
	return ctx;
}

/* Release context back to pool */
void hbf_qjs_pool_release(hbf_qjs_ctx_t *ctx)
{
	int i;

	if (!ctx || !g_pool.initialized)
		return;

	pthread_mutex_lock(&g_pool.mutex);

	/* Find context in pool and mark available */
	for (i = 0; i < g_pool.pool_size; i++) {
		if (g_pool.entries[i].ctx == ctx) {
			g_pool.entries[i].available = 1;
			/* Signal waiting thread */
			pthread_cond_signal(&g_pool.cond);
			hbf_log_debug("Released QuickJS context to pool");
			break;
		}
	}

	pthread_mutex_unlock(&g_pool.mutex);
}

/* Get pool statistics */
void hbf_qjs_pool_stats(int *total, int *available, int *in_use)
{
	int i;
	int avail = 0;

	if (!g_pool.initialized) {
		*total = 0;
		*available = 0;
		*in_use = 0;
		return;
	}

	pthread_mutex_lock(&g_pool.mutex);

	for (i = 0; i < g_pool.pool_size; i++) {
		if (g_pool.entries[i].available)
			avail++;
	}

	*total = g_pool.pool_size;
	*available = avail;
	*in_use = g_pool.pool_size - avail;

	pthread_mutex_unlock(&g_pool.mutex);
}
