# HBF - Wails Xterm Neovim Project Plan

## Project Vision

HBF is a cross-platform terminal application built with Wails that provides a modern terminal interface with integrated Neovim support. The application combines the power of Go for backend operations with XTerm.js for frontend terminal emulation.

## Architecture Overview

- **Backend (Go)**: Process management, shell integration, file operations, Neovim communication
- **Frontend (JavaScript + XTerm.js)**: Terminal UI, user input handling, display rendering
- **Communication**: Wails bridge for seamless frontend-backend interaction

## Development Phases

### Phase 1: Foundation ✅ COMPLETED

- Basic Wails application structure
- XTerm.js integration and terminal UI
- Frontend-backend communication bridge
- Project scaffolding and build system

### Phase 2: Shell Integration 🚧 CURRENT

- Real shell process spawning and management
- Input/output handling between terminal and shell
- Process lifecycle management (start, stop, cleanup)
- Terminal resize and environment handling

### Phase 3: Advanced Terminal Features

- Multiple terminal tabs/sessions
- Terminal customization (themes, fonts, key bindings)
- Command history and search functionality
- Copy/paste and selection improvements

### Phase 4: Neovim Integration

- Embedded Neovim instance management
- File editing capabilities within terminal
- Neovim configuration and plugin support
- Seamless switching between terminal and editor modes

### Phase 5: File Management

- Integrated file browser/explorer
- File operations (create, delete, rename, move)
- Quick file navigation and search
- Git integration for version control

### Phase 6: Polish & Distribution

- Performance optimization
- Error handling and logging
- User preferences and settings
- Cross-platform packaging and distribution
- Documentation and user guides

## Technical Stack

- **Framework**: Wails v2
- **Backend**: Go 1.24+
- **Frontend**: Vanilla JavaScript, XTerm.js v5.5+
- **Build Tool**: Vite v3+
- **Terminal**: XTerm.js with addons (Fit, WebLinks)
- test only with go test library, no assertions libraries

## Target Platforms

- Linux (primary)
- macOS

## Success Metrics

- Functional terminal with shell integration
- Smooth Neovim editing experience
- Cross-platform compatibility
- Performance comparable to native terminals
- User-friendly interface and experience
