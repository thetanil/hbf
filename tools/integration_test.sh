#!/bin/bash
# Integration test for HBF server
# Starts server on random port, tests endpoints, and cleans up
#
# KNOWN ISSUE: Currently fails due to JavaScript context initialization bug
# where router.js's 'app' global is not accessible from C handler.
# See internal/http/handler_test.c for details.

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

    # Clean up temp files and test database
    rm -f "$STDOUT_LOG" "$STDERR_LOG"
    rm -f ./hbf.db ./hbf.db-shm ./hbf.db-wal
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

# Test endpoint
test_endpoint() {
    local method=$1
    local path=$2
    local expected_status=$3
    local description=$4
    local additional_checks=$5

    echo -n "Testing $method $path - $description... "

    response=$(curl -s -w "\n%{http_code}" -X "$method" "http://localhost:$PORT$path")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)

    if [ "$status" != "$expected_status" ]; then
        echo -e "${RED}FAIL${NC}"
        echo "  Expected status: $expected_status"
        echo "  Got status: $status"
        echo "  Response: $body"
        return 1
    fi

    # Additional checks (if provided)
    if [ -n "$additional_checks" ]; then
        if ! echo "$body" | eval "$additional_checks"; then
            echo -e "${RED}FAIL${NC}"
            echo "  Additional check failed: $additional_checks"
            echo "  Response: $body"
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
    echo "HBF Integration Test"
    echo "=================================="
    echo ""

    # Delete test database to ensure fresh state
    echo -e "${YELLOW}Cleaning up test database...${NC}"
    rm -f ./hbf.db ./hbf.db-shm ./hbf.db-wal
    echo -e "${GREEN}Test database cleaned${NC}"
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
    ./bazel-bin/bin/hbf --port "$PORT" --log-level info > "$STDOUT_LOG" 2> "$STDERR_LOG" &
    SERVER_PID=$!
    echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
    echo ""

    # Wait for server to be ready
    if ! wait_for_server "$PORT"; then
        exit 1
    fi
    echo ""

    # Run tests
    echo "Running endpoint tests..."
    echo "=================================="

    # Test 1: Health check
    if test_endpoint "GET" "/health" "200" "Health check" "grep -q '\"status\".*\"ok\"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 2: Homepage (HTML)
    if test_endpoint "GET" "/" "200" "Homepage HTML" "grep -q 'Available Routes'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 3: Hello endpoint
    if test_endpoint "GET" "/hello" "200" "Hello JSON" "grep -q 'Hello.*World'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 4: User endpoint with parameter
    if test_endpoint "GET" "/user/42" "200" "User with ID parameter" "grep -q '\"userId\".*\"42\"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 5: User endpoint with string parameter
    if test_endpoint "GET" "/user/alice" "200" "User with string parameter" "grep -q '\"userId\".*\"alice\"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 6: Echo endpoint
    if test_endpoint "GET" "/echo" "200" "Echo request info" "grep -q '\"method\".*\"GET\"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 7: 404 for non-existent route
    if test_endpoint "GET" "/does-not-exist" "404" "404 for non-existent route" "grep -q 'Not Found'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    # Test 8: Custom header from middleware
    echo -n "Testing middleware custom header... "
    if curl -s -I "http://localhost:$PORT/hello" | grep -q "X-Powered-By: HBF"; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC}"
        failed=$((failed + 1))
    fi

    # Test 9: Dev API blocked without --dev flag
    echo -n "Testing dev API blocked without --dev flag... "
    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT/__dev/api/files")
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "403" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (expected 403, got $status)"
        failed=$((failed + 1))
    fi

    # Restart server with --dev flag for dev API tests
    echo ""
    echo -e "${YELLOW}Restarting server with --dev flag...${NC}"
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    rm -f ./hbf.db ./hbf.db-shm ./hbf.db-wal
    ./bazel-bin/bin/hbf --port "$PORT" --dev --log-level info > "$STDOUT_LOG" 2> "$STDERR_LOG" &
    SERVER_PID=$!
    if ! wait_for_server "$PORT"; then
        exit 1
    fi
    echo ""

    # Test 10: Dev API list files
    echo -n "Testing GET /__dev/api/files (list)... "
    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT/__dev/api/files")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ] && echo "$body" | grep -q '\[.*\]'; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status)"
        failed=$((failed + 1))
    fi

    # Test 11: Dev API write file (PUT)
    echo -n "Testing PUT /__dev/api/file (create)... "
    test_content="<h1>Test Content Modified</h1>"
    response=$(curl -s -w "\n%{http_code}" -X PUT \
        "http://localhost:$PORT/__dev/api/file?name=static/test.html" \
        -d "$test_content")
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status)"
        failed=$((failed + 1))
    fi

    # Test 12: Dev API read file back (GET)
    echo -n "Testing GET /__dev/api/file (read)... "
    response=$(curl -s -w "\n%{http_code}" \
        "http://localhost:$PORT/__dev/api/file?name=static/test.html")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ] && [ "$body" = "$test_content" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status, body: $body)"
        failed=$((failed + 1))
    fi

    # Test 13: Modified file served via static handler
    echo -n "Testing modified file served via /static/... "
    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT/static/test.html")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ] && [ "$body" = "$test_content" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status)"
        failed=$((failed + 1))
    fi

    # Test 14: X-HBF-Dev header present in dev mode
    echo -n "Testing X-HBF-Dev header in dev mode... "
    if curl -s -I "http://localhost:$PORT/static/test.html" | grep -q "X-HBF-Dev: 1"; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC}"
        failed=$((failed + 1))
    fi

    # Test 15: Dev API modify server.js
    echo -n "Testing PUT /__dev/api/file (modify server.js)... "
    modified_js='app = {}; app.handle = function(req, res) { res.set("Content-Type", "text/plain"); res.send("Modified!"); };'
    response=$(curl -s -w "\n%{http_code}" -X PUT \
        "http://localhost:$PORT/__dev/api/file?name=hbf/server.js" \
        -d "$modified_js")
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status)"
        failed=$((failed + 1))
    fi

    # Test 16: Verify overlay persists (create new request context)
    echo -n "Testing overlay persistence (new context)... "
    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT/test-route")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ] && [ "$body" = "Modified!" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status, body: $body)"
        failed=$((failed + 1))
    fi

    # Test 17: Dev API delete file (DELETE)
    echo -n "Testing DELETE /__dev/api/file (tombstone)... "
    response=$(curl -s -w "\n%{http_code}" -X DELETE \
        "http://localhost:$PORT/__dev/api/file?name=static/test.html")
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "200" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (status: $status)"
        failed=$((failed + 1))
    fi

    # Note: Tests 18-19 affected by modified server.js from test 15-16
    # The modified server.js returns "Modified!" for all routes
    # This demonstrates overlay persistence works correctly

    # Test 18: Deleted file not accessible via dev API
    echo -n "Testing deleted file not in dev API... "
    response=$(curl -s -w "\n%{http_code}" "http://localhost:$PORT/__dev/api/file?name=static/test.html")
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "404" ]; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (expected 404 from dev API, got $status)"
        failed=$((failed + 1))
    fi

    # Test 19: Invalid file path validation
    echo -n "Testing invalid file path rejected... "
    response=$(curl -s -w "\n%{http_code}" -X PUT \
        "http://localhost:$PORT/__dev/api/file?name=../../etc/passwd" \
        -d "hack")
    body=$(echo "$response" | head -n -1)
    status=$(echo "$response" | tail -n 1)
    if [ "$status" = "400" ] || echo "$body" | grep -q "Invalid file name"; then
        echo -e "${GREEN}PASS${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (expected 400, got $status, body: $body)"
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
        echo -e "${RED}INTEGRATION TEST FAILED${NC}"

        # Show server logs for debugging
        echo ""
        echo -e "${YELLOW}Server stderr (last 20 lines):${NC}"
        tail -n 20 "$STDERR_LOG"

        exit 1
    else
        echo -e "${RED}Failed: $failed${NC}"
        echo ""
        echo -e "${GREEN}ALL TESTS PASSED!${NC}"
        exit 0
    fi
}

# Run main
main
