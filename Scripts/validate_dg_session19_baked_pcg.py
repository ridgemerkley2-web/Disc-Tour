#!/usr/bin/env python3
"""Validate the additive Session 19 explicit-tree baked-PCG source authority.

This validator intentionally does not approve a release or close Session 13. It
proves the source authority and, in candidate mode, the exact final-archive
NonUFS authored-data binding plus compile-time separation of all graph-generation
APIs into the Editor module. Cooked graph absence, deterministic bake/reopen,
visual, collision, and performance proof stay pending.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
from functools import lru_cache
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = "Config/DG_Session19BakedPcgPolicy.json"
RECEIPT_PATH = "Evidence/Session19/BakedPcgSourceAuthorityReceipt.json"

NORMAL_STATUS = "PASS_SOURCE_AUTHORITY_AND_SHIPPING_PCG_SOURCE_SEPARATION_COOK_PROOF_PENDING"
RELEASE_STATUS = "BLOCKED_PENDING_RUNTIME_AND_BAKED_OUTPUT_ACCEPTANCE_EVIDENCE"
CANDIDATE_STATUS = (
    "PASS_CANDIDATE_BOUND_AUTHORED_DATA_RUNTIME_AND_BAKED_OUTPUT_ACCEPTANCE_PENDING"
)
CANDIDATE_ID_RE = re.compile(r"S19_WindowsShipping_[A-Za-z0-9_-]+")
CANDIDATE_RECEIPT_SCHEMA = "DiscGolfTour.Session19BakedPcgCandidateVerification.v1"
SOURCE_CLOSURE_SCHEMA = "DiscGolfTour.Session19BakedPcgSourceClosurePreBuild.v1"
CANDIDATE_RECEIPT_PATTERN = "Evidence/Session19/BakedPcgCandidateVerification-{candidateId}.json"
SOURCE_CLOSURE_PATTERN = "Evidence/Session19/BakedPcgSourceClosurePreBuild-{candidateId}.json"
NONUFS_MANIFEST_PATH = "Manifest_NonUFSFiles_Win64.txt"
UFS_MANIFEST_PATH = "Manifest_UFSFiles_Win64.txt"

EXPECTED_TARGET = {
    "platform": "Windows",
    "configuration": "Shipping",
    "milestone": "v0.5",
    "courseId": "PineRidgeChampionship",
    "layoutId": "Championship",
    "holeNumbers": [1, 2, 3],
}

EXPECTED_HOLES = [
    {
        "path": "Data/PineRidgeHole1.json",
        "holeNumber": 1,
        "treeCount": 12,
        "sourceBytes": 7740,
        "sourceSha256": "CD1991551FD3F13465A5C6737871A2D4B91A31E078CAD1C97F458A7A5081F317",
        "canonicalPayloadBytes": 816,
        "canonicalPayloadSha256": "5C74CCD61CB4871B062E3A3E8BB4A3A1E5053F2C6F93D58E417F0172E99B26E8",
    },
    {
        "path": "Data/PineRidgeHole2.json",
        "holeNumber": 2,
        "treeCount": 18,
        "sourceBytes": 8586,
        "sourceSha256": "AB55ACE68264C270DE932DC0E0DEA644B844AEBB29F6CC0E994614789CA96F2E",
        "canonicalPayloadBytes": 1180,
        "canonicalPayloadSha256": "E0CF19D98179253592C9C5F1A54270D321E9A56D8A9D8D17781345AA3C8CE1C7",
    },
    {
        "path": "Data/PineRidgeHole3.json",
        "holeNumber": 3,
        "treeCount": 14,
        "sourceBytes": 8509,
        "sourceSha256": "A1FC744234E94FBE383F5AED21DCEF9D84A4213CF9E8625B57CA4C3339EC46A2",
        "canonicalPayloadBytes": 972,
        "canonicalPayloadSha256": "61D73AB730DBAE0835E650A92096FB18D558C552C2EC69133B1B3C64D08E7101",
    },
]

EXPECTED_COMBINED = {
    "treeCount": 44,
    "bytes": 2982,
    "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
}

EXPECTED_NOT_CLAIMED = [
    "ALL_DECORATIVE_NONCOLLIDING_FOLIAGE_IS_PREBAKED",
    "COOKED_ASSET_REGISTRY_ABSENCE",
    "SHIPPING_BINARY_ABSENCE",
    "SHIPPING_UFS_NONUFS_OR_IOSTORE_ABSENCE",
    "SESSION13_RELEASE_BLOCKER_CLOSURE",
]

EXPECTED_PENDING = [
    "SHIPPING_LOG_PROVES_AUTHORED_JSON_FOR_HOLES_1_2_3_WITH_NO_SOURCE_FALLBACK",
    "COOKED_ASSET_REGISTRY_HAS_NO_RUNTIME_PCG_GENERATION_CONSUMER",
    "DETERMINISTIC_BAKE_SAVE_REOPEN_IDENTITY",
    "THREE_HOLE_COLLISION_VISUAL_AND_PERFORMANCE_ACCEPTANCE",
]

EXPECTED_CANDIDATE_EVIDENCE = {
    "state": CANDIDATE_STATUS,
    "receiptPattern": CANDIDATE_RECEIPT_PATTERN,
    "sourceClosurePreBuildReceiptPattern": SOURCE_CLOSURE_PATTERN,
    "archiveDataRoot": "DiscGolfTour/Data",
    "stagingClass": "NONUFS",
    "manifest": NONUFS_MANIFEST_PATH,
    "ufsOrIoStoreCopyRequired": False,
    "liveArchiveHashRevalidationRequired": True,
    "preBuildSourceClosureRequiredForFutureCandidates": True,
    "sourceClosureRetrofitAllowed": False,
    "acceptedProof": [
        "FINAL_WINDOWS_ARCHIVE_CANONICAL_MANIFEST_IDENTITY",
        "EXACT_NONUFS_MANIFEST_MEMBERSHIP_FOR_THREE_AUTHORED_HOLE_JSON_FILES",
        "EXACT_ARCHIVE_BYTES_AND_SHA256_MATCH_SOURCE_AUTHORITY",
        "SHIPPING_SOURCE_HAS_NO_RUNTIME_PCG_MODULE_OR_GENERATION_API",
    ],
    "stillPending": EXPECTED_PENDING,
}

FROZEN_SESSION13 = {
    "Config/DG_Session13CourseAuthoringPcgContract.json": (
        17700,
        "64554C71530F09CE4AC6F779A65A720C195112617DDEA7D08E574F4258A044F0",
    ),
    "Scripts/validate_dg_session13_course_authoring_pcg.py": (
        51940,
        "ABD65FA427ACB7099E603CC045C5333D3E3BD26C8C8E5EF6A07C3406B3A03E2F",
    ),
    "Docs/DG_SESSION13_COURSE_AUTHORING_PCG_AUDIT.md": (
        7257,
        "810610B96E1DCD2F700C908B48E48F9B9E5080B6F3A32FA461D7F60BDAE443F8",
    ),
}


def _canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _read_bytes(relative: str, overrides: dict[str, bytes]) -> bytes:
    if relative in overrides:
        return overrides[relative]
    return (ROOT / relative).read_bytes()


def _read_text(relative: str, overrides: dict[str, bytes]) -> str:
    return _read_bytes(relative, overrides).decode("utf-8")


def _load_json(relative: str, overrides: dict[str, bytes], errors: list[str]) -> Any:
    try:
        return json.loads(_read_text(relative, overrides))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        errors.append(f"{relative}: invalid or unreadable JSON: {exc}")
        return None


def _finite_number(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(value)


def _hole_payload(hole: dict[str, Any]) -> dict[str, Any]:
    return {
        "courseId": hole.get("courseId"),
        "layoutId": hole.get("layoutId"),
        "holeNumber": hole.get("holeNumber"),
        "trees": hole.get("trees"),
    }


def _validate_tree_array(hole: Any, expected: dict[str, Any], errors: list[str]) -> dict[str, Any] | None:
    path = expected["path"]
    if not isinstance(hole, dict):
        errors.append(f"{path}: root must be an object")
        return None
    for key, expected_value in (
        ("schema", "disc_golf_hole_blockout"),
        ("schemaVersion", 1),
        ("courseId", EXPECTED_TARGET["courseId"]),
        ("layoutId", EXPECTED_TARGET["layoutId"]),
        ("holeNumber", expected["holeNumber"]),
    ):
        if hole.get(key) != expected_value:
            errors.append(f"{path}: {key} differs from the baked authority")

    trees = hole.get("trees")
    if not isinstance(trees, list):
        errors.append(f"{path}: trees must be an explicit ordered array")
        return None
    if len(trees) != expected["treeCount"]:
        errors.append(
            f"{path}: tree count {len(trees)} differs from {expected['treeCount']}"
        )

    identities: set[bytes] = set()
    for index, tree in enumerate(trees):
        context = f"{path}: trees[{index}]"
        if not isinstance(tree, dict):
            errors.append(f"{context} must be an object")
            continue
        if set(tree) != {"locationCm", "heightScale"}:
            errors.append(f"{context} must contain only locationCm and heightScale")
        location = tree.get("locationCm")
        if not isinstance(location, dict) or set(location) != {"x", "y", "z"}:
            errors.append(f"{context}.locationCm must contain exactly x, y, z")
        else:
            for axis in ("x", "y", "z"):
                if not _finite_number(location.get(axis)):
                    errors.append(f"{context}.locationCm.{axis} must be finite")
        height = tree.get("heightScale")
        if not _finite_number(height) or not 0.25 <= float(height) <= 4.0:
            errors.append(f"{context}.heightScale must be finite and within [0.25, 4.0]")
        try:
            identity = _canonical_bytes(tree)
            if identity in identities:
                errors.append(f"{context} duplicates an earlier baked placement")
            identities.add(identity)
        except (TypeError, ValueError):
            errors.append(f"{context} cannot be canonicalized")

    return _hole_payload(hole)


def _extract_function(text: str, signature: str) -> tuple[int, int, str] | None:
    match = re.search(signature + r"\s*\{", text)
    if not match:
        return None
    open_brace = text.find("{", match.start())
    depth = 0
    for index in range(open_brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return match.start(), index + 1, text[match.start() : index + 1]
    return None


def _source_files(overrides: dict[str, bytes]) -> dict[str, str]:
    result: dict[str, str] = {}
    for root_name in ("Source/DiscGolfTour", "Source/DiscGolfTourEditor"):
        source_root = ROOT / root_name
        for path in source_root.rglob("*"):
            if path.is_file() and path.suffix.lower() in {".cpp", ".h"}:
                relative = path.relative_to(ROOT).as_posix()
                result[relative] = _read_text(relative, overrides)
    for relative, data in overrides.items():
        if relative.startswith("Source/") and Path(relative).suffix.lower() in {".cpp", ".h"}:
            result[relative] = data.decode("utf-8")
    return result


@lru_cache(maxsize=1)
def _serialized_content_markers() -> tuple[str, ...]:
    content = ROOT / "Content"
    rg = shutil.which("rg")
    if rg:
        command = [
            rg,
            "-a",
            "-l",
            "-e",
            "GenerateForest",
            "-e",
            "GenerateLocal",
            "--glob",
            "*.uasset",
            "--glob",
            "*.umap",
            str(content),
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode not in (0, 1):
            return (f"RG_SCAN_ERROR:{result.stderr.strip()}",)
        return tuple(
            sorted(
                Path(line).relative_to(ROOT).as_posix()
                for line in result.stdout.splitlines()
                if line.strip()
            )
        )

    matches: list[str] = []
    for pattern in ("*.uasset", "*.umap"):
        for path in content.rglob(pattern):
            data = path.read_bytes()
            if b"GenerateForest" in data or b"GenerateLocal" in data:
                matches.append(path.relative_to(ROOT).as_posix())
    return tuple(sorted(matches))


def _hash_file_stable(path: Path) -> tuple[int, str]:
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ValueError(f"file changed while it was hashed: {path}")
    return after.st_size, digest.hexdigest().upper()


def _is_reparse(path: Path) -> bool:
    attributes = getattr(path.lstat(), "st_file_attributes", 0)
    return bool(attributes & getattr(os.stat_result, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def _canonical_relative(value: str) -> str:
    normalized = value.replace("\\", "/")
    if (
        not normalized
        or normalized.startswith("/")
        or re.match(r"^[A-Za-z]:", normalized)
        or any(part in {"", ".", ".."} for part in normalized.split("/"))
    ):
        raise ValueError(f"path is not a canonical relative identity: {value!r}")
    return normalized


def _collect_archive(archive: Path) -> tuple[list[dict[str, Any]], str]:
    if not archive.is_dir() or archive.is_symlink() or _is_reparse(archive):
        raise ValueError("archive must be a regular, non-reparse directory")
    resolved_root = archive.resolve(strict=True)
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for current, directories, filenames in os.walk(archive, followlinks=False):
        current_path = Path(current)
        for name in list(directories):
            path = current_path / name
            if path.is_symlink() or _is_reparse(path):
                raise ValueError(f"archive contains a reparse directory: {path}")
        for name in filenames:
            path = current_path / name
            identity = _canonical_relative(path.relative_to(archive).as_posix())
            if identity.casefold() in seen:
                raise ValueError(f"archive contains a case-insensitive duplicate: {identity}")
            seen.add(identity.casefold())
            if path.is_symlink() or _is_reparse(path) or not path.is_file():
                raise ValueError(f"archive entry is not a regular file: {identity}")
            resolved = path.resolve(strict=True)
            if resolved_root not in resolved.parents:
                raise ValueError(f"archive entry resolves outside the archive: {identity}")
            byte_count, digest = _hash_file_stable(path)
            records.append({"path": identity, "bytes": byte_count, "sha256": digest})
    records.sort(key=lambda value: (value["path"].casefold(), value["path"]))
    payload = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return records, _sha256(payload)


def _parse_stage_manifest(path: Path) -> list[str]:
    if not path.is_file() or path.is_symlink() or _is_reparse(path):
        raise ValueError(f"stage manifest is missing or irregular: {path.name}")
    identities: list[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        parts = line.split("\t")
        if len(parts) != 2 or not parts[1].endswith("Z"):
            raise ValueError(f"malformed stage manifest line {line_number}: {path.name}")
        identities.append(_canonical_relative(parts[0]))
    lowered = [identity.casefold() for identity in identities]
    if not identities or len(lowered) != len(set(lowered)):
        raise ValueError(f"empty or duplicate stage manifest identities: {path.name}")
    return identities


def _source_closure_paths() -> list[str]:
    explicit = {
        POLICY_PATH,
        RECEIPT_PATH,
        "Scripts/validate_dg_session19_baked_pcg.py",
        "Data/PineRidgeCourse.json",
        "Config/DG_Session19ReleaseScopeContract.json",
        "Config/DG_Session19ShippingContentPolicy.json",
        *FROZEN_SESSION13.keys(),
        *(item["path"] for item in EXPECTED_HOLES),
    }
    for root_name in ("Source/DiscGolfTour", "Source/DiscGolfTourEditor"):
        source_root = ROOT / root_name
        for path in source_root.rglob("*"):
            if path.is_file() and path.suffix.lower() in {".cpp", ".h", ".cs"}:
                explicit.add(path.relative_to(ROOT).as_posix())
    return sorted(explicit, key=lambda value: (value.casefold(), value))


def _source_closure() -> dict[str, Any]:
    files: list[dict[str, Any]] = []
    for relative in _source_closure_paths():
        byte_count, digest = _hash_file_stable(ROOT / relative)
        files.append({"path": relative, "bytes": byte_count, "sha256": digest})
    markers = list(_serialized_content_markers())
    payload = {
        "files": files,
        "serializedContentGenerateMarkers": markers,
        "combinedCanonicalTreePayload": EXPECTED_COMBINED,
    }
    return {
        "algorithm": "SHA256_CANONICAL_JSON",
        "fileCount": len(files),
        "serializedContentGenerateMarkerCount": len(markers),
        "sha256": _sha256(_canonical_bytes(payload)),
        "files": files,
    }


def _write_immutable_json(path: Path, value: Any) -> None:
    if path.exists():
        raise ValueError(f"evidence output already exists: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(json.dumps(value, indent=2, ensure_ascii=False).encode("utf-8") + b"\n")


def _source_closure_receipt(candidate_id: str) -> dict[str, Any]:
    return {
        "schema": SOURCE_CLOSURE_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "verifiedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": "PASS_PREBUILD_BAKED_PCG_SOURCE_CLOSURE",
        "policy": POLICY_PATH,
        "closure": _source_closure(),
        "postBuildRevalidationRequired": True,
        "releaseReady": False,
        "receiptPath": SOURCE_CLOSURE_PATTERN.format(candidateId=candidate_id),
    }


def _validate_prebuild_source_closure(
    path: Path, candidate_id: str, current: dict[str, Any]
) -> dict[str, Any]:
    try:
        receipt_bytes = path.read_bytes()
        receipt = json.loads(receipt_bytes.decode("utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"pre-build source-closure receipt is unreadable: {exc}") from exc
    expected = {
        "schema": SOURCE_CLOSURE_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "state": "PASS_PREBUILD_BAKED_PCG_SOURCE_CLOSURE",
        "policy": POLICY_PATH,
        "postBuildRevalidationRequired": True,
        "releaseReady": False,
        "receiptPath": SOURCE_CLOSURE_PATTERN.format(candidateId=candidate_id),
    }
    if (
        not isinstance(receipt, dict)
        or not _parse_utc(receipt.get("verifiedUtc"))
        or any(receipt.get(key) != value for key, value in expected.items())
    ):
        raise ValueError("pre-build source-closure receipt contract differs")
    if receipt.get("closure") != current:
        raise ValueError("baked-PCG source closure changed between pre-build and post-build validation")
    return {
        "path": SOURCE_CLOSURE_PATTERN.format(candidateId=candidate_id),
        "bytes": len(receipt_bytes),
        "sha256": _sha256(receipt_bytes),
        "preBuildSha256": receipt["closure"]["sha256"],
    }


def _candidate_receipt(
    candidate_id: str,
    archive: Path,
    source_closure_input: Path | None,
) -> dict[str, Any]:
    source_errors = audit()
    if source_errors:
        raise ValueError("source authority failed before candidate validation: " + "; ".join(source_errors))
    records, archive_sha = _collect_archive(archive)
    by_path = {item["path"]: item for item in records}
    nonufs_path = archive / NONUFS_MANIFEST_PATH
    ufs_path = archive / UFS_MANIFEST_PATH
    nonufs = _parse_stage_manifest(nonufs_path)
    ufs = _parse_stage_manifest(ufs_path)
    nonufs_lower = [value.casefold() for value in nonufs]
    ufs_lower = [value.casefold() for value in ufs]
    executable_path = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
    executable = by_path.get(executable_path)
    if executable is None:
        raise ValueError("candidate archive is missing the inner Windows Shipping executable")
    staged_files: list[dict[str, Any]] = []
    for expected in EXPECTED_HOLES:
        archive_relative = f"DiscGolfTour/Data/{Path(expected['path']).name}"
        archive_record = by_path.get(archive_relative)
        if archive_record is None:
            raise ValueError(f"candidate archive is missing authored data: {archive_relative}")
        if (
            archive_record["bytes"] != expected["sourceBytes"]
            or archive_record["sha256"] != expected["sourceSha256"]
        ):
            raise ValueError(f"candidate authored data identity differs: {archive_relative}")
        if nonufs_lower.count(archive_relative.casefold()) != 1:
            raise ValueError(f"NonUFS manifest must bind authored data exactly once: {archive_relative}")
        if archive_relative.casefold() in ufs_lower:
            raise ValueError(f"authored data must not be represented in the UFS manifest: {archive_relative}")
        staged_files.append({
            "sourcePath": expected["path"],
            "archivePath": archive_relative,
            "bytes": archive_record["bytes"],
            "sha256": archive_record["sha256"],
            "sourceAuthorityMatch": True,
            "nonUfsManifestOccurrenceCount": 1,
            "ufsManifestOccurrenceCount": 0,
        })

    prebuild = None
    postbuild_sha = None
    if source_closure_input is not None:
        current_closure = _source_closure()
        prebuild = _validate_prebuild_source_closure(
            source_closure_input, candidate_id, current_closure
        )
        postbuild_sha = current_closure["sha256"]
    nonufs_bytes, nonufs_sha = _hash_file_stable(nonufs_path)
    return {
        "schema": CANDIDATE_RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "verifiedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": CANDIDATE_STATUS,
        "policy": POLICY_PATH,
        "archive": {
            "hostPathRecorded": False,
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
            "fileCount": len(records),
            "bytes": sum(item["bytes"] for item in records),
            "canonicalManifestSha256": archive_sha,
            "liveHashRevalidationPass": True,
        },
        "shippingExecutable": {
            "path": executable_path,
            "bytes": executable["bytes"],
            "sha256": executable["sha256"],
            "literalGenerateStringAbsenceClaimed": False,
        },
        "sourceClosure": {
            "requiredForFutureCandidates": True,
            "retrofitAllowed": False,
            "preBuildReceiptProvided": prebuild is not None,
            "preBuildReceipt": prebuild,
            "postBuildSha256": postbuild_sha,
            "prePostMatch": True if prebuild is not None else None,
        },
        "authoredRuntimeData": {
            "stagingClass": "NONUFS",
            "archiveDataRoot": "DiscGolfTour/Data",
            "manifest": {
                "path": NONUFS_MANIFEST_PATH,
                "bytes": nonufs_bytes,
                "sha256": nonufs_sha,
            },
            "files": staged_files,
            "allThreeExactSourceAuthorityMatches": True,
            "allThreeNonUfsManifestBindingsExact": True,
            "allThreeUfsManifestOccurrencesAbsent": True,
            "ioStoreMembershipClaimed": False,
        },
        "technicalBoundary": {
            "candidateBoundAuthoredDataPass": True,
            "runtimeAuthoredSelectionProven": False,
            "cookedRuntimePcgConsumerAbsenceProven": False,
            "reachableRuntimeGenerationCallsiteAbsenceProven": False,
            "literalBinaryStringAbsenceAcceptedAsProof": False,
            "deterministicBakeSaveReopenProven": False,
            "threeHoleCollisionVisualPerformanceAccepted": False,
        },
        "shippingPackageEvidenceAccepted": False,
        "blockerClosed": False,
        "releaseReady": False,
        "remainingEvidence": EXPECTED_PENDING,
        "receiptPath": CANDIDATE_RECEIPT_PATTERN.format(candidateId=candidate_id),
    }


def _parse_utc(value: Any) -> bool:
    if not isinstance(value, str) or not value.endswith("Z"):
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def _validate_expected_candidate_receipt(path: Path, live: dict[str, Any]) -> None:
    try:
        expected = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"expected candidate receipt is unreadable: {exc}") from exc
    if not isinstance(expected, dict) or not _parse_utc(expected.get("verifiedUtc")):
        raise ValueError("expected candidate receipt has an invalid verification timestamp")
    comparable_expected = copy.deepcopy(expected)
    comparable_live = copy.deepcopy(live)
    comparable_expected.pop("verifiedUtc", None)
    comparable_live.pop("verifiedUtc", None)
    if comparable_expected != comparable_live:
        raise ValueError("live candidate revalidation differs from the immutable baked-PCG receipt")


def audit(overrides: dict[str, bytes] | None = None) -> list[str]:
    overrides = overrides or {}
    errors: list[str] = []
    policy = _load_json(POLICY_PATH, overrides, errors)
    receipt = _load_json(RECEIPT_PATH, overrides, errors)
    if not isinstance(policy, dict) or not isinstance(receipt, dict):
        return errors

    expected_policy_flags = {
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
    for key, value in expected_policy_flags.items():
        if policy.get(key) != value:
            errors.append(f"policy.{key} must be {value!r}")
    if policy.get("target") != EXPECTED_TARGET:
        errors.append("policy.target differs from the locked three-hole Windows Shipping scope")

    authority = policy.get("boundedAuthority")
    if not isinstance(authority, dict):
        errors.append("policy.boundedAuthority must be an object")
    else:
        required_authority = {
            "domain": "COMPETITIVE_TREE_COLLISION_AND_AUTHORED_TREE_VISUAL_PLACEMENTS",
            "source": "ORDERED_EXPLICIT_TREES_ARRAYS_IN_AUTHORED_HOLE_JSON",
            "arrayOrderAuthoritative": True,
            "runtimeRandomPlacementAllowedForAuthorityDomain": False,
            "unrealPcgRuntimeGenerationAllowed": False,
            "authoredJsonRequiredInShipping": True,
            "sourceFallbackAcceptedAsShippingAuthority": False,
            "notClaimed": EXPECTED_NOT_CLAIMED,
        }
        for key, value in required_authority.items():
            if authority.get(key) != value:
                errors.append(f"policy.boundedAuthority.{key} differs from the bounded authority")

    canonical = policy.get("canonicalization")
    expected_canonical = {
        "hashAlgorithm": "SHA256",
        "encoding": "UTF-8",
        "jsonRules": "SORT_KEYS_ASCENDING_COMPACT_SEPARATORS_ENSURE_ASCII_FALSE_REJECT_NONFINITE",
        "perHolePayloadFields": ["courseId", "layoutId", "holeNumber", "trees"],
        "arrayOrderPreserved": True,
        "combinedPayload": "OBJECT_WITH_HOLES_ARRAY_OF_ORDERED_PER_HOLE_PAYLOADS",
    }
    if canonical != expected_canonical:
        errors.append("policy.canonicalization differs from the exact hash algorithm")

    calculated_holes: list[dict[str, Any]] = []
    calculated_records: list[dict[str, Any]] = []
    for expected in EXPECTED_HOLES:
        raw = b""
        try:
            raw = _read_bytes(expected["path"], overrides)
        except OSError as exc:
            errors.append(f"{expected['path']}: unreadable: {exc}")
            continue
        hole = _load_json(expected["path"], overrides, errors)
        payload = _validate_tree_array(hole, expected, errors)
        if payload is None:
            continue
        try:
            payload_bytes = _canonical_bytes(payload)
        except (TypeError, ValueError) as exc:
            errors.append(f"{expected['path']}: canonicalization failed: {exc}")
            continue
        record = {
            "path": expected["path"],
            "holeNumber": expected["holeNumber"],
            "treeCount": len(payload["trees"]),
            "sourceBytes": len(raw),
            "sourceSha256": _sha256(raw),
            "canonicalPayloadBytes": len(payload_bytes),
            "canonicalPayloadSha256": _sha256(payload_bytes),
        }
        if record != expected:
            errors.append(f"{expected['path']}: source or canonical identity differs from policy")
        calculated_holes.append(payload)
        calculated_records.append(record)

    combined_bytes = _canonical_bytes({"holes": calculated_holes})
    combined = {
        "treeCount": sum(len(value["trees"]) for value in calculated_holes),
        "bytes": len(combined_bytes),
        "sha256": _sha256(combined_bytes),
    }
    if combined != EXPECTED_COMBINED:
        errors.append("combined three-hole canonical tree identity differs")
    if policy.get("holes") != EXPECTED_HOLES:
        errors.append("policy.holes differs from the exact per-hole identities")
    if policy.get("combinedCanonicalPayload") != EXPECTED_COMBINED:
        errors.append("policy.combinedCanonicalPayload differs")

    manifest = _load_json("Data/PineRidgeCourse.json", overrides, errors)
    expected_manifest_holes = [
        (1, "Data/PineRidgeHole1.json"),
        (2, "Data/PineRidgeHole2.json"),
        (3, "Data/PineRidgeHole3.json"),
    ]
    if not isinstance(manifest, dict):
        pass
    else:
        manifest_holes = manifest.get("holes")
        actual_manifest_holes = []
        if isinstance(manifest_holes, list):
            actual_manifest_holes = [
                (entry.get("holeNumber"), entry.get("definitionFile"))
                for entry in manifest_holes
                if isinstance(entry, dict)
            ]
        if manifest.get("courseId") != EXPECTED_TARGET["courseId"] \
                or manifest.get("layoutId") != EXPECTED_TARGET["layoutId"] \
                or actual_manifest_holes != expected_manifest_holes:
            errors.append("Data/PineRidgeCourse.json does not select the exact three authority files")

    try:
        build_rules = _read_text("Source/DiscGolfTour/DiscGolfTour.Build.cs", overrides)
        for expected in EXPECTED_HOLES:
            token = f'../../{expected["path"]}'
            if build_rules.count(token) != 1:
                errors.append(f"Build.cs must stage {expected['path']} exactly once")
        if '"PCG"' in build_rules:
            errors.append("Shipping runtime module must not depend on PCG")
        editor_rules = _read_text(
            "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs", overrides
        )
        if editor_rules.count('"PCG"') != 1:
            errors.append("Editor module must own exactly one PCG dependency")
    except (OSError, UnicodeError) as exc:
        errors.append(f"Build.cs unreadable: {exc}")

    try:
        parser = _read_text("Source/DiscGolfTour/DiscGolfCourseDefinition.cpp", overrides)
        parser_markers = [
            'FPaths::ProjectDir(), Entry->DefinitionFile',
            'FFileHelper::LoadFileToString(Json, *Path)',
            'ParseJson(Json, OutDefinition, ParseError)',
            'TryGetArrayField(TEXT("trees"), Values)',
            'T.LocationCm = VectorFromJson(O, TEXT("locationCm"), FVector::ZeroVector)',
            'T.HeightScale = O->GetNumberField(TEXT("heightScale"))',
            'D.Trees.Add(T)',
            'OutSource = TEXT("AUTHORED JSON")',
            'OutSource = TEXT("SOURCE FALLBACK")',
        ]
        for marker in parser_markers:
            if marker not in parser:
                errors.append(f"course loader/parser proof marker missing: {marker}")
    except (OSError, UnicodeError) as exc:
        errors.append(f"course loader/parser unreadable: {exc}")

    try:
        bootstrap = _read_text("Source/DiscGolfTour/DevCourseBootstrap.cpp", overrides)
        bootstrap_markers = [
            "DiscGolfCourseDefinition::LoadPineRidgeHole(HoleNumber, Definition, Source, OutError)",
            "BuildAuthoredHole(Definition, Source, QualityProfile, OutError)",
            "for (int32 TreeIndex = 0; TreeIndex < Definition.Trees.Num(); ++TreeIndex)",
            "const FDiscGolfTreeDefinition& Tree = Definition.Trees[TreeIndex]",
            "Foliage->AddAuthoredTreeVisual(Tree.LocationCm, Tree.HeightScale, TreeIndex)",
            "Tree.LocationCm, Tree.HeightScale, Foliage != nullptr",
            "Definition.Trees.Num() + Definition.CollisionFixtures.Num()",
        ]
        for marker in bootstrap_markers:
            if marker not in bootstrap:
                errors.append(f"bootstrap tree-consumer proof marker missing: {marker}")
    except (OSError, UnicodeError) as exc:
        errors.append(f"bootstrap unreadable: {exc}")

    source_files = _source_files(overrides)
    controller_cpp_path = "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp"
    controller_h_path = "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.h"
    controller_cpp = source_files.get(controller_cpp_path, "")
    controller_h = source_files.get(controller_h_path, "")
    extracted = _extract_function(
        controller_cpp,
        r"void\s+ADiscGolfPcgAuthoringController::GenerateForest\s*\(\s*\)",
    )
    if not extracted:
        errors.append("the editor-only GenerateForest definition could not be isolated")
        stripped_controller = controller_cpp
    else:
        start, finish, body = extracted
        stripped_controller = controller_cpp[:start] + controller_cpp[finish:]
        if body.count("GenerateLocal(") != 1:
            errors.append("editor-only GenerateForest must contain exactly one isolated GenerateLocal call")
    if controller_h.count("void GenerateForest();") != 1:
        errors.append("GenerateForest declaration count differs from one editor-only callable API")

    external_generate_forest = 0
    external_generate_local = 0
    for relative, text in source_files.items():
        candidate = stripped_controller if relative == controller_cpp_path else text
        if relative == controller_h_path:
            candidate = candidate.replace("void GenerateForest();", "")
        external_generate_forest += len(re.findall(r"\bGenerateForest\s*\(", candidate))
        external_generate_local += len(re.findall(r"\bGenerateLocal\s*\(", candidate))
    if external_generate_forest != 0:
        errors.append(f"production source has {external_generate_forest} external GenerateForest call(s)")
    if external_generate_local != 0:
        errors.append(f"production source has {external_generate_local} external GenerateLocal call(s)")

    serialized_markers = _serialized_content_markers()
    if serialized_markers:
        errors.append(
            "serialized project content contains GenerateForest/GenerateLocal marker(s): "
            + ", ".join(serialized_markers)
        )

    consumer = policy.get("consumerProof")
    expected_consumer = {
        "courseManifest": "Data/PineRidgeCourse.json",
        "parser": "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp",
        "bootstrap": "Source/DiscGolfTour/DevCourseBootstrap.cpp",
        "runtimeDependencyRules": "Source/DiscGolfTour/DiscGolfTour.Build.cs",
        "requiredAuthoredSourceLabel": "AUTHORED JSON",
        "shippingRejectedSourceLabel": "SOURCE FALLBACK",
        "expectedExternalGenerateForestCallCount": 0,
        "expectedExternalGenerateLocalCallCount": 0,
        "runtimePcgModuleDependencyAllowed": False,
        "authoringGeneratorOwner": controller_cpp_path,
        "authoringOnlyPcgModule": "DiscGolfTourEditor",
        "serializedContentCallMarkerCount": 0,
    }
    if consumer != expected_consumer:
        errors.append("policy.consumerProof differs from the exact source-consumer boundary")
    if policy.get("evidence") != {
        "receipt": RECEIPT_PATH,
        "validator": "Scripts/validate_dg_session19_baked_pcg.py",
        "selfTestMinimumMutationCount": 32,
    }:
        errors.append("policy.evidence differs from the validator and mutation-test boundary")
    if policy.get("candidateEvidence") != EXPECTED_CANDIDATE_EVIDENCE:
        errors.append("policy.candidateEvidence differs from the candidate-bound NonUFS boundary")
    if policy.get("shippingEvidencePending") != EXPECTED_PENDING:
        errors.append("policy.shippingEvidencePending differs from the required Shipping matrix")

    expected_receipt_flags = {
        "schema": "DiscGolfTour.Session19BakedPcgSourceAuthorityReceipt.v2",
        "schemaVersion": 2,
        "session": 19,
        "state": "SOURCE_AUTHORITY_AND_SHIPPING_PCG_SOURCE_SEPARATION_VERIFIED_COOK_PROOF_PENDING",
        "policy": POLICY_PATH,
        "sourceAuthorityAccepted": True,
        "shippingPackageEvidenceAccepted": False,
        "releaseReady": False,
        "blockerClosed": False,
    }
    for key, value in expected_receipt_flags.items():
        if receipt.get(key) != value:
            errors.append(f"receipt.{key} must be {value!r}")
    if receipt.get("holes") != EXPECTED_HOLES:
        errors.append("receipt.holes differs from the exact per-hole identities")
    if receipt.get("combinedCanonicalPayload") != EXPECTED_COMBINED:
        errors.append("receipt.combinedCanonicalPayload differs")
    expected_source_proof = {
        "manifestSelectsAllThreeHoleFiles": True,
        "buildRulesStageAllThreeHoleFiles": True,
        "parserPreservesExplicitTreeArrayOrder": True,
        "bootstrapConsumesDefinitionTrees": True,
        "bootstrapUsesTreeLocationAndHeightScale": True,
        "authoredCollisionTreeCount": 44,
        "externalGenerateForestCallCount": 0,
        "externalGenerateLocalCallCount": 0,
        "runtimePcgModuleDependencyCount": 0,
        "runtimePcgGenerationApiCount": 0,
        "editorOnlyGenerateForestDefinitionCount": 1,
        "serializedContentCallMarkerCount": 0,
    }
    if receipt.get("sourceConsumerProof") != expected_source_proof:
        errors.append("receipt.sourceConsumerProof differs from the verified source facts")
    if receipt.get("notAcceptedByThisReceipt") != EXPECTED_NOT_CLAIMED:
        errors.append("receipt.notAcceptedByThisReceipt overstates the bounded proof")

    release_scope = _load_json("Config/DG_Session19ReleaseScopeContract.json", overrides, errors)
    if isinstance(release_scope, dict):
        feature_scope = release_scope.get("featureScope", {})
        if feature_scope.get("pcgShippingAuthority") != (
                "DETERMINISTIC_AUTHORED_OUTPUT_CANDIDATE_SELECTION_AND_NO_PROJECT_"
                "GENERATION_INVOCATION_PASS_COOKED_RUNTIME_CONSUMER_UNREAL_BAKE_"
                "REOPEN_AND_PERFORMANCE_PENDING") \
                or feature_scope.get("runtimePcgGenerationAllowed") is not False:
            errors.append("Session 19 parent scope no longer requires baked PCG with cooked evidence")
        if "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING" \
                not in release_scope.get("remainingReleaseBlockers", []):
            errors.append("Session 19 parent scope no longer preserves the Session 13 blocker")

    shipping_policy = _load_json("Config/DG_Session19ShippingContentPolicy.json", overrides, errors)
    if isinstance(shipping_policy, dict):
        pcg = shipping_policy.get("featureExclusions", {}).get("pcg", {})
        if pcg != {
            "runtimeGenerationAllowed": False,
            "shippingAuthority": "DETERMINISTIC_BAKED_OUTPUT_ONLY",
        }:
            errors.append("Session 19 Shipping content policy differs from baked-only authority")

    for relative, (expected_bytes, expected_sha) in FROZEN_SESSION13.items():
        try:
            data = _read_bytes(relative, overrides)
        except OSError as exc:
            errors.append(f"frozen Session 13 file unreadable: {relative}: {exc}")
            continue
        if len(data) != expected_bytes or _sha256(data) != expected_sha:
            errors.append(f"frozen Session 13 evidence changed: {relative}")

    return errors


def _json_override(relative: str, mutate: Callable[[Any], None]) -> dict[str, bytes]:
    value = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    mutate(value)
    return {relative: json.dumps(value, indent=2, ensure_ascii=False).encode("utf-8") + b"\n"}


def self_test() -> tuple[int, list[str]]:
    baseline = audit()
    if baseline:
        return 0, ["baseline invalid before mutation self-test", *baseline]

    mutations: list[tuple[str, Callable[[], dict[str, bytes]]]] = [
        ("policy state", lambda: _json_override(POLICY_PATH, lambda v: v.__setitem__("state", "PASS"))),
        ("source evidence", lambda: _json_override(POLICY_PATH, lambda v: v.__setitem__("sourceEvidenceComplete", False))),
        ("shipping evidence", lambda: _json_override(POLICY_PATH, lambda v: v.__setitem__("shippingEvidenceComplete", True))),
        ("release ready", lambda: _json_override(POLICY_PATH, lambda v: v.__setitem__("releaseReady", True))),
        ("blocker closure", lambda: _json_override(POLICY_PATH, lambda v: v.__setitem__("closesSession13Blocker", True))),
        ("hole 1 remove tree", lambda: _json_override(EXPECTED_HOLES[0]["path"], lambda v: v["trees"].pop())),
        ("hole 2 reorder", lambda: _json_override(EXPECTED_HOLES[1]["path"], lambda v: v["trees"].__setitem__(slice(0, 2), list(reversed(v["trees"][:2]))))),
        ("hole 3 scale", lambda: _json_override(EXPECTED_HOLES[2]["path"], lambda v: v["trees"][0].__setitem__("heightScale", 1.03))),
        ("non-finite substitute", lambda: _json_override(EXPECTED_HOLES[0]["path"], lambda v: v["trees"][0].__setitem__("heightScale", None))),
        ("missing axis", lambda: _json_override(EXPECTED_HOLES[1]["path"], lambda v: v["trees"][0]["locationCm"].pop("z"))),
        ("extra tree field", lambda: _json_override(EXPECTED_HOLES[2]["path"], lambda v: v["trees"][0].__setitem__("seed", 1))),
        ("hole identity", lambda: _json_override(EXPECTED_HOLES[0]["path"], lambda v: v.__setitem__("courseId", "Other"))),
        ("policy hole hash", lambda: _json_override(POLICY_PATH, lambda v: v["holes"][0].__setitem__("canonicalPayloadSha256", "0" * 64))),
        ("policy combined hash", lambda: _json_override(POLICY_PATH, lambda v: v["combinedCanonicalPayload"].__setitem__("sha256", "0" * 64))),
        ("receipt hole hash", lambda: _json_override(RECEIPT_PATH, lambda v: v["holes"][1].__setitem__("sourceSha256", "0" * 64))),
        ("receipt blocker", lambda: _json_override(RECEIPT_PATH, lambda v: v.__setitem__("blockerClosed", True))),
        ("manifest route", lambda: _json_override("Data/PineRidgeCourse.json", lambda v: v["holes"][1].__setitem__("definitionFile", "Data/Other.json"))),
        ("build dependency", lambda: {"Source/DiscGolfTour/DiscGolfTour.Build.cs": (ROOT / "Source/DiscGolfTour/DiscGolfTour.Build.cs").read_bytes().replace(b"../../Data/PineRidgeHole2.json", b"../../Data/MissingHole2.json")}),
        ("parser tree append", lambda: {"Source/DiscGolfTour/DiscGolfCourseDefinition.cpp": (ROOT / "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp").read_bytes().replace(b"D.Trees.Add(T)", b"/* mutation removed tree append */")}),
        ("bootstrap consumption", lambda: {"Source/DiscGolfTour/DevCourseBootstrap.cpp": (ROOT / "Source/DiscGolfTour/DevCourseBootstrap.cpp").read_bytes().replace(b"Tree.LocationCm, Tree.HeightScale, Foliage != nullptr", b"FVector::ZeroVector, 1.0f, false")}),
        ("external GenerateForest call", lambda: {"Source/DiscGolfTour/DiscGolfTourGameMode.cpp": (ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp").read_bytes() + b"\nvoid DGMutation(){ GenerateForest(); }\n"}),
        ("external GenerateLocal call", lambda: {"Source/DiscGolfTour/DiscGolfTourGameMode.cpp": (ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp").read_bytes() + b"\nvoid DGMutation2(){ GenerateLocal(); }\n"}),
        ("fallback approval", lambda: _json_override(POLICY_PATH, lambda v: v["boundedAuthority"].__setitem__("sourceFallbackAcceptedAsShippingAuthority", True))),
        ("overclaim removal", lambda: _json_override(RECEIPT_PATH, lambda v: v["notAcceptedByThisReceipt"].pop())),
        ("historical mutation", lambda: {"Config/DG_Session13CourseAuthoringPcgContract.json": (ROOT / "Config/DG_Session13CourseAuthoringPcgContract.json").read_bytes() + b" "}),
        ("candidate archive root", lambda: _json_override(POLICY_PATH, lambda v: v["candidateEvidence"].__setitem__("archiveDataRoot", "Data"))),
        ("candidate staging class", lambda: _json_override(POLICY_PATH, lambda v: v["candidateEvidence"].__setitem__("stagingClass", "UFS"))),
        ("candidate IoStore claim", lambda: _json_override(POLICY_PATH, lambda v: v["candidateEvidence"].__setitem__("ufsOrIoStoreCopyRequired", True))),
        ("candidate closure retrofit", lambda: _json_override(POLICY_PATH, lambda v: v["candidateEvidence"].__setitem__("sourceClosureRetrofitAllowed", True))),
        ("candidate pending proof", lambda: _json_override(POLICY_PATH, lambda v: v["candidateEvidence"]["stillPending"].pop())),
        ("mutation minimum", lambda: _json_override(POLICY_PATH, lambda v: v["evidence"].__setitem__("selfTestMinimumMutationCount", 20))),
    ]

    failures: list[str] = []
    caught = 0
    for name, build_override in mutations:
        errors = audit(build_override())
        if errors:
            caught += 1
        else:
            failures.append(f"mutation escaped detection: {name}")
    candidate_caught, candidate_failures = _candidate_self_test()
    return caught + candidate_caught, failures + candidate_failures


def _candidate_self_test() -> tuple[int, list[str]]:
    candidate_id = "S19_WindowsShipping_SELFTEST"
    failures: list[str] = []
    caught = 0
    with tempfile.TemporaryDirectory(prefix="dg_baked_pcg_") as temporary:
        root = Path(temporary)
        fixture = root / "Windows"
        data_root = fixture / "DiscGolfTour" / "Data"
        data_root.mkdir(parents=True)
        identities: list[str] = []
        for expected in EXPECTED_HOLES:
            identity = f"DiscGolfTour/Data/{Path(expected['path']).name}"
            identities.append(identity)
            shutil.copyfile(ROOT / expected["path"], fixture / identity)
        (fixture / "DiscGolfTour.exe").write_bytes(b"fixture executable")
        inner_executable = fixture / "DiscGolfTour" / "Binaries" / "Win64" / "DiscGolfTour-Win64-Shipping.exe"
        inner_executable.parent.mkdir(parents=True)
        inner_executable.write_bytes(b"fixture inner Shipping executable")
        (fixture / NONUFS_MANIFEST_PATH).write_text(
            "".join(f"{identity}\t2000-01-01T00:00:00.000Z\n" for identity in identities),
            encoding="utf-8",
        )
        (fixture / UFS_MANIFEST_PATH).write_text(
            "DiscGolfTour/Content/Paks/fixture.pak\t2000-01-01T00:00:00.000Z\n",
            encoding="utf-8",
        )
        closure_path = root / "closure.json"
        closure_path.write_bytes(
            json.dumps(_source_closure_receipt(candidate_id), indent=2).encode("utf-8") + b"\n"
        )
        try:
            baseline = _candidate_receipt(candidate_id, fixture, closure_path)
            expected_path = root / "candidate.json"
            expected_path.write_bytes(
                json.dumps(baseline, indent=2).encode("utf-8") + b"\n"
            )
            _validate_expected_candidate_receipt(expected_path, baseline)
        except (OSError, ValueError) as exc:
            return 0, [f"candidate fixture invalid before mutation self-test: {exc}"]

        def expect_failure(name: str, operation: Callable[[], None]) -> None:
            nonlocal caught
            try:
                operation()
            except (OSError, ValueError, json.JSONDecodeError):
                caught += 1
            else:
                failures.append(f"candidate mutation escaped detection: {name}")

        def with_archive_mutation(name: str, mutate: Callable[[Path], None]) -> None:
            mutated = root / f"mutated_{caught}_{len(failures)}"
            shutil.copytree(fixture, mutated)
            mutate(mutated)
            expect_failure(name, lambda: _candidate_receipt(candidate_id, mutated, closure_path))

        with_archive_mutation(
            "authored data bytes",
            lambda value: (value / identities[0]).write_bytes((value / identities[0]).read_bytes() + b" "),
        )
        with_archive_mutation(
            "missing authored data",
            lambda value: (value / identities[1]).unlink(),
        )
        with_archive_mutation(
            "NonUFS route",
            lambda value: (value / NONUFS_MANIFEST_PATH).write_text(
                (value / NONUFS_MANIFEST_PATH).read_text(encoding="utf-8").replace(
                    identities[1], "Data/PineRidgeHole2.json"
                ),
                encoding="utf-8",
            ),
        )
        with_archive_mutation(
            "duplicate NonUFS identity",
            lambda value: (value / NONUFS_MANIFEST_PATH).write_text(
                (value / NONUFS_MANIFEST_PATH).read_text(encoding="utf-8")
                + f"{identities[0]}\t2000-01-01T00:00:00.000Z\n",
                encoding="utf-8",
            ),
        )
        with_archive_mutation(
            "UFS duplicate authored data",
            lambda value: (value / UFS_MANIFEST_PATH).write_text(
                f"{identities[2]}\t2000-01-01T00:00:00.000Z\n",
                encoding="utf-8",
            ),
        )

        closure_value = json.loads(closure_path.read_text(encoding="utf-8"))
        for name, mutate in (
            ("closure candidate ID", lambda value: value.__setitem__("candidateId", "S19_WindowsShipping_OTHER")),
            ("closure timestamp", lambda value: value.__setitem__("verifiedUtc", "now")),
            ("closure source hash", lambda value: value["closure"].__setitem__("sha256", "0" * 64)),
        ):
            candidate = copy.deepcopy(closure_value)
            mutate(candidate)
            path = root / f"{name.replace(' ', '_')}.json"
            path.write_bytes(json.dumps(candidate, indent=2).encode("utf-8") + b"\n")
            expect_failure(name, lambda path=path: _candidate_receipt(candidate_id, fixture, path))

        for name, mutate in (
            ("receipt state", lambda value: value.__setitem__("state", "PASS")),
            ("receipt archive hash", lambda value: value["archive"].__setitem__("canonicalManifestSha256", "0" * 64)),
            ("receipt runtime overclaim", lambda value: value["technicalBoundary"].__setitem__("runtimeAuthoredSelectionProven", True)),
            ("receipt blocker closure", lambda value: value.__setitem__("blockerClosed", True)),
        ):
            candidate = copy.deepcopy(baseline)
            mutate(candidate)
            path = root / f"expected_{name.replace(' ', '_')}.json"
            path.write_bytes(json.dumps(candidate, indent=2).encode("utf-8") + b"\n")
            expect_failure(name, lambda path=path: _validate_expected_candidate_receipt(path, baseline))
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-receipt", type=Path)
    parser.add_argument("--source-closure-input", type=Path)
    parser.add_argument("--source-closure-output", type=Path)
    args = parser.parse_args()

    if args.self_test:
        if any((
            args.release_required,
            args.candidate_id,
            args.archive,
            args.output,
            args.expected_receipt,
            args.source_closure_input,
            args.source_closure_output,
        )):
            parser.error("--self-test does not accept validation or output arguments")
        caught, failures = self_test()
        if failures or caught < 32:
            print(f"Session 19 baked-PCG self-test: FAIL ({caught} mutations caught)")
            for failure in failures:
                print(f"- {failure}")
            return 1
        print(f"Session 19 baked-PCG self-test: PASS ({caught} mutations caught)")
        return 0

    candidate_options = any((
        args.candidate_id,
        args.archive,
        args.output,
        args.expected_receipt,
        args.source_closure_input,
        args.source_closure_output,
    ))
    if candidate_options:
        if not args.candidate_id or not CANDIDATE_ID_RE.fullmatch(args.candidate_id):
            parser.error("candidate mode requires a canonical --candidate-id")
        if args.source_closure_output is not None:
            if any((args.archive, args.output, args.expected_receipt, args.source_closure_input)):
                parser.error("--source-closure-output is an exclusive pre-build mode")
            if args.release_required:
                parser.error("pre-build source closure is not release-required validation")
            expected_path = ROOT / SOURCE_CLOSURE_PATTERN.format(candidateId=args.candidate_id)
            if args.source_closure_output.resolve() != expected_path.resolve():
                parser.error("source-closure output must use the candidate-bound Evidence/Session19 path")
            errors = audit()
            if errors:
                print("Session 19 baked-PCG source closure: FAIL")
                for error in errors:
                    print(f"- {error}")
                return 1
            try:
                _write_immutable_json(
                    args.source_closure_output,
                    _source_closure_receipt(args.candidate_id),
                )
            except (OSError, ValueError) as exc:
                print(f"Session 19 baked-PCG source closure: FAIL\n- {exc}")
                return 1
            print("Session 19 baked-PCG source closure: PASS_PREBUILD_BAKED_PCG_SOURCE_CLOSURE")
            return 0

        if args.archive is None:
            parser.error("candidate validation requires --archive")
        if args.output is not None and args.expected_receipt is not None:
            parser.error("--output and --expected-receipt are mutually exclusive")
        expected_path = ROOT / CANDIDATE_RECEIPT_PATTERN.format(candidateId=args.candidate_id)
        selected_receipt = args.output or args.expected_receipt
        if selected_receipt is not None and selected_receipt.resolve() != expected_path.resolve():
            parser.error("candidate receipt must use the candidate-bound Evidence/Session19 path")
        if args.archive.name.casefold() != "windows":
            parser.error("--archive must identify the final Windows archive root")
        archive_resolved = args.archive.resolve(strict=True)
        if archive_resolved.parent.name != args.candidate_id:
            parser.error("--archive must be the Windows root under the exact candidate ID")
        if args.source_closure_input is not None:
            expected_closure_path = ROOT / SOURCE_CLOSURE_PATTERN.format(
                candidateId=args.candidate_id
            )
            if args.source_closure_input.resolve() != expected_closure_path.resolve():
                parser.error("source-closure input must use the candidate-bound Evidence/Session19 path")
        try:
            receipt = _candidate_receipt(
                args.candidate_id,
                archive_resolved,
                args.source_closure_input,
            )
            if args.expected_receipt is not None:
                _validate_expected_candidate_receipt(args.expected_receipt, receipt)
            if args.output is not None:
                _write_immutable_json(args.output, receipt)
        except (OSError, ValueError) as exc:
            print(f"Session 19 baked-PCG candidate evidence: FAIL\n- {exc}")
            return 1
        print(f"Session 19 baked-PCG candidate evidence: {CANDIDATE_STATUS}")
        if args.release_required:
            return 2
        return 0

    errors = audit()
    if errors:
        print("Session 19 baked-PCG source authority: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    if args.release_required:
        print(f"Session 19 baked-PCG source authority: {RELEASE_STATUS}")
        return 2

    print(f"Session 19 baked-PCG source authority: {NORMAL_STATUS}")
    print("Per-hole canonical tree hashes:")
    for hole in EXPECTED_HOLES:
        print(
            f"- Hole {hole['holeNumber']}: {hole['treeCount']} trees, "
            f"{hole['canonicalPayloadSha256']}"
        )
    print(f"- Combined: {EXPECTED_COMBINED['treeCount']} trees, {EXPECTED_COMBINED['sha256']}")
    print("Candidate runtime-selection and baked-output acceptance evidence remains pending.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
