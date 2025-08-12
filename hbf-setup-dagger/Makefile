# Makefile for persistent rootless Dagger Engine via systemd (per-user)

DAGGER_BIN := $(HOME)/.local/bin/dagger
SERVICE_DIR := $(HOME)/.config/systemd/user
CACHE_DIR := $(HOME)/.cache/dagger
SOCKET_PATH := $(CACHE_DIR)/engine.sock
SERVICE_FILE := $(SERVICE_DIR)/dagger-engine.service

.PHONY: all install service start stop status logs uninstall

all: install service start

install:
	@echo ">> Installing dagger CLI..."
	mkdir -p $(HOME)/.local/bin
	curl -L https://dl.dagger.io/dagger/install.sh | sh
	mv bin/dagger $(DAGGER_BIN)
	chmod +x $(DAGGER_BIN)
	@echo ">> dagger installed to $(DAGGER_BIN)"

service:
	@echo ">> Creating systemd service..."
	mkdir -p $(SERVICE_DIR)
	mkdir -p $(CACHE_DIR)
	@echo "[Unit]" > $(SERVICE_FILE)
	@echo "Description=Persistent Dagger Engine (Rootless)" >> $(SERVICE_FILE)
	@echo "After=network.target" >> $(SERVICE_FILE)
	@echo "" >> $(SERVICE_FILE)
	@echo "[Service]" >> $(SERVICE_FILE)
	@echo "ExecStart=$(DAGGER_BIN) engine --unix $(SOCKET_PATH) --cache-dir $(CACHE_DIR)" >> $(SERVICE_FILE)
	@echo "Restart=on-failure" >> $(SERVICE_FILE)
	@echo "RestartSec=5s" >> $(SERVICE_FILE)
	@echo "StartLimitIntervalSec=60" >> $(SERVICE_FILE)
	@echo "StartLimitBurst=3" >> $(SERVICE_FILE)
	@echo "StandardOutput=journal" >> $(SERVICE_FILE)
	@echo "StandardError=journal" >> $(SERVICE_FILE)
	@echo "" >> $(SERVICE_FILE)
	@echo "# Security hardening" >> $(SERVICE_FILE)
	@echo "NoNewPrivileges=true" >> $(SERVICE_FILE)
	@echo "ProtectSystem=full" >> $(SERVICE_FILE)
	@echo "ProtectHome=read-only" >> $(SERVICE_FILE)
	@echo "PrivateTmp=true" >> $(SERVICE_FILE)
	@echo "ProtectKernelModules=true" >> $(SERVICE_FILE)
	@echo "ProtectKernelTunables=true" >> $(SERVICE_FILE)
	@echo "ProtectControlGroups=true" >> $(SERVICE_FILE)
	@echo "CapabilityBoundingSet=" >> $(SERVICE_FILE)
	@echo "" >> $(SERVICE_FILE)
	@echo "[Install]" >> $(SERVICE_FILE)
	@echo "WantedBy=default.target" >> $(SERVICE_FILE)
	systemctl --user daemon-reload
	@echo ">> Service file created at $(SERVICE_FILE)"

start:
	@echo ">> Enabling and starting dagger-engine..."
	systemctl --user enable --now dagger-engine

stop:
	@echo ">> Stopping dagger-engine..."
	systemctl --user stop dagger-engine

status:
	systemctl --user status dagger-engine

logs:
	journalctl --user -u dagger-engine -f

uninstall: stop
	@echo ">> Removing service..."
	systemctl --user disable dagger-engine
	rm -f $(SERVICE_FILE)
	systemctl --user daemon-reload
	@echo ">> Removing dagger binary..."
	rm -f $(DAGGER_BIN)
	@echo ">> Persistent cache is at $(CACHE_DIR) — remove manually if desired."
