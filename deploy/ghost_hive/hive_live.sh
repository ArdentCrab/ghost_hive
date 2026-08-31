#!/bin/bash
# WSL/Linux one-click Ghost:Hive live
set -e
cd "$(dirname "$0")"
python3 hive_gate.py once
python3 auto_peers.py start
exec python3 ui/hive_manager.py
