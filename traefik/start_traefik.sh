#!/bin/bash

set -euo pipefail

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Path to the traefik binary
TRAEFIK_BINARY="$SCRIPT_DIR/traefik"

# Path to the traefik config
TRAEFIK_CONFIG="$SCRIPT_DIR/traefik.yml"

# Download traefik if not present
if [[ ! -f "$TRAEFIK_BINARY" ]]; then
    echo "Downloading Traefik..."
    TRAEFIK_VERSION="v3.1.0"
    TRAEFIK_URL="https://github.com/traefik/traefik/releases/download/${TRAEFIK_VERSION}/traefik_${TRAEFIK_VERSION}_linux_amd64.tar.gz"
    
    curl -fsSL "$TRAEFIK_URL" | tar -xz -C "$SCRIPT_DIR" traefik
    echo "Traefik downloaded successfully"
fi

# Dynamic config will be managed by services that register with Traefik

echo "Starting Traefik server..."
echo "Dashboard will be available at: http://localhost:9080/dashboard/"
echo "API will be available at: http://localhost:9080/api/"

# Make traefik binary executable
chmod +x "$TRAEFIK_BINARY"

# Start traefik using only config file
exec "$TRAEFIK_BINARY" --configfile="$TRAEFIK_CONFIG"