import { StartTerminal, SendTerminalInput, GetTerminalOutput, ResizeTerminal, IsTerminalRunning, StopTerminal } from '../wailsjs/go/main/App.js';

export class TerminalManager {
  constructor(terminal) {
    this.terminal = terminal;
    this.isConnected = false;
    this.outputPolling = null;
    this.inputBuffer = '';
  }

  async connect() {
    try {
      console.log("Starting terminal...");
      const result = await StartTerminal();
      
      if (result.success) {
        this.isConnected = true;
        console.log("Terminal started successfully");
        
        // Start polling for output
        this.startOutputPolling();
        
        // Setup input handling
        this.setupInputHandling();
        
        return true;
      } else {
        console.error("Failed to start terminal:", result.error);
        return false;
      }
    } catch (error) {
      console.error("Error connecting to terminal:", error);
      return false;
    }
  }

  async disconnect() {
    try {
      this.isConnected = false;
      
      // Stop output polling
      if (this.outputPolling) {
        clearInterval(this.outputPolling);
        this.outputPolling = null;
      }
      
      // Stop terminal
      const result = await StopTerminal();
      if (result.success) {
        console.log("Terminal stopped successfully");
      } else {
        console.error("Error stopping terminal:", result.error);
      }
    } catch (error) {
      console.error("Error disconnecting terminal:", error);
    }
  }

  async sendInput(data) {
    if (!this.isConnected) {
      console.warn("Terminal not connected");
      return false;
    }

    try {
      const result = await SendTerminalInput(data);
      if (!result.success) {
        console.error("Failed to send input:", result.error);
        return false;
      }
      return true;
    } catch (error) {
      console.error("Error sending input:", error);
      return false;
    }
  }

  async resize(cols, rows) {
    if (!this.isConnected) {
      console.warn("Terminal not connected");
      return false;
    }

    try {
      const result = await ResizeTerminal(cols, rows);
      if (!result.success) {
        console.error("Failed to resize terminal:", result.error);
        return false;
      }
      console.log(`Terminal resized to ${cols}x${rows}`);
      return true;
    } catch (error) {
      console.error("Error resizing terminal:", error);
      return false;
    }
  }

  setupInputHandling() {
    if (!this.terminal) return;

    // Handle data input from xterm.js
    this.terminal.onData((data) => {
      this.sendInput(data);
    });

    // Handle terminal resize
    this.terminal.onResize((size) => {
      this.resize(size.cols, size.rows);
    });
  }

  startOutputPolling() {
    // Poll for output every 50ms
    this.outputPolling = setInterval(async () => {
      try {
        const result = await GetTerminalOutput();
        if (result.success && result.output && result.output.length > 0) {
          // Write output to terminal
          this.terminal.write(result.output);
        }
      } catch (error) {
        console.error("Error getting terminal output:", error);
      }
    }, 50);
  }

  async checkConnection() {
    try {
      const isRunning = await IsTerminalRunning();
      if (!isRunning && this.isConnected) {
        console.log("Terminal process died, reconnecting...");
        this.isConnected = false;
        await this.connect();
      }
      return isRunning;
    } catch (error) {
      console.error("Error checking terminal status:", error);
      return false;
    }
  }
}