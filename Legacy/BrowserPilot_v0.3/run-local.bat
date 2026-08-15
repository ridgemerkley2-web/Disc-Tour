@echo off
cd /d "%~dp0"
title Pine Ridge Disc Golf Pilot 0.3
start "" "http://localhost:8765"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0serve.ps1"
if errorlevel 1 (
  echo.
  echo PowerShell server could not start. Trying Python fallback...
  where py >nul 2>nul
  if %errorlevel%==0 (
    py -m http.server 8765
  ) else (
    python -m http.server 8765
  )
)
pause
