[CmdletBinding()]
param(
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'

function Get-ResolvedExactPath {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Expected
    )

    $resolved = (Resolve-Path -LiteralPath $Path).Path.TrimEnd('\')
    if (-not $resolved.Equals($Expected.TrimEnd('\'), [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Resolved path '$resolved' did not match expected path '$Expected'."
    }
    return $resolved
}

function Assert-ChildTarget {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Root,
        [Parameter(Mandatory)] [string]$RequiredLeaf
    )

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (-not $resolved.StartsWith($Root + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target '$resolved' escaped '$Root'."
    }
    if ([System.IO.Path]::GetFileName($resolved) -notlike $RequiredLeaf) {
        throw "Cleanup target '$resolved' did not match leaf '$RequiredLeaf'."
    }
    return $resolved
}

function Get-TargetBytes {
    param([Parameter(Mandatory)] [string[]]$Paths)

    [int64]$total = 0
    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }
        $item = Get-Item -LiteralPath $path -Force
        if ($item.PSIsContainer) {
            $total += (Get-ChildItem -LiteralPath $path -File -Recurse -Force -ErrorAction SilentlyContinue |
                Measure-Object -Property Length -Sum).Sum
        }
        else {
            $total += $item.Length
        }
    }
    return $total
}

$testRoot = Get-ResolvedExactPath -Path 'C:\DGTour_TestRuns' -Expected 'C:\DGTour_TestRuns'
$projectRoot = Get-ResolvedExactPath -Path 'C:\DGTour' -Expected 'C:\DGTour'
$automationRoot = Get-ResolvedExactPath -Path 'C:\DGTour\Saved\Automation' -Expected 'C:\DGTour\Saved\Automation'

$targets = [System.Collections.Generic.List[string]]::new()

$testIntermediates = @(Get-ChildItem -LiteralPath $testRoot -Directory -Filter 'Intermediate' -Recurse -Force -ErrorAction SilentlyContinue |
    ForEach-Object { Assert-ChildTarget -Path $_.FullName -Root $testRoot -RequiredLeaf 'Intermediate' } |
    Sort-Object -Unique)
foreach ($path in $testIntermediates) {
    $targets.Add($path)
}

$registryTargets = @(Get-ChildItem -LiteralPath $automationRoot -Recurse -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'CachedAssetRegistry*' } |
    ForEach-Object { Assert-ChildTarget -Path $_.FullName -Root $automationRoot -RequiredLeaf 'CachedAssetRegistry*' } |
    Sort-Object { $_.Length } -Descending -Unique)
foreach ($path in $registryTargets) {
    $targets.Add($path)
}

$projectIntermediate = Join-Path $projectRoot 'Intermediate'
if (Test-Path -LiteralPath $projectIntermediate) {
    $targets.Add((Get-ResolvedExactPath -Path $projectIntermediate -Expected 'C:\DGTour\Intermediate'))
}

$exactOptionalTargets = @(
    'C:\Users\ridge\AppData\Local\UnrealEngine\Common\DerivedDataCache',
    'C:\DGTour\DerivedDataCache'
)
foreach ($path in $exactOptionalTargets) {
    if (Test-Path -LiteralPath $path) {
        $targets.Add((Get-ResolvedExactPath -Path $path -Expected $path))
    }
}

$uniqueTargets = @($targets | Sort-Object -Unique)
$bytes = if ($uniqueTargets.Count -eq 0) {
    [int64]0
}
else {
    Get-TargetBytes -Paths $uniqueTargets
}
$summary = [ordered]@{
    mode = if ($Execute) { 'execute' } else { 'dry-run' }
    targetCount = $uniqueTargets.Count
    testIntermediateCount = $testIntermediates.Count
    automationRegistryCount = $registryTargets.Count
    targetBytes = $bytes
    targetGiB = [math]::Round($bytes / 1GB, 4)
}

if ($Execute) {
    foreach ($path in $uniqueTargets | Sort-Object { $_.Length } -Descending) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
    $drive = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
    $summary.freeGiBAfter = [math]::Round($drive.FreeSpace / 1GB, 2)
}

[pscustomobject]$summary | ConvertTo-Json -Compress
