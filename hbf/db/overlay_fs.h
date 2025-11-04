/* SPDX-License-Identifier: MIT */
#ifndef OVERLAY_FS_H
#define OVERLAY_FS_H

#include <sqlite3.h>
#include <stddef.h>

/*
 * Versioned file system using SQLite
 *
 * Features:
 * - All file writes create new versions (immutable history)
 * - Fast reads using indexed queries
 * - Single SQLite database, no external dependencies
 */

/*
 * Initialize overlay_fs database
 *
 * Opens or creates the database and applies schema.
 * If embedded fs.db exists, migrates SQLAR data to file_versions.
 *
 * @param db_path: Path to database file (or ":memory:" for in-memory)
 * @param db: Output parameter for database handle
 * @return 0 on success, -1 on error
 */
int overlay_fs_init(const char *db_path, sqlite3 **db);

/*
 * Migrate SQLAR archive to file_versions table
 *
 * Reads all entries from sqlar table, decompresses them,
 * and inserts as version 1 in file_versions.
 * Drops sqlar table afterward and vacuums database.
 *
 * @param db: Database handle
 * @return 0 on success, -1 on error
 */
int overlay_fs_migrate_sqlar(sqlite3 *db);

/*
 * Read latest version of a file
 *
 * Uses optimized query with file_id and MAX(version_number).
 *
 * @param db: Database handle
 * @param path: File path
 * @param data: Output parameter for file data (caller must free)
 * @param size: Output parameter for file size
 * @return 0 on success, -1 on error (file not found or SQL error)
 */
int overlay_fs_read(sqlite3 *db, const char *path,
                    unsigned char **data, size_t *size);

/*
 * Write new version of a file
 *
 * Creates new version entry with incremented version_number.
 * If file doesn't exist, creates new file_id.
 *
 * @param db: Database handle
 * @param path: File path
 * @param data: File data
 * @param size: File size
 * @return 0 on success, -1 on error
 */
int overlay_fs_write(sqlite3 *db, const char *path,
                     const unsigned char *data, size_t size);

/*
 * Check if file exists
 *
 * @param db: Database handle
 * @param path: File path
 * @return 1 if exists, 0 if not found, -1 on error
 */
int overlay_fs_exists(sqlite3 *db, const char *path);

/*
 * Get file version count
 *
 * @param db: Database handle
 * @param path: File path
 * @return Version count (>= 1), 0 if not found, -1 on error
 */
int overlay_fs_version_count(sqlite3 *db, const char *path);

/*
 * Close database handle
 *
 * @param db: Database handle
 */
void overlay_fs_close(sqlite3 *db);

#endif /* OVERLAY_FS_H */
