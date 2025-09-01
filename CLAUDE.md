# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a SDL2 Bazel library project that provides SDL2 v2.32.8 built using `rules_foreign_cc`. The project includes:

1. **SDL2 Library** - C library for graphics, audio, and input handling
2. **Embedded Neovim Wrapper** - Go program that bundles and launches Neovim as a child process

The project is designed to be consumed by Golang Bazel projects and demonstrates binary embedding with Bazel.

**Key Architecture:**

- Uses Bazel with bzlmod (MODULE.bazel) - do not create WORKSPACE files
- SDL2 is downloaded from GitHub and built using CMake via `rules_foreign_cc`
- Built as a static library with comprehensive feature support
- Provides C library that can be consumed by Go programs via CGO

## Build System Commands

### Core Build Commands

```bash
# Build the main SDL2 library
bazel build //:sdl2

# Build all targets
bazel build //...

# Clean build cache
bazel clean
```

### Testing

```bash
# Run the simple SDL2 test (verifies library builds and links)
bazel run //test:test_sdl2_simple

# Build the full SDL2 test
bazel build //test:test_sdl2

# Test embedded Neovim wrapper
bazel run //cmd/nvim-wrapper:nvim-wrapper -- --version

# Run comprehensive test script
./test_nvim.sh
```

## Project Structure

- `MODULE.bazel` - Bazel module configuration with dependencies (includes rules_go and gazelle)
- `BUILD.bazel` - Root build file exposing SDL2 targets and Gazelle configuration
- `third_party/sdl2.BUILD` - SDL2 build configuration using rules_foreign_cc
- `test/` - Contains C test programs to verify SDL2 functionality
- `cmd/nvim-wrapper/` - Go program that embeds and launches Neovim binary
- `assets/` - Contains downloaded Neovim binary distribution
- `examples/` - Example usage patterns (currently placeholder directories)

## Key Components

### SDL2 Library (`//:sdl2`)

The main target that provides SDL2 functionality. Built with the following features enabled:

- Audio, Video, Rendering support
- OpenGL 3.3+, OpenGL ES, Vulkan, Metal backends
- Events, joystick, haptic feedback
- File system operations, threading, timers

### Third-party Build (`third_party/sdl2.BUILD`)

Defines how SDL2 is built using CMake via `rules_foreign_cc`. The build configuration at `third_party/sdl2.BUILD:10-36` specifies all enabled SDL2 features.

### Neovim Wrapper (`//cmd/nvim-wrapper:nvim-wrapper`)

Go binary that embeds the Neovim executable and launches it as a child process. Uses `//go:embed` to include the binary at compile time, extracts it to a temporary directory, and executes it with all command-line arguments forwarded.

### Target Aliases

- `//:sdl2` - Main SDL2 library
- `//:SDL2` - Convenience alias
- `@sdl2//:sdl2` - Direct access to cmake-built library
- `//cmd/nvim-wrapper:nvim-wrapper` - Embedded Neovim wrapper binary

## Usage for Go Projects

When integrating this library into a Go Bazel project:

1. Add dependency in `MODULE.bazel`:

```starlark
git_override(
    module_name = "sdl2_bazel",
    remote = "https://github.com/your-repo.git",
    commit = "commit-hash",
)
```

2. Use in Go binary BUILD files:

```starlark
go_binary(
    name = "my_game",
    srcs = ["main.go"],
    cgo = True,
    cdeps = ["@sdl2_bazel//:sdl2"],
)
```

## Using the Embedded Neovim Wrapper

The `nvim-wrapper` demonstrates how to embed large binaries in Go programs with Bazel:

```bash
# Launch embedded Neovim
bazel run //cmd/nvim-wrapper:nvim-wrapper

# Pass arguments to Neovim
bazel run //cmd/nvim-wrapper:nvim-wrapper -- --help
bazel run //cmd/nvim-wrapper:nvim-wrapper -- file.txt

# The embedded binary includes the full Neovim distribution
# Size: ~11MB embedded in the Go binary
```

### Key Implementation Details:

- Uses `//go:embed` directive to embed the binary at compile time
- Bazel `genrule` copies the Neovim binary as an embedsrc
- Runtime extraction to temporary directory with proper permissions
- Uses `syscall.Exec()` to replace the Go process with Neovim (clean process handoff)

## Development Environment

- The project runs in a devcontainer with Bazel 8.3.1 pre-installed
- Do not install bazelisk - use the installed bazel binary directly
- The devcontainer is Node.js based but includes Bazel for building C/C++ and Go code
- Use `bazel run //:gazelle` to update Go BUILD files when adding dependencies

## Go Neovim Wrapper

The `go/neovim` project creates an embedded Neovim wrapper specifically for Linux:

### Key Features:
- **Linux-only support** - Uses `+build linux` constraint, no Windows compatibility
- **No purego** - Uses `pure = "off"` in Bazel for full system integration
- **Latest Neovim 0.11.3** - Embeds the latest stable release
- **x86_64 architecture** - Targets Linux amd64 systems exclusively

### Build Commands:
```bash
# Build the Go Neovim wrapper
bazel build //go/neovim:neovim

# Test the wrapper
bazel run //go/neovim:neovim -- --version
./test_go_neovim.sh

# Use interactively
bazel run //go/neovim:neovim -- [files...]
```

### Project Structure:
- `go/neovim/main.go` - Go source with Linux build constraint
- `go/neovim/BUILD.bazel` - Bazel configuration with Linux/amd64 targeting
- `third_party/neovim.BUILD` - Bazel build file for downloaded Neovim
- `MODULE.bazel` - Defines Neovim 0.11.3 as http_archive (auto-download)

### Automatic Downloads:
- Neovim binary is downloaded automatically by Bazel during build
- No large binaries committed to git (assets/ directory is .gitignored)
- Uses verified SHA256 hash for reproducible builds
- memorize