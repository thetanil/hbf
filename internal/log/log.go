package log

import (
	"log"
	"os"
)

var Logger = log.New(os.Stdout, "[HBF] ", log.LstdFlags|log.Lshortfile)

func Debug(msg string, args ...interface{}) {
	Logger.Printf("[DEBUG] "+msg, args...)
}

func Info(msg string, args ...interface{}) {
	Logger.Printf("[INFO] "+msg, args...)
}

func Error(msg string, args ...interface{}) {
	Logger.Printf("[ERROR] "+msg, args...)
}

func Fatal(msg string, args ...interface{}) {
	Logger.Printf("[FATAL] "+msg, args...)
	os.Exit(1)
}
