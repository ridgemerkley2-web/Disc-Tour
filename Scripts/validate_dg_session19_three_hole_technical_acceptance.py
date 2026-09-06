#!/usr/bin/env python3
"""Validate candidate-bound v0.5 three-hole evidence without inferring play.

Version 2 distinguishes screenshot-set integrity from single-round continuity.
Screenshots are opaque candidate-bound artifacts: their names, hashes, and PNG
dimensions cannot prove what happened in the game.  A separately supplied JSON
checkpoint chain can validate its own structure and bindings, but it is not
trusted gameplay provenance and can never promote acceptance.  Promotion is
available only from the immutable canonical JSONL journal emitted by the
Shipping game into the fresh UserDir and bound by nonce, candidate identity,
launch record, technical manifest, raw bytes, and the process capture window.
This is bounded operational process evidence, not cryptographic process
attestation or hostile-same-user tamper resistance.

The historical version-1 policy and receipts remain available only through an
explicit, read-only legacy revalidation mode.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
LEGACY_POLICY_PATH = ROOT / "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicy.json"
DEFAULT_POLICY_PATH = ROOT / "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicyV2.json"
LEGACY_STATE = "PASS_CANDIDATE_BOUND_FRESH_INSTALL_THREE_HOLE_TECHNICAL_ACCEPTANCE"
SCREENSHOT_ONLY_STATE = "PASS_CANDIDATE_BOUND_SCREENSHOT_SET_INTEGRITY_SINGLE_ROUND_UNPROVEN"
CHECKPOINT_INTEGRITY_STATE = (
    "PASS_CANDIDATE_BOUND_CHECKPOINT_CHAIN_INTEGRITY_RUNTIME_PROVENANCE_UNPROVEN"
)
RUNTIME_JOURNAL_STATE = (
    "PASS_CANDIDATE_BOUND_FRESH_INSTALL_THREE_HOLE_RUNTIME_JOURNAL_VERIFIED"
)
RELEASE_STATE = "BLOCKED_PENDING_HUMAN_PERFORMANCE_PROVENANCE_LEGAL_AND_RELEASE_APPROVALS"
CANDIDATE_RE = re.compile(r"S19_WindowsShipping_[A-Za-z0-9_-]+")
UUID_RE = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
LEGACY_RECEIPT_SCHEMA = "DiscGolfTour.Session19ThreeHoleTechnicalAcceptanceReceipt.v1"
RECEIPT_SCHEMA = "DiscGolfTour.Session19ThreeHoleTechnicalAcceptanceReceipt.v2"
CHECKPOINT_SCHEMA = "DiscGolfTour.Session19ThreeHoleCheckpointChain.v1"
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_USERDIR_PATH = (
    "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
)
UUID4_RE = re.compile(
    r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
)
EVIDENCE_TRUST_MODEL = (
    "BOUNDED_OPERATIONAL_PROCESS_EVIDENCE_NOT_CRYPTOGRAPHIC_ATTESTATION"
)

LEGACY_EXPECTED_FALSE_CLAIMS = (
    "visualQualityApproval",
    "accessibilityApproval",
    "humanPlayFeelApproval",
    "manualGameplayAcceptance",
    "productOwnerApproval",
    "performanceAcceptance",
    "soakAcceptance",
    "provenanceApproval",
    "legalApproval",
    "distributionClearance",
    "releaseApproval",
    "releaseReady",
)
EXPECTED_FALSE_CLAIMS = LEGACY_EXPECTED_FALSE_CLAIMS + (
    "cryptographicProcessAttestation",
    "tamperProofEvidence",
    "hostileSameUserForgeryResistance",
)

EXPECTED_SEMANTICS = [
    "FRESH_ROUND_ENTRY_HOLE_1_SHOT_1",
    "HOLE_1_TEE_SHOT_1",
    "HOLE_1_RECOVERED_LIE_SHOT_2",
    "HOLE_1_COMPLETE",
    "HOLE_2_TEE_SHOT_1",
    "HOLE_2_RECOVERED_LIE_SHOT_2",
    "HOLE_2_COMPLETE",
    "HOLE_3_TEE_SHOT_1",
    "HOLE_3_RECOVERED_LIE_SHOT_2",
    "FINAL_SCORECARD_THREE_OF_THREE",
]

EXPECTED_SCREENSHOT_PATHS = [
    "Screenshots/001_front_end.png",
    "Screenshots/002_hole1_tee.png",
    "Screenshots/003_hole1_recovered_lie.png",
    "Screenshots/004_hole1_complete.png",
    "Screenshots/005_hole2_tee.png",
    "Screenshots/006_hole2_recovered_lie.png",
    "Screenshots/007_hole2_complete.png",
    "Screenshots/008_hole3_tee.png",
    "Screenshots/009_hole3_recovered_lie.png",
    "Screenshots/010_final_scorecard.png",
]

CHECKPOINT_EVENT_NAMES = [
    "ROUND_STARTED",
    "HOLE_1_TEE",
    "HOLE_1_LIE",
    "HOLE_1_COMPLETED",
    "HOLE_2_TEE",
    "HOLE_2_LIE",
    "HOLE_2_COMPLETED",
    "HOLE_3_TEE",
    "HOLE_3_LIE",
    "HOLE_3_COMPLETED",
    "ROUND_COMPLETED",
    "FINAL_SCORECARD",
]
CHECKPOINT_COMPLETED_HOLES = [0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3]
CHECKPOINT_HOLE_NUMBERS = [None, 1, 1, 1, 2, 2, 2, 3, 3, 3, None, None]


class ContractError(ValueError):
    pass


def _reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ContractError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def _strict_json(data: bytes, label: str) -> Any:
    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=_reject_duplicates,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ContractError(f"non-finite JSON value in {label}: {value}")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ContractError(f"invalid JSON: {label}: {exc}") from exc


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _stat_fingerprint(value: os.stat_result) -> tuple[int, int, int, int, int, int]:
    return (
        stat.S_IFMT(value.st_mode),
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
    )


def _same_open_file(path_stat: os.stat_result, descriptor_stat: os.stat_result) -> bool:
    if path_stat.st_ino and descriptor_stat.st_ino:
        return (
            path_stat.st_dev == descriptor_stat.st_dev
            and path_stat.st_ino == descriptor_stat.st_ino
        )
    return (
        stat.S_IFMT(path_stat.st_mode) == stat.S_IFMT(descriptor_stat.st_mode)
        and path_stat.st_size == descriptor_stat.st_size
        and path_stat.st_mtime_ns == descriptor_stat.st_mtime_ns
        and path_stat.st_ctime_ns == descriptor_stat.st_ctime_ns
    )


def _read_with_stat(
    path: Path, overrides: dict[Path, bytes]
) -> tuple[bytes, os.stat_result]:
    resolved = path.resolve()
    if resolved in overrides:
        return overrides[resolved], path.stat()
    before_path = os.lstat(path)
    if not stat.S_ISREG(before_path.st_mode) or path.is_symlink() or _is_reparse(path):
        raise ContractError(f"file is irregular while read: {path.name}")
    with path.open("rb") as stream:
        before_descriptor = os.fstat(stream.fileno())
        if not _same_open_file(before_path, before_descriptor):
            raise ContractError(f"file changed while opened: {path.name}")
        data = stream.read()
        after_descriptor = os.fstat(stream.fileno())
    after_path = os.lstat(path)
    if (
        len(data) != before_descriptor.st_size
        or _stat_fingerprint(before_descriptor) != _stat_fingerprint(after_descriptor)
        or _stat_fingerprint(before_path) != _stat_fingerprint(after_path)
        or not _same_open_file(after_path, after_descriptor)
        or path.is_symlink()
        or _is_reparse(path)
    ):
        raise ContractError(f"file changed while read: {path.name}")
    return data, after_path


def _read(path: Path, overrides: dict[Path, bytes]) -> bytes:
    return _read_with_stat(path, overrides)[0]


def _load(path: Path, overrides: dict[Path, bytes]) -> Any:
    return _strict_json(_read(path, overrides), path.name)


def _binding(path: Path, overrides: dict[Path, bytes], relative: str) -> dict[str, Any]:
    data = _read(path, overrides)
    return {"path": relative, "bytes": len(data), "sha256": _sha256(data)}


def _canonical_relative(value: str) -> str:
    normalized = value.replace("\\", "/")
    if (
        not normalized
        or normalized.startswith("/")
        or re.match(r"^[A-Za-z]:", normalized)
        or any(part in {"", ".", ".."} for part in normalized.split("/"))
    ):
        raise ContractError(f"non-canonical relative path: {value!r}")
    return normalized


def _is_reparse(path: Path) -> bool:
    attributes = getattr(path.lstat(), "st_file_attributes", 0)
    return bool(attributes & 0x400)


def _require_regular_file_within(path: Path, parent: Path, label: str) -> None:
    """Reject missing, redirected, or out-of-bound evidence files."""
    try:
        parent_resolved = parent.resolve(strict=True)
        path_resolved = path.resolve(strict=True)
    except OSError as exc:
        raise ContractError(f"{label} is missing: {path}") from exc
    if parent_resolved != path_resolved and parent_resolved not in path_resolved.parents:
        raise ContractError(f"{label} resolves outside its evidence root")
    if path.is_symlink() or _is_reparse(path):
        raise ContractError(f"{label} is a redirected file")
    try:
        relative = path.absolute().relative_to(parent.absolute())
        current = parent
    except ValueError:
        relative = path_resolved.relative_to(parent_resolved)
        current = parent_resolved
    if current.is_symlink() or _is_reparse(current) or not current.is_dir():
        raise ContractError(f"{label} evidence root is irregular")
    for part in relative.parts:
        current = current / part
        if current.is_symlink() or _is_reparse(current):
            raise ContractError(f"{label} contains a redirected path component")
    if not path.is_file():
        raise ContractError(f"{label} must be a regular file")


def _require_directory_within(path: Path, parent: Path, label: str) -> None:
    """Require a non-reparse directory and every component below a boundary."""
    try:
        parent_resolved = parent.resolve(strict=True)
        path_resolved = path.resolve(strict=True)
    except OSError as exc:
        raise ContractError(f"{label} is missing: {path}") from exc
    if parent_resolved != path_resolved and parent_resolved not in path_resolved.parents:
        raise ContractError(f"{label} resolves outside its boundary")
    if path.is_symlink() or _is_reparse(path):
        raise ContractError(f"{label} is a redirected directory")
    try:
        relative = path.absolute().relative_to(parent.absolute())
        current = parent
    except ValueError:
        relative = path_resolved.relative_to(parent_resolved)
        current = parent_resolved
    if current.is_symlink() or _is_reparse(current) or not current.is_dir():
        raise ContractError(f"{label} boundary is irregular")
    for part in relative.parts:
        current = current / part
        if current.is_symlink() or _is_reparse(current) or not current.is_dir():
            raise ContractError(f"{label} contains an irregular path component")


def _hash_file(path: Path) -> tuple[int, str]:
    before_path = os.lstat(path)
    if not stat.S_ISREG(before_path.st_mode) or path.is_symlink() or _is_reparse(path):
        raise ContractError(f"file is irregular while hashed: {path.name}")
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        before_descriptor = os.fstat(handle.fileno())
        if not _same_open_file(before_path, before_descriptor):
            raise ContractError(f"file changed while opened for hash: {path.name}")
        for chunk in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
        after_descriptor = os.fstat(handle.fileno())
    after_path = os.lstat(path)
    if (
        _stat_fingerprint(before_descriptor) != _stat_fingerprint(after_descriptor)
        or _stat_fingerprint(before_path) != _stat_fingerprint(after_path)
        or not _same_open_file(after_path, after_descriptor)
        or path.is_symlink()
        or _is_reparse(path)
    ):
        raise ContractError(f"file changed while hashed: {path.name}")
    return after_path.st_size, digest.hexdigest().upper()


def _collect_archive(archive: Path) -> tuple[list[dict[str, Any]], str]:
    if not archive.is_dir() or archive.is_symlink() or _is_reparse(archive):
        raise ContractError("archive root must be a regular non-reparse directory")
    root = archive.resolve(strict=True)
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for current, directories, files in os.walk(archive, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if child.is_symlink() or _is_reparse(child):
                raise ContractError("archive contains a reparse directory")
        for name in files:
            child = current_path / name
            relative = _canonical_relative(child.relative_to(archive).as_posix())
            if relative.casefold() in seen:
                raise ContractError("archive contains a case-insensitive duplicate")
            seen.add(relative.casefold())
            if child.is_symlink() or _is_reparse(child) or not child.is_file():
                raise ContractError("archive contains an irregular file")
            resolved = child.resolve(strict=True)
            if root not in resolved.parents:
                raise ContractError("archive entry resolves outside the archive")
            byte_count, digest = _hash_file(child)
            records.append({"path": relative, "bytes": byte_count, "sha256": digest})
    records.sort(key=lambda value: (value["path"].casefold(), value["path"]))
    payload = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return records, _sha256(payload)


def _png_dimensions(data: bytes, label: str) -> tuple[int, int]:
    if len(data) < 33 or data[:8] != PNG_SIGNATURE:
        raise ContractError(f"milestone is not a valid PNG signature: {label}")
    length = struct.unpack(">I", data[8:12])[0]
    if length != 13 or data[12:16] != b"IHDR":
        raise ContractError(f"milestone has no canonical IHDR: {label}")
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        raise ContractError(f"milestone dimensions are invalid: {label}")
    return width, height


def _claim_boundary(value: Any, technical_key: str) -> None:
    false_keys = {
        "accessibilityApproval",
        "humanPlayFeelApproval",
        "legalApproval",
        "manualGameplayAcceptance",
        "releaseApproval",
        "releaseReady",
        "visualProductApproval",
    }
    expected_keys = false_keys | {technical_key, "manualReviewedUtc", "manualReviewerId"}
    if not isinstance(value, dict) or set(value) != expected_keys:
        raise ContractError("claim boundary keys differ")
    if value.get(technical_key) is not True:
        raise ContractError(f"claim boundary must preserve {technical_key}=true")
    for key in false_keys:
        if value.get(key) is not False:
            raise ContractError(f"claim boundary must preserve {key}=false")
    if value.get("manualReviewedUtc") is not None or value.get("manualReviewerId") is not None:
        raise ContractError("technical collection must not invent a manual reviewer")


def _validate_policy_v1(policy: Any) -> None:
    if not isinstance(policy, dict):
        raise ContractError("policy root must be an object")
    expected_head = {
        "schema": "DiscGolfTour.Session19ThreeHoleTechnicalAcceptancePolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "windows_shipping_v05_candidate_bound_three_hole_technical_acceptance_v1",
        "receiptPattern": "Evidence/Session19/ThreeHoleTechnicalAcceptance-{candidateId}.json",
        "selfTestMinimumMutationCount": 24,
    }
    for key, expected in expected_head.items():
        if policy.get(key) != expected:
            raise ContractError(f"policy.{key} differs")
    candidate = policy.get("candidate")
    if (
        not isinstance(candidate, dict)
        or not CANDIDATE_RE.fullmatch(str(candidate.get("candidateId", "")))
        or not UUID_RE.fullmatch(str(candidate.get("userDirToken", "")))
        or candidate.get("externalRunToken")
        != f"{candidate.get('candidateId')}_{candidate.get('userDirToken')}"
        or candidate.get("platform") != "Windows"
        or candidate.get("configuration") != "Shipping"
        or candidate.get("milestone") != "v0.5"
    ):
        raise ContractError("policy candidate identity is malformed")
    roles = [item.get("role") for item in policy.get("projectEvidenceBindings", [])]
    if roles != [
        "SHIPPING_CANDIDATE_VERIFICATION",
        "CANDIDATE_CONTENT_AUDIT",
        "SHIPPING_BINARY_POLICY",
        "EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION",
        "FRESH_USERDIR_VALIDATION",
    ]:
        raise ContractError("policy project evidence roles differ")
    external_roles = [item.get("role") for item in policy.get("externalEvidenceBindings", [])]
    if external_roles != [
        "LAUNCH_RECORD",
        "TECHNICAL_EVIDENCE_MANIFEST",
        "TECHNICAL_EVIDENCE_MANIFEST_SIDECAR",
    ]:
        raise ContractError("policy external evidence roles differ")
    milestones = policy.get("requiredMilestones")
    if (
        not isinstance(milestones, list)
        or len(milestones) != 10
        or [item.get("semantic") for item in milestones] != EXPECTED_SEMANTICS
        or len({str(item.get("path", "")).casefold() for item in milestones}) != 10
    ):
        raise ContractError("policy milestone sequence differs")
    review = policy.get("artifactSemanticReview")
    if (
        not isinstance(review, dict)
        or review.get("method") != "AGENT_VISUAL_INSPECTION_OF_EXACT_SHA256_BOUND_PNGS"
        or review.get("technicalArtifactObservationOnly") is not True
        or review.get("visualQualityApproval") is not False
    ):
        raise ContractError("policy semantic-review boundary differs")
    observations = review.get("observations", {})
    if (
        observations.get("freshRoundEntryShowsHole1Shot1") is not True
        or observations.get("eachHoleHasTeeAndRecoveredLie") is not True
        or observations.get("hole1CompleteShown") is not True
        or observations.get("hole2CompleteShown") is not True
        or observations.get("finalScorecardShown") is not True
        or observations.get("finalScorecardCompletedHoles") != 3
        or observations.get("finalScorecardTotalHoles") != 3
        or observations.get("finalScorecardStrokes") != 20
        or observations.get("finalScorecardPenalties") != 3
        or observations.get("finalScorecardToPar") != 9
        or observations.get("finalScorecardHoleRows") != [
            {"hole": 1, "par": 3, "strokes": 4, "penalties": 1, "toPar": 1},
            {"hole": 2, "par": 4, "strokes": 10, "penalties": 1, "toPar": 6},
            {"hole": 3, "par": 4, "strokes": 6, "penalties": 1, "toPar": 2},
        ]
    ):
        raise ContractError("policy scorecard observation differs")
    facts = policy.get("requiredTechnicalFacts", {})
    if any(facts.get(key) is not True for key in (
        "emptyExternalUserDirBeforeLaunch",
        "externalUserDirBoundaryValidated",
        "candidateArchivePreservedAcrossRun",
        "candidateArchiveAndExecutableIdentityMatched",
        "threeHoleCompletionTechnicallyObserved",
        "cleanProcessExit",
    )) or facts.get("processExitCode") != 0 or facts.get("processTimedOut") is not False \
            or facts.get("exactMilestonePngCount") != 10:
        raise ContractError("policy technical facts differ")
    performance = policy.get("performanceBoundary", {})
    if (
        performance.get("presentMonRequested") is not False
        or performance.get("presentMonRows") != 0
        or performance.get("quantitativeTimingMetricsAvailable") is not False
        or performance.get("performanceAcceptance") is not False
        or performance.get("soakAcceptance") is not False
        or performance.get("remainingEvidence") != [
            "QUANTITATIVE_FRAME_TIME_AND_HITCH_CAPTURE",
            "BOUNDED_DURATION_SOAK_RUN",
        ]
    ):
        raise ContractError("policy performance/soak boundary differs")
    claims = policy.get("claimBoundary", {})
    if claims.get("technicalThreeHoleAcceptance") is not True:
        raise ContractError("technical acceptance must be true")
    for key in LEGACY_EXPECTED_FALSE_CLAIMS:
        if claims.get(key) is not False:
            raise ContractError(f"policy must preserve {key}=false")


def _validate_project_receipts(
    policy: dict[str, Any], overrides: dict[Path, bytes]
) -> list[dict[str, Any]]:
    candidate = policy["candidate"]
    candidate_id = candidate["candidateId"]
    results: list[dict[str, Any]] = []
    loaded: dict[str, Any] = {}
    for expected in policy["projectEvidenceBindings"]:
        path = ROOT / expected["path"]
        actual = _binding(path, overrides, expected["path"])
        if actual != {key: expected[key] for key in ("path", "bytes", "sha256")}:
            raise ContractError(f"project evidence identity differs: {expected['role']}")
        loaded[expected["role"]] = _load(path, overrides)
        results.append({"role": expected["role"], **actual})

    verification = loaded["SHIPPING_CANDIDATE_VERIFICATION"]
    if (
        verification.get("schema") != "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
        or verification.get("runId") != candidate_id
        or verification.get("state") != "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
        or verification.get("verificationMode") != "DIRECT_BUILD_PASS"
        or verification.get("archiveContract", {}).get("allRequiredFilesPresent") is not True
        or verification.get("innerShippingExecutable", {}).get("bytes") != candidate["shippingExecutableBytes"]
        or verification.get("innerShippingExecutable", {}).get("sha256") != candidate["shippingExecutableSha256"]
        or verification.get("shippingPluginCapabilities", {}).get("finalArchiveCanonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
        or verification.get("releaseReady") is not False
    ):
        raise ContractError("Shipping candidate verification boundary differs")

    content = loaded["CANDIDATE_CONTENT_AUDIT"]
    if (
        content.get("schema") != "DiscGolfTour.Session19CandidateContentAuditReceipt.v1"
        or content.get("runId") != candidate_id
        or content.get("state") != "PASS_BOUNDED_STAGED_CONTENT_AUDIT"
        or content.get("archive", {}).get("bytes") != candidate["archiveBytes"]
        or content.get("archive", {}).get("fileCount") != candidate["archiveFileCount"]
        or content.get("archive", {}).get("canonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
        or content.get("requiredThreeHoleData", {}).get("expectedHoleNumbers") != [1, 2, 3]
        or content.get("issues") != []
        or content.get("releaseBoundary", {}).get("releaseReady") is not False
    ):
        raise ContractError("candidate content receipt boundary differs")

    binary = loaded["SHIPPING_BINARY_POLICY"]
    if (
        binary.get("schema") != "DiscGolfTour.Session19ShippingBinaryEvidence.v1"
        or binary.get("state") != "PASS_BINARY_MARKER_POLICY_ONLY"
        or binary.get("passed") is not True
        or str(binary.get("input", {}).get("sha256", "")).upper()
        != candidate["shippingExecutableSha256"]
        or binary.get("input", {}).get("bytes") != candidate["shippingExecutableBytes"]
        or binary.get("errors") != []
        or binary.get("releaseReady") is not False
    ):
        raise ContractError("Shipping binary receipt boundary differs")

    external = loaded["EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION"]
    if (
        external.get("schema") != "DiscGolfTour.Session19ExternalTechnicalEvidenceValidation.v1"
        or external.get("candidateId") != candidate_id
        or external.get("state") != "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
        or external.get("counts") != {"presentMonRows": 0, "screenshots": 10}
        or external.get("failures") != []
        or external.get("manifest", {}).get("sha256")
        != policy["externalEvidenceBindings"][1]["sha256"]
    ):
        raise ContractError("external technical validation receipt differs")
    external_claims = external.get("claimBoundary", {})
    if external_claims.get("technicalArtifactValidationOnly") is not True:
        raise ContractError("external validation technical-only boundary differs")
    for key in (
        "accessibilityApproval", "humanPlayFeelApproval", "legalApproval",
        "manualGameplayAcceptance", "releaseApproval", "releaseReady",
        "visualProductApproval",
    ):
        if external_claims.get(key) is not False:
            raise ContractError("external validation overclaims approval")

    fresh = loaded["FRESH_USERDIR_VALIDATION"]
    if (
        fresh.get("schema") != "DiscGolfTour.Session19FreshUserDirValidation.v1"
        or fresh.get("candidateId") != candidate_id
        or fresh.get("userDirToken") != candidate["userDirToken"]
        or fresh.get("state") != "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
        or fresh.get("failures") != []
        or fresh.get("counts", {}).get("forbiddenArtifacts") != 0
        or fresh.get("counts", {}).get("unknownFiles") != 0
        or fresh.get("releaseBoundary", {}).get("technicalUserDirAcceptanceOnly") is not True
        or fresh.get("releaseBoundary", {}).get("releaseReadinessClaimed") is not False
    ):
        raise ContractError("fresh UserDir validation receipt differs")
    return results


def _validate_user_dir(
    user_dir: Path, policy: dict[str, Any], overrides: dict[Path, bytes]
) -> dict[str, Any]:
    if user_dir.name != policy["candidate"]["userDirToken"]:
        raise ContractError("UserDir path does not match the candidate token")
    if not user_dir.is_dir() or user_dir.is_symlink() or _is_reparse(user_dir):
        raise ContractError("UserDir is missing or irregular")
    fresh_binding = next(
        item for item in policy["projectEvidenceBindings"] if item["role"] == "FRESH_USERDIR_VALIDATION"
    )
    fresh = _load(ROOT / fresh_binding["path"], overrides)
    observed = sorted(fresh["observedFiles"], key=lambda item: item["relativePath"].casefold())
    actual: list[dict[str, Any]] = []
    for current, directories, files in os.walk(user_dir, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if child.is_symlink() or _is_reparse(child):
                raise ContractError("UserDir contains a reparse directory")
        for name in files:
            child = current_path / name
            if child.is_symlink() or _is_reparse(child) or not child.is_file():
                raise ContractError("UserDir contains an irregular file")
            relative = _canonical_relative(child.relative_to(user_dir).as_posix())
            byte_count, digest = _hash_file(child)
            actual.append({"relativePath": relative, "bytes": byte_count, "sha256": digest.lower()})
    actual.sort(key=lambda item: item["relativePath"].casefold())
    expected = [
        {"relativePath": item["relativePath"], "bytes": item["bytes"], "sha256": item["sha256"]}
        for item in observed
    ]
    if actual != expected:
        raise ContractError("live UserDir inventory differs from the immutable allowlist receipt")
    return {
        "token": policy["candidate"]["userDirToken"],
        "hostPathRecorded": False,
        "emptyBeforeLaunch": True,
        "externalBoundaryValidated": True,
        "postRunAllowlistPass": True,
        "observedFileCount": len(actual),
        "candidateUserDirBindingSha256": fresh["candidateUserDirBindingSha256"].upper(),
    }


def _validate_external(
    external_run: Path, policy: dict[str, Any], overrides: dict[Path, bytes]
) -> tuple[dict[str, Any], dict[str, Any]]:
    candidate = policy["candidate"]
    if external_run.name != candidate["externalRunToken"]:
        raise ContractError("external run directory does not match the candidate/token binding")
    if not external_run.is_dir() or external_run.is_symlink() or _is_reparse(external_run):
        raise ContractError("external run directory is missing or irregular")
    bindings: list[dict[str, Any]] = []
    values: dict[str, Any] = {}
    for expected in policy["externalEvidenceBindings"]:
        path = external_run / expected["path"]
        actual = _binding(path, overrides, expected["path"])
        if actual != {key: expected[key] for key in ("path", "bytes", "sha256")}:
            raise ContractError(f"external evidence identity differs: {expected['role']}")
        bindings.append({"role": expected["role"], **actual})
        if expected["path"].endswith(".json"):
            values[expected["role"]] = _load(path, overrides)

    sidecar = _read(external_run / "technical-evidence-manifest.sha256", overrides).decode("ascii")
    expected_sidecar = (
        policy["externalEvidenceBindings"][1]["sha256"]
        + " *technical-evidence-manifest.json\n"
    )
    if sidecar.replace("\r\n", "\n") != expected_sidecar:
        raise ContractError("technical manifest SHA sidecar differs")

    launch = values["LAUNCH_RECORD"]
    if (
        launch.get("schema") != "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1"
        or launch.get("candidateId") != candidate["candidateId"]
        or launch.get("userDirToken") != candidate["userDirToken"]
        or launch.get("expectedArchiveManifestSha256") != candidate["archiveCanonicalManifestSha256"]
        or launch.get("observedArchiveManifestSha256") != candidate["archiveCanonicalManifestSha256"]
        or launch.get("expectedExeSha256") != candidate["shippingExecutableSha256"]
        or launch.get("observedExeSha256") != candidate["shippingExecutableSha256"]
        or launch.get("hostPathsRecorded") is not False
    ):
        raise ContractError("launch record candidate binding differs")
    _claim_boundary(launch.get("claimBoundary"), "technicalArtifactCollectionOnly")

    manifest = values["TECHNICAL_EVIDENCE_MANIFEST"]
    manifest_candidate = manifest.get("candidate", {})
    if (
        manifest.get("schema") != "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
        or manifest.get("state") != "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION"
        or manifest_candidate.get("candidateId") != candidate["candidateId"]
        or manifest_candidate.get("archiveBefore") != manifest_candidate.get("archiveAfter")
        or manifest_candidate.get("archiveAfter") != {
            "bytes": candidate["archiveBytes"],
            "canonicalManifestSha256": candidate["archiveCanonicalManifestSha256"],
            "fileCount": candidate["archiveFileCount"],
        }
        or manifest_candidate.get("archivePreserved") is not True
        or manifest_candidate.get("observedExeSha256") != candidate["shippingExecutableSha256"]
    ):
        raise ContractError("technical manifest candidate preservation differs")
    _claim_boundary(manifest.get("claimBoundary"), "technicalArtifactCollectionOnly")
    process = manifest.get("process", {})
    if process.get("exitCode") != 0 or process.get("timedOut") is not False:
        raise ContractError("external process did not exit cleanly")
    user = manifest.get("userDir", {})
    if (
        user.get("token") != candidate["userDirToken"]
        or user.get("emptyBeforeLaunch") is not True
        or user.get("externalBoundaryValidated") is not True
        or user.get("hostPathRecorded") is not False
        or user.get("reparsePointsRejected") is not True
    ):
        raise ContractError("manifest does not prove empty-before-launch external UserDir")
    present = manifest.get("evidence", {}).get("presentMon", {})
    if present.get("requested") is not False or present.get("rowCount") != 0:
        raise ContractError("performance evidence boundary differs")

    manifest_screens = manifest.get("evidence", {}).get("screenshots")
    if not isinstance(manifest_screens, list) or len(manifest_screens) != 10:
        raise ContractError("technical manifest must bind ten screenshots")
    actual_names = sorted(
        path.relative_to(external_run).as_posix()
        for path in (external_run / "Screenshots").iterdir()
        if path.is_file()
    )
    expected_names = [item["path"] for item in policy["requiredMilestones"]]
    if actual_names != expected_names:
        raise ContractError("screenshot directory differs from the exact milestone set")
    milestones: list[dict[str, Any]] = []
    for expected, manifest_item in zip(policy["requiredMilestones"], manifest_screens):
        path = external_run / expected["path"]
        data = _read(path, overrides)
        width, height = _png_dimensions(data, expected["path"])
        actual = {
            "path": expected["path"],
            "bytes": len(data),
            "sha256": _sha256(data),
            "width": width,
            "height": height,
            "semantic": expected["semantic"],
        }
        if actual != expected:
            raise ContractError(f"milestone identity differs: {expected['path']}")
        if (
            manifest_item.get("relativePath") != expected["path"]
            or manifest_item.get("bytes") != expected["bytes"]
            or manifest_item.get("sha256") != expected["sha256"]
            or manifest_item.get("width") != expected["width"]
            or manifest_item.get("height") != expected["height"]
            or manifest_item.get("modifiedWithinRunWindow") is not True
        ):
            raise ContractError(f"manifest milestone binding differs: {expected['path']}")
        milestones.append(actual)
    return {"bindings": bindings, "milestones": milestones}, manifest


def _candidate_receipt_v1(
    candidate_id: str,
    archive: Path,
    external_run: Path,
    user_dir: Path,
    policy_path: Path = LEGACY_POLICY_PATH,
    overrides: dict[Path, bytes] | None = None,
) -> dict[str, Any]:
    overrides = overrides or {}
    policy_bytes = _read(policy_path, overrides)
    policy = _strict_json(policy_bytes, policy_path.name)
    _validate_policy_v1(policy)
    candidate = policy["candidate"]
    if candidate_id != candidate["candidateId"]:
        raise ContractError("candidate ID differs from the candidate-bound policy")

    project_bindings = _validate_project_receipts(policy, overrides)
    archive_records, archive_sha = _collect_archive(archive)
    if (
        len(archive_records) != candidate["archiveFileCount"]
        or sum(item["bytes"] for item in archive_records) != candidate["archiveBytes"]
        or archive_sha != candidate["archiveCanonicalManifestSha256"]
    ):
        raise ContractError("live final archive identity differs from the policy")
    executable = next(
        (item for item in archive_records if item["path"] == candidate["shippingExecutablePath"]),
        None,
    )
    if executable != {
        "path": candidate["shippingExecutablePath"],
        "bytes": candidate["shippingExecutableBytes"],
        "sha256": candidate["shippingExecutableSha256"],
    }:
        raise ContractError("live Shipping executable identity differs")
    external, manifest = _validate_external(external_run, policy, overrides)
    user = _validate_user_dir(user_dir, policy, overrides)

    return {
        "schema": LEGACY_RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "verifiedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": LEGACY_STATE,
        "policy": {
            "path": "Config/DG_Session19ThreeHoleTechnicalAcceptancePolicy.json",
            "bytes": len(policy_bytes),
            "sha256": _sha256(policy_bytes),
        },
        "candidate": {
            "archive": {
                "hostPathRecorded": False,
                "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
                "bytes": candidate["archiveBytes"],
                "fileCount": candidate["archiveFileCount"],
                "canonicalManifestSha256": archive_sha,
                "liveRevalidationPass": True,
            },
            "shippingExecutable": executable,
        },
        "projectEvidenceBindings": project_bindings,
        "externalEvidence": {
            "hostPathRecorded": False,
            "recoveryLocationToken": (
                "DGTOUR_ACCEPTANCE/Session19ExternalEvidence/" + candidate["externalRunToken"]
            ),
            **external,
        },
        "freshUserDir": user,
        "process": {
            "exitCode": manifest["process"]["exitCode"],
            "timedOut": manifest["process"]["timedOut"],
            "cleanExit": True,
        },
        "technicalAcceptance": {
            "pass": True,
            "freshInstallBoundaryPass": True,
            "freshRoundEntryToFinalScorecardSequencePass": True,
            "eachHoleTeeAndRecoveredLieObserved": True,
            "threeHoleCompletionObserved": True,
            "finalScorecardCompletedHoles": 3,
            "finalScorecardTotalHoles": 3,
            "semanticReview": policy["artifactSemanticReview"],
        },
        "performanceBoundary": policy["performanceBoundary"],
        "claimBoundary": policy["claimBoundary"],
        "blockerClosed": False,
        "releaseReady": False,
        "remainingEvidence": [
            "VISUAL_QUALITY_PRODUCT_OWNER_APPROVAL",
            "ACCESSIBILITY_APPROVAL",
            "HUMAN_PLAY_FEEL_AND_MANUAL_GAMEPLAY_ACCEPTANCE",
            "QUANTITATIVE_PERFORMANCE_AND_SOAK_ACCEPTANCE",
            "WHOLE_ARCHIVE_PROVENANCE_APPROVAL",
            "LEGAL_AND_DISTRIBUTION_CLEARANCE",
            "RELEASE_APPROVAL",
        ],
        "receiptPath": policy["receiptPattern"].format(candidateId=candidate_id),
    }


def _comparable(value: dict[str, Any]) -> dict[str, Any]:
    result = copy.deepcopy(value)
    result.pop("verifiedUtc", None)
    return result


def _valid_utc(value: Any) -> bool:
    if not isinstance(value, str) or not value.endswith("Z"):
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def _parse_utc(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ContractError(f"{label} must be an ISO-8601 UTC timestamp ending in Z")
    try:
        result = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise ContractError(f"{label} is not a valid UTC timestamp") from exc
    if result.tzinfo != timezone.utc:
        raise ContractError(f"{label} is not UTC")
    return result


def _canonical_jsonl_records(data: bytes, maximum_bytes: int) -> list[dict[str, Any]]:
    if not data or len(data) > maximum_bytes:
        raise ContractError("runtime journal byte count is empty or exceeds policy")
    if data.startswith(b"\xef\xbb\xbf") or b"\r" in data or not data.endswith(b"\n"):
        raise ContractError("runtime journal must be BOM-free canonical LF JSONL")
    records: list[dict[str, Any]] = []
    for index, raw_line in enumerate(data.splitlines(keepends=True), start=1):
        if not raw_line.endswith(b"\n") or raw_line == b"\n":
            raise ContractError(f"runtime journal line {index} is empty or unterminated")
        body = raw_line[:-1]
        try:
            body.decode("ascii")
        except UnicodeDecodeError as exc:
            raise ContractError(
                f"runtime journal line {index} is not ensure-ASCII canonical JSON"
            ) from exc
        value = _strict_json(body, f"runtime journal line {index}")
        if not isinstance(value, dict):
            raise ContractError(f"runtime journal line {index} must be an object")
        canonical = (
            json.dumps(
                value,
                sort_keys=True,
                separators=(",", ":"),
                ensure_ascii=True,
            ).encode("ascii")
            + b"\n"
        )
        if raw_line != canonical:
            raise ContractError(f"runtime journal line {index} is not canonical JSON")
        records.append(value)
    return records


def _write_immutable(path: Path, value: Any) -> None:
    _require_directory_within(path.parent, ROOT, "receipt output directory")
    payload = json.dumps(value, indent=2, ensure_ascii=False).encode("utf-8") + b"\n"
    try:
        with path.open("xb") as handle:
            handle.write(payload)
    except FileExistsError as exc:
        raise ContractError(f"receipt already exists: {path}") from exc


def _expected_receipt_v1(path: Path, live: dict[str, Any]) -> None:
    expected = _strict_json(path.read_bytes(), path.name)
    if (
        not isinstance(expected, dict)
        or expected.get("schema") != LEGACY_RECEIPT_SCHEMA
        or expected.get("schemaVersion") != 1
        or expected.get("session") != 19
        or expected.get("candidateId") != live["candidateId"]
        or expected.get("state") != LEGACY_STATE
        or not _valid_utc(expected.get("verifiedUtc"))
        or _comparable(expected) != _comparable(live)
    ):
        raise ContractError("live technical acceptance differs from the immutable receipt")


def _json_mutation(path: Path, mutate: Callable[[Any], None]) -> bytes:
    value = _strict_json(path.read_bytes(), path.name)
    mutate(value)
    return json.dumps(value, indent=2).encode("utf-8") + b"\n"


def _self_test_v1(
    archive: Path, external_run: Path, user_dir: Path, policy_path: Path
) -> tuple[int, list[str]]:
    policy = _strict_json(policy_path.read_bytes(), policy_path.name)
    candidate_id = policy["candidate"]["candidateId"]
    try:
        baseline = _candidate_receipt_v1(
            candidate_id, archive, external_run, user_dir, policy_path
        )
    except (OSError, ContractError) as exc:
        return 0, [f"baseline invalid before mutation testing: {exc}"]
    mutations: list[tuple[str, Path, Callable[[Any], None]]] = [
        ("policy candidate", policy_path, lambda v: v["candidate"].__setitem__("candidateId", "S19_WindowsShipping_OTHER")),
        ("policy archive hash", policy_path, lambda v: v["candidate"].__setitem__("archiveCanonicalManifestSha256", "0" * 64)),
        ("policy executable hash", policy_path, lambda v: v["candidate"].__setitem__("shippingExecutableSha256", "0" * 64)),
        ("policy evidence bytes", policy_path, lambda v: v["projectEvidenceBindings"][0].__setitem__("bytes", 1)),
        ("policy evidence role", policy_path, lambda v: v["projectEvidenceBindings"][0].__setitem__("role", "WRONG")),
        ("policy external role", policy_path, lambda v: v["externalEvidenceBindings"][0].__setitem__("role", "WRONG")),
        ("policy milestone removed", policy_path, lambda v: v["requiredMilestones"].pop()),
        ("policy milestone hash", policy_path, lambda v: v["requiredMilestones"][9].__setitem__("sha256", "0" * 64)),
        ("policy milestone semantic", policy_path, lambda v: v["requiredMilestones"][9].__setitem__("semantic", "WRONG")),
        ("policy semantic method", policy_path, lambda v: v["artifactSemanticReview"].__setitem__("method", "FILENAME_INFERENCE")),
        ("policy scorecard holes", policy_path, lambda v: v["artifactSemanticReview"]["observations"].__setitem__("finalScorecardCompletedHoles", 2)),
        ("policy scorecard strokes", policy_path, lambda v: v["artifactSemanticReview"]["observations"].__setitem__("finalScorecardStrokes", 24)),
        ("policy technical completion", policy_path, lambda v: v["requiredTechnicalFacts"].__setitem__("threeHoleCompletionTechnicallyObserved", False)),
        ("policy timing overclaim", policy_path, lambda v: v["performanceBoundary"].__setitem__("quantitativeTimingMetricsAvailable", True)),
        ("policy performance approval", policy_path, lambda v: v["claimBoundary"].__setitem__("performanceAcceptance", True)),
        ("policy visual approval", policy_path, lambda v: v["claimBoundary"].__setitem__("visualQualityApproval", True)),
        ("policy accessibility", policy_path, lambda v: v["claimBoundary"].__setitem__("accessibilityApproval", True)),
        ("policy product owner", policy_path, lambda v: v["claimBoundary"].__setitem__("productOwnerApproval", True)),
        ("policy release", policy_path, lambda v: v["claimBoundary"].__setitem__("releaseReady", True)),
    ]
    project = {item["role"]: ROOT / item["path"] for item in policy["projectEvidenceBindings"]}
    mutations.extend([
        ("verification state", project["SHIPPING_CANDIDATE_VERIFICATION"], lambda v: v.__setitem__("state", "PASS")),
        ("content issues", project["CANDIDATE_CONTENT_AUDIT"], lambda v: v.__setitem__("issues", ["mutation"])),
        ("binary pass", project["SHIPPING_BINARY_POLICY"], lambda v: v.__setitem__("passed", False)),
        ("external validation failures", project["EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION"], lambda v: v.__setitem__("failures", ["mutation"])),
        ("fresh UserDir state", project["FRESH_USERDIR_VALIDATION"], lambda v: v.__setitem__("state", "FAIL")),
        ("fresh UserDir unknown", project["FRESH_USERDIR_VALIDATION"], lambda v: v["counts"].__setitem__("unknownFiles", 1)),
        ("launch executable", external_run / "launch-record.json", lambda v: v.__setitem__("observedExeSha256", "0" * 64)),
        ("manifest empty-before", external_run / "technical-evidence-manifest.json", lambda v: v["userDir"].__setitem__("emptyBeforeLaunch", False)),
        ("manifest exit", external_run / "technical-evidence-manifest.json", lambda v: v["process"].__setitem__("exitCode", 1)),
        ("manifest timeout", external_run / "technical-evidence-manifest.json", lambda v: v["process"].__setitem__("timedOut", True)),
        ("manifest archive preservation", external_run / "technical-evidence-manifest.json", lambda v: v["candidate"].__setitem__("archivePreserved", False)),
        ("manifest screenshot hash", external_run / "technical-evidence-manifest.json", lambda v: v["evidence"]["screenshots"][9].__setitem__("sha256", "0" * 64)),
    ])
    failures: list[str] = []
    caught = 0
    for name, path, mutate in mutations:
        override = {path.resolve(): _json_mutation(path, mutate)}
        try:
            _candidate_receipt_v1(
                candidate_id, archive, external_run, user_dir, policy_path, override
            )
        except (OSError, ContractError):
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")

    sidecar_path = external_run / "technical-evidence-manifest.sha256"
    screenshot_path = external_run / policy["requiredMilestones"][9]["path"]
    byte_mutations = [
        ("manifest sidecar", sidecar_path, sidecar_path.read_bytes() + b"mutation"),
        ("PNG bytes", screenshot_path, screenshot_path.read_bytes() + b"mutation"),
        ("PNG signature", screenshot_path, b"not-a-png"),
    ]
    for name, path, data in byte_mutations:
        try:
            _candidate_receipt_v1(
                candidate_id,
                archive,
                external_run,
                user_dir,
                policy_path,
                {path.resolve(): data},
            )
        except (OSError, ContractError):
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")

    with tempfile.TemporaryDirectory(prefix="dg_three_hole_acceptance_") as temporary:
        for name, mutate in (
            ("forged immutable receipt", lambda value: value["claimBoundary"].__setitem__("releaseReady", True)),
            ("forged receipt timestamp", lambda value: value.__setitem__("verifiedUtc", "now")),
        ):
            forged = copy.deepcopy(baseline)
            mutate(forged)
            expected_path = Path(temporary) / f"{name.replace(' ', '_')}.json"
            expected_path.write_bytes(json.dumps(forged, indent=2).encode("utf-8") + b"\n")
            try:
                _expected_receipt_v1(expected_path, baseline)
            except ContractError:
                caught += 1
            else:
                failures.append(f"mutation escaped detection: {name}")
    return caught, failures


def _exact_keys(value: Any, expected: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ContractError(f"{label} must be an object")
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ContractError(f"{label} keys differ (missing={missing}, extra={extra})")
    return value


def _integer(value: Any, label: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ContractError(f"{label} must be an integer >= {minimum}")
    return value


def _digest(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9A-Fa-f]{64}", value):
        raise ContractError(f"{label} must be a SHA-256 digest")
    return value.upper()


def _validate_policy_v2(policy: Any) -> None:
    value = _exact_keys(
        policy,
        {
            "schema",
            "schemaVersion",
            "session",
            "policyId",
            "candidateIdPattern",
            "target",
            "projectEvidence",
            "externalEvidence",
            "checkpointChain",
            "runtimeJournal",
            "states",
            "promotionBoundary",
            "receiptSchema",
            "receiptPattern",
            "selfTestMinimumMutationCount",
            "claimBoundary",
        },
        "v2 policy",
    )
    if (
        value["schema"] != "DiscGolfTour.Session19ThreeHoleTechnicalAcceptancePolicy.v2"
        or type(value["schemaVersion"]) is not int
        or value["schemaVersion"] != 2
        or type(value["session"]) is not int
        or value["session"] != 19
        or value["policyId"]
        != "windows_shipping_v05_candidate_bound_three_hole_artifact_integrity_v2"
        or value["candidateIdPattern"] != r"^S19_WindowsShipping_[A-Za-z0-9_-]+$"
        or value["receiptSchema"] != RECEIPT_SCHEMA
        or value["receiptPattern"]
        != "Evidence/Session19/ThreeHoleTechnicalAcceptance-{candidateId}.json"
        or type(value["selfTestMinimumMutationCount"]) is not int
        or value["selfTestMinimumMutationCount"] != 72
    ):
        raise ContractError("v2 policy identity differs")
    if value["target"] != {
        "platform": "Windows",
        "configuration": "Shipping",
        "milestone": "v0.5",
        "archiveLeaf": "Windows",
        "shippingExecutablePath": (
            "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
        ),
    }:
        raise ContractError("v2 target differs")
    expected_project = [
        {
            "role": "SHIPPING_CANDIDATE_VERIFICATION",
            "pathPattern": "Evidence/Session19/ShippingCandidateVerification-{candidateId}.json",
            "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
            "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
        },
        {
            "role": "CANDIDATE_CONTENT_AUDIT",
            "pathPattern": "Evidence/Session19/CandidateContent-{candidateId}.json",
            "schema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
            "state": "PASS_BOUNDED_STAGED_CONTENT_AUDIT",
        },
        {
            "role": "SHIPPING_BINARY_POLICY",
            "pathPattern": "Evidence/Session19/ShippingBinary-{candidateId}.json",
            "schema": "DiscGolfTour.Session19ShippingBinaryEvidence.v1",
            "state": "PASS_BINARY_MARKER_POLICY_ONLY",
        },
        {
            "role": "EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION",
            "pathPattern": "Evidence/Session19/ExternalTechnicalEvidence-{candidateId}.json",
            "schema": "DiscGolfTour.Session19ExternalTechnicalEvidenceValidation.v1",
            "state": "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
        },
        {
            "role": "FRESH_USERDIR_VALIDATION",
            "pathPattern": "Evidence/Session19/FreshUserDir-{candidateId}.json",
            "schema": "DiscGolfTour.Session19FreshUserDirValidation.v1",
            "state": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        },
    ]
    if value["projectEvidence"] != expected_project:
        raise ContractError("v2 project-evidence contract differs")
    if value["externalEvidence"] != {
        "launchRecord": "launch-record.json",
        "technicalManifest": "technical-evidence-manifest.json",
        "technicalManifestSidecar": "technical-evidence-manifest.sha256",
        "checkpointChain": "checkpoint-chain.json",
        "runtimeJournalUserDirPath": RUNTIME_JOURNAL_USERDIR_PATH,
        "requiredScreenshotPaths": EXPECTED_SCREENSHOT_PATHS,
        "minimumPngWidth": 640,
        "minimumPngHeight": 360,
    }:
        raise ContractError("v2 external-evidence contract differs")
    if value["checkpointChain"] != {
        "schema": CHECKPOINT_SCHEMA,
        "schemaVersion": 1,
        "eventNames": CHECKPOINT_EVENT_NAMES,
        "completedHoles": CHECKPOINT_COMPLETED_HOLES,
        "holeNumbers": CHECKPOINT_HOLE_NUMBERS,
        "holePars": [3, 4, 4],
        "firstSequence": 1,
    }:
        raise ContractError("v2 checkpoint-chain contract differs")
    if value["runtimeJournal"] != {
        "schema": RUNTIME_JOURNAL_SCHEMA,
        "schemaVersion": 1,
        "headerRecordType": "HEADER",
        "eventRecordType": "EVENT",
        "userDirRelativePath": RUNTIME_JOURNAL_USERDIR_PATH,
        "manifestValidationState": "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL",
        "eventNames": CHECKPOINT_EVENT_NAMES,
        "completedHoles": CHECKPOINT_COMPLETED_HOLES,
        "holeNumbers": CHECKPOINT_HOLE_NUMBERS,
        "holePars": [3, 4, 4],
        "firstSequence": 1,
        "expectedRecordCount": 13,
        "maximumBytes": 262144,
        "timestampToleranceSeconds": 5,
        "canonicalJsonLines": {
            "encoding": "UTF-8",
            "bom": False,
            "lineEnding": "LF",
            "terminalLineFeed": True,
            "sortKeys": True,
            "ensureAscii": True,
            "separators": [",", ":"],
        },
    }:
        raise ContractError("v2 runtime-journal contract differs")
    if value["states"] != {
        "screenshotSetIntegrity": SCREENSHOT_ONLY_STATE,
        "checkpointChainIntegrityOnly": CHECKPOINT_INTEGRITY_STATE,
        "runtimeJournalVerified": RUNTIME_JOURNAL_STATE,
    }:
        raise ContractError("v2 states differ")
    if value["promotionBoundary"] != {
        "postRunJsonIsTrustedRuntimeEvidence": False,
        "runtimeEmitterImplemented": True,
        "captureWindowBindsRuntimeJournal": True,
        "derivedCheckpointMayPromote": False,
        "technicalAcceptanceMayBeTrue": True,
        "evidenceTrustModel": EVIDENCE_TRUST_MODEL,
        "requiredFutureEvidence": (
            "IMMUTABLE_GAME_EMITTED_EVENT_JOURNAL_BOUND_BY_LAUNCH_RECORD_AND_"
            "TECHNICAL_MANIFEST_DURING_CAPTURE"
        ),
    }:
        raise ContractError("v2 promotion boundary differs")
    claims = value["claimBoundary"]
    if (
        not isinstance(claims, dict)
        or set(claims) != set(EXPECTED_FALSE_CLAIMS) | {"evidenceTrustModel"}
    ):
        raise ContractError("v2 claim boundary keys differ")
    if any(claims[key] is not False for key in EXPECTED_FALSE_CLAIMS):
        raise ContractError("v2 claim boundary must remain entirely false")
    if claims["evidenceTrustModel"] != EVIDENCE_TRUST_MODEL:
        raise ContractError("v2 evidence trust model differs")


def _project_receipts_v2(
    policy: dict[str, Any], candidate_id: str, overrides: dict[Path, bytes]
) -> tuple[dict[str, Any], list[dict[str, Any]], dict[str, Any]]:
    loaded: dict[str, Any] = {}
    bindings: list[dict[str, Any]] = []
    for expected in policy["projectEvidence"]:
        relative = expected["pathPattern"].format(candidateId=candidate_id)
        if _canonical_relative(relative) != relative:
            raise ContractError("project evidence path is not canonical")
        path = ROOT / relative
        _require_regular_file_within(path, ROOT, "project evidence")
        data = _read(path, overrides)
        receipt = _strict_json(data, path.name)
        if (
            not isinstance(receipt, dict)
            or receipt.get("schema") != expected["schema"]
            or receipt.get("session") != 19
            or receipt.get("state") != expected["state"]
        ):
            raise ContractError(f"project evidence boundary differs: {expected['role']}")
        loaded[expected["role"]] = receipt
        bindings.append(
            {
                "role": expected["role"],
                "path": relative,
                "bytes": len(data),
                "sha256": _sha256(data),
            }
        )

    verification = loaded["SHIPPING_CANDIDATE_VERIFICATION"]
    if (
        type(verification.get("schemaVersion")) is not int
        or verification.get("schemaVersion") != 2
        or verification.get("runId") != candidate_id
        or verification.get("verificationMode") != "DIRECT_BUILD_PASS"
        or verification.get("archiveContract", {}).get("allRequiredFilesPresent") is not True
        or verification.get("releaseReady") is not False
    ):
        raise ContractError("Shipping candidate verification boundary differs")
    executable = verification.get("innerShippingExecutable", {})
    executable_path = policy["target"]["shippingExecutablePath"]
    if (
        executable.get("relativePath") != executable_path
        or _integer(executable.get("bytes"), "verified executable bytes", 1) <= 0
    ):
        raise ContractError("verified Shipping executable identity differs")
    executable_identity = {
        "path": executable_path,
        "bytes": executable["bytes"],
        "sha256": _digest(executable.get("sha256"), "verified executable hash"),
    }

    content = loaded["CANDIDATE_CONTENT_AUDIT"]
    archive = content.get("archive", {})
    if (
        type(content.get("schemaVersion")) is not int
        or content.get("schemaVersion") != 1
        or content.get("runId") != candidate_id
        or content.get("issues") != []
        or content.get("requiredThreeHoleData", {}).get("expectedHoleNumbers") != [1, 2, 3]
        or content.get("releaseBoundary", {}).get("releaseReady") is not False
    ):
        raise ContractError("candidate content receipt boundary differs")
    archive_identity = {
        "bytes": _integer(archive.get("bytes"), "audited archive bytes", 1),
        "fileCount": _integer(archive.get("fileCount"), "audited archive file count", 1),
        "canonicalManifestSha256": _digest(
            archive.get("canonicalManifestSha256"), "audited archive manifest hash"
        ),
    }
    verification_archive_hash = _digest(
        verification.get("shippingPluginCapabilities", {}).get(
            "finalArchiveCanonicalManifestSha256"
        ),
        "verified archive manifest hash",
    )
    if verification_archive_hash != archive_identity["canonicalManifestSha256"]:
        raise ContractError("candidate receipts disagree on archive identity")
    corrected = content.get("bindings", {}).get("correctedCandidateVerification")
    verification_binding = bindings[0]
    if not isinstance(corrected, dict) or {
        "path": corrected.get("path"),
        "bytes": corrected.get("bytes"),
        "sha256": _digest(corrected.get("sha256"), "content verification binding hash"),
    } != {
        "path": verification_binding["path"],
        "bytes": verification_binding["bytes"],
        "sha256": verification_binding["sha256"],
    }:
        raise ContractError("content audit does not bind the selected verification receipt")
    selected_binary = content.get("bindings", {}).get("shippingBinaryReceipt")
    binary_binding = bindings[2]
    if not isinstance(selected_binary, dict) or {
        "path": selected_binary.get("path"),
        "bytes": selected_binary.get("bytes"),
        "sha256": _digest(
            selected_binary.get("sha256"), "content binary-receipt binding hash"
        ),
    } != {
        "path": binary_binding["path"],
        "bytes": binary_binding["bytes"],
        "sha256": binary_binding["sha256"],
    }:
        raise ContractError("content audit does not bind the selected binary receipt")

    binary = loaded["SHIPPING_BINARY_POLICY"]
    binary_input = binary.get("input", {})
    if (
        type(binary.get("schemaVersion")) is not int
        or binary.get("schemaVersion") != 1
        or binary.get("passed") is not True
        or binary.get("errors") != []
        or binary.get("releaseReady") is not False
        or binary_input.get("bytes") != executable_identity["bytes"]
        or _digest(binary_input.get("sha256"), "binary-policy executable hash")
        != executable_identity["sha256"]
    ):
        raise ContractError("Shipping binary receipt boundary differs")

    external = loaded["EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION"]
    external_counts = external.get("counts")
    if (
        type(external.get("schemaVersion")) is not int
        or external.get("schemaVersion") != 1
        or external.get("candidateId") != candidate_id
        or not isinstance(external_counts, dict)
        or set(external_counts) != {"presentMonRows", "screenshots"}
        or type(external_counts.get("presentMonRows")) is not int
        or external_counts.get("presentMonRows") != 0
        or type(external_counts.get("screenshots")) is not int
        or external_counts.get("screenshots") != 10
        or external.get("failures") != []
    ):
        raise ContractError("external technical validation receipt differs")
    _claim_boundary(external.get("claimBoundary"), "technicalArtifactValidationOnly")

    fresh = loaded["FRESH_USERDIR_VALIDATION"]
    token = fresh.get("userDirToken")
    fresh_counts = fresh.get("counts")
    if (
        type(fresh.get("schemaVersion")) is not int
        or fresh.get("schemaVersion") != 1
        or fresh.get("candidateId") != candidate_id
        or not isinstance(token, str)
        or not UUID_RE.fullmatch(token)
        or fresh.get("failures") != []
        or not isinstance(fresh_counts, dict)
        or set(fresh_counts)
        != {"approvedFiles", "forbiddenArtifacts", "observedFiles", "unknownFiles"}
        or type(fresh_counts.get("approvedFiles")) is not int
        or type(fresh_counts.get("forbiddenArtifacts")) is not int
        or fresh_counts.get("forbiddenArtifacts") != 0
        or type(fresh_counts.get("observedFiles")) is not int
        or type(fresh_counts.get("unknownFiles")) is not int
        or fresh_counts.get("unknownFiles") != 0
        or fresh.get("releaseBoundary", {}).get("technicalUserDirAcceptanceOnly") is not True
        or fresh.get("releaseBoundary", {}).get("releaseReadinessClaimed") is not False
    ):
        raise ContractError("fresh UserDir validation receipt differs")
    expected_user_binding = _sha256(
        (
            "DiscGolfTour.Session19FreshUserDirValidation.v1\n"
            f"{candidate_id}\n{token}\n"
        ).encode("utf-8")
    )
    if _digest(
        fresh.get("candidateUserDirBindingSha256"),
        "candidate/UserDir binding hash",
    ) != expected_user_binding:
        raise ContractError("fresh UserDir receipt candidate/token binding differs")

    candidate = {
        "candidateId": candidate_id,
        "userDirToken": token,
        "externalRunToken": f"{candidate_id}_{token}",
        "archive": archive_identity,
        "shippingExecutable": executable_identity,
    }
    return candidate, bindings, loaded


def _validate_user_dir_v2(
    user_dir: Path,
    candidate: dict[str, Any],
    fresh: dict[str, Any],
    overrides: dict[Path, bytes],
) -> dict[str, Any]:
    if user_dir.name != candidate["userDirToken"]:
        raise ContractError("UserDir path does not match the candidate token")
    if not user_dir.is_dir() or user_dir.is_symlink() or _is_reparse(user_dir):
        raise ContractError("UserDir is missing or irregular")
    observed = fresh.get("observedFiles")
    if not isinstance(observed, list):
        raise ContractError("fresh UserDir receipt has no observed-file allowlist")
    expected: list[dict[str, Any]] = []
    for item in observed:
        if not isinstance(item, dict):
            raise ContractError("fresh UserDir observed-file row is malformed")
        relative = _canonical_relative(str(item.get("relativePath", "")))
        expected.append(
            {
                "relativePath": relative,
                "bytes": _integer(item.get("bytes"), f"UserDir bytes: {relative}"),
                "sha256": _digest(item.get("sha256"), f"UserDir hash: {relative}"),
            }
        )
    expected.sort(key=lambda item: (item["relativePath"].casefold(), item["relativePath"]))
    if len({item["relativePath"].casefold() for item in expected}) != len(expected):
        raise ContractError("fresh UserDir allowlist contains duplicate paths")
    counts = fresh["counts"]
    if counts["approvedFiles"] != len(expected) or counts["observedFiles"] != len(expected):
        raise ContractError("fresh UserDir receipt counts do not match its allowlist")
    actual: list[dict[str, Any]] = []
    for current, directories, files in os.walk(user_dir, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if child.is_symlink() or _is_reparse(child):
                raise ContractError("UserDir contains a reparse directory")
        for name in files:
            child = current_path / name
            _require_regular_file_within(child, user_dir, "UserDir file")
            relative = _canonical_relative(child.relative_to(user_dir).as_posix())
            byte_count, digest = _hash_file(child)
            actual.append({"relativePath": relative, "bytes": byte_count, "sha256": digest})
    actual.sort(key=lambda item: (item["relativePath"].casefold(), item["relativePath"]))
    if actual != expected:
        raise ContractError("live UserDir inventory differs from its candidate receipt")
    return {
        "token": candidate["userDirToken"],
        "hostPathRecorded": False,
        "emptyBeforeLaunch": True,
        "externalBoundaryValidated": True,
        "postRunAllowlistPass": True,
        "observedFileCount": len(actual),
        "candidateUserDirBindingSha256": _digest(
            fresh.get("candidateUserDirBindingSha256"), "candidate/UserDir binding hash"
        ),
    }


def _validate_external_v2(
    external_run: Path,
    policy: dict[str, Any],
    candidate: dict[str, Any],
    external_receipt: dict[str, Any],
    overrides: dict[Path, bytes],
) -> tuple[
    dict[str, Any], dict[str, Any], dict[str, str], dict[str, Any]
]:
    if external_run.name != candidate["externalRunToken"]:
        raise ContractError("external run directory does not match candidate/UserDir binding")
    if not external_run.is_dir() or external_run.is_symlink() or _is_reparse(external_run):
        raise ContractError("external run directory is missing or irregular")
    contract = policy["externalEvidence"]
    launch_path = external_run / contract["launchRecord"]
    manifest_path = external_run / contract["technicalManifest"]
    sidecar_path = external_run / contract["technicalManifestSidecar"]
    for path, label in (
        (launch_path, "launch record"),
        (manifest_path, "technical evidence manifest"),
        (sidecar_path, "technical evidence manifest sidecar"),
    ):
        _require_regular_file_within(path, external_run, label)
    launch_bytes = _read(launch_path, overrides)
    manifest_bytes = _read(manifest_path, overrides)
    sidecar_bytes = _read(sidecar_path, overrides)
    launch_sha = _sha256(launch_bytes)
    manifest_sha = _sha256(manifest_bytes)
    expected_sidecar = f"{manifest_sha} *{contract['technicalManifest']}\n".encode("ascii")
    if sidecar_bytes.replace(b"\r\n", b"\n") != expected_sidecar:
        raise ContractError("technical manifest SHA sidecar differs")
    if _digest(
        external_receipt.get("manifest", {}).get("sha256"),
        "external-validation manifest hash",
    ) != manifest_sha:
        raise ContractError("external validation receipt does not bind the live manifest")
    launch = _strict_json(launch_bytes, launch_path.name)
    manifest = _strict_json(manifest_bytes, manifest_path.name)
    archive = candidate["archive"]
    executable = candidate["shippingExecutable"]
    launch_schema = launch.get("schema") if isinstance(launch, dict) else None
    launch_version = launch.get("schemaVersion") if isinstance(launch, dict) else None
    launch_is_v1 = (
        launch_schema
        == "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1"
        and type(launch_version) is int
        and launch_version == 1
    )
    launch_is_v2 = (
        launch_schema
        == "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
        and type(launch_version) is int
        and launch_version == 2
    )
    if (
        not isinstance(launch, dict)
        or not (launch_is_v1 or launch_is_v2)
        or type(launch.get("session")) is not int
        or launch.get("session") != 19
        or launch.get("candidateId") != candidate["candidateId"]
        or launch.get("userDirToken") != candidate["userDirToken"]
        or launch.get("exeRelativePath") != policy["target"]["shippingExecutablePath"]
        or _digest(launch.get("expectedArchiveManifestSha256"), "launch archive hash")
        != archive["canonicalManifestSha256"]
        or _digest(launch.get("observedArchiveManifestSha256"), "observed archive hash")
        != archive["canonicalManifestSha256"]
        or _digest(launch.get("expectedExeSha256"), "launch executable hash")
        != executable["sha256"]
        or _digest(launch.get("observedExeSha256"), "observed executable hash")
        != executable["sha256"]
        or launch.get("hostPathsRecorded") is not False
    ):
        raise ContractError("launch record candidate binding differs")
    if launch_is_v2:
        if (
            not isinstance(launch.get("captureNonce"), str)
            or not UUID4_RE.fullmatch(launch["captureNonce"])
            or launch.get("runtimeCheckpointJournalUserDirRelativePath")
            != RUNTIME_JOURNAL_USERDIR_PATH
        ):
            raise ContractError("launch record runtime-journal binding differs")
    _claim_boundary(launch.get("claimBoundary"), "technicalArtifactCollectionOnly")

    manifest_candidate = manifest.get("candidate", {}) if isinstance(manifest, dict) else {}
    expected_archive = {
        "bytes": archive["bytes"],
        "canonicalManifestSha256": archive["canonicalManifestSha256"],
        "fileCount": archive["fileCount"],
    }
    manifest_schema = manifest.get("schema") if isinstance(manifest, dict) else None
    manifest_version = manifest.get("schemaVersion") if isinstance(manifest, dict) else None
    manifest_is_v1 = (
        manifest_schema == "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
        and type(manifest_version) is int
        and manifest_version == 1
    )
    manifest_is_v2 = (
        manifest_schema == "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
        and type(manifest_version) is int
        and manifest_version == 2
    )
    if (
        not isinstance(manifest, dict)
        or not (manifest_is_v1 or manifest_is_v2)
        or launch_is_v2 != manifest_is_v2
        or type(manifest.get("session")) is not int
        or manifest.get("session") != 19
        or manifest.get("state")
        != "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION"
        or manifest_candidate.get("candidateId") != candidate["candidateId"]
        or manifest_candidate.get("archiveBefore") != expected_archive
        or manifest_candidate.get("archiveAfter") != expected_archive
        or manifest_candidate.get("archivePreserved") is not True
        or _digest(manifest_candidate.get("observedExeSha256"), "manifest executable hash")
        != executable["sha256"]
    ):
        raise ContractError("technical manifest candidate preservation differs")
    _claim_boundary(manifest.get("claimBoundary"), "technicalArtifactCollectionOnly")
    process = manifest.get("process", {})
    if (
        type(process.get("exitCode")) is not int
        or process.get("exitCode") != 0
        or process.get("timedOut") is not False
    ):
        raise ContractError("external process did not exit cleanly")
    user = manifest.get("userDir", {})
    if (
        user.get("token") != candidate["userDirToken"]
        or user.get("emptyBeforeLaunch") is not True
        or user.get("externalBoundaryValidated") is not True
        or user.get("hostPathRecorded") is not False
        or user.get("reparsePointsRejected") is not True
    ):
        raise ContractError("manifest does not prove the fresh external UserDir boundary")
    present = manifest.get("evidence", {}).get("presentMon", {})
    if (
        present.get("requested") is not False
        or type(present.get("rowCount")) is not int
        or present.get("rowCount") != 0
    ):
        raise ContractError("performance evidence boundary differs")

    screenshots_dir = external_run / "Screenshots"
    if (
        not screenshots_dir.is_dir()
        or screenshots_dir.is_symlink()
        or _is_reparse(screenshots_dir)
    ):
        raise ContractError("screenshot directory is missing or irregular")
    actual_names: list[str] = []
    for child in screenshots_dir.iterdir():
        _require_regular_file_within(child, external_run, "screenshot artifact")
        actual_names.append(child.relative_to(external_run).as_posix())
    actual_names.sort(key=lambda name: (name.casefold(), name))
    expected_names = sorted(
        contract["requiredScreenshotPaths"], key=lambda name: (name.casefold(), name)
    )
    if actual_names != expected_names:
        raise ContractError("screenshot directory differs from the exact required set")
    manifest_screens = manifest.get("evidence", {}).get("screenshots")
    if not isinstance(manifest_screens, list) or len(manifest_screens) != len(expected_names):
        raise ContractError("technical manifest must bind the exact screenshot set")
    if [item.get("relativePath") for item in manifest_screens if isinstance(item, dict)] \
            != contract["requiredScreenshotPaths"]:
        raise ContractError("technical manifest screenshot order/path set differs")
    artifacts: list[dict[str, Any]] = []
    for index, (relative, manifest_item) in enumerate(
        zip(contract["requiredScreenshotPaths"], manifest_screens), start=1
    ):
        if not isinstance(manifest_item, dict):
            raise ContractError("technical manifest screenshot row is malformed")
        data = _read(external_run / relative, overrides)
        width, height = _png_dimensions(data, relative)
        actual = {
            "path": relative,
            "bytes": len(data),
            "sha256": _sha256(data),
            "width": width,
            "height": height,
        }
        if (
            width < contract["minimumPngWidth"]
            or height < contract["minimumPngHeight"]
            or manifest_item.get("relativePath") != relative
            or manifest_item.get("bytes") != actual["bytes"]
            or _digest(manifest_item.get("sha256"), f"manifest screenshot hash: {relative}")
            != actual["sha256"]
            or manifest_item.get("width") != width
            or manifest_item.get("height") != height
            or manifest_item.get("modifiedWithinRunWindow") is not True
        ):
            raise ContractError(f"manifest screenshot binding differs: {relative}")
        artifacts.append(
            {
                "artifactId": f"SCREENSHOT_{index:03d}",
                **actual,
                "semanticContentInferred": False,
            }
        )
    bindings = [
        {
            "role": "LAUNCH_RECORD",
            "path": contract["launchRecord"],
            "bytes": len(launch_bytes),
            "sha256": launch_sha,
        },
        {
            "role": "TECHNICAL_EVIDENCE_MANIFEST",
            "path": contract["technicalManifest"],
            "bytes": len(manifest_bytes),
            "sha256": manifest_sha,
        },
        {
            "role": "TECHNICAL_EVIDENCE_MANIFEST_SIDECAR",
            "path": contract["technicalManifestSidecar"],
            "bytes": len(sidecar_bytes),
            "sha256": _sha256(sidecar_bytes),
        },
    ]
    return (
        {"bindings": bindings, "screenshots": artifacts},
        manifest,
        {"launch": launch_sha, "manifest": manifest_sha},
        {
            "launch": launch,
            "manifest": manifest,
            "runtimeJournalCaptureRequested": launch_is_v2 and manifest_is_v2,
        },
    )


def _validate_checkpoint_chain_value(
    chain: Any,
    candidate: dict[str, Any],
    launch_sha: str,
    manifest_sha: str,
) -> dict[str, Any]:
    root = _exact_keys(
        chain,
        {
            "schema",
            "schemaVersion",
            "candidateId",
            "userDirToken",
            "externalRunToken",
            "roundId",
            "launchRecordSha256",
            "technicalEvidenceManifestSha256",
            "events",
            "finalScore",
        },
        "checkpoint chain",
    )
    round_id = root["roundId"]
    if (
        root["schema"] != CHECKPOINT_SCHEMA
        or _integer(root["schemaVersion"], "checkpoint schemaVersion", 1) != 1
        or root["candidateId"] != candidate["candidateId"]
        or root["userDirToken"] != candidate["userDirToken"]
        or root["externalRunToken"] != candidate["externalRunToken"]
        or not isinstance(round_id, str)
        or not UUID_RE.fullmatch(round_id)
        or _digest(root["launchRecordSha256"], "checkpoint launch-record hash")
        != launch_sha
        or _digest(
            root["technicalEvidenceManifestSha256"], "checkpoint manifest hash"
        ) != manifest_sha
    ):
        raise ContractError("checkpoint chain root binding differs")
    events = root["events"]
    if not isinstance(events, list) or len(events) != len(CHECKPOINT_EVENT_NAMES):
        raise ContractError("checkpoint chain must contain the exact event sequence")
    previous_ms = -1
    previous_strokes = 0
    previous_penalties = 0
    completed_totals: list[tuple[int, int]] = []
    for index, event_value in enumerate(events):
        event = _exact_keys(
            event_value,
            {
                "sequence",
                "monotonicMs",
                "event",
                "roundId",
                "holeNumber",
                "completedHoles",
                "totalStrokes",
                "totalPenalties",
            },
            f"checkpoint event {index + 1}",
        )
        sequence = _integer(event["sequence"], "checkpoint sequence", 1)
        monotonic_ms = _integer(event["monotonicMs"], "checkpoint monotonicMs")
        expected_hole = CHECKPOINT_HOLE_NUMBERS[index]
        if expected_hole is None:
            if event["holeNumber"] is not None:
                raise ContractError(f"checkpoint event {index + 1} hole differs")
        elif _integer(event["holeNumber"], "checkpoint holeNumber", 1) != expected_hole:
            raise ContractError(f"checkpoint event {index + 1} hole differs")
        completed_holes = _integer(
            event["completedHoles"], "checkpoint completedHoles"
        )
        strokes = _integer(event["totalStrokes"], "checkpoint totalStrokes")
        penalties = _integer(event["totalPenalties"], "checkpoint totalPenalties")
        if (
            sequence != index + 1
            or monotonic_ms <= previous_ms
            or event["event"] != CHECKPOINT_EVENT_NAMES[index]
            or event["roundId"] != round_id
            or completed_holes != CHECKPOINT_COMPLETED_HOLES[index]
            or strokes < previous_strokes
            or penalties < previous_penalties
            or penalties > strokes
        ):
            raise ContractError(f"checkpoint event {index + 1} is not monotonic/canonical")
        if index == 0 and (strokes != 0 or penalties != 0):
            raise ContractError("checkpoint round must start at zero strokes/penalties")
        if index in (3, 6, 9):
            completed_totals.append((strokes, penalties))
        previous_ms = monotonic_ms
        previous_strokes = strokes
        previous_penalties = penalties

    score = _exact_keys(
        root["finalScore"],
        {
            "completedHoles",
            "totalHoles",
            "parTotal",
            "totalStrokes",
            "totalPenalties",
            "holeRows",
        },
        "checkpoint final score",
    )
    rows = score["holeRows"]
    if not isinstance(rows, list) or len(rows) != 3:
        raise ContractError("checkpoint final score must contain three hole rows")
    normalized_rows: list[dict[str, int]] = []
    for index, row_value in enumerate(rows, start=1):
        row = _exact_keys(
            row_value,
            {"holeNumber", "par", "strokes", "penalties"},
            f"checkpoint score row {index}",
        )
        normalized = {
            "holeNumber": _integer(row["holeNumber"], "score hole number", 1),
            "par": _integer(row["par"], "score par", 1),
            "strokes": _integer(row["strokes"], "score strokes", 1),
            "penalties": _integer(row["penalties"], "score penalties"),
        }
        if normalized["holeNumber"] != index or normalized["par"] != [3, 4, 4][index - 1]:
            raise ContractError("checkpoint hole row order/par differs")
        if normalized["penalties"] > normalized["strokes"]:
            raise ContractError("checkpoint hole-row penalties exceed strokes")
        normalized_rows.append(normalized)
    score_strokes = _integer(score["totalStrokes"], "final totalStrokes", 1)
    score_penalties = _integer(score["totalPenalties"], "final totalPenalties")
    completed_holes = _integer(score["completedHoles"], "final completedHoles")
    total_holes = _integer(score["totalHoles"], "final totalHoles", 1)
    par_total = _integer(score["parTotal"], "final parTotal", 1)
    if (
        completed_holes != 3
        or total_holes != 3
        or par_total != 11
        or score_strokes != sum(row["strokes"] for row in normalized_rows)
        or score_penalties != sum(row["penalties"] for row in normalized_rows)
        or score_strokes != events[-1]["totalStrokes"]
        or score_penalties != events[-1]["totalPenalties"]
        or score_penalties > score_strokes
    ):
        raise ContractError("checkpoint final score totals differ")
    prior_strokes = 0
    prior_penalties = 0
    for index, ((strokes, penalties), row) in enumerate(
        zip(completed_totals, normalized_rows), start=1
    ):
        if (
            strokes - prior_strokes != row["strokes"]
            or penalties - prior_penalties != row["penalties"]
        ):
            raise ContractError(f"checkpoint hole {index} totals do not reconcile")
        prior_strokes = strokes
        prior_penalties = penalties
    for tee_index, lie_index, completed_index, prior_completed_index in (
        (1, 2, 3, 0),
        (4, 5, 6, 3),
        (7, 8, 9, 6),
    ):
        tee = events[tee_index]
        prior = events[prior_completed_index]
        lie = events[lie_index]
        completed = events[completed_index]
        if (
            tee["totalStrokes"] != prior["totalStrokes"]
            or tee["totalPenalties"] != prior["totalPenalties"]
            or lie["totalStrokes"] <= tee["totalStrokes"]
            or completed["totalStrokes"] <= lie["totalStrokes"]
        ):
            raise ContractError("checkpoint tee/lie/completion score transitions differ")
    return {
        "roundId": round_id,
        "eventCount": len(events),
        "firstSequence": 1,
        "lastSequence": len(events),
        "firstMonotonicMs": events[0]["monotonicMs"],
        "lastMonotonicMs": events[-1]["monotonicMs"],
        "finalScore": {
            "completedHoles": 3,
            "totalHoles": 3,
            "parTotal": 11,
            "totalStrokes": score_strokes,
            "totalPenalties": score_penalties,
            "holeRows": normalized_rows,
        },
    }


def _validate_runtime_journal_records(
    records: list[dict[str, Any]],
    candidate: dict[str, Any],
    capture_nonce: str,
    process_started: datetime,
    process_finished: datetime,
    launch_sha: str,
    manifest_sha: str,
    timestamp_tolerance_seconds: int,
) -> dict[str, Any]:
    if len(records) != 1 + len(CHECKPOINT_EVENT_NAMES):
        raise ContractError("runtime journal must contain one header and twelve events")
    header = _exact_keys(
        records[0],
        {
            "archiveManifestSha256",
            "candidateId",
            "captureNonce",
            "executableSha256",
            "recordType",
            "roundId",
            "schema",
            "schemaVersion",
            "session",
            "startedUtc",
            "userDirToken",
        },
        "runtime journal header",
    )
    round_id = header["roundId"]
    started_utc = _parse_utc(header["startedUtc"], "runtime journal startedUtc")
    if (
        header["schema"] != RUNTIME_JOURNAL_SCHEMA
        or _integer(header["schemaVersion"], "runtime journal schemaVersion", 1) != 1
        or _integer(header["session"], "runtime journal session", 1) != 19
        or header["recordType"] != "HEADER"
        or header["candidateId"] != candidate["candidateId"]
        or header["userDirToken"] != candidate["userDirToken"]
        or not isinstance(capture_nonce, str)
        or not UUID4_RE.fullmatch(capture_nonce)
        or header["captureNonce"] != capture_nonce
        or not isinstance(round_id, str)
        or not UUID4_RE.fullmatch(round_id)
        or round_id in {capture_nonce, candidate["userDirToken"]}
        or capture_nonce == candidate["userDirToken"]
        or _digest(header["executableSha256"], "runtime journal executable hash")
        != candidate["shippingExecutable"]["sha256"]
        or _digest(header["archiveManifestSha256"], "runtime journal archive hash")
        != candidate["archive"]["canonicalManifestSha256"]
        or started_utc.timestamp()
        < process_started.timestamp() - timestamp_tolerance_seconds
        or started_utc.timestamp()
        > process_finished.timestamp() + timestamp_tolerance_seconds
    ):
        raise ContractError("runtime journal header binding differs")

    checkpoint_events: list[dict[str, Any]] = []
    final_score: dict[str, Any] | None = None
    base_keys = {
        "completedHoles",
        "event",
        "holeNumber",
        "monotonicMs",
        "recordType",
        "roundId",
        "sequence",
        "totalPenalties",
        "totalStrokes",
    }
    for index, raw_event in enumerate(records[1:]):
        expected_keys = base_keys | ({"finalScore"} if index == 11 else set())
        event = _exact_keys(raw_event, expected_keys, f"runtime journal event {index + 1}")
        if event["recordType"] != "EVENT":
            raise ContractError(f"runtime journal event {index + 1} record type differs")
        checkpoint_events.append({key: event[key] for key in base_keys - {"recordType"}})
        if index == 11:
            final_score = event["finalScore"]
    assert final_score is not None
    if _integer(
        checkpoint_events[0]["monotonicMs"], "first runtime monotonicMs", 1
    ) < 1:
        raise ContractError("runtime journal must start with a positive monotonic time")
    maximum_elapsed = int(
        (process_finished - started_utc).total_seconds() * 1000
    ) + timestamp_tolerance_seconds * 1000
    if checkpoint_events[-1]["monotonicMs"] > maximum_elapsed:
        raise ContractError("runtime journal elapsed time exceeds the capture window")
    validated = _validate_checkpoint_chain_value(
        {
            "schema": CHECKPOINT_SCHEMA,
            "schemaVersion": 1,
            "candidateId": candidate["candidateId"],
            "userDirToken": candidate["userDirToken"],
            "externalRunToken": candidate["externalRunToken"],
            "roundId": round_id,
            "launchRecordSha256": launch_sha,
            "technicalEvidenceManifestSha256": manifest_sha,
            "events": checkpoint_events,
            "finalScore": final_score,
        },
        candidate,
        launch_sha,
        manifest_sha,
    )
    return {
        **validated,
        "captureNonce": capture_nonce,
        "startedUtc": header["startedUtc"],
    }


def _validate_checkpoint_chain_file(
    checkpoint_path: Path,
    external_run: Path,
    policy: dict[str, Any],
    candidate: dict[str, Any],
    hashes: dict[str, str],
    overrides: dict[Path, bytes],
) -> tuple[dict[str, Any], dict[str, Any]]:
    expected = (external_run / policy["externalEvidence"]["checkpointChain"]).resolve()
    if checkpoint_path.resolve() != expected:
        raise ContractError("checkpoint chain must use the policy path inside external-run")
    _require_regular_file_within(checkpoint_path, external_run, "checkpoint chain")
    data = _read(checkpoint_path, overrides)
    validated = _validate_checkpoint_chain_value(
        _strict_json(data, checkpoint_path.name),
        candidate,
        hashes["launch"],
        hashes["manifest"],
    )
    return (
        {
            "path": policy["externalEvidence"]["checkpointChain"],
            "bytes": len(data),
            "sha256": _sha256(data),
            "schema": CHECKPOINT_SCHEMA,
            "structuralIntegrityValidated": True,
            "runtimeOperationalBindingValidated": False,
            "captureWindowBindingPresent": False,
            "semanticGameplayClaimsPermitted": False,
        },
        validated,
    )


def _validate_runtime_journal_file(
    user_dir: Path,
    policy: dict[str, Any],
    candidate: dict[str, Any],
    fresh_receipt: dict[str, Any],
    runtime_context: dict[str, Any],
    hashes: dict[str, str],
    overrides: dict[Path, bytes],
) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    contract = policy["runtimeJournal"]
    relative = contract["userDirRelativePath"]
    journal_path = user_dir / relative
    requested = runtime_context["runtimeJournalCaptureRequested"]
    path_lexically_exists = os.path.lexists(journal_path)
    if not requested:
        if path_lexically_exists:
            raise ContractError(
                "runtime journal exists without v2 launch/manifest capture bindings"
            )
        return None, None
    if not path_lexically_exists:
        raise ContractError("requested game-emitted runtime journal is missing")
    _require_regular_file_within(journal_path, user_dir, "runtime journal")

    launch = runtime_context["launch"]
    manifest = runtime_context["manifest"]
    capture_nonce = launch.get("captureNonce")
    if (
        not isinstance(capture_nonce, str)
        or not UUID4_RE.fullmatch(capture_nonce)
        or launch.get("runtimeCheckpointJournalUserDirRelativePath") != relative
    ):
        raise ContractError("runtime journal launch binding differs")
    process = manifest.get("process")
    if not isinstance(process, dict):
        raise ContractError("runtime journal capture has no process window")
    process_started = _parse_utc(
        process.get("startedUtc"), "runtime journal process startedUtc"
    )
    process_finished = _parse_utc(
        process.get("finishedUtc"), "runtime journal process finishedUtc"
    )
    launch_recorded = _parse_utc(
        launch.get("recordedUtc"), "runtime journal launch recordedUtc"
    )
    tolerance = contract["timestampToleranceSeconds"]
    if (
        process_finished < process_started
        or launch_recorded.timestamp() < process_started.timestamp() - tolerance
        or launch_recorded.timestamp() > process_started.timestamp() + tolerance
        or launch_recorded > process_finished
    ):
        raise ContractError("runtime journal process/launch window differs")

    evidence = manifest.get("evidence")
    if not isinstance(evidence, dict):
        raise ContractError("technical manifest evidence object is malformed")
    manifest_binding = _exact_keys(
        evidence.get("runtimeCheckpointJournal"),
        {
            "bytes",
            "captureNonce",
            "eventCount",
            "firstSequence",
            "lastSequence",
            "modifiedUtc",
            "modifiedWithinRunWindow",
            "roundId",
            "schema",
            "sha256",
            "userDirRelativePath",
            "validationFailures",
            "validationState",
        },
        "technical manifest runtime-journal binding",
    )
    data, journal_stat = _read_with_stat(journal_path, overrides)
    digest = _sha256(data)
    modified_utc = datetime.fromtimestamp(journal_stat.st_mtime, timezone.utc)
    recorded_modified_utc = _parse_utc(
        manifest_binding["modifiedUtc"], "runtime journal manifest modifiedUtc"
    )
    if (
        manifest_binding["userDirRelativePath"] != relative
        or _integer(manifest_binding["bytes"], "runtime journal manifest bytes", 1)
        != len(data)
        or _digest(manifest_binding["sha256"], "runtime journal manifest hash")
        != digest
        or manifest_binding["schema"] != contract["schema"]
        or manifest_binding["captureNonce"] != capture_nonce
        or not isinstance(manifest_binding["roundId"], str)
        or not UUID4_RE.fullmatch(manifest_binding["roundId"])
        or _integer(manifest_binding["eventCount"], "runtime journal event count", 1)
        != len(CHECKPOINT_EVENT_NAMES)
        or _integer(manifest_binding["firstSequence"], "runtime first sequence", 1)
        != 1
        or _integer(manifest_binding["lastSequence"], "runtime last sequence", 1)
        != len(CHECKPOINT_EVENT_NAMES)
        or manifest_binding["validationState"]
        != contract["manifestValidationState"]
        or manifest_binding["validationFailures"] != []
        or manifest_binding["modifiedWithinRunWindow"] is not True
        or abs(recorded_modified_utc.timestamp() - modified_utc.timestamp()) > 0.002
        or modified_utc.timestamp() < process_started.timestamp() - tolerance
        or modified_utc.timestamp() > process_finished.timestamp() + tolerance
    ):
        raise ContractError("technical manifest runtime-journal binding differs")

    observed = fresh_receipt.get("observedFiles")
    if not isinstance(observed, list):
        raise ContractError("fresh UserDir receipt lacks its journal binding")
    fresh_matches = [
        item
        for item in observed
        if isinstance(item, dict) and item.get("relativePath") == relative
    ]
    if len(fresh_matches) != 1:
        raise ContractError("fresh UserDir receipt must bind exactly one runtime journal")
    fresh_binding = fresh_matches[0]
    if (
        _integer(fresh_binding.get("bytes"), "fresh runtime journal bytes", 1)
        != len(data)
        or _digest(fresh_binding.get("sha256"), "fresh runtime journal hash")
        != digest
        or fresh_binding.get("classification")
        != "runtime_checkpoint_journal_candidate_bound"
    ):
        raise ContractError("fresh UserDir receipt runtime-journal binding differs")
    fresh_runtime_binding = _exact_keys(
        fresh_receipt.get("runtimeJournalBinding"),
        {
            "mode",
            "userDirRelativePath",
            "bytes",
            "sha256",
            "captureNonce",
            "roundId",
            "launchRecordSha256",
            "technicalEvidenceManifestSha256",
            "executableSha256",
            "archiveManifestSha256",
            "validationState",
        },
        "fresh UserDir runtime-journal binding",
    )
    if (
        fresh_runtime_binding["mode"] != "CANDIDATE_BOUND_EXTERNAL_CAPTURE"
        or fresh_runtime_binding["userDirRelativePath"] != relative
        or _integer(fresh_runtime_binding["bytes"], "fresh bound journal bytes", 1)
        != len(data)
        or _digest(fresh_runtime_binding["sha256"], "fresh bound journal hash")
        != digest
        or fresh_runtime_binding["captureNonce"] != capture_nonce
        or fresh_runtime_binding["roundId"] != manifest_binding["roundId"]
        or _digest(
            fresh_runtime_binding["launchRecordSha256"],
            "fresh runtime launch-record hash",
        )
        != hashes["launch"]
        or _digest(
            fresh_runtime_binding["technicalEvidenceManifestSha256"],
            "fresh runtime manifest hash",
        )
        != hashes["manifest"]
        or _digest(
            fresh_runtime_binding["executableSha256"],
            "fresh runtime executable hash",
        )
        != candidate["shippingExecutable"]["sha256"]
        or _digest(
            fresh_runtime_binding["archiveManifestSha256"],
            "fresh runtime archive hash",
        )
        != candidate["archive"]["canonicalManifestSha256"]
        or fresh_runtime_binding["validationState"]
        != contract["manifestValidationState"]
    ):
        raise ContractError("fresh UserDir candidate-bound journal contract differs")

    records = _canonical_jsonl_records(data, contract["maximumBytes"])
    validated = _validate_runtime_journal_records(
        records,
        candidate,
        capture_nonce,
        process_started,
        process_finished,
        hashes["launch"],
        hashes["manifest"],
        tolerance,
    )
    if manifest_binding["roundId"] != validated["roundId"]:
        raise ContractError("manifest and raw runtime journal round IDs differ")
    return (
        {
            "path": relative,
            "hostPathRecorded": False,
            "userDirRecoveryLocationToken": (
                f"DGTOUR_EXTERNAL_USERDIR/{candidate['userDirToken']}/{relative}"
            ),
            "bytes": len(data),
            "sha256": digest,
            "schema": contract["schema"],
            "candidateId": candidate["candidateId"],
            "userDirToken": candidate["userDirToken"],
            "executableSha256": candidate["shippingExecutable"]["sha256"],
            "archiveManifestSha256": candidate["archive"][
                "canonicalManifestSha256"
            ],
            "launchRecordSha256": hashes["launch"],
            "technicalEvidenceManifestSha256": hashes["manifest"],
            "captureNonce": capture_nonce,
            "roundId": validated["roundId"],
            "eventCount": validated["eventCount"],
            "firstSequence": validated["firstSequence"],
            "lastSequence": validated["lastSequence"],
            "startedUtc": validated["startedUtc"],
            "modifiedUtc": manifest_binding["modifiedUtc"],
            "canonicalJsonLinesValidated": True,
            "freshUserDirReceiptBindingValidated": True,
            "launchRecordBindingValidated": True,
            "technicalManifestBindingValidated": True,
            "captureWindowValidated": True,
            "runtimeOperationalBindingValidated": True,
        },
        validated,
    )


def _acceptance_sections(
    checkpoint: dict[str, Any] | None,
    runtime_journal: dict[str, Any] | None = None,
) -> tuple[bool, dict[str, Any], dict[str, Any]]:
    checkpoint_supplied = checkpoint is not None
    operationally_bound = runtime_journal is not None
    return (
        operationally_bound,
        {
            "checkpointChainSupplied": checkpoint_supplied,
            "checkpointChainStructuralIntegrityValidated": checkpoint_supplied,
            "derivedCheckpointMayPromote": False,
            "runtimeJournalSupplied": operationally_bound,
            "runtimeJournalCanonicalBytesValidated": operationally_bound,
            "runtimeOperationalBindingValidated": operationally_bound,
            "captureWindowBindingPresent": operationally_bound,
            "singleRoundProven": operationally_bound,
            "freshRoundEntryToFinalScorecardSequencePass": operationally_bound,
            "eachHoleTeeAndRecoveredLieObserved": operationally_bound,
            "threeHoleCompletionObserved": operationally_bound,
        },
        {
            "finalScorecardScoreProven": operationally_bound,
            "completedHolesProven": operationally_bound,
            "totalHolesProven": operationally_bound,
            "holeRowsProven": operationally_bound,
            "roundId": runtime_journal["roundId"] if operationally_bound else None,
            "finalScore": runtime_journal["finalScore"] if operationally_bound else None,
        },
    )


def _candidate_receipt_v2(
    candidate_id: str,
    archive: Path,
    external_run: Path,
    user_dir: Path,
    policy_path: Path,
    checkpoint_path: Path | None,
    overrides: dict[Path, bytes] | None = None,
) -> dict[str, Any]:
    overrides = overrides or {}
    _require_regular_file_within(policy_path, ROOT, "v2 policy")
    policy_bytes = _read(policy_path, overrides)
    policy = _strict_json(policy_bytes, policy_path.name)
    _validate_policy_v2(policy)
    if not CANDIDATE_RE.fullmatch(candidate_id):
        raise ContractError("candidate ID is not canonical")
    candidate, project_bindings, loaded = _project_receipts_v2(
        policy, candidate_id, overrides
    )
    archive_records, archive_sha = _collect_archive(archive)
    expected_archive = candidate["archive"]
    if (
        len(archive_records) != expected_archive["fileCount"]
        or sum(item["bytes"] for item in archive_records) != expected_archive["bytes"]
        or archive_sha != expected_archive["canonicalManifestSha256"]
    ):
        raise ContractError("live final archive differs from candidate receipts")
    executable = next(
        (
            item
            for item in archive_records
            if item["path"] == candidate["shippingExecutable"]["path"]
        ),
        None,
    )
    if executable != candidate["shippingExecutable"]:
        raise ContractError("live Shipping executable differs from candidate receipts")
    external, manifest, hashes, runtime_context = _validate_external_v2(
        external_run,
        policy,
        candidate,
        loaded["EXTERNAL_TECHNICAL_EVIDENCE_VALIDATION"],
        overrides,
    )
    fresh_user = _validate_user_dir_v2(
        user_dir, candidate, loaded["FRESH_USERDIR_VALIDATION"], overrides
    )
    runtime_journal_binding, runtime_journal = _validate_runtime_journal_file(
        user_dir,
        policy,
        candidate,
        loaded["FRESH_USERDIR_VALIDATION"],
        runtime_context,
        hashes,
        overrides,
    )
    checkpoint_binding: dict[str, Any] | None = None
    checkpoint: dict[str, Any] | None = None
    if checkpoint_path is not None:
        checkpoint_binding, checkpoint = _validate_checkpoint_chain_file(
            checkpoint_path,
            external_run,
            policy,
            candidate,
            hashes,
            overrides,
        )
    technical_acceptance, continuity_claims, score_claims = _acceptance_sections(
        checkpoint, runtime_journal
    )
    state = (
        RUNTIME_JOURNAL_STATE
        if runtime_journal is not None
        else CHECKPOINT_INTEGRITY_STATE
        if checkpoint is not None
        else SCREENSHOT_ONLY_STATE
    )
    try:
        policy_relative = policy_path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError as exc:
        raise ContractError("policy must be inside the project root") from exc
    return {
        "schema": RECEIPT_SCHEMA,
        "schemaVersion": 2,
        "session": 19,
        "candidateId": candidate_id,
        "verifiedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": state,
        "policy": {
            "path": policy_relative,
            "bytes": len(policy_bytes),
            "sha256": _sha256(policy_bytes),
            "policyId": policy["policyId"],
        },
        "candidate": {
            "archive": {
                "hostPathRecorded": False,
                "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
                **expected_archive,
                "liveRevalidationPass": True,
            },
            "shippingExecutable": executable,
        },
        "projectEvidenceBindings": project_bindings,
        "externalEvidence": {
            "hostPathRecorded": False,
            "recoveryLocationToken": (
                "DGTOUR_ACCEPTANCE/Session19ExternalEvidence/"
                + candidate["externalRunToken"]
            ),
            **external,
        },
        "freshUserDir": fresh_user,
        "process": {
            "exitCode": manifest["process"]["exitCode"],
            "timedOut": manifest["process"]["timedOut"],
            "cleanExit": True,
        },
        "artifactIntegrity": {
            "pass": True,
            "exactScreenshotSetPass": True,
            "manifestHashesAndDimensionsPass": True,
            "screenshotCount": len(EXPECTED_SCREENSHOT_PATHS),
            "screenshotFilenamesTreatedAsOpaqueArtifactIds": True,
            "semanticContentInferredFromScreenshots": False,
        },
        "checkpointChain": checkpoint_binding,
        "runtimeJournal": runtime_journal_binding,
        "technicalAcceptance": technical_acceptance,
        "continuityClaims": continuity_claims,
        "scoreClaims": score_claims,
        "claimBoundary": policy["claimBoundary"],
        "blockerClosed": False,
        "releaseReady": False,
        "remainingEvidence": (
            [
                "IMMUTABLE_GAME_EMITTED_EVENT_JOURNAL_BOUND_BY_LAUNCH_RECORD_"
                "AND_TECHNICAL_MANIFEST_DURING_CAPTURE"
            ]
            if runtime_journal is None
            else []
        ) + [
            "VISUAL_QUALITY_PRODUCT_OWNER_APPROVAL",
            "ACCESSIBILITY_APPROVAL",
            "HUMAN_PLAY_FEEL_AND_MANUAL_GAMEPLAY_ACCEPTANCE",
            "QUANTITATIVE_PERFORMANCE_AND_SOAK_ACCEPTANCE",
            "WHOLE_ARCHIVE_PROVENANCE_APPROVAL",
            "LEGAL_AND_DISTRIBUTION_CLEARANCE",
            "RELEASE_APPROVAL",
        ],
        "receiptPath": policy["receiptPattern"].format(candidateId=candidate_id),
    }


def _expected_receipt_v2(path: Path, live: dict[str, Any]) -> None:
    expected = _strict_json(path.read_bytes(), path.name)
    if (
        not isinstance(expected, dict)
        or expected.get("schema") != RECEIPT_SCHEMA
        or expected.get("schemaVersion") != 2
        or expected.get("session") != 19
        or expected.get("candidateId") != live["candidateId"]
        or expected.get("state") != live["state"]
        or not _valid_utc(expected.get("verifiedUtc"))
        or _comparable(expected) != _comparable(live)
    ):
        raise ContractError("live v2 acceptance differs from the immutable receipt")


def _synthetic_checkpoint_chain() -> tuple[dict[str, Any], dict[str, Any], str, str]:
    candidate = {
        "candidateId": "S19_WindowsShipping_SELFTEST",
        "userDirToken": "11111111-2222-4333-8444-555555555555",
        "externalRunToken": (
            "S19_WindowsShipping_SELFTEST_11111111-2222-4333-8444-555555555555"
        ),
        "archive": {"canonicalManifestSha256": "D" * 64},
        "shippingExecutable": {"sha256": "C" * 64},
    }
    launch_sha = "A" * 64
    manifest_sha = "B" * 64
    round_id = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
    totals = [
        (0, 0),
        (0, 0),
        (1, 0),
        (4, 1),
        (4, 1),
        (5, 1),
        (14, 2),
        (14, 2),
        (15, 2),
        (20, 3),
        (20, 3),
        (20, 3),
    ]
    events = [
        {
            "sequence": index + 1,
            "monotonicMs": (index + 1) * 100,
            "event": CHECKPOINT_EVENT_NAMES[index],
            "roundId": round_id,
            "holeNumber": CHECKPOINT_HOLE_NUMBERS[index],
            "completedHoles": CHECKPOINT_COMPLETED_HOLES[index],
            "totalStrokes": totals[index][0],
            "totalPenalties": totals[index][1],
        }
        for index in range(len(CHECKPOINT_EVENT_NAMES))
    ]
    return (
        {
            "schema": CHECKPOINT_SCHEMA,
            "schemaVersion": 1,
            "candidateId": candidate["candidateId"],
            "userDirToken": candidate["userDirToken"],
            "externalRunToken": candidate["externalRunToken"],
            "roundId": round_id,
            "launchRecordSha256": launch_sha,
            "technicalEvidenceManifestSha256": manifest_sha,
            "events": events,
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
        candidate,
        launch_sha,
        manifest_sha,
    )


def _synthetic_runtime_journal() -> tuple[
    bytes,
    list[dict[str, Any]],
    dict[str, Any],
    str,
    datetime,
    datetime,
    str,
    str,
]:
    chain, candidate, launch_sha, manifest_sha = _synthetic_checkpoint_chain()
    capture_nonce = "99999999-aaaa-4bbb-8ccc-dddddddddddd"
    started = datetime(2099, 1, 1, 0, 0, 0, tzinfo=timezone.utc)
    finished = datetime(2099, 1, 1, 0, 1, 0, tzinfo=timezone.utc)
    records: list[dict[str, Any]] = [
        {
            "archiveManifestSha256": candidate["archive"][
                "canonicalManifestSha256"
            ],
            "candidateId": candidate["candidateId"],
            "captureNonce": capture_nonce,
            "executableSha256": candidate["shippingExecutable"]["sha256"],
            "recordType": "HEADER",
            "roundId": chain["roundId"],
            "schema": RUNTIME_JOURNAL_SCHEMA,
            "schemaVersion": 1,
            "session": 19,
            "startedUtc": "2099-01-01T00:00:00.100Z",
            "userDirToken": candidate["userDirToken"],
        }
    ]
    for index, checkpoint_event in enumerate(chain["events"]):
        event = {**checkpoint_event, "recordType": "EVENT"}
        if index == len(chain["events"]) - 1:
            event["finalScore"] = copy.deepcopy(chain["finalScore"])
        records.append(event)
    data = b"".join(
        json.dumps(
            record,
            sort_keys=True,
            separators=(",", ":"),
            ensure_ascii=True,
        ).encode("ascii")
        + b"\n"
        for record in records
    )
    return (
        data,
        records,
        candidate,
        capture_nonce,
        started,
        finished,
        launch_sha,
        manifest_sha,
    )


def _self_test_v2(policy_path: Path) -> tuple[int, list[str]]:
    failures: list[str] = []
    try:
        policy = _strict_json(policy_path.read_bytes(), policy_path.name)
        _validate_policy_v2(policy)
        baseline, candidate, launch_sha, manifest_sha = _synthetic_checkpoint_chain()
        accepted = _validate_checkpoint_chain_value(
            baseline, candidate, launch_sha, manifest_sha
        )
        technical, continuity, score = _acceptance_sections(None)
        if (
            technical is not False
            or any(continuity.values())
            or any(value for key, value in score.items() if key not in {"roundId", "finalScore"})
            or score["roundId"] is not None
            or score["finalScore"] is not None
        ):
            failures.append("screenshot-only boundary overclaims continuity or score")
        untrusted, untrusted_continuity, untrusted_score = _acceptance_sections(accepted)
        if (
            untrusted is not False
            or untrusted_continuity["checkpointChainSupplied"] is not True
            or untrusted_continuity[
                "checkpointChainStructuralIntegrityValidated"
            ] is not True
            or any(
                untrusted_continuity[key]
                for key in (
                    "derivedCheckpointMayPromote",
                    "runtimeJournalSupplied",
                    "runtimeJournalCanonicalBytesValidated",
                    "runtimeOperationalBindingValidated",
                    "captureWindowBindingPresent",
                    "singleRoundProven",
                    "freshRoundEntryToFinalScorecardSequencePass",
                    "eachHoleTeeAndRecoveredLieObserved",
                    "threeHoleCompletionObserved",
                )
            )
            or any(
                value
                for key, value in untrusted_score.items()
                if key not in {"roundId", "finalScore"}
            )
            or untrusted_score["roundId"] is not None
            or untrusted_score["finalScore"] is not None
        ):
            failures.append("post-run checkpoint JSON overclaimed runtime provenance")
        (
            runtime_bytes,
            runtime_records,
            runtime_candidate,
            capture_nonce,
            process_started,
            process_finished,
            runtime_launch_sha,
            runtime_manifest_sha,
        ) = _synthetic_runtime_journal()
        canonical_records = _canonical_jsonl_records(runtime_bytes, 262144)
        if canonical_records != runtime_records:
            failures.append("canonical runtime journal bytes did not round-trip")
        runtime = _validate_runtime_journal_records(
            canonical_records,
            runtime_candidate,
            capture_nonce,
            process_started,
            process_finished,
            runtime_launch_sha,
            runtime_manifest_sha,
            5,
        )
        promoted, promoted_continuity, promoted_score = _acceptance_sections(
            None, runtime
        )
        if (
            promoted is not True
            or promoted_continuity["derivedCheckpointMayPromote"] is not False
            or not all(
                promoted_continuity[key]
                for key in (
                    "runtimeJournalSupplied",
                    "runtimeJournalCanonicalBytesValidated",
                    "runtimeOperationalBindingValidated",
                    "captureWindowBindingPresent",
                    "singleRoundProven",
                    "freshRoundEntryToFinalScorecardSequencePass",
                    "eachHoleTeeAndRecoveredLieObserved",
                    "threeHoleCompletionObserved",
                )
            )
            or not all(
                promoted_score[key]
                for key in (
                    "finalScorecardScoreProven",
                    "completedHolesProven",
                    "totalHolesProven",
                    "holeRowsProven",
                )
            )
            or promoted_score["roundId"] != runtime["roundId"]
            or promoted_score["finalScore"] != runtime["finalScore"]
        ):
            failures.append(
                "operationally bound runtime journal did not promote exact claims"
            )
    except (OSError, ContractError) as exc:
        return 0, [f"baseline invalid before v2 mutation testing: {exc}"]

    mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("extra root key", lambda v: v.__setitem__("extra", True)),
        ("missing root key", lambda v: v.pop("finalScore")),
        ("schema", lambda v: v.__setitem__("schema", "wrong")),
        ("schema version", lambda v: v.__setitem__("schemaVersion", 2)),
        ("schema version boolean", lambda v: v.__setitem__("schemaVersion", True)),
        ("candidate", lambda v: v.__setitem__("candidateId", "S19_WindowsShipping_OTHER")),
        ("UserDir token", lambda v: v.__setitem__("userDirToken", "22222222-2222-4222-8222-222222222222")),
        ("external token", lambda v: v.__setitem__("externalRunToken", "wrong")),
        ("noncanonical round", lambda v: v.__setitem__("roundId", v["roundId"].upper())),
        ("launch hash", lambda v: v.__setitem__("launchRecordSha256", "C" * 64)),
        ("manifest hash", lambda v: v.__setitem__("technicalEvidenceManifestSha256", "C" * 64)),
        ("event removed", lambda v: v["events"].pop()),
        ("event added", lambda v: v["events"].append(copy.deepcopy(v["events"][-1]))),
        ("event extra key", lambda v: v["events"][0].__setitem__("extra", 1)),
        ("event missing key", lambda v: v["events"][0].pop("roundId")),
        ("sequence gap", lambda v: v["events"][4].__setitem__("sequence", 7)),
        ("sequence boolean", lambda v: v["events"][0].__setitem__("sequence", True)),
        ("monotonic duplicate", lambda v: v["events"][5].__setitem__("monotonicMs", 400)),
        ("monotonic regression", lambda v: v["events"][5].__setitem__("monotonicMs", 399)),
        ("event name", lambda v: v["events"][4].__setitem__("event", "HOLE_3_TEE")),
        ("event round", lambda v: v["events"][4].__setitem__("roundId", "ffffffff-ffff-4fff-8fff-ffffffffffff")),
        ("event hole", lambda v: v["events"][4].__setitem__("holeNumber", 3)),
        ("event hole boolean", lambda v: v["events"][1].__setitem__("holeNumber", True)),
        ("completed holes", lambda v: v["events"][6].__setitem__("completedHoles", 3)),
        ("completed holes boolean", lambda v: v["events"][3].__setitem__("completedHoles", True)),
        ("strokes regression", lambda v: v["events"][8].__setitem__("totalStrokes", 13)),
        ("penalties regression", lambda v: v["events"][8].__setitem__("totalPenalties", 1)),
        ("score extra key", lambda v: v["finalScore"].__setitem__("toPar", 9)),
        ("completed score holes", lambda v: v["finalScore"].__setitem__("completedHoles", 2)),
        ("completed score holes boolean", lambda v: v["finalScore"].__setitem__("completedHoles", True)),
        ("total score holes", lambda v: v["finalScore"].__setitem__("totalHoles", 4)),
        ("par total", lambda v: v["finalScore"].__setitem__("parTotal", 12)),
        ("stroke total", lambda v: v["finalScore"].__setitem__("totalStrokes", 21)),
        ("penalty total", lambda v: v["finalScore"].__setitem__("totalPenalties", 4)),
        ("row removed", lambda v: v["finalScore"]["holeRows"].pop()),
        ("row extra key", lambda v: v["finalScore"]["holeRows"][0].__setitem__("toPar", 1)),
        ("row order", lambda v: v["finalScore"]["holeRows"][0].__setitem__("holeNumber", 2)),
        ("row par", lambda v: v["finalScore"]["holeRows"][1].__setitem__("par", 3)),
        ("row strokes", lambda v: v["finalScore"]["holeRows"][1].__setitem__("strokes", 9)),
        ("row penalties", lambda v: v["finalScore"]["holeRows"][2].__setitem__("penalties", 0)),
        ("completion reconciliation", lambda v: v["events"][6].__setitem__("totalStrokes", 13)),
    ]
    caught = 0
    for name, mutate in mutations:
        value = copy.deepcopy(baseline)
        mutate(value)
        try:
            _validate_checkpoint_chain_value(value, candidate, launch_sha, manifest_sha)
        except ContractError:
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")

    runtime_mutations: list[
        tuple[str, Callable[[dict[str, Any]], None], str | None]
    ] = [
        ("runtime header extra", lambda v: v["records"][0].__setitem__("extra", 1), None),
        ("runtime header missing", lambda v: v["records"][0].pop("startedUtc"), None),
        ("runtime schema", lambda v: v["records"][0].__setitem__("schema", "wrong"), None),
        ("runtime schema bool", lambda v: v["records"][0].__setitem__("schemaVersion", True), None),
        ("runtime session bool", lambda v: v["records"][0].__setitem__("session", True), None),
        ("runtime candidate", lambda v: v["records"][0].__setitem__("candidateId", "S19_WindowsShipping_OTHER"), None),
        ("runtime token", lambda v: v["records"][0].__setitem__("userDirToken", "22222222-2222-4222-8222-222222222222"), None),
        ("runtime executable", lambda v: v["records"][0].__setitem__("executableSha256", "A" * 64), None),
        ("runtime archive", lambda v: v["records"][0].__setitem__("archiveManifestSha256", "A" * 64), None),
        ("runtime nonce mismatch", lambda v: v["records"][0].__setitem__("captureNonce", "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"), None),
        ("runtime nonce reused", lambda v: v["records"][0].__setitem__("captureNonce", v["candidate"]["userDirToken"]), "11111111-2222-4333-8444-555555555555"),
        ("runtime round non-v4", lambda v: v["records"][0].__setitem__("roundId", "aaaaaaaa-bbbb-3ccc-8ddd-eeeeeeeeeeee"), None),
        ("runtime started before process", lambda v: v["records"][0].__setitem__("startedUtc", "2098-12-31T23:59:54.999Z"), None),
        ("runtime started after process", lambda v: v["records"][0].__setitem__("startedUtc", "2099-01-01T00:01:05.001Z"), None),
        ("runtime event record type", lambda v: v["records"][1].__setitem__("recordType", "HEADER"), None),
        ("runtime first elapsed zero", lambda v: v["records"][1].__setitem__("monotonicMs", 0), None),
        ("runtime elapsed beyond process", lambda v: v["records"][-1].__setitem__("monotonicMs", 70000), None),
        ("runtime event round", lambda v: v["records"][5].__setitem__("roundId", "ffffffff-ffff-4fff-8fff-ffffffffffff"), None),
        ("runtime event bool total", lambda v: v["records"][3].__setitem__("totalStrokes", True), None),
        ("runtime final score removed", lambda v: v["records"][-1].pop("finalScore"), None),
        ("runtime final penalty exceeds strokes", lambda v: v["records"][-1]["finalScore"]["holeRows"][0].__setitem__("penalties", 5), None),
    ]
    for name, mutate, nonce_override in runtime_mutations:
        value = {
            "records": copy.deepcopy(runtime_records),
            "candidate": copy.deepcopy(runtime_candidate),
        }
        mutate(value)
        try:
            _validate_runtime_journal_records(
                value["records"],
                value["candidate"],
                nonce_override or capture_nonce,
                process_started,
                process_finished,
                runtime_launch_sha,
                runtime_manifest_sha,
                5,
            )
        except ContractError:
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")

    first_line_end = runtime_bytes.index(b"\n")
    duplicate_header = (
        b'{"candidateId":"duplicate",' + runtime_bytes[1:first_line_end + 1]
        + runtime_bytes[first_line_end + 1:]
    )
    byte_mutations = [
        ("runtime BOM", b"\xef\xbb\xbf" + runtime_bytes),
        ("runtime CRLF", runtime_bytes.replace(b"\n", b"\r\n")),
        ("runtime no terminal LF", runtime_bytes[:-1]),
        ("runtime presentation whitespace", b"{ " + runtime_bytes[1:]),
        ("runtime blank line", runtime_bytes + b"\n"),
        ("runtime duplicate JSON key", duplicate_header),
    ]
    for name, data in byte_mutations:
        try:
            _canonical_jsonl_records(data, 262144)
        except ContractError:
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")

    with tempfile.TemporaryDirectory(prefix="dg_runtime_journal_") as temporary:
        user_dir = Path(temporary) / runtime_candidate["userDirToken"]
        journal_path = user_dir / RUNTIME_JOURNAL_USERDIR_PATH
        journal_path.parent.mkdir(parents=True)
        journal_path.write_bytes(runtime_bytes)
        journal_mtime = datetime(2099, 1, 1, 0, 0, 30, tzinfo=timezone.utc)
        os.utime(journal_path, (journal_mtime.timestamp(), journal_mtime.timestamp()))
        modified_text = journal_mtime.isoformat(timespec="milliseconds").replace(
            "+00:00", "Z"
        )
        digest = _sha256(runtime_bytes)
        manifest_binding = {
            "bytes": len(runtime_bytes),
            "captureNonce": capture_nonce,
            "eventCount": 12,
            "firstSequence": 1,
            "lastSequence": 12,
            "modifiedUtc": modified_text,
            "modifiedWithinRunWindow": True,
            "roundId": runtime_records[0]["roundId"],
            "schema": RUNTIME_JOURNAL_SCHEMA,
            "sha256": digest,
            "userDirRelativePath": RUNTIME_JOURNAL_USERDIR_PATH,
            "validationFailures": [],
            "validationState": "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL",
        }
        fresh_runtime_binding = {
            "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
            "userDirRelativePath": RUNTIME_JOURNAL_USERDIR_PATH,
            "bytes": len(runtime_bytes),
            "sha256": digest,
            "captureNonce": capture_nonce,
            "roundId": runtime_records[0]["roundId"],
            "launchRecordSha256": runtime_launch_sha,
            "technicalEvidenceManifestSha256": runtime_manifest_sha,
            "executableSha256": runtime_candidate["shippingExecutable"]["sha256"],
            "archiveManifestSha256": runtime_candidate["archive"][
                "canonicalManifestSha256"
            ],
            "validationState": "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL",
        }
        fresh_receipt = {
            "observedFiles": [
                {
                    "relativePath": RUNTIME_JOURNAL_USERDIR_PATH,
                    "bytes": len(runtime_bytes),
                    "sha256": digest,
                    "classification": "runtime_checkpoint_journal_candidate_bound",
                }
            ],
            "runtimeJournalBinding": fresh_runtime_binding,
        }
        runtime_context = {
            "runtimeJournalCaptureRequested": True,
            "launch": {
                "captureNonce": capture_nonce,
                "runtimeCheckpointJournalUserDirRelativePath": (
                    RUNTIME_JOURNAL_USERDIR_PATH
                ),
                "recordedUtc": "2099-01-01T00:00:00.000Z",
            },
            "manifest": {
                "process": {
                    "startedUtc": "2099-01-01T00:00:00.000Z",
                    "finishedUtc": "2099-01-01T00:01:00.000Z",
                },
                "evidence": {"runtimeCheckpointJournal": manifest_binding},
            },
        }
        binding, validated = _validate_runtime_journal_file(
            user_dir,
            policy,
            runtime_candidate,
            fresh_receipt,
            runtime_context,
            {"launch": runtime_launch_sha, "manifest": runtime_manifest_sha},
            {},
        )
        if (
            binding is None
            or validated is None
            or binding["sha256"] != digest
            or validated["roundId"] != runtime_records[0]["roundId"]
        ):
            failures.append("bound runtime journal file did not validate")
        end_to_end_mutations = [
            (
                "runtime manifest raw hash",
                lambda fresh, context: context["manifest"]["evidence"][
                    "runtimeCheckpointJournal"
                ].__setitem__("sha256", "0" * 64),
            ),
            (
                "runtime fresh classification",
                lambda fresh, context: fresh["observedFiles"][0].__setitem__(
                    "classification", "generic"
                ),
            ),
            (
                "runtime fresh binding omitted",
                lambda fresh, context: fresh.pop("runtimeJournalBinding"),
            ),
            (
                "runtime launch nonce",
                lambda fresh, context: context["launch"].__setitem__(
                    "captureNonce", "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
                ),
            ),
        ]
        for name, mutate in end_to_end_mutations:
            fresh_value = copy.deepcopy(fresh_receipt)
            context_value = copy.deepcopy(runtime_context)
            mutate(fresh_value, context_value)
            try:
                _validate_runtime_journal_file(
                    user_dir,
                    policy,
                    runtime_candidate,
                    fresh_value,
                    context_value,
                    {"launch": runtime_launch_sha, "manifest": runtime_manifest_sha},
                    {},
                )
            except ContractError:
                caught += 1
            else:
                failures.append(f"mutation escaped detection: {name}")
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--external-run", type=Path)
    parser.add_argument("--user-dir", type=Path)
    parser.add_argument("--checkpoint-chain", type=Path)
    parser.add_argument("--policy", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-receipt", type=Path)
    parser.add_argument("--legacy-v1-revalidate", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.output is not None and args.expected_receipt is not None:
        parser.error("--output and --expected-receipt are mutually exclusive")
    if args.self_test and any((args.output, args.expected_receipt, args.release_required)):
        parser.error("--self-test does not accept receipt or release arguments")

    if args.legacy_v1_revalidate:
        if args.policy is None:
            parser.error("legacy v1 mode requires an explicit --policy")
        if args.output is not None:
            parser.error("legacy v1 mode is read-only and rejects --output")
        if args.checkpoint_chain is not None:
            parser.error("legacy v1 mode does not accept --checkpoint-chain")
        policy_path = args.policy
        try:
            policy = _strict_json(policy_path.read_bytes(), policy_path.name)
            _validate_policy_v1(policy)
        except (OSError, ContractError) as exc:
            parser.error(f"invalid legacy v1 policy: {exc}")
        candidate = policy["candidate"]
        if args.self_test:
            if any(value is None for value in (args.archive, args.external_run, args.user_dir)):
                parser.error("legacy v1 self-test requires --archive, --external-run, and --user-dir")
            caught, failures = _self_test_v1(
                args.archive, args.external_run, args.user_dir, policy_path
            )
            minimum = policy["selfTestMinimumMutationCount"]
            if failures or caught < minimum:
                print(f"Session 19 legacy v1 self-test: FAIL ({caught} caught)")
                for failure in failures:
                    print(f"- {failure}")
                return 1
            print(f"Session 19 legacy v1 self-test: PASS ({caught}/{caught})")
            return 0
        if args.expected_receipt is None:
            parser.error("legacy v1 mode requires --expected-receipt and never writes receipts")
        if any(value is None for value in (args.archive, args.external_run, args.user_dir)):
            parser.error("--archive, --external-run, and --user-dir are required")
        if args.candidate_id != candidate["candidateId"]:
            parser.error("candidate ID differs from the legacy candidate-bound policy")
        if args.archive.name.casefold() != "windows" or args.archive.parent.name != args.candidate_id:
            parser.error("archive must be the Windows root under the exact candidate ID")
        if args.external_run.name != candidate["externalRunToken"]:
            parser.error("external-run token differs from the legacy policy")
        if args.user_dir.name != candidate["userDirToken"]:
            parser.error("UserDir token differs from the legacy policy")
        expected_path = ROOT / policy["receiptPattern"].format(candidateId=args.candidate_id)
        if args.expected_receipt.resolve() != expected_path.resolve():
            parser.error("legacy receipt path differs from its candidate identity")
        try:
            receipt = _candidate_receipt_v1(
                args.candidate_id,
                args.archive,
                args.external_run,
                args.user_dir,
                policy_path,
            )
            _expected_receipt_v1(args.expected_receipt, receipt)
        except (OSError, ContractError) as exc:
            print("Session 19 legacy v1 technical acceptance revalidation: FAIL")
            print(f"- {exc}")
            return 1
        print(f"Session 19 legacy v1 technical acceptance revalidation: {LEGACY_STATE}")
        if args.release_required:
            print(f"Session 19 release boundary: {RELEASE_STATE}")
            return 2
        return 0

    policy_path = args.policy or DEFAULT_POLICY_PATH
    try:
        policy = _strict_json(policy_path.read_bytes(), policy_path.name)
        _validate_policy_v2(policy)
    except (OSError, ContractError) as exc:
        parser.error(f"invalid v2 policy: {exc}")
    if args.self_test:
        if any((args.candidate_id, args.archive, args.external_run, args.user_dir, args.checkpoint_chain)):
            parser.error("v2 --self-test accepts only an optional --policy")
        caught, failures = _self_test_v2(policy_path)
        minimum = policy["selfTestMinimumMutationCount"]
        if failures or caught < minimum:
            print(f"Session 19 v2 technical acceptance self-test: FAIL ({caught} caught)")
            for failure in failures:
                print(f"- {failure}")
            return 1
        print(f"Session 19 v2 technical acceptance self-test: PASS ({caught}/{caught})")
        return 0

    if any(value is None for value in (args.archive, args.external_run, args.user_dir)):
        parser.error("--archive, --external-run, and --user-dir are required")
    if not args.candidate_id or not CANDIDATE_RE.fullmatch(args.candidate_id):
        parser.error("a canonical --candidate-id is required")
    if args.archive.name.casefold() != policy["target"]["archiveLeaf"].casefold() \
            or args.archive.parent.name != args.candidate_id:
        parser.error("archive must be the Windows root under the exact candidate ID")
    expected_output = ROOT / policy["receiptPattern"].format(candidateId=args.candidate_id)
    selected = args.output or args.expected_receipt
    if selected is not None and selected.resolve() != expected_output.resolve():
        parser.error("receipt path must use the candidate-bound Evidence/Session19 identity")
    try:
        receipt = _candidate_receipt_v2(
            args.candidate_id,
            args.archive,
            args.external_run,
            args.user_dir,
            policy_path,
            args.checkpoint_chain,
        )
        if args.expected_receipt is not None:
            _expected_receipt_v2(args.expected_receipt, receipt)
        if args.output is not None:
            _write_immutable(args.output, receipt)
    except (OSError, ContractError) as exc:
        print("Session 19 v2 three-hole evidence validation: FAIL")
        print(f"- {exc}")
        return 1
    print(f"Session 19 v2 three-hole evidence validation: {receipt['state']}")
    print(f"technicalAcceptance={str(receipt['technicalAcceptance']).lower()}")
    if args.release_required:
        print(f"Session 19 release boundary: {RELEASE_STATE}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
