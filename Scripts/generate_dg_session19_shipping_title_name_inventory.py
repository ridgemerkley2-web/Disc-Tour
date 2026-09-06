#!/usr/bin/env python3
"""Generate or validate candidate-bound Shipping title/name technical evidence.

This script inventories mechanically observable title surfaces and the twelve public
display-name rows required by the Session 19 manual review policy.  It never locks a
public title, clears a name, supplies legal advice, authenticates a reviewer, closes a
manual gate, or claims release readiness.  Absolute host paths are consumed read-only
and are never serialized into the create-new receipt.
"""

from __future__ import annotations

import argparse
import copy
import csv
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Any, Callable, Iterable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ManualReleaseReviewPolicy.json"
PROJECT_PATH = ROOT / "DiscGolfTour.uproject"
SCRIPT_PATH = Path(__file__).resolve()

SCHEMA = "DiscGolfTour.Session19ShippingTitleRuntimeNameInventory.v1"
SCHEMA_VERSION = 1
SESSION = 19
PASS_STATE = (
    "PASS_CANDIDATE_BOUND_TITLE_AND_RUNTIME_NAME_TECHNICAL_INVENTORY_"
    "CLEARANCE_PENDING"
)
TITLE_ARTIFACT_ID = "BOUND_SHIPPING_TITLE_SURFACE_INVENTORY"
NAME_ARTIFACT_ID = "BOUND_SHIPPING_RUNTIME_NAME_SCAN"
OUTPUT_PATTERN = (
    "Evidence/Session19/ShippingTitleRuntimeNameInventory-{candidateId}.json"
)

OUTER_EXE = "DiscGolfTour.exe"
INNER_EXE = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
MAIN_PAK = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak"
MAIN_UTOC = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc"
MAIN_UCAS = "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas"
ASSET_REGISTRY = "DiscGolfTour/AssetRegistry.bin"
PACKAGED_DEFAULT_GAME = "DiscGolfTour/Config/DefaultGame.ini"
MOLD_CLASS = "/Script/DiscGolfTour.DiscMoldDataAsset"
PLASTIC_CLASS = "/Script/DiscGolfTour.DiscPlasticDataAsset"

CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
IOSTORE_HASH_RE = re.compile(r"^(?:0x)?[0-9A-Fa-f]{40}$")
PAK_ENTRY_RE = re.compile(r'^LogPakFile: Display: "(?P<path>[^"]+)" offset:')
PAK_SUMMARY_RE = re.compile(r"^LogPakFile: Display: (\d+) files \(")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$")

REQUIRED_ARCHIVE_FILES = [
    OUTER_EXE,
    INNER_EXE,
    MAIN_PAK,
    MAIN_UTOC,
    MAIN_UCAS,
    "DiscGolfTour/Data/PineRidgeCourse.json",
    "DiscGolfTour/Data/PineRidgeHole1.json",
    "DiscGolfTour/Data/PineRidgeHole2.json",
    "DiscGolfTour/Data/PineRidgeHole3.json",
]

PENDING_TITLE_SURFACES = [
    {
        "surfaceId": "RUNTIME_WINDOW_TITLE_AND_OS_SHELL_PRESENTATION",
        "state": "PENDING_RUNTIME_OR_HUMAN_OBSERVATION",
        "reason": "archive/config/PE scans do not prove the title rendered by the OS shell at runtime",
    },
    {
        "surfaceId": "FRONT_END_HUD_SCORECARD_SETTINGS_CREDITS_RENDERED_TEXT",
        "state": "PENDING_BOUND_FINAL_CAPTURE_AND_HUMAN_REVIEW",
        "reason": "binary strings and package identities cannot prove rendered text, clipping, or substitutions",
    },
    {
        "surfaceId": "INSTALLER_AND_UNINSTALLER_METADATA",
        "state": "PENDING_EXTERNAL_DISTRIBUTION_ARTIFACT",
        "reason": "the Windows archive contains no installer or uninstaller package",
    },
    {
        "surfaceId": "STORE_LISTING_MARKETING_SOCIAL_AND_PRESS_COPY",
        "state": "PENDING_EXTERNAL_PUBLISHING_ARTIFACT",
        "reason": "store and marketing systems are outside the Shipping archive",
    },
    {
        "surfaceId": "CRASH_REPORT_SUPPORT_AND_EXTERNAL_TELEMETRY_SURFACES",
        "state": "PENDING_RUNTIME_OR_EXTERNAL_SERVICE_OBSERVATION",
        "reason": "external crash/support service presentation is not encoded as an authoritative archive surface",
    },
    {
        "surfaceId": "FINAL_SCREENSHOT_AND_CAPTURE_SET",
        "state": "PENDING_BOUND_FINAL_CAPTURE_AND_HUMAN_REVIEW",
        "reason": "this technical scan does not create or approve the policy-required visual capture set",
    },
    {
        "surfaceId": "TITLE_LEGAL_CLEARANCE_AND_TARGET_MARKET_DECISION",
        "state": "PENDING_NAMED_PRODUCT_OWNER_AND_QUALIFIED_LEGAL_REVIEWER",
        "reason": "mechanical observations are not title clearance or a title lock",
    },
]

CLAIM_BOUNDARY = {
    "technicalInventoryOnly": True,
    "publicTitleLocked": False,
    "publicDisplayNamesCleared": False,
    "legalApproval": False,
    "productOwnerApproval": False,
    "manualGateClosed": False,
    "distributionClearance": False,
    "releaseReady": False,
}


class EvidenceError(RuntimeError):
    pass


class DuplicateKeyError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    output: dict[str, Any] = {}
    for key, value in pairs:
        if key in output:
            raise DuplicateKeyError(f"duplicate JSON key: {key}")
        output[key] = value
    return output


def load_json_bytes(data: bytes, label: str) -> Any:
    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=reject_duplicate_keys,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValueError(f"non-finite JSON number: {value}")
            ),
        )
    except (UnicodeDecodeError, json.JSONDecodeError, DuplicateKeyError, ValueError) as exc:
        raise EvidenceError(f"{label} is not strict JSON: {exc}") from exc


def load_json(path: Path) -> Any:
    return load_json_bytes(read_stable(path), str(path))


def canonical_json_bytes(value: Any) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")


def rendered_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


def is_reparse(path: Path) -> bool:
    try:
        info = os.lstat(path)
    except OSError:
        return True
    flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(
        getattr(info, "st_file_attributes", 0) & flag
    )


def exact_file(path: Path, label: str) -> Path:
    try:
        resolved = path.resolve(strict=True)
        info = os.lstat(path)
    except OSError as exc:
        raise EvidenceError(f"{label} is missing: {path}") from exc
    if is_reparse(path) or not stat.S_ISREG(info.st_mode):
        raise EvidenceError(f"{label} must be a regular non-reparse file")
    return resolved


def read_stable(path: Path) -> bytes:
    resolved = exact_file(path, "input file")
    before = resolved.stat()
    data = resolved.read_bytes()
    after = resolved.stat()
    if (
        before.st_size != after.st_size
        or before.st_mtime_ns != after.st_mtime_ns
        or len(data) != before.st_size
    ):
        raise EvidenceError(f"file changed while being read: {path}")
    return data


def hash_stable_file(path: Path) -> tuple[int, str]:
    resolved = exact_file(path, "input file")
    before = resolved.stat()
    digest = hashlib.sha256()
    bytes_read = 0
    with resolved.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            bytes_read += len(block)
            digest.update(block)
    after = resolved.stat()
    if (
        before.st_size != after.st_size
        or before.st_mtime_ns != after.st_mtime_ns
        or bytes_read != before.st_size
    ):
        raise EvidenceError(f"file changed while being hashed: {path}")
    return bytes_read, digest.hexdigest().upper()


def file_identity(path: Path, *, include_name: bool = True) -> dict[str, Any]:
    size, digest = hash_stable_file(path)
    output: dict[str, Any] = {
        "bytes": size,
        "sha256": digest,
        "hostPathRecorded": False,
    }
    if include_name:
        output["fileName"] = path.name
    return output


def project_file_binding(path: Path, role: str) -> dict[str, Any]:
    resolved = exact_file(path, role)
    try:
        relative = resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError as exc:
        raise EvidenceError(f"{role} must remain inside the project root") from exc
    return {"role": role, "path": relative, **file_identity(resolved, include_name=False)}


def canonical_relative(value: str) -> str:
    candidate = value.replace("\\", "/")
    pure = PurePosixPath(candidate)
    if (
        not candidate
        or candidate.startswith("/")
        or pure.is_absolute()
        or pure.as_posix() != candidate
        or any(part in ("", ".", "..") for part in pure.parts)
        or ":" in candidate
        or "\x00" in candidate
    ):
        raise EvidenceError(f"non-canonical relative identity: {value!r}")
    return candidate


def canonical_staged_identity(value: str) -> str:
    candidate = value.replace("\\", "/")
    while candidate.startswith("../../../"):
        candidate = candidate[9:]
    return canonical_relative(candidate.lstrip("/"))


def validate_sha(value: Any, label: str) -> str:
    if type(value) is not str or SHA256_RE.fullmatch(value) is None or value == "0" * 64:
        raise EvidenceError(f"{label} must be a non-placeholder uppercase SHA-256")
    return value


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def validate_utc(value: Any) -> bool:
    if type(value) is not str or UTC_RE.fullmatch(value) is None:
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def find_offsets(data: bytes, token: bytes, limit: int = 16) -> tuple[int, list[int]]:
    count = 0
    offsets: list[int] = []
    start = 0
    while True:
        location = data.find(token, start)
        if location < 0:
            break
        count += 1
        if len(offsets) < limit:
            offsets.append(location)
        start = location + 1
    return count, offsets


def scan_binary_tokens(data: bytes, tokens: Iterable[str]) -> list[dict[str, Any]]:
    output: list[dict[str, Any]] = []
    for token in dict.fromkeys(tokens):
        ascii_count, ascii_offsets = find_offsets(data, token.encode("utf-8"))
        utf16_count, utf16_offsets = find_offsets(data, token.encode("utf-16-le"))
        output.append({
            "token": token,
            "asciiCount": ascii_count,
            "asciiFirstOffsets": ascii_offsets,
            "utf16LeCount": utf16_count,
            "utf16LeFirstOffsets": utf16_offsets,
            "observed": ascii_count + utf16_count > 0,
        })
    return output


def count_token(data: bytes, token: str) -> dict[str, Any]:
    ascii_count, _ = find_offsets(data, token.encode("utf-8"), 0)
    utf16_count, _ = find_offsets(data, token.encode("utf-16-le"), 0)
    return {
        "asciiCount": ascii_count,
        "utf16LeCount": utf16_count,
        "observed": ascii_count + utf16_count > 0,
    }


def run_tool(arguments: list[str], context: str, timeout: int = 300) -> str:
    try:
        completed = subprocess.run(
            arguments,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        raise EvidenceError(f"{context} timed out") from exc
    output = completed.stdout.decode("utf-8", errors="replace")
    if completed.returncode != 0:
        raise EvidenceError(
            f"{context} failed with exit {completed.returncode}: {output[-2000:]}"
        )
    return output


def write_create_new(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = rendered_json_bytes(value)
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise EvidenceError(f"refusing to overwrite existing evidence: {path}") from exc


def artifact_payload(artifact_id: str, value: dict[str, Any]) -> dict[str, Any]:
    return {
        "artifactId": artifact_id,
        "artifactPayloadSha256": sha256_bytes(canonical_json_bytes(value)),
        **value,
    }


def artifact_hash_valid(value: Any, artifact_id: str) -> bool:
    if type(value) is not dict or value.get("artifactId") != artifact_id:
        return False
    recorded = value.get("artifactPayloadSha256")
    payload = {
        key: item
        for key, item in value.items()
        if key not in {"artifactId", "artifactPayloadSha256"}
    }
    return (
        type(recorded) is str
        and SHA256_RE.fullmatch(recorded) is not None
        and recorded == sha256_bytes(canonical_json_bytes(payload))
    )


def validate_manual_policy(policy: Any) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    if type(policy) is not dict:
        raise EvidenceError("manual release review policy must be an object")
    if (
        policy.get("schema") != "DiscGolfTour.Session19ManualReleaseReviewPolicy.v1"
        or policy.get("schemaVersion") != 1
        or policy.get("session") != SESSION
    ):
        raise EvidenceError("manual release review policy identity differs")
    if (
        policy.get("blanketAuthorizationIsApprovalEvidence") is not False
        or policy.get("releaseReady") is not False
        or policy.get("publicReleaseApproved") is not False
    ):
        raise EvidenceError("manual policy invents authorization or release approval")

    names = policy.get("publicNameInventory")
    if type(names) is not dict:
        raise EvidenceError("manual policy public-name inventory is missing")
    title = names.get("title")
    if type(title) is not dict:
        raise EvidenceError("manual policy title inventory is missing")
    if title.get("candidate") is not None or title.get("state") != "NOT_LOCKED_NOT_CLEARED":
        raise EvidenceError("manual policy must keep the final public title unlocked and uncleared")
    working_names = title.get("workingNames")
    if (
        type(working_names) is not list
        or len(working_names) != 3
        or len({value.casefold() for value in working_names if type(value) is str}) != 3
        or any(type(value) is not str or not value for value in working_names)
    ):
        raise EvidenceError("manual policy working-title inventory differs")
    if title.get("sourcePaths") != ["Config/DefaultGame.ini", "DiscGolfTour.uproject"]:
        raise EvidenceError("manual policy title source paths differ")

    equipment = names.get("equipment")
    course = names.get("course")
    if type(equipment) is not list or len(equipment) != 8:
        raise EvidenceError("manual policy must contain exactly eight equipment names")
    if type(course) is not list or len(course) != 4:
        raise EvidenceError("manual policy must contain exactly four course/hole names")
    if [row.get("kind") for row in equipment if type(row) is dict].count("mold") != 5:
        raise EvidenceError("manual policy mold name count differs")
    if [row.get("kind") for row in equipment if type(row) is dict].count("plastic") != 3:
        raise EvidenceError("manual policy plastic name count differs")
    if [row.get("kind") for row in course if type(row) is dict] != ["course", "hole", "hole", "hole"]:
        raise EvidenceError("manual policy course/hole name order differs")

    rows = equipment + course
    stable_ids: list[str] = []
    display_names: list[str] = []
    source_paths: list[str] = []
    for index, row in enumerate(rows):
        if type(row) is not dict:
            raise EvidenceError(f"manual policy public-name row {index} is not an object")
        stable_id = row.get("stableId")
        display_name = row.get("candidateDisplayName")
        source_path = row.get("sourcePath")
        if type(stable_id) is not str or not stable_id:
            raise EvidenceError(f"manual policy public-name row {index} lacks a stable ID")
        if type(display_name) is not str or not display_name:
            raise EvidenceError(f"manual policy public-name row {index} lacks display text")
        if type(source_path) is not str or canonical_relative(source_path) != source_path:
            raise EvidenceError(f"manual policy public-name row {index} has an unsafe source path")
        stable_ids.append(stable_id)
        display_names.append(display_name)
        source_paths.append(source_path)
    if len(stable_ids) != len({value.casefold() for value in stable_ids}):
        raise EvidenceError("manual policy contains duplicate stable IDs")
    if len(display_names) != len({value.casefold() for value in display_names}):
        raise EvidenceError("manual policy contains duplicate candidate display names")
    if len(source_paths) != len({value.casefold() for value in source_paths}):
        raise EvidenceError("manual policy contains duplicate public-name source paths")

    gates = policy.get("reviewGates")
    if type(gates) is not list:
        raise EvidenceError("manual policy review gates are missing")
    by_id = {
        row.get("gateId"): row
        for row in gates
        if type(row) is dict and type(row.get("gateId")) is str
    }
    title_gate = by_id.get("PUBLIC_TITLE_CLEARANCE", {})
    name_gate = by_id.get("PUBLIC_DISPLAY_NAME_CLEARANCE", {})
    if TITLE_ARTIFACT_ID not in title_gate.get("requiredArtifacts", []):
        raise EvidenceError("manual title gate no longer requires the bound title-surface artifact")
    if NAME_ARTIFACT_ID not in name_gate.get("requiredArtifacts", []):
        raise EvidenceError("manual display-name gate no longer requires the runtime name scan")
    if title_gate.get("closed") is not False or name_gate.get("closed") is not False:
        raise EvidenceError("manual title/name gates must remain open pending named review")
    return title, copy.deepcopy(rows)


def collect_archive(archive: Path, candidate_id: str) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    if archive.name != "Windows" or archive.parent.name != candidate_id:
        raise EvidenceError("archive root is not the requested candidate's Windows leaf")
    root = archive.resolve(strict=True)
    if (
        not root.is_dir()
        or is_reparse(archive)
        or is_reparse(archive.parent)
        or is_reparse(archive.parent.parent)
    ):
        raise EvidenceError("archive root must be a regular non-reparse directory")
    seen: set[str] = set()
    records: list[dict[str, Any]] = []
    for current, directories, files in os.walk(root, topdown=True, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if is_reparse(child):
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
            size, digest = hash_stable_file(child)
            records.append({"path": relative, "bytes": size, "sha256": digest})
    records.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    by_path = {item["path"]: item for item in records}
    missing = [path for path in REQUIRED_ARCHIVE_FILES if path not in by_path]
    if missing:
        raise EvidenceError(f"archive lacks required title/name evidence inputs: {missing}")
    material = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n"
        for item in records
    ).encode("utf-8")
    summary = {
        "fileCount": len(records),
        "bytes": sum(item["bytes"] for item in records),
        "canonicalManifestSha256": sha256_bytes(material),
        "hostPathRecorded": False,
        "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
    }
    return summary, by_path


def verification_binding(
    verification_path: Path,
    candidate_id: str,
    archive_summary: dict[str, Any],
    archive_by_path: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    resolved = exact_file(verification_path, "Shipping verification receipt")
    try:
        relative = resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError as exc:
        raise EvidenceError("Shipping verification receipt must be inside the project root") from exc
    expected_relative = (
        "Evidence/Session19/ShippingCandidateVerification-"
        f"{candidate_id}.json"
    )
    if relative != expected_relative:
        raise EvidenceError("Shipping verification receipt path does not match candidate ID")
    data = read_stable(resolved)
    receipt = load_json_bytes(data, relative)
    if type(receipt) is not dict:
        raise EvidenceError("Shipping verification receipt must be an object")
    if (
        receipt.get("schema")
        != "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
        or receipt.get("schemaVersion") != 2
        or receipt.get("session") != SESSION
        or receipt.get("runId") != candidate_id
        or receipt.get("state")
        != "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
        or receipt.get("verificationMode") != "DIRECT_BUILD_PASS"
        or receipt.get("receiptPath") != relative
        or receipt.get("archiveContract", {}).get("allRequiredFilesPresent") is not True
        or receipt.get("releaseExcludedProjectRootsAbsent") is not True
        or receipt.get("shippingPluginCapabilities", {}).get("technicalGatePass") is not True
    ):
        raise EvidenceError("Shipping verification receipt identity or pass state differs")
    receipt_archive = receipt.get("shippingPluginCapabilities", {}).get(
        "finalArchiveCanonicalManifestSha256"
    )
    if receipt_archive != archive_summary["canonicalManifestSha256"]:
        raise EvidenceError("Shipping verification receipt archive hash differs from live archive")
    inner = receipt.get("innerShippingExecutable", {})
    live_inner = archive_by_path[INNER_EXE]
    if (
        inner.get("relativePath") != INNER_EXE
        or inner.get("bytes") != live_inner["bytes"]
        or inner.get("sha256") != live_inner["sha256"]
    ):
        raise EvidenceError("Shipping verification receipt executable binding differs")
    return {
        "path": relative,
        "bytes": len(data),
        "sha256": sha256_bytes(data),
        "schema": receipt["schema"],
        "state": receipt["state"],
    }


def validate_tools(unrealpak: Path, editor: Path) -> dict[str, Any]:
    unrealpak = exact_file(unrealpak, "UnrealPak")
    editor = exact_file(editor, "UnrealEditor-Cmd")
    if unrealpak.name != "UnrealPak.exe":
        raise EvidenceError("--unrealpak must name UnrealPak.exe")
    if editor.name != "UnrealEditor-Cmd.exe":
        raise EvidenceError("--unreal-editor-cmd must name UnrealEditor-Cmd.exe")
    if (
        unrealpak.parent != editor.parent
        or unrealpak.parent.name != "Win64"
        or unrealpak.parent.parent.name != "Binaries"
        or unrealpak.parent.parent.parent.name != "Engine"
    ):
        raise EvidenceError(
            "UnrealPak and UnrealEditor-Cmd must be the same Engine/Binaries/Win64 tool set"
        )
    python = exact_file(Path(sys.executable), "Python interpreter")
    return {
        "unrealPak": file_identity(unrealpak),
        "unrealEditorCmd": file_identity(editor),
        "python": file_identity(python),
    }


def parse_pak_listing(output: str) -> tuple[list[str], dict[str, Any]]:
    identities: list[str] = []
    summary_count: int | None = None
    for line in output.splitlines():
        match = PAK_ENTRY_RE.match(line)
        if match:
            identities.append(canonical_staged_identity(match.group("path")))
        summary = PAK_SUMMARY_RE.match(line)
        if summary:
            summary_count = int(summary.group(1))
    if summary_count is None or summary_count != len(identities):
        raise EvidenceError(
            f"UnrealPak list summary differs: summary={summary_count} parsed={len(identities)}"
        )
    if len({value.casefold() for value in identities}) != len(identities):
        raise EvidenceError("Pak listing contains duplicate or case-colliding identities")
    return identities, {
        "method": "UnrealPak positional -List",
        "entryCount": len(identities),
        "canonicalIdentitySha256": sha256_bytes(
            "".join(f"{value}\n" for value in identities).encode("utf-8")
        ),
    }


def read_iostore_csv(path: Path) -> list[dict[str, str]]:
    data = read_stable(path)
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise EvidenceError("IoStore CSV is not UTF-8") from exc
    reader = csv.DictReader(text.splitlines(), skipinitialspace=True)
    required = {
        "OrderInContainer",
        "ChunkId",
        "PackageId",
        "Filename",
        "ContainerName",
        "Offset",
        "Size",
        "CompressedSize",
        "Hash",
        "ChunkType",
        "Platform",
    }
    if reader.fieldnames is None or not required.issubset(set(reader.fieldnames)):
        raise EvidenceError("IoStore CSV header differs")
    rows = [dict(row) for row in reader]
    if not rows:
        raise EvidenceError("IoStore CSV contains no rows")
    for expected_order, row in enumerate(rows):
        try:
            order = int(row.get("OrderInContainer", ""))
        except ValueError as exc:
            raise EvidenceError("IoStore CSV order is malformed") from exc
        if order != expected_order:
            raise EvidenceError("IoStore CSV order is not contiguous from zero")
    return rows


def expected_equipment_package(row: dict[str, Any]) -> str:
    return "DiscGolfTour/" + row["sourcePath"]


def expected_equipment_object(row: dict[str, Any]) -> str:
    package = row["sourcePath"].removeprefix("Content/").removesuffix(".uasset")
    stem = PurePosixPath(package).name
    return f"/Game/{package}.{stem}"


def validate_iostore_equipment(
    rows: list[dict[str, str]], equipment_rows: list[dict[str, Any]]
) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    expected = {
        expected_equipment_package(row): row for row in equipment_rows
    }
    prefix = "../../../DiscGolfTour/Content/Data/Discs/"
    observed_rows = [
        row
        for row in rows
        if str(row.get("Filename", "")).startswith(prefix)
        and str(row.get("Filename", "")).endswith(".uasset")
    ]
    normalized = [
        canonical_staged_identity(str(row.get("Filename", "")))
        for row in observed_rows
    ]
    if sorted(normalized) != sorted(expected) or len(normalized) != len(set(normalized)):
        raise EvidenceError("IoStore equipment package inventory differs from manual policy")
    output: dict[str, dict[str, Any]] = {}
    for package_path in expected:
        matches = [
            row
            for row in observed_rows
            if canonical_staged_identity(str(row.get("Filename", ""))) == package_path
        ]
        if len(matches) != 1:
            raise EvidenceError(f"IoStore package count differs: {package_path}")
        row = matches[0]
        try:
            size = int(row.get("Size", ""))
            compressed_size = int(row.get("CompressedSize", ""))
            offset = int(row.get("Offset", ""))
        except ValueError as exc:
            raise EvidenceError(f"IoStore numeric identity is malformed: {package_path}") from exc
        if (
            size <= 0
            or compressed_size <= 0
            or compressed_size > size
            or offset < 0
            or not row.get("ChunkId")
            or not row.get("PackageId")
            or not IOSTORE_HASH_RE.fullmatch(str(row.get("Hash", "")))
            or row.get("ChunkType") != "ExportBundleData"
        ):
            raise EvidenceError(f"IoStore export bundle identity differs: {package_path}")
        output[package_path] = {
            "path": package_path,
            "chunkId": row["ChunkId"],
            "packageId": row["PackageId"],
            "offset": offset,
            "size": size,
            "compressedSize": compressed_size,
            "ioHash": str(row["Hash"]).removeprefix("0x").upper(),
            "chunkType": row["ChunkType"],
            "containerName": row["ContainerName"],
            "platform": row["Platform"],
        }
    canonical_rows = sorted(
        "\t".join(
            str(row.get(key, ""))
            for key in (
                "OrderInContainer",
                "ChunkId",
                "PackageId",
                "Filename",
                "ContainerName",
                "Offset",
                "Size",
                "CompressedSize",
                "Hash",
                "ChunkType",
                "Platform",
            )
        )
        + "\n"
        for row in rows
    )
    named = [
        canonical_staged_identity(str(row["Filename"]))
        for row in rows
        if row.get("Filename")
        and not (
            str(row["Filename"]).startswith("<")
            and str(row["Filename"]).endswith(">")
        )
    ]
    return output, {
        "method": "UnrealPak -ListContainer CSV",
        "rowCount": len(rows),
        "namedEntryCount": len(named),
        "equipmentPackageCount": len(output),
        "canonicalInventorySha256": sha256_bytes(
            "".join(canonical_rows).encode("utf-8")
        ),
        "canonicalNamedIdentitySha256": sha256_bytes(
            "".join(f"{value}\n" for value in named).encode("utf-8")
        ),
    }


def decode_text(data: bytes, label: str) -> str:
    encodings = ["utf-8-sig"]
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        encodings.insert(0, "utf-16")
    for encoding in encodings:
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise EvidenceError(f"{label} is not supported UTF text")


def ini_section(text: str, section: str) -> dict[str, str]:
    current: str | None = None
    output: dict[str, str] = {}
    found = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith((";", "#")):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            if current == section:
                if found:
                    raise EvidenceError(f"INI section is duplicated: {section}")
                found = True
            continue
        if current != section or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if key in output:
            raise EvidenceError(f"INI key is duplicated in {section}: {key}")
        output[key] = value.strip()
    if not found:
        raise EvidenceError(f"INI section is missing: {section}")
    return output


def source_authority(
    policy: dict[str, Any], title_policy: dict[str, Any], rows: list[dict[str, Any]]
) -> dict[str, Any]:
    default_game_path = ROOT / "Config/DefaultGame.ini"
    default_game_data = read_stable(default_game_path)
    general = ini_section(
        decode_text(default_game_data, "source DefaultGame.ini"),
        "/Script/EngineSettings.GeneralProjectSettings",
    )
    required_general = [
        "ProjectID", "ProjectName", "Description", "CompanyName", "ProjectVersion"
    ]
    if any(not general.get(key) for key in required_general):
        raise EvidenceError("source DefaultGame.ini lacks required project settings")
    if general["ProjectName"] not in title_policy["workingNames"]:
        raise EvidenceError("source ProjectName is absent from the manual working-name inventory")
    project_data = read_stable(PROJECT_PATH)
    project = load_json_bytes(project_data, "DiscGolfTour.uproject")
    if type(project) is not dict or project.get("FileVersion") != 3:
        raise EvidenceError("project descriptor identity differs")

    source_rows: list[dict[str, Any]] = []
    for row in rows:
        path = ROOT.joinpath(*PurePosixPath(row["sourcePath"]).parts)
        data = read_stable(path)
        binding = {
            "path": row["sourcePath"],
            "bytes": len(data),
            "sha256": sha256_bytes(data),
        }
        if row["kind"] in {"mold", "plastic"}:
            marker = count_token(data, row["candidateDisplayName"])
            if not marker["observed"]:
                raise EvidenceError(
                    f"source equipment package lacks policy display/stable marker: {row['stableId']}"
                )
            source_rows.append({**row, "source": binding, "sourceStringObservation": marker})
        else:
            document = load_json_bytes(data, row["sourcePath"])
            fields = public_name_fields(document, row["kind"], row["sourcePath"])
            if fields["stableId"] != row["stableId"]:
                raise EvidenceError(
                    f"source stable ID differs from policy: {row['stableId']}"
                )
            if fields["displayName"] != row["candidateDisplayName"]:
                raise EvidenceError(
                    f"source display name differs from policy: {row['stableId']}"
                )
            source_rows.append({**row, "source": binding, "sourceFields": fields})
    return {
        "manualPolicy": project_file_binding(POLICY_PATH, "manual-release-review-policy"),
        "defaultGame": {
            "path": "Config/DefaultGame.ini",
            "bytes": len(default_game_data),
            "sha256": sha256_bytes(default_game_data),
            "generalProjectSettings": {key: general[key] for key in required_general},
        },
        "projectDescriptor": {
            "path": "DiscGolfTour.uproject",
            "bytes": len(project_data),
            "sha256": sha256_bytes(project_data),
            "engineAssociation": project.get("EngineAssociation"),
            "category": project.get("Category"),
            "description": project.get("Description"),
            "runtimeModuleNames": [
                module.get("Name")
                for module in project.get("Modules", [])
                if type(module) is dict and module.get("Type") == "Runtime"
            ],
        },
        "publicNameSources": source_rows,
    }


def public_name_fields(document: Any, kind: str, label: str) -> dict[str, Any]:
    if type(document) is not dict:
        raise EvidenceError(f"public-name JSON must be an object: {label}")
    if kind == "course":
        course_id = document.get("courseId")
        display_name = document.get("displayName")
        layout_id = document.get("layoutId")
        if (
            document.get("schema") != "disc_golf_course_manifest"
            or document.get("schemaVersion") != 1
            or type(course_id) is not str
            or not course_id
            or type(display_name) is not str
            or not display_name
            or type(layout_id) is not str
            or not layout_id
        ):
            raise EvidenceError(f"course public-name fields differ: {label}")
        return {
            "stableId": course_id,
            "displayName": display_name,
            "courseId": course_id,
            "layoutId": layout_id,
        }
    if kind == "hole":
        course_id = document.get("courseId")
        hole_number = document.get("holeNumber")
        display_name = document.get("holeName")
        layout_id = document.get("layoutId")
        if (
            document.get("schema") != "disc_golf_hole_blockout"
            or document.get("schemaVersion") != 1
            or type(course_id) is not str
            or not course_id
            or type(hole_number) is not int
            or hole_number <= 0
            or type(display_name) is not str
            or not display_name
            or type(layout_id) is not str
            or not layout_id
        ):
            raise EvidenceError(f"hole public-name fields differ: {label}")
        return {
            "stableId": f"{course_id}.Hole{hole_number}",
            "displayName": display_name,
            "courseId": course_id,
            "layoutId": layout_id,
            "holeNumber": hole_number,
        }
    raise EvidenceError(f"unsupported JSON public-name kind: {kind}")


def decode_dump_page(path: Path) -> str:
    return decode_text(read_stable(path), str(path))


def class_objects(registry_text: str, class_path: str) -> tuple[int, list[str]]:
    lines = registry_text.splitlines()
    header_re = re.compile(r"^\t" + re.escape(class_path) + r" : (\d+) item\(s\)$")
    locations = [index for index, line in enumerate(lines) if header_re.fullmatch(line)]
    if len(locations) != 1:
        raise EvidenceError(f"packaged Asset Registry class section differs: {class_path}")
    index = locations[0]
    match = header_re.fullmatch(lines[index])
    if match is None:
        raise EvidenceError(f"packaged Asset Registry class header differs: {class_path}")
    declared = int(match.group(1))
    objects: list[str] = []
    for line in lines[index + 1:]:
        if line.startswith("\t\t"):
            objects.append(line[2:])
            continue
        if line.startswith("\t") or line.startswith("--- "):
            break
    if declared != len(objects) or len(objects) != len(set(objects)):
        raise EvidenceError(f"packaged Asset Registry class count differs: {class_path}")
    return declared, objects


def validate_asset_registry(
    registry_text: str, equipment_rows: list[dict[str, Any]]
) -> tuple[dict[str, bool], dict[str, Any]]:
    expected_by_kind = {
        "mold": sorted(
            expected_equipment_object(row)
            for row in equipment_rows
            if row["kind"] == "mold"
        ),
        "plastic": sorted(
            expected_equipment_object(row)
            for row in equipment_rows
            if row["kind"] == "plastic"
        ),
    }
    observed_by_kind: dict[str, list[str]] = {}
    declared_by_kind: dict[str, int] = {}
    for kind, class_path in (("mold", MOLD_CLASS), ("plastic", PLASTIC_CLASS)):
        declared, objects = class_objects(registry_text, class_path)
        if sorted(objects) != expected_by_kind[kind]:
            raise EvidenceError(f"packaged Asset Registry {kind} inventory differs")
        declared_by_kind[kind] = declared
        observed_by_kind[kind] = objects
    presence = {
        row["stableId"]: (
            observed_by_kind[row["kind"]].count(expected_equipment_object(row)) == 1
        )
        for row in equipment_rows
    }
    if not all(presence.values()):
        raise EvidenceError("packaged Asset Registry lacks an exact equipment object")
    return presence, {
        "method": "UnrealEditor-Cmd DumpAssetRegistry -All over extracted Shipping AssetRegistry.bin",
        "dumpCanonicalSha256": sha256_bytes(registry_text.encode("utf-8")),
        "classCounts": {
            MOLD_CLASS: declared_by_kind["mold"],
            PLASTIC_CLASS: declared_by_kind["plastic"],
        },
        "classObjects": {
            MOLD_CLASS: observed_by_kind["mold"],
            PLASTIC_CLASS: observed_by_kind["plastic"],
        },
    }


def cooked_suffix(name: str, stem: str) -> str | None:
    if not name.startswith(stem):
        return None
    suffix = name[len(stem):]
    if suffix in {".uheader", ".uexp", ".ubulk", ".uptnl", ".m.ubulk"}:
        return suffix
    return None


def validate_extracted_equipment(
    extracted_root: Path,
    equipment_rows: list[dict[str, Any]],
    iostore: dict[str, dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    base = extracted_root / "DiscGolfTour/Content/Data/Discs"
    if not base.is_dir() or is_reparse(base):
        raise EvidenceError("extracted equipment directory is missing or unsafe")
    all_files = sorted(path for path in base.rglob("*") if path.is_file())
    allowed_files: set[Path] = set()
    output: dict[str, dict[str, Any]] = {}
    for row in equipment_rows:
        package = expected_equipment_package(row)
        package_relative = PurePosixPath(package).with_suffix("").relative_to(
            "DiscGolfTour/Content/Data/Discs"
        )
        cooked_base = base.joinpath(*package_relative.parts)
        parts: dict[str, Path] = {}
        for path in cooked_base.parent.glob(cooked_base.name + ".*"):
            if not path.is_file() or is_reparse(path):
                raise EvidenceError(f"cooked equipment part is unsafe: {package}")
            suffix = cooked_suffix(path.name, cooked_base.name)
            if suffix is None or suffix in parts:
                raise EvidenceError(f"cooked equipment part inventory differs: {path.name}")
            parts[suffix] = path
            allowed_files.add(path.resolve())
        if set(parts).issuperset({".uheader", ".uexp"}) is not True:
            raise EvidenceError(f"required cooked equipment parts are missing: {package}")
        export_parts = [parts[".uheader"], parts[".uexp"]]
        export_bytes = sum(path.stat().st_size for path in export_parts)
        if export_bytes != iostore[package]["size"]:
            raise EvidenceError(f"cooked equipment export size differs: {package}")
        combined = b"".join(read_stable(path) for path in export_parts)
        markers = scan_binary_tokens(
            combined, [row["stableId"], row["candidateDisplayName"], cooked_base.name]
        )
        if not all(item["observed"] for item in markers):
            raise EvidenceError(f"cooked equipment marker is missing: {package}")
        part_records: list[dict[str, Any]] = []
        for suffix in sorted(parts):
            path = parts[suffix]
            part_records.append({
                "part": suffix,
                "fileName": path.name,
                **file_identity(path, include_name=False),
            })
        output[row["stableId"]] = {
            "packagePath": package,
            "objectPath": expected_equipment_object(row),
            "exportBundleBytes": export_bytes,
            "parts": part_records,
            "packageMarkerObservations": markers,
        }
    unexpected = [
        path.relative_to(base).as_posix()
        for path in all_files
        if path.resolve() not in allowed_files
    ]
    if unexpected:
        raise EvidenceError(f"unexpected extracted equipment file(s): {unexpected}")
    return output


def scan_file_tokens(path: Path, tokens: Iterable[str]) -> list[dict[str, Any]]:
    resolved = exact_file(path, "token-scan file")
    unique_tokens = list(dict.fromkeys(tokens))
    encoded: dict[str, dict[bytes, int]] = {
        "ascii": {
            token.encode("utf-8"): index for index, token in enumerate(unique_tokens)
        },
        "utf16": {
            token.encode("utf-16-le"): index for index, token in enumerate(unique_tokens)
        },
    }
    matchers: dict[str, re.Pattern[bytes] | None] = {}
    contained: dict[str, dict[bytes, list[tuple[bytes, int]]]] = {}
    overlap_risks: dict[str, set[bytes]] = {}
    for encoding, values in encoded.items():
        alternatives = sorted(values, key=lambda value: (-len(value), value))
        matchers[encoding] = (
            re.compile(b"(" + b"|".join(re.escape(value) for value in alternatives) + b")")
            if alternatives
            else None
        )
        contained[encoding] = {
            outer: [
                (inner, index) for inner, index in values.items() if inner in outer
            ]
            for outer in alternatives
        }
        risks: set[bytes] = set()
        for left in alternatives:
            for right in alternatives:
                if left == right or left in right or right in left:
                    continue
                maximum = min(len(left), len(right)) - 1
                if any(left[-length:] == right[:length] for length in range(1, maximum + 1)):
                    risks.add(left)
                    risks.add(right)
        overlap_risks[encoding] = risks
    max_pattern = max(
        (len(value) for values in encoded.values() for value in values), default=1
    )
    counts = [{"ascii": 0, "utf16": 0} for _ in unique_tokens]
    offsets = [{"ascii": [], "utf16": []} for _ in unique_tokens]
    processed = 0
    tail = b""
    with resolved.open("rb") as handle:
        while True:
            block = handle.read(8 * 1024 * 1024)
            if not block:
                break
            combined = tail + block
            combined_start = processed - len(tail)
            for encoding, matcher in matchers.items():
                if matcher is None:
                    continue
                for match in matcher.finditer(combined):
                    outer = match.group(1)
                    outer_start = match.start(1)
                    for pattern, token_index in contained[encoding][outer]:
                        nested_start = 0
                        while True:
                            found = outer.find(pattern, nested_start)
                            if found < 0:
                                break
                            absolute = combined_start + outer_start + found
                            if (
                                pattern not in overlap_risks[encoding]
                                and absolute + len(pattern) > processed
                            ):
                                counts[token_index][encoding] += 1
                                if len(offsets[token_index][encoding]) < 16:
                                    offsets[token_index][encoding].append(absolute)
                            nested_start = found + 1
                for pattern in overlap_risks[encoding]:
                    token_index = encoded[encoding][pattern]
                    start = 0
                    while True:
                        found = combined.find(pattern, start)
                        if found < 0:
                            break
                        absolute = combined_start + found
                        if absolute + len(pattern) > processed:
                            counts[token_index][encoding] += 1
                            if len(offsets[token_index][encoding]) < 16:
                                offsets[token_index][encoding].append(absolute)
                        start = found + 1
            processed += len(block)
            tail = combined[-(max_pattern - 1):] if max_pattern > 1 else b""
    output: list[dict[str, Any]] = []
    for index, token in enumerate(unique_tokens):
        output.append({
            "token": token,
            "asciiCount": counts[index]["ascii"],
            "asciiFirstOffsets": offsets[index]["ascii"],
            "utf16LeCount": counts[index]["utf16"],
            "utf16LeFirstOffsets": offsets[index]["utf16"],
            "observed": counts[index]["ascii"] + counts[index]["utf16"] > 0,
        })
    return output


def token_scan_binding(
    archive: Path,
    relative: str,
    archive_by_path: dict[str, dict[str, Any]],
    tokens: Iterable[str],
) -> dict[str, Any]:
    if relative not in archive_by_path:
        raise EvidenceError(f"token scan path is absent from archive: {relative}")
    return {
        "path": relative,
        "bytes": archive_by_path[relative]["bytes"],
        "sha256": archive_by_path[relative]["sha256"],
        "tokenObservations": scan_file_tokens(
            archive.joinpath(*PurePosixPath(relative).parts), tokens
        ),
    }


def pe_version_resource(path: Path) -> dict[str, Any]:
    exact_file(path, "PE version-resource input")
    fields = [
        "ProductName", "FileDescription", "InternalName", "OriginalFilename",
        "CompanyName", "ProductVersion", "FileVersion",
    ]
    empty = {
        "versionResourcePresent": False,
        "translations": [],
        "strings": {field: [] for field in fields},
    }
    if os.name != "nt":
        return {**empty, "observationState": "UNSUPPORTED_NON_WINDOWS_HOST"}
    version = ctypes.windll.version  # type: ignore[attr-defined]
    unused = ctypes.c_uint(0)
    size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(unused))
    if size == 0:
        return {**empty, "observationState": "NO_PE_VERSION_RESOURCE"}
    buffer = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, buffer):
        raise EvidenceError("GetFileVersionInfoW failed for Shipping executable")
    translation_pointer = ctypes.c_void_p()
    translation_bytes = ctypes.c_uint(0)
    translations: list[tuple[int, int]] = []
    if version.VerQueryValueW(
        buffer, r"\VarFileInfo\Translation",
        ctypes.byref(translation_pointer), ctypes.byref(translation_bytes),
    ):
        word_count = translation_bytes.value // ctypes.sizeof(ctypes.c_ushort)
        words = ctypes.cast(
            translation_pointer, ctypes.POINTER(ctypes.c_ushort * word_count)
        ).contents
        translations = [
            (int(words[index]), int(words[index + 1]))
            for index in range(0, word_count - 1, 2)
        ]
    if not translations:
        translations = [(0x0409, 0x04B0)]
    strings: dict[str, list[dict[str, str]]] = {field: [] for field in fields}
    for language, codepage in translations:
        table = f"{language:04X}{codepage:04X}"
        for field in fields:
            value_pointer = ctypes.c_void_p()
            character_count = ctypes.c_uint(0)
            query = rf"\StringFileInfo\{table}\{field}"
            if version.VerQueryValueW(
                buffer, query, ctypes.byref(value_pointer), ctypes.byref(character_count)
            ) and character_count.value:
                value = ctypes.wstring_at(value_pointer, character_count.value).rstrip("\x00")
                strings[field].append({"translation": table, "value": value})
    return {
        "versionResourcePresent": True,
        "observationState": "PE_VERSION_RESOURCE_QUERIED",
        "translations": [f"{language:04X}{codepage:04X}" for language, codepage in translations],
        "strings": strings,
    }


def loose_text_scan(
    archive: Path,
    archive_by_path: dict[str, dict[str, Any]],
    tokens: Iterable[str],
) -> dict[str, Any]:
    suffixes = {".ini", ".json", ".txt", ".cfg", ".xml", ".uproject"}
    candidates = [
        relative for relative in sorted(archive_by_path)
        if PurePosixPath(relative).suffix.lower() in suffixes
    ]
    matched: list[dict[str, Any]] = []
    undecodable: list[str] = []
    for relative in candidates:
        data = read_stable(archive.joinpath(*PurePosixPath(relative).parts))
        try:
            text = decode_text(data, relative)
        except EvidenceError:
            undecodable.append(relative)
            continue
        observations = scan_binary_tokens(text.encode("utf-8"), tokens)
        hits = [item for item in observations if item["observed"]]
        if hits:
            matched.append({
                "path": relative,
                "bytes": archive_by_path[relative]["bytes"],
                "sha256": archive_by_path[relative]["sha256"],
                "tokenObservations": hits,
            })
    material = "".join(f"{value}\n" for value in candidates).encode("utf-8")
    return {
        "method": "strict UTF loose archive text scan",
        "scannedFileCount": len(candidates),
        "scannedIdentitySha256": sha256_bytes(material),
        "matchedFileCount": len(matched),
        "matchedFiles": matched,
        "undecodableFiles": undecodable,
    }


def extract_packaged_config(path: Path, source: dict[str, Any]) -> dict[str, Any]:
    data = read_stable(path)
    general = ini_section(
        decode_text(data, "packaged DefaultGame.ini"),
        "/Script/EngineSettings.GeneralProjectSettings",
    )
    expected = source["defaultGame"]["generalProjectSettings"]
    if any(general.get(key) != value for key, value in expected.items()):
        raise EvidenceError("packaged DefaultGame project settings differ from source authority")
    return {
        "path": PACKAGED_DEFAULT_GAME,
        "bytes": len(data),
        "sha256": sha256_bytes(data),
        "generalProjectSettings": {key: general[key] for key in expected},
        "matchesSourceProjectSettings": True,
    }


def runtime_course_rows(
    archive: Path,
    archive_by_path: dict[str, dict[str, Any]],
    rows: list[dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    output: dict[str, dict[str, Any]] = {}
    for row in rows:
        if row["kind"] not in {"course", "hole"}:
            continue
        relative = "DiscGolfTour/" + row["sourcePath"]
        if relative not in archive_by_path:
            raise EvidenceError(f"Shipping course/name file is missing: {relative}")
        data = read_stable(archive.joinpath(*PurePosixPath(relative).parts))
        fields = public_name_fields(
            load_json_bytes(data, relative), row["kind"], relative
        )
        if (
            fields["stableId"] != row["stableId"]
            or fields["displayName"] != row["candidateDisplayName"]
        ):
            raise EvidenceError(f"Shipping course/name fields differ: {row['stableId']}")
        output[row["stableId"]] = {
            "path": relative,
            "bytes": len(data),
            "sha256": sha256_bytes(data),
            "runtimeFields": fields,
        }
    return output


def artifact_candidate_binding(candidate: dict[str, Any]) -> dict[str, Any]:
    return {
        "candidateId": candidate["candidateId"],
        "archiveCanonicalManifestSha256": candidate["archive"]["canonicalManifestSha256"],
        "shippingExecutableSha256": candidate["innerShippingExecutable"]["sha256"],
        "shippingVerificationReceiptSha256": candidate["shippingVerificationReceipt"]["sha256"],
    }


def artifact_tool_binding(tools: dict[str, Any]) -> dict[str, Any]:
    return {
        role: {
            "fileName": value["fileName"],
            "bytes": value["bytes"],
            "sha256": value["sha256"],
            "hostPathRecorded": value["hostPathRecorded"],
        }
        for role, value in tools.items()
    }


def scan_observations_for(
    scan: dict[str, Any], tokens: Iterable[str]
) -> list[dict[str, Any]]:
    selected = set(tokens)
    return [
        copy.deepcopy(item)
        for item in scan.get("tokenObservations", [])
        if item.get("token") in selected
    ]


def recursively_reject_invented_approval(value: Any, path: str = "$") -> list[str]:
    errors: list[str] = []
    forbidden_true = {
        "approved", "accepted", "cleared", "clearanceAccepted", "legalApproval",
        "productOwnerApproval", "manualGateClosed", "distributionClearance",
        "releaseReady", "publicReleaseApproved", "humanApprovalAccepted",
    }
    if type(value) is dict:
        for key, child in value.items():
            child_path = f"{path}.{key}"
            if key in forbidden_true and child is not False:
                errors.append(f"invented approval/clearance value at {child_path}")
            errors.extend(recursively_reject_invented_approval(child, child_path))
    elif type(value) is list:
        for index, child in enumerate(value):
            errors.extend(recursively_reject_invented_approval(child, f"{path}[{index}]"))
    return errors


def recursively_reject_host_paths(value: Any, path: str = "$") -> list[str]:
    errors: list[str] = []
    if type(value) is str:
        if re.search(r"(?i)(?:^|[\s=])(?:[A-Z]:[\\/]|\\\\)", value):
            errors.append(f"absolute host path serialized at {path}")
    elif type(value) is dict:
        for key, child in value.items():
            errors.extend(recursively_reject_host_paths(child, f"{path}.{key}"))
    elif type(value) is list:
        for index, child in enumerate(value):
            errors.extend(recursively_reject_host_paths(child, f"{path}[{index}]"))
    return errors


def build_record(
    *,
    candidate_id: str,
    generated_utc: str,
    title_policy: dict[str, Any],
    policy_rows: list[dict[str, Any]],
    source: dict[str, Any],
    candidate: dict[str, Any],
    tools: dict[str, Any],
    observations: dict[str, Any],
) -> dict[str, Any]:
    binding = artifact_candidate_binding(candidate)
    title_surfaces = [
        {
            "surfaceId": "SOURCE_DEFAULT_GAME_GENERAL_PROJECT_SETTINGS",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(source["defaultGame"]),
        },
        {
            "surfaceId": "SOURCE_PROJECT_DESCRIPTOR_METADATA",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(source["projectDescriptor"]),
        },
        {
            "surfaceId": "PACKAGED_DEFAULT_GAME_GENERAL_PROJECT_SETTINGS",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(observations["packagedDefaultGame"]),
        },
        {
            "surfaceId": "ARCHIVE_EXECUTABLE_FILENAMES",
            "mechanicalOnly": True,
            "observation": {
                "outer": candidate["outerShippingExecutable"],
                "inner": candidate["innerShippingExecutable"],
            },
        },
        {
            "surfaceId": "OUTER_EXECUTABLE_PE_VERSION_RESOURCE",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(observations["peVersionResources"]["outer"]),
        },
        {
            "surfaceId": "INNER_EXECUTABLE_PE_VERSION_RESOURCE",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(observations["peVersionResources"]["inner"]),
        },
        {
            "surfaceId": "LOOSE_ARCHIVE_TEXT_SURFACES",
            "mechanicalOnly": True,
            "observation": copy.deepcopy(observations["looseText"]),
        },
        {
            "surfaceId": "SHIPPING_EXECUTABLE_BINARY_STRING_SURFACES",
            "mechanicalOnly": True,
            "observation": {
                "outer": copy.deepcopy(observations["tokenScans"]["outerExecutable"]),
                "inner": copy.deepcopy(observations["tokenScans"]["innerExecutable"]),
            },
        },
        {
            "surfaceId": "PAK_AND_IOSTORE_NAMESPACE_AND_PAYLOAD_SURFACES",
            "mechanicalOnly": True,
            "observation": {
                "pakNamespace": copy.deepcopy(observations["pakInventory"]),
                "ioStoreNamespace": copy.deepcopy(observations["ioStoreInventory"]),
                "pakPayload": copy.deepcopy(observations["tokenScans"]["pak"]),
                "utocPayload": copy.deepcopy(observations["tokenScans"]["utoc"]),
                "ucasPayload": copy.deepcopy(observations["tokenScans"]["ucas"]),
            },
        },
    ]
    title_value = {
        "candidateBinding": binding,
        "toolBindings": artifact_tool_binding(tools),
        "finalPublicTitle": None,
        "titleDecisionState": "NOT_LOCKED_NOT_CLEARED",
        "workingNames": copy.deepcopy(title_policy["workingNames"]),
        "mechanicallyObservedSurfaceCount": len(title_surfaces),
        "mechanicallyObservedSurfaces": title_surfaces,
        "pendingUnobservableSurfaces": copy.deepcopy(PENDING_TITLE_SURFACES),
        "clearanceState": "PENDING_NAMED_PRODUCT_OWNER_AND_QUALIFIED_LEGAL_REVIEWER",
        "approvalInferred": False,
        "claimBoundary": copy.deepcopy(CLAIM_BOUNDARY),
    }

    source_by_id = {
        row["stableId"]: row for row in source["publicNameSources"]
    }
    runtime_rows: list[dict[str, Any]] = []
    token_scans = observations["tokenScans"]
    for row in policy_rows:
        tokens = [row["stableId"], row["candidateDisplayName"]]
        if row["kind"] in {"mold", "plastic"}:
            package = expected_equipment_package(row)
            runtime_observation = {
                "ioStoreExportBundle": copy.deepcopy(observations["ioStoreEquipment"][package]),
                "packagedAssetRegistryObjectPath": expected_equipment_object(row),
                "packagedAssetRegistryObjectPresentExactlyOnce": observations[
                    "assetRegistryPresence"
                ][row["stableId"]],
                "extractedCookedPackage": copy.deepcopy(
                    observations["cookedEquipment"][row["stableId"]]
                ),
            }
        else:
            runtime_observation = {
                "looseShippingJson": copy.deepcopy(
                    observations["runtimeCourseRows"][row["stableId"]]
                )
            }
        runtime_rows.append({
            "kind": row["kind"],
            "stableId": row["stableId"],
            "candidateDisplayName": row["candidateDisplayName"],
            "sourcePath": row["sourcePath"],
            "sourceBinding": copy.deepcopy(source_by_id[row["stableId"]]),
            "runtimeObservation": runtime_observation,
            "binaryAndContainerTokenObservations": {
                key: scan_observations_for(scan, tokens)
                for key, scan in token_scans.items()
            },
            "stableIdPreserved": True,
            "clearanceState": "PENDING_HUMAN_OR_LEGAL_DISPOSITION",
            "approvalInferred": False,
        })
    name_value = {
        "candidateBinding": binding,
        "toolBindings": artifact_tool_binding(tools),
        "requiredPolicyRowCount": 12,
        "observedRuntimeRowCount": len(runtime_rows),
        "rows": runtime_rows,
        "assetRegistryObservation": copy.deepcopy(observations["assetRegistry"]),
        "globalBinaryAndContainerTokenScans": copy.deepcopy(token_scans),
        "allStableIdsPreserved": True,
        "publicDisplayNamesCleared": False,
        "ownerApproval": False,
        "legalApproval": False,
        "manualGateClosed": False,
    }
    title_artifact = artifact_payload(TITLE_ARTIFACT_ID, title_value)
    name_artifact = artifact_payload(NAME_ARTIFACT_ID, name_value)
    record = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "session": SESSION,
        "candidateId": candidate_id,
        "generatedUtc": generated_utc,
        "state": PASS_STATE,
        "recordPath": OUTPUT_PATTERN.replace("{candidateId}", candidate_id),
        "claimBoundary": copy.deepcopy(CLAIM_BOUNDARY),
        "sourceAuthority": source,
        "candidateBinding": candidate,
        "tools": tools,
        "containerObservations": observations,
        "artifactIdsAndHashes": [
            {
                "artifactId": TITLE_ARTIFACT_ID,
                "artifactPayloadSha256": title_artifact["artifactPayloadSha256"],
            },
            {
                "artifactId": NAME_ARTIFACT_ID,
                "artifactPayloadSha256": name_artifact["artifactPayloadSha256"],
            },
        ],
        "artifacts": {
            TITLE_ARTIFACT_ID: title_artifact,
            NAME_ARTIFACT_ID: name_artifact,
        },
        "failures": [],
    }
    host_errors = recursively_reject_host_paths(record)
    if host_errors:
        raise EvidenceError("; ".join(host_errors))
    return record


def exact_keys(value: Any, expected: set[str], label: str, errors: list[str]) -> bool:
    if type(value) is not dict:
        errors.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != expected:
        errors.append(
            f"{label} keys differ: missing={sorted(expected - actual)} extra={sorted(actual - expected)}"
        )
        return False
    return True


def validate_record_structure(
    record: Any,
    title_policy: dict[str, Any],
    policy_rows: list[dict[str, Any]],
    policy_sha256: str,
    script_sha256: str,
) -> list[str]:
    errors: list[str] = []
    top_keys = {
        "schema", "schemaVersion", "session", "candidateId", "generatedUtc", "state",
        "recordPath", "claimBoundary", "sourceAuthority", "candidateBinding", "tools",
        "containerObservations", "artifactIdsAndHashes", "artifacts", "failures",
    }
    if not exact_keys(record, top_keys, "record", errors):
        return errors
    candidate_id = record.get("candidateId")
    if (
        record.get("schema") != SCHEMA
        or record.get("schemaVersion") != SCHEMA_VERSION
        or record.get("session") != SESSION
        or type(candidate_id) is not str
        or CANDIDATE_RE.fullmatch(candidate_id) is None
        or record.get("generatedUtc") is None
        or not validate_utc(record.get("generatedUtc"))
        or record.get("state") != PASS_STATE
        or record.get("recordPath") != OUTPUT_PATTERN.replace("{candidateId}", candidate_id or "")
        or record.get("claimBoundary") != CLAIM_BOUNDARY
        or record.get("failures") != []
    ):
        errors.append("record identity, state, claim boundary, or failure list differs")

    errors.extend(recursively_reject_invented_approval(record))
    errors.extend(recursively_reject_host_paths(record))
    source = record.get("sourceAuthority")
    if type(source) is not dict:
        errors.append("source authority is missing")
    else:
        policy_binding = source.get("manualPolicy", {})
        generator_binding = source.get("generator", {})
        if (
            policy_binding.get("path") != "Config/DG_Session19ManualReleaseReviewPolicy.json"
            or policy_binding.get("sha256") != policy_sha256
            or generator_binding.get("path")
            != "Scripts/generate_dg_session19_shipping_title_name_inventory.py"
            or generator_binding.get("sha256") != script_sha256
        ):
            errors.append("source policy or generator binding is stale")
        source_rows = source.get("publicNameSources")
        if type(source_rows) is not list or len(source_rows) != 12:
            errors.append("source authority must bind exactly twelve public-name rows")

    candidate = record.get("candidateBinding")
    binding: dict[str, Any] | None = None
    if type(candidate) is not dict:
        errors.append("candidate binding is missing")
    else:
        try:
            binding = artifact_candidate_binding(candidate)
            if (
                candidate.get("candidateId") != candidate_id
                or candidate.get("archive", {}).get("hostPathRecorded") is not False
                or candidate.get("archive", {}).get("recoveryLocationToken")
                != f"DGTOUR_PACKAGES/{candidate_id}/Windows"
                or candidate.get("innerShippingExecutable", {}).get("path") != INNER_EXE
                or candidate.get("outerShippingExecutable", {}).get("path") != OUTER_EXE
                or candidate.get("shippingVerificationReceipt", {}).get("path")
                != f"Evidence/Session19/ShippingCandidateVerification-{candidate_id}.json"
            ):
                errors.append("candidate paths or candidate ID differ")
            validate_sha(binding["archiveCanonicalManifestSha256"], "archive manifest")
            validate_sha(binding["shippingExecutableSha256"], "Shipping executable")
            validate_sha(binding["shippingVerificationReceiptSha256"], "verification receipt")
        except (EvidenceError, KeyError, TypeError) as exc:
            errors.append(f"candidate hash binding differs: {exc}")

    tools = record.get("tools")
    if type(tools) is not dict or set(tools) != {"unrealPak", "unrealEditorCmd", "python"}:
        errors.append("tool binding set differs")
    else:
        expected_names = {
            "unrealPak": "UnrealPak.exe",
            "unrealEditorCmd": "UnrealEditor-Cmd.exe",
        }
        for role, tool in tools.items():
            if type(tool) is not dict or tool.get("hostPathRecorded") is not False:
                errors.append(f"tool binding differs: {role}")
                continue
            try:
                validate_sha(tool.get("sha256"), f"{role} tool")
            except EvidenceError as exc:
                errors.append(str(exc))
            if role in expected_names and tool.get("fileName") != expected_names[role]:
                errors.append(f"tool filename differs: {role}")

    artifact_rows = record.get("artifactIdsAndHashes")
    artifacts = record.get("artifacts")
    expected_ids = [TITLE_ARTIFACT_ID, NAME_ARTIFACT_ID]
    if (
        type(artifact_rows) is not list
        or [item.get("artifactId") for item in artifact_rows if type(item) is dict]
        != expected_ids
        or type(artifacts) is not dict
        or list(artifacts) != expected_ids
    ):
        errors.append("artifact ID inventory differs")
        return errors
    for row, artifact_id in zip(artifact_rows, expected_ids):
        artifact = artifacts[artifact_id]
        if type(row) is not dict or set(row) != {"artifactId", "artifactPayloadSha256"}:
            errors.append(f"artifact hash row differs: {artifact_id}")
        if not artifact_hash_valid(artifact, artifact_id):
            errors.append(f"artifact payload hash differs: {artifact_id}")
        elif row.get("artifactPayloadSha256") != artifact.get("artifactPayloadSha256"):
            errors.append(f"artifact top-level hash differs: {artifact_id}")

    title = artifacts[TITLE_ARTIFACT_ID]
    title_keys = {
        "artifactId", "artifactPayloadSha256", "candidateBinding", "toolBindings", "finalPublicTitle",
        "titleDecisionState", "workingNames", "mechanicallyObservedSurfaceCount",
        "mechanicallyObservedSurfaces", "pendingUnobservableSurfaces", "clearanceState",
        "approvalInferred", "claimBoundary",
    }
    if exact_keys(title, title_keys, "title artifact", errors):
        surfaces = title.get("mechanicallyObservedSurfaces")
        if (
            binding is None
            or title.get("candidateBinding") != binding
            or title.get("toolBindings") != artifact_tool_binding(tools)
            or title.get("finalPublicTitle") is not None
            or title.get("titleDecisionState") != "NOT_LOCKED_NOT_CLEARED"
            or title.get("workingNames") != title_policy.get("workingNames")
            or type(surfaces) is not list
            or title.get("mechanicallyObservedSurfaceCount") != len(surfaces)
            or len(surfaces) != 9
            or title.get("pendingUnobservableSurfaces") != PENDING_TITLE_SURFACES
            or title.get("clearanceState")
            != "PENDING_NAMED_PRODUCT_OWNER_AND_QUALIFIED_LEGAL_REVIEWER"
            or title.get("approvalInferred") is not False
            or title.get("claimBoundary") != CLAIM_BOUNDARY
        ):
            errors.append("title artifact decision boundary or surface inventory differs")
        elif len({item.get("surfaceId") for item in surfaces if type(item) is dict}) != len(surfaces):
            errors.append("title artifact contains duplicate surface IDs")

    names = artifacts[NAME_ARTIFACT_ID]
    name_keys = {
        "artifactId", "artifactPayloadSha256", "candidateBinding", "toolBindings", "requiredPolicyRowCount",
        "observedRuntimeRowCount", "rows", "assetRegistryObservation",
        "globalBinaryAndContainerTokenScans", "allStableIdsPreserved",
        "publicDisplayNamesCleared", "ownerApproval", "legalApproval", "manualGateClosed",
    }
    if exact_keys(names, name_keys, "runtime-name artifact", errors):
        name_rows = names.get("rows")
        if (
            binding is None
            or names.get("candidateBinding") != binding
            or names.get("toolBindings") != artifact_tool_binding(tools)
            or names.get("requiredPolicyRowCount") != 12
            or names.get("observedRuntimeRowCount") != 12
            or type(name_rows) is not list
            or len(name_rows) != 12
            or names.get("allStableIdsPreserved") is not True
            or names.get("publicDisplayNamesCleared") is not False
            or names.get("ownerApproval") is not False
            or names.get("legalApproval") is not False
            or names.get("manualGateClosed") is not False
        ):
            errors.append("runtime-name artifact decision boundary or row count differs")
        else:
            row_keys = {
                "kind", "stableId", "candidateDisplayName", "sourcePath", "sourceBinding",
                "runtimeObservation", "binaryAndContainerTokenObservations",
                "stableIdPreserved", "clearanceState", "approvalInferred",
            }
            observed_identity: list[tuple[Any, Any, Any, Any]] = []
            for index, (row, policy_row) in enumerate(zip(name_rows, policy_rows)):
                if not exact_keys(row, row_keys, f"runtime-name row {index}", errors):
                    continue
                identity = (
                    row.get("kind"), row.get("stableId"),
                    row.get("candidateDisplayName"), row.get("sourcePath"),
                )
                expected = (
                    policy_row["kind"], policy_row["stableId"],
                    policy_row["candidateDisplayName"], policy_row["sourcePath"],
                )
                expected_source = (
                    source_rows[index]
                    if type(source_rows) is list and len(source_rows) == 12
                    else None
                )
                observed_identity.append(identity)
                if (
                    identity != expected
                    or expected_source is None
                    or row.get("sourceBinding") != expected_source
                    or row.get("stableIdPreserved") is not True
                    or row.get("clearanceState") != "PENDING_HUMAN_OR_LEGAL_DISPOSITION"
                    or row.get("approvalInferred") is not False
                    or type(row.get("runtimeObservation")) is not dict
                    or type(row.get("binaryAndContainerTokenObservations")) is not dict
                ):
                    errors.append(f"runtime-name row differs: {index}")
            if len(observed_identity) != len(set(observed_identity)):
                errors.append("runtime-name artifact contains duplicate rows")
    return sorted(set(errors))


def safe_scratch_remove(path: Path) -> None:
    resolved = path.resolve(strict=True)
    temp_root = Path(tempfile.gettempdir()).resolve(strict=True)
    if (
        resolved.parent != temp_root
        or not resolved.name.startswith("dgt_s19_title_name_")
        or is_reparse(resolved)
    ):
        raise EvidenceError(f"refusing unsafe scratch removal: {resolved}")
    shutil.rmtree(resolved)


def require_exact_extraction(root: Path, relative: str, name: str) -> Path:
    expected = root.joinpath(*PurePosixPath(relative).parts)
    matches = sorted(root.rglob(name))
    if len(matches) != 1 or matches[0].resolve() != expected.resolve():
        raise EvidenceError(
            f"expected one canonical extracted {name}, observed {len(matches)}"
        )
    return exact_file(expected, f"extracted {name}")


def audit_live_candidate(
    candidate_id: str,
    archive: Path,
    verification_path: Path,
    unrealpak_path: Path,
    editor_path: Path,
    generated_utc: str,
) -> dict[str, Any]:
    if CANDIDATE_RE.fullmatch(candidate_id) is None:
        raise EvidenceError("--candidate-id has an invalid Session 19 Shipping identity")
    if not validate_utc(generated_utc):
        raise EvidenceError("generated UTC is invalid")
    policy_data = read_stable(POLICY_PATH)
    policy = load_json_bytes(policy_data, "manual release review policy")
    title_policy, policy_rows = validate_manual_policy(policy)
    source = source_authority(policy, title_policy, policy_rows)
    source["generator"] = project_file_binding(
        SCRIPT_PATH, "shipping-title-runtime-name-inventory-generator"
    )
    archive_summary, archive_by_path = collect_archive(archive, candidate_id)
    verification = verification_binding(
        verification_path, candidate_id, archive_summary, archive_by_path
    )
    tools = validate_tools(unrealpak_path, editor_path)
    unrealpak = exact_file(unrealpak_path, "UnrealPak")
    editor = exact_file(editor_path, "UnrealEditor-Cmd")

    container_paths = {
        "pak": archive.joinpath(*PurePosixPath(MAIN_PAK).parts),
        "utoc": archive.joinpath(*PurePosixPath(MAIN_UTOC).parts),
        "ucas": archive.joinpath(*PurePosixPath(MAIN_UCAS).parts),
    }
    for role, path in container_paths.items():
        exact_file(path, f"Shipping {role}")
        if str({"pak": MAIN_PAK, "utoc": MAIN_UTOC, "ucas": MAIN_UCAS}[role]) not in archive_by_path:
            raise EvidenceError(f"archive manifest lacks required {role}")

    scratch = Path(tempfile.mkdtemp(prefix="dgt_s19_title_name_"))
    cleanup_error: Exception | None = None
    try:
        pak_output = run_tool(
            [str(unrealpak), str(container_paths["pak"]), "-List"],
            "Pak namespace inventory",
        )
        pak_identities, pak_summary = parse_pak_listing(pak_output)
        if ASSET_REGISTRY not in pak_identities or PACKAGED_DEFAULT_GAME not in pak_identities:
            raise EvidenceError("Pak namespace lacks AssetRegistry.bin or packaged DefaultGame.ini")

        csv_path = scratch / "IoStore.csv"
        run_tool(
            [
                str(unrealpak), f"-ListContainer={container_paths['utoc']}",
                f"-CSV={csv_path}",
            ],
            "IoStore namespace inventory",
        )
        iostore_rows = read_iostore_csv(csv_path)
        equipment_rows = [
            row for row in policy_rows if row["kind"] in {"mold", "plastic"}
        ]
        iostore_equipment, iostore_summary = validate_iostore_equipment(
            iostore_rows, equipment_rows
        )

        equipment_extract = scratch / "equipment"
        run_tool(
            [
                str(unrealpak), str(container_paths["utoc"]), "-Extract",
                str(equipment_extract),
                "-Filter=*DiscGolfTour/Content/Data/Discs/*",
            ],
            "Shipping equipment package extraction",
        )
        cooked_equipment = validate_extracted_equipment(
            equipment_extract, equipment_rows, iostore_equipment
        )

        pak_extract = scratch / "pak"
        run_tool(
            [
                str(unrealpak), str(container_paths["pak"]), "-Extract",
                str(pak_extract), "-Filter=*AssetRegistry.bin",
            ],
            "Shipping Asset Registry extraction",
        )
        run_tool(
            [
                str(unrealpak), str(container_paths["pak"]), "-Extract",
                str(pak_extract), "-Filter=*DiscGolfTour/Config/DefaultGame.ini",
            ],
            "Shipping DefaultGame.ini extraction",
        )
        registry_path = require_exact_extraction(
            pak_extract, ASSET_REGISTRY, "AssetRegistry.bin"
        )
        packaged_config_path = require_exact_extraction(
            pak_extract, PACKAGED_DEFAULT_GAME, "DefaultGame.ini"
        )
        packaged_default_game = extract_packaged_config(packaged_config_path, source)

        dump_root = scratch / "registry-dump"
        dump_log = scratch / "DumpAssetRegistry.log"
        dump_output = run_tool(
            [
                str(editor), str(PROJECT_PATH), "-run=DumpAssetRegistry",
                f"-Path={registry_path}", f"-OutDir={dump_root}", "-All",
                "-unattended", "-nop4", "-nosplash", "-nullrhi", "-NoSound",
                f"-abslog={dump_log}",
            ],
            "Shipping Asset Registry dump",
        )
        dump_log_text = decode_text(read_stable(dump_log), "DumpAssetRegistry log")
        dump_authority = dump_output + "\n" + dump_log_text
        if (
            "Success - 0 error(s)" not in dump_authority
            or "Engine exit requested" not in dump_authority
        ):
            raise EvidenceError("DumpAssetRegistry did not report a clean commandlet exit")
        pages = sorted(dump_root.glob("Page_*.txt"))
        if not pages:
            raise EvidenceError("DumpAssetRegistry emitted no page authority")
        registry_text = "\n".join(decode_dump_page(page) for page in pages)
        asset_registry_presence, asset_registry_summary = validate_asset_registry(
            registry_text, equipment_rows
        )
        registry_data = read_stable(registry_path)
        asset_registry_summary = {
            "path": ASSET_REGISTRY,
            "bytes": len(registry_data),
            "sha256": sha256_bytes(registry_data),
            "pageCount": len(pages),
            **asset_registry_summary,
        }

        runtime_courses = runtime_course_rows(archive, archive_by_path, policy_rows)
        tokens = list(dict.fromkeys(
            list(title_policy["workingNames"])
            + [row["stableId"] for row in policy_rows]
            + [row["candidateDisplayName"] for row in policy_rows]
        ))
        token_scans = {
            "outerExecutable": token_scan_binding(
                archive, OUTER_EXE, archive_by_path, tokens
            ),
            "innerExecutable": token_scan_binding(
                archive, INNER_EXE, archive_by_path, tokens
            ),
            "pak": token_scan_binding(archive, MAIN_PAK, archive_by_path, tokens),
            "utoc": token_scan_binding(archive, MAIN_UTOC, archive_by_path, tokens),
            "ucas": token_scan_binding(archive, MAIN_UCAS, archive_by_path, tokens),
        }
        loose_scan = loose_text_scan(archive, archive_by_path, tokens)
        observations = {
            "pakInventory": {
                "container": {"path": MAIN_PAK, **archive_by_path[MAIN_PAK]},
                **pak_summary,
            },
            "ioStoreInventory": {
                "utoc": {"path": MAIN_UTOC, **archive_by_path[MAIN_UTOC]},
                "ucas": {"path": MAIN_UCAS, **archive_by_path[MAIN_UCAS]},
                **iostore_summary,
            },
            "packagedDefaultGame": packaged_default_game,
            "assetRegistry": asset_registry_summary,
            "assetRegistryPresence": asset_registry_presence,
            "ioStoreEquipment": iostore_equipment,
            "cookedEquipment": cooked_equipment,
            "runtimeCourseRows": runtime_courses,
            "peVersionResources": {
                "outer": pe_version_resource(
                    archive.joinpath(*PurePosixPath(OUTER_EXE).parts)
                ),
                "inner": pe_version_resource(
                    archive.joinpath(*PurePosixPath(INNER_EXE).parts)
                ),
            },
            "looseText": loose_scan,
            "tokenScans": token_scans,
        }
        candidate = {
            "candidateId": candidate_id,
            "platform": "Windows",
            "configuration": "Shipping",
            "archive": archive_summary,
            "outerShippingExecutable": {
                "path": OUTER_EXE,
                **archive_by_path[OUTER_EXE],
            },
            "innerShippingExecutable": {
                "path": INNER_EXE,
                **archive_by_path[INNER_EXE],
            },
            "shippingVerificationReceipt": verification,
            "containers": {
                "pak": {"path": MAIN_PAK, **archive_by_path[MAIN_PAK]},
                "utoc": {"path": MAIN_UTOC, **archive_by_path[MAIN_UTOC]},
                "ucas": {"path": MAIN_UCAS, **archive_by_path[MAIN_UCAS]},
            },
        }
        record = build_record(
            candidate_id=candidate_id,
            generated_utc=generated_utc,
            title_policy=title_policy,
            policy_rows=policy_rows,
            source=source,
            candidate=candidate,
            tools=tools,
            observations=observations,
        )
        structure_errors = validate_record_structure(
            record,
            title_policy,
            policy_rows,
            sha256_bytes(policy_data),
            hash_stable_file(SCRIPT_PATH)[1],
        )
        if structure_errors:
            raise EvidenceError(
                "generated record failed internal validation: " + "; ".join(structure_errors)
            )
        return record
    finally:
        try:
            safe_scratch_remove(scratch)
        except Exception as exc:  # preserve the primary evidence failure if one is active
            cleanup_error = exc
        if cleanup_error is not None and sys.exc_info()[0] is None:
            raise EvidenceError(f"scratch cleanup failed: {cleanup_error}")


def expected_record_path(candidate_id: str) -> Path:
    return ROOT.joinpath(
        *PurePosixPath(OUTPUT_PATTERN.replace("{candidateId}", candidate_id)).parts
    )


def validate_exact_record_path(path: Path, candidate_id: str, label: str) -> Path:
    expected = expected_record_path(candidate_id).resolve()
    provided = path.resolve(strict=False)
    if provided != expected:
        raise EvidenceError(
            f"{label} must be {OUTPUT_PATTERN.replace('{candidateId}', candidate_id)}"
        )
    current = expected.parent
    root = ROOT.resolve()
    while current != root:
        if current.exists() and is_reparse(current):
            raise EvidenceError(f"{label} parent must not be a reparse point")
        if root not in current.parents and current != root:
            raise EvidenceError(f"{label} escapes the project root")
        current = current.parent
    return expected


def first_difference(left: Any, right: Any, path: str = "$") -> str | None:
    if type(left) is not type(right):
        return f"{path}: type {type(left).__name__} != {type(right).__name__}"
    if type(left) is dict:
        if set(left) != set(right):
            return f"{path}: key sets differ"
        for key in left:
            found = first_difference(left[key], right[key], f"{path}.{key}")
            if found:
                return found
        return None
    if type(left) is list:
        if len(left) != len(right):
            return f"{path}: list lengths differ"
        for index, (left_item, right_item) in enumerate(zip(left, right)):
            found = first_difference(left_item, right_item, f"{path}[{index}]")
            if found:
                return found
        return None
    if left != right:
        return f"{path}: values differ"
    return None


def refresh_artifact_hashes(record: dict[str, Any]) -> None:
    for row in record["artifactIdsAndHashes"]:
        artifact = record["artifacts"][row["artifactId"]]
        payload = {
            key: value for key, value in artifact.items()
            if key not in {"artifactId", "artifactPayloadSha256"}
        }
        digest = sha256_bytes(canonical_json_bytes(payload))
        artifact["artifactPayloadSha256"] = digest
        row["artifactPayloadSha256"] = digest


def sample_record_for_self_test(
    title_policy: dict[str, Any],
    rows: list[dict[str, Any]],
    policy_sha256: str,
    script_sha256: str,
) -> dict[str, Any]:
    candidate_id = "S19_WindowsShipping_SELFTEST"
    source_rows: list[dict[str, Any]] = []
    for index, row in enumerate(rows):
        source_rows.append({
            **row,
            "source": {
                "path": row["sourcePath"],
                "bytes": index + 1,
                "sha256": f"{index + 1:064X}",
            },
        })
    source = {
        "manualPolicy": {
            "role": "manual-release-review-policy",
            "path": "Config/DG_Session19ManualReleaseReviewPolicy.json",
            "bytes": 1,
            "sha256": policy_sha256,
            "hostPathRecorded": False,
        },
        "generator": {
            "role": "shipping-title-runtime-name-inventory-generator",
            "path": "Scripts/generate_dg_session19_shipping_title_name_inventory.py",
            "bytes": 1,
            "sha256": script_sha256,
            "hostPathRecorded": False,
        },
        "defaultGame": {
            "path": "Config/DefaultGame.ini",
            "generalProjectSettings": {"ProjectName": "Disc Golf Tour"},
        },
        "projectDescriptor": {"path": "DiscGolfTour.uproject"},
        "publicNameSources": source_rows,
    }
    candidate = {
        "candidateId": candidate_id,
        "platform": "Windows",
        "configuration": "Shipping",
        "archive": {
            "fileCount": 9,
            "bytes": 9,
            "canonicalManifestSha256": "A" * 64,
            "hostPathRecorded": False,
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
        },
        "outerShippingExecutable": {
            "path": OUTER_EXE, "bytes": 1, "sha256": "B" * 64,
        },
        "innerShippingExecutable": {
            "path": INNER_EXE, "bytes": 2, "sha256": "C" * 64,
        },
        "shippingVerificationReceipt": {
            "path": f"Evidence/Session19/ShippingCandidateVerification-{candidate_id}.json",
            "bytes": 3,
            "sha256": "D" * 64,
            "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
            "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
        },
        "containers": {
            "pak": {"path": MAIN_PAK, "bytes": 1, "sha256": "E" * 64},
            "utoc": {"path": MAIN_UTOC, "bytes": 1, "sha256": "F" * 64},
            "ucas": {"path": MAIN_UCAS, "bytes": 1, "sha256": "1" * 64},
        },
    }
    empty_scan = {
        "path": "fixture.bin", "bytes": 1, "sha256": "2" * 64,
        "tokenObservations": [],
    }
    equipment = [row for row in rows if row["kind"] in {"mold", "plastic"}]
    course = [row for row in rows if row["kind"] in {"course", "hole"}]
    observations = {
        "pakInventory": {},
        "ioStoreInventory": {},
        "packagedDefaultGame": {},
        "assetRegistry": {},
        "assetRegistryPresence": {row["stableId"]: True for row in equipment},
        "ioStoreEquipment": {
            expected_equipment_package(row): {} for row in equipment
        },
        "cookedEquipment": {row["stableId"]: {} for row in equipment},
        "runtimeCourseRows": {row["stableId"]: {} for row in course},
        "peVersionResources": {"outer": {}, "inner": {}},
        "looseText": {},
        "tokenScans": {
            key: copy.deepcopy(empty_scan)
            for key in ("outerExecutable", "innerExecutable", "pak", "utoc", "ucas")
        },
    }
    tools = {
        "unrealPak": {
            "fileName": "UnrealPak.exe", "bytes": 1, "sha256": "3" * 64,
            "hostPathRecorded": False,
        },
        "unrealEditorCmd": {
            "fileName": "UnrealEditor-Cmd.exe", "bytes": 1, "sha256": "4" * 64,
            "hostPathRecorded": False,
        },
        "python": {
            "fileName": "python.exe", "bytes": 1, "sha256": "5" * 64,
            "hostPathRecorded": False,
        },
    }
    return build_record(
        candidate_id=candidate_id,
        generated_utc="2026-08-26T00:00:00Z",
        title_policy=title_policy,
        policy_rows=rows,
        source=source,
        candidate=candidate,
        tools=tools,
        observations=observations,
    )


def run_self_test() -> int:
    global ROOT
    failures: list[str] = []
    mutation_count = 0
    rejected_mutations = 0
    optional_reparse_test = "NOT_ATTEMPTED"
    try:
        policy_data = read_stable(POLICY_PATH)
        policy = load_json_bytes(policy_data, "manual release review policy")
        title_policy, rows = validate_manual_policy(policy)
        policy_sha = sha256_bytes(policy_data)
        script_sha = hash_stable_file(SCRIPT_PATH)[1]
        sample = sample_record_for_self_test(
            title_policy, rows, policy_sha, script_sha
        )
        baseline_errors = validate_record_structure(
            sample, title_policy, rows, policy_sha, script_sha
        )
        if baseline_errors:
            failures.append(f"baseline record failed: {baseline_errors}")

        def reject_policy(name: str, mutate: Callable[[dict[str, Any]], None]) -> None:
            nonlocal mutation_count, rejected_mutations
            mutation_count += 1
            candidate = copy.deepcopy(policy)
            mutate(candidate)
            try:
                validate_manual_policy(candidate)
            except (EvidenceError, KeyError, TypeError, ValueError):
                rejected_mutations += 1
            else:
                failures.append(f"policy mutation escaped: {name}")

        policy_mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
            ("blanket approval", lambda value: value.__setitem__("blanketAuthorizationIsApprovalEvidence", True)),
            ("release ready", lambda value: value.__setitem__("releaseReady", True)),
            ("public release", lambda value: value.__setitem__("publicReleaseApproved", True)),
            ("locked title", lambda value: value["publicNameInventory"]["title"].__setitem__("candidate", "Disc Golf Tour")),
            ("title state", lambda value: value["publicNameInventory"]["title"].__setitem__("state", "LOCKED")),
            ("working title missing", lambda value: value["publicNameInventory"]["title"]["workingNames"].pop()),
            ("working title duplicate", lambda value: value["publicNameInventory"]["title"]["workingNames"].__setitem__(2, value["publicNameInventory"]["title"]["workingNames"][0])),
            ("title source path", lambda value: value["publicNameInventory"]["title"]["sourcePaths"].__setitem__(0, "../bad")),
            ("equipment missing", lambda value: value["publicNameInventory"]["equipment"].pop()),
            ("equipment extra", lambda value: value["publicNameInventory"]["equipment"].append(copy.deepcopy(value["publicNameInventory"]["equipment"][0]))),
            ("stable ID duplicate", lambda value: value["publicNameInventory"]["equipment"][1].__setitem__("stableId", value["publicNameInventory"]["equipment"][0]["stableId"])),
            ("display name duplicate", lambda value: value["publicNameInventory"]["equipment"][1].__setitem__("candidateDisplayName", value["publicNameInventory"]["equipment"][0]["candidateDisplayName"])),
            ("source path duplicate", lambda value: value["publicNameInventory"]["equipment"][1].__setitem__("sourcePath", value["publicNameInventory"]["equipment"][0]["sourcePath"])),
            ("display blank", lambda value: value["publicNameInventory"]["equipment"][0].__setitem__("candidateDisplayName", "")),
            ("source traversal", lambda value: value["publicNameInventory"]["equipment"][0].__setitem__("sourcePath", "../bad.uasset")),
            ("mold kind", lambda value: value["publicNameInventory"]["equipment"][0].__setitem__("kind", "plastic")),
            ("course missing", lambda value: value["publicNameInventory"]["course"].pop()),
            ("course extra", lambda value: value["publicNameInventory"]["course"].append(copy.deepcopy(value["publicNameInventory"]["course"][0]))),
            ("course kind order", lambda value: value["publicNameInventory"]["course"][0].__setitem__("kind", "hole")),
            ("title artifact removed", lambda value: value["reviewGates"][0]["requiredArtifacts"].remove(TITLE_ARTIFACT_ID)),
            ("name artifact removed", lambda value: value["reviewGates"][1]["requiredArtifacts"].remove(NAME_ARTIFACT_ID)),
            ("title gate closed", lambda value: value["reviewGates"][0].__setitem__("closed", True)),
            ("name gate closed", lambda value: value["reviewGates"][1].__setitem__("closed", True)),
        ]
        for name, mutate in policy_mutations:
            reject_policy(name, mutate)

        def reject_record(
            name: str,
            mutate: Callable[[dict[str, Any]], None],
            refresh: bool = False,
        ) -> None:
            nonlocal mutation_count, rejected_mutations
            mutation_count += 1
            candidate = copy.deepcopy(sample)
            mutate(candidate)
            if refresh:
                refresh_artifact_hashes(candidate)
            if validate_record_structure(
                candidate, title_policy, rows, policy_sha, script_sha
            ):
                rejected_mutations += 1
            else:
                failures.append(f"record mutation escaped: {name}")

        title_artifact_path = lambda value: value["artifacts"][TITLE_ARTIFACT_ID]
        name_artifact_path = lambda value: value["artifacts"][NAME_ARTIFACT_ID]
        record_mutations: list[tuple[str, Callable[[dict[str, Any]], None], bool]] = [
            ("schema", lambda value: value.__setitem__("schema", "bad"), False),
            ("candidate", lambda value: value.__setitem__("candidateId", "wrong"), False),
            ("utc", lambda value: value.__setitem__("generatedUtc", "soon"), False),
            ("state", lambda value: value.__setitem__("state", "APPROVED"), False),
            ("failure injected", lambda value: value["failures"].append("x"), False),
            ("claim release", lambda value: value["claimBoundary"].__setitem__("releaseReady", True), False),
            ("policy stale", lambda value: value["sourceAuthority"]["manualPolicy"].__setitem__("sha256", "6" * 64), False),
            ("script stale", lambda value: value["sourceAuthority"]["generator"].__setitem__("sha256", "7" * 64), False),
            ("archive random", lambda value: value["candidateBinding"]["archive"].__setitem__("canonicalManifestSha256", "8" * 64), False),
            ("archive zero", lambda value: value["candidateBinding"]["archive"].__setitem__("canonicalManifestSha256", "0" * 64), False),
            ("wrong executable", lambda value: value["candidateBinding"]["innerShippingExecutable"].__setitem__("path", "bad.exe"), False),
            ("receipt path", lambda value: value["candidateBinding"]["shippingVerificationReceipt"].__setitem__("path", "Evidence/bad.json"), False),
            ("tool hash", lambda value: value["tools"]["unrealPak"].__setitem__("sha256", "9" * 64), False),
            ("tool host path", lambda value: value["tools"]["unrealPak"].__setitem__("path", r"C:\\bad\\UnrealPak.exe"), False),
            ("extra artifact", lambda value: value["artifacts"].__setitem__("INVENTED_APPROVAL", {}), False),
            ("artifact hash", lambda value: title_artifact_path(value).__setitem__("artifactPayloadSha256", "A" * 64), False),
            ("final title invented", lambda value: title_artifact_path(value).__setitem__("finalPublicTitle", "Disc Golf Tour"), True),
            ("title approved", lambda value: title_artifact_path(value).__setitem__("approvalInferred", True), True),
            ("working title altered", lambda value: title_artifact_path(value)["workingNames"].__setitem__(0, "Changed"), True),
            ("title surface missing", lambda value: title_artifact_path(value)["mechanicallyObservedSurfaces"].pop(), True),
            ("title surface duplicate", lambda value: title_artifact_path(value)["mechanicallyObservedSurfaces"].append(copy.deepcopy(title_artifact_path(value)["mechanicallyObservedSurfaces"][0])), True),
            ("pending surface missing", lambda value: title_artifact_path(value)["pendingUnobservableSurfaces"].pop(), True),
            ("name row missing", lambda value: name_artifact_path(value)["rows"].pop(), True),
            ("name row extra", lambda value: name_artifact_path(value)["rows"].append(copy.deepcopy(name_artifact_path(value)["rows"][0])), True),
            ("name stable ID", lambda value: name_artifact_path(value)["rows"][0].__setitem__("stableId", "Changed"), True),
            ("name display", lambda value: name_artifact_path(value)["rows"][0].__setitem__("candidateDisplayName", "Changed"), True),
            ("source binding altered", lambda value: name_artifact_path(value)["rows"][0]["sourceBinding"]["source"].__setitem__("sha256", "F" * 64), True),
            ("stable not preserved", lambda value: name_artifact_path(value)["rows"][0].__setitem__("stableIdPreserved", False), True),
            ("row approval", lambda value: name_artifact_path(value)["rows"][0].__setitem__("approvalInferred", True), True),
            ("legal approval", lambda value: name_artifact_path(value).__setitem__("legalApproval", True), True),
            ("owner approval", lambda value: name_artifact_path(value).__setitem__("ownerApproval", True), True),
            ("gate closed", lambda value: name_artifact_path(value).__setitem__("manualGateClosed", True), True),
            ("host path nested", lambda value: name_artifact_path(value)["assetRegistryObservation"].__setitem__("path", r"C:\\secret\\AssetRegistry.bin"), True),
            ("unknown row field", lambda value: name_artifact_path(value)["rows"][0].__setitem__("reviewer", "blanket"), True),
        ]
        for name, mutate, refresh in record_mutations:
            reject_record(name, mutate, refresh)

        def expect_failure(name: str, operation: Callable[[], Any]) -> None:
            nonlocal mutation_count, rejected_mutations
            mutation_count += 1
            try:
                operation()
            except (EvidenceError, OSError, ValueError, KeyError, TypeError):
                rejected_mutations += 1
            else:
                failures.append(f"parser/path mutation escaped: {name}")

        expect_failure(
            "duplicate JSON key",
            lambda: load_json_bytes(b'{"a":1,"a":2}', "duplicate fixture"),
        )
        expect_failure(
            "non-finite JSON",
            lambda: load_json_bytes(b'{"a":NaN}', "NaN fixture"),
        )
        expect_failure(
            "INI duplicate key",
            lambda: ini_section("[x]\na=1\na=2\n", "x"),
        )
        expect_failure(
            "INI missing section",
            lambda: ini_section("[y]\na=1\n", "x"),
        )
        expect_failure(
            "course schema",
            lambda: public_name_fields({"schema": "bad"}, "course", "fixture"),
        )
        expect_failure(
            "hole number",
            lambda: public_name_fields({
                "schema": "disc_golf_hole_blockout", "schemaVersion": 1,
                "courseId": "C", "layoutId": "L", "holeNumber": 0,
                "holeName": "H",
            }, "hole", "fixture"),
        )
        expect_failure(
            "Pak summary",
            lambda: parse_pak_listing(
                'LogPakFile: Display: "../../../DiscGolfTour/A" offset: 0\n'
                'LogPakFile: Display: 2 files (x)\n'
            ),
        )
        expect_failure(
            "Pak duplicate",
            lambda: parse_pak_listing(
                'LogPakFile: Display: "../../../DiscGolfTour/A" offset: 0\n'
                'LogPakFile: Display: "../../../DiscGolfTour/a" offset: 1\n'
                'LogPakFile: Display: 2 files (x)\n'
            ),
        )
        valid_pak = parse_pak_listing(
            'LogPakFile: Display: "../../../DiscGolfTour/A" offset: 0\n'
            'LogPakFile: Display: 1 files (x)\n'
        )
        if valid_pak[0] != ["DiscGolfTour/A"]:
            failures.append("valid Pak parser fixture failed")

        equipment_rows = [row for row in rows if row["kind"] in {"mold", "plastic"}]
        csv_rows: list[dict[str, str]] = []
        for index, row in enumerate(equipment_rows):
            csv_rows.append({
                "OrderInContainer": str(index),
                "ChunkId": f"chunk{index}",
                "PackageId": f"package{index}",
                "Filename": "../../../" + expected_equipment_package(row),
                "ContainerName": "DiscGolfTour-Windows",
                "Offset": str(index),
                "Size": "10",
                "CompressedSize": "9",
                "Hash": "A" * 40,
                "ChunkType": "ExportBundleData",
                "Platform": "Windows",
            })
        validate_iostore_equipment(csv_rows, equipment_rows)
        expect_failure(
            "IoStore missing equipment",
            lambda: validate_iostore_equipment(csv_rows[:-1], equipment_rows),
        )
        duplicated_csv = copy.deepcopy(csv_rows)
        duplicated_csv[-1]["Filename"] = duplicated_csv[0]["Filename"]
        expect_failure(
            "IoStore duplicate equipment",
            lambda: validate_iostore_equipment(duplicated_csv, equipment_rows),
        )
        registry_fixture = (
            f"\t{MOLD_CLASS} : 5 item(s)\n"
            + "".join(
                f"\t\t{expected_equipment_object(row)}\n"
                for row in equipment_rows if row["kind"] == "mold"
            )
            + f"\t{PLASTIC_CLASS} : 3 item(s)\n"
            + "".join(
                f"\t\t{expected_equipment_object(row)}\n"
                for row in equipment_rows if row["kind"] == "plastic"
            )
            + "\t/Script/Engine.Other : 0 item(s)\n"
        )
        validate_asset_registry(registry_fixture, equipment_rows)
        expect_failure(
            "Asset Registry missing object",
            lambda: validate_asset_registry(
                registry_fixture.replace(" : 5 item(s)", " : 4 item(s)", 1),
                equipment_rows,
            ),
        )

        with tempfile.TemporaryDirectory(prefix="dgt_s19_name_selftest_") as temp_name:
            temp = Path(temp_name)
            immutable = temp / "immutable.json"
            write_create_new(immutable, {"a": 1})
            expect_failure(
                "create-new overwrite", lambda: write_create_new(immutable, {"a": 2})
            )
            boundary = temp / "boundary.bin"
            with boundary.open("wb") as handle:
                handle.write(b"X" * (8 * 1024 * 1024 - 2))
                handle.write(b"Apex")
            boundary_scan = scan_file_tokens(boundary, ["Apex"])
            if boundary_scan[0]["asciiCount"] != 1:
                failures.append("stream token boundary scan failed")
            nested = temp / "nested.bin"
            nested.write_bytes(b"DiscGolfTour")
            nested_scan = scan_file_tokens(nested, ["DiscGolfTour", "Tour"])
            if [item["asciiCount"] for item in nested_scan] != [1, 1]:
                failures.append("stream nested-token scan failed")

            cooked_root = temp / "cooked"
            extracted_iostore: dict[str, dict[str, Any]] = {}
            for row in equipment_rows:
                package = expected_equipment_package(row)
                cooked_relative = PurePosixPath(package).with_suffix("")
                cooked_base = cooked_root.joinpath(*cooked_relative.parts)
                cooked_base.parent.mkdir(parents=True, exist_ok=True)
                combined = (
                    row["stableId"] + "|" + cooked_base.name
                ).encode("ascii")
                split = max(1, len(combined) // 2)
                cooked_base.with_suffix(".uheader").write_bytes(combined[:split])
                cooked_base.with_suffix(".uexp").write_bytes(combined[split:])
                extracted_iostore[package] = {"size": len(combined)}
            validate_extracted_equipment(
                cooked_root, equipment_rows, extracted_iostore
            )
            missing_part = cooked_root.joinpath(
                *PurePosixPath(expected_equipment_package(equipment_rows[0]))
                .with_suffix(".uexp").parts
            )
            missing_part.unlink()
            expect_failure(
                "cooked equipment missing part",
                lambda: validate_extracted_equipment(
                    cooked_root, equipment_rows, extracted_iostore
                ),
            )

            candidate_root = temp / "S19_WindowsShipping_SELFTEST" / "Windows"
            candidate_root.mkdir(parents=True)
            for relative in REQUIRED_ARCHIVE_FILES:
                path = candidate_root.joinpath(*PurePosixPath(relative).parts)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(relative.encode("utf-8"))
            archive_summary, archive_by_path = collect_archive(
                candidate_root, "S19_WindowsShipping_SELFTEST"
            )
            expect_failure(
                "archive wrong candidate",
                lambda: collect_archive(candidate_root, "S19_WindowsShipping_OTHER"),
            )

            fixture_root = temp / "repo"
            receipt_relative = (
                "Evidence/Session19/ShippingCandidateVerification-"
                "S19_WindowsShipping_SELFTEST.json"
            )
            receipt_path = fixture_root.joinpath(
                *PurePosixPath(receipt_relative).parts
            )
            receipt_path.parent.mkdir(parents=True)
            live_inner = archive_by_path[INNER_EXE]
            receipt_fixture = {
                "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
                "schemaVersion": 2,
                "session": SESSION,
                "runId": "S19_WindowsShipping_SELFTEST",
                "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
                "verificationMode": "DIRECT_BUILD_PASS",
                "receiptPath": receipt_relative,
                "archiveContract": {"allRequiredFilesPresent": True},
                "releaseExcludedProjectRootsAbsent": True,
                "shippingPluginCapabilities": {
                    "technicalGatePass": True,
                    "finalArchiveCanonicalManifestSha256": archive_summary[
                        "canonicalManifestSha256"
                    ],
                },
                "innerShippingExecutable": {
                    "relativePath": INNER_EXE,
                    "bytes": live_inner["bytes"],
                    "sha256": live_inner["sha256"],
                },
            }
            original_root = ROOT
            ROOT = fixture_root
            try:
                receipt_path.write_bytes(rendered_json_bytes(receipt_fixture))
                verification_binding(
                    receipt_path,
                    "S19_WindowsShipping_SELFTEST",
                    archive_summary,
                    archive_by_path,
                )

                def mutated_verification(
                    mutate: Callable[[dict[str, Any]], None]
                ) -> dict[str, Any]:
                    mutated = copy.deepcopy(receipt_fixture)
                    mutate(mutated)
                    receipt_path.write_bytes(rendered_json_bytes(mutated))
                    return verification_binding(
                        receipt_path,
                        "S19_WindowsShipping_SELFTEST",
                        archive_summary,
                        archive_by_path,
                    )

                expect_failure(
                    "verification wrong candidate",
                    lambda: mutated_verification(
                        lambda value: value.__setitem__("runId", "S19_WindowsShipping_OTHER")
                    ),
                )
                expect_failure(
                    "verification random archive hash",
                    lambda: mutated_verification(
                        lambda value: value["shippingPluginCapabilities"].__setitem__(
                            "finalArchiveCanonicalManifestSha256", "A" * 64
                        )
                    ),
                )
                expect_failure(
                    "verification wrong executable hash",
                    lambda: mutated_verification(
                        lambda value: value["innerShippingExecutable"].__setitem__(
                            "sha256", "B" * 64
                        )
                    ),
                )
                expect_failure(
                    "verification self path",
                    lambda: mutated_verification(
                        lambda value: value.__setitem__("receiptPath", "Evidence/bad.json")
                    ),
                )
                other_receipt = fixture_root / "Evidence/Session19/wrong.json"
                other_receipt.write_bytes(rendered_json_bytes(receipt_fixture))
                expect_failure(
                    "verification input path",
                    lambda: verification_binding(
                        other_receipt,
                        "S19_WindowsShipping_SELFTEST",
                        archive_summary,
                        archive_by_path,
                    ),
                )
            finally:
                ROOT = original_root

            tool_root = temp / "UE_A/Engine/Binaries/Win64"
            other_tool_root = temp / "UE_B/Engine/Binaries/Win64"
            tool_root.mkdir(parents=True)
            other_tool_root.mkdir(parents=True)
            fixture_unrealpak = tool_root / "UnrealPak.exe"
            fixture_editor = tool_root / "UnrealEditor-Cmd.exe"
            other_editor = other_tool_root / "UnrealEditor-Cmd.exe"
            fixture_unrealpak.write_bytes(b"pak")
            fixture_editor.write_bytes(b"editor")
            other_editor.write_bytes(b"other")
            validate_tools(fixture_unrealpak, fixture_editor)
            expect_failure(
                "mismatched Unreal tool roots",
                lambda: validate_tools(fixture_unrealpak, other_editor),
            )
            (candidate_root / "DiscGolfTour/Data/PineRidgeHole3.json").unlink()
            expect_failure(
                "archive missing required input",
                lambda: collect_archive(candidate_root, "S19_WindowsShipping_SELFTEST"),
            )
            link = temp / "reparse-link"
            try:
                link.symlink_to(immutable)
            except OSError:
                optional_reparse_test = "SKIPPED_HOST_DENIED_SYMLINK_CREATION"
            else:
                expect_failure("reparse input", lambda: exact_file(link, "fixture"))
                optional_reparse_test = "PASS"
    except Exception as exc:
        failures.append(f"self-test harness error: {exc}")

    result = {
        "status": "PASS" if not failures else "FAIL",
        "schema": SCHEMA,
        "mutationCount": mutation_count,
        "rejectedMutations": rejected_mutations,
        "optionalReparseTest": optional_reparse_test,
        "failures": failures,
    }
    print(json.dumps(result, indent=2))
    return 0 if not failures and mutation_count == rejected_mutations else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--generate", action="store_true",
        help="create a new candidate-bound technical inventory record",
    )
    mode.add_argument(
        "--validate-record", type=Path, metavar="PATH",
        help="rebuild live observations and validate an existing exact-candidate record",
    )
    mode.add_argument(
        "--self-test", action="store_true",
        help="run synthetic/adversarial tests without emitting candidate evidence",
    )
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--shipping-verification", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--unreal-editor-cmd", type=Path)
    parser.add_argument(
        "--output", type=Path,
        help="must match the candidate-specific create-new Evidence/Session19 path",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        return run_self_test()
    missing = [
        option for option, value in (
            ("--candidate-id", args.candidate_id),
            ("--archive", args.archive),
            ("--shipping-verification", args.shipping_verification),
            ("--unrealpak", args.unrealpak),
            ("--unreal-editor-cmd", args.unreal_editor_cmd),
        ) if value is None
    ]
    if missing:
        print(json.dumps({
            "status": "FAIL", "releaseReady": False,
            "errors": [f"required arguments missing: {', '.join(missing)}"],
        }, indent=2))
        return 1
    try:
        candidate_id = str(args.candidate_id)
        if CANDIDATE_RE.fullmatch(candidate_id) is None:
            raise EvidenceError("--candidate-id has an invalid Session 19 Shipping identity")
        policy_data = read_stable(POLICY_PATH)
        policy = load_json_bytes(policy_data, "manual release review policy")
        _title, policy_rows = validate_manual_policy(policy)
        policy_sha = sha256_bytes(policy_data)
        script_sha = hash_stable_file(SCRIPT_PATH)[1]
        if args.validate_record is not None:
            if args.output is not None:
                raise EvidenceError("--output cannot be used with --validate-record")
            record_path = validate_exact_record_path(
                args.validate_record, candidate_id, "--validate-record"
            )
            record = load_json(record_path)
            structural = validate_record_structure(
                record, _title, policy_rows, policy_sha, script_sha
            )
            if structural:
                raise EvidenceError("; ".join(structural))
            if record.get("candidateId") != candidate_id:
                raise EvidenceError("record candidate ID differs from --candidate-id")
            expected = audit_live_candidate(
                candidate_id,
                args.archive,
                args.shipping_verification,
                args.unrealpak,
                args.unreal_editor_cmd,
                record["generatedUtc"],
            )
            difference = first_difference(record, expected)
            if difference:
                raise EvidenceError(
                    "record differs from fresh live candidate reconstruction: " + difference
                )
            print(json.dumps({
                "status": "PASS_CANDIDATE_BOUND_TITLE_AND_RUNTIME_NAME_TECHNICAL_INVENTORY_VERIFIED_CLEARANCE_PENDING",
                "candidateId": candidate_id,
                "recordPath": record["recordPath"],
                "recordSha256": hash_stable_file(record_path)[1],
                "artifactIdsAndHashes": record["artifactIdsAndHashes"],
                "legalApproval": False,
                "productOwnerApproval": False,
                "manualGateClosed": False,
                "releaseReady": False,
            }, indent=2))
            return 0

        output = validate_exact_record_path(
            args.output if args.output is not None else expected_record_path(candidate_id),
            candidate_id,
            "--output",
        )
        record = audit_live_candidate(
            candidate_id,
            args.archive,
            args.shipping_verification,
            args.unrealpak,
            args.unreal_editor_cmd,
            utc_now(),
        )
        write_create_new(output, record)
        written = load_json(output)
        if written != record:
            raise EvidenceError("create-new record verification differs after write")
        print(json.dumps({
            "status": PASS_STATE,
            "candidateId": candidate_id,
            "recordPath": record["recordPath"],
            "recordSha256": hash_stable_file(output)[1],
            "artifactIdsAndHashes": record["artifactIdsAndHashes"],
            "legalApproval": False,
            "productOwnerApproval": False,
            "manualGateClosed": False,
            "releaseReady": False,
        }, indent=2))
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as exc:
        print(json.dumps({
            "status": "FAIL_CANDIDATE_BOUND_TITLE_AND_RUNTIME_NAME_INVENTORY_NOT_ACCEPTED",
            "legalApproval": False,
            "productOwnerApproval": False,
            "manualGateClosed": False,
            "releaseReady": False,
            "errors": [str(exc)],
        }, indent=2))
        return 1


if __name__ == "__main__":
    sys.exit(main())
