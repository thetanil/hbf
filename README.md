# setup for a wails project

Wails + xterm.js + Neovim Goal

Build a desktop app (using Wails) that embeds xterm.js as the frontend terminal
emulator and connects it to Neovim running inside a pseudo-terminal (PTY). The
user should be able to launch the app and interact with Neovim just as they
would in a real terminal.

Components

1. Wails Backend (Go)

Responsible for spawning Neovim as a subprocess inside a pseudo-terminal (PTY).

Uses a PTY library to provide full terminal capabilities (colors, cursor
control, resizing).

Streams Neovim’s stdout/stderr back to the frontend via Wails’ EventsEmit
system.

Receives input (keystrokes) from the frontend and writes them into Neovim’s
stdin.

Handles window resize events so that Neovim is aware of the current terminal
size.

Provides a set of methods that the frontend can call:

Connect() → starts Neovim inside a PTY.

SendInput(data) → sends user keystrokes to Neovim.

Resize(cols, rows) → adjusts Neovim’s terminal size when the frontend resizes.

2. Frontend (Vanilla JavaScript + xterm.js)

Runs inside the Wails webview.

Embeds xterm.js as a terminal emulator inside the app window.

Uses xterm-addon-fit to auto-adjust the terminal to the window size.

Subscribes to Wails events to receive Neovim’s output and display it in the
terminal.

Captures user keystrokes from xterm.js and forwards them to the backend.

Detects window resize events and notifies the backend of the new terminal
dimensions.

3. Integration Flow

The user opens the Wails app → frontend loads and initializes xterm.js.

The frontend calls Connect() on the backend → backend launches Neovim inside a
PTY.

The backend starts streaming Neovim’s output as events → frontend receives them
and displays them in xterm.js.

The user types into xterm.js → frontend sends keystrokes to the backend →
backend writes them into Neovim’s PTY.

Resizing the app window → frontend recalculates cols/rows → backend resizes the
PTY → Neovim adapts its display.

Design Considerations

PTY vs Pipes: Using a PTY is critical because Neovim expects terminal escape
sequences for proper rendering. Pipes would only provide raw text without cursor
movement or colors.

Frontend Tech: Implementation avoids React/TypeScript. Instead, it uses vanilla
JavaScript modules and plain HTML. This keeps the setup simple and lightweight.

Cross-platform: Wails and xterm.js work on Linux, macOS, and Windows. The PTY
library (creack/pty) supports Unix systems directly. On Windows, ConPTY (Windows
10+) is used under the hood.

User Experience:

App launches directly into a Neovim terminal.

Terminal resizes automatically with the window.

Full color and cursor movement supported.

Looks and behaves like running Neovim in a real terminal.

Optional Enhancements

Open files directly: Pass a filename from the command line or menu, and start
Neovim with that file open.

Theming: Match xterm.js colors/fonts with Neovim’s theme.

Configurable binary: Allow users to select which nvim binary to use.

Plugin support: Hook into Neovim’s RPC API (if needed) for tighter integration.

Save/restore session: Reopen with the last active file and cursor position.

Would you like me to also prepare a step-by-step implementation plan (like a
checklist for the developer) so that they can follow it in sequence without
needing to figure out the integration order?

claude is running inside a devcontainer with `claude --dangerously-skip-permissions`
