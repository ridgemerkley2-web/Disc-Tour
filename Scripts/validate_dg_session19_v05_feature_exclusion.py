#!/usr/bin/env python3
"""Candidate-bound v0.5 Shipping exclusion gate for Career/AI and Throw Lab.

The gate is intentionally narrower than release approval.  It independently
binds an immutable archive, its archived Shipping executable, the authoritative
container/content audit, the executable marker audit, and a fresh UserDir
manifest.  Absolute host paths are consumed read-only and never serialized.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tempfile
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POLICY = (
    ROOT / "Config/DG_Session19V05CareerAiThrowLabShippingExclusionPolicy.json"
)
SCHEMA = "DiscGolfTour.Session19V05CareerAiThrowLabShippingExclusionReceipt.v1"
SESSION = 19
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
INNER_EXE = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"


class DuplicateKeyError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_text(text: str) -> Any:
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def load_json(path: Path) -> Any:
    return load_json_text(path.read_text(encoding="utf-8-sig"))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


def canonical_relative(value: str) -> str:
    candidate = value.replace("\\", "/")
    pure = PurePosixPath(candidate)
    if (
        not candidate
        or candidate.startswith("/")
        or pure.is_absolute()
        or any(part in ("", ".", "..") for part in pure.parts)
        or pure.as_posix() != candidate
        or ":" in candidate
        or "\x00" in candidate
    ):
        raise ValueError("non-canonical relative identity")
    return candidate


def is_reparse_point(path: Path) -> bool:
    try:
        info = os.lstat(path)
    except OSError:
        return True
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(
        getattr(info, "st_file_attributes", 0) & reparse_flag
    )


def collect_archive(archive: Path) -> list[dict[str, Any]]:
    root = archive.resolve(strict=True)
    if not root.is_dir() or is_reparse_point(root):
        raise ValueError("archive root is not a regular directory")
    result: list[dict[str, Any]] = []
    casefolded: set[str] = set()
    for current_root, directory_names, file_names in os.walk(
        root, topdown=True, followlinks=False
    ):
        current = Path(current_root)
        for name in list(directory_names):
            candidate = current / name
            if is_reparse_point(candidate):
                raise ValueError("archive contains a reparse point")
        for name in file_names:
            candidate = current / name
            if is_reparse_point(candidate):
                raise ValueError("archive contains a reparse point")
            identity = canonical_relative(candidate.relative_to(root).as_posix())
            folded = identity.casefold()
            if folded in casefolded:
                raise ValueError("archive contains a case-insensitive path collision")
            casefolded.add(folded)
            info = candidate.stat()
            if not stat.S_ISREG(info.st_mode):
                raise ValueError("archive contains a non-regular file")
            result.append(
                {
                    "path": identity,
                    "bytes": info.st_size,
                    "sha256": sha256_file(candidate),
                }
            )
    result.sort(key=lambda item: item["path"].casefold())
    return result


def canonical_archive_sha256(files: Iterable[dict[str, Any]]) -> str:
    material = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in files
    )
    return sha256_bytes(material.encode("utf-8"))


def file_binding(path: Path, label: str) -> dict[str, Any]:
    return {
        "artifact": label,
        "fileName": path.name,
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "hostPathRecorded": False,
    }


def expect_dict(value: Any, code: str, failures: list[dict[str, Any]]) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    failures.append({"code": code})
    return {}


def expect_list(value: Any, code: str, failures: list[dict[str, Any]]) -> list[Any]:
    if isinstance(value, list):
        return value
    failures.append({"code": code})
    return []


def validate_policy(policy: Any) -> list[str]:
    problems: list[str] = []
    if not isinstance(policy, dict):
        return ["POLICY_NOT_OBJECT"]
    if policy.get("schema") != (
        "DiscGolfTour.Session19V05CareerAiThrowLabShippingExclusionPolicy.v1"
    ):
        problems.append("POLICY_SCHEMA")
    if policy.get("schemaVersion") != 1 or policy.get("session") != SESSION:
        problems.append("POLICY_VERSION_OR_SESSION")
    target = policy.get("target")
    if target != {
        "platform": "Win64",
        "configuration": "Shipping",
        "milestone": "v0.5",
    }:
        problems.append("POLICY_TARGET")
    binding = policy.get("candidateBinding")
    if not isinstance(binding, dict) or set(binding.values()) != {True, False}:
        problems.append("POLICY_CANDIDATE_BINDING")
    elif binding.get("hostPathsMayBeRecorded") is not False or any(
        binding.get(key) is not True
        for key in (
            "requireCandidateVerification",
            "requireIndependentArchiveCanonicalManifest",
            "requireCandidateContentReceipt",
            "requireActualArchivedExecutableScan",
            "requireShippingBinaryReceipt",
            "requireFreshUserDirManifest",
        )
    ):
        problems.append("POLICY_CANDIDATE_BINDING")
    required_files = policy.get("requiredArchiveFiles")
    if not isinstance(required_files, list) or INNER_EXE not in required_files:
        problems.append("POLICY_REQUIRED_ARCHIVE_FILES")
    content = policy.get("contentReceipt")
    if not isinstance(content, dict) or content.get("requiredZeroMatchCategories") != [
        "CAREER_AI", "THROW_LAB"
    ]:
        problems.append("POLICY_CONTENT_CATEGORIES")
    binary_scan = policy.get("binaryScan")
    groups = binary_scan.get("forbiddenGroups") if isinstance(binary_scan, dict) else None
    expected_groups = {
        "developer_module",
        "career_entrypoints_saves_and_symbols",
        "ai_entrypoints_spawn_and_symbols",
        "throw_lab_entrypoints_saves_and_symbols",
    }
    if not isinstance(groups, list):
        problems.append("POLICY_BINARY_GROUPS")
    else:
        ids: set[str] = set()
        markers: set[str] = set()
        for group in groups:
            if not isinstance(group, dict) or not isinstance(group.get("id"), str):
                problems.append("POLICY_BINARY_GROUP")
                continue
            ids.add(group["id"])
            values = group.get("markers")
            if not isinstance(values, list) or not values:
                problems.append("POLICY_BINARY_MARKERS")
                continue
            for marker in values:
                if not isinstance(marker, str) or not marker.strip():
                    problems.append("POLICY_BINARY_MARKER")
                    continue
                folded = marker.casefold()
                if folded in markers:
                    problems.append("POLICY_DUPLICATE_BINARY_MARKER")
                markers.add(folded)
        if ids != expected_groups:
            problems.append("POLICY_BINARY_GROUP_IDS")
    fresh = policy.get("freshUserDirManifest")
    if not isinstance(fresh, dict) or not fresh.get("forbiddenPathMarkers"):
        problems.append("POLICY_FRESH_USERDIR")
    assertions = policy.get("technicalAssertions")
    if not isinstance(assertions, dict) or not assertions or not all(
        value is True for value in assertions.values()
    ):
        problems.append("POLICY_TECHNICAL_ASSERTIONS")
    claim = policy.get("claimBoundary")
    required_false_claims = (
        "publicCareerClaimAllowed",
        "publicAiOpponentClaimAllowed",
        "publicThrowLabClaimAllowed",
        "manualGameplayReviewComplete",
        "humanPlayFeelApproval",
        "legalApproval",
        "distributionClearance",
        "releaseReady",
    )
    if not isinstance(claim, dict) or any(
        claim.get(key) is not False for key in required_false_claims
    ) or claim.get("manualGameplayReviewRequired") is not True or claim.get(
        "blockersClosed"
    ) != []:
        problems.append("POLICY_CLAIM_BOUNDARY")
    evidence = policy.get("evidenceContract")
    if not isinstance(evidence, dict) or evidence.get("releaseReady") is not False:
        problems.append("POLICY_EVIDENCE_BOUNDARY")
    return sorted(set(problems))


def scan_binary(path: Path, groups: list[dict[str, Any]]) -> dict[str, Any]:
    patterns: list[tuple[str, str, str, bytes]] = []
    for group in groups:
        for marker in group["markers"]:
            patterns.append((group["id"], marker, "ascii", marker.casefold().encode("ascii")))
            patterns.append(
                (group["id"], marker, "utf-16le", marker.casefold().encode("utf-16le"))
            )
    max_pattern = max(len(item[3]) for item in patterns)
    found: set[tuple[str, str, str]] = set()
    carry = b""
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            window = (carry + block).lower()
            for group_id, marker, encoding, encoded in patterns:
                key = (group_id, marker, encoding)
                if key not in found and encoded in window:
                    found.add(key)
            carry = window[-(max_pattern - 1):] if max_pattern > 1 else b""
    matches = [
        {"groupId": group_id, "marker": marker, "encoding": encoding}
        for group_id, marker, encoding in sorted(found)
    ]
    per_group = []
    for group in groups:
        group_matches = [item for item in matches if item["groupId"] == group["id"]]
        per_group.append(
            {
                "id": group["id"],
                "passed": not group_matches,
                "matchCount": len(group_matches),
                "matches": group_matches,
            }
        )
    return {
        "caseInsensitive": True,
        "encodings": ["ascii", "utf-16le"],
        "forbiddenMatchCount": len(matches),
        "groups": per_group,
    }


def artifact_candidates_match(candidate_id: str, documents: Iterable[dict[str, Any]]) -> bool:
    for document in documents:
        value = document.get("runId", document.get("candidateId"))
        if value is not None and value != candidate_id:
            return False
    return True


def base_receipt(candidate_id: str | None) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "schemaVersion": 1,
        "session": SESSION,
        "candidateId": candidate_id,
        "generatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": "FAIL_CANDIDATE_BOUND_V05_CAREER_AI_THROW_LAB_EXCLUSION",
        "passed": False,
        "bindings": {},
        "archive": {},
        "binaryScan": {},
        "contentExclusion": {},
        "freshUserDirExclusion": {},
        "technicalAssertions": {},
        "failures": [],
        "claimBoundary": {
            "technicalExclusionEvidenceOnly": True,
            "publicCareerClaimAllowed": False,
            "publicAiOpponentClaimAllowed": False,
            "publicThrowLabClaimAllowed": False,
            "manualGameplayReviewRequired": True,
            "manualGameplayReviewComplete": False,
            "humanPlayFeelApproval": False,
            "legalApproval": False,
            "distributionClearance": False,
            "blockerClosureClaimed": False,
            "releaseReady": False,
        },
        "disclaimer": (
            "This receipt proves only bounded technical exclusion assertions for one "
            "immutable candidate. Public product claims, manual gameplay review, human "
            "play-feel approval, legal approval, distribution clearance, blocker closure, "
            "and release readiness remain pending."
        ),
    }


def audit(
    *,
    policy_path: Path,
    candidate_id: str,
    archive: Path,
    verification_path: Path,
    content_path: Path,
    binary_receipt_path: Path,
    fresh_path: Path,
) -> dict[str, Any]:
    receipt = base_receipt(candidate_id)
    failures: list[dict[str, Any]] = receipt["failures"]
    input_paths = {
        "validator": (Path(__file__).resolve(), "validator"),
        "policy": (policy_path, "policy"),
        "candidateVerification": (verification_path, "candidate-verification"),
        "candidateContent": (content_path, "candidate-content"),
        "shippingBinary": (binary_receipt_path, "shipping-binary"),
        "freshUserDirManifest": (fresh_path, "fresh-userdir-manifest"),
    }
    try:
        policy = load_json(policy_path)
        verification = load_json(verification_path)
        content = load_json(content_path)
        binary_receipt = load_json(binary_receipt_path)
        fresh = load_json(fresh_path)
        receipt["bindings"] = {
            key: file_binding(path, label)
            for key, (path, label) in input_paths.items()
        }
    except (OSError, ValueError, TypeError, DuplicateKeyError) as error:
        failures.append({"code": "INPUT_ARTIFACT_UNREADABLE", "errorType": type(error).__name__})
        return receipt

    for code in validate_policy(policy):
        failures.append({"code": code})
    if failures:
        return receipt

    if not artifact_candidates_match(candidate_id, (verification, content, fresh)):
        failures.append({"code": "CANDIDATE_ID_BINDING_MISMATCH"})

    try:
        archive_files = collect_archive(archive)
    except (OSError, ValueError, RuntimeError) as error:
        failures.append({"code": "ARCHIVE_INVENTORY_FAILED", "errorType": type(error).__name__})
        return receipt
    by_path = {item["path"]: item for item in archive_files}
    archive_sha = canonical_archive_sha256(archive_files)
    required_files = policy["requiredArchiveFiles"]
    missing_required = [value for value in required_files if value not in by_path]
    archive_path_matches: list[dict[str, str]] = []
    for item in archive_files:
        normalized = f"/{item['path'].strip('/')}/".casefold()
        for marker in policy["archiveForbiddenPathMarkers"]:
            if marker.casefold() in normalized:
                archive_path_matches.append({"identity": item["path"], "marker": marker})
    receipt["archive"] = {
        "hostPathRecorded": False,
        "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
        "fileCount": len(archive_files),
        "bytes": sum(item["bytes"] for item in archive_files),
        "canonicalManifestSha256": archive_sha,
        "missingRequiredFiles": missing_required,
        "forbiddenPathMatchCount": len(archive_path_matches),
        "forbiddenPathMatches": archive_path_matches,
    }
    if missing_required:
        failures.append({"code": "ARCHIVE_REQUIRED_FILE_MISSING"})
    if archive_path_matches:
        failures.append({"code": "ARCHIVE_FORBIDDEN_PATH_IDENTITY"})

    verification_archive = expect_dict(
        verification.get("archiveContract"), "VERIFICATION_ARCHIVE_CONTRACT", failures
    )
    verification_exe = expect_dict(
        verification.get("innerShippingExecutable"), "VERIFICATION_EXECUTABLE", failures
    )
    plugin_section = expect_dict(
        verification.get("shippingPluginCapabilities"),
        "VERIFICATION_PLUGIN_SECTION",
        failures,
    )
    expected_token = f"DGTOUR_PACKAGES/{candidate_id}/Windows"
    if (
        verification.get("schema")
        != "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
        or not str(verification.get("state", "")).startswith("PASS_")
        or verification.get("releaseReady") is not False
        or verification_archive.get("recoveryLocationToken") != expected_token
        or verification_archive.get("allRequiredFilesPresent") is not True
        or verification_exe.get("relativePath") != INNER_EXE
        or plugin_section.get("finalArchiveCanonicalManifestSha256", "").upper()
        != archive_sha
    ):
        failures.append({"code": "CANDIDATE_VERIFICATION_MISMATCH"})

    content_archive = expect_dict(content.get("archive"), "CONTENT_ARCHIVE", failures)
    categories = expect_dict(
        content.get("forbiddenIdentityCategories"), "CONTENT_CATEGORIES", failures
    )
    content_results: dict[str, Any] = {}
    for category_name in policy["contentReceipt"]["requiredZeroMatchCategories"]:
        category = expect_dict(
            categories.get(category_name), f"CONTENT_CATEGORY_{category_name}", failures
        )
        category_pass = (
            category.get("passed") is True
            and category.get("matchCount") == 0
            and category.get("matches") == []
            and category.get("manifestUfsDiagnosticMatchCount") == 0
            and category.get("manifestUfsDiagnosticMatches") == []
        )
        content_results[category_name] = {
            "passed": category_pass,
            "authoritativeMatchCount": category.get("matchCount"),
            "manifestDiagnosticMatchCount": category.get("manifestUfsDiagnosticMatchCount"),
        }
        if not category_pass:
            failures.append({"code": f"CONTENT_{category_name}_IDENTITY_SURVIVED"})
    receipt["contentExclusion"] = content_results
    if (
        content.get("schema") != policy["contentReceipt"]["schema"]
        or content.get("state") != policy["contentReceipt"]["passState"]
        or content.get("issues") != []
        or content_archive.get("canonicalManifestSha256", "").upper() != archive_sha
        or content_archive.get("fileCount") != len(archive_files)
        or content_archive.get("bytes") != receipt["archive"]["bytes"]
    ):
        failures.append({"code": "CANDIDATE_CONTENT_RECEIPT_MISMATCH"})

    executable = archive / Path(INNER_EXE)
    if executable.exists():
        actual_exe = by_path.get(INNER_EXE, {})
        receipt["binaryScan"] = scan_binary(
            executable, policy["binaryScan"]["forbiddenGroups"]
        )
        if receipt["binaryScan"]["forbiddenMatchCount"]:
            failures.append({"code": "SHIPPING_EXECUTABLE_FORBIDDEN_MARKER"})
        binary_input = expect_dict(binary_receipt.get("input"), "BINARY_RECEIPT_INPUT", failures)
        binary_scan_receipt = expect_dict(
            binary_receipt.get("scan"), "BINARY_RECEIPT_SCAN", failures
        )
        if (
            verification_exe.get("bytes") != actual_exe.get("bytes")
            or str(verification_exe.get("sha256", "")).upper() != actual_exe.get("sha256")
            or binary_receipt.get("schema") != policy["binaryReceipt"]["schema"]
            or binary_receipt.get("state") != policy["binaryReceipt"]["passState"]
            or binary_receipt.get("passed") is not True
            or binary_receipt.get("errors") != []
            or binary_input.get("bytes") != actual_exe.get("bytes")
            or str(binary_input.get("sha256", "")).upper() != actual_exe.get("sha256")
            or binary_scan_receipt.get("forbiddenMatchCount") != 0
            or binary_scan_receipt.get("forbiddenMatches") != []
            or binary_receipt.get("releaseReady") is not False
        ):
            failures.append({"code": "SHIPPING_BINARY_RECEIPT_MISMATCH"})
    else:
        failures.append({"code": "SHIPPING_EXECUTABLE_MISSING"})

    fresh_counts = expect_dict(fresh.get("counts"), "FRESH_USERDIR_COUNTS", failures)
    fresh_files = expect_list(fresh.get("observedFiles"), "FRESH_USERDIR_FILES", failures)
    fresh_failures = expect_list(fresh.get("failures"), "FRESH_USERDIR_FAILURES", failures)
    fresh_path_matches: list[dict[str, str]] = []
    observed_paths: list[str] = []
    for item in fresh_files:
        if not isinstance(item, dict) or not isinstance(item.get("relativePath"), str):
            failures.append({"code": "FRESH_USERDIR_FILE_RECORD_INVALID"})
            continue
        relative = item["relativePath"]
        try:
            canonical_relative(relative)
        except ValueError:
            failures.append({"code": "FRESH_USERDIR_PATH_INVALID"})
            continue
        observed_paths.append(relative)
        folded = relative.casefold()
        for marker in policy["freshUserDirManifest"]["forbiddenPathMarkers"]:
            if marker.casefold() in folded:
                fresh_path_matches.append({"identity": relative, "marker": marker})
    required_profile = policy["freshUserDirManifest"]["requiredProductionProfile"]
    fresh_pass = (
        fresh.get("schema") == policy["freshUserDirManifest"]["schema"]
        and fresh.get("state") == policy["freshUserDirManifest"]["passState"]
        and fresh.get("candidateId") == candidate_id
        and fresh.get("hostPathRecorded") is False
        and fresh_failures == []
        and fresh_counts.get("unknownFiles") == 0
        and fresh_counts.get("forbiddenArtifacts") == 0
        and required_profile in observed_paths
        and not fresh_path_matches
        and fresh.get("releaseBoundary", {}).get("releaseReadinessClaimed") is False
    )
    receipt["freshUserDirExclusion"] = {
        "passed": fresh_pass,
        "userDirToken": fresh.get("userDirToken"),
        "candidateUserDirBindingSha256": fresh.get("candidateUserDirBindingSha256"),
        "observedFileCount": len(observed_paths),
        "unknownFiles": fresh_counts.get("unknownFiles"),
        "forbiddenArtifacts": fresh_counts.get("forbiddenArtifacts"),
        "forbiddenPathMatchCount": len(fresh_path_matches),
        "forbiddenPathMatches": fresh_path_matches,
        "careerSaveCount": sum("dgt_career_" in value.casefold() for value in observed_paths),
        "throwLabSaveCount": sum("dgt_throwlab" in value.casefold() for value in observed_paths),
        "aiSpawnSideEffectCount": sum(
            any(token in value.casefold() for token in ("aigolfer", "aishotplanner"))
            for value in observed_paths
        ),
    }
    if not fresh_pass:
        failures.append({"code": "FRESH_USERDIR_EXCLUSION_MISMATCH"})

    binary_groups = {
        item["id"]: item for item in receipt.get("binaryScan", {}).get("groups", [])
    }
    content_career = content_results.get("CAREER_AI", {}).get("passed") is True
    content_throw = content_results.get("THROW_LAB", {}).get("passed") is True
    archive_clean = not archive_path_matches
    fresh_clean = fresh_pass
    assertions = {
        "careerShippingEntrypointAbsent": binary_groups.get(
            "career_entrypoints_saves_and_symbols", {}
        ).get("passed") is True,
        "careerShippingSaveAbsent": fresh_clean and receipt["freshUserDirExclusion"]["careerSaveCount"] == 0,
        "careerModuleAndSymbolIdentityAbsent": archive_clean and content_career and binary_groups.get(
            "developer_module", {}
        ).get("passed") is True,
        "aiShippingEntrypointAbsent": binary_groups.get(
            "ai_entrypoints_spawn_and_symbols", {}
        ).get("passed") is True,
        "aiSpawnCapabilityIdentityAbsent": archive_clean and content_career and binary_groups.get(
            "ai_entrypoints_spawn_and_symbols", {}
        ).get("passed") is True,
        "aiSpawnFreshUserDirSideEffectAbsent": fresh_clean and receipt["freshUserDirExclusion"]["aiSpawnSideEffectCount"] == 0,
        "throwLabShippingEntrypointAbsent": binary_groups.get(
            "throw_lab_entrypoints_saves_and_symbols", {}
        ).get("passed") is True,
        "throwLabShippingSaveAbsent": fresh_clean and receipt["freshUserDirExclusion"]["throwLabSaveCount"] == 0,
        "throwLabModuleSymbolAndContentIdentityAbsent": archive_clean and content_throw and binary_groups.get(
            "developer_module", {}
        ).get("passed") is True,
    }
    receipt["technicalAssertions"] = assertions
    for name, passed in assertions.items():
        if not passed:
            failures.append({"code": "TECHNICAL_ASSERTION_FAILED", "assertion": name})

    failures.sort(key=lambda item: (item["code"], item.get("assertion", "")))
    if not failures:
        receipt["passed"] = True
        receipt["state"] = policy["evidenceContract"]["passState"]
    return receipt


def write_json_immutable(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise FileExistsError("evidence output already exists") from exc


def fixture_documents(
    archive: Path, candidate_id: str, policy: dict[str, Any], marker: bytes = b""
) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]]:
    files = {
        "DiscGolfTour.exe": b"launcher",
        INNER_EXE: b"MZ" + b"DiscGolfRuntimeFoundation" + marker,
        "Manifest_UFSFiles_Win64.txt": b"safe\trow\n",
        "Manifest_NonUFSFiles_Win64.txt": b"safe\trow\n",
    }
    for relative, payload in files.items():
        destination = archive / Path(relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)
    inventory = collect_archive(archive)
    by_path = {item["path"]: item for item in inventory}
    archive_sha = canonical_archive_sha256(inventory)
    archive_bytes = sum(item["bytes"] for item in inventory)
    exe = by_path[INNER_EXE]
    verification = {
        "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
        "schemaVersion": 2,
        "session": SESSION,
        "runId": candidate_id,
        "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
        "archiveContract": {
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
            "allRequiredFilesPresent": True,
        },
        "innerShippingExecutable": {
            "relativePath": INNER_EXE,
            "bytes": exe["bytes"],
            "sha256": exe["sha256"],
        },
        "shippingPluginCapabilities": {
            "finalArchiveCanonicalManifestSha256": archive_sha,
        },
        "releaseReady": False,
    }
    category = {
        "passed": True,
        "matchCount": 0,
        "matches": [],
        "manifestUfsDiagnosticMatchCount": 0,
        "manifestUfsDiagnosticMatches": [],
    }
    content = {
        "schema": policy["contentReceipt"]["schema"],
        "schemaVersion": 1,
        "session": SESSION,
        "runId": candidate_id,
        "state": policy["contentReceipt"]["passState"],
        "archive": {
            "canonicalManifestSha256": archive_sha,
            "fileCount": len(inventory),
            "bytes": archive_bytes,
        },
        "forbiddenIdentityCategories": {
            "CAREER_AI": copy.deepcopy(category),
            "THROW_LAB": copy.deepcopy(category),
        },
        "issues": [],
    }
    binary = {
        "schema": policy["binaryReceipt"]["schema"],
        "schemaVersion": 1,
        "session": SESSION,
        "state": policy["binaryReceipt"]["passState"],
        "passed": True,
        "errors": [],
        "input": {"bytes": exe["bytes"], "sha256": exe["sha256"]},
        "scan": {"forbiddenMatchCount": 0, "forbiddenMatches": []},
        "releaseReady": False,
    }
    profile = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    fresh = {
        "schema": policy["freshUserDirManifest"]["schema"],
        "schemaVersion": 1,
        "session": SESSION,
        "candidateId": candidate_id,
        "userDirToken": "selftest-userdir-token",
        "candidateUserDirBindingSha256": "0" * 64,
        "hostPathRecorded": False,
        "state": policy["freshUserDirManifest"]["passState"],
        "counts": {"unknownFiles": 0, "forbiddenArtifacts": 0},
        "failures": [],
        "observedFiles": [{"relativePath": profile, "bytes": 8, "sha256": "1" * 64}],
        "releaseBoundary": {"releaseReadinessClaimed": False},
    }
    return verification, content, binary, fresh


def write_fixture_jsons(
    root: Path,
    documents: tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]],
) -> tuple[Path, Path, Path, Path]:
    result: list[Path] = []
    for name, document in zip(("verification", "content", "binary", "fresh"), documents):
        path = root / f"{name}.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        result.append(path)
    return tuple(result)  # type: ignore[return-value]


def self_test(policy_path: Path) -> int:
    try:
        policy = load_json(policy_path)
    except (OSError, ValueError) as error:
        print(json.dumps({"state": "FAIL", "errorType": type(error).__name__}, indent=2))
        return 1
    policy_problems = validate_policy(policy)
    if policy_problems:
        print(json.dumps({"state": "FAIL", "policyProblems": policy_problems}, indent=2))
        return 1

    failures: list[str] = []
    completed = 0
    candidate = "S19_WindowsShipping_20990101T000000Z_selftest"

    def run_case(
        name: str,
        mutate: Any = None,
        marker: bytes = b"",
        should_pass: bool = False,
        expected_code: str | None = None,
    ) -> None:
        nonlocal completed
        with tempfile.TemporaryDirectory(prefix="dgt_s19_v05_exclusion_") as temp_name:
            root = Path(temp_name)
            archive = root / "Windows"
            archive.mkdir()
            documents = list(fixture_documents(archive, candidate, policy, marker))
            if mutate is not None:
                mutate(documents)
            verification, content, binary, fresh = write_fixture_jsons(root, tuple(documents))
            receipt = audit(
                policy_path=policy_path,
                candidate_id=candidate,
                archive=archive,
                verification_path=verification,
                content_path=content,
                binary_receipt_path=binary,
                fresh_path=fresh,
            )
            did_pass = receipt["passed"] is True
            codes = {item["code"] for item in receipt["failures"]}
            if did_pass != should_pass:
                failures.append(f"{name}: unexpected pass/fail")
            if expected_code is not None and expected_code not in codes:
                failures.append(f"{name}: missing {expected_code}")
            serialized = json.dumps(receipt)
            if temp_name in serialized or receipt["claimBoundary"]["releaseReady"] is not False:
                failures.append(f"{name}: host path or release claim leaked")
            completed += 1

    run_case("valid", should_pass=True)
    run_case(
        "candidate-mismatch",
        lambda docs: docs[0].__setitem__("runId", candidate + "x"),
        expected_code="CANDIDATE_ID_BINDING_MISMATCH",
    )
    run_case(
        "content-career-match",
        lambda docs: docs[1]["forbiddenIdentityCategories"]["CAREER_AI"].update(
            {"passed": False, "matchCount": 1, "matches": [{"identity": "Career"}]}
        ),
        expected_code="CONTENT_CAREER_AI_IDENTITY_SURVIVED",
    )
    run_case(
        "content-throwlab-diagnostic",
        lambda docs: docs[1]["forbiddenIdentityCategories"]["THROW_LAB"].update(
            {
                "manifestUfsDiagnosticMatchCount": 1,
                "manifestUfsDiagnosticMatches": [{"identity": "ThrowLab"}],
            }
        ),
        expected_code="CONTENT_THROW_LAB_IDENTITY_SURVIVED",
    )
    run_case(
        "binary-receipt-hash",
        lambda docs: docs[2]["input"].__setitem__("sha256", "F" * 64),
        expected_code="SHIPPING_BINARY_RECEIPT_MISMATCH",
    )
    run_case(
        "fresh-career-save",
        lambda docs: docs[3]["observedFiles"].append(
            {"relativePath": "Saved/SaveGames/DGT_Career_Dev_v1.sav", "bytes": 8, "sha256": "2" * 64}
        ),
        expected_code="FRESH_USERDIR_EXCLUSION_MISMATCH",
    )
    run_case(
        "fresh-throwlab-save",
        lambda docs: docs[3]["observedFiles"].append(
            {"relativePath": "Saved/SaveGames/DGT_ThrowLab_Dev_v1.sav", "bytes": 8, "sha256": "2" * 64}
        ),
        expected_code="FRESH_USERDIR_EXCLUSION_MISMATCH",
    )
    run_case(
        "fresh-ai-side-effect",
        lambda docs: docs[3]["observedFiles"].append(
            {"relativePath": "Saved/AIGolfer/Spawn.json", "bytes": 8, "sha256": "2" * 64}
        ),
        expected_code="FRESH_USERDIR_EXCLUSION_MISMATCH",
    )
    run_case(
        "fresh-failure",
        lambda docs: docs[3]["failures"].append({"code": "UNKNOWN_SAVEGAME"}),
        expected_code="FRESH_USERDIR_EXCLUSION_MISMATCH",
    )
    run_case(
        "career-ascii-marker",
        marker=b"DiscGolfCareerSubsystem",
        expected_code="SHIPPING_EXECUTABLE_FORBIDDEN_MARKER",
    )
    run_case(
        "ai-utf16-marker",
        marker="DiscGolfAIPlannerAdapter".encode("utf-16le"),
        expected_code="SHIPPING_EXECUTABLE_FORBIDDEN_MARKER",
    )
    run_case(
        "throwlab-ascii-marker",
        marker=b"DGT_ToggleThrowLab",
        expected_code="SHIPPING_EXECUTABLE_FORBIDDEN_MARKER",
    )
    run_case(
        "verification-archive-hash",
        lambda docs: docs[0]["shippingPluginCapabilities"].__setitem__(
            "finalArchiveCanonicalManifestSha256", "0" * 64
        ),
        expected_code="CANDIDATE_VERIFICATION_MISMATCH",
    )
    run_case(
        "content-archive-hash",
        lambda docs: docs[1]["archive"].__setitem__("canonicalManifestSha256", "0" * 64),
        expected_code="CANDIDATE_CONTENT_RECEIPT_MISMATCH",
    )

    invalid_policy = copy.deepcopy(policy)
    invalid_policy["claimBoundary"]["releaseReady"] = True
    completed += 1
    if "POLICY_CLAIM_BOUNDARY" not in validate_policy(invalid_policy):
        failures.append("release-claim mutation survived")
    completed += 1
    try:
        load_json_text('{"a":1,"a":2}')
    except DuplicateKeyError:
        pass
    else:
        failures.append("duplicate JSON key survived")
    completed += 1
    try:
        canonical_relative("../escape")
    except ValueError:
        pass
    else:
        failures.append("path traversal survived")
    with tempfile.TemporaryDirectory(prefix="dgt_s19_v05_immutable_") as temp_name:
        existing = Path(temp_name) / "receipt.json"
        existing.write_text("sentinel", encoding="utf-8")
        completed += 1
        try:
            write_json_immutable(existing, {"state": "must-not-overwrite"})
        except FileExistsError:
            if existing.read_text(encoding="utf-8") != "sentinel":
                failures.append("exclusive output collision changed existing bytes")
        else:
            failures.append("exclusive output collision was accepted")

    if failures:
        print("SESSION 19 V05 FEATURE EXCLUSION SELF-TEST FAILED")
        for failure in failures:
            print(" -", failure)
        return 1
    print(f"SESSION 19 V05 FEATURE EXCLUSION SELF-TEST PASS: {completed}/{completed}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--verification", type=Path)
    parser.add_argument("--content-receipt", type=Path)
    parser.add_argument("--binary-receipt", type=Path)
    parser.add_argument("--fresh-userdir-manifest", type=Path)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test(args.policy.resolve())
    required = (
        args.candidate_id,
        args.archive,
        args.verification,
        args.content_receipt,
        args.binary_receipt,
        args.fresh_userdir_manifest,
        args.output,
    )
    if any(value is None for value in required):
        parser.error(
            "validation requires --candidate-id, --archive, --verification, "
            "--content-receipt, --binary-receipt, --fresh-userdir-manifest, and --output"
        )
    if not CANDIDATE_RE.fullmatch(args.candidate_id):
        parser.error("--candidate-id is not a sanitized Session 19 Shipping identifier")

    receipt = audit(
        policy_path=args.policy.resolve(),
        candidate_id=args.candidate_id,
        archive=args.archive.resolve(),
        verification_path=args.verification.resolve(),
        content_path=args.content_receipt.resolve(),
        binary_receipt_path=args.binary_receipt.resolve(),
        fresh_path=args.fresh_userdir_manifest.resolve(),
    )
    try:
        write_json_immutable(args.output.resolve(), receipt)
    except (OSError, FileExistsError) as error:
        print(f"SESSION 19 V05 FEATURE EXCLUSION OUTPUT FAILED: {type(error).__name__}")
        return 1
    print(
        f"SESSION 19 V05 FEATURE EXCLUSION {receipt['state']}: "
        f"archiveFiles={receipt.get('archive', {}).get('fileCount', 0)} "
        f"binaryMatches={receipt.get('binaryScan', {}).get('forbiddenMatchCount', 0)} "
        f"failures={len(receipt['failures'])}"
    )
    return 0 if receipt["passed"] else 2


if __name__ == "__main__":
    sys.exit(main())
