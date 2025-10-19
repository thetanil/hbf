#!/bin/bash
# Test the HBF HTTP server

set -e

echo "Building server..."
bazel build //:hbf

echo "Starting server on port 8888..."
bazel-bin/internal/core/hbf --port 8888 &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo "Testing /health endpoint..."
HEALTH_RESPONSE=$(curl -s http://localhost:8888/health)
echo "Response: $HEALTH_RESPONSE"

# Check if response contains expected fields
if echo "$HEALTH_RESPONSE" | grep -q '"status":"ok"'; then
    echo "✓ /health endpoint works"
else
    echo "✗ /health endpoint failed"
    kill $SERVER_PID
    exit 1
fi

echo "Testing 404 handler..."
NOT_FOUND_RESPONSE=$(curl -s http://localhost:8888/nonexistent)
echo "Response: $NOT_FOUND_RESPONSE"

if echo "$NOT_FOUND_RESPONSE" | grep -q '"error":"Not Found"'; then
    echo "✓ 404 handler works"
else
    echo "✗ 404 handler failed"
    kill $SERVER_PID
    exit 1
fi

echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo "All tests passed!"
