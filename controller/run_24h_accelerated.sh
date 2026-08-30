#!/bin/sh
# Phase A — GhostMode Volltest. Simulator only. 127.0.0.1. No real devices.
set -e
cd "$(dirname "$0")/.."
mkdir -p /tmp/ghost_lab/export /tmp/ghost_lab/logs
unset GHOST_OS_HALT 2>/dev/null || true
export GHOST_DOWN_ARMED=1
export GHOST_LAB_TIME_FACTOR="${GHOST_LAB_TIME_FACTOR:-200}"
export GHOST_LAB_GHOST_MODE=1
exec python3 controller/controller.py --hours 24 --accelerated --time-factor "$GHOST_LAB_TIME_FACTOR" \
  --export-dir /tmp/ghost_lab/export
