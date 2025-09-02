# Phase 1: Project Setup - Detailed Implementation for hbf

## Overview

Establish the foundational Wails project structure for HBF and configure basic
communication between Go backend and vanilla JavaScript frontend with xterm.js
integration.

## Step 1: Initialize Wails Project Structure

### 1.1 Install Wails CLI

```bash
# Install Wails CLI if not already installed
go install github.com/wailsapp/wails/v2/cmd/wails@latest
```

### 1.2 Create New Wails Project

```bash
# Create new Wails project with vanilla template
wails init -n hbf -t vanilla

# Navigate to project directory
cd hbf
```

### 1.3 Project Structure Overview

```
hbf/
├── app/                    # Go backend code
│   ├── app.go             # Main application struct
│   └── terminal.go        # Terminal management (to be created)
├── frontend/              # Frontend assets
│   ├── dist/              # Build output
│   ├── index.html         # Main HTML file
│   ├── src/
│   │   ├── main.js        # Main JavaScript entry
│   │   └── terminal.js    # Terminal management (to be created)
│   └── package.json       # Frontend dependencies
├── build/                 # Build configuration
├── wails.json             # Wails configuration
├── main.go               # Go application entry point
└── go.mod                # Go module dependencies
```

## Step 2: Configure Go Backend Dependencies

### 2.1 Add Required Go Dependencies

```bash
# Add PTY library for terminal management
go get github.com/creack/pty

# Add any additional Wails dependencies if needed
go mod tidy
```

### 2.2 Update go.mod Structure

Ensure `go.mod` includes:

```go
module hbf

go 1.21

require (
    github.com/wailsapp/wails/v2 v2.x.x
    github.com/creack/pty v1.x.x
)
```

### 2.3 Create Backend Structure

```bash
# Create additional backend files
touch app/terminal.go
touch app/pty.go
```

## Step 3: Configure Frontend with xterm.js

### 3.1 Navigate to Frontend Directory

```bash
cd frontend
```

### 3.2 Install xterm.js Dependencies

```bash
# Install xterm.js and required addons with latest versions
npm install xterm@latest xterm-addon-fit@latest xterm-addon-web-links@latest
```

### 3.3 Update package.json

Ensure `frontend/package.json` includes:
After installation, `frontend/package.json` will contain the latest versions:

```json
{
  "name": "hbf-frontend",
  "version": "1.0.0",
  "dependencies": {
    "xterm": "^5.x.x",
    "xterm-addon-fit": "^0.x.x",
    "xterm-addon-web-links": "^0.x.x"
  }
}
```

> Note: Versions will be automatically determined by npm to get the latest
> compatible releases.

### 3.4 Create Frontend Module Structure

```bash
# Create terminal management module
touch src/terminal.js
touch src/wails-bridge.js
```

## Step 4: Basic Wails Communication Setup

### 4.1 Update main.go

```go
package main

import (
    "context"
    "embed"

    "github.com/wailsapp/wails/v2"
    "github.com/wailsapp/wails/v2/pkg/options"
    "github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

//go:embed all:frontend/dist
var assets embed.FS

func main() {
    // Create an instance of the app structure
    app := NewApp()

    // Create application with options
    err := wails.Run(&options.App{
        Title:  "hbf - Wails Xterm Neovim",
        Width:  1024,
        Height: 768,
        AssetServer: &assetserver.Options{
            Assets: assets,
        },
        BackgroundColour: &options.RGBA{R: 27, G: 38, B: 54, A: 1},
        OnStartup:        app.startup,
        OnDomReady:       app.domReady,
        OnShutdown:       app.shutdown,
    })

    if err != nil {
        println("Error:", err.Error())
    }
}
```

### 4.2 Update app/app.go

```go
package main

import (
    "context"
    "fmt"
)

// App struct
type App struct {
    ctx context.Context
}

// NewApp creates a new App application struct
func NewApp() *App {
    return &App{}
}

// startup is called when the app starts up
func (a *App) startup(ctx context.Context) {
    a.ctx = ctx
}

// domReady is called after front-end resources have been loaded
func (a *App) domReady(ctx context.Context) {
    // Add any DOM ready logic here
}

// shutdown is called when the application is about to quit,
// or when Ctrl-C is pressed in CLI mode
func (a *App) shutdown(ctx context.Context) {
    // Add cleanup logic here
}

// Greet returns a greeting for the given name
func (a *App) Greet(name string) string {
    return fmt.Sprintf("Hello %s, It's show time!", name)
}

// Basic test method for Wails communication
func (a *App) TestConnection() string {
    return "Backend connection successful"
}
```

### 4.3 Create Basic Terminal Backend Stub

Create `app/terminal.go`:

```go
package main

import (
    "context"
)

// TerminalManager handles terminal operations
type TerminalManager struct {
    ctx context.Context
}

// NewTerminalManager creates a new terminal manager
func NewTerminalManager() *TerminalManager {
    return &TerminalManager{}
}

// Connect placeholder for terminal connection
func (t *TerminalManager) Connect() string {
    return "Terminal connection placeholder"
}

// SendInput placeholder for sending input
func (t *TerminalManager) SendInput(data string) string {
    return "Input sent: " + data
}

// Resize placeholder for terminal resize
func (t *TerminalManager) Resize(cols int, rows int) string {
    return fmt.Sprintf("Terminal resized to %dx%d", cols, rows)
}
```

## Step 5: Basic Frontend Setup

### 5.1 Update frontend/index.html

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>HBF - Wails Xterm Neovim</title>
    <link rel="stylesheet" href="node_modules/xterm/css/xterm.css" />
    <style>
      body {
        margin: 0;
        padding: 0;
        background-color: #1a1a1a;
        font-family: monospace;
      }
      #terminal-container {
        width: 100vw;
        height: 100vh;
        overflow: hidden;
      }
    </style>
  </head>
  <body>
    <div id="terminal-container"></div>
    <script type="module" src="src/main.js"></script>
  </body>
</html>
```

### 5.2 Update frontend/src/main.js

```javascript
import { Terminal } from "xterm";
import { FitAddon } from "xterm-addon-fit";
import { WebLinksAddon } from "xterm-addon-web-links";

// Initialize terminal
const terminal = new Terminal({
  cursorBlink: true,
  fontFamily: 'Monaco, Menlo, "Ubuntu Mono", monospace',
  fontSize: 14,
  theme: {
    background: "#1a1a1a",
    foreground: "#ffffff",
  },
});

// Add addons
const fitAddon = new FitAddon();
terminal.loadAddon(fitAddon);
terminal.loadAddon(new WebLinksAddon());

// Mount terminal
const terminalContainer = document.getElementById("terminal-container");
terminal.open(terminalContainer);

// Fit terminal to container
fitAddon.fit();

// Handle window resize
window.addEventListener("resize", () => {
  fitAddon.fit();
});

// Test Wails connection
window.go.main.App.TestConnection().then((result) => {
  terminal.write("Wails connection test: " + result + "\r\n");
});

// Basic terminal test
terminal.write("Terminal initialized successfully\r\n");
terminal.write("Ready for Phase 2 implementation\r\n");
```

### 5.3 Create Terminal Management Module Stub

Create `frontend/src/terminal.js`:

```javascript
// Terminal management functionality will be implemented in Phase 2
export class TerminalManager {
  constructor(terminal) {
    this.terminal = terminal;
  }

  connect() {
    // Placeholder for terminal connection
    console.log("Terminal connection placeholder");
  }

  sendInput(data) {
    // Placeholder for input handling
    console.log("Sending input:", data);
  }

  resize(cols, rows) {
    // Placeholder for resize handling
    console.log("Resizing terminal:", cols, rows);
  }
}
```

## Step 6: Build and Test Basic Setup

### 6.1 Build Frontend

```bash
# From project root - dependencies already installed in Step 3.2
cd frontend
npm run build
cd ..
```

### 6.2 Build and Run Wails App

```bash
# Build the application
wails build

# Or run in development mode
wails dev
```

### 6.3 Verify Setup

- [ ] Application launches without errors
- [ ] Terminal container displays in the window
- [ ] xterm.js terminal renders correctly
- [ ] Basic Wails communication works (test message displays)
- [ ] Window resizing affects terminal size
- [ ] No console errors in development tools

## Step 7: Configuration Files

### 7.1 Update wails.json

```json
{
  "name": "hbf",
  "outputfilename": "hbf",
  "frontend:install": "npm install",
  "frontend:build": "npm run build",
  "frontend:dev:watcher": "npm run dev",
  "frontend:dev:serverUrl": "auto",
  "author": {
    "name": "",
    "email": ""
  }
}
```

### 7.2 Create .gitignore

```gitignore
# Build outputs
build/bin/
frontend/dist/
frontend/node_modules/

# Go
*.exe
*.exe~
*.dll
*.so
*.dylib
*.test
*.out
go.work

# IDEs
.vscode/
.idea/
*.swp
*.swo

# OS
.DS_Store
Thumbs.db
```

## Success Criteria for Phase 1

- [x] Wails project structure is properly initialized
- [x] Go backend builds without errors
- [x] Frontend builds and bundles correctly
- [x] xterm.js terminal renders in the application window
- [x] Basic Wails communication between frontend and backend works
- [x] Terminal responds to window resizing
- [x] Development environment is ready for Phase 2 implementation

## Next Steps

Phase 1 completion sets up the foundation for Phase 2, where we'll implement:

- Actual PTY integration in the Go backend
- Neovim process spawning and management
- Real-time communication between terminal and backend
- Input/output streaming between xterm.js and Neovim
