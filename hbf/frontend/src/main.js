import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { WebLinksAddon } from "@xterm/addon-web-links";
import { TerminalManager } from "./terminal.js";

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

// Initialize terminal manager
const terminalManager = new TerminalManager(terminal);

// Auto-connect when page loads (WebSocket-based)
window.addEventListener('load', async () => {
  terminal.write("HBF Terminal - Connecting via WebSocket...\r\n");
  
  try {
    const connected = await terminalManager.connect();
    if (connected) {
      terminal.write("WebSocket terminal connected successfully! You can now run commands.\r\n");
      
      // Simple resize after connection
      setTimeout(() => {
        fitAddon.fit();
      }, 100);
    } else {
      terminal.write("Failed to connect to terminal backend via WebSocket.\r\n");
    }
  } catch (error) {
    terminal.write(`WebSocket connection error: ${error.message}\r\n`);
  }
});

// Handle app shutdown
window.addEventListener('beforeunload', () => {
  terminalManager.disconnect();
});
