#!/bin/bash
# SPDX-License-Identifier: MIT
# Lint all C source files with clang-tidy

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Change to workspace root
cd "$BUILD_WORKSPACE_DIRECTORY" 2>/dev/null || cd "$(dirname "$0")/.." || exit 1

echo "Running clang-tidy on all C source files..."

# Find all C source files (excluding third_party and tests)
C_FILES=$(find internal -name "*.c" -not -name "*_test.c" 2>/dev/null || true)

if [ -z "$C_FILES" ]; then
    echo "No C source files found to lint."
    exit 0
fi

# Track errors
ERRORS=0

for file in $C_FILES; do
    echo "Checking $file..."
    if clang-tidy "$file" -- -std=c99 -I. 2>&1 | grep -q "error:"; then
        echo -e "${RED}✗ $file has errors${NC}"
        ERRORS=$((ERRORS + 1))
    else
        echo -e "${GREEN}✓ $file${NC}"
    fi
done

if [ $ERRORS -eq 0 ]; then
    echo -e "\n${GREEN}All files passed lint checks!${NC}"
    exit 0
else
    echo -e "\n${RED}$ERRORS file(s) failed lint checks${NC}"
    exit 1
fi
