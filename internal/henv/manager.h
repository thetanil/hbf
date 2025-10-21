/* SPDX-License-Identifier: MIT */
#ifndef HBF_HENV_MANAGER_H
#define HBF_HENV_MANAGER_H

#include <sqlite3.h>
#include <stdbool.h>

/*
 * User pod (henv) manager for HBF
 *
 * Provides multi-tenancy infrastructure with isolated user pods,
 * each containing their own SQLite database. Includes connection
 * caching for performance and thread safety.
 */

/*
 * Initialize the henv manager
 *
 * @param storage_dir: Base directory for user pod storage (e.g., "./henvs")
 * @param max_connections: Maximum number of cached connections (0 = unlimited)
 * @return 0 on success, -1 on error
 */
int hbf_henv_init(const char *storage_dir, int max_connections);

/*
 * Shutdown the henv manager and close all cached connections
 */
void hbf_henv_shutdown(void);

/*
 * Create a new user pod directory and initialize the database
 *
 * @param user_hash: 8-character DNS-safe hash identifier
 * @return 0 on success, -1 on error (includes hash collision)
 */
int hbf_henv_create_user_pod(const char *user_hash);

/*
 * Check if a user pod exists
 *
 * @param user_hash: 8-character DNS-safe hash identifier
 * @return true if exists, false otherwise
 */
bool hbf_henv_user_exists(const char *user_hash);

/*
 * Open a database connection to a user pod (with caching)
 *
 * Returns a cached connection if available, otherwise opens a new one.
 * Caller must NOT close the returned handle - it's managed by the cache.
 *
 * @param user_hash: 8-character DNS-safe hash identifier
 * @param db: Output parameter for database handle
 * @return 0 on success, -1 on error
 */
int hbf_henv_open(const char *user_hash, sqlite3 **db);

/*
 * Close all cached connections (for shutdown or maintenance)
 */
void hbf_henv_close_all(void);

/*
 * Get the full path to a user pod's database file
 *
 * @param user_hash: 8-character DNS-safe hash identifier
 * @param path_out: Buffer to receive the path (must be at least 256 bytes)
 * @return 0 on success, -1 on error
 */
int hbf_henv_get_db_path(const char *user_hash, char *path_out);

#endif /* HBF_HENV_MANAGER_H */
