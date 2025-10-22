/* Router recursion/stack overflow test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/core/log.h"
#include "internal/db/db.h"
#include "internal/db/schema.h"
#include "internal/qjs/engine.h"
#include "internal/qjs/loader.h"

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

static void update_script(sqlite3 *db, const char *name, const char *content)
{
    sqlite3_stmt *stmt;
    int ret;

    /* Update existing script content using UPDATE instead of DELETE+INSERT
     * This preserves the original row ID and doesn't interfere with other tests */
    const char *sql =
        "UPDATE nodes SET body = json_object('name', ?, 'content', ?) "
        "WHERE type='script' AND json_extract(body, '$.name') = ?";
    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    assert(ret == SQLITE_OK);

    ret = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    assert(ret == SQLITE_OK);

    ret = sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    assert(ret == SQLITE_OK);

    ret = sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);
    assert(ret == SQLITE_OK);

    ret = sqlite3_step(stmt);
    assert(ret == SQLITE_DONE);

    sqlite3_finalize(stmt);
}

/* router.js is now loaded from the database schema, no need to load from filesystem */

/* server with non-recursive route */
static const char *server_js_non_recursive =
"app.get('/hello', function(req, res){ res._sent = true; });\n";

/* A faulty server that would cause recursion if app.handle calls app.handle */
static const char *server_js_faulty =
"app.use(function(req, res, next){ app.handle(req, res); });\n";

/* server with next() middleware chain */
static const char *server_js_with_next =
"app.use(function(req, res, next){ req._middleware1 = true; next(); });\n"
"app.get('/hello', function(req, res){ res._sent = true; res._hasMiddleware1 = req._middleware1; });\n"
"app.use(function(req, res){ res._fallback = true; });\n";

static void test_router_handle_simple(void)
{
    sqlite3 *db = setup_test_db();
    int ret;
    hbf_qjs_ctx_t *ctx;

    /* router.js is already in schema, just override server.js for this test */
    update_script(db, "server.js", server_js_non_recursive);

    ret = hbf_qjs_init(64, 2000);
    assert(ret == 0);

    ctx = hbf_qjs_ctx_create();
    assert(ctx);

    /* load router (from schema) then server (our test version) */
    ret = hbf_qjs_load_script(ctx, db, "router.js");
    assert(ret == 0);
    ret = hbf_qjs_load_server_js(ctx, db);
    assert(ret == 0);

    /* Now simulate a request by invoking app.handle with JS objects */
    const char *invoke =
        "var req={method:'GET', path:'/hello', params:{}};\n"
        "var res={};\n"
        "app.handle(req,res);\n";

    ret = hbf_qjs_eval(ctx, invoke, strlen(invoke));
    assert(ret == 0);

    hbf_qjs_ctx_destroy(ctx);
    hbf_qjs_shutdown();
    hbf_db_close(db);

    printf("  ✓ Router handle simple route without recursion\n");
}

static void test_router_no_stack_overflow_on_404(void)
{
    sqlite3 *db = setup_test_db();
    int ret;
    hbf_qjs_ctx_t *ctx;

    /* router.js is already in schema, just override server.js for this test */
    update_script(db, "server.js", server_js_faulty);

    ret = hbf_qjs_init(64, 2000);
    assert(ret == 0);

    ctx = hbf_qjs_ctx_create();
    assert(ctx);

    ret = hbf_qjs_load_script(ctx, db, "router.js");
    assert(ret == 0);
    ret = hbf_qjs_load_server_js(ctx, db);
    assert(ret == 0);

    /* Call handle for a path that doesn't match any route; middleware just returns */
    const char *invoke =
        "var req={method:'GET', path:'/nope', params:{}};\n"
        "var res={};\n"
        "app.handle(req,res);\n";

    ret = hbf_qjs_eval(ctx, invoke, strlen(invoke));
    /* Expect failure due to reentrancy guard, not stack overflow */
    assert(ret != 0);

    hbf_qjs_ctx_destroy(ctx);
    hbf_qjs_shutdown();
    hbf_db_close(db);

    printf("  ✓ 404 path handled without stack overflow\n");
}

static void test_router_next_middleware_chain(void)
{
    sqlite3 *db = setup_test_db();
    int ret;
    hbf_qjs_ctx_t *ctx;

    /* router.js is already in schema, just override server.js for this test */
    update_script(db, "server.js", server_js_with_next);

    ret = hbf_qjs_init(64, 2000);
    assert(ret == 0);

    ctx = hbf_qjs_ctx_create();
    assert(ctx);

    ret = hbf_qjs_load_script(ctx, db, "router.js");
    assert(ret == 0);
    ret = hbf_qjs_load_server_js(ctx, db);
    assert(ret == 0);

    /* Test that middleware chain works: middleware1 -> route handler */
    const char *invoke =
        "var req={method:'GET', path:'/hello', params:{}};\n"
        "var res={};\n"
        "app.handle(req,res);\n"
        "if (!res._sent) throw new Error('Route handler not called');\n"
        "if (!res._hasMiddleware1) throw new Error('Middleware not called before route');\n";

    ret = hbf_qjs_eval(ctx, invoke, strlen(invoke));
    assert(ret == 0);

    /* Test that 404 fallback middleware works */
    const char *invoke404 =
        "var req2={method:'GET', path:'/notfound', params:{}};\n"
        "var res2={};\n"
        "app.handle(req2,res2);\n"
        "if (!res2._fallback) throw new Error('Fallback middleware not called');\n";

    ret = hbf_qjs_eval(ctx, invoke404, strlen(invoke404));
    assert(ret == 0);

    hbf_qjs_ctx_destroy(ctx);
    hbf_qjs_shutdown();
    hbf_db_close(db);

    printf("  ✓ next() middleware chain works correctly\n");
}

int main(void)
{
    hbf_log_set_level(HBF_LOG_WARN);
    printf("Router Tests:\n");
    test_router_handle_simple();
    test_router_no_stack_overflow_on_404();
    test_router_next_middleware_chain();
    printf("\nAll router tests passed!\n");
    return 0;
}
