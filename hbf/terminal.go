package main

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"runtime"
	"sync"
	"syscall"
	"time"

	"github.com/creack/pty"
	"github.com/gorilla/websocket"
)

// TerminalManager handles terminal operations with WebSocket support
type TerminalManager struct {
	ctx       context.Context
	cmd       *exec.Cmd
	ptmx      *os.File
	isRunning bool
	mutex     sync.RWMutex
	
	// WebSocket server
	server     *http.Server
	wsUpgrader websocket.Upgrader
	wsToken    string
	wsPort     int
	wsURL      string
	wsConn     *websocket.Conn
	wsConnMu   sync.RWMutex
}

// NewTerminalManager creates a new terminal manager with WebSocket support
func NewTerminalManager() *TerminalManager {
	return &TerminalManager{
		wsUpgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool {
				// Allow all origins for debugging
				log.Printf("WebSocket CheckOrigin: %s", r.Header.Get("Origin"))
				return true
			},
		},
	}
}

// generateSecureToken creates a random token for WebSocket access
func (t *TerminalManager) generateSecureToken() string {
	bytes := make([]byte, 16)
	rand.Read(bytes)
	return hex.EncodeToString(bytes)
}

// findAvailablePort finds an available port in the range 62400-62500
func (t *TerminalManager) findAvailablePort() (int, error) {
	for port := 62400; port <= 62500; port++ {
		addr := fmt.Sprintf(":%d", port)
		listener, err := net.Listen("tcp", addr)
		if err == nil {
			listener.Close()
			return port, nil
		}
	}
	return 0, fmt.Errorf("no available ports in range 62400-62500")
}

// StartShell spawns a new shell process with PTY and WebSocket server
func (t *TerminalManager) StartShell() error {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	log.Printf("StartShell called: isRunning=%t", t.isRunning)

	if t.isRunning {
		return fmt.Errorf("terminal is already running")
	}

	// Generate WebSocket token and find port
	t.wsToken = t.generateSecureToken()
	port, err := t.findAvailablePort()
	if err != nil {
		log.Printf("Failed to find available port: %v", err)
		return fmt.Errorf("failed to find available port: %w", err)
	}
	t.wsPort = port
	t.wsURL = fmt.Sprintf("ws://localhost:%d/ws/%s", port, t.wsToken)
	log.Printf("Generated WebSocket URL: %s", t.wsURL)

	// Start WebSocket server
	if err := t.startWebSocketServer(); err != nil {
		return fmt.Errorf("failed to start WebSocket server: %w", err)
	}

	// Determine shell based on platform
	var shell string
	var args []string

	switch runtime.GOOS {
	case "linux", "darwin":
		shell = "/bin/bash"
		args = []string{"-l"}
	case "windows":
		shell = "powershell.exe"
		args = []string{}
	default:
		return fmt.Errorf("unsupported platform: %s", runtime.GOOS)
	}

	// Create command
	t.cmd = exec.Command(shell, args...)
	t.cmd.Env = append(os.Environ(), "TERM=xterm-256color")

	// Start the command with a pty
	log.Printf("Starting PTY with command: %s %v", shell, args)
	ptmx, err := pty.Start(t.cmd)
	if err != nil {
		log.Printf("Failed to start PTY: %v", err)
		t.stopWebSocketServer()
		return fmt.Errorf("failed to start pty: %w", err)
	}

	t.ptmx = ptmx
	t.isRunning = true
	log.Printf("Terminal started successfully: isRunning=%t", t.isRunning)

	// Start I/O goroutines
	go t.handlePtyOutput()

	return nil
}

// startWebSocketServer starts the WebSocket server
func (t *TerminalManager) startWebSocketServer() error {
	mux := http.NewServeMux()
	mux.HandleFunc(fmt.Sprintf("/ws/%s", t.wsToken), t.handleWebSocket)

	t.server = &http.Server{
		Addr:    fmt.Sprintf(":%d", t.wsPort),
		Handler: mux,
	}

	go func() {
		if err := t.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Printf("WebSocket server error: %v", err)
		}
	}()

	return nil
}

// stopWebSocketServer stops the WebSocket server
func (t *TerminalManager) stopWebSocketServer() {
	if t.server != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 5*1e9) // 5 seconds
		defer cancel()
		t.server.Shutdown(ctx)
	}
}

// handleWebSocket handles WebSocket connections
func (t *TerminalManager) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	log.Printf("WebSocket connection attempt from %s", r.RemoteAddr)
	
	// Close existing connection if any
	t.wsConnMu.Lock()
	if t.wsConn != nil {
		log.Printf("Closing existing WebSocket connection")
		t.wsConn.Close()
	}
	t.wsConnMu.Unlock()
	
	conn, err := t.wsUpgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket upgrade error: %v", err)
		return
	}
	defer conn.Close()
	log.Printf("WebSocket connection established")

	t.wsConnMu.Lock()
	t.wsConn = conn
	t.wsConnMu.Unlock()

	// Send test message to verify connection
	testMsg := "HBF Terminal WebSocket Connected!\r\n"
	err = conn.WriteMessage(websocket.TextMessage, []byte(testMsg))
	if err != nil {
		log.Printf("Failed to send test message: %v", err)
	} else {
		log.Printf("📤 Sent test message (%d bytes)", len(testMsg))
	}

	// Create done channel to coordinate goroutine cleanup
	done := make(chan bool, 1)

	// Handle incoming WebSocket messages (user input)
	go t.handleWebSocketInput(conn, done)

	// Wait for the input handler to finish
	<-done

	log.Printf("WebSocket handler cleanup")

	t.wsConnMu.Lock()
	t.wsConn = nil
	t.wsConnMu.Unlock()
}

// handleWebSocketInput processes input from WebSocket (user typing)
func (t *TerminalManager) handleWebSocketInput(conn *websocket.Conn, done chan<- bool) {
	defer close(done) // Signal when this handler exits
	log.Printf("WebSocket input handler started")
	
	for {
		// Read WebSocket messages (blocking)
		_, message, err := conn.ReadMessage()
		if err != nil {
			log.Printf("WebSocket input read error: %v", err)
			return
		}

		// Input received from WebSocket

		// Send input to PTY
		t.mutex.RLock()
		if t.isRunning && t.ptmx != nil {
			_, err := t.ptmx.Write(message)
			if err != nil {
				log.Printf("PTY write error: %v", err)
			}
		} else {
			log.Printf("PTY not running, cannot send input")
		}
		t.mutex.RUnlock()
	}
}

// handlePtyOutput reads from PTY and sends to WebSocket
func (t *TerminalManager) handlePtyOutput() {
	defer func() {
		t.mutex.Lock()
		t.isRunning = false
		t.mutex.Unlock()
	}()

	log.Printf("PTY output handler started")
	buffer := make([]byte, 1024)
	for {
		n, err := t.ptmx.Read(buffer)
		if err != nil {
			if err == io.EOF {
				log.Printf("PTY read EOF")
				return
			}
			log.Printf("PTY read error: %v", err)
			return
		}

		if n > 0 {
			data := buffer[:n]
			// Forward PTY output to WebSocket
			
			// Send to WebSocket client
			t.wsConnMu.RLock()
			if t.wsConn != nil {
				err := t.wsConn.WriteMessage(websocket.TextMessage, data)
				if err != nil {
					log.Printf("WebSocket write error: %v", err)
				}
			} else {
				log.Printf("No WebSocket connection to send data to")
			}
			t.wsConnMu.RUnlock()
		}
	}
}

// StopShell gracefully terminates the shell process and WebSocket server
func (t *TerminalManager) StopShell() error {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	if !t.isRunning {
		return nil
	}

	// Close WebSocket connection first
	t.wsConnMu.Lock()
	if t.wsConn != nil {
		t.wsConn.Close()
		t.wsConn = nil
	}
	t.wsConnMu.Unlock()

	// Stop WebSocket server
	t.stopWebSocketServer()

	// Close PTY first to signal process to exit
	if t.ptmx != nil {
		t.ptmx.Close()
		t.ptmx = nil
	}

	// Terminate process with timeout
	if t.cmd != nil && t.cmd.Process != nil {
		done := make(chan error, 1)
		go func() {
			done <- t.cmd.Wait()
		}()

		// Try graceful termination first
		if runtime.GOOS != "windows" {
			t.cmd.Process.Signal(syscall.SIGTERM)
		}

		// Wait up to 2 seconds for graceful termination
		select {
		case <-done:
			// Process exited gracefully
		case <-time.After(2 * time.Second):
			// Force kill if graceful termination failed
			log.Printf("Force killing shell process")
			t.cmd.Process.Kill()
			<-done // Wait for Kill to complete
		}
		t.cmd = nil
	}

	t.isRunning = false
	return nil
}

// IsRunning checks if shell process is active
func (t *TerminalManager) IsRunning() bool {
	t.mutex.RLock()
	defer t.mutex.RUnlock()
	return t.isRunning
}

// Resize handles terminal resize
func (t *TerminalManager) Resize(cols int, rows int) error {
	t.mutex.RLock()
	defer t.mutex.RUnlock()

	if !t.isRunning || t.ptmx == nil {
		return fmt.Errorf("terminal not running")
	}

	return pty.Setsize(t.ptmx, &pty.Winsize{
		Rows: uint16(rows),
		Cols: uint16(cols),
	})
}

// GetWebSocketURL returns the WebSocket URL for frontend connection
func (t *TerminalManager) GetWebSocketURL() string {
	t.mutex.RLock()
	defer t.mutex.RUnlock()
	log.Printf("GetWebSocketURL called: isRunning=%t, wsURL=%s", t.isRunning, t.wsURL)
	return t.wsURL
}

// Legacy methods for compatibility (will be removed)
func (t *TerminalManager) SendInput(data string) error {
	// This is now handled via WebSocket, but keeping for compatibility
	t.mutex.RLock()
	defer t.mutex.RUnlock()

	if !t.isRunning || t.ptmx == nil {
		return fmt.Errorf("terminal not running")
	}

	_, err := t.ptmx.WriteString(data)
	return err
}

func (t *TerminalManager) GetOutput() (string, error) {
	// This is now handled via WebSocket, return empty for compatibility
	return "", nil
}