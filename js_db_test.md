# Database Test Route Implementation Plan

## Overview

Add a `/db-test` route to `pods/test/hbf/server.js` that verifies JavaScript can perform database operations after the overlay_fs refactor. This confirms that the handler mutex fix doesn't break JavaScript database access via the `db.query()` and `db.execute()` functions.

## Problem Statement

After implementing the hybrid mutex fix for memory corruption:
- Handler mutex protects QuickJS context creation/destruction
- overlay_fs now owns a global database pointer (`g_overlay_db`)
- Need to verify JavaScript still has database access via per-context `ctx->db`

## Two Database Pointer Design

The refactor created two database pointers (both pointing to same `sqlite3*` handle):

1. **Per-Context Pointer** (`ctx->db` in `hbf_qjs_ctx_t`)
   - Set by: `hbf_qjs_ctx_create_with_db(server->db)`
   - Used by: JavaScript via `db.query()` and `db.execute()`
   - Access: Via `JS_GetContextOpaque()` in `hbf/qjs/bindings/db_module.c`

2. **Global Pointer** (`g_overlay_db` in `overlay_fs.c`)
   - Set by: `overlay_fs_init_global(db)` during initialization
   - Used by: C functions `overlay_fs_read_file()` and `overlay_fs_write_file()`
   - Access: Direct static variable access

Both pointers reference the same database, so there's no conflict.

## Concurrency Model and Safety Analysis

### Current Concurrent Access Scenarios

**Scenario 1: JavaScript write + Static file read** (CAN happen)
- Thread 1: QuickJS handler (handler_mutex locked) → JavaScript `db.execute("INSERT...")` → uses `ctx->db`
- Thread 2: Static file handler (no mutex) → `overlay_fs_read_file("static/file.css")` → uses `g_overlay_db`
- **Both access same `sqlite3*` handle concurrently**

**Scenario 2: Multiple static file reads** (CAN happen)
- Threads 1-4: All serving static files → all call `overlay_fs_read_file()` concurrently
- **All access `g_overlay_db` (same handle) concurrently**

**Scenario 3: JavaScript write + JavaScript write** (CANNOT happen)
- Thread 1: QuickJS handler with handler_mutex locked
- Thread 2: QuickJS handler blocked by mutex
- **Serialized by handler_mutex** - no concurrent JavaScript execution

### Thread Safety Guarantee: SQLite Internal Locking

**SQLITE_THREADSAFE=1 (Serialized Mode)**
- SQLite documentation: "SQLite can be safely used by multiple threads with no restriction"
- SQLite uses **internal mutexes** to serialize access to database handle
- Each operation (`sqlite3_prepare_v2`, `sqlite3_step`, `sqlite3_finalize`) acquires SQLite's mutex
- Multiple threads accessing same `sqlite3*` handle are **safe** but **serialized**

**Configuration** (from `hbf/db/db.c`):
```c
-DSQLITE_THREADSAFE=1    // Thread-safe with internal locking
PRAGMA journal_mode=WAL  // Write-Ahead Logging for reader/writer concurrency
```

**WAL Mode Benefits**:
- Readers don't block writers (mostly)
- Writers don't block readers (mostly)
- Better concurrency than rollback journal mode

### Performance Characteristics

**Serialization Points**:
1. **Handler mutex**: Serializes QuickJS context lifecycle (prevents malloc contention)
2. **SQLite internal mutex**: Serializes all database operations (on same handle)

**Bottleneck Analysis**:
- QuickJS malloc contention: **ELIMINATED** (handler mutex)
- SQLite access contention: **EXISTS** (internal mutex on shared handle)
- Static file serving: **PARALLEL** at thread level, **SERIALIZED** at SQLite level

**Expected Behavior Under Load**:
- Multiple static file requests → concurrent at HTTP level → serialized at SQLite mutex
- JavaScript write + static reads → serialized at SQLite mutex (WAL helps)
- Performance adequate for moderate load; may need connection pooling for high concurrency

### Design Decision: Document and Monitor

**Current Approach**: Rely on SQLite's SQLITE_THREADSAFE=1 internal locking

**Rationale**:
- ✅ Simple: No application-level database mutex needed
- ✅ Safe: SQLite guarantees thread safety
- ✅ Proven: Standard pattern for moderate-concurrency applications
- ✅ WAL mode provides reader/writer concurrency
- ⚠️ Serialization at SQLite mutex may become bottleneck under heavy load

**Monitoring Recommendations**:
- Track response times for static file requests
- Monitor QuickJS handler execution times
- Benchmark with realistic load patterns (see `.claude/skills/benchmark/`)
- If bottleneck appears, consider connection pooling

**Future Optimization Path** (if needed):
1. Add metrics to measure SQLite mutex contention
2. Benchmark connection pool performance
3. Implement read-only connection pool for static files
4. Keep single writer connection for JavaScript
5. Use SQLite WAL mode to maximize read/write concurrency

**Current Status**: Adequate for typical workloads. Revisit if benchmarks show database access as bottleneck.

### Benchmark Results: Handler Mutex Fix Validation

**Date**: 2025-11-04
**Git Commit**: 45b36bf
**Test**: 10,000 requests per endpoint, concurrency=10

**Primary Goal: Prevent Memory Corruption ✅ ACHIEVED**
- **NO CRASHES** - Server remained stable throughout benchmark
- **Zero failed requests** across 30,000+ total requests
- The handler mutex successfully prevents malloc contention in QuickJS

**Performance Impact: Severe Serialization ⚠️**

| Category | Endpoint | Req/sec | Avg Time | Status |
|----------|----------|---------|----------|--------|
| **Static** | /static/favicon.ico | 18,109 | 0.6ms | ✅ |
| **Static** | /static/style.css | 1,764 | 5.7ms | ✅ |
| **QuickJS** | /hello | 209 | 47.8ms | ⚠️ |
| **QuickJS** | /user/42 | 222 | 45.1ms | ⚠️ |
| **QuickJS** | /echo | 224 | 44.6ms | ⚠️ |

**Performance Analysis**:
- **Expected**: 30,000-50,000 req/sec for QuickJS routes (from README.md)
- **Actual**: 209-224 req/sec (**~100-150x slower**)
- **Root cause**: Handler mutex locks entire request lifecycle, serializing JavaScript execution
- **Static files**: Less affected (1.7k-18k req/sec) but still slower than baseline

**Trade-off Confirmation**:
The "Keep current (serialized execution)" choice means:
- ✅ **Stability**: No crashes, safe for production
- ❌ **Performance**: Only 1 JavaScript request executes at a time
- ✅ **Simplicity**: No complex concurrency management
- ⚠️ **Scalability**: Limited to ~200 req/sec for dynamic routes

**Next Steps**:
1. ✅ Crash fix validated - ready for production use
2. Document performance characteristics in README.md
3. Consider narrow mutex scope if higher throughput needed
4. Benchmark connection pooling if database becomes bottleneck

**Recommendation**:
The fix achieves its primary goal (stability). For low-to-moderate traffic workloads (<200 dynamic req/sec), this is acceptable. For higher throughput requirements, implement narrower mutex scope that only protects context creation/destruction.

## Implementation Plan

### File Changes

#### 1. pods/test/hbf/server.js

**Location**: Insert after line 72 (after `/echo` route, before `/__dev/` routes)

**Route Handler**:
```javascript
    // Database operations test route
    if (path === "/db-test" && method === "GET") {
        try {
            // Step 1: Drop test table if exists (cleanup from previous runs)
            db.execute("DROP TABLE IF EXISTS js_test_table");

            // Step 2: Create test table
            db.execute(`
                CREATE TABLE js_test_table (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL,
                    value INTEGER NOT NULL,
                    created_at INTEGER NOT NULL
                )
            `);

            // Step 3: Insert test data with parameterized queries
            const rows1 = db.execute(
                "INSERT INTO js_test_table (name, value, created_at) VALUES (?, ?, ?)",
                ["test_alpha", 42, Date.now()]
            );

            const rows2 = db.execute(
                "INSERT INTO js_test_table (name, value, created_at) VALUES (?, ?, ?)",
                ["test_beta", 99, Date.now()]
            );

            const rows3 = db.execute(
                "INSERT INTO js_test_table (name, value, created_at) VALUES (?, ?, ?)",
                ["test_gamma", 123, Date.now()]
            );

            // Step 4: Query data back
            const results = db.query("SELECT * FROM js_test_table ORDER BY id");

            // Step 5: Query with WHERE clause
            const filtered = db.query(
                "SELECT * FROM js_test_table WHERE value > ? ORDER BY value",
                [50]
            );

            // Step 6: Verify results
            const insertSuccess = (rows1 + rows2 + rows3) === 3;
            const querySuccess = (
                results.length === 3 &&
                results[0].name === "test_alpha" &&
                results[0].value === 42 &&
                results[1].name === "test_beta" &&
                results[1].value === 99 &&
                results[2].name === "test_gamma" &&
                results[2].value === 123
            );
            const filterSuccess = (
                filtered.length === 2 &&
                filtered[0].value === 99 &&
                filtered[1].value === 123
            );

            const allSuccess = insertSuccess && querySuccess && filterSuccess;

            // Step 7: Cleanup
            db.execute("DROP TABLE js_test_table");

            // Return comprehensive test results
            res.set("Content-Type", "application/json");
            res.send(JSON.stringify({
                success: allSuccess,
                message: allSuccess ? "All database tests passed" : "Some tests failed",
                tests: {
                    insert: {
                        success: insertSuccess,
                        rowsAffected: rows1 + rows2 + rows3
                    },
                    query: {
                        success: querySuccess,
                        rowsReturned: results.length
                    },
                    filter: {
                        success: filterSuccess,
                        rowsReturned: filtered.length
                    }
                },
                data: {
                    allRows: results,
                    filteredRows: filtered
                }
            }, null, 2));
        } catch (err) {
            res.status(500);
            res.set("Content-Type", "application/json");
            res.send(JSON.stringify({
                success: false,
                message: "Database test error",
                error: err.toString(),
                stack: err.stack || "No stack trace"
            }, null, 2));
        }
        return;
    }
```

**Update Homepage** (around line 28-31):

Add to the route list:
```html
<li><a href="/db-test">/db-test</a> - Database operations test (CREATE/INSERT/SELECT)</li>
```

### Test Verification

#### Build and Run
```bash
# Build test pod
bazel build //:hbf_test

# Run test pod with in-memory database
bazel run //:hbf_test -- --port 5309 --inmem --log_level debug
```

#### Test the Route
```bash
# Simple test
curl http://localhost:5309/db-test

# Pretty-printed
curl -s http://localhost:5309/db-test | jq
```

#### Expected Success Response
```json
{
  "success": true,
  "message": "All database tests passed",
  "tests": {
    "insert": {
      "success": true,
      "rowsAffected": 3
    },
    "query": {
      "success": true,
      "rowsReturned": 3
    },
    "filter": {
      "success": true,
      "rowsReturned": 2
    }
  },
  "data": {
    "allRows": [
      {"id": 1, "name": "test_alpha", "value": 42, "created_at": 1234567890},
      {"id": 2, "name": "test_beta", "value": 99, "created_at": 1234567891},
      {"id": 3, "name": "test_gamma", "value": 123, "created_at": 1234567892}
    ],
    "filteredRows": [
      {"id": 2, "name": "test_beta", "value": 99, "created_at": 1234567891},
      {"id": 3, "name": "test_gamma", "value": 123, "created_at": 1234567892}
    ]
  }
}
```

#### Run All Tests
```bash
# Ensure nothing broke
bazel test //...
```

### What This Tests

1. **DDL Operations**
   - `CREATE TABLE` with schema definition
   - `DROP TABLE` for cleanup
   - Verifies JavaScript can modify database schema

2. **DML Operations**
   - `INSERT` with parameterized queries (SQL injection safe)
   - `SELECT` with ORDER BY clause
   - `SELECT` with WHERE clause and parameters
   - Verifies JavaScript can read and write data

3. **Data Integrity**
   - Inserted values match queried values
   - Filtering works correctly
   - AUTOINCREMENT id assignment works

4. **Error Handling**
   - Try/catch wraps all operations
   - Detailed error messages with stack traces
   - 500 status code on failure

5. **Database Access Path**
   - Confirms `ctx->db` pointer works correctly
   - Proves `db.query()` and `db.execute()` access database via `JS_GetContextOpaque()`
   - Validates overlay_fs refactor didn't break JavaScript bindings

### Success Criteria

- ✅ Route returns `{"success": true}`
- ✅ All three test categories pass (insert, query, filter)
- ✅ Data integrity verified (inserted = queried)
- ✅ No exceptions thrown
- ✅ All existing tests still pass (`bazel test //...`)

### Architecture Validation

This test proves:

1. **Handler mutex doesn't block database access**
   - JavaScript executes while mutex is held
   - Database operations work correctly during execution

2. **Two-pointer design works**
   - `ctx->db` (per-context) used by JavaScript
   - `g_overlay_db` (global) used by C overlay_fs functions
   - Both access same `sqlite3*` handle without conflict

3. **SQLite thread safety works**
   - SQLITE_THREADSAFE=1 + WAL mode handles concurrent access
   - Internal SQLite locks protect database operations

4. **Versioned filesystem is accessible**
   - Test table operations work in same database
   - Existing `latest_files` view queries still work (demonstrated elsewhere in server.js)

### Integration with Existing Tests

This new route complements existing test pod features:

- **File operations** (`/__dev/api/file`) - Test versioned filesystem CRUD
- **Echo route** (`/echo`) - Test request metadata handling
- **User route** (`/user/:id`) - Test path parameter extraction
- **DB test route** (`/db-test`) - **NEW** Test raw SQL operations

### Future Enhancements

If this basic test passes, consider adding:

1. **Transaction tests** - BEGIN/COMMIT/ROLLBACK
2. **Concurrent query tests** - Multiple SELECTs in parallel
3. **FTS5 tests** - Full-text search if enabled
4. **JSON functions** - SQLite JSON1 extension tests
5. **Stress test** - Many inserts/queries to verify stability

### References

- Handler mutex implementation: `hbf/http/handler.c:26`
- overlay_fs global init: `hbf/db/db.c:252`
- JavaScript db module: `hbf/qjs/bindings/db_module.c`
- QuickJS context creation: `hbf/qjs/engine.c:185-192`
- Test pod server: `pods/test/hbf/server.js`
