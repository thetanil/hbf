export class TerminalManager {
  constructor(terminal) {
    this.terminal = terminal;
    this.isConnected = false;
    this.websocket = null;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 5;
    this.reconnectDelay = 1000;
  }

  async connect() {
    if (this.isConnected || this.websocket) {
      console.log("Already connected, skipping");
      return true;
    }
    
    try {
      console.log("Starting terminal with WebSocket...");
      
      // Import Wails functions dynamically to avoid issues
      const { StartTerminal, GetWebSocketURL } = await import('../wailsjs/go/main/App.js');
      
      // Start the terminal backend
      const result = await StartTerminal();
      
      if (!result.success) {
        console.error("Failed to start terminal:", result.error);
        return false;
      }

      console.log("Terminal backend started successfully");
      
      // Get WebSocket URL
      const wsURL = await GetWebSocketURL();
      console.log("Received WebSocket URL:", wsURL);
      if (!wsURL) {
        console.error("No WebSocket URL received");
        return false;
      }

      console.log("Attempting to connect to WebSocket:", wsURL);
      
      // Connect to WebSocket
      return await this.connectWebSocket(wsURL);
      
    } catch (error) {
      console.error("Error connecting to terminal:", error);
      return false;
    }
  }

  async connectWebSocket(wsURL) {
    return new Promise((resolve, reject) => {
      try {
        this.websocket = new WebSocket(wsURL);
        
        this.websocket.onopen = () => {
          console.log("✅ WebSocket connected successfully to", wsURL);
          this.isConnected = true;
          this.reconnectAttempts = 0;
          this.setupInputHandling();
          
          // Test write directly to terminal on connection
          this.terminal.write("🚀 Frontend WebSocket connected and terminal.write() works!\r\n");
          
          resolve(true);
        };

        this.websocket.onmessage = (event) => {
          console.log("📥 WebSocket received data:", event.data);
          if (event.data instanceof ArrayBuffer) {
            // Handle binary data
            const uint8Array = new Uint8Array(event.data);
            const text = new TextDecoder().decode(uint8Array);
            console.log("📥 Binary data converted to text:", text.length, "chars");
            this.terminal.write(text);
          } else {
            // Handle text data
            console.log("📥 Text data:", event.data.length, "chars");
            this.terminal.write(event.data);
          }
        };

        this.websocket.onclose = (event) => {
          console.log("❌ WebSocket closed:", event.code, event.reason);
          this.isConnected = false;
          // Disable automatic reconnection for debugging
          // this.attemptReconnect(wsURL);
        };

        this.websocket.onerror = (error) => {
          console.error("💥 WebSocket error:", error);
          this.isConnected = false;
          reject(false);
        };

        // Timeout for connection
        setTimeout(() => {
          if (!this.isConnected) {
            this.websocket.close();
            reject(false);
          }
        }, 10000); // 10 second timeout

      } catch (error) {
        console.error("Error creating WebSocket:", error);
        reject(false);
      }
    });
  }

  async attemptReconnect(wsURL) {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.log("Max reconnect attempts reached");
      return;
    }

    this.reconnectAttempts++;
    console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);

    setTimeout(async () => {
      try {
        await this.connectWebSocket(wsURL);
      } catch (error) {
        console.error("Reconnect failed:", error);
      }
    }, this.reconnectDelay * this.reconnectAttempts);
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

  sendInput(data) {
    if (!this.isConnected || !this.websocket) {
      console.warn("WebSocket not connected, cannot send input");
      return false;
    }

    try {
      // Send as binary data to match backend expectations
      const encoder = new TextEncoder();
      const uint8Array = encoder.encode(data);
      this.websocket.send(uint8Array);
      return true;
    } catch (error) {
      console.error("Error sending input:", error);
      return false;
    }
  }

  async resize(cols, rows) {
    if (!this.isConnected) {
      console.warn("Terminal not connected, cannot resize");
      return false;
    }

    try {
      const { ResizeTerminal } = await import('../wailsjs/go/main/App.js');
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

  async disconnect() {
    try {
      this.isConnected = false;
      
      // Close WebSocket connection
      if (this.websocket) {
        this.websocket.close();
        this.websocket = null;
      }
      
      // Stop terminal backend
      const { StopTerminal } = await import('../wailsjs/go/main/App.js');
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

  async checkConnection() {
    try {
      const { IsTerminalRunning } = await import('../wailsjs/go/main/App.js');
      const isRunning = await IsTerminalRunning();
      if (!isRunning && this.isConnected) {
        console.log("Terminal process died, attempting reconnect...");
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