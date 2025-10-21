/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_SCHEMA_H
#define HBF_DB_SCHEMA_H

#include <sqlite3.h>

/*
 * Database schema initialization for HBF
 *
 * Provides functions to initialize the database schema with:
 * - Document-graph core tables (nodes, edges, tags)
 * - HBF system tables (_hbf_*)
 * - FTS5 full-text search
 * - Default configuration values
 */

/*
 * Initialize database schema
 *
 * Creates all tables, indexes, triggers, and inserts default configuration.
 * Safe to call on existing database - all DDL uses IF NOT EXISTS.
 *
 * @param db: SQLite database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_init_schema(sqlite3 *db);

/*
 * Get current schema version from database
 *
 * @param db: SQLite database handle
 * @return Schema version (>= 1), or 0 if not initialized, -1 on error
 */
int hbf_db_get_schema_version(sqlite3 *db);

/*
 * Check if database has been initialized
 *
 * @param db: SQLite database handle
 * @return 1 if initialized, 0 if not, -1 on error
 */
int hbf_db_is_initialized(sqlite3 *db);

#endif /* HBF_DB_SCHEMA_H */
