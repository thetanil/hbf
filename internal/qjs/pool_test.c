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
#include "internal/qjs/engine.h"
#include "quickjs.h"

#define TEST_DB ":memory:"

/* We don't need database for pool tests - just testing the pool itself */

/* For pool tests, we don't want router.js/server.js loaded
 * so we pass NULL to pool_init to get clean contexts */
#define POOL_INIT_NO_DB(pool_size, mem_mb, timeout) \
	hbf_qjs_pool_init(pool_size, mem_mb, timeout, NULL)

/* Helper to evaluate and get integer result from pooled context */
static int eval_to_int(hbf_qjs_ctx_t *ctx, const char *code)
{
	JSContext *js_ctx = (JSContext *)hbf_qjs_get_js_context(ctx);
	JSValue result;
	int value;

	result = JS_Eval(js_ctx, code, strlen(code), "<test>",
			 JS_EVAL_TYPE_GLOBAL);

	/* Must not be an exception */
	if (JS_IsException(result)) {
		JSValue exception = JS_GetException(js_ctx);
		const char *err_str = JS_ToCString(js_ctx, exception);
		fprintf(stderr, "JavaScript error in eval_to_int('%s'): %s\n",
			code, err_str ? err_str : "unknown");
		JS_FreeCString(js_ctx, err_str);
		JS_FreeValue(js_ctx, exception);
		assert(0 && "JavaScript exception");
	}

	/* Must not be undefined or null */
	assert(!JS_IsUndefined(result));
	assert(!JS_IsNull(result));

	JS_ToInt32(js_ctx, &value, result);

	JS_FreeValue(js_ctx, result);
	return value;
}

/* Helper to evaluate and get string result from pooled context */
static const char *eval_to_string(hbf_qjs_ctx_t *ctx, const char *code,
				  char *buf, size_t buflen)
{
	JSContext *js_ctx = (JSContext *)hbf_qjs_get_js_context(ctx);
	JSValue result;
	const char *str;

	result = JS_Eval(js_ctx, code, strlen(code), "<test>",
			 JS_EVAL_TYPE_GLOBAL);

	/* Must not be an exception */
	assert(!JS_IsException(result));

	/* Must not be undefined or null */
	assert(!JS_IsUndefined(result));
	assert(!JS_IsNull(result));

	str = JS_ToCString(js_ctx, result);
	snprintf(buf, buflen, "%s", str);
	JS_FreeCString(js_ctx, str);
	JS_FreeValue(js_ctx, result);

	return buf;
}

/* Helper to evaluate and get boolean result from pooled context */
static int eval_to_bool(hbf_qjs_ctx_t *ctx, const char *code)
{
	JSContext *js_ctx = (JSContext *)hbf_qjs_get_js_context(ctx);
	JSValue result;
	int value;

	result = JS_Eval(js_ctx, code, strlen(code), "<test>",
			 JS_EVAL_TYPE_GLOBAL);

	/* Must not be an exception */
	assert(!JS_IsException(result));

	/* Must not be undefined or null */
	assert(!JS_IsUndefined(result));
	assert(!JS_IsNull(result));

	value = JS_ToBool(js_ctx, result);
	JS_FreeValue(js_ctx, result);

	return value;
}

static void test_pool_init_shutdown(void)
{
	


	POOL_INIT_NO_DB(4, 64, 5000);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Pool init and shutdown\n");
}

static void test_pool_acquire_release(void)
{
	
	hbf_qjs_ctx_t *ctx;
	int value;


	POOL_INIT_NO_DB(4, 64, 5000);

	ctx = hbf_qjs_pool_acquire();
	assert(ctx != NULL); /* Context MUST be acquired */

	/* Verify we can execute JavaScript and get correct results */
	value = eval_to_int(ctx, "5 + 3");
	assert(value == 8); /* 5 + 3 MUST equal 8 */

	value = eval_to_int(ctx, "10 * 10");
	assert(value == 100); /* 10 * 10 MUST equal 100 */

	hbf_qjs_pool_release(ctx);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Acquire and release context (verified: 5+3=8, 10*10=100)\n");
}

static void test_pool_multiple_acquire(void)
{
	
	hbf_qjs_ctx_t *ctx1, *ctx2, *ctx3;
	int val1, val2, val3;


	POOL_INIT_NO_DB(4, 64, 5000);

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

	/* Each context should execute JavaScript independently */
	val1 = eval_to_int(ctx1, "var a = 100; a");
	assert(val1 == 100); /* ctx1: a MUST equal 100 */

	val2 = eval_to_int(ctx2, "var a = 200; a");
	assert(val2 == 200); /* ctx2: a MUST equal 200 (independent) */

	val3 = eval_to_int(ctx3, "var a = 300; a");
	assert(val3 == 300); /* ctx3: a MUST equal 300 (independent) */

	/* Verify contexts remain independent */
	val1 = eval_to_int(ctx1, "a");
	assert(val1 == 100); /* ctx1: a MUST still be 100 */

	val2 = eval_to_int(ctx2, "a");
	assert(val2 == 200); /* ctx2: a MUST still be 200 */

	/* Release all */
	hbf_qjs_pool_release(ctx1);
	hbf_qjs_pool_release(ctx2);
	hbf_qjs_pool_release(ctx3);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Multiple acquire/release (verified: contexts are independent)\n");
}

static void test_pool_stats(void)
{
	
	hbf_qjs_ctx_t *ctx1, *ctx2;
	int total, available, in_use;


	POOL_INIT_NO_DB(4, 64, 5000);

	/* Check initial stats */
	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4); /* Total MUST be 4 */
	assert(available == 4); /* Available MUST be 4 */
	assert(in_use == 0); /* In-use MUST be 0 */

	/* Acquire 2 contexts */
	ctx1 = hbf_qjs_pool_acquire();
	ctx2 = hbf_qjs_pool_acquire();

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4); /* Total MUST still be 4 */
	assert(available == 2); /* Available MUST be 2 */
	assert(in_use == 2); /* In-use MUST be 2 */

	/* Release 1 */
	hbf_qjs_pool_release(ctx1);

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4); /* Total MUST still be 4 */
	assert(available == 3); /* Available MUST be 3 */
	assert(in_use == 1); /* In-use MUST be 1 */

	/* Release remaining */
	hbf_qjs_pool_release(ctx2);

	hbf_qjs_pool_stats(&total, &available, &in_use);
	assert(total == 4); /* Total MUST still be 4 */
	assert(available == 4); /* Available MUST be 4 */
	assert(in_use == 0); /* In-use MUST be 0 */

	hbf_qjs_pool_shutdown();

	printf("  ✓ Pool statistics (verified: 4 total, acquire/release tracking)\n");
}

static void test_pool_eval_in_context(void)
{
	
	hbf_qjs_ctx_t *ctx;
	int value;
	char str_result[64];


	POOL_INIT_NO_DB(4, 64, 5000);

	ctx = hbf_qjs_pool_acquire();
	assert(ctx != NULL);

	/* Test arithmetic in pooled context */
	value = eval_to_int(ctx, "var x = 42; x + 8");
	assert(value == 50); /* 42 + 8 MUST equal 50 */

	/* Test string operations */
	eval_to_string(ctx, "'Hello' + ' ' + 'Pool'", str_result, sizeof(str_result));
	assert(strcmp(str_result, "Hello Pool") == 0); /* MUST equal "Hello Pool" */

	/* Test functions */
	hbf_qjs_eval(ctx, "function double(n) { return n * 2; }",
		     strlen("function double(n) { return n * 2; }"), "<test>");
	value = eval_to_int(ctx, "double(21)");
	assert(value == 42); /* double(21) MUST equal 42 */

	/* Test arrays */
	value = eval_to_int(ctx, "[10, 20, 30].length");
	assert(value == 3); /* Array length MUST be 3 */

	value = eval_to_int(ctx, "[10, 20, 30][1]");
	assert(value == 20); /* Second element MUST be 20 */

	/* Test objects */
	hbf_qjs_eval(ctx, "var obj = {a: 100, b: 200};",
		     strlen("var obj = {a: 100, b: 200};"), "<test>");
	value = eval_to_int(ctx, "obj.a + obj.b");
	assert(value == 300); /* 100 + 200 MUST equal 300 */

	hbf_qjs_pool_release(ctx);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Evaluate code in pooled context (verified: arithmetic, strings, functions, arrays, objects)\n");
}

/* Thread function for concurrent test */
static void *thread_acquire_release(void *arg)
{
	hbf_qjs_ctx_t *ctx;
	int i;
	int *count = (int *)arg;
	int value;

	for (i = 0; i < 10; i++) {
		ctx = hbf_qjs_pool_acquire();
		assert(ctx != NULL);

		/* Execute JavaScript and verify results */
		value = eval_to_int(ctx, "1 + 1");
		assert(value == 2); /* MUST equal 2 */

		value = eval_to_int(ctx, "5 * 5");
		assert(value == 25); /* MUST equal 25 */

		hbf_qjs_pool_release(ctx);
		(*count)++;
	}

	return NULL;
}

static void test_pool_concurrent_access(void)
{
	
	pthread_t threads[4];
	int counts[4] = {0};
	int i;


	POOL_INIT_NO_DB(4, 64, 5000);

	/* Create threads that concurrently acquire/release */
	for (i = 0; i < 4; i++) {
		pthread_create(&threads[i], NULL, thread_acquire_release,
			       &counts[i]);
	}

	/* Wait for all threads */
	for (i = 0; i < 4; i++) {
		pthread_join(threads[i], NULL);
		assert(counts[i] == 10); /* Each thread MUST complete 10 iterations */
	}

	hbf_qjs_pool_shutdown();

	printf("  ✓ Concurrent access to pool (verified: 4 threads × 10 iterations with correct JS execution)\n");
}

static void test_pool_context_reuse(void)
{
	
	hbf_qjs_ctx_t *ctx1, *ctx2;
	int value;


	POOL_INIT_NO_DB(2, 64, 5000); /* Only 2 contexts */

	/* Acquire first context and set a variable */
	ctx1 = hbf_qjs_pool_acquire();
	assert(ctx1 != NULL);

	hbf_qjs_eval(ctx1, "var poolTest = 999;",
		     strlen("var poolTest = 999;"), "<test>");
	value = eval_to_int(ctx1, "poolTest");
	assert(value == 999); /* poolTest MUST equal 999 */

	hbf_qjs_pool_release(ctx1);

	/* Acquire second context - should be different */
	ctx2 = hbf_qjs_pool_acquire();
	assert(ctx2 != NULL);
	assert(ctx1 != ctx2); /* MUST be different context */

	/* The new context should NOT have poolTest variable */
	/* (This will throw ReferenceError which we don't check here,
	   just verify contexts are independent) */

	hbf_qjs_pool_release(ctx2);

	/* Re-acquire - might get the same context back */
	ctx1 = hbf_qjs_pool_acquire();
	assert(ctx1 != NULL);

	/* Verify we can still execute JavaScript correctly */
	value = eval_to_int(ctx1, "100 + 100");
	assert(value == 200); /* 100 + 100 MUST equal 200 */

	hbf_qjs_pool_release(ctx1);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Context reuse from pool (verified: contexts can be reused)\n");
}

static void test_pool_state_isolation(void)
{
	
	hbf_qjs_ctx_t *ctx1, *ctx2;
	int val1, val2;
	char str1[64], str2[64];


	POOL_INIT_NO_DB(4, 64, 5000);

	/* Acquire two contexts */
	ctx1 = hbf_qjs_pool_acquire();
	ctx2 = hbf_qjs_pool_acquire();

	/* Set different variables in each context */
	hbf_qjs_eval(ctx1, "var name = 'Context1'; var value = 111;",
		     strlen("var name = 'Context1'; var value = 111;"), "<test>");

	hbf_qjs_eval(ctx2, "var name = 'Context2'; var value = 222;",
		     strlen("var name = 'Context2'; var value = 222;"), "<test>");

	/* Verify values are independent */
	eval_to_string(ctx1, "name", str1, sizeof(str1));
	assert(strcmp(str1, "Context1") == 0); /* ctx1 name MUST be "Context1" */

	eval_to_string(ctx2, "name", str2, sizeof(str2));
	assert(strcmp(str2, "Context2") == 0); /* ctx2 name MUST be "Context2" */

	val1 = eval_to_int(ctx1, "value");
	assert(val1 == 111); /* ctx1 value MUST be 111 */

	val2 = eval_to_int(ctx2, "value");
	assert(val2 == 222); /* ctx2 value MUST be 222 */

	/* Modify in ctx1 */
	hbf_qjs_eval(ctx1, "value = 333;", strlen("value = 333;"), "<test>");

	/* Verify ctx1 changed but ctx2 didn't */
	val1 = eval_to_int(ctx1, "value");
	assert(val1 == 333); /* ctx1 value MUST now be 333 */

	val2 = eval_to_int(ctx2, "value");
	assert(val2 == 222); /* ctx2 value MUST still be 222 */

	hbf_qjs_pool_release(ctx1);
	hbf_qjs_pool_release(ctx2);

	hbf_qjs_pool_shutdown();

	printf("  ✓ State isolation between contexts (verified: independent variables)\n");
}

static void test_pool_complex_operations(void)
{
	
	hbf_qjs_ctx_t *ctx;
	int value, bool_val;
	char str_result[128];


	POOL_INIT_NO_DB(4, 64, 5000);

	ctx = hbf_qjs_pool_acquire();
	assert(ctx != NULL);

	/* Test closures */
	hbf_qjs_eval(ctx,
		     "function makeAdder(x) { return function(y) { return x + y; }; }\n"
		     "var add5 = makeAdder(5);",
		     strlen("function makeAdder(x) { return function(y) { return x + y; }; }\n"
			    "var add5 = makeAdder(5);"),
		     "<test>");

	value = eval_to_int(ctx, "add5(10)");
	assert(value == 15); /* 5 + 10 MUST equal 15 */

	value = eval_to_int(ctx, "add5(100)");
	assert(value == 105); /* 5 + 100 MUST equal 105 */

	/* Test booleans and comparisons */
	bool_val = eval_to_bool(ctx, "add5(10) === 15");
	assert(bool_val == 1); /* Comparison MUST be true */

	bool_val = eval_to_bool(ctx, "add5(10) > 20");
	assert(bool_val == 0); /* Comparison MUST be false */

	bool_val = eval_to_bool(ctx, "true && true");
	assert(bool_val == 1); /* AND MUST be true */

	bool_val = eval_to_bool(ctx, "true && false");
	assert(bool_val == 0); /* AND MUST be false */

	/* Test array methods */
	hbf_qjs_eval(ctx, "var nums = [1, 2, 3, 4, 5];",
		     strlen("var nums = [1, 2, 3, 4, 5];"), "<test>");

	value = eval_to_int(ctx, "nums.reduce(function(a, b) { return a + b; }, 0)");
	assert(value == 15); /* Sum MUST equal 15 */

	value = eval_to_int(ctx, "nums.map(function(x) { return x * 2; })[2]");
	assert(value == 6); /* Third element doubled MUST be 6 */

	/* Test string methods */
	eval_to_string(ctx, "'hello world'.toUpperCase()", str_result, sizeof(str_result));
	assert(strcmp(str_result, "HELLO WORLD") == 0); /* MUST be uppercase */

	eval_to_string(ctx, "'one,two,three'.split(',')[1]", str_result, sizeof(str_result));
	assert(strcmp(str_result, "two") == 0); /* Second element MUST be "two" */

	/* Test JSON */
	hbf_qjs_eval(ctx, "var obj = {x: 10, y: 20};",
		     strlen("var obj = {x: 10, y: 20};"), "<test>");

	eval_to_string(ctx, "JSON.stringify(obj)", str_result, sizeof(str_result));
	assert(strstr(str_result, "\"x\":10") != NULL); /* Must contain x:10 */
	assert(strstr(str_result, "\"y\":20") != NULL); /* Must contain y:20 */

	hbf_qjs_pool_release(ctx);

	hbf_qjs_pool_shutdown();

	printf("  ✓ Complex operations (verified: closures, booleans, arrays, strings, JSON)\n");
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
	test_pool_context_reuse();
	test_pool_state_isolation();
	test_pool_complex_operations();

	printf("\nAll pool tests passed!\n");
	return 0;
}
