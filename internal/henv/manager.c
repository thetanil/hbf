/* SPDX-License-Identifier: MIT */
#include "manager.h"
#include "../core/log.h"
#include "../db/db.h"
#include "../db/schema.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Connection cache entry */
typedef struct hbf_cache_entry {
	char user_hash[9];
	sqlite3 *db;
	time_t last_used;
	struct hbf_cache_entry *next;
} hbf_cache_entry_t;

/* Global manager state */
static struct {
	char storage_dir[256];
	int max_connections;
	hbf_cache_entry_t *cache_head;
	pthread_mutex_t cache_mutex;
	bool initialized;
} g_henv_manager;

int hbf_henv_init(const char *storage_dir, int max_connections)
{
	struct stat st;

	if (!storage_dir) {
		hbf_log_error("henv_init: storage_dir cannot be NULL");
		return -1;
	}

	if (g_henv_manager.initialized) {
		hbf_log_warn("henv_init: already initialized");
		return 0;
	}

	/* Copy storage directory path */
	if (strlen(storage_dir) >= sizeof(g_henv_manager.storage_dir)) {
		hbf_log_error("henv_init: storage_dir path too long");
		return -1;
	}
	strncpy(g_henv_manager.storage_dir, storage_dir,
		sizeof(g_henv_manager.storage_dir) - 1);
	g_henv_manager.storage_dir[sizeof(g_henv_manager.storage_dir) - 1] = '\0';

	/* Create storage directory if it doesn't exist */
	if (stat(storage_dir, &st) != 0) {
		if (mkdir(storage_dir, 0700) != 0) {
			hbf_log_error("henv_init: failed to create storage_dir %s: %s",
				      storage_dir, strerror(errno));
			return -1;
		}
		hbf_log_info("Created storage directory: %s", storage_dir);
	} else if (!S_ISDIR(st.st_mode)) {
		hbf_log_error("henv_init: storage_dir %s exists but is not a directory",
			      storage_dir);
		return -1;
	}

	g_henv_manager.max_connections = max_connections;
	g_henv_manager.cache_head = NULL;

	if (pthread_mutex_init(&g_henv_manager.cache_mutex, NULL) != 0) {
		hbf_log_error("henv_init: failed to initialize cache mutex");
		return -1;
	}

	g_henv_manager.initialized = true;
	hbf_log_info("Initialized henv manager: storage_dir=%s, max_connections=%d",
		     storage_dir, max_connections);
	return 0;
}

void hbf_henv_shutdown(void)
{
	if (!g_henv_manager.initialized) {
		return;
	}

	hbf_henv_close_all();
	pthread_mutex_destroy(&g_henv_manager.cache_mutex);
	g_henv_manager.initialized = false;
	hbf_log_info("Shutdown henv manager");
}

int hbf_henv_get_db_path(const char *user_hash, char *path_out)
{
	int ret;

	if (!user_hash || !path_out) {
		return -1;
	}

	if (!g_henv_manager.initialized) {
		hbf_log_error("henv_get_db_path: manager not initialized");
		return -1;
	}

	/* Validate user_hash: 8 characters, alphanumeric lowercase */
	if (strlen(user_hash) != 8) {
		hbf_log_error("henv_get_db_path: invalid user_hash length: %s",
			      user_hash);
		return -1;
	}

	ret = snprintf(path_out, 256, "%s/%s/index.db",
		       g_henv_manager.storage_dir, user_hash);
	if (ret < 0 || ret >= 256) {
		hbf_log_error("henv_get_db_path: path too long for user_hash: %s",
			      user_hash);
		return -1;
	}

	return 0;
}

int hbf_henv_create_user_pod(const char *user_hash)
{
	char pod_dir[256];
	char db_path[256];
	struct stat st;
	int ret;
	sqlite3 *db;

	if (!user_hash) {
		hbf_log_error("create_user_pod: user_hash cannot be NULL");
		return -1;
	}

	if (!g_henv_manager.initialized) {
		hbf_log_error("create_user_pod: manager not initialized");
		return -1;
	}

	/* Validate user_hash: 8 characters */
	if (strlen(user_hash) != 8) {
		hbf_log_error("create_user_pod: invalid user_hash length: %s",
			      user_hash);
		return -1;
	}

	/* Build pod directory path */
	ret = snprintf(pod_dir, sizeof(pod_dir), "%s/%s",
		       g_henv_manager.storage_dir, user_hash);
	if (ret < 0 || ret >= (int)sizeof(pod_dir)) {
		hbf_log_error("create_user_pod: pod_dir path too long");
		return -1;
	}

	/* Check for hash collision */
	if (stat(pod_dir, &st) == 0) {
		hbf_log_error("create_user_pod: hash collision detected for %s",
			      user_hash);
		return -1;
	}

	/* Create user pod directory (mode 0700) */
	if (mkdir(pod_dir, 0700) != 0) {
		hbf_log_error("create_user_pod: failed to create pod_dir %s: %s",
			      pod_dir, strerror(errno));
		return -1;
	}

	/* Build database path */
	if (hbf_henv_get_db_path(user_hash, db_path) != 0) {
		rmdir(pod_dir); /* Cleanup on failure */
		return -1;
	}

	/* Create and initialize database */
	if (hbf_db_open(db_path, &db) != 0) {
		hbf_log_error("create_user_pod: failed to create database");
		rmdir(pod_dir); /* Cleanup on failure */
		return -1;
	}

	/* Initialize schema */
	if (hbf_db_init_schema(db) != 0) {
		hbf_log_error("create_user_pod: failed to initialize schema");
		hbf_db_close(db);
		unlink(db_path);
		rmdir(pod_dir);
		return -1;
	}

	hbf_db_close(db);

	/* Set database file permissions to 0600 */
	if (chmod(db_path, 0600) != 0) {
		hbf_log_warn("create_user_pod: failed to set db permissions: %s",
			     strerror(errno));
	}

	hbf_log_info("Created user pod: %s (dir: %s, db: %s)",
		     user_hash, pod_dir, db_path);
	return 0;
}

bool hbf_henv_user_exists(const char *user_hash)
{
	char pod_dir[256];
	struct stat st;
	int ret;

	if (!user_hash || !g_henv_manager.initialized) {
		return false;
	}

	/* Validate user_hash: 8 characters */
	if (strlen(user_hash) != 8) {
		return false;
	}

	ret = snprintf(pod_dir, sizeof(pod_dir), "%s/%s",
		       g_henv_manager.storage_dir, user_hash);
	if (ret < 0 || ret >= (int)sizeof(pod_dir)) {
		return false;
	}

	return stat(pod_dir, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Cache helper: find entry by user_hash */
static hbf_cache_entry_t *cache_find(const char *user_hash)
{
	hbf_cache_entry_t *entry;

	for (entry = g_henv_manager.cache_head; entry != NULL; entry = entry->next) {
		if (strcmp(entry->user_hash, user_hash) == 0) {
			return entry;
		}
	}
	return NULL;
}

/* Cache helper: count entries */
static int cache_count(void)
{
	hbf_cache_entry_t *entry;
	int count = 0;

	for (entry = g_henv_manager.cache_head; entry != NULL; entry = entry->next) {
		count++;
	}
	return count;
}

/* Cache helper: remove least recently used entry */
static void cache_evict_lru(void)
{
	hbf_cache_entry_t *entry, *prev, *lru, *lru_prev;
	time_t oldest_time;

	if (!g_henv_manager.cache_head) {
		return;
	}

	/* Find LRU entry */
	lru = g_henv_manager.cache_head;
	lru_prev = NULL;
	oldest_time = lru->last_used;

	prev = g_henv_manager.cache_head;
	for (entry = g_henv_manager.cache_head->next; entry != NULL; entry = entry->next) {
		if (entry->last_used < oldest_time) {
			lru = entry;
			lru_prev = prev;
			oldest_time = entry->last_used;
		}
		prev = entry;
	}

	/* Remove LRU entry */
	if (lru_prev) {
		lru_prev->next = lru->next;
	} else {
		g_henv_manager.cache_head = lru->next;
	}

	hbf_log_debug("Evicting cached connection for user_hash: %s", lru->user_hash);
	hbf_db_close(lru->db);
	free(lru);
}

/* Cache helper: add new entry */
static int cache_add(const char *user_hash, sqlite3 *db)
{
	hbf_cache_entry_t *entry;

	/* Check if we need to evict */
	if (g_henv_manager.max_connections > 0 &&
	    cache_count() >= g_henv_manager.max_connections) {
		cache_evict_lru();
	}

	entry = malloc(sizeof(hbf_cache_entry_t));
	if (!entry) {
		hbf_log_error("cache_add: malloc failed");
		return -1;
	}

	strncpy(entry->user_hash, user_hash, sizeof(entry->user_hash));
	entry->user_hash[sizeof(entry->user_hash) - 1] = '\0';
	entry->db = db;
	entry->last_used = time(NULL);
	entry->next = g_henv_manager.cache_head;
	g_henv_manager.cache_head = entry;

	hbf_log_debug("Cached connection for user_hash: %s", user_hash);
	return 0;
}

int hbf_henv_open(const char *user_hash, sqlite3 **db)
{
	char db_path[256];
	hbf_cache_entry_t *entry;
	sqlite3 *new_db;

	if (!user_hash || !db) {
		return -1;
	}

	if (!g_henv_manager.initialized) {
		hbf_log_error("henv_open: manager not initialized");
		return -1;
	}

	/* Validate user_hash */
	if (strlen(user_hash) != 8) {
		hbf_log_error("henv_open: invalid user_hash length: %s", user_hash);
		return -1;
	}

	/* Check if user pod exists */
	if (!hbf_henv_user_exists(user_hash)) {
		hbf_log_error("henv_open: user pod does not exist: %s", user_hash);
		return -1;
	}

	pthread_mutex_lock(&g_henv_manager.cache_mutex);

	/* Check cache first */
	entry = cache_find(user_hash);
	if (entry) {
		entry->last_used = time(NULL);
		*db = entry->db;
		pthread_mutex_unlock(&g_henv_manager.cache_mutex);
		hbf_log_debug("Returning cached connection for user_hash: %s", user_hash);
		return 0;
	}

	/* Not in cache, open new connection */
	if (hbf_henv_get_db_path(user_hash, db_path) != 0) {
		pthread_mutex_unlock(&g_henv_manager.cache_mutex);
		return -1;
	}

	if (hbf_db_open(db_path, &new_db) != 0) {
		pthread_mutex_unlock(&g_henv_manager.cache_mutex);
		hbf_log_error("henv_open: failed to open database for %s", user_hash);
		return -1;
	}

	/* Add to cache */
	if (cache_add(user_hash, new_db) != 0) {
		hbf_db_close(new_db);
		pthread_mutex_unlock(&g_henv_manager.cache_mutex);
		return -1;
	}

	*db = new_db;
	pthread_mutex_unlock(&g_henv_manager.cache_mutex);
	hbf_log_debug("Opened new connection for user_hash: %s", user_hash);
	return 0;
}

void hbf_henv_close_all(void)
{
	hbf_cache_entry_t *entry, *next;

	if (!g_henv_manager.initialized) {
		return;
	}

	pthread_mutex_lock(&g_henv_manager.cache_mutex);

	entry = g_henv_manager.cache_head;
	while (entry) {
		next = entry->next;
		hbf_log_debug("Closing cached connection for user_hash: %s",
			      entry->user_hash);
		hbf_db_close(entry->db);
		free(entry);
		entry = next;
	}

	g_henv_manager.cache_head = NULL;
	pthread_mutex_unlock(&g_henv_manager.cache_mutex);

	hbf_log_info("Closed all cached connections");
}
