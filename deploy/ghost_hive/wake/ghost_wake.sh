#!/bin/sh
# Termux / Linux: listen for GHST_WAKE and start ghost peer (SPEC-v2).
# Install: copy bin/arm64/ghost_phone + this script; run in Termux:
#   GHOST_BIND_DIR=$PREFIX/tmp/ghost_hive python wake/ghost_wake.py P ./ghost_phone

DIR="$(cd "$(dirname "$0")/.." && pwd)"
ID="${1:-P}"
BIN="${2:-$DIR/bin/arm64/ghost_phone}"
export GHOST_BIND_DIR="${GHOST_BIND_DIR:-/tmp/ghost_hive}"
exec python3 "$DIR/wake/ghost_wake.py" "$ID" "$BIN"
