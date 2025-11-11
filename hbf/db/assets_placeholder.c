/* SPDX-License-Identifier: MIT */
/*
 * Placeholder asset bundle for HBF
 *
 * This provides empty asset bundle symbols that allow the build to compile.
 * In production, these symbols will be provided by the asset_packer tool.
 */

#include <stddef.h>

/* Empty compressed bundle (just the num_entries field set to 0) */
const unsigned char assets_blob[] = {
	0x78, 0x9c,  /* zlib header */
	0x63, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01,  /* compressed: 4 bytes of zeros */
	0x00, 0x00  /* adler32 checksum */
};

const size_t assets_blob_len = sizeof(assets_blob);
