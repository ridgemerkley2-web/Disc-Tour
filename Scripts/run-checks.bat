@echo off
setlocal
cd /d "%~dp0.."
rem Delegates to Scripts\run_source_checks.py, which runs every source-only check
rem and reports all of them instead of stopping at the first failure. Pass through
rem any arguments, e.g. --fast to skip the mutation harness.
where py >nul 2>&1
if %errorlevel%==0 (
  py -3 Scripts\run_source_checks.py %*
) else (
  python Scripts\run_source_checks.py %*
)
set RESULT=%errorlevel%
echo.
if not "%DGTOUR_NO_PAUSE%"=="1" pause
exit /b %RESULT%
