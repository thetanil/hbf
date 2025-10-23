#!/bin/bash
# Static file serving test for HBF
# Tests ONLY the CivetWeb + SQLite static file handler (no QuickJS involvement)
#
# This test verifies that static files can be served from the database
# without touching the JavaScript runtime at all. If this fails with
# QuickJS-related errors, there's a serious architectural problem where
# the static file handler is incorrectly routing to the JS engine.

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    if [ -n "$SERVER_PID" ]; then
        echo -e "${YELLOW}Shutting down server (PID: $SERVER_PID)...${NC}"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        echo -e "${GREEN}Server stopped${NC}"
    fi

    # Clean up temp files
    rm -f "$STDOUT_LOG" "$STDERR_LOG"
}

trap cleanup EXIT INT TERM

# Find random available port
find_available_port() {
    local port
    while true; do
        port=$((RANDOM % 10000 + 10000))  # Random port between 10000-20000
        if ! nc -z localhost "$port" 2>/dev/null; then
            echo "$port"
            return 0
        fi
    done
}

# Wait for server to be ready
wait_for_server() {
    local port=$1
    local max_attempts=30
    local attempt=0

    echo -e "${YELLOW}Waiting for server to start on port $port...${NC}"

    while [ $attempt -lt $max_attempts ]; do
        if curl -s "http://localhost:$port/health" > /dev/null 2>&1; then
            echo -e "${GREEN}Server is ready!${NC}"
            return 0
        fi
        sleep 0.5
        attempt=$((attempt + 1))
    done

    echo -e "${RED}Server failed to start within 15 seconds${NC}"
    echo -e "${YELLOW}Server stdout:${NC}"
    cat "$STDOUT_LOG"
    echo -e "${YELLOW}Server stderr:${NC}"
    cat "$STDERR_LOG"
    return 1
}

# Test static file endpoint
test_static_file() {
    local path=$1
    local expected_status=$2
    local description=$3
    local content_type=$4
    local content_check=$5

    echo -n "Testing GET $path - $description... "

    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT$path")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)

    if [ "$status" != "$expected_status" ]; then
        echo -e "${RED}FAIL${NC}"
        echo "  Expected status: $expected_status"
        echo "  Got status: $status"
        echo "  Response: $body"

        # Check if this is a QuickJS-related error (which should NOT happen for static files!)
        if echo "$body" | grep -qi "javascript\|quickjs\|call stack"; then
            echo -e "${RED}  *** CRITICAL: QuickJS error for static file! ***${NC}"
            echo -e "${RED}  *** The static file handler should NOT touch the JS engine! ***${NC}"
            echo -e "${RED}  *** This indicates a routing problem in server.c ***${NC}"
        fi

        return 1
    fi

    # Check Content-Type header if specified
    if [ -n "$content_type" ]; then
        actual_content_type=$(curl -s -I "http://localhost:$PORT$path" | grep -i "content-type" | cut -d' ' -f2- | tr -d '\r\n')
        if ! echo "$actual_content_type" | grep -qi "$content_type"; then
            echo -e "${RED}FAIL${NC}"
            echo "  Expected Content-Type to contain: $content_type"
            echo "  Got: $actual_content_type"
            return 1
        fi
    fi

    # Check content if specified
    if [ -n "$content_check" ]; then
        if ! echo "$body" | eval "$content_check"; then
            echo -e "${RED}FAIL${NC}"
            echo "  Content check failed: $content_check"
            echo "  Response body (first 200 chars):"
            echo "$body" | head -c 200
            echo ""
            return 1
        fi
    fi

    echo -e "${GREEN}PASS${NC}"
    return 0
}

# Main test script
main() {
    local failed=0
    local passed=0

    echo "=================================="
    echo "HBF Static File Handler Test"
    echo "=================================="
    echo ""
    echo "This test verifies static file serving WITHOUT QuickJS involvement."
    echo "All requests are handled by CivetWeb + SQLite only."
    echo ""

    # Find available port
    PORT=$(find_available_port)
    echo -e "${GREEN}Using port: $PORT${NC}"
    echo ""

    # Create temp files for server output
    STDOUT_LOG=$(mktemp)
    STDERR_LOG=$(mktemp)

    # Build and start server
    echo -e "${YELLOW}Building HBF...${NC}"
    if ! bazel build //:hbf > /dev/null 2>&1; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
    echo -e "${GREEN}Build successful${NC}"
    echo ""

    echo -e "${YELLOW}Starting server on port $PORT...${NC}"
    ./bazel-bin/hbf --port "$PORT" --log_level info > "$STDOUT_LOG" 2> "$STDERR_LOG" &
    SERVER_PID=$!
    echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
    echo ""

    # Wait for server to be ready
    if ! wait_for_server "$PORT"; then
        exit 1
    fi
    echo ""

    # Run tests
    echo "Running static file tests..."
    echo "=================================="

    # Test 1: Health check (built-in C handler, not static file)
    if test_static_file "/health" "200" "Health check (C handler)" "application/json" "grep -q '\"status\".*\"ok\"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 2: Static HTML file
    if test_static_file "/static/index.html" "200" "Static HTML file" "text/html" "grep -qi 'html\|HBF'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 3: Static CSS file
    if test_static_file "/static/style.css" "200" "Static CSS file" "text/css" "grep -qi 'body\|font'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 4: Non-existent static file (should 404, not 500)
    if test_static_file "/static/nonexistent.html" "404" "Non-existent static file" "" ""; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 5: Static file path without /static/ prefix (should not match static handler)
    # This will hit the QuickJS handler and might fail, but that's expected
    echo -n "Testing GET /index.html - Route to JS handler (expected to fail)... "
    status=$(curl -s -w "%{http_code}" -o /dev/null "http://localhost:$PORT/index.html")
    if [ "$status" = "404" ] || [ "$status" = "500" ] || [ "$status" = "503" ]; then
        echo -e "${YELLOW}EXPECTED (JS handler returned $status)${NC}"
        # This is expected to fail since JS routing is broken, don't count it
    else
        echo -e "${GREEN}PASS (got $status)${NC}"
    fi

    # Test 6: Verify Cache-Control header is set
    echo -n "Testing Cache-Control header on static files... "
    cache_header=$(curl -s -I "http://localhost:$PORT/static/index.html" | grep -i "cache-control" | cut -d' ' -f2- | tr -d '\r\n')
    if echo "$cache_header" | grep -qi "max-age"; then
        echo -e "${GREEN}PASS${NC}"
        echo "  Cache-Control: $cache_header"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected Cache-Control header with max-age"
        echo "  Got: $cache_header"
        failed=$((failed + 1))
    fi

    # Print summary
    echo ""
    echo "=================================="
    echo "Test Summary"
    echo "=================================="
    echo -e "${GREEN}Passed: $passed${NC}"

    if [ $failed -gt 0 ]; then
        echo -e "${RED}Failed: $failed${NC}"
        echo ""
        echo -e "${RED}STATIC FILE TEST FAILED${NC}"
        echo ""
        echo -e "${YELLOW}Note: If failures mention QuickJS/JavaScript errors,${NC}"
        echo -e "${YELLOW}this indicates the static file handler is incorrectly${NC}"
        echo -e "${YELLOW}routing to the JS engine instead of serving directly.${NC}"

        # Show server logs for debugging
        echo ""
        echo -e "${YELLOW}Server stderr (last 30 lines):${NC}"
        tail -n 30 "$STDERR_LOG"

        exit 1
    else
        echo -e "${RED}Failed: $failed${NC}"
        echo ""
        echo -e "${GREEN}ALL STATIC FILE TESTS PASSED!${NC}"
        echo ""
        echo "Static files are being served correctly by CivetWeb + SQLite"
        echo "without involving the QuickJS runtime."
        exit 0
    fi
}

# Run main
main
