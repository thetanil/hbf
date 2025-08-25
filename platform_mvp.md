# Platform MVP Task Summary

## Core Infrastructure

### Build System Foundation
- Set up Bazel workspace with proper dependency management
- Create Bazel packages for SDL2, SDL2_ttf, OpenGL static builds
- Integrate SQLite3 as static library
- Ensure reproducible builds and proper caching
- Script existing CMake builds through Bazel without modifying upstream projects

### Base Runtime Layer
- Initialize SDL2 window management and OpenGL context
- Establish main event loop and frame rendering cycle
- Implement basic resource extraction and temporary file management
- Set up cross-platform binary packaging system

## Graphics and Rendering

### OpenGL Surface Layer
- Implement shader compilation and hot reload system
- Create basic rendering pipeline for GLSL shaders
- Handle shader uniform updates and parameter passing
- Establish rendering loop integrated with SDL2 events

### ImGui Integration
- Integrate ImGui into OpenGL rendering pipeline
- Create basic UI panels for shader controls and debugging
- Handle input event forwarding between SDL2 and ImGui
- Implement example controls: sliders, buttons, parameter panels

## Audio Engine

### SDL2 Audio Integration
- Set up SDL2 audio subsystem for real-time synthesis
- Implement basic audio sequencing and playback
- Create audio buffer management and callback system
- Optional integration with STK libraries for extended synthesis capabilities

## Text Editor Integration

### Neovim Overlay System
- Package Neovim binary as embedded application resource
- Implement subprocess management with msgpack-RPC communication
- Create text rendering overlay using SDL2_ttf
- Handle keyboard input forwarding and buffer synchronization
- Implement transparency and positioning for editor overlay

## Storage and Persistence

### SQLite3 Integration
- Set up database schema for persistent storage
- Implement storage for Markdown documentation and shader cache
- Create messaging system between application components
- Handle application state persistence and restoration

## Scripting Layer

### Starlark Integration
- Embed Starlark interpreter with hermetic evaluation
- Implement deterministic script execution with immutable shared data
- Create API bindings for shader manipulation and application state
- Optional Lua integration for lightweight runtime customization
- Optional Python embedding with module support for advanced scripting

## Application Features

### Live Shader Editing Workflow
- Connect Neovim overlay to shader hot reload system
- Implement real-time shader compilation and error display
- Create workflow for iterative shader development
- Handle file watching and automatic recompilation

### Interactive Documentation System
- Store and retrieve Markdown documents from SQLite
- Create executable test cells within documentation
- Implement conversion between Markdown cells and code files
- Build navigation system using ImGui panels

### Audio Sequencing Interface
- Create UI controls for audio parameter manipulation
- Implement basic sequencing and timing controls
- Connect audio engine to ImGui parameter widgets
- Handle real-time audio parameter updates

## Deployment and Packaging

### Single Binary Output
- Ensure all dependencies are statically linked
- Bundle all resources into final executable
- Create cross-platform build targets for Windows and Linux
- Implement proper resource extraction and cleanup on startup/shutdown

### Testing and Validation
- Verify deterministic execution across all scripting layers
- Test shader hot reload performance and reliability
- Validate audio latency and real-time performance requirements
- Ensure single-file deployment works across target platforms

## Success Criteria

The MVP is complete when:
- Single statically-linked executable runs on Windows and Linux
- OpenGL shaders can be edited live with Neovim overlay and hot reload
- ImGui provides functional UI controls for parameters and debugging
- Audio synthesis works with real-time parameter control
- Interactive Markdown documents can be created and executed
- All resources are embedded and no external dependencies required
- Build system produces reproducible, cached builds