# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HBF (Hyperlinked Browser Framework) is a Wails-based desktop application that embeds xterm.js as a terminal emulator and connects it to shell processes running in pseudo-terminals (PTY). The goal is to provide a native desktop terminal experience with full color support and cursor control.

## Architecture

### Backend (Go)
- **Main Application**: `hbf/main.go` - Wails application entry point with window configuration
- **Application Logic**: `hbf/app.go` - Core application struct with terminal management methods
- **Terminal Management**: `hbf/terminal.go` - TerminalManager with WebSocket server for real-time bidirectional communication
- **Dependencies**: Uses `creack/pty` for cross-platform PTY support, `gorilla/websocket` for WebSocket communication, and Wails v2 for desktop app framework

### Frontend (Vanilla JavaScript + xterm.js)
- **Entry Point**: `hbf/frontend/src/main.js` - Initializes xterm.js terminal with addons and connects via WebSocket
- **Terminal Manager**: `hbf/frontend/src/terminal.js` - Handles WebSocket connection management and real-time I/O
- **Terminal Integration**: Uses xterm.js with FitAddon for responsive resizing and WebLinksAddon for clickable links

### Key Components
- **TerminalManager**: Spawns shell processes in PTY, runs secure WebSocket server for real-time communication
- **WebSocket Architecture**: Token-based secure WebSocket server (port range 62400-62500) for bidirectional terminal I/O
- **PTY Integration**: Uses creack/pty for proper terminal escape sequence support (critical for colors/cursor control)
- **Cross-platform**: Supports Linux/macOS (bash) and Windows (PowerShell)

## Development Commands

### Build and Run
```bash
# Development mode with hot reload (run from hbf/ directory)
cd hbf && wails dev -tags webkit2_41

# Production build  
wails build -tags webkit2_41

# Frontend development (from hbf/frontend/)
npm run dev
npm run build
```

### Project Structure
```
hbf/                    # Wails project root
├── app.go             # Main application logic and terminal methods
├── main.go            # Wails application entry point  
├── terminal.go        # TerminalManager with PTY handling
├── wails.json         # Wails project configuration
├── go.mod             # Go dependencies
└── frontend/          # Frontend assets
    ├── src/
    │   ├── main.js    # xterm.js initialization and backend connection
    │   ├── terminal.js # Terminal management logic
    │   └── wails-bridge.js # Wails communication bridge
    ├── index.html     # Main HTML template
    └── package.json   # Frontend dependencies (xterm.js, vite)
```

## Technical Notes

### PTY vs Pipes
The application uses PTY (pseudo-terminal) instead of pipes because PTY provides full terminal capabilities including escape sequences for colors, cursor control, and terminal resizing. This is essential for proper terminal emulation.

### Terminal Communication Flow (WebSocket-based)
1. Frontend calls `StartTerminal()` on backend via Wails binding
2. Backend spawns shell process in PTY using creack/pty and starts secure WebSocket server
3. Frontend connects to WebSocket server using token-based authentication
4. Real-time bidirectional communication:
   - PTY output → WebSocket → Frontend (xterm.js display)
   - Frontend keystrokes → WebSocket → PTY input
5. No polling required - all I/O is event-driven and real-time

### Cross-platform Considerations  
- Linux/macOS: Uses `/bin/bash -l` as default shell
- Windows: Uses `powershell.exe`
- Environment: Sets `TERM=xterm-256color` for proper color support

### Key Methods
- `StartTerminal()` - Initialize shell process in PTY and start WebSocket server
- `GetWebSocketURL()` - Returns secure WebSocket URL with token for frontend connection
- `ResizeTerminal(cols, rows)` - Handle terminal resize events
- `IsTerminalRunning()` - Check process status
- `StopTerminal()` - Gracefully terminate shell process and WebSocket server
- WebSocket handles all input/output automatically (no manual polling)

## Development Environment

The project runs in a devcontainer with WebKit2 dependencies. Use the `webkit2_41` build tag for proper Wails functionality in the container environment.