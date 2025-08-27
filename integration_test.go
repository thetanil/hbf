package main

import (
	"context"
	"testing"

	"hbf/internal/input"
	"hbf/internal/log"
	"hbf/internal/nvim"
	"hbf/internal/render"
	"hbf/internal/ui"
)

func TestModuleIntegration(t *testing.T) {
	log.Info("Running integration test for all modules")

	ctx := context.Background()

	// Test that all modules can be instantiated
	nvimClient := nvim.New(ctx)
	if nvimClient == nil {
		t.Error("Failed to create nvim client")
	}

	renderer := render.New(800, 600)
	if renderer == nil {
		t.Error("Failed to create renderer")
	}

	stateManager := ui.New()
	if stateManager == nil {
		t.Error("Failed to create state manager")
	}

	inputHandler := input.New()
	if inputHandler == nil {
		t.Error("Failed to create input handler")
	}

	log.Info("Integration test completed successfully")
}
