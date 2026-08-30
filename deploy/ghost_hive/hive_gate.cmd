@echo off
cd /d "%~dp0"
python hive_gate.py %*
exit /b %ERRORLEVEL%
