@echo off
REM Ghost:Hive v2 — One-Click Live (SPEC-v2, host orchestration only)
cd /d "%~dp0"
echo === ghost:hive LIVE ===
python hive_gate.py once
if errorlevel 1 (
  echo Gate failed — peer.bind oder bin/ pruefen. make live
  exit /b 1
)
python auto_peers.py start
if errorlevel 1 (
  echo Auto-peers incomplete
  exit /b 1
)
start "ghost:hive manager" python ui\hive_manager.py
echo Hive live. Manager-Fenster offen. Stick drin lassen.
exit /b 0
