#!/usr/bin/env python3
"""Run one external, candidate-bound Session 19 Shipping evidence session.

The runner creates a new evidence directory, launches the exact Shipping binary
with a fresh external UUID UserDir, inventories ten operator-created PNGs, and
strictly binds the game-emitted three-hole runtime JSONL journal. PresentMon
collection is optional. It never evaluates visual quality, invents gameplay
events, or records human approval.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import validate_dg_session19_external_technical_evidence as evidence


RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_RELATIVE = "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
MANIFEST_SCHEMA_V2 = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
LAUNCH_SCHEMA_V2 = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_JOURNAL_MAX_BYTES = 262144
CAPTURE_ENVIRONMENT_NAMES = {
    "DGT_S19_CAPTURE_CANDIDATE_ID",
    "DGT_S19_CAPTURE_USERDIR_TOKEN",
    "DGT_S19_CAPTURE_EXE_SHA256",
    "DGT_S19_CAPTURE_ARCHIVE_MANIFEST_SHA256",
    "DGT_S19_CAPTURE_NONCE",
}
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


class RuntimeJournalError(ValueError):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


def _strict_json_line(raw: bytes) -> dict[str, Any]:
    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise RuntimeJournalError("JOURNAL_DUPLICATE_JSON_KEY")
            result[key] = value
        return result

    try:
        value = json.loads(
            raw.decode("ascii"),
            object_pairs_hook=reject_duplicates,
            parse_constant=lambda _value: (_ for _ in ()).throw(
                RuntimeJournalError("JOURNAL_NONFINITE_JSON_VALUE")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise RuntimeJournalError("JOURNAL_JSON_INVALID") from exc
    if not isinstance(value, dict):
        raise RuntimeJournalError("JOURNAL_RECORD_NOT_OBJECT")
    canonical = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    if raw != canonical:
        raise RuntimeJournalError("JOURNAL_RECORD_NOT_CANONICAL")
    return value


def _exact_keys(value: Any, expected: set[str], code: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != expected:
        raise RuntimeJournalError(code)
    return value


def _integer(value: Any, code: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise RuntimeJournalError(code)
    return value


def _canonical_uuid(value: Any, code: str, *, version_four: bool = False) -> str:
    if not isinstance(value, str):
        raise RuntimeJournalError(code)
    try:
        parsed = uuid.UUID(value)
    except ValueError as exc:
        raise RuntimeJournalError(code) from exc
    if str(parsed) != value or (version_four and parsed.version != 4):
        raise RuntimeJournalError(code)
    return value


def _utc(value: Any, code: str) -> datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise RuntimeJournalError(code)
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise RuntimeJournalError(code) from exc
    if parsed.tzinfo is None:
        raise RuntimeJournalError(code)
    return parsed.astimezone(timezone.utc)


def inspect_runtime_journal(
    *,
    user_dir: Path,
    candidate_id: str,
    user_token: str,
    exe_sha256: str,
    archive_sha256: str,
    capture_nonce: str,
    started: datetime,
    finished: datetime,
    timestamp_tolerance_seconds: int,
    normal_exit: bool,
) -> dict[str, Any]:
    """Inventory and strictly validate the raw game-emitted journal.

    This function is read-only. It never creates, repairs, translates, or
    supplements a gameplay event.
    """

    journal_path = user_dir / Path(RUNTIME_JOURNAL_RELATIVE)
    failures: list[str] = []
    size = 0
    digest: str | None = None
    modified_utc: str | None = None
    modified_within_window = False
    observed_schema: str | None = None
    observed_nonce: str | None = None
    round_id: str | None = None
    event_count = 0
    first_sequence: int | None = None
    last_sequence: int | None = None
    try:
        resolved_user = user_dir.resolve(strict=True)
        resolved_journal = journal_path.resolve(strict=True)
        if not evidence.within(resolved_journal, resolved_user):
            raise RuntimeJournalError("JOURNAL_PATH_OUTSIDE_USERDIR")
        if evidence.is_reparse(journal_path) or not journal_path.is_file():
            raise RuntimeJournalError("JOURNAL_NOT_REGULAR_FILE")
        before = journal_path.stat()
        if before.st_size <= 0 or before.st_size > RUNTIME_JOURNAL_MAX_BYTES:
            raise RuntimeJournalError("JOURNAL_SIZE_INVALID")
        raw = journal_path.read_bytes()
        after = journal_path.stat()
        if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
            raise RuntimeJournalError("JOURNAL_CHANGED_DURING_READ")
        size = len(raw)
        digest = evidence.sha256_bytes(raw)
        modified = datetime.fromtimestamp(after.st_mtime, timezone.utc)
        modified_utc = evidence.utc_text(modified)
        tolerance = timestamp_tolerance_seconds
        modified_within_window = (
            started.timestamp() - tolerance
            <= after.st_mtime
            <= finished.timestamp() + tolerance
        )
        if not modified_within_window:
            raise RuntimeJournalError("JOURNAL_MTIME_OUTSIDE_PROCESS_WINDOW")
        if b"\r" in raw or not raw.endswith(b"\n"):
            raise RuntimeJournalError("JOURNAL_LINE_ENDINGS_INVALID")
        raw_lines = raw[:-1].split(b"\n")
        if len(raw_lines) != 13 or any(not line for line in raw_lines):
            raise RuntimeJournalError("JOURNAL_RECORD_COUNT_INVALID")
        records = [_strict_json_line(line) for line in raw_lines]

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
            "JOURNAL_HEADER_FIELDS_INVALID",
        )
        observed_schema = header["schema"] if isinstance(header["schema"], str) else None
        observed_nonce = header["captureNonce"] if isinstance(header["captureNonce"], str) else None
        round_id = _canonical_uuid(
            header["roundId"], "JOURNAL_ROUND_ID_INVALID", version_four=True
        )
        _canonical_uuid(
            header["captureNonce"], "JOURNAL_CAPTURE_NONCE_INVALID", version_four=True
        )
        _canonical_uuid(header["userDirToken"], "JOURNAL_USER_TOKEN_INVALID")
        header_started = _utc(header["startedUtc"], "JOURNAL_STARTED_UTC_INVALID")
        if (
            header["schema"] != RUNTIME_JOURNAL_SCHEMA
            or _integer(header["schemaVersion"], "JOURNAL_SCHEMA_VERSION_INVALID", 1) != 1
            or _integer(header["session"], "JOURNAL_SESSION_INVALID", 1) != 19
            or header["recordType"] != "HEADER"
            or header["candidateId"] != candidate_id
            or header["userDirToken"] != user_token
            or header["executableSha256"] != exe_sha256
            or header["archiveManifestSha256"] != archive_sha256
            or header["captureNonce"] != capture_nonce
            or header["captureNonce"] == header["userDirToken"]
            or round_id == header["captureNonce"]
            or round_id == header["userDirToken"]
            or not (
                started.timestamp() - timestamp_tolerance_seconds
                <= header_started.timestamp()
                <= finished.timestamp() + timestamp_tolerance_seconds
            )
        ):
            raise RuntimeJournalError("JOURNAL_HEADER_BINDING_INVALID")

        previous_ms = -1
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
            event = _exact_keys(
                record, expected_keys, "JOURNAL_EVENT_FIELDS_INVALID"
            )
            sequence = _integer(event["sequence"], "JOURNAL_SEQUENCE_INVALID", 1)
            monotonic_ms = _integer(
                event["monotonicMs"], "JOURNAL_MONOTONIC_TIME_INVALID", 1
            )
            completed = _integer(
                event["completedHoles"], "JOURNAL_COMPLETED_HOLES_INVALID"
            )
            strokes = _integer(event["totalStrokes"], "JOURNAL_STROKES_INVALID")
            penalties = _integer(
                event["totalPenalties"], "JOURNAL_PENALTIES_INVALID"
            )
            expected_hole = EXPECTED_RUNTIME_HOLES[index]
            if (
                event["recordType"] != "EVENT"
                or event["roundId"] != round_id
                or event["event"] != EXPECTED_RUNTIME_EVENTS[index]
                or event["holeNumber"] != expected_hole
                or sequence != index + 1
                or monotonic_ms <= previous_ms
                or completed != EXPECTED_RUNTIME_COMPLETED[index]
                or strokes < previous_strokes
                or penalties < previous_penalties
                or penalties > strokes
            ):
                raise RuntimeJournalError("JOURNAL_EVENT_SEQUENCE_INVALID")
            if index == 0 and (strokes != 0 or penalties != 0):
                raise RuntimeJournalError("JOURNAL_ROUND_DID_NOT_START_AT_ZERO")
            if index in (3, 6, 9):
                completed_totals.append((strokes, penalties))
            previous_ms = monotonic_ms
            previous_strokes = strokes
            previous_penalties = penalties
            events.append(event)

        maximum_elapsed_ms = max(
            0,
            int((finished - started).total_seconds() * 1000),
        ) + timestamp_tolerance_seconds * 1000
        if events[-1]["monotonicMs"] > maximum_elapsed_ms:
            raise RuntimeJournalError("JOURNAL_MONOTONIC_TIME_OUTSIDE_PROCESS_WINDOW")

        score = _exact_keys(
            events[-1]["finalScore"],
            {
                "completedHoles",
                "holeRows",
                "parTotal",
                "totalHoles",
                "totalPenalties",
                "totalStrokes",
            },
            "JOURNAL_FINAL_SCORE_FIELDS_INVALID",
        )
        rows_value = score["holeRows"]
        if not isinstance(rows_value, list) or len(rows_value) != 3:
            raise RuntimeJournalError("JOURNAL_FINAL_SCORE_ROWS_INVALID")
        rows: list[dict[str, int]] = []
        for index, value in enumerate(rows_value, start=1):
            row = _exact_keys(
                value,
                {"holeNumber", "par", "penalties", "strokes"},
                "JOURNAL_FINAL_SCORE_ROW_FIELDS_INVALID",
            )
            normalized = {
                "holeNumber": _integer(
                    row["holeNumber"], "JOURNAL_FINAL_HOLE_NUMBER_INVALID", 1
                ),
                "par": _integer(row["par"], "JOURNAL_FINAL_PAR_INVALID", 1),
                "penalties": _integer(
                    row["penalties"], "JOURNAL_FINAL_PENALTIES_INVALID"
                ),
                "strokes": _integer(
                    row["strokes"], "JOURNAL_FINAL_STROKES_INVALID", 1
                ),
            }
            if (
                normalized["holeNumber"] != index
                or normalized["par"] != [3, 4, 4][index - 1]
                or normalized["penalties"] > normalized["strokes"]
            ):
                raise RuntimeJournalError("JOURNAL_FINAL_SCORE_ROW_INVALID")
            rows.append(normalized)
        final_strokes = _integer(
            score["totalStrokes"], "JOURNAL_FINAL_TOTAL_STROKES_INVALID", 1
        )
        final_penalties = _integer(
            score["totalPenalties"], "JOURNAL_FINAL_TOTAL_PENALTIES_INVALID"
        )
        if (
            _integer(score["completedHoles"], "JOURNAL_FINAL_COMPLETED_INVALID") != 3
            or _integer(score["totalHoles"], "JOURNAL_FINAL_HOLE_COUNT_INVALID", 1) != 3
            or _integer(score["parTotal"], "JOURNAL_FINAL_PAR_TOTAL_INVALID", 1) != 11
            or final_strokes != sum(row["strokes"] for row in rows)
            or final_penalties != sum(row["penalties"] for row in rows)
            or final_strokes != events[-1]["totalStrokes"]
            or final_penalties != events[-1]["totalPenalties"]
            or events[-2]["totalStrokes"] != final_strokes
            or events[-2]["totalPenalties"] != final_penalties
        ):
            raise RuntimeJournalError("JOURNAL_FINAL_SCORE_TOTALS_INVALID")
        prior_strokes = 0
        prior_penalties = 0
        for (complete_strokes, complete_penalties), row in zip(completed_totals, rows):
            if (
                complete_strokes - prior_strokes != row["strokes"]
                or complete_penalties - prior_penalties != row["penalties"]
            ):
                raise RuntimeJournalError("JOURNAL_HOLE_TOTALS_DO_NOT_RECONCILE")
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
                raise RuntimeJournalError("JOURNAL_SHOT_TRANSITIONS_INVALID")
        event_count = len(events)
        first_sequence = events[0]["sequence"]
        last_sequence = events[-1]["sequence"]
        if not normal_exit:
            raise RuntimeJournalError("JOURNAL_PROCESS_DID_NOT_EXIT_NORMALLY")
    except RuntimeJournalError as exc:
        failures.append(exc.code)
    except (OSError, KeyError, TypeError, ValueError):
        failures.append("JOURNAL_UNEXPECTED_VALIDATION_ERROR")

    return {
        "bytes": size,
        "captureNonce": observed_nonce,
        "eventCount": event_count,
        "firstSequence": first_sequence,
        "lastSequence": last_sequence,
        "modifiedUtc": modified_utc,
        "modifiedWithinRunWindow": modified_within_window,
        "roundId": round_id,
        "schema": observed_schema,
        "sha256": digest,
        "userDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
        "validationFailures": sorted(set(failures)),
        "validationState": (
            RUNTIME_JOURNAL_PASS if not failures else "FAIL_CLOSED_RUNTIME_JOURNAL"
        ),
    }


def write_bytes_exclusive(path: Path, value: bytes) -> None:
    with path.open("xb") as stream:
        stream.write(value)


def write_json_exclusive(path: Path, value: Any) -> None:
    encoded = (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n").encode("utf-8")
    write_bytes_exclusive(path, encoded)


def make_read_only(path: Path) -> None:
    path.chmod(stat.S_IREAD)


def screenshot_records(
    run_dir: Path,
    policy: dict[str, Any],
    started: datetime,
    finished: datetime,
) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    tolerance = policy["screenshots"]["timestampToleranceSeconds"]
    earliest = started.timestamp() - tolerance
    latest = finished.timestamp() + tolerance
    relative_dir = policy["screenshots"]["relativeDirectory"]
    for name in policy["screenshots"]["requiredFiles"]:
        relative = f"{relative_dir}/{name}"
        path = run_dir / relative
        if not path.is_file() or evidence.is_reparse(path):
            continue
        try:
            width, height = evidence.png_dimensions(path)
        except evidence.EvidenceError:
            # Preserve a final fail-closed manifest even when an operator put an
            # invalid file at a required checkpoint name.  The validator will
            # reject the incomplete record and the file remains inspectable.
            continue
        modified = datetime.fromtimestamp(path.stat().st_mtime, timezone.utc)
        records.append({
            "relativePath": relative,
            "bytes": path.stat().st_size,
            "sha256": evidence.sha256_file(path),
            "width": width,
            "height": height,
            "modifiedUtc": evidence.utc_text(modified),
            "modifiedWithinRunWindow": earliest <= path.stat().st_mtime <= latest,
        })
    return records


def read_presentmon(
    csv_path: Path,
    *,
    requested: bool,
    process_id: int,
    executable_name: str,
    tool_expected_sha256: str | None,
    tool_observed_sha256: str | None,
) -> dict[str, Any]:
    if not requested:
        return {
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
        }
    if not csv_path.is_file() or evidence.is_reparse(csv_path):
        return {
            "requested": True,
            "relativePath": csv_path.name,
            "bytes": 0,
            "sha256": None,
            "rowCount": 0,
            "columns": [],
            "processIdRowsMatched": False,
            "applicationRowsMatched": False,
            "toolExpectedSha256": tool_expected_sha256,
            "toolObservedSha256": tool_observed_sha256,
        }
    with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
        columns = list(reader.fieldnames or [])
    pid_matches = bool(rows) and all(str(row.get("ProcessID", "")).strip() == str(process_id) for row in rows)
    app_matches = bool(rows) and all(Path(str(row.get("Application", "")).strip()).name.casefold() == executable_name.casefold() for row in rows)
    return {
        "requested": True,
        "relativePath": csv_path.name,
        "bytes": csv_path.stat().st_size,
        "sha256": evidence.sha256_file(csv_path),
        "rowCount": len(rows),
        "columns": columns,
        "processIdRowsMatched": pid_matches,
        "applicationRowsMatched": app_matches,
        "toolExpectedSha256": tool_expected_sha256,
        "toolObservedSha256": tool_observed_sha256,
    }


def build_manifest(
    *,
    run_dir: Path,
    candidate_id: str,
    archive_before: dict[str, Any],
    archive_after: dict[str, Any],
    exe_relative: str,
    expected_exe_sha256: str,
    observed_exe_sha256: str,
    expected_archive_sha256: str,
    user_token: str,
    pid: int,
    started: datetime,
    finished: datetime,
    exit_code: int,
    timed_out: bool,
    policy: dict[str, Any],
    policy_binding: dict[str, Any],
    presentmon_requested: bool,
    presentmon_tool_expected_sha256: str | None,
    presentmon_tool_observed_sha256: str | None,
    runtime_journal: dict[str, Any] | None = None,
) -> Path:
    launch_name = policy["evidence"]["launchRecordName"]
    launch_path = run_dir / launch_name
    launch_binding = {
        "relativePath": launch_name,
        "bytes": launch_path.stat().st_size,
        "sha256": evidence.sha256_file(launch_path),
    }
    shots = screenshot_records(run_dir, policy, started, finished)
    pm = read_presentmon(
        run_dir / policy["presentMon"]["relativePath"],
        requested=presentmon_requested,
        process_id=pid,
        executable_name=policy["shippingExecutableBasename"],
        tool_expected_sha256=presentmon_tool_expected_sha256,
        tool_observed_sha256=presentmon_tool_observed_sha256,
    )
    evidence_record: dict[str, Any] = {
        "hostPathRecorded": False,
        "createNewRefuseOverwrite": True,
        "launchRecord": launch_binding,
        "screenshots": shots,
        "presentMon": pm,
    }
    if runtime_journal is not None:
        evidence_record["runtimeCheckpointJournal"] = runtime_journal
    manifest: dict[str, Any] = {
        "schema": MANIFEST_SCHEMA_V2 if runtime_journal is not None else evidence.MANIFEST_SCHEMA,
        "schemaVersion": 2 if runtime_journal is not None else 1,
        "session": 19,
        "generatedUtc": evidence.utc_text(datetime.now(timezone.utc)),
        "state": "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION",
        "policy": policy_binding,
        "candidate": {
            "candidateId": candidate_id,
            "exeRelativePath": exe_relative,
            "expectedExeSha256": expected_exe_sha256,
            "observedExeSha256": observed_exe_sha256,
            "expectedArchiveManifestSha256": expected_archive_sha256,
            "archiveBefore": archive_before,
            "archiveAfter": archive_after,
            "archivePreserved": archive_before == archive_after,
        },
        "userDir": {
            "token": user_token,
            "hostPathRecorded": False,
            "emptyBeforeLaunch": True,
            "externalBoundaryValidated": True,
            "reparsePointsRejected": True,
        },
        "launch": {
            "sanitizedArguments": policy["launch"]["requiredArguments"] + [f"-UserDir=<EXTERNAL_UUID:{user_token}>"] ,
            "workingDirectoryRelative": Path(exe_relative).parent.as_posix(),
            "additionalArgumentsUsed": False,
        },
        "process": {
            "pid": pid,
            "startedUtc": evidence.utc_text(started),
            "finishedUtc": evidence.utc_text(finished),
            "exitCode": exit_code,
            "timedOut": timed_out,
        },
        "evidence": evidence_record,
        "claimBoundary": policy["claimBoundary"],
        "candidateEvidenceBindingSha256": "",
    }
    manifest["candidateEvidenceBindingSha256"] = evidence.candidate_binding(manifest)
    manifest_path = run_dir / policy["evidence"]["manifestName"]
    write_json_exclusive(manifest_path, manifest)
    digest = evidence.sha256_file(manifest_path)
    digest_path = run_dir / policy["evidence"]["manifestDigestName"]
    write_bytes_exclusive(digest_path, f"{digest} *{manifest_path.name}\n".encode("ascii"))
    make_read_only(manifest_path)
    make_read_only(digest_path)
    return manifest_path


def build_fixture_manifest(
    *,
    run_dir: Path,
    candidate_id: str,
    archive_binding: dict[str, Any],
    exe_relative: str,
    exe_sha256: str,
    user_token: str,
    pid: int,
    started: datetime,
    finished: datetime,
    policy: dict[str, Any],
    policy_binding: dict[str, Any],
) -> Path:
    args = policy["launch"]["requiredArguments"] + [f"-UserDir=<EXTERNAL_UUID:{user_token}>"]
    launch = {
        "schema": evidence.LAUNCH_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "recordedUtc": evidence.utc_text(started),
        "policySha256": policy_binding["sha256"],
        "candidateId": candidate_id,
        "exeRelativePath": exe_relative,
        "expectedExeSha256": exe_sha256,
        "observedExeSha256": exe_sha256,
        "expectedArchiveManifestSha256": archive_binding["canonicalManifestSha256"],
        "observedArchiveManifestSha256": archive_binding["canonicalManifestSha256"],
        "userDirToken": user_token,
        "sanitizedArguments": args,
        "pid": pid,
        "hostPathsRecorded": False,
        "claimBoundary": policy["claimBoundary"],
    }
    launch_path = run_dir / policy["evidence"]["launchRecordName"]
    write_json_exclusive(launch_path, launch)
    make_read_only(launch_path)
    return build_manifest(
        run_dir=run_dir,
        candidate_id=candidate_id,
        archive_before=archive_binding,
        archive_after=archive_binding,
        exe_relative=exe_relative,
        expected_exe_sha256=exe_sha256,
        observed_exe_sha256=exe_sha256,
        expected_archive_sha256=archive_binding["canonicalManifestSha256"],
        user_token=user_token,
        pid=pid,
        started=started,
        finished=finished,
        exit_code=0,
        timed_out=False,
        policy=policy,
        policy_binding=policy_binding,
        presentmon_requested=False,
        presentmon_tool_expected_sha256=None,
        presentmon_tool_observed_sha256=None,
    )


def copy_fixture_run(source: Path, destination: Path) -> None:
    shutil.copytree(source, destination)
    for path in destination.rglob("*"):
        if path.is_file():
            path.chmod(stat.S_IWRITE | stat.S_IREAD)


def _self_test_runtime_journal_bytes(
    *,
    candidate_id: str,
    user_token: str,
    exe_sha256: str,
    archive_sha256: str,
    capture_nonce: str,
    round_id: str,
    started_utc: str,
) -> bytes:
    """Construct only an explicitly synthetic self-test fixture."""

    header = {
        "archiveManifestSha256": archive_sha256,
        "candidateId": candidate_id,
        "captureNonce": capture_nonce,
        "executableSha256": exe_sha256,
        "recordType": "HEADER",
        "roundId": round_id,
        "schema": RUNTIME_JOURNAL_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "startedUtc": started_utc,
        "userDirToken": user_token,
    }
    totals = [
        (0, 0), (0, 0), (1, 0), (2, 0),
        (2, 0), (3, 0), (4, 0),
        (4, 0), (5, 0), (6, 0), (6, 0), (6, 0),
    ]
    records: list[dict[str, Any]] = [header]
    for index, name in enumerate(EXPECTED_RUNTIME_EVENTS):
        record: dict[str, Any] = {
            "completedHoles": EXPECTED_RUNTIME_COMPLETED[index],
            "event": name,
            "holeNumber": EXPECTED_RUNTIME_HOLES[index],
            "monotonicMs": index + 1,
            "recordType": "EVENT",
            "roundId": round_id,
            "sequence": index + 1,
            "totalPenalties": totals[index][1],
            "totalStrokes": totals[index][0],
        }
        if name == "FINAL_SCORECARD":
            record["finalScore"] = {
                "completedHoles": 3,
                "holeRows": [
                    {"holeNumber": 1, "par": 3, "penalties": 0, "strokes": 2},
                    {"holeNumber": 2, "par": 4, "penalties": 0, "strokes": 2},
                    {"holeNumber": 3, "par": 4, "penalties": 0, "strokes": 2},
                ],
                "parTotal": 11,
                "totalHoles": 3,
                "totalPenalties": 0,
                "totalStrokes": 6,
            }
        records.append(record)
    return b"".join(
        json.dumps(record, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")
        + b"\n"
        for record in records
    )


def validate_inputs(args: argparse.Namespace, policy: dict[str, Any]) -> dict[str, Any]:
    if re.fullmatch(policy["candidateIdPattern"], args.candidate_id or "") is None:
        raise evidence.EvidenceError("CANDIDATE_ID_INVALID")
    if not evidence.HEX64_RE.fullmatch(args.expected_exe_sha256 or ""):
        raise evidence.EvidenceError("EXPECTED_EXE_SHA256_INVALID")
    if not evidence.HEX64_RE.fullmatch(args.expected_archive_manifest_sha256 or ""):
        raise evidence.EvidenceError("EXPECTED_ARCHIVE_SHA256_INVALID")
    archive = evidence.resolve_directory(Path(args.archive_root), "ARCHIVE_ROOT_INVALID")
    executable = evidence.resolve_file(Path(args.shipping_exe), "SHIPPING_EXECUTABLE_INVALID")
    users = evidence.resolve_directory(Path(args.user_dir), "USERDIR_INVALID")
    output = evidence.resolve_directory(Path(args.output_root), "OUTPUT_ROOT_INVALID")
    project = evidence.ROOT.resolve(strict=True)
    if executable.name != policy["shippingExecutableBasename"] or not evidence.within(executable, archive):
        raise evidence.EvidenceError("SHIPPING_EXECUTABLE_IDENTITY_INVALID")
    try:
        user_token = str(uuid.UUID(users.name))
    except ValueError as exc:
        raise evidence.EvidenceError("USERDIR_TOKEN_NOT_UUID") from exc
    if user_token != users.name:
        raise evidence.EvidenceError("USERDIR_TOKEN_NOT_CANONICAL")
    if any(users.iterdir()):
        raise evidence.EvidenceError("USERDIR_NOT_EMPTY")
    if any(evidence.within(users, parent) or evidence.within(parent, users) for parent in (archive, output, project)):
        raise evidence.EvidenceError("USERDIR_BOUNDARY_INVALID")
    if any(evidence.within(output, parent) or evidence.within(parent, output) for parent in (archive, users, project)):
        raise evidence.EvidenceError("OUTPUT_BOUNDARY_INVALID")
    timeout = args.timeout_seconds
    if not policy["launch"]["minimumTimeoutSeconds"] <= timeout <= policy["launch"]["maximumTimeoutSeconds"]:
        raise evidence.EvidenceError("TIMEOUT_OUT_OF_POLICY")
    presentmon = None
    pm_hash = None
    if args.presentmon_exe or args.expected_presentmon_sha256:
        if not args.presentmon_exe or not evidence.HEX64_RE.fullmatch(args.expected_presentmon_sha256 or ""):
            raise evidence.EvidenceError("PRESENTMON_BINDING_INCOMPLETE")
        presentmon = evidence.resolve_file(Path(args.presentmon_exe), "PRESENTMON_EXECUTABLE_INVALID")
        pm_hash = evidence.sha256_file(presentmon)
        if pm_hash != args.expected_presentmon_sha256:
            raise evidence.EvidenceError("PRESENTMON_HASH_MISMATCH")
    exe_hash = evidence.sha256_file(executable)
    archive_binding = evidence.collect_archive(archive)
    if exe_hash != args.expected_exe_sha256:
        raise evidence.EvidenceError("EXPECTED_EXE_HASH_MISMATCH")
    if archive_binding["canonicalManifestSha256"] != args.expected_archive_manifest_sha256:
        raise evidence.EvidenceError("EXPECTED_ARCHIVE_HASH_MISMATCH")
    return {
        "archive": archive,
        "executable": executable,
        "userDir": users,
        "outputRoot": output,
        "userToken": user_token,
        "exeHash": exe_hash,
        "archiveBinding": archive_binding,
        "presentMon": presentmon,
        "presentMonHash": pm_hash,
    }


def run(args: argparse.Namespace) -> dict[str, Any]:
    policy, policy_binding = evidence.load_policy()
    resolved = validate_inputs(args, policy)
    run_dir = resolved["outputRoot"] / f"{args.candidate_id}_{resolved['userToken']}"
    run_dir.mkdir()
    screenshots_dir = run_dir / policy["screenshots"]["relativeDirectory"]
    screenshots_dir.mkdir()
    print(json.dumps({
        "state": "READY_FOR_BOUNDED_TECHNICAL_CAPTURE",
        "screenshotDirectory": str(screenshots_dir),
        "requiredScreenshotFiles": policy["screenshots"]["requiredFiles"],
        "manualApprovalCaptured": False,
    }, indent=2), flush=True)

    actual_args = policy["launch"]["requiredArguments"] + [f"-UserDir={resolved['userDir']}"]
    sanitized_args = policy["launch"]["requiredArguments"] + [f"-UserDir=<EXTERNAL_UUID:{resolved['userToken']}>"]
    capture_nonce = str(uuid.uuid4())
    # Remove inherited capture variables case-insensitively, then add exactly
    # the five values independently verified before this launch.
    child_environment = {
        key: value
        for key, value in os.environ.items()
        if key.upper() not in CAPTURE_ENVIRONMENT_NAMES
    }
    child_environment.update({
        "DGT_S19_CAPTURE_CANDIDATE_ID": args.candidate_id,
        "DGT_S19_CAPTURE_USERDIR_TOKEN": resolved["userToken"],
        "DGT_S19_CAPTURE_EXE_SHA256": resolved["exeHash"],
        "DGT_S19_CAPTURE_ARCHIVE_MANIFEST_SHA256": resolved["archiveBinding"]["canonicalManifestSha256"],
        "DGT_S19_CAPTURE_NONCE": capture_nonce,
    })
    started = datetime.now(timezone.utc)
    game = subprocess.Popen(
        [str(resolved["executable"]), *actual_args],
        cwd=str(resolved["executable"].parent),
        stdin=subprocess.DEVNULL,
        env=child_environment,
    )
    launch = {
        "schema": LAUNCH_SCHEMA_V2,
        "schemaVersion": 2,
        "session": 19,
        "recordedUtc": evidence.utc_text(started),
        "policySha256": policy_binding["sha256"],
        "candidateId": args.candidate_id,
        "exeRelativePath": resolved["executable"].relative_to(resolved["archive"]).as_posix(),
        "expectedExeSha256": args.expected_exe_sha256,
        "observedExeSha256": resolved["exeHash"],
        "expectedArchiveManifestSha256": args.expected_archive_manifest_sha256,
        "observedArchiveManifestSha256": resolved["archiveBinding"]["canonicalManifestSha256"],
        "userDirToken": resolved["userToken"],
        "captureNonce": capture_nonce,
        "runtimeCheckpointJournalUserDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
        "sanitizedArguments": sanitized_args,
        "pid": game.pid,
        "hostPathsRecorded": False,
        "claimBoundary": policy["claimBoundary"],
    }
    launch_path = run_dir / policy["evidence"]["launchRecordName"]
    write_json_exclusive(launch_path, launch)
    make_read_only(launch_path)

    presentmon_process: subprocess.Popen[bytes] | None = None
    if resolved["presentMon"] is not None:
        template = policy["presentMon"]["runnerArguments"]
        csv_path = run_dir / policy["presentMon"]["relativePath"]
        pm_args = [value.replace("{PID}", str(game.pid)).replace("{CSV}", str(csv_path)) for value in template]
        presentmon_process = subprocess.Popen(
            [str(resolved["presentMon"]), *pm_args],
            cwd=str(run_dir),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    timed_out = False
    try:
        exit_code = game.wait(timeout=args.timeout_seconds)
    except subprocess.TimeoutExpired:
        timed_out = True
        game.terminate()
        try:
            exit_code = game.wait(timeout=10)
        except subprocess.TimeoutExpired:
            game.kill()
            exit_code = game.wait(timeout=10)
    finished = datetime.now(timezone.utc)
    if presentmon_process is not None:
        try:
            presentmon_process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            presentmon_process.terminate()
            try:
                presentmon_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                presentmon_process.kill()
                presentmon_process.wait(timeout=5)
    time.sleep(0.1)
    archive_after = evidence.collect_archive(resolved["archive"])
    observed_exe_after = evidence.sha256_file(resolved["executable"])
    runtime_journal = inspect_runtime_journal(
        user_dir=resolved["userDir"],
        candidate_id=args.candidate_id,
        user_token=resolved["userToken"],
        exe_sha256=resolved["exeHash"],
        archive_sha256=resolved["archiveBinding"]["canonicalManifestSha256"],
        capture_nonce=capture_nonce,
        started=started,
        finished=finished,
        timestamp_tolerance_seconds=policy["screenshots"]["timestampToleranceSeconds"],
        normal_exit=exit_code == policy["launch"]["requiredExitCode"] and not timed_out,
    )
    manifest_path = build_manifest(
        run_dir=run_dir,
        candidate_id=args.candidate_id,
        archive_before=resolved["archiveBinding"],
        archive_after=archive_after,
        exe_relative=resolved["executable"].relative_to(resolved["archive"]).as_posix(),
        expected_exe_sha256=args.expected_exe_sha256,
        observed_exe_sha256=observed_exe_after,
        expected_archive_sha256=args.expected_archive_manifest_sha256,
        user_token=resolved["userToken"],
        pid=game.pid,
        started=started,
        finished=finished,
        exit_code=exit_code,
        timed_out=timed_out,
        policy=policy,
        policy_binding=policy_binding,
        presentmon_requested=resolved["presentMon"] is not None,
        presentmon_tool_expected_sha256=args.expected_presentmon_sha256,
        presentmon_tool_observed_sha256=resolved["presentMonHash"],
        runtime_journal=runtime_journal,
    )
    validation = evidence.validate_evidence(
        archive_root=resolved["archive"],
        shipping_exe=resolved["executable"],
        user_dir=resolved["userDir"],
        manifest_path=manifest_path,
    )
    return {
        "state": validation["state"],
        "candidateId": args.candidate_id,
        "userDirToken": resolved["userToken"],
        "manifest": str(manifest_path),
        "manifestSha256": evidence.sha256_file(manifest_path),
        "runtimeCheckpointJournal": runtime_journal,
        "validationFailures": validation["failures"],
        "claimBoundary": validation["claimBoundary"],
    }


def self_test() -> dict[str, Any]:
    policy, _binding = evidence.load_policy()
    passed = 0
    with tempfile.TemporaryDirectory(prefix="dg_s19_runner_") as temporary:
        base = Path(temporary)
        archive = base / "archive"
        exe = archive / "DiscGolfTour" / "Binaries" / "Win64" / policy["shippingExecutableBasename"]
        exe.parent.mkdir(parents=True)
        exe.write_bytes(b"MZ-self-test")
        user = base / "users" / "12345678-1234-4234-8234-123456789abc"
        user.mkdir(parents=True)
        output = base / "output"
        output.mkdir()
        archive_binding = evidence.collect_archive(archive)
        exe_hash = evidence.sha256_file(exe)
        base_args = argparse.Namespace(
            candidate_id="S19_WindowsShipping_20990101T000000Z_runner",
            archive_root=str(archive), shipping_exe=str(exe), user_dir=str(user),
            output_root=str(output), expected_exe_sha256=exe_hash,
            expected_archive_manifest_sha256=archive_binding["canonicalManifestSha256"],
            presentmon_exe=None, expected_presentmon_sha256=None,
            timeout_seconds=policy["launch"]["defaultTimeoutSeconds"],
        )
        validate_inputs(base_args, policy)
        passed += 1
        cases = [
            ("candidate", "candidate_id", "bad"),
            ("exe-hash-format", "expected_exe_sha256", "0" * 63),
            ("archive-hash", "expected_archive_manifest_sha256", "0" * 64),
            ("timeout-low", "timeout_seconds", 1),
        ]
        for _name, field, value in cases:
            candidate = argparse.Namespace(**vars(base_args))
            setattr(candidate, field, value)
            try:
                validate_inputs(candidate, policy)
            except evidence.EvidenceError:
                passed += 1
            else:
                raise AssertionError(f"runner mutation accepted: {_name}")

        capture_nonce = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
        round_id = "87654321-4321-4321-8321-cba987654321"
        journal_path = user / Path(RUNTIME_JOURNAL_RELATIVE)
        journal_path.parent.mkdir(parents=True)
        started = datetime.now(timezone.utc)
        journal_bytes = _self_test_runtime_journal_bytes(
            candidate_id=base_args.candidate_id,
            user_token=user.name,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            round_id=round_id,
            started_utc=evidence.utc_text(started),
        )
        journal_path.write_bytes(journal_bytes)
        finished = datetime.now(timezone.utc)
        inventory = inspect_runtime_journal(
            user_dir=user,
            candidate_id=base_args.candidate_id,
            user_token=user.name,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            started=started,
            finished=finished,
            timestamp_tolerance_seconds=5,
            normal_exit=True,
        )
        if inventory["validationState"] != RUNTIME_JOURNAL_PASS:
            raise AssertionError(inventory["validationFailures"])
        passed += 1

        journal_path.write_bytes(journal_bytes.replace(b'"session":19', b'"session": 19', 1))
        mutated = inspect_runtime_journal(
            user_dir=user,
            candidate_id=base_args.candidate_id,
            user_token=user.name,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            started=started,
            finished=datetime.now(timezone.utc),
            timestamp_tolerance_seconds=5,
            normal_exit=True,
        )
        if mutated["validationState"] == RUNTIME_JOURNAL_PASS:
            raise AssertionError("noncanonical journal bytes accepted")
        passed += 1

        journal_path.write_bytes(journal_bytes)
        wrong_nonce = inspect_runtime_journal(
            user_dir=user,
            candidate_id=base_args.candidate_id,
            user_token=user.name,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce="ffffffff-ffff-4fff-8fff-ffffffffffff",
            started=started,
            finished=datetime.now(timezone.utc),
            timestamp_tolerance_seconds=5,
            normal_exit=True,
        )
        if wrong_nonce["validationState"] == RUNTIME_JOURNAL_PASS:
            raise AssertionError("wrong launch nonce accepted")
        passed += 1

        abnormal = inspect_runtime_journal(
            user_dir=user,
            candidate_id=base_args.candidate_id,
            user_token=user.name,
            exe_sha256=exe_hash,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            started=started,
            finished=datetime.now(timezone.utc),
            timestamp_tolerance_seconds=5,
            normal_exit=False,
        )
        if abnormal["validationState"] == RUNTIME_JOURNAL_PASS:
            raise AssertionError("abnormal process exit accepted")
        passed += 1

        (user / "stale.sav").write_bytes(b"GVAS")
        try:
            validate_inputs(base_args, policy)
        except evidence.EvidenceError as exc:
            if exc.code != "USERDIR_NOT_EMPTY":
                raise
            passed += 1
        else:
            raise AssertionError("non-empty UserDir accepted")
    return {"schema": evidence.MANIFEST_SCHEMA + ".RunnerSelfTest.v1", "state": "PASS", "testsPassed": passed}


def main(argv: list[str] | None = None) -> int:
    policy, _binding = evidence.load_policy()
    parser = argparse.ArgumentParser(description="Launch external candidate-bound Session 19 Shipping technical evidence.")
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive-root")
    parser.add_argument("--shipping-exe")
    parser.add_argument("--expected-exe-sha256")
    parser.add_argument("--expected-archive-manifest-sha256")
    parser.add_argument("--user-dir")
    parser.add_argument("--output-root")
    parser.add_argument("--presentmon-exe")
    parser.add_argument("--expected-presentmon-sha256")
    parser.add_argument("--timeout-seconds", type=int, default=policy["launch"]["defaultTimeoutSeconds"])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        try:
            result = self_test()
        except Exception as exc:
            result = {"schema": evidence.MANIFEST_SCHEMA + ".RunnerSelfTest.v1", "state": "FAIL", "errorType": type(exc).__name__, "error": str(exc)[:500]}
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result["state"] == "PASS" else 1
    try:
        result = run(args)
    except evidence.EvidenceError as exc:
        result = {"state": "FAIL_CLOSED_RUNNER_INPUT", "failures": [{"code": exc.code}]}
    except (OSError, subprocess.SubprocessError, ValueError, TypeError):
        result = {"state": "FAIL_CLOSED_RUNNER_ERROR", "failures": [{"code": "UNEXPECTED_RUNNER_ERROR"}]}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result.get("state") == "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE" else 1


if __name__ == "__main__":
    sys.exit(main())
