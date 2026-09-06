#!/usr/bin/env python3
"""Read-only Session 10 category and machine-capture readiness audit.

This audit binds the exact Session 10 category policy/receipt to an independently
revalidated three-hole Shipping performance run.  It may accept bounded machine
facts (rendered D3D12, packaged Pine Ridge traversal, and the recorded 30-second
frame-time windows), but it never populates an asset slot or grants visual,
provenance, legal, owner, sustained-performance, or release approval.

The tool writes nothing.  A separate declaration generator provides the only
append-only output path for a future recapture.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Any, Iterable, Optional, Sequence

import validate_dg_session19_shipping_performance as performance


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CANDIDATE_ID = "S19_WindowsShipping_20260827T023919Z_51a1c132f837"
DEFAULT_POLICY_PATH = ROOT / (
    "Config/DG_Session10EnvironmentMachineEvidencePolicy-"
    f"{DEFAULT_CANDIDATE_ID}.json"
)
DEFAULT_ARCHIVE = (
    Path("C:/DGTour_Packages") / DEFAULT_CANDIDATE_ID / "Windows"
)
DEFAULT_EXTERNAL_ROOT = Path("C:/DGTourExternal")

POLICY_SCHEMA = "DiscGolfTour.Session10EnvironmentMachineEvidencePolicy.v1"
AUDIT_SCHEMA = "DiscGolfTour.Session10EnvironmentMachineEvidenceAudit.v1"
PASS_STATE = (
    "PASS_CANDIDATE_BOUND_SESSION10_MACHINE_RHI_ENVIRONMENT_TRAVERSAL_AND_"
    "BOUNDED_PERFORMANCE_TECHNICAL_EVIDENCE"
)
CANDIDATE_RE = re.compile(
    r"^S19_WindowsShipping_[0-9]{8}T[0-9]{6}Z_[0-9a-f]{12}$"
)
SHA_RE = re.compile(r"^[0-9A-F]{64}$")

CATEGORY_ORDER = [
    "TreeConiferLarge",
    "TreeConiferMedium",
    "TreeConiferYoung",
    "TreeDeciduousLarge",
    "TreeDeciduousMedium",
    "Sapling",
    "Shrub",
    "Fern",
    "Grass",
    "GroundCover",
    "Log",
    "Stump",
    "RockSmall",
    "RockLarge",
    "ForestDebris",
    "LeafLitter",
]

EXPECTED_CLAIMS = {
    "candidateBoundCategoryCoverageAudited": True,
    "candidateBoundRenderedRhiTechnicalEvidenceAccepted": True,
    "candidateBoundThreeHoleEnvironmentTraversalTechnicalEvidenceAccepted": True,
    "candidateBoundBoundedPerformanceTechnicalEvidenceAccepted": True,
    "allSixteenProductionSlotsBound": False,
    "allSixteenProductionSlotsReady": False,
    "allRequiredDedicatedProxiesAuthored": False,
    "allRequiredWindMaterialsBound": False,
    "allRequiredLodOrNaniteVerified": False,
    "allThreeHolesLiveCollisionExerciseAccepted": False,
    "visualQualityAccepted": False,
    "sustainedPerformanceCertified": False,
    "humanPerformanceAcceptance": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "ownerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}

EXPECTED_CAPTURE_ARGUMENT_TEMPLATE = [
    "-Course=PineRidge",
    "-Hole=<1|2|3>",
    "-PerformanceCaptureSeconds=30",
    "-ResX=1920",
    "-ResY=1080",
    "-ForceRes",
    "-RenderOffscreen",
    "-dx12",
    "-unattended",
    "-NoLoadExistingSave",
    "-DGNoProfileWrites",
    "-UserDir=<EXTERNAL_UUID>",
]


class AuditError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def _reject_pairs(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AuditError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise AuditError(f"non-finite JSON constant: {value}")


def _validate_numbers(value: Any, label: str = "root") -> None:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return
    if isinstance(value, int):
        if not -(2**63) <= value <= 2**63 - 1:
            raise AuditError(f"{label}: integer is outside signed 64-bit range")
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise AuditError(f"{label}: number is not finite")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _validate_numbers(item, f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            _validate_numbers(item, f"{label}.{key}")
        return
    raise AuditError(f"{label}: unsupported JSON value")


def strict_load_text(text: str, label: str) -> dict[str, Any]:
    try:
        value = json.loads(
            text,
            object_pairs_hook=_reject_pairs,
            parse_constant=_reject_constant,
        )
    except AuditError:
        raise
    except (json.JSONDecodeError, UnicodeError) as exc:
        raise AuditError(f"{label}: invalid JSON ({exc.__class__.__name__})") from None
    _validate_numbers(value, label)
    if not isinstance(value, dict):
        raise AuditError(f"{label}: root must be an object")
    return value


def strict_load(path: Path, label: str) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise AuditError(f"{label}: cannot read file ({exc.__class__.__name__})") from None
    return strict_load_text(text, label)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise AuditError(f"cannot hash {path.name} ({exc.__class__.__name__})") from None
    return digest.hexdigest().upper()


def stable_file_identity(path: Path, label: str) -> dict[str, Any]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise AuditError(f"{label}: cannot resolve file ({exc.__class__.__name__})") from None
    try:
        invalid_file = not resolved.is_file() or performance.is_reparse_point(resolved)
    except performance.EvidenceError as exc:
        raise AuditError(f"{label}: path integrity check failed ({exc})") from None
    if invalid_file:
        raise AuditError(f"{label}: expected a regular non-reparse file")
    before = resolved.stat()
    digest = sha256_file(resolved)
    after = resolved.stat()
    fingerprint = lambda item: (
        item.st_size,
        item.st_mtime_ns,
        item.st_ctime_ns,
        item.st_dev,
        item.st_ino,
    )
    if fingerprint(before) != fingerprint(after):
        raise AuditError(f"{label}: file changed while hashed")
    return {"bytes": before.st_size, "sha256": digest}


def safe_relative_path(root: Path, value: Any, label: str) -> Path:
    if not isinstance(value, str) or not value or "\\" in value:
        raise AuditError(f"{label}: path must be a non-empty normalized POSIX path")
    pure = PurePosixPath(value)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        raise AuditError(f"{label}: path is not a safe relative path")
    root_resolved = root.resolve(strict=True)
    try:
        resolved = (root / Path(*pure.parts)).resolve(strict=True)
        resolved.relative_to(root_resolved)
    except (OSError, RuntimeError, ValueError) as exc:
        raise AuditError(f"{label}: path escapes or is missing ({exc.__class__.__name__})") from None
    return resolved


def _exact_keys(value: Any, expected: set[str], label: str, issues: list[str]) -> bool:
    if not isinstance(value, dict):
        issues.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != expected:
        issues.append(
            f"{label} keys differ: missing={sorted(expected - actual)!r} "
            f"extra={sorted(actual - expected)!r}"
        )
        return False
    return True


def _expect(actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    if actual != expected or type(actual) is not type(expected):
        issues.append(f"{label} differs: expected={expected!r} actual={actual!r}")


def validate_policy_shape(policy: Any) -> list[str]:
    issues: list[str] = []
    top_keys = {
        "schema", "schemaVersion", "session", "milestone", "candidateId",
        "authority", "state", "bindings", "candidateArchive",
        "categoryBaseline", "shippingPerformanceEvidence",
        "captureDeclarationContract", "claimBoundary",
    }
    if not _exact_keys(policy, top_keys, "policy", issues):
        return issues
    for key, expected in {
        "schema": POLICY_SCHEMA,
        "schemaVersion": 1,
        "session": 10,
        "milestone": "v0.5",
        "authority": (
            "ADDITIVE_CANDIDATE_BOUND_MACHINE_ENVIRONMENT_EVIDENCE_NOT_ASSET_"
            "POPULATION_VISUAL_PROVENANCE_LEGAL_OWNER_OR_RELEASE_APPROVAL"
        ),
        "state": (
            "MACHINE_RHI_AND_BOUNDED_THREE_HOLE_PERFORMANCE_EVIDENCE_DECLARED_"
            "ASSET_AND_HUMAN_APPROVALS_PENDING"
        ),
    }.items():
        _expect(policy.get(key), expected, f"policy.{key}", issues)
    candidate = policy.get("candidateId")
    if not isinstance(candidate, str) or CANDIDATE_RE.fullmatch(candidate) is None:
        issues.append("policy.candidateId does not match the exact Shipping grammar")

    bindings = policy.get("bindings")
    if _exact_keys(bindings, {
        "session10TechnicalPolicy", "session10TechnicalReceipt",
        "categoryTechnicalPlan", "shippingPerformanceReceipt",
    }, "policy.bindings", issues):
        for key in (
            "session10TechnicalPolicy", "session10TechnicalReceipt",
            "categoryTechnicalPlan",
        ):
            item = bindings[key]
            if _exact_keys(item, {"path", "bytes", "sha256"}, f"binding.{key}", issues):
                if type(item["bytes"]) is not int or item["bytes"] <= 0:
                    issues.append(f"binding.{key}.bytes must be a positive integer")
                if not isinstance(item["sha256"], str) or SHA_RE.fullmatch(item["sha256"]) is None:
                    issues.append(f"binding.{key}.sha256 is invalid")
        item = bindings["shippingPerformanceReceipt"]
        if _exact_keys(item, {"externalRootRelativePath", "bytes", "sha256"},
                       "binding.shippingPerformanceReceipt", issues):
            if type(item["bytes"]) is not int or item["bytes"] <= 0:
                issues.append("shipping performance receipt bytes must be positive")
            if not isinstance(item["sha256"], str) or SHA_RE.fullmatch(item["sha256"]) is None:
                issues.append("shipping performance receipt SHA-256 is invalid")

    archive = policy.get("candidateArchive")
    if _exact_keys(archive, {
        "archiveRecoveryLocationToken", "archiveFileCount", "archiveBytes",
        "archiveInventorySha256", "archiveCanonicalManifestSha256", "launcher",
        "shippingExecutable",
    }, "policy.candidateArchive", issues):
        for key in ("archiveInventorySha256", "archiveCanonicalManifestSha256"):
            value = archive[key]
            if not isinstance(value, str) or SHA_RE.fullmatch(value) is None:
                issues.append(f"candidateArchive.{key} is invalid")
        for key in ("archiveFileCount", "archiveBytes"):
            if type(archive[key]) is not int or archive[key] <= 0:
                issues.append(f"candidateArchive.{key} must be positive")
        for key in ("launcher", "shippingExecutable"):
            item = archive[key]
            if _exact_keys(item, {"relativePath", "bytes", "sha256"},
                           f"candidateArchive.{key}", issues):
                if type(item["bytes"]) is not int or item["bytes"] <= 0:
                    issues.append(f"candidateArchive.{key}.bytes must be positive")
                if not isinstance(item["sha256"], str) or SHA_RE.fullmatch(item["sha256"]) is None:
                    issues.append(f"candidateArchive.{key}.sha256 is invalid")

    baseline = policy.get("categoryBaseline")
    if _exact_keys(baseline, {
        "categoryCount", "technicalContractCompleteCount", "populatedCategoryCount",
        "productionReadyCategoryCount", "missingCategoryCount",
        "populatedCategories", "missingCategories",
    }, "policy.categoryBaseline", issues):
        _expect(baseline["categoryCount"], 16, "category count", issues)
        _expect(baseline["technicalContractCompleteCount"], 16,
                "technical contract count", issues)
        _expect(baseline["populatedCategoryCount"], 3, "populated count", issues)
        _expect(baseline["productionReadyCategoryCount"], 0,
                "production-ready count", issues)
        _expect(baseline["missingCategoryCount"], 13, "missing count", issues)
        if baseline["populatedCategoryCount"] + baseline["missingCategoryCount"] != 16:
            issues.append("category baseline counts do not partition 16 categories")
        if len(baseline["populatedCategories"]) != baseline["populatedCategoryCount"] \
                or len(baseline["missingCategories"]) != baseline["missingCategoryCount"]:
            issues.append("category baseline list lengths differ from declared counts")
        if len(set(baseline["populatedCategories"])) != len(baseline["populatedCategories"]) \
                or len(set(baseline["missingCategories"])) != len(baseline["missingCategories"]):
            issues.append("category baseline contains duplicate category identities")
        if baseline["populatedCategories"] != [
            item for item in CATEGORY_ORDER
            if item in set(baseline["populatedCategories"])
        ] or baseline["missingCategories"] != [
            item for item in CATEGORY_ORDER
            if item in set(baseline["missingCategories"])
        ]:
            issues.append("category baseline lists do not preserve canonical order")
        if set(baseline["populatedCategories"]) & set(baseline["missingCategories"]):
            issues.append("category baseline populated/missing sets overlap")
        if set(baseline["populatedCategories"] + baseline["missingCategories"]) != set(CATEGORY_ORDER):
            issues.append("category baseline does not cover exactly the 16 categories")

    perf = policy.get("shippingPerformanceEvidence")
    if _exact_keys(perf, {
        "schema", "state", "runId", "runRootRelativePath",
        "requiredHoleNumbers", "requiredCourseId", "requiredCaptureProfile",
        "requiredCameraRoute", "requiredRuntimeMode", "requiredRendered",
        "requiredRhi", "requiredResolution", "requiredFoliageQuality",
        "requiredDurationSecondsPerHole", "requiredMinimumSampleCountPerHole",
        "maximumAcceptedP95FrameMs", "maximumAcceptedHitchRatePercent",
    }, "policy.shippingPerformanceEvidence", issues):
        expected = {
            "schema": performance.RECEIPT_SCHEMA,
            "state": performance.PASS_STATE,
            "requiredHoleNumbers": [1, 2, 3],
            "requiredCourseId": "PineRidgeChampionship",
            "requiredCaptureProfile": "OmenGameplay1080pHighFoliageV1",
            "requiredCameraRoute": "authored_flyover_continuous",
            "requiredRuntimeMode": "packaged",
            "requiredRendered": True,
            "requiredRhi": "D3D12",
            "requiredResolution": [1920, 1080],
            "requiredFoliageQuality": 3,
            "requiredDurationSecondsPerHole": 30,
            "requiredMinimumSampleCountPerHole": 600,
            "maximumAcceptedP95FrameMs": 22.0,
            "maximumAcceptedHitchRatePercent": 1.0,
        }
        for key, value in expected.items():
            _expect(perf[key], value, f"shippingPerformanceEvidence.{key}", issues)

    declaration = policy.get("captureDeclarationContract")
    if _exact_keys(declaration, {
        "runner", "validator", "externalOutputOnly", "appendOnly",
        "archiveMustRemainByteIdentical", "freshExternalUserDirPerHole",
        "captureStartsOnlyByExplicitRunnerInvocation",
        "longCaptureMayNotStartDuringPreflight", "requiredArgumentsTemplate",
    }, "policy.captureDeclarationContract", issues):
        for key in (
            "externalOutputOnly", "appendOnly", "archiveMustRemainByteIdentical",
            "freshExternalUserDirPerHole",
            "captureStartsOnlyByExplicitRunnerInvocation",
            "longCaptureMayNotStartDuringPreflight",
        ):
            _expect(declaration[key], True, f"captureDeclarationContract.{key}", issues)
        _expect(declaration["runner"], "Scripts/run_dg_session19_shipping_performance.py",
                "capture declaration runner", issues)
        _expect(declaration["validator"],
                "Scripts/validate_dg_session19_shipping_performance.py",
                "capture declaration validator", issues)
        _expect(declaration["requiredArgumentsTemplate"],
                EXPECTED_CAPTURE_ARGUMENT_TEMPLATE,
                "capture declaration argument template", issues)

    _expect(policy.get("claimBoundary"), EXPECTED_CLAIMS,
            "policy.claimBoundary", issues)
    return issues


def verify_binding(path: Path, binding: dict[str, Any], label: str) -> None:
    observed = stable_file_identity(path, label)
    if observed != {"bytes": binding["bytes"], "sha256": binding["sha256"]}:
        raise AuditError(
            f"{label}: identity differs: expected={binding!r} observed={observed!r}"
        )


def classify_categories(plan: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    issues: list[str] = []
    if plan.get("schema") != "DiscGolfTour.Session10EnvironmentCategoryTechnicalPlan.v1":
        issues.append("category plan schema differs")
    if plan.get("schemaVersion") != 1 or type(plan.get("schemaVersion")) is not int:
        issues.append("category plan schemaVersion differs")
    categories = plan.get("categories")
    if not isinstance(categories, list) or len(categories) != 16:
        return {
            "categoryCount": len(categories) if isinstance(categories, list) else 0,
            "technicalContractCompleteCount": 0,
            "populatedCategoryCount": 0,
            "productionReadyCategoryCount": 0,
            "missingCategoryCount": 16,
            "populatedCategories": [],
            "missingCategories": CATEGORY_ORDER,
            "categoryRows": [],
        }, issues + ["category plan must contain exactly 16 category rows"]

    rows: list[dict[str, Any]] = []
    populated: list[str] = []
    missing: list[str] = []
    technical_count = 0
    ready_count = 0
    for index, (expected_name, row) in enumerate(zip(CATEGORY_ORDER, categories)):
        if not isinstance(row, dict):
            issues.append(f"category row {index} is not an object")
            continue
        if row.get("index") != index or type(row.get("index")) is not int:
            issues.append(f"category row {index} has wrong index")
        if row.get("category") != expected_name:
            issues.append(f"category row {index} has wrong category identity")
        visual_paths = row.get("visualObjectPaths")
        if not isinstance(visual_paths, list) or not all(
            isinstance(item, str) and item.startswith("/Game/") for item in visual_paths
        ):
            issues.append(f"category {expected_name} visualObjectPaths are invalid")
            visual_paths = []
        is_populated = bool(visual_paths)
        (populated if is_populated else missing).append(expected_name)
        technical_complete = row.get("categoryTechnicalContractComplete") is True
        production_ready = row.get("assetProductionReady") is True
        technical_count += int(technical_complete)
        ready_count += int(production_ready)
        blockers: list[str] = []
        if not is_populated:
            blockers.append("ASSET_SELECTION_AND_LICENSE_RECORD")
        if row.get("dedicatedProxyRequirement") != "None" \
                and row.get("dedicatedProxyRequirementSatisfied") is not True:
            blockers.append("DEDICATED_PROXY")
        if row.get("windBindingRequired") is True \
                and not str(row.get("windBindingVerificationState", "")).startswith("VERIFIED"):
            blockers.append("WIND_BINDING")
        if not str(row.get("lodNaniteVerificationState", "")).startswith("VERIFIED"):
            blockers.append("LOD_OR_NANITE")
        if "PENDING" in str(row.get("scaleReviewState", "")) \
                or "REVIEW_REQUIRED" in str(row.get("scaleReviewState", "")):
            blockers.append("SCALE_VISUAL_REVIEW")
        if production_ready and blockers:
            issues.append(
                f"category {expected_name} claims production ready with blockers {blockers!r}"
            )
        rows.append({
            "index": index,
            "category": expected_name,
            "bindingState": row.get("bindingState"),
            "variantCount": len(visual_paths),
            "populated": is_populated,
            "technicalContractComplete": technical_complete,
            "productionReady": production_ready,
            "remainingTechnicalAndApprovalBlockers": blockers,
        })
    coverage = {
        "categoryCount": 16,
        "technicalContractCompleteCount": technical_count,
        "populatedCategoryCount": len(populated),
        "productionReadyCategoryCount": ready_count,
        "missingCategoryCount": len(missing),
        "populatedCategories": populated,
        "missingCategories": missing,
        "categoryRows": rows,
    }
    return coverage, issues


def machine_facts(
    reports: list[dict[str, Any]], contract: dict[str, Any]
) -> tuple[dict[str, Any], list[str]]:
    issues: list[str] = []
    expected_holes = contract["requiredHoleNumbers"]
    holes = [report.get("hole_number") for report in reports]
    if holes != expected_holes:
        issues.append(f"performance report holes differ: {holes!r}")
    gpu_names: list[str] = []
    rows: list[dict[str, Any]] = []
    for index, report in enumerate(reports):
        label = f"performance report {index + 1}"
        expected = {
            "schema": "disc_golf_performance_capture",
            "version": 2,
            "result": "PASS",
            "course_id": contract["requiredCourseId"],
            "capture_profile": contract["requiredCaptureProfile"],
            "camera_route": contract["requiredCameraRoute"],
            "runtime_mode": contract["requiredRuntimeMode"],
            "rendered": contract["requiredRendered"],
            "rhi": contract["requiredRhi"],
            "resolution_x": contract["requiredResolution"][0],
            "resolution_y": contract["requiredResolution"][1],
            "requested_duration_seconds": contract["requiredDurationSecondsPerHole"],
        }
        for key, value in expected.items():
            if report.get(key) != value or type(report.get(key)) is not type(value):
                issues.append(f"{label}.{key} differs")
        quality = report.get("quality")
        if not isinstance(quality, dict) \
                or quality.get("foliage") != contract["requiredFoliageQuality"]:
            issues.append(f"{label} foliage quality differs")
        summary = report.get("summary")
        if not isinstance(summary, dict):
            issues.append(f"{label} summary is missing")
            continue
        sample_count = summary.get("sample_count")
        p95 = summary.get("p95_frame_ms")
        hitch_rate = summary.get("hitch_rate_percent")
        if type(sample_count) is not int \
                or sample_count < contract["requiredMinimumSampleCountPerHole"]:
            issues.append(f"{label} sample count is insufficient")
        if type(p95) not in (int, float) or isinstance(p95, bool) \
                or not math.isfinite(float(p95)) \
                or float(p95) > contract["maximumAcceptedP95FrameMs"]:
            issues.append(f"{label} P95 frame time exceeds the bounded contract")
        if type(hitch_rate) not in (int, float) or isinstance(hitch_rate, bool) \
                or not math.isfinite(float(hitch_rate)) \
                or float(hitch_rate) > contract["maximumAcceptedHitchRatePercent"]:
            issues.append(f"{label} hitch rate exceeds the bounded contract")
        gpu = report.get("gpu_brand")
        if not isinstance(gpu, str) or not gpu.strip():
            issues.append(f"{label} GPU adapter is missing")
        else:
            gpu_names.append(gpu)
        rows.append({
            "holeNumber": report.get("hole_number"),
            "rhi": report.get("rhi"),
            "gpuAdapter": gpu,
            "resolution": [report.get("resolution_x"), report.get("resolution_y")],
            "captureProfile": report.get("capture_profile"),
            "foliageQuality": quality.get("foliage") if isinstance(quality, dict) else None,
            "sampleCount": sample_count,
            "averageFps": summary.get("average_fps"),
            "p95FrameMs": p95,
            "hitchCount": summary.get("hitch_count"),
            "hitchRatePercent": hitch_rate,
            "usedPhysicalBytes": summary.get("used_physical_bytes"),
        })
    if len(set(gpu_names)) != 1:
        issues.append("three-hole GPU adapter identity is not exact and stable")
    valid_p95 = [float(row["p95FrameMs"]) for row in rows
                 if type(row.get("p95FrameMs")) in (int, float)
                 and not isinstance(row.get("p95FrameMs"), bool)]
    valid_fps = [float(row["averageFps"]) for row in rows
                 if type(row.get("averageFps")) in (int, float)
                 and not isinstance(row.get("averageFps"), bool)]
    facts = {
        "actualRenderedRhiTechnicalEvidenceAccepted": not issues,
        "threeHoleEnvironmentTraversalTechnicalEvidenceAccepted": not issues,
        "boundedThreeHolePerformanceTechnicalEvidenceAccepted": not issues,
        "holeNumbers": holes,
        "gpuAdapter": gpu_names[0] if len(set(gpu_names)) == 1 else None,
        "worstP95FrameMs": max(valid_p95) if valid_p95 else None,
        "lowestAverageFps": min(valid_fps) if valid_fps else None,
        "totalHitchCount": sum(
            row.get("hitchCount", 0) for row in rows
            if type(row.get("hitchCount")) is int
        ),
        "runs": rows,
        "scope": (
            "THREE_30_SECOND_PACKAGED_FLYOVER_WINDOWS_NOT_SUSTAINED_THERMAL_"
            "SOAK_OR_HUMAN_VISUAL_PERFORMANCE_APPROVAL"
        ),
    }
    return facts, issues


def _compare_receipts(
    recorded: dict[str, Any], recomputed: dict[str, Any], issues: list[str]
) -> None:
    stable_keys = {
        "schema", "schemaVersion", "session", "candidateId", "state", "passed",
        "runId", "archive", "runManifest", "runs", "claimBoundary", "errors",
        "tooling",
    }
    if set(recorded) != stable_keys | {"verifiedUtc"}:
        issues.append("shipping performance receipt keys differ")
        return
    for key in sorted(stable_keys):
        if recorded.get(key) != recomputed.get(key):
            issues.append(f"shipping performance receipt field changed: {key}")


def audit_selected(
    policy_path: Path,
    archive: Path,
    external_root: Path,
) -> dict[str, Any]:
    issues: list[str] = []
    try:
        policy_resolved = policy_path.resolve(strict=True)
        policy_resolved.relative_to(ROOT.resolve())
    except (OSError, RuntimeError, ValueError) as exc:
        return {
            "schema": AUDIT_SCHEMA,
            "passed": False,
            "state": "FAIL_CLOSED",
            "issues": [
                "machine evidence policy must be a source-controlled project file "
                f"({exc.__class__.__name__})"
            ],
            "claimBoundary": {"releaseReady": False},
        }
    try:
        policy = strict_load(policy_resolved, "machine evidence policy")
    except AuditError as exc:
        return {
            "schema": AUDIT_SCHEMA,
            "passed": False,
            "state": "FAIL_CLOSED",
            "issues": [str(exc)],
            "claimBoundary": {"releaseReady": False},
        }
    issues.extend(validate_policy_shape(policy))
    if issues:
        return {
            "schema": AUDIT_SCHEMA,
            "candidateId": policy.get("candidateId"),
            "passed": False,
            "state": "FAIL_CLOSED",
            "issues": issues,
            "claimBoundary": {"releaseReady": False},
        }

    candidate = policy["candidateId"]
    bindings = policy["bindings"]
    loaded: dict[str, dict[str, Any]] = {}
    paths: dict[str, Path] = {}
    for key in (
        "session10TechnicalPolicy", "session10TechnicalReceipt",
        "categoryTechnicalPlan",
    ):
        binding = bindings[key]
        try:
            path = safe_relative_path(ROOT, binding["path"], f"binding {key}")
            verify_binding(path, binding, f"binding {key}")
            paths[key] = path
            loaded[key] = strict_load(path, f"binding {key}")
        except AuditError as exc:
            issues.append(str(exc))

    try:
        external_root_resolved = external_root.resolve(strict=True)
        if not external_root_resolved.is_dir() \
                or performance.is_reparse_point(external_root_resolved):
            raise AuditError("external root must be a regular non-reparse directory")
        if performance.is_same_or_under(external_root_resolved, ROOT) \
                or performance.is_same_or_under(ROOT, external_root_resolved) \
                or performance.is_same_or_under(external_root_resolved, archive) \
                or performance.is_same_or_under(archive, external_root_resolved):
            raise AuditError("external root must be outside project/archive roots")
        perf_binding = bindings["shippingPerformanceReceipt"]
        perf_receipt_path = safe_relative_path(
            external_root_resolved,
            perf_binding["externalRootRelativePath"],
            "shipping performance receipt",
        )
        verify_binding(perf_receipt_path, perf_binding, "shipping performance receipt")
        perf_receipt = strict_load(perf_receipt_path, "shipping performance receipt")
    except (AuditError, performance.EvidenceError) as exc:
        issues.append(str(exc))
        perf_receipt_path = Path()
        perf_receipt = {}

    try:
        archive_resolved = performance.validate_candidate_archive_path(archive, candidate)
        archive_manifest = performance.build_archive_manifest(archive_resolved, candidate)
    except (performance.EvidenceError, OSError) as exc:
        issues.append(f"candidate archive validation failed: {exc}")
        archive_resolved = archive
        archive_manifest = {}

    expected_archive = policy["candidateArchive"]
    if archive_manifest:
        observed_archive = {
            "archiveFileCount": archive_manifest["fileCount"],
            "archiveBytes": archive_manifest["totalBytes"],
            "archiveInventorySha256": archive_manifest["inventorySha256"],
            "launcher": {
                "relativePath": archive_manifest["launcher"]["relativePath"],
                "bytes": archive_manifest["launcher"]["bytes"],
                "sha256": archive_manifest["launcher"]["sha256"],
            },
            "shippingExecutable": {
                "relativePath": archive_manifest["shippingExecutable"]["relativePath"],
                "bytes": archive_manifest["shippingExecutable"]["bytes"],
                "sha256": archive_manifest["shippingExecutable"]["sha256"],
            },
        }
        for key, value in observed_archive.items():
            if expected_archive.get(key) != value:
                issues.append(f"candidate archive binding differs: {key}")

    technical_policy = loaded.get("session10TechnicalPolicy", {})
    technical_receipt = loaded.get("session10TechnicalReceipt", {})
    category_plan = loaded.get("categoryTechnicalPlan", {})
    if technical_policy.get("candidateId") != candidate:
        issues.append("Session 10 technical policy candidate differs")
    if technical_receipt.get("candidateId") != candidate:
        issues.append("Session 10 technical receipt candidate differs")
    if technical_receipt.get("issues") != []:
        issues.append("Session 10 technical receipt has issues")
    if technical_receipt.get("policy", {}).get("sha256") != \
            bindings["session10TechnicalPolicy"]["sha256"]:
        issues.append("Session 10 receipt does not bind the selected technical policy")
    technical_candidate = technical_policy.get("candidate", {})
    archive_cross_checks = {
        "archiveRecoveryLocationToken": technical_candidate.get(
            "archiveRecoveryLocationToken"
        ),
        "archiveFileCount": technical_candidate.get("archiveFileCount"),
        "archiveBytes": technical_candidate.get("archiveBytes"),
        "archiveCanonicalManifestSha256": technical_candidate.get(
            "archiveCanonicalManifestSha256"
        ),
        "shippingExecutable": {
            "relativePath": technical_candidate.get(
                "shippingExecutableRelativePath"
            ),
            "bytes": technical_candidate.get("shippingExecutableBytes"),
            "sha256": technical_candidate.get("shippingExecutableSha256"),
        },
    }
    for key, value in archive_cross_checks.items():
        if policy["candidateArchive"].get(key) != value:
            issues.append(
                f"machine policy archive does not cross-bind Session 10 policy: {key}"
            )
    prior_claims = technical_receipt.get("claimBoundary", {})
    for key in (
        "visualQualityAccepted", "performanceApproval", "provenanceApproval",
        "legalApproval", "ownerApproval", "releaseApproval", "releaseReady",
    ):
        if prior_claims.get(key) is not False:
            issues.append(f"Session 10 technical receipt overclaims {key}")

    coverage, category_issues = classify_categories(category_plan)
    issues.extend(category_issues)
    baseline = policy["categoryBaseline"]
    for key in (
        "categoryCount", "technicalContractCompleteCount", "populatedCategoryCount",
        "productionReadyCategoryCount", "missingCategoryCount",
        "populatedCategories", "missingCategories",
    ):
        if coverage.get(key) != baseline.get(key):
            issues.append(f"category coverage differs from candidate baseline: {key}")
    receipt_slots = technical_receipt.get("observations", {}).get("slotBinding", {})
    if receipt_slots.get("populatedCategoryCount") != coverage["populatedCategoryCount"] \
            or receipt_slots.get("missingCategoryCount") != coverage["missingCategoryCount"]:
        issues.append("Session 10 receipt slot counts differ from the category plan")

    reports: list[dict[str, Any]] = []
    recomputed_receipt: dict[str, Any] = {}
    if perf_receipt and archive_manifest:
        perf_contract = policy["shippingPerformanceEvidence"]
        if perf_receipt.get("candidateId") != candidate \
                or perf_receipt.get("schema") != perf_contract["schema"] \
                or perf_receipt.get("state") != perf_contract["state"] \
                or perf_receipt.get("runId") != perf_contract["runId"] \
                or perf_receipt.get("passed") is not True:
            issues.append("shipping performance receipt identity/state differs")
        try:
            run_root = safe_relative_path(
                external_root_resolved,
                perf_contract["runRootRelativePath"],
                "shipping performance run",
            )
            if not run_root.is_dir():
                raise AuditError("shipping performance run must be a directory")
            recomputed_receipt = performance.validate_run(
                run_root, archive_resolved, candidate
            )
            if recomputed_receipt.get("passed") is not True:
                issues.extend(
                    f"shipping performance revalidation: {item}"
                    for item in recomputed_receipt.get("errors", ["failed closed"])
                )
            else:
                _compare_receipts(perf_receipt, recomputed_receipt, issues)
            for row in sorted(perf_receipt.get("runs", []),
                              key=lambda item: item.get("holeNumber", 0)):
                binding = row.get("performanceReport", {})
                path = safe_relative_path(
                    run_root, binding.get("relativePath"),
                    f"hole {row.get('holeNumber')} performance report",
                )
                verify_binding(path, {
                    "bytes": binding.get("bytes"),
                    "sha256": binding.get("sha256"),
                }, f"hole {row.get('holeNumber')} performance report")
                reports.append(strict_load(path, f"hole {row.get('holeNumber')} report"))
        except (AuditError, performance.EvidenceError, OSError) as exc:
            issues.append(f"shipping performance evidence failed closed: {exc}")

    machine, machine_issues = machine_facts(
        reports, policy["shippingPerformanceEvidence"]
    )
    issues.extend(machine_issues)
    claims = policy["claimBoundary"]
    if machine["actualRenderedRhiTechnicalEvidenceAccepted"] != \
            claims["candidateBoundRenderedRhiTechnicalEvidenceAccepted"]:
        issues.append("machine RHI fact does not support the supplemental claim")
    if machine["threeHoleEnvironmentTraversalTechnicalEvidenceAccepted"] != \
            claims["candidateBoundThreeHoleEnvironmentTraversalTechnicalEvidenceAccepted"]:
        issues.append("machine traversal fact does not support the supplemental claim")
    if machine["boundedThreeHolePerformanceTechnicalEvidenceAccepted"] != \
            claims["candidateBoundBoundedPerformanceTechnicalEvidenceAccepted"]:
        issues.append("bounded performance fact does not support the supplemental claim")

    report = {
        "schema": AUDIT_SCHEMA,
        "schemaVersion": 1,
        "session": 10,
        "candidateId": candidate,
        "auditedUtc": utc_now(),
        "state": PASS_STATE if not issues else "FAIL_CLOSED",
        "passed": not issues,
        "policy": {
            "path": policy_path.resolve().relative_to(ROOT.resolve()).as_posix(),
            **stable_file_identity(policy_path, "machine evidence policy"),
        },
        "candidateArchive": {
            "fileCount": archive_manifest.get("fileCount"),
            "bytes": archive_manifest.get("totalBytes"),
            "inventorySha256": archive_manifest.get("inventorySha256"),
            "shippingExecutable": archive_manifest.get("shippingExecutable"),
            "preservedReadOnly": True,
        },
        "categoryCoverage": coverage,
        "machineEvidence": machine,
        "remainingBlockers": {
            "missingProductionSlotBindings": coverage["missingCategories"],
            "populatedButNotProductionReady": [
                row["category"] for row in coverage["categoryRows"]
                if row["populated"] and not row["productionReady"]
            ],
            "productionReadyCategoryCount": coverage["productionReadyCategoryCount"],
            "sustainedThermalSoak": False,
            "humanVisualQualityAcceptance": False,
            "humanPerformanceAcceptance": False,
            "provenanceApproval": False,
            "legalApproval": False,
            "ownerApproval": False,
            "releaseApproval": False,
        },
        "claimBoundary": claims,
        "issues": issues,
    }
    return report


def run_self_test(policy_path: Path = DEFAULT_POLICY_PATH) -> tuple[int, dict[str, Any]]:
    failures: list[dict[str, str]] = []
    checks = 0

    def check(name: str, condition: bool) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            failures.append({"case": name, "error": "assertion failed"})

    try:
        policy = strict_load(policy_path, "self-test policy")
        check("policy-baseline", not validate_policy_shape(policy))
        for name, mutate in (
            ("candidate-mutation", lambda item: item.__setitem__("candidateId", "wrong")),
            ("rhi-weakened", lambda item: item["shippingPerformanceEvidence"].__setitem__("requiredRhi", "Null")),
            ("hole-removed", lambda item: item["shippingPerformanceEvidence"]["requiredHoleNumbers"].pop()),
            ("slot-overclaim", lambda item: item["claimBoundary"].__setitem__("allSixteenProductionSlotsBound", True)),
            ("visual-overclaim", lambda item: item["claimBoundary"].__setitem__("visualQualityAccepted", True)),
            ("legal-overclaim", lambda item: item["claimBoundary"].__setitem__("legalApproval", True)),
            ("release-overclaim", lambda item: item["claimBoundary"].__setitem__("releaseReady", True)),
            ("preflight-launch-enabled", lambda item: item["captureDeclarationContract"].__setitem__("longCaptureMayNotStartDuringPreflight", False)),
            ("profile-write-guard-removed", lambda item: item["captureDeclarationContract"]["requiredArgumentsTemplate"].remove("-DGNoProfileWrites")),
        ):
            candidate = copy.deepcopy(policy)
            mutate(candidate)
            check(name, bool(validate_policy_shape(candidate)))

        plan_path = safe_relative_path(
            ROOT, policy["bindings"]["categoryTechnicalPlan"]["path"],
            "self-test category plan",
        )
        plan = strict_load(plan_path, "self-test category plan")
        coverage, coverage_issues = classify_categories(plan)
        check("category-baseline", not coverage_issues)
        check("category-count", coverage["categoryCount"] == 16)
        check("category-populated", coverage["populatedCategoryCount"] == 3)
        check("category-missing", coverage["missingCategoryCount"] == 13)
        changed_plan = copy.deepcopy(plan)
        changed_plan["categories"][0]["assetProductionReady"] = True
        _, changed_issues = classify_categories(changed_plan)
        check("ready-with-blockers-rejected", bool(changed_issues))

        contract = policy["shippingPerformanceEvidence"]
        synthetic: list[dict[str, Any]] = []
        for hole in (1, 2, 3):
            synthetic.append({
                "schema": "disc_golf_performance_capture",
                "version": 2,
                "result": "PASS",
                "course_id": contract["requiredCourseId"],
                "hole_number": hole,
                "capture_profile": contract["requiredCaptureProfile"],
                "camera_route": contract["requiredCameraRoute"],
                "requested_duration_seconds": 30,
                "rendered": True,
                "rhi": "D3D12",
                "gpu_brand": "Synthetic GPU",
                "resolution_x": 1920,
                "resolution_y": 1080,
                "runtime_mode": "packaged",
                "quality": {"foliage": 3},
                "summary": {
                    "sample_count": 600,
                    "average_fps": 120.0,
                    "p95_frame_ms": 9.0,
                    "hitch_count": 0,
                    "hitch_rate_percent": 0.0,
                    "used_physical_bytes": 1,
                },
            })
        facts, fact_issues = machine_facts(synthetic, contract)
        check("machine-baseline", not fact_issues and facts["actualRenderedRhiTechnicalEvidenceAccepted"])
        broken = copy.deepcopy(synthetic)
        broken[0]["rhi"] = "Null"
        _, broken_issues = machine_facts(broken, contract)
        check("null-rhi-rejected", bool(broken_issues))
        broken = copy.deepcopy(synthetic)
        broken[1]["summary"]["p95_frame_ms"] = 22.1
        _, broken_issues = machine_facts(broken, contract)
        check("p95-over-budget-rejected", bool(broken_issues))
        broken = copy.deepcopy(synthetic)
        broken.pop()
        _, broken_issues = machine_facts(broken, contract)
        check("missing-hole-rejected", bool(broken_issues))

        try:
            strict_load_text('{"schema":1,"schema":2}', "duplicate")
        except AuditError:
            duplicate_rejected = True
        else:
            duplicate_rejected = False
        check("duplicate-json-key-rejected", duplicate_rejected)
        try:
            strict_load_text('{"value":NaN}', "non-finite")
        except AuditError:
            non_finite_rejected = True
        else:
            non_finite_rejected = False
        check("non-finite-json-rejected", non_finite_rejected)
        try:
            safe_relative_path(ROOT, "../escape.json", "traversal")
        except AuditError:
            traversal_rejected = True
        else:
            traversal_rejected = False
        check("path-traversal-rejected", traversal_rejected)
    except (AuditError, OSError, KeyError, TypeError, ValueError) as exc:
        failures.append({"case": "self-test-exception", "error": str(exc)})
    result = {
        "schema": "DiscGolfTour.Session10EnvironmentMachineEvidenceAuditSelfTest.v1",
        "passed": not failures,
        "assertionCount": checks,
        "failures": failures,
        "processLaunched": False,
        "evidenceWritten": False,
    }
    return (0 if not failures else 1), result


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY_PATH)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--external-root", type=Path, default=DEFAULT_EXTERNAL_ROOT)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument(
        "--require-release-ready", action="store_true",
        help="exit 2 after a valid audit while declared human/content blockers remain",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if args.policy != DEFAULT_POLICY_PATH \
                or args.archive != DEFAULT_ARCHIVE \
                or args.external_root != DEFAULT_EXTERNAL_ROOT \
                or args.require_release_ready:
            print("--self-test accepts no path or gate overrides", file=sys.stderr)
            return 2
        code, result = run_self_test()
        print(json.dumps(result, indent=2, sort_keys=True))
        return code
    report = audit_selected(
        args.policy.resolve(), args.archive.resolve(), args.external_root.resolve()
    )
    print(json.dumps(report, indent=2, sort_keys=True))
    if not report.get("passed"):
        return 1
    if args.require_release_ready and report.get("claimBoundary", {}).get("releaseReady") is not True:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
