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

## Development Environment ✅ **CURRENT**

- **Target**: ✅ Debian-slim devcontainer + Bazel (Working)
- **Language**: ✅ Go 1.21 with raylib-go bindings + neovim/go-client (Ready)
- **Build System**: ✅ Bazel with Go rules via bzlmod (Working)
- **Dependencies**: ✅ Managed via MODULE.bazel + go.mod (Configured)
- **IDE**: ✅ VS Code with Go extension + gopls working
- **Container**: ✅ Dev container with all tools installed

## Stage 0: Project Bootstrap & Bazel Setup ✅ **COMPLETED**

**Goals**: Establish build system and project structure

**Tasks**: ✅ **ALL COMPLETE**
- ✅ Set up Bazel workspace with Go rules (using bzlmod)
- ✅ Configure dependencies via MODULE.bazel (ready for raylib/neovim deps)
- ✅ Create modular project structure:
  ```
  /cmd/hbf/               # Main binary ✅
  /internal/log/          # Logging framework ✅
  /internal/errors/       # Error handling utilities ✅
  /internal/nvim/         # Neovim client wrapper ✅
  /internal/render/       # Raylib renderer ✅
  /internal/ui/           # UI state management ✅
  /internal/input/        # Input handling ✅
  /.devcontainer/         # Container setup ✅
  ```
- ✅ Add basic logging and error handling framework
- ✅ Set up testing infrastructure (unit + integration)

**Bazel Targets**: ✅ **ALL WORKING**
```starlark
//cmd/hbf:hbf                    # Main binary ✅
//internal/nvim:nvim_test        # Client wrapper tests ✅
//internal/log:log_test          # Logging tests ✅
//:hbf_test                      # Integration test ✅
```

**Current Status**: 
- ✅ All modules compile and test successfully
- ✅ Comprehensive logging system in place
- ✅ Modular architecture established
- ✅ Development workflow documented in README.md
- ✅ Container environment working with Go 1.21

**Deliverable**: ✅ **ACHIEVED** - Working Bazel build that compiles all Go modules with proper structure

---

## Stage 1: Foundation + go-client Integration 🚧 **NEXT - READY TO START**

**Goals**: Establish Neovim connection using go-client and basic raylib window

**Prerequisites**: ✅ Stage 0 completed with all infrastructure in place

**Tasks**:
- 📋 Add dependencies to go.mod and MODULE.bazel:
  ```go
  // go.mod additions needed:
  require (
      github.com/neovim/go-client v1.2.1
      github.com/gen2brain/raylib-go/raylib v0.55.1
  )
  ```
- 📋 Implement Neovim process spawning and connection in `internal/nvim/client.go`:
  ```go
  func (c *Client) ConnectEmbedded() error {
      v, err := nvim.NewEmbedded(&nvim.EmbedOptions{
          Args: []string{"--embed", "--headless"},
      })
      if err != nil {
          return errors.WrapError(err, "failed to start embedded neovim")
      }
      c.nvim = v
      return nil
  }
  ```
- 📋 Set up UI attachment with proper error handling in `internal/nvim/client.go`
- 📋 Implement basic raylib window in `internal/render/renderer.go`:
  ```go
  func (r *Renderer) Initialize() error {
      rl.InitWindow(r.width, r.height, "HBF - Neovim GUI")
      rl.SetTargetFPS(60)
      return nil
  }
  ```
- 📋 Add event loop in main.go with graceful shutdown
- 📋 Update integration tests to use real dependencies

**Implementation Details**:
- **Neovim Client** (`internal/nvim/client.go`):
  - Wrap go-client with HBF-specific methods
  - Handle connection lifecycle (connect, disconnect, reconnect)
  - Add comprehensive error handling and logging
- **Renderer** (`internal/render/renderer.go`):
  - Initialize raylib window and context
  - Basic render loop structure
  - Window event handling (close, resize)
- **Main Loop** (`cmd/hbf/main.go`):
  - Coordinate nvim client and renderer
  - Handle shutdown signals
  - Error recovery strategies

**Architecture Decisions**:
- Use go-client's built-in RPC handling (no manual msgpack)
- Separate goroutines for UI events and Neovim events
- Channel-based communication between components
- Context-based cancellation for clean shutdown

**Testing Strategy**:
- Mock Neovim attachment for unit tests (`internal/nvim/client_test.go`)
- Integration test with real embedded Neovim (`integration_test.go`)
- Error condition testing (connection failures, process crashes)
- Raylib window creation/destruction tests

**Deliverable**: Raylib window that successfully connects to Neovim via go-client and displays "Connected to Neovim" message

---

## Stage 2: Redraw Event Handling 📋 **PLANNED**

**Goals**: Process Neovim UI events and render basic grid

**Prerequisites**: Stage 1 completed with working Neovim connection and raylib window

**Tasks**:
- 📋 Handle core redraw notifications using go-client:
  ```go
  // In internal/nvim/client.go
  func (c *Client) SetupRedrawHandler(onRedraw func(updates [][]interface{})) error {
      return c.nvim.RegisterHandler("redraw", func(updates ...[]interface{}) {
          log.Debug("Received redraw events: %d", len(updates))
          onRedraw(updates)
      })
  }
  ```
- 📋 Implement grid state management in `internal/ui/state.go`:
  ```go
  type GridState struct {
      Width, Height int
      Cells         [][]Cell
      CursorRow, CursorCol int
  }
  
  func (s *StateManager) HandleGridResize(width, height int)
  func (s *StateManager) HandleGridClear()
  func (s *StateManager) HandleGridLine(row int, cells []CellData)
  ```
- 📋 Render monospace text using raylib in `internal/render/renderer.go`:
  ```go
  func (r *Renderer) RenderGrid(grid *GridState) {
      font := rl.GetFontDefault()
      for row := 0; row < grid.Height; row++ {
          for col := 0; col < grid.Width; col++ {
              cell := grid.Cells[row][col]
              rl.DrawText(cell.Text, col*r.cellWidth, row*r.cellHeight, r.fontSize, cell.FgColor)
          }
      }
  }
  ```
- 📋 Add font loading and basic highlight support
- 📋 Handle window resize and grid adjustment
- 📋 Implement cursor rendering

**Implementation Details**:
- **Event Processing**: Parse Neovim's redraw event format
- **Grid Management**: Efficient cell storage and updates  
- **Font Rendering**: Monospace font with proper metrics
- **Color System**: RGB color handling from highlight groups
- **Performance**: Only redraw changed regions

**go-client Integration**:
- Use `nvim.RegisterHandler()` for redraw events
- Parse redraw event arrays according to UI protocol spec
- Handle UI option changes and grid updates
- Error handling for malformed events

**Testing**:
- Unit tests for redraw event parsing
- Grid state consistency tests  
- Visual verification of text rendering
- Font loading and metrics tests

**Deliverable**: Window displays Neovim buffer content with proper cursor positioning (static display)

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

### Key Dependencies (Bazel via bzlmod) ✅ **INFRASTRUCTURE READY**
```bazel
# MODULE.bazel - Current working setup
bazel_dep(name = "rules_go", version = "0.42.0")
bazel_dep(name = "gazelle", version = "0.33.0")

# Go toolchain configured for Go 1.21
go_toolchains = use_extension("@rules_go//go:extensions.bzl", "go_toolchains")
go_toolchains.go_register_toolchain(version = "1.21.12")

# Dependencies from go.mod (to be added in Stage 1)
go_deps = use_extension("@gazelle//:extensions.bzl", "go_deps")
go_deps.from_file(go_mod = "//:go.mod")
# use_repo will be auto-generated by: bazel run //:gazelle -- update-repos -from_file=go.mod
```

```go
// go.mod - Dependencies to be added in Stage 1
module hbf

go 1.21

require (
    github.com/neovim/go-client v1.2.1      // ← Add in Stage 1
    github.com/gen2brain/raylib-go/raylib v0.55.1  // ← Add in Stage 1
)
```

### Testing Strategy ✅ **CURRENT WORKING SETUP**
```bash
# All tests passing (verified working)
bazel test //...                    # Runs all 3 tests (all pass)

# Module-specific tests (verified working)
bazel test //internal/log:all       # Logging framework tests
bazel test //internal/nvim:all      # Neovim client tests

# Integration tests (verified working)
bazel test //:hbf_test             # Full module integration

# Build and run (verified working)
bazel run //cmd/hbf:hbf           # Main application

# Development workflow (verified working)
bazel run //:gazelle -- update-repos -from_file=go.mod  # Sync dependencies
bazel run //:gazelle              # Generate BUILD files
```

**Current Test Results**: ✅ All tests pass
- `//:hbf_test` - Integration test ✅ PASSED
- `//internal/log:log_test` - Logging tests ✅ PASSED  
- `//internal/nvim:nvim_test` - Nvim client tests ✅ PASSED

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

## Current Development Status 🎯

### ✅ **COMPLETED - Stage 0: Project Bootstrap & Bazel Setup**
- Complete modular architecture established
- All build and test infrastructure working
- Development workflow documented and tested
- Ready for Stage 1 implementation

### 🚧 **CURRENT FOCUS - Stage 1: Foundation + go-client Integration** 
**Ready to start immediately** - all prerequisites met

**Next Actions**:
1. Add `github.com/neovim/go-client` and `github.com/gen2brain/raylib-go/raylib` to `go.mod`
2. Run `bazel run //:gazelle -- update-repos -from_file=go.mod` to sync dependencies  
3. Implement `ConnectEmbedded()` method in `internal/nvim/client.go`
4. Add raylib window initialization in `internal/render/renderer.go`
5. Update main loop to coordinate nvim + raylib

### 📋 **PLANNED - Later Stages**
- **Stage 2**: Redraw Event Handling (detailed plan ready)
- **Stage 3**: Input & Bidirectional Communication  
- **Stage 4**: UI Elements & Advanced Rendering
- **Stage 5**: Performance & Visual Polish
- **Stage 6**: System Integration

---

## Success Metrics

- **Stage 0**: ✅ **ACHIEVED** - Project bootstrap complete with working build system
- **Stage 1-2**: Basic text display works (Neovim content visible in raylib window)  
- **Stage 3-4**: Daily editing is functional (input handling + cursor movement)
- **Stage 5-6**: Production ready (performance optimized + OS integration)

**Performance Targets**:
- Startup: <2 seconds
- Input latency: <16ms  
- Memory: <200MB
- CPU idle: <5%

---

## Development Notes & Lessons Learned

### Stage 0 Insights:
- **bzlmod approach works well** - cleaner than WORKSPACE for dependency management
- **Go 1.21 is stable choice** - avoid non-existent versions like 1.25
- **Modular architecture pays off** - clear separation of concerns established
- **Testing infrastructure essential** - integration tests catch architectural issues early
- **Logging framework crucial** - comprehensive logging added from start

### For Stage 1:
- **Dependency versions verified**: 
  - `github.com/neovim/go-client v1.2.1` (latest stable)
  - `github.com/gen2brain/raylib-go/raylib v0.55.1` (current)
- **Container environment ready** - all tools installed and working  
- **Build system proven** - handles complex Go projects with external deps
- **Error handling patterns established** - consistent across all modules