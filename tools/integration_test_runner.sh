#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Bazel sh_test: boots HBF test pod and verifies endpoint status codes.
# Usage: integration_test_runner.sh /path/to/hbf_test
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [[ $# -lt 1 ]]; then
  echo -e "${RED}Usage: $0 <server_binary>${NC}" >&2
  exit 2
fi
SERVER_BIN="$1"
shift || true

PORT=""
SERVER_PID=""
STDOUT_LOG=""
STDERR_LOG=""

cleanup() {
  local code=$?
  if [[ -n "${SERVER_PID}" ]]; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
  [[ -n "${STDOUT_LOG}" ]] && rm -f "${STDOUT_LOG}" || true
  [[ -n "${STDERR_LOG}" ]] && rm -f "${STDERR_LOG}" || true
  exit $code
}
trap cleanup EXIT INT TERM

find_available_port() {
  # Try a few random high ports; avoid netcat dependency by optimistic probing
  for _ in $(seq 1 50); do
    local p=$(( (RANDOM % 10000) + 20000 ))
    if ! curl -sS "http://127.0.0.1:${p}/health" -m 0.1 >/dev/null 2>&1; then
      echo "$p"
      return 0
    fi
  done
  # Fallback to default if none found
  echo 5309
}

wait_for_server() {
  local port="$1"
  local max_attempts=60
  local attempt=0
  while [[ $attempt -lt $max_attempts ]]; do
    if curl -sS "http://127.0.0.1:${port}/health" -m 0.5 >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
    attempt=$((attempt+1))
  done
  echo -e "${RED}Server failed to become ready on port ${port}${NC}"
  echo -e "${YELLOW}--- server stdout ---${NC}"; [[ -f "$STDOUT_LOG" ]] && tail -n +1 "$STDOUT_LOG" || true
  echo -e "${YELLOW}--- server stderr ---${NC}"; [[ -f "$STDERR_LOG" ]] && tail -n +1 "$STDERR_LOG" || true
  return 1
}

run_checks_for() {
  local expected_code="$1"; shift
  local file="$1"; shift
  local failed=0

  while read -r method path rest; do
    # skip comments/blank
    [[ -z "${method:-}" ]] && continue
    [[ "$method" == \#* ]] && continue
    # allow annotating with DEV to skip here
    if [[ "$method" == DEV ]]; then
      continue
    fi
    if [[ -z "${path:-}" ]]; then
      echo -e "${YELLOW}Skipping malformed line in ${file}${NC}" >&2
      continue
    fi
    # Issue request
    local status
    status=$(curl -s -o /dev/null -w '%{http_code}' -X "$method" "http://127.0.0.1:${PORT}${path}") || status="000"
    if [[ "$status" != "$expected_code" ]]; then
      echo -e "${RED}FAIL${NC} ${method} ${path} (expected ${expected_code}, got ${status})"
      failed=$((failed+1))
    else
      echo -e "${GREEN}PASS${NC} ${method} ${path} -> ${status}"
    fi
  done <"$file"

  return $failed
}

main() {
  local total_failed=0

  PORT=$(find_available_port)
  STDOUT_LOG=$(mktemp)
  STDERR_LOG=$(mktemp)

  echo -e "${YELLOW}Starting server: ${SERVER_BIN} --port ${PORT}${NC}"
  "${SERVER_BIN}" --port "${PORT}" --log-level info >"$STDOUT_LOG" 2>"$STDERR_LOG" &
  SERVER_PID=$!
  if ! wait_for_server "$PORT"; then
    exit 1
  fi

  echo "Checking endpoints (expected 200s)"
  if ! run_checks_for 200 "${RUNFILES_DIR:-$PWD}/tools/integration_endpoints_200.txt"; then
    total_failed=$((total_failed+$?))
  fi

  echo "Checking endpoints (expected 404s)"
  if ! run_checks_for 404 "${RUNFILES_DIR:-$PWD}/tools/integration_endpoints_404.txt"; then
    total_failed=$((total_failed+$?))
  fi

  echo "Checking endpoints (expected 403s)"
  if ! run_checks_for 403 "${RUNFILES_DIR:-$PWD}/tools/integration_endpoints_403.txt"; then
    total_failed=$((total_failed+$?))
  fi

  if [[ $total_failed -gt 0 ]]; then
    echo -e "${RED}${total_failed} endpoint checks failed${NC}"
    echo -e "${YELLOW}--- server stderr (tail) ---${NC}"; tail -n 50 "$STDERR_LOG" || true
    exit 1
  fi
  echo -e "${GREEN}All endpoint checks passed${NC}"
}

main "$@"
