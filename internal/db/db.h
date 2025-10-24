/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_H
#define HBF_DB_H

#include <sqlite3.h>
#include <stddef.h>

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
