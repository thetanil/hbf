package render

import (
	"hbf/internal/log"
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
	// TODO: Initialize raylib window
	return nil
}

// Close cleans up rendering resources
func (r *Renderer) Close() {
	log.Info("Closing renderer...")
	// TODO: Clean up raylib resources
}
