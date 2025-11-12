/* SPDX-License-Identifier: MIT */
#include "overlay_fs.h"
#include "hbf/shell/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <zlib.h>

/* Global database handle for filesystem operations */
/* Set by overlay_fs_init_global(), used by overlay_fs_read_file/overlay_fs_write_file */
static sqlite3 *g_overlay_db = NULL;

static int exec_sql_file(sqlite3 *db, const char *sql)
{
	char *errmsg = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);

	if (rc != SQLITE_OK) {
		hbf_log_error("SQL error: %s", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
}

int overlay_fs_check_schema(sqlite3 *db)
{
	const char *sql =
		"SELECT COUNT(1) FROM sqlite_master WHERE type='table' AND name IN ('file_versions','file_ids')";
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("overlay_fs: schema check prepare failed: %s", sqlite3_errmsg(db));
		return -1;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		hbf_log_error("overlay_fs: schema check step failed: %s", sqlite3_errmsg(db));
		return -1;
	}
	int cnt = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	if (cnt < 2) {
		hbf_log_error("FATAL: overlay_fs schema is missing. Expected tables 'file_versions' and 'file_ids'. This DB must be built with hbf/db/overlay_schema.sql at build time.");
		return -1;
	}
	return 0;
}

int overlay_fs_init(const char *db_path, sqlite3 **db)
{
	int rc;

	if (!db_path || !db) {
		hbf_log_error("overlay_fs_init: invalid arguments");
		return -1;
	}

	/* Open database */
	rc = sqlite3_open(db_path, db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Cannot open database: %s", sqlite3_errmsg(*db));
		return -1;
	}

	/* Enable foreign keys and set pragmas */
	if (exec_sql_file(*db, "PRAGMA foreign_keys=ON;") < 0) {
		goto err;
	}
	if (exec_sql_file(*db, "PRAGMA journal_mode=WAL;") < 0) {
		goto err;
	}
	if (exec_sql_file(*db, "PRAGMA synchronous=NORMAL;") < 0) {
		goto err;
	}

	/* Require schema to already exist; do not create fallback here */
	if (overlay_fs_check_schema(*db) < 0) {
		goto err;
	}

	/* No embedded fallback schema application here */

	return 0;

err:
	sqlite3_close(*db);
	*db = NULL;
	return -1;
}


/* SHA-256 implementation for bundle ID computation */
#define ROTRIGHT(a, b) (((a) >> (b)) | ((a) << (32 - (b))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))
#define SIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))

static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

typedef struct {
	uint8_t data[64];
	uint32_t datalen;
	uint64_t bitlen;
	uint32_t state[8];
} sha256_ctx_t;

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[])
{
	uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

	for (i = 0, j = 0; i < 16; ++i, j += 4) {
		m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
		       ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
	}
	for (; i < 64; ++i) {
		m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 64; ++i) {
		t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + m[i];
		t2 = EP0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
	ctx->datalen = 0;
	ctx->bitlen = 0;
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t data[], size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i) {
		ctx->data[ctx->datalen] = data[i];
		ctx->datalen++;
		if (ctx->datalen == 64) {
			sha256_transform(ctx, ctx->data);
			ctx->bitlen += 512;
			ctx->datalen = 0;
		}
	}
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[])
{
	uint32_t i;

	i = ctx->datalen;

	if (ctx->datalen < 56) {
		ctx->data[i++] = 0x80;
		while (i < 56) {
			ctx->data[i++] = 0x00;
		}
	} else {
		ctx->data[i++] = 0x80;
		while (i < 64) {
			ctx->data[i++] = 0x00;
		}
		sha256_transform(ctx, ctx->data);
		memset(ctx->data, 0, 56);
	}

	ctx->bitlen += (uint64_t)ctx->datalen * 8;
	ctx->data[63] = (uint8_t)(ctx->bitlen);
	ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
	ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
	ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
	ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
	ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
	ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
	ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
	sha256_transform(ctx, ctx->data);

	for (i = 0; i < 4; ++i) {
		hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
	}
}

/* Compute SHA-256 of data and return as hex string */
static void compute_bundle_id(const uint8_t *data, size_t len, char *bundle_id)
{
	sha256_ctx_t ctx;
	uint8_t hash[32];
	size_t i;

	sha256_init(&ctx);
	sha256_update(&ctx, data, len);
	sha256_final(&ctx, hash);

	/* Convert to hex string */
	for (i = 0; i < 32; i++) {
		sprintf(bundle_id + (i * 2), "%02x", hash[i]);
	}
	bundle_id[64] = '\0';
}

/* Read u32 from buffer (little-endian) */
static uint32_t read_u32(const uint8_t *buf)
{
	return (uint32_t)buf[0] |
	       ((uint32_t)buf[1] << 8) |
	       ((uint32_t)buf[2] << 16) |
	       ((uint32_t)buf[3] << 24);
}

migrate_status_t overlay_fs_migrate_assets(
	sqlite3 *db,
	const uint8_t *bundle_blob,
	size_t bundle_len
)
{
	char bundle_id[65];
	sqlite3_stmt *stmt = NULL;
	int rc;
	uint8_t *decompressed = NULL;
	uLongf decompressed_len;
	const uint8_t *ptr;
	const uint8_t *end;
	uint32_t num_entries;
	uint32_t i;
	int migrated_count = 0;

	if (!db || !bundle_blob || bundle_len == 0) {
		hbf_log_error("overlay_fs_migrate_assets: invalid arguments");
		return MIGRATE_ERR_DB;
	}

	/* Compute bundle ID (SHA-256 of compressed data) */
	compute_bundle_id(bundle_blob, bundle_len, bundle_id);

	/* Check if already applied (idempotency) */
	const char *check_sql = "SELECT 1 FROM migrations WHERE bundle_id = ? LIMIT 1";
	rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to check migrations: %s", sqlite3_errmsg(db));
		return MIGRATE_ERR_DB;
	}

	sqlite3_bind_text(stmt, 1, bundle_id, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW) {
		hbf_log_info("Asset bundle already applied (bundle_id=%s)", bundle_id);
		return MIGRATE_ERR_ALREADY_APPLIED;
	}

	hbf_log_info("Migrating asset bundle (bundle_id=%s, %zu bytes compressed)",
	             bundle_id, bundle_len);

	/* Decompress bundle */
	decompressed_len = (uLongf)(bundle_len * 10); /* Initial guess */
	decompressed = malloc(decompressed_len);
	if (!decompressed) {
		hbf_log_error("Failed to allocate decompression buffer");
		return MIGRATE_ERR_DECOMPRESS;
	}

	rc = uncompress(decompressed, &decompressed_len, bundle_blob, (uLong)bundle_len);
	if (rc != Z_OK) {
		hbf_log_error("Decompression failed: %d", rc);
		free(decompressed);
		return MIGRATE_ERR_DECOMPRESS;
	}

	hbf_log_info("Decompressed to %lu bytes", decompressed_len);

	/* Parse bundle */
	ptr = decompressed;
	end = decompressed + decompressed_len;

	/* Read num_entries */
	if (ptr + 4 > end) {
		hbf_log_error("Bundle too short for num_entries");
		free(decompressed);
		return MIGRATE_ERR_CORRUPT;
	}
	num_entries = read_u32(ptr);
	ptr += 4;

	hbf_log_info("Bundle contains %u entries", num_entries);

	/* Begin transaction */
	if (exec_sql_file(db, "BEGIN IMMEDIATE;") < 0) {
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	/* Process each entry */
	for (i = 0; i < num_entries; i++) {
		uint32_t name_len;
		const char *name;
		uint32_t data_len;
		const uint8_t *data;

		/* Read name_len */
		if (ptr + 4 > end) {
			hbf_log_error("Bundle truncated at entry %u name_len", i);
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_CORRUPT;
		}
		name_len = read_u32(ptr);
		ptr += 4;

		/* Read name */
		if (ptr + name_len > end) {
			hbf_log_error("Bundle truncated at entry %u name", i);
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_CORRUPT;
		}
		name = (const char *)ptr;
		ptr += name_len;

		/* Read data_len */
		if (ptr + 4 > end) {
			hbf_log_error("Bundle truncated at entry %u data_len", i);
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_CORRUPT;
		}
		data_len = read_u32(ptr);
		ptr += 4;

		/* Read data */
		if (ptr + data_len > end) {
			hbf_log_error("Bundle truncated at entry %u data", i);
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_CORRUPT;
		}
		data = ptr;
		ptr += data_len;

		/* Allocate null-terminated name string */
		char *name_str = malloc(name_len + 1);
		if (!name_str) {
			hbf_log_error("Failed to allocate name string");
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_DB;
		}
		memcpy(name_str, name, name_len);
		name_str[name_len] = '\0';

		/* Write to overlay_fs */
		if (overlay_fs_write(db, name_str, data, (size_t)data_len) < 0) {
			hbf_log_error("Failed to migrate file: %s", name_str);
			free(name_str);
			exec_sql_file(db, "ROLLBACK;");
			free(decompressed);
			return MIGRATE_ERR_DB;
		}

		hbf_log_debug("Migrated: %s (%u bytes)", name_str, data_len);
		free(name_str);
		migrated_count++;
	}

	/* Record migration */
	const char *insert_sql =
		"INSERT INTO migrations (bundle_id, applied_at, entries) "
		"VALUES (?, strftime('%s','now'), ?)";

	rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare migration insert: %s", sqlite3_errmsg(db));
		exec_sql_file(db, "ROLLBACK;");
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	sqlite3_bind_text(stmt, 1, bundle_id, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, migrated_count);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		hbf_log_error("Failed to record migration: %s", sqlite3_errmsg(db));
		exec_sql_file(db, "ROLLBACK;");
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	/* Commit transaction */
	if (exec_sql_file(db, "COMMIT;") < 0) {
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	free(decompressed);

	hbf_log_info("Successfully migrated %d files from asset bundle", migrated_count);
	return MIGRATE_OK;
}

int overlay_fs_read(sqlite3 *db, const char *path,
                    unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!db || !path || !data || !size) {
		hbf_log_error("overlay_fs_read: invalid arguments");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Fast query using file_id and MAX(version_number) */
	const char *sql =
		"WITH file_info AS (\n"
		"    SELECT file_id FROM file_ids WHERE path = ?\n"
		")\n"
		"SELECT fv.data\n"
		"FROM file_versions fv, file_info fi\n"
		"WHERE fv.file_id = fi.file_id\n"
		"ORDER BY fv.version_number DESC\n"
		"LIMIT 1";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare read: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(stmt, 0);
		int blob_size = sqlite3_column_bytes(stmt, 0);

		if (blob_size > 0) {
			*data = malloc((size_t)blob_size);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			memcpy(*data, blob, (size_t)blob_size);
			*size = (size_t)blob_size;
		} else {
			/* Empty file - allocate minimal buffer */
			*data = malloc(1);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			*size = 0;
		}
		sqlite3_finalize(stmt);
		return 0;
	}

	if (rc == SQLITE_DONE) {
		/* File not found */
		sqlite3_finalize(stmt);
		return -1;
	}

	hbf_log_error("Error reading file: %s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return -1;
}

int overlay_fs_write(sqlite3 *db, const char *path,
                     const unsigned char *data, size_t size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	int file_id = -1;
	int next_version = 1;

	if (!db || !path) {
		hbf_log_error("overlay_fs_write: invalid arguments");
		return -1;
	}

	/* Allow empty files (data can be NULL or empty string if size is 0) */
	if (size > 0 && !data) {
		hbf_log_error("overlay_fs_write: data is NULL but size > 0");
		return -1;
	}

	/* Get or create file_id */
	const char *get_id_sql = "SELECT file_id FROM file_ids WHERE path = ?";

	rc = sqlite3_prepare_v2(db, get_id_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare get_id: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		file_id = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	if (file_id < 0) {
		/* Create new file_id */
		const char *insert_id_sql = "INSERT INTO file_ids (path) VALUES (?)";

		rc = sqlite3_prepare_v2(db, insert_id_sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to prepare insert_id: %s", sqlite3_errmsg(db));
			return -1;
		}

		sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		if (rc != SQLITE_DONE) {
			hbf_log_error("Failed to insert file_id: %s", sqlite3_errmsg(db));
			return -1;
		}

		file_id = (int)sqlite3_last_insert_rowid(db);
	} else {
		/* Get next version number */
		const char *get_version_sql =
			"SELECT MAX(version_number) FROM file_versions WHERE file_id = ?";
		rc = sqlite3_prepare_v2(db, get_version_sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to prepare get_version: %s", sqlite3_errmsg(db));
			return -1;
		}

		sqlite3_bind_int(stmt, 1, file_id);
		rc = sqlite3_step(stmt);

		if (rc == SQLITE_ROW) {
			next_version = sqlite3_column_int(stmt, 0) + 1;
		}
		sqlite3_finalize(stmt);
	}

	/* Insert new version */
	const char *insert_sql =
		"INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) "
		"VALUES (?, ?, ?, ?, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare insert: %s", sqlite3_errmsg(db));
		return -1;
	}

	time_t now = time(NULL);

	sqlite3_bind_int(stmt, 1, file_id);
	sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, next_version);
	sqlite3_bind_int64(stmt, 4, (sqlite3_int64)now);
	sqlite3_bind_int64(stmt, 5, (sqlite3_int64)size);
	/* For empty files (size=0), use empty string to avoid binding NULL */
	/* sqlite3_bind_blob with NULL pointer binds SQL NULL, not zero-length BLOB */
	if (size == 0) {
		sqlite3_bind_blob(stmt, 6, "", 0, SQLITE_STATIC);
	} else {
		sqlite3_bind_blob(stmt, 6, data, (int)size, SQLITE_STATIC);
	}

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		hbf_log_error("Failed to insert version: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

int overlay_fs_exists(sqlite3 *db, const char *path)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!db || !path) {
		hbf_log_error("overlay_fs_exists: invalid arguments");
		return -1;
	}

	const char *sql = "SELECT 1 FROM file_ids WHERE path = ? LIMIT 1";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare exists: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW) {
		return 1;
	}
	if (rc == SQLITE_DONE) {
		return 0;
	}
	return -1;
}

int overlay_fs_version_count(sqlite3 *db, const char *path)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	int count = 0;

	if (!db || !path) {
		hbf_log_error("overlay_fs_version_count: invalid arguments");
		return -1;
	}

	const char *sql =
		"SELECT COUNT(*) FROM file_versions fv "
		"JOIN file_ids fi ON fv.file_id = fi.file_id "
		"WHERE fi.path = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare count: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_ROW) {
		return -1;
	}

	return count;
}

void overlay_fs_close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
	}
}

void overlay_fs_init_global(sqlite3 *db)
{
	g_overlay_db = db;
	if (db) {
		hbf_log_info("overlay_fs: Global database handle initialized");
	} else {
		hbf_log_warn("overlay_fs: Global database handle set to NULL");
	}
}

int overlay_fs_read_file(const char *path, int dev,
                         unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!g_overlay_db) {
		hbf_log_error("overlay_fs_read_file: global database not initialized");
		return -1;
	}

	if (!path || !data || !size) {
		hbf_log_error("overlay_fs_read_file: invalid arguments");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Read from latest_files view (contains migrated SQLAR data + overlays) */
	/* dev parameter reserved for future use */
	(void)dev; /* Suppress unused parameter warning */

	const char *sql = "SELECT data FROM latest_files WHERE path = ?";

	rc = sqlite3_prepare_v2(g_overlay_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read: %s",
		              sqlite3_errmsg(g_overlay_db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(stmt, 0);
		int blob_size = sqlite3_column_bytes(stmt, 0);

		if (blob_size > 0) {
			*data = malloc((size_t)blob_size);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			memcpy(*data, blob, (size_t)blob_size);
			*size = (size_t)blob_size;
		} else {
			/* Empty file - allocate minimal buffer */
			*data = malloc(1);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			*size = 0;
		}
		sqlite3_finalize(stmt);
		hbf_log_debug("overlay_fs: Read file '%s' (%zu bytes)", path, *size);
		return 0;
	}

	if (rc == SQLITE_DONE) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("overlay_fs: File not found: %s", path);
		return -1;
	}

	hbf_log_error("Error reading file: %s", sqlite3_errmsg(g_overlay_db));
	sqlite3_finalize(stmt);
	return -1;
}

int overlay_fs_write_file(const char *path,
                          const unsigned char *data, size_t size)
{
	if (!g_overlay_db) {
		hbf_log_error("overlay_fs_write_file: global database not initialized");
		return -1;
	}

	/* Delegate to existing overlay_fs_write implementation */
	return overlay_fs_write(g_overlay_db, path, data, size);
}
