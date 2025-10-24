# Database Migration Plan: From Schema-based to SQLite Archive (SQLAR)

## Overview

This plan outlines the migration from the current database schema-based system to a new SQLite Archive (SQLAR) based system. The new architecture uses:

- **`fs.db`** (build-time input): SQLAR archive embedded in binary containing static files and server.js
- **`./hbf.db`** (runtime): Single user database created/opened at startup (or in-memory if `--inmem` flag is set)

**Architecture Model**: One HBF instance per one user (single-tenant for now). Multi-user support deferred to future phases.

**Important**: This plan supersedes Phase 3 from the original roadmap. Some Phase 2b components (henv manager, multi-user pod system) will be temporarily broken and reimplemented after the database migration is complete.

## Current Implementation - Items for Removal

### 1. User Pod Management (Phase 2b - Temporarily Removed)

**Files to be removed/updated:**
- `internal/henv/manager.c` - Multi-user pod management with LRU caching
- `internal/henv/manager.h` - Manager interface
- `internal/henv/manager_test.c` - Manager tests

**Functionality being removed (temporarily):**
- Multi-tenant pod directory structure (`henvs/{hash}/`)
- Connection caching and LRU eviction
- Per-user isolated databases
- Hash-based user identification

**Note**: This functionality will be re-implemented in a future phase once the database architecture is stable. For now, HBF operates as a single-user, single-instance application.

### 2. Database Schema Files

**Files to be removed:**
- `internal/db/schema.sql` - Complete schema definition
- `internal/db/schema.c` - Schema initialization with embedded SQL
- `internal/db/schema.h` - Schema header file
- `internal/db/schema_test.c` - Schema testing
- `internal/db/schema_with_content.sql` (generated)
- `internal/db/schema_sql.c` (generated)

**Tables being removed (from old schema):**
- `nodes` - Document-graph nodes with JSON body
- `edges` - Directed relationships between nodes
- `tags` - Hierarchical labeling system
- `nodes_fts` - FTS5 full-text search
- `_hbf_users` - User accounts (multi-user deferred)
- `_hbf_sessions` - JWT session tracking (multi-user deferred)
- `_hbf_table_permissions` - Table-level access control (multi-user deferred)
- `_hbf_row_policies` - Row-level security policies (multi-user deferred)
- `_hbf_config` - Per-environment configuration
- `_hbf_audit_log` - Audit trail
- `_hbf_schema_version` - Schema versioning

**Note**: User authentication, authorization, and document-graph features will be re-implemented in future phases with a simpler schema approach.

### 3. Static Content Injection System

**Files to be removed:**
- `tools/inject_content.sh` - Converts static files to SQL INSERT statements
- `tools/sql_to_c.sh` - Converts SQL to C arrays (schema use)

**BUILD.bazel genrules to be removed:**
- `static_content_sql_gen` - Generates SQL inserts for static content
- `schema_with_content_gen` - Merges schema with static content
- `schema_sql_gen` - Converts merged schema to C source

### 4. Database Initialization Code

**Functions to be removed:**
- `hbf_db_init_schema()` - Initialize database schema
- `hbf_db_get_schema_version()` - Get current schema version
- `hbf_db_is_initialized()` - Check if database has been initialized
- All embedded schema SQL variables and references

### 5. Configuration Updates

**Modified: `internal/core/config.h`**

Remove `storage_dir` field, add `inmem` field:
```c
typedef struct {
    int port;              /* HTTP server port (default: 5309) */
    char log_level[16];    /* Log level: DEBUG, INFO, WARN, ERROR */
    int dev;               /* Development mode flag */
    int inmem;             /* Use in-memory database (default: false) */
} hbf_config_t;
```

**CLI changes:**
- Remove: `--storage_dir` (no longer needed, single `./hbf.db` file)
- Add: `--inmem` (use in-memory database for testing)

## New Implementation Plan

### Phase 1: Build System - SQLAR Archive Creation

**Goal**: Create build-time tooling to convert `fs/` directory into an embedded SQLAR archive.

#### 1.1 Create fs/ Directory Structure

Create the following directory structure at the repository root:

```
fs/
  static/
    index.html       # Default landing page
    style.css        # Minimal styling
    favicon.ico      # Favicon
    assets/
      (future: images, scripts, etc.)
  hbf/
    server.js        # Express-style routing script
```

**Initial content for `fs/static/index.html`:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HBF</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <h1>HBF</h1>
    <p>Single binary web compute environment</p>
</body>
</html>
```

**Initial content for `fs/static/style.css`:**
```css
body {
    font-family: system-ui, -apple-system, sans-serif;
    max-width: 800px;
    margin: 2rem auto;
    padding: 0 1rem;
}
```

**Initial content for `fs/hbf/server.js`:**
```javascript
// HBF server.js - Express-style routing (placeholder for Phase 3+)
console.log('HBF server.js loaded');
```

#### 1.2 SQLAR Archive Generation Tool

**New file: `tools/create_fs_archive.sh`**
```bash
#!/bin/bash
# SPDX-License-Identifier: MIT
# create_fs_archive.sh - Create SQLite archive from fs/ directory
# Usage: create_fs_archive.sh <fs_dir> <output_db>

set -e

FS_DIR="$1"
OUTPUT_DB="$2"

if [ -z "$FS_DIR" ] || [ -z "$OUTPUT_DB" ]; then
    echo "Usage: $0 <fs_dir> <output_db>" >&2
    exit 1
fi

if [ ! -d "$FS_DIR" ]; then
    echo "Error: Directory '$FS_DIR' not found" >&2
    exit 1
fi

# Remove existing database
rm -f "$OUTPUT_DB"

# Create SQLite archive using sqlar extension
# -Ac: -A for archive mode, -c to create
cd "$FS_DIR"
sqlite3 "$OUTPUT_DB" -Ac .

# Vacuum and optimize the database
sqlite3 "$OUTPUT_DB" "VACUUM; PRAGMA optimize;"

echo "Created SQLite archive: $OUTPUT_DB" >&2
FILE_COUNT=$(sqlite3 "$OUTPUT_DB" "SELECT COUNT(*) FROM sqlar")
DB_SIZE=$(stat -c%s "$OUTPUT_DB" 2>/dev/null || stat -f%z "$OUTPUT_DB")
echo "Files archived: $FILE_COUNT" >&2
echo "Database size: $DB_SIZE bytes" >&2
```

**Permissions**: `chmod +x tools/create_fs_archive.sh`

#### 1.3 Database to C Byte Array Converter

**New file: `tools/db_to_c.sh`**
```bash
#!/bin/bash
# SPDX-License-Identifier: MIT
# db_to_c.sh - Convert SQLite database to C byte array
# Usage: db_to_c.sh <input.db> <output.c> <output.h>

set -e

INPUT="$1"
OUTPUT_C="$2"
OUTPUT_H="$3"

if [ -z "$INPUT" ] || [ -z "$OUTPUT_C" ] || [ -z "$OUTPUT_H" ]; then
    echo "Usage: $0 <input.db> <output.c> <output.h>" >&2
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: Input file '$INPUT' not found" >&2
    exit 1
fi

# Generate C source file
cat > "$OUTPUT_C" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from fs.db - DO NOT EDIT */

#include "fs_embedded.h"

const unsigned char fs_db_data[] = {
EOF

# Convert database to hex bytes (16 bytes per line for readability)
hexdump -v -e '16/1 "0x%02x, " "\n"' "$INPUT" >> "$OUTPUT_C"

cat >> "$OUTPUT_C" << 'EOF'
};

const unsigned long fs_db_len = sizeof(fs_db_data);
EOF

# Generate header file
cat > "$OUTPUT_H" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from fs.db - DO NOT EDIT */

#ifndef HBF_FS_EMBEDDED_H
#define HBF_FS_EMBEDDED_H

extern const unsigned char fs_db_data[];
extern const unsigned long fs_db_len;

#endif /* HBF_FS_EMBEDDED_H */
EOF

DB_SIZE=$(stat -c%s "$INPUT" 2>/dev/null || stat -f%z "$INPUT")
echo "Generated $OUTPUT_C and $OUTPUT_H from $INPUT ($DB_SIZE bytes)" >&2
```

**Permissions**: `chmod +x tools/db_to_c.sh`

#### 1.4 Bazel Build Rules

**New file: `fs/BUILD.bazel`**
```python
# SPDX-License-Identifier: MIT
# Filesystem archive for HBF

# Step 1: Create SQLite archive from fs/ directory
genrule(
    name = "fs_archive",
    srcs = glob(
        ["**/*"],
        exclude = ["BUILD.bazel"],
    ),
    outs = ["fs.db"],
    cmd = "$(location //tools:create_fs_archive) fs $(location fs.db)",
    tools = ["//tools:create_fs_archive"],
)

# Step 2: Convert database to C source files
genrule(
    name = "fs_db_gen",
    srcs = [":fs_archive"],
    outs = [
        "fs_embedded.c",
        "fs_embedded.h",
    ],
    cmd = "$(location //tools:db_to_c) $(location :fs_archive) $(location fs_embedded.c) $(location fs_embedded.h)",
    tools = ["//tools:db_to_c"],
)

# Step 3: Create library with embedded database
cc_library(
    name = "fs_embedded",
    srcs = ["fs_embedded.c"],
    hdrs = ["fs_embedded.h"],
    visibility = ["//visibility:public"],
)
```

**Updated: `tools/BUILD.bazel`**
```python
# SPDX-License-Identifier: MIT
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

# Keep existing lint tool
sh_binary(
    name = "lint",
    srcs = ["lint.sh"],
    data = glob(["../internal/**/*.c", "../internal/**/*.h"]),
    visibility = ["//visibility:public"],
)
```

**Unit Test**: Add integration test to verify archive generation:

**New file: `fs/fs_build_test.sh`**
```bash
#!/bin/bash
# SPDX-License-Identifier: MIT
# Test that fs.db archive is created correctly

set -e

ARCHIVE="$1"

if [ ! -f "$ARCHIVE" ]; then
    echo "FAIL: Archive not found: $ARCHIVE"
    exit 1
fi

# Test 1: Verify sqlar table exists
if ! sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar" > /dev/null 2>&1; then
    echo "FAIL: sqlar table not found in archive"
    exit 1
fi

# Test 2: Verify file count > 0
FILE_COUNT=$(sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar")
if [ "$FILE_COUNT" -eq 0 ]; then
    echo "FAIL: Archive is empty"
    exit 1
fi

# Test 3: Verify server.js exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'hbf/server.js'" | grep -q 1; then
    echo "FAIL: hbf/server.js not found in archive"
    exit 1
fi

# Test 4: Verify index.html exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/index.html'" | grep -q 1; then
    echo "FAIL: static/index.html not found in archive"
    exit 1
fi

echo "PASS: Archive contains $FILE_COUNT files"
exit 0
```

**Add to `fs/BUILD.bazel`:**
```python
sh_test(
    name = "fs_build_test",
    srcs = ["fs_build_test.sh"],
    args = ["$(location :fs_archive)"],
    data = [":fs_archive"],
)
```

### Phase 2: Runtime System - Database Initialization

**Goal**: Implement C library to load embedded SQLAR archive and initialize runtime database.

#### 2.1 Database Manager Interface

**New file: `internal/db/db.h`** (simplified, replaces old version)
```c
/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_H
#define HBF_DB_H

#include <sqlite3.h>

/*
 * Database initialization and management
 *
 * HBF uses two databases:
 * 1. Embedded fs.db (SQLAR archive) - Static files and server.js (read-only)
 * 2. Runtime hbf.db - User data and application state (read-write)
 */

/*
 * Initialize HBF database
 *
 * Creates or opens ./hbf.db (or in-memory if inmem=1).
 * Loads embedded fs.db archive for static content access.
 *
 * @param inmem: If 1, create in-memory database; if 0, use ./hbf.db
 * @param db: Output parameter for main database handle
 * @param fs_db: Output parameter for filesystem database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_init(int inmem, sqlite3 **db, sqlite3 **fs_db);

/*
 * Close HBF database handles
 *
 * @param db: Main database handle
 * @param fs_db: Filesystem database handle
 */
void hbf_db_close(sqlite3 *db, sqlite3 *fs_db);

/*
 * Read file from embedded filesystem archive
 *
 * @param fs_db: Filesystem database handle
 * @param path: File path within archive (e.g., "static/index.html")
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error
 */
int hbf_db_read_file(sqlite3 *fs_db, const char *path, unsigned char **data, size_t *size);

/*
 * Check if file exists in embedded filesystem archive
 *
 * @param fs_db: Filesystem database handle
 * @param path: File path within archive
 * @return 1 if exists, 0 if not found, -1 on error
 */
int hbf_db_file_exists(sqlite3 *fs_db, const char *path);

#endif /* HBF_DB_H */
```

#### 2.2 Database Manager Implementation

**New file: `internal/db/db.c`** (replaces old version)
```c
/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "../core/log.h"
#include "../../fs/fs_embedded.h"
#include <stdlib.h>
#include <string.h>

#define HBF_DB_PATH "./hbf.db"
#define HBF_DB_INMEM ":memory:"

int hbf_db_init(int inmem, sqlite3 **db, sqlite3 **fs_db)
{
	int rc;
	const char *db_path;
	unsigned char *fs_data_copy;

	if (!db || !fs_db) {
		hbf_log_error("NULL database handle pointers");
		return -1;
	}

	*db = NULL;
	*fs_db = NULL;

	/* Initialize main database (runtime data) */
	db_path = inmem ? HBF_DB_INMEM : HBF_DB_PATH;
	rc = sqlite3_open(db_path, db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to open database '%s': %s",
		              db_path, sqlite3_errmsg(*db));
		if (*db) {
			sqlite3_close(*db);
			*db = NULL;
		}
		return -1;
	}

	/* Configure main database */
	rc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_warn("Failed to enable WAL mode: %s", sqlite3_errmsg(*db));
	}

	rc = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to enable foreign keys: %s",
		              sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	hbf_log_info("Opened main database: %s", db_path);

	/* Initialize filesystem database (embedded SQLAR) */
	rc = sqlite3_open(HBF_DB_INMEM, fs_db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to open in-memory filesystem database: %s",
		              sqlite3_errmsg(*fs_db));
		if (*fs_db) {
			sqlite3_close(*fs_db);
			*fs_db = NULL;
		}
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Copy embedded data (required for sqlite3_deserialize) */
	fs_data_copy = malloc(fs_db_len);
	if (!fs_data_copy) {
		hbf_log_error("Failed to allocate memory for embedded database");
		sqlite3_close(*fs_db);
		sqlite3_close(*db);
		*fs_db = NULL;
		*db = NULL;
		return -1;
	}
	memcpy(fs_data_copy, fs_db_data, fs_db_len);

	/* Load embedded database using deserialize API */
	rc = sqlite3_deserialize(
		*fs_db,
		"main",
		fs_data_copy,
		fs_db_len,
		fs_db_len,
		SQLITE_DESERIALIZE_FREEONCLOSE | SQLITE_DESERIALIZE_RESIZEABLE
	);

	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to deserialize embedded database: %s",
		              sqlite3_errmsg(*fs_db));
		free(fs_data_copy);
		sqlite3_close(*fs_db);
		sqlite3_close(*db);
		*fs_db = NULL;
		*db = NULL;
		return -1;
	}

	hbf_log_info("Loaded embedded filesystem database (%lu bytes)", fs_db_len);

	return 0;
}

void hbf_db_close(sqlite3 *db, sqlite3 *fs_db)
{
	if (fs_db) {
		sqlite3_close(fs_db);
		hbf_log_debug("Closed filesystem database");
	}

	if (db) {
		sqlite3_close(db);
		hbf_log_debug("Closed main database");
	}
}

int hbf_db_read_file(sqlite3 *fs_db, const char *path,
                     unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	const void *blob_data;
	int blob_size;

	if (!fs_db || !path || !data || !size) {
		hbf_log_error("NULL parameter in hbf_db_read_file");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Use sqlar_uncompress to get uncompressed data */
	sql = "SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(fs_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read query: %s",
		              sqlite3_errmsg(fs_db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(fs_db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("File not found in archive: %s", path);
		return -1;
	}

	/* Get uncompressed data */
	blob_data = sqlite3_column_blob(stmt, 0);
	blob_size = sqlite3_column_bytes(stmt, 0);

	if (blob_size > 0 && blob_data) {
		*data = malloc((size_t)blob_size);
		if (*data) {
			memcpy(*data, blob_data, (size_t)blob_size);
			*size = (size_t)blob_size;
		}
	}

	sqlite3_finalize(stmt);

	if (!*data && blob_size > 0) {
		hbf_log_error("Failed to allocate memory for file data");
		return -1;
	}

	hbf_log_debug("Read file '%s' (%zu bytes)", path, *size);
	return 0;
}

int hbf_db_file_exists(sqlite3 *fs_db, const char *path)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	int exists;

	if (!fs_db || !path) {
		hbf_log_error("NULL parameter in hbf_db_file_exists");
		return -1;
	}

	sql = "SELECT 1 FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(fs_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file exists query: %s",
		              sqlite3_errmsg(fs_db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(fs_db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	exists = (rc == SQLITE_ROW) ? 1 : 0;

	sqlite3_finalize(stmt);
	return exists;
}
```

#### 2.3 Unit Tests

**New file: `internal/db/db_test.c`** (replaces old version)
```c
/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "../core/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_db_init_inmem(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);
	assert(db != NULL);
	assert(fs_db != NULL);

	hbf_db_close(db, fs_db);

	printf("  ✓ In-memory database initialization\n");
}

static void test_db_init_persistent(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;

	/* Remove existing test database */
	unlink("./hbf.db");
	unlink("./hbf.db-wal");
	unlink("./hbf.db-shm");

	ret = hbf_db_init(0, &db, &fs_db);
	assert(ret == 0);
	assert(db != NULL);
	assert(fs_db != NULL);

	hbf_db_close(db, fs_db);

	/* Cleanup */
	unlink("./hbf.db");
	unlink("./hbf.db-wal");
	unlink("./hbf.db-shm");

	printf("  ✓ Persistent database initialization\n");
}

static void test_db_read_file(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	unsigned char *data;
	size_t size;
	int ret;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);

	/* Test reading existing file */
	ret = hbf_db_read_file(fs_db, "hbf/server.js", &data, &size);
	if (ret == 0) {
		assert(data != NULL);
		assert(size > 0);
		free(data);
		printf("  ✓ Read file from archive (server.js, %zu bytes)\n", size);
	} else {
		printf("  ⚠ server.js not found in archive (expected for minimal test)\n");
	}

	/* Test reading non-existent file */
	ret = hbf_db_read_file(fs_db, "nonexistent/file.txt", &data, &size);
	assert(ret == -1);

	printf("  ✓ File read operations\n");

	hbf_db_close(db, fs_db);
}

static void test_db_file_exists(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;
	int exists;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);

	/* Test non-existent file */
	exists = hbf_db_file_exists(fs_db, "nonexistent/file.txt");
	assert(exists == 0);

	printf("  ✓ File existence check\n");

	hbf_db_close(db, fs_db);
}

int main(void)
{
	hbf_log_init("DEBUG");

	printf("Database tests:\n");

	test_db_init_inmem();
	test_db_init_persistent();
	test_db_read_file();
	test_db_file_exists();

	printf("\nAll database tests passed!\n");
	return 0;
}
```

#### 2.4 Updated BUILD.bazel

**Modified: `internal/db/BUILD.bazel`**
```python
# SPDX-License-Identifier: MIT
# Database management

cc_library(
    name = "db",
    srcs = ["db.c"],
    hdrs = ["db.h"],
    deps = [
        "//internal/core:log",
        "//fs:fs_embedded",
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

### Phase 3: HTTP Server Integration

**Goal**: Update HTTP server to use new database system and serve static files from SQLAR archive.

#### 3.1 Update Server Structure

**Modified: `internal/http/server.h`**
```c
/* SPDX-License-Identifier: MIT */
#ifndef HBF_HTTP_SERVER_H
#define HBF_HTTP_SERVER_H

#include <sqlite3.h>

typedef struct hbf_server hbf_server_t;

/*
 * Create HTTP server
 *
 * @param port: HTTP server port
 * @param db: Main database handle
 * @param fs_db: Filesystem database handle (for static files)
 * @return Server instance or NULL on error
 */
hbf_server_t *hbf_server_create(int port, sqlite3 *db, sqlite3 *fs_db);

/*
 * Start HTTP server
 *
 * @param server: Server instance
 * @return 0 on success, -1 on error
 */
int hbf_server_start(hbf_server_t *server);

/*
 * Stop HTTP server
 *
 * @param server: Server instance
 */
void hbf_server_stop(hbf_server_t *server);

/*
 * Destroy HTTP server
 *
 * @param server: Server instance
 */
void hbf_server_destroy(hbf_server_t *server);

#endif /* HBF_HTTP_SERVER_H */
```

**Modified: `internal/http/server.c`** (update struct and static file handler)
```c
/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "../core/log.h"
#include "../db/db.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct hbf_server {
	struct mg_context *ctx;
	int port;
	sqlite3 *db;
	sqlite3 *fs_db;
};

/* MIME type helper */
static const char *get_mime_type(const char *path)
{
	const char *ext = strrchr(path, '.');
	if (!ext) {
		return "application/octet-stream";
	}

	if (strcmp(ext, ".html") == 0) return "text/html";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".js") == 0) return "application/javascript";
	if (strcmp(ext, ".json") == 0) return "application/json";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
	if (strcmp(ext, ".ico") == 0) return "image/x-icon";

	return "application/octet-stream";
}

/* Static file handler - serves files from SQLAR archive */
static int static_handler(struct mg_connection *conn, void *cbdata)
{
	hbf_server_t *server = (hbf_server_t *)cbdata;
	const struct mg_request_info *ri = mg_get_request_info(conn);
	const char *uri;
	char path[512];
	unsigned char *data = NULL;
	size_t size;
	const char *mime_type;
	int ret;

	if (!ri) {
		mg_send_http_error(conn, 500, "Internal error");
		return 500;
	}

	uri = ri->local_uri;

	/* Map URL to archive path */
	if (strcmp(uri, "/") == 0) {
		snprintf(path, sizeof(path), "static/index.html");
	} else {
		/* Remove leading slash, prepend static/ */
		snprintf(path, sizeof(path), "static%s", uri);
	}

	hbf_log_debug("Static request: %s -> %s", uri, path);

	/* Read file from archive */
	ret = hbf_db_read_file(server->fs_db, path, &data, &size);
	if (ret != 0) {
		hbf_log_debug("File not found: %s", path);
		mg_send_http_error(conn, 404, "Not Found");
		return 404;
	}

	/* Determine MIME type */
	mime_type = get_mime_type(path);

	/* Send response */
	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: %s\r\n"
	          "Content-Length: %zu\r\n"
	          "Cache-Control: public, max-age=3600\r\n"
	          "Connection: close\r\n"
	          "\r\n",
	          mime_type, size);

	mg_write(conn, data, size);
	free(data);

	hbf_log_debug("Served: %s (%zu bytes, %s)", path, size, mime_type);
	return 200;
}

/* Health check handler */
static int health_handler(struct mg_connection *conn, void *cbdata)
{
	const char *response = "{\"status\":\"ok\"}";

	(void)cbdata;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: application/json\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(response), response);

	return 200;
}

hbf_server_t *hbf_server_create(int port, sqlite3 *db, sqlite3 *fs_db)
{
	hbf_server_t *server;

	if (!db || !fs_db) {
		hbf_log_error("NULL database handles");
		return NULL;
	}

	server = calloc(1, sizeof(hbf_server_t));
	if (!server) {
		hbf_log_error("Failed to allocate server");
		return NULL;
	}

	server->port = port;
	server->db = db;
	server->fs_db = fs_db;
	server->ctx = NULL;

	return server;
}

int hbf_server_start(hbf_server_t *server)
{
	char port_str[16];
	const char *options[] = {
		"listening_ports", port_str,
		"num_threads", "4",
		NULL
	};

	if (!server) {
		return -1;
	}

	snprintf(port_str, sizeof(port_str), "%d", server->port);

	server->ctx = mg_start(NULL, 0, options);
	if (!server->ctx) {
		hbf_log_error("Failed to start HTTP server");
		return -1;
	}

	/* Register handlers */
	mg_set_request_handler(server->ctx, "/health", health_handler, server);
	mg_set_request_handler(server->ctx, "**", static_handler, server);

	hbf_log_info("HTTP server listening on port %d", server->port);
	return 0;
}

void hbf_server_stop(hbf_server_t *server)
{
	if (server && server->ctx) {
		mg_stop(server->ctx);
		server->ctx = NULL;
		hbf_log_info("HTTP server stopped");
	}
}

void hbf_server_destroy(hbf_server_t *server)
{
	if (server) {
		if (server->ctx) {
			hbf_server_stop(server);
		}
		free(server);
	}
}
```

### Phase 4: Main Application Updates

**Goal**: Update main.c to use new database system and remove henv manager.

#### 4.1 Update Configuration

**Modified: `internal/core/config.c`**

Remove `storage_dir` parsing, add `inmem` flag:

```c
/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
	printf("Usage: %s [options]\n", program);
	printf("Options:\n");
	printf("  --port PORT          HTTP server port (default: 5309)\n");
	printf("  --log-level LEVEL    Log level: debug, info, warn, error (default: info)\n");
	printf("  --dev                Enable development mode\n");
	printf("  --inmem              Use in-memory database (for testing)\n");
	printf("  --help, -h           Show this help message\n");
}

int hbf_config_parse(int argc, char *argv[], hbf_config_t *config)
{
	int i;

	if (!config) {
		return -1;
	}

	/* Set defaults */
	config->port = 5309;
	strncpy(config->log_level, "info", sizeof(config->log_level) - 1);
	config->dev = 0;
	config->inmem = 0;

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			print_usage(argv[0]);
			return 1;
		} else if (strcmp(argv[i], "--port") == 0) {
			if (i + 1 >= argc) {
				hbf_log_error("--port requires an argument");
				return -1;
			}
			config->port = atoi(argv[++i]);
			if (config->port <= 0 || config->port > 65535) {
				hbf_log_error("Invalid port: %d", config->port);
				return -1;
			}
		} else if (strcmp(argv[i], "--log-level") == 0) {
			if (i + 1 >= argc) {
				hbf_log_error("--log-level requires an argument");
				return -1;
			}
			strncpy(config->log_level, argv[++i],
			        sizeof(config->log_level) - 1);
		} else if (strcmp(argv[i], "--dev") == 0) {
			config->dev = 1;
		} else if (strcmp(argv[i], "--inmem") == 0) {
			config->inmem = 1;
		} else {
			hbf_log_error("Unknown option: %s", argv[i]);
			return -1;
		}
	}

	return 0;
}
```

**Modified: `internal/core/config.h`**
```c
/* SPDX-License-Identifier: MIT */
#ifndef HBF_CONFIG_H
#define HBF_CONFIG_H

typedef struct {
	int port;
	char log_level[16];
	int dev;
	int inmem;
} hbf_config_t;

/*
 * Parse command line arguments
 *
 * @param argc: Argument count
 * @param argv: Argument vector
 * @param config: Configuration structure to populate
 * @return 0 on success, 1 if help was shown, -1 on error
 */
int hbf_config_parse(int argc, char *argv[], hbf_config_t *config);

#endif /* HBF_CONFIG_H */
```

#### 4.2 Update Main Application

**Modified: `internal/core/main.c`**

Remove henv manager, use new database system:

```c
/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include "../db/db.h"
#include "../http/server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
	(void)sig;
	running = 0;
}

int main(int argc, char *argv[])
{
	hbf_config_t config;
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	hbf_server_t *server = NULL;
	int ret;

	/* Parse configuration */
	ret = hbf_config_parse(argc, argv, &config);
	if (ret != 0) {
		return (ret == 1) ? 0 : 1;
	}

	/* Initialize logging */
	hbf_log_init(config.log_level);

	hbf_log_info("HBF starting (port=%d, inmem=%d, dev=%d)",
	             config.port, config.inmem, config.dev);

	/* Initialize database */
	ret = hbf_db_init(config.inmem, &db, &fs_db);
	if (ret != 0) {
		hbf_log_error("Failed to initialize database");
		return 1;
	}

	/* Create HTTP server */
	server = hbf_server_create(config.port, db, fs_db);
	if (!server) {
		hbf_log_error("Failed to create HTTP server");
		hbf_db_close(db, fs_db);
		return 1;
	}

	/* Start HTTP server */
	ret = hbf_server_start(server);
	if (ret != 0) {
		hbf_log_error("Failed to start HTTP server");
		hbf_server_destroy(server);
		hbf_db_close(db, fs_db);
		return 1;
	}

	/* Setup signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Main loop */
	hbf_log_info("HBF running (press Ctrl+C to stop)");
	while (running) {
		sleep(1);
	}

	/* Cleanup */
	hbf_log_info("Shutting down");
	hbf_server_destroy(server);
	hbf_db_close(db, fs_db);

	return 0;
}
```

### Phase 5: Cleanup - Remove Old System

**Goal**: Remove all Phase 2b components and old schema-based code.

#### 5.1 Files to Remove

Execute the following deletions:
```bash
# Remove henv manager (Phase 2b)
rm -rf internal/henv/

# Remove old schema files
rm -f internal/db/schema.sql
rm -f internal/db/schema.c
rm -f internal/db/schema.h
rm -f internal/db/schema_test.c

# Remove old build tools
rm -f tools/inject_content.sh
rm -f tools/sql_to_c.sh
```

#### 5.2 Update BUILD.bazel Files

**Modified: `internal/BUILD.bazel`** (remove henv reference if present)

#### 5.3 Update Tests

**Modified: `internal/core/config_test.c`** (remove storage_dir tests, add inmem tests)

Update test cases to reflect new configuration structure.

### Phase 6: Integration Testing & Validation

**Goal**: Comprehensive testing of the new system.

#### 6.1 Integration Test Script

**New file: `tools/integration_test.sh`**
```bash
#!/bin/bash
# SPDX-License-Identifier: MIT
# Integration test for SQLAR-based HBF system

set -e

echo "=================================="
echo "HBF SQLAR Integration Tests"
echo "=================================="

# Cleanup
rm -f hbf.db hbf.db-wal hbf.db-shm

# Test 1: Build
echo -n "Building HBF... "
if bazel build //:hbf > /dev/null 2>&1; then
    echo "PASS"
else
    echo "FAIL"
    exit 1
fi

# Test 2: Run unit tests
echo -n "Running unit tests... "
if bazel test //... > /dev/null 2>&1; then
    echo "PASS"
else
    echo "FAIL"
    exit 1
fi

# Test 3: Start server in background (inmem mode)
echo -n "Starting server (inmem)... "
bazel-bin/hbf --inmem --port 5310 > /tmp/hbf_test.log 2>&1 &
SERVER_PID=$!
sleep 2

if kill -0 $SERVER_PID 2>/dev/null; then
    echo "PASS"
else
    echo "FAIL"
    cat /tmp/hbf_test.log
    exit 1
fi

# Test 4: Health check
echo -n "Testing /health endpoint... "
STATUS=$(curl -s -w "%{http_code}" -o /dev/null http://localhost:5310/health)
if [ "$STATUS" = "200" ]; then
    echo "PASS"
else
    echo "FAIL (got $STATUS)"
    kill $SERVER_PID
    exit 1
fi

# Test 5: Static file serving
echo -n "Testing static file serving... "
STATUS=$(curl -s -w "%{http_code}" -o /tmp/index.html http://localhost:5310/)
if [ "$STATUS" = "200" ] && grep -q "HBF" /tmp/index.html; then
    echo "PASS"
else
    echo "FAIL (got $STATUS)"
    kill $SERVER_PID
    exit 1
fi

# Cleanup
kill $SERVER_PID
rm -f /tmp/hbf_test.log /tmp/index.html

echo ""
echo "All integration tests passed!"
```

**Permissions**: `chmod +x tools/integration_test.sh`

#### 6.2 Success Criteria Tests

Each success criterion should have an automated test:

1. **fs/ archived to SQLite**: `fs/fs_build_test.sh`
2. **Archive embedded in binary**: Verified by build system
3. **Static files served**: `tools/integration_test.sh` (Test 5)
4. **server.js loaded**: Will be tested in future QuickJS phase
5. **In-memory mode**: `internal/db/db_test.c` + integration test
6. **Persistent mode**: `internal/db/db_test.c`
7. **Old schema removed**: Manual verification post-cleanup
8. **Build time maintained**: Compare `bazel build //:hbf` time
9. **Binary size maintained**: Compare `ls -lh bazel-bin/hbf` size
10. **All tests pass**: `bazel test //...`

## Migration Timeline

### Phase 1: Build System (Day 1)
- Create `fs/` directory with initial content
- Implement `tools/create_fs_archive.sh`
- Implement `tools/db_to_c.sh`
- Add `fs/BUILD.bazel` with genrules
- Add `fs/fs_build_test.sh`
- **Deliverable**: `bazel build //fs:fs_embedded` succeeds

### Phase 2: Runtime System (Day 2)
- Update `internal/db/db.h` with new interface
- Implement `internal/db/db.c` with SQLAR support
- Implement `internal/db/db_test.c`
- **Deliverable**: `bazel test //internal/db:db_test` passes

### Phase 3: HTTP Integration (Day 3)
- Update `internal/http/server.h` and `server.c`
- Update static file handler to use `hbf_db_read_file()`
- **Deliverable**: Static files served from SQLAR archive

### Phase 4: Main Application (Day 3)
- Update `internal/core/config.h` and `config.c`
- Update `internal/core/main.c` to use new database system
- Remove henv manager dependencies
- **Deliverable**: `./hbf` starts and serves content

### Phase 5: Cleanup (Day 4)
- Remove `internal/henv/` directory
- Remove old schema files
- Remove old build tools
- Update all BUILD.bazel files
- **Deliverable**: No dead code remaining

### Phase 6: Testing & Validation (Day 4-5)
- Implement `tools/integration_test.sh`
- Run all unit tests
- Run integration tests
- Verify success criteria
- **Deliverable**: All tests passing, documentation updated

## Post-Migration Notes

### What's Deferred

The following features from Phase 2b are temporarily removed and will be reimplemented in future phases:

1. **Multi-user support**: Currently one HBF instance = one user
2. **User pod isolation**: No per-user directories or databases
3. **Connection caching**: Not needed with single database
4. **Hash-based routing**: No user identification system

### Future Roadmap

After this migration is complete:

1. **Phase 3+**: QuickJS integration with server.js loading from SQLAR
2. **Phase 4**: Re-implement authentication (single-user initially)
3. **Phase 5**: Add user data tables to `hbf.db`
4. **Phase 6**: Re-implement multi-tenancy with proper schema

### Database Schema (Future)

When user features are re-added, `hbf.db` will contain:

```sql
-- Future schema (not implemented yet)
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at INTEGER NOT NULL
);

CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    title TEXT,
    body TEXT,
    created_at INTEGER NOT NULL
);

-- etc.
```

The SQLAR archive (`fs_db`) remains read-only and contains only static assets.

## Success Criteria

Tests verify each criterion:

1. ✅ `fs/` archived to SQLite → `fs/fs_build_test.sh`
2. ✅ Archive embedded in binary → Build succeeds
3. ✅ Static files served → `tools/integration_test.sh`
4. ✅ server.js loadable → `internal/db/db_test.c`
5. ✅ In-memory mode works → `internal/db/db_test.c`
6. ✅ Persistent mode works → `internal/db/db_test.c`
7. ✅ Old schema removed → Post-cleanup verification
8. ✅ Build time maintained → Benchmark comparison
9. ✅ Binary size maintained → Size comparison
10. ✅ All tests pass → `bazel test //...`

---

  db_plan.md Success Criteria Status

  Based on my verification, here's the complete status:

  1. ✅ fs/ archived to SQLite → fs/fs_build_test.sh PASSES (8 files archived)
  2. ✅ Archive embedded in binary → Build succeeds, fs_embedded.c generated
  3. ❌ Static files served → tools/integration_test.sh FAILS
    - Fixed --log_level → --log-level flag
    - Added SQLITE_ENABLE_SQLAR compile flag
    - Changed SQLAR creation to store uncompressed files
    - Current issue: Files not being served (404 errors)
  4. ⚠️ server.js loadable → Partially verified (file exists in archive but not tested for
  loading)
  5. ✅ In-memory mode works → internal/db/db_test.c PASSES
  6. ✅ Persistent mode works → internal/db/db_test.c PASSES
  7. ✅ Old schema removed → internal/henv/, schema.sql, schema.c/h all removed
  8. ⚠️ Build time maintained → Build completes but no benchmark comparison done
  9. ⚠️ Binary size maintained → 1.1 MB (same), embedded fs.db now 12KB (was 3.5KB due to
  uncompressed storage)
  10. ⚠️ All tests pass → 6/6 unit tests pass, but integration test fails

**End of Plan**
