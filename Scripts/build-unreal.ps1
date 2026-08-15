param(
    [string]$EngineRoot = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectRoot "DiscGolfTour.uproject"

if (-not $EngineRoot) {
    $candidates = @(
        "C:\Program Files\Epic Games\UE_5.8",
        "D:\Epic Games\UE_5.8",
        "D:\UE_5.8"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $EngineRoot = $candidate
            break
        }
    }
}

if (-not $EngineRoot -or -not (Test-Path $EngineRoot)) {
    throw "Could not locate Unreal Engine 5.8. Re-run with -EngineRoot 'C:\path\to\UE_5.8'."
}

$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $BuildBat)) {
    throw "Build.bat not found under $EngineRoot"
}

Write-Host "Building DiscGolfTourEditor with $EngineRoot"
& $BuildBat DiscGolfTourEditor Win64 Development "-Project=$Project" -WaitMutex -FromMsBuild
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Unreal editor build succeeded."
