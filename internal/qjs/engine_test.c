/* QuickJS engine tests */
#include "internal/qjs/engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "internal/core/log.h"

static void test_init_shutdown(void)
{
	int ret;

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	hbf_qjs_shutdown();

	printf("  ✓ Init and shutdown\n");
}

static void test_ctx_create_destroy(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Create and destroy context\n");
}

static void test_eval_simple_expression(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *code = "1 + 1;";

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret == 0);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Evaluate simple expression\n");
}

static void test_eval_error(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *code = "syntax error here!";
	const char *error;

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret != 0);

	error = hbf_qjs_get_error(ctx);
	assert(error != NULL);
	assert(strlen(error) > 0);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Error handling for syntax error\n");
}

static void test_eval_function(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *code = "function greet(name) { return 'Hello, ' + name; }";

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret == 0);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Evaluate function definition\n");
}

static void test_timeout_enforcement(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	/* Infinite loop - should timeout */
	const char *code = "while(true) {}";

	ret = hbf_qjs_init(64, 100); /* 100ms timeout */
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	/* Should fail due to timeout */
	assert(ret != 0);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Timeout enforcement\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_set_level(HBF_LOG_WARN);

	printf("QuickJS Engine Tests:\n");

	test_init_shutdown();
	test_ctx_create_destroy();
	test_eval_simple_expression();
	test_eval_error();
	test_eval_function();
	test_timeout_enforcement();

	printf("\nAll engine tests passed!\n");
	return 0;
}
