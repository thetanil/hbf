package nvim

import (
	"context"
	"hbf/internal/log"
)

// Client wraps the neovim go-client with HBF-specific functionality
type Client struct {
	// nvim *nvim.Nvim  // Will be added when we import the dependency
	ctx context.Context
}

// New creates a new HBF Neovim client
func New(ctx context.Context) *Client {
	log.Info("Creating new Neovim client wrapper")
	return &Client{
		ctx: ctx,
	}
}

// Connect establishes connection to Neovim process
func (c *Client) Connect() error {
	log.Info("Connecting to Neovim process...")
	// TODO: Implement neovim connection using go-client
	return nil
}

// Disconnect closes the connection to Neovim
func (c *Client) Disconnect() error {
	log.Info("Disconnecting from Neovim process...")
	// TODO: Implement proper cleanup
	return nil
}
