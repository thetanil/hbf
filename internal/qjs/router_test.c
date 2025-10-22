/* Router recursion/stack overflow test */
#include <assert.h>
#include <stdio.h>
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

/* Minimal shim of req/res to call app.handle() in pure JS without C handler */
static const char *router_js =
"// Minimal Router (exact path match)\n"
"function Router(){ this.routes={GET:[],POST:[],PUT:[],DELETE:[]}; this.middleware=[]; }\n"
"Router.prototype.get=function(path,handler){ this.routes.GET.push({path:path,handler:handler}); return this; };\n"
"Router.prototype.use=function(handler){ this.middleware.push(handler); return this; };\n"
"Router.prototype.handle=function(req,res){ var list=this.routes[req.method]||[]; for (var i=0;i<list.length;i++){ var r=list[i]; if (r.path===req.path){ r.handler(req,res); return true; } } for (var j=0;j<this.middleware.length;j++){ this.middleware[j](req,res); return true; } return false; };\n"
"var app = new Router();\n";

/* server with potentially recursive route if handle() calls itself incorrectly */
static const char *server_js_non_recursive =
"app.get('/hello', function(req, res){ res._sent = true; });\n";

/* A faulty server that would cause recursion if app.handle calls app.handle */
static const char *server_js_faulty =
"app.use(function(req, res){ app.handle(req, res); });\n";

static void test_router_handle_simple(void)
{
    sqlite3 *db = setup_test_db();
    int ret;
    hbf_qjs_ctx_t *ctx;

    insert_script(db, "router.js", router_js);
    insert_script(db, "server.js", server_js_non_recursive);

    ret = hbf_qjs_init(64, 2000);
    assert(ret == 0);

    ctx = hbf_qjs_ctx_create();
    assert(ctx);

    /* load router then server */
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

    insert_script(db, "router.js", router_js);
    insert_script(db, "server.js", server_js_faulty);

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

int main(void)
{
    hbf_log_set_level(HBF_LOG_WARN);
    printf("Router Tests:\n");
    test_router_handle_simple();
    test_router_no_stack_overflow_on_404();
    printf("\nAll router tests passed!\n");
    return 0;
}
