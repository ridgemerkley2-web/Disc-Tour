#!/usr/bin/env python3
"""Validate bounded Session 10 environment evidence in one Shipping candidate.

This validator proves the exact source-controlled engineering contract for all
16 environment categories, candidate/container inclusion for the currently
populated slots, explicit Shipping exclusion of the editor PCG graph, authored
gameplay proxy-definition coverage, quarantined-root absence, and hashes of an
external runtime capture.  It does not convert those
facts into authored asset completeness, dedicated-proxy completion, material
wind binding, real-RHI, visual-quality, performance, provenance, legal, owner,
distribution, or release approval.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session10EnvironmentShippingTechnicalPolicy.json"
SHA_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)
LEGACY_POLICY_SCHEMA = "DiscGolfTour.Session10EnvironmentShippingTechnicalPolicy.v3"
TRUSTED_POLICY_SCHEMA = "DiscGolfTour.Session10EnvironmentShippingTechnicalPolicy.v4"
LEGACY_MANIFEST_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
LEGACY_LAUNCH_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1"
TRUSTED_MANIFEST_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
TRUSTED_LAUNCH_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_RELATIVE = "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_TRUST_BOUNDARY = {
    "evidenceTrustModel": (
        "BOUNDED_OPERATIONAL_PROCESS_EVIDENCE_NOT_CRYPTOGRAPHIC_ATTESTATION"
    ),
    "cryptographicProcessAttestation": False,
    "tamperProofEvidence": False,
    "hostileSameUserForgeryResistance": False,
}
TRUSTED_RUNTIME_JOURNAL_CONTRACT = {
    "manifestSchema": TRUSTED_MANIFEST_SCHEMA,
    "manifestSchemaVersion": 2,
    "launchRecordSchema": TRUSTED_LAUNCH_SCHEMA,
    "launchRecordSchemaVersion": 2,
    "runtimeJournalSchema": RUNTIME_JOURNAL_SCHEMA,
    "runtimeJournalUserDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
    "runtimeJournalValidationState": RUNTIME_JOURNAL_PASS,
    "requireIndependentRawJournalValidation": True,
    "trustBoundary": RUNTIME_TRUST_BOUNDARY,
}
SLOTS = [
    "TreeConiferLarge", "TreeConiferMedium", "TreeConiferYoung",
    "TreeDeciduousLarge", "TreeDeciduousMedium", "Sapling", "Shrub",
    "Fern", "Grass", "GroundCover", "Log", "Stump", "RockSmall",
    "RockLarge", "ForestDebris", "LeafLitter",
]
POPULATED = [
    {
        "slot": "TreeConiferYoung",
        "collisionMode": "TrunkOrBranchBlocking",
        "variantObjectPaths": [
            "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.fir_sapling_a",
            "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_b.fir_sapling_b",
            "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_c.fir_sapling_c",
        ],
        "dedicatedProxyPathsComplete": False,
        "productionReady": False,
    },
    {
        "slot": "Shrub",
        "collisionMode": "ShrubOverlap",
        "variantObjectPaths": [
            "/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.shrub_04_1k",
        ],
        "dedicatedProxyPathsComplete": False,
        "productionReady": False,
    },
    {
        "slot": "RockLarge",
        "collisionMode": "SolidBlocking",
        "variantObjectPaths": [
            "/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.boulder_01_1k",
        ],
        "dedicatedProxyPathsComplete": False,
        "productionReady": False,
    },
]
SHIPPING_ENVIRONMENT_ASSETS = [
    "DiscGolfTour/Content/Environment/Forest/DA_TemperateMountainForest_Assets.uasset",
    "DiscGolfTour/Content/Environment/Forest/DA_TemperateMountainForest.uasset",
    "DiscGolfTour/Content/Environment/Forest/Materials/MPC_EnvironmentWind.uasset",
]
SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS = [
    "DiscGolfTour/Content/Environment/Forest/PCG/PCG_TemperateMountainForest.uasset",
]
VISUAL_VARIANTS = [
    "DiscGolfTour/Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.uasset",
    "DiscGolfTour/Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_b.uasset",
    "DiscGolfTour/Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_c.uasset",
    "DiscGolfTour/Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.uasset",
    "DiscGolfTour/Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.uasset",
]
HOLES = [
    {"holeNumber": 1, "treeBlockingProxies": 12, "fixtureBlockingProxies": 2,
     "interactionVolumes": 1, "blockingFixtureTypes": ["Rock", "Sign"],
     "interactionFixtureTypes": ["DenseGrass"]},
    {"holeNumber": 2, "treeBlockingProxies": 18, "fixtureBlockingProxies": 2,
     "interactionVolumes": 1, "blockingFixtureTypes": ["Rock", "Sign"],
     "interactionFixtureTypes": ["DenseGrass"]},
    {"holeNumber": 3, "treeBlockingProxies": 14, "fixtureBlockingProxies": 2,
     "interactionVolumes": 1, "blockingFixtureTypes": ["Rock", "Sign"],
     "interactionFixtureTypes": ["DenseGrass"]},
]
QUARANTINE_TOKENS = [
    "PN_interactiveSpruceForest", "Stump_Scanned", "WaterMaterials",
]
SELECTED_ENVIRONMENT_MILESTONES = [
    "HOLE_1_TEE_ENVIRONMENT_RENDERED",
    "HOLE_1_RECOVERED_LIE_ENVIRONMENT_RENDERED",
    "HOLE_3_TEE_ENVIRONMENT_RENDERED",
]
CLAIMS = {
    "candidateBoundTechnicalSlotBindingAccepted": True,
    "allSixteenCategoryTechnicalContractsAccepted": True,
    "collisionModeMetadataAccepted": True,
    "proxyRequirementMetadataAccepted": True,
    "lodWindAndCullingMetadataAccepted": True,
    "authoredCollisionInteractionProxyCoverageAccepted": True,
    "forbiddenQuarantinedContentAbsenceAccepted": True,
    "shippingPcgGraphAbsenceAccepted": True,
    "candidateScreenshotHashBindingAccepted": True,
    "allSixteenProductionSlotsBound": False,
    "allRequiredDedicatedProxiesAuthored": False,
    "allRequiredWindMaterialsBound": False,
    "allRequiredLodOrNaniteVerified": False,
    "allThreeHolesLiveCollisionExerciseAccepted": False,
    "actualRenderedRhiAccepted": False,
    "visualQualityAccepted": False,
    "performanceApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "ownerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}
BINDING_KEYS = {
    "session10EnvironmentContract", "candidateContentAudit",
    "candidateVerification", "externalTechnicalValidation",
    "assignedSlotReport", "hole1RuntimeStatistics", "presentationContract",
    "hole1Definition", "hole2Definition", "hole3Definition",
    "environmentAssetSet", "environmentPreset", "environmentWind",
    "environmentPcgGraph", "categoryTechnicalPlan",
}
PLAN_BINDING_KEYS = {
    "environmentTypesHeader", "environmentDataAssetsSource",
    "environmentAssetBinderHeader", "environmentAssetBinderSource",
    "assignedReadinessReport",
}
CATEGORY_DEFAULTS = [
    ("TreeConiferLarge", "TrunkOrBranchBlocking", "CollisionProxyMesh", 760.0, 28000.0, 50000.0, 18000.0, True),
    ("TreeConiferMedium", "TrunkOrBranchBlocking", "CollisionProxyMesh", 620.0, 25000.0, 45000.0, 18000.0, True),
    ("TreeConiferYoung", "TrunkOrBranchBlocking", "CollisionProxyMesh", 440.0, 19000.0, 34000.0, 18000.0, True),
    ("TreeDeciduousLarge", "TrunkOrBranchBlocking", "CollisionProxyMesh", 800.0, 28000.0, 50000.0, 18000.0, True),
    ("TreeDeciduousMedium", "TrunkOrBranchBlocking", "CollisionProxyMesh", 600.0, 23000.0, 42000.0, 18000.0, True),
    ("Sapling", "ShrubOverlap", "InteractionProxyMesh", 260.0, 13000.0, 25000.0, 13000.0, True),
    ("Shrub", "ShrubOverlap", "InteractionProxyMesh", 180.0, 10000.0, 22000.0, 10000.0, True),
    ("Fern", "None", "None", 80.0, 7000.0, 16000.0, 7000.0, True),
    ("Grass", "None", "None", 35.0, 4500.0, 12500.0, 4500.0, True),
    ("GroundCover", "None", "None", 45.0, 5000.0, 14000.0, 5000.0, True),
    ("Log", "SolidBlocking", "CollisionProxyMesh", 500.0, 18000.0, 32000.0, 18000.0, False),
    ("Stump", "SolidBlocking", "CollisionProxyMesh", 350.0, 15000.0, 28000.0, 15000.0, False),
    ("RockSmall", "SolidBlocking", "CollisionProxyMesh", 220.0, 13000.0, 26000.0, 13000.0, False),
    ("RockLarge", "SolidBlocking", "CollisionProxyMesh", 600.0, 23000.0, 42000.0, 18000.0, False),
    ("ForestDebris", "None", "None", 90.0, 7000.0, 16000.0, 7000.0, False),
    ("LeafLitter", "None", "None", 45.0, 5000.0, 14000.0, 5000.0, False),
]
PLAN_SUMMARY = {
    "categoryTechnicalContractCount": 16,
    "categoryTechnicalContractsComplete": 16,
    "provisionalVisualBoundCategoryCount": 3,
    "missingVisualCategoryCount": 13,
    "boundVisualVariantCount": 5,
    "productionReadyCategoryCount": 0,
    "blockingProxyRequiredCategoryCount": 9,
    "blockingProxyRequirementSatisfiedCategoryCount": 0,
    "interactionProxyRequiredCategoryCount": 2,
    "interactionProxyRequirementSatisfiedCategoryCount": 0,
    "noProxyRequiredCategoryCount": 5,
    "windRequiredCategoryCount": 10,
    "windVerifiedCategoryCount": 0,
    "lodOrNaniteVerifiedCategoryCount": 2,
    "lodOrNaniteUnresolvedCategoryCount": 14,
    "baseCullingMetadataCompleteCategoryCount": 16,
    "qualityScalingMetadataCompleteTierCount": 3,
    "allSixteenProductionSlotsBound": False,
    "allSixteenProductionSlotsReady": False,
}
PLAN_CLAIMS = {
    "allSixteenCategoryTechnicalContractsAccepted": True,
    "collisionModeMetadataAccepted": True,
    "proxyRequirementMetadataAccepted": True,
    "lodWindAndCullingMetadataAccepted": True,
    "sourceBindingsAccepted": True,
    "allSixteenProductionSlotsBound": False,
    "allRequiredDedicatedProxiesAuthored": False,
    "allRequiredWindMaterialsBound": False,
    "allRequiredLodOrNaniteVerified": False,
    "visualQualityAccepted": False,
    "performanceApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "ownerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}


class StrictJsonError(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def _constant(value: str) -> None:
    raise StrictJsonError(f"non-finite JSON constant: {value}")


def _validate_numbers(value: Any, label: str = "root") -> None:
    if type(value) is int and not -(2**63) <= value <= 2**63 - 1:
        raise StrictJsonError(f"integer outside signed 64-bit range at {label}")
    if type(value) is float and not (-1.7976931348623157e308 <= value <= 1.7976931348623157e308):
        raise StrictJsonError(f"non-finite number at {label}")
    if type(value) is dict:
        for key, item in value.items():
            _validate_numbers(item, f"{label}.{key}")
    elif type(value) is list:
        for index, item in enumerate(value):
            _validate_numbers(item, f"{label}[{index}]")


def load_json(path: Path) -> Any:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_pairs,
            parse_constant=_constant,
        )
        _validate_numbers(value)
        return value
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_identity(path: Path, root: Path = ROOT) -> dict[str, Any]:
    return {
        "path": path.relative_to(root).as_posix(),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def exact_keys(value: Any, keys: set[str], label: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != keys:
        issues.append(
            f"{label} keys differ: missing={sorted(keys-actual)} extra={sorted(actual-keys)}"
        )
        return False
    return True


def expect(actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    if type(actual) is not type(expected) or actual != expected:
        issues.append(f"{label} differs: expected {expected!r}, got {actual!r}")


def expect_positive_int(value: Any, label: str, issues: list[str]) -> None:
    if type(value) is not int or value < 1:
        issues.append(f"{label} must be a positive integer")


def expect_sha256(value: Any, label: str, issues: list[str]) -> None:
    if type(value) is not str or SHA_RE.fullmatch(value) is None:
        issues.append(f"{label} must be uppercase SHA-256")


def safe_project_path(value: Any, root: Path, label: str, issues: list[str]) -> Path | None:
    if type(value) is not str or not value:
        issues.append(f"{label} must be a non-empty project-relative path")
        return None
    pure = PurePosixPath(value)
    if pure.is_absolute() or ".." in pure.parts or "\\" in value or ":" in value:
        issues.append(f"{label} is unsafe: {value!r}")
        return None
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError:
        issues.append(f"{label} escapes the project root")
        return None
    return path


def selected_policy_path(value: Path, root: Path) -> Path:
    """Resolve a selected policy without allowing it to escape its project root."""
    path = (value if value.is_absolute() else root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError as exc:
        raise StrictJsonError("selected policy must be inside the project root") from exc
    return path


def trusted_runtime_journal_required(policy: Any) -> bool:
    return (
        type(policy) is dict
        and policy.get("schema") == TRUSTED_POLICY_SCHEMA
        and type(policy.get("schemaVersion")) is int
        and policy.get("schemaVersion") == 4
    )


def external_schema_contract_issues(
    policy: dict[str, Any], manifest: Any, launch: Any
) -> list[str]:
    """Reject cross-lane v1/v2 evidence before any semantic validation."""

    issues: list[str] = []
    trusted = trusted_runtime_journal_required(policy)
    expected_manifest_schema = (
        TRUSTED_MANIFEST_SCHEMA if trusted else LEGACY_MANIFEST_SCHEMA
    )
    expected_manifest_version = 2 if trusted else 1
    expected_launch_schema = TRUSTED_LAUNCH_SCHEMA if trusted else LEGACY_LAUNCH_SCHEMA
    expected_launch_version = 2 if trusted else 1
    if type(manifest) is not dict:
        issues.append("external manifest must be an object")
    else:
        if manifest.get("schema") != expected_manifest_schema:
            issues.append("external manifest schema differs from selected policy lane")
        if (
            type(manifest.get("schemaVersion")) is not int
            or manifest.get("schemaVersion") != expected_manifest_version
        ):
            issues.append("external manifest schemaVersion differs from selected policy lane")
    if type(launch) is not dict:
        issues.append("external launch record must be an object")
    else:
        if launch.get("schema") != expected_launch_schema:
            issues.append("external launch schema differs from selected policy lane")
        if (
            type(launch.get("schemaVersion")) is not int
            or launch.get("schemaVersion") != expected_launch_version
        ):
            issues.append("external launch schemaVersion differs from selected policy lane")
    return issues


def validate_trusted_runtime_journal(
    *,
    root: Path,
    archive: Path,
    executable: Path,
    user_dir: Path,
    manifest_path: Path,
    launch_path: Path,
    manifest: dict[str, Any],
    launch: dict[str, Any],
    policy: dict[str, Any],
) -> tuple[dict[str, Any] | None, list[str]]:
    """Independently validate the raw game-emitted journal and return host-free binding."""

    issues = external_schema_contract_issues(policy, manifest, launch)
    if issues:
        return None, issues
    try:
        sys.path.insert(0, str(root / "Scripts"))
        from validate_dg_session19_external_technical_evidence import validate_evidence

        validation = validate_evidence(
            archive_root=archive,
            shipping_exe=executable,
            user_dir=user_dir,
            manifest_path=manifest_path,
        )
    except Exception as exc:
        return None, [
            "trusted runtime journal validation failed closed: "
            f"{exc.__class__.__name__}"
        ]
    if (
        validation.get("state")
        != "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
        or validation.get("failures") != []
    ):
        codes = sorted({
            str(item.get("code"))
            for item in validation.get("failures", [])
            if type(item) is dict
        })
        return None, [
            "trusted runtime journal independent validation failed: "
            + (",".join(codes) if codes else "UNKNOWN")
        ]
    if validation.get("manifest", {}).get("sha256") != sha256(manifest_path):
        return None, ["trusted runtime journal manifest changed across validation"]

    runtime = manifest.get("evidence", {}).get("runtimeCheckpointJournal")
    launch_binding = manifest.get("evidence", {}).get("launchRecord")
    contract = policy.get("trustedRuntimeJournalContract")
    if (
        contract != TRUSTED_RUNTIME_JOURNAL_CONTRACT
        or type(runtime) is not dict
        or type(launch_binding) is not dict
    ):
        return None, ["trusted runtime journal contract or manifest binding differs"]
    if (
        set(launch_binding) != {"relativePath", "bytes", "sha256"}
        or launch_binding.get("relativePath") != "launch-record.json"
        or type(launch_binding.get("bytes")) is not int
        or launch_binding.get("bytes", 0) <= 0
        or launch_binding.get("bytes") != launch_path.stat().st_size
        or type(launch_binding.get("sha256")) is not str
        or SHA_RE.fullmatch(launch_binding.get("sha256", "")) is None
        or launch_binding.get("sha256") != sha256(launch_path)
    ):
        return None, ["trusted runtime journal launch-record binding differs"]
    expected_keys = {
        "bytes", "captureNonce", "eventCount", "firstSequence", "lastSequence",
        "modifiedUtc", "modifiedWithinRunWindow", "roundId", "schema", "sha256",
        "userDirRelativePath", "validationFailures", "validationState",
    }
    if set(runtime) != expected_keys:
        return None, ["trusted runtime journal manifest fields differ"]
    if (
        runtime.get("schema") != contract["runtimeJournalSchema"]
        or runtime.get("userDirRelativePath")
        != contract["runtimeJournalUserDirRelativePath"]
        or runtime.get("validationState") != contract["runtimeJournalValidationState"]
        or runtime.get("validationFailures") != []
        or runtime.get("modifiedWithinRunWindow") is not True
        or runtime.get("captureNonce") != launch.get("captureNonce")
        or launch.get("runtimeCheckpointJournalUserDirRelativePath")
        != contract["runtimeJournalUserDirRelativePath"]
        or manifest.get("candidate", {}).get("candidateId") != policy.get("candidateId")
        or manifest.get("userDir", {}).get("token")
        != policy.get("externalRuntimeEvidence", {}).get("externalUserDirToken")
        or type(runtime.get("bytes")) is not int
        or runtime.get("bytes", 0) <= 0
        or type(runtime.get("eventCount")) is not int
        or runtime.get("eventCount") != 12
        or type(runtime.get("firstSequence")) is not int
        or runtime.get("firstSequence") != 1
        or type(runtime.get("lastSequence")) is not int
        or runtime.get("lastSequence") != 12
        or type(runtime.get("sha256")) is not str
        or SHA_RE.fullmatch(runtime.get("sha256", "")) is None
        or type(runtime.get("captureNonce")) is not str
        or UUID_RE.fullmatch(runtime.get("captureNonce", "")) is None
        or type(runtime.get("roundId")) is not str
        or UUID_RE.fullmatch(runtime.get("roundId", "")) is None
    ):
        return None, ["trusted runtime journal manifest/launch binding differs"]
    return {
        "schema": runtime["schema"],
        "userDirRelativePath": runtime["userDirRelativePath"],
        "bytes": runtime["bytes"],
        "sha256": runtime["sha256"],
        "captureNonce": runtime["captureNonce"],
        "roundId": runtime["roundId"],
        "eventCount": runtime["eventCount"],
        "firstSequence": runtime["firstSequence"],
        "lastSequence": runtime["lastSequence"],
        "modifiedWithinRunWindow": True,
        "validationState": runtime["validationState"],
        "independentValidationState": validation["state"],
        "launchRecordSha256": launch_binding["sha256"],
        "technicalEvidenceManifestSha256": sha256(manifest_path),
        "executableSha256": policy["candidate"]["shippingExecutableSha256"],
        "archiveManifestSha256": policy["candidate"][
            "archiveCanonicalManifestSha256"
        ],
        "trustBoundary": copy.deepcopy(RUNTIME_TRUST_BOUNDARY),
    }, []


def verify_binding(
    binding: Any, root: Path, label: str, issues: list[str], *, check_file: bool
) -> Path | None:
    if not exact_keys(binding, {"path", "bytes", "sha256"}, label, issues):
        return None
    path = safe_project_path(binding["path"], root, f"{label}.path", issues)
    if type(binding["bytes"]) is not int or type(binding["bytes"]) is bool or binding["bytes"] < 1:
        issues.append(f"{label}.bytes must be a positive integer")
    if type(binding["sha256"]) is not str or not SHA_RE.fullmatch(binding["sha256"]):
        issues.append(f"{label}.sha256 must be uppercase SHA-256")
    if check_file and path is not None:
        if not path.is_file():
            issues.append(f"{label} is missing: {binding['path']}")
        elif file_identity(path, root) != binding:
            issues.append(f"{label} identity differs")
    return path


def validate_category_plan(
    plan: Any, root: Path, *, check_files: bool
) -> tuple[list[str], dict[str, Path]]:
    issues: list[str] = []
    paths: dict[str, Path] = {}
    root_keys = {
        "schema", "schemaVersion", "session", "milestone", "authority", "state",
        "bindings", "categoryOrder", "runtimeInvariants", "qualityTiers",
        "windContract", "categories", "summary", "residualBlockers", "claimBoundary",
    }
    if not exact_keys(plan, root_keys, "category plan", issues):
        return issues, paths
    for key, expected in {
        "schema": "DiscGolfTour.Session10EnvironmentCategoryTechnicalPlan.v1",
        "schemaVersion": 1,
        "session": 10,
        "milestone": "v0.5",
        "authority": "SOURCE_CONTROLLED_ENVIRONMENT_CATEGORY_ENGINEERING_CONTRACT_NOT_ASSET_VISUAL_PERFORMANCE_PROVENANCE_LEGAL_OWNER_OR_RELEASE_APPROVAL",
        "state": "SIXTEEN_CATEGORY_TECHNICAL_CONTRACT_COMPLETE_ASSET_AUTHORING_AND_APPROVAL_PENDING",
    }.items():
        expect(plan[key], expected, f"category plan.{key}", issues)

    if exact_keys(plan["bindings"], PLAN_BINDING_KEYS, "category plan.bindings", issues):
        for key, binding in plan["bindings"].items():
            path = verify_binding(
                binding, root, f"category plan.bindings.{key}", issues,
                check_file=check_files,
            )
            if path is not None:
                paths[key] = path

    expect(plan["categoryOrder"], SLOTS, "category plan order", issues)
    expect(plan["runtimeInvariants"], {
        "categoryCount": 16,
        "visualMeshesOwnCompetitiveCollision": False,
        "competitiveCollisionOwner": "PROJECT_OWNED_AUTHORED_FIXTURES_AND_DEDICATED_PROXIES",
        "collisionInvariantAcrossQuality": True,
        "visualMeshesRequireCollisionDisabled": True,
        "complexAsSimpleAcceptedForDedicatedProxies": False,
        "runtimePcgGenerationPolicy": "GENERATE_ON_DEMAND_PENDING_COMPLETE_APPROVED_BINDINGS",
        "environmentActorsTick": False,
    }, "category plan runtime invariants", issues)
    expect(plan["qualityTiers"], [
        {"id": "Performance", "densityScale": 0.58, "cullDistanceScale": 0.72,
         "shadowDistanceScale": 0.62, "allowNaniteForSuitableSolids": True,
         "enableCanopyInteractionVolumes": True, "enableGrassShadows": False,
         "affectsCompetitiveCollision": False},
        {"id": "High", "densityScale": 1.0, "cullDistanceScale": 1.0,
         "shadowDistanceScale": 1.0, "allowNaniteForSuitableSolids": True,
         "enableCanopyInteractionVolumes": True, "enableGrassShadows": False,
         "affectsCompetitiveCollision": False},
        {"id": "Cinematic", "densityScale": 1.3, "cullDistanceScale": 1.35,
         "shadowDistanceScale": 1.45, "allowNaniteForSuitableSolids": True,
         "enableCanopyInteractionVolumes": True, "enableGrassShadows": True,
         "affectsCompetitiveCollision": False},
    ], "category plan quality tiers", issues)
    expect(plan["windContract"], {
        "materialParameterCollection": "/Game/Environment/Forest/Materials/MPC_EnvironmentWind.MPC_EnvironmentWind",
        "requiredParameters": ["WindDirection", "WindSpeedMps", "GustStrengthMps"],
        "defaultDirection": [1.0, 0.25, 0.0],
        "defaultSpeedMps": 2.8,
        "defaultGustStrengthMps": 1.4,
        "defaultGustFrequencyHz": 0.12,
        "flightWindSynchronizationDefault": False,
        "presentationOnlyUntilExplicitOptIn": True,
    }, "category plan wind contract", issues)

    category_keys = {
        "index", "category", "bindingState", "visualObjectPaths", "collisionMode",
        "dedicatedProxyRequirement", "dedicatedProxyPaths",
        "dedicatedProxyRequirementSatisfied", "visualCollisionPolicy",
        "minimumLodCount", "naniteAlternativeAllowed", "lodNaniteVerificationState",
        "windBindingRequired", "windBindingVerificationState", "scaleReviewState",
        "minimumSpacingCm", "cullStartCm", "cullEndCm", "shadowCullDistanceCm",
        "categoryTechnicalContractComplete", "assetProductionReady",
    }
    categories = plan["categories"]
    if type(categories) is not list or len(categories) != 16:
        issues.append("category plan.categories must contain exactly 16 entries")
        categories = []
    populated_by_name = {entry["slot"]: entry for entry in POPULATED}
    evidence_states = {
        "TreeConiferYoung": (
            "VERIFIED_LOD_CHAIN_COUNTS_4_4_4",
            "MISSING_ON_ALL_BOUND_VARIANTS",
            "REVIEW_REQUIRED_OUTSIDE_CATEGORY_RANGE",
        ),
        "Shrub": (
            "BLOCKED_ONE_LOD_NON_NANITE",
            "MISSING_ON_BOUND_VARIANT",
            "STATIC_RANGE_CHECK_PASSED_VISUAL_REVIEW_PENDING",
        ),
        "RockLarge": (
            "VERIFIED_LOD_CHAIN_COUNT_3",
            "NOT_REQUIRED",
            "STATIC_RANGE_CHECK_PASSED_VISUAL_REVIEW_PENDING",
        ),
    }
    for index, defaults in enumerate(CATEGORY_DEFAULTS):
        if index >= len(categories) or type(categories[index]) is not dict:
            issues.append(f"category plan entry {index} is missing or invalid")
            continue
        row = categories[index]
        if not exact_keys(row, category_keys, f"category plan.categories[{index}]", issues):
            continue
        name, mode, proxy_requirement, spacing, cull_start, cull_end, shadow, needs_wind = defaults
        populated = populated_by_name.get(name)
        visual_paths = populated["variantObjectPaths"] if populated else []
        lod_state, wind_state, scale_state = evidence_states.get(name, (
            "PENDING_VISUAL_BINDING",
            "PENDING_VISUAL_BINDING" if needs_wind else "NOT_REQUIRED",
            "PENDING_VISUAL_BINDING",
        ))
        if mode in {"TrunkOrBranchBlocking", "SolidBlocking"}:
            visual_collision = "VISUAL_MESH_NO_COLLISION_SEPARATE_BLOCKING_PROXY"
        elif mode in {"CanopyOverlap", "ShrubOverlap"}:
            visual_collision = "VISUAL_MESH_NO_COLLISION_NONBLOCKING_INTERACTION_PROXY"
        else:
            visual_collision = "VISUAL_MESH_NO_COLLISION_NO_PROXY"
        expected_row = {
            "index": index,
            "category": name,
            "bindingState": (
                "PROVISIONAL_VISUAL_BOUND_NOT_PRODUCTION_READY" if populated
                else "MISSING_ASSET_SELECTION_AND_LICENSE_RECORD"
            ),
            "visualObjectPaths": visual_paths,
            "collisionMode": mode,
            "dedicatedProxyRequirement": proxy_requirement,
            "dedicatedProxyPaths": [],
            "dedicatedProxyRequirementSatisfied": proxy_requirement == "None",
            "visualCollisionPolicy": visual_collision,
            "minimumLodCount": 2,
            "naniteAlternativeAllowed": True,
            "lodNaniteVerificationState": lod_state,
            "windBindingRequired": needs_wind,
            "windBindingVerificationState": wind_state,
            "scaleReviewState": scale_state,
            "minimumSpacingCm": spacing,
            "cullStartCm": cull_start,
            "cullEndCm": cull_end,
            "shadowCullDistanceCm": shadow,
            "categoryTechnicalContractComplete": True,
            "assetProductionReady": False,
        }
        expect(row, expected_row, f"category plan category {name}", issues)

    expect(plan["summary"], PLAN_SUMMARY, "category plan summary", issues)
    expect(plan["residualBlockers"], [
        "THIRTEEN_VISUAL_CATEGORY_BINDINGS_REQUIRE_APPROVED_ASSET_SELECTION_AND_LICENSE_RECORDS",
        "NINE_BLOCKING_CATEGORIES_REQUIRE_SEPARATE_SIMPLE_COLLISION_PROXY_ASSETS",
        "TWO_OVERLAP_CATEGORIES_REQUIRE_SEPARATE_NONBLOCKING_INTERACTION_PROXY_ASSETS",
        "TEN_WIND_REACTIVE_CATEGORIES_REQUIRE_VERIFIED_SHARED_WIND_MATERIAL_BINDINGS",
        "FOURTEEN_CATEGORIES_REQUIRE_FINAL_LOD_OR_NANITE_VERIFICATION",
        "TREE_CONIFER_YOUNG_SCALE_REVIEW_REMAINS_REQUIRED",
        "ALL_SIXTEEN_CATEGORIES_REQUIRE_HUMAN_VISUAL_OWNER_PERFORMANCE_AND_FINAL_USE_REVIEW",
    ], "category plan residual blockers", issues)
    expect(plan["claimBoundary"], PLAN_CLAIMS, "category plan claims", issues)

    if check_files and not issues:
        types_text = paths["environmentTypesHeader"].read_text(encoding="utf-8")
        enum_match = re.search(
            r"enum class EDiscGolfEnvironmentAssetCategory\s*:\s*uint8\s*\{([^}]*)\}",
            types_text, re.DOTALL,
        )
        observed_enum = [] if enum_match is None else [
            line.strip().rstrip(",")
            for line in enum_match.group(1).splitlines()
            if line.strip()
        ]
        expect(observed_enum, SLOTS, "environment category enum order", issues)

        data_source = paths["environmentDataAssetsSource"].read_text(encoding="utf-8")
        for name, mode, _proxy, spacing, start, end, _shadow, _wind in CATEGORY_DEFAULTS:
            pattern = (
                rf"MakeSlot\(EDiscGolfEnvironmentAssetCategory::{re.escape(name)},\s*"
                rf"EDiscGolfEnvironmentCollisionMode::{re.escape(mode)},\s*"
                rf"{spacing:.1f}f,\s*{start:.1f}f,\s*{end:.1f}f\)"
            )
            if re.search(pattern, data_source, re.DOTALL) is None:
                issues.append(f"native MakeSlot metadata differs for {name}")

        binder_source = paths["environmentAssetBinderSource"].read_text(encoding="utf-8")
        for token in [
            "Mesh->GetNumLODs() < 2 && !Mesh->GetNaniteSettings().bEnabled",
            "No wind/gust parameter was detected in assigned materials.",
            "CollisionProxyMesh must provide simple collision and cannot use complex-as-simple.",
            "Overlap slot policy requires an InteractionProxyMesh.",
            "Visual foliage carries collision; disable it before approval.",
        ]:
            if token not in binder_source:
                issues.append(f"asset binder validation seam missing: {token}")

        report = load_json(paths["assignedReadinessReport"])
        report_rows = {
            row.get("slot"): row for row in report.get("slots", []) if type(row) is dict
        }
        expect(list(report_rows), SLOTS, "assigned report category order", issues)
        for name in SLOTS:
            candidates = report_rows.get(name, {}).get("candidates", [])
            visual_paths = [
                item.get("asset_path") for item in candidates
                if type(item) is dict and item.get("asset_path")
            ]
            expected_paths = populated_by_name.get(name, {}).get("variantObjectPaths", [])
            expect(visual_paths, expected_paths, f"assigned report paths {name}", issues)
        observed_lods = {
            name: [item.get("lod_count") for item in report_rows[name]["candidates"]]
            for name in ["TreeConiferYoung", "Shrub", "RockLarge"]
        }
        expect(observed_lods, {
            "TreeConiferYoung": [4, 4, 4], "Shrub": [1], "RockLarge": [3],
        }, "assigned report LOD evidence", issues)
        for name in ["TreeConiferYoung", "Shrub"]:
            for item in report_rows[name]["candidates"]:
                expect(item.get("wind_parameters_detected"), False,
                       f"assigned report wind evidence {name}", issues)

    return issues, paths


def validate_policy(policy: Any, root: Path, *, check_files: bool) -> tuple[list[str], dict[str, Path]]:
    issues: list[str] = []
    paths: dict[str, Path] = {}
    trusted_policy = trusted_runtime_journal_required(policy)
    root_keys = {
        "schema", "schemaVersion", "session", "milestone", "candidateId",
        "authority", "state", "bindings", "candidate", "slotBinding",
        "shippingPresence", "proxyCoverage", "quarantine",
        "externalRuntimeEvidence", "claimBoundary",
    }
    if trusted_policy:
        root_keys.add("trustedRuntimeJournalContract")
    if not exact_keys(policy, root_keys, "policy", issues):
        return issues, paths
    expected_schema = TRUSTED_POLICY_SCHEMA if trusted_policy else LEGACY_POLICY_SCHEMA
    expected_version = 4 if trusted_policy else 3
    for key, expected in {
        "schema": expected_schema,
        "schemaVersion": expected_version,
        "session": 10,
        "milestone": "v0.5",
        "authority": "ADDITIVE_CANDIDATE_BOUND_TECHNICAL_EVIDENCE_NOT_VISUAL_PERFORMANCE_PROVENANCE_LEGAL_OWNER_OR_RELEASE_APPROVAL",
        "state": "SIXTEEN_CATEGORY_TECHNICAL_CONTRACT_ACCEPTED_ASSET_AUTHORING_AND_APPROVAL_PENDING",
    }.items():
        expect(policy[key], expected, f"policy.{key}", issues)
    if trusted_policy:
        expect(
            policy["trustedRuntimeJournalContract"],
            TRUSTED_RUNTIME_JOURNAL_CONTRACT,
            "policy.trustedRuntimeJournalContract",
            issues,
        )
    candidate_id = policy["candidateId"]
    if type(candidate_id) is not str or CANDIDATE_ID_RE.fullmatch(candidate_id) is None:
        issues.append("policy.candidateId must be a safe candidate identifier")

    expected_binding_keys = set(BINDING_KEYS)
    if trusted_policy:
        expected_binding_keys.add("freshUserDirValidation")
    if exact_keys(
        policy["bindings"], expected_binding_keys, "policy.bindings", issues
    ):
        for key, binding in policy["bindings"].items():
            path = verify_binding(
                binding, root, f"policy.bindings.{key}", issues, check_file=check_files
            )
            if path is not None:
                paths[key] = path

    candidate = policy["candidate"]
    if exact_keys(candidate, {
        "archiveRecoveryLocationToken", "archiveFileCount", "archiveBytes",
        "archiveCanonicalManifestSha256", "shippingExecutableRelativePath",
        "shippingExecutableBytes", "shippingExecutableSha256",
    }, "policy.candidate", issues):
        expect(
            candidate["archiveRecoveryLocationToken"],
            f"DGTOUR_PACKAGES/{candidate_id}/Windows",
            "policy.candidate.archiveRecoveryLocationToken", issues,
        )
        expect_positive_int(candidate["archiveFileCount"], "policy.candidate.archiveFileCount", issues)
        expect_positive_int(candidate["archiveBytes"], "policy.candidate.archiveBytes", issues)
        expect_sha256(
            candidate["archiveCanonicalManifestSha256"],
            "policy.candidate.archiveCanonicalManifestSha256", issues,
        )
        executable_path = safe_project_path(
            candidate["shippingExecutableRelativePath"], root,
            "policy.candidate.shippingExecutableRelativePath", issues,
        )
        if executable_path is not None and executable_path.suffix.casefold() != ".exe":
            issues.append("policy.candidate.shippingExecutableRelativePath must identify an executable")
        expect_positive_int(
            candidate["shippingExecutableBytes"],
            "policy.candidate.shippingExecutableBytes", issues,
        )
        expect_sha256(
            candidate["shippingExecutableSha256"],
            "policy.candidate.shippingExecutableSha256", issues,
        )

    slots = policy["slotBinding"]
    if exact_keys(slots, {
        "categoryCount", "categoryOrder", "populatedCategoryCount",
        "populatedCategories", "missingCategoryCount", "readyCategoryCount",
        "categoryTechnicalContractCount", "categoryTechnicalContractsComplete",
        "baseCullingMetadataCompleteCategoryCount",
        "qualityScalingMetadataCompleteTierCount",
        "blockingProxyRequiredCategoryCount",
        "blockingProxyRequirementSatisfiedCategoryCount",
        "interactionProxyRequiredCategoryCount",
        "interactionProxyRequirementSatisfiedCategoryCount",
        "windRequiredCategoryCount", "windVerifiedCategoryCount",
        "lodOrNaniteVerifiedCategoryCount", "lodOrNaniteUnresolvedCategoryCount",
        "allSixteenProductionSlotsBound", "technicalBindingScope",
    }, "policy.slotBinding", issues):
        expect(slots["categoryCount"], 16, "slot category count", issues)
        expect(slots["categoryOrder"], SLOTS, "slot category order", issues)
        expect(slots["populatedCategoryCount"], 3, "populated slot count", issues)
        expect(slots["populatedCategories"], POPULATED, "populated slots", issues)
        expect(slots["missingCategoryCount"], 13, "missing slot count", issues)
        expect(slots["readyCategoryCount"], 0, "ready slot count", issues)
        for key, expected in {
            "categoryTechnicalContractCount": 16,
            "categoryTechnicalContractsComplete": 16,
            "baseCullingMetadataCompleteCategoryCount": 16,
            "qualityScalingMetadataCompleteTierCount": 3,
            "blockingProxyRequiredCategoryCount": 9,
            "blockingProxyRequirementSatisfiedCategoryCount": 0,
            "interactionProxyRequiredCategoryCount": 2,
            "interactionProxyRequirementSatisfiedCategoryCount": 0,
            "windRequiredCategoryCount": 10,
            "windVerifiedCategoryCount": 0,
            "lodOrNaniteVerifiedCategoryCount": 2,
            "lodOrNaniteUnresolvedCategoryCount": 14,
        }.items():
            expect(slots[key], expected, f"slot binding {key}", issues)
        expect(slots["allSixteenProductionSlotsBound"], False, "all slots bound", issues)
        expect(slots["technicalBindingScope"],
               "SIXTEEN_CATEGORY_SOURCE_CONTRACT_COMPLETE_EXACT_THREE_PROVISIONAL_SLOT_FAMILIES_AND_FIVE_VARIANTS_ONLY",
               "slot binding scope", issues)

    presence = policy["shippingPresence"]
    expected_presence = {
        "requiredEnvironmentDataAssets": SHIPPING_ENVIRONMENT_ASSETS,
        "requiredBoundVisualVariants": VISUAL_VARIANTS,
        "requiredAbsentEnvironmentPcgAssets": SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS,
        "requiredInUfsManifest": True,
        "requiredInFinalContainerNamedInventory": True,
        "requiredAbsentFromUfsManifest": True,
        "requiredAbsentFromFinalContainerNamedInventory": True,
    }
    expect(presence, expected_presence, "policy.shippingPresence", issues)

    proxy = policy["proxyCoverage"]
    expected_proxy = {
        "collisionProfileId": "PineRidgeCompetitiveV2_Fixtures",
        "visualFoliageOwnsCompetitiveCollision": False,
        "competitiveCollisionOwner": "PROJECT_OWNED_AUTHORED_FIXTURES_AND_PROXIES",
        "holes": HOLES,
        "authoredBlockingProxyCount": 50,
        "authoredInteractionVolumeCount": 3,
        "hole1RuntimeStatisticsCorroboratesBlockingProxies": 14,
        "hole1RuntimeStatisticsCorroboratesInteractionVolumes": 1,
        "allThreeHolesLiveCollisionExerciseProven": False,
        "productionSlotProxyApproval": False,
    }
    expect(proxy, expected_proxy, "policy.proxyCoverage", issues)
    expect(policy["quarantine"], {
        "candidateContentCategory": "QUARANTINED_FAB_ROOTS",
        "forbiddenTokens": QUARANTINE_TOKENS,
        "authoritativeMatchCount": 0,
        "manifestDiagnosticMatchCount": 0,
    }, "policy.quarantine", issues)
    external = policy["externalRuntimeEvidence"]
    if exact_keys(external, {
        "manifestFileName", "manifestSha256", "externalUserDirToken",
        "requiredScreenshotCount", "selectedEnvironmentScreenshots",
        "technicalArtifactCollectionOnly", "actualRenderedRhiAttested",
        "visualQualityApproved",
    }, "policy.externalRuntimeEvidence", issues):
        manifest_path = safe_project_path(
            external["manifestFileName"], root,
            "policy.externalRuntimeEvidence.manifestFileName", issues,
        )
        if (
            manifest_path is not None
            and PurePosixPath(external["manifestFileName"]).name != external["manifestFileName"]
        ):
            issues.append("policy.externalRuntimeEvidence.manifestFileName must be a file name")
        expect_sha256(
            external["manifestSha256"],
            "policy.externalRuntimeEvidence.manifestSha256", issues,
        )
        if (
            type(external["externalUserDirToken"]) is not str
            or UUID_RE.fullmatch(external["externalUserDirToken"]) is None
        ):
            issues.append("policy.externalRuntimeEvidence.externalUserDirToken must be a canonical lowercase UUID")
        expect(external["requiredScreenshotCount"], 10, "external required screenshot count", issues)
        selected = external["selectedEnvironmentScreenshots"]
        if type(selected) is not list or len(selected) != len(SELECTED_ENVIRONMENT_MILESTONES):
            issues.append("policy external selected environment screenshot set differs")
        else:
            expect(
                [item.get("milestone") if type(item) is dict else None for item in selected],
                SELECTED_ENVIRONMENT_MILESTONES,
                "policy external selected environment screenshot milestones", issues,
            )
            selected_paths: list[str] = []
            for index, screenshot in enumerate(selected):
                label = f"policy.externalRuntimeEvidence.selectedEnvironmentScreenshots[{index}]"
                if not exact_keys(
                    screenshot,
                    {"milestone", "relativePath", "bytes", "sha256", "width", "height"},
                    label, issues,
                ):
                    continue
                path = safe_project_path(
                    screenshot["relativePath"], root, f"{label}.relativePath", issues,
                )
                if path is not None:
                    relative = screenshot["relativePath"]
                    if not relative.startswith("Screenshots/") or path.suffix.casefold() != ".png":
                        issues.append(f"{label}.relativePath must identify a Screenshots PNG")
                    selected_paths.append(relative.casefold())
                expect_positive_int(screenshot["bytes"], f"{label}.bytes", issues)
                expect_sha256(screenshot["sha256"], f"{label}.sha256", issues)
                for dimension in ("width", "height"):
                    expect_positive_int(screenshot[dimension], f"{label}.{dimension}", issues)
                if (
                    type(screenshot["width"]) is int
                    and type(screenshot["height"]) is int
                    and (screenshot["width"] < 1280 or screenshot["height"] < 720)
                ):
                    issues.append(f"{label} resolution must be at least 1280x720")
            if len(selected_paths) != len(set(selected_paths)):
                issues.append("policy external selected screenshot paths must be unique")
        expect(external["technicalArtifactCollectionOnly"], True, "external technical scope", issues)
        expect(external["actualRenderedRhiAttested"], False, "external rendered RHI claim", issues)
        expect(external["visualQualityApproved"], False, "external visual quality claim", issues)
    expect(policy["claimBoundary"], CLAIMS, "policy.claimBoundary", issues)
    return issues, paths


def _archive_summary(files: list[dict[str, Any]]) -> dict[str, Any]:
    canonical = hashlib.sha256(
        "".join(
            f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n"
            for item in files
        ).encode("utf-8")
    ).hexdigest().upper()
    return {
        "fileCount": len(files),
        "bytes": sum(item["bytes"] for item in files),
        "canonicalManifestSha256": canonical,
    }


def _png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("not a canonical PNG header")
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def _object_to_staged(path: str) -> str:
    package, _, _object = path.rpartition(".")
    if not package.startswith("/Game/"):
        raise ValueError(f"slot object path is outside /Game: {path}")
    return "DiscGolfTour/Content/" + package.removeprefix("/Game/") + ".uasset"


def validate_live(
    policy: dict[str, Any],
    root: Path,
    archive: Path,
    unrealpak: Path,
    external_run: Path,
    user_dir: Path | None = None,
) -> tuple[list[str], dict[str, Any]]:
    issues, paths = validate_policy(policy, root, check_files=True)
    observations: dict[str, Any] = {}
    if issues:
        return issues, observations
    candidate_id = policy["candidateId"]
    external = policy["externalRuntimeEvidence"]
    trusted_runtime_required = trusted_runtime_journal_required(policy)
    if trusted_runtime_required and user_dir is None:
        issues.append("trusted runtime journal policy requires an explicit UserDir")
    if not trusted_runtime_required and user_dir is not None:
        issues.append("historical v1 policy does not accept a trusted-journal UserDir")
    category_plan: dict[str, Any] = {}
    if "categoryTechnicalPlan" in paths:
        try:
            loaded_plan = load_json(paths["categoryTechnicalPlan"])
        except StrictJsonError as exc:
            issues.append(f"category technical plan failed strict JSON parsing: {exc}")
        else:
            if type(loaded_plan) is not dict:
                issues.append("category technical plan root must be an object")
            else:
                category_plan = loaded_plan
                plan_issues, _ = validate_category_plan(
                    category_plan, root, check_files=True
                )
                issues.extend(plan_issues)
    archive_token = PurePosixPath(policy["candidate"]["archiveRecoveryLocationToken"])
    if archive.name != archive_token.name or archive.parent.name != archive_token.parent.name:
        issues.append("archive is not the exact candidate Windows directory")
    if not archive.is_dir():
        issues.append("archive directory is missing")
    if not unrealpak.is_file() or unrealpak.name.casefold() != "unrealpak.exe":
        issues.append("explicit UnrealPak input is invalid")
    expected_external_name = f"{candidate_id}_{external['externalUserDirToken']}"
    if not external_run.is_dir() or external_run.name != expected_external_name:
        issues.append("external run is not the exact candidate/token directory")
    for boundary in [archive, external_run]:
        if boundary.exists() and boundary.is_symlink():
            issues.append(f"external boundary must not be a symlink: {boundary.name}")
    if issues:
        return issues, observations

    try:
        sys.path.insert(0, str(root / "Scripts"))
        from validate_dg_session19_candidate_content import (
            collect_archive, inventory_pak, inventory_utoc, parse_manifest,
        )
        archive_files = collect_archive(archive)
        ufs = parse_manifest(archive / "Manifest_UFSFiles_Win64.txt")
        container_names: list[str] = []
        container_summaries: list[dict[str, Any]] = []
        paks = archive / "DiscGolfTour/Content/Paks"
        for path in sorted([*paks.glob("*.pak"), *paks.glob("*.utoc")]):
            if path.suffix.casefold() == ".pak":
                names, summary = inventory_pak(unrealpak, path)
            else:
                names, summary = inventory_utoc(unrealpak, path)
            container_names.extend(names)
            container_summaries.append({"path": path.relative_to(archive).as_posix(), **summary})
    except Exception as exc:
        issues.append(f"candidate archive inventory failed closed: {exc.__class__.__name__}: {exc}")
        return issues, observations

    archive_summary = _archive_summary(archive_files)
    expect(archive_summary, {
        "fileCount": policy["candidate"]["archiveFileCount"],
        "bytes": policy["candidate"]["archiveBytes"],
        "canonicalManifestSha256": policy["candidate"]["archiveCanonicalManifestSha256"],
    }, "live archive summary", issues)
    executable = archive / policy["candidate"]["shippingExecutableRelativePath"]
    if not executable.is_file():
        issues.append("bound Shipping executable is missing")
    else:
        expect(executable.stat().st_size, policy["candidate"]["shippingExecutableBytes"],
               "Shipping executable bytes", issues)
        expect(sha256(executable), policy["candidate"]["shippingExecutableSha256"],
               "Shipping executable SHA-256", issues)

    ufs_fold = {item.casefold() for item in ufs}
    container_fold = {item.casefold() for item in container_names}
    required = [*SHIPPING_ENVIRONMENT_ASSETS, *VISUAL_VARIANTS]
    for identity in required:
        if identity.casefold() not in ufs_fold:
            issues.append(f"required environment identity missing from UFS manifest: {identity}")
        if identity.casefold() not in container_fold:
            issues.append(f"required environment identity missing from final container: {identity}")
    for identity in SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS:
        if identity.casefold() in ufs_fold:
            issues.append(f"Shipping-excluded environment PCG identity survived in UFS manifest: {identity}")
        if identity.casefold() in container_fold:
            issues.append(f"Shipping-excluded environment PCG identity survived in final container: {identity}")

    authoritative = [item["path"] for item in archive_files] + container_names
    quarantine_hits = sorted({
        identity for identity in authoritative
        if any(token.casefold() in identity.casefold() for token in QUARANTINE_TOKENS)
    })
    if quarantine_hits:
        issues.append(f"quarantined identities survived in candidate: {quarantine_hits[:10]}")

    content = load_json(paths["candidateContentAudit"])
    for key, expected in {
        "schema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
        "runId": candidate_id,
        "state": "PASS_BOUNDED_STAGED_CONTENT_AUDIT",
        "issues": [],
    }.items():
        expect(content.get(key), expected, f"candidate content.{key}", issues)
    expect(content.get("archive", {}).get("canonicalManifestSha256"),
           policy["candidate"]["archiveCanonicalManifestSha256"],
           "candidate content archive SHA-256", issues)
    quarantine = content.get("forbiddenIdentityCategories", {}).get("QUARANTINED_FAB_ROOTS")
    expect(quarantine, {
        "passed": True, "matchCount": 0, "matches": [],
        "manifestUfsDiagnosticMatchCount": 0,
        "manifestUfsDiagnosticMatches": [],
    }, "candidate content quarantine category", issues)
    release_boundary = content.get("releaseBoundary", {})
    for key, expected in {
        "freshInstallGameplayAcceptancePerformed": False,
        "provenanceClassificationPerformed": False,
        "legalOrVisualApprovalPerformed": False,
        "blockerClosed": False,
        "releaseReady": False,
    }.items():
        expect(release_boundary.get(key), expected, f"candidate content boundary.{key}", issues)

    session10 = load_json(paths["session10EnvironmentContract"])
    for key, expected in {
        "schema": "DiscGolfTour.Session10EnvironmentContract.v1",
        "schema_version": 1,
        "audit_scope": "TECHNICAL_ENVIRONMENT_SOURCE_AND_EDITOR_AUTHORING_GATE_NOT_PRODUCTION_ART_OR_RELEASE_APPROVAL",
        "authority": "ENVIRONMENT_PRESENTATION_ONLY_NO_GAMEPLAY_PHYSICS_RULES_OR_RELEASE_AUTHORITY",
    }.items():
        expect(session10.get(key), expected, f"Session 10 contract.{key}", issues)
    source_inventory = session10.get("source_environment_inventory", {})
    expect(source_inventory.get("expected_file_count"), 4,
           "Session 10 environment source asset count", issues)
    expect(source_inventory.get("production_environment_ready"), False,
           "Session 10 production environment readiness", issues)
    expect(source_inventory.get("release_use_allowed"), False,
           "Session 10 environment release use", issues)
    readiness = session10.get("assigned_readiness_contract", {})
    for key, expected in {
        "readiness_evaluated": True,
        "structurally_complete": True,
        "production_ready": False,
        "expected_slot_count": 16,
        "expected_populated_slot_count": 3,
        "expected_missing_slot_count": 13,
        "expected_ready_slot_count": 0,
    }.items():
        expect(readiness.get(key), expected, f"Session 10 readiness.{key}", issues)
    runtime_pcg = session10.get("runtime_pcg_contract", {})
    for key, expected in {
        "generation_trigger": "GenerateOnDemand",
        "expected_runtime_generate_forest_callsite_count": 0,
        "expected_generated_instance_count": 0,
        "authored_environment_fallback_active": True,
        "release_use_allowed": False,
    }.items():
        expect(runtime_pcg.get(key), expected, f"Session 10 runtime PCG.{key}", issues)
    presentation_authority = session10.get("presentation_contract", {})
    for key, expected in {
        "collision_profile_id": "PineRidgeCompetitiveV2_Fixtures",
        "collision_invariant_across_quality": True,
        "assets_ready_expected": False,
    }.items():
        expect(presentation_authority.get(key), expected,
               f"Session 10 presentation authority.{key}", issues)

    verification = load_json(paths["candidateVerification"])
    expect(verification.get("runId"), candidate_id, "candidate verification id", issues)
    expect(verification.get("innerShippingExecutable"), {
        "relativePath": policy["candidate"]["shippingExecutableRelativePath"],
        "bytes": policy["candidate"]["shippingExecutableBytes"],
        "sha256": policy["candidate"]["shippingExecutableSha256"],
    }, "candidate verification executable", issues)
    expect(verification.get("releaseReady"), False, "candidate verification release", issues)

    slot_report = load_json(paths["assignedSlotReport"])
    for key, expected in {
        "schema": "disc_golf_environment_asset_binding_report",
        "schema_version": 2,
        "automatic_apply_performed": False,
        "readiness_evaluated": True,
        "structurally_complete": True,
        "production_ready": False,
        "report_kind": "assigned_readiness",
    }.items():
        expect(slot_report.get(key), expected, f"slot report.{key}", issues)
    report_slots = slot_report.get("slots", [])
    expect([item.get("slot") for item in report_slots if type(item) is dict],
           SLOTS, "slot report order", issues)
    observed_populated: list[dict[str, Any]] = []
    for item in report_slots:
        if type(item) is not dict:
            continue
        candidates = item.get("candidates", [])
        populated_paths = [
            candidate.get("asset_path") for candidate in candidates
            if type(candidate) is dict and candidate.get("asset_path")
        ]
        for candidate in candidates:
            if type(candidate) is dict and candidate.get("asset_path"):
                if candidate.get("collision_proxy_path") != "":
                    issues.append(f"slot unexpectedly has a collision proxy path: {item.get('slot')}")
                if candidate.get("interaction_proxy_path") != "":
                    issues.append(f"slot unexpectedly has an interaction proxy path: {item.get('slot')}")
        if populated_paths:
            expected_slot = next(
                (value for value in POPULATED if value["slot"] == item.get("slot")), None
            )
            observed_populated.append({
                "slot": item.get("slot"), "variantObjectPaths": populated_paths,
            })
            if expected_slot is None:
                issues.append(f"unexpected populated slot: {item.get('slot')}")
            else:
                expect(populated_paths, expected_slot["variantObjectPaths"],
                       f"slot variants {item.get('slot')}", issues)
    expect(len(observed_populated), 3, "observed populated slot count", issues)
    expect(sum(len(item["variantObjectPaths"]) for item in observed_populated),
           5, "observed slot variant count", issues)

    asset_set_bytes = paths["environmentAssetSet"].read_bytes()
    for value in POPULATED:
        category = f"EDiscGolfEnvironmentAssetCategory::{value['slot']}".encode("ascii")
        mode = f"EDiscGolfEnvironmentCollisionMode::{value['collisionMode']}".encode("ascii")
        if category not in asset_set_bytes:
            issues.append(f"asset set lacks serialized category marker: {value['slot']}")
        if mode not in asset_set_bytes:
            issues.append(f"asset set lacks serialized collision-mode marker: {value['collisionMode']}")
        for object_path in value["variantObjectPaths"]:
            package_path = object_path.rpartition(".")[0].encode("ascii")
            if package_path not in asset_set_bytes:
                issues.append(f"asset set lacks serialized variant path: {object_path}")
            if _object_to_staged(object_path).casefold() not in container_fold:
                issues.append(f"serialized slot variant is absent from container: {object_path}")
    for marker in [b"CollisionProxyMesh", b"InteractionProxyMesh"]:
        if marker not in asset_set_bytes:
            issues.append(f"asset set lacks required proxy data field: {marker.decode()}")

    content_holes = {
        item.get("path"): item
        for item in content.get("requiredThreeHoleData", {}).get("files", [])
        if type(item) is dict
    }
    observed_holes: list[dict[str, Any]] = []
    for expected_hole in HOLES:
        number = expected_hole["holeNumber"]
        document = load_json(paths[f"hole{number}Definition"])
        trees = document.get("trees", [])
        fixtures = document.get("collisionFixtures", [])
        blocking = sorted(item.get("type") for item in fixtures if item.get("type") != "DenseGrass")
        interactions = sorted(item.get("type") for item in fixtures if item.get("type") == "DenseGrass")
        observed = {
            "holeNumber": number,
            "treeBlockingProxies": len(trees),
            "fixtureBlockingProxies": len(blocking),
            "interactionVolumes": len(interactions),
            "blockingFixtureTypes": blocking,
            "interactionFixtureTypes": interactions,
        }
        observed_holes.append(observed)
        expect(observed, expected_hole, f"hole {number} proxy definition coverage", issues)
        staged_data = f"DiscGolfTour/Data/PineRidgeHole{number}.json"
        candidate_record = content_holes.get(staged_data, {})
        expect(candidate_record.get("presentInArchive"), True,
               f"hole {number} present in archive", issues)
        expect(candidate_record.get("presentInNonUfsManifest"), True,
               f"hole {number} present in NonUFS", issues)
        expect(candidate_record.get("sha256"), sha256(paths[f"hole{number}Definition"]),
               f"hole {number} candidate/source hash", issues)

    presentation = load_json(paths["presentationContract"])
    expect(presentation.get("collisionProfileId"), "PineRidgeCompetitiveV2_Fixtures",
           "presentation collision profile", issues)
    expect(presentation.get("collisionInvariantAcrossQuality"), True,
           "presentation collision invariant", issues)
    expect(presentation.get("assetsReady"), False, "presentation assets ready", issues)
    for tier in presentation.get("qualityTiers", []):
        expect(tier.get("affectsCollision"), False,
               f"presentation tier {tier.get('id')} collision", issues)

    statistics = load_json(paths["hole1RuntimeStatistics"])
    for key, expected in {
        "holeNumber": 1,
        "visualAcceptance": "PENDING_FINAL_FAB_IMPORT",
        "assetMode": "PROVISIONAL_CC0_AND_ENGINE_CONTENT",
        "collisionProxies": 14,
        "interactionVolumes": 1,
        "pcgGeneratedInstances": 0,
        "environmentActorsTick": False,
        "collisionQualityInvariant": True,
    }.items():
        expect(statistics.get(key), expected, f"hole1 statistics.{key}", issues)

    external_validation = load_json(paths["externalTechnicalValidation"])
    expect(external_validation.get("candidateId"), candidate_id,
           "external validation candidate", issues)
    expect(external_validation.get("state"),
           "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
           "external validation state", issues)
    expect(external_validation.get("failures"), [], "external validation failures", issues)
    expect(external_validation.get("manifest", {}).get("sha256"),
           policy["externalRuntimeEvidence"]["manifestSha256"],
           "external validation manifest hash", issues)
    expect(external_validation.get("claimBoundary", {}).get("releaseReady"), False,
           "external validation release claim", issues)

    manifest_path = external_run / policy["externalRuntimeEvidence"]["manifestFileName"]
    if not manifest_path.is_file():
        issues.append("external technical-evidence manifest is missing")
        return issues, observations
    expect(sha256(manifest_path), policy["externalRuntimeEvidence"]["manifestSha256"],
           "external manifest SHA-256", issues)
    manifest = load_json(manifest_path)
    runtime_journal_binding: dict[str, Any] | None = None
    if trusted_runtime_required:
        launch_path = external_run / "launch-record.json"
        if not launch_path.is_file():
            issues.append("trusted external launch record is missing")
            launch: dict[str, Any] = {}
        else:
            launch = load_json(launch_path)
        if user_dir is not None and executable.is_file():
            runtime_journal_binding, runtime_issues = validate_trusted_runtime_journal(
                root=root,
                archive=archive,
                executable=executable,
                user_dir=user_dir,
                manifest_path=manifest_path,
                launch_path=launch_path,
                manifest=manifest,
                launch=launch,
                policy=policy,
            )
            issues.extend(runtime_issues)
            if runtime_journal_binding is not None:
                fresh = load_json(paths["freshUserDirValidation"])
                expected_fresh_binding = {
                    "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
                    "userDirRelativePath": runtime_journal_binding[
                        "userDirRelativePath"
                    ],
                    "bytes": runtime_journal_binding["bytes"],
                    "sha256": runtime_journal_binding["sha256"],
                    "captureNonce": runtime_journal_binding["captureNonce"],
                    "roundId": runtime_journal_binding["roundId"],
                    "launchRecordSha256": runtime_journal_binding[
                        "launchRecordSha256"
                    ],
                    "technicalEvidenceManifestSha256": runtime_journal_binding[
                        "technicalEvidenceManifestSha256"
                    ],
                    "executableSha256": runtime_journal_binding[
                        "executableSha256"
                    ],
                    "archiveManifestSha256": runtime_journal_binding[
                        "archiveManifestSha256"
                    ],
                    "validationState": runtime_journal_binding[
                        "validationState"
                    ],
                }
                if (
                    type(fresh) is not dict
                    or fresh.get("candidateId") != candidate_id
                    or fresh.get("userDirToken") != external["externalUserDirToken"]
                    or fresh.get("state") != "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
                    or fresh.get("failures") != []
                    or fresh.get("runtimeJournalBinding")
                    != expected_fresh_binding
                ):
                    issues.append(
                        "fresh UserDir trusted runtime journal binding differs"
                    )
        else:
            issues.extend(external_schema_contract_issues(policy, manifest, launch))
    else:
        for key, expected in {
            "schema": LEGACY_MANIFEST_SCHEMA,
            "schemaVersion": 1,
        }.items():
            expect(manifest.get(key), expected, f"external manifest.{key}", issues)
    for key, expected in {
        "state": "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION",
    }.items():
        expect(manifest.get(key), expected, f"external manifest.{key}", issues)
    candidate_manifest = manifest.get("candidate", {})
    expect(candidate_manifest.get("candidateId"), candidate_id,
           "external manifest candidate", issues)
    expect(candidate_manifest.get("archivePreserved"), True,
           "external manifest archive preservation", issues)
    expect(candidate_manifest.get("archiveBefore"), archive_summary,
           "external manifest archive before", issues)
    expect(candidate_manifest.get("archiveAfter"), archive_summary,
           "external manifest archive after", issues)
    expect(candidate_manifest.get("observedExeSha256"),
           policy["candidate"]["shippingExecutableSha256"],
           "external manifest executable", issues)
    expect(manifest.get("userDir", {}).get("token"), external["externalUserDirToken"],
           "external manifest user-dir token", issues)
    expect(manifest.get("userDir", {}).get("emptyBeforeLaunch"), True,
           "external manifest empty user-dir", issues)
    expect(manifest.get("process", {}).get("exitCode"), 0,
           "external process exit", issues)
    expect(manifest.get("process", {}).get("timedOut"), False,
           "external process timeout", issues)
    external_claims = manifest.get("claimBoundary", {})
    for key, expected in {
        "technicalArtifactCollectionOnly": True,
        "humanPlayFeelApproval": False,
        "legalApproval": False,
        "manualGameplayAcceptance": False,
        "releaseApproval": False,
        "releaseReady": False,
        "visualProductApproval": False,
    }.items():
        expect(external_claims.get(key), expected, f"external claims.{key}", issues)

    screenshot_rows = manifest.get("evidence", {}).get("screenshots", [])
    expect(
        len(screenshot_rows), external["requiredScreenshotCount"],
        "external screenshot count", issues,
    )
    screenshot_by_path = {
        row.get("relativePath"): row for row in screenshot_rows if type(row) is dict
    }
    for row in screenshot_rows:
        relative = row.get("relativePath") if type(row) is dict else None
        if type(relative) is not str:
            issues.append("external screenshot row lacks a relative path")
            continue
        pure = PurePosixPath(relative)
        if pure.is_absolute() or ".." in pure.parts or "\\" in relative or ":" in relative:
            issues.append(f"external screenshot path is unsafe: {relative!r}")
            continue
        path = external_run / relative
        if not path.is_file() or path.is_symlink():
            issues.append(f"external screenshot is missing or linked: {relative}")
            continue
        expect(path.stat().st_size, row.get("bytes"), f"screenshot bytes {relative}", issues)
        expect(sha256(path), row.get("sha256"), f"screenshot hash {relative}", issues)
        try:
            dimensions = _png_dimensions(path)
        except (OSError, ValueError) as exc:
            issues.append(f"external screenshot PNG invalid: {relative}: {exc}")
        else:
            expect(dimensions, (row.get("width"), row.get("height")),
                   f"screenshot dimensions {relative}", issues)
        expect(row.get("modifiedWithinRunWindow"), True,
               f"screenshot run window {relative}", issues)
    selected_screenshots = external["selectedEnvironmentScreenshots"]
    for selected in selected_screenshots:
        row = screenshot_by_path.get(selected["relativePath"], {})
        for key in ["relativePath", "bytes", "sha256", "width", "height"]:
            expect(row.get(key), selected[key],
                   f"selected screenshot {selected['milestone']}.{key}", issues)

    observations = {
        "archive": archive_summary,
        "containerInventories": container_summaries,
        "shippingPresence": {
            "environmentDataAssetCount": len(SHIPPING_ENVIRONMENT_ASSETS),
            "boundVisualVariantCount": len(VISUAL_VARIANTS),
            "shippingExcludedEnvironmentPcgAssetCount": len(SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS),
            "allRequiredInUfsManifest": all(item.casefold() in ufs_fold for item in required),
            "allRequiredInFinalContainerNamedInventory": all(
                item.casefold() in container_fold for item in required
            ),
            "allShippingExcludedEnvironmentPcgAbsentFromUfsManifest": all(
                item.casefold() not in ufs_fold
                for item in SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS
            ),
            "allShippingExcludedEnvironmentPcgAbsentFromFinalContainerNamedInventory": all(
                item.casefold() not in container_fold
                for item in SHIPPING_EXCLUDED_ENVIRONMENT_ASSETS
            ),
        },
        "slotBinding": {
            "categoryCount": 16,
            "populatedCategoryCount": 3,
            "boundVariantCount": 5,
            "missingCategoryCount": 13,
            "readyCategoryCount": 0,
            "populatedCategories": POPULATED,
            "allSixteenProductionSlotsBound": False,
        },
        "categoryTechnicalPlan": {
            "categoryTechnicalContractCount": category_plan["summary"]["categoryTechnicalContractCount"],
            "categoryTechnicalContractsComplete": category_plan["summary"]["categoryTechnicalContractsComplete"],
            "baseCullingMetadataCompleteCategoryCount": category_plan["summary"]["baseCullingMetadataCompleteCategoryCount"],
            "qualityScalingMetadataCompleteTierCount": category_plan["summary"]["qualityScalingMetadataCompleteTierCount"],
            "blockingProxyRequiredCategoryCount": category_plan["summary"]["blockingProxyRequiredCategoryCount"],
            "blockingProxyRequirementSatisfiedCategoryCount": category_plan["summary"]["blockingProxyRequirementSatisfiedCategoryCount"],
            "interactionProxyRequiredCategoryCount": category_plan["summary"]["interactionProxyRequiredCategoryCount"],
            "interactionProxyRequirementSatisfiedCategoryCount": category_plan["summary"]["interactionProxyRequirementSatisfiedCategoryCount"],
            "windRequiredCategoryCount": category_plan["summary"]["windRequiredCategoryCount"],
            "windVerifiedCategoryCount": category_plan["summary"]["windVerifiedCategoryCount"],
            "lodOrNaniteVerifiedCategoryCount": category_plan["summary"]["lodOrNaniteVerifiedCategoryCount"],
            "lodOrNaniteUnresolvedCategoryCount": category_plan["summary"]["lodOrNaniteUnresolvedCategoryCount"],
            "allSixteenProductionSlotsReady": category_plan["summary"]["allSixteenProductionSlotsReady"],
            "claimBoundary": category_plan["claimBoundary"],
        },
        "proxyCoverage": {
            "holes": observed_holes,
            "authoredBlockingProxyCount": 50,
            "authoredInteractionVolumeCount": 3,
            "hole1RuntimeStatisticsCorroborated": True,
            "allThreeHolesLiveCollisionExerciseProven": False,
            "productionSlotProxyApproval": False,
        },
        "quarantine": {
            "authoritativeMatchCount": len(quarantine_hits),
            "matches": quarantine_hits,
            "candidateContentCategoryPassed": quarantine == {
                "passed": True, "matchCount": 0, "matches": [],
                "manifestUfsDiagnosticMatchCount": 0,
                "manifestUfsDiagnosticMatches": [],
            },
        },
        "externalRuntimeEvidence": {
            "manifestSha256": sha256(manifest_path),
            "screenshotCount": len(screenshot_rows),
            "selectedEnvironmentScreenshots": selected_screenshots,
            "technicalArtifactCollectionOnly": True,
            "actualRenderedRhiAttested": False,
            "visualQualityApproved": False,
        },
    }
    if runtime_journal_binding is not None:
        observations["externalRuntimeEvidence"]["runtimeCheckpointJournal"] = (
            runtime_journal_binding
        )
    return issues, observations


Mutation = tuple[str, Callable[[dict[str, Any]], None]]


def runtime_contract_self_tests(root: Path) -> tuple[int, list[str]]:
    failures: list[str] = []
    count = 0
    legacy_policy = {"schema": LEGACY_POLICY_SCHEMA, "schemaVersion": 3}
    trusted_policy = {
        "schema": TRUSTED_POLICY_SCHEMA,
        "schemaVersion": 4,
        "trustedRuntimeJournalContract": copy.deepcopy(
            TRUSTED_RUNTIME_JOURNAL_CONTRACT
        ),
    }
    legacy_manifest = {"schema": LEGACY_MANIFEST_SCHEMA, "schemaVersion": 1}
    legacy_launch = {"schema": LEGACY_LAUNCH_SCHEMA, "schemaVersion": 1}
    trusted_manifest = {"schema": TRUSTED_MANIFEST_SCHEMA, "schemaVersion": 2}
    trusted_launch = {"schema": TRUSTED_LAUNCH_SCHEMA, "schemaVersion": 2}
    cases = [
        ("v1", legacy_policy, legacy_manifest, legacy_launch, True),
        ("v2", trusted_policy, trusted_manifest, trusted_launch, True),
        ("v1-policy-v2-evidence", legacy_policy, trusted_manifest, trusted_launch, False),
        ("v2-policy-v1-evidence", trusted_policy, legacy_manifest, legacy_launch, False),
        ("v1-manifest-v2-launch", legacy_policy, legacy_manifest, trusted_launch, False),
        ("v2-manifest-v1-launch", trusted_policy, trusted_manifest, legacy_launch, False),
        ("v1-policy-mixed-evidence", legacy_policy, trusted_manifest, legacy_launch, False),
        ("v2-policy-mixed-evidence", trusted_policy, legacy_manifest, trusted_launch, False),
    ]
    for name, selected, manifest, launch, should_pass in cases:
        count += 1
        actual_pass = not external_schema_contract_issues(selected, manifest, launch)
        if actual_pass is not should_pass:
            failures.append(f"runtime schema contract case differed: {name}")
    count += 1
    try:
        sys.path.insert(0, str(root / "Scripts"))
        from validate_dg_session19_external_technical_evidence import _self_test

        authoritative = _self_test()
        if (
            authoritative.get("state") != "PASS"
            or type(authoritative.get("testsPassed")) is not int
            or authoritative.get("testsPassed", 0) < 25
        ):
            failures.append(
                "authoritative v1/v2/raw-journal forgery self-test did not pass"
            )
    except Exception as exc:
        failures.append(
            "authoritative raw-journal self-test failed closed: "
            f"{exc.__class__.__name__}"
        )
    return count, failures


def run_self_test(
    policy: dict[str, Any], root: Path, archive: Path, unrealpak: Path,
    external_run: Path, user_dir: Path | None = None,
) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    baseline, _ = validate_live(
        policy, root, archive, unrealpak, external_run, user_dir
    )
    if baseline:
        failures.append("baseline failed: " + "; ".join(baseline[:5]))
    mutations: list[Mutation] = [
        ("extra", lambda d: d.__setitem__("releaseApproved", True)),
        ("schema", lambda d: d.__setitem__("schema", "wrong")),
        ("version-bool", lambda d: d.__setitem__("schemaVersion", True)),
        ("candidate", lambda d: d.__setitem__("candidateId", "wrong")),
        ("candidate-recovery-token", lambda d: d["candidate"].__setitem__("archiveRecoveryLocationToken", "DGTOUR_PACKAGES/wrong/Windows")),
        ("authority", lambda d: d.__setitem__("authority", "OWNER_APPROVAL")),
        ("binding-hash", lambda d: d["bindings"]["assignedSlotReport"].__setitem__("sha256", "g" * 64)),
        ("archive-hash", lambda d: d["candidate"].__setitem__("archiveCanonicalManifestSha256", "g" * 64)),
        ("exe-hash", lambda d: d["candidate"].__setitem__("shippingExecutableSha256", "g" * 64)),
        ("slot-count", lambda d: d["slotBinding"].__setitem__("categoryCount", 15)),
        ("slot-order", lambda d: d["slotBinding"]["categoryOrder"].reverse()),
        ("populated-count", lambda d: d["slotBinding"].__setitem__("populatedCategoryCount", 16)),
        ("populated-missing", lambda d: d["slotBinding"]["populatedCategories"].pop()),
        ("variant-path", lambda d: d["slotBinding"]["populatedCategories"][0]["variantObjectPaths"].__setitem__(0, "/Game/Wrong.Wrong")),
        ("proxy-paths-promoted", lambda d: d["slotBinding"]["populatedCategories"][0].__setitem__("dedicatedProxyPathsComplete", True)),
        ("technical-contract-count", lambda d: d["slotBinding"].__setitem__("categoryTechnicalContractCount", 15)),
        ("culling-contract-count", lambda d: d["slotBinding"].__setitem__("baseCullingMetadataCompleteCategoryCount", 15)),
        ("blocking-proxy-satisfied-overclaim", lambda d: d["slotBinding"].__setitem__("blockingProxyRequirementSatisfiedCategoryCount", 9)),
        ("wind-verified-overclaim", lambda d: d["slotBinding"].__setitem__("windVerifiedCategoryCount", 10)),
        ("lod-verified-overclaim", lambda d: d["slotBinding"].__setitem__("lodOrNaniteVerifiedCategoryCount", 16)),
        ("production-slot-promoted", lambda d: d["slotBinding"].__setitem__("allSixteenProductionSlotsBound", True)),
        ("ufs-not-required", lambda d: d["shippingPresence"].__setitem__("requiredInUfsManifest", False)),
        ("container-not-required", lambda d: d["shippingPresence"].__setitem__("requiredInFinalContainerNamedInventory", False)),
        ("pcg-ufs-absence-not-required", lambda d: d["shippingPresence"].__setitem__("requiredAbsentFromUfsManifest", False)),
        ("pcg-container-absence-not-required", lambda d: d["shippingPresence"].__setitem__("requiredAbsentFromFinalContainerNamedInventory", False)),
        ("proxy-count", lambda d: d["proxyCoverage"].__setitem__("authoredBlockingProxyCount", 49)),
        ("proxy-owner", lambda d: d["proxyCoverage"].__setitem__("competitiveCollisionOwner", "VISUAL_FOLIAGE")),
        ("live-all-holes-overclaim", lambda d: d["proxyCoverage"].__setitem__("allThreeHolesLiveCollisionExerciseProven", True)),
        ("proxy-approval-overclaim", lambda d: d["proxyCoverage"].__setitem__("productionSlotProxyApproval", True)),
        ("quarantine-token", lambda d: d["quarantine"]["forbiddenTokens"].pop()),
        ("quarantine-count", lambda d: d["quarantine"].__setitem__("authoritativeMatchCount", 1)),
        ("manifest-hash", lambda d: d["externalRuntimeEvidence"].__setitem__("manifestSha256", "g" * 64)),
        ("external-token", lambda d: d["externalRuntimeEvidence"].__setitem__("externalUserDirToken", "wrong")),
        ("screenshot-hash", lambda d: d["externalRuntimeEvidence"]["selectedEnvironmentScreenshots"][0].__setitem__("sha256", "g" * 64)),
        ("rhi-overclaim", lambda d: d["externalRuntimeEvidence"].__setitem__("actualRenderedRhiAttested", True)),
        ("visual-overclaim", lambda d: d["externalRuntimeEvidence"].__setitem__("visualQualityApproved", True)),
        ("claim-all-slots", lambda d: d["claimBoundary"].__setitem__("allSixteenProductionSlotsBound", True)),
        ("claim-proxies-authored", lambda d: d["claimBoundary"].__setitem__("allRequiredDedicatedProxiesAuthored", True)),
        ("claim-wind-bound", lambda d: d["claimBoundary"].__setitem__("allRequiredWindMaterialsBound", True)),
        ("claim-lod-verified", lambda d: d["claimBoundary"].__setitem__("allRequiredLodOrNaniteVerified", True)),
        ("claim-rhi", lambda d: d["claimBoundary"].__setitem__("actualRenderedRhiAccepted", True)),
        ("claim-visual", lambda d: d["claimBoundary"].__setitem__("visualQualityAccepted", True)),
        ("claim-performance", lambda d: d["claimBoundary"].__setitem__("performanceApproval", True)),
        ("claim-provenance", lambda d: d["claimBoundary"].__setitem__("provenanceApproval", True)),
        ("claim-legal", lambda d: d["claimBoundary"].__setitem__("legalApproval", True)),
        ("claim-owner", lambda d: d["claimBoundary"].__setitem__("ownerApproval", True)),
        ("claim-release", lambda d: d["claimBoundary"].__setitem__("releaseReady", True)),
    ]
    for name, mutation in mutations:
        candidate = copy.deepcopy(policy)
        mutation(candidate)
        issues, _ = validate_policy(candidate, root, check_files=False)
        if not issues:
            failures.append(f"mutation survived: {name}")
    plan_path = root / policy["bindings"]["categoryTechnicalPlan"]["path"]
    try:
        plan = load_json(plan_path)
    except StrictJsonError as exc:
        failures.append(f"category plan self-test baseline load failed: {exc}")
        plan = {}
    plan_mutations: list[Mutation] = [
        ("plan-extra", lambda d: d.__setitem__("releaseApproved", True)),
        ("plan-order", lambda d: d["categoryOrder"].reverse()),
        ("plan-category-mode", lambda d: d["categories"][0].__setitem__("collisionMode", "None")),
        ("plan-proxy-overclaim", lambda d: d["categories"][0].__setitem__("dedicatedProxyRequirementSatisfied", True)),
        ("plan-lod", lambda d: d["categories"][6].__setitem__("minimumLodCount", 1)),
        ("plan-wind", lambda d: d["categories"][6].__setitem__("windBindingVerificationState", "VERIFIED")),
        ("plan-cull", lambda d: d["categories"][8].__setitem__("cullEndCm", 500000.0)),
        ("plan-ready", lambda d: d["categories"][13].__setitem__("assetProductionReady", True)),
        ("plan-summary-ready", lambda d: d["summary"].__setitem__("productionReadyCategoryCount", 16)),
        ("plan-claim-release", lambda d: d["claimBoundary"].__setitem__("releaseReady", True)),
    ]
    if plan:
        baseline_plan, _ = validate_category_plan(plan, root, check_files=False)
        if baseline_plan:
            failures.append("category plan baseline failed: " + "; ".join(baseline_plan[:5]))
        for name, mutation in plan_mutations:
            candidate_plan = copy.deepcopy(plan)
            mutation(candidate_plan)
            plan_issues, _ = validate_category_plan(
                candidate_plan, root, check_files=False
            )
            if not plan_issues:
                failures.append(f"category plan mutation survived: {name}")
    strict_cases = [
        ("duplicate-key", '{"schema":1,"schema":2}'),
        ("nan", '{"value":NaN}'),
        ("overflow", '{"value":1e309}'),
    ]
    for name, payload in strict_cases:
        try:
            value = json.loads(payload, object_pairs_hook=_pairs, parse_constant=_constant)
            _validate_numbers(value)
        except StrictJsonError:
            continue
        failures.append(f"strict JSON case survived: {name}")

    alternate = copy.deepcopy(policy)
    alternate_id = "SELFTEST_WindowsShipping_20990101T000000Z_0123456789ab"
    alternate["candidateId"] = alternate_id
    alternate["candidate"].update({
        "archiveRecoveryLocationToken": f"DGTOUR_PACKAGES/{alternate_id}/Windows",
        "archiveFileCount": 41,
        "archiveBytes": 2345678901,
        "archiveCanonicalManifestSha256": "A" * 64,
        "shippingExecutableRelativePath": "DiscGolfTour/Binaries/Win64/DiscGolfTour-Alternate-Win64-Shipping.exe",
        "shippingExecutableBytes": 172598400,
        "shippingExecutableSha256": "B" * 64,
    })
    alternate_external = alternate["externalRuntimeEvidence"]
    alternate_external.update({
        "manifestSha256": "C" * 64,
        "externalUserDirToken": "11111111-2222-3333-8444-555555555555",
    })
    for index, screenshot in enumerate(alternate_external["selectedEnvironmentScreenshots"], 1):
        screenshot.update({
            "relativePath": f"Screenshots/alternate_environment_{index}.png",
            "bytes": 3000000 + index,
            "sha256": f"{index:X}" * 64,
            "width": 2048,
            "height": 1152,
        })
    alternate_issues, _ = validate_policy(alternate, root, check_files=False)
    if alternate_issues:
        failures.append("alternate candidate-scoped policy failed: " + "; ".join(alternate_issues[:5]))
    trusted_policy = copy.deepcopy(policy)
    trusted_policy["schema"] = TRUSTED_POLICY_SCHEMA
    trusted_policy["schemaVersion"] = 4
    trusted_policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    trusted_policy["bindings"].setdefault("freshUserDirValidation", {
        "path": (
            "Evidence/Session19/FreshUserDir-"
            f"{trusted_policy['candidateId']}.json"
        ),
        "bytes": 1,
        "sha256": "A" * 64,
    })
    trusted_issues, _ = validate_policy(trusted_policy, root, check_files=False)
    if trusted_issues:
        failures.append(
            "trusted candidate-scoped policy failed: "
            + "; ".join(trusted_issues[:5])
        )
    forged_contract = copy.deepcopy(trusted_policy)
    forged_contract["trustedRuntimeJournalContract"][
        "runtimeJournalValidationState"
    ] = "PASS_UNTRUSTED_INVENTORY_ONLY"
    forged_issues, _ = validate_policy(forged_contract, root, check_files=False)
    if not forged_issues:
        failures.append("forged trusted-runtime policy contract survived")
    runtime_count, runtime_failures = runtime_contract_self_tests(root)
    failures.extend(runtime_failures)
    total = (
        len(mutations) + len(plan_mutations) + len(strict_cases) + 3
        + runtime_count
    )
    return not failures, total, failures


def write_receipt(path: Path, policy_path: Path, policy: dict[str, Any],
                  observations: dict[str, Any], root: Path) -> None:
    trusted = trusted_runtime_journal_required(policy)
    receipt = {
        "schema": (
            "DiscGolfTour.Session10EnvironmentShippingTechnicalEvidence.v4"
            if trusted else
            "DiscGolfTour.Session10EnvironmentShippingTechnicalEvidence.v3"
        ),
        "schemaVersion": 4 if trusted else 3,
        "session": 10,
        "milestone": "v0.5",
        "candidateId": policy["candidateId"],
        "authority": policy["authority"],
        "state": "PASS_BOUNDED_ENVIRONMENT_16_CATEGORY_CONTRACT_SLOT_PROXY_QUARANTINE_AND_SCREENSHOT_TECHNICAL_EVIDENCE",
        "policy": file_identity(policy_path, root),
        "bindings": policy["bindings"],
        "observations": observations,
        "claimBoundary": policy["claimBoundary"],
        "issues": [],
    }
    payload = (json.dumps(receipt, indent=2) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        # Exclusive creation is the immutability boundary.  A pre-check plus
        # os.replace() can overwrite a receipt created by another process in
        # the gap between those operations.
        with path.open("xb") as handle:
            created = True
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except Exception:
        if created:
            try:
                path.unlink()
            except OSError:
                pass
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--candidate-id", required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--unrealpak", type=Path, required=True)
    parser.add_argument("--external-run", type=Path, required=True)
    parser.add_argument(
        "--user-dir", type=Path,
        help="Required only by a selected v4 trusted-runtime-journal policy.",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-release-ready", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    for name, value in [
        ("--archive", args.archive), ("--unrealpak", args.unrealpak),
        ("--external-run", args.external_run),
        *(([("--user-dir", args.user_dir)]) if args.user_dir is not None else []),
    ]:
        if not value.is_absolute():
            parser.error(f"{name} must be an absolute path")
    try:
        policy_path = selected_policy_path(args.policy, root)
        policy = load_json(policy_path)
    except StrictJsonError as exc:
        print(f"SESSION 10 ENVIRONMENT SHIPPING INVALID: {exc}")
        return 1
    if type(policy) is not dict:
        print("SESSION 10 ENVIRONMENT SHIPPING INVALID: policy root must be an object")
        return 1
    if args.candidate_id != policy.get("candidateId"):
        print("SESSION 10 ENVIRONMENT SHIPPING INVALID: --candidate-id differs from selected policy")
        return 1
    trusted_runtime_required = trusted_runtime_journal_required(policy)
    if trusted_runtime_required and args.user_dir is None:
        print(
            "SESSION 10 ENVIRONMENT SHIPPING INVALID: selected v4 policy "
            "requires --user-dir"
        )
        return 1
    if not trusted_runtime_required and args.user_dir is not None:
        print(
            "SESSION 10 ENVIRONMENT SHIPPING INVALID: historical v3 policy "
            "does not accept --user-dir"
        )
        return 1
    archive = args.archive.resolve()
    unrealpak = args.unrealpak.resolve()
    external_run = args.external_run.resolve()
    user_dir = args.user_dir.resolve() if args.user_dir is not None else None
    if args.self_test:
        passed, count, failures = run_self_test(
            policy, root, archive, unrealpak, external_run, user_dir
        )
        if not passed:
            print("SESSION 10 ENVIRONMENT SHIPPING SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(f"SESSION 10 ENVIRONMENT SHIPPING SELF-TEST PASS cases={count}")
        return 0
    issues, observations = validate_live(
        policy, root, archive, unrealpak, external_run, user_dir
    )
    if issues:
        print("SESSION 10 ENVIRONMENT SHIPPING INVALID")
        for issue in issues:
            print(f" - {issue}")
        return 1
    if args.output is not None:
        output = args.output if args.output.is_absolute() else root / args.output
        try:
            write_receipt(output, policy_path, policy, observations, root)
        except (OSError, FileExistsError) as exc:
            print(f"SESSION 10 ENVIRONMENT SHIPPING OUTPUT FAILED: {exc}")
            return 1
    if args.require_release_ready:
        print(
            "SESSION 10 ENVIRONMENT SHIPPING RELEASE BLOCKED: technical slot/proxy/"
            "quarantine/screenshot evidence and all 16 source category contracts pass, "
            "but complete asset bindings, dedicated proxies, wind/LOD closure, live "
            "three-hole collision exercise, actual-RHI visual quality, performance, "
            "provenance, legal, owner, and release approvals remain false"
        )
        return 2
    print(
        "SESSION 10 ENVIRONMENT SHIPPING PASS "
        f"candidate={policy['candidateId']} category_contracts=16/16 populated_slots=3/16 variants=5 "
        "authored_blocking_proxies=50 interaction_volumes=3 quarantine_matches=0 "
        "slot_proxies=0/11 wind=0/10 lod_or_nanite=2/16 screenshots=3/10 "
        "rhi=false visual=false performance=false provenance=false "
        "legal=false owner=false release_ready=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
