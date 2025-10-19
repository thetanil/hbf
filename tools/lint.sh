#!/bin/bash
# SPDX-License-Identifier: MIT
# Lint all C source files with clang-tidy

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Change to workspace root
cd "$BUILD_WORKSPACE_DIRECTORY" 2>/dev/null || cd "$(dirname "$0")/.." || exit 1

# Use system clang-tidy-18 (matches LLVM toolchain version)
CLANG_TIDY=$(which clang-tidy-18 2>/dev/null)

if [ -z "$CLANG_TIDY" ]; then
    echo -e "${RED}Error: clang-tidy-18 not found${NC}"
    echo -e "${YELLOW}Install with: sudo apt-get install clang-tidy-18${NC}"
    exit 1
fi

echo "Running clang-tidy on all C source files..."
echo "Using: $CLANG_TIDY"

# Find all C source files (excluding third_party and tests)
C_FILES=$(find internal -name "*.c" -not -name "*_test.c" 2>/dev/null || true)

if [ -z "$C_FILES" ]; then
    echo "No C source files found to lint."
    exit 0
fi

# Track errors and warnings
ERRORS=0
WARNINGS=0

for file in $C_FILES; do
    echo "Checking $file..."
    OUTPUT=$($CLANG_TIDY "$file" -- -std=c99 -I. -D_POSIX_C_SOURCE=200809L 2>&1 || true)

    if echo "$OUTPUT" | grep -q "error:"; then
        echo -e "${RED}✗ $file has errors${NC}"
        echo "$OUTPUT" | grep "error:"
        ERRORS=$((ERRORS + 1))
    elif echo "$OUTPUT" | grep -q "warning:"; then
        echo -e "${YELLOW}⚠ $file has warnings${NC}"
        echo "$OUTPUT" | grep "warning:" | head -5
        WARNINGS=$((WARNINGS + 1))
    else
        echo -e "${GREEN}✓ $file${NC}"
    fi
done

echo ""
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}All files passed lint checks!${NC}"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}$WARNINGS file(s) have warnings (not treated as errors)${NC}"
    exit 0
else
    echo -e "${RED}$ERRORS file(s) failed lint checks${NC}"
    [ $WARNINGS -gt 0 ] && echo -e "${YELLOW}$WARNINGS file(s) have warnings${NC}"
    exit 1
fi
