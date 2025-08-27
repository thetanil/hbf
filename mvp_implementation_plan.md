# HBF MVP 2: Neovim-Raylib GUI Implementation Plan

## Architecture Overview

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

## Development Environment

- **Target**: Debian-slim devcontainer + Bazel
- **Language**: Go with raylib-go bindings + neovim/go-client
- **Build System**: Bazel with Go rules
- **Dependencies**: Managed via Bazel WORKSPACE/MODULE.bazel

## Stage 0: Project Bootstrap & Bazel Setup

**Goals**: Establish build system and project structure

**Tasks**:
- Set up Bazel workspace with Go rules
- Configure dependencies via Bazel:
  - `github.com/gen2brain/raylib-go/raylib`
  - `github.com/neovim/go-client/nvim`
- Create modular project structure:
  ```
  /cmd/hbf/          # Main binary
  /internal/nvim/    # Neovim client wrapper
  /internal/render/  # Raylib rendering
  /internal/ui/      # UI state management
  /internal/input/   # Input handling
  /build/            # Bazel build files
  /devcontainer/     # Container setup
  ```
- Add basic logging and error handling framework
- Set up testing infrastructure (unit + integration)

**Bazel Targets**:
```starlark
//cmd/hbf:hbf                    # Main binary
//internal/nvim:nvim_test        # Client wrapper tests
//build:integration_test         # Full stack tests
```

**Deliverable**: Working Bazel build that compiles empty Go modules

---

## Stage 1: Foundation + go-client Integration

**Goals**: Establish Neovim connection using go-client and basic raylib window

**Tasks**:
- Integrate neovim/go-client library
- Implement Neovim process spawning and connection
- Set up UI attachment with proper error handling:
  ```go
  v, err := nvim.NewEmbedded(&nvim.EmbedOptions{
      Args: []string{"--embed", "--headless"},
  })
  v.AttachUI(width, height, options)
  ```
- Open raylib window with basic event loop
- Add comprehensive logging for debugging
- Implement graceful shutdown and cleanup

**Architecture Decisions**:
- Use go-client's built-in RPC handling
- Separate goroutines for UI events and Neovim events
- Channel-based communication between components

**Testing**:
- Mock Neovim attachment for unit tests
- Integration test with real embedded Neovim
- Error condition testing (connection failures, process crashes)

**Deliverable**: Raylib window that successfully connects to Neovim via go-client

---

## Stage 2: Redraw Event Handling

**Goals**: Process Neovim UI events and render basic grid

**Tasks**:
- Handle core redraw notifications using go-client:
  ```go
  v.RegisterHandler("redraw", func(updates ...[]interface{}) {
      // Process grid_resize, grid_clear, grid_line events
  })
  ```
- Implement grid state management (resize, clear, line updates)
- Render monospace text using raylib's text functions  
- Add font loading and basic highlight support
- Handle window resize and grid adjustment

**go-client Integration**:
- Use `nvim.RegisterHandler()` for redraw events
- Parse redraw event arrays according to UI protocol
- Handle UI option changes and grid updates

**Testing**:
- Unit tests for redraw event parsing
- Grid state consistency tests
- Visual verification of text rendering

**Deliverable**: Window displays Neovim buffer content (static display)

---

## Stage 3: Input & Bidirectional Communication

**Goals**: Forward input to Neovim and handle cursor

**Tasks**:
- Implement keyboard input forwarding via go-client:
  ```go
  v.Input(keyString)
  ```
- Handle special keys and modifiers
- Add cursor rendering from redraw events
- Implement mode-aware cursor display
- Add basic mouse support
- Handle window resize notifications to Neovim

**Input Architecture**:
- Raylib key code → Neovim key string mapping
- Input queue with proper ordering
- Mode-sensitive input handling

**Testing**:
- Input forwarding accuracy
- Cursor positioning validation
- Mode transition testing

**Deliverable**: Functional text editing with proper cursor handling

---

## Stage 4: UI Elements & Advanced Rendering

**Goals**: Support Neovim's UI components

**Tasks**:
- Implement floating windows/popups from redraw events
- Add cmdline rendering (command mode, search)
- Handle completion menus and wildmenu
- Implement statusline/tabline rendering
- Add message area support
- Expand highlight group support

**UI Management**:
- Multi-grid handling (main grid + overlays)
- Z-order management for popups
- Dynamic positioning and clipping

**Testing**:
- UI element positioning accuracy
- Popup rendering and interaction
- Highlight application validation

**Deliverable**: Core Neovim UI elements render correctly

---

## Stage 5: Performance & Visual Polish

**Goals**: Optimize rendering and improve visual quality

**Tasks**:
- Optimize text rendering:
  - Glyph texture caching
  - Batch rendering operations
  - Dirty region tracking
- Add font configuration support
- Implement better text quality (anti-aliasing)
- Support HiDPI/scaling
- Add configuration system

**Performance Targets**:
- 60fps on 1080p displays
- <16ms input latency
- Memory usage under 100MB

**Deliverable**: Smooth, production-ready editor

---

## Stage 6: System Integration

**Goals**: OS integration and robustness

**Tasks**:
- Clipboard integration (system copy/paste)
- File drag & drop support
- Window management improvements
- Configuration file support
- Error recovery and stability improvements

**Deliverable**: Feature-complete GUI with OS integration

---

## Development Workflow

### Key Dependencies (Bazel)
```starlark
# In WORKSPACE or MODULE.bazel
go_repository(
    name = "com_github_neovim_go_client",
    importpath = "github.com/neovim/go-client",
    version = "v1.2.1",
)

go_repository(
    name = "com_github_gen2brain_raylib_go",
    importpath = "github.com/gen2brain/raylib-go/raylib",
    version = "v1.0.0",
)
```

### Testing Strategy
```bash
# Unit tests
bazel test //internal/...

# Integration with real Neovim
bazel test //build:integration_test

# Performance benchmarks  
bazel run //build:benchmarks
```

### Debugging Tools
- Built-in redraw event logging
- Visual grid boundaries in debug mode
- Performance metrics display
- RPC message tracing

---

## Risk Mitigation

**Technical Risks**:
- go-client API changes → Pin specific version, monitor releases
- Raylib compatibility → Test on target platforms early
- Performance bottlenecks → Profile continuously, optimize incrementally

**Timeline Risks**:
- Complex UI protocol → Start with subset, expand gradually
- Rendering complexity → MVP-first approach

---

## Success Metrics

- **Stage 1-2**: Basic text display works
- **Stage 3-4**: Daily editing is functional  
- **Stage 5-6**: Production ready

**Performance Targets**:
- Startup: <2 seconds
- Input latency: <16ms  
- Memory: <200MB
- CPU idle: <5%