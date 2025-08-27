package log

import (
	"bytes"
	"strings"
	"testing"
)

func TestLogging(t *testing.T) {
	// Capture log output for testing
	var buf bytes.Buffer
	Logger.SetOutput(&buf)

	Info("test message: %s", "hello")

	output := buf.String()
	if !strings.Contains(output, "[INFO] test message: hello") {
		t.Errorf("Expected log message not found in output: %s", output)
	}
}
