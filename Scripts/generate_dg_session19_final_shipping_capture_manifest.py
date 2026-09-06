#!/usr/bin/env python3
"""Generate or validate the Session 19 final Shipping capture manifest.

The manifest is an objective, external-file inventory.  It deliberately does not
authenticate where pixels came from, perform visual/legal/accessibility review, or
approve a release.  Generation and validation both re-read the exact Shipping
archive, verification receipt, capture tool, policy, and 36 PNG files.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import re
import stat
import struct
import sys
import tempfile
import uuid
import zlib
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ManualReleaseReviewPolicy.json"
EVIDENCE_DIR = ROOT / "Evidence/Session19"

SCHEMA = "DiscGolfTour.Session19FinalCaptureSetManifest.v1"
RECEIPT_SCHEMA = "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
RECEIPT_STATE = "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
STATE = "PASS_BOUND_EXTERNAL_PNG_INVENTORY_HUMAN_REVIEWS_PENDING"
AUTHORITY = (
    "TECHNICAL_EXTERNAL_PNG_INVENTORY_NOT_ART_ACCESSIBILITY_LEGAL_PRODUCT_"
    "OWNER_OR_RELEASE_APPROVAL"
)
ARTIFACT_ID = "BOUND_FINAL_CAPTURE_MANIFEST"
INNER_EXE = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
REQUIRED_ARCHIVE_FILES = {
    "DiscGolfTour.exe",
    INNER_EXE,
    "Manifest_UFSFiles_Win64.txt",
    "Manifest_NonUFSFiles_Win64.txt",
    "NOTICES.txt",
}
EXPECTED_VIEWS = [
    "front_end_title",
    "settings_accessibility",
    "hole1_intro_route_tee",
    "hole1_flight_lie_score",
    "hole2_intro_route_tee",
    "hole2_flight_lie_score",
    "hole3_intro_route_tee",
    "hole3_flight_lie_score",
    "scorecard",
    "round_complete",
    "character_setup",
    "character_drive_release_followthrough",
    "character_approach",
    "character_putt",
    "environment_fairway_close",
    "environment_forest_route_gap",
    "environment_gallery_lake",
    "all_logo_signage_sponsor_and_brand_surfaces",
]
EXPECTED_RESOLUTIONS = ["1280x720", "1920x1080"]
EXPECTED_DIMENSIONS = {"1280x720": (1280, 720), "1920x1080": (1920, 1080)}
CAPTURE_METHODS = {
    "WINDOWS_GRAPHICS_CAPTURE",
    "OS_LEVEL_SCREENSHOT",
    "EXTERNAL_CAPTURE_DEVICE",
}
APPROVAL_FIELDS = {
    "humanVisualReviewPerformed": False,
    "artApproved": False,
    "accessibilityApproved": False,
    "logoOrTradeDressApproved": False,
    "legalApproved": False,
    "productOwnerApproved": False,
    "publicReleaseApproved": False,
    "releaseReady": False,
}

CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
FILE_ATTRIBUTE_REPARSE_POINT = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)


class EvidenceError(RuntimeError):
    """A fail-closed evidence-contract failure."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n").encode("utf-8")


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise EvidenceError(f"JSON contains duplicate key {key!r}")
        result[key] = value
    return result


def is_reparse(path: Path) -> bool:
    try:
        info = path.lstat()
    except OSError as exc:
        raise EvidenceError(f"cannot stat path: {path}") from exc
    return path.is_symlink() or bool(
        getattr(info, "st_file_attributes", 0) & FILE_ATTRIBUTE_REPARSE_POINT
    )


def reject_reparse_chain(path: Path, label: str) -> None:
    """Reject a lexical path whose existing root/ancestor is a redirector."""
    absolute = path.absolute()
    parts = absolute.parts
    if not parts:
        raise EvidenceError(f"{label} is empty")
    cursor = Path(parts[0])
    if cursor.exists() and is_reparse(cursor):
        raise EvidenceError(f"{label} crosses a symlink or reparse point")
    for part in parts[1:]:
        cursor = cursor / part
        if not cursor.exists():
            break
        if is_reparse(cursor):
            raise EvidenceError(f"{label} crosses a symlink or reparse point")


def stable_read(path: Path, label: str, maximum_bytes: int | None = None) -> bytes:
    if is_reparse(path):
        raise EvidenceError(f"{label} is a symlink or reparse point")
    try:
        before = path.stat()
    except OSError as exc:
        raise EvidenceError(f"{label} is missing or unreadable") from exc
    if not stat.S_ISREG(before.st_mode):
        raise EvidenceError(f"{label} is not a regular file")
    if maximum_bytes is not None and before.st_size > maximum_bytes:
        raise EvidenceError(f"{label} exceeds the maximum accepted size")
    try:
        data = path.read_bytes()
        after = path.stat()
    except OSError as exc:
        raise EvidenceError(f"{label} changed or became unreadable") from exc
    identity_before = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    identity_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
    if identity_before != identity_after or len(data) != after.st_size:
        raise EvidenceError(f"{label} changed while it was being read")
    return data


def hash_stable_file(path: Path, label: str) -> tuple[int, str, tuple[int, int]]:
    if is_reparse(path):
        raise EvidenceError(f"{label} is a symlink or reparse point")
    try:
        before = path.stat()
    except OSError as exc:
        raise EvidenceError(f"{label} is missing or unreadable") from exc
    if not stat.S_ISREG(before.st_mode):
        raise EvidenceError(f"{label} is not a regular file")
    digest = hashlib.sha256()
    size = 0
    try:
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                size += len(chunk)
                digest.update(chunk)
        after = path.stat()
    except OSError as exc:
        raise EvidenceError(f"{label} changed or became unreadable") from exc
    identity_before = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    identity_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
    if identity_before != identity_after or size != after.st_size:
        raise EvidenceError(f"{label} changed while it was being hashed")
    return size, digest.hexdigest().upper(), (after.st_dev, after.st_ino)


def load_json(path: Path, label: str) -> tuple[Any, bytes]:
    data = stable_read(path, label, 64 * 1024 * 1024)
    try:
        value = json.loads(data.decode("utf-8-sig"), object_pairs_hook=reject_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"{label} is not strict UTF-8 JSON: {exc}") from exc
    return value, data


def is_within(child: Path, parent: Path) -> bool:
    try:
        common = os.path.commonpath(
            [os.path.normcase(str(child.resolve())), os.path.normcase(str(parent.resolve()))]
        )
    except (OSError, ValueError):
        return False
    return common == os.path.normcase(str(parent.resolve()))


def require_disjoint(first: Path, second: Path, label: str) -> None:
    if is_within(first, second) or is_within(second, first):
        raise EvidenceError(f"{label} must be disjoint")


def canonical_relative(value: str) -> str:
    if type(value) is not str or not value or "\\" in value or "\x00" in value or ":" in value:
        raise EvidenceError("path is not a canonical relative POSIX path")
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value or any(part in ("", ".", "..") for part in pure.parts):
        raise EvidenceError("path is not a canonical relative POSIX path")
    return value


def safe_project_file(project_root: Path, path: Path, label: str) -> tuple[Path, str]:
    reject_reparse_chain(project_root, "project root")
    lexical_root = project_root.absolute()
    lexical_path = path.absolute()
    try:
        lexical_relative = lexical_path.relative_to(lexical_root)
    except ValueError as exc:
        raise EvidenceError(f"{label} must be lexically inside the project root") from exc
    cursor = lexical_root
    for part in lexical_relative.parts:
        cursor = cursor / part
        if is_reparse(cursor):
            raise EvidenceError(f"{label} crosses a symlink or reparse point")
    try:
        root = project_root.resolve(strict=True)
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise EvidenceError(f"{label} does not resolve") from exc
    if not is_within(resolved, root):
        raise EvidenceError(f"{label} must be inside the project root")
    relative = resolved.relative_to(root).as_posix()
    canonical_relative(relative)
    if not resolved.is_file():
        raise EvidenceError(f"{label} must be a regular file")
    return resolved, relative


def validate_uuid4(value: str) -> str:
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError) as exc:
        raise EvidenceError("capture session ID must be a canonical lowercase UUIDv4") from exc
    if parsed.version != 4 or str(parsed) != value:
        raise EvidenceError("capture session ID must be a canonical lowercase UUIDv4")
    return value


def validate_policy(policy_path: Path, project_root: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    resolved, relative = safe_project_file(project_root, policy_path, "manual release-review policy")
    policy, data = load_json(resolved, "manual release-review policy")
    if type(policy) is not dict:
        raise EvidenceError("manual release-review policy must be an object")
    capture = policy.get("finalCaptureContract", {})
    gates = policy.get("reviewGates", [])
    by_id = {
        row.get("gateId"): row for row in gates
        if type(row) is dict and type(row.get("gateId")) is str
    }
    if (
        policy.get("schema") != "DiscGolfTour.Session19ManualReleaseReviewPolicy.v1"
        or policy.get("schemaVersion") != 1
        or policy.get("session") != 19
        or policy.get("releaseReady") is not False
        or policy.get("publicReleaseApproved") is not False
        or policy.get("blanketAuthorizationIsApprovalEvidence") is not False
        or capture.get("acceptedCaptureManifestPath") is not None
        or capture.get("mustBeFromBoundShippingCandidate") is not True
        or capture.get("savedOrDevelopmentCapturesAreFinalEvidence") is not False
        or capture.get("requiredViews") != EXPECTED_VIEWS
        or capture.get("requiredResolutions") != EXPECTED_RESOLUTIONS
        or ARTIFACT_ID not in by_id.get("LOGO_AND_TRADE_DRESS_REVIEW", {}).get("requiredArtifacts", [])
        or ARTIFACT_ID not in by_id.get("FINAL_VISUAL_PRODUCT_APPROVAL", {}).get("requiredArtifacts", [])
        or by_id.get("LOGO_AND_TRADE_DRESS_REVIEW", {}).get("closed") is not False
        or by_id.get("FINAL_VISUAL_PRODUCT_APPROVAL", {}).get("closed") is not False
    ):
        raise EvidenceError("manual policy no longer matches the fail-closed final capture contract")
    return policy, {
        "path": relative,
        "bytes": len(data),
        "sha256": sha256_bytes(data),
    }


def collect_archive(archive: Path, candidate_id: str, project_root: Path) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    if not archive.is_absolute():
        raise EvidenceError("--archive must be absolute")
    if archive.name != "Windows" or archive.parent.name != candidate_id:
        raise EvidenceError("archive root is not the requested candidate's Windows leaf")
    try:
        root = archive.resolve(strict=True)
    except OSError as exc:
        raise EvidenceError("archive root does not resolve") from exc
    reject_reparse_chain(archive, "archive root")
    if not root.is_dir() or is_reparse(archive) or is_reparse(archive.parent):
        raise EvidenceError("archive root must be a regular non-reparse directory")
    require_disjoint(root, project_root, "archive and project roots")
    seen: set[str] = set()
    records: list[dict[str, Any]] = []
    for current, directories, files in os.walk(root, topdown=True, followlinks=False):
        current_path = Path(current)
        directories.sort(key=str.casefold)
        files.sort(key=str.casefold)
        for name in directories:
            if is_reparse(current_path / name):
                raise EvidenceError("archive contains a reparse directory")
        for name in files:
            child = current_path / name
            if is_reparse(child):
                raise EvidenceError("archive contains a reparse file")
            relative = canonical_relative(child.relative_to(root).as_posix())
            folded = relative.casefold()
            if folded in seen:
                raise EvidenceError("archive contains a case-insensitive path collision")
            seen.add(folded)
            size, digest, _ = hash_stable_file(child, f"archive file {relative}")
            records.append({"path": relative, "bytes": size, "sha256": digest})
    records.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    by_path = {item["path"]: item for item in records}
    missing = sorted(REQUIRED_ARCHIVE_FILES - set(by_path))
    if missing:
        raise EvidenceError(f"archive lacks required Shipping files: {missing}")
    material = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return {
        "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
        "hostPathRecorded": False,
        "fileCount": len(records),
        "bytes": sum(item["bytes"] for item in records),
        "canonicalManifestSha256": sha256_bytes(material),
    }, by_path


def validate_receipt(
    receipt_path: Path,
    candidate_id: str,
    archive_summary: dict[str, Any],
    archive_by_path: dict[str, dict[str, Any]],
    project_root: Path,
) -> dict[str, Any]:
    resolved, relative = safe_project_file(project_root, receipt_path, "Shipping verification receipt")
    expected = f"Evidence/Session19/ShippingCandidateVerification-{candidate_id}.json"
    if relative != expected:
        raise EvidenceError("Shipping verification receipt path does not match candidate ID")
    receipt, data = load_json(resolved, "Shipping verification receipt")
    if type(receipt) is not dict:
        raise EvidenceError("Shipping verification receipt must be an object")
    if (
        receipt.get("schema") != RECEIPT_SCHEMA
        or receipt.get("schemaVersion") != 2
        or receipt.get("session") != 19
        or receipt.get("runId") != candidate_id
        or receipt.get("state") != RECEIPT_STATE
        or receipt.get("verificationMode") != "DIRECT_BUILD_PASS"
        or receipt.get("receiptPath") != relative
        or receipt.get("archiveContract", {}).get("allRequiredFilesPresent") is not True
        or receipt.get("archiveContract", {}).get("hostPathRecorded") is not False
        or receipt.get("archiveContract", {}).get("recoveryLocationToken")
        != archive_summary["recoveryLocationToken"]
        or receipt.get("releaseExcludedProjectRootsAbsent") is not True
        or receipt.get("shippingPluginCapabilities", {}).get("technicalGatePass") is not True
        or receipt.get("shippingPluginCapabilities", {}).get("finalArchiveCanonicalManifestSha256")
        != archive_summary["canonicalManifestSha256"]
    ):
        raise EvidenceError("Shipping verification receipt identity or live archive binding differs")
    inner = receipt.get("innerShippingExecutable", {})
    live_exe = archive_by_path[INNER_EXE]
    if (
        inner.get("relativePath") != INNER_EXE
        or inner.get("bytes") != live_exe["bytes"]
        or inner.get("sha256") != live_exe["sha256"]
    ):
        raise EvidenceError("Shipping verification receipt executable binding differs from live archive")
    return {
        "path": relative,
        "bytes": len(data),
        "sha256": sha256_bytes(data),
        "schema": RECEIPT_SCHEMA,
        "state": RECEIPT_STATE,
    }


def file_identity(path: Path, label: str, path_token: str | None = None) -> dict[str, Any]:
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise EvidenceError(f"{label} does not resolve") from exc
    size, digest, _ = hash_stable_file(resolved, label)
    result = {
        "name": resolved.name,
        "bytes": size,
        "sha256": digest,
        "hostPathRecorded": False,
    }
    if path_token is not None:
        result["pathToken"] = path_token
    return result


def tool_identities(capture_tool: Path, project_root: Path) -> dict[str, Any]:
    if not capture_tool.is_absolute():
        raise EvidenceError("--capture-tool must be absolute")
    live_project_root = project_root.resolve(strict=True)
    if live_project_root == ROOT.resolve(strict=True):
        generator = Path(__file__).resolve(strict=True)
    else:
        # Self-test fixtures use an isolated project tree containing an exact copy
        # of this tool. Production never takes a project-root override.
        generator = (
            live_project_root
            / "Scripts/generate_dg_session19_final_shipping_capture_manifest.py"
        ).resolve(strict=True)
    try:
        generator_token = generator.relative_to(live_project_root).as_posix()
    except (OSError, ValueError) as exc:
        raise EvidenceError("manifest generator/validator is not inside the project root") from exc
    if generator_token != "Scripts/generate_dg_session19_final_shipping_capture_manifest.py":
        raise EvidenceError("manifest generator/validator path is unexpected")
    return {
        "captureTool": file_identity(capture_tool, "external capture tool"),
        "manifestGeneratorValidator": file_identity(generator, "manifest generator/validator", generator_token),
        "python": file_identity(Path(sys.executable), "Python interpreter"),
    }


def parse_png(data: bytes, label: str) -> tuple[int, int]:
    if len(data) < 57 or not data.startswith(PNG_SIGNATURE):
        raise EvidenceError(f"{label} is not a complete PNG")
    offset = len(PNG_SIGNATURE)
    chunks: list[tuple[bytes, bytes]] = []
    while offset < len(data):
        if offset + 12 > len(data):
            raise EvidenceError(f"{label} has a truncated PNG chunk")
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        chunk_type = data[offset + 4:offset + 8]
        end = offset + 12 + length
        if length > 256 * 1024 * 1024 or end > len(data):
            raise EvidenceError(f"{label} has an invalid PNG chunk length")
        payload = data[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack(">I", data[offset + 8 + length:end])[0]
        observed_crc = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
        if expected_crc != observed_crc:
            raise EvidenceError(f"{label} has a PNG CRC mismatch")
        chunks.append((chunk_type, payload))
        offset = end
        if chunk_type == b"IEND":
            break
    if offset != len(data) or not chunks or chunks[0][0] != b"IHDR" or chunks[-1][0] != b"IEND":
        raise EvidenceError(f"{label} has invalid PNG chunk ordering or trailing bytes")
    if sum(1 for kind, _ in chunks if kind == b"IHDR") != 1:
        raise EvidenceError(f"{label} must contain exactly one PNG IHDR")
    if sum(1 for kind, _ in chunks if kind == b"IEND") != 1 or chunks[-1][1] != b"":
        raise EvidenceError(f"{label} must contain exactly one empty PNG IEND")
    ihdr = chunks[0][1]
    if len(ihdr) != 13:
        raise EvidenceError(f"{label} has an invalid PNG IHDR")
    width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", ihdr)
    valid_depths = {0: {1, 2, 4, 8, 16}, 2: {8, 16}, 3: {1, 2, 4, 8}, 4: {8, 16}, 6: {8, 16}}
    if (
        width < 1 or height < 1
        or color not in valid_depths or depth not in valid_depths[color]
        or compression != 0 or filtering != 0 or interlace != 0
    ):
        raise EvidenceError(f"{label} has unsupported or invalid PNG image parameters")
    idat = b"".join(payload for kind, payload in chunks if kind == b"IDAT")
    if not idat:
        raise EvidenceError(f"{label} has no PNG image data")
    kinds = [kind for kind, _ in chunks]
    unknown_critical = [
        kind for kind in kinds
        if 65 <= kind[0] <= 90 and kind not in {b"IHDR", b"PLTE", b"IDAT", b"IEND"}
    ]
    if unknown_critical:
        raise EvidenceError(f"{label} contains an unknown critical PNG chunk")
    plte_payloads = [payload for kind, payload in chunks if kind == b"PLTE"]
    if color == 3:
        if (
            len(plte_payloads) != 1
            or len(plte_payloads[0]) < 3
            or len(plte_payloads[0]) > 768
            or len(plte_payloads[0]) % 3 != 0
            or kinds.index(b"PLTE") > kinds.index(b"IDAT")
        ):
            raise EvidenceError(f"{label} has an invalid indexed-color PNG palette")
    elif color in {0, 4} and plte_payloads:
        raise EvidenceError(f"{label} has a forbidden grayscale PNG palette")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    row_bytes = (width * channels * depth + 7) // 8
    expected_raw = height * (row_bytes + 1)
    inflater = zlib.decompressobj()
    try:
        raw = inflater.decompress(idat, expected_raw + 1)
        if len(raw) <= expected_raw:
            raw += inflater.flush(expected_raw + 1 - len(raw))
    except zlib.error as exc:
        raise EvidenceError(f"{label} PNG image data does not decompress") from exc
    if (
        len(raw) != expected_raw or not inflater.eof
        or inflater.unused_data or inflater.unconsumed_tail
    ):
        raise EvidenceError(f"{label} PNG decompressed size differs from its dimensions")
    stride = row_bytes + 1
    if any(raw[offset] > 4 for offset in range(0, len(raw), stride)):
        raise EvidenceError(f"{label} has an invalid PNG scanline filter")
    return width, height


def expected_capture_filename(view: str, resolution: str) -> str:
    return f"{view}__{resolution}.png"


def required_capture_plan(
    project_root: Path = ROOT,
    policy_path: Path = POLICY_PATH,
) -> dict[str, Any]:
    """Return the authoritative, read-only filename/dimension capture matrix.

    This is an operator planning aid only.  It deliberately does not inspect a
    candidate, authenticate capture provenance, write evidence, or make any human
    approval claim.
    """
    _, policy_identity = validate_policy(policy_path, project_root)
    rows = [
        {
            "sequence": index,
            "viewId": view,
            "resolution": resolution,
            "filename": expected_capture_filename(view, resolution),
            "width": EXPECTED_DIMENSIONS[resolution][0],
            "height": EXPECTED_DIMENSIONS[resolution][1],
        }
        for index, (view, resolution) in enumerate(
            (
                (view, resolution)
                for view in EXPECTED_VIEWS
                for resolution in EXPECTED_RESOLUTIONS
            ),
            start=1,
        )
    ]
    return {
        "schema": "DiscGolfTour.Session19FinalCapturePlan.v1",
        "schemaVersion": 1,
        "session": 19,
        "state": "PASS_READ_ONLY_CAPTURE_PLAN_CANDIDATE_BINDING_AND_HUMAN_REVIEWS_PENDING",
        "authority": "OPERATOR_PLANNING_AID_NOT_EVIDENCE_OR_APPROVAL",
        "policyIdentity": policy_identity,
        "captureRootContract": {
            "externalToProjectAndCandidateArchive": True,
            "filesOnlyNoSubdirectoriesOrSidecars": True,
            "requiredFileCount": len(rows),
            "requiredFormat": "PNG",
            "byteIdenticalImagesForbidden": True,
            "hardLinksAndReparsePointsForbidden": True,
        },
        "captures": rows,
        "approvalClaims": copy.deepcopy(APPROVAL_FIELDS),
        "releaseReady": False,
        "publicReleaseApproved": False,
    }


def collect_captures(
    capture_root: Path,
    candidate_id: str,
    session_id: str,
    project_root: Path,
    archive: Path,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    if not capture_root.is_absolute():
        raise EvidenceError("--capture-root must be absolute")
    try:
        root = capture_root.resolve(strict=True)
    except OSError as exc:
        raise EvidenceError("capture root does not resolve") from exc
    reject_reparse_chain(capture_root, "capture root")
    if not root.is_dir() or is_reparse(capture_root):
        raise EvidenceError("capture root must be a regular non-reparse directory")
    require_disjoint(root, project_root, "capture and project roots")
    require_disjoint(root, archive, "capture and archive roots")
    expected = {
        expected_capture_filename(view, resolution)
        for view in EXPECTED_VIEWS for resolution in EXPECTED_RESOLUTIONS
    }
    observed: set[str] = set()
    for current, directories, files in os.walk(root, topdown=True, followlinks=False):
        current_path = Path(current)
        if current_path != root:
            raise EvidenceError("capture root must not contain subdirectories")
        for name in directories:
            child = current_path / name
            if is_reparse(child):
                raise EvidenceError("capture root contains a reparse directory")
            raise EvidenceError("capture root must contain only the exact 36 PNG files")
        for name in files:
            child = current_path / name
            if is_reparse(child):
                raise EvidenceError("capture root contains a reparse file")
            if name in observed:
                raise EvidenceError("capture root contains duplicate filenames")
            observed.add(name)
    if observed != expected:
        missing = sorted(expected - observed)
        extra = sorted(observed - expected)
        raise EvidenceError(f"capture root file set differs; missing={missing} extra={extra}")

    rows: list[dict[str, Any]] = []
    hashes: set[str] = set()
    file_ids: set[tuple[int, int]] = set()
    total_bytes = 0
    for view in EXPECTED_VIEWS:
        for resolution in EXPECTED_RESOLUTIONS:
            filename = expected_capture_filename(view, resolution)
            path = root / filename
            resolved = path.resolve(strict=True)
            if not is_within(resolved, root) or is_reparse(path):
                raise EvidenceError(f"capture {filename} escapes the external capture root")
            data = stable_read(path, f"capture {filename}", 256 * 1024 * 1024)
            info = path.stat()
            file_id = (info.st_dev, info.st_ino)
            if file_id in file_ids:
                raise EvidenceError("capture set contains hard-linked duplicate files")
            file_ids.add(file_id)
            digest = sha256_bytes(data)
            if digest in hashes:
                raise EvidenceError("capture set contains byte-identical duplicate images")
            hashes.add(digest)
            width, height = parse_png(data, f"capture {filename}")
            if (width, height) != EXPECTED_DIMENSIONS[resolution]:
                raise EvidenceError(
                    f"capture {filename} is {width}x{height}, not {resolution}"
                )
            total_bytes += len(data)
            rows.append({
                "artifactId": f"FINAL_SHIPPING_CAPTURE:{view}:{resolution}",
                "viewId": view,
                "resolution": resolution,
                "relativePath": filename,
                "recoveryLocationToken": (
                    f"DGTOUR_EXTERNAL/FINAL_SHIPPING_CAPTURES/{candidate_id}/"
                    f"{session_id}/{filename}"
                ),
                "format": "PNG",
                "width": width,
                "height": height,
                "bytes": len(data),
                "sha256": digest,
            })
    return rows, {
        "hostPathRecorded": False,
        "recoveryLocationToken": (
            f"DGTOUR_EXTERNAL/FINAL_SHIPPING_CAPTURES/{candidate_id}/{session_id}"
        ),
        "fileCount": len(rows),
        "bytes": total_bytes,
    }


def expected_manifest_path(project_root: Path, candidate_id: str, session_id: str) -> tuple[Path, str]:
    relative = f"Evidence/Session19/FinalShippingCaptureManifest-{candidate_id}-{session_id}.json"
    return project_root.joinpath(*PurePosixPath(relative).parts), relative


def build_manifest(
    *,
    candidate_id: str,
    archive: Path,
    receipt_path: Path,
    capture_root: Path,
    capture_tool: Path,
    capture_session_id: str,
    capture_method: str,
    project_root: Path = ROOT,
    policy_path: Path = POLICY_PATH,
) -> dict[str, Any]:
    if not CANDIDATE_RE.fullmatch(candidate_id):
        raise EvidenceError("candidate ID is malformed")
    validate_uuid4(capture_session_id)
    if capture_method not in CAPTURE_METHODS:
        raise EvidenceError("capture method is not an allowed external method")
    _, policy_identity = validate_policy(policy_path, project_root)
    archive_summary, archive_by_path = collect_archive(archive, candidate_id, project_root)
    receipt_identity = validate_receipt(
        receipt_path, candidate_id, archive_summary, archive_by_path, project_root
    )
    tools = tool_identities(capture_tool, project_root)
    captures, capture_root_identity = collect_captures(
        capture_root, candidate_id, capture_session_id, project_root, archive
    )
    live_exe = archive_by_path[INNER_EXE]
    _, manifest_relative = expected_manifest_path(project_root, candidate_id, capture_session_id)
    return {
        "schema": SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "artifactId": ARTIFACT_ID,
        "manifestPath": manifest_relative,
        "generatedUtc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "state": STATE,
        "authority": AUTHORITY,
        "candidateBinding": {
            "candidateId": candidate_id,
            "shippingArchiveManifestSha256": archive_summary["canonicalManifestSha256"],
            "shippingExecutableSha256": live_exe["sha256"],
        },
        "candidateEvidence": {
            "archive": archive_summary,
            "shippingExecutable": copy.deepcopy(live_exe),
            "shippingVerification": receipt_identity,
        },
        "policyIdentity": policy_identity,
        "toolIdentities": tools,
        "captureProvenance": {
            "captureSessionId": capture_session_id,
            "captureMethod": capture_method,
            "captureMethodIsOperatorDeclaredNotMechanicallyAuthenticated": True,
            "captureRoot": capture_root_identity,
            "externalOnlyPathValidationPass": True,
        },
        "captureContract": {
            "requiredViews": EXPECTED_VIEWS,
            "requiredResolutions": EXPECTED_RESOLUTIONS,
            "expectedCaptureCount": len(EXPECTED_VIEWS) * len(EXPECTED_RESOLUTIONS),
            "observedCaptureCount": len(captures),
            "matrixComplete": True,
            "actualPngHashesVerified": True,
            "actualPngDimensionsVerified": True,
            "liveArchiveRevalidated": True,
        },
        "captures": captures,
        "approvalClaims": copy.deepcopy(APPROVAL_FIELDS),
        "remainingHumanReview": [
            "PRODUCT_OWNER_FINAL_VISUAL_REVIEW",
            "ART_DIRECTOR_OR_DELEGATE_REVIEW",
            "ACCESSIBILITY_REVIEW",
            "QUALIFIED_LEGAL_LOGO_AND_TRADE_DRESS_REVIEW",
        ],
        "releaseReady": False,
        "publicReleaseApproved": False,
    }


def require_exact_keys(value: Any, expected: set[str], label: str) -> dict[str, Any]:
    if type(value) is not dict:
        raise EvidenceError(f"{label} must be an object")
    if set(value) != expected:
        raise EvidenceError(f"{label} fields differ: expected={sorted(expected)} actual={sorted(value)}")
    return value


def validate_manifest(
    manifest: Any,
    *,
    record_path: Path,
    candidate_id: str,
    archive: Path,
    receipt_path: Path,
    capture_root: Path,
    capture_tool: Path,
    capture_session_id: str,
    capture_method: str,
    project_root: Path = ROOT,
    policy_path: Path = POLICY_PATH,
) -> dict[str, Any]:
    live = build_manifest(
        candidate_id=candidate_id,
        archive=archive,
        receipt_path=receipt_path,
        capture_root=capture_root,
        capture_tool=capture_tool,
        capture_session_id=capture_session_id,
        capture_method=capture_method,
        project_root=project_root,
        policy_path=policy_path,
    )
    top = require_exact_keys(manifest, set(live), "final capture manifest")
    expected_path, expected_relative = expected_manifest_path(project_root, candidate_id, capture_session_id)
    try:
        if record_path.resolve(strict=False) != expected_path.resolve(strict=False):
            raise EvidenceError("final capture manifest path is not the exact candidate/session evidence path")
    except OSError as exc:
        raise EvidenceError("final capture manifest path cannot be resolved") from exc
    if top.get("manifestPath") != expected_relative:
        raise EvidenceError("final capture manifest self-path differs")
    if (
        top.get("schema") != SCHEMA or top.get("schemaVersion") != 1
        or top.get("session") != 19 or top.get("artifactId") != ARTIFACT_ID
        or top.get("state") != STATE or top.get("authority") != AUTHORITY
        or type(top.get("generatedUtc")) is not str
        or UTC_RE.fullmatch(top["generatedUtc"]) is None
        or top.get("releaseReady") is not False
        or top.get("publicReleaseApproved") is not False
    ):
        raise EvidenceError("final capture manifest identity or fail-closed state differs")
    binding = require_exact_keys(
        top.get("candidateBinding"),
        {"candidateId", "shippingArchiveManifestSha256", "shippingExecutableSha256"},
        "candidateBinding",
    )
    if binding != live["candidateBinding"]:
        raise EvidenceError("candidateBinding differs from the live candidate")
    candidate_evidence = require_exact_keys(
        top.get("candidateEvidence"),
        {"archive", "shippingExecutable", "shippingVerification"},
        "candidateEvidence",
    )
    if candidate_evidence != live["candidateEvidence"]:
        raise EvidenceError("candidate evidence differs from the live archive or receipt")
    if top.get("policyIdentity") != live["policyIdentity"]:
        raise EvidenceError("manual capture policy identity differs from the live policy")
    if top.get("toolIdentities") != live["toolIdentities"]:
        raise EvidenceError("tool identities differ from the live tools")
    provenance = require_exact_keys(
        top.get("captureProvenance"),
        {
            "captureSessionId", "captureMethod",
            "captureMethodIsOperatorDeclaredNotMechanicallyAuthenticated",
            "captureRoot", "externalOnlyPathValidationPass",
        },
        "captureProvenance",
    )
    if provenance != live["captureProvenance"]:
        raise EvidenceError("capture provenance or external-root identity differs")
    if top.get("captureContract") != live["captureContract"]:
        raise EvidenceError("capture contract summary differs")
    if top.get("captures") != live["captures"]:
        raise EvidenceError("capture rows differ from the 36 live external PNG files")
    approvals = require_exact_keys(top.get("approvalClaims"), set(APPROVAL_FIELDS), "approvalClaims")
    if approvals != APPROVAL_FIELDS:
        raise EvidenceError("capture manifest must not contain any human or release approval claim")
    if top.get("remainingHumanReview") != live["remainingHumanReview"]:
        raise EvidenceError("remaining human review inventory differs")
    return {
        "state": STATE,
        "candidateId": candidate_id,
        "captureSessionId": capture_session_id,
        "captureCount": len(live["captures"]),
        "archiveCanonicalManifestSha256": live["candidateBinding"]["shippingArchiveManifestSha256"],
        "shippingExecutableSha256": live["candidateBinding"]["shippingExecutableSha256"],
        "humanReviewsPending": True,
        "releaseReady": False,
    }


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def make_test_png(width: int, height: int, marker: int) -> bytes:
    row = bytes([0]) + bytes([marker & 0xFF]) * width
    raw = row * height
    return (
        PNG_SIGNATURE
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )


def write_json_test(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical_json_bytes(value))


def self_test() -> dict[str, Any]:
    tests: list[str] = []

    def expect_rejected(label: str, action: Callable[[], Any]) -> None:
        try:
            action()
        except (EvidenceError, OSError, ValueError):
            tests.append(label)
            return
        raise AssertionError(f"self-test mutation was accepted: {label}")

    with tempfile.TemporaryDirectory(prefix="dg-s19-final-captures-") as raw:
        base = Path(raw)
        project = base / "Project"
        packages = base / "Packages"
        external = base / "External"
        candidate = "S19_WindowsShipping_20260826T010203Z_abcdef123456"
        session_id = "12345678-1234-4234-9234-1234567890ab"
        archive = packages / candidate / "Windows"
        capture_root = external / "Captures"
        receipt_path = project / f"Evidence/Session19/ShippingCandidateVerification-{candidate}.json"
        policy_path = project / "Config/DG_Session19ManualReleaseReviewPolicy.json"
        record_path, record_relative = expected_manifest_path(project, candidate, session_id)
        capture_tool = external / "CaptureTool.exe"
        generator_copy = project / "Scripts/generate_dg_session19_final_shipping_capture_manifest.py"
        archive.mkdir(parents=True)
        capture_root.mkdir(parents=True)
        capture_tool.write_bytes(b"capture-tool-v1")
        generator_copy.parent.mkdir(parents=True)
        generator_copy.write_bytes(Path(__file__).read_bytes())
        archive_files = {
            "DiscGolfTour.exe": b"outer-exe",
            INNER_EXE: b"shipping-exe",
            "Manifest_UFSFiles_Win64.txt": b"ufs",
            "Manifest_NonUFSFiles_Win64.txt": b"nonufs",
            "NOTICES.txt": b"notices",
        }
        for relative, data in archive_files.items():
            target = archive.joinpath(*PurePosixPath(relative).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        policy = {
            "schema": "DiscGolfTour.Session19ManualReleaseReviewPolicy.v1",
            "schemaVersion": 1,
            "session": 19,
            "releaseReady": False,
            "publicReleaseApproved": False,
            "blanketAuthorizationIsApprovalEvidence": False,
            "finalCaptureContract": {
                "acceptedCaptureManifestPath": None,
                "mustBeFromBoundShippingCandidate": True,
                "requiredViews": EXPECTED_VIEWS,
                "requiredResolutions": EXPECTED_RESOLUTIONS,
                "savedOrDevelopmentCapturesAreFinalEvidence": False,
            },
            "reviewGates": [
                {"gateId": "LOGO_AND_TRADE_DRESS_REVIEW", "requiredArtifacts": [ARTIFACT_ID], "closed": False},
                {"gateId": "FINAL_VISUAL_PRODUCT_APPROVAL", "requiredArtifacts": [ARTIFACT_ID], "closed": False},
            ],
        }
        write_json_test(policy_path, policy)
        plan = required_capture_plan(project, policy_path)
        expected_names = [
            expected_capture_filename(view, resolution)
            for view in EXPECTED_VIEWS
            for resolution in EXPECTED_RESOLUTIONS
        ]
        if (
            plan.get("state")
            != "PASS_READ_ONLY_CAPTURE_PLAN_CANDIDATE_BINDING_AND_HUMAN_REVIEWS_PENDING"
            or plan.get("captureRootContract", {}).get("requiredFileCount") != 36
            or [row.get("filename") for row in plan.get("captures", [])] != expected_names
            or len(set(expected_names)) != 36
            or plan.get("approvalClaims") != APPROVAL_FIELDS
            or plan.get("releaseReady") is not False
            or plan.get("publicReleaseApproved") is not False
        ):
            raise AssertionError("read-only required-capture plan differs from the policy matrix")
        tests.append("valid_read_only_required_capture_plan")
        marker = 1
        for view in EXPECTED_VIEWS:
            for resolution in EXPECTED_RESOLUTIONS:
                width, height = EXPECTED_DIMENSIONS[resolution]
                (capture_root / expected_capture_filename(view, resolution)).write_bytes(
                    make_test_png(width, height, marker)
                )
                marker += 1
        archive_summary, archive_by_path = collect_archive(archive, candidate, project)
        receipt = {
            "schema": RECEIPT_SCHEMA,
            "schemaVersion": 2,
            "session": 19,
            "runId": candidate,
            "state": RECEIPT_STATE,
            "verificationMode": "DIRECT_BUILD_PASS",
            "receiptPath": f"Evidence/Session19/{receipt_path.name}",
            "archiveContract": {
                "recoveryLocationToken": archive_summary["recoveryLocationToken"],
                "hostPathRecorded": False,
                "allRequiredFilesPresent": True,
            },
            "innerShippingExecutable": {
                "relativePath": INNER_EXE,
                "bytes": archive_by_path[INNER_EXE]["bytes"],
                "sha256": archive_by_path[INNER_EXE]["sha256"],
            },
            "shippingPluginCapabilities": {
                "technicalGatePass": True,
                "finalArchiveCanonicalManifestSha256": archive_summary["canonicalManifestSha256"],
            },
            "releaseExcludedProjectRootsAbsent": True,
        }
        write_json_test(receipt_path, receipt)
        manifest = build_manifest(
            candidate_id=candidate, archive=archive, receipt_path=receipt_path,
            capture_root=capture_root, capture_tool=capture_tool,
            capture_session_id=session_id, capture_method="WINDOWS_GRAPHICS_CAPTURE",
            project_root=project, policy_path=policy_path,
        )
        manifest["manifestPath"] = record_relative
        validate_manifest(
            manifest, record_path=record_path, candidate_id=candidate, archive=archive,
            receipt_path=receipt_path, capture_root=capture_root, capture_tool=capture_tool,
            capture_session_id=session_id, capture_method="WINDOWS_GRAPHICS_CAPTURE",
            project_root=project, policy_path=policy_path,
        )
        tests.append("valid_36_capture_external_manifest")

        def reject_record(label: str, mutator: Callable[[dict[str, Any]], None]) -> None:
            candidate_manifest = copy.deepcopy(manifest)
            mutator(candidate_manifest)
            expect_rejected(label, lambda: validate_manifest(
                candidate_manifest, record_path=record_path, candidate_id=candidate,
                archive=archive, receipt_path=receipt_path, capture_root=capture_root,
                capture_tool=capture_tool, capture_session_id=session_id,
                capture_method="WINDOWS_GRAPHICS_CAPTURE", project_root=project,
                policy_path=policy_path,
            ))

        reject_record("missing_capture_row", lambda value: value["captures"].pop())
        reject_record("duplicate_capture_row", lambda value: value["captures"].__setitem__(1, copy.deepcopy(value["captures"][0])))
        reject_record("capture_hash_mutated", lambda value: value["captures"][0].__setitem__("sha256", "A" * 64))
        reject_record("capture_dimensions_mutated", lambda value: value["captures"][0].__setitem__("width", 1279))
        reject_record("capture_path_mutated", lambda value: value["captures"][0].__setitem__("relativePath", "../escape.png"))
        reject_record("candidate_id_mutated", lambda value: value["candidateBinding"].__setitem__("candidateId", "S19_WindowsShipping_stale"))
        reject_record("archive_hash_mutated", lambda value: value["candidateBinding"].__setitem__("shippingArchiveManifestSha256", "A" * 64))
        reject_record("executable_hash_mutated", lambda value: value["candidateBinding"].__setitem__("shippingExecutableSha256", "B" * 64))
        reject_record("receipt_hash_mutated", lambda value: value["candidateEvidence"]["shippingVerification"].__setitem__("sha256", "C" * 64))
        reject_record("policy_hash_mutated", lambda value: value["policyIdentity"].__setitem__("sha256", "D" * 64))
        reject_record("capture_tool_hash_mutated", lambda value: value["toolIdentities"]["captureTool"].__setitem__("sha256", "E" * 64))
        reject_record("generator_hash_mutated", lambda value: value["toolIdentities"]["manifestGeneratorValidator"].__setitem__("sha256", "F" * 64))
        reject_record("capture_method_claim_mutated", lambda value: value["captureProvenance"].__setitem__("captureMethodIsOperatorDeclaredNotMechanicallyAuthenticated", False))
        reject_record("matrix_claim_mutated", lambda value: value["captureContract"].__setitem__("matrixComplete", False))
        reject_record("human_review_fabricated", lambda value: value["approvalClaims"].__setitem__("humanVisualReviewPerformed", True))
        reject_record("art_approval_fabricated", lambda value: value["approvalClaims"].__setitem__("artApproved", True))
        reject_record("accessibility_approval_fabricated", lambda value: value["approvalClaims"].__setitem__("accessibilityApproved", True))
        reject_record("legal_approval_fabricated", lambda value: value["approvalClaims"].__setitem__("legalApproved", True))
        reject_record("product_owner_approval_fabricated", lambda value: value["approvalClaims"].__setitem__("productOwnerApproved", True))
        reject_record("release_ready_fabricated", lambda value: value.__setitem__("releaseReady", True))
        reject_record("public_release_fabricated", lambda value: value.__setitem__("publicReleaseApproved", True))
        reject_record("unexpected_manifest_field", lambda value: value.__setitem__("approval", True))
        reject_record("wrong_manifest_state", lambda value: value.__setitem__("state", "APPROVED"))

        first_path = capture_root / expected_capture_filename(EXPECTED_VIEWS[0], EXPECTED_RESOLUTIONS[0])
        original_png = first_path.read_bytes()
        first_path.write_bytes(original_png + b"tamper")
        expect_rejected("live_png_changed", lambda: validate_manifest(
            manifest, record_path=record_path, candidate_id=candidate, archive=archive,
            receipt_path=receipt_path, capture_root=capture_root, capture_tool=capture_tool,
            capture_session_id=session_id, capture_method="WINDOWS_GRAPHICS_CAPTURE",
            project_root=project, policy_path=policy_path,
        ))
        first_path.write_bytes(original_png)

        second_path = capture_root / expected_capture_filename(EXPECTED_VIEWS[0], EXPECTED_RESOLUTIONS[1])
        original_second = second_path.read_bytes()
        second_path.write_bytes(original_png)
        expect_rejected("duplicate_live_png", lambda: build_manifest(
            candidate_id=candidate, archive=archive, receipt_path=receipt_path,
            capture_root=capture_root, capture_tool=capture_tool, capture_session_id=session_id,
            capture_method="WINDOWS_GRAPHICS_CAPTURE", project_root=project,
            policy_path=policy_path,
        ))
        second_path.write_bytes(original_second)

        extra = capture_root / "unexpected.png"
        extra.write_bytes(original_png)
        expect_rejected("unexpected_capture_file", lambda: build_manifest(
            candidate_id=candidate, archive=archive, receipt_path=receipt_path,
            capture_root=capture_root, capture_tool=capture_tool, capture_session_id=session_id,
            capture_method="WINDOWS_GRAPHICS_CAPTURE", project_root=project,
            policy_path=policy_path,
        ))
        extra.unlink()

        exe_path = archive.joinpath(*PurePosixPath(INNER_EXE).parts)
        original_exe = exe_path.read_bytes()
        exe_path.write_bytes(original_exe + b"tamper")
        expect_rejected("stale_archive_or_executable", lambda: build_manifest(
            candidate_id=candidate, archive=archive, receipt_path=receipt_path,
            capture_root=capture_root, capture_tool=capture_tool, capture_session_id=session_id,
            capture_method="WINDOWS_GRAPHICS_CAPTURE", project_root=project,
            policy_path=policy_path,
        ))
        exe_path.write_bytes(original_exe)

        original_tool = capture_tool.read_bytes()
        capture_tool.write_bytes(b"capture-tool-v2")
        expect_rejected("live_capture_tool_changed", lambda: validate_manifest(
            manifest, record_path=record_path, candidate_id=candidate, archive=archive,
            receipt_path=receipt_path, capture_root=capture_root, capture_tool=capture_tool,
            capture_session_id=session_id, capture_method="WINDOWS_GRAPHICS_CAPTURE",
            project_root=project, policy_path=policy_path,
        ))
        capture_tool.write_bytes(original_tool)

        expect_rejected("project_contained_capture_root", lambda: collect_captures(
            project, candidate, session_id, project, archive
        ))
        expect_rejected("archive_contained_capture_root", lambda: collect_captures(
            archive, candidate, session_id, project, archive
        ))
        expect_rejected("wrong_candidate_archive_leaf", lambda: collect_archive(
            archive, "S19_WindowsShipping_other", project
        ))
        expect_rejected("invalid_session_id", lambda: validate_uuid4("not-a-uuid"))
        expect_rejected("non_v4_session_id", lambda: validate_uuid4("12345678-1234-1234-9234-1234567890ab"))
        expect_rejected("development_capture_method", lambda: build_manifest(
            candidate_id=candidate, archive=archive, receipt_path=receipt_path,
            capture_root=capture_root, capture_tool=capture_tool, capture_session_id=session_id,
            capture_method="UNREAL_EDITOR_SCREENSHOT", project_root=project,
            policy_path=policy_path,
        ))

        real_is_reparse = is_reparse
        try:
            globals()["is_reparse"] = lambda path: (
                Path(path) == capture_root or real_is_reparse(Path(path))
            )
            expect_rejected("simulated_reparse_capture_root", lambda: collect_captures(
                capture_root, candidate, session_id, project, archive
            ))
        finally:
            globals()["is_reparse"] = real_is_reparse

        reparse_file = capture_root / expected_capture_filename(
            EXPECTED_VIEWS[0], EXPECTED_RESOLUTIONS[0]
        )
        try:
            globals()["is_reparse"] = lambda path: (
                Path(path) == reparse_file or real_is_reparse(Path(path))
            )
            expect_rejected("simulated_reparse_capture_file", lambda: collect_captures(
                capture_root, candidate, session_id, project, archive
            ))
        finally:
            globals()["is_reparse"] = real_is_reparse

        if hasattr(os, "symlink"):
            link_root = external / "CaptureLink"
            try:
                os.symlink(capture_root, link_root, target_is_directory=True)
            except OSError:
                pass
            else:
                expect_rejected("reparse_capture_root", lambda: collect_captures(
                    link_root, candidate, session_id, project, archive
                ))

        bad_crc = bytearray(original_png)
        bad_crc[-5] ^= 1
        first_path.write_bytes(bytes(bad_crc))
        expect_rejected("png_crc_corruption", lambda: collect_captures(
            capture_root, candidate, session_id, project, archive
        ))
        first_path.write_bytes(original_png)

        first_path.write_bytes(original_png[:-4])
        expect_rejected("truncated_png", lambda: collect_captures(
            capture_root, candidate, session_id, project, archive
        ))
        first_path.write_bytes(original_png)

        if len(tests) < 32:
            raise AssertionError(f"self-test coverage unexpectedly low: {len(tests)}")
    return {
        "state": "PASS_ADVERSARIAL_SELF_TESTS",
        "testCount": len(tests),
        "tests": tests,
        "releaseReady": False,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--generate", action="store_true")
    modes.add_argument("--validate-record", type=Path)
    modes.add_argument("--print-required-captures", action="store_true")
    modes.add_argument("--self-test", action="store_true")
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--shipping-verification", type=Path)
    parser.add_argument("--capture-root", type=Path)
    parser.add_argument("--capture-tool", type=Path)
    parser.add_argument("--capture-session-id")
    parser.add_argument("--capture-method", choices=sorted(CAPTURE_METHODS), default="WINDOWS_GRAPHICS_CAPTURE")
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_test:
            print(json.dumps(self_test(), indent=2, sort_keys=True))
            return 0
        if args.print_required_captures:
            print(json.dumps(required_capture_plan(), indent=2, sort_keys=True))
            return 0
        required = {
            "--candidate-id": args.candidate_id,
            "--archive": args.archive,
            "--shipping-verification": args.shipping_verification,
            "--capture-root": args.capture_root,
            "--capture-tool": args.capture_tool,
            "--capture-session-id": args.capture_session_id,
        }
        missing = [name for name, value in required.items() if value is None]
        if missing:
            raise EvidenceError(f"missing required arguments: {missing}")
        expected_output, _ = expected_manifest_path(ROOT, args.candidate_id, args.capture_session_id)
        if args.generate:
            output = args.output if args.output is not None else expected_output
            if output.resolve(strict=False) != expected_output.resolve(strict=False):
                raise EvidenceError("--output must be the exact candidate/session evidence path")
            manifest = build_manifest(
                candidate_id=args.candidate_id, archive=args.archive,
                receipt_path=args.shipping_verification, capture_root=args.capture_root,
                capture_tool=args.capture_tool, capture_session_id=args.capture_session_id,
                capture_method=args.capture_method,
            )
            validate_manifest(
                manifest, record_path=output, candidate_id=args.candidate_id,
                archive=args.archive, receipt_path=args.shipping_verification,
                capture_root=args.capture_root, capture_tool=args.capture_tool,
                capture_session_id=args.capture_session_id,
                capture_method=args.capture_method,
            )
            output.parent.mkdir(parents=True, exist_ok=True)
            try:
                with output.open("xb") as handle:
                    handle.write(canonical_json_bytes(manifest))
            except FileExistsError as exc:
                raise EvidenceError("refusing to overwrite an existing capture manifest") from exc
            stored, _ = load_json(output, "generated final capture manifest")
            summary = validate_manifest(
                stored, record_path=output, candidate_id=args.candidate_id,
                archive=args.archive, receipt_path=args.shipping_verification,
                capture_root=args.capture_root, capture_tool=args.capture_tool,
                capture_session_id=args.capture_session_id,
                capture_method=args.capture_method,
            )
            summary.update({
                "manifestPath": output.relative_to(ROOT).as_posix(),
                "manifestSha256": sha256_bytes(stable_read(output, "generated final capture manifest")),
            })
            print(json.dumps(summary, indent=2, sort_keys=True))
            return 0
        if args.output is not None:
            raise EvidenceError("--output is valid only with --generate")
        record, _ = load_json(args.validate_record, "final capture manifest")
        summary = validate_manifest(
            record, record_path=args.validate_record, candidate_id=args.candidate_id,
            archive=args.archive, receipt_path=args.shipping_verification,
            capture_root=args.capture_root, capture_tool=args.capture_tool,
            capture_session_id=args.capture_session_id,
            capture_method=args.capture_method,
        )
        summary.update({
            "manifestPath": args.validate_record.relative_to(ROOT).as_posix(),
            "manifestSha256": sha256_bytes(stable_read(args.validate_record, "final capture manifest")),
        })
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0
    except (EvidenceError, OSError, ValueError) as exc:
        print(json.dumps({
            "state": "FAIL_CLOSED",
            "error": str(exc),
            "releaseReady": False,
        }, indent=2, sort_keys=True), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
