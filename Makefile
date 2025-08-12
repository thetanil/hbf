# Top-level Makefile for HBF project

.PHONY: dagger-all dagger-install dagger-service dagger-start dagger-stop dagger-status dagger-logs dagger-uninstall dagger-test dagger-cache-test help

# Default target
help:
	@echo "Available targets:"
	@echo "  help              - Show this help message"
	@echo ""
	@echo "Dagger Engine Management:"
	@echo "  dagger-all        - Install, create service, and start dagger engine"
	@echo "  dagger-install    - Install dagger CLI"
	@echo "  dagger-service    - Create systemd service for dagger engine"
	@echo "  dagger-start      - Enable and start dagger-engine service"
	@echo "  dagger-stop       - Stop dagger-engine service"
	@echo "  dagger-status     - Show dagger-engine service status"
	@echo "  dagger-logs       - Follow dagger-engine service logs"
	@echo "  dagger-test       - Test dagger engine by pulling alpine image (warms up engine)"
	@echo "  dagger-cache-test - Test dagger cache effectiveness by running same operation twice"
	@echo "  dagger-uninstall  - Uninstall dagger engine and remove service"

# Dagger Engine Management targets
dagger-all:
	@$(MAKE) -C hbf-setup-dagger all

dagger-install:
	@$(MAKE) -C hbf-setup-dagger install

dagger-service:
	@$(MAKE) -C hbf-setup-dagger service

dagger-start:
	@$(MAKE) -C hbf-setup-dagger start

dagger-stop:
	@$(MAKE) -C hbf-setup-dagger stop

dagger-status:
	@$(MAKE) -C hbf-setup-dagger status

dagger-logs:
	@$(MAKE) -C hbf-setup-dagger logs

dagger-test:
	@$(MAKE) -C hbf-setup-dagger test

dagger-cache-test:
	@$(MAKE) -C hbf-setup-dagger cache-test

dagger-uninstall:
	@$(MAKE) -C hbf-setup-dagger uninstall
