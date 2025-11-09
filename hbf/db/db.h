/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_H
#define HBF_DB_H

#include <sqlite3.h>
#include <stddef.h>

/*
 * Database initialization and management
 *
 * HBF uses one main database with embedded template hydration:
 * 1. Runtime hbf.db (./hbf.db or :memory:) - Contains SQLAR table with app assets
 * 2. Embedded fs.db template - Used internally to hydrate main DB when needed
 *
 * The embedded fs_db is kept private within the DB module and not exposed
 * to callers. All runtime asset reads go through the main database.
 */

/*
 * Initialize HBF database
 *
 * Creates or opens ./hbf.db (or in-memory if inmem=1).
 * Hydrates from embedded fs.db template when needed.
 *
 * @param inmem: If 1, create in-memory database; if 0, use ./hbf.db
 * @param db: Output parameter for main database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_init(int inmem, sqlite3 **db);

/*
 * Close HBF database handle
 *
 * Also closes internal fs_db template handle if open.
 *
 * @param db: Main database handle
 */
void hbf_db_close(sqlite3 *db);

/*
 * Read file from main database SQLAR archive
 *
 * @param db: Main database handle
 * @param path: File path within archive (e.g., "hbf/server.js")
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error
 */
int hbf_db_read_file_from_main(sqlite3 *db, const char *path,
                                unsigned char **data, size_t *size);

/*
 * Read file with optional overlay support
 *
 * When use_overlay=1, reads from latest_fs view (overlay + base).
 * When use_overlay=0, reads from base sqlar only.
 *
 * @param db: Main database handle
 * @param path: File path within archive (e.g., "static/index.html")
 * @param use_overlay: If 1, read from overlay; if 0, read from base only
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error
 */
int hbf_db_read_file(sqlite3 *db, const char *path, int use_overlay,
                     unsigned char **data, size_t *size);

/*
 * Check if file exists in main database SQLAR archive
 *
 * @param db: Main database handle
 * @param path: File path within archive
 * @return 1 if exists, 0 if not found, -1 on error
 */
int hbf_db_file_exists_in_main(sqlite3 *db, const char *path);

/*
 * Check if database has sqlar table
 *
 * @param db: Database handle
 * @return 1 if sqlar table exists, 0 if not, -1 on error
 */
int hbf_db_check_sqlar_table(sqlite3 *db);

#endif /* HBF_DB_H */
