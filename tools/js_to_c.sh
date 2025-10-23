#!/bin/bash
# Convert JS file to C array (no xxd required)
# Usage: js_to_c.sh input.js output.c output.h

set -e

INPUT="$1"
OUTPUT_C="$2"
OUTPUT_H="$3"

if [ -z "$INPUT" ] || [ -z "$OUTPUT_C" ] || [ -z "$OUTPUT_H" ]; then
    echo "Usage: $0 input.js output.c output.h"
    exit 1
fi

cat > "$OUTPUT_C" << EOF
/* Auto-generated from $INPUT - DO NOT EDIT */
#include "hbf_simple/server_js.h"
#include <stddef.h>
const char server_js_source[] = {
EOF

od -An -tx1 -v "$INPUT" | while read -r line; do
    for byte in $line; do
        echo -n "0x$byte," >> "$OUTPUT_C"
    done
    echo "" >> "$OUTPUT_C"
done

echo "0x00 };" >> "$OUTPUT_C"
echo "const size_t server_js_source_len = sizeof(server_js_source) - 1;" >> "$OUTPUT_C"

cat > "$OUTPUT_H" << EOF
/* Auto-generated from $INPUT - DO NOT EDIT */
#ifndef HBF_SIMPLE_SERVER_JS_H
#define HBF_SIMPLE_SERVER_JS_H
#include <stddef.h>
extern const char server_js_source[];
extern const size_t server_js_source_len;
#endif
EOF
