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

# Test 5: Verify style.css exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/style.css'" | grep -q 1; then
    echo "FAIL: static/style.css not found in archive"
    exit 1
fi

# Test 6: Verify favicon.ico exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/favicon.ico'" | grep -q 1; then
    echo "FAIL: static/favicon.ico not found in archive"
    exit 1
fi

# Test 7: Verify HTMX exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/vendor/htmx.min.js'" | grep -q 1; then
    echo "FAIL: static/vendor/htmx.min.js not found in archive"
    exit 1
fi

# Test 8: Verify Monaco loader exists
if ! sqlite3 "$ARCHIVE" "SELECT 1 FROM sqlar WHERE name = 'static/monaco/vs/loader.js'" | grep -q 1; then
    echo "FAIL: static/monaco/vs/loader.js not found in archive"
    exit 1
fi

# Test 9: Verify Monaco has multiple files (should be 50+)
MONACO_COUNT=$(sqlite3 "$ARCHIVE" "SELECT COUNT(*) FROM sqlar WHERE name LIKE 'static/monaco/vs/%'")
if [ "$MONACO_COUNT" -lt 50 ]; then
    echo "FAIL: Monaco incomplete ($MONACO_COUNT files, expected 50+)"
    exit 1
fi

echo "PASS: Archive contains $FILE_COUNT files (Monaco files: $MONACO_COUNT)"
exit 0
