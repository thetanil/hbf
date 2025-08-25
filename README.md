# HBF - Bazel Build Framework

## something to build....

an integrated platform for audio and graphics which is at a top level cpp
statically compiled cross platform single file executable which has no external
dependencies (even glibc). the application will have a gui made of several layers.

primary language is incorrect
    - the prefered language is C99 or C89
    - C++ required for imgui, so a top level cpp build will be needed
    - bazel, init.db and nvim delivered as resources, extracted and called externally
    - sdl2 with opengl, audio, and ttf statically linked in
    - sqlite3 statically linked in
audio sequencing and synthesis should use sdl2 audio
    - stk libs might be made available
i will build this all with bazel 
    - scripted builds of cmake calls as bazel packages is fine
    - do not change the build system of the dependant projects
    - they should be mostly bazel library packages which cache well

write a markdown specification. this is a technical document for the developer
to understand the design. do not use code fences. include hierarchy of tasks to
plan to implement, and order them to minize dependencies and make smaller steps


- opengl surface for shaders
- an imgui layer for UI elements
- a neovim layer for live editing of elements in the application

sdl2 opengl
nvim overlay with transparency using sdl_ttf for fonts
sqlite3 for doc storage, message queuing, cache
- starlark
    - Deterministic evaluation. Executing the same code twice will give the same results.
    - Hermetic execution. Execution cannot access the file system, network, system clock. It is safe to execute untrusted code.
    - Parallel evaluation. Modules can be loaded in parallel. To guarantee a thread-safe execution, shared data becomes immutable.
- maybe embed lua
- maybe embed python with module support (no starlark maybe, provide own api)

for matt:
editor with live reload shaders
gif

for docs:
clickable md which has testable md cells
extract to code file

for game use case
imgui text, just do it, with buttons
md renders html not as ui


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