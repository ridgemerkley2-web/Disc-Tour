#!/usr/bin/env python3
"""Validate the additive Session 19 release-scope authority without approving release."""

from __future__ import annotations

import argparse
from collections import Counter
import csv
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any, Callable

import validate_dg_session19_production_motion as production_motion_validator
import validate_dg_session19_v05_equipment_shipping as equipment_validator


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONTRACT_PATH = Path("Config/DG_Session19ReleaseScopeContract.json")
HISTORICAL_CONTRACT_PROFILE = "HISTORICAL_RELEASE_SCOPE_SNAPSHOT_V3"
FRESH_CANDIDATE_CONTRACT_PROFILE = (
    "FRESH_CANDIDATE_RUNTIME_JOURNAL_OPERATIONAL_EVIDENCE_V1"
)
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
LOWER_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
V05_EQUIPMENT_CANDIDATE_POLICY_PATTERN = (
    "Config/DG_Session19V05EquipmentShippingTechnicalPolicy-{candidateId}.json"
)
V05_EQUIPMENT_RECEIPT_PATTERN = (
    "Evidence/Session19/V05EquipmentShippingTechnical-{candidateId}.json"
)

BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
    "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
    "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
    "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
    "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING",
]

EXPECTED_RELEASE_TARGET = {
    "platform": "Windows",
    "configuration": "Shipping",
    "milestone": "v0.5",
    "courseId": "PineRidgeChampionship",
    "layoutId": "Championship",
    "holeNumbers": [1, 2, 3],
    "freshInstallRequired": True,
    "cleanNonIterativeBuildCookStagePakIoStoreArchiveRequired": True,
}

EXPECTED_FEATURE_SCOPE = {
    "career": "EXCLUDED_FROM_V05_SHIPPING_TECHNICAL_EVIDENCE_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING",
    "aiOpponents": "EXCLUDED_FROM_V05_SHIPPING_TECHNICAL_EVIDENCE_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING",
    "throwLab": "DEVELOPMENT_ONLY_SHIPPING_TECHNICAL_EXCLUSION_PASS_PUBLIC_CLAIM_ACCEPTANCE_PENDING",
    "pcgShippingAuthority": (
        "DETERMINISTIC_AUTHORED_OUTPUT_CANDIDATE_SELECTION_AND_NO_PROJECT_GENERATION_"
        "INVOCATION_PASS_COOKED_RUNTIME_CONSUMER_UNREAL_BAKE_REOPEN_AND_PERFORMANCE_PENDING"
    ),
    "runtimePcgGenerationAllowed": False,
    "characterFramework": "CLEANROOM_SOURCE_RETAINED_ASSET_MIGRATION_AND_SHIPPING_ABSENCE_TECHNICAL_PASS_RIGHTS_AND_RUNTIME_ACCEPTANCE_PENDING",
    "unusedFabImports": "EXTERNAL_QUARANTINE_AND_SHIPPING_ABSENCE_TECHNICAL_PASS_PROVENANCE_AND_OWNER_CLOSURE_PENDING",
}

EXPECTED_QUARANTINE = {
    "projectRoots": [
        "Content/PN_interactiveSpruceForest",
        "Content/Stump_Scanned",
        "Content/WaterMaterials",
    ],
    "mustBeOutsideProjectRoot": True,
    "shippingCookAllowed": False,
    "hostPathsRecorded": False,
    "currentState": "EXTERNAL_QUARANTINE_AND_CURRENT_CANDIDATE_SHIPPING_ABSENCE_TECHNICAL_PASS_PROVENANCE_AND_OWNER_CLOSURE_PENDING",
    "externalQuarantineEvidenceAccepted": True,
    "shippingAbsenceEvidenceAccepted": True,
    "blockerClosed": False,
    "receipt": {
        "path": "Evidence/Session19/UnusedFabExternalQuarantineReceipt.json",
        "bytes": 2152,
        "sha256": "761D61CFC6B9CA9A19788F5959E42600ED26F6FE119F541CD09198AFF2B18C47",
    },
    "beforeMoveManifest": {
        "path": "Evidence/Session19/QuarantinedFabBeforeMove.tsv",
        "bytes": 83517,
        "sha256": "D9F5194FFF35B98C4E32DA39B88E838BA49BC8D86782D6DED371B1871192AF02",
        "rows": 533,
    },
}

EXPECTED_SHIPPING_POLICY_BINDING = {
    "path": "Config/DG_Session19ShippingContentPolicy.json",
    "bytes": 10490,
    "sha256": "2B807E7A18840B42319BBA55F48C44CB26DA8CBBF528A06B0C08176ACDBDF473",
    "validatorPath": "Scripts/validate_dg_session19_candidate_content.py",
    "validatorBytes": 65437,
    "validatorSha256": "70605633EAEDBBE230F86AC5C4B048501EE7DFEDF257D4E2A97BB17748860A22",
    "currentState": (
        "CURRENT_CANDIDATE_BOUNDED_STAGED_CONTENT_AND_EXTERNAL_TECHNICAL_PASS_"
        "MANUAL_GAMEPLAY_ACCEPTANCE_PENDING"
    ),
    "policyBound": True,
    "candidateContentReceiptAccepted": True,
    "freshUserDirTechnicalReceiptAccepted": True,
    "freshInstallGameplayAcceptanceAccepted": False,
    "closesReleaseBlocker": False,
}

EXPECTED_SHIPPING_PLUGIN_BINDING = {
    "policyPath": "Config/DG_Session19ShippingPluginCapabilityPolicy.json",
    "policyBytes": 8770,
    "policySha256": "26F140F20D651651DE38C8B17DF55C0BA5B0A068CEEB39552B2FA60F0ACD3D12",
    "policySchema": "DiscGolfTour.Session19ShippingPluginCapabilityPolicy.v3",
    "policySchemaVersion": 3,
    "validatorPath": "Scripts/validate_dg_session19_shipping_plugin_capabilities.py",
    "validatorBytes": 84859,
    "validatorSha256": "9630058B1E6EDCE020E0196E667504AC016C5B156DEF75A7BE561716285E2813",
    "runnerPath": "Scripts/run-session19-shipping-candidate.ps1",
    "runnerBytes": 30026,
    "runnerSha256": "BE49A5555D8C236BAFBAF624F8288DBA585DEA5D89EF35EDDB5BFFEBB9F9CDD7",
    "verifierPath": "Scripts/verify-session19-shipping-candidate.ps1",
    "verifierBytes": 66780,
    "verifierSha256": "622305F7A2DE4E832106912D475835D2C0E25FE36100308E38011980D9F371FE",
    "currentState": "CURRENT_CANDIDATE_TECHNICAL_PLUGIN_DESCRIPTOR_GATE_PASS",
    "finalPayloadAuthority": "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE",
    "reviewedDisabledPluginCount": 31,
    "reviewedMetadataOnlyDependencyPluginCount": 13,
    "requiredDependencyClosurePluginCount": 15,
    "requiredPreservedPluginCount": 2,
    "requiredAbsentPluginCount": 2,
    "requiredAbsentSurvivorCount": 0,
    "reviewedResidualMetadataExclusionCount": 2,
    "unreviewedEditorDeveloperOnlySurvivorCount": 0,
    "mandatoryDependencyGapCount": 0,
    "exactDescriptorExclusionAuthorityAccepted": True,
    "freshReviewedDependencyClosureReceiptAccepted": True,
    "releaseReady": False,
    "blockerClosed": False,
}

EXPECTED_REQUIRED_NEVER_COOK_VIRTUAL_ROOTS = [
    "/Engine/VREditor",
    "/Engine/EditorMaterials",
    "/Engine/EditorMeshes",
    "/Engine/EditorResources",
    "/SpeedTreeImporter",
    "/Landmass",
]

EXPECTED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES = [
    "ConcertMain",
    "ConcertSyncClient",
    "ConcertSyncCore",
    "ConcertSharedSlate",
    "AssetManagerEditor",
]

EXPECTED_SHIPPING_DENIED_PLUGINS = [
    "ChaosVD",
    "ConcertMain",
    "ConcertSyncClient",
    "ConcertSyncCore",
    "ConcertSharedSlate",
    "AssetManagerEditor",
    "ConsoleVariables",
    "FacialAnimation",
    "SpeedTreeImporter",
]

EXPECTED_EDITOR_ONLY_PLUGINS: list[str] = []
EXPECTED_SHIPPING_DEPENDENCY_ENABLED_PLUGINS = [
    "BlueprintMaterialTextureNodes",
    "EditorScriptingUtilities",
    "Landmass",
]
EXPECTED_RESIDUAL_PAK_EXCLUSIONS = [
    ".../Engine/Plugins/ChaosClothAssetEditorCore/Config/DefaultChaosClothAssetEditorCore.ini",
    ".../Engine/Plugins/Editor/EditorScriptingUtilities/Config/DefaultEditorScriptingUtilities.ini",
]
EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS = [
    "/Engine/Plugins/Experimental/Landmass/Binaries/",
    "/Engine/Plugins/Experimental/Landmass/Config/",
    "/Engine/Plugins/Experimental/Landmass/Content/",
    "/Engine/Plugins/Experimental/Landmass/Resources/",
    "/Engine/Plugins/Experimental/Landmass/Source/",
]
EXPECTED_REQUIRED_PRESENT_PLUGIN_DESCRIPTORS = [
    "Engine/Plugins/Animation/ControlRigModules/ControlRigModules.uplugin",
    "Engine/Plugins/BaseMaterial/BaseMaterial.uplugin",
]
EXPECTED_REQUIRED_ABSENT_PLUGIN_DESCRIPTORS = [
    "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
    "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin",
]
EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS = [
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
    "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin",
]
EXPECTED_REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS = [
    descriptor for descriptor in EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS
    if descriptor not in {
        "Engine/Plugins/Editor/BlueprintMaterialTextureNodes/BlueprintMaterialTextureNodes.uplugin",
        "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
    }
]
EXPECTED_METADATA_ONLY_FORBIDDEN_SUBDIRECTORIES = [
    "Binaries", "Config", "Content", "Resources", "Source",
]

EXPECTED_SHIPPING_PACKAGE_HARDENING_BINDING = {
    "defaultPakFileRules": {
        "path": "Config/DefaultPakFileRules.ini",
        "bytes": 3303,
        "sha256": "F9EBECA5023163592B8B5756925BA272E0D45EBCC0B2B5332B70F4D18FD71CB4",
    },
    "defaultGame": {
        "path": "Config/DefaultGame.ini",
        "bytes": 2891,
        "sha256": "304390E67BF6C8FE0BD8D3BE1D830B303F4FBB0ED26358D8B638F9C1CF6DF296",
    },
    "projectDescriptor": {
        "path": "DiscGolfTour.uproject",
        "bytes": 2975,
        "sha256": "D88EAF4CC35491DFE9E64EDD000E860996DCA3213690F6F94E491C24DC96BB38",
    },
    "shippingGameTarget": {
        "path": "Source/DiscGolfTour.Target.cs",
        "bytes": 1026,
        "sha256": "6930D056321E4ABBEE290A8CA20B9917B2CCAA8E270D039B01B34C2DD1827843",
    },
    "requiredNeverCookVirtualRoots": EXPECTED_REQUIRED_NEVER_COOK_VIRTUAL_ROOTS,
    "shippingIgnoredPluginDependencies": EXPECTED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES,
    "projectTargetConfigurationShippingDeniedPlugins": EXPECTED_SHIPPING_DENIED_PLUGINS,
    "projectEditorOnlyPlugins": EXPECTED_EDITOR_ONLY_PLUGINS,
    "projectShippingDependencyEnabledPlugins": EXPECTED_SHIPPING_DEPENDENCY_ENABLED_PLUGINS,
    "residualExactPakExclusions": EXPECTED_RESIDUAL_PAK_EXCLUSIONS,
    "shippingForbiddenPluginContentRoots": EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS,
    "requiredPresentFinalPayloadPluginDescriptors": EXPECTED_REQUIRED_PRESENT_PLUGIN_DESCRIPTORS,
    "requiredAbsentFinalPayloadPluginDescriptors": EXPECTED_REQUIRED_ABSENT_PLUGIN_DESCRIPTORS,
    "requiredDependencyClosureFinalPayloadDescriptors": EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS,
    "reviewedMetadataOnlyDependencyDescriptors": EXPECTED_REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS,
    "metadataOnlyDependencyForbiddenSubdirectories": EXPECTED_METADATA_ONLY_FORBIDDEN_SUBDIRECTORIES,
    "sourceAndConfigHardeningAccepted": True,
    "freshShippingPackageProofAccepted": True,
    "releaseReady": False,
}

EXPECTED_STAGED_PROVENANCE_BINDING = {
    "path": "Config/DG_Session19StagedProvenancePolicy.json",
    "bytes": 10071,
    "sha256": "7106CA1BE45D649DE5410B531DB3AE93062C739C4133C5055C8F55E9D8029AC7",
    "generatorPath": "Scripts/generate_dg_session19_staged_provenance.py",
    "generatorBytes": 95668,
    "generatorSha256": "36E3C59B80C271915FB596442EB26A7E3ACF9EC2ED47208EC2F64C7C7FD81734",
    "currentState": (
        "CURRENT_CANDIDATE_EXACT_PACKAGING_IDENTITY_AND_TECHNICAL_GENERATION_CLASS_"
        "7158_OF_7158_PASS_UNDERLYING_LICENSE_CONTENT_RIGHTS_INDEPENDENT_LEGAL_"
        "AND_DISTRIBUTION_REVIEW_PENDING"
    ),
    "negativeFixtureOnly": "S18_DEVELOPMENT_REJECTED",
    "passingReceiptAccepted": False,
    "blockerClosed": False,
}

CANDIDATE_ID_PLACEHOLDER = "{candidateId}"
EXPECTED_CURRENT_CANDIDATE_EVIDENCE = {
    "candidateId": CANDIDATE_ID_PLACEHOLDER,
    "currentState": (
        "TECHNICAL_CANDIDATE_EVIDENCE_PASS_"
        "PROVENANCE_LEGAL_GAMEPLAY_AND_OWNER_REVIEW_BLOCKED"
    ),
    "archiveRecoveryLocationToken": f"DGTOUR_PACKAGES/{CANDIDATE_ID_PLACEHOLDER}/Windows",
    "archiveCanonicalManifestSha256": (
        "1FBFC3D2AAB6E90F17D104CA9DCED6B4753EC8666E520DDACBF5AB5581CF9822"
    ),
    "buildLog": {
        "path": f"Evidence/Session19/ShippingBuildCookRun-{CANDIDATE_ID_PLACEHOLDER}.log",
        "bytes": 131652,
        "sha256": "C1B4281A0EA81D0486CA97CA2766E5C03A6BA957D0B1D59E56D8DEB96AA24CE3",
    },
    "buildReceipt": {
        "path": f"Evidence/Session19/ShippingCandidate-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 5090,
        "sha256": "A0B2BD77C4C57B134282747C3A8D302E5E4F8B16CFE3C77111AAB306ABBE4549",
        "schema": "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1",
        "schemaVersion": 1,
        "state": "BUILD_PASS_PROVENANCE_AND_ACCEPTANCE_PENDING",
        "uatExitCode": 0,
        "releaseReady": False,
    },
    "verificationReceipt": {
        "path": f"Evidence/Session19/ShippingCandidateVerification-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 6452,
        "sha256": "6D24E65950CF5E370831CC6849805D902F688D4C6B5C5A4F9A62388609635A50",
        "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
        "schemaVersion": 2,
        "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
        "releaseReady": False,
    },
    "pluginCapabilityReceipt": {
        "path": f"Evidence/Session19/ShippingPluginCapability-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 260168,
        "sha256": "7E9F853630FB96C46D8D757FC045F9419CDBD76167B711BA058B7D267DAE2B46",
        "schema": "DiscGolfTour.Session19ShippingPluginCapabilityReceipt.v3",
        "schemaVersion": 3,
        "state": (
            "PASS_REVIEWED_METADATA_ONLY_AND_DEPENDENCY_CLOSED_"
            "PLUGIN_DESCRIPTORS_IN_FINAL_PAYLOAD"
        ),
        "reviewedDisabledPluginCount": 31,
        "reviewedMetadataOnlyDependencyPluginCount": 13,
        "requiredDependencyClosurePluginCount": 15,
        "requiredPreservedPluginCount": 2,
        "requiredAbsentPluginCount": 2,
        "reviewedResidualMetadataExclusionCount": 2,
        "finalPayloadPluginDescriptorCount": 148,
        "unreviewedEditorDeveloperOnlySurvivorCount": 0,
        "missingRequiredDependencyClosurePluginCount": 0,
        "mandatoryDependencyGapCount": 0,
        "requiredAbsentSurvivorCount": 0,
        "reviewedResidualMetadataSurvivorCount": 0,
        "technicalGatePass": True,
        "releaseReady": False,
        "blockerClosed": False,
    },
    "binaryReceipt": {
        "path": f"Evidence/Session19/ShippingBinary-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 1346,
        "sha256": "6C216138F01D45E33DCAAEEB6F151F02977C40F106BA61D660AB9A34195A23F0",
        "schema": "DiscGolfTour.Session19ShippingBinaryEvidence.v1",
        "schemaVersion": 1,
        "state": "PASS_BINARY_MARKER_POLICY_ONLY",
        "executableSha256": (
            "29A8E1681AC09749AFD08FA691706E81BED0AB1ABC8AFF7F113A9E59B08B3BF2"
        ),
        "passed": True,
        "forbiddenMatchCount": 0,
        "releaseReady": False,
    },
    "contentReceipt": {
        "path": f"Evidence/Session19/CandidateContent-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 27368,
        "sha256": "566346F30D32DA7C17BFE9BE750C2FCFDE389564B8FA0E753D70C0F2E6D21377",
        "schema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
        "schemaVersion": 1,
        "state": "PASS_BOUNDED_STAGED_CONTENT_AUDIT",
        "archiveFileCount": 31,
        "archiveBytes": 1245992471,
        "auditedIdentityCount": 5616,
        "authoritativeIdentityCount": 2802,
        "issueCount": 0,
        "boundedStagedContentAuditPassed": True,
        "freshInstallGameplayAcceptancePerformed": False,
        "provenanceClassificationPerformed": False,
        "legalOrVisualApprovalPerformed": False,
        "releaseReady": False,
        "blockerClosed": False,
    },
    "thirdPartyNoticeReceipt": {
        "path": f"Evidence/Session19/ThirdPartyNoticeVerification-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 4816,
        "sha256": "E2D0C8A2A9581D90E3B22AC15E93C33F1FF75DF4297961EAD4FD1BCEBF521172",
        "schema": "DiscGolfTour.Session19RuntimeThirdPartyNoticeVerification.v1",
        "schemaVersion": 1,
        "state": "PASS_TECHNICAL_NOTICE_COVERAGE_LEGAL_APPROVAL_PENDING",
        "supportedDllIdentityCount": 13,
        "stagedDllIdentityCount": 13,
        "mappedStagedDllIdentityCount": 13,
        "unmappedStagedDllIdentityCount": 0,
        "technicalCoverageOnly": True,
        "legalApproval": False,
        "distributionClearance": False,
        "releaseReady": False,
    },
    "provenanceClassificationDraft": {
        "path": f"Evidence/Session19/StagedProvenanceClassificationDraft-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 3161688,
        "sha256": "686C0B878A5A3CCB3B14BEC22E36B160F7DCD10D6C68DADBE61BBAF740A47B25",
        "schema": "DiscGolfTour.Session19StagedProvenanceClassification.v1",
        "schemaVersion": 1,
        "state": "DRAFT_UNREVIEWED_NOT_SHIPPING_APPROVED",
        "recordCount": 7158,
        "reviewed": False,
        "shippingApproved": False,
    },
    "provenanceDraftAudit": {
        "path": f"Evidence/Session19/StagedProvenanceDraftAudit-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 3281530,
        "sha256": "82489811A4DE2C50133FB1ED3729959627CC319D3D122A638F0C7D767655DDCC",
        "schema": "DiscGolfTour.Session19StagedProvenanceReceipt.v1",
        "schemaVersion": 1,
        "status": "FAIL_CLOSED",
        "discoveredIdentityCount": 7158,
        "classifiedIdentityCount": 0,
        "unclassifiedIdentityCount": 7158,
        "reasonCodes": [
            "CLASSIFICATION_MANIFEST_MISSING",
            "UNCLASSIFIED_IDENTITY",
        ],
        "evidenceRole": "HISTORICAL_PRE_ATTRIBUTION_FAIL_CLOSED_SNAPSHOT",
        "technicalOriginSupersededBy": (
            f"Evidence/Session19/TechnicalOriginAttributionResolved-{CANDIDATE_ID_PLACEHOLDER}.json"
        ),
        "technicalInventoryComplete": False,
        "releaseReady": False,
        "releaseUseAllowed": False,
        "blockerClosed": False,
    },
    "freshUserDirValidator": {
        "path": "Scripts/validate_dg_session19_fresh_userdir.py",
        "bytes": 20821,
        "sha256": "D576CB74C8AA6F39E43A16CE4489AE88966B1042A2B47EFC3636C6E5FB9A38BB",
        "selfTestCaseCount": 18,
    },
    "freshUserDirReceipt": {
        "path": f"Evidence/Session19/FreshUserDir-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 2799,
        "sha256": "F7BE478D1EC968AAABB8E50F682EDA15CD197C6C2185973EFC3FC499DB3E3A34",
        "schema": "DiscGolfTour.Session19FreshUserDirValidation.v1",
        "schemaVersion": 1,
        "state": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        "observedFileCount": 4,
        "approvedFileCount": 4,
        "unknownFileCount": 0,
        "forbiddenArtifactCount": 0,
        "failureCount": 0,
        "technicalUserDirAcceptanceOnly": True,
        "gameplayAcceptanceClaimed": False,
        "releaseReadinessClaimed": False,
    },
    "technicalPackageEvidenceAccepted": True,
    "freshUserDirTechnicalAcceptanceAccepted": True,
    "freshInstallGameplayAcceptanceAccepted": False,
    "provenanceClassificationAccepted": False,
    "legalApproval": False,
    "distributionClearance": False,
    "manualOwnerReviewAccepted": False,
    "releaseReady": False,
    "releaseUseAllowed": False,
    "blockerClosed": False,
}

EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE = {
    "currentState": "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
    "policy": {
        "path": "Scripts/DG_Session19ExternalTechnicalEvidencePolicy.json",
        "bytes": 2968,
        "sha256": "8A57FB1BBF99BE2572BC1CC5F65451F047748419CD74335F00CE6A15F6B62054",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_external_technical_evidence.py",
        "bytes": 37674,
        "sha256": "AE9F1F08F905ADBB5D24F892C75CD5CE59FED5F910E333EC84F302A8919D5188",
    },
    "runner": {
        "path": "Scripts/run_dg_session19_external_technical_evidence.py",
        "bytes": 22743,
        "sha256": "295F9E3FC9BF5C08AA205FB1246718C4ED81B0B36B413518B65A679C92227D35",
    },
    "receipt": {
        "path": f"Evidence/Session19/ExternalTechnicalEvidence-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 800,
        "sha256": "0C23512F4E9B72DCCE05839AC0FB52016368D9A32B52BED0720B162E425287A8",
    },
    "technicalEvidenceAccepted": True,
    "screenshotCount": 10,
    "presentMonRowCount": 0,
    "manualGameplayAcceptance": False,
    "humanPlayFeelApproval": False,
    "visualProductApproval": False,
    "accessibilityApproval": False,
    "legalApproval": False,
    "releaseReady": False,
}

EXPECTED_V05_FEATURE_EXCLUSION = {
    "currentState": "PASS_CANDIDATE_BOUND_V05_CAREER_AI_THROW_LAB_EXCLUSION",
    "policy": {
        "path": "Config/DG_Session19V05CareerAiThrowLabShippingExclusionPolicy.json",
        "bytes": 6262,
        "sha256": "2B9810CF2B700DF6036EC6A31F3D229BDD198B293C2157D206F298548C6B7F8D",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_v05_feature_exclusion.py",
        "bytes": 38384,
        "sha256": "31911DAF54F41C732A92193D67E2F41D73E0A9F8637D52B07F2667ADC6464ECE",
    },
    "receipt": {
        "path": f"Evidence/Session19/V05CareerAiThrowLabExclusion-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 5275,
        "sha256": "13D40242775FBB037A07D2451FC0D914C4629E7F92141E766B4B5567387207D7",
    },
    "technicalExclusionAccepted": True,
    "publicCareerClaimAllowed": False,
    "publicAiOpponentClaimAllowed": False,
    "publicThrowLabClaimAllowed": False,
    "manualGameplayReviewComplete": False,
    "legalApproval": False,
    "distributionClearance": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_V05_EQUIPMENT_TECHNICAL = {
    "currentState": "PASS_CANDIDATE_BOUND_V05_EQUIPMENT_TECHNICAL_EVIDENCE",
    "policy": {
        "path": "Config/DG_Session19V05EquipmentShippingTechnicalPolicy.json",
        "bytes": 14801,
        "sha256": "B4C156119BC4425365FC31166FC05FCD3A1185B52B82D814785642F3D3D64022",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_v05_equipment_shipping.py",
        "bytes": 57859,
        "sha256": "9E59FCCDE2FDFF4094B017515FE18A36E998813CD5E489B6D84A17F3D517A454",
    },
    "receipt": {
        "path": f"Evidence/Session19/V05EquipmentShippingTechnical-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 20127,
        "sha256": "D9211C4EF858216DF6B17916DBE16110B2F6A2D2C96F3A930A05EDA5BF5F4F08",
    },
    "technicalEvidenceAccepted": True,
    "boundedCatalogAssetCount": 8,
    "calibrationRepeatCount": 5,
    "equipmentNameClearance": False,
    "manualPlayFeelApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "distributionClearance": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_PHYSICS_MEASURED_REFERENCE_PREPARATION = {
    "currentState": "PASS_PREDECLARED_CAPTURE_PROTOCOL_AND_EMPTY_TEMPLATE_RELEASE_REQUIRED_FAIL_CLOSED",
    "policy": {
        "path": "Config/DG_PhysicsMeasuredReferencePolicy.json",
        "bytes": 4589,
        "sha256": "19D7BB42DA929FD86E7C39391D8874F3CB8D84D358B122BE6204034B141DEA08",
    },
    "validator": {
        "path": "Scripts/validate_dg_physics_measured_reference.py",
        "bytes": 42802,
        "sha256": "DDE76C11073804CE05C58C2568D46E71A41D40A15F85C7FCBFDDF19C53ECAFBE",
    },
    "datasetTemplate": {
        "path": "Evidence/Session19/PhysicsMeasuredReferenceDataset.template.json",
        "bytes": 732,
        "sha256": "CBC678CCACDCF2B6487022466A020AB91ED386F6B5CDC7C8ACCF20AE2B54E018",
    },
    "equipmentCategoryCount": 5,
    "launchConditionCount": 4,
    "plannedMeasuredThrowCount": 100,
    "plannedRouteTelemetryAttemptCount": 60,
    "measuredThrowCount": 0,
    "acceptedMeasuredReferenceDataset": False,
    "physicsOwnerApproval": False,
    "playtestOwnerApproval": False,
    "productOwnerApproval": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL = {
    "currentState": (
        "PASS_BOUNDED_ENVIRONMENT_16_CATEGORY_CONTRACT_SLOT_PROXY_QUARANTINE_"
        "AND_SCREENSHOT_TECHNICAL_EVIDENCE_3_OF_16_BOUND_0_OF_16_READY"
    ),
    "technicalPlan": {
        "path": "Config/DG_Session10EnvironmentCategoryTechnicalPlan.json",
        "bytes": 21024,
        "sha256": "A4DCB8CD0D685A0DE31BFC8518C43AF783C68DCB2FD2A1D42037AD8CA8C12C8D",
    },
    "policy": {
        "path": "Config/DG_Session10EnvironmentShippingTechnicalPolicy.json",
        "bytes": 11465,
        "sha256": "293B0EF8327FCBB4AE2C84FBCF8CB84E6EE09F6A428500B1318966305DD30A1F",
    },
    "validator": {
        "path": "Scripts/validate_dg_session10_environment_shipping.py",
        "bytes": 69906,
        "sha256": "2B3FB45B571EC5175448DEAD73040E575AF6E3891ECCE644B5B614A16B22D259",
    },
    "receipt": {
        "path": f"Evidence/Session10/EnvironmentShippingTechnicalV2-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 12027,
        "sha256": "2F2564DA8AD8D786581A2E15E3D5636E23EFCF673A41825607BF7E9B442E0DC2",
    },
    "technicalEvidenceAccepted": True,
    "allSixteenCategoryTechnicalContractsAccepted": True,
    "productionBoundCategoryCount": 3,
    "productionReadyCategoryCount": 0,
    "dedicatedProxyCompleteCategoryCount": 0,
    "windVerifiedCategoryCount": 0,
    "lodOrNaniteVerifiedCategoryCount": 2,
    "allSixteenProductionSlotsBound": False,
    "allThreeHolesLiveCollisionExerciseAccepted": False,
    "actualRenderedRhiAccepted": False,
    "visualQualityAccepted": False,
    "performanceApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "ownerApproval": False,
    "releaseReady": False,
}

EXPECTED_SESSION12_PRESENTATION_TECHNICAL = {
    "currentState": "PASS_CANDIDATE_BOUND_SESSION12_PRESENTATION_TECHNICAL_WITH_SHIPPING_AUDIO_COOK",
    "audioEventCoverageManifest": {
        "path": "Config/DG_Session12AudioEventCoverage.json",
        "bytes": 9642,
        "sha256": "91723978D733BF895277EAE2F5369EB559F47D1CA841703A1418D1B37DB39982",
    },
    "policy": {
        "path": "Config/DG_Session19Session12PresentationTechnicalPolicy.json",
        "bytes": 16520,
        "sha256": "D899B6CFA78BBA82FC7848D6182444EAC9A0E4A82DBBDA49A7A01A27710A6BFA",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_session12_presentation_technical.py",
        "bytes": 81166,
        "sha256": "A83DFACA9DC7EC4017523AC3D5F386E975B082777D17AC93E4EB8E67CC8E73D4",
    },
    "receipt": {
        "path": f"Evidence/Session19/Session12PresentationTechnicalAudioCoverage-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 36776,
        "sha256": "B2AF7B7C6271794D19F4547BB41D4BA23B384D742403B4BAA6F5DB81C78870A0",
    },
    "shippingAudioCookReceipt": {
        "path": f"Evidence/Session19/ShippingAudioCook-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 29815,
        "sha256": "A9272AACB90977D5487C9D8418819973B32D1B47BE44DE2E886929753DFF180D",
    },
    "technicalEvidenceAccepted": True,
    "audioRuntimeHookCategoryCount": 12,
    "productionAudioAssetCategoryCount": 0,
    "unboundAudioAssetCategoryCount": 12,
    "intentionalSilenceApprovedCategoryCount": 0,
    "productionAudioAssetsPresent": False,
    "shippingAudioCookProven": True,
    "audibleRuntimePlaybackProven": False,
    "unsupportedDeviceCoverage": False,
    "manualCameraReplayAndMixApproval": False,
    "visualQualityApproval": False,
    "accessibilityApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "distributionClearance": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION = {
    "currentState": "PASS_EXACT_TECHNICAL_ATTRIBUTION_LEGAL_REVIEW_PENDING",
    "policy": {
        "path": "Config/DG_Session19TechnicalOriginAttributionPolicy.json",
        "bytes": 5465,
        "sha256": "78D9212A93D7F87786516A1F20B3C44655D8AD3B09B3FE89162703C3BAC03F68",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_technical_origin_attribution.py",
        "bytes": 57127,
        "sha256": "6FD8A067F4FE0E956ACB0D8E437E16A7FD6ED1EEAD59447A16265E5C763349E7",
    },
    "proposalGenerator": {
        "path": "Scripts/generate_dg_session19_technical_origin_proposal.py",
        "bytes": 37731,
        "sha256": "15383355C32F01220D91148F1085CA240EF1812856F222D24EF70E829E43F319",
    },
    "proposalValidator": {
        "path": "Scripts/validate_dg_session19_technical_origin_proposal.py",
        "bytes": 12253,
        "sha256": "323750279B14011809FF582E14C2408230591DA2D24D7AD4C245CBBB17A9C45E",
    },
    "proposal": {
        "path": f"Evidence/Session19/TechnicalOriginProposal-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 5242737,
        "sha256": "8F1203FB77EA6FB57BD6F02CCDC8FE74A90454EF6698B90C1E13D5DFC4FFBFA8",
    },
    "receipt": {
        "path": f"Evidence/Session19/TechnicalOriginAttributionResolved-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 6703741,
        "sha256": "FF2DABD455895198DA04D732838AD54250E0BDACC69F9E9A1334D70A6996586C",
    },
    "auditedIdentityCount": 7158,
    "authoritativelyAttributedTechnicalOriginCount": 7158,
    "unresolvedIdentityCount": 0,
    "unresolvedAnonymousChunkCount": 0,
    "unresolvedPathIdentityCount": 0,
    "independentReviewComplete": False,
    "legalReviewComplete": False,
    "distributionClearanceComplete": False,
    "shippingApprovalGranted": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE = {
    "currentState": "PASS_CANDIDATE_BOUND_FRESH_INSTALL_THREE_HOLE_TECHNICAL_ACCEPTANCE",
    "policy": {
        "path": "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicy.json",
        "bytes": 7441,
        "sha256": "798F6A229055CD72821D4755C059099211F7A366085129D5A119AEB82E1FC40C",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_three_hole_technical_acceptance.py",
        "bytes": 41569,
        "sha256": "EE90D0EC312F074545E61C2E23DD86215A3EF1B05046FCDC20B58172974B7578",
    },
    "receipt": {
        "path": f"Evidence/Session19/ThreeHoleTechnicalAcceptance-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 9354,
        "sha256": "551069BA4ED7FA5E3FEB7AA254725C91AC5E5FCEC3F9BD209831432E9855CD5B",
    },
    "technicalThreeHoleAcceptance": True,
    "completedHoleCount": 3,
    "visualQualityApproval": False,
    "accessibilityApproval": False,
    "humanPlayFeelApproval": False,
    "manualGameplayAcceptance": False,
    "productOwnerApproval": False,
    "performanceAcceptance": False,
    "soakAcceptance": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "distributionClearance": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_BAKED_PCG_BINDING = {
    "policyPath": "Config/DG_Session19BakedPcgPolicy.json",
    "policyBytes": 5509,
    "policySha256": "A40390DF6D0BE32537D5555C5E5238AB1147B1068C2BC70B337A5B5A50CBCE4B",
    "validatorPath": "Scripts/validate_dg_session19_baked_pcg.py",
    "validatorBytes": 57232,
    "validatorSha256": "38C66E12B3818A2A55440F87E3A05B036A69E63BC507A932DEECA834A17ADC62",
    "receiptPath": "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json",
    "receiptBytes": 2931,
    "receiptSha256": "4C7189B03C723A5906F284D5ECF7C7AEFB7EE5368F7E16B837CACF145C879B47",
    "sourceClosurePreBuildReceiptPath": f"Evidence/Session19/BakedPcgSourceClosurePreBuild-{CANDIDATE_ID_PLACEHOLDER}.json",
    "sourceClosurePreBuildReceiptBytes": 47676,
    "sourceClosurePreBuildReceiptSha256": "C8B5A948F1257ED544F300B035445B11367C52955E715FC0E2470B5328124B4D",
    "candidateReceiptPath": f"Evidence/Session19/BakedPcgCandidateVerification-{CANDIDATE_ID_PLACEHOLDER}.json",
    "candidateReceiptBytes": 4053,
    "candidateReceiptSha256": "F525135FD745C06B63FF67E49FA64F6076E5FD005157F68FE9FD56DF8E0BB1FB",
    "currentState": "PASS_CANDIDATE_BOUND_AUTHORED_DATA_RUNTIME_AND_BAKED_OUTPUT_ACCEPTANCE_PENDING",
    "sourceAuthorityAccepted": True,
    "shippingPackageEvidenceAccepted": False,
    "blockerClosed": False,
}

EXPECTED_BAKED_PCG_RUNTIME_CLOSURE = {
    "currentState": "PASS_RETIRED_LEGACY_RUNTIME_CLOSURE_AND_FRESH_SHIPPING_PCG_ABSENCE_HUMAN_GATES_PENDING",
    "policy": {
        "path": "Config/DG_Session19BakedPcgRuntimeClosurePolicy.json",
        "bytes": 4594,
        "sha256": "C46B1611CDC62559046D6C93657E8893C8E70F8E52F22FAF95B4706D11D2BCD7",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_baked_pcg_runtime_closure.py",
        "bytes": 15769,
        "sha256": "DEB6FBD8F9976BD5F7FF36DCC87B2BB5E4B6405F94FB3847721711118013EACB",
    },
    "shippingSeparationPolicy": {
        "path": "Config/DG_Session19ShippingPcgSeparationPolicy.json",
        "bytes": 3975,
        "sha256": "F57EEA493F93546E53AA7D74DF9D5743C1D9AFF20DBCCED318B768E9635C33E3",
    },
    "shippingSeparationValidator": {
        "path": "Scripts/validate_dg_session19_shipping_pcg_separation.py",
        "bytes": 31880,
        "sha256": "35BEB62EF0571DDBF190FD03A02F10C0B81266A420BC5F3641ED6CD80F8F2B92",
    },
    "shippingPcgSeparationReceipt": {
        "path": f"Evidence/Session19/ShippingPcgSeparation-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 2616,
        "sha256": "86273D5AA9AE2C0BB03BCF4C80147864E3E5AC4BD3870A07D19E89CDE365FB1C",
    },
    "legacyCandidateRuntimeClosureRetired": True,
    "sourceConfigSeparationProven": True,
    "freshShippingCookAbsenceProven": True,
    "unrealPcgBakeSaveReopen": False,
    "visualQualityApproval": False,
    "collisionVisualApproval": False,
    "performanceAcceptance": False,
    "soakAcceptance": False,
    "manualGameplayAcceptance": False,
    "humanPlayFeelApproval": False,
    "accessibilityApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "distributionClearance": False,
    "releaseApproval": False,
    "session13BlockerClosed": False,
    "releaseReady": False,
}

EXPECTED_PRODUCTION_MOTION_PREPARATION = {
    "currentState": "PASS_PRODUCTION_MOTION_RUNTIME_BINDING_AND_SHIPPING_COOK_TECHNICAL_HUMAN_GATES_PENDING",
    "policy": {
        "path": "Config/DG_Session19ProductionMotionAuthoringPolicy.json",
        "bytes": 18135,
        "sha256": "85BE2A3C5FE352491D6BF753F0B8FD8E08C3F4B8B1A2CA5A6782DA454F15614D",
    },
    "validator": {
        "path": "Scripts/validate_dg_session19_production_motion.py",
        "bytes": 35429,
        "sha256": "31E11AF20A212D3A8B6D0171799393C4AD54162E196FFAC6DA1386ABAA7A0F15",
    },
    "evidence": {
        "path": "Evidence/Session19/ProductionMotionTechnicalCandidateEvidence-ThrowPhysicsAudit-R2.json",
        "bytes": 7224,
        "sha256": "DC5D784D0ACEC48D48F0F3CE6700A825BE5037039F7D0FEEC21691816925CB98",
    },
    "shippingCookPresenceReceipt": {
        "path": f"Evidence/Session19/ProductionMotionCookPresence-{CANDIDATE_ID_PLACEHOLDER}.json",
        "bytes": 7794,
        "sha256": "E514F3A4AFB7CB2A47F8A543A3DF4967FBF7120768A65C8EA1D9D4395BEE42E9",
    },
    "motionFamilyContractCount": 3,
    "semanticPhaseCount": 8,
    "targetAssetCount": 7,
    "targetAssetPresentCount": 7,
    "protectedSyntheticAssetCount": 5,
    "protectedSyntheticAssetReuseCount": 0,
    "authoringPrerequisiteFileCount": 4,
    "authoringPrerequisiteFilesPresent": 4,
    "shippingRuntimeBindingActive": True,
    "shippingCookPresenceAccepted": True,
    "motionQualityAccepted": False,
    "discContactQualityAccepted": False,
    "performerReleaseAccepted": False,
    "legalApproval": False,
    "ownerApproval": False,
    "blockerClosed": False,
    "releaseReady": False,
}

EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING = {
    "policyPath": "Config/DG_Session19SerializedAssetMigrationPolicy.json",
    "policyBytes": 5365,
    "policySha256": "EA2B8D4232F787606BEDEDB84CFAB20E6EE7C3B91542BBBDE5E5D8A083A79139",
    "validatorPath": "Scripts/validate_dg_session19_serialized_asset_migration.py",
    "validatorBytes": 110997,
    "validatorSha256": "46ECB5BFCF5471ADF2633E4B65BBBA52506096FCF91B647124C2ED8ABCCE1EC5",
    "receiptPath": "Evidence/Session19/SerializedAssetMigrationReceipt.json",
    "receiptBytes": 5435,
    "receiptSha256": "AC1CB2B61D07850E60A40C0701D878F4D7AAFF95CF04B18C280DB418DC91B9A4",
    "inventoryPath": "Evidence/Session19/SerializedAssetMigrationInventory.tsv",
    "inventoryBytes": 10279,
    "inventorySha256": "1B7B853153ADD65924B9D090B91B219A225F8E327DD95617505B5DE95CDC953B",
    "migrationScriptPath": "Scripts/migrate_dg_session19_serialized_assets.py",
    "migrationScriptBytes": 15439,
    "migrationScriptSha256": "426CC6451F6535E4ACDD0DC0D804CF0D1FAA8B8CBB311A40BF2ABA3BD8A3450B",
    "candidateReceiptPath": f"Evidence/Session19/SerializedAssetMigrationShipping-{CANDIDATE_ID_PLACEHOLDER}.json",
    "candidateReceiptBytes": 6498,
    "candidateReceiptSha256": "6D382AD94BB4584494C6737F70A8AA9855C3798B71192F6A787DFAF6D35C98D5",
    "currentState": "PASS_CANDIDATE_BOUND_TECHNICAL_MIGRATION_SHIPPING_NON_DISTRIBUTION",
    "sourceAndRetainedAssetMigrationAccepted": True,
    "shippingPackageEvidenceAccepted": True,
    "legacySourceTreeRetainedPendingVerifiedQuarantine": True,
    "blockerClosed": False,
}

FRAMEWORK_QUARANTINE_RUN_ID = "S19_Framework_20260825T031617Z_c68ee2e4e6be"
FRAMEWORK_QUARANTINE_MANIFEST_PATH = (
    "Evidence/Session19/CharacterFrameworkBeforeMove-"
    f"{FRAMEWORK_QUARANTINE_RUN_ID}.tsv"
)
FRAMEWORK_QUARANTINE_RECEIPT_PATH = (
    "Evidence/Session19/CharacterFrameworkExternalQuarantine-"
    f"{FRAMEWORK_QUARANTINE_RUN_ID}.json"
)
FRAMEWORK_PROJECT_ROOTS = [
    "Plugins/DiscGolfCharacterFramework",
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/DiscGolfCharacterFramework",
]
EXPECTED_FRAMEWORK_QUARANTINE_BINDING = {
    "runId": FRAMEWORK_QUARANTINE_RUN_ID,
    "projectRoots": FRAMEWORK_PROJECT_ROOTS,
    "mustBeOutsideProjectRoot": True,
    "hostPathsRecorded": False,
    "currentState": "EXTERNAL_QUARANTINE_AND_CURRENT_CANDIDATE_SHIPPING_ABSENCE_TECHNICAL_PASS_RUNTIME_AND_RIGHTS_ACCEPTANCE_PENDING",
    "projectSourceRemovalAccepted": True,
    "shippingPackageAbsenceAccepted": True,
    "replacementRuntimeAcceptanceAccepted": False,
    "blockerClosed": False,
    "receipt": {
        "path": FRAMEWORK_QUARANTINE_RECEIPT_PATH,
        "bytes": 2254,
        "sha256": "7E01A86BCE40A53A4019F03FD8A5F041C57CC6343B6C1A057A470B2787551952",
    },
    "beforeMoveManifest": {
        "path": FRAMEWORK_QUARANTINE_MANIFEST_PATH,
        "bytes": 96660,
        "sha256": "EEF548159719E2DCF5A0966621DF1F6A1313EB1A2A62B526C952AE8A29E766D5",
        "rows": 513,
    },
}

EXPECTED_FRAMEWORK_QUARANTINE_RECEIPT = {
    "schema": "DiscGolfTour.Session19CharacterFrameworkExternalQuarantineReceipt.v1",
    "schemaVersion": 1,
    "session": 19,
    "runId": FRAMEWORK_QUARANTINE_RUN_ID,
    "verifiedUtc": "2026-08-25T03:18:34.7077150Z",
    "operation": "RECOVERABLE_MOVE_OUTSIDE_PROJECT",
    "verificationRepair": True,
    "moveCommandCompleted": True,
    "state": "EXTERNAL_QUARANTINE_VERIFIED_SHIPPING_ABSENCE_PENDING",
    "projectSourcesAbsent": True,
    "destinationOutsideProject": True,
    "destinationIntegrityVerified": True,
    "deleted": False,
    "hostPathRecorded": False,
    "recoveryLocationToken": (
        f"DGTOUR_EXTERNAL_QUARANTINE/{FRAMEWORK_QUARANTINE_RUN_ID}"
    ),
    "beforeMoveManifest": {
        "path": FRAMEWORK_QUARANTINE_MANIFEST_PATH,
        "bytes": 96660,
        "sha256": "EEF548159719E2DCF5A0966621DF1F6A1313EB1A2A62B526C952AE8A29E766D5",
        "rows": 513,
    },
    "roots": [
        {
            "id": "project_plugin",
            "projectPath": "Plugins/DiscGolfCharacterFramework",
            "recoveryRelativePath": "ProjectPlugin/DiscGolfCharacterFramework",
            "files": 417,
            "bytes": 106181459,
            "sourceAbsent": True,
            "destinationPresent": True,
            "matchingFiles": 417,
            "actualFiles": 417,
            "reparseEntries": 0,
        },
        {
            "id": "buildkit_plugin",
            "projectPath": (
                "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/"
                "DiscGolfCharacterFramework"
            ),
            "recoveryRelativePath": "BuildKitPlugin/DiscGolfCharacterFramework",
            "files": 96,
            "bytes": 183360,
            "sourceAbsent": True,
            "destinationPresent": True,
            "matchingFiles": 96,
            "actualFiles": 96,
            "reparseEntries": 0,
        },
    ],
    "totals": {
        "files": 513,
        "bytes": 106364819,
        "matchingFiles": 513,
        "differenceCount": 0,
    },
    "releaseClosure": {
        "projectRootAbsenceSatisfied": True,
        "shippingPackageAbsenceSatisfied": False,
        "blockerClosed": False,
        "remainingEvidence": [
            "FRESH_WINDOWS_SHIPPING_UFS_NONUFS_AND_IOSTORE_ABSENCE",
        ],
    },
    "receiptPath": FRAMEWORK_QUARANTINE_RECEIPT_PATH,
}

EXPECTED_MANUAL_REVIEW_ARTIFACTS = [
    {"path": "Config/DG_Session19ManualReleaseReviewPolicy.json", "bytes": 11742,
     "sha256": "B2E21CD147E4D23BAEB800CA1F635B6F8A365AC93805038C0698708261217FAC"},
    {"path": "Scripts/validate_dg_session19_manual_release_review.py", "bytes": 33359,
     "sha256": "7DCDDD769ADB953A739B7847E0772FF58D4910167B6A6919397422F4692D78B5"},
    {"path": "Docs/DG_SESSION19_MANUAL_RELEASE_REVIEW.md", "bytes": 11243,
     "sha256": "E3B2A85CE6CE4F5E56E6BA0B8AB1C437E9ADD1C28711802C1D414D97E40B49DC"},
    {"path": "Evidence/Session19/ManualReleaseReviewInventory.json", "bytes": 13286,
     "sha256": "0DBE2CFF60279E8EEE20AB7E69F759F45DFEB93037B04B7CD156BB4FCC9A7B73"},
    {"path": "Evidence/Session19/ManualReleaseDecisionRecord.template.json", "bytes": 1606,
     "sha256": "39D51D1E768D7E3E9764FB8217B7A1EF02D27A3507CABEF4574E7890A5DC870A"},
]
EXPECTED_MANUAL_REVIEW_BINDING = {
    "currentState": "PENDING_NAMED_HUMAN_DECISIONS_AND_MISSING_PRODUCTION_ARTIFACTS",
    "validatorStatus": "PASS_REVIEW_PACKET_VALID_HUMAN_APPROVALS_PENDING",
    "packetValid": True,
    "humanApprovalGateCount": 8,
    "humanApprovedGateCount": 0,
    "humanBlockedGateCount": 8,
    "humanApprovalAccepted": False,
    "blanketAuthorizationAcceptedAsApproval": False,
    "releaseReady": False,
    "artifacts": EXPECTED_MANUAL_REVIEW_ARTIFACTS,
}

EXPECTED_STRATEGIES: dict[str, tuple[str, str, list[str]]] = {
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED": (
        "MANUAL_APPROVAL_AND_CLEARANCE",
        "LOCK_AND_CLEAR_OR_REPLACE_THE_PUBLIC_TITLE",
        [
            "OWNER_APPROVED_PUBLIC_TITLE_RECORD",
            "LEGAL_CLEARANCE_RECORD",
            "SHIPPING_PACKAGE_AND_UI_TITLE_INVENTORY",
        ],
    ),
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE": (
        "MANUAL_APPROVAL_AND_CLEARANCE",
        "CLEAR_OR_REPLACE_EVERY_SHIPPING_EQUIPMENT_AND_COURSE_DISPLAY_NAME",
        [
            "APPROVED_SHIPPING_DISPLAY_NAME_INVENTORY",
            "LEGAL_CLEARANCE_OR_REPLACEMENT_RECORD",
            "SHIPPING_RUNTIME_AND_ASSET_SCAN",
        ],
    ),
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED": (
        "REPLACE_OR_REMOVE_AND_PROVE_ABSENT",
        "REPLACE_OR_REMOVE_THE_CHARACTER_FRAMEWORK_BEFORE_THE_SHIPPING_BUILD",
        [
            "PROJECT_SOURCE_AND_BUILD_RULE_DEPENDENCY_SCAN",
            "SHIPPING_BINARY_MODULE_AND_SYMBOL_SCAN",
            "SHIPPING_UFS_AND_NONUFS_ABSENCE",
            "REPLACEMENT_CHARACTER_RUNTIME_ACCEPTANCE",
        ],
    ),
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE": (
        "REMOVE_OR_EXCLUDE_AND_PROVE_ABSENT",
        "CLASSIFY_AND_REMOVE_OR_EXCLUDE_EVERY_DEVELOPMENT_ONLY_RUNTIME_ITEM",
        [
            "SHIPPING_CONTENT_CLASSIFICATION_MANIFEST",
            "SHIPPING_UFS_NONUFS_AND_IOSTORE_ABSENCE",
            "FRESH_INSTALL_NO_DEVELOPMENT_ENTRYPOINT_ACCEPTANCE",
        ],
    ),
    "QUARANTINED_IMPORT_RECEIPTS_PENDING": (
        "EXTERNAL_QUARANTINE_AND_PROVE_ABSENT",
        "MOVE_UNUSED_FAB_IMPORT_ROOTS_OUTSIDE_THE_PROJECT_AND_KEEP_THEM_OUT_OF_SHIPPING",
        [
            "EXTERNAL_QUARANTINE_INVENTORY_WITHOUT_HOST_PATH_SECRETS",
            "PROJECT_ROOT_ABSENCE_SCAN",
            "SHIPPING_UFS_NONUFS_AND_IOSTORE_ABSENCE",
        ],
    ),
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED": (
        "IMPLEMENT_COMPLETE_STAGED_PAYLOAD_PROVENANCE",
        "CLASSIFY_AND_BIND_EVERY_SHIPPING_FILE_DEPENDENCY_CONTAINER_AND_CHUNK",
        [
            "CANONICAL_SHIPPING_ARCHIVE_FILE_MANIFEST",
            "PAK_AND_IOSTORE_FILE_PACKAGE_AND_CHUNK_INVENTORY",
            "DEPENDENCY_PROVENANCE_AND_LICENSE_CLASSIFICATION",
            "INDEPENDENT_CLOSURE_VALIDATION",
        ],
    ),
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED": (
        "MANUAL_REVIEW_AND_APPROVAL",
        "COMPLETE_DOCUMENTED_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_ON_FINAL_SHIPPING_CAPTURES",
        [
            "FINAL_SHIPPING_VISUAL_CAPTURE_INVENTORY",
            "OWNER_VISUAL_REVIEW_RECORD",
            "LEGAL_TRADE_DRESS_AND_LOGO_REVIEW_RECORD",
        ],
    ),
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY": (
        "IMPLEMENT_AND_ACCEPT",
        "APPROVE_ALL_ENVIRONMENT_BINDINGS_PROXIES_WIND_LODS_VISUALS_AND_FINAL_CONTENT_PROVENANCE",
        [
            "ALL_REQUIRED_ENVIRONMENT_SLOTS_READY_REPORT",
            "COLLISION_AND_INTERACTION_PROXY_ACCEPTANCE",
            "REAL_RHI_VISUAL_AND_PERFORMANCE_APPROVAL",
            "SHIPPING_ENVIRONMENT_PACKAGE_PROVENANCE",
        ],
    ),
    "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING": (
        "IMPLEMENT_AND_ACCEPT_WITH_BOUNDED_SHIPPING_CATALOG",
        "LOCK_A_BOUNDED_SHIPPING_CATALOG_CALIBRATE_EXPOSED_PHYSICS_AND_KEEP_THROW_LAB_DEVELOPMENT_ONLY",
        [
            "APPROVED_SHIPPING_EQUIPMENT_CATALOG",
            "REPEATED_TOLERANCED_PHYSICS_CALIBRATION",
            "SAVE_MIGRATION_AND_ATOMIC_FALLBACK_ACCEPTANCE",
            "SHIPPING_THROW_LAB_ABSENCE_AND_BAG_SELECTION_ACCEPTANCE",
        ],
    ),
    "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING": (
        "IMPLEMENT_AND_ACCEPT",
        "SHIP_APPROVED_UI_ACCESSIBILITY_INPUT_CAMERA_REPLAY_AND_AUTHORED_AUDIO_COVERAGE",
        [
            "SHIPPING_UI_AND_ACCESSIBILITY_MATRIX",
            "SUPPORTED_INPUT_DEVICE_ACCEPTANCE",
            "AUTHORED_AUDIO_LICENSE_AND_EVENT_COVERAGE_MANIFEST",
            "MANUAL_CAMERA_REPLAY_AND_MIX_APPROVAL",
        ],
    ),
    "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING": (
        "IMPLEMENT_AND_ACCEPT_BAKED_OUTPUT",
        "DETERMINISTICALLY_BAKE_VALIDATE_HASH_AND_COOK_PCG_OUTPUT_WITHOUT_RUNTIME_GENERATION",
        [
            "AUTHOR_VALIDATE_BAKE_REOPEN_ROUNDTRIP",
            "DETERMINISTIC_BAKED_OUTPUT_MANIFEST",
            "COOKED_COURSE_AND_BAKED_OUTPUT_HASH_BINDING",
            "THREE_HOLE_STREAM_COLLISION_VISUAL_AND_PERFORMANCE_ACCEPTANCE",
        ],
    ),
    "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING": (
        "EXCLUDE_FROM_RELEASE_SCOPE_AND_PROVE_ABSENT",
        "EXCLUDE_CAREER_AND_AI_FROM_V05_SHIPPING_WITH_NO_ENTRYPOINT_SAVE_SPAWN_OR_PRODUCT_CLAIM",
        [
            "SHIPPING_FEATURE_AND_ENTRYPOINT_INVENTORY",
            "FRESH_INSTALL_NO_CAREER_SAVE_OR_AI_SPAWN_ACCEPTANCE",
            "SHIPPING_PACKAGE_EXCLUSION_EVIDENCE",
            "PUBLIC_FEATURE_CLAIM_REVIEW",
        ],
    ),
    "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING": (
        "IMPLEMENT_AND_ACCEPT_THREE_HOLE_V05",
        "COMPLETE_THE_FINAL_CONTENT_THREE_HOLE_NEW_PLAYER_SHIPPING_ACCEPTANCE_WITH_NO_TECHNICAL_SUBSTITUTIONS",
        [
            "APPROVED_CHARACTER_MOTION_ENVIRONMENT_EQUIPMENT_UI_AND_AUDIO_INVENTORY",
            "FRESH_INSTALL_NEW_PLAYER_THREE_HOLE_ACCEPTANCE",
            "FINAL_CONTENT_SHIPPING_PERFORMANCE_AND_SOAK",
            "MULTI_RESOLUTION_VISUAL_ACCESSIBILITY_AND_PRODUCT_OWNER_APPROVAL",
        ],
    ),
}

EXPECTED_GATE_IDS = [
    "WINDOWS_SHIPPING_PACKAGE",
    "CAREER_AI_SHIPPING_EXCLUSION",
    "THROW_LAB_SHIPPING_EXCLUSION",
    "SESSION10_ENVIRONMENT_TECHNICAL",
    "SESSION11_EQUIPMENT_TECHNICAL",
    "SESSION12_PRESENTATION_TECHNICAL",
    "BAKED_PCG_SHIPPING_AUTHORITY",
    "CHARACTER_FRAMEWORK_REPLACEMENT_OR_REMOVAL",
    "UNUSED_FAB_EXTERNAL_QUARANTINE",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE",
    "MANUAL_HUMAN_RELEASE_REVIEW",
    "THREE_HOLE_V05_PRODUCTION_ACCEPTANCE",
]

QUARANTINE_RECEIPT_PATH = "Evidence/Session19/UnusedFabExternalQuarantineReceipt.json"
QUARANTINE_MANIFEST_PATH = "Evidence/Session19/QuarantinedFabBeforeMove.tsv"

EXPECTED_GATES = [
    {"gateId": gate_id, "state": "PENDING", "accepted": False, "artifactPaths": []}
    for gate_id in EXPECTED_GATE_IDS
]
EXPECTED_GATES[0] = {
    "gateId": "WINDOWS_SHIPPING_PACKAGE",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_BUILD_ARCHIVE_BINARY_CONTENT_PLUGIN_"
        "THIRDPARTY_EXTERNAL_AND_FRESH_USERDIR_TECHNICAL_"
        "PROVENANCE_LEGAL_GAMEPLAY_AND_OWNER_REVIEW_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE[key]["path"]
        for key in (
            "buildLog", "buildReceipt", "verificationReceipt",
            "pluginCapabilityReceipt", "binaryReceipt", "contentReceipt",
            "thirdPartyNoticeReceipt", "freshUserDirReceipt",
        )
    ] + [EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE["receipt"]["path"]],
}
EXPECTED_GATES[1] = {
    "gateId": "CAREER_AI_SHIPPING_EXCLUSION",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_TECHNICAL_EXCLUSION_"
        "PUBLIC_CLAIM_AND_MANUAL_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["binaryReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["freshUserDirReceipt"]["path"],
        EXPECTED_V05_FEATURE_EXCLUSION["receipt"]["path"],
    ],
}
EXPECTED_GATES[2] = {
    "gateId": "THROW_LAB_SHIPPING_EXCLUSION",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_TECHNICAL_EXCLUSION_"
        "PUBLIC_CLAIM_AND_MANUAL_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["binaryReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["freshUserDirReceipt"]["path"],
        EXPECTED_V05_FEATURE_EXCLUSION["receipt"]["path"],
    ],
}
EXPECTED_GATES[3] = {
    "gateId": "SESSION10_ENVIRONMENT_TECHNICAL",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_ENVIRONMENT_TECHNICAL_"
        "RHI_VISUAL_PERFORMANCE_PROVENANCE_LEGAL_AND_OWNER_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL[key]["path"]
        for key in ("technicalPlan", "policy", "validator", "receipt")
    ] + [EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE["receipt"]["path"]],
}
EXPECTED_GATES[4] = {
    "gateId": "SESSION11_EQUIPMENT_TECHNICAL",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_EQUIPMENT_CATALOG_DETERMINISTIC_CALIBRATION_SAVE_"
        "THROW_LAB_EXCLUSION_AND_MEASURED_REFERENCE_CAPTURE_PROTOCOL_TECHNICAL_"
        "MEASURED_DATA_NAME_PLAY_FEEL_PROVENANCE_LEGAL_AND_OWNER_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_V05_EQUIPMENT_TECHNICAL[key]["path"]
        for key in ("policy", "validator", "receipt")
    ] + [
        EXPECTED_PHYSICS_MEASURED_REFERENCE_PREPARATION[key]["path"]
        for key in ("policy", "validator", "datasetTemplate")
    ] + [EXPECTED_V05_FEATURE_EXCLUSION["receipt"]["path"]],
}
EXPECTED_GATES[5] = {
    "gateId": "SESSION12_PRESENTATION_TECHNICAL",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_PRESENTATION_INPUT_AUDIO_TECHNICAL_"
        "DEVICE_AUDIO_VISUAL_ACCESSIBILITY_PROVENANCE_LEGAL_AND_OWNER_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_SESSION12_PRESENTATION_TECHNICAL[key]["path"]
        for key in ("audioEventCoverageManifest", "policy", "validator", "receipt")
    ] + [
        EXPECTED_SESSION12_PRESENTATION_TECHNICAL["shippingAudioCookReceipt"]["path"],
        EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE["receipt"]["path"],
    ],
}
EXPECTED_GATES[6] = {
    "gateId": "BAKED_PCG_SHIPPING_AUTHORITY",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_AUTHORED_DATA_SOURCE_CLOSURE_RUNTIME_SELECTION_"
        "AND_NO_PROJECT_AUTHORED_GENERATION_INVOCATION_PROVEN_FRESH_SHIPPING_PCG_"
        "ABSENCE_UNREAL_BAKE_REOPEN_COLLISION_VISUAL_PERFORMANCE_AND_SOAK_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_BAKED_PCG_BINDING["policyPath"],
        EXPECTED_BAKED_PCG_BINDING["validatorPath"],
        EXPECTED_BAKED_PCG_BINDING["receiptPath"],
        EXPECTED_BAKED_PCG_BINDING["sourceClosurePreBuildReceiptPath"],
        EXPECTED_BAKED_PCG_BINDING["candidateReceiptPath"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["policy"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["validator"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingSeparationPolicy"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingSeparationValidator"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingPcgSeparationReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE["receipt"]["path"],
    ],
}
EXPECTED_GATES[7] = {
    "gateId": "CHARACTER_FRAMEWORK_REPLACEMENT_OR_REMOVAL",
    "state": (
        "PARTIAL_PASS_SOURCE_EXTERNAL_QUARANTINE_RETAINED_ASSET_MIGRATION_CURRENT_CANDIDATE_"
        "PACKAGE_ABSENCE_AND_FAIL_CLOSED_PRODUCTION_MOTION_AUTHORING_PLAN_RUNTIME_ASSETS_"
        "RIGHTS_QUALITY_AND_OWNER_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["validator"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["evidence"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["shippingCookPresenceReceipt"]["path"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["policyPath"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["migrationScriptPath"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["validatorPath"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["inventoryPath"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["receiptPath"],
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["candidateReceiptPath"],
        FRAMEWORK_QUARANTINE_MANIFEST_PATH,
        FRAMEWORK_QUARANTINE_RECEIPT_PATH,
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["binaryReceipt"]["path"],
    ],
}
EXPECTED_GATES[8] = {
    "gateId": "UNUSED_FAB_EXTERNAL_QUARANTINE",
    "state": (
        "PARTIAL_PASS_EXTERNAL_QUARANTINE_AND_CURRENT_CANDIDATE_SHIPPING_ABSENCE_"
        "PROVENANCE_AND_OWNER_CLOSURE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        QUARANTINE_RECEIPT_PATH,
        QUARANTINE_MANIFEST_PATH,
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_V05_FEATURE_EXCLUSION["receipt"]["path"],
    ],
}
EXPECTED_GATES[9] = {
    "gateId": "ACTUAL_STAGED_PACKAGE_PROVENANCE",
    "state": (
        "PARTIAL_PASS_EXACT_PACKAGING_IDENTITY_AND_TECHNICAL_GENERATION_CLASS_"
        "7158_OF_7158_UNDERLYING_LICENSE_CONTENT_RIGHTS_LEGAL_AND_DISTRIBUTION_REVIEW_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        "Config/DG_Session19StagedProvenancePolicy.json",
        "Scripts/generate_dg_session19_staged_provenance.py",
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["provenanceClassificationDraft"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["provenanceDraftAudit"]["path"],
        EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION["proposal"]["path"],
        EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION["receipt"]["path"],
    ],
}
EXPECTED_GATES[10] = {
    "gateId": "MANUAL_HUMAN_RELEASE_REVIEW",
    "state": "PENDING",
    "accepted": False,
    "artifactPaths": [item["path"] for item in EXPECTED_MANUAL_REVIEW_ARTIFACTS],
}
EXPECTED_GATES[11] = {
    "gateId": "THREE_HOLE_V05_PRODUCTION_ACCEPTANCE",
    "state": (
        "PARTIAL_PASS_CURRENT_CANDIDATE_FRESH_INSTALL_THREE_HOLE_TECHNICAL_"
        "VISUAL_PERFORMANCE_SOAK_ACCESSIBILITY_PROVENANCE_LEGAL_AND_OWNER_ACCEPTANCE_PENDING"
    ),
    "accepted": False,
    "artifactPaths": [
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["verificationReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["contentReceipt"]["path"],
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE["freshUserDirReceipt"]["path"],
        EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE["receipt"]["path"],
        EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE["receipt"]["path"],
    ],
}

EXPECTED_QUARANTINE_RECEIPT = {
    "schema": "DiscGolfTour.Session19UnusedFabExternalQuarantineReceipt.v1",
    "schemaVersion": 1,
    "session": 19,
    "runId": "Session19ReleaseScope_20260824_01",
    "verifiedUtc": "2026-08-25T01:57:33Z",
    "operation": "RECOVERABLE_MOVE_OUTSIDE_PROJECT",
    "state": "EXTERNAL_QUARANTINE_VERIFIED_SHIPPING_ABSENCE_PENDING",
    "projectSourcesAbsent": True,
    "destinationOutsideProject": True,
    "destinationIntegrityVerified": True,
    "deleted": False,
    "hostPathRecorded": False,
    "recoveryLocationToken": (
        "DGTOUR_EXTERNAL_QUARANTINE/Session19ReleaseScope_20260824_01/Content"
    ),
    "beforeMoveManifest": {
        "path": QUARANTINE_MANIFEST_PATH,
        "bytes": 83517,
        "sha256": "D9F5194FFF35B98C4E32DA39B88E838BA49BC8D86782D6DED371B1871192AF02",
        "rows": 533,
    },
    "roots": [
        {
            "productId": "fab_project_nature_spruce_forest",
            "projectPath": "Content/PN_interactiveSpruceForest",
            "recoveryRelativePath": "PN_interactiveSpruceForest",
            "files": 363,
            "bytes": 1565612902,
            "sourceAbsent": True,
            "destinationPresent": True,
            "matchingFiles": 363,
            "reparseEntries": 0,
        },
        {
            "productId": "fab_greenbuggames_stump_scanned",
            "projectPath": "Content/Stump_Scanned",
            "recoveryRelativePath": "Stump_Scanned",
            "files": 63,
            "bytes": 680263631,
            "sourceAbsent": True,
            "destinationPresent": True,
            "matchingFiles": 63,
            "reparseEntries": 0,
        },
        {
            "productId": "fab_tharlevfx_water_materials",
            "projectPath": "Content/WaterMaterials",
            "recoveryRelativePath": "WaterMaterials",
            "files": 107,
            "bytes": 91003274,
            "sourceAbsent": True,
            "destinationPresent": True,
            "matchingFiles": 107,
            "reparseEntries": 0,
        },
    ],
    "totals": {
        "files": 533,
        "bytes": 2336879807,
        "matchingFiles": 533,
        "differenceCount": 0,
    },
    "releaseClosure": {
        "projectRootAbsenceSatisfied": True,
        "shippingPackageAbsenceSatisfied": False,
        "blockerClosed": False,
        "remainingEvidence": [
            "FRESH_WINDOWS_SHIPPING_UFS_NONUFS_AND_IOSTORE_ABSENCE",
        ],
    },
}

EXPECTED_POLICY_NEVER_COOK_ROOTS = [
    "/Game/DiscGolf/Animation/Mocap",
    "/Game/DiscGolf/Animation/Throws",
    "/Game/DiscGolf/Characters/Customization",
    "/Game/DiscGolf/Characters/MetaHuman/Source",
    "/Game/DiscGolf/Cook",
    "/Game/DiscGolf/Materials/CharacterCustomization",
    "/Game/DiscGolf/Materials/Outfits",
    "/Game/DiscGolf/Outfits",
    "/Game/DiscGolf/Tests",
    "/Game/PN_interactiveSpruceForest",
    "/Game/Stump_Scanned",
    "/Game/WaterMaterials",
    *EXPECTED_REQUIRED_NEVER_COOK_VIRTUAL_ROOTS,
]
EXPECTED_DEFAULT_GAME_NEVER_COOK_ROOTS = [
    "/Game/Environment/Forest/PCG",
    *EXPECTED_POLICY_NEVER_COOK_ROOTS,
]

EXPECTED_POLICY_SCALARS = {
    "schema": "DiscGolfTour.Session19ShippingContentPolicy.v1",
    "schemaVersion": 1,
    "session": 19,
    "policyId": "windows_shipping_v05_three_hole_policy_v1",
    "state": "ENFORCED_IN_SOURCE_AND_CONFIG_PENDING_FRESH_SHIPPING_PROOF",
    "releaseReady": False,
}

EXPECTED_POLICY_TARGET = {
    "platform": "Windows",
    "configuration": "Shipping",
    "milestone": "v0.5",
    "holes": [1, 2, 3],
}

EXPECTED_COMPILE_DEFINITIONS = {
    "DG_RELEASE_V05_SCOPE": 1,
    "DG_WITH_CAREER_AI": 0,
    "DG_WITH_THROW_LAB": 0,
    "DG_WITH_DEVELOPMENT_CONTENT": 0,
}

EXPECTED_PACKAGE_HARDENING = {
    "pakRules": {
        "path": "Config/DefaultPakFileRules.ini",
        "bytes": 3303,
        "sha256": "F9EBECA5023163592B8B5756925BA272E0D45EBCC0B2B5332B70F4D18FD71CB4",
    },
    "packagingConfig": {
        "path": "Config/DefaultGame.ini",
        "bytes": 2891,
        "sha256": "304390E67BF6C8FE0BD8D3BE1D830B303F4FBB0ED26358D8B638F9C1CF6DF296",
    },
    "projectDescriptor": {
        "path": "DiscGolfTour.uproject",
        "bytes": 2975,
        "sha256": "D88EAF4CC35491DFE9E64EDD000E860996DCA3213690F6F94E491C24DC96BB38",
    },
    "shippingGameTarget": {
        "path": "Source/DiscGolfTour.Target.cs",
        "bytes": 1026,
        "sha256": "6930D056321E4ABBEE290A8CA20B9917B2CCAA8E270D039B01B34C2DD1827843",
    },
    "shippingPluginCapabilityPolicy": {
        "path": "Config/DG_Session19ShippingPluginCapabilityPolicy.json",
        "bytes": 8770,
        "sha256": "26F140F20D651651DE38C8B17DF55C0BA5B0A068CEEB39552B2FA60F0ACD3D12",
    },
    "reviewedExactPluginDescriptorExclusionCount": 31,
    "broadPluginDirectoryExclusionsAllowed": False,
    "preserveRuntimeCapablePlugins": True,
    "preserveContentOnlyPlugins": True,
    "requiredNeverCookVirtualRoots": EXPECTED_REQUIRED_NEVER_COOK_VIRTUAL_ROOTS,
    "shippingIgnoredPluginDependencies": EXPECTED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES,
    "projectTargetConfigurationShippingDeniedPlugins": EXPECTED_SHIPPING_DENIED_PLUGINS,
    "projectEditorOnlyPlugins": EXPECTED_EDITOR_ONLY_PLUGINS,
    "projectShippingDependencyEnabledPlugins": EXPECTED_SHIPPING_DEPENDENCY_ENABLED_PLUGINS,
    "residualExactPakExclusions": EXPECTED_RESIDUAL_PAK_EXCLUSIONS,
    "shippingForbiddenPluginContentRoots": EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS,
    "requiredPresentFinalPayloadPluginDescriptors": EXPECTED_REQUIRED_PRESENT_PLUGIN_DESCRIPTORS,
    "requiredAbsentFinalPayloadPluginDescriptors": EXPECTED_REQUIRED_ABSENT_PLUGIN_DESCRIPTORS,
    "requiredDependencyClosureFinalPayloadDescriptors": EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS,
    "reviewedMetadataOnlyDependencyDescriptors": EXPECTED_REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS,
    "metadataOnlyDependencyForbiddenSubdirectories": EXPECTED_METADATA_ONLY_FORBIDDEN_SUBDIRECTORIES,
}

EXPECTED_AUTHORITATIVE_IDENTITY_POLICY = {
    "authoritativeScopes": [
        "ARCHIVE_FILE",
        "NONUFS_ENTRY",
        "CONTAINER_NAMED_ENTRY",
    ],
    "manifestUfsDevelopmentRows": "NON_AUTHORITATIVE_DIAGNOSTIC_ONLY",
    "requiredDependencyClosureDescriptorExceptions": EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS,
    "forbiddenIdentityExceptions": [],
}

EXPECTED_POLICY_EVIDENCE = [
    "CLEAN_NON_ITERATIVE_WINDOWS_SHIPPING_BUILD_COOK_STAGE_PAK_IOSTORE_ARCHIVE",
    "WHOLE_ARCHIVE_SHA256_MANIFEST",
    "WHOLE_IOSTORE_ENTRY_AND_CHUNK_INVENTORY",
    "ZERO_FORBIDDEN_PATH_OR_LOOSE_FILE_MATCHES",
    "FRESH_INSTALL_THREE_HOLE_ACCEPTANCE",
    "ZERO_CAREER_AI_OR_THROW_LAB_ENTRYPOINT_SIDE_EFFECTS",
]

EXPECTED_FORBIDDEN_SHIPPING_TOKENS = [
    "/Animation/Mocap/",
    "/Animation/Throws/AM_DG_RHBH_Prototype",
    "/Animation/Throws/A_DG_RHBH_Prototype",
    "/Characters/Customization/",
    "/Characters/MetaHuman/Source/",
    "/DiscGolf/Cook/DA_DG_RuntimeCookManifest",
    "/Materials/CharacterCustomization/",
    "/Materials/Outfits/",
    "/Outfits/",
    "/DiscGolf/Tests/",
    "/PN_interactiveSpruceForest/",
    "/Stump_Scanned/",
    "/WaterMaterials/",
    "/Plugins/DiscGolfCharacterFramework/",
    *EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS,
]

EXPECTED_FORBIDDEN_LOOSE_PATTERNS = [
    "*.pdb",
    "*/Data/PhysicsRegressionPresets.json",
    "*/Plugins/DiscGolfCharacterFramework/*",
]

EXPECTED_REQUIRED_LOOSE_DATA = [
    "DiscGolfTour/Data/PineRidgeCourse.json",
    "DiscGolfTour/Data/PineRidgeHole1.json",
    "DiscGolfTour/Data/PineRidgeHole2.json",
    "DiscGolfTour/Data/PineRidgeHole3.json",
    "DiscGolfTour/Data/PineRidgePresentation.json",
]

EXPECTED_FEATURE_EXCLUSIONS = {
    "career": {
        "shippingEnabled": False,
        "forbiddenSavePrefix": "DGT_Career_",
        "freshInstallSaveMustNotExist": True,
        "publicClaimAllowed": False,
    },
    "aiOpponents": {
        "shippingEnabled": False,
        "runtimeSpawnMustNotOccur": True,
        "publicClaimAllowed": False,
    },
    "throwLab": {
        "shippingEnabled": False,
        "forbiddenSavePrefix": "DGT_ThrowLab",
        "consoleEntryPointMustBeInert": True,
        "publicClaimAllowed": False,
    },
    "pcg": {
        "runtimeGenerationAllowed": False,
        "shippingAuthority": "DETERMINISTIC_BAKED_OUTPUT_ONLY",
    },
}

EXPECTED_FROZEN_HISTORY: dict[str, tuple[int, str]] = {
    "Config/DG_BrandLicenseContract.json": (9267, "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001"),
    "Scripts/validate_dg_session9_brand_license.py": (74292, "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550"),
    "Docs/DG_BRAND_ASSET_AUDIT.md": (7579, "4FDA6D610555E2D159C32BAB4D92AB7E599937BA4E5510B9901013EFAD13737E"),
    "Config/DG_Session10EnvironmentContract.json": (19976, "05931329CE877E80D4D36691D9091BBD6DDACE8F3F40612C5BE11B47744CDDC5"),
    "Scripts/validate_dg_session10_environment.py": (76608, "13F3AECBB569466B6E3E8C49E0B3FA599EF57B13CE23604602CDC3158809112A"),
    "Docs/DG_SESSION10_ENVIRONMENT_AUDIT.md": (14074, "4FF60C5413672A3D72A8AFE76B0E94B6852EB3F950C835E315F3F9C8446449EC"),
    "Config/DG_Session11EquipmentThrowLabContract.json": (20213, "5306B7C1354775A896332474711B5E6E92C44B44F7EFDE0D9353EB4E9DBCA79D"),
    "Scripts/validate_dg_session11_equipment_throw_lab.py": (36254, "E9D262E61122176EB4515D4AA2A4854D6354EADA2871B3A01299B17B13179D8A"),
    "Docs/DG_SESSION11_EQUIPMENT_THROW_LAB_AUDIT.md": (8507, "04F0F1FF988B274069F93385F5AAF3E6B67B2ADD1EC9EECDEE8C8969312FC8EC"),
    "Config/DG_Session12PresentationContract.json": (14978, "B8385D58B102C06AC19A986B14E2858C1906F7BD79D28D48C9AB555D0EB4AB3F"),
    "Scripts/validate_dg_session12_presentation.py": (30684, "D2FCEE88310BF00B3A7F0EE1803E8F2216D5E42C15161B631E8A473A683B5388"),
    "Docs/DG_SESSION12_PRESENTATION_AUDIT.md": (12898, "2A560B5374422C5E4962654C8A9A5056D4827ED0C75CD462A88ED6826E51FB70"),
    "Config/DG_Session13CourseAuthoringPcgContract.json": (17700, "64554C71530F09CE4AC6F779A65A720C195112617DDEA7D08E574F4258A044F0"),
    "Scripts/validate_dg_session13_course_authoring_pcg.py": (51940, "ABD65FA427ACB7099E603CC045C5333D3E3BD26C8C8E5EF6A07C3406B3A03E2F"),
    "Docs/DG_SESSION13_COURSE_AUTHORING_PCG_AUDIT.md": (7257, "810610B96E1DCD2F700C908B48E48F9B9E5080B6F3A32FA461D7F60BDAE443F8"),
    "Config/DG_Session14CareerAiTestingContract.json": (16762, "5ADBA78E08C508C62C5BA71E7145C8939398F2819EF5B357F42D1E01EEF6C48D"),
    "Scripts/validate_dg_session14_career_ai.py": (34967, "9A7E15D9D0F0F7BF36CD1960FFA0D8ED924376975CD84DC6208B4A1E288EF691"),
    "Docs/DG_SESSION14_CAREER_AI_TESTING_AUDIT.md": (8323, "8A1867CDF159D2C41863A2C3A7E0B847C746152E62E9269F4078A062D3A771DA"),
    "Config/DG_Session15VerticalSliceContract.json": (17917, "DDE8DF325D5E6765D814C9B2656FBA0D544F79F8EE549BCA63FD8F4D64EA9142"),
    "Scripts/validate_dg_session15_vertical_slice.py": (46943, "457BC09E5D223E4B5B91250073894A56F68E8157254776DEF798F305F7B44EC2"),
    "Docs/DG_SESSION15_VERTICAL_SLICE_AUDIT.md": (9164, "EF7F6F83461D88CA1F536C7B082118FB909EAB62A2F8F0747436DA31C6DDC51D"),
    "Config/DG_Session16CorePlayabilityContract.json": (23985, "A30DC355EB098D2C9FF5F7CDD1BF3139FCE207EC4E9D79C57548DD961869B692"),
    "Scripts/validate_dg_session16_core_playability.py": (55677, "2364D916BDC5E62B0609F27ACB4BA3E936318B9A7985EC9DF939F4EB33182A9F"),
    "Docs/DG_SESSION16_CORE_PLAYABILITY_AUDIT.md": (9880, "B4F1F3DB2376071290BF243AABC7C958B5CFF6429CD7C181A53979DCA9AC8BDC"),
    "Docs/DG_SESSION17_INTERACTIVE_ROUND_FLOW_AUDIT.md": (6208, "19330DAAB484491CBC265025857CD4C016274076CEDF595F540A000B07FD1670"),
    "Config/DG_Session18PolyHavenProvenanceContract.json": (6766, "93412BAE4021EB03B8F41FC54CE681A3F06953C896687AF041451237CA410D1D"),
    "Scripts/validate_dg_session18_poly_haven_provenance.py": (73507, "F48F5D3C3FA29A4234556DA99B17993255B9F0C9CF7114B0A6F3C7EC2587AD59"),
    "Docs/DG_SESSION18_POLY_HAVEN_PROVENANCE_AUDIT.md": (6778, "BB6595E1127A8819D4BC063AF9A4F6CDD29D9D084F68FC11AC2002C9B815BA88"),
}

ROOT_KEYS = {
    "schema", "schemaVersion", "session", "contractId", "authority",
    "normalStatus", "releaseStatus", "scopeLocked", "evidenceComplete",
    "releaseReady", "releaseUseAllowed", "publicReleaseApproved",
    "releaseTarget", "featureScope", "unusedFabExternalQuarantine", "shippingContentPolicy",
    "shippingPluginCapability", "shippingPackageHardening", "stagedProvenancePolicy",
    "currentShippingCandidateEvidence", "externalTechnicalEvidence",
    "v05FeatureExclusion", "v05EquipmentTechnical", "physicsMeasuredReferencePreparation",
    "session10EnvironmentTechnical", "session12PresentationTechnical",
    "technicalOriginAttribution", "threeHoleTechnicalAcceptance",
    "bakedPcgSourceAuthority", "bakedPcgRuntimeClosure", "productionMotionPreparation",
    "serializedAssetMigration",
    "characterFrameworkExternalQuarantine", "manualReleaseReview",
    "closureStrategies", "evidenceGates", "continuity",
    "inheritedReleaseBlockers", "resolvedBySession19",
    "remainingReleaseBlockers", "closureSummary", "requiredFiles", "validation",
}


class StrictJsonError(ValueError):
    pass


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> Any:
    raise StrictJsonError(f"non-finite JSON number: {value}")


def load_json_text_strict(text: str) -> Any:
    return json.loads(
        text,
        object_pairs_hook=_reject_duplicate_keys,
        parse_constant=_reject_constant,
    )


def load_json_strict(path: Path) -> Any:
    try:
        return load_json_text_strict(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _selectable_binding_key(trail: tuple[Any, ...], value: Any) -> bool:
    if trail and trail[0] == "continuity":
        return False
    if type(value) is str and CANDIDATE_ID_PLACEHOLDER in value:
        return True
    key = next((item for item in reversed(trail) if type(item) is str), "")
    lowered = key.casefold()
    if key in {
        "candidateId", "archiveRecoveryLocationToken", "requiredFiles",
        "artifactPaths",
    }:
        return True
    if lowered.endswith(("path", "paths", "sha256", "bytes")):
        return True
    return key == "evidenceRequirements" and type(value) is str and "/" in value


def selected_binding_expectation(
        static_expected: Any, selected: Any, trail: tuple[Any, ...] = ()) -> Any:
    """Overlay only candidate/file bindings from the selected contract authority."""
    if _selectable_binding_key(trail, static_expected):
        return copy.deepcopy(selected)
    if type(static_expected) is dict and type(selected) is dict:
        return {
            key: selected_binding_expectation(
                value, selected.get(key), trail + (key,)
            )
            for key, value in static_expected.items()
        }
    if type(static_expected) is list and type(selected) is list:
        return [
            selected_binding_expectation(
                value,
                selected[index] if index < len(selected) else None,
                trail + (index,),
            )
            for index, value in enumerate(static_expected)
        ]
    return copy.deepcopy(static_expected)


def validate_selected_binding_shapes(
        value: Any, root: Path, issues: list[str], trail: tuple[Any, ...] = ()) -> None:
    if type(value) is dict:
        for key, child in value.items():
            child_trail = trail + (key,)
            lowered = key.casefold()
            label = ".".join(str(item) for item in child_trail)
            if key == "candidateId":
                if type(child) is not str or not CANDIDATE_ID_RE.fullmatch(child):
                    issues.append(f"{label} must be a canonical candidate ID")
            elif lowered.endswith("sha256"):
                if type(child) is not str or not SHA256_RE.fullmatch(child):
                    issues.append(f"{label} must be uppercase SHA-256")
            elif lowered.endswith("bytes"):
                if type(child) is not int or type(child) is bool or child < 0:
                    issues.append(f"{label} must be a non-negative integer byte count")
            elif lowered.endswith("path"):
                validate_relative_path(
                    child, label, root, issues, must_exist=False,
                )
            elif lowered.endswith("paths") or key in {"requiredFiles", "artifactPaths"}:
                if type(child) is not list:
                    issues.append(f"{label} must be an array of project-relative paths")
                else:
                    for index, path_value in enumerate(child):
                        validate_relative_path(
                            path_value, f"{label}[{index}]", root, issues,
                            must_exist=False,
                        )
            validate_selected_binding_shapes(child, root, issues, child_trail)
    elif type(value) is list:
        for index, child in enumerate(value):
            validate_selected_binding_shapes(child, root, issues, trail + (index,))


def validate_candidate_binding_templates(
        template: Any, selected: Any, candidate_id: str,
        issues: list[str], trail: tuple[Any, ...] = ()) -> None:
    if type(template) is dict and type(selected) is dict:
        for key, value in template.items():
            if key in selected:
                validate_candidate_binding_templates(
                    value, selected[key], candidate_id, issues, trail + (key,)
                )
    elif type(template) is list and type(selected) is list:
        for index, value in enumerate(template[:len(selected)]):
            validate_candidate_binding_templates(
                value, selected[index], candidate_id, issues, trail + (index,)
            )
    elif type(template) is str and CANDIDATE_ID_PLACEHOLDER in template:
        expected = template.replace(CANDIDATE_ID_PLACEHOLDER, candidate_id)
        if selected != expected:
            label = ".".join(str(item) for item in trail)
            issues.append(
                f"{label} is not bound to selected candidate {candidate_id}"
            )


def write_json_exclusive(path: Path, value: Any) -> None:
    data = (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise FileExistsError(
            f"release-scope output already exists; refusing overwrite: {path}"
        ) from exc


def validate_file_binding(
        root: Path, binding: dict[str, Any], path_key: str, bytes_key: str,
        sha_key: str, label: str) -> tuple[Path | None, list[str]]:
    issues: list[str] = []
    path = validate_relative_path(
        binding.get(path_key), f"{label} path", root, issues, must_exist=True,
    )
    if path is None:
        return None, issues
    expected_bytes = binding.get(bytes_key)
    expected_sha = binding.get(sha_key)
    actual_bytes = path.stat().st_size
    actual_sha = sha256_file(path)
    if type(expected_bytes) is not int or type(expected_bytes) is bool:
        issues.append(f"{label} bound byte size must be an integer")
    elif actual_bytes != expected_bytes:
        issues.append(
            f"{label} byte size differs: expected {expected_bytes}, got {actual_bytes}"
        )
    if type(expected_sha) is not str or not SHA256_RE.fullmatch(expected_sha):
        issues.append(f"{label} bound SHA-256 must be uppercase SHA-256")
    elif actual_sha != expected_sha:
        issues.append(f"{label} SHA-256 differs: expected {expected_sha}, got {actual_sha}")
    return path, issues


def require_exact_keys(value: Any, keys: set[str], label: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != keys:
        missing = sorted(keys - actual)
        extra = sorted(actual - keys)
        issues.append(f"{label} keys differ: missing={missing} extra={extra}")
        return False
    return True


def require_typed_equal(actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    if type(actual) is not type(expected):
        issues.append(
            f"{label} type differs: expected {type(expected).__name__}, "
            f"got {type(actual).__name__}"
        )
        return
    if type(expected) is dict:
        if set(actual) != set(expected):
            issues.append(
                f"{label} keys differ: missing={sorted(set(expected) - set(actual))} "
                f"extra={sorted(set(actual) - set(expected))}"
            )
            return
        for key in expected:
            require_typed_equal(actual[key], expected[key], f"{label}.{key}", issues)
        return
    if type(expected) is list:
        if len(actual) != len(expected):
            issues.append(f"{label} length differs: expected {len(expected)}, got {len(actual)}")
            return
        for index, (left, right) in enumerate(zip(actual, expected)):
            require_typed_equal(left, right, f"{label}[{index}]", issues)
        return
    if actual != expected:
        issues.append(f"{label} differs: expected {expected!r}, got {actual!r}")


def validate_relative_path(
        value: Any, label: str, root: Path, issues: list[str], *, must_exist: bool) -> Path | None:
    if type(value) is not str:
        issues.append(f"{label} must be a string path")
        return None
    if not value or value != value.strip() or "\\" in value or "\x00" in value or ":" in value:
        issues.append(f"{label} is not a canonical project-relative POSIX path")
        return None
    pure = PurePosixPath(value)
    if pure.is_absolute() or value.startswith("/") or any(part in ("", ".", "..") for part in pure.parts):
        issues.append(f"{label} escapes or is not canonical beneath the project root")
        return None
    if pure.as_posix() != value:
        issues.append(f"{label} is not canonical POSIX form")
        return None
    candidate = root.joinpath(*pure.parts)
    resolved_root = root.resolve()
    try:
        resolved = candidate.resolve(strict=must_exist)
    except OSError as exc:
        issues.append(f"{label} cannot be resolved: {exc}")
        return None
    if resolved != resolved_root and resolved_root not in resolved.parents:
        issues.append(f"{label} resolves outside the project root")
        return None
    if must_exist and (not candidate.is_file() or candidate.is_symlink()):
        issues.append(f"{label} must be an existing regular non-symlink file")
        return None
    return candidate


def is_canonical_relative_posix(value: Any) -> bool:
    if type(value) is not str:
        return False
    if not value or value != value.strip() or "\\" in value or "\x00" in value or ":" in value:
        return False
    pure = PurePosixPath(value)
    return (
        not pure.is_absolute()
        and not value.startswith("/")
        and all(part not in ("", ".", "..") for part in pure.parts)
        and pure.as_posix() == value
    )


def selected_contract_profile(
        contract_binding: dict[str, Any] | None) -> str:
    """Select the profile from the caller-selected path, never a JSON self-claim."""
    if type(contract_binding) is not dict:
        return HISTORICAL_CONTRACT_PROFILE
    path = contract_binding.get("path")
    if path == DEFAULT_CONTRACT_PATH.as_posix():
        return HISTORICAL_CONTRACT_PROFILE
    return FRESH_CANDIDATE_CONTRACT_PROFILE


def _is_reparse_component(path: Path) -> bool:
    try:
        value = os.lstat(path)
    except OSError:
        return True
    return path.is_symlink() or bool(
        getattr(value, "st_file_attributes", 0) & 0x400
    )


def _bound_json_document(
        root: Path, binding: Any, label: str, issues: list[str]) -> Any | None:
    """Read one bound JSON file once and reject path/descriptor identity drift."""
    if type(binding) is not dict:
        issues.append(f"{label} binding must be an object")
        return None
    path = validate_relative_path(
        binding.get("path"), f"{label} path", root, issues, must_exist=True,
    )
    expected_bytes = binding.get("bytes")
    expected_sha = binding.get("sha256")
    if type(expected_bytes) is not int or type(expected_bytes) is bool or expected_bytes < 0:
        issues.append(f"{label} bound byte size must be a non-negative integer")
    if type(expected_sha) is not str or not SHA256_RE.fullmatch(expected_sha):
        issues.append(f"{label} bound SHA-256 must be uppercase SHA-256")
    if path is None:
        return None
    current = root
    for part in PurePosixPath(binding["path"]).parts:
        current = current / part
        if _is_reparse_component(current):
            issues.append(f"{label} path contains a reparse component")
            return None
    try:
        with path.open("rb") as handle:
            before = os.fstat(handle.fileno())
            raw = handle.read()
            after = os.fstat(handle.fileno())
        path_after = os.stat(path)
    except OSError as exc:
        issues.append(f"{label} could not be read stably: {exc}")
        return None
    before_id = (
        before.st_dev, before.st_ino, before.st_size,
        getattr(before, "st_mtime_ns", int(before.st_mtime * 1_000_000_000)),
    )
    after_id = (
        after.st_dev, after.st_ino, after.st_size,
        getattr(after, "st_mtime_ns", int(after.st_mtime * 1_000_000_000)),
    )
    path_id = (
        path_after.st_dev, path_after.st_ino, path_after.st_size,
        getattr(path_after, "st_mtime_ns", int(path_after.st_mtime * 1_000_000_000)),
    )
    if before_id != after_id or after_id != path_id:
        issues.append(f"{label} changed identity while being read")
        return None
    actual_sha = hashlib.sha256(raw).hexdigest().upper()
    if type(expected_bytes) is int and type(expected_bytes) is not bool:
        if len(raw) != expected_bytes:
            issues.append(
                f"{label} byte size differs: expected {expected_bytes}, got {len(raw)}"
            )
    if type(expected_sha) is str and SHA256_RE.fullmatch(expected_sha):
        if actual_sha != expected_sha:
            issues.append(
                f"{label} SHA-256 differs: expected {expected_sha}, got {actual_sha}"
            )
    try:
        return load_json_text_strict(raw.decode("utf-8-sig"))
    except (UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        issues.append(f"{label} is invalid JSON: {exc}")
        return None


def _profile_document(
        name: str, root: Path, binding: Any, label: str, issues: list[str],
        documents: dict[str, Any] | None) -> Any | None:
    if type(documents) is dict and name in documents:
        return copy.deepcopy(documents[name])
    return _bound_json_document(root, binding, label, issues)


def _binding_projection(value: Any) -> dict[str, Any] | None:
    if type(value) is not dict:
        return None
    if not {"path", "bytes", "sha256"}.issubset(value):
        return None
    return {
        "path": value.get("path"),
        "bytes": value.get("bytes"),
        "sha256": value.get("sha256"),
    }


def _candidate_v05_equipment_policy_binding(
        receipt: Any, candidate_id: str) -> dict[str, Any] | None:
    if type(receipt) is not dict:
        return None
    bindings = receipt.get("bindings")
    embedded = bindings.get("policy") if type(bindings) is dict else None
    if type(embedded) is not dict:
        return None
    return {
        "path": V05_EQUIPMENT_CANDIDATE_POLICY_PATTERN.format(
            candidateId=candidate_id
        ),
        "bytes": embedded.get("bytes"),
        "sha256": embedded.get("sha256"),
    }


def _validate_v05_equipment_candidate_policy(
        receipt: Any, receipt_binding: Any, policy: Any,
        policy_binding: Any, candidate_id: str,
        issues: list[str]) -> dict[str, Any] | None:
    """Bind the fresh equipment authority carried by its exact receipt."""
    issue_count = len(issues)
    receipt_projection = _binding_projection(receipt_binding)
    expected_receipt_path = V05_EQUIPMENT_RECEIPT_PATTERN.format(
        candidateId=candidate_id
    )
    if receipt_projection is None:
        issues.append("v0.5 equipment receipt binding must contain path/bytes/sha256")
    else:
        require_typed_equal(
            receipt_projection["path"], expected_receipt_path,
            "v0.5 equipment receipt binding.path", issues,
        )
        if (
                type(receipt_projection["bytes"]) is not int
                or type(receipt_projection["bytes"]) is bool
                or receipt_projection["bytes"] <= 0):
            issues.append("v0.5 equipment receipt binding.bytes must be positive")
        if (
                type(receipt_projection["sha256"]) is not str
                or not SHA256_RE.fullmatch(receipt_projection["sha256"])):
            issues.append(
                "v0.5 equipment receipt binding.sha256 must be uppercase SHA-256"
            )

    if type(receipt) is not dict:
        issues.append("v0.5 equipment receipt must be an object")
        return None
    for key, expected in {
            "schema": equipment_validator.SCHEMA,
            "schemaVersion": 1,
            "session": 19,
            "candidateId": candidate_id,
            "state": EXPECTED_V05_EQUIPMENT_TECHNICAL["currentState"],
            "passed": True,
            "failures": [],
    }.items():
        require_typed_equal(
            receipt.get(key), expected, f"v0.5 equipment receipt.{key}", issues,
        )

    bindings = receipt.get("bindings")
    embedded = bindings.get("policy") if type(bindings) is dict else None
    embedded_keys = {
        "artifact", "fileName", "bytes", "sha256", "hostPathRecorded",
    }
    if not require_exact_keys(
            embedded, embedded_keys,
            "v0.5 equipment receipt.bindings.policy", issues):
        return None
    expected_policy_path = V05_EQUIPMENT_CANDIDATE_POLICY_PATTERN.format(
        candidateId=candidate_id
    )
    expected_file_name = PurePosixPath(expected_policy_path).name
    require_typed_equal(
        embedded["artifact"], "policy",
        "v0.5 equipment receipt.bindings.policy.artifact", issues,
    )
    require_typed_equal(
        embedded["fileName"], expected_file_name,
        "v0.5 equipment receipt.bindings.policy.fileName", issues,
    )
    require_typed_equal(
        embedded["hostPathRecorded"], False,
        "v0.5 equipment receipt.bindings.policy.hostPathRecorded", issues,
    )
    if (
            type(embedded["bytes"]) is not int
            or type(embedded["bytes"]) is bool
            or embedded["bytes"] <= 0):
        issues.append(
            "v0.5 equipment receipt.bindings.policy.bytes must be positive"
        )
    if (
            type(embedded["sha256"]) is not str
            or not SHA256_RE.fullmatch(embedded["sha256"])):
        issues.append(
            "v0.5 equipment receipt.bindings.policy.sha256 must be uppercase SHA-256"
        )
    derived_binding = _candidate_v05_equipment_policy_binding(
        receipt, candidate_id
    )
    if derived_binding is None:
        issues.append("v0.5 equipment candidate policy binding is unavailable")
        return None
    if _binding_projection(policy_binding) != derived_binding:
        issues.append(
            "v0.5 equipment candidate policy binding differs from receipt authority"
        )
        return None
    if type(policy) is not dict:
        issues.append("v0.5 equipment candidate policy must be an object")
        return None
    require_typed_equal(
        policy.get("candidateId"), candidate_id,
        "v0.5 equipment candidate policy.candidateId", issues,
    )
    policy_issues = equipment_validator.validate_policy(
        policy, require_candidate_bindings=True,
    )
    if policy_issues:
        issues.extend(
            "v0.5 equipment candidate policy invalid: " + issue
            for issue in policy_issues
        )
    if len(issues) != issue_count:
        return None
    return derived_binding


def _candidate_identity_value(document: Any, path: tuple[str, ...]) -> Any:
    value = document
    for key in path:
        if type(value) is not dict:
            return None
        value = value.get(key)
    return value


def _validate_consumed_candidate_identity(
        document: Any, label: str, candidate_id: str,
        archive_sha256: str, executable_bytes: int, executable_sha256: str,
        archive_bytes: int, archive_file_count: int,
        issues: list[str]) -> None:
    """Reject cross-candidate receipts wherever their schema exposes identity."""
    if type(document) is not dict:
        issues.append(f"{label} must be an object for candidate identity binding")
        return
    groups = (
        ((("candidateId",), ("runId",), ("candidate", "candidateId")), candidate_id, "candidate ID", False),
        ((("archiveCanonicalManifestSha256",), ("archiveManifestSha256",),
          ("finalArchiveCanonicalManifestSha256",),
          ("shippingPluginCapabilities", "finalArchiveCanonicalManifestSha256"),
          ("finalPayload", "archive", "canonicalManifestSha256"),
          ("bakedPcg", "archiveCanonicalManifestSha256"),
          ("archive", "canonicalManifestSha256")), archive_sha256, "archive canonical SHA-256", True),
        ((("executableSha256",), ("input", "sha256"),
          ("innerShippingExecutable", "sha256")), executable_sha256, "Shipping executable SHA-256", True),
        ((("executableBytes",), ("input", "bytes"),
          ("innerShippingExecutable", "bytes")), executable_bytes, "Shipping executable bytes", False),
        ((("archiveBytes",), ("archive", "bytes"),
          ("finalPayload", "archive", "bytes")), archive_bytes, "archive bytes", False),
        ((("archiveFileCount",), ("archive", "fileCount"),
          ("finalPayload", "archive", "fileCount")), archive_file_count, "archive file count", False),
    )
    for paths, expected, field_label, casefold in groups:
        for path in paths:
            actual = _candidate_identity_value(document, path)
            if actual is None:
                continue
            matches = (
                actual.upper() == expected.upper()
                if casefold and type(actual) is str and type(expected) is str
                else type(actual) is type(expected) and actual == expected
            )
            if not matches:
                issues.append(
                    f"{label}.{'.'.join(path)} {field_label} differs: "
                    f"expected {expected!r}, got {actual!r}"
                )


def _candidate_section_binding(
        candidate_section: dict[str, Any], key: str, expected_path: str,
        label: str, issues: list[str]) -> dict[str, Any] | None:
    """Select one exact candidate receipt/tool binding from contract authority."""
    projection = _binding_projection(candidate_section.get(key))
    if projection is None:
        issues.append(f"{label} must contain path/bytes/sha256")
        return None
    require_typed_equal(projection["path"], expected_path, f"{label}.path", issues)
    if (
            type(projection["bytes"]) is not int
            or type(projection["bytes"]) is bool
            or projection["bytes"] <= 0):
        issues.append(f"{label}.bytes must be a positive integer")
    if (
            type(projection["sha256"]) is not str
            or not SHA256_RE.fullmatch(projection["sha256"])):
        issues.append(f"{label}.sha256 must be uppercase SHA-256")
    return copy.deepcopy(projection)


def _nonnegative_document_int(
        document: Any, path: tuple[str, ...], label: str,
        issues: list[str]) -> int | None:
    value = _candidate_identity_value(document, path)
    if type(value) is not int or type(value) is bool or value < 0:
        issues.append(f"{label} must be a non-negative integer")
        return None
    return value


def _derive_current_candidate_evidence(
        *, candidate_section: dict[str, Any], loaded: dict[str, Any],
        candidate_id: str, archive_sha256: str, executable_bytes: int,
        executable_sha256: str, archive_bytes: int, archive_file_count: int,
        fresh_facts: dict[str, Any], origin_facts: dict[str, Any],
        origin_section: dict[str, Any], fresh_self_test_count: int,
        issues: list[str]) -> dict[str, Any] | None:
    """Derive current candidate facts from bound receipts, never snapshot values."""
    issue_count = len(issues)
    require_exact_keys(
        candidate_section, set(EXPECTED_CURRENT_CANDIDATE_EVIDENCE),
        "currentShippingCandidateEvidence source scaffold", issues,
    )
    document_names = (
        "buildReceipt", "verificationReceipt", "pluginCapabilityReceipt",
        "binaryReceipt", "contentReceipt", "thirdPartyNoticeReceipt",
        "provenanceClassificationDraft", "provenanceDraftAudit",
        "freshUserDirReceipt",
    )
    documents = {name: loaded.get(name) for name in document_names}
    for name, document in documents.items():
        if type(document) is not dict:
            issues.append(f"{name} must be an object for candidate derivation")
    if len(issues) != issue_count:
        return None

    build = documents["buildReceipt"]
    verification = documents["verificationReceipt"]
    plugin = documents["pluginCapabilityReceipt"]
    binary = documents["binaryReceipt"]
    content = documents["contentReceipt"]
    notice = documents["thirdPartyNoticeReceipt"]
    draft = documents["provenanceClassificationDraft"]
    audit = documents["provenanceDraftAudit"]

    receipt_paths = {
        "buildReceipt": f"Evidence/Session19/ShippingCandidate-{candidate_id}.json",
        "verificationReceipt": (
            "Evidence/Session19/ShippingCandidateVerification-"
            f"{candidate_id}.json"
        ),
        "pluginCapabilityReceipt": (
            f"Evidence/Session19/ShippingPluginCapability-{candidate_id}.json"
        ),
        "binaryReceipt": f"Evidence/Session19/ShippingBinary-{candidate_id}.json",
        "contentReceipt": f"Evidence/Session19/CandidateContent-{candidate_id}.json",
        "thirdPartyNoticeReceipt": (
            "Evidence/Session19/ThirdPartyNoticeVerification-"
            f"{candidate_id}.json"
        ),
        "provenanceClassificationDraft": (
            "Evidence/Session19/StagedProvenanceClassificationDraft-"
            f"{candidate_id}.json"
        ),
        "provenanceDraftAudit": (
            f"Evidence/Session19/StagedProvenanceDraftAudit-{candidate_id}.json"
        ),
        "freshUserDirReceipt": f"Evidence/Session19/FreshUserDir-{candidate_id}.json",
    }
    bindings: dict[str, dict[str, Any]] = {}
    for name, expected_path in receipt_paths.items():
        binding = _candidate_section_binding(
            candidate_section, name, expected_path,
            f"currentShippingCandidateEvidence.{name}", issues,
        )
        if binding is not None:
            bindings[name] = binding
    validator_binding = _candidate_section_binding(
        candidate_section, "freshUserDirValidator",
        "Scripts/validate_dg_session19_fresh_userdir.py",
        "currentShippingCandidateEvidence.freshUserDirValidator", issues,
    )

    build_log = _binding_projection(build.get("buildLog"))
    verification_build_log = _binding_projection(verification.get("buildLog"))
    expected_build_log_path = (
        f"Evidence/Session19/ShippingBuildCookRun-{candidate_id}.log"
    )
    if build_log is None:
        issues.append("Shipping build receipt.buildLog must contain path/bytes/sha256")
    else:
        require_typed_equal(
            build_log["path"], expected_build_log_path,
            "Shipping build receipt.buildLog.path", issues,
        )
        if (
                type(build_log["bytes"]) is not int
                or type(build_log["bytes"]) is bool
                or build_log["bytes"] <= 0):
            issues.append("Shipping build receipt.buildLog.bytes must be positive")
        if (
                type(build_log["sha256"]) is not str
                or not SHA256_RE.fullmatch(build_log["sha256"])):
            issues.append(
                "Shipping build receipt.buildLog.sha256 must be uppercase SHA-256"
            )
    require_typed_equal(
        verification_build_log, build_log,
        "Shipping verification/build receipt build-log binding", issues,
    )
    require_typed_equal(
        _candidate_identity_value(build, ("buildLog", "present")), True,
        "Shipping build receipt.buildLog.present", issues,
    )
    require_typed_equal(
        _candidate_identity_value(
            verification, ("buildLog", "requiredMarkersPresent")
        ), True, "Shipping verification build-log required markers", issues,
    )
    require_typed_equal(
        _candidate_identity_value(
            verification, ("buildLog", "forbiddenMarkersAbsent")
        ), True, "Shipping verification build-log forbidden markers", issues,
    )

    required_receipt_values = (
        (build, "schema", "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1", "Shipping build receipt"),
        (build, "schemaVersion", 1, "Shipping build receipt"),
        (build, "runId", candidate_id, "Shipping build receipt"),
        (build, "state", "BUILD_PASS_PROVENANCE_AND_ACCEPTANCE_PENDING", "Shipping build receipt"),
        (build, "uatExitCode", 0, "Shipping build receipt"),
        (build, "releaseReady", False, "Shipping build receipt"),
        (verification, "schema", "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2", "Shipping verification receipt"),
        (verification, "schemaVersion", 2, "Shipping verification receipt"),
        (verification, "runId", candidate_id, "Shipping verification receipt"),
        (verification, "state", "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING", "Shipping verification receipt"),
        (verification, "releaseReady", False, "Shipping verification receipt"),
        (plugin, "schema", "DiscGolfTour.Session19ShippingPluginCapabilityReceipt.v3", "plugin capability receipt"),
        (plugin, "schemaVersion", 3, "plugin capability receipt"),
        (plugin, "candidateId", candidate_id, "plugin capability receipt"),
        (plugin, "state", "PASS_REVIEWED_METADATA_ONLY_AND_DEPENDENCY_CLOSED_PLUGIN_DESCRIPTORS_IN_FINAL_PAYLOAD", "plugin capability receipt"),
        (binary, "schema", "DiscGolfTour.Session19ShippingBinaryEvidence.v1", "Shipping binary receipt"),
        (binary, "schemaVersion", 1, "Shipping binary receipt"),
        (binary, "session", 19, "Shipping binary receipt"),
        (binary, "state", "PASS_BINARY_MARKER_POLICY_ONLY", "Shipping binary receipt"),
        (binary, "passed", True, "Shipping binary receipt"),
        (binary, "releaseReady", False, "Shipping binary receipt"),
        (content, "schema", "DiscGolfTour.Session19CandidateContentAuditReceipt.v1", "candidate content receipt"),
        (content, "schemaVersion", 1, "candidate content receipt"),
        (content, "runId", candidate_id, "candidate content receipt"),
        (content, "state", "PASS_BOUNDED_STAGED_CONTENT_AUDIT", "candidate content receipt"),
        (notice, "schema", "DiscGolfTour.Session19RuntimeThirdPartyNoticeVerification.v1", "third-party notice receipt"),
        (notice, "schemaVersion", 1, "third-party notice receipt"),
        (notice, "session", 19, "third-party notice receipt"),
        (notice, "candidateId", candidate_id, "third-party notice receipt"),
        (notice, "state", "PASS_TECHNICAL_NOTICE_COVERAGE_LEGAL_APPROVAL_PENDING", "third-party notice receipt"),
    )
    for document, key, expected, label in required_receipt_values:
        require_typed_equal(document.get(key), expected, f"{label}.{key}", issues)

    expected_archive_token = f"DGTOUR_PACKAGES/{candidate_id}/Windows"
    require_typed_equal(
        _candidate_identity_value(content, ("archive", "recoveryLocationToken")),
        expected_archive_token, "candidate content archive recovery token", issues,
    )
    require_typed_equal(
        _candidate_identity_value(
            verification, ("archiveContract", "recoveryLocationToken")
        ), expected_archive_token,
        "Shipping verification archive recovery token", issues,
    )
    require_typed_equal(
        _candidate_identity_value(build, ("archive", "recoveryLocationToken")),
        f"DGTOUR_PACKAGES/{candidate_id}",
        "Shipping build archive recovery token", issues,
    )
    require_typed_equal(
        _binding_projection(verification.get("buildReceipt")),
        bindings.get("buildReceipt"),
        "Shipping verification build-receipt binding", issues,
    )
    require_typed_equal(
        _candidate_identity_value(verification, ("buildReceipt", "state")),
        build.get("state"), "Shipping verification/build receipt state", issues,
    )
    require_typed_equal(
        _candidate_identity_value(verification, ("buildReceipt", "uatExitCode")),
        build.get("uatExitCode"),
        "Shipping verification/build receipt UAT exit code", issues,
    )
    for actual, label in (
            (
                _candidate_identity_value(
                    build, ("shippingPluginCapabilities", "receipt")
                ), "Shipping build plugin-capability binding",
            ),
            (
                verification.get("shippingPluginCapabilities"),
                "Shipping verification plugin-capability binding",
            )):
        require_typed_equal(
            _binding_projection(actual), bindings.get("pluginCapabilityReceipt"),
            label, issues,
        )

    plugin_count_sources = {
        "reviewedDisabledPluginCount": ("reviewedDisabledPluginCount",),
        "reviewedMetadataOnlyDependencyPluginCount": (
            "reviewedMetadataOnlyDependencyPluginCount",
        ),
        "requiredDependencyClosurePluginCount": (
            "requiredDependencyClosurePluginCount",
        ),
        "requiredPreservedPluginCount": ("requiredPreservedPluginCount",),
        "requiredAbsentPluginCount": ("requiredAbsentPluginCount",),
        "reviewedResidualMetadataExclusionCount": (
            "reviewedResidualMetadataExclusionCount",
        ),
        "finalPayloadPluginDescriptorCount": (
            "finalPayload", "pluginDescriptorCount",
        ),
        "unreviewedEditorDeveloperOnlySurvivorCount": (
            "finalPayload", "unreviewedEditorDeveloperOnlySurvivorCount",
        ),
        "missingRequiredDependencyClosurePluginCount": (
            "finalPayload", "missingRequiredDependencyClosureDescriptorCount",
        ),
        "mandatoryDependencyGapCount": (
            "finalPayload", "mandatoryDependencyGapCount",
        ),
        "requiredAbsentSurvivorCount": (
            "finalPayload", "requiredAbsentSurvivorCount",
        ),
        "reviewedResidualMetadataSurvivorCount": (
            "finalPayload", "reviewedResidualMetadataSurvivorCount",
        ),
    }
    plugin_counts = {
        target: _nonnegative_document_int(
            plugin, source,
            f"plugin capability receipt.{'.'.join(source)}", issues,
        )
        for target, source in plugin_count_sources.items()
    }
    for key in (
            "unreviewedEditorDeveloperOnlySurvivorCount",
            "missingRequiredDependencyClosurePluginCount",
            "mandatoryDependencyGapCount", "requiredAbsentSurvivorCount",
            "reviewedResidualMetadataSurvivorCount"):
        require_typed_equal(plugin_counts[key], 0, f"plugin capability {key}", issues)
    plugin_boundary = plugin.get("releaseBoundary")
    require_typed_equal(
        _candidate_identity_value(
            plugin_boundary, ("technicalPluginDescriptorGatePass",)
        ), True, "plugin capability technical gate", issues,
    )
    require_typed_equal(
        _candidate_identity_value(plugin_boundary, ("releaseReady",)), False,
        "plugin capability releaseReady", issues,
    )
    require_typed_equal(
        _candidate_identity_value(plugin_boundary, ("blockerClosed",)), False,
        "plugin capability blockerClosed", issues,
    )
    require_typed_equal(plugin.get("issues"), [], "plugin capability issues", issues)

    plugin_archive_files = _candidate_identity_value(
        plugin, ("finalPayload", "archive", "files")
    )
    executable_rows = (
        [
            row for row in plugin_archive_files
            if type(row) is dict and row.get("path")
            == "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
        ]
        if type(plugin_archive_files) is list else []
    )
    if len(executable_rows) != 1:
        issues.append(
            "plugin capability final archive must contain exactly one Shipping executable"
        )
    else:
        require_typed_equal(
            executable_rows[0].get("bytes"), executable_bytes,
            "plugin capability Shipping executable bytes", issues,
        )
        _require_matching_sha256_digest(
            executable_rows[0].get("sha256"), executable_sha256,
            "plugin capability Shipping executable SHA-256", issues,
        )

    _require_matching_sha256_digest(
        _candidate_identity_value(binary, ("input", "sha256")),
        executable_sha256,
        "Shipping binary/verification executable SHA-256", issues,
    )
    require_typed_equal(
        _candidate_identity_value(binary, ("input", "bytes")), executable_bytes,
        "Shipping binary/verification executable bytes", issues,
    )
    forbidden_match_count = _nonnegative_document_int(
        binary, ("scan", "forbiddenMatchCount"),
        "Shipping binary receipt.scan.forbiddenMatchCount", issues,
    )
    require_typed_equal(
        forbidden_match_count, 0,
        "Shipping binary receipt forbidden-match count", issues,
    )
    require_typed_equal(
        _candidate_identity_value(binary, ("scan", "missingRequiredMarkerIds")),
        [], "Shipping binary receipt missing required markers", issues,
    )
    require_typed_equal(binary.get("errors"), [], "Shipping binary receipt.errors", issues)

    audited_identity_count = _nonnegative_document_int(
        content, ("inventory", "auditedIdentityCount"),
        "candidate content inventory auditedIdentityCount", issues,
    )
    authoritative_identity_count = _nonnegative_document_int(
        content, ("inventory", "authoritativeIdentityCount"),
        "candidate content inventory authoritativeIdentityCount", issues,
    )
    if (
            audited_identity_count is not None
            and authoritative_identity_count is not None
            and authoritative_identity_count > audited_identity_count):
        issues.append(
            "candidate content authoritative identity count exceeds audited count"
        )
    content_issues = content.get("issues")
    require_typed_equal(content_issues, [], "candidate content receipt.issues", issues)
    expected_content_boundary = {
        "boundedStagedContentAuditPassed": True,
        "freshInstallGameplayAcceptancePerformed": False,
        "provenanceClassificationPerformed": False,
        "legalOrVisualApprovalPerformed": False,
        "blockerClosed": False,
        "releaseReady": False,
    }
    require_typed_equal(
        content.get("releaseBoundary"), expected_content_boundary,
        "candidate content release boundary", issues,
    )

    notice_count_keys = (
        "supportedDllIdentityCount", "stagedDllIdentityCount",
        "mappedStagedDllIdentityCount", "unmappedStagedDllIdentityCount",
    )
    notice_counts = {
        key: _nonnegative_document_int(
            notice, ("coverage", key), f"third-party notice coverage.{key}", issues,
        )
        for key in notice_count_keys
    }
    require_typed_equal(
        notice_counts["stagedDllIdentityCount"],
        notice_counts["supportedDllIdentityCount"],
        "third-party notice supported/staged DLL count", issues,
    )
    require_typed_equal(
        notice_counts["mappedStagedDllIdentityCount"],
        notice_counts["stagedDllIdentityCount"],
        "third-party notice mapped/staged DLL count", issues,
    )
    require_typed_equal(
        notice_counts["unmappedStagedDllIdentityCount"], 0,
        "third-party notice unmapped DLL count", issues,
    )
    expected_notice_boundary = {
        "releaseReady": False,
        "legalApproval": False,
        "distributionClearance": False,
        "technicalCoverageOnly": True,
        "remainingDecision": "INDEPENDENT_LICENSE_REVIEW_AND_DISTRIBUTION_APPROVAL",
    }
    require_typed_equal(
        notice.get("releaseBoundary"), expected_notice_boundary,
        "third-party notice release boundary", issues,
    )

    if len(issues) != issue_count:
        return None
    assert build_log is not None
    assert validator_binding is not None
    assert audited_identity_count is not None
    assert authoritative_identity_count is not None
    assert forbidden_match_count is not None
    assert all(value is not None for value in plugin_counts.values())
    assert all(value is not None for value in notice_counts.values())

    current = selected_binding_expectation(
        EXPECTED_CURRENT_CANDIDATE_EVIDENCE, candidate_section,
        ("currentShippingCandidateEvidence",),
    )
    current.update({
        "candidateId": candidate_id,
        "archiveRecoveryLocationToken": expected_archive_token,
        "archiveCanonicalManifestSha256": archive_sha256.upper(),
        "buildLog": copy.deepcopy(build_log),
    })
    current["buildReceipt"].update({
        key: build[key]
        for key in ("schema", "schemaVersion", "state", "uatExitCode", "releaseReady")
    })
    current["verificationReceipt"].update({
        key: verification[key]
        for key in ("schema", "schemaVersion", "state", "releaseReady")
    })
    current["pluginCapabilityReceipt"].update({
        "schema": plugin["schema"],
        "schemaVersion": plugin["schemaVersion"],
        "state": plugin["state"],
        **plugin_counts,
        "technicalGatePass": plugin_boundary["technicalPluginDescriptorGatePass"],
        "releaseReady": plugin_boundary["releaseReady"],
        "blockerClosed": plugin_boundary["blockerClosed"],
    })
    current["binaryReceipt"].update({
        "schema": binary["schema"],
        "schemaVersion": binary["schemaVersion"],
        "state": binary["state"],
        "executableSha256": executable_sha256.upper(),
        "passed": binary["passed"],
        "forbiddenMatchCount": forbidden_match_count,
        "releaseReady": binary["releaseReady"],
    })
    current["contentReceipt"].update({
        "schema": content["schema"],
        "schemaVersion": content["schemaVersion"],
        "state": content["state"],
        "archiveFileCount": archive_file_count,
        "archiveBytes": archive_bytes,
        "auditedIdentityCount": audited_identity_count,
        "authoritativeIdentityCount": authoritative_identity_count,
        "issueCount": len(content_issues),
        **expected_content_boundary,
    })
    current["thirdPartyNoticeReceipt"].update({
        "schema": notice["schema"],
        "schemaVersion": notice["schemaVersion"],
        "state": notice["state"],
        **notice_counts,
        "technicalCoverageOnly": expected_notice_boundary["technicalCoverageOnly"],
        "legalApproval": expected_notice_boundary["legalApproval"],
        "distributionClearance": expected_notice_boundary["distributionClearance"],
        "releaseReady": expected_notice_boundary["releaseReady"],
    })
    current["provenanceClassificationDraft"].update({
        "schema": draft["schema"],
        "schemaVersion": draft["schemaVersion"],
        "state": draft["state"],
        "recordCount": origin_facts["recordCount"],
        "reviewed": False,
        "shippingApproved": False,
    })
    audit_classification = audit["classification"]
    audit_boundary = audit["releaseBoundary"]
    current["provenanceDraftAudit"].update({
        "schema": audit["schema"],
        "schemaVersion": audit["schemaVersion"],
        "status": audit["status"],
        "discoveredIdentityCount": audit_classification["discoveredIdentityCount"],
        "classifiedIdentityCount": audit_classification["classifiedIdentityCount"],
        "unclassifiedIdentityCount": audit_classification["unclassifiedIdentityCount"],
        "reasonCodes": copy.deepcopy(audit["reasonCodes"]),
        "evidenceRole": "HISTORICAL_PRE_ATTRIBUTION_FAIL_CLOSED_SNAPSHOT",
        "technicalOriginSupersededBy": origin_section["receipt"]["path"],
        "technicalInventoryComplete": audit_boundary["technicalInventoryComplete"],
        "releaseReady": audit_boundary["releaseReady"],
        "releaseUseAllowed": audit_boundary["releaseUseAllowed"],
        "blockerClosed": audit_boundary["blockerClosed"],
    })
    current["freshUserDirValidator"] = validator_binding
    current["freshUserDirValidator"]["selfTestCaseCount"] = fresh_self_test_count
    current["freshUserDirReceipt"].update(copy.deepcopy(fresh_facts))
    return current


_FRESH_USERDIR_SELF_TEST_COUNT: int | None = None


def _fresh_userdir_self_test_count(issues: list[str]) -> int | None:
    global _FRESH_USERDIR_SELF_TEST_COUNT
    if _FRESH_USERDIR_SELF_TEST_COUNT is not None:
        return _FRESH_USERDIR_SELF_TEST_COUNT
    try:
        import validate_dg_session19_fresh_userdir as fresh_validator

        result = fresh_validator._self_test()
    except Exception as exc:
        issues.append(
            "fresh UserDir validator self-test could not be independently run: "
            f"{type(exc).__name__}"
        )
        return None
    if (
            type(result) is not dict
            or set(result) != {"schema", "state", "testsPassed"}
            or result.get("state") != "PASS"
            or type(result.get("testsPassed")) is not int
            or type(result.get("testsPassed")) is bool
            or result["testsPassed"] < 22):
        issues.append("fresh UserDir validator self-test must pass at least 22 cases")
        return None
    _FRESH_USERDIR_SELF_TEST_COUNT = result["testsPassed"]
    return _FRESH_USERDIR_SELF_TEST_COUNT


def _validate_fresh_userdir_receipt(
        receipt: Any, candidate_id: str, issues: list[str]) -> dict[str, Any] | None:
    root_keys = {
        "schema", "schemaVersion", "session", "candidateId", "userDirToken",
        "candidateUserDirBindingSha256", "hostPathRecorded", "policy",
        "observedFiles", "counts", "failures", "state", "releaseBoundary",
        "runtimeJournalBinding",
    }
    if not require_exact_keys(receipt, root_keys, "fresh UserDir receipt", issues):
        return None
    try:
        import validate_dg_session19_fresh_userdir as fresh_validator
    except Exception as exc:
        issues.append(f"fresh UserDir validator import failed: {type(exc).__name__}")
        return None
    require_typed_equal(
        receipt["schema"], "DiscGolfTour.Session19FreshUserDirValidation.v1",
        "fresh UserDir receipt.schema", issues,
    )
    require_typed_equal(receipt["schemaVersion"], 1, "fresh UserDir receipt.schemaVersion", issues)
    require_typed_equal(receipt["session"], 19, "fresh UserDir receipt.session", issues)
    require_typed_equal(receipt["candidateId"], candidate_id, "fresh UserDir receipt.candidateId", issues)
    token = receipt["userDirToken"]
    if type(token) is not str or not fresh_validator.USERDIR_TOKEN_RE.fullmatch(token):
        issues.append("fresh UserDir receipt.userDirToken is malformed")
        token = "INVALID_TOKEN"
    expected_binding = fresh_validator._binding_sha256(candidate_id, token)
    require_typed_equal(
        receipt["candidateUserDirBindingSha256"], expected_binding,
        "fresh UserDir receipt.candidateUserDirBindingSha256", issues,
    )
    require_typed_equal(receipt["hostPathRecorded"], False, "fresh UserDir receipt.hostPathRecorded", issues)
    expected_policy = copy.deepcopy(
        fresh_validator._base_receipt(candidate_id, token)["policy"]
    )
    expected_policy["runtimeJournalAllowance"] = (
        "EXACT_CANDIDATE_BOUND_EXTERNAL_CAPTURE_ONLY"
    )
    require_typed_equal(receipt["policy"], expected_policy, "fresh UserDir receipt.policy", issues)
    require_typed_equal(
        receipt["state"], "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        "fresh UserDir receipt.state", issues,
    )
    require_typed_equal(receipt["failures"], [], "fresh UserDir receipt.failures", issues)
    require_typed_equal(
        receipt["releaseBoundary"], {
            "technicalUserDirAcceptanceOnly": True,
            "gameplayAcceptanceClaimed": False,
            "releaseReadinessClaimed": False,
        }, "fresh UserDir receipt.releaseBoundary", issues,
    )
    counts = receipt["counts"]
    count_keys = {"observedFiles", "approvedFiles", "unknownFiles", "forbiddenArtifacts"}
    if not require_exact_keys(counts, count_keys, "fresh UserDir receipt.counts", issues):
        counts = {}
    for key in count_keys:
        value = counts.get(key)
        if type(value) is not int or type(value) is bool or value < 0:
            issues.append(f"fresh UserDir receipt.counts.{key} must be a non-negative integer")
    observed = receipt["observedFiles"]
    if type(observed) is not list:
        issues.append("fresh UserDir receipt.observedFiles must be an array")
        observed = []
    observed_paths: list[str] = []
    for index, item in enumerate(observed):
        label = f"fresh UserDir receipt.observedFiles[{index}]"
        if not require_exact_keys(
                item, {"relativePath", "bytes", "sha256", "classification"},
                label, issues):
            continue
        path_value = item["relativePath"]
        if not is_canonical_relative_posix(path_value):
            issues.append(f"{label}.relativePath is not canonical")
        elif type(path_value) is str:
            observed_paths.append(path_value)
        if type(item["bytes"]) is not int or type(item["bytes"]) is bool or item["bytes"] <= 0:
            issues.append(f"{label}.bytes must be a positive integer")
        if type(item["sha256"]) is not str or not LOWER_SHA256_RE.fullmatch(item["sha256"]):
            issues.append(f"{label}.sha256 must be lowercase SHA-256")
        if type(item["classification"]) is not str or not item["classification"]:
            issues.append(f"{label}.classification must be a non-empty string")
    if observed_paths != sorted(observed_paths, key=str.casefold):
        issues.append("fresh UserDir observed-file paths are not in canonical order")
    if len({path.casefold() for path in observed_paths}) != len(observed_paths):
        issues.append("fresh UserDir receipt contains duplicate or case-colliding paths")
    require_typed_equal(counts.get("observedFiles"), len(observed), "fresh UserDir observed count", issues)
    require_typed_equal(counts.get("approvedFiles"), len(observed), "fresh UserDir approved count", issues)
    require_typed_equal(counts.get("unknownFiles"), 0, "fresh UserDir unknown count", issues)
    require_typed_equal(counts.get("forbiddenArtifacts"), 0, "fresh UserDir forbidden count", issues)

    runtime = receipt["runtimeJournalBinding"]
    runtime_keys = {
        "mode", "userDirRelativePath", "bytes", "sha256", "captureNonce",
        "roundId", "launchRecordSha256", "technicalEvidenceManifestSha256",
        "executableSha256", "archiveManifestSha256", "validationState",
    }
    if not require_exact_keys(runtime, runtime_keys, "fresh UserDir runtimeJournalBinding", issues):
        return None
    require_typed_equal(runtime["mode"], "CANDIDATE_BOUND_EXTERNAL_CAPTURE", "fresh runtime mode", issues)
    require_typed_equal(
        runtime["userDirRelativePath"], fresh_validator.RUNTIME_JOURNAL_RELATIVE,
        "fresh runtime journal path", issues,
    )
    require_typed_equal(
        runtime["validationState"], fresh_validator.RUNTIME_JOURNAL_PASS,
        "fresh runtime validation state", issues,
    )
    if type(runtime["bytes"]) is not int or type(runtime["bytes"]) is bool or runtime["bytes"] <= 0:
        issues.append("fresh runtime journal bytes must be a positive integer")
    for key in (
            "sha256", "launchRecordSha256", "technicalEvidenceManifestSha256",
            "executableSha256", "archiveManifestSha256"):
        if type(runtime[key]) is not str or not SHA256_RE.fullmatch(runtime[key]):
            issues.append(f"fresh runtime {key} must be uppercase SHA-256")
    uuid4 = re.compile(
        r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
    )
    for key in ("captureNonce", "roundId"):
        if type(runtime[key]) is not str or not uuid4.fullmatch(runtime[key]):
            issues.append(f"fresh runtime {key} must be a lowercase UUIDv4")
    journal_rows = [
        item for item in observed
        if type(item) is dict
        and item.get("relativePath") == fresh_validator.RUNTIME_JOURNAL_RELATIVE
    ]
    if len(journal_rows) != 1:
        issues.append("fresh UserDir receipt must contain exactly one bound runtime journal")
    else:
        require_typed_equal(
            journal_rows[0].get("classification"),
            "runtime_checkpoint_journal_candidate_bound",
            "fresh runtime journal classification", issues,
        )
        require_typed_equal(journal_rows[0].get("bytes"), runtime["bytes"], "fresh runtime journal bytes", issues)
        observed_hash = journal_rows[0].get("sha256")
        require_typed_equal(
            observed_hash.upper() if type(observed_hash) is str else observed_hash,
            runtime["sha256"], "fresh runtime journal SHA-256", issues,
        )
    return {
        "schema": receipt["schema"],
        "schemaVersion": receipt["schemaVersion"],
        "state": receipt["state"],
        "observedFileCount": counts.get("observedFiles"),
        "approvedFileCount": counts.get("approvedFiles"),
        "unknownFileCount": counts.get("unknownFiles"),
        "forbiddenArtifactCount": counts.get("forbiddenArtifacts"),
        "failureCount": len(receipt["failures"]),
        "technicalUserDirAcceptanceOnly": True,
        "gameplayAcceptanceClaimed": False,
        "releaseReadinessClaimed": False,
        "runtimeJournalBinding": copy.deepcopy(runtime),
    }


def _require_matching_binding(
        actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    require_typed_equal(
        _binding_projection(actual), _binding_projection(expected), label, issues,
    )


def _validate_origin_profile_documents(
        *, candidate_id: str, policy: Any, draft: Any, draft_audit: Any,
        proposal: Any, receipt: Any, current: dict[str, Any],
        staged: dict[str, Any], origin: dict[str, Any], issues: list[str],
) -> dict[str, Any] | None:
    draft_root_keys = {"schema", "schemaVersion", "candidateId", "state", "records"}
    if not require_exact_keys(draft, draft_root_keys, "provenance classification draft", issues):
        return None
    require_typed_equal(
        draft["schema"], "DiscGolfTour.Session19StagedProvenanceClassification.v1",
        "provenance classification draft.schema", issues,
    )
    require_typed_equal(draft["schemaVersion"], 1, "provenance classification draft.schemaVersion", issues)
    require_typed_equal(draft["candidateId"], candidate_id, "provenance classification draft.candidateId", issues)
    require_typed_equal(
        draft["state"], "DRAFT_UNREVIEWED_NOT_SHIPPING_APPROVED",
        "provenance classification draft.state", issues,
    )
    draft_records = draft["records"]
    if type(draft_records) is not list or not draft_records:
        issues.append("provenance classification draft.records must be a non-empty array")
        return None
    draft_record_keys = {
        "scope", "container", "identity", "classification", "authorityId",
        "licenseId", "shippingApproved", "evidencePaths",
    }
    draft_identities: list[tuple[Any, Any, Any]] = []
    for index, record in enumerate(draft_records):
        label = f"provenance classification draft.records[{index}]"
        if not require_exact_keys(record, draft_record_keys, label, issues):
            continue
        require_typed_equal(record["classification"], "UNCLASSIFIED", f"{label}.classification", issues)
        require_typed_equal(record["authorityId"], "PENDING", f"{label}.authorityId", issues)
        require_typed_equal(record["licenseId"], "PENDING", f"{label}.licenseId", issues)
        require_typed_equal(record["shippingApproved"], False, f"{label}.shippingApproved", issues)
        scope = record["scope"]
        container = record["container"]
        identity = record["identity"]
        if type(scope) is not str or not scope:
            issues.append(f"{label}.scope must be a non-empty string")
            scope = f"INVALID_SCOPE_{index}"
        if container is not None and (type(container) is not str or not container):
            issues.append(f"{label}.container must be null or a non-empty string")
            container = f"INVALID_CONTAINER_{index}"
        if type(identity) is not str or not identity:
            issues.append(f"{label}.identity must be a non-empty string")
            identity = f"INVALID_IDENTITY_{index}"
        draft_identities.append((scope, container, identity))
    if len(set(draft_identities)) != len(draft_identities):
        issues.append("provenance classification draft contains duplicate identities")

    audit_root_keys = {
        "schema", "schemaVersion", "session", "candidateId", "generatedUtc",
        "status", "target", "archive", "uatLog", "stageManifests", "unrealPak",
        "containers", "classification", "reasonCodes", "issues", "releaseBoundary",
    }
    if not require_exact_keys(draft_audit, audit_root_keys, "provenance draft audit", issues):
        return None
    require_typed_equal(
        draft_audit["schema"], "DiscGolfTour.Session19StagedProvenanceReceipt.v1",
        "provenance draft audit.schema", issues,
    )
    require_typed_equal(draft_audit["schemaVersion"], 1, "provenance draft audit.schemaVersion", issues)
    require_typed_equal(draft_audit["session"], 19, "provenance draft audit.session", issues)
    require_typed_equal(draft_audit["candidateId"], candidate_id, "provenance draft audit.candidateId", issues)
    require_typed_equal(draft_audit["status"], "FAIL_CLOSED", "provenance draft audit.status", issues)
    classification = draft_audit["classification"]
    if require_exact_keys(
            classification,
            {"manifest", "discoveredIdentityCount", "classifiedIdentityCount", "unclassifiedIdentityCount"},
            "provenance draft audit.classification", issues):
        require_typed_equal(classification["manifest"], None, "provenance draft audit manifest", issues)
        require_typed_equal(
            classification["discoveredIdentityCount"], len(draft_records),
            "provenance draft audit discovered count", issues,
        )
        require_typed_equal(classification["classifiedIdentityCount"], 0, "provenance draft audit classified count", issues)
        require_typed_equal(
            classification["unclassifiedIdentityCount"], len(draft_records),
            "provenance draft audit unclassified count", issues,
        )
    require_typed_equal(
        draft_audit["reasonCodes"],
        ["CLASSIFICATION_MANIFEST_MISSING", "UNCLASSIFIED_IDENTITY"],
        "provenance draft audit.reasonCodes", issues,
    )
    audit_issues = draft_audit["issues"]
    if (
            type(audit_issues) is not list
            or [item.get("code") if type(item) is dict else None for item in audit_issues]
            != ["CLASSIFICATION_MANIFEST_MISSING", "UNCLASSIFIED_IDENTITY"]):
        issues.append("provenance draft audit issues do not match its fail-closed reason codes")
    boundary = draft_audit["releaseBoundary"]
    if type(boundary) is not dict:
        issues.append("provenance draft audit.releaseBoundary must be an object")
    else:
        for key in ("technicalInventoryComplete", "releaseReady", "releaseUseAllowed", "blockerClosed"):
            require_typed_equal(boundary.get(key), False, f"provenance draft audit.releaseBoundary.{key}", issues)

    proposal_root_keys = {
        "schema", "schemaVersion", "candidateId", "state", "purpose", "reviewed",
        "shippingApproved", "legalReviewed", "legalApproved", "licenseApproved",
        "distributionApproved", "authoritativeClassificationCompatible", "inputBinding",
        "scopePolicy", "technicalEvidence", "ruleCatalog", "records", "summary",
        "releaseBoundary",
    }
    if not require_exact_keys(proposal, proposal_root_keys, "technical-origin proposal", issues):
        return None
    require_typed_equal(
        proposal["schema"], "DiscGolfTour.Session19TechnicalOriginProposal.v1",
        "technical-origin proposal.schema", issues,
    )
    require_typed_equal(proposal["schemaVersion"], 1, "technical-origin proposal.schemaVersion", issues)
    require_typed_equal(proposal["candidateId"], candidate_id, "technical-origin proposal.candidateId", issues)
    for key in (
            "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
            "licenseApproved", "distributionApproved", "authoritativeClassificationCompatible"):
        require_typed_equal(proposal[key], False, f"technical-origin proposal.{key}", issues)
    proposal_records = proposal["records"]
    if type(proposal_records) is not list:
        issues.append("technical-origin proposal.records must be an array")
        proposal_records = []
    proposal_identities: list[tuple[Any, Any, Any]] = []
    for index, record in enumerate(proposal_records):
        if type(record) is not dict:
            issues.append(f"technical-origin proposal.records[{index}] must be an object")
            proposal_identities.append((f"INVALID_{index}", None, f"INVALID_{index}"))
            continue
        proposal_identities.append(
            (record.get("scope"), record.get("container"), record.get("identity"))
        )
    require_typed_equal(proposal_identities, draft_identities, "technical-origin proposal identity order", issues)
    proposal_summary = proposal["summary"]
    if type(proposal_summary) is not dict:
        issues.append("technical-origin proposal.summary must be an object")
    else:
        require_typed_equal(
            proposal_summary.get("sourceRecordCount"), len(draft_records),
            "technical-origin proposal sourceRecordCount", issues,
        )

    receipt_root_keys = {
        "schema", "schemaVersion", "session", "candidateId", "state", "purpose",
        "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
        "licenseApproved", "distributionApproved", "releaseReady",
        "authoritativeProvenanceClassificationCompatible", "inputBindings",
        "liveCandidateInventory", "technicalSourceManifests", "records", "summary",
        "releaseBoundary",
    }
    if not require_exact_keys(receipt, receipt_root_keys, "technical-origin receipt", issues):
        return None
    require_typed_equal(
        receipt["schema"], "DiscGolfTour.Session19TechnicalOriginAttributionAudit.v1",
        "technical-origin receipt.schema", issues,
    )
    require_typed_equal(receipt["schemaVersion"], 1, "technical-origin receipt.schemaVersion", issues)
    require_typed_equal(receipt["session"], 19, "technical-origin receipt.session", issues)
    require_typed_equal(receipt["candidateId"], candidate_id, "technical-origin receipt.candidateId", issues)
    require_typed_equal(
        receipt["state"], "PASS_EXACT_TECHNICAL_ATTRIBUTION_LEGAL_REVIEW_PENDING",
        "technical-origin receipt.state", issues,
    )
    for key in (
            "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
            "licenseApproved", "distributionApproved", "releaseReady",
            "authoritativeProvenanceClassificationCompatible"):
        require_typed_equal(receipt[key], False, f"technical-origin receipt.{key}", issues)

    input_bindings = receipt["inputBindings"]
    input_binding_keys = {
        "validator", "policy", "stagedProvenancePolicy", "sourceDraft",
        "technicalOriginProposal", "candidateContent", "thirdPartyAuthority",
        "polyHavenDerivedReceipt", "shippingBuildReceipt", "engineBuildVersion",
    }
    if require_exact_keys(input_bindings, input_binding_keys, "technical-origin receipt.inputBindings", issues):
        for key, expected in (
            ("validator", origin.get("validator")),
            ("policy", origin.get("policy")),
            ("stagedProvenancePolicy", staged),
            ("sourceDraft", current.get("provenanceClassificationDraft")),
            ("technicalOriginProposal", origin.get("proposal")),
            ("candidateContent", current.get("contentReceipt")),
        ):
            _require_matching_binding(
                input_bindings.get(key), expected,
                f"technical-origin input binding {key}", issues,
            )

    records = receipt["records"]
    if type(records) is not list or not records:
        issues.append("technical-origin receipt.records must be a non-empty array")
        return None
    record_keys = {
        "scope", "container", "identity", "technicalDisposition", "originCategory",
        "originRuleId", "licenseSourceCategory", "technicalAuthorityIds",
        "sourceProposalRuleId", "sourceProposalConfidence", "reviewed",
        "shippingApproved", "legalReviewed", "legalApproved", "licenseApproved",
        "distributionApproved",
    }
    receipt_identities: list[tuple[Any, Any, Any]] = []
    attributed_disposition = "AUTHORITATIVELY_ATTRIBUTED_TECHNICAL_ORIGIN_ONLY"
    unresolved_anon_disposition = "UNRESOLVED_ANONYMOUS_IDENTITY"
    unresolved_path_disposition = "UNRESOLVED_NO_AUTHORITATIVE_TECHNICAL_ORIGIN"
    allowed_dispositions = {
        attributed_disposition, unresolved_anon_disposition, unresolved_path_disposition,
    }
    for index, record in enumerate(records):
        label = f"technical-origin receipt.records[{index}]"
        if not require_exact_keys(record, record_keys, label, issues):
            continue
        scope = record["scope"]
        container = record["container"]
        identity = record["identity"]
        if type(scope) is not str or not scope:
            issues.append(f"{label}.scope must be a non-empty string")
            scope = f"INVALID_SCOPE_{index}"
        if container is not None and (type(container) is not str or not container):
            issues.append(f"{label}.container must be null or a non-empty string")
            container = f"INVALID_CONTAINER_{index}"
        if type(identity) is not str or not identity:
            issues.append(f"{label}.identity must be a non-empty string")
            identity = f"INVALID_IDENTITY_{index}"
        receipt_identities.append((scope, container, identity))
        if type(record["technicalDisposition"]) is not str or record["technicalDisposition"] not in allowed_dispositions:
            issues.append(f"{label}.technicalDisposition is unknown")
        if record["technicalDisposition"] == attributed_disposition:
            if type(record["originCategory"]) is not str or not record["originCategory"]:
                issues.append(f"{label} is attributed without an origin category")
            if (
                    type(record["licenseSourceCategory"]) is not str
                    or record["licenseSourceCategory"] == "UNRESOLVED_NO_LICENSE_SOURCE"):
                issues.append(f"{label} is attributed without a license-source category")
        elif record["originCategory"] is not None:
            issues.append(f"{label} is unresolved but has an origin category")
        for key in (
                "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
                "licenseApproved", "distributionApproved"):
            require_typed_equal(record[key], False, f"{label}.{key}", issues)
    require_typed_equal(receipt_identities, draft_identities, "technical-origin receipt identity order", issues)

    audited = len(records)
    attributed = sum(record.get("technicalDisposition") == attributed_disposition for record in records)
    unresolved_anon = sum(record.get("technicalDisposition") == unresolved_anon_disposition for record in records)
    unresolved_path = sum(record.get("technicalDisposition") == unresolved_path_disposition for record in records)
    resolved_anon = sum(
        record.get("scope") == "CONTAINER_ANONYMOUS_CHUNK"
        and record.get("technicalDisposition") == attributed_disposition
        for record in records
    )
    def sorted_string_counts(values: list[Any], label: str) -> dict[str, int]:
        normalized: list[str] = []
        for index, value in enumerate(values):
            if type(value) is not str or not value:
                issues.append(f"{label}[{index}] must be a non-empty string")
                normalized.append(f"INVALID_{index}")
            else:
                normalized.append(value)
        return dict(sorted(Counter(normalized).items()))

    summary_expected = {
        "auditedIdentityCount": audited,
        "authoritativelyAttributedTechnicalOriginCount": attributed,
        "unresolvedIdentityCount": unresolved_anon + unresolved_path,
        "resolvedAnonymousChunkTechnicalIdentityCount": resolved_anon,
        "unresolvedAnonymousChunkCount": unresolved_anon,
        "unresolvedPathIdentityCount": unresolved_path,
        "scopeCounts": sorted_string_counts(
            [record.get("scope") for record in records], "technical-origin scopes"
        ),
        "technicalDispositionCounts": sorted_string_counts(
            [record.get("technicalDisposition") for record in records],
            "technical-origin dispositions",
        ),
        "originCategoryCounts": sorted_string_counts(
            [
                record.get("originCategory") for record in records
                if record.get("originCategory") is not None
            ], "technical-origin categories",
        ),
        "licenseSourceCategoryCounts": sorted_string_counts(
            [record.get("licenseSourceCategory") for record in records],
            "technical-origin license categories",
        ),
        "legalReviewedRecordCount": 0,
        "shippingApprovedRecordCount": 0,
        "distributionApprovedRecordCount": 0,
    }
    require_typed_equal(receipt["summary"], summary_expected, "technical-origin receipt.summary", issues)
    if attributed != audited or unresolved_anon != 0 or unresolved_path != 0:
        issues.append("fresh candidate profile requires exhaustive technical-origin attribution")
    remaining = [
        "INDEPENDENT_REVIEWED_EXACT_IDENTITY_CLASSIFICATION",
        "LEGAL_AND_LICENSE_APPROVAL",
    ]
    if unresolved_anon:
        remaining.append("ANONYMOUS_IOSTORE_IDENTITY_RESOLUTION_OR_ACCEPTED_AUTHORITY")
    remaining.append("DISTRIBUTION_CLEARANCE")
    require_typed_equal(
        receipt["releaseBoundary"], {
            "technicalAttributionIsLegalClassification": False,
            "independentReviewComplete": False,
            "legalReviewComplete": False,
            "distributionClearanceComplete": False,
            "shippingApprovalGranted": False,
            "releaseReady": False,
            "releaseUseAllowed": False,
            "blockerClosed": False,
            "remainingBlockers": remaining,
        }, "technical-origin receipt.releaseBoundary", issues,
    )

    if type(policy) is not dict:
        issues.append("technical-origin policy must be an object")
    else:
        policy_inputs = policy.get("inputs")
        policy_count = (
            policy_inputs.get("expectedAuditedIdentityCount")
            if type(policy_inputs) is dict else None
        )
        require_typed_equal(policy_count, audited, "technical-origin policy expected identity count", issues)
        try:
            import validate_dg_session19_technical_origin_attribution as origin_validator

            policy_without_candidate_count = copy.deepcopy(policy)
            if type(policy_without_candidate_count.get("inputs")) is dict:
                # The origin validator's historical policy function freezes only this
                # candidate count. Validate every other exact policy clause through it.
                policy_without_candidate_count["inputs"]["expectedAuditedIdentityCount"] = 7158
            origin_validator.validate_policy_document(policy_without_candidate_count)
        except Exception as exc:
            issues.append(
                "technical-origin policy validation failed outside its candidate count: "
                f"{type(exc).__name__}: {exc}"
            )
    return {
        "recordCount": len(draft_records),
        "discoveredIdentityCount": len(draft_records),
        "classifiedIdentityCount": 0,
        "unclassifiedIdentityCount": len(draft_records),
        "auditedIdentityCount": audited,
        "authoritativelyAttributedTechnicalOriginCount": attributed,
        "unresolvedIdentityCount": unresolved_anon + unresolved_path,
        "unresolvedAnonymousChunkCount": unresolved_anon,
        "unresolvedPathIdentityCount": unresolved_path,
        "state": receipt["state"],
    }


def _normalized_sha256_digest(
        value: Any, label: str, issues: list[str],
) -> str | None:
    """Return one case-normalized digest after strict 64-hex validation."""
    if type(value) is not str or re.fullmatch(r"[0-9A-Fa-f]{64}", value) is None:
        issues.append(f"{label} must be a canonical 64-hex SHA-256 digest")
        return None
    return value.upper()


def _require_matching_sha256_digest(
        actual: Any, expected: Any, label: str, issues: list[str],
) -> None:
    actual_digest = _normalized_sha256_digest(actual, f"{label} actual", issues)
    expected_digest = _normalized_sha256_digest(
        expected, f"{label} expected", issues,
    )
    if (
            actual_digest is not None
            and expected_digest is not None
            and actual_digest != expected_digest):
        issues.append(
            f"{label} differs: expected {expected_digest!r}, got {actual_digest!r}"
        )


def _validate_three_hole_final_score(
        value: Any, hole_pars: Any, issues: list[str],
) -> None:
    score_keys = {
        "completedHoles", "totalHoles", "parTotal", "totalStrokes",
        "totalPenalties", "holeRows",
    }
    if not require_exact_keys(
            value, score_keys, "three-hole receipt finalScore", issues):
        return
    if (
            type(hole_pars) is not list
            or not hole_pars
            or any(type(par) is not int or par <= 0 for par in hole_pars)):
        issues.append("three-hole v2 policy hole pars must be positive integers")
        return

    expected_holes = len(hole_pars)
    expected_par_total = sum(hole_pars)
    scalar_rules = (
        ("completedHoles", expected_holes, 0),
        ("totalHoles", expected_holes, 1),
        ("parTotal", expected_par_total, 1),
    )
    for key, expected, minimum in scalar_rules:
        actual = value[key]
        if type(actual) is not int or actual < minimum:
            issues.append(
                f"three-hole receipt finalScore.{key} must be an integer >= {minimum}"
            )
        elif actual != expected:
            issues.append(
                f"three-hole receipt finalScore.{key} differs: "
                f"expected {expected!r}, got {actual!r}"
            )

    total_strokes = value["totalStrokes"]
    total_penalties = value["totalPenalties"]
    if type(total_strokes) is not int or total_strokes < 1:
        issues.append(
            "three-hole receipt finalScore.totalStrokes must be an integer >= 1"
        )
    if type(total_penalties) is not int or total_penalties < 0:
        issues.append(
            "three-hole receipt finalScore.totalPenalties must be an integer >= 0"
        )
    if (
            type(total_strokes) is int
            and type(total_penalties) is int
            and not isinstance(total_strokes, bool)
            and not isinstance(total_penalties, bool)
            and total_penalties > total_strokes):
        issues.append(
            "three-hole receipt finalScore penalties exceed total strokes"
        )

    rows = value["holeRows"]
    if type(rows) is not list or len(rows) != expected_holes:
        issues.append(
            "three-hole receipt finalScore.holeRows must contain exactly "
            f"{expected_holes} rows"
        )
        return
    row_strokes = 0
    row_penalties = 0
    rows_valid_for_totals = True
    for index, row_value in enumerate(rows, start=1):
        row_keys = {"holeNumber", "par", "strokes", "penalties"}
        if not require_exact_keys(
                row_value, row_keys,
                f"three-hole receipt finalScore row {index}", issues):
            rows_valid_for_totals = False
            continue
        row_rules = (
            ("holeNumber", index, 1),
            ("par", hole_pars[index - 1], 1),
        )
        for key, expected, minimum in row_rules:
            actual = row_value[key]
            if type(actual) is not int or actual < minimum:
                issues.append(
                    f"three-hole receipt finalScore row {index}.{key} "
                    f"must be an integer >= {minimum}"
                )
                rows_valid_for_totals = False
            elif actual != expected:
                issues.append(
                    f"three-hole receipt finalScore row {index}.{key} differs: "
                    f"expected {expected!r}, got {actual!r}"
                )
                rows_valid_for_totals = False
        strokes = row_value["strokes"]
        penalties = row_value["penalties"]
        if type(strokes) is not int or strokes < 1:
            issues.append(
                f"three-hole receipt finalScore row {index}.strokes "
                "must be an integer >= 1"
            )
            rows_valid_for_totals = False
        if type(penalties) is not int or penalties < 0:
            issues.append(
                f"three-hole receipt finalScore row {index}.penalties "
                "must be an integer >= 0"
            )
            rows_valid_for_totals = False
        if (
                type(strokes) is int
                and type(penalties) is int
                and not isinstance(strokes, bool)
                and not isinstance(penalties, bool)):
            if penalties > strokes:
                issues.append(
                    f"three-hole receipt finalScore row {index} penalties exceed strokes"
                )
                rows_valid_for_totals = False
            row_strokes += strokes
            row_penalties += penalties

    if rows_valid_for_totals and type(total_strokes) is int and not isinstance(
            total_strokes, bool) and row_strokes != total_strokes:
        issues.append(
            "three-hole receipt finalScore totalStrokes does not equal hole-row sum"
        )
    if rows_valid_for_totals and type(total_penalties) is int and not isinstance(
            total_penalties, bool) and row_penalties != total_penalties:
        issues.append(
            "three-hole receipt finalScore totalPenalties does not equal hole-row sum"
        )


def _validate_three_hole_v2_documents(
        policy: Any, receipt: Any, fresh_receipt: Any, candidate_id: str,
        policy_binding: Any, receipt_binding: Any, issues: list[str],
) -> dict[str, Any] | None:
    try:
        import validate_dg_session19_three_hole_technical_acceptance as three_hole

        three_hole._validate_policy_v2(policy)
    except Exception as exc:
        issues.append(f"three-hole v2 policy validation failed: {type(exc).__name__}: {exc}")
        return None
    receipt_keys = {
        "schema", "schemaVersion", "session", "candidateId", "verifiedUtc", "state",
        "policy", "candidate", "projectEvidenceBindings", "externalEvidence",
        "freshUserDir", "process", "artifactIntegrity", "checkpointChain",
        "runtimeJournal", "technicalAcceptance", "continuityClaims", "scoreClaims",
        "claimBoundary", "blockerClosed", "releaseReady", "remainingEvidence",
        "receiptPath",
    }
    if not require_exact_keys(receipt, receipt_keys, "three-hole v2 receipt", issues):
        return None
    require_typed_equal(receipt["schema"], three_hole.RECEIPT_SCHEMA, "three-hole receipt.schema", issues)
    require_typed_equal(receipt["schemaVersion"], 2, "three-hole receipt.schemaVersion", issues)
    require_typed_equal(receipt["session"], 19, "three-hole receipt.session", issues)
    require_typed_equal(receipt["candidateId"], candidate_id, "three-hole receipt.candidateId", issues)
    require_typed_equal(receipt["state"], three_hole.RUNTIME_JOURNAL_STATE, "three-hole receipt.state", issues)
    require_typed_equal(receipt["technicalAcceptance"], True, "three-hole receipt.technicalAcceptance", issues)
    require_typed_equal(receipt["blockerClosed"], False, "three-hole receipt.blockerClosed", issues)
    require_typed_equal(receipt["releaseReady"], False, "three-hole receipt.releaseReady", issues)
    require_typed_equal(
        receipt["receiptPath"], receipt_binding.get("path") if type(receipt_binding) is dict else None,
        "three-hole receipt.receiptPath", issues,
    )
    receipt_policy = receipt["policy"]
    if require_exact_keys(
            receipt_policy, {"path", "bytes", "sha256", "policyId"},
            "three-hole receipt.policy", issues):
        _require_matching_binding(receipt_policy, policy_binding, "three-hole receipt policy binding", issues)
        require_typed_equal(receipt_policy["policyId"], policy["policyId"], "three-hole receipt policyId", issues)
    require_typed_equal(receipt["claimBoundary"], policy["claimBoundary"], "three-hole receipt.claimBoundary", issues)
    claim_boundary = receipt["claimBoundary"]
    if type(claim_boundary) is not dict:
        issues.append("three-hole receipt.claimBoundary must be an object")
        return None
    require_typed_equal(
        claim_boundary.get("evidenceTrustModel") if type(claim_boundary) is dict else None,
        three_hole.EVIDENCE_TRUST_MODEL, "three-hole evidence trust model", issues,
    )
    for key in (
            "cryptographicProcessAttestation", "tamperProofEvidence",
            "hostileSameUserForgeryResistance"):
        require_typed_equal(
            claim_boundary.get(key) if type(claim_boundary) is dict else None,
            False, f"three-hole claimBoundary.{key}", issues,
        )
    continuity = receipt["continuityClaims"]
    continuity_keys = {
        "checkpointChainSupplied", "checkpointChainStructuralIntegrityValidated",
        "derivedCheckpointMayPromote", "runtimeJournalSupplied",
        "runtimeJournalCanonicalBytesValidated", "runtimeOperationalBindingValidated",
        "captureWindowBindingPresent", "singleRoundProven",
        "freshRoundEntryToFinalScorecardSequencePass",
        "eachHoleTeeAndRecoveredLieObserved", "threeHoleCompletionObserved",
    }
    if require_exact_keys(continuity, continuity_keys, "three-hole receipt.continuityClaims", issues):
        require_typed_equal(continuity["derivedCheckpointMayPromote"], False, "three-hole derived checkpoint promotion", issues)
        for key in continuity_keys - {
                "checkpointChainSupplied", "checkpointChainStructuralIntegrityValidated",
                "derivedCheckpointMayPromote"}:
            require_typed_equal(continuity[key], True, f"three-hole continuityClaims.{key}", issues)
        if continuity["checkpointChainSupplied"] is not continuity["checkpointChainStructuralIntegrityValidated"]:
            issues.append("three-hole checkpoint supplied/integrity claims disagree")
    score = receipt["scoreClaims"]
    score_keys = {
        "finalScorecardScoreProven", "completedHolesProven", "totalHolesProven",
        "holeRowsProven", "roundId", "finalScore",
    }
    if require_exact_keys(score, score_keys, "three-hole receipt.scoreClaims", issues):
        for key in (
                "finalScorecardScoreProven", "completedHolesProven", "totalHolesProven",
                "holeRowsProven"):
            require_typed_equal(score[key], True, f"three-hole scoreClaims.{key}", issues)
        _validate_three_hole_final_score(
            score["finalScore"], policy["runtimeJournal"]["holePars"], issues,
        )
        if type(score["roundId"]) is not str or not three_hole.UUID4_RE.fullmatch(score["roundId"]):
            issues.append("three-hole score roundId must be a lowercase UUIDv4")
    runtime = receipt["runtimeJournal"]
    runtime_keys = {
        "path", "hostPathRecorded", "userDirRecoveryLocationToken", "bytes", "sha256",
        "schema", "candidateId", "userDirToken", "executableSha256",
        "archiveManifestSha256", "launchRecordSha256",
        "technicalEvidenceManifestSha256", "captureNonce", "roundId", "eventCount",
        "firstSequence", "lastSequence", "startedUtc", "modifiedUtc",
        "canonicalJsonLinesValidated", "freshUserDirReceiptBindingValidated",
        "launchRecordBindingValidated", "technicalManifestBindingValidated",
        "captureWindowValidated", "runtimeOperationalBindingValidated",
    }
    if not require_exact_keys(runtime, runtime_keys, "three-hole receipt.runtimeJournal", issues):
        return None
    require_typed_equal(runtime["schema"], three_hole.RUNTIME_JOURNAL_SCHEMA, "three-hole runtime schema", issues)
    require_typed_equal(runtime["candidateId"], candidate_id, "three-hole runtime candidateId", issues)
    require_typed_equal(runtime["eventCount"], 12, "three-hole runtime eventCount", issues)
    require_typed_equal(runtime["firstSequence"], 1, "three-hole runtime firstSequence", issues)
    require_typed_equal(runtime["lastSequence"], 12, "three-hole runtime lastSequence", issues)
    require_typed_equal(runtime["hostPathRecorded"], False, "three-hole runtime hostPathRecorded", issues)
    for key in (
            "canonicalJsonLinesValidated", "freshUserDirReceiptBindingValidated",
            "launchRecordBindingValidated", "technicalManifestBindingValidated",
            "captureWindowValidated", "runtimeOperationalBindingValidated"):
        require_typed_equal(runtime[key], True, f"three-hole runtime {key}", issues)
    require_typed_equal(runtime["roundId"], score.get("roundId") if type(score) is dict else None, "three-hole runtime roundId", issues)

    fresh_runtime = fresh_receipt.get("runtimeJournalBinding") if type(fresh_receipt) is dict else None
    if type(fresh_runtime) is not dict:
        issues.append("three-hole receipt has no independently validated fresh runtime binding")
    else:
        for runtime_key, fresh_key in (
                ("sha256", "sha256"),
                ("captureNonce", "captureNonce"),
                ("roundId", "roundId"),
                ("launchRecordSha256", "launchRecordSha256"),
                ("technicalEvidenceManifestSha256", "technicalEvidenceManifestSha256"),
                ("executableSha256", "executableSha256"),
                ("archiveManifestSha256", "archiveManifestSha256")):
            require_typed_equal(
                runtime[runtime_key], fresh_runtime.get(fresh_key),
                f"three-hole/fresh runtime {runtime_key}", issues,
            )
        require_typed_equal(runtime["userDirToken"], fresh_receipt.get("userDirToken"), "three-hole/fresh userDirToken", issues)
    fresh_summary = receipt["freshUserDir"]
    if type(fresh_summary) is not dict:
        issues.append("three-hole receipt.freshUserDir must be an object")
    else:
        require_typed_equal(fresh_summary.get("token"), fresh_receipt.get("userDirToken"), "three-hole fresh token", issues)
        require_typed_equal(
            fresh_summary.get("observedFileCount"), fresh_receipt.get("counts", {}).get("observedFiles"),
            "three-hole fresh observed count", issues,
        )
        _require_matching_sha256_digest(
            fresh_summary.get("candidateUserDirBindingSha256"),
            fresh_receipt.get("candidateUserDirBindingSha256"),
            "three-hole fresh candidate/UserDir binding", issues,
        )
    completed_hole_count = len(policy["runtimeJournal"]["holePars"])
    if completed_hole_count != 3:
        issues.append("three-hole v2 policy must describe exactly three completed holes")
    return {
        "state": receipt["state"],
        "technicalThreeHoleAcceptance": receipt["technicalAcceptance"],
        "completedHoleCount": completed_hole_count,
        "runtimeOperationalBindingValidated": runtime["runtimeOperationalBindingValidated"],
        "cryptographicProcessAttestation": claim_boundary["cryptographicProcessAttestation"],
        "tamperProofEvidence": claim_boundary["tamperProofEvidence"],
        "hostileSameUserForgeryResistance": claim_boundary["hostileSameUserForgeryResistance"],
        "evidenceTrustModel": claim_boundary["evidenceTrustModel"],
    }


def _validate_operational_runtime_binding(
        value: Any, fresh_receipt: Any, label: str, issues: list[str],
) -> dict[str, Any] | None:
    keys = {
        "schema", "userDirRelativePath", "bytes", "sha256", "captureNonce",
        "roundId", "eventCount", "firstSequence", "lastSequence",
        "modifiedWithinRunWindow", "validationState", "independentValidationState",
        "launchRecordSha256", "technicalEvidenceManifestSha256", "executableSha256",
        "archiveManifestSha256", "trustBoundary",
    }
    if not require_exact_keys(value, keys, label, issues):
        return None
    require_typed_equal(
        value["schema"], "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1",
        f"{label}.schema", issues,
    )
    require_typed_equal(
        value["userDirRelativePath"],
        "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl",
        f"{label}.userDirRelativePath", issues,
    )
    require_typed_equal(value["eventCount"], 12, f"{label}.eventCount", issues)
    require_typed_equal(value["firstSequence"], 1, f"{label}.firstSequence", issues)
    require_typed_equal(value["lastSequence"], 12, f"{label}.lastSequence", issues)
    require_typed_equal(
        value["modifiedWithinRunWindow"], True,
        f"{label}.modifiedWithinRunWindow", issues,
    )
    require_typed_equal(
        value["validationState"], "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL",
        f"{label}.validationState", issues,
    )
    require_typed_equal(
        value["independentValidationState"],
        "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
        f"{label}.independentValidationState", issues,
    )
    if type(value["bytes"]) is not int or type(value["bytes"]) is bool or value["bytes"] <= 0:
        issues.append(f"{label}.bytes must be a positive integer")
    for key in (
            "sha256", "launchRecordSha256", "technicalEvidenceManifestSha256",
            "executableSha256", "archiveManifestSha256"):
        if type(value[key]) is not str or not SHA256_RE.fullmatch(value[key]):
            issues.append(f"{label}.{key} must be uppercase SHA-256")
    uuid = re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")
    for key in ("captureNonce", "roundId"):
        if type(value[key]) is not str or not uuid.fullmatch(value[key]):
            issues.append(f"{label}.{key} must be a lowercase UUID")
    expected_boundary = {
        "evidenceTrustModel": (
            "BOUNDED_OPERATIONAL_PROCESS_EVIDENCE_NOT_CRYPTOGRAPHIC_ATTESTATION"
        ),
        "cryptographicProcessAttestation": False,
        "tamperProofEvidence": False,
        "hostileSameUserForgeryResistance": False,
    }
    require_typed_equal(value["trustBoundary"], expected_boundary, f"{label}.trustBoundary", issues)
    fresh_runtime = (
        fresh_receipt.get("runtimeJournalBinding")
        if type(fresh_receipt) is dict else None
    )
    if type(fresh_runtime) is not dict:
        issues.append(f"{label} cannot reconcile a fresh UserDir runtime binding")
    else:
        for key in (
                "userDirRelativePath", "bytes", "sha256", "captureNonce", "roundId",
                "launchRecordSha256", "technicalEvidenceManifestSha256",
                "executableSha256", "archiveManifestSha256", "validationState"):
            require_typed_equal(
                value[key], fresh_runtime.get(key),
                f"{label}/fresh runtime {key}", issues,
            )
    return expected_boundary


def _validate_session10_fresh_profile(
        *, receipt: Any, policy: Any, policy_binding: Any,
        fresh_receipt: Any, candidate_id: str, root: Path,
        issues: list[str],
) -> dict[str, Any] | None:
    receipt_keys = {
        "schema", "schemaVersion", "session", "milestone", "candidateId",
        "authority", "state", "policy", "bindings", "observations",
        "claimBoundary", "issues",
    }
    if not require_exact_keys(receipt, receipt_keys, "Session 10 trusted receipt", issues):
        return None
    require_typed_equal(
        receipt["schema"],
        "DiscGolfTour.Session10EnvironmentShippingTechnicalEvidence.v4",
        "Session 10 trusted receipt.schema", issues,
    )
    require_typed_equal(receipt["schemaVersion"], 4, "Session 10 trusted receipt.schemaVersion", issues)
    require_typed_equal(receipt["session"], 10, "Session 10 trusted receipt.session", issues)
    require_typed_equal(receipt["candidateId"], candidate_id, "Session 10 trusted receipt.candidateId", issues)
    require_typed_equal(receipt["issues"], [], "Session 10 trusted receipt.issues", issues)
    _require_matching_binding(
        receipt["policy"], policy_binding, "Session 10 selected policy binding", issues,
    )
    expected_policy_path = (
        "Config/DG_Session10EnvironmentShippingTechnicalPolicy-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        policy_binding.get("path") if type(policy_binding) is dict else None,
        expected_policy_path, "Session 10 candidate policy path", issues,
    )
    try:
        import validate_dg_session10_environment_shipping as session10_validator

        policy_issues, _ = session10_validator.validate_policy(
            policy, root, check_files=False,
        )
    except Exception as exc:
        issues.append(f"Session 10 candidate policy validation failed: {type(exc).__name__}: {exc}")
        return None
    issues.extend(f"Session 10 candidate policy: {issue}" for issue in policy_issues)
    if type(policy) is not dict:
        return None
    require_typed_equal(policy.get("schema"), session10_validator.TRUSTED_POLICY_SCHEMA, "Session 10 candidate policy.schema", issues)
    require_typed_equal(policy.get("schemaVersion"), 4, "Session 10 candidate policy.schemaVersion", issues)
    require_typed_equal(policy.get("candidateId"), candidate_id, "Session 10 candidate policy.candidateId", issues)
    observations = receipt["observations"]
    runtime = (
        observations.get("externalRuntimeEvidence", {}).get("runtimeCheckpointJournal")
        if type(observations) is dict
        and type(observations.get("externalRuntimeEvidence")) is dict
        else None
    )
    boundary = _validate_operational_runtime_binding(
        runtime, fresh_receipt,
        "Session 10 observations.externalRuntimeEvidence.runtimeCheckpointJournal",
        issues,
    )
    if boundary is None:
        return None
    return {
        "policy": copy.deepcopy(policy_binding),
        "runtimeOperationalBindingValidated": True,
        **boundary,
    }


def _validate_session12_fresh_profile(
        *, receipt: Any, policy: Any, policy_binding: Any,
        fresh_receipt: Any, candidate_id: str, issues: list[str],
) -> dict[str, Any] | None:
    receipt_keys = {
        "schema", "schemaVersion", "session", "candidateId", "generatedUtc",
        "state", "releaseState", "policy", "candidateArchive",
        "session12ContractValidator", "projectEvidence", "freshInstallEvidence",
        "technicalClaims", "unsupportedClaims", "truthBoundary",
    }
    if not require_exact_keys(receipt, receipt_keys, "Session 12 trusted receipt", issues):
        return None
    try:
        import validate_dg_session19_session12_presentation_technical as session12_validator
    except Exception as exc:
        issues.append(f"Session 12 validator import failed: {type(exc).__name__}")
        return None
    require_typed_equal(receipt["schema"], session12_validator.TRUSTED_RECEIPT_SCHEMA, "Session 12 trusted receipt.schema", issues)
    require_typed_equal(receipt["schemaVersion"], 3, "Session 12 trusted receipt.schemaVersion", issues)
    require_typed_equal(receipt["session"], 19, "Session 12 trusted receipt.session", issues)
    require_typed_equal(receipt["candidateId"], candidate_id, "Session 12 trusted receipt.candidateId", issues)
    require_typed_equal(receipt["state"], session12_validator.STATE, "Session 12 trusted receipt.state", issues)
    require_typed_equal(receipt["releaseState"], session12_validator.RELEASE_STATE, "Session 12 trusted receipt.releaseState", issues)
    _require_matching_binding(
        receipt["policy"], policy_binding, "Session 12 selected policy binding", issues,
    )
    expected_policy_path = (
        "Config/DG_Session19Session12PresentationTechnicalPolicy-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        policy_binding.get("path") if type(policy_binding) is dict else None,
        expected_policy_path, "Session 12 candidate policy path", issues,
    )
    try:
        session12_validator._validate_policy(policy)
    except Exception as exc:
        issues.append(f"Session 12 candidate policy validation failed: {type(exc).__name__}: {exc}")
        return None
    require_typed_equal(policy.get("schema"), session12_validator.TRUSTED_POLICY_SCHEMA, "Session 12 candidate policy.schema", issues)
    require_typed_equal(policy.get("schemaVersion"), 3, "Session 12 candidate policy.schemaVersion", issues)
    require_typed_equal(
        policy.get("candidate", {}).get("candidateId")
        if type(policy.get("candidate")) is dict else None,
        candidate_id, "Session 12 candidate policy.candidateId", issues,
    )
    fresh_install = receipt["freshInstallEvidence"]
    runtime = (
        fresh_install.get("runtimeCheckpointJournal")
        if type(fresh_install) is dict else None
    )
    boundary = _validate_operational_runtime_binding(
        runtime, fresh_receipt,
        "Session 12 freshInstallEvidence.runtimeCheckpointJournal", issues,
    )
    if boundary is None:
        return None
    return {
        "policy": copy.deepcopy(policy_binding),
        "runtimeOperationalBindingValidated": True,
        **boundary,
    }


def _validate_serialized_migration_fresh_profile(
        *, receipt: Any, candidate_policy: Any, policy_binding: Any,
        candidate_id: str, root: Path, issues: list[str],
) -> dict[str, Any] | None:
    expected_policy_path = (
        "Config/DG_Session19SerializedAssetMigrationCandidatePolicy-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        policy_binding.get("path") if type(policy_binding) is dict else None,
        expected_policy_path, "serialized migration candidate policy path", issues,
    )
    try:
        import validate_dg_session19_serialized_asset_migration as serialized_validator

        policy_issues, authority = serialized_validator.validate_candidate_policy(
            candidate_policy, root, policy_binding=policy_binding,
            check_files=False,
        )
    except Exception as exc:
        issues.append(
            "serialized migration candidate policy validation failed: "
            f"{type(exc).__name__}: {exc}"
        )
        return None
    issues.extend(
        f"serialized migration candidate policy: {issue}"
        for issue in policy_issues
    )
    if authority is None:
        return None
    try:
        receipt_issues, _ = serialized_validator.validate_shipping_evidence(
            receipt, root, candidate_authority=authority,
            check_files=False, live_reaudit=False,
        )
    except Exception as exc:
        issues.append(
            "serialized migration candidate receipt validation failed: "
            f"{type(exc).__name__}: {exc}"
        )
        return None
    issues.extend(
        f"serialized migration candidate receipt: {issue}"
        for issue in receipt_issues
    )
    if type(receipt) is not dict:
        return None
    require_typed_equal(
        receipt.get("schema"),
        "DiscGolfTour.Session19SerializedAssetMigrationShippingEvidence.v2",
        "serialized migration candidate receipt.schema", issues,
    )
    require_typed_equal(
        receipt.get("schemaVersion"), 2,
        "serialized migration candidate receipt.schemaVersion", issues,
    )
    require_typed_equal(
        receipt.get("candidateId"), candidate_id,
        "serialized migration candidate receipt.candidateId", issues,
    )
    require_typed_equal(
        receipt.get("candidatePolicy"), policy_binding,
        "serialized migration candidate receipt.candidatePolicy", issues,
    )
    if receipt_issues:
        return None
    return {
        "candidatePolicyPath": policy_binding["path"],
        "candidatePolicyBytes": policy_binding["bytes"],
        "candidatePolicySha256": policy_binding["sha256"],
    }


def _validate_production_motion_fresh_profile(
        *, receipt: Any, policy: Any, policy_binding: Any,
        receipt_binding: Any, cook_receipt: Any, cook_receipt_binding: Any,
        candidate_content_binding: Any,
        candidate_id: str, root: Path, issues: list[str],
) -> dict[str, Any] | None:
    """Validate the candidate receipt as the selector for its motion policy."""
    expected_receipt_path = (
        "Evidence/Session19/ProductionMotionTechnicalCandidateEvidence-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        receipt_binding.get("path") if type(receipt_binding) is dict else None,
        expected_receipt_path, "production-motion candidate receipt path", issues,
    )
    expected_cook_receipt_path = (
        "Evidence/Session19/ProductionMotionCookPresence-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        cook_receipt_binding.get("path")
        if type(cook_receipt_binding) is dict else None,
        expected_cook_receipt_path,
        "production-motion candidate cook receipt path", issues,
    )
    expected_policy_path = (
        "Config/DG_Session19ProductionMotionAuthoringPolicy-"
        f"{candidate_id}.json"
    )
    require_typed_equal(
        policy_binding.get("path") if type(policy_binding) is dict else None,
        expected_policy_path, "production-motion candidate policy path", issues,
    )
    if not require_exact_keys(
            policy_binding, {"path", "bytes", "sha256"},
            "production-motion candidate policy binding", issues):
        return None
    if (
            type(policy_binding["bytes"]) is not int
            or type(policy_binding["bytes"]) is bool
            or policy_binding["bytes"] <= 0):
        issues.append("production-motion candidate policy bytes must be positive")
    if (
            type(policy_binding["sha256"]) is not str
            or not SHA256_RE.fullmatch(policy_binding["sha256"])):
        issues.append(
            "production-motion candidate policy SHA-256 must be uppercase SHA-256"
        )
    receipt_keys = {
        "schema", "schemaVersion", "session", "milestone", "authority",
        "state", "policy", "bindings", "observations", "residualBlockers",
        "claimBoundary", "issues",
    }
    if not require_exact_keys(
            receipt, receipt_keys, "production-motion candidate receipt", issues):
        return None
    require_typed_equal(
        receipt["schema"],
        "DiscGolfTour.Session19ProductionMotionTechnicalCandidateEvidence.v2",
        "production-motion candidate receipt.schema", issues,
    )
    require_typed_equal(
        receipt["schemaVersion"], 2,
        "production-motion candidate receipt.schemaVersion", issues,
    )
    require_typed_equal(
        receipt["session"], 19,
        "production-motion candidate receipt.session", issues,
    )
    require_typed_equal(
        receipt["milestone"], "v0.5",
        "production-motion candidate receipt.milestone", issues,
    )
    require_typed_equal(
        receipt["state"],
        "PASS_TECHNICAL_PROCEDURAL_CANDIDATES_AUTHORED_RUNTIME_AND_COOK_"
        "BOUND_HUMAN_REVIEW_AND_FRESH_CANDIDATE_PENDING",
        "production-motion candidate receipt.state", issues,
    )
    require_typed_equal(
        receipt["issues"], [],
        "production-motion candidate receipt.issues", issues,
    )
    require_typed_equal(
        receipt["policy"], policy_binding,
        "production-motion selected policy binding", issues,
    )
    try:
        policy_issues, observations = production_motion_validator.validate_policy(
            policy, root, check_files=False,
            expected_candidate_id=candidate_id,
        )
    except Exception as exc:
        issues.append(
            "production-motion candidate policy validation failed: "
            f"{type(exc).__name__}: {exc}"
        )
        return None
    issues.extend(
        f"production-motion candidate policy: {issue}"
        for issue in policy_issues
    )
    if type(policy) is not dict:
        return None
    require_typed_equal(
        receipt["authority"], policy.get("authority"),
        "production-motion candidate receipt.authority", issues,
    )
    require_typed_equal(
        receipt["bindings"], policy.get("bindings"),
        "production-motion candidate receipt.bindings", issues,
    )
    require_typed_equal(
        receipt["observations"], observations,
        "production-motion candidate receipt.observations", issues,
    )
    require_typed_equal(
        receipt["residualBlockers"], policy.get("residualBlockers"),
        "production-motion candidate receipt.residualBlockers", issues,
    )
    require_typed_equal(
        receipt["claimBoundary"], policy.get("claimBoundary"),
        "production-motion candidate receipt.claimBoundary", issues,
    )
    policy_content_binding = (
        policy.get("bindings", {}).get("candidateContentAudit")
        if type(policy.get("bindings")) is dict else None
    )
    _require_matching_binding(
        policy_content_binding, candidate_content_binding,
        "production-motion candidate-content receipt binding", issues,
    )
    try:
        import validate_dg_session19_production_motion_cook as motion_cook

        cook_issues = motion_cook.receipt_errors(cook_receipt)
    except Exception as exc:
        issues.append(
            "production-motion candidate cook receipt validation failed: "
            f"{type(exc).__name__}: {exc}"
        )
        return None
    issues.extend(
        f"production-motion candidate cook receipt: {issue}"
        for issue in cook_issues
    )
    if type(cook_receipt) is not dict:
        return None
    require_typed_equal(
        cook_receipt.get("candidateId"), candidate_id,
        "production-motion candidate cook receipt.candidateId", issues,
    )
    cook_archive = cook_receipt.get("archive")
    if type(cook_archive) is not dict:
        cook_archive = {}
    require_typed_equal(
        cook_archive.get("ioStoreTargetCount"),
        observations.get("targetAssetsPresent"),
        "production-motion authored/cooked target count reconciliation", issues,
    )
    require_typed_equal(
        cook_archive.get("assetRegistryTargetCount"),
        observations.get("targetAssetsPresent"),
        "production-motion authored/registry target count reconciliation", issues,
    )
    cook_boundary = cook_receipt.get("claimBoundary")
    if type(cook_boundary) is not dict:
        cook_boundary = {}
    if policy_issues or cook_issues:
        return None
    return {
        "policy": copy.deepcopy(policy_binding),
        "evidence": copy.deepcopy(receipt_binding),
        "shippingCookPresenceReceipt": copy.deepcopy(cook_receipt_binding),
        "motionFamilyContractCount": observations["motionFamilyContracts"],
        "semanticPhaseCount": observations["semanticCoverageCount"],
        "targetAssetCount": observations["targetAssetCount"],
        "targetAssetPresentCount": observations["targetAssetsPresent"],
        "protectedSyntheticAssetCount": observations[
            "protectedSyntheticAssetCount"
        ],
        "protectedSyntheticAssetReuseCount": observations[
            "protectedSyntheticAssetReuseCount"
        ],
        "authoringPrerequisiteFileCount": policy[
            "guardedAuthoringContract"
        ]["currentRequiredAuthoringFileCount"],
        "authoringPrerequisiteFilesPresent": observations[
            "authoringPrerequisiteFilesPresent"
        ],
        "shippingRuntimeBindingActive": observations[
            "shippingProductionMotionBindingActive"
        ],
        "shippingCookPresenceAccepted": cook_boundary.get(
            "technicalCookPresenceAccepted"
        ),
    }


def derive_fresh_candidate_expectations(
        authority: dict[str, Any], root: Path, issues: list[str],
        documents: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    candidate_section = authority.get("currentShippingCandidateEvidence")
    staged_section = authority.get("stagedProvenancePolicy")
    origin_section = authority.get("technicalOriginAttribution")
    three_section = authority.get("threeHoleTechnicalAcceptance")
    v05_equipment_section = authority.get("v05EquipmentTechnical")
    session10_section = authority.get("session10EnvironmentTechnical")
    session12_section = authority.get("session12PresentationTechnical")
    production_motion_section = authority.get("productionMotionPreparation")
    serialized_section = authority.get("serializedAssetMigration")
    if not all(type(value) is dict for value in (
            candidate_section, staged_section, origin_section, three_section,
            v05_equipment_section,
            session10_section, session12_section, production_motion_section,
            serialized_section)):
        issues.append("fresh candidate profile requires all candidate evidence sections")
        return None
    candidate_id = candidate_section.get("candidateId")
    if type(candidate_id) is not str or not CANDIDATE_ID_RE.fullmatch(candidate_id):
        issues.append("fresh candidate profile candidateId is malformed")
        return None
    canonical_paths = (
        (candidate_section.get("freshUserDirValidator", {}).get("path"),
         "Scripts/validate_dg_session19_fresh_userdir.py", "fresh UserDir validator"),
        (origin_section.get("policy", {}).get("path"),
         "Config/DG_Session19TechnicalOriginAttributionPolicy.json", "technical-origin policy"),
        (origin_section.get("validator", {}).get("path"),
         "Scripts/validate_dg_session19_technical_origin_attribution.py", "technical-origin validator"),
        (three_section.get("policy", {}).get("path"),
         "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicyV2.json", "three-hole v2 policy"),
        (three_section.get("validator", {}).get("path"),
         "Scripts/validate_dg_session19_three_hole_technical_acceptance.py", "three-hole validator"),
    )
    for actual, expected, label in canonical_paths:
        require_typed_equal(actual, expected, f"fresh profile {label} path", issues)

    document_specs = (
        ("buildReceipt", candidate_section.get("buildReceipt"), "Shipping build receipt"),
        ("verificationReceipt", candidate_section.get("verificationReceipt"), "Shipping verification receipt"),
        ("pluginCapabilityReceipt", candidate_section.get("pluginCapabilityReceipt"), "plugin capability receipt"),
        ("binaryReceipt", candidate_section.get("binaryReceipt"), "Shipping binary receipt"),
        ("contentReceipt", candidate_section.get("contentReceipt"), "candidate content receipt"),
        ("thirdPartyNoticeReceipt", candidate_section.get("thirdPartyNoticeReceipt"), "third-party notice receipt"),
        ("provenanceClassificationDraft", candidate_section.get("provenanceClassificationDraft"), "provenance classification draft"),
        ("provenanceDraftAudit", candidate_section.get("provenanceDraftAudit"), "provenance draft audit"),
        ("freshUserDirReceipt", candidate_section.get("freshUserDirReceipt"), "fresh UserDir receipt"),
        ("technicalOriginPolicy", origin_section.get("policy"), "technical-origin policy"),
        ("technicalOriginProposal", origin_section.get("proposal"), "technical-origin proposal"),
        ("technicalOriginReceipt", origin_section.get("receipt"), "technical-origin receipt"),
        ("threeHolePolicy", three_section.get("policy"), "three-hole v2 policy"),
        ("threeHoleReceipt", three_section.get("receipt"), "three-hole v2 receipt"),
        (
            "v05EquipmentReceipt",
            v05_equipment_section.get("receipt"),
            "v0.5 equipment candidate receipt",
        ),
        ("session10Receipt", session10_section.get("receipt"), "Session 10 trusted receipt"),
        ("session12Receipt", session12_section.get("receipt"), "Session 12 trusted receipt"),
        (
            "productionMotionReceipt",
            production_motion_section.get("evidence"),
            "production-motion candidate receipt",
        ),
        (
            "productionMotionCookReceipt",
            production_motion_section.get("shippingCookPresenceReceipt"),
            "production-motion candidate cook receipt",
        ),
        (
            "serializedCandidateReceipt",
            {
                "path": serialized_section.get("candidateReceiptPath"),
                "bytes": serialized_section.get("candidateReceiptBytes"),
                "sha256": serialized_section.get("candidateReceiptSha256"),
            },
            "serialized migration candidate receipt",
        ),
    )
    loaded: dict[str, Any] = {}
    for name, binding, label in document_specs:
        loaded[name] = _profile_document(
            name, root, binding, label, issues, documents,
        )
    if any(value is None for value in loaded.values()):
        return None
    equipment_policy_binding = _candidate_v05_equipment_policy_binding(
        loaded["v05EquipmentReceipt"], candidate_id,
    )
    if equipment_policy_binding is None:
        issues.append(
            "v0.5 equipment receipt lacks its candidate policy binding"
        )
        return None
    loaded["v05EquipmentPolicy"] = _profile_document(
        "v05EquipmentPolicy", root, equipment_policy_binding,
        "v0.5 equipment candidate policy", issues, documents,
    )
    if loaded["v05EquipmentPolicy"] is None:
        return None
    equipment_policy_authority_binding = equipment_policy_binding
    if (
            type(documents) is dict
            and "v05EquipmentPolicyBinding" in documents):
        equipment_policy_authority_binding = copy.deepcopy(
            documents["v05EquipmentPolicyBinding"]
        )
    verification = loaded["verificationReceipt"]
    content = loaded["contentReceipt"]
    archive_sha256 = _candidate_identity_value(
        verification, ("shippingPluginCapabilities", "finalArchiveCanonicalManifestSha256"))
    executable_bytes = _candidate_identity_value(
        verification, ("innerShippingExecutable", "bytes"))
    executable_sha256 = _candidate_identity_value(
        verification, ("innerShippingExecutable", "sha256"))
    archive_bytes = _candidate_identity_value(content, ("archive", "bytes"))
    archive_file_count = _candidate_identity_value(content, ("archive", "fileCount"))
    if type(archive_sha256) is not str or not SHA256_RE.fullmatch(archive_sha256):
        issues.append("Shipping verification receipt lacks canonical archive SHA-256 anchor")
    if type(executable_sha256) is not str or not SHA256_RE.fullmatch(executable_sha256):
        issues.append("Shipping verification receipt lacks Shipping executable SHA-256 anchor")
    if type(executable_bytes) is not int or type(executable_bytes) is bool or executable_bytes <= 0:
        issues.append("Shipping verification receipt lacks Shipping executable byte anchor")
    if type(archive_bytes) is not int or type(archive_bytes) is bool or archive_bytes <= 0:
        issues.append("candidate content receipt lacks archive byte anchor")
    if type(archive_file_count) is not int or type(archive_file_count) is bool or archive_file_count <= 0:
        issues.append("candidate content receipt lacks archive file-count anchor")
    if not issues:
        for name, _binding, label in document_specs:
            _validate_consumed_candidate_identity(
                loaded[name], label, candidate_id, archive_sha256,
                executable_bytes, executable_sha256, archive_bytes,
                archive_file_count, issues,
            )
    if issues:
        return None
    session10_policy_binding = (
        loaded["session10Receipt"].get("policy")
        if type(loaded["session10Receipt"]) is dict else None
    )
    session12_policy_binding = (
        loaded["session12Receipt"].get("policy")
        if type(loaded["session12Receipt"]) is dict else None
    )
    loaded["session10Policy"] = _profile_document(
        "session10Policy", root, session10_policy_binding,
        "Session 10 candidate policy", issues, documents,
    )
    loaded["session12Policy"] = _profile_document(
        "session12Policy", root, session12_policy_binding,
        "Session 12 candidate policy", issues, documents,
    )
    production_motion_policy_binding = (
        loaded["productionMotionReceipt"].get("policy")
        if type(loaded["productionMotionReceipt"]) is dict else None
    )
    loaded["productionMotionPolicy"] = _profile_document(
        "productionMotionPolicy", root, production_motion_policy_binding,
        "production-motion candidate policy", issues, documents,
    )
    serialized_policy_binding = (
        loaded["serializedCandidateReceipt"].get("candidatePolicy")
        if type(loaded["serializedCandidateReceipt"]) is dict else None
    )
    loaded["serializedCandidatePolicy"] = _profile_document(
        "serializedCandidatePolicy", root, serialized_policy_binding,
        "serialized migration candidate policy", issues, documents,
    )
    if (
            loaded["session10Policy"] is None
            or loaded["session12Policy"] is None
            or loaded["productionMotionPolicy"] is None
            or loaded["serializedCandidatePolicy"] is None):
        return None
    equipment_policy_binding = _validate_v05_equipment_candidate_policy(
        receipt=loaded["v05EquipmentReceipt"],
        receipt_binding=v05_equipment_section.get("receipt"),
        policy=loaded["v05EquipmentPolicy"],
        policy_binding=equipment_policy_authority_binding,
        candidate_id=candidate_id,
        issues=issues,
    )
    fresh_facts = _validate_fresh_userdir_receipt(
        loaded["freshUserDirReceipt"], candidate_id, issues,
    )
    origin_facts = _validate_origin_profile_documents(
        candidate_id=candidate_id,
        policy=loaded["technicalOriginPolicy"],
        draft=loaded["provenanceClassificationDraft"],
        draft_audit=loaded["provenanceDraftAudit"],
        proposal=loaded["technicalOriginProposal"],
        receipt=loaded["technicalOriginReceipt"],
        current=candidate_section,
        staged=staged_section,
        origin=origin_section,
        issues=issues,
    )
    three_facts = _validate_three_hole_v2_documents(
        loaded["threeHolePolicy"], loaded["threeHoleReceipt"],
        loaded["freshUserDirReceipt"], candidate_id,
        three_section.get("policy"), three_section.get("receipt"), issues,
    )
    session10_facts = _validate_session10_fresh_profile(
        receipt=loaded["session10Receipt"],
        policy=loaded["session10Policy"],
        policy_binding=session10_policy_binding,
        fresh_receipt=loaded["freshUserDirReceipt"],
        candidate_id=candidate_id,
        root=root,
        issues=issues,
    )
    session12_facts = _validate_session12_fresh_profile(
        receipt=loaded["session12Receipt"],
        policy=loaded["session12Policy"],
        policy_binding=session12_policy_binding,
        fresh_receipt=loaded["freshUserDirReceipt"],
        candidate_id=candidate_id,
        issues=issues,
    )
    production_motion_facts = _validate_production_motion_fresh_profile(
        receipt=loaded["productionMotionReceipt"],
        policy=loaded["productionMotionPolicy"],
        policy_binding=production_motion_policy_binding,
        receipt_binding=production_motion_section.get("evidence"),
        cook_receipt=loaded["productionMotionCookReceipt"],
        cook_receipt_binding=production_motion_section.get(
            "shippingCookPresenceReceipt"
        ),
        candidate_content_binding=candidate_section.get("contentReceipt"),
        candidate_id=candidate_id,
        root=root,
        issues=issues,
    )
    serialized_facts = _validate_serialized_migration_fresh_profile(
        receipt=loaded["serializedCandidateReceipt"],
        candidate_policy=loaded["serializedCandidatePolicy"],
        policy_binding=serialized_policy_binding,
        candidate_id=candidate_id,
        root=root,
        issues=issues,
    )
    self_test_count = _fresh_userdir_self_test_count(issues)
    if (
            equipment_policy_binding is None
            or fresh_facts is None or origin_facts is None or three_facts is None
            or session10_facts is None or session12_facts is None
            or production_motion_facts is None
            or serialized_facts is None
            or self_test_count is None):
        return None

    current_expected = _derive_current_candidate_evidence(
        candidate_section=candidate_section,
        loaded=loaded,
        candidate_id=candidate_id,
        archive_sha256=archive_sha256,
        executable_bytes=executable_bytes,
        executable_sha256=executable_sha256,
        archive_bytes=archive_bytes,
        archive_file_count=archive_file_count,
        fresh_facts=fresh_facts,
        origin_facts=origin_facts,
        origin_section=origin_section,
        fresh_self_test_count=self_test_count,
        issues=issues,
    )
    if current_expected is None:
        return None

    origin_expected = selected_binding_expectation(
        EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION, origin_section,
        ("technicalOriginAttribution",),
    )
    origin_expected.update({
        "auditedIdentityCount": origin_facts["auditedIdentityCount"],
        "authoritativelyAttributedTechnicalOriginCount": origin_facts[
            "authoritativelyAttributedTechnicalOriginCount"
        ],
        "unresolvedIdentityCount": origin_facts["unresolvedIdentityCount"],
        "unresolvedAnonymousChunkCount": origin_facts["unresolvedAnonymousChunkCount"],
        "unresolvedPathIdentityCount": origin_facts["unresolvedPathIdentityCount"],
    })
    staged_expected = selected_binding_expectation(
        EXPECTED_STAGED_PROVENANCE_BINDING, staged_section,
        ("stagedProvenancePolicy",),
    )
    staged_expected["currentState"] = (
        "CURRENT_CANDIDATE_EXACT_PACKAGING_IDENTITY_AND_TECHNICAL_GENERATION_CLASS_"
        f"{origin_facts['authoritativelyAttributedTechnicalOriginCount']}_OF_"
        f"{origin_facts['auditedIdentityCount']}_PASS_UNDERLYING_LICENSE_CONTENT_RIGHTS_"
        "INDEPENDENT_LEGAL_AND_DISTRIBUTION_REVIEW_PENDING"
    )
    three_expected = {
        "currentState": three_facts["state"],
        "policy": copy.deepcopy(three_section["policy"]),
        "validator": copy.deepcopy(three_section["validator"]),
        "receipt": copy.deepcopy(three_section["receipt"]),
        "technicalThreeHoleAcceptance": three_facts["technicalThreeHoleAcceptance"],
        "completedHoleCount": three_facts["completedHoleCount"],
        "runtimeOperationalBindingValidated": three_facts["runtimeOperationalBindingValidated"],
        "cryptographicProcessAttestation": three_facts["cryptographicProcessAttestation"],
        "tamperProofEvidence": three_facts["tamperProofEvidence"],
        "hostileSameUserForgeryResistance": three_facts["hostileSameUserForgeryResistance"],
        "evidenceTrustModel": three_facts["evidenceTrustModel"],
        "visualQualityApproval": False,
        "accessibilityApproval": False,
        "humanPlayFeelApproval": False,
        "manualGameplayAcceptance": False,
        "productOwnerApproval": False,
        "performanceAcceptance": False,
        "soakAcceptance": False,
        "provenanceApproval": False,
        "legalApproval": False,
        "distributionClearance": False,
        "blockerClosed": False,
        "releaseReady": False,
    }
    v05_equipment_expected = selected_binding_expectation(
        EXPECTED_V05_EQUIPMENT_TECHNICAL, v05_equipment_section,
        ("v05EquipmentTechnical",),
    )
    v05_equipment_expected["policy"] = copy.deepcopy(
        equipment_policy_binding
    )
    session10_expected = selected_binding_expectation(
        EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL, session10_section,
        ("session10EnvironmentTechnical",),
    )
    session10_expected.update(session10_facts)
    session12_expected = selected_binding_expectation(
        EXPECTED_SESSION12_PRESENTATION_TECHNICAL, session12_section,
        ("session12PresentationTechnical",),
    )
    session12_expected.update(session12_facts)
    production_motion_expected = selected_binding_expectation(
        EXPECTED_PRODUCTION_MOTION_PREPARATION, production_motion_section,
        ("productionMotionPreparation",),
    )
    production_motion_expected.update(production_motion_facts)
    serialized_expected = selected_binding_expectation(
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING, serialized_section,
        ("serializedAssetMigration",),
    )
    serialized_expected.update(serialized_facts)
    return {
        "stagedProvenancePolicy": staged_expected,
        "currentShippingCandidateEvidence": current_expected,
        "technicalOriginAttribution": origin_expected,
        "threeHoleTechnicalAcceptance": three_expected,
        "v05EquipmentTechnical": v05_equipment_expected,
        "session10EnvironmentTechnical": session10_expected,
        "session12PresentationTechnical": session12_expected,
        "productionMotionPreparation": production_motion_expected,
        "serializedAssetMigration": serialized_expected,
        "metrics": {
            **origin_facts,
            "freshUserDirSelfTestCaseCount": self_test_count,
            "freshUserDirObservedFileCount": fresh_facts["observedFileCount"],
            "runtimeOperationalBindingValidated": three_facts[
                "runtimeOperationalBindingValidated"
            ],
            "session10RuntimeOperationalBindingValidated": session10_facts[
                "runtimeOperationalBindingValidated"
            ],
            "session12RuntimeOperationalBindingValidated": session12_facts[
                "runtimeOperationalBindingValidated"
            ],
        },
    }


def validate_quarantine_receipt(
        receipt: Any, binding: dict[str, Any] | None = None) -> list[str]:
    issues: list[str] = []
    expected = EXPECTED_QUARANTINE_RECEIPT
    if type(binding) is dict:
        expected = selected_binding_expectation(
            EXPECTED_QUARANTINE_RECEIPT,
            {"beforeMoveManifest": binding.get("beforeMoveManifest")},
            ("unusedFabReceipt",),
        )
    require_typed_equal(
        receipt, expected,
        "unused Fab external-quarantine receipt", issues,
    )
    if receipt != expected or type(receipt) is not dict:
        return issues

    token = receipt["recoveryLocationToken"]
    if (
            not is_canonical_relative_posix(token)
            or not token.startswith("DGTOUR_EXTERNAL_QUARANTINE/")
            or token.count("/") != 2):
        issues.append(
            "unused Fab receipt recoveryLocationToken must be a sanitized logical token, not a host path"
        )
    for index, item in enumerate(receipt["roots"]):
        if not is_canonical_relative_posix(item["projectPath"]):
            issues.append(f"unused Fab receipt roots[{index}].projectPath is not canonical")
        if not is_canonical_relative_posix(item["recoveryRelativePath"]):
            issues.append(f"unused Fab receipt roots[{index}].recoveryRelativePath is not canonical")
    return issues


def validate_quarantine_manifest(path: Path) -> list[str]:
    issues: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        return [f"unused Fab before-move manifest cannot be read as UTF-8: {exc}"]
    lines = text.splitlines()
    if len(lines) != 533:
        issues.append(f"unused Fab before-move manifest row count differs: expected 533, got {len(lines)}")

    expected_products = {
        item["productId"]: (item["files"], item["bytes"])
        for item in EXPECTED_QUARANTINE_RECEIPT["roots"]
    }
    observed: dict[str, list[int]] = {key: [0, 0] for key in expected_products}
    identities: set[tuple[str, str]] = set()
    for line_number, line in enumerate(lines, 1):
        fields = line.split("\t")
        if len(fields) != 4:
            issues.append(f"unused Fab manifest row {line_number} must have exactly four TSV fields")
            continue
        product_id, relative_path, byte_text, digest = fields
        if product_id not in expected_products:
            issues.append(f"unused Fab manifest row {line_number} has an unknown productId")
            continue
        if not is_canonical_relative_posix(relative_path):
            issues.append(f"unused Fab manifest row {line_number} path is not canonical relative POSIX")
        identity = (product_id.casefold(), relative_path.casefold())
        if identity in identities:
            issues.append(f"unused Fab manifest row {line_number} duplicates or case-collides")
        identities.add(identity)
        if not re.fullmatch(r"0|[1-9][0-9]*", byte_text):
            issues.append(f"unused Fab manifest row {line_number} byte count is not canonical decimal")
            continue
        byte_count = int(byte_text)
        if byte_count <= 0:
            issues.append(f"unused Fab manifest row {line_number} byte count must be positive")
        if not SHA256_RE.fullmatch(digest):
            issues.append(f"unused Fab manifest row {line_number} digest must be uppercase SHA-256")
        observed[product_id][0] += 1
        observed[product_id][1] += byte_count

    for product_id, (expected_files, expected_bytes) in expected_products.items():
        actual_files, actual_bytes = observed[product_id]
        if (actual_files, actual_bytes) != (expected_files, expected_bytes):
            issues.append(
                f"unused Fab manifest {product_id} totals differ: expected "
                f"{expected_files}/{expected_bytes}, got {actual_files}/{actual_bytes}"
            )
    return issues


def validate_quarantine_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    receipt_path, receipt_issues = validate_file_binding(
        root, binding.get("receipt", {}), "path", "bytes", "sha256",
        "unused Fab receipt",
    )
    issues.extend(receipt_issues)
    manifest_path, manifest_issues = validate_file_binding(
        root, binding.get("beforeMoveManifest", {}), "path", "bytes", "sha256",
        "unused Fab before-move manifest",
    )
    issues.extend(manifest_issues)

    if receipt_path is not None:
        try:
            receipt = load_json_strict(receipt_path)
        except StrictJsonError as exc:
            issues.append(f"unused Fab external-quarantine receipt is invalid: {exc}")
        else:
            issues.extend(validate_quarantine_receipt(receipt, binding))
    if manifest_path is not None:
        issues.extend(validate_quarantine_manifest(manifest_path))

    for project_path in binding.get("projectRoots", []):
        candidate = validate_relative_path(
            project_path, f"quarantined source {project_path}", root, issues, must_exist=False,
        )
        if candidate is not None and (candidate.exists() or candidate.is_symlink()):
            issues.append(f"quarantined source still exists inside project: {project_path}")
    return issues


def validate_shipping_policy_document(policy: Any) -> list[str]:
    issues: list[str] = []
    policy_keys = {
        "schema", "schemaVersion", "session", "policyId", "state", "target",
        "compileDefinitions", "packageHardening", "neverCookRoots", "forbiddenShippingPathTokens",
        "forbiddenLooseFilePatterns", "requiredLooseDataFiles", "featureExclusions",
        "authoritativeIdentityPolicy", "evidenceRequiredBeforeClosure", "releaseReady",
    }
    if not require_exact_keys(policy, policy_keys, "Session 19 Shipping content policy", issues):
        return issues
    for key, expected in EXPECTED_POLICY_SCALARS.items():
        require_typed_equal(policy[key], expected, f"Shipping policy.{key}", issues)
    require_typed_equal(policy["target"], EXPECTED_POLICY_TARGET, "Shipping policy.target", issues)
    require_typed_equal(
        policy["compileDefinitions"], EXPECTED_COMPILE_DEFINITIONS,
        "Shipping policy.compileDefinitions", issues,
    )
    require_typed_equal(
        policy["packageHardening"], EXPECTED_PACKAGE_HARDENING,
        "Shipping policy.packageHardening", issues,
    )
    require_typed_equal(
        policy["neverCookRoots"], EXPECTED_POLICY_NEVER_COOK_ROOTS,
        "Shipping policy.neverCookRoots", issues,
    )
    require_typed_equal(
        policy["authoritativeIdentityPolicy"], EXPECTED_AUTHORITATIVE_IDENTITY_POLICY,
        "Shipping policy.authoritativeIdentityPolicy", issues,
    )
    require_typed_equal(
        policy["forbiddenShippingPathTokens"], EXPECTED_FORBIDDEN_SHIPPING_TOKENS,
        "Shipping policy.forbiddenShippingPathTokens", issues,
    )
    require_typed_equal(
        policy["forbiddenLooseFilePatterns"], EXPECTED_FORBIDDEN_LOOSE_PATTERNS,
        "Shipping policy.forbiddenLooseFilePatterns", issues,
    )
    require_typed_equal(
        policy["requiredLooseDataFiles"], EXPECTED_REQUIRED_LOOSE_DATA,
        "Shipping policy.requiredLooseDataFiles", issues,
    )
    require_typed_equal(
        policy["featureExclusions"], EXPECTED_FEATURE_EXCLUSIONS,
        "Shipping policy.featureExclusions", issues,
    )
    require_typed_equal(
        policy["evidenceRequiredBeforeClosure"], EXPECTED_POLICY_EVIDENCE,
        "Shipping policy.evidenceRequiredBeforeClosure", issues,
    )
    for label, values in (
        ("neverCookRoots", policy["neverCookRoots"]),
        ("forbiddenShippingPathTokens", policy["forbiddenShippingPathTokens"]),
        ("forbiddenLooseFilePatterns", policy["forbiddenLooseFilePatterns"]),
        ("requiredLooseDataFiles", policy["requiredLooseDataFiles"]),
    ):
        if type(values) is list and len({value.casefold() for value in values if type(value) is str}) != len(values):
            issues.append(f"Shipping policy.{label} contains duplicates or case collisions")
    return issues


def validate_shipping_policy_evidence(
        root: Path, binding: dict[str, Any], package_binding: dict[str, Any],
        required_files: list[Any]) -> list[str]:
    issues: list[str] = []
    policy_path = validate_relative_path(
        binding.get("path"), "Shipping content policy path",
        root, issues, must_exist=True,
    )
    if policy_path is not None:
        actual_bytes = policy_path.stat().st_size
        if actual_bytes != binding.get("bytes"):
            issues.append(
                f"Shipping content policy byte size differs: expected "
                f"{binding.get('bytes')}, got {actual_bytes}"
            )
        actual_hash = sha256_file(policy_path)
        if actual_hash != binding.get("sha256"):
            issues.append(
                f"Shipping content policy SHA-256 differs: expected "
                f"{binding.get('sha256')}, got {actual_hash}"
            )
        try:
            policy = load_json_strict(policy_path)
        except StrictJsonError as exc:
            issues.append(f"Shipping content policy is invalid: {exc}")
        else:
            issues.extend(validate_shipping_policy_document(policy))

    _, validator_issues = validate_file_binding(
        root, binding, "validatorPath", "validatorBytes",
        "validatorSha256", "Shipping content validator",
    )
    issues.extend(validator_issues)

    ini_path = validate_relative_path(
        package_binding.get("defaultGame", {}).get("path"),
        "Shipping NeverCook config", root, issues, must_exist=True,
    )
    if ini_path is not None:
        try:
            ini_text = ini_path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            issues.append(f"Shipping NeverCook config cannot be read: {exc}")
        else:
            configured_roots = re.findall(
                r'^\+DirectoriesToNeverCook=\(Path="([^"]+)"\)\s*$', ini_text, re.MULTILINE,
            )
            if set(configured_roots) != set(EXPECTED_DEFAULT_GAME_NEVER_COOK_ROOTS):
                issues.append(
                    "DefaultGame.ini NeverCook roots differ from the Session 19 Shipping policy"
                )
            if len(configured_roots) != len(EXPECTED_DEFAULT_GAME_NEVER_COOK_ROOTS):
                issues.append("DefaultGame.ini NeverCook roots contain duplicates or omissions")
            if configured_roots.count("/Game/DiscGolf/Cook") != 1:
                issues.append(
                    "DefaultGame.ini must NeverCook the development runtime-cook-manifest root exactly once"
                )
            if 'PrimaryAssetType="DGRuntimeCookManifest"' in ini_text:
                issues.append(
                    "DefaultGame.ini must not AlwaysCook the forbidden development runtime-cook manifest"
                )
            if ini_text.splitlines().count("bSkipEditorContent=True") != 1:
                issues.append("DefaultGame.ini must enable bSkipEditorContent exactly once")

    build_matches = [
        value for value in required_files
        if type(value) is str
        and PurePosixPath(value).name == "DiscGolfTour.Build.cs"
    ]
    if len(build_matches) != 1:
        issues.append(
            "requiredFiles must bind exactly one DiscGolfTour.Build.cs compile-definition source"
        )
        build_path = None
    else:
        build_path = validate_relative_path(
            build_matches[0], "Shipping compile-definition source",
            root, issues, must_exist=True,
        )
    if build_path is not None:
        try:
            build_text = build_path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            issues.append(f"Shipping compile-definition source cannot be read: {exc}")
        else:
            shipping_selector = (
                "bool bReleaseShipping = Target.Configuration == "
                "UnrealTargetConfiguration.Shipping;"
            )
            if build_text.count(shipping_selector) != 1:
                issues.append("DiscGolfTour.Build.cs lacks the unique Shipping selector")
            expected_lines = {
                "DG_RELEASE_V05_SCOPE": 'PublicDefinitions.Add("DG_RELEASE_V05_SCOPE=" + (bReleaseShipping ? "1" : "0"));',
                "DG_WITH_CAREER_AI": 'PublicDefinitions.Add("DG_WITH_CAREER_AI=" + (bReleaseShipping ? "0" : "1"));',
                "DG_WITH_THROW_LAB": 'PublicDefinitions.Add("DG_WITH_THROW_LAB=0");',
                "DG_WITH_DEVELOPMENT_CONTENT": 'PublicDefinitions.Add("DG_WITH_DEVELOPMENT_CONTENT=" + (bReleaseShipping ? "0" : "1"));',
            }
            for definition, source_line in expected_lines.items():
                if build_text.count(source_line) != 1:
                    issues.append(
                        f"DiscGolfTour.Build.cs does not uniquely define {definition} for Shipping"
                    )
    return issues


def validate_shipping_plugin_capability_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    policy_path: Path | None = None
    for prefix, label in (
        ("policy", "Shipping plugin capability policy"),
        ("validator", "Shipping plugin capability validator"),
        ("runner", "Shipping candidate runner"),
        ("verifier", "Shipping candidate verifier"),
    ):
        bound_path, binding_issues = validate_file_binding(
            root, binding, f"{prefix}Path", f"{prefix}Bytes",
            f"{prefix}Sha256", label,
        )
        issues.extend(binding_issues)
        if prefix == "policy":
            policy_path = bound_path
    if policy_path is not None:
        try:
            policy = load_json_strict(policy_path)
        except StrictJsonError as exc:
            issues.append(f"Shipping plugin capability policy is invalid: {exc}")
        else:
            if type(policy) is not dict:
                issues.append("Shipping plugin capability policy must be an object")
            else:
                require_typed_equal(
                    policy.get("schema"), binding.get("policySchema"),
                    "Shipping plugin capability policy.schema", issues,
                )
                require_typed_equal(
                    policy.get("schemaVersion"),
                    binding.get("policySchemaVersion"),
                    "Shipping plugin capability policy.schemaVersion", issues,
                )
                require_typed_equal(
                    len(policy.get("reviewedDisabledPlugins", [])), 31,
                    "Shipping plugin capability reviewed-disabled count", issues,
                )
                require_typed_equal(
                    len(policy.get("requiredDependencyClosureFinalPayloadDescriptors", [])), 15,
                    "Shipping plugin capability dependency-closure count", issues,
                )
                require_typed_equal(
                    len(policy.get("reviewedMetadataOnlyDependencyDescriptors", [])), 13,
                    "Shipping plugin capability metadata-only count", issues,
                )
    return issues


def validate_shipping_package_hardening_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    for key, label in (
        ("defaultPakFileRules", "DefaultPakFileRules hardening"),
        ("defaultGame", "DefaultGame hardening"),
        ("projectDescriptor", "project descriptor hardening"),
        ("shippingGameTarget", "Shipping game target hardening"),
    ):
        _, binding_issues = validate_file_binding(
            root, binding.get(key, {}),
            "path", "bytes", "sha256", label,
        )
        issues.extend(binding_issues)
    return issues


def validate_current_candidate_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    binding_labels = (
        ("buildLog", "selected candidate build log"),
        ("buildReceipt", "selected candidate build receipt"),
        ("verificationReceipt", "selected candidate verification receipt"),
        ("pluginCapabilityReceipt", "selected candidate plugin capability receipt"),
        ("binaryReceipt", "selected candidate binary receipt"),
        ("contentReceipt", "selected candidate content receipt"),
        ("thirdPartyNoticeReceipt", "selected candidate ThirdParty notice receipt"),
        ("provenanceClassificationDraft", "selected candidate provenance classification draft"),
        ("provenanceDraftAudit", "selected candidate provenance draft audit"),
        ("freshUserDirValidator", "selected candidate fresh UserDir validator"),
        ("freshUserDirReceipt", "selected candidate fresh UserDir receipt"),
    )
    for key, label in binding_labels:
        _, binding_issues = validate_file_binding(
            root, binding.get(key, {}),
            "path", "bytes", "sha256", label,
        )
        issues.extend(binding_issues)
    return issues


def validate_bound_evidence_bundle(
        root: Path, expected: dict[str, Any], artifact_keys: tuple[str, ...],
        label: str) -> list[str]:
    issues: list[str] = []
    for key in artifact_keys:
        binding = expected.get(key)
        if type(binding) is not dict:
            issues.append(f"{label} {key} binding must be an object")
            continue
        _, binding_issues = validate_file_binding(
            root, binding, "path", "bytes", "sha256", f"{label} {key}",
        )
        issues.extend(binding_issues)
    return issues


def validate_staged_provenance_policy_evidence(
        root: Path, binding: dict[str, Any],
        plugin_binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    policy_path = validate_relative_path(
        binding.get("path"),
        "staged provenance policy path", root, issues, must_exist=True,
    )
    _, generator_issues = validate_file_binding(
        root, binding, "generatorPath", "generatorBytes",
        "generatorSha256", "staged provenance generator",
    )
    issues.extend(generator_issues)
    if policy_path is None:
        return issues
    actual_bytes = policy_path.stat().st_size
    if actual_bytes != binding.get("bytes"):
        issues.append(
            f"staged provenance policy byte size differs: expected "
            f"{binding.get('bytes')}, got {actual_bytes}"
        )
    actual_hash = sha256_file(policy_path)
    if actual_hash != binding.get("sha256"):
        issues.append(
            f"staged provenance policy SHA-256 differs: expected "
            f"{binding.get('sha256')}, got {actual_hash}"
        )
    try:
        policy = load_json_strict(policy_path)
    except StrictJsonError as exc:
        issues.append(f"staged provenance policy is invalid: {exc}")
        return issues
    if type(policy) is not dict:
        issues.append("staged provenance policy must be an object")
        return issues
    require_typed_equal(
        policy.get("schema"), "DiscGolfTour.Session19StagedProvenancePolicy.v1",
        "staged provenance policy.schema", issues,
    )
    require_typed_equal(policy.get("schemaVersion"), 1, "staged provenance policy.schemaVersion", issues)
    require_typed_equal(policy.get("session"), 19, "staged provenance policy.session", issues)
    require_typed_equal(
        policy.get("state"), "IMPLEMENTED_PENDING_FRESH_WINDOWS_SHIPPING_CANDIDATE",
        "staged provenance policy.state", issues,
    )
    require_typed_equal(policy.get("releaseReady"), False, "staged provenance policy.releaseReady", issues)
    require_typed_equal(
        policy.get("closesReleaseBlockerWithoutAcceptedReceipt"), False,
        "staged provenance policy.closesReleaseBlockerWithoutAcceptedReceipt", issues,
    )
    required_engine_development_roots = {
        "/Engine/Content/VREditor/",
        "/Engine/Content/EditorMeshes/",
        "/Engine/Content/Slate/Testing/",
        "/Engine/Plugins/Developer/Concert/",
        "/Engine/Plugins/ChaosVD/",
    }
    development_tokens = policy.get("forbiddenDevelopmentPathTokens")
    if type(development_tokens) is not list or not all(
        type(token) is str and token for token in development_tokens
    ):
        issues.append("staged provenance policy.forbiddenDevelopmentPathTokens is invalid")
    else:
        configured = {token.casefold() for token in development_tokens}
        for required_root in sorted(required_engine_development_roots):
            if required_root.casefold() not in configured:
                issues.append(
                    "staged provenance policy omits required engine development root: "
                    + required_root
                )
        for runtime_slate_identity in (
            "Engine/Content/Slate/Fonts/Roboto-Regular.ttf",
            "Engine/Content/Slate/Common/Selection.png",
            "Engine/Content/Slate/Starship/Common/Window/WindowButton_Close.png",
        ):
            normalized = "/" + runtime_slate_identity + "/"
            if any(token.casefold() in normalized.casefold() for token in development_tokens):
                issues.append(
                    "staged provenance policy over-classifies required runtime Slate: "
                    + runtime_slate_identity
                )
    negative = policy.get("negativeFixture")
    if type(negative) is not dict:
        issues.append("staged provenance policy.negativeFixture must be an object")
    else:
        require_typed_equal(
            negative.get("expectedResult"), "REJECT",
            "staged provenance policy.negativeFixture.expectedResult", issues,
        )
        require_typed_equal(
            negative.get("mayNeverSatisfyPositiveEvidence"), True,
            "staged provenance policy.negativeFixture.mayNeverSatisfyPositiveEvidence", issues,
        )
    forbidden_paths = policy.get("forbiddenPathTokens")
    if type(forbidden_paths) is not list:
        issues.append("staged provenance policy.forbiddenPathTokens must be an array")
    else:
        if "/Engine/Plugins/Experimental/Landmass/" in forbidden_paths:
            issues.append(
                "staged provenance policy must allow the exact Landmass descriptor root"
            )
        for subtree in EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS:
            if forbidden_paths.count(subtree) != 1:
                issues.append(
                    "staged provenance policy must forbid exact Landmass subtree once: "
                    + subtree
                )
    expected_closure = {
        "capabilityPolicy": {
            "path": plugin_binding.get("policyPath"),
            "bytes": plugin_binding.get("policyBytes"),
            "sha256": plugin_binding.get("policySha256"),
        },
        "requiredFinalPayloadDescriptors": EXPECTED_REQUIRED_DEPENDENCY_CLOSURE_PLUGIN_DESCRIPTORS,
        "reviewedMetadataOnlyDependencyDescriptors": EXPECTED_REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS,
        "descriptorOnlyForbiddenSubdirectories": EXPECTED_METADATA_ONLY_FORBIDDEN_SUBDIRECTORIES,
        "reviewedResidualMetadataExclusions": [
            "Engine/Plugins/ChaosClothAssetEditorCore/Config/DefaultChaosClothAssetEditorCore.ini",
            "Engine/Plugins/Editor/EditorScriptingUtilities/Config/DefaultEditorScriptingUtilities.ini",
        ],
        "landmassDescriptor": "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
        "landmassNeverCookVirtualRoot": "/Landmass",
        "landmassForbiddenFinalPayloadSubtrees": EXPECTED_SHIPPING_FORBIDDEN_PLUGIN_CONTENT_ROOTS,
    }
    require_typed_equal(
        policy.get("pluginDependencyClosure"), expected_closure,
        "staged provenance policy.pluginDependencyClosure", issues,
    )
    return issues


def validate_baked_pcg_policy_document(policy: Any) -> list[str]:
    issues: list[str] = []
    policy_keys = {
        "schema", "schemaVersion", "session", "policyId", "authority", "state",
        "sourceEvidenceComplete", "shippingEvidenceComplete", "releaseReady",
        "releaseUseAllowed", "closesSession13Blocker", "target", "boundedAuthority",
        "canonicalization", "holes", "combinedCanonicalPayload", "consumerProof",
        "evidence", "candidateEvidence", "shippingEvidencePending", "remainingReleaseBlocker",
    }
    if not require_exact_keys(policy, policy_keys, "baked-PCG policy", issues):
        return issues
    expected_scalars = {
        "schema": "DiscGolfTour.Session19BakedPcgPolicy.v3",
        "schemaVersion": 3,
        "session": 19,
        "policyId": "pine_ridge_three_hole_explicit_tree_bake_v3",
        "authority": "ADDITIVE_SOURCE_AUTHORITY_EVIDENCE_NOT_RELEASE_APPROVAL",
        "state": "SOURCE_AUTHORITY_AND_SHIPPING_PCG_SOURCE_SEPARATION_VERIFIED_COOK_PROOF_PENDING",
        "sourceEvidenceComplete": True,
        "shippingEvidenceComplete": False,
        "releaseReady": False,
        "releaseUseAllowed": False,
        "closesSession13Blocker": False,
        "remainingReleaseBlocker": "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    }
    for key, expected in expected_scalars.items():
        require_typed_equal(policy[key], expected, f"baked-PCG policy.{key}", issues)
    require_typed_equal(
        policy["combinedCanonicalPayload"],
        {
            "treeCount": 44,
            "bytes": 2982,
            "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
        },
        "baked-PCG policy.combinedCanonicalPayload", issues,
    )
    bounded = policy["boundedAuthority"]
    if type(bounded) is not dict:
        issues.append("baked-PCG policy.boundedAuthority must be an object")
    else:
        require_typed_equal(
            bounded.get("runtimeRandomPlacementAllowedForAuthorityDomain"), False,
            "baked-PCG policy.boundedAuthority.runtimeRandomPlacementAllowedForAuthorityDomain",
            issues,
        )
        require_typed_equal(
            bounded.get("unrealPcgRuntimeGenerationAllowed"), False,
            "baked-PCG policy.boundedAuthority.unrealPcgRuntimeGenerationAllowed", issues,
        )
        require_typed_equal(
            bounded.get("sourceFallbackAcceptedAsShippingAuthority"), False,
            "baked-PCG policy.boundedAuthority.sourceFallbackAcceptedAsShippingAuthority",
            issues,
        )
        not_claimed = bounded.get("notClaimed")
        if type(not_claimed) is not list or "SESSION13_RELEASE_BLOCKER_CLOSURE" not in not_claimed:
            issues.append("baked-PCG policy must explicitly exclude Session 13 blocker closure")
    candidate = policy["candidateEvidence"]
    if type(candidate) is not dict:
        issues.append("baked-PCG policy.candidateEvidence must be an object")
    else:
        require_typed_equal(
            candidate.get("state"),
            "PASS_CANDIDATE_BOUND_AUTHORED_DATA_RUNTIME_AND_BAKED_OUTPUT_ACCEPTANCE_PENDING",
            "baked-PCG policy.candidateEvidence.state", issues,
        )
        require_typed_equal(
            candidate.get("stagingClass"), "NONUFS",
            "baked-PCG policy.candidateEvidence.stagingClass", issues,
        )
        require_typed_equal(
            candidate.get("ufsOrIoStoreCopyRequired"), False,
            "baked-PCG policy.candidateEvidence.ufsOrIoStoreCopyRequired", issues,
        )
        require_typed_equal(
            candidate.get("sourceClosureRetrofitAllowed"), False,
            "baked-PCG policy.candidateEvidence.sourceClosureRetrofitAllowed", issues,
        )
    return issues


def validate_baked_pcg_receipt_document(
        receipt: Any, policy_path: str) -> list[str]:
    issues: list[str] = []
    receipt_keys = {
        "schema", "schemaVersion", "session", "runId", "verifiedUtc", "state",
        "policy", "sourceAuthorityAccepted", "shippingPackageEvidenceAccepted",
        "releaseReady", "blockerClosed", "holes", "combinedCanonicalPayload",
        "sourceConsumerProof", "boundedClaim", "notAcceptedByThisReceipt",
        "remainingEvidence",
    }
    if not require_exact_keys(receipt, receipt_keys, "baked-PCG receipt", issues):
        return issues
    expected_scalars = {
        "schema": "DiscGolfTour.Session19BakedPcgSourceAuthorityReceipt.v2",
        "schemaVersion": 2,
        "session": 19,
        "state": "SOURCE_AUTHORITY_AND_SHIPPING_PCG_SOURCE_SEPARATION_VERIFIED_COOK_PROOF_PENDING",
        "policy": policy_path,
        "sourceAuthorityAccepted": True,
        "shippingPackageEvidenceAccepted": False,
        "releaseReady": False,
        "blockerClosed": False,
    }
    for key, expected in expected_scalars.items():
        require_typed_equal(receipt[key], expected, f"baked-PCG receipt.{key}", issues)
    require_typed_equal(
        receipt["combinedCanonicalPayload"],
        {
            "treeCount": 44,
            "bytes": 2982,
            "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
        },
        "baked-PCG receipt.combinedCanonicalPayload", issues,
    )
    not_accepted = receipt["notAcceptedByThisReceipt"]
    if type(not_accepted) is not list or "SESSION13_RELEASE_BLOCKER_CLOSURE" not in not_accepted:
        issues.append("baked-PCG receipt must explicitly exclude Session 13 blocker closure")
    return issues


def validate_baked_pcg_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    artifacts = (
        ("policy", "policyPath", "policyBytes", "policySha256"),
        ("validator", "validatorPath", "validatorBytes", "validatorSha256"),
        ("receipt", "receiptPath", "receiptBytes", "receiptSha256"),
        ("source closure prebuild receipt", "sourceClosurePreBuildReceiptPath",
         "sourceClosurePreBuildReceiptBytes", "sourceClosurePreBuildReceiptSha256"),
        ("candidate receipt", "candidateReceiptPath", "candidateReceiptBytes",
         "candidateReceiptSha256"),
    )
    if "candidatePolicyPath" in binding:
        artifacts = artifacts + ((
            "candidate policy", "candidatePolicyPath", "candidatePolicyBytes",
            "candidatePolicySha256",
        ),)
    resolved: dict[str, Path] = {}
    for label, path_key, bytes_key, hash_key in artifacts:
        path = validate_relative_path(
            binding.get(path_key), f"baked-PCG {label} path",
            root, issues, must_exist=True,
        )
        if path is None:
            continue
        resolved[label] = path
        actual_bytes = path.stat().st_size
        expected_bytes = binding.get(bytes_key)
        if actual_bytes != expected_bytes:
            issues.append(
                f"baked-PCG {label} byte size differs: expected {expected_bytes}, got {actual_bytes}"
            )
        actual_hash = sha256_file(path)
        expected_hash = binding.get(hash_key)
        if actual_hash != expected_hash:
            issues.append(
                f"baked-PCG {label} SHA-256 differs: expected {expected_hash}, got {actual_hash}"
            )
    validators: tuple[tuple[str, Callable[[Any], list[str]]], ...] = (
        ("policy", validate_baked_pcg_policy_document),
        ("receipt", lambda document: validate_baked_pcg_receipt_document(
            document, binding.get("policyPath")
        )),
    )
    for label, validator in validators:
        path = resolved.get(label)
        if path is None:
            continue
        try:
            document = load_json_strict(path)
        except StrictJsonError as exc:
            issues.append(f"baked-PCG {label} is invalid: {exc}")
        else:
            issues.extend(validator(document))
    return issues


def _validate_serialized_candidate_project_bindings(
        root: Path, receipt: Any, candidate_authority: Any,
) -> list[str]:
    """Verify every redacted receipt binding against its live project file."""
    issues: list[str] = []
    if type(receipt) is not dict:
        return ["serialized migration candidate receipt must be an object"]
    if type(candidate_authority) is not dict:
        return ["serialized migration candidate authority must be an object"]
    bindings = receipt.get("bindings")
    expected_paths = candidate_authority.get("bindingPaths")
    if type(bindings) is not dict:
        return ["serialized migration candidate receipt.bindings must be an object"]
    if type(expected_paths) is not dict:
        return ["serialized migration candidate authority.bindingPaths must be an object"]
    if not require_exact_keys(
            bindings, set(expected_paths),
            "serialized migration candidate receipt.bindings", issues):
        return issues
    for key, expected_path in expected_paths.items():
        binding = bindings[key]
        if type(binding) is not dict:
            issues.append(
                f"serialized migration candidate receipt binding {key} must be an object"
            )
            continue
        require_typed_equal(
            binding.get("path"), expected_path,
            f"serialized migration candidate receipt binding {key} path", issues,
        )
        _, binding_issues = validate_file_binding(
            root, binding, "path", "bytes", "sha256",
            f"serialized migration candidate receipt binding {key}",
        )
        issues.extend(binding_issues)
    return issues


def validate_serialized_asset_migration_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    artifacts = [
        ("policy", "policyPath", "policyBytes", "policySha256"),
        ("validator", "validatorPath", "validatorBytes", "validatorSha256"),
        ("receipt", "receiptPath", "receiptBytes", "receiptSha256"),
        ("inventory", "inventoryPath", "inventoryBytes", "inventorySha256"),
        ("migration script", "migrationScriptPath", "migrationScriptBytes", "migrationScriptSha256"),
        ("candidate receipt", "candidateReceiptPath", "candidateReceiptBytes",
         "candidateReceiptSha256"),
    ]
    if any(
            key in binding
            for key in (
                "candidatePolicyPath", "candidatePolicyBytes",
                "candidatePolicySha256",
            )):
        artifacts.append(
            ("candidate policy", "candidatePolicyPath", "candidatePolicyBytes",
             "candidatePolicySha256")
        )
    resolved: dict[str, Path] = {}
    for label, path_key, bytes_key, hash_key in artifacts:
        path = validate_relative_path(
            binding.get(path_key),
            f"serialized migration {label} path", root, issues, must_exist=True,
        )
        if path is None:
            continue
        resolved[label] = path
        actual_bytes = path.stat().st_size
        expected_bytes = binding.get(bytes_key)
        if actual_bytes != expected_bytes:
            issues.append(
                f"serialized migration {label} byte size differs: "
                f"expected {expected_bytes}, got {actual_bytes}")
        actual_hash = sha256_file(path)
        expected_hash = binding.get(hash_key)
        if actual_hash != expected_hash:
            issues.append(
                f"serialized migration {label} SHA-256 differs: "
                f"expected {expected_hash}, got {actual_hash}")
    receipt_path = resolved.get("receipt")
    if receipt_path is not None:
        try:
            receipt = load_json_strict(receipt_path)
        except StrictJsonError as exc:
            issues.append(f"serialized migration receipt is invalid: {exc}")
        else:
            try:
                from validate_dg_session19_serialized_asset_migration import (
                    validate as validate_serialized_receipt,
                )
            except ImportError as exc:
                issues.append(f"serialized migration validator import failed: {exc}")
            else:
                receipt_issues, _ = validate_serialized_receipt(
                    receipt, root, check_files=True)
                issues.extend(
                    f"serialized migration receipt: {issue}"
                    for issue in receipt_issues
                )
    candidate_policy_path = resolved.get("candidate policy")
    candidate_receipt_path = resolved.get("candidate receipt")
    if candidate_policy_path is not None and candidate_receipt_path is not None:
        try:
            import validate_dg_session19_serialized_asset_migration as serialized_validator

            policy_issues, candidate_authority, observed_binding = (
                serialized_validator.load_candidate_policy(
                    Path(binding["candidatePolicyPath"]), root,
                    check_files=True,
                )
            )
            issues.extend(
                f"serialized migration candidate policy: {issue}"
                for issue in policy_issues
            )
            require_typed_equal(
                observed_binding,
                {
                    "path": binding["candidatePolicyPath"],
                    "bytes": binding["candidatePolicyBytes"],
                    "sha256": binding["candidatePolicySha256"],
                },
                "serialized migration selected candidate policy", issues,
            )
            candidate_receipt = load_json_strict(candidate_receipt_path)
            if candidate_authority is not None:
                candidate_issues, _ = serialized_validator.validate_shipping_evidence(
                    candidate_receipt, root,
                    candidate_authority=candidate_authority,
                    # The release contract deliberately records no host archive,
                    # executable, UnrealPak, or external-run paths.  Validate the
                    # complete selected receipt structure here without inventing
                    # those inputs, then independently verify every project-local
                    # bound file below.
                    check_files=False,
                    live_reaudit=False,
                )
                issues.extend(
                    f"serialized migration candidate receipt: {issue}"
                    for issue in candidate_issues
                )
                issues.extend(
                    _validate_serialized_candidate_project_bindings(
                        root, candidate_receipt, candidate_authority,
                    )
                )
        except (ImportError, StrictJsonError, OSError) as exc:
            issues.append(f"serialized migration candidate evidence is invalid: {exc}")
    return issues


def validate_framework_quarantine_receipt(
        receipt: Any, binding: dict[str, Any] | None = None) -> list[str]:
    issues: list[str] = []
    expected = EXPECTED_FRAMEWORK_QUARANTINE_RECEIPT
    if type(binding) is dict:
        expected = selected_binding_expectation(
            EXPECTED_FRAMEWORK_QUARANTINE_RECEIPT,
            {
                "beforeMoveManifest": binding.get("beforeMoveManifest"),
                "receiptPath": binding.get("receipt", {}).get("path"),
            },
            ("characterFrameworkReceipt",),
        )
    require_typed_equal(
        receipt, expected,
        "character framework external-quarantine receipt", issues,
    )
    if type(receipt) is not dict or receipt != expected:
        return issues
    token = receipt["recoveryLocationToken"]
    if (
            not is_canonical_relative_posix(token)
            or not token.startswith("DGTOUR_EXTERNAL_QUARANTINE/")
            or token.count("/") != 1):
        issues.append(
            "character framework receipt recoveryLocationToken must be a sanitized "
            "logical token, not a host path"
        )
    for index, item in enumerate(receipt["roots"]):
        if not is_canonical_relative_posix(item["projectPath"]):
            issues.append(
                f"character framework receipt roots[{index}].projectPath is not canonical")
        if not is_canonical_relative_posix(item["recoveryRelativePath"]):
            issues.append(
                f"character framework receipt roots[{index}].recoveryRelativePath is not canonical")
    return issues


def validate_framework_quarantine_manifest(path: Path) -> list[str]:
    issues: list[str] = []
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            rows = list(reader)
            fieldnames = reader.fieldnames
    except (OSError, UnicodeError, csv.Error) as exc:
        return [f"character framework before-move manifest cannot be read: {exc}"]
    expected_fields = ["rootId", "relativePath", "bytes", "sha256"]
    if fieldnames != expected_fields:
        issues.append(
            "character framework before-move manifest header differs: "
            f"expected {expected_fields}, got {fieldnames}")
    if len(rows) != 513:
        issues.append(
            "character framework before-move manifest row count differs: "
            f"expected 513, got {len(rows)}")
    expected_roots = {
        "project_plugin": (417, 106181459),
        "buildkit_plugin": (96, 183360),
    }
    observed = {key: [0, 0] for key in expected_roots}
    identities: set[tuple[str, str]] = set()
    for line_number, row in enumerate(rows, 2):
        if set(row) != set(expected_fields) or None in row:
            issues.append(
                f"character framework manifest row {line_number} has malformed fields")
            continue
        root_id = row["rootId"]
        relative_path = row["relativePath"]
        byte_text = row["bytes"]
        digest = row["sha256"]
        if root_id not in expected_roots:
            issues.append(
                f"character framework manifest row {line_number} has unknown rootId")
            continue
        if not is_canonical_relative_posix(relative_path):
            issues.append(
                f"character framework manifest row {line_number} path is not canonical")
        identity_key = (root_id.casefold(), relative_path.casefold())
        if identity_key in identities:
            issues.append(
                f"character framework manifest row {line_number} duplicates or case-collides")
        identities.add(identity_key)
        if not re.fullmatch(r"0|[1-9][0-9]*", byte_text):
            issues.append(
                f"character framework manifest row {line_number} byte count is not canonical")
            continue
        byte_count = int(byte_text)
        if byte_count <= 0:
            issues.append(
                f"character framework manifest row {line_number} byte count must be positive")
        if not SHA256_RE.fullmatch(digest):
            issues.append(
                f"character framework manifest row {line_number} digest must be uppercase SHA-256")
        observed[root_id][0] += 1
        observed[root_id][1] += byte_count
    for root_id, expected in expected_roots.items():
        actual = tuple(observed[root_id])
        if actual != expected:
            issues.append(
                f"character framework manifest {root_id} totals differ: "
                f"expected {expected[0]}/{expected[1]}, got {actual[0]}/{actual[1]}")
    return issues


def validate_framework_quarantine_evidence(
        root: Path, binding: dict[str, Any] | None = None) -> list[str]:
    issues: list[str] = []
    if binding is None:
        try:
            historical = load_json_strict(root / DEFAULT_CONTRACT_PATH)
        except StrictJsonError as exc:
            return [f"historical release-scope contract cannot be loaded: {exc}"]
        binding = (
            historical.get("characterFrameworkExternalQuarantine", {})
            if type(historical) is dict else {}
        )
    receipt_path, receipt_issues = validate_file_binding(
        root, binding.get("receipt", {}), "path", "bytes", "sha256",
        "character framework quarantine receipt",
    )
    issues.extend(receipt_issues)
    manifest_path, manifest_issues = validate_file_binding(
        root, binding.get("beforeMoveManifest", {}), "path", "bytes", "sha256",
        "character framework before-move manifest",
    )
    issues.extend(manifest_issues)
    if receipt_path is not None:
        try:
            receipt = load_json_strict(receipt_path)
        except StrictJsonError as exc:
            issues.append(f"character framework external-quarantine receipt is invalid: {exc}")
        else:
            issues.extend(validate_framework_quarantine_receipt(receipt, binding))
    if manifest_path is not None:
        issues.extend(validate_framework_quarantine_manifest(manifest_path))
    for project_path in binding.get("projectRoots", []):
        candidate = validate_relative_path(
            project_path, f"quarantined framework source {project_path}",
            root, issues, must_exist=False)
        if candidate is not None and (candidate.exists() or candidate.is_symlink()):
            issues.append(
                f"quarantined character framework source still exists inside project: {project_path}")
    return issues


def validate_manual_review_evidence(
        root: Path, binding: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    resolved: dict[str, Path] = {}
    for artifact in binding.get("artifacts", []):
        relative_path = artifact["path"]
        path = validate_relative_path(
            relative_path, f"manual review artifact {relative_path}",
            root, issues, must_exist=True)
        if path is None:
            continue
        resolved[relative_path] = path
        actual_bytes = path.stat().st_size
        actual_hash = sha256_file(path)
        if actual_bytes != artifact["bytes"]:
            issues.append(
                f"manual review artifact byte size differs for {relative_path}: "
                f"expected {artifact['bytes']}, got {actual_bytes}")
        if actual_hash != artifact["sha256"]:
            issues.append(
                f"manual review artifact SHA-256 differs for {relative_path}: "
                f"expected {artifact['sha256']}, got {actual_hash}")
    try:
        from validate_dg_session19_manual_release_review import (
            collect_inventory as collect_manual_inventory,
            validate_inventory as validate_manual_inventory,
            validate_policy as validate_manual_policy,
            validate_template as validate_manual_template,
        )
    except ImportError as exc:
        issues.append(f"manual review validator import failed: {exc}")
        return issues
    document_names = {
        "policy": "DG_Session19ManualReleaseReviewPolicy.json",
        "template": "ManualReleaseDecisionRecord.template.json",
        "inventory": "ManualReleaseReviewInventory.json",
    }
    documents: dict[str, Any] = {}
    for label, file_name in document_names.items():
        matches = [
            path for relative_path, path in resolved.items()
            if PurePosixPath(relative_path).name == file_name
        ]
        if len(matches) != 1:
            issues.append(
                f"manual review {label} binding must identify exactly one {file_name}"
            )
            continue
        path = matches[0]
        try:
            documents[label] = load_json_strict(path)
        except StrictJsonError as exc:
            issues.append(f"manual review {label} is invalid: {exc}")
    policy = documents.get("policy")
    template = documents.get("template")
    inventory = documents.get("inventory")
    if type(policy) is dict:
        issues.extend(f"manual review policy: {issue}" for issue in validate_manual_policy(policy))
    if type(template) is dict:
        issues.extend(f"manual review template: {issue}" for issue in validate_manual_template(template))
    if type(inventory) is dict:
        issues.extend(f"manual review inventory: {issue}" for issue in validate_manual_inventory(inventory))
    if type(policy) is dict and type(inventory) is dict:
        observed, inventory_issues = collect_manual_inventory(policy)
        issues.extend(f"manual review objective inventory: {issue}" for issue in inventory_issues)
        if inventory != observed:
            issues.append("manual review objective inventory is stale")
    return issues


def validate_contract(
        contract: Any, root: Path = ROOT, *, check_files: bool = True,
        authority_contract: dict[str, Any] | None = None,
        contract_binding: dict[str, Any] | None = None,
        candidate_profile_documents: dict[str, Any] | None = None,
) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    authority = authority_contract if type(authority_contract) is dict else contract
    authority = authority if type(authority) is dict else {}
    candidate_section = authority.get("currentShippingCandidateEvidence", {})
    candidate_id = (
        candidate_section.get("candidateId")
        if type(candidate_section) is dict else None
    )
    contract_profile = selected_contract_profile(contract_binding)

    def selected_section(name: str) -> dict[str, Any]:
        value = authority.get(name)
        return value if type(value) is dict else {}

    summary: dict[str, Any] = {
        "session": 19,
        "candidateId": candidate_id,
        "contract": copy.deepcopy(contract_binding),
        "contractProfile": contract_profile,
        "platform": None,
        "configuration": None,
        "holeCount": 0,
        "strategyCount": 0,
        "evidenceGateCount": 0,
        "pendingEvidenceGateCount": 0,
        "partialEvidenceGateCount": 0,
        "failClosedEvidenceGateCount": 0,
        "inheritedBlockerCount": 0,
        "remainingBlockerCount": 0,
        "resolvedBlockerCount": 0,
        "releaseReady": False,
    }

    if not require_exact_keys(contract, ROOT_KEYS, "contract", issues):
        return issues, summary

    scalar_expectations = {
        "schema": "DiscGolfTour.Session19ReleaseScopeContract.v3",
        "schemaVersion": 3,
        "session": 19,
        "contractId": authority.get("contractId"),
        "authority": "ADDITIVE_RELEASE_SCOPE_DECISION_NOT_RELEASE_APPROVAL",
        "normalStatus": (
            "PASS_RELEASE_SCOPE_AUTHORITY_CURRENT_CANDIDATE_"
            "TECHNICAL_EVIDENCE_BOUND_RELEASE_BLOCKED"
        ),
        "releaseStatus": (
            "BLOCKED_INDEPENDENT_PROVENANCE_REVIEW_LEGAL_DISTRIBUTION_"
            "GAMEPLAY_AND_OWNER_REVIEW"
        ),
        "scopeLocked": True,
        "evidenceComplete": False,
        "releaseReady": False,
        "releaseUseAllowed": False,
        "publicReleaseApproved": False,
    }
    for key, expected in scalar_expectations.items():
        require_typed_equal(contract[key], expected, key, issues)

    if type(candidate_id) is not str or not CANDIDATE_ID_RE.fullmatch(candidate_id):
        issues.append("currentShippingCandidateEvidence.candidateId is malformed")
        candidate_id = "INVALID_CANDIDATE"
    validate_selected_binding_shapes(contract, root, issues)
    profile_expectations: dict[str, Any] | None = None
    if contract_profile == FRESH_CANDIDATE_CONTRACT_PROFILE:
        profile_expectations = derive_fresh_candidate_expectations(
            authority, root, issues, candidate_profile_documents,
        )
        if profile_expectations is not None:
            summary.update(profile_expectations["metrics"])

    require_typed_equal(contract["releaseTarget"], EXPECTED_RELEASE_TARGET, "releaseTarget", issues)
    require_typed_equal(contract["featureScope"], EXPECTED_FEATURE_SCOPE, "featureScope", issues)
    require_typed_equal(
        contract["unusedFabExternalQuarantine"], selected_binding_expectation(
            EXPECTED_QUARANTINE, authority.get("unusedFabExternalQuarantine"),
            ("unusedFabExternalQuarantine",),
        ),
        "unusedFabExternalQuarantine", issues,
    )
    require_typed_equal(
        contract["shippingContentPolicy"], selected_binding_expectation(
            EXPECTED_SHIPPING_POLICY_BINDING, authority.get("shippingContentPolicy"),
            ("shippingContentPolicy",),
        ),
        "shippingContentPolicy", issues,
    )
    require_typed_equal(
        contract["shippingPluginCapability"], selected_binding_expectation(
            EXPECTED_SHIPPING_PLUGIN_BINDING, authority.get("shippingPluginCapability"),
            ("shippingPluginCapability",),
        ),
        "shippingPluginCapability", issues,
    )
    require_typed_equal(
        contract["shippingPackageHardening"], selected_binding_expectation(
            EXPECTED_SHIPPING_PACKAGE_HARDENING_BINDING,
            authority.get("shippingPackageHardening"),
            ("shippingPackageHardening",),
        ),
        "shippingPackageHardening", issues,
    )
    if contract_profile == HISTORICAL_CONTRACT_PROFILE or profile_expectations is not None:
        staged_expected = (
            profile_expectations["stagedProvenancePolicy"]
            if profile_expectations is not None
            else selected_binding_expectation(
                EXPECTED_STAGED_PROVENANCE_BINDING,
                authority.get("stagedProvenancePolicy"),
                ("stagedProvenancePolicy",),
            )
        )
        current_expected = (
            profile_expectations["currentShippingCandidateEvidence"]
            if profile_expectations is not None
            else selected_binding_expectation(
                EXPECTED_CURRENT_CANDIDATE_EVIDENCE,
                authority.get("currentShippingCandidateEvidence"),
                ("currentShippingCandidateEvidence",),
            )
        )
        require_typed_equal(
            contract["stagedProvenancePolicy"], staged_expected,
            "stagedProvenancePolicy", issues,
        )
        require_typed_equal(
            contract["currentShippingCandidateEvidence"], current_expected,
            "currentShippingCandidateEvidence", issues,
        )
    for key, expected, label in (
        ("externalTechnicalEvidence", EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE,
         "externalTechnicalEvidence"),
        ("v05FeatureExclusion", EXPECTED_V05_FEATURE_EXCLUSION, "v05FeatureExclusion"),
        ("v05EquipmentTechnical", EXPECTED_V05_EQUIPMENT_TECHNICAL,
         "v05EquipmentTechnical"),
        ("physicsMeasuredReferencePreparation",
         EXPECTED_PHYSICS_MEASURED_REFERENCE_PREPARATION,
         "physicsMeasuredReferencePreparation"),
        ("session10EnvironmentTechnical", EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL,
         "session10EnvironmentTechnical"),
        ("session12PresentationTechnical", EXPECTED_SESSION12_PRESENTATION_TECHNICAL,
         "session12PresentationTechnical"),
        ("technicalOriginAttribution", EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION,
         "technicalOriginAttribution"),
        ("threeHoleTechnicalAcceptance", EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE,
         "threeHoleTechnicalAcceptance"),
    ):
        if (
                contract_profile == FRESH_CANDIDATE_CONTRACT_PROFILE
                and key in {
                    "v05EquipmentTechnical",
                    "session10EnvironmentTechnical",
                    "session12PresentationTechnical",
                    "technicalOriginAttribution",
                    "threeHoleTechnicalAcceptance",
                }
        ):
            if profile_expectations is None:
                continue
            dynamic_expected = profile_expectations[key]
        else:
            dynamic_expected = selected_binding_expectation(
                expected, authority.get(key), (key,)
            )
        require_typed_equal(
            contract[key],
            dynamic_expected,
            label, issues,
        )
    require_typed_equal(
        contract["bakedPcgSourceAuthority"], selected_binding_expectation(
            EXPECTED_BAKED_PCG_BINDING, authority.get("bakedPcgSourceAuthority"),
            ("bakedPcgSourceAuthority",),
        ),
        "bakedPcgSourceAuthority", issues,
    )
    require_typed_equal(
        contract["bakedPcgRuntimeClosure"], selected_binding_expectation(
            EXPECTED_BAKED_PCG_RUNTIME_CLOSURE, authority.get("bakedPcgRuntimeClosure"),
            ("bakedPcgRuntimeClosure",),
        ),
        "bakedPcgRuntimeClosure", issues,
    )
    require_typed_equal(
        contract["productionMotionPreparation"],
        profile_expectations["productionMotionPreparation"]
        if profile_expectations is not None
        else selected_binding_expectation(
            EXPECTED_PRODUCTION_MOTION_PREPARATION,
            authority.get("productionMotionPreparation"),
            ("productionMotionPreparation",),
        ),
        "productionMotionPreparation", issues,
    )
    require_typed_equal(
        contract["serializedAssetMigration"],
        profile_expectations["serializedAssetMigration"]
        if profile_expectations is not None
        else selected_binding_expectation(
            EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING,
            authority.get("serializedAssetMigration"),
            ("serializedAssetMigration",),
        ),
        "serializedAssetMigration", issues,
    )
    require_typed_equal(
        contract["characterFrameworkExternalQuarantine"],
        selected_binding_expectation(
            EXPECTED_FRAMEWORK_QUARANTINE_BINDING,
            authority.get("characterFrameworkExternalQuarantine"),
            ("characterFrameworkExternalQuarantine",),
        ),
        "characterFrameworkExternalQuarantine", issues,
    )
    require_typed_equal(
        contract["manualReleaseReview"], selected_binding_expectation(
            EXPECTED_MANUAL_REVIEW_BINDING, authority.get("manualReleaseReview"),
            ("manualReleaseReview",),
        ),
        "manualReleaseReview", issues,
    )
    for section_name, template in (
        ("currentShippingCandidateEvidence", EXPECTED_CURRENT_CANDIDATE_EVIDENCE),
        ("externalTechnicalEvidence", EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE),
        ("v05FeatureExclusion", EXPECTED_V05_FEATURE_EXCLUSION),
        ("v05EquipmentTechnical", EXPECTED_V05_EQUIPMENT_TECHNICAL),
        ("session10EnvironmentTechnical", EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL),
        ("session12PresentationTechnical", EXPECTED_SESSION12_PRESENTATION_TECHNICAL),
        ("technicalOriginAttribution", EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION),
        ("threeHoleTechnicalAcceptance", EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE),
        ("bakedPcgSourceAuthority", EXPECTED_BAKED_PCG_BINDING),
        ("bakedPcgRuntimeClosure", EXPECTED_BAKED_PCG_RUNTIME_CLOSURE),
        ("productionMotionPreparation", EXPECTED_PRODUCTION_MOTION_PREPARATION),
        ("serializedAssetMigration", EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING),
    ):
        validate_candidate_binding_templates(
            template, contract.get(section_name), candidate_id, issues,
            (section_name,),
        )
    summary["platform"] = contract["releaseTarget"].get("platform") if type(contract["releaseTarget"]) is dict else None
    summary["configuration"] = contract["releaseTarget"].get("configuration") if type(contract["releaseTarget"]) is dict else None
    holes = contract["releaseTarget"].get("holeNumbers", []) if type(contract["releaseTarget"]) is dict else []
    summary["holeCount"] = len(holes) if type(holes) is list else 0

    for index, path_value in enumerate(
            contract["unusedFabExternalQuarantine"].get("projectRoots", [])
            if type(contract["unusedFabExternalQuarantine"]) is dict else []):
        validate_relative_path(
            path_value, f"unusedFabExternalQuarantine.projectRoots[{index}]",
            root, issues, must_exist=False,
        )
    for index, path_value in enumerate(
            contract["characterFrameworkExternalQuarantine"].get("projectRoots", [])
            if type(contract["characterFrameworkExternalQuarantine"]) is dict else []):
        validate_relative_path(
            path_value, f"characterFrameworkExternalQuarantine.projectRoots[{index}]",
            root, issues, must_exist=False,
        )

    strategies = contract["closureStrategies"]
    if type(strategies) is not list:
        issues.append("closureStrategies must be an array")
        strategies = []
    summary["strategyCount"] = len(strategies)
    authority_strategies = authority.get("closureStrategies", [])
    if type(authority_strategies) is not list:
        authority_strategies = []
    strategy_keys = {
        "blockerId", "resolutionMode", "approvedStrategy", "initialState",
        "closed", "evidenceRequirements",
    }
    observed_strategy_ids: list[str] = []
    for index, item in enumerate(strategies):
        label = f"closureStrategies[{index}]"
        if not require_exact_keys(item, strategy_keys, label, issues):
            continue
        blocker_id = item["blockerId"]
        if type(blocker_id) is not str:
            issues.append(f"{label}.blockerId must be a string")
            continue
        observed_strategy_ids.append(blocker_id)
        expected = EXPECTED_STRATEGIES.get(blocker_id)
        if expected is None:
            issues.append(f"{label}.blockerId is not an inherited blocker")
            continue
        require_typed_equal(item["resolutionMode"], expected[0], f"{label}.resolutionMode", issues)
        require_typed_equal(item["approvedStrategy"], expected[1], f"{label}.approvedStrategy", issues)
        require_typed_equal(item["initialState"], "PENDING_EVIDENCE", f"{label}.initialState", issues)
        require_typed_equal(item["closed"], False, f"{label}.closed", issues)
        authority_item = (
            authority_strategies[index]
            if index < len(authority_strategies)
            and type(authority_strategies[index]) is dict
            else {}
        )
        expected_requirements = selected_binding_expectation(
            expected[2], authority_item.get("evidenceRequirements"),
            ("closureStrategies", index, "evidenceRequirements"),
        )
        require_typed_equal(
            item["evidenceRequirements"], expected_requirements,
            f"{label}.evidenceRequirements", issues,
        )
        validate_candidate_binding_templates(
            expected[2], item["evidenceRequirements"], candidate_id, issues,
            ("closureStrategies", index, "evidenceRequirements"),
        )
    require_typed_equal(observed_strategy_ids, BLOCKERS, "closureStrategies blocker order", issues)
    if len({value.casefold() for value in observed_strategy_ids}) != len(observed_strategy_ids):
        issues.append("closureStrategies contains duplicate or case-colliding blocker IDs")

    gates = contract["evidenceGates"]
    if type(gates) is not list:
        issues.append("evidenceGates must be an array")
        gates = []
    summary["evidenceGateCount"] = len(gates)
    observed_gate_ids: list[str] = []
    gate_keys = {"gateId", "state", "accepted", "artifactPaths"}
    for index, gate in enumerate(gates):
        label = f"evidenceGates[{index}]"
        if not require_exact_keys(gate, gate_keys, label, issues):
            continue
        if type(gate["gateId"]) is str:
            observed_gate_ids.append(gate["gateId"])
        else:
            issues.append(f"{label}.gateId must be a string")
        if type(gate["artifactPaths"]) is list:
            for path_index, path_value in enumerate(gate["artifactPaths"]):
                validate_relative_path(
                    path_value, f"{label}.artifactPaths[{path_index}]",
                    root, issues, must_exist=check_files,
                )
    require_typed_equal(
        gates,
        selected_binding_expectation(
            EXPECTED_GATES, authority.get("evidenceGates"), ("evidenceGates",)
        ),
        "evidenceGates", issues,
    )
    validate_candidate_binding_templates(
        EXPECTED_GATES, gates, candidate_id, issues, ("evidenceGates",)
    )
    require_typed_equal(observed_gate_ids, EXPECTED_GATE_IDS, "evidence gate order", issues)
    summary["pendingEvidenceGateCount"] = sum(
        type(gate) is dict and gate.get("state") == "PENDING" and gate.get("accepted") is False
        for gate in gates
    )
    summary["partialEvidenceGateCount"] = sum(
        type(gate) is dict and str(gate.get("state", "")).startswith("PARTIAL_")
        and gate.get("accepted") is False
        for gate in gates
    )
    summary["failClosedEvidenceGateCount"] = sum(
        type(gate) is dict and str(gate.get("state", "")).startswith("FAIL_CLOSED")
        and gate.get("accepted") is False
        for gate in gates
    )

    require_typed_equal(contract["inheritedReleaseBlockers"], BLOCKERS, "inheritedReleaseBlockers", issues)
    require_typed_equal(contract["resolvedBySession19"], [], "resolvedBySession19", issues)
    require_typed_equal(contract["remainingReleaseBlockers"], BLOCKERS, "remainingReleaseBlockers", issues)
    summary["inheritedBlockerCount"] = len(contract["inheritedReleaseBlockers"]) if type(contract["inheritedReleaseBlockers"]) is list else 0
    summary["resolvedBlockerCount"] = len(contract["resolvedBySession19"]) if type(contract["resolvedBySession19"]) is list else 0
    summary["remainingBlockerCount"] = len(contract["remainingReleaseBlockers"]) if type(contract["remainingReleaseBlockers"]) is list else 0
    summary["releaseReady"] = contract["releaseReady"] is True

    require_typed_equal(contract["closureSummary"], {
        "inheritedBlockerCount": 13,
        "resolvedBySession19Count": 0,
        "remainingBlockerCount": 13,
        "allStrategiesApproved": True,
        "acceptedEvidenceGateCount": 0,
        "partiallySatisfiedEvidenceGateCount": 11,
        "failClosedEvidenceGateCount": 0,
        "releaseReady": False,
    }, "closureSummary", issues)
    required_files = [
        "Config/DG_Session19ReleaseScopeContract.json",
        "Scripts/validate_dg_session19_release_scope.py",
        "Config/DG_Session19ShippingContentPolicy.json",
        "Scripts/validate_dg_session19_candidate_content.py",
        "Config/DefaultPakFileRules.ini",
        "Config/DefaultGame.ini",
        "DiscGolfTour.uproject",
        "Source/DiscGolfTour.Target.cs",
        "Config/DG_Session19ShippingPluginCapabilityPolicy.json",
        "Scripts/validate_dg_session19_shipping_plugin_capabilities.py",
        "Scripts/run-session19-shipping-candidate.ps1",
        "Scripts/verify-session19-shipping-candidate.ps1",
        "Source/DiscGolfTour/DiscGolfTour.Build.cs",
        "Config/DG_Session19StagedProvenancePolicy.json",
        "Scripts/generate_dg_session19_staged_provenance.py",
        *[
            EXPECTED_CURRENT_CANDIDATE_EVIDENCE[key]["path"]
            for key in (
                "buildLog", "buildReceipt", "verificationReceipt",
                "pluginCapabilityReceipt", "binaryReceipt", "contentReceipt",
                "thirdPartyNoticeReceipt", "provenanceClassificationDraft",
                "provenanceDraftAudit", "freshUserDirValidator", "freshUserDirReceipt",
            )
        ],
        *[
            binding["path"]
            for expected, keys in (
                (EXPECTED_EXTERNAL_TECHNICAL_EVIDENCE,
                 ("policy", "validator", "runner", "receipt")),
                (EXPECTED_V05_FEATURE_EXCLUSION, ("policy", "validator", "receipt")),
                (EXPECTED_V05_EQUIPMENT_TECHNICAL, ("policy", "validator", "receipt")),
                (EXPECTED_PHYSICS_MEASURED_REFERENCE_PREPARATION,
                 ("policy", "validator", "datasetTemplate")),
                (EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL,
                 ("technicalPlan", "policy", "validator", "receipt")),
                (EXPECTED_SESSION12_PRESENTATION_TECHNICAL,
                 ("audioEventCoverageManifest", "policy", "validator", "receipt",
                  "shippingAudioCookReceipt")),
                (EXPECTED_TECHNICAL_ORIGIN_ATTRIBUTION,
                 ("policy", "validator", "proposalGenerator", "proposalValidator", "proposal", "receipt")),
                (EXPECTED_THREE_HOLE_TECHNICAL_ACCEPTANCE, ("policy", "validator", "receipt")),
            )
            for binding in (expected[key] for key in keys)
        ],
        "Config/DG_Session19BakedPcgPolicy.json",
        "Scripts/validate_dg_session19_baked_pcg.py",
        "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json",
        EXPECTED_BAKED_PCG_BINDING["sourceClosurePreBuildReceiptPath"],
        EXPECTED_BAKED_PCG_BINDING["candidateReceiptPath"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["policy"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["validator"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingSeparationPolicy"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingSeparationValidator"]["path"],
        EXPECTED_BAKED_PCG_RUNTIME_CLOSURE["shippingPcgSeparationReceipt"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["validator"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["evidence"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["shippingCookPresenceReceipt"]["path"],
        "Config/DG_Session19ShippingAudioPolicy.json",
        "Config/DG_Session19SerializedAssetMigrationPolicy.json",
        "Scripts/migrate_dg_session19_serialized_assets.py",
        "Scripts/validate_dg_session19_serialized_asset_migration.py",
        "Evidence/Session19/SerializedAssetMigrationInventory.tsv",
        "Evidence/Session19/SerializedAssetMigrationReceipt.json",
        EXPECTED_SERIALIZED_ASSET_MIGRATION_BINDING["candidateReceiptPath"],
        FRAMEWORK_QUARANTINE_MANIFEST_PATH,
        FRAMEWORK_QUARANTINE_RECEIPT_PATH,
        "Evidence/Session19/UnusedFabExternalQuarantineReceipt.json",
        "Evidence/Session19/QuarantinedFabBeforeMove.tsv",
        *[item["path"] for item in EXPECTED_MANUAL_REVIEW_ARTIFACTS],
    ]
    # Required file identities are contract-versioned bindings.  The list above
    # remains only the historical semantic template; the selected authority is
    # the source of truth for validation and file resolution.
    required_files = authority.get("requiredFiles")
    require_typed_equal(contract["requiredFiles"], required_files, "requiredFiles", issues)
    if type(contract_binding) is dict:
        selected_contract_path = contract_binding.get("path")
        if (
                type(required_files) is not list
                or selected_contract_path not in required_files):
            issues.append(
                "requiredFiles must bind the explicitly selected contract path"
            )
    if (
            contract_profile == FRESH_CANDIDATE_CONTRACT_PROFILE
            and type(required_files) is list):
        required_profile_paths: list[Any] = [
            selected_section("stagedProvenancePolicy").get("path"),
            selected_section("stagedProvenancePolicy").get("generatorPath"),
        ]
        selected_current = selected_section("currentShippingCandidateEvidence")
        required_profile_paths.extend(
            selected_current.get(key, {}).get("path")
            for key in (
                "buildLog", "buildReceipt", "verificationReceipt",
                "pluginCapabilityReceipt", "binaryReceipt", "contentReceipt",
                "thirdPartyNoticeReceipt", "provenanceClassificationDraft",
                "provenanceDraftAudit", "freshUserDirValidator", "freshUserDirReceipt",
            )
        )
        selected_origin = selected_section("technicalOriginAttribution")
        required_profile_paths.extend(
            selected_origin.get(key, {}).get("path")
            for key in (
                "policy", "validator", "proposalGenerator", "proposalValidator",
                "proposal", "receipt",
            )
        )
        selected_three = selected_section("threeHoleTechnicalAcceptance")
        required_profile_paths.extend(
            selected_three.get(key, {}).get("path")
            for key in ("policy", "validator", "receipt")
        )
        selected_equipment = selected_section("v05EquipmentTechnical")
        required_profile_paths.extend(
            selected_equipment.get(key, {}).get("path")
            for key in ("policy", "validator", "receipt")
        )
        selected_session10 = selected_section("session10EnvironmentTechnical")
        required_profile_paths.extend(
            selected_session10.get(key, {}).get("path")
            for key in ("technicalPlan", "policy", "validator", "receipt")
        )
        selected_session12 = selected_section("session12PresentationTechnical")
        required_profile_paths.extend(
            selected_session12.get(key, {}).get("path")
            for key in (
                "audioEventCoverageManifest", "policy", "validator", "receipt",
                "shippingAudioCookReceipt",
            )
        )
        selected_production_motion = selected_section(
            "productionMotionPreparation"
        )
        required_profile_paths.extend(
            selected_production_motion.get(key, {}).get("path")
            for key in (
                "policy", "validator", "evidence",
                "shippingCookPresenceReceipt",
            )
        )
        selected_serialized = selected_section("serializedAssetMigration")
        required_profile_paths.extend(
            selected_serialized.get(key)
            for key in (
                "policyPath", "candidatePolicyPath", "validatorPath", "receiptPath",
                "inventoryPath", "migrationScriptPath", "candidateReceiptPath",
            )
        )
        for path_value in required_profile_paths:
            if type(path_value) is not str or path_value not in required_files:
                issues.append(
                    "fresh candidate requiredFiles omits a selected evidence binding: "
                    f"{path_value!r}"
                )
    for index, path_value in enumerate(contract["requiredFiles"] if type(contract["requiredFiles"]) is list else []):
        validate_relative_path(path_value, f"requiredFiles[{index}]", root, issues, must_exist=check_files)
    require_typed_equal(contract["validation"], {
        "validatorPath": "Scripts/validate_dg_session19_release_scope.py",
        "normalExpectedExitCode": 0,
        "releaseRequiredExpectedExitCode": 2,
        "selfTestMinimumMutationCount": 200,
        "normalModeMustRemainReleaseBlocked": True,
        "writesReports": False,
    }, "validation", issues)
    if type(contract["validation"]) is dict:
        validate_relative_path(
            contract["validation"].get("validatorPath"), "validation.validatorPath",
            root, issues, must_exist=check_files,
        )

    continuity = contract["continuity"]
    continuity_keys = {"policy", "sourceAuthority", "frozenFiles"}
    if require_exact_keys(continuity, continuity_keys, "continuity", issues):
        require_typed_equal(
            continuity["policy"],
            "SESSIONS_9_THROUGH_18_ARE_IMMUTABLE_HISTORICAL_EVIDENCE",
            "continuity.policy", issues,
        )
        require_typed_equal(
            continuity["sourceAuthority"],
            "Config/DG_Session18PolyHavenProvenanceContract.json",
            "continuity.sourceAuthority", issues,
        )
        validate_relative_path(
            continuity["sourceAuthority"], "continuity.sourceAuthority",
            root, issues, must_exist=check_files,
        )
        frozen = continuity["frozenFiles"]
        if type(frozen) is not list:
            issues.append("continuity.frozenFiles must be an array")
            frozen = []
        frozen_keys = {"path", "bytes", "sha256"}
        observed_paths: list[str] = []
        for index, item in enumerate(frozen):
            label = f"continuity.frozenFiles[{index}]"
            if not require_exact_keys(item, frozen_keys, label, issues):
                continue
            path_value = item["path"]
            if type(path_value) is str:
                observed_paths.append(path_value)
            else:
                issues.append(f"{label}.path must be a string")
                continue
            expected = EXPECTED_FROZEN_HISTORY.get(path_value)
            if expected is None:
                issues.append(f"{label}.path is not in the immutable Session 9-18 inventory")
                continue
            require_typed_equal(item["bytes"], expected[0], f"{label}.bytes", issues)
            require_typed_equal(item["sha256"], expected[1], f"{label}.sha256", issues)
            if type(item["sha256"]) is not str or not SHA256_RE.fullmatch(item["sha256"]):
                issues.append(f"{label}.sha256 must be uppercase SHA-256")
            file_path = validate_relative_path(
                path_value, f"{label}.path", root, issues, must_exist=check_files,
            )
            if check_files and file_path is not None:
                stat = file_path.stat()
                if stat.st_size != expected[0]:
                    issues.append(f"{label} byte size drifted: expected {expected[0]}, got {stat.st_size}")
                actual_hash = sha256_file(file_path)
                if actual_hash != expected[1]:
                    issues.append(f"{label} SHA-256 drifted: expected {expected[1]}, got {actual_hash}")
        require_typed_equal(
            observed_paths, list(EXPECTED_FROZEN_HISTORY),
            "continuity.frozenFiles path order", issues,
        )
        if len({value.casefold() for value in observed_paths}) != len(observed_paths):
            issues.append("continuity.frozenFiles contains duplicate or case-colliding paths")

    if check_files:
        issues.extend(validate_quarantine_evidence(
            root, selected_section("unusedFabExternalQuarantine")))
        issues.extend(validate_shipping_policy_evidence(
            root, selected_section("shippingContentPolicy"),
            selected_section("shippingPackageHardening"),
            authority.get("requiredFiles", [])
            if type(authority.get("requiredFiles")) is list else []))
        issues.extend(validate_shipping_plugin_capability_evidence(
            root, selected_section("shippingPluginCapability")))
        issues.extend(validate_shipping_package_hardening_evidence(
            root, selected_section("shippingPackageHardening")))
        issues.extend(validate_current_candidate_evidence(
            root, selected_section("currentShippingCandidateEvidence")))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("externalTechnicalEvidence"),
            ("policy", "validator", "runner", "receipt"), "external technical evidence"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("v05FeatureExclusion"),
            ("policy", "validator", "receipt"), "v0.5 feature exclusion"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("v05EquipmentTechnical"),
            ("policy", "validator", "receipt"), "v0.5 equipment technical evidence"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("physicsMeasuredReferencePreparation"),
            ("policy", "validator", "datasetTemplate"),
            "measured-reference capture preparation"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("session10EnvironmentTechnical"),
            ("technicalPlan", "policy", "validator", "receipt"),
            "Session 10 environment technical evidence"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("session12PresentationTechnical"),
            ("audioEventCoverageManifest", "policy", "validator", "receipt",
             "shippingAudioCookReceipt"),
            "Session 12 presentation technical evidence"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("technicalOriginAttribution"),
            ("policy", "validator", "proposalGenerator", "proposalValidator", "proposal", "receipt"),
            "technical-origin attribution"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("threeHoleTechnicalAcceptance"),
            ("policy", "validator", "receipt"), "three-hole technical acceptance"))
        issues.extend(validate_staged_provenance_policy_evidence(
            root, selected_section("stagedProvenancePolicy"),
            selected_section("shippingPluginCapability")))
        issues.extend(validate_baked_pcg_evidence(
            root, selected_section("bakedPcgSourceAuthority")))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("bakedPcgRuntimeClosure"),
            ("policy", "validator", "shippingSeparationPolicy",
             "shippingSeparationValidator", "shippingPcgSeparationReceipt"),
            "retired legacy PCG closure and fresh Shipping separation"))
        issues.extend(validate_bound_evidence_bundle(
            root, selected_section("productionMotionPreparation"),
            ("policy", "validator", "evidence", "shippingCookPresenceReceipt"),
            "production-motion authoring preparation"))
        try:
            production_binding = selected_section("productionMotionPreparation")
            production_motion_policy = production_motion_validator.load_json(
                root / production_binding["policy"]["path"]
            )
            production_motion_issues, _ = production_motion_validator.validate_policy(
                production_motion_policy, root, check_files=True,
                expected_candidate_id=(
                    candidate_id
                    if contract_profile == FRESH_CANDIDATE_CONTRACT_PROFILE
                    else None
                ),
            )
        except Exception as exc:
            issues.append(
                f"production-motion current-policy validation failed: {exc}"
            )
        else:
            issues.extend(
                f"production-motion current-policy validation: {issue}"
                for issue in production_motion_issues
            )
        issues.extend(validate_serialized_asset_migration_evidence(
            root, selected_section("serializedAssetMigration")))
        issues.extend(validate_framework_quarantine_evidence(
            root, selected_section("characterFrameworkExternalQuarantine")))
        issues.extend(validate_manual_review_evidence(
            root, selected_section("manualReleaseReview")))
        continuity_binding = selected_section("continuity")
        source_path = validate_relative_path(
            continuity_binding.get("sourceAuthority"),
            "Session 18 source authority", root, issues, must_exist=True,
        )
        if source_path is not None:
            try:
                source = load_json_strict(source_path)
            except StrictJsonError as exc:
                issues.append(f"Session 18 source authority is invalid: {exc}")
            else:
                if type(source) is not dict:
                    issues.append("Session 18 source authority must be an object")
                else:
                    require_typed_equal(
                        source.get("remainingReleaseBlockers"), BLOCKERS,
                        "Session 18 remainingReleaseBlockers continuity", issues,
                    )
                    exit_gate = source.get("exitGate")
                    if type(exit_gate) is not dict:
                        issues.append("Session 18 exitGate continuity must be an object")
                    else:
                        require_typed_equal(
                            exit_gate.get("publicReleaseReady"), False,
                            "Session 18 publicReleaseReady continuity", issues,
                        )
                        require_typed_equal(
                            exit_gate.get("remainingReleaseBlockerCount"), 13,
                            "Session 18 remainingReleaseBlockerCount continuity", issues,
                        )

    return issues, summary


Mutation = tuple[str, Callable[[dict[str, Any]], None]]


def _set(mapping: dict[str, Any], key: str, value: Any) -> None:
    mapping[key] = value


def run_self_test(
        contract: dict[str, Any], root: Path,
        contract_binding: dict[str, Any] | None = None,
) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    authority = copy.deepcopy(contract)
    base_issues, _ = validate_contract(
        # The default contract is an immutable historical snapshot. Its self-test
        # exercises that exact schema without pretending subsequently evolved
        # tooling still has the historical receipt hashes.
        contract, root, check_files=False, authority_contract=authority,
        contract_binding=contract_binding)
    if base_issues:
        failures.append("baseline contract failed: " + "; ".join(base_issues[:5]))

    parser_cases = {
        "duplicate-key": '{"a":1,"a":2}',
        "nan": '{"a":NaN}',
        "positive-infinity": '{"a":Infinity}',
        "negative-infinity": '{"a":-Infinity}',
    }
    count = 0
    for name, text in parser_cases.items():
        count += 1
        try:
            load_json_text_strict(text)
        except (StrictJsonError, json.JSONDecodeError):
            pass
        else:
            failures.append(f"parser mutation survived: {name}")

    mutations: list[Mutation] = [
        ("extra-root-key", lambda c: _set(c, "unexpected", True)),
        ("missing-root-key", lambda c: c.pop("authority")),
        ("schema", lambda c: _set(c, "schema", "wrong")),
        ("schema-bool-version", lambda c: _set(c, "schemaVersion", True)),
        ("contract-id", lambda c: _set(c, "contractId", "session19_windows_shipping_v05_scope_authority_v2")),
        ("session", lambda c: _set(c, "session", 18)),
        ("authority", lambda c: _set(c, "authority", "RELEASE_APPROVAL")),
        ("normal-status", lambda c: _set(c, "normalStatus", "PASS")),
        ("release-status", lambda c: _set(c, "releaseStatus", "RELEASE_READY")),
        ("scope-unlocked", lambda c: _set(c, "scopeLocked", False)),
        ("evidence-complete", lambda c: _set(c, "evidenceComplete", True)),
        ("release-ready", lambda c: _set(c, "releaseReady", True)),
        ("release-use", lambda c: _set(c, "releaseUseAllowed", True)),
        ("public-approved", lambda c: _set(c, "publicReleaseApproved", True)),
        ("platform", lambda c: _set(c["releaseTarget"], "platform", "Linux")),
        ("configuration", lambda c: _set(c["releaseTarget"], "configuration", "Development")),
        ("milestone", lambda c: _set(c["releaseTarget"], "milestone", "v1.0")),
        ("hole-list", lambda c: _set(c["releaseTarget"], "holeNumbers", [1, 2])),
        ("hole-bool", lambda c: _set(c["releaseTarget"], "holeNumbers", [1, 2, True])),
        ("career-included", lambda c: _set(c["featureScope"], "career", "INCLUDED")),
        ("ai-included", lambda c: _set(c["featureScope"], "aiOpponents", "INCLUDED")),
        ("throwlab-shipping", lambda c: _set(c["featureScope"], "throwLab", "SHIPPING")),
        ("runtime-pcg", lambda c: _set(c["featureScope"], "runtimePcgGenerationAllowed", True)),
        ("character-framework-retained", lambda c: _set(c["featureScope"], "characterFramework", "RETAIN")),
        ("fab-root-missing", lambda c: c["unusedFabExternalQuarantine"]["projectRoots"].pop()),
        ("fab-root-absolute", lambda c: _set(c["unusedFabExternalQuarantine"], "projectRoots", ["C:/escape", "Content/Stump_Scanned", "Content/WaterMaterials"])),
        ("fab-root-traversal", lambda c: _set(c["unusedFabExternalQuarantine"], "projectRoots", ["../escape", "Content/Stump_Scanned", "Content/WaterMaterials"])),
        ("fab-root-backslash", lambda c: _set(c["unusedFabExternalQuarantine"], "projectRoots", ["Content\\PN_interactiveSpruceForest", "Content/Stump_Scanned", "Content/WaterMaterials"])),
        ("fab-host-paths", lambda c: _set(c["unusedFabExternalQuarantine"], "hostPathsRecorded", True)),
        ("fab-shipping-cook", lambda c: _set(c["unusedFabExternalQuarantine"], "shippingCookAllowed", True)),
        ("fab-receipt-hash", lambda c: _set(c["unusedFabExternalQuarantine"]["receipt"], "sha256", "0" * 64)),
        ("fab-blocker-closed", lambda c: _set(c["unusedFabExternalQuarantine"], "blockerClosed", True)),
        ("policy-hash", lambda c: _set(c["shippingContentPolicy"], "sha256", "0" * 64)),
        ("policy-validator-hash", lambda c: _set(c["shippingContentPolicy"], "validatorSha256", "0" * 64)),
        ("policy-content-receipt", lambda c: _set(c["shippingContentPolicy"], "candidateContentReceiptAccepted", False)),
        ("policy-gameplay-claim", lambda c: _set(c["shippingContentPolicy"], "freshInstallGameplayAcceptanceAccepted", True)),
        ("policy-closes-blocker", lambda c: _set(c["shippingContentPolicy"], "closesReleaseBlocker", True)),
        ("policy-ordinal-candidate-state", lambda c: _set(c["shippingContentPolicy"], "currentState", "CANDIDATE16_PASS")),
        ("plugin-policy-hash", lambda c: _set(c["shippingPluginCapability"], "policySha256", "0" * 64)),
        ("plugin-validator-hash", lambda c: _set(c["shippingPluginCapability"], "validatorSha256", "0" * 64)),
        ("plugin-runner-hash", lambda c: _set(c["shippingPluginCapability"], "runnerSha256", "0" * 64)),
        ("plugin-verifier-hash", lambda c: _set(c["shippingPluginCapability"], "verifierSha256", "0" * 64)),
        ("plugin-preserved-count", lambda c: _set(c["shippingPluginCapability"], "requiredPreservedPluginCount", 4)),
        ("plugin-absent-count", lambda c: _set(c["shippingPluginCapability"], "requiredAbsentPluginCount", 0)),
        ("plugin-absent-survivor", lambda c: _set(c["shippingPluginCapability"], "requiredAbsentSurvivorCount", 1)),
        ("plugin-schema-version", lambda c: _set(c["shippingPluginCapability"], "policySchemaVersion", 2)),
        ("plugin-closure-count", lambda c: _set(c["shippingPluginCapability"], "requiredDependencyClosurePluginCount", 14)),
        ("plugin-fresh-dependency-closure-receipt", lambda c: _set(c["shippingPluginCapability"], "freshReviewedDependencyClosureReceiptAccepted", False)),
        ("plugin-release-ready", lambda c: _set(c["shippingPluginCapability"], "releaseReady", True)),
        ("hardening-pak-hash", lambda c: _set(c["shippingPackageHardening"]["defaultPakFileRules"], "sha256", "0" * 64)),
        ("hardening-game-hash", lambda c: _set(c["shippingPackageHardening"]["defaultGame"], "sha256", "0" * 64)),
        ("hardening-uproject-hash", lambda c: _set(c["shippingPackageHardening"]["projectDescriptor"], "sha256", "0" * 64)),
        ("hardening-target-hash", lambda c: _set(c["shippingPackageHardening"]["shippingGameTarget"], "sha256", "0" * 64)),
        ("hardening-nevercook-root", lambda c: c["shippingPackageHardening"]["requiredNeverCookVirtualRoots"].pop()),
        ("hardening-landmass-ignore", lambda c: c["shippingPackageHardening"]["shippingIgnoredPluginDependencies"].append("Landmass")),
        ("hardening-landmass-deny", lambda c: c["shippingPackageHardening"]["projectTargetConfigurationShippingDeniedPlugins"].append("Landmass")),
        ("hardening-landmass-final-root", lambda c: c["shippingPackageHardening"]["shippingForbiddenPluginContentRoots"].pop()),
        ("hardening-required-present-plugin", lambda c: c["shippingPackageHardening"]["requiredPresentFinalPayloadPluginDescriptors"].pop()),
        ("hardening-required-absent-plugin", lambda c: c["shippingPackageHardening"]["requiredAbsentFinalPayloadPluginDescriptors"].pop()),
        ("hardening-closure-plugin", lambda c: c["shippingPackageHardening"]["requiredDependencyClosureFinalPayloadDescriptors"].pop()),
        ("hardening-metadata-only-plugin", lambda c: c["shippingPackageHardening"]["reviewedMetadataOnlyDependencyDescriptors"].pop()),
        ("hardening-fresh-proof", lambda c: _set(c["shippingPackageHardening"], "freshShippingPackageProofAccepted", False)),
        ("hardening-release-ready", lambda c: _set(c["shippingPackageHardening"], "releaseReady", True)),
        ("staged-policy-hash", lambda c: _set(c["stagedProvenancePolicy"], "sha256", "0" * 64)),
        ("staged-generator-hash", lambda c: _set(c["stagedProvenancePolicy"], "generatorSha256", "0" * 64)),
        ("staged-policy-receipt", lambda c: _set(c["stagedProvenancePolicy"], "passingReceiptAccepted", True)),
        ("staged-policy-closed", lambda c: _set(c["stagedProvenancePolicy"], "blockerClosed", True)),
        ("candidate-id", lambda c: _set(c["currentShippingCandidateEvidence"], "candidateId", "invented")),
        ("candidate-archive-manifest", lambda c: _set(c["currentShippingCandidateEvidence"], "archiveCanonicalManifestSha256", "0" * 64)),
        ("candidate-exe-hash", lambda c: _set(c["currentShippingCandidateEvidence"]["binaryReceipt"], "executableSha256", "0" * 64)),
        ("candidate-build-hash", lambda c: _set(c["currentShippingCandidateEvidence"]["buildReceipt"], "sha256", "0" * 64)),
        ("candidate-plugin-schema", lambda c: _set(c["currentShippingCandidateEvidence"]["pluginCapabilityReceipt"], "schemaVersion", 2)),
        ("candidate-plugin-gap", lambda c: _set(c["currentShippingCandidateEvidence"]["pluginCapabilityReceipt"], "mandatoryDependencyGapCount", 1)),
        ("candidate-content-gameplay", lambda c: _set(c["currentShippingCandidateEvidence"]["contentReceipt"], "freshInstallGameplayAcceptancePerformed", True)),
        ("candidate-thirdparty-legal", lambda c: _set(c["currentShippingCandidateEvidence"]["thirdPartyNoticeReceipt"], "legalApproval", True)),
        ("candidate-provenance-count", lambda c: _set(c["currentShippingCandidateEvidence"]["provenanceDraftAudit"], "unclassifiedIdentityCount", 7157)),
        ("candidate-provenance-record-count", lambda c: _set(c["currentShippingCandidateEvidence"]["provenanceClassificationDraft"], "recordCount", 7157)),
        ("candidate-provenance-reasons", lambda c: c["currentShippingCandidateEvidence"]["provenanceDraftAudit"]["reasonCodes"].pop()),
        ("candidate-provenance-role", lambda c: _set(c["currentShippingCandidateEvidence"]["provenanceDraftAudit"], "evidenceRole", "CURRENT")),
        ("candidate-fresh-unknown", lambda c: _set(c["currentShippingCandidateEvidence"]["freshUserDirReceipt"], "unknownFileCount", 1)),
        ("candidate-fresh-gameplay", lambda c: _set(c["currentShippingCandidateEvidence"]["freshUserDirReceipt"], "gameplayAcceptanceClaimed", True)),
        ("candidate-provenance-accepted", lambda c: _set(c["currentShippingCandidateEvidence"], "provenanceClassificationAccepted", True)),
        ("candidate-release-ready", lambda c: _set(c["currentShippingCandidateEvidence"], "releaseReady", True)),
        ("external-receipt-hash", lambda c: _set(c["externalTechnicalEvidence"]["receipt"], "sha256", "0" * 64)),
        ("external-manual-gameplay", lambda c: _set(c["externalTechnicalEvidence"], "manualGameplayAcceptance", True)),
        ("feature-exclusion-policy-hash", lambda c: _set(c["v05FeatureExclusion"]["policy"], "sha256", "0" * 64)),
        ("feature-exclusion-public-career", lambda c: _set(c["v05FeatureExclusion"], "publicCareerClaimAllowed", True)),
        ("equipment-receipt-hash", lambda c: _set(c["v05EquipmentTechnical"]["receipt"], "sha256", "0" * 64)),
        ("equipment-name-clearance", lambda c: _set(c["v05EquipmentTechnical"], "equipmentNameClearance", True)),
        ("measured-reference-policy-hash", lambda c: _set(c["physicsMeasuredReferencePreparation"]["policy"], "sha256", "0" * 64)),
        ("measured-reference-count", lambda c: _set(c["physicsMeasuredReferencePreparation"], "measuredThrowCount", 100)),
        ("measured-reference-owner-approval", lambda c: _set(c["physicsMeasuredReferencePreparation"], "physicsOwnerApproval", True)),
        ("environment-receipt-hash", lambda c: _set(c["session10EnvironmentTechnical"]["receipt"], "sha256", "0" * 64)),
        ("environment-plan-hash", lambda c: _set(c["session10EnvironmentTechnical"]["technicalPlan"], "sha256", "0" * 64)),
        ("environment-visual-approval", lambda c: _set(c["session10EnvironmentTechnical"], "visualQualityAccepted", True)),
        ("presentation-receipt-hash", lambda c: _set(c["session12PresentationTechnical"]["receipt"], "sha256", "0" * 64)),
        ("presentation-audio-cook-receipt-hash", lambda c: _set(c["session12PresentationTechnical"]["shippingAudioCookReceipt"], "sha256", "0" * 64)),
        ("presentation-audio-manifest-hash", lambda c: _set(c["session12PresentationTechnical"]["audioEventCoverageManifest"], "sha256", "0" * 64)),
        ("presentation-shipping-cook", lambda c: _set(c["session12PresentationTechnical"], "shippingAudioCookProven", False)),
        ("presentation-audible-overclaim", lambda c: _set(c["session12PresentationTechnical"], "audibleRuntimePlaybackProven", True)),
        ("presentation-production-audio", lambda c: _set(c["session12PresentationTechnical"], "productionAudioAssetsPresent", True)),
        ("origin-unresolved-count", lambda c: _set(c["technicalOriginAttribution"], "unresolvedIdentityCount", 1)),
        ("origin-independent-review", lambda c: _set(c["technicalOriginAttribution"], "independentReviewComplete", True)),
        ("origin-legal-review", lambda c: _set(c["technicalOriginAttribution"], "legalReviewComplete", True)),
        ("three-hole-receipt-hash", lambda c: _set(c["threeHoleTechnicalAcceptance"]["receipt"], "sha256", "0" * 64)),
        ("three-hole-performance", lambda c: _set(c["threeHoleTechnicalAcceptance"], "performanceAcceptance", True)),
        ("baked-pcg-policy-hash", lambda c: _set(c["bakedPcgSourceAuthority"], "policySha256", "0" * 64)),
        ("baked-pcg-prebuild-hash", lambda c: _set(c["bakedPcgSourceAuthority"], "sourceClosurePreBuildReceiptSha256", "0" * 64)),
        ("baked-pcg-candidate-hash", lambda c: _set(c["bakedPcgSourceAuthority"], "candidateReceiptSha256", "0" * 64)),
        ("baked-pcg-source-rejected", lambda c: _set(c["bakedPcgSourceAuthority"], "sourceAuthorityAccepted", False)),
        ("baked-pcg-shipping-accepted", lambda c: _set(c["bakedPcgSourceAuthority"], "shippingPackageEvidenceAccepted", True)),
        ("baked-pcg-blocker-closed", lambda c: _set(c["bakedPcgSourceAuthority"], "blockerClosed", True)),
        ("baked-pcg-separation-policy-hash", lambda c: _set(c["bakedPcgRuntimeClosure"]["shippingSeparationPolicy"], "sha256", "0" * 64)),
        ("baked-pcg-separation-validator-hash", lambda c: _set(c["bakedPcgRuntimeClosure"]["shippingSeparationValidator"], "sha256", "0" * 64)),
        ("baked-pcg-separation-receipt-hash", lambda c: _set(c["bakedPcgRuntimeClosure"]["shippingPcgSeparationReceipt"], "sha256", "0" * 64)),
        ("baked-pcg-shipping-absence", lambda c: _set(c["bakedPcgRuntimeClosure"], "freshShippingCookAbsenceProven", False)),
        ("baked-pcg-legacy-not-retired", lambda c: _set(c["bakedPcgRuntimeClosure"], "legacyCandidateRuntimeClosureRetired", False)),
        ("baked-pcg-performance-overclaim", lambda c: _set(c["bakedPcgRuntimeClosure"], "performanceAcceptance", True)),
        ("baked-pcg-runtime-release-ready", lambda c: _set(c["bakedPcgRuntimeClosure"], "releaseReady", True)),
        ("production-motion-policy-hash", lambda c: _set(c["productionMotionPreparation"]["policy"], "sha256", "0" * 64)),
        ("production-motion-cook-receipt-hash", lambda c: _set(c["productionMotionPreparation"]["shippingCookPresenceReceipt"], "sha256", "0" * 64)),
        ("production-motion-target-present", lambda c: _set(c["productionMotionPreparation"], "targetAssetPresentCount", 6)),
        ("production-motion-runtime-ready", lambda c: _set(c["productionMotionPreparation"], "shippingRuntimeBindingActive", False)),
        ("production-motion-cook-present", lambda c: _set(c["productionMotionPreparation"], "shippingCookPresenceAccepted", False)),
        ("production-motion-owner-overclaim", lambda c: _set(c["productionMotionPreparation"], "ownerApproval", True)),
        ("serialized-policy-hash", lambda c: _set(c["serializedAssetMigration"], "policySha256", "0" * 64)),
        ("serialized-validator-hash", lambda c: _set(c["serializedAssetMigration"], "validatorSha256", "0" * 64)),
        ("serialized-receipt-hash", lambda c: _set(c["serializedAssetMigration"], "receiptSha256", "0" * 64)),
        ("serialized-source-rejected", lambda c: _set(c["serializedAssetMigration"], "sourceAndRetainedAssetMigrationAccepted", False)),
        ("serialized-candidate-hash", lambda c: _set(c["serializedAssetMigration"], "candidateReceiptSha256", "0" * 64)),
        ("serialized-shipping-rejected", lambda c: _set(c["serializedAssetMigration"], "shippingPackageEvidenceAccepted", False)),
        ("serialized-tree-not-retained", lambda c: _set(c["serializedAssetMigration"], "legacySourceTreeRetainedPendingVerifiedQuarantine", False)),
        ("serialized-blocker-closed", lambda c: _set(c["serializedAssetMigration"], "blockerClosed", True)),
        ("framework-quarantine-run", lambda c: _set(c["characterFrameworkExternalQuarantine"], "runId", "invented")),
        ("framework-quarantine-host-paths", lambda c: _set(c["characterFrameworkExternalQuarantine"], "hostPathsRecorded", True)),
        ("framework-quarantine-source-rejected", lambda c: _set(c["characterFrameworkExternalQuarantine"], "projectSourceRemovalAccepted", False)),
        ("framework-quarantine-shipping-rejected", lambda c: _set(c["characterFrameworkExternalQuarantine"], "shippingPackageAbsenceAccepted", False)),
        ("framework-quarantine-runtime-accepted", lambda c: _set(c["characterFrameworkExternalQuarantine"], "replacementRuntimeAcceptanceAccepted", True)),
        ("framework-quarantine-closed", lambda c: _set(c["characterFrameworkExternalQuarantine"], "blockerClosed", True)),
        ("manual-review-human-approved", lambda c: _set(c["manualReleaseReview"], "humanApprovalAccepted", True)),
        ("manual-review-blanket-approved", lambda c: _set(c["manualReleaseReview"], "blanketAuthorizationAcceptedAsApproval", True)),
        ("manual-review-release-ready", lambda c: _set(c["manualReleaseReview"], "releaseReady", True)),
        ("manual-review-artifact-hash", lambda c: _set(c["manualReleaseReview"]["artifacts"][0], "sha256", "0" * 64)),
        ("strategy-missing", lambda c: c["closureStrategies"].pop()),
        ("strategy-reordered", lambda c: c["closureStrategies"].reverse()),
        ("strategy-closed", lambda c: _set(c["closureStrategies"][0], "closed", True)),
        ("strategy-state", lambda c: _set(c["closureStrategies"][0], "initialState", "ACCEPTED")),
        ("strategy-mode", lambda c: _set(c["closureStrategies"][11], "resolutionMode", "IMPLEMENT_AND_ACCEPT")),
        ("strategy-evidence", lambda c: c["closureStrategies"][0]["evidenceRequirements"].pop()),
        ("strategy-extra-key", lambda c: _set(c["closureStrategies"][0], "approval", True)),
        ("gate-missing", lambda c: c["evidenceGates"].pop()),
        ("gate-accepted", lambda c: _set(c["evidenceGates"][0], "accepted", True)),
        ("gate-state", lambda c: _set(c["evidenceGates"][0], "state", "PASS")),
        ("gate-artifact-traversal", lambda c: _set(c["evidenceGates"][0], "artifactPaths", ["../escape.json"])),
        ("resolved-blocker", lambda c: c["resolvedBySession19"].append(BLOCKERS[11])),
        ("remaining-blocker-missing", lambda c: c["remainingReleaseBlockers"].pop()),
        ("inherited-duplicate", lambda c: _set(c, "inheritedReleaseBlockers", BLOCKERS[:-1] + [BLOCKERS[-2]])),
        ("closure-count", lambda c: _set(c["closureSummary"], "remainingBlockerCount", 12)),
        ("closure-ready", lambda c: _set(c["closureSummary"], "releaseReady", True)),
        ("required-absolute", lambda c: _set(c, "requiredFiles", ["C:/escape", "Scripts/validate_dg_session19_release_scope.py"])),
        ("validator-path", lambda c: _set(c["validation"], "validatorPath", "../escape.py")),
        ("validator-release-code-bool", lambda c: _set(c["validation"], "releaseRequiredExpectedExitCode", True)),
        ("continuity-policy", lambda c: _set(c["continuity"], "policy", "MUTABLE")),
        ("continuity-source", lambda c: _set(c["continuity"], "sourceAuthority", "../escape.json")),
        ("continuity-file-missing", lambda c: c["continuity"]["frozenFiles"].pop()),
        ("continuity-file-reordered", lambda c: c["continuity"]["frozenFiles"].reverse()),
        ("continuity-path", lambda c: _set(c["continuity"]["frozenFiles"][0], "path", "../escape")),
        ("continuity-bytes-bool", lambda c: _set(c["continuity"]["frozenFiles"][0], "bytes", True)),
        ("continuity-hash", lambda c: _set(c["continuity"]["frozenFiles"][0], "sha256", "0" * 64)),
        ("continuity-lower-hash", lambda c: _set(c["continuity"]["frozenFiles"][0], "sha256", c["continuity"]["frozenFiles"][0]["sha256"].lower())),
    ]
    for name, mutate in mutations:
        count += 1
        candidate = copy.deepcopy(contract)
        mutate(candidate)
        mutation_issues, _ = validate_contract(
            candidate, root, check_files=False, authority_contract=authority,
            contract_binding=contract_binding)
        if not mutation_issues:
            failures.append(f"contract mutation survived: {name}")

    def replace_candidate(value: Any, before: str, after: str) -> Any:
        if type(value) is dict:
            return {
                key: replace_candidate(child, before, after)
                for key, child in value.items()
            }
        if type(value) is list:
            return [replace_candidate(child, before, after) for child in value]
        if type(value) is str:
            return value.replace(before, after)
        return value

    count += 1
    historical_candidate = contract["currentShippingCandidateEvidence"]["candidateId"]
    alternate_candidate = "S19_WindowsShipping_20990101T000000Z_abcdef123456"
    alternate = replace_candidate(
        copy.deepcopy(contract), historical_candidate, alternate_candidate)
    alternate["contractId"] = "session19_synthetic_selectable_candidate_contract_v3"
    alternate_contract_path = (
        "Config/DG_Session19ReleaseScopeContract.synthetic-self-test.json"
    )
    alternate["requiredFiles"] = [
        alternate_contract_path
        if path == DEFAULT_CONTRACT_PATH.as_posix() else path
        for path in alternate["requiredFiles"]
    ]
    old_three_hole_policy = "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicy.json"
    new_three_hole_policy = "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicyV2.json"
    alternate = replace_candidate(alternate, old_three_hole_policy, new_three_hole_policy)
    v2_policy_raw = (root / new_three_hole_policy).read_bytes()
    v2_policy_binding = {
        "path": new_three_hole_policy,
        "bytes": len(v2_policy_raw),
        "sha256": hashlib.sha256(v2_policy_raw).hexdigest().upper(),
    }
    alternate["threeHoleTechnicalAcceptance"]["policy"] = v2_policy_binding

    # Use a deliberately different two-identity/two-file candidate so this test
    # proves the fresh profile does not silently retain 7158/4 snapshot values.
    def loaded_candidate_document(path: str) -> Any:
        return replace_candidate(
            load_json_strict(root / path), historical_candidate, alternate_candidate,
        )

    source_documents = {
        "buildReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["buildReceipt"]["path"]
        ),
        "verificationReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["verificationReceipt"]["path"]
        ),
        "pluginCapabilityReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["pluginCapabilityReceipt"]["path"]
        ),
        "binaryReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["binaryReceipt"]["path"]
        ),
        "contentReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["contentReceipt"]["path"]
        ),
        "thirdPartyNoticeReceipt": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["thirdPartyNoticeReceipt"]["path"]
        ),
        "provenanceClassificationDraft": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["provenanceClassificationDraft"]["path"]
        ),
        "provenanceDraftAudit": loaded_candidate_document(
            contract["currentShippingCandidateEvidence"]["provenanceDraftAudit"]["path"]
        ),
        "technicalOriginPolicy": load_json_strict(
            root / contract["technicalOriginAttribution"]["policy"]["path"]
        ),
        "technicalOriginProposal": loaded_candidate_document(
            contract["technicalOriginAttribution"]["proposal"]["path"]
        ),
        "technicalOriginReceipt": loaded_candidate_document(
            contract["technicalOriginAttribution"]["receipt"]["path"]
        ),
        "threeHolePolicy": load_json_text_strict(v2_policy_raw.decode("utf-8-sig")),
        "productionMotionCookReceipt": loaded_candidate_document(
            contract["productionMotionPreparation"][
                "shippingCookPresenceReceipt"
            ]["path"]
        ),
    }
    equipment_policy_path = V05_EQUIPMENT_CANDIDATE_POLICY_PATTERN.format(
        candidateId=alternate_candidate
    )
    equipment_policy_binding = {
        "path": equipment_policy_path,
        "bytes": 1,
        "sha256": "3" * 64,
    }
    equipment_policy = load_json_strict(
        root / EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]["path"]
    )
    equipment_policy["candidateId"] = alternate_candidate
    equipment_policy["automation"]["candidateLogsNotBeforeUtc"] = (
        equipment_validator.candidate_build_utc_text(alternate_candidate)
    )
    equipment_policy["automation"]["candidateFreshnessRequiredTests"] = (
        copy.deepcopy(equipment_validator.POST_FIX_REQUIRED_TESTS)
    )
    equipment_policy["automation"]["candidateLogBindings"] = [
        {
            "fileName": f"Automation_{alternate_candidate}_A.log",
            "bytes": 1,
            "sha256": "1" * 64,
        },
        {
            "fileName": f"Automation_{alternate_candidate}_B.log",
            "bytes": 2,
            "sha256": "2" * 64,
        },
    ]
    equipment_policy_issues = equipment_validator.validate_policy(
        equipment_policy, require_candidate_bindings=True,
    )
    if equipment_policy_issues:
        failures.append(
            "synthetic v0.5 equipment candidate policy setup failed: "
            + "; ".join(equipment_policy_issues[:5])
        )
    equipment_receipt = loaded_candidate_document(
        contract["v05EquipmentTechnical"]["receipt"]["path"]
    )
    equipment_receipt["candidateId"] = alternate_candidate
    equipment_receipt["bindings"]["policy"] = {
        "artifact": "policy",
        "fileName": PurePosixPath(equipment_policy_path).name,
        "bytes": equipment_policy_binding["bytes"],
        "sha256": equipment_policy_binding["sha256"],
        "hostPathRecorded": False,
    }
    source_documents["v05EquipmentPolicy"] = equipment_policy
    source_documents["v05EquipmentPolicyBinding"] = copy.deepcopy(
        equipment_policy_binding
    )
    source_documents["v05EquipmentReceipt"] = equipment_receipt
    synthetic_verification = source_documents["verificationReceipt"]
    synthetic_content = source_documents["contentReceipt"]
    synthetic_archive_sha = synthetic_verification[
        "shippingPluginCapabilities"
    ]["finalArchiveCanonicalManifestSha256"]
    synthetic_executable = synthetic_verification["innerShippingExecutable"]
    # Candidate generation begins with a token-replaced historical contract.
    # Deliberately poison every family of non-file-binding facts so this fixture
    # proves fresh derivation does not require hand-editing snapshot values.
    stale_current = alternate["currentShippingCandidateEvidence"]
    stale_current["currentState"] = "STALE_HISTORICAL_STATE"
    stale_current["archiveRecoveryLocationToken"] = (
        f"DGTOUR_PACKAGES/{alternate_candidate}/StaleWindows"
    )
    stale_current["archiveCanonicalManifestSha256"] = "0" * 64
    stale_current["buildLog"] = {
        "path": f"Evidence/Session19/ShippingBuildCookRun-{alternate_candidate}.log",
        "bytes": 1,
        "sha256": "1" * 64,
    }
    stale_current["buildReceipt"]["state"] = "STALE_BUILD_STATE"
    stale_current["verificationReceipt"]["schemaVersion"] = 99
    stale_current["pluginCapabilityReceipt"]["finalPayloadPluginDescriptorCount"] = 999
    stale_current["binaryReceipt"]["executableSha256"] = "2" * 64
    stale_current["contentReceipt"].update({
        "archiveBytes": synthetic_content["archive"]["bytes"] + 1,
        "archiveFileCount": synthetic_content["archive"]["fileCount"] + 1,
        "auditedIdentityCount": 1,
    })
    stale_current["thirdPartyNoticeReceipt"]["mappedStagedDllIdentityCount"] = 0
    stale_current["provenanceClassificationDraft"]["recordCount"] = 999
    stale_current["provenanceDraftAudit"]["discoveredIdentityCount"] = 999
    stale_current["freshUserDirValidator"]["selfTestCaseCount"] = 1
    stale_current["freshUserDirReceipt"]["observedFileCount"] = 999
    stale_current["technicalPackageEvidenceAccepted"] = False
    stale_current["releaseReady"] = True
    production_motion_policy_path = (
        "Config/DG_Session19ProductionMotionAuthoringPolicy-"
        f"{alternate_candidate}.json"
    )
    production_motion_receipt_path = (
        "Evidence/Session19/ProductionMotionTechnicalCandidateEvidence-"
        f"{alternate_candidate}.json"
    )
    production_motion_policy_binding = {
        "path": production_motion_policy_path,
        "bytes": 1,
        "sha256": "5" * 64,
    }
    production_motion_receipt_binding = {
        "path": production_motion_receipt_path,
        "bytes": 1,
        "sha256": "4" * 64,
    }
    production_motion_policy = load_json_strict(
        root / EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]["path"]
    )
    production_motion_policy["bindings"]["candidateContentAudit"] = {
        key: alternate["currentShippingCandidateEvidence"]["contentReceipt"][key]
        for key in ("path", "bytes", "sha256")
    }
    production_policy_issues, production_observations = (
        production_motion_validator.validate_policy(
            production_motion_policy, root, check_files=False,
            expected_candidate_id=alternate_candidate,
        )
    )
    if production_policy_issues:
        failures.append(
            "synthetic production-motion candidate policy setup failed: "
            + "; ".join(production_policy_issues[:5])
        )
    source_documents["productionMotionPolicy"] = production_motion_policy
    source_documents["productionMotionReceipt"] = {
        "schema": (
            "DiscGolfTour.Session19ProductionMotionTechnicalCandidateEvidence.v2"
        ),
        "schemaVersion": 2,
        "session": 19,
        "milestone": "v0.5",
        "authority": production_motion_policy["authority"],
        "state": (
            "PASS_TECHNICAL_PROCEDURAL_CANDIDATES_AUTHORED_RUNTIME_AND_COOK_"
            "BOUND_HUMAN_REVIEW_AND_FRESH_CANDIDATE_PENDING"
        ),
        "policy": copy.deepcopy(production_motion_policy_binding),
        "bindings": copy.deepcopy(production_motion_policy["bindings"]),
        "observations": production_observations,
        "residualBlockers": copy.deepcopy(
            production_motion_policy["residualBlockers"]
        ),
        "claimBoundary": copy.deepcopy(
            production_motion_policy["claimBoundary"]
        ),
        "issues": [],
    }
    alternate["productionMotionPreparation"]["policy"] = copy.deepcopy(
        production_motion_policy_binding
    )
    alternate["productionMotionPreparation"]["evidence"] = copy.deepcopy(
        production_motion_receipt_binding
    )
    alternate["requiredFiles"].extend([
        production_motion_policy_path,
        production_motion_receipt_path,
    ])
    source_documents["provenanceClassificationDraft"]["records"] = (
        source_documents["provenanceClassificationDraft"]["records"][:2]
    )
    audit_classification = source_documents["provenanceDraftAudit"]["classification"]
    audit_classification.update({
        "discoveredIdentityCount": 2,
        "classifiedIdentityCount": 0,
        "unclassifiedIdentityCount": 2,
    })
    source_documents["technicalOriginPolicy"]["inputs"]["expectedAuditedIdentityCount"] = 2
    source_documents["technicalOriginProposal"]["records"] = (
        source_documents["technicalOriginProposal"]["records"][:2]
    )
    source_documents["technicalOriginProposal"]["summary"]["sourceRecordCount"] = 2
    origin_records = source_documents["technicalOriginReceipt"]["records"][:2]
    source_documents["technicalOriginReceipt"]["records"] = origin_records
    attributed_value = "AUTHORITATIVELY_ATTRIBUTED_TECHNICAL_ORIGIN_ONLY"
    unresolved_anon_value = "UNRESOLVED_ANONYMOUS_IDENTITY"
    unresolved_path_value = "UNRESOLVED_NO_AUTHORITATIVE_TECHNICAL_ORIGIN"
    source_documents["technicalOriginReceipt"]["summary"] = {
        "auditedIdentityCount": len(origin_records),
        "authoritativelyAttributedTechnicalOriginCount": sum(
            item["technicalDisposition"] == attributed_value for item in origin_records
        ),
        "unresolvedIdentityCount": sum(
            item["technicalDisposition"] in {unresolved_anon_value, unresolved_path_value}
            for item in origin_records
        ),
        "resolvedAnonymousChunkTechnicalIdentityCount": sum(
            item["scope"] == "CONTAINER_ANONYMOUS_CHUNK"
            and item["technicalDisposition"] == attributed_value
            for item in origin_records
        ),
        "unresolvedAnonymousChunkCount": sum(
            item["technicalDisposition"] == unresolved_anon_value for item in origin_records
        ),
        "unresolvedPathIdentityCount": sum(
            item["technicalDisposition"] == unresolved_path_value for item in origin_records
        ),
        "scopeCounts": dict(sorted(Counter(item["scope"] for item in origin_records).items())),
        "technicalDispositionCounts": dict(sorted(Counter(
            item["technicalDisposition"] for item in origin_records
        ).items())),
        "originCategoryCounts": dict(sorted(Counter(
            item["originCategory"] for item in origin_records
            if item["originCategory"] is not None
        ).items())),
        "licenseSourceCategoryCounts": dict(sorted(Counter(
            item["licenseSourceCategory"] for item in origin_records
        ).items())),
        "legalReviewedRecordCount": 0,
        "shippingApprovedRecordCount": 0,
        "distributionApprovedRecordCount": 0,
    }

    import validate_dg_session19_fresh_userdir as fresh_validator
    import validate_dg_session19_three_hole_technical_acceptance as three_hole_validator

    userdir_token = "11111111-2222-4333-8444-555555555555"
    capture_nonce = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
    round_id = "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff"
    journal_sha = "A" * 64
    launch_sha = "B" * 64
    manifest_sha = "C" * 64
    executable_sha = "D" * 64
    archive_sha = "E" * 64
    fresh_receipt = fresh_validator._base_receipt(alternate_candidate, userdir_token)
    fresh_receipt["policy"]["runtimeJournalAllowance"] = (
        "EXACT_CANDIDATE_BOUND_EXTERNAL_CAPTURE_ONLY"
    )
    fresh_receipt["observedFiles"] = [
        {
            "relativePath": fresh_validator.REQUIRED_PROFILE,
            "bytes": 4,
            "sha256": "1" * 64,
            "classification": "production_profile",
        },
        {
            "relativePath": fresh_validator.RUNTIME_JOURNAL_RELATIVE,
            "bytes": 1024,
            "sha256": journal_sha.lower(),
            "classification": "runtime_checkpoint_journal_candidate_bound",
        },
    ]
    fresh_receipt["observedFiles"].sort(key=lambda item: item["relativePath"].casefold())
    fresh_receipt["counts"] = {
        "observedFiles": 2,
        "approvedFiles": 2,
        "unknownFiles": 0,
        "forbiddenArtifacts": 0,
    }
    fresh_receipt["runtimeJournalBinding"] = {
        "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
        "userDirRelativePath": fresh_validator.RUNTIME_JOURNAL_RELATIVE,
        "bytes": 1024,
        "sha256": journal_sha,
        "captureNonce": capture_nonce,
        "roundId": round_id,
        "launchRecordSha256": launch_sha,
        "technicalEvidenceManifestSha256": manifest_sha,
        "executableSha256": executable_sha,
        "archiveManifestSha256": archive_sha,
        "validationState": fresh_validator.RUNTIME_JOURNAL_PASS,
    }
    fresh_receipt["state"] = "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
    source_documents["freshUserDirReceipt"] = fresh_receipt

    three_receipt_path = alternate["threeHoleTechnicalAcceptance"]["receipt"]["path"]
    source_documents["threeHoleReceipt"] = {
        "schema": three_hole_validator.RECEIPT_SCHEMA,
        "schemaVersion": 2,
        "session": 19,
        "candidateId": alternate_candidate,
        "verifiedUtc": "2099-01-01T00:00:00Z",
        "state": three_hole_validator.RUNTIME_JOURNAL_STATE,
        "policy": {**v2_policy_binding, "policyId": source_documents["threeHolePolicy"]["policyId"]},
        "candidate": {},
        "projectEvidenceBindings": [],
        "externalEvidence": {},
        "freshUserDir": {
            "token": userdir_token,
            "hostPathRecorded": False,
            "emptyBeforeLaunch": True,
            "externalBoundaryValidated": True,
            "postRunAllowlistPass": True,
            "observedFileCount": 2,
            # The fresh-UserDir receipt uses lowercase canonical JSON while the
            # three-hole receipt emits normalized uppercase digests.  This
            # baseline proves the adapter treats case-only SHA-256 differences
            # as equivalent after validating both values as exact 64-hex.
            "candidateUserDirBindingSha256": fresh_receipt[
                "candidateUserDirBindingSha256"
            ].upper(),
        },
        "process": {"exitCode": 0, "timedOut": False, "cleanExit": True},
        "artifactIntegrity": {},
        "checkpointChain": None,
        "runtimeJournal": {
            "path": fresh_validator.RUNTIME_JOURNAL_RELATIVE,
            "hostPathRecorded": False,
            "userDirRecoveryLocationToken": (
                f"DGTOUR_EXTERNAL_USERDIR/{userdir_token}/"
                + fresh_validator.RUNTIME_JOURNAL_RELATIVE
            ),
            "bytes": 1024,
            "sha256": journal_sha,
            "schema": three_hole_validator.RUNTIME_JOURNAL_SCHEMA,
            "candidateId": alternate_candidate,
            "userDirToken": userdir_token,
            "executableSha256": executable_sha,
            "archiveManifestSha256": archive_sha,
            "launchRecordSha256": launch_sha,
            "technicalEvidenceManifestSha256": manifest_sha,
            "captureNonce": capture_nonce,
            "roundId": round_id,
            "eventCount": 12,
            "firstSequence": 1,
            "lastSequence": 12,
            "startedUtc": "2099-01-01T00:00:00Z",
            "modifiedUtc": "2099-01-01T00:00:01Z",
            "canonicalJsonLinesValidated": True,
            "freshUserDirReceiptBindingValidated": True,
            "launchRecordBindingValidated": True,
            "technicalManifestBindingValidated": True,
            "captureWindowValidated": True,
            "runtimeOperationalBindingValidated": True,
        },
        "technicalAcceptance": True,
        "continuityClaims": {
            "checkpointChainSupplied": False,
            "checkpointChainStructuralIntegrityValidated": False,
            "derivedCheckpointMayPromote": False,
            "runtimeJournalSupplied": True,
            "runtimeJournalCanonicalBytesValidated": True,
            "runtimeOperationalBindingValidated": True,
            "captureWindowBindingPresent": True,
            "singleRoundProven": True,
            "freshRoundEntryToFinalScorecardSequencePass": True,
            "eachHoleTeeAndRecoveredLieObserved": True,
            "threeHoleCompletionObserved": True,
        },
        "scoreClaims": {
            "finalScorecardScoreProven": True,
            "completedHolesProven": True,
            "totalHolesProven": True,
            "holeRowsProven": True,
            "roundId": round_id,
            "finalScore": {
                "completedHoles": 3,
                "totalHoles": 3,
                "parTotal": 11,
                "totalStrokes": 20,
                "totalPenalties": 3,
                "holeRows": [
                    {"holeNumber": 1, "par": 3, "strokes": 4, "penalties": 1},
                    {"holeNumber": 2, "par": 4, "strokes": 10, "penalties": 1},
                    {"holeNumber": 3, "par": 4, "strokes": 6, "penalties": 1},
                ],
            },
        },
        "claimBoundary": copy.deepcopy(source_documents["threeHolePolicy"]["claimBoundary"]),
        "blockerClosed": False,
        "releaseReady": False,
        "remainingEvidence": [],
        "receiptPath": three_receipt_path,
    }
    import validate_dg_session10_environment_shipping as session10_validator
    import validate_dg_session19_session12_presentation_technical as session12_validator

    session10_policy = loaded_candidate_document(
        contract["session10EnvironmentTechnical"]["policy"]["path"]
    )
    session10_policy["schema"] = session10_validator.TRUSTED_POLICY_SCHEMA
    session10_policy["schemaVersion"] = 4
    session10_policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        session10_validator.TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    session10_policy["bindings"].setdefault("freshUserDirValidation", {
        "path": f"Evidence/Session19/FreshUserDir-{alternate_candidate}.json",
        "bytes": 1,
        "sha256": "9" * 64,
    })
    session10_policy_path = (
        "Config/DG_Session10EnvironmentShippingTechnicalPolicy-"
        f"{alternate_candidate}.json"
    )
    session10_policy_binding = {
        "path": session10_policy_path,
        "bytes": 1,
        "sha256": "8" * 64,
    }
    runtime_operational = {
        "schema": three_hole_validator.RUNTIME_JOURNAL_SCHEMA,
        "userDirRelativePath": fresh_validator.RUNTIME_JOURNAL_RELATIVE,
        "bytes": 1024,
        "sha256": journal_sha,
        "captureNonce": capture_nonce,
        "roundId": round_id,
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
        "modifiedWithinRunWindow": True,
        "validationState": fresh_validator.RUNTIME_JOURNAL_PASS,
        "independentValidationState": (
            "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
        ),
        "launchRecordSha256": launch_sha,
        "technicalEvidenceManifestSha256": manifest_sha,
        "executableSha256": executable_sha,
        "archiveManifestSha256": archive_sha,
        "trustBoundary": copy.deepcopy(session10_validator.RUNTIME_TRUST_BOUNDARY),
    }
    source_documents["session10Policy"] = session10_policy
    source_documents["session10Receipt"] = {
        "schema": "DiscGolfTour.Session10EnvironmentShippingTechnicalEvidence.v4",
        "schemaVersion": 4,
        "session": 10,
        "milestone": "v0.5",
        "candidateId": alternate_candidate,
        "authority": session10_policy["authority"],
        "state": (
            "PASS_BOUNDED_ENVIRONMENT_16_CATEGORY_CONTRACT_SLOT_PROXY_"
            "QUARANTINE_AND_SCREENSHOT_TECHNICAL_EVIDENCE"
        ),
        "policy": copy.deepcopy(session10_policy_binding),
        "bindings": copy.deepcopy(session10_policy["bindings"]),
        "observations": {
            "externalRuntimeEvidence": {
                "runtimeCheckpointJournal": copy.deepcopy(runtime_operational),
            },
        },
        "claimBoundary": copy.deepcopy(session10_policy["claimBoundary"]),
        "issues": [],
    }

    session12_policy = loaded_candidate_document(
        contract["session12PresentationTechnical"]["policy"]["path"]
    )
    session12_policy["schema"] = session12_validator.TRUSTED_POLICY_SCHEMA
    session12_policy["schemaVersion"] = 3
    session12_policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        session12_validator.TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    session12_policy_path = (
        "Config/DG_Session19Session12PresentationTechnicalPolicy-"
        f"{alternate_candidate}.json"
    )
    session12_policy_binding = {
        "path": session12_policy_path,
        "bytes": 1,
        "sha256": "7" * 64,
    }
    source_documents["session12Policy"] = session12_policy
    source_documents["session12Receipt"] = {
        "schema": session12_validator.TRUSTED_RECEIPT_SCHEMA,
        "schemaVersion": 3,
        "session": 19,
        "candidateId": alternate_candidate,
        "generatedUtc": "2099-01-01T00:00:00Z",
        "state": session12_validator.STATE,
        "releaseState": session12_validator.RELEASE_STATE,
        "policy": copy.deepcopy(session12_policy_binding),
        "candidateArchive": {},
        "session12ContractValidator": {},
        "projectEvidence": {},
        "freshInstallEvidence": {
            "runtimeCheckpointJournal": copy.deepcopy(runtime_operational),
        },
        "technicalClaims": copy.deepcopy(session12_policy["technicalClaims"]),
        "unsupportedClaims": copy.deepcopy(session12_policy["unsupportedClaims"]),
        "truthBoundary": {},
    }
    import validate_dg_session19_serialized_asset_migration as serialized_validator

    serialized_policy_path = (
        "Config/DG_Session19SerializedAssetMigrationCandidatePolicy-"
        f"{alternate_candidate}.json"
    )
    serialized_policy_binding = {
        "path": serialized_policy_path,
        "bytes": 1,
        "sha256": "6" * 64,
    }
    serialized_policy = {
        "schema": serialized_validator.CANDIDATE_POLICY_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": alternate_candidate,
        "policyId": (
            "session19_serialized_asset_migration_candidate_"
            + alternate_candidate
        ),
        "authority": serialized_validator.CANDIDATE_POLICY_AUTHORITY,
        "state": "CANDIDATE_INPUTS_BOUND_SUPPLEMENT_RECEIPT_REQUIRED",
        "releaseReady": False,
        "shippingEvidenceId": (
            "session19_retained_serialized_asset_migration_shipping_"
            f"non_distribution_{alternate_candidate}"
        ),
        "shippingReceiptPath": alternate["serializedAssetMigration"][
            "candidateReceiptPath"
        ],
        "bindings": serialized_validator._candidate_binding_paths(alternate_candidate),
        "archive": {
            **copy.deepcopy(serialized_validator.SHIPPING_ARCHIVE),
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{alternate_candidate}/Windows",
        },
        "executable": {
            "relativePath": serialized_validator.SHIPPING_EXE_RELATIVE.as_posix(),
            "bytes": serialized_validator.SHIPPING_EXE_BYTES,
            "sha256": serialized_validator.SHIPPING_EXE_SHA256,
        },
        "externalEvidence": {
            "userDirToken": userdir_token,
            "runDirectoryLeaf": f"{alternate_candidate}_{userdir_token}",
            "manifestSha256": manifest_sha,
            "externalTechnicalValidationState": (
                "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
            ),
            "freshUserDirValidationState": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        },
        "runtimeScreenshots": copy.deepcopy(serialized_validator.SHIPPING_SCREENSHOTS),
    }
    serialized_policy_issues, serialized_authority = (
        serialized_validator.validate_candidate_policy(
            serialized_policy, root,
            policy_binding=serialized_policy_binding,
            check_files=False,
        )
    )
    if serialized_policy_issues or serialized_authority is None:
        failures.append(
            "synthetic serialized candidate policy setup failed: "
            + "; ".join(serialized_policy_issues[:5])
        )
    else:
        serialized_receipt = loaded_candidate_document(
            contract["serializedAssetMigration"]["candidateReceiptPath"]
        )
        serialized_receipt["schema"] = serialized_authority["receiptSchema"]
        serialized_receipt["schemaVersion"] = serialized_authority[
            "receiptSchemaVersion"
        ]
        serialized_receipt["candidateId"] = alternate_candidate
        serialized_receipt["candidatePolicy"] = copy.deepcopy(
            serialized_policy_binding
        )
        serialized_receipt["evidenceId"] = serialized_authority["evidenceId"]
        serialized_receipt["archive"] = copy.deepcopy(
            serialized_authority["archive"]
        )
        serialized_receipt["runtimeScreenshots"] = copy.deepcopy(
            serialized_authority["runtimeScreenshots"]
        )
        for key, path in serialized_authority["bindingPaths"].items():
            serialized_receipt["bindings"][key]["path"] = path
        source_documents["serializedCandidatePolicy"] = serialized_policy
        source_documents["serializedCandidateReceipt"] = serialized_receipt
        alternate["requiredFiles"].append(serialized_policy_path)
    alternate = _replace_string_tokens(alternate, {
        EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]["path"]:
            equipment_policy_path,
        EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL["policy"]["path"]:
            session10_policy_path,
        EXPECTED_SESSION12_PRESENTATION_TECHNICAL["policy"]["path"]:
            session12_policy_path,
    })
    profile_build_issues: list[str] = []
    alternate_expectations = derive_fresh_candidate_expectations(
        alternate, root, profile_build_issues, source_documents,
    )
    if alternate_expectations is None or profile_build_issues:
        failures.append(
            "alternate synthetic profile construction failed: "
            + "; ".join(profile_build_issues[:5])
        )
    else:
        for section_name in (
                "stagedProvenancePolicy", "currentShippingCandidateEvidence",
                "v05EquipmentTechnical",
                "session10EnvironmentTechnical", "session12PresentationTechnical",
                "technicalOriginAttribution", "threeHoleTechnicalAcceptance",
                "productionMotionPreparation",
                "serializedAssetMigration"):
            alternate[section_name] = alternate_expectations[section_name]
    alternate_bytes = (
        json.dumps(alternate, indent=2, ensure_ascii=False) + "\n"
    ).encode("utf-8")
    alternate_binding = {
        "path": alternate_contract_path,
        "bytes": len(alternate_bytes),
        "sha256": hashlib.sha256(alternate_bytes).hexdigest().upper(),
    }
    alternate_issues, alternate_summary = validate_contract(
        alternate, root, check_files=False, authority_contract=alternate,
        contract_binding=alternate_binding,
        candidate_profile_documents=source_documents,
    )
    if alternate_issues:
        failures.append(
            "alternate synthetic candidate contract failed: "
            + "; ".join(alternate_issues[:5])
        )
    elif (
            alternate_summary.get("candidateId") != alternate_candidate
            or alternate_summary.get("contract") != alternate_binding):
        failures.append("alternate synthetic contract result lost its selected bindings")
    elif (
            alternate_summary.get("auditedIdentityCount") != 2
            or alternate_summary.get("freshUserDirObservedFileCount") != 2
            or alternate_summary.get("runtimeOperationalBindingValidated") is not True
            or alternate_summary.get("contractProfile")
            != FRESH_CANDIDATE_CONTRACT_PROFILE):
        failures.append("alternate synthetic contract retained historical candidate facts")
    elif (
            alternate["v05EquipmentTechnical"]["policy"]
            != equipment_policy_binding
            or equipment_policy_path not in alternate["requiredFiles"]
            or EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]["path"]
            in alternate["requiredFiles"]):
        failures.append(
            "alternate synthetic contract lost its candidate equipment policy authority"
        )

    count += 1
    if alternate_expectations is not None:
        derived_current = alternate["currentShippingCandidateEvidence"]
        expected_derived_facts = {
            "currentState": (
                "TECHNICAL_CANDIDATE_EVIDENCE_PASS_"
                "PROVENANCE_LEGAL_GAMEPLAY_AND_OWNER_REVIEW_BLOCKED"
            ),
            "archiveRecoveryLocationToken": synthetic_content["archive"][
                "recoveryLocationToken"
            ],
            "archiveCanonicalManifestSha256": synthetic_archive_sha,
            "buildLog": _binding_projection(
                source_documents["buildReceipt"]["buildLog"]
            ),
            "buildState": source_documents["buildReceipt"]["state"],
            "verificationSchemaVersion": source_documents[
                "verificationReceipt"
            ]["schemaVersion"],
            "pluginDescriptorCount": source_documents[
                "pluginCapabilityReceipt"
            ]["finalPayload"]["pluginDescriptorCount"],
            "executableSha256": synthetic_executable["sha256"].upper(),
            "archiveBytes": synthetic_content["archive"]["bytes"],
            "archiveFileCount": synthetic_content["archive"]["fileCount"],
            "auditedIdentityCount": synthetic_content["inventory"][
                "auditedIdentityCount"
            ],
            "mappedStagedDllIdentityCount": source_documents[
                "thirdPartyNoticeReceipt"
            ]["coverage"]["mappedStagedDllIdentityCount"],
            "draftRecordCount": 2,
            "auditDiscoveredIdentityCount": 2,
            "freshObservedFileCount": 2,
            "technicalPackageEvidenceAccepted": True,
            "releaseReady": False,
        }
        observed_derived_facts = {
            "currentState": derived_current["currentState"],
            "archiveRecoveryLocationToken": derived_current[
                "archiveRecoveryLocationToken"
            ],
            "archiveCanonicalManifestSha256": derived_current[
                "archiveCanonicalManifestSha256"
            ],
            "buildLog": derived_current["buildLog"],
            "buildState": derived_current["buildReceipt"]["state"],
            "verificationSchemaVersion": derived_current[
                "verificationReceipt"
            ]["schemaVersion"],
            "pluginDescriptorCount": derived_current[
                "pluginCapabilityReceipt"
            ]["finalPayloadPluginDescriptorCount"],
            "executableSha256": derived_current["binaryReceipt"][
                "executableSha256"
            ],
            "archiveBytes": derived_current["contentReceipt"]["archiveBytes"],
            "archiveFileCount": derived_current["contentReceipt"][
                "archiveFileCount"
            ],
            "auditedIdentityCount": derived_current["contentReceipt"][
                "auditedIdentityCount"
            ],
            "mappedStagedDllIdentityCount": derived_current[
                "thirdPartyNoticeReceipt"
            ]["mappedStagedDllIdentityCount"],
            "draftRecordCount": derived_current[
                "provenanceClassificationDraft"
            ]["recordCount"],
            "auditDiscoveredIdentityCount": derived_current[
                "provenanceDraftAudit"
            ]["discoveredIdentityCount"],
            "freshObservedFileCount": derived_current["freshUserDirReceipt"][
                "observedFileCount"
            ],
            "technicalPackageEvidenceAccepted": derived_current[
                "technicalPackageEvidenceAccepted"
            ],
            "releaseReady": derived_current["releaseReady"],
        }
        if observed_derived_facts != expected_derived_facts:
            failures.append(
                "fresh current-candidate derivation retained stale scaffold facts: "
                f"expected {expected_derived_facts!r}, got {observed_derived_facts!r}"
            )

    digest_comparison_cases = (
        ("case-only-equivalence", "a" * 64, "A" * 64, False),
        ("real-mismatch", "A" * 64, "B" * 64, True),
        ("actual-nonhex", "G" * 64, "A" * 64, True),
        ("expected-nonhex", "A" * 64, "g" * 64, True),
        ("short-digest", "A" * 63, "A" * 64, True),
    )
    for name, actual, expected, should_fail in digest_comparison_cases:
        count += 1
        digest_issues: list[str] = []
        _require_matching_sha256_digest(
            actual, expected, f"self-test digest {name}", digest_issues,
        )
        if bool(digest_issues) is not should_fail:
            failures.append(
                f"SHA-256 comparison self-test differed: {name} "
                f"issues={digest_issues!r}"
            )

    identity_fixture = {
        "candidateId": alternate_candidate,
        "archiveCanonicalManifestSha256": "A" * 64,
        "executableSha256": "B" * 64,
        "executableBytes": 123,
        "archiveBytes": 456,
        "archiveFileCount": 7,
    }
    identity_mutations: tuple[tuple[str, Callable[[dict[str, Any]], None]], ...] = (
        ("candidate", lambda d: _set(d, "candidateId", "S19_WindowsShipping_Wrong")),
        ("archive-sha", lambda d: _set(d, "archiveCanonicalManifestSha256", "C" * 64)),
        ("executable-sha", lambda d: _set(d, "executableSha256", "D" * 64)),
        ("executable-bytes", lambda d: _set(d, "executableBytes", 124)),
        ("archive-bytes", lambda d: _set(d, "archiveBytes", 457)),
        ("archive-count", lambda d: _set(d, "archiveFileCount", 8)),
    )
    count += 1
    baseline_identity_issues: list[str] = []
    _validate_consumed_candidate_identity(
        identity_fixture, "identity fixture", alternate_candidate,
        "A" * 64, 123, "B" * 64, 456, 7, baseline_identity_issues,
    )
    if baseline_identity_issues:
        failures.append("candidate identity baseline failed: " + "; ".join(baseline_identity_issues))
    for name, mutate in identity_mutations:
        count += 1
        mutated_identity = copy.deepcopy(identity_fixture)
        mutate(mutated_identity)
        identity_issues: list[str] = []
        _validate_consumed_candidate_identity(
            mutated_identity, "identity fixture", alternate_candidate,
            "A" * 64, 123, "B" * 64, 456, 7, identity_issues,
        )
        if not identity_issues:
            failures.append(f"candidate identity mutation survived: {name}")

    with tempfile.TemporaryDirectory(
            prefix="dg-s19-release-scope-serialized-bindings-") as folder:
        binding_root = Path(folder)
        binding_paths = {
            "first": "Evidence/first.json",
            "second": "Evidence/second.bin",
        }
        binding_receipt = {"bindings": {}}
        for key, relative in binding_paths.items():
            path = binding_root.joinpath(*PurePosixPath(relative).parts)
            path.parent.mkdir(parents=True, exist_ok=True)
            payload = (key + "\n").encode("utf-8")
            path.write_bytes(payload)
            binding_receipt["bindings"][key] = {
                "path": relative,
                "bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest().upper(),
            }
        binding_authority = {"bindingPaths": binding_paths}
        count += 1
        baseline_binding_issues = _validate_serialized_candidate_project_bindings(
            binding_root, binding_receipt, binding_authority,
        )
        if baseline_binding_issues:
            failures.append(
                "serialized project-binding baseline failed: "
                + "; ".join(baseline_binding_issues)
            )
        binding_mutations: tuple[
            tuple[str, Callable[[dict[str, Any]], None]], ...
        ] = (
            ("receipt-path", lambda d: _set(
                d["bindings"]["first"], "path", "Evidence/second.bin")),
            ("receipt-bytes", lambda d: _set(
                d["bindings"]["first"], "bytes", 999)),
            ("receipt-valid-but-wrong-hash", lambda d: _set(
                d["bindings"]["first"], "sha256", "F" * 64)),
            ("receipt-nonhex-hash", lambda d: _set(
                d["bindings"]["first"], "sha256", "G" * 64)),
            ("receipt-extra-binding", lambda d: _set(
                d["bindings"], "extra", copy.deepcopy(d["bindings"]["first"]))),
            ("receipt-missing-binding", lambda d: d["bindings"].pop("second")),
        )
        for name, mutate in binding_mutations:
            count += 1
            mutated_binding_receipt = copy.deepcopy(binding_receipt)
            mutate(mutated_binding_receipt)
            binding_issues = _validate_serialized_candidate_project_bindings(
                binding_root, mutated_binding_receipt, binding_authority,
            )
            if not binding_issues:
                failures.append(
                    f"serialized project-binding mutation survived: {name}"
                )
        count += 1
        drift_path = binding_root.joinpath(
            *PurePosixPath(binding_paths["second"]).parts
        )
        original_drift_bytes = drift_path.read_bytes()
        drift_path.write_bytes(original_drift_bytes + b"drift")
        drift_issues = _validate_serialized_candidate_project_bindings(
            binding_root, binding_receipt, binding_authority,
        )
        if not drift_issues:
            failures.append("serialized project-bound file drift survived")

    candidate_profile_mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("fresh-observed-frozen", lambda c: _set(
            c["currentShippingCandidateEvidence"]["freshUserDirReceipt"],
            "observedFileCount", 4)),
        ("fresh-self-test-frozen", lambda c: _set(
            c["currentShippingCandidateEvidence"]["freshUserDirValidator"],
            "selfTestCaseCount", 18)),
        ("origin-audited-frozen", lambda c: _set(
            c["technicalOriginAttribution"], "auditedIdentityCount", 7158)),
        ("origin-state-frozen", lambda c: _set(
            c["stagedProvenancePolicy"], "currentState",
            EXPECTED_STAGED_PROVENANCE_BINDING["currentState"])),
        ("three-hole-runtime-binding", lambda c: _set(
            c["threeHoleTechnicalAcceptance"],
            "runtimeOperationalBindingValidated", False)),
        ("three-hole-crypto-overclaim", lambda c: _set(
            c["threeHoleTechnicalAcceptance"],
            "cryptographicProcessAttestation", True)),
        ("equipment-historical-policy", lambda c: _set(
            c["v05EquipmentTechnical"], "policy",
            copy.deepcopy(EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]))),
        ("equipment-policy-required-file-missing", lambda c: c[
            "requiredFiles"
        ].remove(equipment_policy_path)),
        ("session10-historical-policy", lambda c: _set(
            c["session10EnvironmentTechnical"], "policy",
            copy.deepcopy(EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL["policy"]))),
        ("session10-runtime-binding", lambda c: _set(
            c["session10EnvironmentTechnical"],
            "runtimeOperationalBindingValidated", False)),
        ("session12-historical-policy", lambda c: _set(
            c["session12PresentationTechnical"], "policy",
            copy.deepcopy(EXPECTED_SESSION12_PRESENTATION_TECHNICAL["policy"]))),
        ("session12-tamperproof-overclaim", lambda c: _set(
            c["session12PresentationTechnical"], "tamperProofEvidence", True)),
        ("production-motion-historical-policy", lambda c: _set(
            c["productionMotionPreparation"], "policy",
            copy.deepcopy(EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]))),
        ("production-motion-historical-evidence", lambda c: _set(
            c["productionMotionPreparation"], "evidence",
            copy.deepcopy(EXPECTED_PRODUCTION_MOTION_PREPARATION["evidence"]))),
        ("serialized-candidate-policy-missing", lambda c: c[
            "serializedAssetMigration"
        ].pop("candidatePolicyPath", None)),
        ("serialized-candidate-policy-historical", lambda c: _set(
            c["serializedAssetMigration"], "candidatePolicyPath",
            c["serializedAssetMigration"]["policyPath"])),
        ("serialized-candidate-policy-hash", lambda c: _set(
            c["serializedAssetMigration"], "candidatePolicySha256", "0" * 64)),
    ]
    for name, mutate in candidate_profile_mutations:
        count += 1
        mutated = copy.deepcopy(alternate)
        mutate(mutated)
        mutation_issues, _ = validate_contract(
            mutated, root, check_files=False, authority_contract=alternate,
            contract_binding=alternate_binding,
            candidate_profile_documents=source_documents,
        )
        if not mutation_issues:
            failures.append(f"candidate profile mutation survived: {name}")

    candidate_document_mutations: list[
        tuple[str, Callable[[dict[str, Any]], None]]
    ] = [
        ("cross-receipt-verification-content-archive-sha", lambda d: _set(
            d["verificationReceipt"]["shippingPluginCapabilities"],
            "finalArchiveCanonicalManifestSha256", "F" * 64)),
        ("cross-receipt-plugin-verification-archive-sha", lambda d: _set(
            d["pluginCapabilityReceipt"]["finalPayload"]["archive"],
            "canonicalManifestSha256", "F" * 64)),
        ("cross-receipt-binary-verification-executable-sha", lambda d: _set(
            d["binaryReceipt"]["input"], "sha256", "F" * 64)),
        ("cross-receipt-audit-content-archive-bytes", lambda d: _set(
            d["provenanceDraftAudit"]["archive"], "bytes",
            d["provenanceDraftAudit"]["archive"]["bytes"] + 1)),
        ("cross-receipt-verification-build-log", lambda d: _set(
            d["verificationReceipt"]["buildLog"], "sha256", "F" * 64)),
        ("equipment-receipt-policy-binding-missing", lambda d: d[
            "v05EquipmentReceipt"
        ]["bindings"].pop("policy")),
        ("equipment-receipt-policy-binding-mismatch", lambda d: _set(
            d["v05EquipmentReceipt"]["bindings"]["policy"],
            "sha256", "0" * 64)),
        ("equipment-receipt-historical-policy", lambda d: _set(
            d["v05EquipmentReceipt"]["bindings"]["policy"],
            "fileName",
            PurePosixPath(
                EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]["path"]
            ).name)),
        ("three-hole-final-score-scalar", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"], "finalScore", 20)),
        ("three-hole-final-score-extra-key", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"]["finalScore"], "toPar", 9)),
        ("three-hole-final-score-total", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"]["finalScore"],
            "totalStrokes", 21)),
        ("three-hole-final-score-row-par", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"]["finalScore"]["holeRows"][1],
            "par", 3)),
        ("three-hole-final-score-row-bool", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"]["finalScore"]["holeRows"][0],
            "strokes", True)),
        ("three-hole-final-score-row-penalties", lambda d: _set(
            d["threeHoleReceipt"]["scoreClaims"]["finalScore"]["holeRows"][0],
            "penalties", 5)),
        ("three-hole-userdir-binding-mismatch", lambda d: _set(
            d["threeHoleReceipt"]["freshUserDir"],
            "candidateUserDirBindingSha256", "F" * 64)),
        ("three-hole-userdir-binding-nonhex", lambda d: _set(
            d["threeHoleReceipt"]["freshUserDir"],
            "candidateUserDirBindingSha256", "G" * 64)),
        ("serialized-policy-candidate", lambda d: _set(
            d["serializedCandidatePolicy"], "candidateId",
            "S19_WindowsShipping_20990101T000001Z_wrong")),
        ("serialized-policy-project-binding", lambda d: _set(
            d["serializedCandidatePolicy"]["bindings"],
            "candidateVerification", "Evidence/Session19/wrong.json")),
        ("serialized-receipt-candidate", lambda d: _set(
            d["serializedCandidateReceipt"], "candidateId",
            "S19_WindowsShipping_20990101T000001Z_wrong")),
        ("serialized-receipt-state", lambda d: _set(
            d["serializedCandidateReceipt"], "state", "PASS_INVENTED")),
        ("serialized-receipt-policy-binding", lambda d: _set(
            d["serializedCandidateReceipt"]["candidatePolicy"],
            "sha256", "0" * 64)),
        ("serialized-receipt-project-binding-path", lambda d: _set(
            d["serializedCandidateReceipt"]["bindings"]["candidateVerification"],
            "path", "Evidence/Session19/wrong.json")),
        ("serialized-receipt-project-binding-nonhex", lambda d: _set(
            d["serializedCandidateReceipt"]["bindings"]["candidateVerification"],
            "sha256", "G" * 64)),
        ("production-motion-receipt-historical-policy", lambda d: _set(
            d["productionMotionReceipt"], "policy",
            copy.deepcopy(EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]))),
        ("production-motion-receipt-observation", lambda d: _set(
            d["productionMotionReceipt"]["observations"],
            "freshShippingCandidateIncludesProductionMotion", True)),
        ("production-motion-policy-content-binding", lambda d: _set(
            d["productionMotionPolicy"]["bindings"]["candidateContentAudit"],
            "sha256", "0" * 64)),
        ("production-motion-cook-candidate", lambda d: _set(
            d["productionMotionCookReceipt"], "candidateId",
            "S19_WindowsShipping_20990101T000001Z_wrong")),
    ]
    for name, mutate in candidate_document_mutations:
        count += 1
        mutated_documents = copy.deepcopy(source_documents)
        mutate(mutated_documents)
        mutation_issues, _ = validate_contract(
            alternate, root, check_files=False, authority_contract=alternate,
            contract_binding=alternate_binding,
            candidate_profile_documents=mutated_documents,
        )
        if not mutation_issues:
            failures.append(f"candidate evidence mutation survived: {name}")

    with tempfile.TemporaryDirectory(prefix="dg-s19-release-scope-output-") as folder:
        output = Path(folder) / "result.json"
        result = {"candidateId": alternate_candidate, "contract": alternate_binding}
        write_json_exclusive(output, result)
        original = output.read_bytes()
        count += 1
        try:
            write_json_exclusive(output, result)
        except FileExistsError:
            if output.read_bytes() != original:
                failures.append("rejected explicit output changed existing bytes")
        else:
            failures.append("existing explicit output was overwritten")

    evidence_mutations: list[
        tuple[str, Path, Callable[[dict[str, Any]], None], Callable[[Any], list[str]]]
    ] = [
        ("receipt-extra-key", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "hostPath", "C:/secret"), validate_quarantine_receipt),
        ("receipt-version-bool", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "schemaVersion", True), validate_quarantine_receipt),
        ("receipt-host-path-flag", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "hostPathRecorded", True), validate_quarantine_receipt),
        ("receipt-host-path-token", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "recoveryLocationToken", "C:/DGTour_Quarantine/Content"),
         validate_quarantine_receipt),
        ("receipt-manifest-traversal", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d["beforeMoveManifest"], "path", "../escape.tsv"),
         validate_quarantine_receipt),
        ("receipt-shipping-absence", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d["releaseClosure"], "shippingPackageAbsenceSatisfied", True),
         validate_quarantine_receipt),
        ("receipt-blocker-closed", root / authority["unusedFabExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d["releaseClosure"], "blockerClosed", True),
         validate_quarantine_receipt),
        ("policy-extra-key", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d, "releaseApproved", True), validate_shipping_policy_document),
        ("policy-version-bool", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d, "schemaVersion", True), validate_shipping_policy_document),
        ("policy-release-ready", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d, "releaseReady", True), validate_shipping_policy_document),
        ("policy-definition-bool", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d["compileDefinitions"], "DG_WITH_CAREER_AI", False),
         validate_shipping_policy_document),
        ("policy-nevercook-missing", root / authority["shippingContentPolicy"]["path"],
         lambda d: d["neverCookRoots"].pop(), validate_shipping_policy_document),
        ("policy-nevercook-pcg-overreach", root / authority["shippingContentPolicy"]["path"],
         lambda d: d["neverCookRoots"].append("/Game/Environment/Forest/PCG"),
         validate_shipping_policy_document),
        ("policy-manifest-ufs-authoritative", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d["authoritativeIdentityPolicy"], "manifestUfsDevelopmentRows", "AUTHORITATIVE"),
         validate_shipping_policy_document),
        ("policy-landmass-token-missing", root / authority["shippingContentPolicy"]["path"],
         lambda d: d["forbiddenShippingPathTokens"].remove("/Engine/Plugins/Experimental/Landmass/Content/"),
         validate_shipping_policy_document),
        ("policy-career-enabled", root / authority["shippingContentPolicy"]["path"],
         lambda d: _set(d["featureExclusions"]["career"], "shippingEnabled", True),
         validate_shipping_policy_document),
        ("baked-policy-source-incomplete", root / authority["bakedPcgSourceAuthority"]["policyPath"],
         lambda d: _set(d, "sourceEvidenceComplete", False),
         validate_baked_pcg_policy_document),
        ("baked-policy-shipping-complete", root / authority["bakedPcgSourceAuthority"]["policyPath"],
         lambda d: _set(d, "shippingEvidenceComplete", True),
         validate_baked_pcg_policy_document),
        ("baked-policy-blocker-closed", root / authority["bakedPcgSourceAuthority"]["policyPath"],
         lambda d: _set(d, "closesSession13Blocker", True),
         validate_baked_pcg_policy_document),
        ("baked-receipt-source-rejected", root / authority["bakedPcgSourceAuthority"]["receiptPath"],
         lambda d: _set(d, "sourceAuthorityAccepted", False),
         lambda d: validate_baked_pcg_receipt_document(
             d, authority["bakedPcgSourceAuthority"]["policyPath"])),
        ("baked-receipt-shipping-accepted", root / authority["bakedPcgSourceAuthority"]["receiptPath"],
         lambda d: _set(d, "shippingPackageEvidenceAccepted", True),
         lambda d: validate_baked_pcg_receipt_document(
             d, authority["bakedPcgSourceAuthority"]["policyPath"])),
        ("baked-receipt-blocker-closed", root / authority["bakedPcgSourceAuthority"]["receiptPath"],
         lambda d: _set(d, "blockerClosed", True),
         lambda d: validate_baked_pcg_receipt_document(
             d, authority["bakedPcgSourceAuthority"]["policyPath"])),
        ("framework-receipt-host-path", root / authority["characterFrameworkExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "recoveryLocationToken", "C:/secret"),
         validate_framework_quarantine_receipt),
        ("framework-receipt-source-present", root / authority["characterFrameworkExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "projectSourcesAbsent", False),
         validate_framework_quarantine_receipt),
        ("framework-receipt-destination-unverified", root / authority["characterFrameworkExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d, "destinationIntegrityVerified", False),
         validate_framework_quarantine_receipt),
        ("framework-receipt-shipping-accepted", root / authority["characterFrameworkExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d["releaseClosure"], "shippingPackageAbsenceSatisfied", True),
         validate_framework_quarantine_receipt),
        ("framework-receipt-blocker-closed", root / authority["characterFrameworkExternalQuarantine"]["receipt"]["path"],
         lambda d: _set(d["releaseClosure"], "blockerClosed", True),
         validate_framework_quarantine_receipt),
        ("production-motion-current-pawn-binding",
         root / authority["productionMotionPreparation"]["policy"]["path"],
         lambda d: _set(d["bindings"]["golferPawnSource"], "sha256", "0" * 64),
         lambda d: production_motion_validator.validate_policy(
             d, root, check_files=True)[0]),
    ]
    loaded_documents: dict[Path, Any] = {}
    for name, path, mutate, validator in evidence_mutations:
        count += 1
        if path not in loaded_documents:
            try:
                loaded_documents[path] = load_json_strict(path)
            except StrictJsonError as exc:
                failures.append(f"self-test fixture cannot load {path}: {exc}")
                loaded_documents[path] = None
        fixture = loaded_documents[path]
        if type(fixture) is not dict:
            continue
        candidate = copy.deepcopy(fixture)
        mutate(candidate)
        if not validator(candidate):
            failures.append(f"evidence mutation survived: {name}")

    minimum = contract.get("validation", {}).get("selfTestMinimumMutationCount")
    if type(minimum) is not int or count < minimum:
        failures.append(f"self-test count {count} is below declared minimum {minimum!r}")
    return not failures, count, failures


def _replace_string_tokens(value: Any, replacements: dict[str, str]) -> Any:
    if type(value) is dict:
        return {
            key: _replace_string_tokens(child, replacements)
            for key, child in value.items()
        }
    if type(value) is list:
        return [_replace_string_tokens(child, replacements) for child in value]
    if type(value) is str:
        result = value
        for before, after in replacements.items():
            result = result.replace(before, after)
        return result
    return value


def _refresh_project_file_bindings(value: Any, root: Path) -> None:
    """Refresh only recognizable path/bytes/SHA triplets that resolve to files."""
    if type(value) is dict:
        triplets: list[tuple[str, str, str]] = []
        if {"path", "bytes", "sha256"}.issubset(value):
            triplets.append(("path", "bytes", "sha256"))
        for key in list(value):
            if type(key) is not str or not key.endswith("Path"):
                continue
            stem = key[:-4]
            bytes_key = stem + "Bytes"
            sha_key = stem + "Sha256"
            if bytes_key in value and sha_key in value:
                triplets.append((key, bytes_key, sha_key))
        for path_key, bytes_key, sha_key in triplets:
            token = value.get(path_key)
            if not is_canonical_relative_posix(token):
                continue
            path = root.joinpath(*PurePosixPath(token).parts)
            try:
                resolved = path.resolve(strict=True)
                resolved.relative_to(root.resolve())
            except (OSError, ValueError):
                continue
            if not path.is_file() or _is_reparse_component(path):
                continue
            raw = path.read_bytes()
            value[bytes_key] = len(raw)
            value[sha_key] = hashlib.sha256(raw).hexdigest().upper()
        for child in value.values():
            _refresh_project_file_bindings(child, root)
    elif type(value) is list:
        for child in value:
            _refresh_project_file_bindings(child, root)


def generate_candidate_contract(
        root: Path, candidate_id: str, destination_token: str,
) -> tuple[dict[str, Any] | None, list[str]]:
    """Build one preflighted candidate contract without mutating its authority."""
    issues: list[str] = []
    if (
            type(candidate_id) is not str
            or not re.fullmatch(r"S19_WindowsShipping_[A-Za-z0-9_-]+", candidate_id)):
        return None, ["--candidate-id must be a canonical S19 Windows Shipping ID"]
    destination = validate_relative_path(
        destination_token, "candidate contract destination", root, issues,
        must_exist=False,
    )
    if destination is None:
        return None, issues
    if destination_token == DEFAULT_CONTRACT_PATH.as_posix():
        issues.append("candidate contract generation may not target the historical contract")
    if not destination_token.startswith("Config/"):
        issues.append("candidate contract generation must target Config/")
    if candidate_id not in Path(destination_token).name:
        issues.append("candidate contract filename must contain the candidate ID")
    if os.path.lexists(destination):
        issues.append("candidate contract destination already exists; refusing overwrite")
    if issues:
        return None, issues
    try:
        historical = load_json_strict(root / DEFAULT_CONTRACT_PATH)
    except StrictJsonError as exc:
        return None, [f"historical contract is invalid: {exc}"]
    if type(historical) is not dict:
        return None, ["historical contract root must be an object"]
    historical_candidate = historical.get("currentShippingCandidateEvidence", {}).get(
        "candidateId"
    )
    if type(historical_candidate) is not str:
        return None, ["historical contract candidate ID is unavailable"]
    if candidate_id == historical_candidate:
        return None, ["candidate contract must select a candidate newer than the historical snapshot"]

    generated = _replace_string_tokens(
        copy.deepcopy(historical),
        {
            historical_candidate: candidate_id,
            "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicy.json": (
                "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicyV2.json"
            ),
        },
    )
    generated["contractId"] = (
        "session19_fresh_candidate_runtime_journal_operational_evidence_v1_"
        + candidate_id
    )
    generated["requiredFiles"] = [
        destination_token if path == DEFAULT_CONTRACT_PATH.as_posix() else path
        for path in generated["requiredFiles"]
    ]
    generated["threeHoleTechnicalAcceptance"]["policy"]["path"] = (
        "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicyV2.json"
    )
    generated["productionMotionPreparation"]["evidence"]["path"] = (
        "Evidence/Session19/ProductionMotionTechnicalCandidateEvidence-"
        f"{candidate_id}.json"
    )
    _refresh_project_file_bindings(generated, root)

    profile_expectations = derive_fresh_candidate_expectations(
        generated, root, issues,
    )
    if profile_expectations is None:
        return None, issues
    for section_name in (
            "stagedProvenancePolicy", "currentShippingCandidateEvidence",
            "v05EquipmentTechnical",
            "session10EnvironmentTechnical", "session12PresentationTechnical",
            "technicalOriginAttribution", "threeHoleTechnicalAcceptance",
            "productionMotionPreparation",
            "serializedAssetMigration"):
        generated[section_name] = profile_expectations[section_name]
    generated = _replace_string_tokens(generated, {
        EXPECTED_V05_EQUIPMENT_TECHNICAL["policy"]["path"]:
            generated["v05EquipmentTechnical"]["policy"]["path"],
        EXPECTED_SESSION10_ENVIRONMENT_TECHNICAL["policy"]["path"]:
            generated["session10EnvironmentTechnical"]["policy"]["path"],
        EXPECTED_SESSION12_PRESENTATION_TECHNICAL["policy"]["path"]:
            generated["session12PresentationTechnical"]["policy"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["policy"]["path"]:
            generated["productionMotionPreparation"]["policy"]["path"],
        EXPECTED_PRODUCTION_MOTION_PREPARATION["evidence"]["path"]:
            generated["productionMotionPreparation"]["evidence"]["path"],
    })
    serialized_candidate_policy_path = generated["serializedAssetMigration"][
        "candidatePolicyPath"
    ]
    if serialized_candidate_policy_path not in generated["requiredFiles"]:
        generated["requiredFiles"].append(serialized_candidate_policy_path)
    equipment_candidate_policy_path = generated["v05EquipmentTechnical"][
        "policy"
    ]["path"]
    if equipment_candidate_policy_path not in generated["requiredFiles"]:
        generated["requiredFiles"].append(equipment_candidate_policy_path)
    for production_motion_path in (
            generated["productionMotionPreparation"]["policy"]["path"],
            generated["productionMotionPreparation"]["evidence"]["path"]):
        if production_motion_path not in generated["requiredFiles"]:
            generated["requiredFiles"].append(production_motion_path)
    _refresh_project_file_bindings(generated, root)

    required_files = generated.get("requiredFiles")
    if type(required_files) is not list:
        issues.append("generated candidate requiredFiles must be an array")
    else:
        for index, token in enumerate(required_files):
            if token == destination_token:
                continue
            validate_relative_path(
                token, f"generated requiredFiles[{index}]", root, issues,
                must_exist=True,
            )
    if issues:
        return None, issues
    encoded = (json.dumps(generated, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    binding = {
        "path": destination_token,
        "bytes": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest().upper(),
    }
    validation_issues, _ = validate_contract(
        generated, root, check_files=False, authority_contract=generated,
        contract_binding=binding,
    )
    if validation_issues:
        issues.extend(
            f"generated contract preflight: {issue}" for issue in validation_issues
        )
        return None, issues
    return generated, issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument(
        "--contract", type=Path, default=DEFAULT_CONTRACT_PATH,
        help="Project-relative release-scope contract (default: historical Config contract).",
    )
    parser.add_argument(
        "--output", type=Path,
        help="Optional append-only JSON validation result.",
    )
    parser.add_argument(
        "--generate-candidate-contract", type=Path,
        help=(
            "Append-only candidate contract path under Config; derives the fresh "
            "profile from the candidate's bound receipts and policies."
        ),
    )
    parser.add_argument(
        "--candidate-id",
        help="Candidate ID used only with --generate-candidate-contract.",
    )
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-release-ready", "--require-release", action="store_true")
    args = parser.parse_args()

    if args.self_test and args.output is not None:
        parser.error("--self-test does not write --output")
    if args.generate_candidate_contract is not None and (
            args.self_test or args.output is not None or args.require_release_ready):
        parser.error(
            "--generate-candidate-contract cannot be combined with --self-test, "
            "--output, or --require-release-ready"
        )
    if (args.generate_candidate_contract is None) != (args.candidate_id is None):
        parser.error(
            "--generate-candidate-contract and --candidate-id must be supplied together"
        )

    root = args.root.resolve()
    if args.generate_candidate_contract is not None:
        destination = args.generate_candidate_contract
        if not destination.is_absolute():
            destination = root / destination
        destination = destination.resolve(strict=False)
        try:
            destination_relative = destination.relative_to(root)
        except ValueError:
            print(
                "SESSION 19 CANDIDATE CONTRACT GENERATION REFUSED: "
                "destination must be inside --root"
            )
            return 1
        destination_token = PurePosixPath(*destination_relative.parts).as_posix()
        generated, generation_issues = generate_candidate_contract(
            root, args.candidate_id, destination_token,
        )
        if generated is None:
            print("SESSION 19 CANDIDATE CONTRACT GENERATION REFUSED")
            for issue in generation_issues:
                print(" -", issue)
            return 1
        try:
            write_json_exclusive(destination, generated)
        except (FileExistsError, OSError) as exc:
            print(f"SESSION 19 CANDIDATE CONTRACT GENERATION REFUSED: {exc}")
            return 1
        raw = destination.read_bytes()
        print(
            "SESSION 19 CANDIDATE CONTRACT GENERATED_APPEND_ONLY: "
            f"candidate={args.candidate_id} contract={destination_token} "
            f"bytes={len(raw)} sha256={hashlib.sha256(raw).hexdigest().upper()}"
        )
        return 0

    contract_path = args.contract
    if not contract_path.is_absolute():
        contract_path = root / contract_path
    contract_path = contract_path.resolve()
    try:
        contract_relative = contract_path.relative_to(root)
    except ValueError:
        print("SESSION 19 RELEASE SCOPE INVALID: --contract must be inside --root")
        return 1
    contract_path_token = PurePosixPath(*contract_relative.parts).as_posix()
    try:
        contract_bytes = contract_path.read_bytes()
        contract = load_json_text_strict(contract_bytes.decode("utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        print(f"SESSION 19 RELEASE SCOPE INVALID: {exc}")
        return 1
    if type(contract) is not dict:
        print("SESSION 19 RELEASE SCOPE INVALID: contract root must be an object")
        return 1
    contract_binding = {
        "path": contract_path_token,
        "bytes": len(contract_bytes),
        "sha256": hashlib.sha256(contract_bytes).hexdigest().upper(),
    }

    if args.self_test:
        passed, count, failures = run_self_test(
            contract, root, contract_binding=contract_binding)
        if not passed:
            print(f"SESSION 19 RELEASE SCOPE SELF-TEST FAILED: {count} cases")
            for failure in failures:
                print(" -", failure)
            return 1
        print(f"SESSION 19 RELEASE SCOPE SELF-TEST PASS: {count}/{count}")
        return 0

    issues, summary = validate_contract(
        contract, root, check_files=True, authority_contract=contract,
        contract_binding=contract_binding,
    )
    result_state = (
        "INVALID_SELECTED_CONTRACT_OR_BOUND_EVIDENCE"
        if issues else
        "BLOCKED_PENDING_EVIDENCE"
        if args.require_release_ready else
        "PASS_AUTHORITY_RELEASE_BLOCKED"
    )
    result = {
        "schema": "DiscGolfTour.Session19ReleaseScopeValidationResult.v1",
        "schemaVersion": 1,
        "state": result_state,
        "valid": not issues,
        "releaseReady": False,
        "candidateId": summary.get("candidateId"),
        "contract": contract_binding,
        "summary": summary,
        "issues": issues,
    }
    if args.output is not None:
        try:
            write_json_exclusive(args.output.resolve(), result)
        except (FileExistsError, OSError) as exc:
            print(f"SESSION 19 RELEASE SCOPE OUTPUT REFUSED: {exc}")
            return 1
    if issues:
        print("SESSION 19 RELEASE SCOPE INVALID")
        for issue in issues:
            print(" -", issue)
        return 1

    selected_binding_suffix = ""
    if contract_path_token != DEFAULT_CONTRACT_PATH.as_posix():
        selected_binding_suffix = (
            f" candidate={summary['candidateId']} contract={contract_path_token} "
            f"contract_sha256={contract_binding['sha256']}"
        )

    if args.require_release_ready:
        print(
            "SESSION 19 RELEASE SCOPE BLOCKED_PENDING_EVIDENCE: "
            f"platform={summary['platform']} configuration={summary['configuration']} "
            f"holes={summary['holeCount']} blockers={summary['remainingBlockerCount']} "
            f"pending_gates={summary['pendingEvidenceGateCount']} "
            f"partial_gates={summary['partialEvidenceGateCount']}"
            f"{selected_binding_suffix}"
        )
        return 2

    print(
        "SESSION 19 RELEASE SCOPE PASS_AUTHORITY_RELEASE_BLOCKED: "
        f"platform={summary['platform']} configuration={summary['configuration']} "
        f"holes={summary['holeCount']} strategies={summary['strategyCount']} "
        f"blockers={summary['remainingBlockerCount']} "
        f"pending_gates={summary['pendingEvidenceGateCount']} "
        f"partial_gates={summary['partialEvidenceGateCount']}"
        f"{selected_binding_suffix}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
