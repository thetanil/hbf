#!/bin/bash
# Integration test for hbf_simple server (minimal QuickJS HTTP server)
# Starts server on random port, tests endpoints, and cleans up

set -e  # Exit on error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        echo -e "${YELLOW}Shutting down server (PID: $SERVER_PID)...${NC}"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        echo -e "${GREEN}Server stopped${NC}"
    fi
    rm -f "$STDOUT_LOG" "$STDERR_LOG"
}
trap cleanup EXIT INT TERM

find_available_port() {
    local port
    while true; do
        port=$((RANDOM % 10000 + 10000))
        if ! nc -z localhost "$port" 2>/dev/null; then
            echo "$port"
            return 0
        fi
    done
}

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

main() {
    local failed=0
    local passed=0
    echo "=================================="
    echo "hbf_simple Integration Test"
    echo "=================================="
    echo ""
    PORT=$(find_available_port)
    echo -e "${GREEN}Using port: $PORT${NC}"
    echo ""
    STDOUT_LOG=$(mktemp)
    STDERR_LOG=$(mktemp)
    echo -e "${YELLOW}Building hbf_simple...${NC}"
    if ! bazel build //hbf_simple:hbf_simple > /dev/null 2>&1; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
    echo -e "${GREEN}Build successful${NC}"
    echo ""
    echo -e "${YELLOW}Starting hbf_simple on port $PORT...${NC}"
    ./bazel-bin/hbf_simple/hbf_simple --port "$PORT" > "$STDOUT_LOG" 2> "$STDERR_LOG" &
    SERVER_PID=$!
    echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
    echo ""
    if ! wait_for_server "$PORT"; then
        exit 1
    fi
    echo ""
    echo "Running endpoint tests..."
    echo "=================================="
    # QJS Health check
    if test_endpoint "GET" "/qjshealth" "200" "QJS Health check" "grep -q '"status".*"ok"'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
    # Homepage
    if test_endpoint "GET" "/" "200" "Homepage" "grep -q 'Hello'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
    # Echo endpoint
    if test_endpoint "POST" "/echo" "200" "Echo POST" "grep -q 'received'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
    # 404 for non-existent route
    if test_endpoint "GET" "/does-not-exist" "404" "404 for non-existent route" "grep -q 'Not Found'"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
    echo ""
    echo "=================================="
    echo "Test Summary"
    echo "=================================="
    echo -e "${GREEN}Passed: $passed${NC}"
    if [ $failed -gt 0 ]; then
        echo -e "${RED}Failed: $failed${NC}"
        echo ""
        echo -e "${RED}INTEGRATION TEST FAILED${NC}"
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

main
