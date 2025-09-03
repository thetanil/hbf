package main

import (
	"context"
	"fmt"
)

// App struct
type App struct {
	ctx             context.Context
	terminalManager *TerminalManager
}

// NewApp creates a new App application struct
func NewApp() *App {
	return &App{
		terminalManager: NewTerminalManager(),
	}
}

// startup is called when the app starts. The context is saved
// so we can call the runtime methods
func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
	a.terminalManager.ctx = ctx
}

// domReady is called after front-end resources have been loaded
func (a *App) domReady(ctx context.Context) {
	// Add any DOM ready logic here
}

// shutdown is called when the application is about to quit,
// or when Ctrl-C is pressed in CLI mode
func (a *App) shutdown(ctx context.Context) {
	// Stop terminal if running
	if a.terminalManager.IsRunning() {
		a.terminalManager.StopShell()
	}
}

// Greet returns a greeting for the given name
func (a *App) Greet(name string) string {
	return fmt.Sprintf("Hello %s, It's show time!", name)
}

// Basic test method for Wails communication
func (a *App) TestConnection() string {
	return "Backend connection successful"
}

// StartTerminal initializes terminal session
func (a *App) StartTerminal() map[string]interface{} {
	err := a.terminalManager.StartShell()
	if err != nil {
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}
	}
	
	return map[string]interface{}{
		"success": true,
		"message": "Terminal started successfully",
	}
}

// SendTerminalInput sends input to terminal
func (a *App) SendTerminalInput(input string) map[string]interface{} {
	err := a.terminalManager.SendInput(input)
	if err != nil {
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}
	}
	
	return map[string]interface{}{
		"success": true,
	}
}

// GetTerminalOutput retrieves output from terminal
func (a *App) GetTerminalOutput() map[string]interface{} {
	output, err := a.terminalManager.GetOutput()
	if err != nil {
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
			"output":  "",
		}
	}
	
	return map[string]interface{}{
		"success": true,
		"output":  output,
	}
}

// ResizeTerminal handles terminal resize
func (a *App) ResizeTerminal(cols, rows int) map[string]interface{} {
	err := a.terminalManager.Resize(cols, rows)
	if err != nil {
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}
	}
	
	return map[string]interface{}{
		"success": true,
	}
}

// IsTerminalRunning checks if terminal is running
func (a *App) IsTerminalRunning() bool {
	return a.terminalManager.IsRunning()
}

// StopTerminal stops the terminal
func (a *App) StopTerminal() map[string]interface{} {
	err := a.terminalManager.StopShell()
	if err != nil {
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}
	}
	
	return map[string]interface{}{
		"success": true,
		"message": "Terminal stopped successfully",
	}
}
