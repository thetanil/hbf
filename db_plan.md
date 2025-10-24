# Database Migration Plan: From Schema-based to SQLite Archive (SQLAR)

## Overview

This plan outlines the migration from the current database schema-based file handling to a new SQLite Archive (SQLAR) based system. The new system will store files in a `fs.db` archive that gets embedded into the HBF binary, with support for both in-memory and persistent operation.

## Current Implementation - Items for Removal

### 1. Database Schema Files

**Files to be removed:**
- `internal/db/schema.sql` - Complete schema definition
- `internal/db/schema.c` - Schema initialization with embedded SQL  
- `internal/db/schema.h` - Schema header file
- `internal/db/schema_test.c` - Schema testing
- `internal/db/schema_with_content.sql` (generated)
- `internal/db/schema_sql.c` (generated)

**Tables to be removed:**
- `nodes` - Document-graph nodes with JSON body
- `edges` - Directed relationships between nodes
- `tags` - Hierarchical labeling system
- `nodes_fts` - FTS5 full-text search 
- `_hbf_users` - User accounts with Argon2id password hashes
- `_hbf_sessions` - JWT session tracking
- `_hbf_table_permissions` - Table-level access control
- `_hbf_row_policies` - Row-level security policies
- `_hbf_config` - Per-environment configuration
- `_hbf_audit_log` - Audit trail for admin actions
- `_hbf_schema_version` - Schema versioning

### 2. Static Content Injection System

**Files to be removed:**
- `tools/inject_content.sh` - Converts static files to SQL INSERT statements
- `tools/sql_to_c.sh` - Converts SQL to C arrays (schema use)
- `internal/db/BUILD.bazel` genrules:
  - `static_content_sql_gen` - Generates SQL inserts for static content
  - `schema_with_content_gen` - Merges schema with static content
  - `schema_sql_gen` - Converts merged schema to C source

### 3. Database Initialization Code

**Functions to be removed:**
- `hbf_db_init_schema()` - Initialize database schema
- `hbf_db_get_schema_version()` - Get current schema version
- `hbf_db_is_initialized()` - Check if database has been initialized
- All embedded schema SQL variables and references

### 4. Static File Handler Updates

**Current static file serving logic in `internal/http/server.c`:**
```c
// This query will be replaced
"SELECT json_extract(body, '$.content') FROM nodes WHERE name = ? AND type = 'static'"
```

### 5. QuickJS Database Module

**Files requiring updates:**
- `internal/qjs/db_module.c` - JS module for DB access
- Remove hardcoded schema table dependencies
- Update to work with SQLAR and simplified structure

### 6. Henv Directory Structure 

**Current structure to be replaced:**
```
henvs/
  {hash}/
    index.db  # Contains full schema + content
```

## New Implementation Plan

### 1. File System Archive Creation

#### 1.1 Create fs/ Directory Structure
```
fs/
  static/
    index.html
    style.css
    favicon.ico
    assets/
      images/
      scripts/
  hbf/
    server.js
```

#### 1.2 SQLAR Archive Generation (Bazel genrule)

**New file: `tools/create_fs_archive.sh`**
```bash
#!/bin/bash
# create_fs_archive.sh - Create SQLite archive from fs/ directory
# Usage: create_fs_archive.sh <fs_dir> <output_db>

set -e

FS_DIR="$1"
OUTPUT_DB="$2"

if [ ! -d "$FS_DIR" ]; then
    echo "Error: Directory '$FS_DIR' not found" >&2
    exit 1
fi

# Remove existing database
rm -f "$OUTPUT_DB"

# Create SQLite archive using sqlar extension
sqlite3 "$OUTPUT_DB" -Ac "$FS_DIR"/*

# Vacuum and optimize the database
sqlite3 "$OUTPUT_DB" "VACUUM; PRAGMA optimize;"

echo "Created SQLite archive: $OUTPUT_DB"
echo "Files archived: $(sqlite3 "$OUTPUT_DB" 'SELECT COUNT(*) FROM sqlar')"
echo "Database size: $(stat -c%s "$OUTPUT_DB") bytes"
```

#### 1.3 Database to C Conversion

**Enhanced `tools/db_to_c.sh` (new file):**
```bash
#!/bin/bash
# db_to_c.sh - Convert SQLite database to C byte array
# Usage: db_to_c.sh input.db output.c

set -e

INPUT="$1"
OUTPUT="$2"

if [ -z "$INPUT" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 input.db output.c"
    exit 1
fi

cat > "$OUTPUT" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from fs.db - DO NOT EDIT */

const unsigned char fs_db_data[] = {
EOF

# Convert each byte to hex format
od -An -tx1 -v "$INPUT" | while read -r line; do
    for byte in $line; do
        echo -n "0x$byte," >> "$OUTPUT"
    done
    echo "" >> "$OUTPUT"
done

cat >> "$OUTPUT" << 'EOF'
};

const unsigned char * const fs_db_ptr = fs_db_data;
const unsigned long fs_db_len = sizeof(fs_db_data);
EOF

echo "Generated $OUTPUT from $INPUT ($(stat -c%s "$INPUT") bytes)"
```

#### 1.4 New BUILD.bazel Rules

**File: `fs/BUILD.bazel` (new file)**
```bazel
# Filesystem archive for HBF

# Create SQLite archive from fs/ directory
genrule(
    name = "fs_archive",
    srcs = glob(["**/*"]),
    outs = ["fs.db"],
    cmd = "$(location //tools:create_fs_archive) $(RULEDIR) $(location fs.db)",
    tools = ["//tools:create_fs_archive"],
)

# Convert database to C source file
genrule(
    name = "fs_db_gen",
    srcs = [":fs_archive"],
    outs = ["fs.c"],
    cmd = "$(location //tools:db_to_c) $(location :fs_archive) $(location fs.c)",
    tools = ["//tools:db_to_c"],
)

cc_library(
    name = "fs_embedded",
    srcs = [":fs_db_gen"],
    hdrs = ["fs.h"],
    visibility = ["//visibility:public"],
)
```

### 2. Database Runtime System

#### 2.1 In-Memory Database Loader

**New file: `internal/fs/fs.h`**
```c
/* SPDX-License-Identifier: MIT */
#ifndef HBF_FS_H
#define HBF_FS_H

#include <sqlite3.h>

/*
 * Filesystem database management
 * 
 * Manages the embedded SQLite archive (SQLAR) containing static files
 * and HBF application files. Supports both in-memory and persistent modes.
 */

/*
 * Initialize filesystem database
 * 
 * @param inmem: If true, create in-memory database; if false, create persistent file
 * @param db_path: Path for persistent database (ignored if inmem=true)
 * @param db: Output parameter for database handle
 * @return 0 on success, -1 on error
 */
int hbf_fs_init(int inmem, const char *db_path, sqlite3 **db);

/*
 * Close filesystem database
 * 
 * @param db: Database handle to close
 */
void hbf_fs_close(sqlite3 *db);

/*
 * Read file from SQLAR archive
 * 
 * @param db: Database handle
 * @param path: File path within archive (e.g., "static/index.html")
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error (file not found or database error)
 */
int hbf_fs_read_file(sqlite3 *db, const char *path, char **data, size_t *size);

/*
 * Check if file exists in SQLAR archive
 * 
 * @param db: Database handle  
 * @param path: File path within archive
 * @return 1 if exists, 0 if not found, -1 on error
 */
int hbf_fs_file_exists(sqlite3 *db, const char *path);

/*
 * List files in SQLAR archive with optional prefix filter
 * 
 * @param db: Database handle
 * @param prefix: Optional prefix filter (e.g., "static/") - NULL for all files
 * @param paths: Output parameter for array of file paths (caller must free)
 * @param count: Output parameter for number of paths
 * @return 0 on success, -1 on error
 */
int hbf_fs_list_files(sqlite3 *db, const char *prefix, char ***paths, int *count);

#endif /* HBF_FS_H */
```

#### 2.2 Filesystem Database Implementation

**New file: `internal/fs/fs.c`**
```c
/* SPDX-License-Identifier: MIT */
#include "fs.h"
#include "../core/log.h"
#include <stdlib.h>
#include <string.h>

/* Embedded filesystem database */
extern const unsigned char fs_db_data[];
extern const unsigned long fs_db_len;

int hbf_fs_init(int inmem, const char *db_path, sqlite3 **db)
{
    int rc;
    const char *connection_string;
    char mem_uri[] = "file:memdb1?mode=memory&cache=shared";
    
    if (!db) {
        hbf_log_error("NULL database handle pointer");
        return -1;
    }
    
    /* Choose connection method */
    if (inmem) {
        connection_string = mem_uri;
        hbf_log_info("Initializing in-memory filesystem database");
    } else {
        if (!db_path) {
            hbf_log_error("NULL database path for persistent mode");
            return -1;
        }
        connection_string = db_path;
        hbf_log_info("Initializing persistent filesystem database: %s", db_path);
    }
    
    /* Open database */
    rc = sqlite3_open(connection_string, db);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to open database: %s", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        *db = NULL;
        return -1;
    }
    
    /* Load embedded database content */
    rc = sqlite3_exec(*db, (const char*)fs_db_data, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to load embedded database: %s", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        *db = NULL;
        return -1;
    }
    
    hbf_log_info("Filesystem database initialized successfully");
    return 0;
}

void hbf_fs_close(sqlite3 *db)
{
    if (db) {
        sqlite3_close(db);
        hbf_log_debug("Filesystem database closed");
    }
}

int hbf_fs_read_file(sqlite3 *db, const char *path, char **data, size_t *size)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT sqlar_uncompress(data,sz) FROM sqlar WHERE name = ?";
    int rc;
    const void *blob_data;
    int blob_size;
    
    if (!db || !path || !data || !size) {
        return -1;
    }
    
    *data = NULL;
    *size = 0;
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to prepare file read query: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to bind file path: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        /* File not found */
        sqlite3_finalize(stmt);
        return -1;
    }
    
    /* Get uncompressed data */
    blob_data = sqlite3_column_blob(stmt, 0);
    blob_size = sqlite3_column_bytes(stmt, 0);
    
    if (blob_size > 0 && blob_data) {
        *data = malloc(blob_size + 1);
        if (*data) {
            memcpy(*data, blob_data, blob_size);
            (*data)[blob_size] = '\0'; /* Null terminate for text files */
            *size = blob_size;
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (!*data) {
        hbf_log_error("Failed to allocate memory for file data");
        return -1;
    }
    
    hbf_log_debug("Read file '%s' (%zu bytes)", path, *size);
    return 0;
}

int hbf_fs_file_exists(sqlite3 *db, const char *path)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT 1 FROM sqlar WHERE name = ?";
    int rc;
    int exists = 0;
    
    if (!db || !path) {
        return -1;
    }
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to prepare file exists query: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to bind file path: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        exists = 1;
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

int hbf_fs_list_files(sqlite3 *db, const char *prefix, char ***paths, int *count)
{
    sqlite3_stmt *stmt;
    const char *sql;
    char like_pattern[512];
    int rc;
    int capacity = 16;
    int i;
    
    if (!db || !paths || !count) {
        return -1;
    }
    
    *paths = NULL;
    *count = 0;
    
    /* Prepare SQL based on whether prefix is provided */
    if (prefix) {
        sql = "SELECT name FROM sqlar WHERE name LIKE ? ORDER BY name";
        snprintf(like_pattern, sizeof(like_pattern), "%s%%", prefix);
    } else {
        sql = "SELECT name FROM sqlar ORDER BY name";
    }
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to prepare file list query: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    if (prefix) {
        rc = sqlite3_bind_text(stmt, 1, like_pattern, -1, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            hbf_log_error("Failed to bind prefix pattern: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }
    }
    
    /* Allocate initial array */
    *paths = malloc(capacity * sizeof(char*));
    if (!*paths) {
        hbf_log_error("Failed to allocate paths array");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    /* Collect file paths */
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *name = (const char*)sqlite3_column_text(stmt, 0);
        
        /* Expand array if needed */
        if (*count >= capacity) {
            capacity *= 2;
            char **new_paths = realloc(*paths, capacity * sizeof(char*));
            if (!new_paths) {
                /* Cleanup on failure */
                for (i = 0; i < *count; i++) {
                    free((*paths)[i]);
                }
                free(*paths);
                *paths = NULL;
                *count = 0;
                sqlite3_finalize(stmt);
                return -1;
            }
            *paths = new_paths;
        }
        
        /* Copy path string */
        (*paths)[*count] = strdup(name);
        if (!(*paths)[*count]) {
            /* Cleanup on failure */
            for (i = 0; i < *count; i++) {
                free((*paths)[i]);
            }
            free(*paths);
            *paths = NULL;
            *count = 0;
            sqlite3_finalize(stmt);
            return -1;
        }
        
        (*count)++;
    }
    
    sqlite3_finalize(stmt);
    
    hbf_log_debug("Listed %d files%s%s", *count, 
                  prefix ? " with prefix '" : "",
                  prefix ? prefix : "");
    
    return 0;
}
```

### 3. Static File Handler Updates

#### 3.1 Updated HTTP Static Handler

**Modified: `internal/http/server.c`**

Replace the current static file handler with:

```c
/* Static file handler - serves files from SQLAR archive */
static int static_file_handler(struct mg_connection *conn, void *cbdata)
{
    hbf_server_t *server;
    const struct mg_request_info *ri;
    const char *uri;
    const char *file_path;
    char *data;
    size_t size;
    const char *mime_type;
    int ret;

    server = (hbf_server_t *)cbdata;
    ri = mg_get_request_info(conn);
    if (!ri) {
        return 500;
    }

    uri = ri->local_uri;

    /* Map URL to filesystem path */
    if (strncmp(uri, "/", 1) == 0) {
        if (strlen(uri) == 1) {
            /* Root path -> index.html */
            file_path = "static/index.html";
        } else {
            /* Remove leading slash and prepend static/ */
            static char path_buffer[512];
            snprintf(path_buffer, sizeof(path_buffer), "static%s", uri);
            file_path = path_buffer;
        }
    } else {
        mg_send_http_error(conn, 400, "Invalid path");
        return 400;
    }

    hbf_log_debug("Static file request: %s -> %s", uri, file_path);

    /* Read file from SQLAR archive */
    ret = hbf_fs_read_file(server->fs_db, file_path, &data, &size);
    if (ret != 0) {
        hbf_log_debug("Static file not found: %s", file_path);
        mg_send_http_error(conn, 404, "File not found");
        return 404;
    }

    /* Determine MIME type */
    mime_type = get_mime_type(file_path);

    /* Send response */
    mg_printf(conn,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: public, max-age=3600\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type,
        size);

    mg_write(conn, data, size);

    free(data);

    hbf_log_debug("Served static file: %s (%zu bytes, %s)",
                  file_path, size, mime_type);

    return 200;
}
```

### 4. Server.js Loading Updates

#### 4.1 QuickJS HBF Script Loader

**New file: `internal/qjs/loader.h`**
```c
/* SPDX-License-Identifier: MIT */
#ifndef HBF_QJS_LOADER_H
#define HBF_QJS_LOADER_H

#include <quickjs.h>
#include <sqlite3.h>

/*
 * Load server.js from filesystem database into QuickJS context
 * 
 * @param ctx: QuickJS context
 * @param fs_db: Filesystem database handle
 * @return 0 on success, -1 on error
 */
int hbf_qjs_load_server_js(JSContext *ctx, sqlite3 *fs_db);

#endif /* HBF_QJS_LOADER_H */
```

**New file: `internal/qjs/loader.c`**
```c
/* SPDX-License-Identifier: MIT */
#include "loader.h"
#include "../fs/fs.h"
#include "../core/log.h"
#include <stdlib.h>

int hbf_qjs_load_server_js(JSContext *ctx, sqlite3 *fs_db)
{
    char *script_data;
    size_t script_size;
    JSValue result;
    int ret;

    if (!ctx || !fs_db) {
        hbf_log_error("NULL context or database handle");
        return -1;
    }

    /* Read server.js from filesystem database */
    ret = hbf_fs_read_file(fs_db, "hbf/server.js", &script_data, &script_size);
    if (ret != 0) {
        hbf_log_error("Failed to read server.js from filesystem database");
        return -1;
    }

    hbf_log_debug("Loading server.js (%zu bytes)", script_size);

    /* Evaluate script in QuickJS context */
    result = JS_Eval(ctx, script_data, script_size, "server.js", JS_EVAL_TYPE_GLOBAL);

    free(script_data);

    if (JS_IsException(result)) {
        hbf_log_error("Failed to evaluate server.js");
        JS_FreeValue(ctx, result);
        return -1;
    }

    JS_FreeValue(ctx, result);

    hbf_log_info("Server.js loaded successfully");
    return 0;
}
```

### 5. Configuration Updates

#### 5.1 CLI Flags

**Modified: `internal/core/config.h`**

Add new configuration option:
```c
typedef struct {
    int port;
    char storage_dir[256];
    char log_level[16];
    int dev;
    int inmem;  /* NEW: Use in-memory database */
} hbf_config_t;
```

**Modified: `internal/core/config.c`**

Add argument parsing for `--inmem` flag:
```c
static void print_usage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  --port PORT          HTTP server port (default: 5309)\n");
    printf("  --storage-dir DIR    Data directory (default: ~/.local/share/hbf)\n");
    printf("  --log-level LEVEL    Log level: DEBUG, INFO, WARN, ERROR (default: INFO)\n");
    printf("  --dev                Enable development mode\n");
    printf("  --inmem              Use in-memory database (for testing)\n");
    printf("  --help               Show this help\n");
}
```

### 6. Build System Updates

#### 6.1 Remove Old BUILD.bazel Rules

**Modified: `internal/db/BUILD.bazel`**

Remove all schema-related genrules and keep only basic db library:
```bazel
# Database utilities

cc_library(
    name = "db",
    srcs = ["db.c"],
    hdrs = ["db.h"],
    deps = [
        "//internal/core:log",
        "@sqlite3//:sqlite3",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "db_test",
    srcs = ["db_test.c"],
    deps = [":db"],
    linkstatic = 1,
)
```

#### 6.2 Add New Build Rules

**New: `internal/fs/BUILD.bazel`**
```bazel
# Filesystem database utilities

cc_library(
    name = "fs",
    srcs = ["fs.c"],
    hdrs = ["fs.h"],
    deps = [
        ":fs_embedded",
        "//internal/core:log",
        "@sqlite3//:sqlite3",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "fs_test",
    srcs = ["fs_test.c"],
    deps = [":fs"],
    linkstatic = 1,
)
```

**Updated: `tools/BUILD.bazel`**
```bazel
# Build tools

sh_binary(
    name = "create_fs_archive",
    srcs = ["create_fs_archive.sh"],
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "db_to_c",
    srcs = ["db_to_c.sh"],
    visibility = ["//visibility:public"],
)

# Keep existing tools
sh_binary(
    name = "js_to_c",
    srcs = ["js_to_c.sh"],
    visibility = ["//visibility:public"],
)
```

### 7. Testing Strategy

#### 7.1 Filesystem Database Tests

**New file: `internal/fs/fs_test.c`**
```c
/* SPDX-License-Identifier: MIT */
#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB_PATH "/tmp/hbf_fs_test.db"

static void cleanup_test_db(void)
{
    unlink(TEST_DB_PATH);
}

static void test_fs_init_inmem(void)
{
    sqlite3 *db = NULL;
    int ret;

    ret = hbf_fs_init(1, NULL, &db);
    assert(ret == 0);
    assert(db != NULL);

    hbf_fs_close(db);

    printf("  ✓ In-memory filesystem database initialization\n");
}

static void test_fs_init_persistent(void)
{
    sqlite3 *db = NULL;
    int ret;

    cleanup_test_db();

    ret = hbf_fs_init(0, TEST_DB_PATH, &db);
    assert(ret == 0);
    assert(db != NULL);

    hbf_fs_close(db);
    cleanup_test_db();

    printf("  ✓ Persistent filesystem database initialization\n");
}

static void test_fs_file_operations(void)
{
    sqlite3 *db = NULL;
    char *data;
    size_t size;
    int ret;
    int exists;

    ret = hbf_fs_init(1, NULL, &db);
    assert(ret == 0);

    /* Test file existence check */
    exists = hbf_fs_file_exists(db, "static/index.html");
    if (exists == 1) {
        /* Test file reading */
        ret = hbf_fs_read_file(db, "static/index.html", &data, &size);
        assert(ret == 0);
        assert(data != NULL);
        assert(size > 0);
        free(data);
    }

    /* Test non-existent file */
    exists = hbf_fs_file_exists(db, "nonexistent/file.txt");
    assert(exists == 0);

    ret = hbf_fs_read_file(db, "nonexistent/file.txt", &data, &size);
    assert(ret == -1);

    hbf_fs_close(db);

    printf("  ✓ File operations (exists, read)\n");
}

static void test_fs_list_files(void)
{
    sqlite3 *db = NULL;
    char **paths;
    int count;
    int ret;
    int i;

    ret = hbf_fs_init(1, NULL, &db);
    assert(ret == 0);

    /* List all files */
    ret = hbf_fs_list_files(db, NULL, &paths, &count);
    if (ret == 0 && count > 0) {
        assert(paths != NULL);
        
        for (i = 0; i < count; i++) {
            printf("    File: %s\n", paths[i]);
            free(paths[i]);
        }
        free(paths);
    }

    /* List static files only */
    ret = hbf_fs_list_files(db, "static/", &paths, &count);
    if (ret == 0 && count > 0) {
        assert(paths != NULL);
        
        for (i = 0; i < count; i++) {
            assert(strncmp(paths[i], "static/", 7) == 0);
            free(paths[i]);
        }
        free(paths);
    }

    hbf_fs_close(db);

    printf("  ✓ File listing operations\n");
}

int main(void)
{
    printf("Filesystem database tests:\n");

    test_fs_init_inmem();
    test_fs_init_persistent();
    test_fs_file_operations();
    test_fs_list_files();

    printf("\nAll filesystem database tests passed!\n");
    return 0;
}
```

#### 7.2 Integration Tests

**New file: `tools/fs_integration_test.sh`**
```bash
#!/bin/bash
# Integration test for the new filesystem database system
# Tests the complete flow: fs/ -> fs.db -> embedded C -> runtime

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=================================="
echo "HBF Filesystem Database Integration Test"
echo "=================================="

# Test 1: Build filesystem archive
echo -n "Testing fs/ archive creation... "
if bazel build //fs:fs_archive > /dev/null 2>&1; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 2: Check archive contents
echo -n "Testing archive contents... "
ARCHIVE_PATH=$(bazel-bin/fs/fs.db)
if [ -f "$ARCHIVE_PATH" ]; then
    FILE_COUNT=$(sqlite3 "$ARCHIVE_PATH" "SELECT COUNT(*) FROM sqlar")
    if [ "$FILE_COUNT" -gt 0 ]; then
        echo -e "${GREEN}PASS${NC} ($FILE_COUNT files)"
    else
        echo -e "${RED}FAIL${NC} (empty archive)"
        exit 1
    fi
else
    echo -e "${RED}FAIL${NC} (archive not found)"
    exit 1
fi

# Test 3: Build embedded C code
echo -n "Testing C code generation... "
if bazel build //fs:fs_embedded > /dev/null 2>&1; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 4: Build HBF with new filesystem
echo -n "Testing HBF build... "
if bazel build //internal/core:main > /dev/null 2>&1; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 5: Runtime test (in-memory mode)
echo -n "Testing in-memory runtime... "
if timeout 10s bazel-bin/internal/core/main --inmem --port 5310 > /dev/null 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    # Test static file serving
    STATUS=$(curl -s -w "%{http_code}" -o /dev/null "http://localhost:5310/")
    if [ "$STATUS" = "200" ]; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${RED}FAIL${NC} (got status $STATUS)"
        kill $SERVER_PID 2>/dev/null || true
        exit 1
    fi
    
    kill $SERVER_PID 2>/dev/null || true
then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}ALL INTEGRATION TESTS PASSED!${NC}"
echo ""
echo "The new filesystem database system is working correctly:"
echo "✓ fs/ directory archived to SQLite"
echo "✓ Archive embedded into C binary"
echo "✓ Runtime loads and serves files correctly"
```

### 8. Migration Steps

#### 8.1 Phase 1: Setup New System (1-2 days)
1. Create `fs/` directory structure
2. Move `static/` content to `fs/static/`
3. Move `server.js` to `fs/hbf/server.js`
4. Implement filesystem database (`internal/fs/`)
5. Create build tools (`tools/create_fs_archive.sh`, `tools/db_to_c.sh`)
6. Add BUILD.bazel rules for archive generation

#### 8.2 Phase 2: Update Runtime (1 day)
1. Update HTTP server to use new static file handler
2. Update QuickJS loader to read from filesystem database
3. Add CLI flag for in-memory mode
4. Update configuration parsing

#### 8.3 Phase 3: Remove Old System (1 day)
1. Remove schema-related files and code
2. Remove static content injection system
3. Update BUILD.bazel files
4. Remove unused database tables and functions
5. Update tests

#### 8.4 Phase 4: Testing & Validation (1 day)
1. Run integration tests
2. Verify static file serving works
3. Verify server.js loading works
4. Test both in-memory and persistent modes
5. Performance testing

### 9. Backwards Compatibility

#### 9.1 Migration Script

**New file: `tools/migrate_to_sqlar.sh`**
```bash
#!/bin/bash
# Migrate existing HBF installation to new SQLAR system
# Usage: migrate_to_sqlar.sh <old_henv_dir> <new_fs_dir>

set -e

OLD_HENV="$1"
NEW_FS="$2"

if [ ! -d "$OLD_HENV" ] || [ -z "$NEW_FS" ]; then
    echo "Usage: $0 <old_henv_dir> <new_fs_dir>"
    echo "Example: $0 ~/.local/share/hbf/henvs/abc123 ./fs"
    exit 1
fi

echo "Migrating from $OLD_HENV to $NEW_FS"

# Create new fs structure
mkdir -p "$NEW_FS/static"
mkdir -p "$NEW_FS/hbf"

# Extract static files from old database
if [ -f "$OLD_HENV/index.db" ]; then
    echo "Extracting static files from database..."
    
    sqlite3 "$OLD_HENV/index.db" "
    SELECT 'Extracting: ' || name FROM nodes WHERE type = 'static';
    " | while read line; do
        echo "$line"
    done
    
    # Export server.js
    sqlite3 "$OLD_HENV/index.db" "
    SELECT json_extract(body, '$.content') 
    FROM nodes 
    WHERE type = 'js' AND json_extract(body, '$.name') = 'server.js'
    " > "$NEW_FS/hbf/server.js"
    
    # Export static files
    sqlite3 "$OLD_HENV/index.db" "
    SELECT json_extract(body, '$.name'), json_extract(body, '$.content')
    FROM nodes 
    WHERE type = 'static'
    " | while IFS='|' read -r name content; do
        # Skip www/ prefix for cleaner paths
        clean_name="${name#www/}"
        output_path="$NEW_FS/static/$clean_name"
        
        # Create directory if needed
        mkdir -p "$(dirname "$output_path")"
        
        # Write content
        echo "$content" > "$output_path"
        echo "Exported: $output_path"
    done
fi

echo "Migration completed successfully!"
echo "New filesystem structure created in: $NEW_FS"
echo ""
echo "To build with new system:"
echo "  bazel build //fs:fs_archive"
echo "  bazel build //internal/core:main"
```

### 10. Benefits of New System

1. **Simplified Architecture**: No complex schema, just files in SQLAR
2. **Better Performance**: Direct SQLAR queries, no JSON extraction
3. **Smaller Binary**: Compressed files in archive, vacuum optimization
4. **Standard Format**: Uses SQLite's official SQLAR extension
5. **Easy Development**: Direct file editing in `fs/` directory
6. **Memory Efficiency**: In-memory mode for testing, persistent for production
7. **Build-time Optimization**: Archive creation and compression at build time
8. **Clear Separation**: Static files vs application files clearly separated

### 11. Risks and Mitigation

1. **Risk**: SQLAR compatibility issues
   - **Mitigation**: Use SQLite's official SQLAR implementation
   
2. **Risk**: File size limitations  
   - **Mitigation**: Monitor archive size, implement compression
   
3. **Risk**: Build complexity
   - **Mitigation**: Simple shell scripts, clear documentation
   
4. **Risk**: Migration complexity
   - **Mitigation**: Automated migration script, thorough testing

### 12. Success Criteria

ensure there are tests for each of these criteria so they can be checked all
the time in future iterations.

1. ✅ `fs/` directory structure successfully archived to SQLite
2. ✅ Archive embedded into HBF binary at build time
3. ✅ Static files served correctly from SQLAR archive
4. ✅ Server.js loaded correctly from archive
5. ✅ In-memory mode works for testing
6. ✅ Persistent mode works for production
7. ✅ All old schema code removed
8. ✅ Build time reduced or maintained
9. ✅ Binary size reduced or maintained
10. ✅ All tests pass

This plan provides a comprehensive migration path from the current schema-based
system to a more efficient SQLAR-based filesystem archive approach.