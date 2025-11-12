/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_H
#define HBF_DB_H

#include <sqlite3.h>
#include <stddef.h>

/*
 * Database initialization and management
 *
 * HBF uses a single database for storing all application assets:
 * - Default: On-disk SQLite at ./hbf.db (persistent across restarts)
 * - Testing: In-memory (:memory:) via --inmem flag
 *
 * Assets are embedded at build time via asset_packer tool, then migrated
 * into the database at startup using the overlay_fs versioned filesystem.
 * Migration is idempotent based on SHA256 bundle hash.
 */

/*
 * Initialize HBF database
 *
 * Opens ./hbf.db (or :memory: if inmem=1), creates schema, and migrates
 * embedded asset bundle into the database. Migration is idempotent.
 *
 * @param inmem: If 1, create in-memory database; if 0, use ./hbf.db
 * @param db: Output parameter for database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_init(int inmem, sqlite3 **db);

/*
 * Close HBF database handle
 *
 * @param db: Database handle
 */
void hbf_db_close(sqlite3 *db);

/*
 * Read file from main database
 *
 * @param db: Database handle
 * @param path: File path (e.g., "hbf/server.js")
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error
 */
int hbf_db_read_file_from_main(sqlite3 *db, const char *path,
                                unsigned char **data, size_t *size);

/*
 * Read file with optional overlay support
 *
 * Reads from latest_files view which includes both base files and overlays.
 * The use_overlay parameter is currently unused (all reads include overlays).
 *
 * @param db: Database handle
 * @param path: File path (e.g., "static/index.html")
 * @param use_overlay: Reserved for future use
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error
 */
int hbf_db_read_file(sqlite3 *db, const char *path, int use_overlay,
                     unsigned char **data, size_t *size);

/*
 * Check if file exists in main database
 *
 * @param db: Database handle
 * @param path: File path
 * @return 1 if exists, 0 if not found, -1 on error
 */
int hbf_db_file_exists_in_main(sqlite3 *db, const char *path);

/*
 * Check if database has sqlar table (legacy function)
 *
 * This function is deprecated and will be removed in a future version.
 * SQLAR is no longer used; assets are now stored in the versioned filesystem.
 *
 * @param db: Database handle
 * @return 1 if sqlar table exists, 0 if not, -1 on error
 */
int hbf_db_check_sqlar_table(sqlite3 *db);

#endif /* HBF_DB_H */
