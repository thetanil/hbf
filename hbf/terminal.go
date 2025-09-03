package main

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"runtime"
	"sync"
	"syscall"

	"github.com/creack/pty"
)

// TerminalManager handles terminal operations
type TerminalManager struct {
	ctx      context.Context
	cmd      *exec.Cmd
	ptmx     *os.File
	isRunning bool
	mutex    sync.RWMutex
	outputCh chan string
	errorCh  chan error
}

// NewTerminalManager creates a new terminal manager
func NewTerminalManager() *TerminalManager {
	return &TerminalManager{
		outputCh: make(chan string, 100),
		errorCh:  make(chan error, 10),
	}
}

// StartShell spawns a new shell process with PTY
func (t *TerminalManager) StartShell() error {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	if t.isRunning {
		return fmt.Errorf("terminal is already running")
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
	ptmx, err := pty.Start(t.cmd)
	if err != nil {
		return fmt.Errorf("failed to start pty: %w", err)
	}

	t.ptmx = ptmx
	t.isRunning = true

	// Start output reading goroutine
	go t.readOutput()

	return nil
}

// StopShell gracefully terminates the shell process
func (t *TerminalManager) StopShell() error {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	if !t.isRunning {
		return nil
	}

	// Close PTY
	if t.ptmx != nil {
		t.ptmx.Close()
	}

	// Terminate process
	if t.cmd != nil && t.cmd.Process != nil {
		if runtime.GOOS == "windows" {
			t.cmd.Process.Kill()
		} else {
			t.cmd.Process.Signal(syscall.SIGTERM)
		}
		t.cmd.Wait()
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

// SendInput sends user input to shell process
func (t *TerminalManager) SendInput(data string) error {
	t.mutex.RLock()
	defer t.mutex.RUnlock()

	if !t.isRunning || t.ptmx == nil {
		return fmt.Errorf("terminal not running")
	}

	_, err := t.ptmx.WriteString(data)
	return err
}

// GetOutput retrieves output from the shell
func (t *TerminalManager) GetOutput() (string, error) {
	select {
	case output := <-t.outputCh:
		return output, nil
	case err := <-t.errorCh:
		return "", err
	default:
		return "", nil
	}
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

// readOutput reads output from PTY in a goroutine
func (t *TerminalManager) readOutput() {
	defer func() {
		t.mutex.Lock()
		t.isRunning = false
		t.mutex.Unlock()
	}()

	reader := bufio.NewReader(t.ptmx)
	buffer := make([]byte, 1024)

	for {
		n, err := reader.Read(buffer)
		if err != nil {
			if err == io.EOF {
				return
			}
			select {
			case t.errorCh <- err:
			default:
			}
			return
		}

		if n > 0 {
			output := string(buffer[:n])
			select {
			case t.outputCh <- output:
			default:
				// Channel full, skip this output
			}
		}
	}
}