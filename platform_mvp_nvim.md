# Platform Phase 1: Neovim MVP Implementation

## Overview

Phase 1 focuses on implementing a minimal viable Neovim overlay that provides live text editing capabilities without the complexity of full terminal emulation. This approach prioritizes getting a working editor integrated quickly while maintaining the foundation for future enhancements.

## Core Strategy

### Headless Neovim + Simple Text Display
- Use `nvim --headless` with msgpack-RPC for clean programmatic control
- Render text content using SDL2_ttf as a simple overlay
- Skip complex terminal emulation, VT sequences, and advanced UI features
- Focus on core text editing functionality first

### Key Simplifications for MVP

1. **No Terminal Emulation**
   - Direct buffer content rendering instead of terminal output parsing
   - Avoids complexity of VT100/ANSI sequence handling
   - Uses nvim's buffer API directly via RPC

2. **Simple Font Rendering**
   - Monospace font only (SDL2_ttf)
   - Plain text rendering without syntax highlighting initially
   - Fixed character grid layout

3. **Basic Transparency**
   - Simple alpha blending over OpenGL surface
   - No complex compositing or window management

4. **Embedded Binary**
   - Package nvim binary as application resource
   - Extract to temporary directory at runtime
   - Self-contained deployment

## Implementation Tasks

### Task 1: Minimal Neovim Subprocess Integration
**Goal:** Launch nvim --headless and establish msgpack-RPC communication

**Components:**
- Subprocess management (stdin/stdout pipes)
- msgpack serialization/deserialization
- Basic RPC message handling
- Process lifecycle management

**Key APIs:**
```cpp
class NeovimProcess {
    bool launch();
    bool send_request(const msgpack::object& request);
    msgpack::object receive_response();
    void shutdown();
};
```

### Task 2: Simple Text Rendering Overlay
**Goal:** Display nvim buffer content as text overlay using SDL2_ttf

**Components:**
- SDL2_ttf font loading and glyph rendering
- Text buffer to texture conversion
- Basic cursor positioning
- Overlay positioning and sizing

**Key APIs:**
```cpp
class TextOverlay {
    bool initialize(SDL_Renderer* renderer);
    void render_buffer(const std::vector<std::string>& lines);
    void set_cursor(int row, int col);
    void set_transparency(float alpha);
};
```

### Task 3: Transparency and Positioning
**Goal:** Make text overlay blend properly with OpenGL background

**Components:**
- Alpha blending setup
- Overlay positioning controls
- Background color handling
- Resize handling

### Task 4: Keyboard Input Forwarding  
**Goal:** Capture SDL2 keyboard events and send to nvim via RPC

**Components:**
- SDL2 keyboard event handling
- Key mapping to nvim input format
- Input buffering and transmission
- Special key handling (arrows, function keys, etc.)

**Key APIs:**
```cpp
class InputHandler {
    void handle_keydown(const SDL_KeyboardEvent& event);
    void send_key_to_nvim(const std::string& key_sequence);
    void handle_text_input(const std::string& text);
};
```

### Task 5: Resource Packaging
**Goal:** Embed nvim binary and extract at runtime

**Components:**
- Binary resource embedding (using xxd or similar)
- Temporary file extraction
- Cleanup on application exit
- Cross-platform path handling

## Technical Architecture

### Communication Flow
```
SDL2 Events → InputHandler → msgpack-RPC → nvim --headless
                     ↓
OpenGL Surface ← TextOverlay ← Buffer API ← nvim --headless
```

### Threading Model
- Main thread: SDL2 event loop, OpenGL rendering
- Background thread: nvim process communication
- Message queue: Thread-safe communication between layers

### Data Flow
1. User input → SDL2 events → RPC commands → nvim
2. nvim buffer changes → RPC notifications → text update → overlay render
3. Frame render: OpenGL background → text overlay → present

## Success Criteria

Phase 1 is complete when:
- [x] nvim --headless launches and responds to RPC commands
- [x] Text from nvim buffer displays as overlay on OpenGL surface  
- [x] Keyboard input reaches nvim and updates are visible
- [x] Basic transparency works (text over graphics)
- [x] nvim binary is embedded and self-contained

## Future Enhancements (Post-MVP)

### Phase 2 Additions:
- Syntax highlighting integration
- Cursor rendering and animation  
- Scrolling and viewport management
- Multiple buffer support
- nvim UI event handling (popup menus, command line)

### Phase 3 Additions:
- Plugin support and configuration
- Advanced rendering (ligatures, custom fonts)
- Integration with shader editing workflow
- Performance optimization and caching

## Implementation Notes

### msgpack-RPC Protocol
- Use nvim's RPC API v0 for compatibility
- Essential calls: `nvim_buf_get_lines`, `nvim_input`, `nvim_buf_attach`
- Handle async notifications for real-time buffer updates

### Resource Management  
- Extract nvim to `~/.cache/platform/nvim/` or similar
- Version the extracted binary to handle updates
- Cleanup on shutdown and handle extraction failures

### Error Handling
- Graceful degradation if nvim fails to launch
- Fallback to basic text input without nvim features
- User feedback for subprocess communication issues

This phase provides a solid foundation for the integrated editor while keeping complexity manageable for rapid development and iteration.