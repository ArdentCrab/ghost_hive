#!/bin/sh
set -e
cd "$(dirname "$0")/.."
make lab
mkdir -p /tmp/ghost_lab/export /tmp/ghost_lab/logs
exec python3 controller/controller.py --hours 24 --export-dir /tmp/ghost_lab/export
