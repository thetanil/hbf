package render

import (
	"hbf/internal/log"

	rl "github.com/gen2brain/raylib-go/raylib"
)

// Renderer handles all raylib rendering operations
type Renderer struct {
	width  int
	height int
}

// New creates a new renderer
func New(width, height int) *Renderer {
	log.Info("Creating new renderer: %dx%d", width, height)
	return &Renderer{
		width:  width,
		height: height,
	}
}

// Initialize sets up the raylib window and rendering context
func (r *Renderer) Initialize() error {
	log.Info("Initializing renderer...")

	rl.InitWindow(int32(r.width), int32(r.height), "HBF - Neovim GUI")
	rl.SetTargetFPS(60)

	log.Info("Raylib window initialized successfully")
	return nil
}

// Close cleans up rendering resources
func (r *Renderer) Close() {
	log.Info("Closing renderer...")
	rl.CloseWindow()
	log.Info("Raylib window closed successfully")
}

// BeginDrawing starts a drawing frame
func (r *Renderer) BeginDrawing() {
	rl.BeginDrawing()
	rl.ClearBackground(rl.Black)
}

// EndDrawing ends the current drawing frame
func (r *Renderer) EndDrawing() {
	rl.EndDrawing()
}

// ShouldClose returns true if the window should close
func (r *Renderer) ShouldClose() bool {
	return rl.WindowShouldClose()
}

// DrawText renders text at the specified position
func (r *Renderer) DrawText(text string, x, y, fontSize int32, color rl.Color) {
	rl.DrawText(text, x, y, fontSize, color)
}
