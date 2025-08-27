package main

import (
	"context"

	"hbf/internal/input"
	"hbf/internal/log"
	"hbf/internal/nvim"
	"hbf/internal/render"
	"hbf/internal/ui"
)

func main() {
	log.Info("Starting HBF - Neovim GUI with Raylib")

	ctx := context.Background()

	// Initialize all components
	nvimClient := nvim.New(ctx)
	renderer := render.New(800, 600)
	stateManager := ui.New()
	inputHandler := input.New()

	log.Info("All modules initialized successfully")
	log.Info("HBF Stage 0 (Bootstrap & Bazel Setup) completed!")

	// TODO: In Stage 1, we'll actually connect to Neovim and initialize Raylib
	_ = nvimClient
	_ = renderer
	_ = stateManager
	_ = inputHandler
}
