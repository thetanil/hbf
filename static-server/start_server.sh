#!/bin/bash

set -euo pipefail

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Function to cleanup on exit
cleanup() {
    echo "Cleaning up static server..."
    kill $SERVER_PID 2>/dev/null || true
    exit 0
}

trap cleanup SIGTERM SIGINT EXIT

echo "Starting static HTTP server on port 3000..."
cd "$SCRIPT_DIR"

# Start the HTTP server in background
python3 -m http.server 3000 &
SERVER_PID=$!

# Wait a moment for server to start
sleep 1

# Register with Traefik (if it's running)
if ./register_with_traefik.sh; then
    echo "Successfully registered with Traefik at http://localhost:8070/static/"
else
    echo "Could not register with Traefik (is it running?)"
    echo "Direct access available at: http://localhost:3000"
fi

echo "Static server is running. Press Ctrl+C to stop."

# Keep running
wait $SERVER_PID