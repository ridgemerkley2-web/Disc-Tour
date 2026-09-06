#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.8",
    [string]$PackageRoot = "C:\DGTour_Packages",
    [string]$RunId = "",
    [switch]$PreflightOnly,
    [switch]$SelfTest
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

function New-ExclusiveUtf8StreamWriter {
    param([Parameter(Mandatory = $true)][string]$Path)

    $encoding = [System.Text.UTF8Encoding]::new($false)
    $stream = [System.IO.FileStream]::new(
        $Path,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::Read
    )
    try {
        return [System.IO.StreamWriter]::new($stream, $encoding)
    }
    catch {
        $stream.Dispose()
        throw
    }
}

function Write-ImmutableUtf8Json {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value,
        [int]$Depth = 8
    )

    $json = ($Value | ConvertTo-Json -Depth $Depth) + [Environment]::NewLine
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json)
    $stream = [System.IO.FileStream]::new(
        $Path,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None
    )
    try {
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
}

function Invoke-SelfTest {
    $state = [ordered]@{ Checks = 0; Failures = @() }
    function Expect-True {
        param([string]$Name, [bool]$Condition)
        $state.Checks++
        if (-not $Condition) {
            $state.Failures += $Name
        }
    }

    $fixtureRoot = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ("DGTourShippingRunnerSelfTest_{0}" -f [Guid]::NewGuid().ToString("N"))
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $logPath = Join-Path $fixtureRoot "Build.log"
    $receiptPath = Join-Path $fixtureRoot "Receipt.json"
    try {
        $writer = New-ExclusiveUtf8StreamWriter -Path $logPath
        try {
            $writer.WriteLine("first")
        }
        finally {
            $writer.Dispose()
        }
        Expect-True "exclusive log initial create" (
            [System.IO.File]::ReadAllText($logPath) -eq "first$([Environment]::NewLine)"
        )

        $logCollisionRejected = $false
        try {
            $collisionWriter = New-ExclusiveUtf8StreamWriter -Path $logPath
            $collisionWriter.Dispose()
        }
        catch [System.IO.IOException] {
            $logCollisionRejected = $true
        }
        Expect-True "exclusive log collision rejected" $logCollisionRejected

        $receipt = [ordered]@{ state = "BUILD_FAILED"; releaseReady = $false }
        Write-ImmutableUtf8Json -Path $receiptPath -Value $receipt
        $receiptHashBefore = (Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash
        Expect-True "immutable receipt initial create" (Test-Path -LiteralPath $receiptPath -PathType Leaf)

        $receiptCollisionRejected = $false
        try {
            Write-ImmutableUtf8Json -Path $receiptPath -Value ([ordered]@{ state = "OVERWRITE" })
        }
        catch [System.IO.IOException] {
            $receiptCollisionRejected = $true
        }
        $receiptHashAfter = (Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash
        Expect-True "immutable receipt collision rejected without mutation" (
            $receiptCollisionRejected -and $receiptHashBefore -eq $receiptHashAfter
        )
    }
    finally {
        foreach ($path in @($logPath, $receiptPath)) {
            if ([System.IO.File]::Exists($path)) {
                [System.IO.File]::Delete($path)
            }
        }
        if ([System.IO.Directory]::Exists($fixtureRoot)) {
            [System.IO.Directory]::Delete($fixtureRoot, $false)
        }
    }

    if ($state.Failures.Count -ne 0) {
        Write-Output "SESSION 19 SHIPPING CANDIDATE RUNNER SELF-TEST FAILED: $($state.Checks) cases"
        $state.Failures | ForEach-Object { Write-Output " - $_" }
        return 1
    }
    Write-Output "SESSION 19 SHIPPING CANDIDATE RUNNER SELF-TEST PASS: $($state.Checks)/$($state.Checks)"
    return 0
}

if ($SelfTest) {
    if ($PreflightOnly -or -not [string]::IsNullOrWhiteSpace($RunId)) {
        throw "SelfTest does not accept RunId or PreflightOnly."
    }
    $selfTestResult = @(Invoke-SelfTest)
    $selfTestResult | Select-Object -SkipLast 1 | ForEach-Object { Write-Output $_ }
    exit ([int]$selfTestResult[-1])
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$projectFile = Join-Path $projectRoot "DiscGolfTour.uproject"
$runUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$unrealPak = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealPak.exe"
$unrealEditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$protectedSave = Join-Path $projectRoot "Saved\SaveGames\DiscGolfTour_Profile_0.sav"
$evidenceRoot = Join-Path $projectRoot "Evidence\Session19"
$thirdPartyNoticeGenerator = Join-Path $projectRoot "Scripts\generate_dg_session19_third_party_notices.py"
$thirdPartyAuthorityPath = Join-Path $evidenceRoot "UE58RuntimeThirdPartyLicenseAuthority.json"
$thirdPartyNoticeTemplatePath = Join-Path $evidenceRoot "UE58RuntimeThirdPartyNOTICES.txt"
$shippingPluginCapabilityValidator = Join-Path $projectRoot "Scripts\validate_dg_session19_shipping_plugin_capabilities.py"
$bakedPcgValidator = Join-Path $projectRoot "Scripts\validate_dg_session19_baked_pcg.py"
$shippingPcgSeparationValidator = Join-Path $projectRoot "Scripts\validate_dg_session19_shipping_pcg_separation.py"

Assert-NoReparsePointInExistingPath -Path $evidenceRoot -Label "EvidenceRoot" -AllowedReparsePaths @($projectRoot)

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Project descriptor is missing: $projectFile"
}
if (-not (Test-Path -LiteralPath $runUat -PathType Leaf)) {
    throw "RunUAT was not found under the requested engine root: $runUat"
}
if (-not (Test-Path -LiteralPath $unrealPak -PathType Leaf)) {
    throw "UnrealPak was not found under the requested engine root: $unrealPak"
}
if (-not (Test-Path -LiteralPath $unrealEditorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd was not found under the requested engine root: $unrealEditorCmd"
}
if (-not (Test-Path -LiteralPath $thirdPartyNoticeGenerator -PathType Leaf)) {
    throw "Third-party notice generator is missing: $thirdPartyNoticeGenerator"
}
if (-not (Test-Path -LiteralPath $shippingPluginCapabilityValidator -PathType Leaf)) {
    throw "Shipping plugin capability validator is missing: $shippingPluginCapabilityValidator"
}
if (-not (Test-Path -LiteralPath $bakedPcgValidator -PathType Leaf)) {
    throw "Baked-PCG candidate validator is missing: $bakedPcgValidator"
}
if (-not (Test-Path -LiteralPath $shippingPcgSeparationValidator -PathType Leaf)) {
    throw "Shipping PCG separation validator is missing: $shippingPcgSeparationValidator"
}
if (-not (Test-Path -LiteralPath $protectedSave -PathType Leaf)) {
    throw "Protected profile is missing: $protectedSave"
}
$pythonCommand = Get-Command python -ErrorAction Stop
$pythonExe = $pythonCommand.Source
& $pythonExe $shippingPluginCapabilityValidator --engine-root $EngineRoot
if ($LASTEXITCODE -ne 0) {
    throw "Shipping plugin capability exact descriptor exclusion preflight failed closed."
}
& $pythonExe $bakedPcgValidator
if ($LASTEXITCODE -ne 0) {
    throw "Baked-PCG source authority preflight failed closed."
}
& $pythonExe $shippingPcgSeparationValidator
if ($LASTEXITCODE -ne 0) {
    throw "Shipping/editor PCG compile and cook separation preflight failed closed."
}

$protectedSaveBefore = (Get-FileHash -LiteralPath $protectedSave -Algorithm SHA256).Hash
if ($protectedSaveBefore -ne "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14") {
    throw "Protected profile does not match the frozen Session 12-19 authority."
}

foreach ($relativePath in @(
    "Content\PN_interactiveSpruceForest",
    "Content\Stump_Scanned",
    "Content\WaterMaterials",
    "Plugins\DiscGolfCharacterFramework",
    "_BuildKit\DiscGolfCorePlayabilityKit_v1.5\Plugins\DiscGolfCharacterFramework"
)) {
    $candidate = Join-Path $projectRoot $relativePath
    if (Test-Path -LiteralPath $candidate) {
        throw "Externally quarantined release-excluded root unexpectedly exists in the project: $relativePath"
    }
}

$projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$legacyPluginEntry = @($projectDescriptor.Plugins | Where-Object { $_.Name -eq "DiscGolfCharacterFramework" })
if ($legacyPluginEntry.Count -ne 0) {
    throw "DiscGolfCharacterFramework must be absent from the project descriptor before a release candidate is built."
}
$foundationModule = @($projectDescriptor.Modules | Where-Object { $_.Name -eq "DiscGolfRuntimeFoundation" })
if ($foundationModule.Count -ne 1 -or $foundationModule[0].Type -ne "Runtime") {
    throw "DiscGolfRuntimeFoundation must be declared exactly once as a Runtime module."
}
$developerModule = @($projectDescriptor.Modules | Where-Object { $_.Name -eq "DiscGolfTourDeveloper" })
if ($developerModule.Count -ne 1 -or $developerModule[0].Type -ne "DeveloperTool") {
    throw "DiscGolfTourDeveloper must be declared exactly once as a DeveloperTool module."
}
if ($developerModule[0].PSObject.Properties.Name -notcontains "TargetConfigurationDenyList" -or
    @($developerModule[0].TargetConfigurationDenyList) -notcontains "Shipping") {
    throw "DiscGolfTourDeveloper must explicitly deny the Shipping configuration."
}

if ([string]::IsNullOrWhiteSpace($RunId)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ")
    $suffix = [Guid]::NewGuid().ToString("N").Substring(0, 12)
    $RunId = "S19_WindowsShipping_${stamp}_${suffix}"
}
if ($RunId -notmatch '^S19_WindowsShipping_[A-Za-z0-9_-]+$') {
    throw "RunId must start with S19_WindowsShipping_ and contain only letters, digits, underscore, or hyphen."
}

$packageRootFull = [System.IO.Path]::GetFullPath($PackageRoot)
$projectPrefix = $projectRoot.TrimEnd('\') + '\'
$packagePrefix = $packageRootFull.TrimEnd('\') + '\'
if ($packageRootFull.Equals($projectRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    $packageRootFull.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $projectRoot.StartsWith($packagePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "PackageRoot must be a separate location outside the project tree."
}
Assert-NoReparsePointInExistingPath -Path $packageRootFull -Label "PackageRoot"
$projectPhysical = Get-PhysicalPath -Path $projectRoot
$packagePhysical = Get-PhysicalPath -Path $packageRootFull
$projectPhysicalPrefix = $projectPhysical.TrimEnd('\') + '\'
$packagePhysicalPrefix = $packagePhysical.TrimEnd('\') + '\'
if ($packagePhysical.Equals($projectPhysical, [System.StringComparison]::OrdinalIgnoreCase) -or
    $packagePhysical.StartsWith($projectPhysicalPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $projectPhysical.StartsWith($packagePhysicalPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "PackageRoot must resolve to a separate physical location outside the project tree."
}
$archiveRoot = [System.IO.Path]::GetFullPath((Join-Path $packageRootFull $RunId))
if (-not $archiveRoot.StartsWith($packagePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Resolved archive path escaped the requested package root."
}
if (Test-Path -LiteralPath $archiveRoot) {
    throw "Fresh candidate archive already exists; choose a new RunId: $archiveRoot"
}

$buildLog = Join-Path $evidenceRoot ("ShippingBuildCookRun-{0}.log" -f $RunId)
$receiptPath = Join-Path $evidenceRoot ("ShippingCandidate-{0}.json" -f $RunId)
$candidateVerificationPath = Join-Path $evidenceRoot ("ShippingCandidateVerification-{0}.json" -f $RunId)
$thirdPartyVerificationPath = Join-Path $evidenceRoot ("ThirdPartyNoticeVerification-{0}.json" -f $RunId)
$shippingPluginCapabilityReceiptPath = Join-Path $evidenceRoot ("ShippingPluginCapability-{0}.json" -f $RunId)
$bakedPcgSourceClosurePath = Join-Path $evidenceRoot ("BakedPcgSourceClosurePreBuild-{0}.json" -f $RunId)
$bakedPcgCandidateReceiptPath = Join-Path $evidenceRoot ("BakedPcgCandidateVerification-{0}.json" -f $RunId)
$shippingPcgSeparationReceiptPath = Join-Path $evidenceRoot ("ShippingPcgSeparation-{0}.json" -f $RunId)
if ((Test-Path -LiteralPath $buildLog) -or
    (Test-Path -LiteralPath $receiptPath) -or
    (Test-Path -LiteralPath $candidateVerificationPath) -or
    (Test-Path -LiteralPath $thirdPartyVerificationPath) -or
    (Test-Path -LiteralPath $shippingPluginCapabilityReceiptPath) -or
    (Test-Path -LiteralPath $bakedPcgSourceClosurePath) -or
    (Test-Path -LiteralPath $bakedPcgCandidateReceiptPath) -or
    (Test-Path -LiteralPath $shippingPcgSeparationReceiptPath)) {
    throw "Fresh evidence paths already exist for this RunId."
}

if ($PreflightOnly) {
    Write-Output ([ordered]@{
        state = "PREFLIGHT_PASS_NO_BUILD_STARTED"
        runId = $RunId
        projectDescriptorCompatible = $true
        releaseExcludedRootsAbsent = $true
        protectedProfileSha256 = $protectedSaveBefore
        packageRootOutsideProject = $true
        evidencePathsFresh = $true
        candidateVerificationPathFresh = $true
        thirdPartyNoticeGeneratorPresent = $true
        shippingPluginCapabilityPreflightPass = $true
        bakedPcgSourceAuthorityPreflightPass = $true
        shippingPcgSeparationPreflightPass = $true
        bakedPcgSourceClosurePathFresh = $true
        bakedPcgCandidateReceiptPathFresh = $true
        shippingPcgSeparationReceiptPathFresh = $true
        buildStarted = $false
        filesystemMutated = $false
    } | ConvertTo-Json -Depth 3)
    return
}

New-Item -ItemType Directory -Path $archiveRoot | Out-Null
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null

& $pythonExe $bakedPcgValidator `
    --candidate-id $RunId `
    --source-closure-output $bakedPcgSourceClosurePath
if ($LASTEXITCODE -ne 0) {
    throw "Baked-PCG pre-build source closure generation failed closed."
}

$arguments = @(
    "BuildCookRun",
    "-project=$projectFile",
    "-noP4",
    "-platform=Win64",
    "-clientconfig=Shipping",
    "-build",
    "-cook",
    "-stage",
    "-pak",
    "-iostore",
    "-archive",
    "-archivedirectory=$archiveRoot",
    "-clean",
    '-ubtargs=-NoUBA -MaxParallelActions=4',
    "-distribution",
    "-nodebuginfo",
    "-utf8output"
)

$startedUtc = [DateTime]::UtcNow
$uatExitCode = -1
$caughtMessage = ""
$buildLogWriter = $null
$thirdPartyTechnicalCoveragePass = $false
$thirdPartyNoticeInstalled = $false
$shippingPluginCapabilityPass = $false
$bakedPcgSourceClosurePass = $true
$bakedPcgCandidateDataPass = $false
$shippingPcgSeparationPass = $false
try {
    $buildLogWriter = New-ExclusiveUtf8StreamWriter -Path $buildLog
    & $runUat @arguments 2>&1 | ForEach-Object {
        $line = [string]$_
        $buildLogWriter.WriteLine($line)
        Write-Output $line
    }
    $uatExitCode = $LASTEXITCODE
    if ($uatExitCode -eq 0) {
        $candidateWindowsRoot = Join-Path $archiveRoot "Windows"
        $candidateUfsManifest = Join-Path $candidateWindowsRoot "Manifest_UFSFiles_Win64.txt"
        if (-not (Test-Path -LiteralPath $candidateUfsManifest -PathType Leaf)) {
            throw "Shipping archive is missing the UFS manifest needed for plugin diagnostics."
        }

        & $pythonExe $thirdPartyNoticeGenerator `
            --engine-root $EngineRoot `
            --authority-output $thirdPartyAuthorityPath `
            --notices-output $thirdPartyNoticeTemplatePath
        if ($LASTEXITCODE -ne 0) {
            throw "Third-party license authority and notice generation failed."
        }

        $candidateNoticePath = Join-Path $candidateWindowsRoot "NOTICES.txt"
        $candidateNonUfsManifest = Join-Path $candidateWindowsRoot "Manifest_NonUFSFiles_Win64.txt"
        if (-not (Test-Path -LiteralPath $candidateWindowsRoot -PathType Container) -or
            -not (Test-Path -LiteralPath $candidateNonUfsManifest -PathType Leaf)) {
            throw "Shipping archive is missing the Windows root or NonUFS manifest needed for notice installation."
        }
        Copy-Item -LiteralPath $thirdPartyNoticeTemplatePath -Destination $candidateNoticePath -Force
        $noticeTimestamp = [DateTime]::ParseExact(
            "2000-01-01T00:00:00.000Z",
            "yyyy-MM-dd'T'HH:mm:ss.fff'Z'",
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal
        )
        [System.IO.File]::SetLastWriteTimeUtc($candidateNoticePath, $noticeTimestamp)

        $manifestEncoding = [System.Text.UTF8Encoding]::new($false)
        $manifestLines = [System.IO.File]::ReadAllLines($candidateNonUfsManifest, $manifestEncoding)
        $noticeRowIndexes = @(
            for ($index = 0; $index -lt $manifestLines.Count; ++$index) {
                if ($manifestLines[$index].StartsWith("NOTICES.txt`t", [System.StringComparison]::Ordinal)) {
                    $index
                }
            }
        )
        if ($noticeRowIndexes.Count -ne 1) {
            throw "NonUFS manifest must contain exactly one NOTICES.txt row before deterministic rebinding."
        }
        $manifestLines[$noticeRowIndexes[0]] = "NOTICES.txt`t2000-01-01T00:00:00.000Z"
        [System.IO.File]::WriteAllLines($candidateNonUfsManifest, $manifestLines, $manifestEncoding)
        $thirdPartyNoticeInstalled = $true

        & $pythonExe $thirdPartyNoticeGenerator `
            --engine-root $EngineRoot `
            --authority-output $thirdPartyAuthorityPath `
            --notices-output $thirdPartyNoticeTemplatePath `
            --archive $candidateWindowsRoot `
            --candidate-id $RunId `
            --verification-output $thirdPartyVerificationPath
        if ($LASTEXITCODE -ne 0) {
            throw "Staged runtime DLL or third-party notice verification failed closed."
        }
        $thirdPartyTechnicalCoveragePass = $true

        & $pythonExe $shippingPluginCapabilityValidator `
            --engine-root $EngineRoot `
            --archive $candidateWindowsRoot `
            --unrealpak $unrealPak `
            --manifest $candidateUfsManifest `
            --candidate-id $RunId `
            --output $shippingPluginCapabilityReceiptPath
        if ($LASTEXITCODE -ne 0) {
            throw "Editor/developer-only plugin descriptors or reviewed residual metadata survived the final archive payload."
        }
        $shippingPluginCapabilityPass = $true

        & $pythonExe $bakedPcgValidator `
            --candidate-id $RunId `
            --archive $candidateWindowsRoot `
            --source-closure-input $bakedPcgSourceClosurePath `
            --output $bakedPcgCandidateReceiptPath
        if ($LASTEXITCODE -ne 0) {
            throw "Candidate-bound baked-PCG authored-data or source-closure validation failed closed."
        }
        $bakedPcgCandidateDataPass = $true

        & $pythonExe $shippingPcgSeparationValidator `
            --archive $candidateWindowsRoot `
            --unrealpak $unrealPak `
            --unreal-editor-cmd $unrealEditorCmd `
            --candidate-id $RunId `
            --output $shippingPcgSeparationReceiptPath
        if ($LASTEXITCODE -ne 0) {
            throw "Fresh Shipping container, registry, cooked-asset, and binary PCG absence validation failed closed."
        }
        $shippingPcgSeparationPass = $true
    }
}
catch {
    $caughtMessage = $_.Exception.Message
}
finally {
    if ($null -ne $buildLogWriter) {
        $buildLogWriter.Flush()
        $buildLogWriter.Dispose()
    }
    $protectedSavePresentAfter = Test-Path -LiteralPath $protectedSave -PathType Leaf
    $protectedSaveAfter = if ($protectedSavePresentAfter) {
        (Get-FileHash -LiteralPath $protectedSave -Algorithm SHA256).Hash
    } else { "" }
    $savePreserved = $protectedSavePresentAfter -and $protectedSaveBefore -eq $protectedSaveAfter
    $finishedUtc = [DateTime]::UtcNow
    $archiveToken = "DGTOUR_PACKAGES/$RunId"
    $logRelative = "Evidence/Session19/" + [System.IO.Path]::GetFileName($buildLog)
    $receiptRelative = "Evidence/Session19/" + [System.IO.Path]::GetFileName($receiptPath)
    $candidateWindowsRoot = Join-Path $archiveRoot "Windows"
    $windowsArchivePresent = Test-Path -LiteralPath $candidateWindowsRoot -PathType Container
    $requiredArchiveRelativePaths = @(
        "DiscGolfTour.exe",
        "DiscGolfTour\Binaries\Win64\DiscGolfTour-Win64-Shipping.exe",
        "Manifest_UFSFiles_Win64.txt",
        "Manifest_NonUFSFiles_Win64.txt"
    )
    $missingArchiveFiles = @($requiredArchiveRelativePaths | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $candidateWindowsRoot $_) -PathType Leaf)
    })
    $requiredArchiveFilesPresent = $windowsArchivePresent -and $missingArchiveFiles.Count -eq 0

    $receipt = [ordered]@{
        schema = "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1"
        schemaVersion = 1
        session = 19
        runId = $RunId
        target = [ordered]@{
            platform = "Win64"
            configuration = "Shipping"
            milestone = "v0.5"
            holes = @(1, 2, 3)
        }
        invocation = [ordered]@{
            clean = $true
            iterativeCook = $false
            build = $true
            cook = $true
            stage = $true
            pak = $true
            ioStore = $true
            archive = $true
            distribution = $true
            debugInfo = $false
        }
        startedUtc = $startedUtc.ToString("o")
        finishedUtc = $finishedUtc.ToString("o")
        uatExitCode = $uatExitCode
        exception = $caughtMessage
        state = if ($uatExitCode -eq 0 -and $savePreserved -and $requiredArchiveFilesPresent -and
            $thirdPartyTechnicalCoveragePass -and $shippingPluginCapabilityPass -and
            $bakedPcgSourceClosurePass -and $bakedPcgCandidateDataPass -and
            $shippingPcgSeparationPass) {
            "BUILD_PASS_PROVENANCE_AND_ACCEPTANCE_PENDING"
        } else {
            "BUILD_FAILED"
        }
        archive = [ordered]@{
            hostPathRecorded = $false
            recoveryLocationToken = $archiveToken
            windowsArchivePresent = $windowsArchivePresent
            requiredFilesPresent = $requiredArchiveFilesPresent
            missingRequiredFiles = $missingArchiveFiles
        }
        buildLog = [ordered]@{
            path = $logRelative
            present = Test-Path -LiteralPath $buildLog -PathType Leaf
            bytes = if (Test-Path -LiteralPath $buildLog -PathType Leaf) {
                (Get-Item -LiteralPath $buildLog).Length
            } else { 0 }
            sha256 = if (Test-Path -LiteralPath $buildLog -PathType Leaf) {
                (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash
            } else { "" }
        }
        protectedProfile = [ordered]@{
            path = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
            presentBefore = $true
            presentAfter = $protectedSavePresentAfter
            beforeSha256 = $protectedSaveBefore
            afterSha256 = $protectedSaveAfter
            preserved = $savePreserved
        }
        thirdPartyRuntimeNotices = [ordered]@{
            technicalCoveragePass = $thirdPartyTechnicalCoveragePass
            stagedNoticeInstalled = $thirdPartyNoticeInstalled
            legalApproval = $false
            distributionClearance = $false
            policyPath = "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json"
            authority = [ordered]@{
                path = "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json"
                present = Test-Path -LiteralPath $thirdPartyAuthorityPath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $thirdPartyAuthorityPath -PathType Leaf) {
                    (Get-Item -LiteralPath $thirdPartyAuthorityPath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $thirdPartyAuthorityPath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $thirdPartyAuthorityPath -Algorithm SHA256).Hash
                } else { "" }
            }
            noticeTemplate = [ordered]@{
                path = "Evidence/Session19/UE58RuntimeThirdPartyNOTICES.txt"
                present = Test-Path -LiteralPath $thirdPartyNoticeTemplatePath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $thirdPartyNoticeTemplatePath -PathType Leaf) {
                    (Get-Item -LiteralPath $thirdPartyNoticeTemplatePath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $thirdPartyNoticeTemplatePath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $thirdPartyNoticeTemplatePath -Algorithm SHA256).Hash
                } else { "" }
            }
            candidateVerification = [ordered]@{
                path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($thirdPartyVerificationPath)
                present = Test-Path -LiteralPath $thirdPartyVerificationPath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $thirdPartyVerificationPath -PathType Leaf) {
                    (Get-Item -LiteralPath $thirdPartyVerificationPath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $thirdPartyVerificationPath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $thirdPartyVerificationPath -Algorithm SHA256).Hash
                } else { "" }
            }
            remainingDecision = "INDEPENDENT_LICENSE_REVIEW_AND_DISTRIBUTION_APPROVAL"
        }
        shippingPluginCapabilities = [ordered]@{
            exactDescriptorExclusionAuthorityPass = $true
            candidateZeroSurvivorPass = $shippingPluginCapabilityPass
            payloadAuthority = "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE"
            policyPath = "Config/DG_Session19ShippingPluginCapabilityPolicy.json"
            receipt = [ordered]@{
                path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($shippingPluginCapabilityReceiptPath)
                present = Test-Path -LiteralPath $shippingPluginCapabilityReceiptPath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $shippingPluginCapabilityReceiptPath -PathType Leaf) {
                    (Get-Item -LiteralPath $shippingPluginCapabilityReceiptPath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $shippingPluginCapabilityReceiptPath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $shippingPluginCapabilityReceiptPath -Algorithm SHA256).Hash
                } else { "" }
            }
            releaseReady = $false
        }
        bakedPcg = [ordered]@{
            state = if ($shippingPcgSeparationPass) {
                "PASS_CANDIDATE_BOUND_AUTHORED_DATA_AND_SHIPPING_PCG_ABSENCE_HUMAN_ACCEPTANCE_PENDING"
            } else {
                "SHIPPING_PCG_ABSENCE_NOT_PROVEN"
            }
            sourceClosurePrePostMatch = $bakedPcgSourceClosurePass -and $bakedPcgCandidateDataPass
            candidateBoundAuthoredDataPass = $bakedPcgCandidateDataPass
            freshShippingCookPcgAbsenceProven = $shippingPcgSeparationPass
            runtimeAuthoredSelectionProven = $false
            deterministicBakeSaveReopenProven = $false
            threeHoleCollisionVisualPerformanceAccepted = $false
            sourceClosureReceipt = [ordered]@{
                path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($bakedPcgSourceClosurePath)
                present = Test-Path -LiteralPath $bakedPcgSourceClosurePath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $bakedPcgSourceClosurePath -PathType Leaf) {
                    (Get-Item -LiteralPath $bakedPcgSourceClosurePath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $bakedPcgSourceClosurePath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $bakedPcgSourceClosurePath -Algorithm SHA256).Hash
                } else { "" }
            }
            candidateReceipt = [ordered]@{
                path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($bakedPcgCandidateReceiptPath)
                present = Test-Path -LiteralPath $bakedPcgCandidateReceiptPath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $bakedPcgCandidateReceiptPath -PathType Leaf) {
                    (Get-Item -LiteralPath $bakedPcgCandidateReceiptPath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $bakedPcgCandidateReceiptPath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $bakedPcgCandidateReceiptPath -Algorithm SHA256).Hash
                } else { "" }
            }
            shippingPcgSeparationReceipt = [ordered]@{
                path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($shippingPcgSeparationReceiptPath)
                present = Test-Path -LiteralPath $shippingPcgSeparationReceiptPath -PathType Leaf
                bytes = if (Test-Path -LiteralPath $shippingPcgSeparationReceiptPath -PathType Leaf) {
                    (Get-Item -LiteralPath $shippingPcgSeparationReceiptPath).Length
                } else { 0 }
                sha256 = if (Test-Path -LiteralPath $shippingPcgSeparationReceiptPath -PathType Leaf) {
                    (Get-FileHash -LiteralPath $shippingPcgSeparationReceiptPath -Algorithm SHA256).Hash
                } else { "" }
            }
            blockerClosed = $false
            releaseReady = $false
        }
        nextRequiredEvidence = @(
            "WHOLE_ARCHIVE_AND_CONTAINER_PROVENANCE",
            "SHIPPING_FEATURE_AND_FORBIDDEN_CONTENT_ABSENCE",
            "FRESH_INSTALL_THREE_HOLE_ACCEPTANCE",
            "INDEPENDENT_LICENSE_REVIEW_AND_DISTRIBUTION_APPROVAL",
            "WHOLE_ARCHIVE_EXACT_IDENTITY_CLASSIFICATION"
        )
        releaseReady = $false
        receiptPath = $receiptRelative
    }
    Write-ImmutableUtf8Json -Path $receiptPath -Value $receipt

    if (-not $savePreserved) {
        throw "Protected profile changed during the Shipping build."
    }
}

if ($uatExitCode -ne 0) {
    if (-not [string]::IsNullOrWhiteSpace($caughtMessage)) {
        throw "Shipping BuildCookRun failed before a normal exit: $caughtMessage"
    }
    throw "Shipping BuildCookRun failed with exit code $uatExitCode."
}
if (-not $requiredArchiveFilesPresent) {
    throw "Shipping BuildCookRun exited successfully but the archive is incomplete: $($missingArchiveFiles -join ', ')"
}
if (-not $shippingPluginCapabilityPass) {
    if (-not [string]::IsNullOrWhiteSpace($caughtMessage)) {
        throw "Shipping build completed but plugin capability coverage failed: $caughtMessage"
    }
    throw "Shipping build completed but plugin capability zero-survivor coverage did not pass."
}
if (-not $thirdPartyTechnicalCoveragePass) {
    if (-not [string]::IsNullOrWhiteSpace($caughtMessage)) {
        throw "Shipping build completed but third-party notice coverage failed: $caughtMessage"
    }
    throw "Shipping build completed but third-party notice coverage did not pass."
}
if (-not $bakedPcgCandidateDataPass) {
    if (-not [string]::IsNullOrWhiteSpace($caughtMessage)) {
        throw "Shipping build completed but candidate-bound baked-PCG validation failed: $caughtMessage"
    }
    throw "Shipping build completed but candidate-bound baked-PCG validation did not pass."
}
if (-not $shippingPcgSeparationPass) {
    if (-not [string]::IsNullOrWhiteSpace($caughtMessage)) {
        throw "Shipping build completed but fresh Shipping PCG absence coverage failed: $caughtMessage"
    }
    throw "Shipping build completed but fresh Shipping PCG absence coverage did not pass."
}

Write-Output ([ordered]@{
    runId = $RunId
    archive = $archiveRoot
    log = $buildLog
    receipt = $receiptPath
    shippingPcgSeparationReceipt = $shippingPcgSeparationReceiptPath
} | ConvertTo-Json -Depth 3)
