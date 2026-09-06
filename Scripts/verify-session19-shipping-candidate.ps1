#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$RunId = "",
    [string]$PackageRoot = "C:\DGTour_Packages",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.8",
    [switch]$LegacyCandidate1Recovery,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ExpectedProtectedSaveSha256 = "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
$LegacyCandidate1RunId = "S19_WindowsShipping_20260825T032238Z_b969c38b32f5"
$LegacyMissingInnerPath = "DiscGolfTour\Binaries\Win64\DiscGolfTour.exe"
$DirectBuildState = "BUILD_PASS_PROVENANCE_AND_ACCEPTANCE_PENDING"
$VerificationPassState = "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
$BakedPcgCandidateState = "PASS_CANDIDATE_BOUND_AUTHORED_DATA_AND_SHIPPING_PCG_ABSENCE_HUMAN_ACCEPTANCE_PENDING"
$ShippingPcgSeparationState = "PASS_FRESH_SHIPPING_COOK_PCG_ABSENCE_HUMAN_GATES_PENDING"
$RequiredPreservedPluginDescriptors = @(
    "Engine/Plugins/Animation/ControlRigModules/ControlRigModules.uplugin",
    "Engine/Plugins/BaseMaterial/BaseMaterial.uplugin"
)
$RequiredAbsentPluginDescriptors = @(
    "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
    "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin"
)
$RequiredDependencyClosurePluginDescriptors = @(
    "Engine/Plugins/Editor/BlueprintMaterialTextureNodes/BlueprintMaterialTextureNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetDataflowNodes/ChaosClothAssetDataflowNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetEditorCore/ChaosClothAssetEditorCore.uplugin",
    "Engine/Plugins/Experimental/ChaosSolverPlugin/ChaosSolverPlugin.uplugin",
    "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin",
    "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin",
    "Engine/Plugins/Experimental/EditorDataStorageFeatures/EditorDataStorageFeatures.uplugin",
    "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin",
    "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin",
    "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
    "Engine/Plugins/MovieScene/LevelSequenceEditor/LevelSequenceEditor.uplugin",
    "Engine/Plugins/Animation/LiveLinkDevice/LiveLinkDevice.uplugin",
    "Engine/Plugins/Developer/PluginUtils/PluginUtils.uplugin",
    "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin",
    "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin"
)
$ReviewedMetadataOnlyDependencyPluginDescriptors = @(
    "Engine/Plugins/ChaosClothAssetDataflowNodes/ChaosClothAssetDataflowNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetEditorCore/ChaosClothAssetEditorCore.uplugin",
    "Engine/Plugins/Experimental/ChaosSolverPlugin/ChaosSolverPlugin.uplugin",
    "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin",
    "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin",
    "Engine/Plugins/Experimental/EditorDataStorageFeatures/EditorDataStorageFeatures.uplugin",
    "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin",
    "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin",
    "Engine/Plugins/MovieScene/LevelSequenceEditor/LevelSequenceEditor.uplugin",
    "Engine/Plugins/Animation/LiveLinkDevice/LiveLinkDevice.uplugin",
    "Engine/Plugins/Developer/PluginUtils/PluginUtils.uplugin",
    "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin",
    "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin"
)
$RequiredLogTokens = @(
    "BuildCookRun",
    "-platform=Win64",
    "-clientconfig=Shipping",
    "-build",
    "-cook",
    "-stage",
    "-pak",
    "-iostore",
    "-archive",
    "-clean",
    "-NoUBA",
    "MaxParallelActions=4",
    "-distribution",
    "IsIterativeCook=false",
    "BUILD SUCCESSFUL",
    "AutomationTool exiting with ExitCode=0"
)
$ForbiddenLogTokens = @(
    "-clientconfig=Development",
    "ClientConfigsToBuild=Development",
    "IsIterativeCook=true",
    "IterativeDeploy=True",
    "SkipCook=True",
    "Distribution=False",
    "BUILD FAILED"
)

function Test-ExactBoolean {
    param(
        [Parameter(Mandatory = $true)]$Value,
        [Parameter(Mandatory = $true)][bool]$Expected
    )
    return $Value -is [bool] -and $Value -eq $Expected
}

function Assert-DirectBuildReceipt {
    param(
        [Parameter(Mandatory = $true)]$Receipt,
        [Parameter(Mandatory = $true)][string]$ExpectedRunId
    )

    if ($Receipt.schema -ne "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1" -or
        $Receipt.schemaVersion -is [bool] -or [int64]$Receipt.schemaVersion -ne 1 -or
        $Receipt.session -is [bool] -or [int64]$Receipt.session -ne 19 -or
        $Receipt.runId -ne $ExpectedRunId -or
        $Receipt.uatExitCode -is [bool] -or [int64]$Receipt.uatExitCode -ne 0 -or
        $Receipt.state -ne $DirectBuildState -or
        -not (Test-ExactBoolean $Receipt.releaseReady $false)) {
        throw "Build receipt does not prove the exact direct Candidate 3 build result."
    }
    if ($Receipt.target.platform -ne "Win64" -or
        $Receipt.target.configuration -ne "Shipping" -or
        $Receipt.target.milestone -ne "v0.5" -or
        (@($Receipt.target.holes) -join ",") -ne "1,2,3") {
        throw "Build receipt target differs from the Windows Shipping three-hole contract."
    }
    foreach ($name in @("clean", "build", "cook", "stage", "pak", "ioStore", "archive", "distribution")) {
        if (-not (Test-ExactBoolean $Receipt.invocation.$name $true)) {
            throw "Build receipt invocation.$name must be boolean true."
        }
    }
    foreach ($name in @("iterativeCook", "debugInfo")) {
        if (-not (Test-ExactBoolean $Receipt.invocation.$name $false)) {
            throw "Build receipt invocation.$name must be boolean false."
        }
    }
    if (-not (Test-ExactBoolean $Receipt.archive.hostPathRecorded $false) -or
        $Receipt.archive.recoveryLocationToken -ne "DGTOUR_PACKAGES/$ExpectedRunId" -or
        -not (Test-ExactBoolean $Receipt.archive.windowsArchivePresent $true) -or
        -not (Test-ExactBoolean $Receipt.archive.requiredFilesPresent $true) -or
        @($Receipt.archive.missingRequiredFiles).Count -ne 0) {
        throw "Build receipt archive postconditions do not prove a complete direct candidate."
    }
    if (-not (Test-ExactBoolean $Receipt.protectedProfile.presentBefore $true) -or
        -not (Test-ExactBoolean $Receipt.protectedProfile.presentAfter $true) -or
        -not (Test-ExactBoolean $Receipt.protectedProfile.preserved $true) -or
        $Receipt.protectedProfile.beforeSha256 -ne $ExpectedProtectedSaveSha256 -or
        $Receipt.protectedProfile.afterSha256 -ne $ExpectedProtectedSaveSha256) {
        throw "Build receipt does not preserve the exact protected profile authority."
    }
    $expectedLogPath = "Evidence/Session19/ShippingBuildCookRun-$ExpectedRunId.log"
    $expectedReceiptPath = "Evidence/Session19/ShippingCandidate-$ExpectedRunId.json"
    if ($Receipt.buildLog.path -ne $expectedLogPath -or
        -not (Test-ExactBoolean $Receipt.buildLog.present $true) -or
        $Receipt.receiptPath -ne $expectedReceiptPath) {
        throw "Build receipt evidence paths differ from the immutable RunId contract."
    }
    $notices = $Receipt.thirdPartyRuntimeNotices
    if (-not (Test-ExactBoolean $notices.technicalCoveragePass $true) -or
        -not (Test-ExactBoolean $notices.stagedNoticeInstalled $true) -or
        -not (Test-ExactBoolean $notices.legalApproval $false) -or
        -not (Test-ExactBoolean $notices.distributionClearance $false) -or
        $notices.policyPath -ne "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json" -or
        $notices.authority.path -ne "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json" -or
        -not (Test-ExactBoolean $notices.authority.present $true) -or
        $notices.noticeTemplate.path -ne "Evidence/Session19/UE58RuntimeThirdPartyNOTICES.txt" -or
        -not (Test-ExactBoolean $notices.noticeTemplate.present $true) -or
        $notices.candidateVerification.path -ne
            "Evidence/Session19/ThirdPartyNoticeVerification-$ExpectedRunId.json" -or
        -not (Test-ExactBoolean $notices.candidateVerification.present $true)) {
        throw "Build receipt does not prove the exact technical third-party notice boundary."
    }
    $plugins = $Receipt.shippingPluginCapabilities
    if (-not (Test-ExactBoolean $plugins.exactDescriptorExclusionAuthorityPass $true) -or
        -not (Test-ExactBoolean $plugins.candidateZeroSurvivorPass $true) -or
        $plugins.policyPath -ne "Config/DG_Session19ShippingPluginCapabilityPolicy.json" -or
        $plugins.payloadAuthority -ne "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE" -or
        $plugins.receipt.path -ne
            "Evidence/Session19/ShippingPluginCapability-$ExpectedRunId.json" -or
        -not (Test-ExactBoolean $plugins.receipt.present $true) -or
        -not (Test-ExactBoolean $plugins.releaseReady $false)) {
        throw "Build receipt does not prove the exact Shipping plugin-capability boundary."
    }
    $bakedPcg = $Receipt.bakedPcg
    if ($bakedPcg.state -ne $BakedPcgCandidateState -or
        -not (Test-ExactBoolean $bakedPcg.sourceClosurePrePostMatch $true) -or
        -not (Test-ExactBoolean $bakedPcg.candidateBoundAuthoredDataPass $true) -or
        -not (Test-ExactBoolean $bakedPcg.freshShippingCookPcgAbsenceProven $true) -or
        -not (Test-ExactBoolean $bakedPcg.runtimeAuthoredSelectionProven $false) -or
        -not (Test-ExactBoolean $bakedPcg.deterministicBakeSaveReopenProven $false) -or
        -not (Test-ExactBoolean $bakedPcg.threeHoleCollisionVisualPerformanceAccepted $false) -or
        $bakedPcg.sourceClosureReceipt.path -ne
            "Evidence/Session19/BakedPcgSourceClosurePreBuild-$ExpectedRunId.json" -or
        -not (Test-ExactBoolean $bakedPcg.sourceClosureReceipt.present $true) -or
        $bakedPcg.candidateReceipt.path -ne
            "Evidence/Session19/BakedPcgCandidateVerification-$ExpectedRunId.json" -or
        -not (Test-ExactBoolean $bakedPcg.candidateReceipt.present $true) -or
        $bakedPcg.shippingPcgSeparationReceipt.path -ne
            "Evidence/Session19/ShippingPcgSeparation-$ExpectedRunId.json" -or
        -not (Test-ExactBoolean $bakedPcg.shippingPcgSeparationReceipt.present $true) -or
        -not (Test-ExactBoolean $bakedPcg.blockerClosed $false) -or
        -not (Test-ExactBoolean $bakedPcg.releaseReady $false)) {
        throw "Build receipt does not preserve the candidate-bound baked-PCG technical/pending boundary."
    }
}

function Assert-ShippingPcgSeparationReceipt {
    param(
        [Parameter(Mandatory = $true)]$Receipt,
        [Parameter(Mandatory = $true)][string]$ExpectedRunId
    )

    if ($Receipt.schema -ne "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1" -or
        $Receipt.schemaVersion -is [bool] -or [int64]$Receipt.schemaVersion -ne 1 -or
        $Receipt.session -is [bool] -or [int64]$Receipt.session -ne 19 -or
        $Receipt.candidateId -ne $ExpectedRunId -or
        $Receipt.state -ne $ShippingPcgSeparationState -or
        -not (Test-ExactBoolean $Receipt.session13BlockerClosed $false) -or
        -not (Test-ExactBoolean $Receipt.releaseReady $false)) {
        throw "Shipping PCG separation receipt identity/state differs."
    }
    if ($Receipt.source.runtimePcgModuleDependencyCount -is [bool] -or
        [int64]$Receipt.source.runtimePcgModuleDependencyCount -ne 0 -or
        $Receipt.source.runtimePcgGenerationSymbolCount -is [bool] -or
        [int64]$Receipt.source.runtimePcgGenerationSymbolCount -ne 0 -or
        (@($Receipt.source.pcgPluginTargets) -join ",") -ne "Editor" -or
        -not (Test-ExactBoolean $Receipt.source.forestGraphPropertyEditorOnly $true) -or
        -not (Test-ExactBoolean $Receipt.source.shippingNeverCookGraphDirectory $true) -or
        -not (Test-ExactBoolean $Receipt.source.editorAuthoringControllerPresent $true) -or
        -not (Test-ExactBoolean $Receipt.source.editorCustomNodePresent $true) -or
        $Receipt.source.authoredTreePayload.treeCount -is [bool] -or
        [int64]$Receipt.source.authoredTreePayload.treeCount -ne 44 -or
        $Receipt.source.authoredTreePayload.bytes -is [bool] -or
        [int64]$Receipt.source.authoredTreePayload.bytes -ne 2982 -or
        $Receipt.source.authoredTreePayload.sha256 -ne
            "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6") {
        throw "Shipping PCG separation source facts differ."
    }
    $cook = $Receipt.cook
    foreach ($name in @(
        "projectPcgGraphEntryCount",
        "pcgModuleEntryCount",
        "archivePcgModuleFileCount",
        "assetRegistryProjectPcgGraphCount",
        "serializedProjectPcgMarkerFileCount",
        "shippingBinaryProjectPcgMarkerFileCount"
    )) {
        if ($cook.$name -is [bool] -or [int64]$cook.$name -ne 0) {
            throw "Shipping PCG separation cook.$name must be numeric zero."
        }
    }
    if (-not (Test-ExactBoolean $cook.archiveHostPathRecorded $false) -or
        -not (Test-ExactBoolean $cook.freshShippingCookAbsenceProven $true) -or
        $cook.archiveFileCount -is [bool] -or [int64]$cook.archiveFileCount -le 0 -or
        $cook.ioStoreChunkCount -is [bool] -or [int64]$cook.ioStoreChunkCount -le 0 -or
        $cook.extractedProjectCookedFileCount -is [bool] -or
        [int64]$cook.extractedProjectCookedFileCount -le 0) {
        throw "Shipping PCG separation cook inventory facts differ."
    }
    $expectedIdentityPaths = [ordered]@{
        pak = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak"
        utoc = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc"
        ucas = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas"
        shippingBinary = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
    }
    foreach ($name in $expectedIdentityPaths.Keys) {
        $record = $cook.containerIdentity.$name
        if ($record.path -ne $expectedIdentityPaths[$name] -or
            $record.bytes -is [bool] -or [int64]$record.bytes -le 0 -or
            $record.sha256 -notmatch '^[0-9A-F]{64}$') {
            throw "Shipping PCG separation container identity differs: $name"
        }
    }
    $claims = $Receipt.claimBoundary
    if (-not (Test-ExactBoolean $claims.sourceConfigSeparationProven $true) -or
        -not (Test-ExactBoolean $claims.freshShippingCookAbsenceProven $true)) {
        throw "Shipping PCG separation proven technical claim boundary differs."
    }
    foreach ($name in @(
        "unrealPcgBakeSaveReopenProven",
        "collisionVisualApproval",
        "performanceAcceptance",
        "soakAcceptance",
        "manualGameplayAcceptance",
        "humanPlayFeelApproval",
        "provenanceApproval",
        "legalApproval",
        "distributionClearance",
        "session13BlockerClosed",
        "releaseReady"
    )) {
        if (-not (Test-ExactBoolean $claims.$name $false)) {
            throw "Shipping PCG separation claimBoundary.$name must remain false."
        }
    }
}

function Assert-ShippingPluginCapabilityReceipt {
    param(
        [Parameter(Mandatory = $true)]$Receipt,
        [Parameter(Mandatory = $true)][string]$ExpectedRunId
    )

    $preservedIdentities = @($Receipt.requiredPreservedPlugins | ForEach-Object { $_.identity })
    $absentIdentities = @($Receipt.requiredAbsentPlugins | ForEach-Object { $_.identity })
    $dependencyClosureIdentities = @(
        $Receipt.requiredDependencyClosurePlugins | ForEach-Object { $_.identity }
    )
    $metadataOnlyDependencyIdentities = @(
        $Receipt.reviewedMetadataOnlyDependencyPlugins |
            ForEach-Object { $_.descriptor.path }
    )
    $metadataOnlyDependencySurvivors = @(
        $Receipt.finalPayload.reviewedMetadataOnlyDependencySurvivors | Sort-Object
    )
    $expectedMetadataOnlyDependencySurvivors = @(
        $ReviewedMetadataOnlyDependencyPluginDescriptors | Sort-Object
    )
    $invalidMetadataOnlyBindings = @(
        $Receipt.reviewedMetadataOnlyDependencyPlugins | Where-Object {
            -not (Test-ExactBoolean $_.metadataOnlyShippingException $true)
        }
    )
    $invalidDependencyClosureBindings = @(
        $Receipt.requiredDependencyClosurePlugins | Where-Object {
            $expectedMetadataOnly =
                $ReviewedMetadataOnlyDependencyPluginDescriptors -contains $_.identity
            -not (Test-ExactBoolean `
                $_.metadataOnlyShippingException $expectedMetadataOnly)
        }
    )
    $dependencyPayloadBindings = @(
        $Receipt.finalPayload.requiredDependencyClosurePayloadBindings
    )
    $dependencyPayloadBindingIdentities = @(
        $dependencyPayloadBindings | ForEach-Object { $_.identity }
    )
    $invalidDependencyPayloadBindings = @(
        $dependencyPayloadBindings | Where-Object {
            $sourceBytes = $_.installedSource.bytes
            $payloadBytes = $_.finalPayload.bytes
            $sourceSha256 = [string]$_.installedSource.sha256
            $payloadSha256 = [string]$_.finalPayload.sha256
            $_.installedSource.path -ne $_.identity -or
            $sourceBytes -is [bool] -or [int64]$sourceBytes -le 0 -or
            $sourceSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            $null -eq $_.finalPayload -or
            $payloadBytes -is [bool] -or [int64]$payloadBytes -le 0 -or
            $payloadSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            [int64]$sourceBytes -ne [int64]$payloadBytes -or
            $sourceSha256 -cne $payloadSha256 -or
            -not (Test-ExactBoolean $_.matchesInstalledSource $true)
        }
    )
    if ($Receipt.schema -ne
            "DiscGolfTour.Session19ShippingPluginCapabilityReceipt.v3" -or
        $Receipt.schemaVersion -is [bool] -or
        [int64]$Receipt.schemaVersion -ne 3 -or
        $Receipt.session -is [bool] -or
        [int64]$Receipt.session -ne 19 -or
        $Receipt.candidateId -ne $ExpectedRunId -or
        $Receipt.state -ne
            "PASS_REVIEWED_METADATA_ONLY_AND_DEPENDENCY_CLOSED_PLUGIN_DESCRIPTORS_IN_FINAL_PAYLOAD" -or
        $Receipt.reviewedDisabledPluginCount -is [bool] -or
        [int64]$Receipt.reviewedDisabledPluginCount -ne 31 -or
        $Receipt.reviewedMetadataOnlyDependencyPluginCount -is [bool] -or
        [int64]$Receipt.reviewedMetadataOnlyDependencyPluginCount -ne 13 -or
        ($metadataOnlyDependencyIdentities -join "|") -ne
            ($ReviewedMetadataOnlyDependencyPluginDescriptors -join "|") -or
        $invalidMetadataOnlyBindings.Count -ne 0 -or
        $Receipt.requiredDependencyClosurePluginCount -is [bool] -or
        [int64]$Receipt.requiredDependencyClosurePluginCount -ne 15 -or
        ($dependencyClosureIdentities -join "|") -ne
            ($RequiredDependencyClosurePluginDescriptors -join "|") -or
        $invalidDependencyClosureBindings.Count -ne 0 -or
        $Receipt.requiredPreservedPluginCount -is [bool] -or
        [int64]$Receipt.requiredPreservedPluginCount -ne 2 -or
        ($preservedIdentities -join "|") -ne ($RequiredPreservedPluginDescriptors -join "|") -or
        $Receipt.requiredAbsentPluginCount -is [bool] -or
        [int64]$Receipt.requiredAbsentPluginCount -ne 2 -or
        ($absentIdentities -join "|") -ne ($RequiredAbsentPluginDescriptors -join "|") -or
        $Receipt.reviewedResidualMetadataExclusionCount -is [bool] -or
        [int64]$Receipt.reviewedResidualMetadataExclusionCount -ne 2 -or
        $Receipt.finalPayload.authority -ne "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE" -or
        -not (Test-ExactBoolean $Receipt.finalPayload.archive.stableAcrossInventory $true) -or
        $Receipt.finalPayload.containerCount -is [bool] -or
        [int64]$Receipt.finalPayload.containerCount -le 0 -or
        $Receipt.finalPayload.pluginDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.pluginDescriptorCount -le 0 -or
        $Receipt.finalPayload.reviewedMetadataOnlyDependencySurvivorCount -is [bool] -or
        [int64]$Receipt.finalPayload.reviewedMetadataOnlyDependencySurvivorCount -ne 13 -or
        ($metadataOnlyDependencySurvivors -join "|") -ne
            ($expectedMetadataOnlyDependencySurvivors -join "|") -or
        $Receipt.finalPayload.unreviewedEditorDeveloperOnlySurvivorCount -is [bool] -or
        [int64]$Receipt.finalPayload.unreviewedEditorDeveloperOnlySurvivorCount -ne 0 -or
        @($Receipt.finalPayload.unreviewedEditorDeveloperOnlySurvivors).Count -ne 0 -or
        $Receipt.finalPayload.unclassifiedDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.unclassifiedDescriptorCount -ne 0 -or
        @($Receipt.finalPayload.unclassifiedDescriptors).Count -ne 0 -or
        $Receipt.finalPayload.reviewedResidualMetadataSurvivorCount -is [bool] -or
        [int64]$Receipt.finalPayload.reviewedResidualMetadataSurvivorCount -ne 0 -or
        @($Receipt.finalPayload.reviewedResidualMetadataSurvivors).Count -ne 0 -or
        $Receipt.finalPayload.requiredPreservedDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredPreservedDescriptorCount -ne 2 -or
        $Receipt.finalPayload.missingRequiredPreservedDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.missingRequiredPreservedDescriptorCount -ne 0 -or
        @($Receipt.finalPayload.missingRequiredPreservedDescriptors).Count -ne 0 -or
        $Receipt.finalPayload.requiredAbsentDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredAbsentDescriptorCount -ne 2 -or
        $Receipt.finalPayload.requiredAbsentSurvivorCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredAbsentSurvivorCount -ne 0 -or
        @($Receipt.finalPayload.requiredAbsentSurvivors).Count -ne 0 -or
        $Receipt.finalPayload.requiredDependencyClosureDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredDependencyClosureDescriptorCount -ne 15 -or
        $Receipt.finalPayload.missingRequiredDependencyClosureDescriptorCount -is [bool] -or
        [int64]$Receipt.finalPayload.missingRequiredDependencyClosureDescriptorCount -ne 0 -or
        @($Receipt.finalPayload.missingRequiredDependencyClosureDescriptors).Count -ne 0 -or
        $Receipt.finalPayload.requiredDependencyClosurePayloadBindingCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredDependencyClosurePayloadBindingCount -ne 15 -or
        $dependencyPayloadBindings.Count -ne 15 -or
        ($dependencyPayloadBindingIdentities -join "|") -ne
            ($RequiredDependencyClosurePluginDescriptors -join "|") -or
        $invalidDependencyPayloadBindings.Count -ne 0 -or
        $Receipt.finalPayload.requiredDependencyClosurePayloadMismatchCount -is [bool] -or
        [int64]$Receipt.finalPayload.requiredDependencyClosurePayloadMismatchCount -ne 0 -or
        @($Receipt.finalPayload.requiredDependencyClosurePayloadMismatches).Count -ne 0 -or
        $Receipt.finalPayload.mandatoryDependencyEdgeCount -is [bool] -or
        [int64]$Receipt.finalPayload.mandatoryDependencyEdgeCount -le 0 -or
        $Receipt.finalPayload.mandatoryDependencyGapCount -is [bool] -or
        [int64]$Receipt.finalPayload.mandatoryDependencyGapCount -ne 0 -or
        @($Receipt.finalPayload.mandatoryDependencyGaps).Count -ne 0 -or
        -not (Test-ExactBoolean $Receipt.finalPayload.dependencyClosureComplete $true) -or
        $Receipt.stagingManifestDiagnostics.descriptorCount -is [bool] -or
        [int64]$Receipt.stagingManifestDiagnostics.descriptorCount -le 0 -or
        -not (Test-ExactBoolean `
            $Receipt.stagingManifestDiagnostics.authoritativeForDistribution $false) -or
        -not (Test-ExactBoolean $Receipt.stagingManifestDiagnostics.diagnosticOnly $true) -or
        @($Receipt.reasonCodes).Count -ne 0 -or
        @($Receipt.issues).Count -ne 0 -or
        -not (Test-ExactBoolean $Receipt.releaseBoundary.technicalPluginDescriptorGatePass $true) -or
        -not (Test-ExactBoolean $Receipt.releaseBoundary.releaseReady $false) -or
        -not (Test-ExactBoolean $Receipt.releaseBoundary.blockerClosed $false) -or
        @($Receipt.releaseBoundary.remainingEvidence).Count -ne 0) {
        throw "Shipping plugin-capability receipt does not prove the exact preserved/absent, reviewed metadata-only, and dependency-closed boundary."
    }
}

function Assert-LegacyCandidate1Receipt {
    param(
        [Parameter(Mandatory = $true)]$Receipt,
        [Parameter(Mandatory = $true)][string]$ExpectedRunId
    )

    if ($ExpectedRunId -ne $LegacyCandidate1RunId -or
        $Receipt.runId -ne $LegacyCandidate1RunId -or
        $Receipt.uatExitCode -is [bool] -or [int64]$Receipt.uatExitCode -ne 0 -or
        $Receipt.state -ne "BUILD_FAILED" -or
        @($Receipt.archive.missingRequiredFiles).Count -ne 1 -or
        $Receipt.archive.missingRequiredFiles[0] -ne $LegacyMissingInnerPath -or
        -not (Test-ExactBoolean $Receipt.protectedProfile.preserved $true)) {
        throw "Legacy recovery is restricted to Candidate 1's exact known filename postcondition."
    }
}

function Assert-BuildLogContract {
    param([Parameter(Mandatory = $true)][string]$Text)

    $missing = @($RequiredLogTokens | Where-Object {
        $Text.IndexOf($_, [System.StringComparison]::OrdinalIgnoreCase) -lt 0
    })
    $forbidden = @($ForbiddenLogTokens | Where-Object {
        $Text.IndexOf($_, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
    })
    if ($missing.Count -ne 0 -or $forbidden.Count -ne 0) {
        throw "UAT log does not prove the clean non-iterative Shipping distribution build contract."
    }
}

function Write-ImmutableUtf8Json {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value
    )

    $payload = $Value | ConvertTo-Json -Depth 10
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None
    )
    try {
        $bytes = $utf8.GetBytes($payload + "`n")
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
}

function Invoke-SelfTest {
    $state = [pscustomobject]@{
        Checks = 0
        Failures = [System.Collections.Generic.List[string]]::new()
    }
    function Expect-Pass {
        param([string]$Name, [scriptblock]$Action)
        $state.Checks++
        try { & $Action } catch { $state.Failures.Add("$Name unexpectedly failed: $($_.Exception.Message)") }
    }
    function Expect-Fail {
        param([string]$Name, [scriptblock]$Action)
        $state.Checks++
        try { & $Action; $state.Failures.Add("$Name unexpectedly passed") } catch { }
    }

    $fixtureRunId = "S19_WindowsShipping_SELFTEST"
    $direct = [pscustomobject]@{
        schema = "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1"
        schemaVersion = 1
        session = 19
        runId = $fixtureRunId
        target = [pscustomobject]@{
            platform = "Win64"; configuration = "Shipping"; milestone = "v0.5"; holes = @(1, 2, 3)
        }
        invocation = [pscustomobject]@{
            clean = $true; iterativeCook = $false; build = $true; cook = $true
            stage = $true; pak = $true; ioStore = $true; archive = $true
            distribution = $true; debugInfo = $false
        }
        uatExitCode = 0
        state = $DirectBuildState
        archive = [pscustomobject]@{
            hostPathRecorded = $false
            recoveryLocationToken = "DGTOUR_PACKAGES/$fixtureRunId"
            windowsArchivePresent = $true
            requiredFilesPresent = $true
            missingRequiredFiles = @()
        }
        buildLog = [pscustomobject]@{
            path = "Evidence/Session19/ShippingBuildCookRun-$fixtureRunId.log"
            present = $true
        }
        protectedProfile = [pscustomobject]@{
            presentBefore = $true; presentAfter = $true
            beforeSha256 = $ExpectedProtectedSaveSha256
            afterSha256 = $ExpectedProtectedSaveSha256
            preserved = $true
        }
        thirdPartyRuntimeNotices = [pscustomobject]@{
            technicalCoveragePass = $true
            stagedNoticeInstalled = $true
            legalApproval = $false
            distributionClearance = $false
            policyPath = "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json"
            authority = [pscustomobject]@{
                path = "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json"
                present = $true
            }
            noticeTemplate = [pscustomobject]@{
                path = "Evidence/Session19/UE58RuntimeThirdPartyNOTICES.txt"
                present = $true
            }
            candidateVerification = [pscustomobject]@{
                path = "Evidence/Session19/ThirdPartyNoticeVerification-$fixtureRunId.json"
                present = $true
            }
        }
        shippingPluginCapabilities = [pscustomobject]@{
            exactDescriptorExclusionAuthorityPass = $true
            candidateZeroSurvivorPass = $true
            payloadAuthority = "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE"
            policyPath = "Config/DG_Session19ShippingPluginCapabilityPolicy.json"
            receipt = [pscustomobject]@{
                path = "Evidence/Session19/ShippingPluginCapability-$fixtureRunId.json"
                present = $true
            }
            releaseReady = $false
        }
        bakedPcg = [pscustomobject]@{
            state = $BakedPcgCandidateState
            sourceClosurePrePostMatch = $true
            candidateBoundAuthoredDataPass = $true
            freshShippingCookPcgAbsenceProven = $true
            runtimeAuthoredSelectionProven = $false
            deterministicBakeSaveReopenProven = $false
            threeHoleCollisionVisualPerformanceAccepted = $false
            sourceClosureReceipt = [pscustomobject]@{
                path = "Evidence/Session19/BakedPcgSourceClosurePreBuild-$fixtureRunId.json"
                present = $true
            }
            candidateReceipt = [pscustomobject]@{
                path = "Evidence/Session19/BakedPcgCandidateVerification-$fixtureRunId.json"
                present = $true
            }
            shippingPcgSeparationReceipt = [pscustomobject]@{
                path = "Evidence/Session19/ShippingPcgSeparation-$fixtureRunId.json"
                present = $true
            }
            blockerClosed = $false
            releaseReady = $false
        }
        releaseReady = $false
        receiptPath = "Evidence/Session19/ShippingCandidate-$fixtureRunId.json"
    }
    $clone = {
        param($Value)
        return ($Value | ConvertTo-Json -Depth 10 | ConvertFrom-Json)
    }

    Expect-Pass "direct receipt" { Assert-DirectBuildReceipt $direct $fixtureRunId }
    foreach ($mutation in @(
        { param($v) $v.state = "BUILD_FAILED" },
        { param($v) $v.runId = "S19_WindowsShipping_OTHER" },
        { param($v) $v.uatExitCode = 1 },
        { param($v) $v.invocation.iterativeCook = $true },
        { param($v) $v.archive.missingRequiredFiles = @("missing") },
        { param($v) $v.protectedProfile.preserved = $false },
        { param($v) $v.target.holes = @(1, 2) },
        { param($v) $v.releaseReady = $true },
        { param($v) $v.thirdPartyRuntimeNotices.technicalCoveragePass = $false },
        { param($v) $v.thirdPartyRuntimeNotices.legalApproval = $true },
        { param($v) $v.shippingPluginCapabilities.candidateZeroSurvivorPass = $false },
        { param($v) $v.shippingPluginCapabilities.payloadAuthority = "STAGING_MANIFEST" },
        { param($v) $v.shippingPluginCapabilities.releaseReady = $true },
        { param($v) $v.bakedPcg.state = "PASS" },
        { param($v) $v.bakedPcg.sourceClosurePrePostMatch = $false },
        { param($v) $v.bakedPcg.candidateBoundAuthoredDataPass = $false },
        { param($v) $v.bakedPcg.freshShippingCookPcgAbsenceProven = $false },
        { param($v) $v.bakedPcg.runtimeAuthoredSelectionProven = $true },
        { param($v) $v.bakedPcg.sourceClosureReceipt.path = "Evidence/Session19/Wrong.json" },
        { param($v) $v.bakedPcg.candidateReceipt.present = $false },
        { param($v) $v.bakedPcg.shippingPcgSeparationReceipt.present = $false },
        { param($v) $v.bakedPcg.blockerClosed = $true }
    )) {
        $candidate = & $clone $direct
        & $mutation $candidate
        Expect-Fail "direct mutation" { Assert-DirectBuildReceipt $candidate $fixtureRunId }
    }

    $shippingPcg = [pscustomobject]@{
        schema = "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1"
        schemaVersion = 1
        session = 19
        candidateId = $fixtureRunId
        state = $ShippingPcgSeparationState
        source = [pscustomobject]@{
            runtimePcgModuleDependencyCount = 0
            runtimePcgGenerationSymbolCount = 0
            pcgPluginTargets = @("Editor")
            forestGraphPropertyEditorOnly = $true
            shippingNeverCookGraphDirectory = $true
            editorAuthoringControllerPresent = $true
            editorCustomNodePresent = $true
            authoredTreePayload = [pscustomobject]@{
                treeCount = 44
                bytes = 2982
                sha256 = "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6"
            }
        }
        cook = [pscustomobject]@{
            archiveHostPathRecorded = $false
            archiveFileCount = 10
            containerIdentity = [pscustomobject]@{
                pak = [pscustomobject]@{
                    path = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak"
                    bytes = 1
                    sha256 = "A" * 64
                }
                utoc = [pscustomobject]@{
                    path = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc"
                    bytes = 1
                    sha256 = "A" * 64
                }
                ucas = [pscustomobject]@{
                    path = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas"
                    bytes = 1
                    sha256 = "A" * 64
                }
                shippingBinary = [pscustomobject]@{
                    path = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
                    bytes = 1
                    sha256 = "A" * 64
                }
            }
            ioStoreChunkCount = 1
            projectPcgGraphEntryCount = 0
            pcgModuleEntryCount = 0
            archivePcgModuleFileCount = 0
            assetRegistryProjectPcgGraphCount = 0
            extractedProjectCookedFileCount = 1
            serializedProjectPcgMarkerFileCount = 0
            shippingBinaryProjectPcgMarkerFileCount = 0
            freshShippingCookAbsenceProven = $true
        }
        claimBoundary = [pscustomobject]@{
            sourceConfigSeparationProven = $true
            freshShippingCookAbsenceProven = $true
            unrealPcgBakeSaveReopenProven = $false
            collisionVisualApproval = $false
            performanceAcceptance = $false
            soakAcceptance = $false
            manualGameplayAcceptance = $false
            humanPlayFeelApproval = $false
            provenanceApproval = $false
            legalApproval = $false
            distributionClearance = $false
            session13BlockerClosed = $false
            releaseReady = $false
        }
        session13BlockerClosed = $false
        releaseReady = $false
    }
    Expect-Pass "Shipping PCG separation receipt" {
        Assert-ShippingPcgSeparationReceipt $shippingPcg $fixtureRunId
    }
    foreach ($mutation in @(
        { param($v) $v.schemaVersion = 2 },
        { param($v) $v.candidateId = "S19_WindowsShipping_OTHER" },
        { param($v) $v.state = "PASS" },
        { param($v) $v.source.runtimePcgModuleDependencyCount = 1 },
        { param($v) $v.source.runtimePcgGenerationSymbolCount = 1 },
        { param($v) $v.source.pcgPluginTargets = @("Editor", "Game") },
        { param($v) $v.source.authoredTreePayload.sha256 = "0" * 64 },
        { param($v) $v.cook.projectPcgGraphEntryCount = 1 },
        { param($v) $v.cook.pcgModuleEntryCount = 1 },
        { param($v) $v.cook.archivePcgModuleFileCount = 1 },
        { param($v) $v.cook.shippingBinaryProjectPcgMarkerFileCount = 1 },
        { param($v) $v.cook.freshShippingCookAbsenceProven = $false },
        { param($v) $v.cook.containerIdentity.utoc.sha256 = "bad" },
        { param($v) $v.claimBoundary.unrealPcgBakeSaveReopenProven = $true },
        { param($v) $v.claimBoundary.performanceAcceptance = $true },
        { param($v) $v.session13BlockerClosed = $true }
    )) {
        $candidate = & $clone $shippingPcg
        & $mutation $candidate
        Expect-Fail "Shipping PCG separation mutation" {
            Assert-ShippingPcgSeparationReceipt $candidate $fixtureRunId
        }
    }

    $plugin = [pscustomobject]@{
        schema = "DiscGolfTour.Session19ShippingPluginCapabilityReceipt.v3"
        schemaVersion = 3
        session = 19
        candidateId = $fixtureRunId
        state = "PASS_REVIEWED_METADATA_ONLY_AND_DEPENDENCY_CLOSED_PLUGIN_DESCRIPTORS_IN_FINAL_PAYLOAD"
        reviewedDisabledPluginCount = 31
        reviewedMetadataOnlyDependencyPluginCount = 13
        reviewedMetadataOnlyDependencyPlugins = @(
            $ReviewedMetadataOnlyDependencyPluginDescriptors | ForEach-Object {
                [pscustomobject]@{
                    name = [System.IO.Path]::GetFileNameWithoutExtension($_)
                    descriptor = [pscustomobject]@{ path = $_ }
                    moduleTypes = @("Editor")
                    capability = "EDITOR_DEVELOPER_ONLY"
                    metadataOnlyShippingException = $true
                }
            }
        )
        requiredDependencyClosurePluginCount = 15
        requiredDependencyClosurePlugins = @(
            $RequiredDependencyClosurePluginDescriptors | ForEach-Object {
                $metadataOnly = $ReviewedMetadataOnlyDependencyPluginDescriptors -contains $_
                [pscustomobject]@{
                    identity = $_
                    descriptor = [pscustomobject]@{ path = $_ }
                    moduleTypes = if ($metadataOnly) { @("Editor") } else { @("Runtime") }
                    capability = if ($metadataOnly) {
                        "EDITOR_DEVELOPER_ONLY"
                    }
                    else {
                        "RUNTIME_CAPABLE"
                    }
                    metadataOnlyShippingException = $metadataOnly
                }
            }
        )
        requiredPreservedPluginCount = 2
        requiredPreservedPlugins = @(
            [pscustomobject]@{ identity = $RequiredPreservedPluginDescriptors[0] },
            [pscustomobject]@{ identity = $RequiredPreservedPluginDescriptors[1] }
        )
        requiredAbsentPluginCount = 2
        requiredAbsentPlugins = @(
            [pscustomobject]@{ identity = $RequiredAbsentPluginDescriptors[0] },
            [pscustomobject]@{ identity = $RequiredAbsentPluginDescriptors[1] }
        )
        reviewedResidualMetadataExclusionCount = 2
        finalPayload = [pscustomobject]@{
            authority = "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE"
            archive = [pscustomobject]@{ stableAcrossInventory = $true }
            containerCount = 2
            pluginDescriptorCount = 17
            reviewedMetadataOnlyDependencySurvivorCount = 13
            reviewedMetadataOnlyDependencySurvivors = @(
                $ReviewedMetadataOnlyDependencyPluginDescriptors
            )
            unreviewedEditorDeveloperOnlySurvivorCount = 0
            unreviewedEditorDeveloperOnlySurvivors = @()
            unclassifiedDescriptorCount = 0
            unclassifiedDescriptors = @()
            reviewedResidualMetadataSurvivorCount = 0
            reviewedResidualMetadataSurvivors = @()
            requiredPreservedDescriptorCount = 2
            missingRequiredPreservedDescriptorCount = 0
            missingRequiredPreservedDescriptors = @()
            requiredAbsentDescriptorCount = 2
            requiredAbsentSurvivorCount = 0
            requiredAbsentSurvivors = @()
            requiredDependencyClosureDescriptorCount = 15
            missingRequiredDependencyClosureDescriptorCount = 0
            missingRequiredDependencyClosureDescriptors = @()
            requiredDependencyClosurePayloadBindingCount = 15
            requiredDependencyClosurePayloadBindings = @(
                $RequiredDependencyClosurePluginDescriptors | ForEach-Object {
                    [pscustomobject]@{
                        identity = $_
                        installedSource = [pscustomobject]@{
                            path = $_
                            bytes = 100
                            sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        }
                        finalPayload = [pscustomobject]@{
                            bytes = 100
                            sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        }
                        matchesInstalledSource = $true
                    }
                }
            )
            requiredDependencyClosurePayloadMismatchCount = 0
            requiredDependencyClosurePayloadMismatches = @()
            mandatoryDependencyEdgeCount = 20
            mandatoryDependencyGapCount = 0
            mandatoryDependencyGaps = @()
            dependencyClosureComplete = $true
        }
        stagingManifestDiagnostics = [pscustomobject]@{
            descriptorCount = 2
            authoritativeForDistribution = $false
            diagnosticOnly = $true
        }
        reasonCodes = @()
        issues = @()
        releaseBoundary = [pscustomobject]@{
            technicalPluginDescriptorGatePass = $true
            releaseReady = $false
            blockerClosed = $false
            remainingEvidence = @()
        }
    }
    Expect-Pass "plugin preserved/absent metadata-only dependency closure boundary" {
        Assert-ShippingPluginCapabilityReceipt $plugin $fixtureRunId
    }
    foreach ($mutation in @(
        { param($v) $v.schemaVersion = 2 },
        { param($v) $v.reviewedDisabledPluginCount = 44 },
        { param($v) $v.reviewedMetadataOnlyDependencyPluginCount = 12 },
        { param($v) $v.reviewedMetadataOnlyDependencyPlugins[0].descriptor.path = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.reviewedMetadataOnlyDependencyPlugins[0].metadataOnlyShippingException = $false },
        { param($v) $v.requiredDependencyClosurePluginCount = 14 },
        { param($v) $v.requiredDependencyClosurePlugins[0].identity = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.requiredDependencyClosurePlugins[0].metadataOnlyShippingException = $true },
        { param($v) $v.requiredPreservedPluginCount = 4 },
        { param($v) $v.requiredPreservedPlugins[0].identity = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.requiredAbsentPluginCount = 1 },
        { param($v) $v.requiredAbsentPlugins[1].identity = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.reviewedResidualMetadataExclusionCount = 1 },
        { param($v) $v.finalPayload.reviewedMetadataOnlyDependencySurvivorCount = 12 },
        { param($v) $v.finalPayload.reviewedMetadataOnlyDependencySurvivors[0] = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.finalPayload.unreviewedEditorDeveloperOnlySurvivorCount = 1 },
        { param($v) $v.finalPayload.unreviewedEditorDeveloperOnlySurvivors = @("Engine/Plugins/Wrong.uplugin") },
        { param($v) $v.finalPayload.requiredPreservedDescriptorCount = 4 },
        { param($v) $v.finalPayload.missingRequiredPreservedDescriptorCount = 1 },
        { param($v) $v.finalPayload.requiredAbsentDescriptorCount = 1 },
        { param($v) $v.finalPayload.requiredAbsentSurvivorCount = 1 },
        { param($v) $v.finalPayload.requiredAbsentSurvivors = @($RequiredAbsentPluginDescriptors[0]) },
        { param($v) $v.finalPayload.requiredDependencyClosureDescriptorCount = 14 },
        { param($v) $v.finalPayload.missingRequiredDependencyClosureDescriptorCount = 1 },
        { param($v) $v.finalPayload.missingRequiredDependencyClosureDescriptors = @("Engine/Plugins/Wrong.uplugin") },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadBindingCount = 14 },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadBindings[0].identity = "Engine/Plugins/Wrong.uplugin" },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadBindings[0].finalPayload.bytes = 99 },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadBindings[0].matchesInstalledSource = $false },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadMismatchCount = 1 },
        { param($v) $v.finalPayload.requiredDependencyClosurePayloadMismatches = @([pscustomobject]@{ identity = "Engine/Plugins/Wrong.uplugin" }) },
        { param($v) $v.finalPayload.mandatoryDependencyEdgeCount = 0 },
        { param($v) $v.finalPayload.mandatoryDependencyGapCount = 1 },
        { param($v) $v.finalPayload.mandatoryDependencyGaps = @([pscustomobject]@{ parentName = "A"; dependencyName = "B" }) },
        { param($v) $v.finalPayload.dependencyClosureComplete = $false },
        { param($v) $v.releaseBoundary.technicalPluginDescriptorGatePass = $false },
        { param($v) $v.releaseBoundary.releaseReady = $true }
    )) {
        $candidate = & $clone $plugin
        & $mutation $candidate
        Expect-Fail "plugin mutation" {
            Assert-ShippingPluginCapabilityReceipt $candidate $fixtureRunId
        }
    }

    $legacy = [pscustomobject]@{
        runId = $LegacyCandidate1RunId
        uatExitCode = 0
        state = "BUILD_FAILED"
        archive = [pscustomobject]@{ missingRequiredFiles = @($LegacyMissingInnerPath) }
        protectedProfile = [pscustomobject]@{ preserved = $true }
    }
    Expect-Pass "legacy Candidate1 receipt" {
        Assert-LegacyCandidate1Receipt $legacy $LegacyCandidate1RunId
    }
    Expect-Fail "legacy wrong RunId" {
        Assert-LegacyCandidate1Receipt $legacy "S19_WindowsShipping_OTHER"
    }
    $legacyWrong = & $clone $legacy
    $legacyWrong.archive.missingRequiredFiles = @("other")
    Expect-Fail "legacy wrong missing path" {
        Assert-LegacyCandidate1Receipt $legacyWrong $LegacyCandidate1RunId
    }

    $validLog = ($RequiredLogTokens -join "`n")
    Expect-Pass "valid Shipping log" { Assert-BuildLogContract $validLog }
    Expect-Fail "missing Shipping log marker" {
        Assert-BuildLogContract ($validLog.Replace("BUILD SUCCESSFUL", ""))
    }
    Expect-Fail "missing bounded UBT no-UBA marker" {
        Assert-BuildLogContract ($validLog.Replace("-NoUBA", ""))
    }
    Expect-Fail "missing bounded UBT parallelism marker" {
        Assert-BuildLogContract ($validLog.Replace("MaxParallelActions=4", ""))
    }
    Expect-Fail "forbidden Development log marker" {
        Assert-BuildLogContract ($validLog + "`n-clientconfig=Development")
    }

    if ($state.Failures.Count -ne 0) {
        Write-Output "SESSION 19 SHIPPING CANDIDATE VERIFIER SELF-TEST FAILED: $($state.Checks) cases"
        $state.Failures | ForEach-Object { Write-Output " - $_" }
        return 1
    }
    Write-Output "SESSION 19 SHIPPING CANDIDATE VERIFIER SELF-TEST PASS: $($state.Checks)/$($state.Checks)"
    return 0
}

if ($SelfTest) {
    if (-not [string]::IsNullOrWhiteSpace($RunId) -or $LegacyCandidate1Recovery) {
        throw "SelfTest does not accept RunId or LegacyCandidate1Recovery."
    }
    $selfTestResult = @(Invoke-SelfTest)
    $selfTestResult | Select-Object -SkipLast 1 | ForEach-Object { Write-Output $_ }
    exit ([int]$selfTestResult[-1])
}

if ([string]::IsNullOrWhiteSpace($RunId) -or
    $RunId -notmatch '^S19_WindowsShipping_[A-Za-z0-9_-]+$') {
    throw "RunId must be a Session 19 Windows Shipping identifier."
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$packageRootFull = [System.IO.Path]::GetFullPath($PackageRoot)
$archiveRoot = [System.IO.Path]::GetFullPath((Join-Path $packageRootFull $RunId))
$windowsRoot = Join-Path $archiveRoot "Windows"
$packagePrefix = $packageRootFull.TrimEnd('\') + '\'
if (-not $archiveRoot.StartsWith($packagePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Resolved archive escaped PackageRoot."
}
if (-not (Test-Path -LiteralPath $windowsRoot -PathType Container)) {
    throw "Windows archive is missing."
}

$evidenceRoot = Join-Path $projectRoot "Evidence\Session19"
$buildLog = Join-Path $evidenceRoot ("ShippingBuildCookRun-{0}.log" -f $RunId)
$buildReceiptPath = Join-Path $evidenceRoot ("ShippingCandidate-{0}.json" -f $RunId)
$verificationPath = Join-Path $evidenceRoot ("ShippingCandidateVerification-{0}.json" -f $RunId)
foreach ($path in @($buildLog, $buildReceiptPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required build evidence is missing: $path"
    }
}
if (Test-Path -LiteralPath $verificationPath) {
    throw "Verification receipt already exists; evidence is immutable: $verificationPath"
}

$protectedSave = Join-Path $projectRoot "Saved\SaveGames\DiscGolfTour_Profile_0.sav"
if (-not (Test-Path -LiteralPath $protectedSave -PathType Leaf)) {
    throw "Protected profile is missing."
}
$saveHash = (Get-FileHash -LiteralPath $protectedSave -Algorithm SHA256).Hash
if ($saveHash -ne $ExpectedProtectedSaveSha256) {
    throw "Protected profile hash differs from the frozen authority."
}

foreach ($relativePath in @(
    "Content\PN_interactiveSpruceForest",
    "Content\Stump_Scanned",
    "Content\WaterMaterials",
    "Plugins\DiscGolfCharacterFramework",
    "_BuildKit\DiscGolfCorePlayabilityKit_v1.5\Plugins\DiscGolfCharacterFramework"
)) {
    if (Test-Path -LiteralPath (Join-Path $projectRoot $relativePath)) {
        throw "Release-excluded project root exists: $relativePath"
    }
}

$projectDescriptor = Get-Content -LiteralPath (Join-Path $projectRoot "DiscGolfTour.uproject") -Raw | ConvertFrom-Json
if (@($projectDescriptor.Plugins | Where-Object { $_.Name -eq "DiscGolfCharacterFramework" }).Count -ne 0) {
    throw "Legacy plugin remains in the project descriptor."
}

$requiredFiles = @(
    "DiscGolfTour.exe",
    "DiscGolfTour\Binaries\Win64\DiscGolfTour-Win64-Shipping.exe",
    "Manifest_UFSFiles_Win64.txt",
    "Manifest_NonUFSFiles_Win64.txt"
)
if (-not $LegacyCandidate1Recovery) {
    $requiredFiles += "NOTICES.txt"
}
$missingFiles = @($requiredFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $windowsRoot $_) -PathType Leaf)
})
if ($missingFiles.Count -ne 0) {
    throw "Archive is incomplete: $($missingFiles -join ', ')"
}

$buildReceipt = Get-Content -LiteralPath $buildReceiptPath -Raw | ConvertFrom-Json
$verificationMode = if ($LegacyCandidate1Recovery) {
    Assert-LegacyCandidate1Receipt $buildReceipt $RunId
    "LEGACY_CANDIDATE1_FILENAME_RECOVERY"
}
else {
    Assert-DirectBuildReceipt $buildReceipt $RunId
    "DIRECT_BUILD_PASS"
}

$logText = Get-Content -LiteralPath $buildLog -Raw
Assert-BuildLogContract $logText
$actualLogBytes = (Get-Item -LiteralPath $buildLog).Length
$actualLogSha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash
if (-not $LegacyCandidate1Recovery -and
    ($buildReceipt.buildLog.bytes -is [bool] -or
     [int64]$buildReceipt.buildLog.bytes -ne $actualLogBytes -or
     $buildReceipt.buildLog.sha256 -ne $actualLogSha256)) {
    throw "Build receipt log binding differs from the immutable UAT log."
}

$thirdPartyVerificationBinding = $null
$shippingPluginCapabilityBinding = $null
$bakedPcgVerificationBinding = $null
if (-not $LegacyCandidate1Recovery) {
    $noticeAuthorityPath = Join-Path $evidenceRoot "UE58RuntimeThirdPartyLicenseAuthority.json"
    $noticeTemplatePath = Join-Path $evidenceRoot "UE58RuntimeThirdPartyNOTICES.txt"
    $noticeVerificationPath = Join-Path $evidenceRoot ("ThirdPartyNoticeVerification-{0}.json" -f $RunId)
    foreach ($path in @($noticeAuthorityPath, $noticeTemplatePath, $noticeVerificationPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required third-party notice evidence is missing: $path"
        }
    }
    $noticeFields = $buildReceipt.thirdPartyRuntimeNotices
    foreach ($binding in @(
        @($noticeFields.authority, $noticeAuthorityPath),
        @($noticeFields.noticeTemplate, $noticeTemplatePath),
        @($noticeFields.candidateVerification, $noticeVerificationPath)
    )) {
        $record = $binding[0]
        $path = [string]$binding[1]
        if ($record.bytes -is [bool] -or
            [int64]$record.bytes -ne (Get-Item -LiteralPath $path).Length -or
            $record.sha256 -ne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) {
            throw "Third-party notice evidence binding differs from its immutable file."
        }
    }
    $noticeVerification = Get-Content -LiteralPath $noticeVerificationPath -Raw | ConvertFrom-Json
    if ($noticeVerification.schema -ne
            "DiscGolfTour.Session19RuntimeThirdPartyNoticeVerification.v1" -or
        $noticeVerification.candidateId -ne $RunId -or
        $noticeVerification.state -ne
            "PASS_TECHNICAL_NOTICE_COVERAGE_LEGAL_APPROVAL_PENDING" -or
        -not (Test-ExactBoolean $noticeVerification.notices.exactMatch $true) -or
        -not (Test-ExactBoolean $noticeVerification.notices.nonUfsManifestExactBinding $true) -or
        $noticeVerification.coverage.unmappedStagedDllIdentityCount -is [bool] -or
        [int64]$noticeVerification.coverage.unmappedStagedDllIdentityCount -ne 0 -or
        $noticeVerification.coverage.stagedDllIdentityCount -is [bool] -or
        [int64]$noticeVerification.coverage.stagedDllIdentityCount -le 0 -or
        -not (Test-ExactBoolean $noticeVerification.releaseBoundary.technicalCoverageOnly $true) -or
        -not (Test-ExactBoolean $noticeVerification.releaseBoundary.legalApproval $false) -or
        -not (Test-ExactBoolean $noticeVerification.releaseBoundary.distributionClearance $false) -or
        -not (Test-ExactBoolean $noticeVerification.releaseBoundary.releaseReady $false)) {
        throw "Third-party notice verification receipt does not prove the technical-only boundary."
    }
    $candidateNoticePath = Join-Path $windowsRoot "NOTICES.txt"
    if ((Get-FileHash -LiteralPath $candidateNoticePath -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $noticeTemplatePath -Algorithm SHA256).Hash) {
        throw "Staged NOTICES.txt differs from the deterministic notice template."
    }
    $noticeRows = @(
        Get-Content -LiteralPath (Join-Path $windowsRoot "Manifest_NonUFSFiles_Win64.txt") |
            Where-Object { $_.StartsWith("NOTICES.txt`t", [System.StringComparison]::Ordinal) }
    )
    if ($noticeRows.Count -ne 1 -or
        $noticeRows[0] -ne "NOTICES.txt`t2000-01-01T00:00:00.000Z") {
        throw "NonUFS manifest does not bind NOTICES.txt to the deterministic timestamp."
    }
    $thirdPartyVerificationBinding = [ordered]@{
        path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($noticeVerificationPath)
        bytes = (Get-Item -LiteralPath $noticeVerificationPath).Length
        sha256 = (Get-FileHash -LiteralPath $noticeVerificationPath -Algorithm SHA256).Hash
        technicalCoveragePass = $true
        legalApproval = $false
        distributionClearance = $false
    }

    $pluginReceiptPath = Join-Path $evidenceRoot (
        "ShippingPluginCapability-{0}.json" -f $RunId
    )
    if (-not (Test-Path -LiteralPath $pluginReceiptPath -PathType Leaf)) {
        throw "Required Shipping plugin-capability receipt is missing: $pluginReceiptPath"
    }
    $pluginFields = $buildReceipt.shippingPluginCapabilities
    if ($pluginFields.receipt.bytes -is [bool] -or
        [int64]$pluginFields.receipt.bytes -ne
            (Get-Item -LiteralPath $pluginReceiptPath).Length -or
        $pluginFields.receipt.sha256 -ne
            (Get-FileHash -LiteralPath $pluginReceiptPath -Algorithm SHA256).Hash) {
        throw "Shipping plugin-capability receipt binding differs from its immutable file."
    }
    $pluginReceipt = Get-Content -LiteralPath $pluginReceiptPath -Raw | ConvertFrom-Json
    Assert-ShippingPluginCapabilityReceipt $pluginReceipt $RunId
    $pluginValidator = Join-Path $projectRoot `
        "Scripts\validate_dg_session19_shipping_plugin_capabilities.py"
    $unrealPak = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealPak.exe"
    if (-not (Test-Path -LiteralPath $pluginValidator -PathType Leaf) -or
        -not (Test-Path -LiteralPath $unrealPak -PathType Leaf)) {
        throw "Plugin re-audit authority or UnrealPak is missing."
    }
    $pythonExe = (Get-Command python -ErrorAction Stop).Source
    $pluginManifestPath = Join-Path $windowsRoot "Manifest_UFSFiles_Win64.txt"
    $pluginReauditOutput = @(
        & $pythonExe $pluginValidator `
            --engine-root $EngineRoot `
            --archive $windowsRoot `
            --unrealpak $unrealPak `
            --manifest $pluginManifestPath `
            --candidate-id $RunId `
            --expected-receipt $pluginReceiptPath 2>&1
    )
    if ($LASTEXITCODE -ne 0) {
        throw "Live final archive plugin re-audit differs from its immutable receipt: $($pluginReauditOutput -join ' ')"
    }
    $shippingPluginCapabilityBinding = [ordered]@{
        path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($pluginReceiptPath)
        bytes = (Get-Item -LiteralPath $pluginReceiptPath).Length
        sha256 = (Get-FileHash -LiteralPath $pluginReceiptPath -Algorithm SHA256).Hash
        reviewedDisabledPluginDescriptorCount = 31
        reviewedMetadataOnlyDependencyPluginDescriptorCount = 13
        reviewedMetadataOnlyDependencySurvivorCount = 13
        unreviewedEditorDeveloperOnlySurvivorCount = 0
        reviewedResidualMetadataExclusionCount = 2
        requiredDependencyClosurePluginDescriptorCount = 15
        missingRequiredDependencyClosurePluginDescriptorCount = 0
        requiredDependencyClosurePayloadBindingCount = `
            [int64]$pluginReceipt.finalPayload.requiredDependencyClosurePayloadBindingCount
        requiredDependencyClosurePayloadMismatchCount = `
            [int64]$pluginReceipt.finalPayload.requiredDependencyClosurePayloadMismatchCount
        mandatoryDependencyEdgeCount = `
            [int64]$pluginReceipt.finalPayload.mandatoryDependencyEdgeCount
        mandatoryDependencyGapCount = 0
        dependencyClosureComplete = $true
        requiredPreservedRuntimeOrContentDescriptorCount = 2
        requiredAbsentShippingDeniedDescriptorCount = 2
        requiredAbsentShippingDeniedSurvivorCount = 0
        finalPayloadDescriptorCount = [int64]$pluginReceipt.finalPayload.pluginDescriptorCount
        stagingManifestDiagnosticDescriptorCount = `
            [int64]$pluginReceipt.stagingManifestDiagnostics.descriptorCount
        stagingManifestAuthoritativeForDistribution = $false
        finalArchiveCanonicalManifestSha256 = `
            $pluginReceipt.finalPayload.archive.canonicalManifestSha256
        technicalGatePass = $true
        releaseReady = $false
    }

    $bakedPcgValidator = Join-Path $projectRoot "Scripts\validate_dg_session19_baked_pcg.py"
    $bakedPcgSourceClosurePath = Join-Path $evidenceRoot (
        "BakedPcgSourceClosurePreBuild-{0}.json" -f $RunId
    )
    $bakedPcgCandidateReceiptPath = Join-Path $evidenceRoot (
        "BakedPcgCandidateVerification-{0}.json" -f $RunId
    )
    $shippingPcgSeparationValidator = Join-Path $projectRoot `
        "Scripts\validate_dg_session19_shipping_pcg_separation.py"
    $shippingPcgSeparationReceiptPath = Join-Path $evidenceRoot (
        "ShippingPcgSeparation-{0}.json" -f $RunId
    )
    foreach ($path in @(
        $bakedPcgValidator,
        $bakedPcgSourceClosurePath,
        $bakedPcgCandidateReceiptPath,
        $shippingPcgSeparationValidator,
        $shippingPcgSeparationReceiptPath
    )) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required baked-PCG validation authority is missing: $path"
        }
    }
    foreach ($binding in @(
        @($buildReceipt.bakedPcg.sourceClosureReceipt, $bakedPcgSourceClosurePath),
        @($buildReceipt.bakedPcg.candidateReceipt, $bakedPcgCandidateReceiptPath),
        @($buildReceipt.bakedPcg.shippingPcgSeparationReceipt, $shippingPcgSeparationReceiptPath)
    )) {
        $record = $binding[0]
        $path = [string]$binding[1]
        if ($record.bytes -is [bool] -or
            [int64]$record.bytes -ne (Get-Item -LiteralPath $path).Length -or
            $record.sha256 -ne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) {
            throw "Baked-PCG build-receipt binding differs from its immutable evidence."
        }
    }
    $bakedPcgReauditOutput = @(
        & $pythonExe $bakedPcgValidator `
            --candidate-id $RunId `
            --archive $windowsRoot `
            --source-closure-input $bakedPcgSourceClosurePath `
            --expected-receipt $bakedPcgCandidateReceiptPath 2>&1
    )
    if ($LASTEXITCODE -ne 0) {
        throw "Live final archive baked-PCG re-audit differs from its immutable receipt: $($bakedPcgReauditOutput -join ' ')"
    }
    $bakedPcgReceipt = Get-Content -LiteralPath $bakedPcgCandidateReceiptPath -Raw | ConvertFrom-Json
    $shippingPcgSeparationReceipt = Get-Content -LiteralPath `
        $shippingPcgSeparationReceiptPath -Raw | ConvertFrom-Json
    Assert-ShippingPcgSeparationReceipt $shippingPcgSeparationReceipt $RunId
    $unrealEditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (-not (Test-Path -LiteralPath $unrealEditorCmd -PathType Leaf)) {
        throw "UnrealEditor-Cmd is missing for the fresh Shipping PCG absence re-audit."
    }
    $shippingPcgReauditOutput = @(
        & $pythonExe $shippingPcgSeparationValidator `
            --archive $windowsRoot `
            --unrealpak $unrealPak `
            --unreal-editor-cmd $unrealEditorCmd `
            --candidate-id $RunId `
            --expected-receipt $shippingPcgSeparationReceiptPath 2>&1
    )
    if ($LASTEXITCODE -ne 0) {
        throw "Live final archive Shipping PCG absence re-audit differs from its immutable receipt: $($shippingPcgReauditOutput -join ' ')"
    }
    $bakedPcgVerificationBinding = [ordered]@{
        path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($bakedPcgCandidateReceiptPath)
        bytes = (Get-Item -LiteralPath $bakedPcgCandidateReceiptPath).Length
        sha256 = (Get-FileHash -LiteralPath $bakedPcgCandidateReceiptPath -Algorithm SHA256).Hash
        state = $BakedPcgCandidateState
        archiveCanonicalManifestSha256 = $bakedPcgReceipt.archive.canonicalManifestSha256
        sourceClosurePrePostMatch = $true
        candidateBoundAuthoredDataPass = $true
        freshShippingCookPcgAbsenceProven = $true
        runtimeAuthoredSelectionProven = $false
        deterministicBakeSaveReopenProven = $false
        threeHoleCollisionVisualPerformanceAccepted = $false
        shippingPcgSeparationReceipt = [ordered]@{
            path = "Evidence/Session19/" +
                [System.IO.Path]::GetFileName($shippingPcgSeparationReceiptPath)
            bytes = (Get-Item -LiteralPath $shippingPcgSeparationReceiptPath).Length
            sha256 = (Get-FileHash -LiteralPath `
                $shippingPcgSeparationReceiptPath -Algorithm SHA256).Hash
            state = $ShippingPcgSeparationState
            containerIdentity = $shippingPcgSeparationReceipt.cook.containerIdentity
            sourceConfigSeparationProven = $true
            freshShippingCookAbsenceProven = $true
            unrealPcgBakeSaveReopenProven = $false
            humanGatesAccepted = $false
            releaseReady = $false
        }
        blockerClosed = $false
        releaseReady = $false
    }
}

$innerExeRelative = "DiscGolfTour\Binaries\Win64\DiscGolfTour-Win64-Shipping.exe"
$innerExe = Join-Path $windowsRoot $innerExeRelative
$receipt = [ordered]@{
    schema = "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
    schemaVersion = 2
    session = 19
    runId = $RunId
    verifiedUtc = [DateTime]::UtcNow.ToString("o")
    state = $VerificationPassState
    verificationMode = $verificationMode
    buildReceipt = [ordered]@{
        state = $buildReceipt.state
        uatExitCode = 0
        recoveryApplied = [bool]$LegacyCandidate1Recovery
        legacyRecoveryRestrictedToCandidate1 = $true
        path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($buildReceiptPath)
        bytes = (Get-Item -LiteralPath $buildReceiptPath).Length
        sha256 = (Get-FileHash -LiteralPath $buildReceiptPath -Algorithm SHA256).Hash
    }
    archiveContract = [ordered]@{
        recoveryLocationToken = "DGTOUR_PACKAGES/$RunId/Windows"
        hostPathRecorded = $false
        requiredFiles = @($requiredFiles | ForEach-Object { $_.Replace('\', '/') })
        allRequiredFilesPresent = $true
        ufsManifestBytes = (Get-Item -LiteralPath (Join-Path $windowsRoot "Manifest_UFSFiles_Win64.txt")).Length
        nonUfsManifestBytes = (Get-Item -LiteralPath (Join-Path $windowsRoot "Manifest_NonUFSFiles_Win64.txt")).Length
    }
    innerShippingExecutable = [ordered]@{
        relativePath = $innerExeRelative.Replace('\', '/')
        bytes = (Get-Item -LiteralPath $innerExe).Length
        sha256 = (Get-FileHash -LiteralPath $innerExe -Algorithm SHA256).Hash
    }
    buildLog = [ordered]@{
        path = "Evidence/Session19/" + [System.IO.Path]::GetFileName($buildLog)
        bytes = $actualLogBytes
        sha256 = $actualLogSha256
        requiredMarkersPresent = $true
        forbiddenMarkersAbsent = $true
    }
    thirdPartyRuntimeNotices = $thirdPartyVerificationBinding
    shippingPluginCapabilities = $shippingPluginCapabilityBinding
    bakedPcg = $bakedPcgVerificationBinding
    protectedProfile = [ordered]@{
        path = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
        sha256 = $saveHash
        preserved = $true
    }
    releaseExcludedProjectRootsAbsent = $true
    nextRequiredEvidence = @(
        "WHOLE_ARCHIVE_AND_CONTAINER_PROVENANCE",
        "SHIPPING_BINARY_POLICY",
        "SHIPPING_FEATURE_AND_FORBIDDEN_CONTENT_ABSENCE",
        "FRESH_INSTALL_THREE_HOLE_ACCEPTANCE"
    )
    releaseReady = $false
    receiptPath = "Evidence/Session19/" + [System.IO.Path]::GetFileName($verificationPath)
}
Write-ImmutableUtf8Json -Path $verificationPath -Value $receipt
Write-Output ($receipt | ConvertTo-Json -Depth 10)
