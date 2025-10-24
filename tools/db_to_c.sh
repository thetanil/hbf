#!/bin/bash
# SPDX-License-Identifier: MIT
# db_to_c.sh - Convert SQLite database to C byte array
# Usage: db_to_c.sh <input.db> <output.c> <output.h>

set -e

INPUT="$1"
OUTPUT_C="$2"
OUTPUT_H="$3"

if [ -z "$INPUT" ] || [ -z "$OUTPUT_C" ] || [ -z "$OUTPUT_H" ]; then
    echo "Usage: $0 <input.db> <output.c> <output.h>" >&2
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: Input file '$INPUT' not found" >&2
    exit 1
fi

# Generate C source file
cat > "$OUTPUT_C" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from fs.db - DO NOT EDIT */

#include "fs_embedded.h"

const unsigned char fs_db_data[] = {
EOF

# Convert database to hex bytes (16 bytes per line for readability)
hexdump -v -e '16/1 "0x%02x, " "\n"' "$INPUT" >> "$OUTPUT_C"

cat >> "$OUTPUT_C" << 'EOF'
};

const unsigned long fs_db_len = sizeof(fs_db_data);
EOF

# Generate header file
cat > "$OUTPUT_H" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from fs.db - DO NOT EDIT */

#ifndef HBF_FS_EMBEDDED_H
#define HBF_FS_EMBEDDED_H

extern const unsigned char fs_db_data[];
extern const unsigned long fs_db_len;

#endif /* HBF_FS_EMBEDDED_H */
EOF

DB_SIZE=$(stat -c%s "$INPUT" 2>/dev/null || stat -f%z "$INPUT")
echo "Generated $OUTPUT_C and $OUTPUT_H from $INPUT ($DB_SIZE bytes)" >&2
