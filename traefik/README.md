# Traefik ibazel Module

This module provides a Traefik server that can be started with ibazel for automatic reloading.

## Usage

1. First install bazelisk and ibazel:
   ```bash
   ./install-bazel-tools.sh
   ```

2. Start the Traefik server with ibazel:
   ```bash
   ibazel run //traefik:traefik_server
   ```

3. Access the Traefik dashboard at: http://localhost:8080/dashboard/

The server will automatically reload when configuration files change.