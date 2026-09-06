#!/usr/bin/env python3
"""Fail-closed validation for external Session 19 Shipping evidence.

The validator binds objective image/CSV artifacts to one immutable Shipping
archive and process run. Historical manifest v1 behavior remains available;
manifest v2 additionally binds the raw game-emitted runtime journal in the
redirected UserDir. It deliberately does not turn technical artifacts into
human gameplay, visual, accessibility, legal, or release approval.
"""

from __future__ import annotations

import argparse
import copy
import csv
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
from datetime import datetime, timedelta, timezone
from pathlib import Path, PurePosixPath
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
POLICY_PATH = Path(__file__).resolve().with_name(
    "DG_Session19ExternalTechnicalEvidencePolicy.json"
)
POLICY_SHA256 = "8A57FB1BBF99BE2572BC1CC5F65451F047748419CD74335F00CE6A15F6B62054"
MANIFEST_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
LAUNCH_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1"
MANIFEST_SCHEMA_V2 = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
LAUNCH_SCHEMA_V2 = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_RELATIVE = "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_JOURNAL_MAX_BYTES = 262144
STRUCTURED_EVIDENCE_MAX_BYTES = 16 * 1024 * 1024
EXPECTED_RUNTIME_EVENTS = [
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
EXPECTED_RUNTIME_HOLES = [None, 1, 1, 1, 2, 2, 2, 3, 3, 3, None, None]
EXPECTED_RUNTIME_COMPLETED = [0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3]
RECEIPT_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceValidation.v1"
HEX64_RE = re.compile(r"^[0-9A-F]{64}$")


class EvidenceError(RuntimeError):
    """A path-safe fail-closed evidence error."""

    def __init__(self, code: str, relative_path: str | None = None):
        super().__init__(code)
        self.code = code
        self.relative_path = relative_path


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


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
    # CPython supplies stable file IDs on supported Windows filesystems and
    # POSIX.  Fall back to the remaining immutable snapshot fields only on a
    # filesystem that reports a zero inode.
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


def _read_file_stable(
    path: Path,
    maximum_bytes: int,
    *,
    _post_read_hook: Any | None = None,
) -> tuple[bytes, os.stat_result]:
    """Read one regular non-reparse file from a stable open-file identity."""

    before_path = os.lstat(path)
    if is_reparse(path) or not stat.S_ISREG(before_path.st_mode):
        raise EvidenceError("FILE_NOT_REGULAR_OR_REPARSE")
    with path.open("rb") as stream:
        before_descriptor = os.fstat(stream.fileno())
        if not _same_open_file(before_path, before_descriptor):
            raise EvidenceError("FILE_CHANGED_DURING_READ")
        raw = stream.read(maximum_bytes + 1)
        if _post_read_hook is not None:
            _post_read_hook(path)
        after_descriptor = os.fstat(stream.fileno())
    after_path = os.lstat(path)
    if (
        len(raw) != before_descriptor.st_size
        or len(raw) > maximum_bytes
        or _stat_fingerprint(before_descriptor) != _stat_fingerprint(after_descriptor)
        or _stat_fingerprint(before_path) != _stat_fingerprint(after_path)
        or not _same_open_file(after_path, after_descriptor)
        or is_reparse(path)
    ):
        raise EvidenceError("FILE_CHANGED_DURING_READ")
    return raw, after_path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    before_path = os.lstat(path)
    if is_reparse(path) or not stat.S_ISREG(before_path.st_mode):
        raise EvidenceError("FILE_NOT_REGULAR_OR_REPARSE")
    with path.open("rb") as stream:
        before_descriptor = os.fstat(stream.fileno())
        if not _same_open_file(before_path, before_descriptor):
            raise EvidenceError("FILE_CHANGED_DURING_HASH")
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
        after_descriptor = os.fstat(stream.fileno())
    after_path = os.lstat(path)
    if (
        _stat_fingerprint(before_descriptor) != _stat_fingerprint(after_descriptor)
        or _stat_fingerprint(before_path) != _stat_fingerprint(after_path)
        or not _same_open_file(after_path, after_descriptor)
        or is_reparse(path)
    ):
        raise EvidenceError("FILE_CHANGED_DURING_HASH")
    return digest.hexdigest().upper()


def canonical_relative(value: str) -> str:
    candidate = value.replace("\\", "/")
    pure = PurePosixPath(candidate)
    if (
        not candidate
        or pure.is_absolute()
        or pure.as_posix() != candidate
        or any(part in ("", ".", "..") for part in pure.parts)
        or ":" in candidate
        or "\x00" in candidate
    ):
        raise EvidenceError("NON_CANONICAL_RELATIVE_PATH")
    return candidate


def is_reparse(path: Path) -> bool:
    try:
        info = os.lstat(path)
    except OSError:
        return True
    flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(getattr(info, "st_file_attributes", 0) & flag)


def within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def resolve_directory(path: Path, code: str) -> Path:
    try:
        result = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EvidenceError(code) from exc
    if not result.is_dir() or is_reparse(result):
        raise EvidenceError(code)
    return result


def resolve_file(path: Path, code: str) -> Path:
    try:
        result = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EvidenceError(code) from exc
    if not result.is_file() or is_reparse(result):
        raise EvidenceError(code)
    return result


def resolve_descendant_file(root: Path, relative: str, code: str) -> Path:
    """Resolve a regular descendant while rejecting every reparse component."""

    current = root
    for part in PurePosixPath(relative).parts:
        current = current / part
        if is_reparse(current):
            raise EvidenceError(code, relative)
    resolved = resolve_file(current, code)
    if not within(resolved, root):
        raise EvidenceError(code, relative)
    return resolved


def load_policy() -> tuple[dict[str, Any], dict[str, Any]]:
    raw = POLICY_PATH.read_bytes()
    digest = sha256_bytes(raw)
    if digest != POLICY_SHA256:
        raise EvidenceError("POLICY_HASH_MISMATCH")
    policy = json.loads(raw.decode("utf-8"))
    if (
        policy.get("schema")
        != "DiscGolfTour.Session19ExternalTechnicalEvidencePolicy.v1"
        or policy.get("schemaVersion") != 1
        or policy.get("session") != 19
        or policy.get("claimBoundary")
        != {
            "technicalArtifactCollectionOnly": True,
            "manualGameplayAcceptance": False,
            "manualReviewerId": None,
            "manualReviewedUtc": None,
            "humanPlayFeelApproval": False,
            "visualProductApproval": False,
            "accessibilityApproval": False,
            "legalApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        }
    ):
        raise EvidenceError("POLICY_CONTRACT_INVALID")
    return policy, {
        "relativePath": "Scripts/" + POLICY_PATH.name,
        "bytes": len(raw),
        "sha256": digest,
    }


def collect_archive(root: Path) -> dict[str, Any]:
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for path in sorted(root.rglob("*"), key=lambda item: item.as_posix().casefold()):
        relative = canonical_relative(path.relative_to(root).as_posix())
        if is_reparse(path):
            raise EvidenceError("ARCHIVE_REPARSE_POINT", relative)
        if path.is_dir():
            continue
        if not path.is_file():
            raise EvidenceError("ARCHIVE_NON_REGULAR_FILE", relative)
        folded = relative.casefold()
        if folded in seen:
            raise EvidenceError("ARCHIVE_CASE_COLLISION", relative)
        seen.add(folded)
        records.append({
            "path": relative,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        })
    material = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return {
        "canonicalManifestSha256": sha256_bytes(material),
        "fileCount": len(records),
        "bytes": sum(item["bytes"] for item in records),
    }


def parse_utc(value: Any) -> datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ValueError("not canonical UTC")
    result = datetime.fromisoformat(value[:-1] + "+00:00")
    if result.tzinfo != timezone.utc:
        raise ValueError("not UTC")
    return result


def _runtime_integer(
    value: Any,
    code: str,
    minimum: int = 0,
    maximum: int | None = None,
) -> int:
    if (
        type(value) is not int
        or value < minimum
        or (maximum is not None and value > maximum)
    ):
        raise EvidenceError(code)
    return value


def _runtime_uuid(value: Any, code: str, *, version_four: bool = False) -> str:
    if not isinstance(value, str):
        raise EvidenceError(code)
    try:
        parsed = uuid.UUID(value)
    except ValueError as exc:
        raise EvidenceError(code) from exc
    if str(parsed) != value or (version_four and parsed.version != 4):
        raise EvidenceError(code)
    return value


def _runtime_json_record(raw: bytes) -> dict[str, Any]:
    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise EvidenceError("RUNTIME_JOURNAL_DUPLICATE_JSON_KEY")
            result[key] = value
        return result

    try:
        value = json.loads(
            raw.decode("ascii"),
            object_pairs_hook=reject_duplicates,
            parse_constant=lambda _value: (_ for _ in ()).throw(
                EvidenceError("RUNTIME_JOURNAL_NONFINITE_JSON_VALUE")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise EvidenceError("RUNTIME_JOURNAL_JSON_INVALID") from exc
    if not isinstance(value, dict):
        raise EvidenceError("RUNTIME_JOURNAL_RECORD_NOT_OBJECT")
    canonical = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    if raw != canonical:
        raise EvidenceError("RUNTIME_JOURNAL_RECORD_NOT_CANONICAL")
    return value


def _strict_json_object(raw: bytes, code: str) -> dict[str, Any]:
    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise EvidenceError(code)
            result[key] = value
        return result

    try:
        value = json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=reject_duplicates,
            parse_constant=lambda _value: (_ for _ in ()).throw(
                EvidenceError(code)
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise EvidenceError(code) from exc
    if not isinstance(value, dict):
        raise EvidenceError(code)
    return value


def _runtime_exact_keys(value: Any, expected: set[str], code: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != expected:
        raise EvidenceError(code)
    return value


def validate_runtime_journal_bytes(
    raw: bytes,
    *,
    candidate_id: str,
    user_token: str,
    executable_sha256: str,
    archive_manifest_sha256: str,
    capture_nonce: str,
    process_started: datetime,
    process_finished: datetime,
    timestamp_tolerance_seconds: int,
) -> dict[str, Any]:
    """Independently validate every canonical game-emitted JSONL record."""

    if not 0 < len(raw) <= RUNTIME_JOURNAL_MAX_BYTES:
        raise EvidenceError("RUNTIME_JOURNAL_SIZE_INVALID")
    if b"\r" in raw or not raw.endswith(b"\n"):
        raise EvidenceError("RUNTIME_JOURNAL_LINE_ENDINGS_INVALID")
    raw_lines = raw[:-1].split(b"\n")
    if len(raw_lines) != 13 or any(not line for line in raw_lines):
        raise EvidenceError("RUNTIME_JOURNAL_RECORD_COUNT_INVALID")
    records = [_runtime_json_record(line) for line in raw_lines]

    header = _runtime_exact_keys(
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
        "RUNTIME_JOURNAL_HEADER_FIELDS_INVALID",
    )
    round_id = _runtime_uuid(
        header["roundId"], "RUNTIME_JOURNAL_ROUND_ID_INVALID", version_four=True
    )
    _runtime_uuid(
        header["captureNonce"],
        "RUNTIME_JOURNAL_CAPTURE_NONCE_INVALID",
        version_four=True,
    )
    _runtime_uuid(header["userDirToken"], "RUNTIME_JOURNAL_USER_TOKEN_INVALID")
    try:
        header_started = parse_utc(header["startedUtc"])
    except (TypeError, ValueError) as exc:
        raise EvidenceError("RUNTIME_JOURNAL_STARTED_UTC_INVALID") from exc
    if (
        header["schema"] != RUNTIME_JOURNAL_SCHEMA
        or _runtime_integer(
            header["schemaVersion"], "RUNTIME_JOURNAL_SCHEMA_VERSION_INVALID", 1
        )
        != 1
        or _runtime_integer(header["session"], "RUNTIME_JOURNAL_SESSION_INVALID", 1)
        != 19
        or header["recordType"] != "HEADER"
        or header["candidateId"] != candidate_id
        or header["userDirToken"] != user_token
        or header["executableSha256"] != executable_sha256
        or header["archiveManifestSha256"] != archive_manifest_sha256
        or header["captureNonce"] != capture_nonce
        or header["captureNonce"] == header["userDirToken"]
        or round_id == header["captureNonce"]
        or round_id == header["userDirToken"]
        or not (
            process_started - timedelta(seconds=timestamp_tolerance_seconds)
            <= header_started
            <= process_finished + timedelta(seconds=timestamp_tolerance_seconds)
        )
    ):
        raise EvidenceError("RUNTIME_JOURNAL_HEADER_BINDING_INVALID")

    previous_ms = 0
    previous_strokes = 0
    previous_penalties = 0
    completed_totals: list[tuple[int, int]] = []
    events: list[dict[str, Any]] = []
    for index, record in enumerate(records[1:]):
        expected_keys = {
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
        if index == 11:
            expected_keys.add("finalScore")
        event = _runtime_exact_keys(
            record, expected_keys, "RUNTIME_JOURNAL_EVENT_FIELDS_INVALID"
        )
        sequence = _runtime_integer(
            event["sequence"], "RUNTIME_JOURNAL_SEQUENCE_INVALID", 1, 12
        )
        monotonic_ms = _runtime_integer(
            event["monotonicMs"],
            "RUNTIME_JOURNAL_MONOTONIC_TIME_INVALID",
            1,
            (1 << 63) - 1,
        )
        completed = _runtime_integer(
            event["completedHoles"], "RUNTIME_JOURNAL_COMPLETED_HOLES_INVALID", 0, 3
        )
        strokes = _runtime_integer(
            event["totalStrokes"], "RUNTIME_JOURNAL_STROKES_INVALID", 0, (1 << 31) - 1
        )
        penalties = _runtime_integer(
            event["totalPenalties"],
            "RUNTIME_JOURNAL_PENALTIES_INVALID",
            0,
            (1 << 31) - 1,
        )
        expected_hole = EXPECTED_RUNTIME_HOLES[index]
        actual_hole = event["holeNumber"]
        hole_valid = (
            actual_hole is None
            if expected_hole is None
            else type(actual_hole) is int and actual_hole == expected_hole
        )
        if (
            event["recordType"] != "EVENT"
            or event["roundId"] != round_id
            or event["event"] != EXPECTED_RUNTIME_EVENTS[index]
            or not hole_valid
            or sequence != index + 1
            or monotonic_ms <= previous_ms
            or completed != EXPECTED_RUNTIME_COMPLETED[index]
            or strokes < previous_strokes
            or penalties < previous_penalties
            or penalties > strokes
        ):
            raise EvidenceError("RUNTIME_JOURNAL_EVENT_SEQUENCE_INVALID")
        if index == 0 and (strokes != 0 or penalties != 0):
            raise EvidenceError("RUNTIME_JOURNAL_ROUND_DID_NOT_START_AT_ZERO")
        if index in (3, 6, 9):
            completed_totals.append((strokes, penalties))
        previous_ms = monotonic_ms
        previous_strokes = strokes
        previous_penalties = penalties
        events.append(event)

    maximum_elapsed_ms = max(
        0,
        int((process_finished - process_started).total_seconds() * 1000),
    ) + timestamp_tolerance_seconds * 1000
    if events[-1]["monotonicMs"] > maximum_elapsed_ms:
        raise EvidenceError("RUNTIME_JOURNAL_MONOTONIC_TIME_OUTSIDE_PROCESS_WINDOW")

    score = _runtime_exact_keys(
        events[-1]["finalScore"],
        {
            "completedHoles",
            "holeRows",
            "parTotal",
            "totalHoles",
            "totalPenalties",
            "totalStrokes",
        },
        "RUNTIME_JOURNAL_FINAL_SCORE_FIELDS_INVALID",
    )
    rows_value = score["holeRows"]
    if not isinstance(rows_value, list) or len(rows_value) != 3:
        raise EvidenceError("RUNTIME_JOURNAL_FINAL_SCORE_ROWS_INVALID")
    rows: list[dict[str, int]] = []
    for index, value in enumerate(rows_value, start=1):
        row = _runtime_exact_keys(
            value,
            {"holeNumber", "par", "penalties", "strokes"},
            "RUNTIME_JOURNAL_FINAL_SCORE_ROW_FIELDS_INVALID",
        )
        normalized = {
            "holeNumber": _runtime_integer(
                row["holeNumber"], "RUNTIME_JOURNAL_FINAL_HOLE_NUMBER_INVALID", 1, 3
            ),
            "par": _runtime_integer(
                row["par"], "RUNTIME_JOURNAL_FINAL_PAR_INVALID", 1, (1 << 31) - 1
            ),
            "penalties": _runtime_integer(
                row["penalties"],
                "RUNTIME_JOURNAL_FINAL_PENALTIES_INVALID",
                0,
                (1 << 31) - 1,
            ),
            "strokes": _runtime_integer(
                row["strokes"],
                "RUNTIME_JOURNAL_FINAL_STROKES_INVALID",
                1,
                (1 << 31) - 1,
            ),
        }
        if (
            normalized["holeNumber"] != index
            or normalized["par"] != [3, 4, 4][index - 1]
            or normalized["penalties"] > normalized["strokes"]
        ):
            raise EvidenceError("RUNTIME_JOURNAL_FINAL_SCORE_ROW_INVALID")
        rows.append(normalized)
    final_strokes = _runtime_integer(
        score["totalStrokes"], "RUNTIME_JOURNAL_FINAL_TOTAL_STROKES_INVALID", 1
    )
    final_penalties = _runtime_integer(
        score["totalPenalties"], "RUNTIME_JOURNAL_FINAL_TOTAL_PENALTIES_INVALID"
    )
    if (
        _runtime_integer(
            score["completedHoles"], "RUNTIME_JOURNAL_FINAL_COMPLETED_INVALID"
        )
        != 3
        or _runtime_integer(
            score["totalHoles"], "RUNTIME_JOURNAL_FINAL_HOLE_COUNT_INVALID", 1
        )
        != 3
        or _runtime_integer(
            score["parTotal"], "RUNTIME_JOURNAL_FINAL_PAR_TOTAL_INVALID", 1
        )
        != 11
        or final_strokes != sum(row["strokes"] for row in rows)
        or final_penalties != sum(row["penalties"] for row in rows)
        or final_strokes != events[-1]["totalStrokes"]
        or final_penalties != events[-1]["totalPenalties"]
        or events[-2]["totalStrokes"] != final_strokes
        or events[-2]["totalPenalties"] != final_penalties
    ):
        raise EvidenceError("RUNTIME_JOURNAL_FINAL_SCORE_TOTALS_INVALID")
    prior_strokes = 0
    prior_penalties = 0
    for (complete_strokes, complete_penalties), row in zip(completed_totals, rows):
        if (
            complete_strokes - prior_strokes != row["strokes"]
            or complete_penalties - prior_penalties != row["penalties"]
        ):
            raise EvidenceError("RUNTIME_JOURNAL_HOLE_TOTALS_DO_NOT_RECONCILE")
        prior_strokes = complete_strokes
        prior_penalties = complete_penalties
    for tee_index, lie_index, complete_index, prior_index in (
        (1, 2, 3, 0),
        (4, 5, 6, 3),
        (7, 8, 9, 6),
    ):
        if (
            events[tee_index]["totalStrokes"]
            != events[prior_index]["totalStrokes"]
            or events[tee_index]["totalPenalties"]
            != events[prior_index]["totalPenalties"]
            or events[lie_index]["totalStrokes"]
            <= events[tee_index]["totalStrokes"]
            or events[complete_index]["totalStrokes"]
            <= events[lie_index]["totalStrokes"]
        ):
            raise EvidenceError("RUNTIME_JOURNAL_SHOT_TRANSITIONS_INVALID")

    return {
        "captureNonce": header["captureNonce"],
        "eventCount": len(events),
        "firstSequence": events[0]["sequence"],
        "lastSequence": events[-1]["sequence"],
        "roundId": round_id,
        "schema": header["schema"],
        "finalScore": score,
    }


def utc_text(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(33)
    if (
        len(header) < 33
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or struct.unpack(">I", header[8:12])[0] != 13
        or header[12:16] != b"IHDR"
        or struct.unpack(">I", header[29:33])[0]
        != (zlib.crc32(header[12:29]) & 0xFFFFFFFF)
    ):
        raise EvidenceError("SCREENSHOT_INVALID_PNG")
    width, height = struct.unpack(">II", header[16:24])
    if width <= 0 or height <= 0:
        raise EvidenceError("SCREENSHOT_INVALID_DIMENSIONS")
    return width, height


def candidate_binding(manifest: dict[str, Any]) -> str:
    screenshots = [
        {
            "relativePath": item["relativePath"],
            "bytes": item["bytes"],
            "sha256": item["sha256"],
            "width": item["width"],
            "height": item["height"],
        }
        for item in manifest["evidence"]["screenshots"]
    ]
    presentmon = manifest["evidence"]["presentMon"]
    material = {
        "schema": manifest["schema"],
        "policySha256": manifest["policy"]["sha256"],
        "candidate": manifest["candidate"],
        "userDirToken": manifest["userDir"]["token"],
        "process": manifest["process"],
        "launchRecordSha256": manifest["evidence"]["launchRecord"]["sha256"],
        "screenshots": screenshots,
        "presentMon": {
            "requested": presentmon["requested"],
            "relativePath": presentmon["relativePath"],
            "bytes": presentmon["bytes"],
            "sha256": presentmon["sha256"],
            "rowCount": presentmon["rowCount"],
        },
    }
    if manifest["schema"] == MANIFEST_SCHEMA_V2:
        material["runtimeCheckpointJournal"] = manifest["evidence"][
            "runtimeCheckpointJournal"
        ]
    encoded = json.dumps(material, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return sha256_bytes(encoded)


def failure(code: str, relative_path: str | None = None) -> dict[str, str]:
    result = {"code": code}
    if relative_path is not None:
        result["relativePath"] = relative_path
    return result


def exact_keys(value: Any, keys: set[str], code: str, failures: list[dict[str, str]]) -> None:
    if not isinstance(value, dict) or set(value) != keys:
        failures.append(failure(code))


def validate_evidence(
    *,
    archive_root: Path,
    shipping_exe: Path,
    user_dir: Path,
    manifest_path: Path,
) -> dict[str, Any]:
    failures: list[dict[str, str]] = []
    receipt: dict[str, Any] = {
        "schema": RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "state": "FAIL_CLOSED_EXTERNAL_TECHNICAL_EVIDENCE",
        "manifest": {"sha256": None},
        "candidateId": None,
        "counts": {"screenshots": 0, "presentMonRows": 0},
        "failures": failures,
        "claimBoundary": {
            "technicalArtifactValidationOnly": True,
            "manualGameplayAcceptance": False,
            "manualReviewerId": None,
            "manualReviewedUtc": None,
            "humanPlayFeelApproval": False,
            "visualProductApproval": False,
            "accessibilityApproval": False,
            "legalApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    try:
        policy, policy_binding = load_policy()
        archive = resolve_directory(archive_root, "ARCHIVE_ROOT_INVALID")
        executable = resolve_file(shipping_exe, "SHIPPING_EXECUTABLE_INVALID")
        users = resolve_directory(user_dir, "USERDIR_INVALID")
        manifest_file = resolve_file(manifest_path, "MANIFEST_INVALID")
        evidence_root = manifest_file.parent.resolve(strict=True)
        if is_reparse(evidence_root):
            raise EvidenceError("EVIDENCE_ROOT_REPARSE_POINT")
        project = ROOT.resolve(strict=True)
        if not within(executable, archive):
            raise EvidenceError("EXECUTABLE_OUTSIDE_ARCHIVE")
        if any(within(users, parent) or within(parent, users) for parent in (archive, evidence_root, project)):
            raise EvidenceError("USERDIR_BOUNDARY_INVALID")
        if any(within(evidence_root, parent) or within(parent, evidence_root) for parent in (archive, users, project)):
            raise EvidenceError("EVIDENCE_BOUNDARY_INVALID")
        try:
            token = str(uuid.UUID(users.name))
        except ValueError as exc:
            raise EvidenceError("USERDIR_TOKEN_NOT_UUID") from exc
        if token != users.name:
            raise EvidenceError("USERDIR_TOKEN_NOT_CANONICAL")

        manifest_raw, _manifest_stat = _read_file_stable(
            manifest_file, STRUCTURED_EVIDENCE_MAX_BYTES
        )
        manifest_sha = sha256_bytes(manifest_raw)
        receipt["manifest"]["sha256"] = manifest_sha
        manifest = _strict_json_object(manifest_raw, "MANIFEST_JSON_INVALID")
        exact_keys(manifest, {
            "schema", "schemaVersion", "session", "generatedUtc", "state", "policy",
            "candidate", "userDir", "launch", "process", "evidence", "claimBoundary",
            "candidateEvidenceBindingSha256",
        }, "MANIFEST_FIELDS_INVALID", failures)
        manifest_v2 = (
            manifest.get("schema") == MANIFEST_SCHEMA_V2
            and type(manifest.get("schemaVersion")) is int
            and manifest.get("schemaVersion") == 2
        )
        manifest_v1 = (
            manifest.get("schema") == MANIFEST_SCHEMA
            and type(manifest.get("schemaVersion")) is int
            and manifest.get("schemaVersion") == 1
        )
        if not (manifest_v1 or manifest_v2) or manifest.get("session") != 19:
            failures.append(failure("MANIFEST_SCHEMA_INVALID"))
        if manifest.get("state") != "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION":
            failures.append(failure("MANIFEST_STATE_INVALID"))
        if manifest.get("policy") != policy_binding:
            failures.append(failure("MANIFEST_POLICY_BINDING_INVALID"))
        if manifest.get("claimBoundary") != policy["claimBoundary"]:
            failures.append(failure("MANIFEST_CLAIM_BOUNDARY_INVALID"))

        candidate = manifest.get("candidate", {})
        exact_keys(candidate, {
            "candidateId", "exeRelativePath", "expectedExeSha256", "observedExeSha256",
            "expectedArchiveManifestSha256", "archiveBefore", "archiveAfter", "archivePreserved",
        }, "CANDIDATE_FIELDS_INVALID", failures)
        candidate_id = candidate.get("candidateId")
        receipt["candidateId"] = candidate_id
        if not isinstance(candidate_id, str) or re.fullmatch(policy["candidateIdPattern"], candidate_id) is None:
            failures.append(failure("CANDIDATE_ID_INVALID"))
        exe_relative = canonical_relative(str(candidate.get("exeRelativePath", "")))
        if executable.name != policy["shippingExecutableBasename"] or executable.relative_to(archive).as_posix() != exe_relative:
            failures.append(failure("EXECUTABLE_IDENTITY_INVALID"))
        current_exe_sha = sha256_file(executable)
        for key in ("expectedExeSha256", "observedExeSha256"):
            if candidate.get(key) != current_exe_sha or not HEX64_RE.fullmatch(str(candidate.get(key, ""))):
                failures.append(failure("EXECUTABLE_HASH_MISMATCH"))
                break
        current_archive = collect_archive(archive)
        for key in ("archiveBefore", "archiveAfter"):
            if candidate.get(key) != current_archive:
                failures.append(failure("ARCHIVE_MANIFEST_MISMATCH"))
                break
        if candidate.get("expectedArchiveManifestSha256") != current_archive["canonicalManifestSha256"]:
            failures.append(failure("EXPECTED_ARCHIVE_HASH_MISMATCH"))
        if candidate.get("archivePreserved") is not True:
            failures.append(failure("ARCHIVE_NOT_RECORDED_PRESERVED"))

        user_record = manifest.get("userDir", {})
        exact_keys(user_record, {
            "token", "hostPathRecorded", "emptyBeforeLaunch", "externalBoundaryValidated",
            "reparsePointsRejected",
        }, "USERDIR_FIELDS_INVALID", failures)
        if user_record != {
            "token": token,
            "hostPathRecorded": False,
            "emptyBeforeLaunch": True,
            "externalBoundaryValidated": True,
            "reparsePointsRejected": True,
        }:
            failures.append(failure("USERDIR_RECORD_INVALID"))

        launch = manifest.get("launch", {})
        exact_keys(launch, {"sanitizedArguments", "workingDirectoryRelative", "additionalArgumentsUsed"}, "LAUNCH_FIELDS_INVALID", failures)
        expected_args = policy["launch"]["requiredArguments"] + [f"-UserDir=<EXTERNAL_UUID:{token}>"]
        if launch.get("sanitizedArguments") != expected_args or launch.get("additionalArgumentsUsed") is not False:
            failures.append(failure("LAUNCH_ARGUMENTS_INVALID"))
        expected_cwd = Path(exe_relative).parent.as_posix()
        if launch.get("workingDirectoryRelative") != expected_cwd:
            failures.append(failure("LAUNCH_WORKING_DIRECTORY_INVALID"))

        process = manifest.get("process", {})
        exact_keys(process, {"pid", "startedUtc", "finishedUtc", "exitCode", "timedOut"}, "PROCESS_FIELDS_INVALID", failures)
        try:
            started = parse_utc(process.get("startedUtc"))
            finished = parse_utc(process.get("finishedUtc"))
            generated = parse_utc(manifest.get("generatedUtc"))
            if not started < finished <= generated:
                failures.append(failure("TIMESTAMP_ORDER_INVALID"))
        except (TypeError, ValueError):
            started = finished = datetime(1970, 1, 1, tzinfo=timezone.utc)
            failures.append(failure("TIMESTAMP_INVALID"))
        if type(process.get("pid")) is not int or process.get("pid", 0) <= 0:
            failures.append(failure("PROCESS_ID_INVALID"))
        if (
            type(process.get("exitCode")) is not int
            or process.get("exitCode") != policy["launch"]["requiredExitCode"]
            or process.get("timedOut") is not False
        ):
            failures.append(failure("PROCESS_COMPLETION_INVALID"))

        evidence = manifest.get("evidence", {})
        evidence_fields = {
            "hostPathRecorded", "createNewRefuseOverwrite", "launchRecord",
            "screenshots", "presentMon",
        }
        if manifest_v2:
            evidence_fields.add("runtimeCheckpointJournal")
        exact_keys(evidence, evidence_fields, "EVIDENCE_FIELDS_INVALID", failures)
        if evidence.get("hostPathRecorded") is not False or evidence.get("createNewRefuseOverwrite") is not True:
            failures.append(failure("EVIDENCE_BOUNDARY_RECORD_INVALID"))

        launch_record_binding = evidence.get("launchRecord", {})
        exact_keys(launch_record_binding, {"relativePath", "bytes", "sha256"}, "LAUNCH_RECORD_BINDING_FIELDS_INVALID", failures)
        launch_relative = canonical_relative(str(launch_record_binding.get("relativePath", "")))
        launch_file = resolve_descendant_file(
            evidence_root, launch_relative, "LAUNCH_RECORD_MISSING"
        )
        launch_raw, _launch_stat = _read_file_stable(
            launch_file, STRUCTURED_EVIDENCE_MAX_BYTES
        )
        if launch_relative != policy["evidence"]["launchRecordName"] or launch_record_binding.get("bytes") != len(launch_raw) or launch_record_binding.get("sha256") != sha256_bytes(launch_raw):
            failures.append(failure("LAUNCH_RECORD_BINDING_INVALID"))
        launch_record = _strict_json_object(
            launch_raw, "LAUNCH_RECORD_JSON_INVALID"
        )
        launch_fields = {
            "schema", "schemaVersion", "session", "recordedUtc", "policySha256", "candidateId",
            "exeRelativePath", "expectedExeSha256", "observedExeSha256",
            "expectedArchiveManifestSha256", "observedArchiveManifestSha256", "userDirToken",
            "sanitizedArguments", "pid", "hostPathsRecorded", "claimBoundary",
        }
        if manifest_v2:
            launch_fields.update({
                "captureNonce", "runtimeCheckpointJournalUserDirRelativePath",
            })
        exact_keys(launch_record, launch_fields, "LAUNCH_RECORD_FIELDS_INVALID", failures)
        if (
            (
                manifest_v2
                and (
                    launch_record.get("schema") != LAUNCH_SCHEMA_V2
                    or type(launch_record.get("schemaVersion")) is not int
                    or launch_record.get("schemaVersion") != 2
                )
            )
            or (
                not manifest_v2
                and (
                    launch_record.get("schema") != LAUNCH_SCHEMA
                    or type(launch_record.get("schemaVersion")) is not int
                    or launch_record.get("schemaVersion") != 1
                )
            )
            or launch_record.get("session") != 19
            or launch_record.get("policySha256") != policy_binding["sha256"]
            or launch_record.get("candidateId") != candidate_id
            or launch_record.get("exeRelativePath") != exe_relative
            or launch_record.get("expectedExeSha256") != candidate.get("expectedExeSha256")
            or launch_record.get("observedExeSha256") != current_exe_sha
            or launch_record.get("expectedArchiveManifestSha256") != candidate.get("expectedArchiveManifestSha256")
            or launch_record.get("observedArchiveManifestSha256") != current_archive["canonicalManifestSha256"]
            or launch_record.get("userDirToken") != token
            or launch_record.get("sanitizedArguments") != expected_args
            or launch_record.get("pid") != process.get("pid")
            or launch_record.get("hostPathsRecorded") is not False
            or launch_record.get("claimBoundary") != policy["claimBoundary"]
            or (
                manifest_v2
                and launch_record.get("runtimeCheckpointJournalUserDirRelativePath")
                != RUNTIME_JOURNAL_RELATIVE
            )
        ):
            failures.append(failure("LAUNCH_RECORD_INVALID"))
        try:
            recorded = parse_utc(launch_record.get("recordedUtc"))
            if recorded < started - timedelta(seconds=5) or recorded > finished:
                failures.append(failure("LAUNCH_RECORD_TIMESTAMP_INVALID"))
        except (TypeError, ValueError):
            failures.append(failure("LAUNCH_RECORD_TIMESTAMP_INVALID"))

        if manifest_v2:
            capture_nonce = launch_record.get("captureNonce")
            try:
                _runtime_uuid(
                    capture_nonce, "CAPTURE_NONCE_INVALID", version_four=True
                )
            except EvidenceError:
                failures.append(failure("CAPTURE_NONCE_INVALID"))

            runtime_journal = evidence.get("runtimeCheckpointJournal", {})
            exact_keys(runtime_journal, {
                "bytes", "captureNonce", "eventCount", "firstSequence",
                "lastSequence", "modifiedUtc", "modifiedWithinRunWindow",
                "roundId", "schema", "sha256", "userDirRelativePath",
                "validationFailures", "validationState",
            }, "RUNTIME_JOURNAL_BINDING_FIELDS_INVALID", failures)
            journal_relative = canonical_relative(str(
                runtime_journal.get("userDirRelativePath", "")
            ))
            journal_file = resolve_descendant_file(
                users, journal_relative, "RUNTIME_JOURNAL_MISSING"
            )
            raw_journal, journal_stat = _read_file_stable(
                journal_file, RUNTIME_JOURNAL_MAX_BYTES
            )
            journal_size = len(raw_journal)
            journal_sha = sha256_bytes(raw_journal)
            journal_modified = datetime.fromtimestamp(
                journal_stat.st_mtime, timezone.utc
            )
            journal_within_window = (
                started - timedelta(seconds=policy["screenshots"]["timestampToleranceSeconds"])
                <= journal_modified
                <= finished + timedelta(seconds=policy["screenshots"]["timestampToleranceSeconds"])
            )
            try:
                parsed_journal = validate_runtime_journal_bytes(
                    raw_journal,
                    candidate_id=candidate_id,
                    user_token=token,
                    executable_sha256=current_exe_sha,
                    archive_manifest_sha256=current_archive[
                        "canonicalManifestSha256"
                    ],
                    capture_nonce=capture_nonce,
                    process_started=started,
                    process_finished=finished,
                    timestamp_tolerance_seconds=policy["screenshots"][
                        "timestampToleranceSeconds"
                    ],
                )
            except EvidenceError as exc:
                parsed_journal = None
                failures.append(failure(exc.code))
            if (
                journal_relative != RUNTIME_JOURNAL_RELATIVE
                or journal_size <= 0
                or journal_size > RUNTIME_JOURNAL_MAX_BYTES
                or type(runtime_journal.get("bytes")) is not int
                or runtime_journal.get("bytes") != journal_size
                or runtime_journal.get("sha256") != journal_sha
                or runtime_journal.get("modifiedUtc") != utc_text(journal_modified)
                or runtime_journal.get("modifiedWithinRunWindow") is not True
                or not journal_within_window
                or runtime_journal.get("schema") != RUNTIME_JOURNAL_SCHEMA
                or runtime_journal.get("captureNonce") != capture_nonce
                or type(runtime_journal.get("eventCount")) is not int
                or runtime_journal.get("eventCount") != 12
                or type(runtime_journal.get("firstSequence")) is not int
                or runtime_journal.get("firstSequence") != 1
                or type(runtime_journal.get("lastSequence")) is not int
                or runtime_journal.get("lastSequence") != 12
                or runtime_journal.get("validationFailures") != []
                or runtime_journal.get("validationState") != RUNTIME_JOURNAL_PASS
                or parsed_journal is None
                or runtime_journal.get("roundId")
                != (parsed_journal or {}).get("roundId")
                or runtime_journal.get("schema")
                != (parsed_journal or {}).get("schema")
                or runtime_journal.get("captureNonce")
                != (parsed_journal or {}).get("captureNonce")
                or runtime_journal.get("eventCount")
                != (parsed_journal or {}).get("eventCount")
                or runtime_journal.get("firstSequence")
                != (parsed_journal or {}).get("firstSequence")
                or runtime_journal.get("lastSequence")
                != (parsed_journal or {}).get("lastSequence")
            ):
                failures.append(failure("RUNTIME_JOURNAL_BINDING_INVALID"))

        screenshot_records = evidence.get("screenshots", [])
        if not isinstance(screenshot_records, list):
            screenshot_records = []
            failures.append(failure("SCREENSHOT_RECORDS_INVALID"))
        expected_shots = [f"{policy['screenshots']['relativeDirectory']}/{name}" for name in policy["screenshots"]["requiredFiles"]]
        if [item.get("relativePath") for item in screenshot_records if isinstance(item, dict)] != expected_shots:
            failures.append(failure("SCREENSHOT_SET_INVALID"))
        for item in screenshot_records:
            if not isinstance(item, dict):
                failures.append(failure("SCREENSHOT_RECORD_INVALID"))
                continue
            exact_keys(item, {"relativePath", "bytes", "sha256", "width", "height", "modifiedUtc", "modifiedWithinRunWindow"}, "SCREENSHOT_FIELDS_INVALID", failures)
            relative = canonical_relative(str(item.get("relativePath", "")))
            shot = resolve_file(evidence_root / relative, "SCREENSHOT_MISSING")
            width, height = png_dimensions(shot)
            shot_size = shot.stat().st_size
            shot_time = datetime.fromtimestamp(shot.stat().st_mtime, timezone.utc)
            within_window = started - timedelta(seconds=policy["screenshots"]["timestampToleranceSeconds"]) <= shot_time <= finished + timedelta(seconds=policy["screenshots"]["timestampToleranceSeconds"])
            if (
                item.get("bytes") != shot_size
                or item.get("sha256") != sha256_file(shot)
                or item.get("width") != width
                or item.get("height") != height
                or item.get("modifiedUtc") != utc_text(shot_time)
                or item.get("modifiedWithinRunWindow") is not True
                or not within_window
                or shot_size <= 0
                or shot_size > policy["screenshots"]["maximumFileBytes"]
                or not policy["screenshots"]["minimumWidth"] <= width <= policy["screenshots"]["maximumWidth"]
                or not policy["screenshots"]["minimumHeight"] <= height <= policy["screenshots"]["maximumHeight"]
            ):
                failures.append(failure("SCREENSHOT_BINDING_INVALID", relative))
        receipt["counts"]["screenshots"] = len(screenshot_records)

        pm = evidence.get("presentMon", {})
        exact_keys(pm, {
            "requested", "relativePath", "bytes", "sha256", "rowCount", "columns",
            "processIdRowsMatched", "applicationRowsMatched", "toolExpectedSha256", "toolObservedSha256",
        }, "PRESENTMON_FIELDS_INVALID", failures)
        pm_relative: str | None = None
        if pm.get("requested") is True:
            pm_relative = canonical_relative(str(pm.get("relativePath", "")))
            pm_file = resolve_file(evidence_root / pm_relative, "PRESENTMON_MISSING")
            if pm_relative != policy["presentMon"]["relativePath"] or pm.get("bytes") != pm_file.stat().st_size or pm.get("sha256") != sha256_file(pm_file) or pm_file.stat().st_size <= 0 or pm_file.stat().st_size > policy["presentMon"]["maximumFileBytes"]:
                failures.append(failure("PRESENTMON_BINDING_INVALID"))
            with pm_file.open("r", encoding="utf-8-sig", newline="") as stream:
                reader = csv.DictReader(stream)
                rows = list(reader)
                columns = list(reader.fieldnames or [])
            if any(name not in columns for name in policy["presentMon"]["requiredColumns"]):
                failures.append(failure("PRESENTMON_COLUMNS_INVALID"))
            pid_matches = bool(rows) and all(str(row.get("ProcessID", "")).strip() == str(process.get("pid")) for row in rows)
            app_matches = bool(rows) and all(Path(str(row.get("Application", "")).strip()).name.casefold() == executable.name.casefold() for row in rows)
            if (
                len(rows) < policy["presentMon"]["minimumDataRows"]
                or pm.get("rowCount") != len(rows)
                or pm.get("columns") != columns
                or pm.get("processIdRowsMatched") is not True
                or pm.get("applicationRowsMatched") is not True
                or not pid_matches
                or not app_matches
                or not HEX64_RE.fullmatch(str(pm.get("toolExpectedSha256", "")))
                or pm.get("toolObservedSha256") != pm.get("toolExpectedSha256")
            ):
                failures.append(failure("PRESENTMON_CONTENT_INVALID"))
            receipt["counts"]["presentMonRows"] = len(rows)
        elif pm != {
            "requested": False,
            "relativePath": None,
            "bytes": 0,
            "sha256": None,
            "rowCount": 0,
            "columns": [],
            "processIdRowsMatched": None,
            "applicationRowsMatched": None,
            "toolExpectedSha256": None,
            "toolObservedSha256": None,
        }:
            failures.append(failure("PRESENTMON_ABSENT_RECORD_INVALID"))

        digest_relative = policy["evidence"]["manifestDigestName"]
        digest_file = resolve_file(evidence_root / digest_relative, "MANIFEST_DIGEST_MISSING")
        expected_digest = f"{manifest_sha} *{policy['evidence']['manifestName']}\n"
        if digest_file.read_text(encoding="ascii") != expected_digest:
            failures.append(failure("MANIFEST_DIGEST_INVALID"))

        allowed = {
            policy["evidence"]["launchRecordName"],
            policy["evidence"]["manifestName"],
            policy["evidence"]["manifestDigestName"],
            *expected_shots,
        }
        if pm_relative is not None:
            allowed.add(pm_relative)
        actual: set[str] = set()
        actual_directories: set[str] = set()
        for path in evidence_root.rglob("*"):
            relative = canonical_relative(path.relative_to(evidence_root).as_posix())
            if is_reparse(path):
                failures.append(failure("EVIDENCE_REPARSE_POINT", relative))
            elif path.is_file():
                actual.add(relative)
            elif path.is_dir():
                actual_directories.add(relative)
            else:
                failures.append(failure("EVIDENCE_NON_REGULAR_ENTRY", relative))
        for relative in sorted(actual - allowed):
            failures.append(failure("UNKNOWN_EVIDENCE_FILE", relative))
        for relative in sorted(allowed - actual):
            failures.append(failure("MISSING_EVIDENCE_FILE", relative))
        approved_directories = {policy["screenshots"]["relativeDirectory"]}
        for relative in sorted(actual_directories - approved_directories):
            failures.append(failure("UNKNOWN_EVIDENCE_DIRECTORY", relative))

        if manifest.get("candidateEvidenceBindingSha256") != candidate_binding(manifest):
            failures.append(failure("CANDIDATE_EVIDENCE_BINDING_INVALID"))
    except EvidenceError as exc:
        failures.append(failure(exc.code, exc.relative_path))
    except (OSError, UnicodeError, json.JSONDecodeError, csv.Error, KeyError, TypeError, ValueError):
        failures.append(failure("UNEXPECTED_FAIL_CLOSED_VALIDATION_ERROR"))

    unique = {(item["code"], item.get("relativePath")): item for item in failures}
    receipt["failures"] = sorted(unique.values(), key=lambda item: (item["code"], item.get("relativePath", "")))
    if not receipt["failures"]:
        receipt["state"] = "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
    return receipt


def write_json_exclusive(path: Path, value: Any) -> None:
    data = json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(data)


def _png(width: int = 640, height: int = 360) -> bytes:
    def chunk(name: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + (b"\x00\x00\x00" * width) for _ in range(height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def _self_test() -> dict[str, Any]:
    # Importing the runner here avoids a production dependency cycle while
    # exercising the exact manifest writer used by real runs.
    import run_dg_session19_external_technical_evidence as runner

    passed = 0
    candidate_id = "S19_WindowsShipping_20990101T000000Z_selftest"
    with tempfile.TemporaryDirectory(prefix="dg_s19_ext_evidence_") as temporary:
        base = Path(temporary)
        archive = base / "archive"
        executable = archive / "DiscGolfTour" / "Binaries" / "Win64" / "DiscGolfTour-Win64-Shipping.exe"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"MZ" + (b"\x00" * 126))
        (archive / "payload.bin").write_bytes(b"candidate-payload")
        user_token = "12345678-1234-4234-8234-123456789abc"
        user_dir = base / "users" / user_token
        user_dir.mkdir(parents=True)
        output_parent = base / "evidence"
        output_parent.mkdir()
        archive_binding = collect_archive(archive)
        exe_hash = sha256_file(executable)
        policy, policy_binding = load_policy()
        run_dir = output_parent / f"{candidate_id}_{user_token}"
        screenshots_dir = run_dir / policy["screenshots"]["relativeDirectory"]
        screenshots_dir.mkdir(parents=True)
        now = datetime.now(timezone.utc)
        for name in policy["screenshots"]["requiredFiles"]:
            path = screenshots_dir / name
            path.write_bytes(_png())
            os.utime(path, (now.timestamp(), now.timestamp()))
        manifest_path = runner.build_fixture_manifest(
            run_dir=run_dir,
            candidate_id=candidate_id,
            archive_binding=archive_binding,
            exe_relative=executable.relative_to(archive).as_posix(),
            exe_sha256=exe_hash,
            user_token=user_token,
            pid=4242,
            started=now - timedelta(seconds=2),
            finished=now,
            policy=policy,
            policy_binding=policy_binding,
        )
        valid = validate_evidence(archive_root=archive, shipping_exe=executable, user_dir=user_dir, manifest_path=manifest_path)
        if valid["state"] != "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE":
            raise AssertionError([item["code"] for item in valid["failures"]])
        passed += 1

        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

        presentmon_dir = output_parent / "valid-presentmon"
        runner.copy_fixture_run(run_dir, presentmon_dir)
        pm_path = presentmon_dir / policy["presentMon"]["relativePath"]
        pm_path.write_text(
            "Application,ProcessID,msBetweenPresents\n"
            f"{executable.name},4242,16.667\n",
            encoding="utf-8",
            newline="\n",
        )
        pm_manifest_path = presentmon_dir / policy["evidence"]["manifestName"]
        pm_manifest = copy.deepcopy(manifest)
        pm_manifest["evidence"]["presentMon"] = {
            "requested": True,
            "relativePath": policy["presentMon"]["relativePath"],
            "bytes": pm_path.stat().st_size,
            "sha256": sha256_file(pm_path),
            "rowCount": 1,
            "columns": ["Application", "ProcessID", "msBetweenPresents"],
            "processIdRowsMatched": True,
            "applicationRowsMatched": True,
            "toolExpectedSha256": "A" * 64,
            "toolObservedSha256": "A" * 64,
        }
        pm_manifest["candidateEvidenceBindingSha256"] = candidate_binding(pm_manifest)
        pm_manifest_path.write_text(
            json.dumps(pm_manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        pm_digest = presentmon_dir / policy["evidence"]["manifestDigestName"]
        pm_digest.write_text(
            f"{sha256_file(pm_manifest_path)} *{policy['evidence']['manifestName']}\n",
            encoding="ascii",
            newline="\n",
        )
        pm_valid = validate_evidence(
            archive_root=archive,
            shipping_exe=executable,
            user_dir=user_dir,
            manifest_path=pm_manifest_path,
        )
        if pm_valid["state"] != "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE":
            raise AssertionError([item["code"] for item in pm_valid["failures"]])
        passed += 1

        bad_pm_dir = output_parent / "mutation-presentmon-pid"
        runner.copy_fixture_run(presentmon_dir, bad_pm_dir)
        (bad_pm_dir / policy["presentMon"]["relativePath"]).write_text(
            "Application,ProcessID,msBetweenPresents\n"
            f"{executable.name},7,16.667\n",
            encoding="utf-8",
            newline="\n",
        )
        bad_pm = validate_evidence(
            archive_root=archive,
            shipping_exe=executable,
            user_dir=user_dir,
            manifest_path=bad_pm_dir / policy["evidence"]["manifestName"],
        )
        if "PRESENTMON_CONTENT_INVALID" not in {
            item["code"] for item in bad_pm["failures"]
        }:
            raise AssertionError("PresentMon PID mismatch was not rejected")
        passed += 1

        mutations: list[tuple[str, Any, str]] = [
            ("claim", lambda m: m["claimBoundary"].__setitem__("visualProductApproval", True), "MANIFEST_CLAIM_BOUNDARY_INVALID"),
            ("schema-version-bool", lambda m: m.__setitem__("schemaVersion", True), "MANIFEST_SCHEMA_INVALID"),
            ("exe-hash", lambda m: m["candidate"].__setitem__("expectedExeSha256", "0" * 64), "EXECUTABLE_HASH_MISMATCH"),
            ("archive-hash", lambda m: m["candidate"].__setitem__("expectedArchiveManifestSha256", "0" * 64), "EXPECTED_ARCHIVE_HASH_MISMATCH"),
            ("exit", lambda m: m["process"].__setitem__("exitCode", 1), "PROCESS_COMPLETION_INVALID"),
            ("exit-bool", lambda m: m["process"].__setitem__("exitCode", False), "PROCESS_COMPLETION_INVALID"),
            ("pid-bool", lambda m: m["process"].__setitem__("pid", True), "PROCESS_ID_INVALID"),
            ("timeout", lambda m: m["process"].__setitem__("timedOut", True), "PROCESS_COMPLETION_INVALID"),
            ("user-token", lambda m: m["userDir"].__setitem__("token", "00000000-0000-4000-8000-000000000000"), "USERDIR_RECORD_INVALID"),
            ("launch-arg", lambda m: m["launch"]["sanitizedArguments"].__setitem__(0, "-nullrhi"), "LAUNCH_ARGUMENTS_INVALID"),
            ("binding", lambda m: m.__setitem__("candidateEvidenceBindingSha256", "0" * 64), "CANDIDATE_EVIDENCE_BINDING_INVALID"),
            ("screenshot-hash", lambda m: m["evidence"]["screenshots"][0].__setitem__("sha256", "0" * 64), "SCREENSHOT_BINDING_INVALID"),
            ("shot-set", lambda m: m["evidence"]["screenshots"].pop(), "SCREENSHOT_SET_INVALID"),
        ]
        for index, (name, mutate, expected_code) in enumerate(mutations):
            case_dir = output_parent / f"mutation-{index:02d}-{name}"
            runner.copy_fixture_run(run_dir, case_dir)
            case_manifest_path = case_dir / policy["evidence"]["manifestName"]
            value = copy.deepcopy(manifest)
            mutate(value)
            case_manifest_path.chmod(stat.S_IWRITE | stat.S_IREAD)
            case_manifest_path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
            digest = sha256_file(case_manifest_path)
            digest_path = case_dir / policy["evidence"]["manifestDigestName"]
            digest_path.chmod(stat.S_IWRITE | stat.S_IREAD)
            digest_path.write_text(f"{digest} *{policy['evidence']['manifestName']}\n", encoding="ascii", newline="\n")
            result = validate_evidence(archive_root=archive, shipping_exe=executable, user_dir=user_dir, manifest_path=case_manifest_path)
            codes = {item["code"] for item in result["failures"]}
            if result["state"].startswith("PASS") or expected_code not in codes:
                raise AssertionError(f"{name}: {sorted(codes)}")
            passed += 1

        unknown = output_parent / "mutation-unknown"
        runner.copy_fixture_run(run_dir, unknown)
        (unknown / "unbound.txt").write_text("unbound", encoding="utf-8")
        result = validate_evidence(archive_root=archive, shipping_exe=executable, user_dir=user_dir, manifest_path=unknown / policy["evidence"]["manifestName"])
        if "UNKNOWN_EVIDENCE_FILE" not in {item["code"] for item in result["failures"]}:
            raise AssertionError("unknown evidence file was not rejected")
        passed += 1

        missing = output_parent / "mutation-missing"
        runner.copy_fixture_run(run_dir, missing)
        (missing / policy["screenshots"]["relativeDirectory"] / policy["screenshots"]["requiredFiles"][0]).unlink()
        result = validate_evidence(archive_root=archive, shipping_exe=executable, user_dir=user_dir, manifest_path=missing / policy["evidence"]["manifestName"])
        if not {"SCREENSHOT_MISSING", "MISSING_EVIDENCE_FILE"} & {item["code"] for item in result["failures"]}:
            raise AssertionError("missing screenshot was not rejected")
        passed += 1

        # Manifest v2 must be independently justified by every raw JSONL event;
        # the inventory's validationState is never an authority of its own.
        runtime_dir = output_parent / "valid-runtime-journal"
        runtime_screenshots = runtime_dir / policy["screenshots"]["relativeDirectory"]
        runtime_screenshots.mkdir(parents=True)
        for name in policy["screenshots"]["requiredFiles"]:
            destination = runtime_screenshots / name
            destination.write_bytes(_png())
            os.utime(destination, (now.timestamp(), now.timestamp()))
        capture_nonce = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
        round_id = "87654321-4321-4321-8321-cba987654321"
        runtime_started = now - timedelta(seconds=2)
        runtime_finished = now
        journal_path = user_dir / Path(RUNTIME_JOURNAL_RELATIVE)
        journal_path.parent.mkdir(parents=True)
        runtime_bytes = runner._self_test_runtime_journal_bytes(
            candidate_id=candidate_id,
            user_token=user_token,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            round_id=round_id,
            started_utc=utc_text(runtime_started + timedelta(seconds=1)),
        )
        journal_path.write_bytes(runtime_bytes)
        inventory = runner.inspect_runtime_journal(
            user_dir=user_dir,
            candidate_id=candidate_id,
            user_token=user_token,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            started=runtime_started,
            finished=runtime_finished,
            timestamp_tolerance_seconds=policy["screenshots"][
                "timestampToleranceSeconds"
            ],
            normal_exit=True,
        )
        launch = {
            "schema": LAUNCH_SCHEMA_V2,
            "schemaVersion": 2,
            "session": 19,
            "recordedUtc": utc_text(runtime_started),
            "policySha256": policy_binding["sha256"],
            "candidateId": candidate_id,
            "exeRelativePath": executable.relative_to(archive).as_posix(),
            "expectedExeSha256": exe_hash,
            "observedExeSha256": exe_hash,
            "expectedArchiveManifestSha256": archive_binding[
                "canonicalManifestSha256"
            ],
            "observedArchiveManifestSha256": archive_binding[
                "canonicalManifestSha256"
            ],
            "userDirToken": user_token,
            "captureNonce": capture_nonce,
            "runtimeCheckpointJournalUserDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
            "sanitizedArguments": policy["launch"]["requiredArguments"]
            + [f"-UserDir=<EXTERNAL_UUID:{user_token}>"],
            "pid": 4343,
            "hostPathsRecorded": False,
            "claimBoundary": policy["claimBoundary"],
        }
        runtime_launch_path = runtime_dir / policy["evidence"]["launchRecordName"]
        runner.write_json_exclusive(runtime_launch_path, launch)
        runner.make_read_only(runtime_launch_path)
        runtime_manifest_path = runner.build_manifest(
            run_dir=runtime_dir,
            candidate_id=candidate_id,
            archive_before=archive_binding,
            archive_after=archive_binding,
            exe_relative=executable.relative_to(archive).as_posix(),
            expected_exe_sha256=exe_hash,
            observed_exe_sha256=exe_hash,
            expected_archive_sha256=archive_binding["canonicalManifestSha256"],
            user_token=user_token,
            pid=4343,
            started=runtime_started,
            finished=runtime_finished,
            exit_code=0,
            timed_out=False,
            policy=policy,
            policy_binding=policy_binding,
            presentmon_requested=False,
            presentmon_tool_expected_sha256=None,
            presentmon_tool_observed_sha256=None,
            runtime_journal=inventory,
        )
        runtime_valid = validate_evidence(
            archive_root=archive,
            shipping_exe=executable,
            user_dir=user_dir,
            manifest_path=runtime_manifest_path,
        )
        if runtime_valid["state"] != (
            "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
        ):
            raise AssertionError(
                [item["code"] for item in runtime_valid["failures"]]
            )
        passed += 1
        runtime_manifest = json.loads(
            runtime_manifest_path.read_text(encoding="utf-8")
        )

        parsed_runtime_records = [
            json.loads(line.decode("ascii"))
            for line in runtime_bytes[:-1].split(b"\n")
        ]

        def encode_runtime(records: list[dict[str, Any]]) -> bytes:
            return b"".join(
                json.dumps(
                    record,
                    sort_keys=True,
                    separators=(",", ":"),
                    ensure_ascii=True,
                ).encode("ascii")
                + b"\n"
                for record in records
            )

        forged_journals: list[tuple[str, bytes]] = []
        arbitrary_records = copy.deepcopy(parsed_runtime_records)
        arbitrary_records[5] = {}
        forged_journals.append(("arbitrary-event-payload", encode_runtime(arbitrary_records)))
        reordered_records = copy.deepcopy(parsed_runtime_records)
        reordered_records[4], reordered_records[5] = (
            reordered_records[5],
            reordered_records[4],
        )
        forged_journals.append(("event-reordering", encode_runtime(reordered_records)))
        totals_records = copy.deepcopy(parsed_runtime_records)
        totals_records[7]["totalStrokes"] = 99
        forged_journals.append(("score-total-forgery", encode_runtime(totals_records)))
        boolean_records = copy.deepcopy(parsed_runtime_records)
        boolean_records[2]["sequence"] = True
        forged_journals.append(("boolean-event-integer", encode_runtime(boolean_records)))
        duplicate_lines = runtime_bytes[:-1].split(b"\n")
        duplicate_lines[2] = duplicate_lines[2][:-1] + b',"sequence":2}'
        forged_journals.append(("duplicate-event-key", b"\n".join(duplicate_lines) + b"\n"))

        for index, (name, forged_raw) in enumerate(forged_journals):
            journal_path.write_bytes(forged_raw)
            case_dir = output_parent / f"runtime-forgery-{index:02d}-{name}"
            runner.copy_fixture_run(runtime_dir, case_dir)
            case_manifest_path = case_dir / policy["evidence"]["manifestName"]
            case_manifest = copy.deepcopy(runtime_manifest)
            case_inventory = case_manifest["evidence"]["runtimeCheckpointJournal"]
            case_inventory["bytes"] = len(forged_raw)
            case_inventory["sha256"] = sha256_bytes(forged_raw)
            # Model an attacker who rewrites every untrusted inventory and
            # digest field while retaining the asserted PASS state.
            case_manifest["candidateEvidenceBindingSha256"] = candidate_binding(
                case_manifest
            )
            case_manifest_path.chmod(stat.S_IWRITE | stat.S_IREAD)
            case_manifest_path.write_text(
                json.dumps(case_manifest, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            case_digest_path = case_dir / policy["evidence"]["manifestDigestName"]
            case_digest_path.chmod(stat.S_IWRITE | stat.S_IREAD)
            case_digest_path.write_text(
                f"{sha256_file(case_manifest_path)} *{policy['evidence']['manifestName']}\n",
                encoding="ascii",
                newline="\n",
            )
            forged_result = validate_evidence(
                archive_root=archive,
                shipping_exe=executable,
                user_dir=user_dir,
                manifest_path=case_manifest_path,
            )
            forged_codes = {item["code"] for item in forged_result["failures"]}
            if forged_result["state"].startswith("PASS") or not any(
                code.startswith("RUNTIME_JOURNAL_") for code in forged_codes
            ):
                raise AssertionError(f"{name}: {sorted(forged_codes)}")
            passed += 1
        journal_path.write_bytes(runtime_bytes)

        race_path = base / "stable-read-race.bin"
        race_path.write_bytes(b"before")
        race_rejected = False
        try:
            _read_file_stable(
                race_path,
                1024,
                _post_read_hook=lambda path: path.write_bytes(b"after-change"),
            )
        except (EvidenceError, PermissionError, OSError):
            race_rejected = True
        if not race_rejected:
            raise AssertionError("changed-during-read mutation was accepted")
        passed += 1

    return {"schema": RECEIPT_SCHEMA + ".SelfTest.v1", "state": "PASS", "testsPassed": passed}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate candidate-bound external Shipping technical evidence.")
    parser.add_argument("--archive-root")
    parser.add_argument("--shipping-exe")
    parser.add_argument("--user-dir")
    parser.add_argument("--manifest")
    parser.add_argument("--output")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        try:
            result = _self_test()
        except Exception as exc:
            result = {"schema": RECEIPT_SCHEMA + ".SelfTest.v1", "state": "FAIL", "errorType": type(exc).__name__, "error": str(exc)[:500]}
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result["state"] == "PASS" else 1
    if not all((args.archive_root, args.shipping_exe, args.user_dir, args.manifest)):
        result = {"schema": RECEIPT_SCHEMA, "state": "FAIL_INPUT", "failures": [{"code": "MISSING_REQUIRED_ARGUMENT"}]}
    else:
        result = validate_evidence(
            archive_root=Path(args.archive_root),
            shipping_exe=Path(args.shipping_exe),
            user_dir=Path(args.user_dir),
            manifest_path=Path(args.manifest),
        )
    if args.output:
        try:
            output = Path(args.output)
            if output.resolve().parent == Path(args.manifest).resolve().parent:
                raise EvidenceError("OUTPUT_MUST_BE_OUTSIDE_EVIDENCE_RUN")
            write_json_exclusive(output, result)
        except (OSError, EvidenceError):
            result["state"] = "FAIL_OUTPUT"
            result.setdefault("failures", []).append({"code": "OUTPUT_WRITE_REJECTED"})
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result.get("state") == "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE" else 1


if __name__ == "__main__":
    sys.exit(main())
