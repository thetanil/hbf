package input

import (
	"hbf/internal/log"
)

// Handler manages keyboard and mouse input
type Handler struct {
	// TODO: Add input state tracking
}

// New creates a new input handler
func New() *Handler {
	log.Info("Creating new input handler")
	return &Handler{}
}

// ProcessKeyboard processes keyboard input events
func (h *Handler) ProcessKeyboard() {
	log.Debug("Processing keyboard input")
	// TODO: Implement keyboard processing with raylib
}

// ProcessMouse processes mouse input events
func (h *Handler) ProcessMouse() {
	log.Debug("Processing mouse input")
	// TODO: Implement mouse processing with raylib
}
