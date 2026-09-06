#!/usr/bin/env python3
"""Run the exact guarded three-hole Shipping performance capture externally."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any, Optional, Sequence
import uuid

from validate_dg_session19_shipping_performance import (
    ARCHIVE_MANIFEST_SCHEMA,
    EXECUTABLE_RELATIVE,
    LAUNCHER_RELATIVE,
    LAUNCH_SCHEMA,
    PROCESS_LOG_SCHEMA,
    RUN_MANIFEST_SCHEMA,
    EvidenceError,
    archive_inventory,
    build_archive_manifest,
    file_binding,
    required_arguments,
    utc_now,
    validate_receipt_output_path,
    validate_run,
    write_exclusive_json,
)


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_ENV_PREFIX = "DGT_S19_CAPTURE_"


def is_same_or_under(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def relative_binding(run_root: Path, path: Path) -> dict[str, Any]:
    return {"relativePath": path.relative_to(run_root).as_posix(), **file_binding(path)}


def write_process_log(
    path: Path, candidate_id: str, run_id: str, hole: int, token: str,
    started: datetime, finished: datetime, pid: int, exit_code: int,
    stdout: bytes, stderr: bytes,
) -> dict[str, Any]:
    elapsed = (finished - started).total_seconds()
    value = {
        "schema": PROCESS_LOG_SCHEMA,
        "schemaVersion": 1,
        "candidateId": candidate_id,
        "runId": run_id,
        "holeNumber": hole,
        "userDirToken": token,
        "startedUtc": started.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "finishedUtc": finished.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "elapsedSeconds": elapsed,
        "pid": pid,
        "exitCode": exit_code,
        "stdout": stdout.decode("utf-8", errors="replace"),
        "stderr": stderr.decode("utf-8", errors="replace"),
    }
    write_exclusive_json(path, value)
    return value


def preflight(
    archive: Path, candidate_id: str, external_root: Path,
    receipt_output: Optional[Path] = None,
) -> None:
    if not archive.is_dir():
        raise EvidenceError("Shipping archive is missing")
    if not (archive / LAUNCHER_RELATIVE).is_file() \
            or not (archive / EXECUTABLE_RELATIVE).is_file():
        raise EvidenceError("Shipping archive launcher or primary executable is missing")
    if is_same_or_under(external_root, archive) or is_same_or_under(external_root, ROOT):
        raise EvidenceError("external root must be outside the archive and project roots")
    # build_archive_manifest validates the candidate grammar and complete archive.
    build_archive_manifest(archive, candidate_id)
    if receipt_output is not None:
        validate_receipt_output_path(receipt_output, archive, external_root)
    external_root.mkdir(parents=True, exist_ok=True)


def capture(
    archive: Path, candidate_id: str, external_root: Path,
    timeout_seconds: int, receipt_output: Optional[Path],
) -> tuple[Path, dict[str, Any]]:
    archive = archive.resolve()
    external_root = external_root.resolve()
    preflight(archive, candidate_id, external_root, receipt_output)
    run_id = str(uuid.uuid4())
    run_root = external_root / "ShippingPerformance" / candidate_id / run_id
    if receipt_output is not None:
        validate_receipt_output_path(
            receipt_output, archive, external_root, run_root
        )
    try:
        run_root.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        raise EvidenceError("generated run UUID already exists") from None
    (run_root / "LaunchRecords").mkdir()
    (run_root / "ProcessLogs").mkdir()
    (run_root / "UserDirs").mkdir()

    archive_manifest = build_archive_manifest(archive, candidate_id)
    if archive_manifest["schema"] != ARCHIVE_MANIFEST_SCHEMA:
        raise EvidenceError("internal archive-manifest schema mismatch")
    archive_manifest_path = run_root / "ArchiveManifest.json"
    write_exclusive_json(archive_manifest_path, archive_manifest)
    before_sha = archive_manifest["inventorySha256"]
    executable = archive / EXECUTABLE_RELATIVE
    launch_records: list[dict[str, Any]] = []

    child_environment = {
        key: value for key, value in os.environ.items()
        if not key.upper().startswith(CAPTURE_ENV_PREFIX)
    }
    creation_flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    for hole in (1, 2, 3):
        token = str(uuid.uuid4())
        user_dir = run_root / "UserDirs" / token
        if user_dir.exists():
            raise EvidenceError("generated UserDir UUID already exists")
        actual_arguments = required_arguments(hole, str(user_dir.resolve()), False)
        sanitized_arguments = required_arguments(hole, token, True)
        started = datetime.now(timezone.utc)
        process = subprocess.Popen(
            [str(executable), *actual_arguments],
            cwd=str(executable.parent),
            env=child_environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=creation_flags,
        )
        try:
            stdout, stderr = process.communicate(timeout=timeout_seconds)
            exit_code = int(process.returncode)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate()
            exit_code = -1
        finished = datetime.now(timezone.utc)
        process_log_path = run_root / "ProcessLogs" / f"Hole{hole}.json"
        process_log = write_process_log(
            process_log_path, candidate_id, run_id, hole, token,
            started, finished, process.pid, exit_code, stdout, stderr,
        )

        capture_directory = user_dir / "Saved" / "PerformanceCaptures"
        timestamped = sorted(capture_directory.glob("Performance_*.json"))
        latest = capture_directory / "LatestPerformance.json"
        guard = capture_directory / "ReleasePerformanceCapture.guard"
        if len(timestamped) != 1 or not latest.is_file() or not guard.is_file():
            raise EvidenceError(
                f"hole {hole} did not emit the exclusive guard, exactly one timestamped "
                "schema-v2 report, and Latest"
            )
        record = {
            "schema": LAUNCH_SCHEMA,
            "schemaVersion": 1,
            "session": 19,
            "candidateId": candidate_id,
            "runId": run_id,
            "holeNumber": hole,
            "userDirToken": token,
            "sanitizedArguments": sanitized_arguments,
            "archiveInventorySha256": before_sha,
            "shippingExecutableSha256": archive_manifest["shippingExecutable"]["sha256"],
            "startedUtc": process_log["startedUtc"],
            "finishedUtc": process_log["finishedUtc"],
            "elapsedSeconds": process_log["elapsedSeconds"],
            "pid": process.pid,
            "exitCode": exit_code,
            "processLog": relative_binding(run_root, process_log_path),
            "performanceReport": relative_binding(run_root, timestamped[0]),
            "latestReport": relative_binding(run_root, latest),
            "captureGuard": relative_binding(run_root, guard),
        }
        record_path = run_root / "LaunchRecords" / f"Hole{hole}.json"
        write_exclusive_json(record_path, record)
        launch_records.append({"holeNumber": hole, "record": relative_binding(run_root, record_path)})
        if exit_code != 0:
            raise EvidenceError(f"hole {hole} Shipping process exited {exit_code}")

    _, after_sha, _ = archive_inventory(archive)
    run_manifest = {
        "schema": RUN_MANIFEST_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "runId": run_id,
        "generatedUtc": utc_now(),
        "archiveManifest": relative_binding(run_root, archive_manifest_path),
        "archiveBeforeInventorySha256": before_sha,
        "archiveAfterInventorySha256": after_sha,
        "archivePreserved": before_sha == after_sha,
        "launcher": {
            "relativePath": LAUNCHER_RELATIVE,
            **file_binding(archive / LAUNCHER_RELATIVE),
        },
        "shippingExecutable": {
            "relativePath": EXECUTABLE_RELATIVE,
            **file_binding(executable),
        },
        "launchRecords": launch_records,
    }
    write_exclusive_json(run_root / "RunManifest.json", run_manifest)
    receipt = validate_run(run_root, archive, candidate_id)
    if receipt_output is not None:
        if not receipt["passed"]:
            raise EvidenceError("three-hole run failed validation; no PASS receipt was emitted")
        output = validate_receipt_output_path(
            receipt_output, archive, external_root, run_root
        )
        write_exclusive_json(output, receipt)
        post_write_receipt = validate_run(run_root, archive, candidate_id)
        if (not post_write_receipt["passed"]
                or post_write_receipt["archive"] != receipt["archive"]
                or post_write_receipt["runManifest"] != receipt["runManifest"]
                or post_write_receipt["runs"] != receipt["runs"]):
            raise EvidenceError(
                "run/archive validation changed after external receipt write"
            )
    return run_root, receipt


def run_self_test() -> int:
    checks = 0
    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)
    with __import__("tempfile").TemporaryDirectory(prefix="dg-s19-perf-runner-") as temporary:
        temp = Path(temporary)
        candidate = "S19_WindowsShipping_20990101T000000Z_abcdef123456"
        archive = temp / candidate / "Windows"
        (archive / Path(EXECUTABLE_RELATIVE).parent).mkdir(parents=True)
        (archive / LAUNCHER_RELATIVE).write_bytes(b"launcher")
        (archive / EXECUTABLE_RELATIVE).write_bytes(b"shipping")
        external = temp / "External"
        # A temporary archive and external sibling exercise only preflight and
        # manifest composition; self-test never launches a process.
        preflight(archive, candidate, external)
        manifest = build_archive_manifest(archive, candidate)
        require(manifest["candidateId"] == candidate, "candidate binding differs")
        require(manifest["fileCount"] == 2, "archive inventory differs")
        require(len(manifest["inventorySha256"]) == 64, "archive digest differs")
        token = "11111111-2222-4333-8444-555555555555"
        require(required_arguments(2, token, True)[1] == "-Hole=2", "hole argument differs")
        require(required_arguments(2, token, True)[-1] == f"-UserDir=<EXTERNAL_UUID:{token}>",
                "sanitized UserDir argument differs")
        try:
            preflight(archive, candidate, archive / "Nested")
        except EvidenceError:
            nested_rejected = True
        else:
            nested_rejected = False
        require(nested_rejected, "archive-local external root must be rejected")
        try:
            preflight(archive, candidate, external, archive / "Receipt.json")
        except EvidenceError:
            archive_receipt_rejected = True
        else:
            archive_receipt_rejected = False
        require(archive_receipt_rejected, "archive-local receipt output must be rejected")
        try:
            preflight(archive, candidate[:-1] + "7", external)
        except EvidenceError:
            mislabeled_candidate_rejected = True
        else:
            mislabeled_candidate_rejected = False
        require(mislabeled_candidate_rejected,
                "archive path must exactly match the declared candidate ID")
    print(f"Session 19 Shipping performance runner self-test OK ({checks} assertions; no process launched).")
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--external-root", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=180)
    parser.add_argument("--receipt-output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if any(value is not None for value in (
            args.archive, args.candidate_id, args.external_root, args.receipt_output,
        )):
            print("--self-test accepts no capture arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (AssertionError, EvidenceError, OSError) as exc:
            print(f"Shipping performance runner self-test FAILED: {exc}", file=sys.stderr)
            return 1
    if args.archive is None or args.candidate_id is None or args.external_root is None:
        print("--archive, --candidate-id, and --external-root are required", file=sys.stderr)
        return 2
    if not 60 <= args.timeout_seconds <= 600:
        print("--timeout-seconds must be between 60 and 600", file=sys.stderr)
        return 2
    try:
        run_root, receipt = capture(
            args.archive, args.candidate_id, args.external_root,
            args.timeout_seconds, args.receipt_output,
        )
    except (EvidenceError, OSError, subprocess.SubprocessError) as exc:
        print(f"Shipping performance capture failed closed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({
        "state": receipt["state"], "passed": receipt["passed"],
        "candidateId": receipt["candidateId"], "runToken": run_root.name,
        "runCount": len(receipt["runs"]), "receiptWritten": args.receipt_output is not None,
        "releaseReady": False,
    }, indent=2, sort_keys=True))
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
