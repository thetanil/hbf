#!/bin/bash
# Convert SQL file to C array
# Usage: sql_to_c.sh input.sql output.c

set -e

INPUT="$1"
OUTPUT="$2"

if [ -z "$INPUT" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 input.sql output.c"
    exit 1
fi

# Use byte array instead of string literal to avoid C99 string length limits
cat > "$OUTPUT" << 'EOF'
/* SPDX-License-Identifier: MIT */
/* Auto-generated from schema.sql - DO NOT EDIT */

const char hbf_schema_sql[] = {
EOF

# Convert each byte to hex format
od -An -tx1 -v "$INPUT" | while read -r line; do
    for byte in $line; do
        echo -n "0x$byte," >> "$OUTPUT"
    done
    echo "" >> "$OUTPUT"
done

# Add null terminator
echo "0x00" >> "$OUTPUT"

cat >> "$OUTPUT" << 'EOF'
};

const char * const hbf_schema_sql_ptr = hbf_schema_sql;
const unsigned long hbf_schema_sql_len = sizeof(hbf_schema_sql) - 1;
EOF

echo "Generated $OUTPUT from $INPUT"
