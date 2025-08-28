package main

import (
	"context"
	"os"
	"os/signal"
	"syscall"

	"hbf/internal/errors"
	"hbf/internal/input"
	"hbf/internal/log"
	"hbf/internal/nvim"
	"hbf/internal/render"
	"hbf/internal/ui"

	rl "github.com/gen2brain/raylib-go/raylib"
)

func main() {
	log.Info("Starting HBF - Neovim GUI with Raylib")

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Set up graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Initialize all components
	nvimClient := nvim.New(ctx)
	renderer := render.New(800, 600)
	stateManager := ui.New()
	inputHandler := input.New()

	log.Info("All modules initialized successfully")

	// Stage 1 Implementation: Connect to Neovim and initialize Raylib
	log.Info("Stage 1: Connecting to Neovim and initializing Raylib...")

	// Connect to embedded Neovim
	err := nvimClient.ConnectEmbedded()
	if err != nil {
		errors.FatalError(err, "Failed to connect to embedded Neovim")
		return
	}
	defer nvimClient.Disconnect()

	// Initialize Raylib window
	err = renderer.Initialize()
	if err != nil {
		errors.FatalError(err, "Failed to initialize renderer")
		return
	}
	defer renderer.Close()

	log.Info("HBF Stage 1 (Foundation + go-client Integration) completed!")
	log.Info("Starting main event loop...")

	// Attach UI to Neovim in a separate goroutine to avoid blocking
	go func() {
		log.Info("Attempting to attach UI to Neovim...")
		err := nvimClient.AttachUI(800, 600)
		if err != nil {
			log.Error("Failed to attach UI to Neovim: %v", err)
		} else {
			log.Info("Successfully attached UI to Neovim")
		}
	}()

	// Main event loop
	running := true
	for running && !renderer.ShouldClose() {
		select {
		case <-sigChan:
			log.Info("Received shutdown signal")
			running = false
		default:
			// Handle events and render
			renderer.BeginDrawing()

			// Display "Connected to Neovim" message
			renderer.DrawText("Connected to Neovim", 10, 10, 20, rl.Green)
			renderer.DrawText("HBF Stage 1 Complete!", 10, 40, 16, rl.White)

			renderer.EndDrawing()
		}
	}

	log.Info("HBF shutting down gracefully...")

	// TODO: In Stage 1, we'll actually connect to Neovim and initialize Raylib
	_ = stateManager
	_ = inputHandler
}
