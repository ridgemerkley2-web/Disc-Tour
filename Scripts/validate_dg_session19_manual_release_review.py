#!/usr/bin/env python3
"""Validate and inventory the fail-closed Session 19 human release-review packet.

This validator deliberately does not perform legal review, infer license rights, or
turn a blanket authorization into an approval record. Normal mode validates the
packet and objective repository inventory. --require-human-approval remains blocked
until a separate, candidate-bound, sanitized decision record is deliberately supplied
with --decision-record and passes every fail-closed mechanical check.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
import tempfile
from collections import Counter
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Any, Iterable

import generate_dg_session19_final_shipping_capture_manifest as final_capture_manifest

import generate_dg_session19_shipping_title_name_inventory as title_name_inventory


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ManualReleaseReviewPolicy.json"
TEMPLATE_PATH = ROOT / "Evidence/Session19/ManualReleaseDecisionRecord.template.json"
EVIDENCE_PATH = ROOT / "Evidence/Session19/ManualReleaseReviewInventory.json"

EXPECTED_GATE_IDS = [
    "PUBLIC_TITLE_CLEARANCE",
    "PUBLIC_DISPLAY_NAME_CLEARANCE",
    "LOGO_AND_TRADE_DRESS_REVIEW",
    "FINAL_VISUAL_PRODUCT_APPROVAL",
    "AUTHORED_AUDIO_LICENSE_COVERAGE_AND_MIX",
    "PRODUCTION_CHARACTER_MOTION_AND_VISUAL_APPROVAL",
    "PRODUCTION_ENVIRONMENT_ACCEPTANCE",
    "PHYSICS_CALIBRATION_AND_PLAY_FEEL_APPROVAL",
]
EXPECTED_AUDIO_CATEGORIES = [
    "ThrowRelease",
    "AirborneFlight",
    "GroundContact",
    "GroundState",
    "BasketOutcome",
    "Penalty",
    "HoleStart",
    "HoleCompletion",
    "HoleTransition",
    "RoundCompletion",
    "Replay",
    "Flyover",
]
EXPECTED_WORKING_NAMES = ["DGTour", "DiscGolfTour", "Disc Golf Tour"]
EXPECTED_MOLDS = ["Apex", "Vector", "Line", "Compass", "Touch"]
EXPECTED_PLASTICS = ["Base", "Tour", "Crystal"]
EXPECTED_COURSE_NAMES = [
    ("PineRidgeChampionship", "Pine Ridge Championship"),
    ("PineRidgeChampionship.Hole1", "Pine Ridge Opening"),
    ("PineRidgeChampionship.Hole2", "Needle Gate"),
    ("PineRidgeChampionship.Hole3", "Gallery Lake"),
]
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".exr"}
DECISION_RECORD_SCHEMA = "DiscGolfTour.Session19ManualReleaseDecisionRecord.v1"
FINAL_CAPTURE_MANIFEST_SCHEMA = "DiscGolfTour.Session19FinalCaptureSetManifest.v1"
SHIPPING_RECEIPT_SCHEMA = "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
SHIPPING_RECEIPT_STATE = "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
TITLE_NAME_INVENTORY_SCHEMA = (
    "DiscGolfTour.Session19ShippingTitleRuntimeNameInventory.v1"
)
TITLE_NAME_INVENTORY_STATE = (
    "PASS_CANDIDATE_BOUND_TITLE_AND_RUNTIME_NAME_TECHNICAL_INVENTORY_"
    "CLEARANCE_PENDING"
)
TITLE_NAME_INVENTORY_ARTIFACT_IDS = [
    "BOUND_SHIPPING_TITLE_SURFACE_INVENTORY",
    "BOUND_SHIPPING_RUNTIME_NAME_SCAN",
]
TITLE_NAME_INVENTORY_GATES = {
    "PUBLIC_TITLE_CLEARANCE",
    "PUBLIC_DISPLAY_NAME_CLEARANCE",
}
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_ID_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$")
SANITIZED_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:@/+\-]{2,255}$")
PLACEHOLDER_IDS = {
    "anonymous",
    "blanket",
    "blanket_approval",
    "blanket_authorization",
    "i_approve_it_all",
    "n/a",
    "na",
    "none",
    "not_provided",
    "null",
    "owner",
    "pending",
    "tbd",
    "todo",
    "unknown",
    "unsigned",
    "user",
}


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def files_under(path: Path, extensions: set[str] | None = None) -> list[Path]:
    if not path.exists():
        return []
    result = []
    for candidate in path.rglob("*"):
        if not candidate.is_file():
            continue
        if extensions is not None and candidate.suffix.lower() not in extensions:
            continue
        result.append(candidate)
    return sorted(result, key=lambda item: relative(item).casefold())


def canonical_tree(root: Path) -> dict[str, Any]:
    files = files_under(root)
    rows = []
    total_bytes = 0
    for path in files:
        size = path.stat().st_size
        total_bytes += size
        rows.append(f"{relative(path)}\t{size}\t{sha256(path)}")
    combined = hashlib.sha256(("\n".join(rows) + ("\n" if rows else "")).encode("utf-8")).hexdigest().upper()
    return {
        "root": relative(root),
        "fileCount": len(files),
        "totalBytes": total_bytes,
        "canonicalRelativePathSizeHashSha256": combined,
    }


def expect(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def validate_policy(policy: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    expect(errors, policy.get("schema") == "DiscGolfTour.Session19ManualReleaseReviewPolicy.v1", "policy schema mismatch")
    expect(errors, policy.get("schemaVersion") == 1, "policy schemaVersion must be 1")
    expect(errors, policy.get("session") == 19, "policy session must be 19")
    expect(errors, policy.get("releaseReady") is False, "policy must remain releaseReady=false")
    expect(errors, policy.get("publicReleaseApproved") is False, "policy must remain publicReleaseApproved=false")
    expect(errors, policy.get("blanketAuthorizationIsApprovalEvidence") is False, "blanket authorization must not be approval evidence")

    scope = policy.get("scope", {})
    expect(errors, scope.get("platform") == "Windows", "scope platform must be Windows")
    expect(errors, scope.get("configuration") == "Shipping", "scope configuration must be Shipping")
    expect(errors, scope.get("milestone") == "v0.5", "scope milestone must be v0.5")
    expect(errors, scope.get("holeNumbers") == [1, 2, 3], "scope holes must be exactly 1,2,3")

    decision = policy.get("decisionPolicy", {})
    expect(errors, decision.get("allowedStates") == ["PENDING", "APPROVED", "REJECTED"], "decision states mismatch")
    expect(errors, decision.get("currentState") == "PENDING", "decision policy must remain pending")
    expect(errors, decision.get("acceptedDecisionRecordPath") is None, "accepted decision record must remain null until real review")
    expect(errors, decision.get("approvalRecords") == [], "approval records must be empty in this pending packet")
    expect(errors, len(decision.get("requiredCandidateBindingFields", [])) == 4, "candidate binding must require four fields")
    expect(errors, len(decision.get("requiredReviewerFields", [])) == 5, "reviewer record must require five fields")

    names = policy.get("publicNameInventory", {})
    title = names.get("title", {})
    expect(errors, title.get("candidate") is None, "final public title candidate must be null")
    expect(errors, title.get("workingNames") == EXPECTED_WORKING_NAMES, "working title inventory mismatch")
    equipment = names.get("equipment", [])
    expect(errors, len(equipment) == 8, "equipment name inventory must contain eight rows")
    mold_ids = [row.get("stableId") for row in equipment if row.get("kind") == "mold"]
    plastic_ids = [row.get("stableId") for row in equipment if row.get("kind") == "plastic"]
    expect(errors, mold_ids == EXPECTED_MOLDS, "mold name inventory mismatch")
    expect(errors, plastic_ids == EXPECTED_PLASTICS, "plastic name inventory mismatch")
    expect(errors, all(row.get("candidateDisplayName") == row.get("stableId") for row in equipment), "equipment candidate names must match current stable IDs")
    course = names.get("course", [])
    expect(errors, [(row.get("stableId"), row.get("candidateDisplayName")) for row in course] == EXPECTED_COURSE_NAMES, "course/hole name inventory mismatch")

    audio = policy.get("audioCoverageContract", {})
    expect(errors, audio.get("requiredSemanticCategories") == EXPECTED_AUDIO_CATEGORIES, "audio semantic category inventory mismatch")
    expect(errors, audio.get("acceptedAuthoredAudioManifestPath") is None, "authored audio manifest must be null while missing")
    expect(errors, audio.get("acceptedMixReviewPath") is None, "mix review must be null while missing")
    expect(errors, audio.get("silentFallbackIsProductionCoverage") is False, "silent fallback cannot count as production audio")

    captures = policy.get("finalCaptureContract", {})
    expect(errors, captures.get("acceptedCaptureManifestPath") is None, "accepted capture manifest must be null")
    expect(errors, captures.get("mustBeFromBoundShippingCandidate") is True, "final captures must bind to Shipping candidate")
    expect(errors, captures.get("requiredResolutions") == ["1280x720", "1920x1080"], "capture resolutions mismatch")
    expect(errors, len(captures.get("requiredViews", [])) == 18, "capture contract must contain eighteen views")
    expect(errors, captures.get("savedOrDevelopmentCapturesAreFinalEvidence") is False, "development captures cannot be final evidence")

    gates = policy.get("reviewGates", [])
    gate_ids = [gate.get("gateId") for gate in gates]
    expect(errors, gate_ids == EXPECTED_GATE_IDS, "review gate order or identity mismatch")
    expect(errors, len(set(gate_ids)) == len(EXPECTED_GATE_IDS), "review gate IDs must be unique")
    for gate in gates:
        gate_id = gate.get("gateId", "<missing>")
        expect(errors, gate.get("state") == "PENDING", f"{gate_id} must remain pending")
        expect(errors, gate.get("closed") is False, f"{gate_id} must remain closed=false")
        expect(errors, bool(gate.get("blockerIds")), f"{gate_id} requires blocker IDs")
        expect(errors, bool(gate.get("objectiveBoundary")), f"{gate_id} requires an objective boundary")
        expect(errors, len(gate.get("requiredHumanRoles", [])) >= 2, f"{gate_id} requires named human roles")
        expect(errors, len(gate.get("requiredDecisions", [])) >= 3, f"{gate_id} requires explicit decisions")
        expect(errors, len(gate.get("requiredArtifacts", [])) >= 3, f"{gate_id} requires explicit artifacts")

    expected_required = {
        "Config/DG_Session19ManualReleaseReviewPolicy.json",
        "Scripts/validate_dg_session19_manual_release_review.py",
        "Docs/DG_SESSION19_MANUAL_RELEASE_REVIEW.md",
        "Evidence/Session19/ManualReleaseReviewInventory.json",
        "Evidence/Session19/ManualReleaseDecisionRecord.template.json",
    }
    expect(errors, set(policy.get("requiredFiles", [])) == expected_required, "required file inventory mismatch")
    validation = policy.get("validation", {})
    expect(errors, validation.get("normalExpectedExitCode") == 0, "normal expected exit code must be 0")
    expect(errors, validation.get("humanApprovalRequiredExpectedExitCode") == 2, "human-required expected exit code must be 2")
    expect(errors, validation.get("normalModeMustRemainHumanBlocked") is True, "normal mode must remain human blocked")
    return errors


def validate_template(template: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    expect(errors, template.get("schema") == "DiscGolfTour.Session19ManualReleaseDecisionRecord.v1", "decision template schema mismatch")
    expect(errors, template.get("template") is True, "decision record must be clearly marked template")
    binding = template.get("candidateBinding", {})
    expect(errors, len(binding) == 4 and all(value is None for value in binding.values()), "template candidate binding must be empty")
    decisions = template.get("gateDecisions", [])
    expect(errors, [row.get("gateId") for row in decisions] == EXPECTED_GATE_IDS, "template gate inventory mismatch")
    for row in decisions:
        gate_id = row.get("gateId", "<missing>")
        expect(errors, row.get("decision") == "PENDING", f"template {gate_id} must be pending")
        expect(errors, row.get("reviewers") == [], f"template {gate_id} reviewers must be empty")
        expect(errors, row.get("artifactIds") == [], f"template {gate_id} artifacts must be empty")
    expect(errors, template.get("releaseDecision") == "PENDING", "template release decision must be pending")
    expect(errors, template.get("releaseDecisionReviewers") == [], "template release reviewers must be empty")
    return errors


def valid_sanitized_id(value: Any) -> bool:
    return (
        type(value) is str
        and value == value.strip()
        and SANITIZED_ID_RE.fullmatch(value) is not None
        and value.casefold() not in PLACEHOLDER_IDS
    )


def valid_reviewed_utc(value: Any) -> bool:
    if type(value) is not str or UTC_RE.fullmatch(value) is None:
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def repository_relative_file(
    repository_root: Path,
    value: Any,
    label: str,
    errors: list[str],
) -> Path | None:
    if type(value) is not str or not value:
        errors.append(f"{label} must be a non-empty repository-relative path")
        return None
    if "\\" in value or "\x00" in value or ":" in value:
        errors.append(f"{label} must use a canonical repository-relative POSIX path")
        return None
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value or any(part in ("", ".", "..") for part in pure.parts):
        errors.append(f"{label} must use a canonical repository-relative POSIX path")
        return None

    root = repository_root.resolve()
    candidate = repository_root.joinpath(*pure.parts)
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError):
        errors.append(f"{label} does not resolve to a contained repository file")
        return None
    if candidate.is_symlink() or not resolved.is_file():
        errors.append(f"{label} must resolve to a regular non-symlink file")
        return None
    return resolved


def validate_shipping_candidate_binding(
    binding: dict[str, Any],
    policy: dict[str, Any],
    repository_root: Path,
) -> tuple[list[str], str | None]:
    errors: list[str] = []
    required_fields = policy.get("decisionPolicy", {}).get("requiredCandidateBindingFields", [])
    if type(binding) is not dict:
        return ["decision record candidateBinding must be an object"], None
    expect(
        errors,
        set(binding) == set(required_fields),
        "decision record candidateBinding must contain exactly the four policy fields",
    )

    receipt_token = binding.get("shippingArchiveReceiptPath")
    manifest_sha = binding.get("shippingArchiveManifestSha256")
    executable_sha = binding.get("shippingExecutableSha256")
    capture_sha = binding.get("captureSetManifestSha256")
    for field, value in (
        ("shippingArchiveManifestSha256", manifest_sha),
        ("shippingExecutableSha256", executable_sha),
        ("captureSetManifestSha256", capture_sha),
    ):
        expect(
            errors,
            type(value) is str and SHA256_RE.fullmatch(value) is not None and value != "0" * 64,
            f"candidateBinding.{field} must be a non-placeholder uppercase SHA-256",
        )

    receipt_path = repository_relative_file(
        repository_root,
        receipt_token,
        "candidateBinding.shippingArchiveReceiptPath",
        errors,
    )
    if receipt_path is None:
        return errors, None
    try:
        receipt = load_json(receipt_path)
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"shipping archive receipt is unreadable JSON: {exc}")
        return errors, None
    if type(receipt) is not dict:
        errors.append("shipping archive receipt must be a JSON object")
        return errors, None

    candidate_id = receipt.get("runId")
    expect(errors, receipt.get("schema") == SHIPPING_RECEIPT_SCHEMA, "shipping archive receipt schema mismatch")
    expect(errors, receipt.get("schemaVersion") == 2, "shipping archive receipt schemaVersion must be 2")
    expect(errors, receipt.get("session") == 19, "shipping archive receipt session must be 19")
    expect(
        errors,
        type(candidate_id) is str and CANDIDATE_ID_RE.fullmatch(candidate_id) is not None,
        "shipping archive receipt candidate ID is malformed",
    )
    expect(errors, receipt.get("state") == SHIPPING_RECEIPT_STATE, "shipping archive receipt is not in the verified pass state")
    expect(errors, receipt.get("verificationMode") == "DIRECT_BUILD_PASS", "shipping archive receipt must prove a direct build")
    expect(errors, receipt.get("archiveContract", {}).get("allRequiredFilesPresent") is True, "shipping archive receipt does not prove all required files")
    expect(errors, receipt.get("releaseExcludedProjectRootsAbsent") is True, "shipping archive receipt does not prove excluded project roots absent")
    expect(errors, receipt.get("shippingPluginCapabilities", {}).get("technicalGatePass") is True, "shipping archive receipt plugin technical gate did not pass")

    expected_receipt_name = (
        f"ShippingCandidateVerification-{candidate_id}.json"
        if type(candidate_id) is str
        else None
    )
    expect(errors, receipt_path.name == expected_receipt_name, "shipping archive receipt filename does not bind its candidate ID")
    expect(errors, receipt.get("receiptPath") == receipt_token, "shipping archive receipt self-path differs from candidateBinding")

    receipt_manifest_sha = receipt.get("shippingPluginCapabilities", {}).get(
        "finalArchiveCanonicalManifestSha256"
    )
    receipt_executable = receipt.get("innerShippingExecutable", {})
    receipt_executable_sha = receipt_executable.get("sha256")
    expect(
        errors,
        type(receipt_manifest_sha) is str and SHA256_RE.fullmatch(receipt_manifest_sha) is not None,
        "shipping archive receipt canonical manifest hash is malformed",
    )
    expect(
        errors,
        type(receipt_executable_sha) is str and SHA256_RE.fullmatch(receipt_executable_sha) is not None,
        "shipping archive receipt executable hash is malformed",
    )
    expect(
        errors,
        receipt_executable.get("relativePath")
        == "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
        "shipping archive receipt does not bind the inner Shipping executable",
    )
    expect(
        errors,
        manifest_sha == receipt_manifest_sha,
        "candidateBinding shipping archive manifest hash differs from its receipt",
    )
    expect(
        errors,
        executable_sha == receipt_executable_sha,
        "candidateBinding Shipping executable hash differs from its receipt",
    )
    return errors, candidate_id if type(candidate_id) is str else None


def validate_capture_manifest_binding(
    manifest: Any,
    manifest_path: Path,
    decision_binding: Any,
    candidate_id: str | None,
    policy: dict[str, Any],
    repository_root: Path = ROOT,
) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    summary: dict[str, Any] = {
        "accepted": False,
        "path": manifest_path.relative_to(repository_root.resolve()).as_posix(),
        "sha256": sha256(manifest_path),
        "captureCount": 0,
    }
    if type(manifest) is not dict:
        return summary, ["final capture-set manifest must be a JSON object"]
    if type(decision_binding) is not dict:
        return summary, ["decision record candidateBinding is unavailable for capture-manifest validation"]

    expect(errors, manifest.get("schema") == FINAL_CAPTURE_MANIFEST_SCHEMA, "final capture-set manifest schema mismatch")
    expect(errors, manifest.get("schemaVersion") == 1, "final capture-set manifest schemaVersion must be 1")
    expect(errors, manifest.get("session") == 19, "final capture-set manifest session must be 19")
    observed_sha = summary["sha256"]
    expect(
        errors,
        observed_sha == decision_binding.get("captureSetManifestSha256"),
        "final capture-set manifest file hash differs from candidateBinding.captureSetManifestSha256",
    )

    manifest_binding = manifest.get("candidateBinding")
    if type(manifest_binding) is not dict:
        errors.append("final capture-set manifest candidateBinding must be an object")
        manifest_binding = {}
    expected_binding_fields = {
        "candidateId",
        "shippingArchiveManifestSha256",
        "shippingExecutableSha256",
    }
    expect(
        errors,
        set(manifest_binding) == expected_binding_fields,
        "final capture-set manifest candidateBinding fields differ",
    )
    expect(errors, manifest_binding.get("candidateId") == candidate_id, "final capture-set manifest candidate ID differs")
    expect(
        errors,
        manifest_binding.get("shippingArchiveManifestSha256")
        == decision_binding.get("shippingArchiveManifestSha256"),
        "final capture-set manifest archive hash differs",
    )
    expect(
        errors,
        manifest_binding.get("shippingExecutableSha256")
        == decision_binding.get("shippingExecutableSha256"),
        "final capture-set manifest executable hash differs",
    )

    captures = manifest.get("captures")
    if type(captures) is not list:
        errors.append("final capture-set manifest captures must be an array")
        captures = []
    summary["captureCount"] = len(captures)
    required_views = policy.get("finalCaptureContract", {}).get("requiredViews", [])
    required_resolutions = policy.get("finalCaptureContract", {}).get("requiredResolutions", [])
    expected_pairs = {
        (view, resolution)
        for view in required_views
        for resolution in required_resolutions
    }
    observed_pairs: list[tuple[Any, Any]] = []
    artifact_ids: list[Any] = []
    capture_hashes: list[Any] = []
    for index, capture in enumerate(captures):
        label = f"final capture-set manifest captures[{index}]"
        if type(capture) is not dict:
            errors.append(f"{label} must be an object")
            continue
        view = capture.get("viewId")
        resolution = capture.get("resolution")
        artifact_id = capture.get("artifactId")
        capture_hash = capture.get("sha256")
        observed_pairs.append((view, resolution))
        artifact_ids.append(artifact_id)
        capture_hashes.append(capture_hash)
        expect(errors, type(view) is str and view in required_views, f"{label} viewId is not policy-authorized")
        expect(errors, type(resolution) is str and resolution in required_resolutions, f"{label} resolution is not policy-authorized")
        expect(errors, valid_sanitized_id(artifact_id), f"{label} artifactId must be a sanitized non-placeholder ID")
        expect(
            errors,
            type(capture_hash) is str
            and SHA256_RE.fullmatch(capture_hash) is not None
            and capture_hash != "0" * 64,
            f"{label} sha256 must be a non-placeholder uppercase SHA-256",
        )
    expect(errors, set(observed_pairs) == expected_pairs, "final capture-set manifest view/resolution matrix differs")
    expect(errors, len(observed_pairs) == len(expected_pairs), "final capture-set manifest contains duplicate view/resolution rows")
    expect(errors, len(artifact_ids) == len(set(value for value in artifact_ids if type(value) is str)), "final capture-set manifest artifact IDs must be unique strings")
    expect(errors, len(capture_hashes) == len(set(value for value in capture_hashes if type(value) is str)), "final capture-set manifest capture hashes must be unique strings")

    summary["accepted"] = not errors
    return summary, errors


def validate_title_name_inventory_binding(
    inventory: Any,
    inventory_path: Path,
    decision_binding: Any,
    candidate_id: str | None,
    policy: dict[str, Any],
    repository_root: Path = ROOT,
) -> tuple[dict[str, Any], list[str]]:
    """Validate the exact candidate-bound title/name technical inventory.

    The generator's complete structural validator remains the authority for the
    nested observations.  This layer additionally binds that immutable record to
    the completed decision record and the exact Shipping verification receipt.
    """
    errors: list[str] = []
    try:
        relative_path = inventory_path.relative_to(repository_root.resolve()).as_posix()
    except ValueError:
        relative_path = str(inventory_path)
        errors.append("title/name inventory is outside the repository root")
    observed_sha = sha256(inventory_path)
    summary: dict[str, Any] = {
        "accepted": False,
        "path": relative_path,
        "sha256": observed_sha,
        "candidateId": inventory.get("candidateId") if type(inventory) is dict else None,
        "artifactIds": [],
    }
    if type(inventory) is not dict:
        return summary, errors + ["title/name inventory must be a JSON object"]
    if type(decision_binding) is not dict:
        return summary, errors + [
            "decision record candidateBinding is unavailable for title/name inventory validation"
        ]

    expect(errors, inventory.get("schema") == TITLE_NAME_INVENTORY_SCHEMA, "title/name inventory schema mismatch")
    expect(errors, inventory.get("schemaVersion") == 1, "title/name inventory schemaVersion must be 1")
    expect(errors, inventory.get("session") == 19, "title/name inventory session must be 19")
    expect(errors, inventory.get("state") == TITLE_NAME_INVENTORY_STATE, "title/name inventory is not in the technical pass state")
    expect(errors, inventory.get("candidateId") == candidate_id, "title/name inventory candidate ID differs")
    expected_relative = (
        "Evidence/Session19/ShippingTitleRuntimeNameInventory-"
        f"{candidate_id}.json"
        if type(candidate_id) is str
        else None
    )
    expect(errors, relative_path == expected_relative, "title/name inventory path does not bind the candidate ID")
    expect(errors, inventory.get("recordPath") == expected_relative, "title/name inventory self-path differs")

    try:
        policy_data = title_name_inventory.read_stable(
            title_name_inventory.POLICY_PATH
        )
        title_policy, public_name_rows = title_name_inventory.validate_manual_policy(
            policy
        )
        structure_errors = title_name_inventory.validate_record_structure(
            inventory,
            title_policy,
            public_name_rows,
            hashlib.sha256(policy_data).hexdigest().upper(),
            title_name_inventory.hash_stable_file(
                title_name_inventory.SCRIPT_PATH
            )[1],
        )
        errors.extend(
            f"title/name inventory structure: {message}"
            for message in structure_errors
        )
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors.append(f"title/name inventory authority validation failed: {exc}")

    inventory_binding = inventory.get("candidateBinding")
    if type(inventory_binding) is not dict:
        errors.append("title/name inventory candidateBinding must be an object")
        inventory_binding = {}
    expect(
        errors,
        inventory_binding.get("candidateId") == candidate_id,
        "title/name inventory nested candidate ID differs",
    )
    expect(
        errors,
        inventory_binding.get("archive", {}).get("canonicalManifestSha256")
        == decision_binding.get("shippingArchiveManifestSha256"),
        "title/name inventory archive hash differs from the decision record",
    )
    expect(
        errors,
        inventory_binding.get("innerShippingExecutable", {}).get("sha256")
        == decision_binding.get("shippingExecutableSha256"),
        "title/name inventory executable hash differs from the decision record",
    )
    receipt_token = decision_binding.get("shippingArchiveReceiptPath")
    receipt_errors: list[str] = []
    receipt_path = repository_relative_file(
        repository_root,
        receipt_token,
        "title/name inventory Shipping verification binding",
        receipt_errors,
    )
    errors.extend(receipt_errors)
    if receipt_path is not None:
        receipt_binding = inventory_binding.get("shippingVerificationReceipt", {})
        expect(
            errors,
            receipt_binding.get("path") == receipt_token,
            "title/name inventory Shipping verification path differs",
        )
        expect(
            errors,
            receipt_binding.get("bytes") == receipt_path.stat().st_size,
            "title/name inventory Shipping verification byte size differs",
        )
        expect(
            errors,
            receipt_binding.get("sha256") == sha256(receipt_path),
            "title/name inventory Shipping verification hash differs",
        )

    artifact_rows = inventory.get("artifactIdsAndHashes")
    observed_artifact_ids = (
        [
            row.get("artifactId")
            for row in artifact_rows
            if type(row) is dict
        ]
        if type(artifact_rows) is list
        else []
    )
    summary["artifactIds"] = observed_artifact_ids
    expect(
        errors,
        observed_artifact_ids == TITLE_NAME_INVENTORY_ARTIFACT_IDS,
        "title/name inventory must contain the exact two required artifact IDs",
    )
    expect(
        errors,
        type(artifact_rows) is list and len(artifact_rows) == 2,
        "title/name inventory artifact hash inventory must contain exactly two rows",
    )
    if type(artifact_rows) is list:
        for index, row in enumerate(artifact_rows):
            expect(
                errors,
                type(row) is dict
                and set(row) == {"artifactId", "artifactPayloadSha256"}
                and type(row.get("artifactPayloadSha256")) is str
                and SHA256_RE.fullmatch(row["artifactPayloadSha256"]) is not None
                and row["artifactPayloadSha256"] != "0" * 64,
                f"title/name inventory artifact hash row[{index}] differs",
            )

    summary["accepted"] = not errors
    return summary, errors


def validate_reviewer(
    reviewer: Any,
    label: str,
    required_fields: set[str],
    allowed_roles: set[str],
    allowed_artifact_ids: set[str] | None,
) -> list[str]:
    errors: list[str] = []
    if type(reviewer) is not dict:
        return [f"{label} must be an object"]
    missing = required_fields - set(reviewer)
    expect(errors, not missing, f"{label} is missing required reviewer fields: {sorted(missing)}")
    role = reviewer.get("role")
    expect(
        errors,
        type(role) is str and role in allowed_roles,
        f"{label} role is not authorized for this decision",
    )
    expect(errors, valid_sanitized_id(reviewer.get("reviewerId")), f"{label} reviewerId must be a sanitized non-placeholder ID")
    expect(errors, valid_reviewed_utc(reviewer.get("reviewedUtc")), f"{label} reviewedUtc must be a valid UTC timestamp ending in Z")
    expect(errors, reviewer.get("decision") == "APPROVED", f"{label} decision must be APPROVED")
    artifact_id = reviewer.get("reviewArtifactId")
    expect(errors, valid_sanitized_id(artifact_id), f"{label} reviewArtifactId must be a sanitized non-placeholder ID")
    if allowed_artifact_ids is not None:
        expect(
            errors,
            type(artifact_id) is str and artifact_id in allowed_artifact_ids,
            f"{label} reviewArtifactId is not bound by its gate artifactIds",
        )
    return errors


def validate_decision_record(
    record: Any,
    policy: dict[str, Any],
    repository_root: Path = ROOT,
    title_name_inventory_sha256: str | None = None,
) -> tuple[dict[str, Any], list[str]]:
    """Validate one deliberately supplied completed human decision record.

    This checks mechanically verifiable structure and exact candidate bindings. It
    never creates reviewers, interprets blanket authorization, or substitutes for
    the named humans' underlying review.
    """
    errors: list[str] = []
    summary: dict[str, Any] = {
        "accepted": False,
        "candidateId": None,
        "approvedGateCount": 0,
        "gateCount": len(EXPECTED_GATE_IDS),
    }
    if type(record) is not dict:
        return summary, ["completed decision record must be a JSON object"]

    required_top_level = {
        "schema",
        "template",
        "candidateBinding",
        "gateDecisions",
        "releaseDecision",
        "releaseDecisionReviewers",
        "notes",
    }
    missing_top = required_top_level - set(record)
    expect(errors, not missing_top, f"completed decision record is missing fields: {sorted(missing_top)}")
    expect(errors, record.get("schema") == DECISION_RECORD_SCHEMA, "completed decision record schema mismatch")
    expect(errors, record.get("template") is False, "completed decision record must explicitly set template=false")
    expect(errors, type(record.get("notes")) is str, "completed decision record notes must be a string")

    binding_errors, candidate_id = validate_shipping_candidate_binding(
        record.get("candidateBinding"), policy, repository_root
    )
    errors.extend(binding_errors)
    summary["candidateId"] = candidate_id

    policy_gates = policy.get("reviewGates", [])
    policy_by_gate = {
        gate.get("gateId"): gate for gate in policy_gates if type(gate) is dict
    }
    gate_decisions = record.get("gateDecisions")
    if type(gate_decisions) is not list:
        errors.append("completed decision record gateDecisions must be an array")
        gate_decisions = []
    gate_ids = [
        row.get("gateId")
        if type(row) is dict and type(row.get("gateId")) is str
        else None
        for row in gate_decisions
    ]
    expect(errors, gate_ids == EXPECTED_GATE_IDS, "completed decision record gate inventory or order mismatch")
    expect(errors, len(set(gate_ids)) == len(gate_ids), "completed decision record gate IDs must be unique")

    reviewer_fields = set(policy.get("decisionPolicy", {}).get("requiredReviewerFields", []))
    all_policy_roles = {
        role
        for gate in policy_gates
        for role in gate.get("requiredHumanRoles", [])
        if type(role) is str
    }
    approved_gate_reviewers: set[tuple[str, str]] = set()
    all_artifact_ids: set[str] = set()
    capture_sha = (
        record.get("candidateBinding", {}).get("captureSetManifestSha256")
        if type(record.get("candidateBinding")) is dict
        else None
    )
    approved_gate_count = 0

    for index, row in enumerate(gate_decisions):
        gate_error_count_before = len(errors)
        if type(row) is not dict:
            errors.append(f"gateDecisions[{index}] must be an object")
            continue
        gate_id = row.get("gateId") if type(row.get("gateId")) is str else None
        gate_label = f"gate {gate_id if type(gate_id) is str else index}"
        gate_policy = policy_by_gate.get(gate_id)
        if gate_policy is None:
            errors.append(f"{gate_label} has no policy authority")
            continue
        expect(errors, row.get("decision") == "APPROVED", f"{gate_label} decision must be APPROVED")

        artifacts = row.get("artifactIds")
        if type(artifacts) is not list:
            errors.append(f"{gate_label} artifactIds must be an array")
            artifacts = []
        artifact_set = {value for value in artifacts if type(value) is str}
        expect(errors, len(artifact_set) == len(artifacts), f"{gate_label} artifactIds must be unique strings")
        for artifact_id in artifacts:
            expect(errors, valid_sanitized_id(artifact_id), f"{gate_label} contains an invalid or placeholder artifact ID")
        required_artifacts = set(gate_policy.get("requiredArtifacts", []))
        missing_artifacts = sorted(required_artifacts - artifact_set)
        expect(errors, not missing_artifacts, f"{gate_label} is missing required artifact IDs: {missing_artifacts}")
        if "BOUND_FINAL_CAPTURE_MANIFEST" in required_artifacts:
            expect(
                errors,
                capture_sha in artifact_set,
                f"{gate_label} does not bind candidateBinding.captureSetManifestSha256 in artifactIds",
            )
        if gate_id in TITLE_NAME_INVENTORY_GATES:
            expect(
                errors,
                type(title_name_inventory_sha256) is str
                and SHA256_RE.fullmatch(title_name_inventory_sha256) is not None
                and title_name_inventory_sha256 in artifact_set,
                f"{gate_label} does not bind the exact title/name inventory SHA-256 in artifactIds",
            )
        all_artifact_ids.update(artifact_set)

        exceptions = row.get("exceptions")
        expect(errors, exceptions == [], f"{gate_label} contains unresolved exceptions")

        reviewers = row.get("reviewers")
        if type(reviewers) is not list:
            errors.append(f"{gate_label} reviewers must be an array")
            reviewers = []
        required_roles = set(gate_policy.get("requiredHumanRoles", []))
        observed_roles = [
            reviewer.get("role")
            if type(reviewer) is dict and type(reviewer.get("role")) is str
            else None
            for reviewer in reviewers
        ]
        expect(errors, set(observed_roles) == required_roles, f"{gate_label} reviewer roles must exactly match policy roles")
        expect(errors, len(observed_roles) == len(set(observed_roles)), f"{gate_label} reviewer roles must be unique")
        for reviewer_index, reviewer in enumerate(reviewers):
            reviewer_label = f"{gate_label} reviewer[{reviewer_index}]"
            errors.extend(
                validate_reviewer(
                    reviewer,
                    reviewer_label,
                    reviewer_fields,
                    required_roles,
                    artifact_set,
                )
            )
            if type(reviewer) is dict and type(reviewer.get("role")) is str and type(reviewer.get("reviewerId")) is str:
                approved_gate_reviewers.add((reviewer["role"], reviewer["reviewerId"]))

        if (
            row.get("decision") == "APPROVED"
            and exceptions == []
            and set(observed_roles) == required_roles
            and required_artifacts <= artifact_set
            and len(errors) == gate_error_count_before
        ):
            approved_gate_count += 1

    summary["approvedGateCount"] = approved_gate_count
    expect(errors, record.get("releaseDecision") == "APPROVED", "releaseDecision must be APPROVED")
    release_reviewers = record.get("releaseDecisionReviewers")
    if type(release_reviewers) is not list:
        errors.append("releaseDecisionReviewers must be an array")
        release_reviewers = []
    expect(errors, bool(release_reviewers), "releaseDecisionReviewers must contain a named final approver")
    release_identities: list[tuple[Any, Any]] = []
    for index, reviewer in enumerate(release_reviewers):
        reviewer_label = f"releaseDecisionReviewers[{index}]"
        errors.extend(
            validate_reviewer(
                reviewer,
                reviewer_label,
                reviewer_fields,
                all_policy_roles,
                all_artifact_ids,
            )
        )
        if type(reviewer) is dict:
            identity = (
                reviewer.get("role") if type(reviewer.get("role")) is str else None,
                reviewer.get("reviewerId")
                if type(reviewer.get("reviewerId")) is str
                else None,
            )
            release_identities.append(identity)
            expect(
                errors,
                identity in approved_gate_reviewers,
                f"{reviewer_label} identity is not bound to an approved gate disposition",
            )
    expect(errors, len(release_identities) == len(set(release_identities)), "releaseDecisionReviewers identities must be unique")
    expect(errors, any(role == "PRODUCT_OWNER" for role, _ in release_identities), "releaseDecisionReviewers must include PRODUCT_OWNER")

    summary["accepted"] = not errors
    return summary, errors


def ini_value(text: str, key: str) -> str | None:
    match = re.search(rf"(?m)^\s*{re.escape(key)}\s*=\s*(.+?)\s*$", text)
    return match.group(1).strip() if match else None


def image_summary(root: Path) -> dict[str, Any]:
    images = files_under(root, IMAGE_EXTENSIONS)
    by_root: Counter[str] = Counter()
    for path in images:
        rel = relative(path)
        parts = rel.split("/")
        group = "/".join(parts[:3]) if len(parts) >= 3 else "/".join(parts[:-1])
        by_root[group] += 1
    return {
        "root": relative(root),
        "fileCount": len(images),
        "totalBytes": sum(path.stat().st_size for path in images),
        "countsByTopGrouping": dict(sorted(by_root.items())),
    }


def collect_inventory(policy: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    default_game_path = ROOT / "Config/DefaultGame.ini"
    uproject_path = ROOT / "DiscGolfTour.uproject"
    default_game = default_game_path.read_text(encoding="utf-8-sig")
    uproject = load_json(uproject_path)
    project_name = ini_value(default_game, "ProjectName")
    project_version = ini_value(default_game, "ProjectVersion")

    course_manifest = load_json(ROOT / "Data/PineRidgeCourse.json")
    holes = [load_json(ROOT / f"Data/PineRidgeHole{number}.json") for number in (1, 2, 3)]
    fallback_discs = load_json(ROOT / "Data/FallbackDiscs.json")
    observed_molds = [row.get("id") for row in fallback_discs.get("molds", [])]
    expect(errors, observed_molds == EXPECTED_MOLDS, "Data/FallbackDiscs.json mold IDs diverge from review inventory")
    expect(errors, course_manifest.get("courseId") == "PineRidgeChampionship", "course manifest ID mismatch")
    expect(errors, course_manifest.get("displayName") == "Pine Ridge Championship", "course display name mismatch")
    expect(errors, [hole.get("holeName") for hole in holes] == [name for _, name in EXPECTED_COURSE_NAMES[1:]], "hole display names mismatch")
    for row in policy["publicNameInventory"]["equipment"]:
        expect(errors, (ROOT / row["sourcePath"]).is_file(), f"missing equipment name source {row['sourcePath']}")

    brand = load_json(ROOT / "Config/DG_BrandLicenseContract.json")
    active_brand = next((row for row in brand.get("brands", []) if row.get("brand_id") == brand.get("policy", {}).get("active_brand_id")), None)
    expect(errors, active_brand is not None, "active generic brand declaration missing")
    authorized_logos = active_brand.get("authorized_logo_count") if active_brand else None

    media_extensions = set(policy["audioCoverageContract"]["acceptedMediaExtensions"])
    audio_media = sorted(
        files_under(ROOT / "Content", media_extensions) + files_under(ROOT / "SourceArt", media_extensions),
        key=lambda item: relative(item).casefold(),
    )
    audio_named_assets = [
        path for path in files_under(ROOT / "Content", {".uasset"})
        if re.search(r"(?i)(^|[/_])(audio|sound|music|sfx|ambience|voice)([/_]|$)", relative(path))
    ]
    router_cpp = (ROOT / "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp").read_text(encoding="utf-8-sig")
    semantic_header = (ROOT / "Source/DiscGolfTour/DiscGolfPresentationAudio.h").read_text(encoding="utf-8-sig")
    expect(errors, "SILENT ASSET FALLBACK" in router_cpp, "silent audio fallback marker missing")
    for category in EXPECTED_AUDIO_CATEGORIES:
        expect(errors, category in semantic_header, f"semantic audio category missing from runtime: {category}")

    animation_root = ROOT / "Content/DiscGolf/Animation"
    animation_assets = files_under(animation_root, {".uasset"})
    motion_sequences = [path for path in animation_assets if path.name.startswith("A_")]
    excluded_motion_roots = [
        ROOT / "Content/DiscGolf/Animation/Mocap",
        ROOT / "Content/DiscGolf/Animation/Throws",
    ]
    production_motion = [
        path for path in motion_sequences
        if not any(root == path or root in path.parents for root in excluded_motion_roots)
    ]
    serialized_policy = load_json(ROOT / "Config/DG_Session19SerializedAssetMigrationPolicy.json")
    retained_packages = serialized_policy.get("retainedPackages", [])
    retained_paths = []
    for package in retained_packages:
        disk = ROOT / ("Content/" + package.removeprefix("/Game/") + ".uasset")
        retained_paths.append(disk)
        expect(errors, disk.is_file(), f"missing retained character package {package}")

    environment_roots = [
        ROOT / "Content/Environment/Forest",
        ROOT / "Content/Presentation/Course/PineRidge",
    ]
    environment_stats = [canonical_tree(path) for path in environment_roots]
    expect(errors, all(stat["fileCount"] > 0 for stat in environment_stats), "environment content roots must not be empty")

    evidence_images = image_summary(ROOT / "Evidence/Session19")
    saved_images = image_summary(ROOT / "Saved")
    accepted_capture = policy["finalCaptureContract"]["acceptedCaptureManifestPath"]
    expect(errors, accepted_capture is None, "current packet must not bind a final capture manifest")

    latest_physics_path = ROOT / "Saved/PhysicsRegressionReports/LatestPhysicsRegression.json"
    latest_route_path = ROOT / "Saved/RouteTelemetryReports/LatestRouteTelemetry.json"
    latest_physics = load_json(latest_physics_path) if latest_physics_path.is_file() else None
    latest_route = load_json(latest_route_path) if latest_route_path.is_file() else None
    physics_summary = {
        "developmentRegressionArtifact": relative(latest_physics_path) if latest_physics else None,
        "developmentRegressionSha256": sha256(latest_physics_path) if latest_physics else None,
        "developmentRegressionPassed": latest_physics.get("passed") if latest_physics else None,
        "developmentRegressionCompletedUtc": latest_physics.get("completed_utc") if latest_physics else None,
        "developmentRegressionScenarioCount": len(latest_physics.get("results", [])) if latest_physics else 0,
        "developmentRegressionPresetIds": [row.get("preset_id") for row in latest_physics.get("results", [])] if latest_physics else [],
        "routeTelemetryArtifact": relative(latest_route_path) if latest_route else None,
        "routeTelemetrySha256": sha256(latest_route_path) if latest_route else None,
        "routeTelemetryComplete": latest_route.get("complete") if latest_route else None,
        "routeTelemetryAttemptCount": len(latest_route.get("attempts", [])) if latest_route else 0,
        "acceptedMeasuredReferenceDatasetPath": None,
        "engineeringRegressionIsRealWorldCalibration": False,
    }

    inventory = {
        "schema": "DiscGolfTour.Session19ManualReleaseReviewInventory.v1",
        "schemaVersion": 1,
        "session": 19,
        "authority": "OBJECTIVE_REPOSITORY_SNAPSHOT_NOT_LEGAL_OR_HUMAN_APPROVAL",
        "policyArtifact": {
            "path": relative(POLICY_PATH),
            "sha256": sha256(POLICY_PATH),
        },
        "releaseReady": False,
        "humanApprovalGateCount": len(EXPECTED_GATE_IDS),
        "humanApprovedGateCount": 0,
        "humanBlockedGateIds": EXPECTED_GATE_IDS,
        "publicTitleAndSurfaces": {
            "finalPublicTitle": None,
            "workingNames": EXPECTED_WORKING_NAMES,
            "defaultGameProjectName": project_name,
            "defaultGameProjectVersion": project_version,
            "uprojectDescription": uproject.get("Description"),
            "sourceArtifacts": [
                {"path": relative(default_game_path), "sha256": sha256(default_game_path)},
                {"path": relative(uproject_path), "sha256": sha256(uproject_path)},
            ],
            "objectiveStatus": "INVENTORIED_FINAL_TITLE_NULL_OWNER_AND_LEGAL_DECISIONS_REQUIRED",
        },
        "publicDisplayNames": {
            "equipment": policy["publicNameInventory"]["equipment"],
            "courseAndHoles": policy["publicNameInventory"]["course"],
            "equipmentNameCount": 8,
            "courseAndHoleNameCount": 4,
            "objectiveStatus": "TWELVE_ROWS_INVENTORIED_ROW_LEVEL_CLEARANCE_OR_REPLACEMENT_REQUIRED",
        },
        "brandAndTradeDress": {
            "activeBrandId": brand.get("policy", {}).get("active_brand_id"),
            "activeBrandStatus": active_brand.get("status") if active_brand else None,
            "authorizedLogoCount": authorized_logos,
            "manualVisualBrandReviewRequired": brand.get("policy", {}).get("manual_visual_brand_review_required"),
            "acceptedBoundReviewArtifact": None,
            "objectiveStatus": "GENERIC_DECLARATION_AND_STRING_POLICY_EXIST_VISUAL_ORIGINALITY_REQUIRES_HUMAN_REVIEW",
        },
        "finalVisualApproval": {
            "acceptedShippingCaptureManifest": accepted_capture,
            "requiredViewCount": len(policy["finalCaptureContract"]["requiredViews"]),
            "requiredResolutionCount": len(policy["finalCaptureContract"]["requiredResolutions"]),
            "session19SourceControlledImageEvidence": evidence_images,
            "unboundDevelopmentOrSavedImageCandidates": saved_images,
            "objectiveStatus": "NO_BOUND_SESSION19_SHIPPING_CAPTURE_MANIFEST_DEVELOPMENT_CAPTURES_NOT_ACCEPTED",
        },
        "authoredAudio": {
            "semanticCategoryCount": len(EXPECTED_AUDIO_CATEGORIES),
            "semanticCategories": EXPECTED_AUDIO_CATEGORIES,
            "authoredMediaFileCount": len(audio_media),
            "authoredMediaFiles": [relative(path) for path in audio_media],
            "audioNamedUassetCandidateCount": len(audio_named_assets),
            "audioNamedUassetCandidates": [relative(path) for path in audio_named_assets],
            "silentFallbackPresent": True,
            "acceptedAuthoredAudioManifest": None,
            "acceptedMixReview": None,
            "objectiveStatus": "SEMANTIC_ROUTING_IMPLEMENTED_AUTHORED_MEDIA_LICENSE_COVERAGE_AND_MIX_NOT_PRESENT",
        },
        "productionCharacterMotionAndVisual": {
            "animationAssetCount": len(animation_assets),
            "motionSequenceIdentityCount": len(motion_sequences),
            "productionMotionSequenceIdentityCountOutsideExcludedRoots": len(production_motion),
            "excludedMotionSequenceIdentities": [relative(path) for path in motion_sequences if path not in production_motion],
            "retainedCharacterPackageCount": len(retained_paths),
            "retainedCharacterPackages": [
                {"package": package, "path": relative(path), "bytes": path.stat().st_size, "sha256": sha256(path)}
                for package, path in zip(retained_packages, retained_paths)
            ],
            "acceptedProductionMotionManifest": None,
            "acceptedCharacterVisualReview": None,
            "objectiveStatus": "TECHNICAL_CHARACTER_PACKAGES_EXIST_ZERO_PRODUCTION_MOTION_SEQUENCES_OUTSIDE_EXCLUDED_ROOTS",
        },
        "productionEnvironment": {
            "contentRoots": environment_stats,
            "polyHavenDerivedRuntimeReceipt": "Evidence/Session18/PolyHavenPackageIdentityReceipt.json",
            "bakedPcgSourceAuthorityReceipt": "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json",
            "acceptedBoundShippingEnvironmentCaptureManifest": None,
            "acceptedBoundShippingPerformanceReport": None,
            "acceptedEnvironmentReview": None,
            "objectiveStatus": "IMPLEMENTATION_AND_PROVENANCE_EVIDENCE_NARROW_SCOPE_FINAL_SHIPPING_VISUAL_PERFORMANCE_AND_OWNER_ACCEPTANCE_MISSING",
        },
        "physicsCalibrationAndPlayFeel": physics_summary,
        "decisionRecord": {
            "templatePath": relative(TEMPLATE_PATH),
            "templateSha256": sha256(TEMPLATE_PATH),
            "acceptedDecisionRecordPath": policy["decisionPolicy"]["acceptedDecisionRecordPath"],
            "blanketAuthorizationAccepted": False,
            "objectiveStatus": "TEMPLATE_ONLY_ALL_EIGHT_GATES_PENDING",
        },
    }
    return inventory, errors


def validate_inventory(inventory: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    expect(errors, inventory.get("schema") == "DiscGolfTour.Session19ManualReleaseReviewInventory.v1", "inventory schema mismatch")
    expect(errors, inventory.get("releaseReady") is False, "inventory must remain releaseReady=false")
    expect(errors, inventory.get("humanApprovalGateCount") == 8, "inventory must expose eight human gates")
    expect(errors, inventory.get("humanApprovedGateCount") == 0, "inventory must not infer human approvals")
    expect(errors, inventory.get("humanBlockedGateIds") == EXPECTED_GATE_IDS, "inventory human gate list mismatch")
    expect(errors, inventory.get("publicTitleAndSurfaces", {}).get("finalPublicTitle") is None, "inventory final title must be null")
    expect(errors, inventory.get("publicDisplayNames", {}).get("equipmentNameCount") == 8, "inventory equipment name count mismatch")
    expect(errors, inventory.get("publicDisplayNames", {}).get("courseAndHoleNameCount") == 4, "inventory course name count mismatch")
    expect(errors, inventory.get("brandAndTradeDress", {}).get("authorizedLogoCount") == 0, "authorized logo count must remain zero")
    expect(errors, inventory.get("finalVisualApproval", {}).get("acceptedShippingCaptureManifest") is None, "final capture must remain unbound")
    expect(errors, inventory.get("authoredAudio", {}).get("silentFallbackPresent") is True, "silent fallback fact missing")
    expect(errors, inventory.get("authoredAudio", {}).get("acceptedAuthoredAudioManifest") is None, "audio manifest must remain unaccepted")
    expect(errors, inventory.get("productionCharacterMotionAndVisual", {}).get("acceptedProductionMotionManifest") is None, "motion manifest must remain unaccepted")
    expect(errors, inventory.get("productionEnvironment", {}).get("acceptedEnvironmentReview") is None, "environment review must remain unaccepted")
    expect(errors, inventory.get("physicsCalibrationAndPlayFeel", {}).get("engineeringRegressionIsRealWorldCalibration") is False, "engineering regression must not claim calibration")
    expect(errors, inventory.get("physicsCalibrationAndPlayFeel", {}).get("acceptedMeasuredReferenceDatasetPath") is None, "measured reference dataset must remain unaccepted")
    expect(errors, inventory.get("decisionRecord", {}).get("blanketAuthorizationAccepted") is False, "blanket authorization must remain rejected as approval evidence")
    return errors


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def write_json_once_or_verify(path: Path, value: Any) -> bool:
    """Create immutable evidence, or verify an existing byte-identical record.

    Returns True only when this call created the file. Existing evidence is never
    replaced or timestamp-touched; a stale/different inventory fails closed.
    """
    payload = json_bytes(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
        return True
    except FileExistsError:
        if path.is_symlink() or not path.is_file() or path.read_bytes() != payload:
            raise FileExistsError(
                f"immutable evidence already exists with different bytes: {path}"
            )
        return False


def self_test_decision_fixture(
    policy: dict[str, Any], repository_root: Path
) -> tuple[
    dict[str, Any], dict[str, Any], Path, dict[str, Any], Path,
    dict[str, Any], Path,
]:
    candidate_id = "S19_WindowsShipping_SELFTEST"
    manifest_sha = "A" * 64
    executable_sha = "C" * 64
    receipt_relative = (
        "Evidence/Session19/ShippingCandidateVerification-"
        f"{candidate_id}.json"
    )
    receipt_path = repository_root.joinpath(*PurePosixPath(receipt_relative).parts)
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt = {
        "schema": SHIPPING_RECEIPT_SCHEMA,
        "schemaVersion": 2,
        "session": 19,
        "runId": candidate_id,
        "state": SHIPPING_RECEIPT_STATE,
        "verificationMode": "DIRECT_BUILD_PASS",
        "archiveContract": {"allRequiredFilesPresent": True},
        "innerShippingExecutable": {
            "relativePath": "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
            "sha256": executable_sha,
        },
        "shippingPluginCapabilities": {
            "finalArchiveCanonicalManifestSha256": manifest_sha,
            "technicalGatePass": True,
        },
        "releaseExcludedProjectRootsAbsent": True,
        "receiptPath": receipt_relative,
    }
    receipt_path.write_bytes(json_bytes(receipt))

    policy_data = title_name_inventory.read_stable(
        title_name_inventory.POLICY_PATH
    )
    title_policy, public_name_rows = title_name_inventory.validate_manual_policy(
        policy
    )
    title_inventory = title_name_inventory.sample_record_for_self_test(
        title_policy,
        public_name_rows,
        hashlib.sha256(policy_data).hexdigest().upper(),
        title_name_inventory.hash_stable_file(
            title_name_inventory.SCRIPT_PATH
        )[1],
    )
    verification_identity = title_inventory["candidateBinding"][
        "shippingVerificationReceipt"
    ]
    verification_identity["bytes"] = receipt_path.stat().st_size
    verification_identity["sha256"] = sha256(receipt_path)
    for artifact in title_inventory["artifacts"].values():
        artifact["candidateBinding"]["shippingVerificationReceiptSha256"] = (
            verification_identity["sha256"]
        )
    title_name_inventory.refresh_artifact_hashes(title_inventory)
    title_inventory_path = repository_root.joinpath(
        *PurePosixPath(title_inventory["recordPath"]).parts
    )
    title_inventory_path.write_bytes(json_bytes(title_inventory))
    title_inventory_sha = sha256(title_inventory_path)

    capture_manifest = {
        "schema": FINAL_CAPTURE_MANIFEST_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateBinding": {
            "candidateId": candidate_id,
            "shippingArchiveManifestSha256": manifest_sha,
            "shippingExecutableSha256": executable_sha,
        },
        "captures": [
            {
                "viewId": view,
                "resolution": resolution,
                "artifactId": f"CAP_{view}_{resolution}",
                "sha256": hashlib.sha256(
                    f"{view}\t{resolution}".encode("utf-8")
                ).hexdigest().upper(),
            }
            for view in policy["finalCaptureContract"]["requiredViews"]
            for resolution in policy["finalCaptureContract"]["requiredResolutions"]
        ],
    }
    capture_manifest_path = (
        repository_root
        / "Evidence/Session19/FinalCaptureSetManifest-S19_WindowsShipping_FIXTURE.json"
    )
    capture_manifest_path.write_bytes(json_bytes(capture_manifest))
    capture_sha = sha256(capture_manifest_path)

    gate_decisions = []
    for gate in policy["reviewGates"]:
        artifacts = list(gate["requiredArtifacts"])
        if "BOUND_FINAL_CAPTURE_MANIFEST" in artifacts:
            artifacts.append(capture_sha)
        if gate["gateId"] in TITLE_NAME_INVENTORY_GATES:
            artifacts.append(title_inventory_sha)
        reviewers = []
        for role in gate["requiredHumanRoles"]:
            reviewers.append({
                "role": role,
                "reviewerId": f"RID_{role}",
                "reviewedUtc": "2026-08-26T21:00:00Z",
                "decision": "APPROVED",
                "reviewArtifactId": artifacts[0],
            })
        gate_decisions.append({
            "gateId": gate["gateId"],
            "decision": "APPROVED",
            "reviewers": reviewers,
            "artifactIds": artifacts,
            "exceptions": [],
        })

    record = {
        "schema": DECISION_RECORD_SCHEMA,
        "template": False,
        "candidateBinding": {
            "shippingArchiveReceiptPath": receipt_relative,
            "shippingArchiveManifestSha256": manifest_sha,
            "shippingExecutableSha256": executable_sha,
            "captureSetManifestSha256": capture_sha,
        },
        "gateDecisions": gate_decisions,
        "releaseDecision": "APPROVED",
        "releaseDecisionReviewers": [{
            "role": "PRODUCT_OWNER",
            "reviewerId": "RID_PRODUCT_OWNER",
            "reviewedUtc": "2026-08-26T22:00:00Z",
            "decision": "APPROVED",
            "reviewArtifactId": gate_decisions[0]["artifactIds"][0],
        }],
        "notes": "Named reviewers completed the candidate-bound decision record.",
    }
    return (
        record,
        receipt,
        receipt_path,
        capture_manifest,
        capture_manifest_path,
        title_inventory,
        title_inventory_path,
    )


def run_self_test(policy: dict[str, Any], template: dict[str, Any]) -> int:
    cases: list[tuple[str, Any]] = []

    def mutation(name: str, mutator: Any) -> None:
        candidate = copy.deepcopy(policy)
        mutator(candidate)
        cases.append((name, candidate))

    mutation("release_ready_true", lambda p: p.__setitem__("releaseReady", True))
    mutation("public_release_true", lambda p: p.__setitem__("publicReleaseApproved", True))
    mutation("blanket_auth_true", lambda p: p.__setitem__("blanketAuthorizationIsApprovalEvidence", True))
    mutation("wrong_platform", lambda p: p["scope"].__setitem__("platform", "Linux"))
    mutation("wrong_configuration", lambda p: p["scope"].__setitem__("configuration", "Development"))
    mutation("wrong_holes", lambda p: p["scope"].__setitem__("holeNumbers", [1]))
    mutation("decision_approved", lambda p: p["decisionPolicy"].__setitem__("currentState", "APPROVED"))
    mutation("accepted_record_invented", lambda p: p["decisionPolicy"].__setitem__("acceptedDecisionRecordPath", "invented.json"))
    mutation("approval_record_invented", lambda p: p["decisionPolicy"].__setitem__("approvalRecords", [{}]))
    mutation("title_invented", lambda p: p["publicNameInventory"]["title"].__setitem__("candidate", "Disc Golf Tour"))
    mutation("working_name_removed", lambda p: p["publicNameInventory"]["title"]["workingNames"].pop())
    mutation("equipment_removed", lambda p: p["publicNameInventory"]["equipment"].pop())
    mutation("mold_renamed", lambda p: p["publicNameInventory"]["equipment"][0].__setitem__("stableId", "Renamed"))
    mutation("plastic_renamed", lambda p: p["publicNameInventory"]["equipment"][5].__setitem__("stableId", "Renamed"))
    mutation("course_renamed", lambda p: p["publicNameInventory"]["course"][0].__setitem__("candidateDisplayName", "Renamed"))
    mutation("audio_category_removed", lambda p: p["audioCoverageContract"]["requiredSemanticCategories"].pop())
    mutation("audio_manifest_invented", lambda p: p["audioCoverageContract"].__setitem__("acceptedAuthoredAudioManifestPath", "invented.json"))
    mutation("silent_fallback_accepted", lambda p: p["audioCoverageContract"].__setitem__("silentFallbackIsProductionCoverage", True))
    mutation("capture_manifest_invented", lambda p: p["finalCaptureContract"].__setitem__("acceptedCaptureManifestPath", "invented.json"))
    mutation("capture_view_removed", lambda p: p["finalCaptureContract"]["requiredViews"].pop())
    mutation("capture_resolution_removed", lambda p: p["finalCaptureContract"]["requiredResolutions"].pop())
    mutation("gate_removed", lambda p: p["reviewGates"].pop())
    mutation("gate_reordered", lambda p: p["reviewGates"].reverse())
    mutation("gate_approved", lambda p: p["reviewGates"][0].__setitem__("state", "APPROVED"))
    mutation("gate_closed", lambda p: p["reviewGates"][0].__setitem__("closed", True))
    mutation("gate_roles_removed", lambda p: p["reviewGates"][0].__setitem__("requiredHumanRoles", []))
    mutation("gate_decisions_removed", lambda p: p["reviewGates"][0].__setitem__("requiredDecisions", []))
    mutation("gate_artifacts_removed", lambda p: p["reviewGates"][0].__setitem__("requiredArtifacts", []))
    mutation("required_file_removed", lambda p: p["requiredFiles"].pop())
    mutation("normal_exit_wrong", lambda p: p["validation"].__setitem__("normalExpectedExitCode", 2))

    failures = []
    for name, candidate in cases:
        if not validate_policy(candidate):
            failures.append(name)

    template_cases = []
    for name, mutator in [
        ("template_false", lambda t: t.__setitem__("template", False)),
        ("template_binding_filled", lambda t: t["candidateBinding"].__setitem__("shippingExecutableSha256", "0" * 64)),
        ("template_gate_approved", lambda t: t["gateDecisions"][0].__setitem__("decision", "APPROVED")),
        ("template_reviewer_invented", lambda t: t["gateDecisions"][0].__setitem__("reviewers", [{}])),
        ("template_release_approved", lambda t: t.__setitem__("releaseDecision", "APPROVED")),
    ]:
        candidate = copy.deepcopy(template)
        mutator(candidate)
        template_cases.append((name, candidate))
    for name, candidate in template_cases:
        if not validate_template(candidate):
            failures.append(name)

    immutable_writer_cases = 3
    with tempfile.TemporaryDirectory(prefix="dg_manual_review_writer_") as temporary:
        output = Path(temporary) / "inventory.json"
        baseline_value = {"schema": "self-test", "releaseReady": False}
        baseline_bytes = json_bytes(baseline_value)
        if not write_json_once_or_verify(output, baseline_value):
            failures.append("immutable writer did not report initial creation")
        before_mtime = output.stat().st_mtime_ns
        if write_json_once_or_verify(output, baseline_value):
            failures.append("immutable writer rewrote an identical record")
        if output.stat().st_mtime_ns != before_mtime or output.read_bytes() != baseline_bytes:
            failures.append("immutable writer changed identical existing evidence")
        try:
            write_json_once_or_verify(output, {"schema": "different"})
        except FileExistsError:
            if output.read_bytes() != baseline_bytes:
                failures.append("immutable writer changed mismatched existing evidence")
        else:
            failures.append("immutable writer accepted mismatched existing evidence")

    decision_case_count = 0
    with tempfile.TemporaryDirectory(prefix="dg_manual_review_decision_") as temporary:
        fixture_root = Path(temporary)
        (
            valid_record,
            valid_receipt,
            receipt_path,
            valid_capture_manifest,
            capture_manifest_path,
            valid_title_inventory,
            title_inventory_path,
        ) = self_test_decision_fixture(policy, fixture_root)
        title_inventory_summary, title_inventory_errors = (
            validate_title_name_inventory_binding(
                valid_title_inventory,
                title_inventory_path,
                valid_record["candidateBinding"],
                "S19_WindowsShipping_SELFTEST",
                policy,
                fixture_root,
            )
        )
        if (
            title_inventory_errors
            or title_inventory_summary.get("accepted") is not True
        ):
            failures.append(
                "valid title/name inventory rejected: "
                + "; ".join(title_inventory_errors)
            )
        title_inventory_sha = sha256(title_inventory_path)
        valid_summary, valid_errors = validate_decision_record(
            valid_record,
            policy,
            fixture_root,
            title_inventory_sha,
        )
        if valid_errors or valid_summary.get("accepted") is not True:
            failures.append(
                "valid completed decision record rejected: "
                + "; ".join(valid_errors)
            )

        capture_summary, capture_errors = validate_capture_manifest_binding(
            valid_capture_manifest,
            capture_manifest_path,
            valid_record["candidateBinding"],
            valid_summary.get("candidateId"),
            policy,
            fixture_root,
        )
        if capture_errors or capture_summary.get("accepted") is not True:
            failures.append(
                "valid final capture-set manifest rejected: "
                + "; ".join(capture_errors)
            )

        invalid_summary, invalid_errors = validate_decision_record(
            [], policy, fixture_root, title_inventory_sha
        )
        decision_case_count += 1
        if not invalid_errors or invalid_summary.get("accepted") is not False:
            failures.append("non-object completed decision record accepted")

        def rejected_decision(
            name: str,
            record_mutator: Any | None = None,
            receipt_mutator: Any | None = None,
        ) -> None:
            nonlocal decision_case_count
            decision_case_count += 1
            candidate_record = copy.deepcopy(valid_record)
            candidate_receipt = copy.deepcopy(valid_receipt)
            if record_mutator is not None:
                record_mutator(candidate_record)
            if receipt_mutator is not None:
                receipt_mutator(candidate_receipt)
            receipt_path.write_bytes(json_bytes(candidate_receipt))
            candidate_summary, candidate_errors = validate_decision_record(
                candidate_record,
                policy,
                fixture_root,
                title_inventory_sha,
            )
            if not candidate_errors or candidate_summary.get("accepted") is not False:
                failures.append(f"completed decision mutation accepted: {name}")

        rejected_decision("record-not-object", lambda r: r.clear())
        rejected_decision("record-schema", lambda r: r.__setitem__("schema", "wrong"))
        rejected_decision("record-template", lambda r: r.__setitem__("template", True))
        rejected_decision("record-notes", lambda r: r.__setitem__("notes", None))
        rejected_decision("binding-field-removed", lambda r: r["candidateBinding"].pop("captureSetManifestSha256"))
        rejected_decision("binding-path-blank", lambda r: r["candidateBinding"].__setitem__("shippingArchiveReceiptPath", ""))
        rejected_decision("binding-path-traversal", lambda r: r["candidateBinding"].__setitem__("shippingArchiveReceiptPath", "../receipt.json"))
        rejected_decision("binding-manifest-malformed", lambda r: r["candidateBinding"].__setitem__("shippingArchiveManifestSha256", "a" * 64))
        rejected_decision("binding-manifest-placeholder", lambda r: r["candidateBinding"].__setitem__("shippingArchiveManifestSha256", "0" * 64))
        rejected_decision("binding-manifest-mismatch", lambda r: r["candidateBinding"].__setitem__("shippingArchiveManifestSha256", "D" * 64))
        rejected_decision("binding-executable-mismatch", lambda r: r["candidateBinding"].__setitem__("shippingExecutableSha256", "D" * 64))
        rejected_decision("binding-capture-placeholder", lambda r: r["candidateBinding"].__setitem__("captureSetManifestSha256", "0" * 64))
        rejected_decision("receipt-schema", receipt_mutator=lambda r: r.__setitem__("schema", "wrong"))
        rejected_decision("receipt-session", receipt_mutator=lambda r: r.__setitem__("session", 18))
        rejected_decision("receipt-candidate", receipt_mutator=lambda r: r.__setitem__("runId", "S19_WindowsShipping_OTHER"))
        rejected_decision("receipt-state", receipt_mutator=lambda r: r.__setitem__("state", "PENDING"))
        rejected_decision("receipt-mode", receipt_mutator=lambda r: r.__setitem__("verificationMode", "RECOVERED"))
        rejected_decision("receipt-files", receipt_mutator=lambda r: r["archiveContract"].__setitem__("allRequiredFilesPresent", False))
        rejected_decision("receipt-exclusions", receipt_mutator=lambda r: r.__setitem__("releaseExcludedProjectRootsAbsent", False))
        rejected_decision("receipt-plugin-gate", receipt_mutator=lambda r: r["shippingPluginCapabilities"].__setitem__("technicalGatePass", False))
        rejected_decision("gate-removed", lambda r: r["gateDecisions"].pop())
        rejected_decision("gate-reordered", lambda r: r["gateDecisions"].reverse())
        rejected_decision("gate-id-not-string", lambda r: r["gateDecisions"][0].__setitem__("gateId", []))
        rejected_decision("gate-pending", lambda r: r["gateDecisions"][0].__setitem__("decision", "PENDING"))
        rejected_decision("gate-rejected", lambda r: r["gateDecisions"][0].__setitem__("decision", "REJECTED"))
        rejected_decision("gate-reviewers-blank", lambda r: r["gateDecisions"][0].__setitem__("reviewers", []))
        rejected_decision("gate-role-missing", lambda r: r["gateDecisions"][0]["reviewers"].pop())
        rejected_decision("gate-reviewer-id-blank", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("reviewerId", ""))
        rejected_decision("gate-reviewer-id-placeholder", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("reviewerId", "user"))
        rejected_decision("gate-reviewer-field-missing", lambda r: r["gateDecisions"][0]["reviewers"][0].pop("reviewedUtc"))
        rejected_decision("gate-reviewer-role-not-string", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("role", []))
        rejected_decision("gate-review-time", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("reviewedUtc", "not-utc"))
        rejected_decision("gate-review-pending", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("decision", "PENDING"))
        rejected_decision("gate-review-rejected", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("decision", "REJECTED"))
        rejected_decision("gate-review-artifact-unbound", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("reviewArtifactId", "UNBOUND_ARTIFACT"))
        rejected_decision("gate-review-artifact-not-string", lambda r: r["gateDecisions"][0]["reviewers"][0].__setitem__("reviewArtifactId", []))
        rejected_decision("gate-artifact-removed", lambda r: r["gateDecisions"][0]["artifactIds"].pop(0))
        rejected_decision("gate-artifact-duplicate", lambda r: r["gateDecisions"][0]["artifactIds"].append(r["gateDecisions"][0]["artifactIds"][0]))
        rejected_decision("gate-title-inventory-hash-unbound", lambda r: r["gateDecisions"][0]["artifactIds"].remove(title_inventory_sha))
        rejected_decision("gate-name-inventory-hash-unbound", lambda r: r["gateDecisions"][1]["artifactIds"].remove(title_inventory_sha))
        rejected_decision("gate-capture-hash-unbound", lambda r: r["gateDecisions"][2]["artifactIds"].remove(r["candidateBinding"]["captureSetManifestSha256"]))
        rejected_decision("gate-exception-open", lambda r: r["gateDecisions"][0]["exceptions"].append({"exceptionId": "OPEN_1"}))
        rejected_decision("release-pending", lambda r: r.__setitem__("releaseDecision", "PENDING"))
        rejected_decision("release-rejected", lambda r: r.__setitem__("releaseDecision", "REJECTED"))
        rejected_decision("release-reviewers-blank", lambda r: r.__setitem__("releaseDecisionReviewers", []))
        rejected_decision("release-reviewer-pending", lambda r: r["releaseDecisionReviewers"][0].__setitem__("decision", "PENDING"))
        rejected_decision("release-reviewer-field-missing", lambda r: r["releaseDecisionReviewers"][0].pop("reviewArtifactId"))
        rejected_decision("release-reviewer-not-gate-bound", lambda r: r["releaseDecisionReviewers"][0].__setitem__("reviewerId", "RID_PRODUCT_OWNER_OTHER"))
        rejected_decision("release-reviewer-id-not-string", lambda r: r["releaseDecisionReviewers"][0].__setitem__("reviewerId", []))
        rejected_decision("release-reviewer-no-product-owner", lambda r: r["releaseDecisionReviewers"][0].update({"role": "QUALIFIED_LEGAL_REVIEWER", "reviewerId": "RID_QUALIFIED_LEGAL_REVIEWER"}))
        rejected_decision("blanket-template-copy", lambda r: r.update(copy.deepcopy(template)))

        def rejected_capture(
            name: str,
            manifest_mutator: Any | None = None,
            record_mutator: Any | None = None,
        ) -> None:
            nonlocal decision_case_count
            decision_case_count += 1
            candidate_manifest = copy.deepcopy(valid_capture_manifest)
            candidate_record = copy.deepcopy(valid_record)
            if manifest_mutator is not None:
                manifest_mutator(candidate_manifest)
            capture_manifest_path.write_bytes(json_bytes(candidate_manifest))
            if record_mutator is not None:
                record_mutator(candidate_record)
            candidate_capture_summary, candidate_capture_errors = (
                validate_capture_manifest_binding(
                    candidate_manifest,
                    capture_manifest_path,
                    candidate_record["candidateBinding"],
                    valid_summary.get("candidateId"),
                    policy,
                    fixture_root,
                )
            )
            if (
                not candidate_capture_errors
                or candidate_capture_summary.get("accepted") is not False
            ):
                failures.append(f"final capture-set mutation accepted: {name}")

        rejected_capture(
            "capture-random-file-hash",
            record_mutator=lambda r: r["candidateBinding"].__setitem__(
                "captureSetManifestSha256", "D" * 64
            ),
        )
        rejected_capture(
            "capture-stale-archive",
            lambda m: m["candidateBinding"].__setitem__(
                "shippingArchiveManifestSha256", "D" * 64
            ),
            lambda r: r["candidateBinding"].__setitem__(
                "captureSetManifestSha256", sha256(capture_manifest_path)
            ),
        )
        rejected_capture(
            "capture-wrong-candidate",
            lambda m: m["candidateBinding"].__setitem__(
                "candidateId", "S19_WindowsShipping_OTHER"
            ),
            lambda r: r["candidateBinding"].__setitem__(
                "captureSetManifestSha256", sha256(capture_manifest_path)
            ),
        )
        rejected_capture(
            "capture-wrong-executable",
            lambda m: m["candidateBinding"].__setitem__(
                "shippingExecutableSha256", "D" * 64
            ),
            lambda r: r["candidateBinding"].__setitem__(
                "captureSetManifestSha256", sha256(capture_manifest_path)
            ),
        )
        rejected_capture(
            "capture-view-missing",
            lambda m: m["captures"].pop(),
            lambda r: r["candidateBinding"].__setitem__(
                "captureSetManifestSha256", sha256(capture_manifest_path)
            ),
        )

        missing_capture_errors: list[str] = []
        repository_relative_file(
            fixture_root,
            "Evidence/Session19/missing-capture.json",
            "--capture-manifest",
            missing_capture_errors,
        )
        decision_case_count += 1
        if not missing_capture_errors:
            failures.append("missing final capture-set manifest accepted")

        def rejected_title_inventory(
            name: str,
            inventory_mutator: Any,
        ) -> None:
            nonlocal decision_case_count
            decision_case_count += 1
            candidate_inventory = copy.deepcopy(valid_title_inventory)
            inventory_mutator(candidate_inventory)
            title_inventory_path.write_bytes(json_bytes(candidate_inventory))
            candidate_summary, candidate_errors = (
                validate_title_name_inventory_binding(
                    candidate_inventory,
                    title_inventory_path,
                    valid_record["candidateBinding"],
                    "S19_WindowsShipping_SELFTEST",
                    policy,
                    fixture_root,
                )
            )
            if not candidate_errors or candidate_summary.get("accepted") is not False:
                failures.append(f"title/name inventory mutation accepted: {name}")

        rejected_title_inventory(
            "schema",
            lambda value: value.__setitem__("schema", "wrong"),
        )
        rejected_title_inventory(
            "candidate",
            lambda value: value.__setitem__(
                "candidateId", "S19_WindowsShipping_OTHER"
            ),
        )
        rejected_title_inventory(
            "archive",
            lambda value: value["candidateBinding"]["archive"].__setitem__(
                "canonicalManifestSha256", "D" * 64
            ),
        )
        rejected_title_inventory(
            "executable",
            lambda value: value["candidateBinding"][
                "innerShippingExecutable"
            ].__setitem__("sha256", "D" * 64),
        )
        rejected_title_inventory(
            "verification-receipt",
            lambda value: value["candidateBinding"][
                "shippingVerificationReceipt"
            ].__setitem__("sha256", "D" * 64),
        )
        rejected_title_inventory(
            "artifact-id-missing",
            lambda value: value["artifactIdsAndHashes"].pop(),
        )
        rejected_title_inventory(
            "artifact-payload",
            lambda value: value["artifacts"][
                TITLE_NAME_INVENTORY_ARTIFACT_IDS[0]
            ].__setitem__("finalPublicTitle", "Invented"),
        )

        title_inventory_path.write_bytes(json_bytes(valid_title_inventory))
        decision_case_count += 1
        missing_title_errors: list[str] = []
        repository_relative_file(
            fixture_root,
            "Evidence/Session19/missing-title-name-inventory.json",
            "--title-name-inventory",
            missing_title_errors,
        )
        if not missing_title_errors:
            failures.append("missing title/name inventory accepted")

    total = (
        len(cases)
        + len(template_cases)
        + immutable_writer_cases
        + decision_case_count
    )
    minimum = policy.get("validation", {}).get("selfTestMinimumMutationCount", 24)
    if total < minimum:
        failures.append(f"only {total} mutations; minimum {minimum}")
    if failures:
        print(json.dumps({"selfTest": "FAIL", "mutationCount": total, "failures": failures}, indent=2))
        return 1
    print(json.dumps({"selfTest": "PASS", "mutationCount": total, "rejectedMutations": total}, indent=2))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--write-evidence", action="store_true")
    parser.add_argument("--require-human-approval", action="store_true")
    parser.add_argument(
        "--decision-record",
        help=(
            "repository-relative path to a deliberately completed, candidate-bound "
            "Session 19 manual release decision record"
        ),
    )
    parser.add_argument(
        "--capture-manifest",
        help=(
            "repository-relative path to the final 18-view by two-resolution "
            "capture-set manifest bound by --decision-record"
        ),
    )
    parser.add_argument(
        "--shipping-archive",
        type=Path,
        help="absolute path to the exact candidate's immutable Windows archive leaf",
    )
    parser.add_argument(
        "--external-capture-root",
        type=Path,
        help="absolute external-only directory containing the exact 36 PNG files",
    )
    parser.add_argument(
        "--capture-tool",
        type=Path,
        help="absolute path to the external capture executable or script",
    )
    parser.add_argument(
        "--capture-session-id",
        help="canonical lowercase UUIDv4 bound by the final capture manifest",
    )
    parser.add_argument(
        "--capture-method",
        choices=sorted(final_capture_manifest.CAPTURE_METHODS),
        help="external capture method bound by the final capture manifest",
    )
    parser.add_argument(
        "--title-name-inventory",
        help=(
            "repository-relative path to the exact candidate-bound Shipping "
            "title/runtime-name technical inventory referenced by gates 1 and 2"
        ),
    )
    args = parser.parse_args()

    try:
        policy = load_json(POLICY_PATH)
        template = load_json(TEMPLATE_PATH)
    except (OSError, json.JSONDecodeError) as exc:
        print(json.dumps({"status": "FAIL", "errors": [str(exc)]}, indent=2))
        return 1

    if args.self_test:
        return run_self_test(policy, template)

    errors = validate_policy(policy) + validate_template(template)
    inventory, inventory_errors = collect_inventory(policy)
    errors += inventory_errors
    errors += validate_inventory(inventory)
    for required in policy.get("requiredFiles", []):
        if required == relative(EVIDENCE_PATH) and args.write_evidence:
            continue
        if not (ROOT / required).is_file():
            errors.append(f"required file missing: {required}")

    if not args.write_evidence and EVIDENCE_PATH.is_file():
        try:
            recorded_inventory = load_json(EVIDENCE_PATH)
            if recorded_inventory != inventory:
                errors.append("recorded objective inventory is stale; rerun with --write-evidence")
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"cannot read recorded objective inventory: {exc}")

    decision_summary: dict[str, Any] | None = None
    capture_manifest_summary: dict[str, Any] | None = None
    title_name_inventory_summary: dict[str, Any] | None = None
    decision_record_path: Path | None = None
    decision_errors: list[str] = []
    if args.decision_record:
        decision_record_path = repository_relative_file(
            ROOT,
            args.decision_record,
            "--decision-record",
            decision_errors,
        )
        if decision_record_path == TEMPLATE_PATH.resolve():
            decision_errors.append("--decision-record cannot select the template")
        if decision_record_path is not None:
            try:
                decision_record = load_json(decision_record_path)
            except (OSError, json.JSONDecodeError) as exc:
                decision_errors.append(f"cannot read completed decision record: {exc}")
            else:
                _binding_errors, candidate_id = validate_shipping_candidate_binding(
                    decision_record.get("candidateBinding")
                    if type(decision_record) is dict
                    else None,
                    policy,
                    ROOT,
                )
                decision_errors.extend(_binding_errors)
                title_name_inventory_sha: str | None = None
                if not args.title_name_inventory:
                    decision_errors.append(
                        "--title-name-inventory is required with --decision-record"
                    )
                else:
                    title_name_inventory_path = repository_relative_file(
                        ROOT,
                        args.title_name_inventory,
                        "--title-name-inventory",
                        decision_errors,
                    )
                    if title_name_inventory_path is not None:
                        try:
                            title_name_inventory_record = load_json(
                                title_name_inventory_path
                            )
                        except (OSError, json.JSONDecodeError) as exc:
                            decision_errors.append(
                                f"cannot read title/name inventory: {exc}"
                            )
                        else:
                            (
                                title_name_inventory_summary,
                                title_name_inventory_errors,
                            ) = validate_title_name_inventory_binding(
                                title_name_inventory_record,
                                title_name_inventory_path,
                                decision_record.get("candidateBinding")
                                if type(decision_record) is dict
                                else None,
                                candidate_id,
                                policy,
                                ROOT,
                            )
                            decision_errors.extend(title_name_inventory_errors)
                            title_name_inventory_sha = (
                                title_name_inventory_summary.get("sha256")
                            )
                decision_summary, validation_errors = validate_decision_record(
                    decision_record,
                    policy,
                    ROOT,
                    title_name_inventory_sha,
                )
                decision_errors.extend(validation_errors)
                if not args.capture_manifest:
                    decision_errors.append(
                        "--capture-manifest is required with --decision-record"
                    )
                else:
                    capture_manifest_path = repository_relative_file(
                        ROOT,
                        args.capture_manifest,
                        "--capture-manifest",
                        decision_errors,
                    )
                    if capture_manifest_path is not None:
                        try:
                            capture_manifest = load_json(capture_manifest_path)
                        except (OSError, json.JSONDecodeError) as exc:
                            decision_errors.append(
                                f"cannot read final capture-set manifest: {exc}"
                            )
                        else:
                            (
                                capture_manifest_summary,
                                capture_manifest_errors,
                            ) = validate_capture_manifest_binding(
                                capture_manifest,
                                capture_manifest_path,
                                decision_record.get("candidateBinding")
                                if type(decision_record) is dict
                                else None,
                                decision_summary.get("candidateId")
                                if decision_summary is not None
                                else None,
                                policy,
                                ROOT,
                            )
                            decision_errors.extend(capture_manifest_errors)
                            strict_inputs = {
                                "--shipping-archive": args.shipping_archive,
                                "--external-capture-root": args.external_capture_root,
                                "--capture-tool": args.capture_tool,
                                "--capture-session-id": args.capture_session_id,
                                "--capture-method": args.capture_method,
                            }
                            missing_strict_inputs = [
                                name for name, value in strict_inputs.items()
                                if value is None
                            ]
                            if missing_strict_inputs:
                                decision_errors.append(
                                    "strict external capture validation requires: "
                                    + ", ".join(missing_strict_inputs)
                                )
                                if capture_manifest_summary is not None:
                                    capture_manifest_summary["accepted"] = False
                            elif candidate_id is not None:
                                try:
                                    strict_summary = final_capture_manifest.validate_manifest(
                                        capture_manifest,
                                        record_path=capture_manifest_path,
                                        candidate_id=candidate_id,
                                        archive=args.shipping_archive,
                                        receipt_path=(
                                            ROOT
                                            / decision_record["candidateBinding"][
                                                "shippingArchiveReceiptPath"
                                            ]
                                        ),
                                        capture_root=args.external_capture_root,
                                        capture_tool=args.capture_tool,
                                        capture_session_id=args.capture_session_id,
                                        capture_method=args.capture_method,
                                    )
                                except (
                                    final_capture_manifest.EvidenceError,
                                    OSError,
                                    ValueError,
                                ) as exc:
                                    decision_errors.append(
                                        "strict external capture validation failed: "
                                        + str(exc)
                                    )
                                    if capture_manifest_summary is not None:
                                        capture_manifest_summary["accepted"] = False
                                else:
                                    if capture_manifest_summary is not None:
                                        capture_manifest_summary[
                                            "externalFileValidation"
                                        ] = strict_summary
    else:
        if args.capture_manifest:
            decision_errors.append(
                "--capture-manifest cannot be used without --decision-record"
            )
        if args.title_name_inventory:
            decision_errors.append(
                "--title-name-inventory cannot be used without --decision-record"
            )
        for option, value in (
            ("--shipping-archive", args.shipping_archive),
            ("--external-capture-root", args.external_capture_root),
            ("--capture-tool", args.capture_tool),
            ("--capture-session-id", args.capture_session_id),
            ("--capture-method", args.capture_method),
        ):
            if value is not None:
                decision_errors.append(
                    f"{option} cannot be used without --decision-record"
                )

    if errors:
        print(json.dumps({"status": "FAIL", "releaseReady": False, "errors": sorted(set(errors))}, indent=2))
        return 1

    if decision_errors:
        result = {
            "status": "BLOCKED_MANUAL_RELEASE_DECISION_RECORD_NOT_ACCEPTED",
            "releaseReady": False,
            "manualHumanApprovalAccepted": False,
            "decisionRecordPath": (
                relative(decision_record_path)
                if decision_record_path is not None
                else args.decision_record
            ),
            "decisionRecord": decision_summary,
            "captureManifest": capture_manifest_summary,
            "titleNameInventory": title_name_inventory_summary,
            "errors": sorted(set(decision_errors)),
        }
        print(json.dumps(result, indent=2))
        return 2 if args.require_human_approval else 1

    evidence_created = False
    if args.write_evidence:
        try:
            evidence_created = write_json_once_or_verify(EVIDENCE_PATH, inventory)
        except (OSError, FileExistsError) as exc:
            print(json.dumps({
                "status": "FAIL",
                "releaseReady": False,
                "errors": [str(exc)],
            }, indent=2))
            return 1

    decision_accepted = bool(
        decision_summary is not None
        and decision_summary.get("accepted") is True
        and capture_manifest_summary is not None
        and capture_manifest_summary.get("accepted") is True
        and title_name_inventory_summary is not None
        and title_name_inventory_summary.get("accepted") is True
    )
    result = {
        "status": (
            "PASS_COMPLETED_MANUAL_RELEASE_DECISION_RECORD_ACCEPTED"
            if decision_accepted
            else policy["normalStatus"]
        ),
        "releaseStatus": (
            "MANUAL_HUMAN_APPROVAL_REQUIREMENT_SATISFIED_OTHER_RELEASE_GATES_OUT_OF_SCOPE"
            if decision_accepted
            else policy["releaseStatus"]
        ),
        "releaseReady": False,
        "manualHumanApprovalAccepted": decision_accepted,
        "humanApprovalGateCount": 8,
        "humanApprovedGateCount": 8 if decision_accepted else 0,
        "objectiveInventoryValid": True,
        "evidencePath": relative(EVIDENCE_PATH),
        "evidenceWritten": evidence_created,
        "evidenceVerifiedExisting": bool(args.write_evidence and not evidence_created),
        "decisionRecord": (
            {
                **decision_summary,
                "path": relative(decision_record_path),
                "sha256": sha256(decision_record_path),
                "captureManifest": capture_manifest_summary,
                "titleNameInventory": title_name_inventory_summary,
            }
            if decision_accepted and decision_summary is not None and decision_record_path is not None
            else None
        ),
        "blockerSummary": {
            "objectivelyNarrowed": [
                "TITLE_AND_PUBLIC_SURFACES_INVENTORIED",
                "TWELVE_DISPLAY_NAMES_INVENTORIED",
                "AUDIO_SEMANTIC_VOCABULARY_AND_MEDIA_ABSENCE_INVENTORIED",
                "CHARACTER_TECHNICAL_ASSETS_AND_PRODUCTION_MOTION_GAP_INVENTORIED",
                "ENVIRONMENT_CONTENT_ROOTS_CRYPTOGRAPHICALLY_SUMMARIZED",
                "REGRESSION_AND_ROUTE_PLAYTEST_EVIDENCE_BOUNDARIES_INVENTORIED"
            ],
            "stillHumanBlocked": [] if decision_accepted else EXPECTED_GATE_IDS,
        },
    }
    print(json.dumps(result, indent=2))
    if args.require_human_approval and not decision_accepted:
        return policy["validation"]["humanApprovalRequiredExpectedExitCode"]
    return 0


if __name__ == "__main__":
    sys.exit(main())
