#!/bin/sh

# Dagger Engine Management Script
# POSIX compliant with error handling and safety features

set -e  # Exit on any error
set -u  # Exit on undefined variables

# Define paths
readonly DAGGER_BIN="${HOME}/.local/bin/dagger"
readonly SERVICE_DIR="${HOME}/.config/systemd/user"
readonly CACHE_DIR="${HOME}/.cache/dagger"
readonly SOCKET_PATH="${CACHE_DIR}/engine.sock"
readonly SERVICE_FILE="${SERVICE_DIR}/dagger-engine.service"

# Available commands
readonly COMMANDS="all install service start stop status logs uninstall test cache-test help"

# Logging functions
log_info() {
    printf ">> %s\n" "$1"
}

log_error() {
    printf "Error: %s\n" "$1" >&2
}

log_warn() {
    printf "Warning: %s\n" "$1" >&2
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Ensure required commands are available
check_dependencies() {
    local missing=""
    
    if ! command_exists systemctl; then
        missing="${missing} systemctl"
    fi
    
    if ! command_exists curl; then
        missing="${missing} curl"
    fi
    
    if [ -n "$missing" ]; then
        log_error "Missing required commands:$missing"
        return 1
    fi
}

# Create directory with error handling
safe_mkdir() {
    if ! mkdir -p "$1" 2>/dev/null; then
        log_error "Failed to create directory: $1"
        return 1
    fi
}

# Install dagger CLI
install_dagger() {
    log_info "Installing dagger CLI..."
    
    # Create local bin directory
    safe_mkdir "$(dirname "$DAGGER_BIN")"
    
    # Download and install dagger
    if ! curl -L https://dl.dagger.io/dagger/install.sh | sh; then
        log_error "Failed to download or install dagger"
        return 1
    fi
    
    # Move dagger to the correct location
    if [ -f "bin/dagger" ]; then
        if ! mv bin/dagger "$DAGGER_BIN"; then
            log_error "Failed to move dagger binary to $DAGGER_BIN"
            return 1
        fi
    else
        log_error "Dagger binary not found after installation"
        return 1
    fi
    
    # Make executable
    if ! chmod +x "$DAGGER_BIN"; then
        log_error "Failed to make dagger executable"
        return 1
    fi
    
    log_info "dagger installed to $DAGGER_BIN"
    
    # Clean up
    [ -d "bin" ] && rm -rf bin
}

# Create systemd service
create_service() {
    log_info "Creating systemd service..."
    
    # Create directories
    safe_mkdir "$SERVICE_DIR"
    safe_mkdir "$CACHE_DIR"
    
    # Create service file
    cat > "$SERVICE_FILE" << EOF
[Unit]
Description=Persistent Dagger Engine (Rootless)
After=network.target

[Service]
ExecStart=$DAGGER_BIN engine --unix $SOCKET_PATH --cache-dir $CACHE_DIR
Restart=on-failure
RestartSec=5s
StartLimitIntervalSec=60
StartLimitBurst=3
StandardOutput=journal
StandardError=journal

# Security hardening
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=read-only
PrivateTmp=true
ProtectKernelModules=true
ProtectKernelTunables=true
ProtectControlGroups=true
CapabilityBoundingSet=

[Install]
WantedBy=default.target
EOF

    if [ $? -ne 0 ]; then
        log_error "Failed to create service file"
        return 1
    fi
    
    # Reload systemd
    if ! systemctl --user daemon-reload; then
        log_error "Failed to reload systemd daemon"
        return 1
    fi
    
    log_info "Service file created at $SERVICE_FILE"
}

# Start dagger engine service
start_service() {
    log_info "Enabling and starting dagger-engine..."
    
    if ! systemctl --user enable --now dagger-engine; then
        log_error "Failed to enable and start dagger-engine service"
        return 1
    fi
    
    log_info "Dagger engine service started successfully"
}

# Stop dagger engine service
stop_service() {
    log_info "Stopping dagger-engine..."
    
    if ! systemctl --user stop dagger-engine; then
        log_error "Failed to stop dagger-engine service"
        return 1
    fi
    
    log_info "Dagger engine service stopped"
}

# Show service status
show_status() {
    systemctl --user status dagger-engine
}

# Show service logs
show_logs() {
    journalctl --user -u dagger-engine -f
}

# Uninstall dagger
uninstall_dagger() {
    log_info "Uninstalling dagger..."
    
    # Stop service first
    if systemctl --user is-active --quiet dagger-engine; then
        stop_service
    fi
    
    # Disable and remove service
    if systemctl --user is-enabled --quiet dagger-engine; then
        log_info "Removing service..."
        systemctl --user disable dagger-engine || log_warn "Failed to disable service"
    fi
    
    if [ -f "$SERVICE_FILE" ]; then
        rm -f "$SERVICE_FILE" || log_warn "Failed to remove service file"
    fi
    
    # Reload systemd
    systemctl --user daemon-reload || log_warn "Failed to reload systemd daemon"
    
    # Remove binary
    if [ -f "$DAGGER_BIN" ]; then
        log_info "Removing dagger binary..."
        rm -f "$DAGGER_BIN" || log_warn "Failed to remove dagger binary"
    fi
    
    log_info "Persistent cache is at $CACHE_DIR — remove manually if desired."
    log_info "Uninstall completed"
}

# Test dagger engine
test_engine() {
    log_info "Testing dagger engine with a simple pipeline..."
    
    if [ ! -f "$DAGGER_BIN" ]; then
        log_error "Dagger binary not found. Please install first."
        return 1
    fi
    
    log_info "Running 'dagger core container from --address alpine:latest image-ref' to test engine..."
    
    # Set environment to suppress nag messages
    export DAGGER_NO_NAG=1
    
    if ! "$DAGGER_BIN" core container from --address alpine:latest image-ref; then
        log_error "Dagger engine test failed"
        return 1
    fi
    
    log_info "Dagger engine test completed successfully"
}

# Test cache effectiveness
test_cache() {
    log_info "Testing dagger engine cache effectiveness..."
    
    if [ ! -f "$DAGGER_BIN" ]; then
        log_error "Dagger binary not found. Please install first."
        return 1
    fi
    
    # Set environment to suppress nag messages
    export DAGGER_NO_NAG=1
    
    log_info "First run - should pull image and cache it:"
    if command_exists time; then
        time "$DAGGER_BIN" core container from --address alpine:latest image-ref
    else
        "$DAGGER_BIN" core container from --address alpine:latest image-ref
    fi
    
    echo ""
    log_info "Second run - should use cache (much faster):"
    if command_exists time; then
        time "$DAGGER_BIN" core container from --address alpine:latest image-ref
    else
        "$DAGGER_BIN" core container from --address alpine:latest image-ref
    fi
    
    echo ""
    log_info "Cache test complete. Second run should be significantly faster if cache is working."
}

# Show help
show_help() {
    cat << EOF
Dagger Engine Management Script

Usage: $0 <command>

Available commands:
  all               - Install, create service, and start dagger engine
  install           - Install dagger CLI
  service           - Create systemd service for dagger engine
  start             - Enable and start dagger-engine service
  stop              - Stop dagger-engine service
  status            - Show dagger-engine service status
  logs              - Follow dagger-engine service logs
  test              - Test dagger engine by pulling alpine image
  cache-test        - Test dagger cache effectiveness
  uninstall         - Uninstall dagger engine and remove service
  help              - Show this help message

Examples:
  $0 all            # Complete setup
  $0 install        # Just install the CLI
  $0 status         # Check service status
  $0 test           # Test the engine
EOF
}

# Main function
main() {
    # Check dependencies first for commands that need them
    case "$1" in
        install|service|start|stop|status|logs|uninstall|all)
            check_dependencies || exit 1
            ;;
    esac
    
    case "$1" in
        all)
            install_dagger && create_service && start_service
            ;;
        install)
            install_dagger
            ;;
        service)
            create_service
            ;;
        start)
            start_service
            ;;
        stop)
            stop_service
            ;;
        status)
            show_status
            ;;
        logs)
            show_logs
            ;;
        uninstall)
            uninstall_dagger
            ;;
        test)
            test_engine
            ;;
        cache-test)
            test_cache
            ;;
        help|--help|-h)
            show_help
            ;;
        --complete)
            echo "$COMMANDS"
            ;;
        "")
            log_error "No command specified"
            show_help
            exit 1
            ;;
        *)
            log_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

# Execute main function with all arguments
main "$@"
