#!/usr/bin/env python3
"""Validate and optionally receipt one exact three-hole Shipping performance run.

The schema-v2 game reports describe rendered performance, but do not identify the
executable or immutable archive that emitted them. This validator adds that
external binding without treating power, wall-power, driver, or thermal facts as
captured evidence.
"""

from __future__ import annotations

import argparse
from contextlib import redirect_stdout
from datetime import datetime, timedelta, timezone
import hashlib
from io import StringIO
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
from typing import Any, Iterable, Optional, Sequence
import uuid

from validate_performance_capture import (
    matches_performance_budget_contract,
    validate as validate_schema_v2_report,
)


ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_MANIFEST_SCHEMA = "DiscGolfTour.Session19ShippingPerformanceArchiveManifest.v1"
RUN_MANIFEST_SCHEMA = "DiscGolfTour.Session19ShippingPerformanceRunManifest.v1"
LAUNCH_SCHEMA = "DiscGolfTour.Session19ShippingPerformanceLaunch.v1"
PROCESS_LOG_SCHEMA = "DiscGolfTour.Session19ShippingPerformanceProcessLog.v1"
RECEIPT_SCHEMA = "DiscGolfTour.Session19ShippingPerformanceEvidence.v1"
PASS_STATE = "PASS_CANDIDATE_BOUND_THREE_HOLE_RENDERED_PERFORMANCE"
FAIL_STATE = "FAIL_CLOSED"
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_\d{8}T\d{6}Z_[0-9a-f]{12}$")
UPPER_SHA_RE = re.compile(r"^[0-9A-F]{64}$")
LAUNCHER_RELATIVE = "DiscGolfTour.exe"
EXECUTABLE_RELATIVE = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"


class EvidenceError(RuntimeError):
    pass


class DuplicateKeyError(ValueError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def parse_utc(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise EvidenceError(f"{label} is not a UTC timestamp")
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        raise EvidenceError(f"{label} is not a UTC timestamp") from None
    return parsed


def validate_run_chronology(
    archive_generated: datetime,
    run_rows: Sequence[dict[str, Any]],
    run_manifest_generated: datetime,
    verified: datetime,
) -> None:
    if [row.get("holeNumber") for row in run_rows] != [1, 2, 3]:
        raise EvidenceError("launch chronology is not in exact hole order")
    timeline = [archive_generated]
    for row in run_rows:
        timeline.extend((
            parse_utc(row.get("startedUtc"), "launch startedUtc"),
            parse_utc(row.get("finishedUtc"), "launch finishedUtc"),
        ))
    timeline.append(run_manifest_generated)
    if any(later < earlier for earlier, later in zip(timeline, timeline[1:])):
        raise EvidenceError(
            "archive/run manifest chronology contradicts sequential launch windows"
        )
    tolerance = timedelta(seconds=2)
    if run_manifest_generated > verified + tolerance \
            or verified > datetime.now(timezone.utc) + tolerance:
        raise EvidenceError("evidence chronology extends beyond validation time")


def reject_duplicate_keys(pairs: Sequence[tuple[str, Any]]) -> dict[str, Any]:
    output: dict[str, Any] = {}
    for key, value in pairs:
        if key in output:
            raise DuplicateKeyError(key)
        output[key] = value
    return output


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, DuplicateKeyError) as exc:
        raise EvidenceError(f"invalid JSON artifact: {path.name} ({exc.__class__.__name__})") from None
    if type(value) is not dict:
        raise EvidenceError(f"JSON artifact is not an object: {path.name}")
    return value


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def write_exclusive_json(path: Path, value: Any) -> None:
    payload = canonical_json_bytes(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError:
        raise EvidenceError(f"refusing to overwrite existing output: {path.name}") from None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
    except OSError as exc:
        raise EvidenceError(f"could not hash {path.name} ({exc.__class__.__name__})") from None
    return digest.hexdigest().upper()


def file_binding(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise EvidenceError(f"bound file is missing: {path.name}")
    return {"fileName": path.name, "bytes": path.stat().st_size, "sha256": sha256_file(path)}


def is_reparse_point(path: Path) -> bool:
    """Return True for symlinks and Windows junction/reparse-point entries."""
    if path.is_symlink():
        return True
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError as exc:
        raise EvidenceError(
            f"could not inspect path integrity: {path.name} ({exc.__class__.__name__})"
        ) from None
    return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def is_same_or_under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, RuntimeError, ValueError):
        return False


def validate_candidate_archive_path(archive: Path, candidate_id: str) -> Path:
    if not CANDIDATE_RE.fullmatch(candidate_id):
        raise EvidenceError("candidate ID is invalid")
    try:
        resolved = archive.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EvidenceError(
            f"candidate archive path cannot be resolved ({exc.__class__.__name__})"
        ) from None
    if (not resolved.is_dir() or resolved.name != "Windows"
            or resolved.parent.name != candidate_id):
        raise EvidenceError(
            "candidate archive must be the exact <candidateId>/Windows directory"
        )
    return resolved


def validate_receipt_output_path(
    output: Path, archive: Path, evidence_root: Path,
    run_root: Optional[Path] = None,
) -> Path:
    """Keep append-only receipts inside the selected external evidence root."""
    resolved_output = output.resolve()
    resolved_archive = archive.resolve()
    resolved_evidence_root = evidence_root.resolve()
    if (is_same_or_under(resolved_evidence_root, resolved_archive)
            or is_same_or_under(resolved_evidence_root, ROOT)
            or not is_same_or_under(resolved_output, resolved_evidence_root)
            or is_same_or_under(resolved_output, resolved_archive)
            or is_same_or_under(resolved_output, ROOT)
            or (run_root is not None
                and is_same_or_under(resolved_output, run_root.resolve()))):
        raise EvidenceError(
            "receipt output must be inside the selected external evidence root "
            "and outside the archive/project/run root"
        )
    return resolved_output


def iter_regular_files_without_reparse(root: Path, label: str) -> list[Path]:
    if not root.is_dir() or is_reparse_point(root):
        raise EvidenceError(f"{label} root is missing or is a reparse point")
    files: list[Path] = []

    def fail_walk(exc: OSError) -> None:
        raise EvidenceError(
            f"could not enumerate {label} ({exc.__class__.__name__})"
        ) from None

    for directory, directory_names, file_names in os.walk(
            root, topdown=True, onerror=fail_walk, followlinks=False):
        directory_path = Path(directory)
        for name in directory_names:
            child = directory_path / name
            if is_reparse_point(child):
                raise EvidenceError(f"{label} contains a reparse point: {name}")
        for name in file_names:
            child = directory_path / name
            if is_reparse_point(child) or not child.is_file():
                raise EvidenceError(f"{label} contains a non-regular file: {name}")
            files.append(child)
    return files


def validate_exact_directory_entries(
    directory: Path, expected_files: set[str], expected_directories: set[str], label: str,
) -> None:
    if not directory.is_dir() or is_reparse_point(directory):
        raise EvidenceError(f"{label} directory is missing or redirected")
    entries = list(directory.iterdir())
    files = {entry.name for entry in entries if entry.is_file() and not is_reparse_point(entry)}
    directories = {
        entry.name for entry in entries if entry.is_dir() and not is_reparse_point(entry)
    }
    if (files != expected_files or directories != expected_directories
            or len(entries) != len(files) + len(directories)):
        raise EvidenceError(f"{label} contains unbound or missing evidence entries")


def canonical_uuid4(value: Any) -> bool:
    if not isinstance(value, str) or value.lower() != value:
        return False
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError):
        return False
    return str(parsed) == value and parsed.version == 4 and parsed.variant == uuid.RFC_4122


def required_arguments(hole: int, token: str, sanitized: bool) -> list[str]:
    user_dir = f"<EXTERNAL_UUID:{token}>" if sanitized else token
    return [
        "-Course=PineRidge",
        f"-Hole={hole}",
        "-PerformanceCaptureSeconds=30",
        "-ResX=1920",
        "-ResY=1080",
        "-ForceRes",
        "-RenderOffscreen",
        "-dx12",
        "-unattended",
        "-NoLoadExistingSave",
        "-DGNoProfileWrites",
        f"-UserDir={user_dir}",
    ]


def archive_inventory(archive: Path) -> tuple[list[dict[str, Any]], str, int]:
    if not archive.is_dir():
        raise EvidenceError("archive directory is missing")
    rows: list[dict[str, Any]] = []
    total = 0
    for path in sorted(iter_regular_files_without_reparse(archive, "archive"),
                       key=lambda item: item.relative_to(archive).as_posix().casefold()):
        relative = path.relative_to(archive).as_posix()
        size = path.stat().st_size
        digest = sha256_file(path)
        rows.append({"path": relative, "bytes": size, "sha256": digest})
        total += size
    if not rows:
        raise EvidenceError("archive contains no files")
    canonical_rows = canonical_json_bytes(rows)
    return rows, hashlib.sha256(canonical_rows).hexdigest().upper(), total


def build_archive_manifest(archive: Path, candidate_id: str) -> dict[str, Any]:
    archive = validate_candidate_archive_path(archive, candidate_id)
    rows, inventory_sha, total = archive_inventory(archive)
    launcher = archive / LAUNCHER_RELATIVE
    executable = archive / EXECUTABLE_RELATIVE
    return {
        "schema": ARCHIVE_MANIFEST_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "generatedUtc": utc_now(),
        "inventorySha256": inventory_sha,
        "fileCount": len(rows),
        "totalBytes": total,
        "launcher": {"relativePath": LAUNCHER_RELATIVE, **file_binding(launcher)},
        "shippingExecutable": {"relativePath": EXECUTABLE_RELATIVE, **file_binding(executable)},
        "files": rows,
    }


def require_keys(value: Any, expected: Iterable[str], label: str) -> None:
    if type(value) is not dict or set(value) != set(expected):
        raise EvidenceError(f"{label} keys differ from the evidence schema")


def validate_archive_manifest(manifest: dict[str, Any], archive: Path, candidate_id: str) -> None:
    archive = validate_candidate_archive_path(archive, candidate_id)
    require_keys(manifest, {
        "schema", "schemaVersion", "session", "candidateId", "generatedUtc",
        "inventorySha256", "fileCount", "totalBytes", "launcher",
        "shippingExecutable", "files",
    }, "archive manifest")
    if (manifest["schema"] != ARCHIVE_MANIFEST_SCHEMA
            or type(manifest["schemaVersion"]) is not int or manifest["schemaVersion"] != 1
            or type(manifest["session"]) is not int or manifest["session"] != 19
            or manifest["candidateId"] != candidate_id):
        raise EvidenceError("archive manifest identity differs")
    parse_utc(manifest["generatedUtc"], "archive manifest generatedUtc")
    rows, inventory_sha, total = archive_inventory(archive)
    if (manifest["files"] != rows or manifest["inventorySha256"] != inventory_sha
            or manifest["fileCount"] != len(rows) or manifest["totalBytes"] != total):
        raise EvidenceError("live archive differs from its pre-run canonical inventory")
    for label, relative, path in (
        ("launcher", LAUNCHER_RELATIVE, archive / LAUNCHER_RELATIVE),
        ("shippingExecutable", EXECUTABLE_RELATIVE, archive / EXECUTABLE_RELATIVE),
    ):
        expected = {"relativePath": relative, **file_binding(path)}
        if manifest[label] != expected:
            raise EvidenceError(f"archive {label} binding differs")


def validate_file_binding(
    binding: Any, path: Path, label: str, container_root: Path,
) -> None:
    require_keys(binding, {"relativePath", "fileName", "bytes", "sha256"}, label)
    relative = Path(binding["relativePath"]) if isinstance(binding["relativePath"], str) else None
    if relative is None or relative.is_absolute():
        raise EvidenceError(f"{label} relative path is invalid")
    if ".." in relative.parts:
        raise EvidenceError(f"{label} relative path escapes the run root")
    try:
        resolved_root = container_root.resolve(strict=True)
        resolved_path = path.resolve(strict=True)
        resolved_path.relative_to(resolved_root)
    except (OSError, RuntimeError, ValueError):
        raise EvidenceError(f"{label} resolved path escapes the run root") from None
    if is_reparse_point(container_root):
        raise EvidenceError(f"{label} run root is a reparse point")
    current = container_root
    for part in relative.parts:
        current /= part
        if is_reparse_point(current):
            raise EvidenceError(f"{label} path contains a reparse point")
    if binding["fileName"] != path.name or not path.is_file() \
            or binding["bytes"] != path.stat().st_size \
            or binding["sha256"] != sha256_file(path):
        raise EvidenceError(f"{label} hash/size binding differs")


def validate_shipping_report(
    report_path: Path, expected_hole: int, started: datetime, finished: datetime,
) -> dict[str, Any]:
    report = load_json(report_path)
    report_stdout = StringIO()
    try:
        with redirect_stdout(report_stdout):
            validate_schema_v2_report(report_path)
    except (AssertionError, OSError, ValueError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"schema-v2 performance report failed validation: {exc}") from None
    if (report.get("result") != "PASS"
            or report.get("course_id") != "PineRidgeChampionship"
            or type(report.get("hole_number")) is not int
            or report["hole_number"] != expected_hole
            or report.get("requested_duration_seconds") != 30
            or report.get("runtime_mode") != "packaged"
            or report.get("rendered") is not True
            or not matches_performance_budget_contract(report.get("budget"))
            or not isinstance(report.get("rhi"), str)
            or report["rhi"].casefold() != "d3d12"):
        raise EvidenceError("schema-v2 report differs from the Shipping D3D12 hole contract")
    captured = parse_utc(report.get("captured_utc"), "performance report captured_utc")
    if captured < started or captured > finished:
        raise EvidenceError("performance report timestamp is outside the process window")
    return report


def validate_launch_record(
    record: dict[str, Any], run_root: Path, candidate_id: str,
    expected_run_id: str, archive_inventory_sha: str,
    executable_sha: str, expected_hole: int,
) -> dict[str, Any]:
    require_keys(record, {
        "schema", "schemaVersion", "session", "candidateId", "runId",
        "holeNumber", "userDirToken", "sanitizedArguments", "archiveInventorySha256",
        "shippingExecutableSha256", "startedUtc", "finishedUtc", "elapsedSeconds",
        "pid", "exitCode", "processLog", "performanceReport", "latestReport",
        "captureGuard",
    }, "launch record")
    if (record["schema"] != LAUNCH_SCHEMA
            or type(record["schemaVersion"]) is not int or record["schemaVersion"] != 1
            or type(record["session"]) is not int or record["session"] != 19
            or record["candidateId"] != candidate_id
            or record["runId"] != expected_run_id
            or type(record["holeNumber"]) is not int
            or record["holeNumber"] != expected_hole):
        raise EvidenceError("launch record identity differs")
    token = record["userDirToken"]
    if not canonical_uuid4(token):
        raise EvidenceError("launch UserDir token is not a canonical UUIDv4")
    if record["sanitizedArguments"] != required_arguments(expected_hole, token, True):
        raise EvidenceError("launch arguments differ from the exact guarded command")
    if record["archiveInventorySha256"] != archive_inventory_sha \
            or record["shippingExecutableSha256"] != executable_sha:
        raise EvidenceError("launch record candidate binding differs")
    started = parse_utc(record["startedUtc"], "launch startedUtc")
    finished = parse_utc(record["finishedUtc"], "launch finishedUtc")
    elapsed = (finished - started).total_seconds()
    if (type(record["elapsedSeconds"]) not in (int, float)
            or abs(float(record["elapsedSeconds"]) - elapsed) > 0.05 or elapsed < 39.0):
        raise EvidenceError("launch elapsed time cannot contain a 10s warm-up and 30s capture")
    if (type(record["pid"]) is not int or record["pid"] < 1
            or type(record["exitCode"]) is not int or record["exitCode"] != 0):
        raise EvidenceError("Shipping capture process did not exit successfully")

    user_dir = run_root / "UserDirs" / token
    if not user_dir.is_dir() or (user_dir / "Saved" / "SaveGames").exists():
        raise EvidenceError("capture UserDir is missing or contains forbidden save state")
    resolved: dict[str, Path] = {}
    for field, label in (
        ("processLog", "process log"),
        ("performanceReport", "performance report"),
        ("latestReport", "latest performance report"),
        ("captureGuard", "release capture guard"),
    ):
        binding = record[field]
        if type(binding) is not dict or not isinstance(binding.get("relativePath"), str):
            raise EvidenceError(f"{label} binding is invalid")
        path = run_root / binding["relativePath"]
        validate_file_binding(binding, path, label, run_root)
        resolved[field] = path

    guard = user_dir / "Saved" / "PerformanceCaptures" / "ReleasePerformanceCapture.guard"
    if (resolved["captureGuard"].resolve() != guard.resolve()
            or resolved["captureGuard"].stat().st_size != 0):
        raise EvidenceError("exclusive release-capture namespace guard binding differs")

    process_log = load_json(resolved["processLog"])
    require_keys(process_log, {
        "schema", "schemaVersion", "candidateId", "runId", "holeNumber",
        "userDirToken", "startedUtc", "finishedUtc", "elapsedSeconds", "pid",
        "exitCode", "stdout", "stderr",
    }, "process log")
    if (process_log["schema"] != PROCESS_LOG_SCHEMA
            or type(process_log["schemaVersion"]) is not int
            or process_log["schemaVersion"] != 1
            or process_log["candidateId"] != candidate_id
            or process_log["runId"] != record["runId"]
            or type(process_log["holeNumber"]) is not int
            or process_log["holeNumber"] != expected_hole
            or process_log["userDirToken"] != token
            or type(process_log["pid"]) is not int
            or type(process_log["exitCode"]) is not int
            or process_log["exitCode"] != 0):
        raise EvidenceError("process log identity/result differs")
    for field in ("startedUtc", "finishedUtc", "elapsedSeconds", "pid", "exitCode"):
        if process_log[field] != record[field]:
            raise EvidenceError("process log chronology/process binding differs")
    if not isinstance(process_log["stdout"], str) or not isinstance(process_log["stderr"], str):
        raise EvidenceError("process log output fields are not text")

    if sha256_file(resolved["performanceReport"]) != sha256_file(resolved["latestReport"]):
        raise EvidenceError("timestamped and Latest schema-v2 reports differ")
    report = validate_shipping_report(
        resolved["performanceReport"], expected_hole, started, finished
    )
    timestamped = list((user_dir / "Saved" / "PerformanceCaptures").glob("Performance_*.json"))
    if len(timestamped) != 1 or timestamped[0].resolve() != resolved["performanceReport"].resolve():
        raise EvidenceError("capture UserDir does not contain exactly one timestamped report")
    expected_latest = user_dir / "Saved" / "PerformanceCaptures" / "LatestPerformance.json"
    if resolved["latestReport"].resolve() != expected_latest.resolve():
        raise EvidenceError("Latest report binding is outside its capture UserDir")

    return {
        "holeNumber": expected_hole,
        "userDirToken": token,
        "startedUtc": record["startedUtc"],
        "finishedUtc": record["finishedUtc"],
        "elapsedSeconds": record["elapsedSeconds"],
        "processLog": record["processLog"],
        "performanceReport": record["performanceReport"],
        "latestReport": record["latestReport"],
        "captureGuard": record["captureGuard"],
        "summary": report["summary"],
        "rhi": report["rhi"],
        "gpuBrand": report["gpu_brand"],
    }


def validate_run(run_root: Path, archive: Path, candidate_id: str) -> dict[str, Any]:
    errors: list[str] = []
    receipt: dict[str, Any] = {
        "schema": RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "runId": "",
        "verifiedUtc": utc_now(),
        "state": FAIL_STATE,
        "passed": False,
        "archive": {},
        "runManifest": {},
        "tooling": {
            "runner": file_binding(ROOT / "Scripts" / "run_dg_session19_shipping_performance.py"),
            "validator": file_binding(Path(__file__).resolve()),
            "schemaV2Validator": file_binding(ROOT / "Scripts" / "validate_performance_capture.py"),
        },
        "runs": [],
        "errors": errors,
        "claimBoundary": {
            "candidateBoundRenderedPerformance": False,
            "gpuThreadProfiling": False,
            "thermalSoak": False,
            "powerModeRecorded": False,
            "wallPowerRecorded": False,
            "gpuDriverRecorded": False,
            "releaseReady": False,
        },
    }
    try:
        verified = parse_utc(receipt["verifiedUtc"], "receipt verifiedUtc")
        if not CANDIDATE_RE.fullmatch(candidate_id):
            raise EvidenceError("candidate ID is invalid")
        validate_candidate_archive_path(archive, candidate_id)
        iter_regular_files_without_reparse(run_root, "performance evidence run")
        manifest_path = run_root / "ArchiveManifest.json"
        run_manifest_path = run_root / "RunManifest.json"
        archive_manifest = load_json(manifest_path)
        validate_archive_manifest(archive_manifest, archive, candidate_id)
        run_manifest = load_json(run_manifest_path)
        require_keys(run_manifest, {
            "schema", "schemaVersion", "session", "candidateId", "runId",
            "generatedUtc", "archiveManifest", "archiveBeforeInventorySha256",
            "archiveAfterInventorySha256", "archivePreserved", "launcher",
            "shippingExecutable", "launchRecords",
        }, "run manifest")
        if (run_manifest["schema"] != RUN_MANIFEST_SCHEMA
                or type(run_manifest["schemaVersion"]) is not int
                or run_manifest["schemaVersion"] != 1
                or type(run_manifest["session"]) is not int
                or run_manifest["session"] != 19
                or run_manifest["candidateId"] != candidate_id
                or not canonical_uuid4(run_manifest["runId"])):
            raise EvidenceError("run manifest identity differs")
        run_manifest_generated = parse_utc(
            run_manifest["generatedUtc"], "run manifest generatedUtc"
        )
        if (run_root.name != run_manifest["runId"]
                or run_root.parent.name != candidate_id
                or run_root.parent.parent.name != "ShippingPerformance"):
            raise EvidenceError(
                "run root must be the exact ShippingPerformance/<candidateId>/<runId> directory"
            )
        manifest_binding = {"relativePath": "ArchiveManifest.json", **file_binding(manifest_path)}
        if run_manifest["archiveManifest"] != manifest_binding:
            raise EvidenceError("run manifest does not hash-bind ArchiveManifest.json")
        validate_file_binding(
            run_manifest["archiveManifest"], manifest_path, "archive manifest", run_root
        )
        inventory_sha = archive_manifest["inventorySha256"]
        if (run_manifest["archiveBeforeInventorySha256"] != inventory_sha
                or run_manifest["archiveAfterInventorySha256"] != inventory_sha
                or run_manifest["archivePreserved"] is not True):
            raise EvidenceError("archive preservation binding differs")
        expected_launcher = {
            "relativePath": LAUNCHER_RELATIVE,
            **file_binding(archive / LAUNCHER_RELATIVE),
        }
        expected_executable = {
            "relativePath": EXECUTABLE_RELATIVE,
            **file_binding(archive / EXECUTABLE_RELATIVE),
        }
        if run_manifest["launcher"] != expected_launcher \
                or run_manifest["shippingExecutable"] != expected_executable:
            raise EvidenceError("run manifest executable binding differs")
        records = run_manifest["launchRecords"]
        if type(records) is not list or len(records) != 3:
            raise EvidenceError("run manifest must bind exactly three launch records")
        if [binding.get("holeNumber") if type(binding) is dict else None
                for binding in records] != [1, 2, 3]:
            raise EvidenceError("run manifest launch records are not in exact hole order")
        validate_exact_directory_entries(
            run_root / "LaunchRecords",
            {"Hole1.json", "Hole2.json", "Hole3.json"}, set(), "launch records",
        )
        validate_exact_directory_entries(
            run_root / "ProcessLogs",
            {"Hole1.json", "Hole2.json", "Hole3.json"}, set(), "process logs",
        )
        seen_tokens: set[str] = set()
        seen_holes: set[int] = set()
        run_rows: list[dict[str, Any]] = []
        for binding in records:
            if type(binding) is not dict or type(binding.get("holeNumber")) is not int:
                raise EvidenceError("launch-record binding is malformed")
            hole = binding["holeNumber"]
            if hole not in (1, 2, 3) or hole in seen_holes:
                raise EvidenceError("launch-record hole coverage is duplicate or invalid")
            record_binding = binding.get("record")
            if type(record_binding) is not dict or not isinstance(record_binding.get("relativePath"), str):
                raise EvidenceError("launch-record file binding is malformed")
            record_path = run_root / record_binding["relativePath"]
            validate_file_binding(record_binding, record_path, "launch record", run_root)
            record = load_json(record_path)
            row = validate_launch_record(
                record, run_root, candidate_id, run_manifest["runId"], inventory_sha,
                expected_executable["sha256"], hole,
            )
            if row["userDirToken"] in seen_tokens:
                raise EvidenceError("capture UserDir UUID was reused")
            seen_tokens.add(row["userDirToken"])
            seen_holes.add(hole)
            row["launchRecord"] = record_binding
            run_rows.append(row)
        if seen_holes != {1, 2, 3} or len(seen_tokens) != 3:
            raise EvidenceError("three unique hole captures are required")
        validate_exact_directory_entries(
            run_root / "UserDirs", set(), seen_tokens, "capture UserDirs"
        )
        archive_manifest_generated = parse_utc(
            archive_manifest["generatedUtc"], "archive manifest generatedUtc"
        )
        validate_run_chronology(
            archive_manifest_generated, run_rows, run_manifest_generated, verified
        )
        receipt["archive"] = {
            "manifest": manifest_binding,
            "inventorySha256": inventory_sha,
            "fileCount": archive_manifest["fileCount"],
            "totalBytes": archive_manifest["totalBytes"],
            "launcher": expected_launcher,
            "shippingExecutable": expected_executable,
            "preservedBeforeAndAfter": True,
        }
        receipt["runId"] = run_manifest["runId"]
        receipt["runManifest"] = {
            "relativePath": "RunManifest.json",
            **file_binding(run_manifest_path),
        }
        receipt["runs"] = sorted(run_rows, key=lambda row: row["holeNumber"])
        receipt["claimBoundary"]["candidateBoundRenderedPerformance"] = True
        receipt["state"] = PASS_STATE
        receipt["passed"] = True
    except EvidenceError as exc:
        errors.append(str(exc))
    return receipt


def synthetic_report(hole: int, captured_utc: str) -> dict[str, Any]:
    return {
        "schema": "disc_golf_performance_capture", "version": 2,
        "captured_utc": captured_utc, "result": "PASS",
        "course_id": "PineRidgeChampionship", "hole_number": hole,
        "capture_profile": "OmenGameplay1080pHighFoliageV1",
        "camera_route": "authored_flyover_continuous", "warmup_seconds": 10,
        "requested_duration_seconds": 30, "rendered": True, "rhi": "D3D12",
        "gpu_brand": "Synthetic Test Adapter", "resolution_x": 1920,
        "resolution_y": 1080, "runtime_mode": "packaged",
        "quality": {
            "resolution_quality": 100, "view_distance": 2, "anti_aliasing": 2,
            "shadow": 2, "global_illumination": 2, "reflection": 2,
            "post_process": 2, "texture": 2, "effects": 2, "foliage": 3,
            "shading": 2,
        },
        "budget": {
            "target_frame_ms": 16.670000076293945, "warning_p95_frame_ms": 22,
            "fail_p95_frame_ms": 33.340000152587891, "hitch_frame_ms": 50,
            "max_hitch_rate_percent": 1, "warning_used_physical_bytes": 3758096384,
            "max_used_physical_bytes": 4294967296, "minimum_sample_count": 120,
            "window_sample_count": 600,
        },
        "summary": {
            "sample_count": 600, "average_frame_ms": 10, "p95_frame_ms": 12,
            "max_frame_ms": 14, "average_fps": 100, "hitch_count": 0,
            "hitch_rate_percent": 0, "used_physical_bytes": 1073741824,
        },
    }


def run_self_test() -> int:
    checks = 0
    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    self_test_parent = Path(os.environ["USERPROFILE"]) if os.name == "nt" else None
    with tempfile.TemporaryDirectory(
            prefix="dg-s19p-", dir=self_test_parent) as temporary:
        root = Path(temporary)
        candidate_id = "S19_WindowsShipping_20990101T000000Z_abcdef123456"
        archive = root / candidate_id / "Windows"
        (archive / Path(EXECUTABLE_RELATIVE).parent).mkdir(parents=True)
        (archive / LAUNCHER_RELATIVE).write_bytes(b"MZ launcher")
        (archive / EXECUTABLE_RELATIVE).write_bytes(b"MZ shipping executable")
        run_id = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
        evidence_root = root / "E"
        run_root = evidence_root / "ShippingPerformance" / candidate_id / run_id
        run_root.mkdir(parents=True)
        manifest = build_archive_manifest(archive, candidate_id)
        launch_base = datetime.now(timezone.utc) - timedelta(minutes=4)
        manifest["generatedUtc"] = launch_base.isoformat(
            timespec="milliseconds"
        ).replace("+00:00", "Z")
        write_exclusive_json(run_root / "ArchiveManifest.json", manifest)
        records = []
        for hole in (1, 2, 3):
            token = f"11111111-2222-4333-8{hole}44-55555555555{hole}"
            user_dir = run_root / "UserDirs" / token
            captures = user_dir / "Saved" / "PerformanceCaptures"
            captures.mkdir(parents=True)
            (captures / "ReleasePerformanceCapture.guard").write_bytes(b"")
            started = launch_base + timedelta(minutes=hole)
            finished = started + timedelta(seconds=41)
            report = synthetic_report(
                hole,
                (finished - timedelta(seconds=1)).isoformat().replace("+00:00", "Z"),
            )
            timestamped = captures / f"Performance_20990101_000{hole}40.json"
            latest = captures / "LatestPerformance.json"
            timestamped.write_bytes(canonical_json_bytes(report))
            latest.write_bytes(canonical_json_bytes(report))
            process_log_path = run_root / "ProcessLogs" / f"Hole{hole}.json"
            process_log = {
                "schema": PROCESS_LOG_SCHEMA, "schemaVersion": 1,
                "candidateId": candidate_id, "runId": run_id, "holeNumber": hole,
                "userDirToken": token, "startedUtc": started.isoformat().replace("+00:00", "Z"),
                "finishedUtc": finished.isoformat().replace("+00:00", "Z"),
                "elapsedSeconds": 41.0, "pid": 1000 + hole, "exitCode": 0,
                "stdout": "", "stderr": "",
            }
            write_exclusive_json(process_log_path, process_log)
            def relative_binding(path: Path) -> dict[str, Any]:
                return {"relativePath": path.relative_to(run_root).as_posix(), **file_binding(path)}
            record = {
                "schema": LAUNCH_SCHEMA, "schemaVersion": 1, "session": 19,
                "candidateId": candidate_id, "runId": run_id, "holeNumber": hole,
                "userDirToken": token, "sanitizedArguments": required_arguments(hole, token, True),
                "archiveInventorySha256": manifest["inventorySha256"],
                "shippingExecutableSha256": manifest["shippingExecutable"]["sha256"],
                "startedUtc": process_log["startedUtc"], "finishedUtc": process_log["finishedUtc"],
                "elapsedSeconds": 41.0, "pid": 1000 + hole, "exitCode": 0,
                "processLog": relative_binding(process_log_path),
                "performanceReport": relative_binding(timestamped),
                "latestReport": relative_binding(latest),
                "captureGuard": relative_binding(
                    captures / "ReleasePerformanceCapture.guard"
                ),
            }
            record_path = run_root / "LaunchRecords" / f"Hole{hole}.json"
            write_exclusive_json(record_path, record)
            records.append({"holeNumber": hole, "record": relative_binding(record_path)})
        run_manifest = {
            "schema": RUN_MANIFEST_SCHEMA, "schemaVersion": 1, "session": 19,
            "candidateId": candidate_id, "runId": run_id,
            "generatedUtc": (
                launch_base + timedelta(minutes=4)
            ).isoformat(timespec="milliseconds").replace("+00:00", "Z"),
            "archiveManifest": {"relativePath": "ArchiveManifest.json", **file_binding(run_root / "ArchiveManifest.json")},
            "archiveBeforeInventorySha256": manifest["inventorySha256"],
            "archiveAfterInventorySha256": manifest["inventorySha256"],
            "archivePreserved": True,
            "launcher": {"relativePath": LAUNCHER_RELATIVE, **file_binding(archive / LAUNCHER_RELATIVE)},
            "shippingExecutable": {"relativePath": EXECUTABLE_RELATIVE, **file_binding(archive / EXECUTABLE_RELATIVE)},
            "launchRecords": records,
        }
        write_exclusive_json(run_root / "RunManifest.json", run_manifest)
        passing = validate_run(run_root, archive, candidate_id)
        require(passing["passed"],
                f"complete three-hole candidate binding should pass: {passing['errors']}")
        require(
            matches_performance_budget_contract(
                synthetic_report(1, passing["runs"][0]["finishedUtc"])["budget"]
            ),
            "legitimate Unreal float32-serialized budget must match the fixed contract",
        )
        require(len(passing["runs"]) == 3, "receipt should retain three runs")
        require(not passing["claimBoundary"]["thermalSoak"], "thermal soak cannot be inferred")
        require(not passing["claimBoundary"]["powerModeRecorded"], "power mode cannot be inferred")
        require(len({row["userDirToken"] for row in passing["runs"]}) == 3,
                "receipt must retain unique UserDir tokens")

        archive_generated = parse_utc(manifest["generatedUtc"], "archive generatedUtc")
        run_generated = parse_utc(run_manifest["generatedUtc"], "run generatedUtc")
        verified = parse_utc(passing["verifiedUtc"], "receipt verifiedUtc")
        try:
            validate_run_chronology(
                archive_generated, passing["runs"],
                verified + timedelta(days=1), verified,
            )
        except EvidenceError as exc:
            future_rejected = "beyond validation time" in str(exc)
        else:
            future_rejected = False
        require(future_rejected, "future-dated evidence must be rejected")

        overlapping_rows = [dict(row) for row in passing["runs"]]
        first_finish = parse_utc(overlapping_rows[0]["finishedUtc"], "hole 1 finish")
        overlapping_rows[1]["startedUtc"] = (
            first_finish - timedelta(seconds=1)
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z")
        try:
            validate_run_chronology(
                archive_generated, overlapping_rows, run_generated, verified
            )
        except EvidenceError as exc:
            overlap_rejected = "sequential launch windows" in str(exc)
        else:
            overlap_rejected = False
        require(overlap_rejected, "overlapping hole processes must be rejected")

        output = evidence_root / "Receipt.json"
        output = validate_receipt_output_path(
            output, archive, evidence_root, run_root
        )
        write_exclusive_json(output, passing)
        before = output.read_bytes()
        try:
            write_exclusive_json(output, passing)
        except EvidenceError:
            rejected_overwrite = True
        else:
            rejected_overwrite = False
        require(rejected_overwrite and output.read_bytes() == before,
                "receipt output must be append-only")
        post_receipt_validation = validate_run(run_root, archive, candidate_id)
        require(post_receipt_validation["passed"],
                "external receipt write must preserve run/archive validation")

        try:
            validate_candidate_archive_path(archive, candidate_id[:-1] + "7")
        except EvidenceError:
            mislabeled_archive_rejected = True
        else:
            mislabeled_archive_rejected = False
        require(mislabeled_archive_rejected,
                "archive directory cannot be relabeled as a different candidate")

        try:
            validate_receipt_output_path(
                archive / "Receipt.json", archive, evidence_root, run_root
            )
        except EvidenceError:
            archive_output_rejected = True
        else:
            archive_output_rejected = False
        require(archive_output_rejected, "archive-local receipt output must be rejected")

        try:
            validate_receipt_output_path(
                run_root / "Receipt.json", archive, evidence_root, run_root
            )
        except EvidenceError:
            run_output_rejected = True
        else:
            run_output_rejected = False
        require(run_output_rejected, "run-local receipt output must be rejected")

        extra_token = "99999999-8888-4777-8666-555555555555"
        extra_user_dir = run_root / "UserDirs" / extra_token
        extra_user_dir.mkdir()
        extra_attempt = validate_run(run_root, archive, candidate_id)
        require(
            not extra_attempt["passed"]
            and any("unbound or missing evidence entries" in item
                    for item in extra_attempt["errors"]),
            "an unbound fourth capture UserDir must fail",
        )
        extra_user_dir.rmdir()

        hole_one_record_path = run_root / "LaunchRecords" / "Hole1.json"
        hole_one_record = load_json(hole_one_record_path)
        hole_one_process_path = run_root / hole_one_record["processLog"]["relativePath"]
        original_process_log = hole_one_process_path.read_bytes()
        altered_process_log = load_json(hole_one_process_path)
        altered_process_log["pid"] += 1
        hole_one_process_path.write_bytes(canonical_json_bytes(altered_process_log))
        hole_one_record["processLog"] = relative_binding(hole_one_process_path)
        try:
            validate_launch_record(
                hole_one_record, run_root, candidate_id, run_id,
                manifest["inventorySha256"], manifest["shippingExecutable"]["sha256"], 1,
            )
        except EvidenceError as exc:
            process_mismatch_rejected = "chronology/process binding differs" in str(exc)
        else:
            process_mismatch_rejected = False
        require(process_mismatch_rejected,
                "process log fields must exactly match their launch record")
        hole_one_process_path.write_bytes(original_process_log)

        run_manifest_path = run_root / "RunManifest.json"
        original_run_manifest = run_manifest_path.read_bytes()
        contradictory_run_manifest = load_json(run_manifest_path)
        contradictory_run_manifest["generatedUtc"] = launch_base.isoformat(
            timespec="milliseconds"
        ).replace("+00:00", "Z")
        run_manifest_path.write_bytes(canonical_json_bytes(contradictory_run_manifest))
        contradictory_chronology = validate_run(run_root, archive, candidate_id)
        require(
            not contradictory_chronology["passed"]
            and any("chronology contradicts" in item
                    for item in contradictory_chronology["errors"]),
            "post-run manifest timestamp cannot predate a launch finish",
        )
        run_manifest_path.write_bytes(original_run_manifest)

        outside_directory = root / "Outside"
        outside_directory.mkdir()
        outside_file = outside_directory / "artifact.json"
        outside_file.write_bytes(b"outside")
        traversal_binding = {
            "relativePath": "../Outside/artifact.json",
            **file_binding(outside_file),
        }
        try:
            validate_file_binding(
                traversal_binding, run_root / ".." / "Outside" / "artifact.json",
                "path traversal", run_root,
            )
        except EvidenceError:
            traversal_rejected = True
        else:
            traversal_rejected = False
        require(traversal_rejected, "relative file bindings cannot escape the run root")

        link = run_root / "LinkedOutside"
        try:
            link.symlink_to(outside_directory, target_is_directory=True)
        except OSError:
            pass
        else:
            escaped_binding = {
                "relativePath": "LinkedOutside/artifact.json",
                **file_binding(outside_file),
            }
            try:
                validate_file_binding(
                    escaped_binding, link / "artifact.json", "symlink escape", run_root
                )
            except EvidenceError:
                symlink_escape_rejected = True
            else:
                symlink_escape_rejected = False
            require(symlink_escape_rejected,
                    "resolved file bindings must reject symlink escapes")
            link.unlink()

        report_path = run_root / hole_one_record["performanceReport"]["relativePath"]
        original_report = report_path.read_bytes()
        report_started = parse_utc(
            load_json(hole_one_record_path)["startedUtc"], "report launch start"
        )
        report_finished = parse_utc(
            load_json(hole_one_record_path)["finishedUtc"], "report launch finish"
        )

        null_rhi_report = load_json(report_path)
        null_rhi_report["rhi"] = "NullRHI"
        report_path.write_bytes(canonical_json_bytes(null_rhi_report))
        try:
            validate_shipping_report(report_path, 1, report_started, report_finished)
        except EvidenceError:
            null_rhi_rejected = True
        else:
            null_rhi_rejected = False
        require(null_rhi_rejected, "semantic report validation must reject NullRHI")

        weakened_budget_report = json.loads(original_report)
        weakened_budget_report["budget"]["fail_p95_frame_ms"] = 40
        report_path.write_bytes(canonical_json_bytes(weakened_budget_report))
        try:
            validate_shipping_report(report_path, 1, report_started, report_finished)
        except EvidenceError as exc:
            weakened_budget_rejected = "source-controlled contract" in str(exc)
        else:
            weakened_budget_rejected = False
        require(weakened_budget_rejected,
                "semantic report validation must reject a weakened budget")

        boolean_hole_report = json.loads(original_report)
        boolean_hole_report["hole_number"] = True
        report_path.write_bytes(canonical_json_bytes(boolean_hole_report))
        try:
            validate_shipping_report(report_path, 1, report_started, report_finished)
        except EvidenceError as exc:
            boolean_hole_rejected = "invalid hole number" in str(exc)
        else:
            boolean_hole_rejected = False
        require(boolean_hole_rejected,
                "semantic report validation must reject boolean numeric fields")
        report_path.write_bytes(original_report)

        (archive / "mutation.bin").write_bytes(b"changed")
        require(not validate_run(run_root, archive, candidate_id)["passed"],
                "archive mutation must fail preservation")

    print(f"Session 19 Shipping performance evidence self-test OK ({checks} assertions).")
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-root", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--output", type=Path, help="Optional append-only sanitized receipt path.")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if any(value is not None for value in (args.run_root, args.archive, args.candidate_id, args.output)):
            print("--self-test accepts no evidence arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (AssertionError, EvidenceError, OSError) as exc:
            print(f"Shipping performance evidence self-test FAILED: {exc}", file=sys.stderr)
            return 1
    if args.run_root is None or args.archive is None or args.candidate_id is None:
        print("--run-root, --archive, and --candidate-id are required", file=sys.stderr)
        return 2
    run_root = args.run_root.resolve()
    archive = args.archive.resolve()
    receipt = validate_run(run_root, archive, args.candidate_id)
    evidence_written = False
    if args.output is not None:
        try:
            if not receipt["passed"]:
                raise EvidenceError("refusing to emit a PASS receipt for invalid evidence")
            evidence_root = run_root.parents[2]
            output = validate_receipt_output_path(
                args.output, archive, evidence_root, run_root
            )
            write_exclusive_json(output, receipt)
            post_write_receipt = validate_run(run_root, archive, args.candidate_id)
            if (not post_write_receipt["passed"]
                    or post_write_receipt["archive"] != receipt["archive"]
                    or post_write_receipt["runManifest"] != receipt["runManifest"]
                    or post_write_receipt["runs"] != receipt["runs"]):
                raise EvidenceError(
                    "run/archive validation changed after external receipt write"
                )
            evidence_written = True
        except EvidenceError as exc:
            print(str(exc), file=sys.stderr)
            return 2
    print(json.dumps({
        "state": receipt["state"], "passed": receipt["passed"],
        "candidateId": receipt["candidateId"], "runCount": len(receipt["runs"]),
        "errors": receipt["errors"], "evidenceWritten": evidence_written,
        "releaseReady": False,
    }, indent=2, sort_keys=True))
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
