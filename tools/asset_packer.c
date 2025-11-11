/* SPDX-License-Identifier: MIT */
/*
 * asset_packer - Hermetic asset bundling tool for HBF
 *
 * Takes input files, packs them into a deterministic binary format,
 * compresses with miniz, and outputs as C arrays.
 *
 * Usage:
 *   asset_packer --output-source out.c --output-header out.h \
 *                --symbol-name assets_blob file1.js file2.html ...
 *
 * Binary format:
 *   [num_entries:u32]
 *   [name_len:u32][name:bytes][data_len:u32][data:bytes]
 *   ...
 *
 * The entire bundle is then compressed with miniz at max compression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <miniz.h>

#define MAX_PATH_LEN 4096
#define MAX_FILE_SIZE (100 * 1024 * 1024) /* 100MB per file */

typedef struct {
	char *path;
	uint8_t *data;
	uint32_t data_len;
} file_entry_t;

typedef struct {
	file_entry_t *entries;
	uint32_t count;
	uint32_t capacity;
} bundle_t;

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [OPTIONS] FILE...\n", prog);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --output-source FILE  Output C source file\n");
	fprintf(stderr, "  --output-header FILE  Output C header file\n");
	fprintf(stderr, "  --symbol-name NAME    Symbol name (default: assets_blob)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "  %s --output-source out.c --output-header out.h \\\n", prog);
	fprintf(stderr, "     --symbol-name assets_blob file1.js file2.html\n");
}

static int bundle_init(bundle_t *bundle, uint32_t capacity)
{
	bundle->entries = calloc(capacity, sizeof(file_entry_t));
	if (!bundle->entries) {
		return -1;
	}
	bundle->count = 0;
	bundle->capacity = capacity;
	return 0;
}

static void bundle_free(bundle_t *bundle)
{
	uint32_t i;

	if (!bundle) {
		return;
	}

	for (i = 0; i < bundle->count; i++) {
		free(bundle->entries[i].path);
		free(bundle->entries[i].data);
	}
	free(bundle->entries);
	bundle->entries = NULL;
	bundle->count = 0;
	bundle->capacity = 0;
}

static int bundle_add_file(bundle_t *bundle, const char *path)
{
	FILE *f;
	long file_size;
	uint8_t *data;
	size_t bytes_read;
	file_entry_t *entry;

	if (bundle->count >= bundle->capacity) {
		fprintf(stderr, "Error: bundle capacity exceeded\n");
		return -1;
	}

	/* Open file */
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Error: failed to open file '%s'\n", path);
		return -1;
	}

	/* Get file size */
	if (fseek(f, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error: failed to seek in file '%s'\n", path);
		fclose(f);
		return -1;
	}

	file_size = ftell(f);
	if (file_size < 0) {
		fprintf(stderr, "Error: failed to get file size '%s'\n", path);
		fclose(f);
		return -1;
	}

	if (file_size > MAX_FILE_SIZE) {
		fprintf(stderr, "Error: file '%s' exceeds maximum size\n", path);
		fclose(f);
		return -1;
	}

	rewind(f);

	/* Read file data */
	data = malloc((size_t)file_size);
	if (!data) {
		fprintf(stderr, "Error: failed to allocate memory for file '%s'\n", path);
		fclose(f);
		return -1;
	}

	bytes_read = fread(data, 1, (size_t)file_size, f);
	fclose(f);

	if (bytes_read != (size_t)file_size) {
		fprintf(stderr, "Error: failed to read complete file '%s'\n", path);
		free(data);
		return -1;
	}

	/* Add to bundle */
	entry = &bundle->entries[bundle->count];
	entry->path = strdup(path);
	if (!entry->path) {
		fprintf(stderr, "Error: failed to allocate path string\n");
		free(data);
		return -1;
	}
	entry->data = data;
	entry->data_len = (uint32_t)file_size;
	bundle->count++;

	return 0;
}

/* Compare function for qsort - lexicographic ordering */
static int compare_entries(const void *a, const void *b)
{
	const file_entry_t *ea = (const file_entry_t *)a;
	const file_entry_t *eb = (const file_entry_t *)b;
	return strcmp(ea->path, eb->path);
}

static int bundle_pack(bundle_t *bundle, uint8_t **out_data, size_t *out_len)
{
	size_t total_size;
	uint8_t *packed;
	uint8_t *ptr;
	uint32_t i;

	/* Sort entries for deterministic output */
	qsort(bundle->entries, bundle->count, sizeof(file_entry_t), compare_entries);

	/* Calculate total size: num_entries + sum(name_len + name + data_len + data) */
	total_size = sizeof(uint32_t); /* num_entries */
	for (i = 0; i < bundle->count; i++) {
		uint32_t name_len = (uint32_t)strlen(bundle->entries[i].path);
		total_size += sizeof(uint32_t); /* name_len */
		total_size += name_len;         /* name */
		total_size += sizeof(uint32_t); /* data_len */
		total_size += bundle->entries[i].data_len; /* data */
	}

	/* Allocate packed buffer */
	packed = malloc(total_size);
	if (!packed) {
		fprintf(stderr, "Error: failed to allocate packed buffer\n");
		return -1;
	}

	/* Pack entries */
	ptr = packed;

	/* Write num_entries */
	memcpy(ptr, &bundle->count, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	/* Write each entry */
	for (i = 0; i < bundle->count; i++) {
		uint32_t name_len = (uint32_t)strlen(bundle->entries[i].path);

		/* name_len */
		memcpy(ptr, &name_len, sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		/* name */
		memcpy(ptr, bundle->entries[i].path, name_len);
		ptr += name_len;

		/* data_len */
		memcpy(ptr, &bundle->entries[i].data_len, sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		/* data */
		memcpy(ptr, bundle->entries[i].data, bundle->entries[i].data_len);
		ptr += bundle->entries[i].data_len;
	}

	*out_data = packed;
	*out_len = total_size;
	return 0;
}

static int compress_bundle(const uint8_t *data, size_t data_len,
                           uint8_t **out_data, size_t *out_len)
{
	mz_ulong compressed_len;
	uint8_t *compressed;
	int status;

	/* Allocate worst-case compressed size */
	compressed_len = mz_compressBound((mz_ulong)data_len);
	compressed = malloc(compressed_len);
	if (!compressed) {
		fprintf(stderr, "Error: failed to allocate compression buffer\n");
		return -1;
	}

	/* Compress at maximum level (9) */
	status = mz_compress2(compressed, &compressed_len, data, (mz_ulong)data_len, 9);
	if (status != MZ_OK) {
		fprintf(stderr, "Error: compression failed with status %d\n", status);
		free(compressed);
		return -1;
	}

	*out_data = compressed;
	*out_len = (size_t)compressed_len;
	return 0;
}

static int write_c_source(const char *output_source, const char *output_header,
                          const char *symbol_name, const uint8_t *data, size_t data_len)
{
	FILE *f;
	size_t i;

	/* Write source file */
	f = fopen(output_source, "w");
	if (!f) {
		fprintf(stderr, "Error: failed to open output source '%s'\n", output_source);
		return -1;
	}

	fprintf(f, "/* Auto-generated by asset_packer - do not edit */\n");
	fprintf(f, "#include \"%s\"\n\n", output_header);
	fprintf(f, "const unsigned char %s[] = {", symbol_name);

	for (i = 0; i < data_len; i++) {
		if (i % 12 == 0) {
			fprintf(f, "\n\t");
		}
		fprintf(f, "0x%02x", data[i]);
		if (i < data_len - 1) {
			fprintf(f, ", ");
		}
	}

	fprintf(f, "\n};\n\n");
	fprintf(f, "const size_t %s_len = %zu;\n", symbol_name, data_len);

	fclose(f);

	/* Write header file */
	f = fopen(output_header, "w");
	if (!f) {
		fprintf(stderr, "Error: failed to open output header '%s'\n", output_header);
		return -1;
	}

	fprintf(f, "/* Auto-generated by asset_packer - do not edit */\n");
	fprintf(f, "#ifndef ASSETS_BLOB_H\n");
	fprintf(f, "#define ASSETS_BLOB_H\n\n");
	fprintf(f, "#include <stddef.h>\n\n");
	fprintf(f, "extern const unsigned char %s[];\n", symbol_name);
	fprintf(f, "extern const size_t %s_len;\n\n", symbol_name);
	fprintf(f, "#endif /* ASSETS_BLOB_H */\n");

	fclose(f);

	return 0;
}

int main(int argc, char **argv)
{
	const char *output_source = NULL;
	const char *output_header = NULL;
	const char *symbol_name = "assets_blob";
	int first_file_arg = -1;
	int num_files = 0;
	bundle_t bundle;
	uint8_t *packed_data = NULL;
	size_t packed_len = 0;
	uint8_t *compressed_data = NULL;
	size_t compressed_len = 0;
	int i;
	int rc = 0;

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--output-source") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: --output-source requires argument\n");
				usage(argv[0]);
				return 1;
			}
			output_source = argv[++i];
		} else if (strcmp(argv[i], "--output-header") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: --output-header requires argument\n");
				usage(argv[0]);
				return 1;
			}
			output_header = argv[++i];
		} else if (strcmp(argv[i], "--symbol-name") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: --symbol-name requires argument\n");
				usage(argv[0]);
				return 1;
			}
			symbol_name = argv[++i];
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
			usage(argv[0]);
			return 1;
		} else {
			/* First file argument */
			if (first_file_arg < 0) {
				first_file_arg = i;
			}
			num_files++;
		}
	}

	/* Validate arguments */
	if (!output_source || !output_header) {
		fprintf(stderr, "Error: --output-source and --output-header are required\n");
		usage(argv[0]);
		return 1;
	}

	if (num_files == 0) {
		fprintf(stderr, "Error: no input files specified\n");
		usage(argv[0]);
		return 1;
	}

	/* Initialize bundle */
	if (bundle_init(&bundle, (uint32_t)num_files) != 0) {
		fprintf(stderr, "Error: failed to initialize bundle\n");
		return 1;
	}

	/* Add all files */
	for (i = first_file_arg; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (bundle_add_file(&bundle, argv[i]) != 0) {
				bundle_free(&bundle);
				return 1;
			}
		}
	}

	/* Pack bundle */
	if (bundle_pack(&bundle, &packed_data, &packed_len) != 0) {
		bundle_free(&bundle);
		return 1;
	}

	printf("Packed %u files into %zu bytes\n", bundle.count, packed_len);

	/* Compress bundle */
	if (compress_bundle(packed_data, packed_len, &compressed_data, &compressed_len) != 0) {
		free(packed_data);
		bundle_free(&bundle);
		return 1;
	}

	printf("Compressed to %zu bytes (%.1f%% of original)\n",
	       compressed_len, (100.0 * compressed_len) / packed_len);

	/* Write C source and header */
	if (write_c_source(output_source, output_header, symbol_name,
	                   compressed_data, compressed_len) != 0) {
		rc = 1;
	} else {
		printf("Generated %s and %s\n", output_source, output_header);
	}

	/* Cleanup */
	free(compressed_data);
	free(packed_data);
	bundle_free(&bundle);

	return rc;
}
