#!/bin/bash
# SPDX-License-Identifier: MIT
# Test that fs.db archive is created correctly

set -e

ARCHIVE="$1"

if [ ! -f "$ARCHIVE" ]; then
    echo "FAIL: Archive not found: $ARCHIVE"
    exit 1
fi

# Test 1: Verify sqlar table exists
if ! sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar" > /dev/null 2>&1; then
    echo "FAIL: sqlar table not found in archive"
    exit 1
fi

# Test 2: Verify file count > 0
FILE_COUNT=$(sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar")
if [ "$FILE_COUNT" -eq 0 ]; then
    echo "FAIL: Archive is empty"
    exit 1
fi

# Test 3: Verify server.js exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'hbf/server.js'" | grep -q 1; then
    echo "FAIL: hbf/server.js not found in archive"
    exit 1
fi

# Test 4: Verify index.html exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/index.html'" | grep -q 1; then
    echo "FAIL: static/index.html not found in archive"
    exit 1
fi

echo "PASS: Archive contains $FILE_COUNT files"
exit 0
