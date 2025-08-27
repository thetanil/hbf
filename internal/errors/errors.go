package errors

import (
	"fmt"
	"hbf/internal/log"
)

// HandleError logs an error with context if it's not nil
func HandleError(err error, context string) {
	if err != nil {
		log.Error("%s: %v", context, err)
	}
}

// FatalError logs a fatal error and panics
func FatalError(err error, context string) {
	if err != nil {
		log.Fatal("FATAL - %s: %v", context, err)
	}
}

// WrapError wraps an error with additional context
func WrapError(err error, context string) error {
	if err == nil {
		return nil
	}
	return fmt.Errorf("%s: %w", context, err)
}
