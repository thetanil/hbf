# Wails Development Container

This dev container is configured for Wails application development with full GUI support and enhanced webkit compatibility.

## Features

- **Node.js 20** base image with all necessary development tools
- **Go 1.21.5** with proper environment configuration
- **Enhanced webkit support** - Includes both webkit2gtk-4.0 and webkit2gtk-4.1 libraries
- **Comprehensive GUI libraries** for Linux desktop application development
- **Wails CLI** pre-installed and configured
- **Development tools** including git, vim, nano, fzf, and more

## Quick Start

### Building Your Wails Application

Use the provided build script for optimal compatibility:

```bash
# For development mode
.devcontainer/wails-build.sh dev

# For production build (default)
.devcontainer/wails-build.sh build

# Run diagnostics
.devcontainer/wails-build.sh doctor
```

### Manual Build Commands

If you prefer manual control:

```bash
# Development mode
wails dev -tags webkit2_41

# Production build  
wails build -tags webkit2_41

# Check system configuration
wails doctor
```

## Webkit Compatibility

This container includes enhanced webkit support:

- **Primary**: webkit2gtk-4.1 (latest, recommended)
- **Build tags**: Use `-tags webkit2_41` for optimal compatibility
- **Fallback support**: Additional libraries included for broader compatibility

## Installed Packages

### Core Development
- build-essential, pkg-config, git
- Go 1.21.5, Node.js 20, npm
- Wails CLI (latest)

### GUI Libraries
- libgtk-3-dev, libgtk2.0-dev
- libwebkit2gtk-4.1-dev
- libnss3-dev, libatk-bridge2.0-dev
- Various X11 and graphics libraries
- dbus-x11, xvfb for headless testing

### Additional Tools
- vim, nano, less, fzf
- gh (GitHub CLI)
- git-delta for enhanced git diffs
- zsh with oh-my-zsh configuration

## Environment Variables

The container sets up proper environment variables for development:

```bash
GOPATH=/home/node/go
GOBIN=$GOPATH/bin
PATH includes Go and npm global bins
DISPLAY=:1 (for GUI applications)
```

## Troubleshooting

### Build Issues
1. Always use the webkit2_41 build tag
2. Check `wails doctor` output for missing dependencies
3. Use the provided build script for automated environment setup

### GUI Applications
- GUI applications require X11 forwarding or VNC
- DISPLAY environment variable is pre-configured
- xvfb available for headless testing

### Common Commands

```bash
# Check Wails configuration
wails doctor

# Initialize new Wails project
wails init -n myapp -t vanilla

# Test terminal functionality (from our implementation)
go run test_terminal.go terminal.go
```

## Development Workflow

1. Start the dev container
2. Navigate to your Wails project directory
3. Use `.devcontainer/wails-build.sh dev` for live development
4. Build production version with `.devcontainer/wails-build.sh build`
5. Test the built application in `/build/bin/`

## Support

This configuration supports:
- ✅ Linux desktop applications
- ✅ Cross-platform Wails development
- ✅ Terminal/shell integration (as implemented in Phase 2)
- ✅ Modern webkit rendering
- ✅ Node.js frontend tooling

For issues, check the Wails documentation or the build script output for specific error details.