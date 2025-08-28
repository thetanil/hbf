package nvim

import (
	"context"
	"hbf/internal/errors"
	"hbf/internal/log"

	"github.com/neovim/go-client/nvim"
)

// Client wraps the neovim go-client with HBF-specific functionality
type Client struct {
	nvim *nvim.Nvim
	ctx  context.Context
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

// ConnectEmbedded starts an embedded Neovim process and connects to it
func (c *Client) ConnectEmbedded() error {
	log.Info("Starting embedded Neovim process...")

	v, err := nvim.NewEmbedded(&nvim.EmbedOptions{
		Args: []string{"--embed", "--headless"},
	})
	if err != nil {
		return errors.WrapError(err, "failed to start embedded neovim")
	}

	c.nvim = v
	log.Info("Successfully connected to embedded Neovim")
	return nil
}

// Disconnect closes the connection to Neovim
func (c *Client) Disconnect() error {
	log.Info("Disconnecting from Neovim process...")

	if c.nvim != nil {
		err := c.nvim.Close()
		if err != nil {
			return errors.WrapError(err, "failed to close neovim connection")
		}
		c.nvim = nil
	}

	log.Info("Successfully disconnected from Neovim")
	return nil
}

// AttachUI sets up UI attachment with proper error handling
func (c *Client) AttachUI(width, height int) error {
	if c.nvim == nil {
		return errors.New("neovim client not connected")
	}

	log.Info("Attaching UI with dimensions: %dx%d", width, height)

	err := c.nvim.AttachUI(width, height, map[string]interface{}{
		"rgb":           true,
		"ext_popupmenu": true,
		"ext_tabline":   true,
		"ext_cmdline":   true,
		"ext_wildmenu":  true,
	})

	if err != nil {
		return errors.WrapError(err, "failed to attach UI")
	}

	log.Info("Successfully attached UI")
	return nil
}
