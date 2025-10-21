/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_DB_H
#define HBF_DB_DB_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * SQLite database wrapper for HBF
 *
 * Provides basic database operations with proper error handling,
 * prepared statements, and result mapping.
 */

/*
 * Open a SQLite database connection with HBF defaults:
 * - foreign_keys=ON
 * - journal_mode=WAL
 * - synchronous=NORMAL
 *
 * @param path: Path to the SQLite database file
 * @param db: Output parameter for the database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_open(const char *path, sqlite3 **db);

/*
 * Close a SQLite database connection
 *
 * @param db: Database handle to close
 */
void hbf_db_close(sqlite3 *db);

/*
 * Execute a SQL statement without results (DDL, INSERT, UPDATE, DELETE)
 *
 * @param db: Database handle
 * @param sql: SQL statement to execute
 * @return 0 on success, -1 on error
 */
int hbf_db_exec(sqlite3 *db, const char *sql);

/*
 * Get the last error message from SQLite
 *
 * @param db: Database handle
 * @return Error message string (do not free)
 */
const char *hbf_db_error(sqlite3 *db);

/*
 * Get the last inserted row ID
 *
 * @param db: Database handle
 * @return Last insert row ID
 */
int64_t hbf_db_last_insert_id(sqlite3 *db);

/*
 * Get the number of rows affected by the last statement
 *
 * @param db: Database handle
 * @return Number of rows changed
 */
int hbf_db_changes(sqlite3 *db);

/*
 * Begin a transaction
 *
 * @param db: Database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_begin(sqlite3 *db);

/*
 * Commit a transaction
 *
 * @param db: Database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_commit(sqlite3 *db);

/*
 * Rollback a transaction
 *
 * @param db: Database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_rollback(sqlite3 *db);

#endif /* HBF_DB_DB_H */
