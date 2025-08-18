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

# Create a basic dynamic config if it doesn't exist
DYNAMIC_CONFIG="/tmp/traefik-dynamic.yml"
if [[ ! -f "$DYNAMIC_CONFIG" ]]; then
    cat > "$DYNAMIC_CONFIG" << EOF
http:
  routers:
    api:
      rule: "PathPrefix(\`/api\`) || PathPrefix(\`/dashboard\`)"
      service: "api@internal"
  
  services: {}
EOF
fi

echo "Starting Traefik server..."
echo "Dashboard will be available at: http://localhost:8090/dashboard/"
echo "API will be available at: http://localhost:8090/api/"

# Make traefik binary executable
chmod +x "$TRAEFIK_BINARY"

# Start traefik with explicit entrypoints only
exec "$TRAEFIK_BINARY" --configfile="$TRAEFIK_CONFIG" --entrypoints.web.address=:8090 --entrypoints.websecure.address=:8443