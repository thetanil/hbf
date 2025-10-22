/* QuickJS engine tests */
#include "internal/qjs/engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "internal/core/log.h"
#include "quickjs.h"

/* Helper to evaluate and get integer result */
static int eval_to_int(hbf_qjs_ctx_t *ctx, const char *code)
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

	JS_ToInt32(js_ctx, &value, result);

	JS_FreeValue(js_ctx, result);
	return value;
}

/* Helper to evaluate and get string result */
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

/* Helper to evaluate and get boolean result */
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

static void test_init_shutdown(void)
{
	hbf_qjs_init(64, 5000);
	hbf_qjs_shutdown();

	printf("  ✓ Init and shutdown\n");
}

static void test_ctx_create_destroy(void)
{
	hbf_qjs_ctx_t *ctx;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL); /* Context MUST be created */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Create and destroy context\n");
}

static void test_eval_simple_expression(void)
{
	hbf_qjs_ctx_t *ctx;
	JSContext *js_ctx;
	JSValue result;
	int value;
	const char *code = "1 + 1;";

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	js_ctx = (JSContext *)hbf_qjs_get_js_context(ctx);

	/* Evaluate and get the actual result */
	result = JS_Eval(js_ctx, code, strlen(code), "<test>",
			 JS_EVAL_TYPE_GLOBAL);

	/* Check result is valid */
	assert(!JS_IsException(result));
	assert(!JS_IsUndefined(result));
	assert(!JS_IsNull(result));

	/* Convert to integer and verify it's 2 */
	JS_ToInt32(js_ctx, &value, result);
	assert(value == 2); /* "1 + 1" MUST equal 2 */

	JS_FreeValue(js_ctx, result);

	/* Test more expressions */
	value = eval_to_int(ctx, "2 * 3");
	assert(value == 6); /* "2 * 3" MUST equal 6 */

	value = eval_to_int(ctx, "10 - 7");
	assert(value == 3); /* "10 - 7" MUST equal 3 */

	value = eval_to_int(ctx, "100 / 4");
	assert(value == 25); /* "100 / 4" MUST equal 25 */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Evaluate simple expression (verified: 1+1=2, 2*3=6, 10-7=3, 100/4=25)\n");
}

static void test_eval_error(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *code = "syntax error here!";
	const char *error;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret != 0); /* MUST fail for syntax error */

	error = hbf_qjs_get_error(ctx);
	assert(error != NULL); /* Error message MUST exist */
	assert(strlen(error) > 0); /* Error message MUST have content */
	assert(strstr(error, "SyntaxError") != NULL); /* MUST contain "SyntaxError" */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Error handling for syntax error (verified: contains 'SyntaxError')\n");
}

static void test_eval_function(void)
{
	hbf_qjs_ctx_t *ctx;
	char result[64];
	const char *code = "function greet(name) { return 'Hello, ' + name; }";

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	hbf_qjs_eval(ctx, code, strlen(code), "<test>");

	/* Verify function was created and can be called */
	eval_to_string(ctx, "greet('World')", result, sizeof(result));
	assert(strcmp(result, "Hello, World") == 0); /* MUST return 'Hello, World' */

	eval_to_string(ctx, "greet('Alice')", result, sizeof(result));
	assert(strcmp(result, "Hello, Alice") == 0); /* MUST return 'Hello, Alice' */

	eval_to_string(ctx, "greet('Bob')", result, sizeof(result));
	assert(strcmp(result, "Hello, Bob") == 0); /* MUST return 'Hello, Bob' */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Evaluate function definition (verified: greet() returns correct strings)\n");
}

static void test_timeout_enforcement(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;
	/* Infinite loop - should timeout */
	const char *code = "while(true) {}";

	hbf_qjs_init(64, 100); /* 100ms timeout */

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	ret = hbf_qjs_eval(ctx, code, strlen(code), "<test>");
	assert(ret != 0); /* MUST fail due to timeout */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Timeout enforcement (verified: infinite loop times out)\n");
}

static void test_variables_and_state(void)
{
	hbf_qjs_ctx_t *ctx;
	int result;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Set variables */
	hbf_qjs_eval(ctx, "var x = 10; var y = 20;",
		     strlen("var x = 10; var y = 20;"), "<test>");

	/* Verify variables persist */
	result = eval_to_int(ctx, "x");
	assert(result == 10); /* x MUST equal 10 */

	result = eval_to_int(ctx, "y");
	assert(result == 20); /* y MUST equal 20 */

	/* Verify arithmetic with variables */
	result = eval_to_int(ctx, "x + y");
	assert(result == 30); /* 10 + 20 MUST equal 30 */

	result = eval_to_int(ctx, "x * y");
	assert(result == 200); /* 10 * 20 MUST equal 200 */

	/* Modify variables */
	hbf_qjs_eval(ctx, "x = 5;", strlen("x = 5;"), "<test>");

	result = eval_to_int(ctx, "x");
	assert(result == 5); /* x MUST now equal 5 */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Variables and state persistence (verified: x=10, y=20, x+y=30, modified x=5)\n");
}

static void test_objects_and_properties(void)
{
	hbf_qjs_ctx_t *ctx;
	char result[64];
	int num_result;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Create object */
	hbf_qjs_eval(ctx,
			   "var person = { name: 'Alice', age: 30, city: 'NYC' };",
			   strlen("var person = { name: 'Alice', age: 30, city: 'NYC' };"),
			   "<test>");

	/* Verify string property */
	eval_to_string(ctx, "person.name", result, sizeof(result));
	assert(strcmp(result, "Alice") == 0);

	eval_to_string(ctx, "person.city", result, sizeof(result));
	assert(strcmp(result, "NYC") == 0);

	/* Verify numeric property */
	num_result = eval_to_int(ctx, "person.age");
	assert(num_result == 30);

	/* Modify property */
	hbf_qjs_eval(ctx, "person.age = 31;",
			   strlen("person.age = 31;"), "<test>");

	num_result = eval_to_int(ctx, "person.age");
	assert(num_result == 31);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Objects and properties (verified: {name:'Alice', age:30})\n");
}

static void test_arrays_and_methods(void)
{
	hbf_qjs_ctx_t *ctx;
	int result;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Create array */
	hbf_qjs_eval(ctx, "var arr = [1, 2, 3, 4, 5];",
			   strlen("var arr = [1, 2, 3, 4, 5];"), "<test>");

	/* Verify array length */
	result = eval_to_int(ctx, "arr.length");
	assert(result == 5);

	/* Verify array elements */
	result = eval_to_int(ctx, "arr[0]");
	assert(result == 1);

	result = eval_to_int(ctx, "arr[4]");
	assert(result == 5);

	/* Use array methods */
	hbf_qjs_eval(ctx, "arr.push(6);", strlen("arr.push(6);"),
			   "<test>");

	result = eval_to_int(ctx, "arr.length");
	assert(result == 6);

	result = eval_to_int(ctx, "arr[5]");
	assert(result == 6);

	/* Verify reduce */
	result = eval_to_int(ctx,
			     "arr.reduce(function(a, b) { return a + b; }, 0)");
	assert(result == 21); /* 1+2+3+4+5+6 = 21 */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Arrays and methods (verified: [1,2,3,4,5], sum=21)\n");
}

static void test_closures_and_scope(void)
{
	hbf_qjs_ctx_t *ctx;
	int result;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Create closure */
	hbf_qjs_eval(ctx,
			   "function makeCounter() {\n"
			   "  var count = 0;\n"
			   "  return function() {\n"
			   "    return ++count;\n"
			   "  };\n"
			   "}\n"
			   "var counter = makeCounter();",
			   strlen("function makeCounter() {\n"
				  "  var count = 0;\n"
				  "  return function() {\n"
				  "    return ++count;\n"
				  "  };\n"
				  "}\n"
				  "var counter = makeCounter();"),
			   "<test>");

	/* Call closure multiple times */
	result = eval_to_int(ctx, "counter()");
	assert(result == 1);

	result = eval_to_int(ctx, "counter()");
	assert(result == 2);

	result = eval_to_int(ctx, "counter()");
	assert(result == 3);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Closures and scope (verified: counter increments 1,2,3)\n");
}

static void test_boolean_logic(void)
{
	hbf_qjs_ctx_t *ctx;
	int result;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Test boolean values */
	result = eval_to_bool(ctx, "true");
	assert(result == 1);

	result = eval_to_bool(ctx, "false");
	assert(result == 0);

	/* Test comparisons */
	result = eval_to_bool(ctx, "5 > 3");
	assert(result == 1);

	result = eval_to_bool(ctx, "5 < 3");
	assert(result == 0);

	result = eval_to_bool(ctx, "10 === 10");
	assert(result == 1);

	result = eval_to_bool(ctx, "'hello' === 'hello'");
	assert(result == 1);

	result = eval_to_bool(ctx, "'hello' === 'world'");
	assert(result == 0);

	/* Test logical operators */
	result = eval_to_bool(ctx, "true && true");
	assert(result == 1);

	result = eval_to_bool(ctx, "true && false");
	assert(result == 0);

	result = eval_to_bool(ctx, "true || false");
	assert(result == 1);

	result = eval_to_bool(ctx, "!false");
	assert(result == 1);

	result = eval_to_bool(ctx, "!true");
	assert(result == 0);

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Boolean logic (verified: true/false, comparisons, logical ops)\n");
}

static void test_console_log(void)
{
	hbf_qjs_ctx_t *ctx;
	int ret;

	hbf_qjs_init(64, 5000);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Test console.log with single argument */
	ret = hbf_qjs_eval(ctx, "console.log('Hello, world!');",
			   strlen("console.log('Hello, world!');"), "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.log with multiple arguments */
	ret = hbf_qjs_eval(ctx, "console.log('The answer is', 42);",
			   strlen("console.log('The answer is', 42);"),
			   "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.log with numbers */
	ret = hbf_qjs_eval(ctx, "console.log(1, 2, 3, 4, 5);",
			   strlen("console.log(1, 2, 3, 4, 5);"), "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.log with objects */
	ret = hbf_qjs_eval(ctx,
			   "console.log({name: 'Alice', age: 30});",
			   strlen("console.log({name: 'Alice', age: 30});"),
			   "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.warn */
	ret = hbf_qjs_eval(ctx, "console.warn('This is a warning');",
			   strlen("console.warn('This is a warning');"),
			   "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.error */
	ret = hbf_qjs_eval(ctx, "console.error('This is an error');",
			   strlen("console.error('This is an error');"),
			   "<test>");
	assert(ret == 0); /* MUST succeed */

	/* Test console.debug */
	ret = hbf_qjs_eval(ctx, "console.debug('Debug message');",
			   strlen("console.debug('Debug message');"),
			   "<test>");
	assert(ret == 0); /* MUST succeed */

	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();

	printf("  ✓ Console module (verified: log/warn/error/debug work)\n");
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
	test_variables_and_state();
	test_objects_and_properties();
	test_arrays_and_methods();
	test_closures_and_scope();
	test_boolean_logic();
	test_console_log();

	/* DB module tests */
	hbf_qjs_init(64, 5000);
	hbf_qjs_ctx_t *ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);
	JSContext *js_ctx = (JSContext *)hbf_qjs_get_js_context(ctx);

	/* Create test table and insert data */
	hbf_qjs_eval(ctx, "db.execute('CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)')", strlen("db.execute('CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)')"), "<test>");
	hbf_qjs_eval(ctx, "db.execute('INSERT INTO test (name) VALUES (?)', ['Alice'])", strlen("db.execute('INSERT INTO test (name) VALUES (?)', ['Alice'])"), "<test>");
	hbf_qjs_eval(ctx, "db.execute('INSERT INTO test (name) VALUES (?)', ['Bob'])", strlen("db.execute('INSERT INTO test (name) VALUES (?)', ['Bob'])"), "<test>");

	/* Query data */
	JSValue result = JS_Eval(js_ctx, "db.query('SELECT * FROM test ORDER BY id')", strlen("db.query('SELECT * FROM test ORDER BY id')"), "<test>", JS_EVAL_TYPE_GLOBAL);
	assert(JS_IsArray(js_ctx, result));
	int64_t len = 0;
	JSValue len_val = JS_GetPropertyStr(js_ctx, result, "length");
	JS_ToInt64(js_ctx, &len, len_val);
	JS_FreeValue(js_ctx, len_val);
	assert(len == 2);
	JSValue row0 = JS_GetPropertyUint32(js_ctx, result, 0);
	JSValue row1 = JS_GetPropertyUint32(js_ctx, result, 1);
	JSValue name0 = JS_GetPropertyStr(js_ctx, row0, "name");
	JSValue name1 = JS_GetPropertyStr(js_ctx, row1, "name");
	const char *s0 = JS_ToCString(js_ctx, name0);
	const char *s1 = JS_ToCString(js_ctx, name1);
	assert(strcmp(s0, "Alice") == 0);
	assert(strcmp(s1, "Bob") == 0);
	JS_FreeCString(js_ctx, s0);
	JS_FreeCString(js_ctx, s1);
	JS_FreeValue(js_ctx, name0);
	JS_FreeValue(js_ctx, name1);
	JS_FreeValue(js_ctx, row0);
	JS_FreeValue(js_ctx, row1);
	JS_FreeValue(js_ctx, result);

	/* Update data and check affected rows */
	result = JS_Eval(js_ctx, "db.execute('UPDATE test SET name = ? WHERE id = 1', ['Carol'])", strlen("db.execute('UPDATE test SET name = ? WHERE id = 1', ['Carol'])"), "<test>", JS_EVAL_TYPE_GLOBAL);
	int affected;
	JS_ToInt32(js_ctx, &affected, result);
	assert(affected == 1);
	JS_FreeValue(js_ctx, result);

	/* Query updated row */
	result = JS_Eval(js_ctx, "db.query('SELECT name FROM test WHERE id = 1')", strlen("db.query('SELECT name FROM test WHERE id = 1')"), "<test>", JS_EVAL_TYPE_GLOBAL);
	row0 = JS_GetPropertyUint32(js_ctx, result, 0);
	name0 = JS_GetPropertyStr(js_ctx, row0, "name");
	s0 = JS_ToCString(js_ctx, name0);
	assert(strcmp(s0, "Carol") == 0);
	JS_FreeCString(js_ctx, s0);
	JS_FreeValue(js_ctx, name0);
	JS_FreeValue(js_ctx, row0);
	JS_FreeValue(js_ctx, result);

	JS_RunGC(ctx->rt);
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();
	printf("  ✓ DB module: db.query and db.execute work as expected\n");

	printf("\nAll engine tests passed!\n");
	return 0;
}
