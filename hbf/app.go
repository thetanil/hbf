package main

import (
	"context"
	"fmt"
)

// App struct
type App struct {
	ctx context.Context
}

// NewApp creates a new App application struct
func NewApp() *App {
	return &App{}
}

// startup is called when the app starts. The context is saved
// so we can call the runtime methods
func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
}

// domReady is called after front-end resources have been loaded
func (a *App) domReady(ctx context.Context) {
	// Add any DOM ready logic here
}

// shutdown is called when the application is about to quit,
// or when Ctrl-C is pressed in CLI mode
func (a *App) shutdown(ctx context.Context) {
	// Add cleanup logic here
}

// Greet returns a greeting for the given name
func (a *App) Greet(name string) string {
	return fmt.Sprintf("Hello %s, It's show time!", name)
}

// Basic test method for Wails communication
func (a *App) TestConnection() string {
	return "Backend connection successful"
}
