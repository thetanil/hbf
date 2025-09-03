# Phase 2: Shell Integration

## Overview

Phase 2 focuses on implementing real shell integration, transforming the current visual-only terminal into a functional terminal emulator that can execute commands and interact with the operating system.

## Goals

- Implement actual shell process spawning and management
- Enable real command execution through the terminal
- Handle input/output streaming between XTerm.js and shell processes
- Provide proper terminal environment and resize handling

## Implementation Tasks

### Backend Implementation (Go)

#### 1. Process Management

**File: `terminal.go`**

- Replace placeholder `TerminalManager` with actual shell process management
- Implement shell spawning (bash/sh on Linux/Mac, PowerShell/cmd on Windows)
- Add process lifecycle management (start, stop, cleanup)
- Handle process exit codes and cleanup

**Key Methods to Implement:**

- `StartShell()` - Spawn new shell process with PTY
- `StopShell()` - Gracefully terminate shell process
- `IsRunning()` - Check if shell process is active

#### 2. PTY Integration

- Add PTY (pseudo-terminal) support using appropriate Go library
- Handle PTY creation and configuration
- Manage PTY sizing and resize events
- Implement proper signal handling

**Dependencies to Add:**

- `github.com/creack/pty` or similar PTY library

#### 3. Input/Output Streaming

**Update `terminal.go` methods:**

- `SendInput(data string)` - Send user input to shell process
- `ReadOutput()` - Read shell output for forwarding to frontend
- Implement WebSocket or similar real-time communication
- Handle input buffering and output streaming

#### 4. Terminal Environment

- Set up proper environment variables (PATH, TERM, etc.)
- Handle working directory changes
- Implement terminal resize functionality
- Configure shell-specific settings

### Frontend Implementation (JavaScript)

#### 1. Real-time Communication

**File: `frontend/src/terminal.js`**

- Implement actual terminal input handling
- Connect XTerm.js input events to backend
- Handle real-time output streaming from backend
- Add proper error handling and reconnection logic

#### 2. Terminal Input Handling

**Update `TerminalManager` class:**

- `connect()` - Establish real connection to backend shell
- `sendInput(data)` - Send user keystrokes to shell
- `handleOutput(data)` - Display shell output in terminal
- Implement proper key binding and special key handling

#### 3. Terminal Features

- Handle terminal resize events properly
- Implement proper cursor positioning
- Add support for terminal control sequences
- Handle clipboard operations (copy/paste)

### Communication Protocol

#### 1. Wails Method Updates

**Add to `app.go`:**

- `StartTerminal()` - Initialize terminal session
- `SendTerminalInput(input string)` - Send input to terminal
- `ResizeTerminal(cols, rows int)` - Handle terminal resize
- `GetTerminalOutput()` - Retrieve output (if using polling)

#### 2. Event-Based Communication

- Consider implementing WebSocket-like event streaming
- Handle real-time bidirectional communication
- Implement proper error handling and recovery

### Error Handling & Edge Cases

#### 1. Process Management

- Handle shell process crashes
- Implement graceful cleanup on app shutdown
- Handle multiple terminal sessions (future consideration)
- Manage resource cleanup and memory leaks

#### 2. Platform Compatibility

- Linux/Mac: Handle different shell types (bash, zsh, fish)
- Environment variable handling per platform
- Path separator and command differences

### Testing Strategy

#### 1. Basic Functionality

- Test shell process spawning
- Verify input/output streaming
- Test terminal resize functionality
- Validate proper cleanup

#### 2. Command Testing

- Basic commands: `ls`, `cd`, `pwd`, `echo`
- Interactive commands: `nano`, `vim`, `htop`
- Long-running processes: `tail -f`, `ping`
- Process interruption: Ctrl+C, Ctrl+Z

#### 3. Cross-Platform Testing

- Test on Linux
- Verify shell compatibility
- Test environment variable handling

## Implementation Priority

### Phase 2.1: Core Shell Integration

1. Basic PTY integration and shell spawning
2. Simple input/output handling
3. Process lifecycle management
4. Basic command execution testing

### Phase 2.2: Enhanced Features

1. Proper terminal environment setup
2. Resize handling and terminal configuration
3. Advanced input handling (special keys, clipboard)
4. Error handling and recovery

### Phase 2.3: Polish & Testing

1. Cross-platform testing and fixes
2. Performance optimization
3. Memory leak prevention
4. User experience improvements

## Success Criteria

- [ ] Shell processes spawn successfully
- [ ] Commands execute and return proper output
- [ ] Interactive commands work (nano, vim basic functionality)
- [ ] Terminal resize works correctly
- [ ] Proper cleanup on exit
- [ ] Cross-platform compatibility
- [ ] No memory leaks or resource issues

## Risks & Mitigation

- **Risk**: PTY complexity across platforms
  - **Mitigation**: Use well-tested Go PTY libraries
- **Risk**: Performance issues with output streaming
  - **Mitigation**: Implement efficient buffering and batching
- **Risk**: Security concerns with shell access
  - **Mitigation**: Proper input sanitization and process isolation

## Dependencies

- Go PTY library (github.com/creack/pty)
- Potential WebSocket library for real-time communication

## Estimated Timeline

- **Phase 2.1**: 1-2 weeks
- **Phase 2.2**: 1 week
- **Phase 2.3**: 1 week
- **Total Phase 2**: 3-4 weeks
