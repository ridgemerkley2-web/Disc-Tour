@echo off
setlocal
where py >nul 2>&1
if %errorlevel%==0 (
  py Scripts\validate_project.py
) else (
  python Scripts\validate_project.py
)
pause
