package ui

import (
	"hbf/internal/log"
)

// StateManager manages the UI state and grid data from Neovim
type StateManager struct {
	gridWidth  int
	gridHeight int
}

// New creates a new state manager
func New() *StateManager {
	log.Info("Creating new UI state manager")
	return &StateManager{}
}

// UpdateGrid updates the grid state from Neovim events
func (s *StateManager) UpdateGrid(width, height int) {
	log.Debug("Updating grid size: %dx%d", width, height)
	s.gridWidth = width
	s.gridHeight = height
}

// GetGridSize returns the current grid dimensions
func (s *StateManager) GetGridSize() (int, int) {
	return s.gridWidth, s.gridHeight
}
