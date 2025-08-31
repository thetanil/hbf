# SDL2 Bazel Usage Guide

## Overview

This repository provides SDL2 v2.32.8 built with Bazel using `rules_foreign_cc`. The library includes support for:

- Audio
- Video rendering
- OpenGL 3.3+
- OpenGL ES
- Vulkan
- Metal
- Events, joystick, haptic feedback
- File system operations
- Threading and timers

## Using in Golang Projects

### Step 1: Add as a dependency

In your Golang Bazel project's `MODULE.bazel`, add this repository as a dependency:

```starlark
git_override(
    module_name = "sdl2_bazel",
    remote = "https://github.com/your-username/your-repo.git",
    commit = "your-commit-hash",
)
```

Or if published as a Bazel module:

```starlark
bazel_dep(name = "sdl2_bazel", version = "1.0.0")
```

### Step 2: Use in your BUILD files

For Go programs that need to link with SDL2:

```starlark
load("@rules_go//go:def.bzl", "go_binary")

go_binary(
    name = "my_game",
    srcs = ["main.go"],
    cgo = True,
    cdeps = ["@sdl2_bazel//:sdl2"],
)
```

### Step 3: CGO Integration

In your Go code, use CGO to interface with SDL2:

```go
package main

/*
#include <SDL2/SDL.h>
*/
import "C"
import (
    "fmt"
    "unsafe"
)

func main() {
    if C.SDL_Init(C.SDL_INIT_VIDEO) < 0 {
        panic(fmt.Sprintf("SDL_Init failed: %s", C.GoString(C.SDL_GetError())))
    }
    defer C.SDL_Quit()
    
    // Your SDL2 code here
}
```

## Available Targets

- `//:sdl2` - Main SDL2 library (alias)
- `//:SDL2` - Alternative alias for convenience
- `@sdl2//:sdl2` - Direct access to the cmake-built library

## Build Configuration

The SDL2 library is built with the following CMake options:

```
SDL_STATIC=ON           # Static library build
SDL_SHARED=OFF          # No shared library
SDL_AUDIO=ON            # Audio support
SDL_VIDEO=ON            # Video support
SDL_RENDER=ON           # Rendering support
SDL_OPENGL=ON           # OpenGL support
SDL_OPENGLES=ON         # OpenGL ES support
SDL_VULKAN=ON           # Vulkan support
SDL_METAL=ON            # Metal support (macOS/iOS)
```

## Troubleshooting

### Missing system dependencies

On Linux, you may need to install system dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install libgl1-mesa-dev libasound2-dev

# Fedora/RHEL
sudo dnf install mesa-libGL-devel alsa-lib-devel
```

### CGO compilation issues

Ensure your Go binary has `cgo = True` in the BUILD file and includes the SDL2 cdeps.

## Bazel Commands

### Build the SDL2 library
```bash
bazel build //:sdl2
# or
bazel build @sdl2//:sdl2
```

### Test the SDL2 build
```bash
# Build and run the simple C test
bazel run //test:test_sdl2_simple

# Build a C program that uses SDL2
bazel build //test:test_sdl2
```

### Build all targets
```bash
bazel build //...
```

### Clean build cache
```bash
bazel clean
```

## Example Project Structure

```
my_game/
├── MODULE.bazel          # Includes sdl2_bazel dependency
├── BUILD.bazel           # Go binary with SDL2 cdeps
├── main.go              # Go source with CGO SDL2 calls
└── ...
```

## Test Results

The included test programs verify that SDL2 v2.32.8 builds and links correctly:

```
$ bazel run //test:test_sdl2_simple
SDL2 compiled version: 2.32.8
SDL2 linked version: 2.32.8
SDL2 library test completed successfully!
```