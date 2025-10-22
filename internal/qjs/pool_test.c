/* QuickJS pool tests */
#include "internal/qjs/pool.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/core/log.h"
#include "internal/db/db.h"
#include "internal/db/schema.h"

#define TEST_DB ":memory:"

static sqlite3 *setup_test_db(void)
{
	sqlite3 *db;
	int ret;

	ret = hbf_db_open(TEST_DB, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	return db;
}

static void test_pool_init_shutdown(void)
{
	sqlite3 *db;
	int ret;

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Pool init and shutdown\n");
}

static void test_pool_acquire_release(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	ctx = hbf_qjs_pool_acquire();
	assert(ctx != NULL);

	hbf_qjs_pool_release(ctx);

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Acquire and release context\n");
}

static void test_pool_multiple_acquire(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx1, *ctx2, *ctx3;
	int ret;

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	/* Acquire multiple contexts */
	ctx1 = hbf_qjs_pool_acquire();
	assert(ctx1 != NULL);

	ctx2 = hbf_qjs_pool_acquire();
	assert(ctx2 != NULL);

	ctx3 = hbf_qjs_pool_acquire();
	assert(ctx3 != NULL);

	/* Should be different contexts */
	assert(ctx1 != ctx2);
	assert(ctx2 != ctx3);
	assert(ctx1 != ctx3);

	/* Release all */
	hbf_qjs_pool_release(ctx1);
	hbf_qjs_pool_release(ctx2);
	hbf_qjs_pool_release(ctx3);

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Multiple acquire/release\n");
}

static void test_pool_stats(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx1, *ctx2;
	int total, available, in_use;
	int ret;

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	/* Check initial stats */
	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4);
	assert(available == 4);
	assert(in_use == 0);

	/* Acquire 2 contexts */
	ctx1 = hbf_qjs_pool_acquire();
	ctx2 = hbf_qjs_pool_acquire();

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4);
	assert(available == 2);
	assert(in_use == 2);

	/* Release 1 */
	hbf_qjs_pool_release(ctx1);

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4);
	assert(available == 3);
	assert(in_use == 1);

	/* Release remaining */
	hbf_qjs_pool_release(ctx2);

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4);
	assert(available == 4);
	assert(in_use == 0);

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Pool statistics\n");
}

static void test_pool_eval_in_context(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *code = "var x = 42; x + 8;";

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	ctx = hbf_qjs_pool_acquire();
	assert(ctx != NULL);

	/* Evaluate code in pooled context */
	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret == 0);

	hbf_qjs_pool_release(ctx);

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Evaluate code in pooled context\n");
}

/* Thread function for concurrent test */
static void *thread_acquire_release(void *arg)
{
	hbf_qjs_ctx_t *ctx;
	int i;
	int *count = (int *)arg;

	for (i = 0; i < 10; i++) {
		ctx = hbf_qjs_pool_acquire();
		assert(ctx != NULL);

		/* Do some work */
		const char *code = "1 + 1;";
		hbf_qjs_eval(ctx, code, strlen(code), "<test>");

		hbf_qjs_pool_release(ctx);
		(*count)++;
	}

	return NULL;
}

static void test_pool_concurrent_access(void)
{
	sqlite3 *db;
	pthread_t threads[4];
	int counts[4] = {0};
	int ret;
	int i;

	db = setup_test_db();

	ret = hbf_qjs_pool_init(4, 64, 5000, db);
	assert(ret == 0);

	/* Create threads that concurrently acquire/release */
	for (i = 0; i < 4; i++) {
		pthread_create(&threads[i], NULL, thread_acquire_release,
			       &counts[i]);
	}

	/* Wait for all threads */
	for (i = 0; i < 4; i++) {
		pthread_join(threads[i], NULL);
		assert(counts[i] == 10);
	}

	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Concurrent access to pool\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_set_level(HBF_LOG_WARN);

	printf("QuickJS Pool Tests:\n");

	test_pool_init_shutdown();
	test_pool_acquire_release();
	test_pool_multiple_acquire();
	test_pool_stats();
	test_pool_eval_in_context();
	test_pool_concurrent_access();

	printf("\nAll pool tests passed!\n");
	return 0;
}
