# HBF - Bazel Build Framework

A system of Bazel builds with external repos and interdependencies, featuring ibazel integration for development workflows.

## Quick Start

1. **Install build tools:**
   ```bash
   ./install-bazel-tools.sh
   source ~/.bashrc  # or restart terminal
   ```

2. **Run existing modules:**
   ```bash
   # Start Traefik server with auto-reload
   ibazel run //traefik:traefik_server
   
   # Build all targets
   bazel build //...
   
   # Test all targets
   bazel test //...
   ```

3. **Access services:**
   - Traefik dashboard: http://localhost:8090/dashboard/
   - Traefik API: http://localhost:8090/api/

## Project Structure

```
/
├── WORKSPACE              # Bazel workspace definition
├── install-bazel-tools.sh # Tool installation script
└── traefik/              # Traefik server module
    ├── BUILD.bazel       # Bazel build configuration
    ├── traefik.yml       # Traefik configuration
    └── start_traefik.sh  # Startup script
```

## Adding New Modules

To add a new buildable module:

1. **Create module directory:**
   ```bash
   mkdir my-service
   ```

2. **Create BUILD.bazel file:**
   ```bazel
   # my-service/BUILD.bazel
   sh_binary(
       name = "my_service",
       srcs = ["start_service.sh"],
       data = [
           "config.yml",
           # Add other data files here
       ],
       visibility = ["//visibility:public"],
   )
   ```

3. **Create startup script:**
   ```bash
   # my-service/start_service.sh
   #!/bin/bash
   set -euo pipefail
   
   SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
   # Your service startup logic here
   ```

4. **Run with ibazel:**
   ```bash
   ibazel run //my-service:my_service
   ```

## Development Workflow

- Use `ibazel run //module:target` for auto-reloading during development
- Use `bazel build //...` to build all targets
- Use `bazel test //...` to run all tests
- Configuration changes trigger automatic restarts with ibazel

## External Dependencies

Add external repositories in `WORKSPACE`:
```bazel
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "my_dependency",
    urls = ["https://example.com/archive.tar.gz"],
    sha256 = "...",
)
``` 