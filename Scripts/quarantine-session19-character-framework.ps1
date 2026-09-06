#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$QuarantineRoot = "C:\DGTour_Quarantine",
    [string]$RunId = "",
    [switch]$VerifyExistingRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-NoReparsePointInExistingPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label,
        [string[]]$AllowedReparsePaths = @()
    )

    $allowed = @($AllowedReparsePaths | ForEach-Object { [System.IO.Path]::GetFullPath($_) })
    $current = [System.IO.Path]::GetFullPath($Path)
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            $isAllowed = @($allowed | Where-Object {
                $_.Equals($current, [System.StringComparison]::OrdinalIgnoreCase)
            }).Count -ne 0
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -and -not $isAllowed) {
                throw "$Label contains an existing reparse-point ancestor: $current"
            }
        }
        $parent = [System.IO.Directory]::GetParent($current)
        if ($null -eq $parent) {
            break
        }
        $current = $parent.FullName
    }
}

function Get-PhysicalPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $full = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetPathRoot($full)
    $physical = $root
    $relative = $full.Substring($root.Length)
    foreach ($segment in @($relative -split '[\\/]' | Where-Object { $_ -ne "" })) {
        $candidate = Join-Path $physical $segment
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate -Force
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                $resolved = $item.ResolveLinkTarget($true)
                if ($null -eq $resolved) {
                    throw "Could not resolve reparse point while checking target isolation: $candidate"
                }
                $physical = $resolved.FullName
                continue
            }
        }
        $physical = $candidate
    }
    return [System.IO.Path]::GetFullPath($physical)
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$evidenceRoot = Join-Path $projectRoot "Evidence\Session19"
$quarantineRootFull = [System.IO.Path]::GetFullPath($QuarantineRoot)

Assert-NoReparsePointInExistingPath -Path $quarantineRootFull -Label "QuarantineRoot"
Assert-NoReparsePointInExistingPath -Path $evidenceRoot -Label "EvidenceRoot" -AllowedReparsePaths @($projectRoot)

if ($VerifyExistingRun -and [string]::IsNullOrWhiteSpace($RunId)) {
    throw "VerifyExistingRun requires the exact existing RunId."
}
if ([string]::IsNullOrWhiteSpace($RunId)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ")
    $suffix = [Guid]::NewGuid().ToString("N").Substring(0, 12)
    $RunId = "S19_Framework_${stamp}_${suffix}"
}
if ($RunId -notmatch '^S19_Framework_[A-Za-z0-9_-]+$') {
    throw "RunId must start with S19_Framework_ and contain only letters, digits, underscore, or hyphen."
}

$projectPrefix = $projectRoot.TrimEnd('\') + '\'
$quarantinePrefix = $quarantineRootFull.TrimEnd('\') + '\'
if ($quarantineRootFull.Equals($projectRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    $quarantineRootFull.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $projectRoot.StartsWith($quarantinePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "QuarantineRoot must be a separate location outside the project tree."
}
$projectPhysical = Get-PhysicalPath -Path $projectRoot
$quarantinePhysical = Get-PhysicalPath -Path $quarantineRootFull
$projectPhysicalPrefix = $projectPhysical.TrimEnd('\') + '\'
$quarantinePhysicalPrefix = $quarantinePhysical.TrimEnd('\') + '\'
if ($quarantinePhysical.Equals($projectPhysical, [System.StringComparison]::OrdinalIgnoreCase) -or
    $quarantinePhysical.StartsWith($projectPhysicalPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $projectPhysical.StartsWith($quarantinePhysicalPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "QuarantineRoot must resolve to a separate physical location outside the project tree."
}

$runRoot = [System.IO.Path]::GetFullPath((Join-Path $quarantineRootFull $RunId))
if (-not $runRoot.StartsWith($quarantinePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Resolved quarantine run path escaped QuarantineRoot."
}
if ($VerifyExistingRun) {
    if (-not (Test-Path -LiteralPath $runRoot -PathType Container)) {
        throw "Existing quarantine run destination is missing: $runRoot"
    }
} elseif (Test-Path -LiteralPath $runRoot) {
    throw "Fresh quarantine destination already exists: $runRoot"
}

$roots = @(
    [ordered]@{
        id = "project_plugin"
        projectPath = "Plugins/DiscGolfCharacterFramework"
        source = Join-Path $projectRoot "Plugins\DiscGolfCharacterFramework"
        recoveryRelativePath = "ProjectPlugin/DiscGolfCharacterFramework"
        destination = Join-Path $runRoot "ProjectPlugin\DiscGolfCharacterFramework"
    },
    [ordered]@{
        id = "buildkit_plugin"
        projectPath = "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/DiscGolfCharacterFramework"
        source = Join-Path $projectRoot "_BuildKit\DiscGolfCorePlayabilityKit_v1.5\Plugins\DiscGolfCharacterFramework"
        recoveryRelativePath = "BuildKitPlugin/DiscGolfCharacterFramework"
        destination = Join-Path $runRoot "BuildKitPlugin\DiscGolfCharacterFramework"
    }
)

foreach ($root in $roots) {
    $resolvedSource = [System.IO.Path]::GetFullPath($root.source)
    if (-not $resolvedSource.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Framework source escaped the project root: $resolvedSource"
    }
    if (-not $VerifyExistingRun) {
        if (-not (Test-Path -LiteralPath $resolvedSource -PathType Container)) {
            throw "Required framework source is missing: $($root.projectPath)"
        }
        $sourceItem = Get-Item -LiteralPath $resolvedSource -Force
        if (($sourceItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Framework source root is a reparse point and will not be moved: $($root.projectPath)"
        }
        $reparseEntry = Get-ChildItem -LiteralPath $resolvedSource -Force -Recurse |
            Where-Object { ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 } |
            Select-Object -First 1
        if ($null -ne $reparseEntry) {
            throw "Framework source contains a reparse point and will not be moved: $($root.projectPath)"
        }
    }
}

New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$manifestPath = Join-Path $evidenceRoot ("CharacterFrameworkBeforeMove-{0}.tsv" -f $RunId)
$receiptPath = Join-Path $evidenceRoot ("CharacterFrameworkExternalQuarantine-{0}.json" -f $RunId)
if ($VerifyExistingRun) {
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Existing before-move manifest is missing: $manifestPath"
    }
    if (Test-Path -LiteralPath $receiptPath) {
        throw "Receipt already exists; verification repair will not overwrite it: $receiptPath"
    }
} elseif ((Test-Path -LiteralPath $manifestPath) -or (Test-Path -LiteralPath $receiptPath)) {
    throw "Evidence paths for this RunId already exist."
}

$beforeByRoot = [ordered]@{}
$manifestRows = [System.Collections.Generic.List[object]]::new()
if ($VerifyExistingRun) {
    $allowedRootIds = @($roots | ForEach-Object { $_.id })
    $seenManifestKeys = @{}
    $importedRows = @(Import-Csv -LiteralPath $manifestPath -Delimiter "`t" -Encoding utf8)
    foreach ($row in $importedRows) {
        if (@($row.PSObject.Properties.Name) -join ',' -ne 'rootId,relativePath,bytes,sha256') {
            throw "Existing before-move manifest has unexpected columns."
        }
        if ($allowedRootIds -notcontains $row.rootId -or
            [string]::IsNullOrWhiteSpace($row.relativePath) -or
            $row.relativePath.Contains('\') -or
            [System.IO.Path]::IsPathFullyQualified($row.relativePath) -or
            @($row.relativePath.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count -ne 0 -or
            $row.sha256 -notmatch '^[0-9A-F]{64}$') {
            throw "Existing before-move manifest contains an unsafe or malformed row."
        }
        [long]$byteCount = 0
        if (-not [long]::TryParse($row.bytes, [ref]$byteCount) -or $byteCount -lt 0) {
            throw "Existing before-move manifest contains an invalid byte count."
        }
        $key = "$($row.rootId)`0$($row.relativePath)"
        if ($seenManifestKeys.ContainsKey($key)) {
            throw "Existing before-move manifest contains a duplicate path."
        }
        $seenManifestKeys[$key] = $true
        $manifestRows.Add([pscustomobject][ordered]@{
            rootId = $row.rootId
            relativePath = $row.relativePath
            bytes = $byteCount
            sha256 = $row.sha256
        })
    }
    foreach ($root in $roots) {
        $entries = @($manifestRows | Where-Object { $_.rootId -eq $root.id } | ForEach-Object {
            [pscustomobject][ordered]@{
                relativePath = $_.relativePath
                bytes = [long]$_.bytes
                sha256 = $_.sha256
            }
        } | Sort-Object relativePath)
        if ($entries.Count -eq 0) {
            throw "Existing before-move manifest has no entries for $($root.id)."
        }
        $beforeByRoot[$root.id] = $entries
    }
} else {
    foreach ($root in $roots) {
        $sourceRoot = [System.IO.Path]::GetFullPath($root.source)
        $entries = [System.Collections.Generic.List[object]]::new()
        Get-ChildItem -LiteralPath $sourceRoot -File -Force -Recurse | ForEach-Object {
            $relativePath = [System.IO.Path]::GetRelativePath($sourceRoot, $_.FullName).Replace('\', '/')
            $entry = [pscustomobject][ordered]@{
                relativePath = $relativePath
                bytes = [long]$_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
            $entries.Add($entry)
            $manifestRows.Add([pscustomobject][ordered]@{
                rootId = $root.id
                relativePath = $relativePath
                bytes = [long]$_.Length
                sha256 = $entry.sha256
            })
        }
        if ($entries.Count -eq 0) {
            throw "Framework source is unexpectedly empty and will not be moved: $($root.projectPath)"
        }
        $beforeByRoot[$root.id] = @($entries | Sort-Object relativePath)
    }

    $manifestRows |
        Sort-Object rootId, relativePath |
        Export-Csv -LiteralPath $manifestPath -Delimiter "`t" -NoTypeInformation -Encoding utf8
}
$manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash

$moveFailure = ""
if (-not $VerifyExistingRun) {
    New-Item -ItemType Directory -Path $runRoot | Out-Null
    try {
        foreach ($root in $roots) {
            $destinationParent = Split-Path -Parent $root.destination
            New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
            Move-Item -LiteralPath $root.source -Destination $root.destination
        }
    }
    catch {
        $moveFailure = $_.Exception.Message
    }
}

$differenceCount = 0
$receiptRoots = [System.Collections.Generic.List[object]]::new()
foreach ($root in $roots) {
    $expected = @($beforeByRoot[$root.id])
    $actualByPath = @{}
    $destinationPresent = Test-Path -LiteralPath $root.destination -PathType Container
    $destinationReparseEntries = 0
    if ($destinationPresent) {
        $destinationItem = Get-Item -LiteralPath $root.destination -Force
        if (($destinationItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            $destinationReparseEntries++
        }
        $destinationReparseEntries += @(
            Get-ChildItem -LiteralPath $root.destination -Force -Recurse |
                Where-Object { ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 }
        ).Count
        if ($destinationReparseEntries -ne 0) {
            $differenceCount += $destinationReparseEntries
        }
        Get-ChildItem -LiteralPath $root.destination -File -Force -Recurse | ForEach-Object {
            $relativePath = [System.IO.Path]::GetRelativePath($root.destination, $_.FullName).Replace('\', '/')
            $actualByPath[$relativePath] = [ordered]@{
                bytes = $_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        }
    }

    $matchingFiles = 0
    foreach ($expectedEntry in $expected) {
        if ($actualByPath.ContainsKey($expectedEntry.relativePath) -and
            $actualByPath[$expectedEntry.relativePath].bytes -eq $expectedEntry.bytes -and
            $actualByPath[$expectedEntry.relativePath].sha256 -eq $expectedEntry.sha256) {
            $matchingFiles++
        } else {
            $differenceCount++
        }
    }
    foreach ($actualPath in $actualByPath.Keys) {
        if (-not ($expected.relativePath -contains $actualPath)) {
            $differenceCount++
        }
    }

    [long]$expectedBytes = 0
    foreach ($expectedEntry in $expected) {
        $expectedBytes += [long]$expectedEntry.bytes
    }

    $receiptRoots.Add([ordered]@{
        id = $root.id
        projectPath = $root.projectPath
        recoveryRelativePath = $root.recoveryRelativePath
        files = $expected.Count
        bytes = $expectedBytes
        sourceAbsent = -not (Test-Path -LiteralPath $root.source)
        destinationPresent = $destinationPresent
        matchingFiles = $matchingFiles
        actualFiles = $actualByPath.Count
        reparseEntries = $destinationReparseEntries
    })
}

$sourceAbsenceVerified = @($receiptRoots | Where-Object { -not $_.sourceAbsent }).Count -eq 0
$destinationIntegrityVerified = $differenceCount -eq 0 -and
    @($receiptRoots | Where-Object { -not $_.destinationPresent -or $_.files -ne $_.matchingFiles }).Count -eq 0

[long]$totalFiles = 0
[long]$totalBytes = 0
[long]$totalMatchingFiles = 0
foreach ($receiptRoot in $receiptRoots) {
    $totalFiles += [long]$receiptRoot.files
    $totalBytes += [long]$receiptRoot.bytes
    $totalMatchingFiles += [long]$receiptRoot.matchingFiles
}

$manifestRelative = "Evidence/Session19/" + [System.IO.Path]::GetFileName($manifestPath)
$receiptRelative = "Evidence/Session19/" + [System.IO.Path]::GetFileName($receiptPath)
$receipt = [ordered]@{
    schema = "DiscGolfTour.Session19CharacterFrameworkExternalQuarantineReceipt.v1"
    schemaVersion = 1
    session = 19
    runId = $RunId
    verifiedUtc = [DateTime]::UtcNow.ToString("o")
    operation = "RECOVERABLE_MOVE_OUTSIDE_PROJECT"
    verificationRepair = [bool]$VerifyExistingRun
    moveCommandCompleted = if ($VerifyExistingRun) {
        $sourceAbsenceVerified -and $destinationIntegrityVerified
    } else {
        [string]::IsNullOrWhiteSpace($moveFailure)
    }
    state = if ($sourceAbsenceVerified -and $destinationIntegrityVerified) {
        "EXTERNAL_QUARANTINE_VERIFIED_SHIPPING_ABSENCE_PENDING"
    } else {
        "QUARANTINE_VERIFICATION_FAILED"
    }
    projectSourcesAbsent = $sourceAbsenceVerified
    destinationOutsideProject = $true
    destinationIntegrityVerified = $destinationIntegrityVerified
    deleted = $false
    hostPathRecorded = $false
    recoveryLocationToken = "DGTOUR_EXTERNAL_QUARANTINE/$RunId"
    beforeMoveManifest = [ordered]@{
        path = $manifestRelative
        bytes = (Get-Item -LiteralPath $manifestPath).Length
        sha256 = $manifestHash
        rows = $manifestRows.Count
    }
    roots = @($receiptRoots)
    totals = [ordered]@{
        files = [int]$totalFiles
        bytes = $totalBytes
        matchingFiles = [int]$totalMatchingFiles
        differenceCount = $differenceCount
    }
    releaseClosure = [ordered]@{
        projectRootAbsenceSatisfied = $sourceAbsenceVerified
        shippingPackageAbsenceSatisfied = $false
        blockerClosed = $false
        remainingEvidence = @("FRESH_WINDOWS_SHIPPING_UFS_NONUFS_AND_IOSTORE_ABSENCE")
    }
    receiptPath = $receiptRelative
}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8

if (-not $sourceAbsenceVerified -or -not $destinationIntegrityVerified) {
    if (-not [string]::IsNullOrWhiteSpace($moveFailure)) {
        throw "Framework quarantine move did not complete. A fail-closed recovery audit was written to $receiptPath."
    }
    throw "Framework quarantine moved data but failed the post-move integrity audit. Inspect $receiptPath."
}

Write-Output ($receipt | ConvertTo-Json -Depth 8)
