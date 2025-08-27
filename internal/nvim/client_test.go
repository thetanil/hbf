package nvim

import (
	"context"
	"testing"
)

func TestClientCreation(t *testing.T) {
	ctx := context.Background()
	client := New(ctx)

	if client == nil {
		t.Error("Expected client to be created, got nil")
	}

	if client.ctx != ctx {
		t.Error("Expected client context to match provided context")
	}
}
