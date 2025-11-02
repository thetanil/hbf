/* JS module for DB access: db.query and db.execute */
#include <stdint.h>
#include "quickjs.h"
#include "hbf/db/db.h"
#include "hbf/qjs/engine.h"
#include "hbf/qjs/db_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>





static JSValue js_db_query(JSContext *ctx, JSValueConst this_val __attribute__((unused)), int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "db.query: missing SQL argument");
    }
    const char *sql = JS_ToCString(ctx, argv[0]);
    if (!sql) {
        return JS_ThrowTypeError(ctx, "db.query: invalid SQL");
    }
    JSValue params = argc > 1 ? argv[1] : JS_UNDEFINED;
    hbf_qjs_ctx_t *engine_ctx = (hbf_qjs_ctx_t *)JS_GetContextOpaque(ctx);
    sqlite3 *db = engine_ctx ? engine_ctx->db : NULL;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        JS_FreeCString(ctx, sql);
    return JS_ThrowInternalError(ctx, "db.query: prepare failed: %s", sqlite3_errmsg(db));
    }
    // Bind params (only array of values supported for now)
    if (!JS_IsUndefined(params) && JS_IsArray(ctx, params)) {
        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, params, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);
        for (int i = 0; i < len; i++) {
            JSValue val = JS_GetPropertyUint32(ctx, params, (uint32_t)i);
            if (JS_IsNumber(val)) {
                double num;
                JS_ToFloat64(ctx, &num, val);
                sqlite3_bind_double(stmt, i + 1, num);
            } else if (JS_IsString(val)) {
                const char *s = JS_ToCString(ctx, val);
                sqlite3_bind_text(stmt, i + 1, s, -1, SQLITE_TRANSIENT);
                JS_FreeCString(ctx, s);
            } else if (JS_IsNull(val) || JS_IsUndefined(val)) {
                sqlite3_bind_null(stmt, i + 1);
            }
            JS_FreeValue(ctx, val);
        }
    }
    // Build result array
    JSValue result = JS_NewArray(ctx);
    int row = 0;
    rc = sqlite3_step(stmt);
    while (rc == SQLITE_ROW) {
        int cols = sqlite3_column_count(stmt);
        JSValue obj = JS_NewObject(ctx);
        for (int c = 0; c < cols; c++) {
            const char *col = sqlite3_column_name(stmt, c);
            switch (sqlite3_column_type(stmt, c)) {
                case SQLITE_INTEGER:
                    JS_SetPropertyStr(ctx, obj, col, JS_NewInt64(ctx, sqlite3_column_int64(stmt, c)));
                    break;
                case SQLITE_FLOAT:
                    JS_SetPropertyStr(ctx, obj, col, JS_NewFloat64(ctx, sqlite3_column_double(stmt, c)));
                    break;
                case SQLITE_TEXT:
                    JS_SetPropertyStr(ctx, obj, col, JS_NewString(ctx, (const char *)sqlite3_column_text(stmt, c)));
                    break;
                case SQLITE_NULL:
                case SQLITE_BLOB:
                default:
                    JS_SetPropertyStr(ctx, obj, col, JS_NULL);
                    break;
            }
        }
        JS_SetPropertyUint32(ctx, result, (uint32_t)row++, JS_DupValue(ctx, obj));
        JS_FreeValue(ctx, obj);
        rc = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    JS_FreeCString(ctx, sql);
    return result;
}

static JSValue js_db_execute(JSContext *ctx, JSValueConst this_val __attribute__((unused)), int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "db.execute: missing SQL argument");
    }
    const char *sql = JS_ToCString(ctx, argv[0]);
    if (!sql) {
        return JS_ThrowTypeError(ctx, "db.execute: invalid SQL");
    }
    JSValue params = argc > 1 ? argv[1] : JS_UNDEFINED;
    hbf_qjs_ctx_t *engine_ctx = (hbf_qjs_ctx_t *)JS_GetContextOpaque(ctx);
    sqlite3 *db = engine_ctx ? engine_ctx->db : NULL;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        JS_FreeCString(ctx, sql);
    return JS_ThrowInternalError(ctx, "db.execute: prepare failed: %s", sqlite3_errmsg(db));
    }
    // Bind params (only array of values supported for now)
    if (!JS_IsUndefined(params) && JS_IsArray(ctx, params)) {
        int64_t len = 0;
        JSValue len_val = JS_GetPropertyStr(ctx, params, "length");
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);
        for (int i = 0; i < len; i++) {
            JSValue val = JS_GetPropertyUint32(ctx, params, (uint32_t)i);
            if (JS_IsNumber(val)) {
                double num;
                JS_ToFloat64(ctx, &num, val);
                sqlite3_bind_double(stmt, i + 1, num);
            } else if (JS_IsString(val)) {
                const char *s = JS_ToCString(ctx, val);
                sqlite3_bind_text(stmt, i + 1, s, -1, SQLITE_TRANSIENT);
                JS_FreeCString(ctx, s);
            } else if (JS_IsNull(val) || JS_IsUndefined(val)) {
                sqlite3_bind_null(stmt, i + 1);
            }
            JS_FreeValue(ctx, val);
        }
    }
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    JS_FreeCString(ctx, sql);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    return JS_ThrowInternalError(ctx, "db.execute: step failed: %s", sqlite3_errmsg(db));
    }
    return JS_NewInt32(ctx, changes);
}

static const JSCFunctionListEntry db_funcs[] = {
    JS_CFUNC_DEF("query", 2, js_db_query),
    JS_CFUNC_DEF("execute", 2, js_db_execute)
};

int hbf_qjs_init_db_module(JSContext *ctx) {
    JSValue global_obj;
    JSValue db_obj = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, db_obj, db_funcs, sizeof(db_funcs)/sizeof(JSCFunctionListEntry));
    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "db", db_obj);
    JS_FreeValue(ctx, global_obj);
    return 0;
}
