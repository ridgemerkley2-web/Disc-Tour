@echo off
setlocal
cd /d "%~dp0.."
where git >nul 2>&1 || (echo Git is not installed or not on PATH.& pause & exit /b 1)
git init
where git-lfs >nul 2>&1 && git lfs install

git add .
git commit -m "Bootstrap Disc Golf Tour full game v0.1"
if errorlevel 1 (
    echo.
    echo The repository was initialized and files were staged, but the first commit did not complete.
    echo If Git asked for your name/email, configure those and then run: git commit -m "Bootstrap Disc Golf Tour full game v0.1"
) else (
    echo.
    echo Repository initialized with the v0.1 baseline commit.
)
echo Add a GitHub remote whenever you are ready.
pause
