# HBF - Bazel Build Framework

## rework

make a repo for repos with otterdog
make a repo for secrets with pass
just start building a local pipeline
static web site
publish to mastadon
render a https://pypi.org/project/mitsuba/ image
lint python
generate 2d isometric art for game with 3d rotoscoping from 
pytests
tests in jupyter
publish to docker cluster




each tiny service has it's own docker compose which works together with the
others

tiny builds into tiny containers live deployed with ibazel

bring in common-ui

bazel build //my_service:my_service_binary
docker build -t my_service:latest .
docker save my_service:latest | docker compose -f docker-compose.yml load
docker compose up -d --no-deps my_service

and use

################################################################################
# Did you know iBazel can invoke programs like Gazelle, buildozer, and         #
# other BUILD file generators for you automatically based on bazel output?     #
# Documentation at: https://github.com/bazelbuild/bazel-watcher#output-runner  #
################################################################################

to run the docker commands which are output from the bazel service bin build

## old

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