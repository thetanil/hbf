/* Simple QuickJS test utility */
#include <stdio.h>
#include <string.h>

#include "internal/core/log.h"
#include "internal/qjs/engine.h"

int main(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;

	/* Initialize logging */
	hbf_log_set_level(HBF_LOG_INFO);
	hbf_log_info("QuickJS Test Utility");

	/* Initialize QuickJS engine */
	ret = hbf_qjs_init(64, 5000);
	if (ret != 0) {
		hbf_log_error("Failed to initialize QuickJS");
		return 1;
	}

	/* Create context */
	ctx = hbf_qjs_ctx_create();
	if (!ctx) {
		hbf_log_error("Failed to create context");
		hbf_qjs_shutdown();
		return 1;
	}

	hbf_log_info("QuickJS context created successfully");

	/* Test 1: Simple arithmetic */
	printf("\nTest 1: Simple arithmetic (2 + 2)\n");
	const char *code1 = "2 + 2;";
	ret = hbf_qjs_eval(ctx, code1, strlen(code1), "<test>");
	if (ret == 0) {
		printf("✓ SUCCESS: Evaluated '%s'\n", code1);
	} else {
		printf("✗ FAILED: %s\n", hbf_qjs_get_error(ctx));
	}

	/* Test 2: String concatenation */
	printf("\nTest 2: String concatenation\n");
	const char *code2 = "'Hello, ' + 'QuickJS!';";
	ret = hbf_qjs_eval(ctx, code2, strlen(code2), "<test>");
	if (ret == 0) {
		printf("✓ SUCCESS: Evaluated '%s'\n", code2);
	} else {
		printf("✗ FAILED: %s\n", hbf_qjs_get_error(ctx));
	}

	/* Test 3: Function definition and call */
	printf("\nTest 3: Function definition\n");
	const char *code3 = "function greet(name) { return 'Hello, ' + name + '!'; }; greet('HBF');";
	ret = hbf_qjs_eval(ctx, code3, strlen(code3), "<test>");
	if (ret == 0) {
		printf("✓ SUCCESS: Defined and called function\n");
	} else {
		printf("✗ FAILED: %s\n", hbf_qjs_get_error(ctx));
	}

	/* Test 4: Object creation */
	printf("\nTest 4: Object creation\n");
	const char *code4 = "var obj = {name: 'HBF', version: '0.1.0'}; obj.name;";
	ret = hbf_qjs_eval(ctx, code4, strlen(code4), "<test>");
	if (ret == 0) {
		printf("✓ SUCCESS: Created and accessed object\n");
	} else {
		printf("✗ FAILED: %s\n", hbf_qjs_get_error(ctx));
	}

	/* Test 5: Array operations */
	printf("\nTest 5: Array operations\n");
	const char *code5 = "var arr = [1, 2, 3]; arr.map(function(x) { return x * 2; });";
	ret = hbf_qjs_eval(ctx, code5, strlen(code5), "<test>");
	if (ret == 0) {
		printf("✓ SUCCESS: Array map operation\n");
	} else {
		printf("✗ FAILED: %s\n", hbf_qjs_get_error(ctx));
	}

	/* Test 6: Error handling */
	printf("\nTest 6: Error handling (intentional syntax error)\n");
	const char *code6 = "this is invalid syntax!";
	ret = hbf_qjs_eval(ctx, code6, strlen(code6), "<test>");
	if (ret != 0) {
		printf("✓ SUCCESS: Error caught: %s\n", hbf_qjs_get_error(ctx));
	} else {
		printf("✗ FAILED: Should have caught syntax error\n");
	}

	/* Cleanup */
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("\n=== All tests completed ===\n");
	hbf_log_info("QuickJS test completed successfully");

	return 0;
}
