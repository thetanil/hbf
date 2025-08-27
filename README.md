# HBF - High-Performance Neovim GUI

HBF is a modern GUI frontend for Neovim built with Go, Raylib, and Bazel. It provides a fast, lightweight graphical interface that connects to Neovim via the embedded RPC protocol.

## Architecture

```
┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐
│   Raylib GUI    │◄──►│  go-client   │◄──►│ Neovim --embed │
│   (rendering)   │    │   (RPC)      │    │   (backend)     │
└─────────────────┘    └──────────────┘    └─────────────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌──────────────┐
│  Input Handler  │    │ State Manager│
│ (kbd/mouse)     │    │ (grid/UI)    │
└─────────────────┘    └──────────────┘
```

## Features

- **High Performance**: Built with Raylib for fast rendering
- **Native Neovim Integration**: Uses official go-client for RPC communication
- **Cross-Platform**: Supports Linux, macOS, and Windows
- **Modern Build System**: Uses Bazel for reproducible builds

## Prerequisites

- **Go 1.21+**
- **Bazel 6.0+** (or use the provided install script)
- **Neovim 0.8+** (for --embed support)

### Install Bazel (if needed)

```bash
# Run the provided install script
./scripts/bazel-install.sh
```

## Building and Running

### Quick Start

```bash
# Build and run the application
bazel run //cmd/hbf:hbf
```

### Development Workflow

```bash
# 1. Sync Go dependencies from go.mod to Bazel
bazel run //:gazelle -- update-repos -from_file=go.mod

# 2. Generate/update BUILD files
bazel run //:gazelle

# 3. Build the binary
bazel build //cmd/hbf:hbf

# 4. Run the application
bazel run //cmd/hbf:hbf

# Or combine steps 3-4:
bazel run //cmd/hbf:hbf
```

### Testing

```bash
# Run all tests
bazel test //...

# Run specific module tests
bazel test //internal/log:all
bazel test //internal/nvim:all

# Run integration tests
bazel test //:hbf_test
```

### Development Commands

```bash
# Clean build artifacts
bazel clean

# Build with debugging info
bazel build //cmd/hbf:hbf --compilation_mode=dbg

# Run with verbose logging
bazel run //cmd/hbf:hbf -- --verbose
```

## Project Structure

```
├── cmd/hbf/              # Main application entry point
├── internal/
│   ├── log/              # Logging framework
│   ├── errors/           # Error handling utilities
│   ├── nvim/             # Neovim client wrapper
│   ├── render/           # Raylib renderer
│   ├── ui/               # UI state manager
│   └── input/            # Keyboard/mouse input handler
├── scripts/              # Build and development scripts
├── MODULE.bazel          # Bazel module configuration
├── go.mod               # Go module dependencies
└── integration_test.go  # Integration tests
```

## Development Status

- ✅ **Stage 0**: Project Bootstrap & Bazel Setup (Complete)
- 🚧 **Stage 1**: Foundation + go-client Integration (In Progress)
- 📋 **Stage 2**: Basic Raylib Window + Grid Rendering
- 📋 **Stage 3**: Input Handling & Event Processing
- 📋 **Stage 4**: Complete MVP Implementation

## Contributing

1. Ensure Bazel and Go are installed
2. Clone the repository
3. Run the development workflow commands above
4. Make your changes
5. Test with `bazel test //...`

## License

See [LICENSE](LICENSE) file for details.
