import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { WebLinksAddon } from "@xterm/addon-web-links";

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
