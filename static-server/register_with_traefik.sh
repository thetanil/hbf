#!/bin/bash

set -euo pipefail

# Wait for Traefik to be ready
echo "Waiting for Traefik to be ready..."
for i in {1..30}; do
    if curl -s http://localhost:8070/ping > /dev/null 2>&1; then
        echo "Traefik is ready!"
        break
    fi
    echo "Waiting for Traefik... ($i/30)"
    sleep 2
done

# Update the dynamic configuration to register this service
echo "Registering static server with Traefik..."
cat > /tmp/traefik-dynamic.yml << EOF
http:
  routers:
    api:
      rule: "PathPrefix(\\\`/api\\\`) || PathPrefix(\\\`/dashboard\\\`)"
      service: "api@internal"
    
    static-server:
      rule: "PathPrefix(\\\`/static\\\`)"
      service: "static-server"
      middlewares:
        - "static-strip-prefix"

  middlewares:
    static-strip-prefix:
      stripPrefix:
        prefixes:
          - "/static"

  services:
    static-server:
      loadBalancer:
        servers:
          - url: "http://localhost:3000"
EOF

echo "Static server registered with Traefik at /static/"