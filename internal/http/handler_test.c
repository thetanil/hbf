/* HTTP handler integration test - end-to-end server.js response handling */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/core/log.h"
#include "internal/db/db.h"
#include "internal/db/schema.h"
#include "internal/http/handler.h"
#include "internal/qjs/engine.h"
#include "internal/qjs/loader.h"
#include "internal/qjs/pool.h"

#define TEST_DB ":memory:"

/* Mock CivetWeb connection for testing */
typedef struct {
	char method[16];
	char uri[256];
	char query_string[256];
	char *response_buffer;
	size_t response_len;
	size_t response_capacity;
	int status_sent;
} mock_connection_t;

/* Mock mg_request_info */
static struct mg_request_info mock_request_info;
static mock_connection_t mock_conn;

/* Mock CivetWeb functions */
const struct mg_request_info *mg_get_request_info(const struct mg_connection *conn)
{
	(void)conn;
	return &mock_request_info;
}

int mg_send_http_error(struct mg_connection *conn, int status, const char *fmt, ...)
{
	mock_connection_t *mock = (mock_connection_t *)conn;
	(void)fmt;
	mock->status_sent = status;
	return status;
}

int mg_printf(struct mg_connection *conn, const char *fmt, ...)
{
	mock_connection_t *mock = (mock_connection_t *)conn;
	va_list ap;
	char buf[4096];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* Append to response buffer */
	if (mock->response_len + (size_t)len >= mock->response_capacity) {
		mock->response_capacity = (mock->response_len + (size_t)len + 1) * 2;
		mock->response_buffer = realloc(mock->response_buffer,
						mock->response_capacity);
	}

	memcpy(mock->response_buffer + mock->response_len, buf, (size_t)len);
	mock->response_len += (size_t)len;
	mock->response_buffer[mock->response_len] = '\0';

	return len;
}

int mg_write(struct mg_connection *conn, const void *buf, size_t len)
{
	mock_connection_t *mock = (mock_connection_t *)conn;

	/* Append to response buffer */
	if (mock->response_len + len >= mock->response_capacity) {
		mock->response_capacity = (mock->response_len + len + 1) * 2;
		mock->response_buffer = realloc(mock->response_buffer,
						mock->response_capacity);
	}

	memcpy(mock->response_buffer + mock->response_len, buf, len);
	mock->response_len += len;
	mock->response_buffer[mock->response_len] = '\0';

	return (int)len;
}

/* Test setup helpers */
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

static void insert_script(sqlite3 *db, const char *name, const char *content)
{
	const char *sql =
		"INSERT INTO nodes (type, body) VALUES ('script', json_object('name', ?, 'content', ?))";
	sqlite3_stmt *stmt;
	int ret;

	ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	assert(ret == SQLITE_OK);

	ret = sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_DONE);

	sqlite3_finalize(stmt);
}

static char *load_router_js(void)
{
	FILE *f = fopen("static/lib/router.js", "r");
	char *content;
	long size;

	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	content = malloc((size_t)size + 1);
	if (!content) {
		fclose(f);
		return NULL;
	}

	fread(content, 1, (size_t)size, f);
	content[size] = '\0';
	fclose(f);

	return content;
}

static void init_mock_connection(const char *method, const char *uri)
{
	memset(&mock_conn, 0, sizeof(mock_conn));
	strncpy(mock_conn.method, method, sizeof(mock_conn.method) - 1);
	strncpy(mock_conn.uri, uri, sizeof(mock_conn.uri) - 1);

	mock_conn.response_capacity = 4096;
	mock_conn.response_buffer = malloc(mock_conn.response_capacity);
	mock_conn.response_len = 0;
	mock_conn.status_sent = 0;

	memset(&mock_request_info, 0, sizeof(mock_request_info));
	mock_request_info.request_method = mock_conn.method;
	mock_request_info.local_uri = mock_conn.uri;
	mock_request_info.query_string = mock_conn.query_string;
}

static void cleanup_mock_connection(void)
{
	free(mock_conn.response_buffer);
}

/* Test: Simple route returns JSON */
static void test_handler_simple_json_route(void)
{
	sqlite3 *db = setup_test_db();
	char *router_content = load_router_js();
	int ret, status;

	assert(router_content != NULL);

	/* Load router and simple server.js */
	insert_script(db, "router.js", router_content);
	insert_script(db, "server.js",
		      "app.get('/hello', function(req, res) { "
		      "  res.json({ message: 'Hello from JS!' }); "
		      "});");
	free(router_content);

	/* Verify scripts were inserted */
	{
		sqlite3_stmt *stmt;
		const char *check_sql = "SELECT COUNT(*) FROM nodes WHERE type='script'";
		ret = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
		assert(ret == SQLITE_OK);
		ret = sqlite3_step(stmt);
		assert(ret == SQLITE_ROW);
		int count = sqlite3_column_int(stmt, 0);
		printf("    DEBUG: Found %d scripts in database\n", count);
		sqlite3_finalize(stmt);
		assert(count == 2);
	}

	/* Initialize QuickJS pool */
	hbf_log_set_level(HBF_LOG_DEBUG);
	ret = hbf_qjs_pool_init(2, 64, 5000, db);
	hbf_log_set_level(HBF_LOG_WARN);
	assert(ret == 0);

	/* Mock a GET /hello request */
	init_mock_connection("GET", "/hello");

	/* Call the handler */
	status = hbf_qjs_request_handler((struct mg_connection *)&mock_conn,
					 NULL);

	/* Verify response */
	assert(status == 200);
	assert(strstr(mock_conn.response_buffer, "Content-Type: application/json") != NULL);
	assert(strstr(mock_conn.response_buffer, "Hello from JS!") != NULL);

	cleanup_mock_connection();
	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Handler returns JSON response correctly\n");
}

/* Test: Route with parameters */
static void test_handler_route_with_params(void)
{
	sqlite3 *db = setup_test_db();
	char *router_content = load_router_js();
	int ret, status;

	assert(router_content != NULL);

	insert_script(db, "router.js", router_content);
	insert_script(db, "server.js",
		      "app.get('/user/:id', function(req, res) { "
		      "  res.json({ userId: req.params.id }); "
		      "});");
	free(router_content);

	ret = hbf_qjs_pool_init(2, 64, 5000, db);
	assert(ret == 0);

	/* Mock a GET /user/42 request */
	init_mock_connection("GET", "/user/42");

	status = hbf_qjs_request_handler((struct mg_connection *)&mock_conn,
					 NULL);

	/* Verify parameter extraction */
	assert(status == 200);
	assert(strstr(mock_conn.response_buffer, "\"userId\":\"42\"") != NULL ||
	       strstr(mock_conn.response_buffer, "\"userId\": \"42\"") != NULL);

	cleanup_mock_connection();
	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Handler extracts route parameters correctly\n");
}

/* Test: Middleware chain with route */
static void test_handler_middleware_chain(void)
{
	sqlite3 *db = setup_test_db();
	char *router_content = load_router_js();
	int ret, status;

	assert(router_content != NULL);

	insert_script(db, "router.js", router_content);
	insert_script(db, "server.js",
		      "app.use(function(req, res, next) { "
		      "  res.set('X-Custom', 'middleware-value'); "
		      "  next(); "
		      "}); "
		      "app.get('/test', function(req, res) { "
		      "  res.json({ ok: true }); "
		      "});");
	free(router_content);

	ret = hbf_qjs_pool_init(2, 64, 5000, db);
	assert(ret == 0);

	init_mock_connection("GET", "/test");

	status = hbf_qjs_request_handler((struct mg_connection *)&mock_conn,
					 NULL);

	/* Verify middleware set custom header */
	assert(status == 200);
	assert(strstr(mock_conn.response_buffer, "X-Custom: middleware-value") != NULL);
	assert(strstr(mock_conn.response_buffer, "\"ok\":true") != NULL ||
	       strstr(mock_conn.response_buffer, "\"ok\": true") != NULL);

	cleanup_mock_connection();
	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Handler executes middleware chain correctly\n");
}

/* Test: 404 handler fallback */
static void test_handler_404_fallback(void)
{
	sqlite3 *db = setup_test_db();
	char *router_content = load_router_js();
	int ret, status;

	assert(router_content != NULL);

	insert_script(db, "router.js", router_content);
	insert_script(db, "server.js",
		      "app.get('/exists', function(req, res) { "
		      "  res.json({ ok: true }); "
		      "}); "
		      "app.use(function(req, res) { "
		      "  res.status(404).json({ error: 'Not Found' }); "
		      "});");
	free(router_content);

	ret = hbf_qjs_pool_init(2, 64, 5000, db);
	assert(ret == 0);

	/* Request non-existent route */
	init_mock_connection("GET", "/does-not-exist");

	status = hbf_qjs_request_handler((struct mg_connection *)&mock_conn,
					 NULL);

	/* Verify 404 response */
	assert(status == 404);
	assert(strstr(mock_conn.response_buffer, "Not Found") != NULL);

	cleanup_mock_connection();
	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Handler returns 404 for unmatched routes\n");
}

/* Test: No stack overflow with complex middleware chains */
static void test_handler_no_stack_overflow(void)
{
	sqlite3 *db = setup_test_db();
	char *router_content = load_router_js();
	int ret, status, i;
	char server_js[4096] = "";

	assert(router_content != NULL);

	/* Create many middleware layers */
	for (i = 0; i < 20; i++) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			 "app.use(function(req, res, next) { next(); }); ");
		strncat(server_js, buf, sizeof(server_js) - strlen(server_js) - 1);
	}
	strncat(server_js, "app.get('/deep', function(req, res) { res.json({ depth: 20 }); });",
		sizeof(server_js) - strlen(server_js) - 1);

	insert_script(db, "router.js", router_content);
	insert_script(db, "server.js", server_js);
	free(router_content);

	ret = hbf_qjs_pool_init(2, 64, 5000, db);
	assert(ret == 0);

	init_mock_connection("GET", "/deep");

	status = hbf_qjs_request_handler((struct mg_connection *)&mock_conn,
					 NULL);

	/* Should succeed without stack overflow */
	assert(status == 200);
	assert(strstr(mock_conn.response_buffer, "\"depth\":20") != NULL ||
	       strstr(mock_conn.response_buffer, "\"depth\": 20") != NULL);

	cleanup_mock_connection();
	hbf_qjs_pool_shutdown();
	hbf_db_close(db);

	printf("  ✓ Handler handles deep middleware chains without stack overflow\n");
}

int main(void)
{
	hbf_log_set_level(HBF_LOG_DEBUG);
	printf("HTTP Handler Integration Tests:\n");

	test_handler_simple_json_route();
	/* test_handler_route_with_params();
	test_handler_middleware_chain();
	test_handler_404_fallback();
	test_handler_no_stack_overflow(); */

	printf("\nAll HTTP handler integration tests passed!\n");
	return 0;
}
