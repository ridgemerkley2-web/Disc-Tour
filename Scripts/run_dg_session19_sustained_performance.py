#!/usr/bin/env python3
"""Guarded external runner for a predeclared sustained Shipping capture.

Capture mode launches the unchanged bound Shipping executable with a fixed,
write-contained argument allowlist, records PRE/POST archive and host snapshots,
samples nvidia-smi once per second, and runs a timed PresentMon capture.  It does
not add Unreal trace flags: the installed Unreal Insights executable has no
reliable external attach/export CLI for this Shipping process.  Consequently,
capture mode deliberately stops without a RunManifest and reports that a real
trace plus timing export are still required.

Finalize mode accepts explicit external Unreal trace/timing artifacts from the
same capture window, validates them, creates ThreadProfile.json, and publishes
the exact flat RunManifest last.  Neither mode emits a technical receipt or
claims human performance, product-owner, or release acceptance.
"""

from __future__ import annotations

import argparse
import csv
import ctypes
from datetime import datetime, timedelta, timezone
import io
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Any, Optional, Sequence

import audit_dg_session19_performance_certification_readiness as readiness
import generate_dg_session19_sustained_performance_declaration as declaration_tool
import validate_dg_session19_shipping_performance as shipping
import validate_dg_session19_sustained_performance as certification


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_STATE_SCHEMA = "DiscGolfTour.Session19SustainedShippingCaptureState.v1"
CAPTURE_STATE_NAME = "CaptureState.json"
FAILURE_NAME = "CaptureFailure.json"
PRESENTMON_RAW_NAME = "PresentMonRaw.csv"
FIXED_GAME_ARGUMENTS = (
    "-Course=PineRidge",
    "-Hole=1",
    "-ResX=1920",
    "-ResY=1080",
    "-ForceRes",
    "-dx12",
    "-NoLoadExistingSave",
    "-DGNoProfileWrites",
)
FORBIDDEN_ARGUMENT_PREFIXES = (
    "-trace", "-stat", "-exec", "-benchmark", "-nullrhi", "-fixedseed",
    "-fps", "-usefixedtimestep", "-log", "-abslog",
)
PRESENTMON_REQUIRED_HELP = (
    "--process_id", "--output_file", "--timed", "--terminate_after_timed",
    "--session_name", "--v1_metrics",
)


class RunnerError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RunnerError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def parse_utc(value: Any, label: str) -> datetime:
    try:
        return shipping.parse_utc(value, label)
    except shipping.EvidenceError as exc:
        raise RunnerError(str(exc)) from None


def resolve_regular_tool(explicit: Optional[Path], defaults: Sequence[Path], label: str) -> Path:
    candidates = ([explicit] if explicit is not None else []) + list(defaults)
    for candidate in candidates:
        if candidate is None:
            continue
        try:
            resolved = candidate.resolve(strict=True)
        except (OSError, RuntimeError):
            continue
        if resolved.is_file() and not shipping.is_reparse_point(resolved):
            return resolved
    raise RunnerError(f"{label} executable is unavailable")


class VS_FIXEDFILEINFO(ctypes.Structure):
    _fields_ = [
        ("dwSignature", ctypes.c_uint32), ("dwStrucVersion", ctypes.c_uint32),
        ("dwFileVersionMS", ctypes.c_uint32), ("dwFileVersionLS", ctypes.c_uint32),
        ("dwProductVersionMS", ctypes.c_uint32), ("dwProductVersionLS", ctypes.c_uint32),
        ("dwFileFlagsMask", ctypes.c_uint32), ("dwFileFlags", ctypes.c_uint32),
        ("dwFileOS", ctypes.c_uint32), ("dwFileType", ctypes.c_uint32),
        ("dwFileSubtype", ctypes.c_uint32), ("dwFileDateMS", ctypes.c_uint32),
        ("dwFileDateLS", ctypes.c_uint32),
    ]


def windows_file_version(path: Path) -> str:
    if os.name != "nt":
        return "NON_WINDOWS_SELF_TEST"
    version = ctypes.WinDLL("version", use_last_error=True)
    version.GetFileVersionInfoSizeW.argtypes = [
        ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint32)
    ]
    version.GetFileVersionInfoSizeW.restype = ctypes.c_uint32
    version.GetFileVersionInfoW.argtypes = [
        ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p
    ]
    version.GetFileVersionInfoW.restype = ctypes.c_int
    version.VerQueryValueW.argtypes = [
        ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_uint),
    ]
    version.VerQueryValueW.restype = ctypes.c_int
    ignored = ctypes.c_uint32()
    size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(ignored))
    if not size:
        raise RunnerError(f"{path.name} has no readable Windows version resource")
    buffer = ctypes.create_string_buffer(size)
    require(bool(version.GetFileVersionInfoW(str(path), 0, size, buffer)),
            f"{path.name} version resource could not be read")
    pointer = ctypes.c_void_p()
    length = ctypes.c_uint()
    require(bool(version.VerQueryValueW(buffer, "\\", ctypes.byref(pointer),
                                        ctypes.byref(length))),
            f"{path.name} fixed version resource is missing")
    info = ctypes.cast(pointer, ctypes.POINTER(VS_FIXEDFILEINFO)).contents
    require(info.dwSignature == 0xFEEF04BD, f"{path.name} version signature differs")
    return ".".join(str(value) for value in (
        info.dwFileVersionMS >> 16, info.dwFileVersionMS & 0xFFFF,
        info.dwFileVersionLS >> 16, info.dwFileVersionLS & 0xFFFF,
    ))


def tool_binding(path: Path) -> dict[str, Any]:
    binding = shipping.file_binding(path)
    try:
        version = windows_file_version(path)
    except RunnerError:
        # The signed FrameViewSDK PresentMon binary carries no VERSIONINFO.
        # Record that fact while retaining its exact immutable identity.
        version = f"UNVERSIONED_BINARY_SHA256_{binding['sha256']}"
    return {**binding, "version": version}


def run_command(args: Sequence[str], timeout_seconds: int = 15) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            list(args), check=False, capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=timeout_seconds,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", exc.__class__.__name__
    return completed.returncode, completed.stdout, completed.stderr


def validate_presentmon_capability(path: Path) -> None:
    code, stdout, stderr = run_command((str(path), "--help"), 15)
    text = stdout + "\n" + stderr
    # This FrameViewSDK build returns 1 after printing valid --help to stderr.
    require(code in {0, 1} and all(token in text for token in PRESENTMON_REQUIRED_HELP),
            "PresentMon CLI does not expose the exact required timed-capture switches")


def parse_nvidia_rows(text: str, fields: Sequence[str]) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    try:
        for values in csv.reader(io.StringIO(text)):
            if not values or all(not value.strip() for value in values):
                continue
            require(len(values) == len(fields), "nvidia-smi field count differs")
            rows.append(dict(zip(fields, (value.strip() for value in values))))
    except csv.Error:
        raise RunnerError("nvidia-smi output is malformed CSV") from None
    return rows


def query_nvidia(path: Path, fields: Sequence[str]) -> dict[str, str]:
    code, stdout, _ = run_command((
        str(path), f"--query-gpu={','.join(fields)}", "--format=csv,noheader,nounits",
    ), 15)
    require(code == 0, "nvidia-smi query failed")
    rows = parse_nvidia_rows(stdout, fields)
    require(len(rows) == 1, "exactly one NVIDIA GPU is required for this policy")
    row = rows[0]
    for field, value in row.items():
        require(value not in {"", "N/A", "[N/A]", "Not Supported"},
                f"nvidia-smi field {field} is unavailable")
    return row


def effective_power_limit_field(contract: dict[str, Any]) -> str:
    fields = contract.get("requiredNvidiaSmiQueryFields")
    require(type(fields) is list, "nvidia-smi query field contract is missing")
    declared = contract.get("effectivePowerLimitQueryField", "power.limit")
    require(declared in {"power.limit", "enforced.power.limit"},
            "effective power-limit query field is unsupported")
    require(fields.count(declared) == 1,
            "effective power-limit query field is not declared exactly once")
    other = ({"power.limit", "enforced.power.limit"} - {declared})
    require(not any(field in fields for field in other),
            "multiple power-limit semantics are forbidden")
    return declared


def nvidia_row_to_sensor(
    row: dict[str, str], index: int, timestamp: str, contract: dict[str, Any]
) -> list[Any]:
    mapping = tuple(contract["requiredNvidiaSmiQueryFields"])
    effective_power_limit_field(contract)
    require(tuple(row) == mapping, "nvidia-smi query order differs")
    return [index, timestamp, *(row[key] for key in mapping)]


def active_power_snapshot() -> dict[str, Any]:
    scheme = readiness.query_power_scheme()
    system = readiness.query_system_power_status()
    require(scheme.get("available") is True, "active Windows power scheme is unavailable")
    require(system.get("available") is True, "Windows AC-line status is unavailable")
    return {
        "activeSchemeGuid": scheme["schemeGuid"],
        "activeSchemeName": scheme["schemeName"] or "UNNAMED",
        "acLineStatus": system["acLineStatus"],
        "batterySaverOn": bool(system.get("batterySaverOn")),
    }


def storage_snapshot(path: Path) -> dict[str, Any]:
    anchor = path.resolve().anchor
    require(bool(anchor), "external root has no storage-volume anchor")
    return {"volumeId": anchor.rstrip("\\/"), "freeBytes": shutil.disk_usage(anchor).free}


def host_snapshot(
    declaration: dict[str, Any], phase: str, nvidia_smi: Path, external_root: Path
) -> dict[str, Any]:
    fields = declaration["nvidiaSensorContract"]["requiredNvidiaSmiQueryFields"]
    power_limit_field = effective_power_limit_field(declaration["nvidiaSensorContract"])
    gpu = query_nvidia(nvidia_smi, fields)
    return {
        "schema": declaration["schemas"]["hostSnapshot"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "phase": phase,
        "capturedUtc": utc_now(),
        "gpu": {
            "name": gpu["name"], "uuid": gpu["uuid"],
            "driverVersion": gpu["driver_version"],
            "powerDrawW": float(gpu["power.draw"]),
            "powerLimitW": float(gpu[power_limit_field]),
        },
        "power": active_power_snapshot(),
        "storage": storage_snapshot(external_root),
    }


def archive_snapshot(
    declaration: dict[str, Any], phase: str, archive: Path
) -> dict[str, Any]:
    identity = declaration_tool.archive_identity(archive, declaration["candidateId"])
    require(identity == declaration["archive"], f"archive changed before {phase} snapshot")
    return {
        "schema": declaration["schemas"]["archiveSnapshot"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "phase": phase,
        "capturedUtc": utc_now(),
        "archive": identity,
    }


def write_exclusive_csv_header(path: Path, columns: Sequence[str]):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("x", encoding="utf-8", newline="", buffering=1)
    writer = csv.writer(handle, lineterminator="\n")
    writer.writerow(columns)
    handle.flush()
    os.fsync(handle.fileno())
    return handle, writer


def copy_exclusive(source: Path, target: Path) -> None:
    payload = certification.stable_bytes(source, source.name)
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        with target.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError:
        raise RunnerError(f"refusing to overwrite {target.name}") from None


def normalized_presentmon_rows(raw_path: Path, output_path: Path, contract: dict[str, Any], process_id: int) -> int:
    payload = certification.stable_bytes(raw_path, "raw PresentMon CSV", 256 * 1024 * 1024)
    try:
        text = payload.decode("utf-8-sig")
        reader = csv.DictReader(io.StringIO(text, newline=""), strict=True)
        require(reader.fieldnames is not None, "raw PresentMon header is missing")
        missing = set(contract["requiredCsvColumns"]) - set(reader.fieldnames)
        require(not missing, f"raw PresentMon columns are missing: {sorted(missing)}")
        rows = list(reader)
    except (UnicodeDecodeError, csv.Error):
        raise RunnerError("raw PresentMon CSV is malformed") from None
    require(bool(rows), "raw PresentMon CSV contains no frames")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        handle = output_path.open("x", encoding="utf-8", newline="")
    except FileExistsError:
        raise RunnerError("refusing to overwrite normalized PresentMon CSV") from None
    with handle:
        writer = csv.DictWriter(
            handle, fieldnames=contract["requiredCsvColumns"], lineterminator="\n",
        )
        writer.writeheader()
        for index, row in enumerate(rows):
            application = Path(row["Application"]).name
            require(application == contract["requiredApplication"],
                    f"PresentMon row {index} application differs")
            require(certification.csv_int(row["ProcessID"], "PresentMon process ID", 1)
                    == process_id, f"PresentMon row {index} process ID differs")
            projected = {key: row[key] for key in contract["requiredCsvColumns"]}
            projected["Application"] = application
            writer.writerow(projected)
        handle.flush()
        os.fsync(handle.fileno())
    return len(rows)


def fixed_game_arguments(user_dir: Path) -> list[str]:
    arguments = [*FIXED_GAME_ARGUMENTS, f"-UserDir={user_dir.resolve()}"]
    lowered = [value.casefold() for value in arguments]
    require(not any(value.startswith(FORBIDDEN_ARGUMENT_PREFIXES) for value in lowered),
            "fixed Shipping arguments unexpectedly contain a forbidden diagnostic flag")
    require(sum(value.startswith("-userdir=") for value in lowered) == 1,
            "fixed Shipping launch must contain exactly one external UserDir")
    return arguments


def sanitized_game_arguments() -> list[str]:
    return [*FIXED_GAME_ARGUMENTS, "-UserDir=<EXTERNAL_CERTIFICATION_USERDIR>"]


def validate_external_layout(
    external_root: Path, declaration_path: Path, archive: Path,
    prior_run_root: Path, run_id: str,
) -> dict[str, Path]:
    require(external_root.is_absolute(), "external root must be absolute")
    try:
        external_root = external_root.resolve(strict=True)
        declaration_path = declaration_path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise RunnerError(f"external root/declaration is missing ({exc.__class__.__name__})") from None
    for forbidden, label in ((ROOT, "project"), (archive, "archive"),
                             (prior_run_root, "three-hole run")):
        require(not shipping.is_same_or_under(external_root, forbidden),
                f"external root must be outside the {label}")
    require(shipping.is_same_or_under(declaration_path, external_root),
            "declaration must be inside the selected external root")
    paths = {
        "run": external_root / "Runs" / run_id,
        "working": external_root / "Working" / run_id,
        "userDir": external_root / "UserDirs" / run_id,
    }
    for path in paths.values():
        require(not shipping.is_same_or_under(path, ROOT)
                and not shipping.is_same_or_under(path, archive)
                and not shipping.is_same_or_under(path, prior_run_root),
                f"external output path is unsafe: {path.name}")
        require(not declaration_tool.path_chain_has_reparse(path.parent),
                f"external output parent contains a reparse point: {path.parent}")
    return paths


def preflight(
    *,
    declaration_path: Path,
    archive: Path,
    receipt_path: Path,
    prior_run_root: Path,
    candidate_id: str,
    external_root: Path,
    nvidia_smi: Path,
    presentmon: Path,
    for_capture: bool,
) -> tuple[dict[str, Any], dict[str, Path], dict[str, Any]]:
    archive = shipping.validate_candidate_archive_path(archive, candidate_id)
    declaration, _ = certification.validate_declaration(
        declaration_path, archive, receipt_path, prior_run_root, candidate_id,
        readiness.validate_receipt_against_run,
    )
    paths = validate_external_layout(
        external_root, declaration_path, archive, prior_run_root,
        declaration["certificationRunId"],
    )
    nvidia_smi = resolve_regular_tool(
        nvidia_smi, (Path(r"C:\Windows\System32\nvidia-smi.exe"),), "nvidia-smi"
    )
    presentmon = resolve_regular_tool(
        presentmon,
        (Path(r"C:\Program Files\NVIDIA Corporation\FrameViewSDK\bin\PresentMon_x64.exe"),),
        "PresentMon",
    )
    require(nvidia_smi.name.lower() == "nvidia-smi.exe", "nvidia-smi tool name differs")
    require(re.fullmatch(r"(?i:PresentMon(?:_x64)?\.exe)", presentmon.name) is not None,
            "PresentMon tool name differs")
    validate_presentmon_capability(presentmon)
    host = host_snapshot(declaration, "PRE", nvidia_smi, external_root)
    contract = declaration["hostSnapshotContract"]
    certification.validate_host_snapshot(
        host, phase="PRE", candidate_id=candidate_id,
        run_id=declaration["certificationRunId"],
        schema=declaration["schemas"]["hostSnapshot"], contract=contract,
    )
    require(declaration_tool.archive_identity(archive, candidate_id) == declaration["archive"],
            "live archive differs during preflight")
    if for_capture:
        for path in paths.values():
            require(not path.exists(), f"capture output already exists: {path}")
    inventory = {
        "nvidiaSmi": tool_binding(nvidia_smi),
        "presentMon": tool_binding(presentmon),
        "hostSnapshotValid": True,
        "archiveIdentityValid": True,
        "fixedShippingArguments": sanitized_game_arguments(),
        "automatedUnrealTraceExportAvailable": False,
    }
    return declaration, paths, inventory


def terminate_owned_process(process: Optional[subprocess.Popen[Any]]) -> None:
    if process is None or process.poll() is not None:
        return
    try:
        process.terminate()
        process.wait(timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        try:
            process.kill()
            process.wait(timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            pass


def capture(
    *,
    declaration_path: Path,
    declaration: dict[str, Any],
    paths: dict[str, Path],
    archive: Path,
    nvidia_smi: Path,
    presentmon: Path,
) -> dict[str, Any]:
    run_root, working, user_dir = paths["run"], paths["working"], paths["userDir"]
    for path in (run_root, working, user_dir):
        path.mkdir(parents=True, exist_ok=False)
    names = declaration["requiredArtifacts"]
    shipping.write_exclusive_json(
        run_root / names["archivePre"], archive_snapshot(declaration, "PRE", archive)
    )
    pre_host = host_snapshot(declaration, "PRE", nvidia_smi, paths["run"].parents[1])
    shipping.write_exclusive_json(run_root / names["hostPre"], pre_host)
    executable = archive / shipping.EXECUTABLE_RELATIVE
    game: Optional[subprocess.Popen[Any]] = None
    present: Optional[subprocess.Popen[Any]] = None
    sensor_handle = None
    run_started = utc_now()
    present_started = None
    present_finished = None
    process_id = None
    duration = declaration["declaredDurationSeconds"]
    raw_present = working / PRESENTMON_RAW_NAME
    try:
        game = subprocess.Popen(
            [str(executable), *fixed_game_arguments(user_dir)],
            cwd=str(executable.parent), stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        process_id = int(game.pid)
        present_started = utc_now()
        session_name = "DGT_S19_" + declaration["certificationRunId"].replace("-", "")[:20]
        present = subprocess.Popen(
            [
                str(presentmon), "--process_id", str(process_id),
                "--output_file", str(raw_present), "--timed", str(duration),
                "--terminate_after_timed", "--terminate_on_proc_exit",
                "--session_name", session_name, "--v1_metrics", "--no_console_stats",
            ],
            cwd=str(working), stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        sensor_path = run_root / names["nvidiaSensors"]
        sensor_handle, sensor_writer = write_exclusive_csv_header(
            sensor_path, declaration["nvidiaSensorContract"]["requiredCsvColumns"]
        )
        fields = declaration["nvidiaSensorContract"]["requiredNvidiaSmiQueryFields"]
        start_monotonic = time.monotonic()
        for index in range(duration + 1):
            deadline = start_monotonic + index
            remaining = deadline - time.monotonic()
            if remaining > 0:
                time.sleep(remaining)
            require(game.poll() is None, "Shipping process exited before capture completed")
            require(present.poll() is None or index == duration,
                    "PresentMon exited before capture completed")
            sensor_writer.writerow(nvidia_row_to_sensor(
                query_nvidia(nvidia_smi, fields), index, utc_now(),
                declaration["nvidiaSensorContract"],
            ))
            sensor_handle.flush()
            os.fsync(sensor_handle.fileno())
        run_finished = utc_now()
        sensor_handle.close()
        sensor_handle = None
        try:
            present.wait(timeout=30)
        except subprocess.TimeoutExpired:
            raise RunnerError("PresentMon did not terminate after its timed window") from None
        require(present.returncode == 0, f"PresentMon exited {present.returncode}")
        present_finished = utc_now()
        normalized = run_root / names["presentMonCsv"]
        normalized_presentmon_rows(
            raw_present, normalized, declaration["presentMonContract"], process_id
        )
        nvidia_capture = {
            "schema": declaration["schemas"]["nvidiaSensorCapture"],
            "schemaVersion": 1, "session": 19,
            "candidateId": declaration["candidateId"],
            "certificationRunId": declaration["certificationRunId"],
            "startedUtc": run_started, "finishedUtc": run_finished,
            "intervalMilliseconds": 1000,
            "tool": tool_binding(nvidia_smi), "queryFields": fields,
            "csv": certification.full_binding(run_root, run_root / names["nvidiaSensors"]),
        }
        shipping.write_exclusive_json(run_root / names["nvidiaSensorCapture"], nvidia_capture)
        present_capture = {
            "schema": declaration["schemas"]["presentMonCapture"],
            "schemaVersion": 1, "session": 19,
            "candidateId": declaration["candidateId"],
            "certificationRunId": declaration["certificationRunId"],
            "processId": process_id,
            "application": declaration["presentMonContract"]["requiredApplication"],
            "startedUtc": present_started, "finishedUtc": present_finished,
            "tool": tool_binding(presentmon),
            "csv": certification.full_binding(run_root, normalized),
        }
        shipping.write_exclusive_json(run_root / names["presentMonCapture"], present_capture)
        shipping.write_exclusive_json(
            run_root / names["archivePost"], archive_snapshot(declaration, "POST", archive)
        )
        post_host = host_snapshot(declaration, "POST", nvidia_smi, paths["run"].parents[1])
        shipping.write_exclusive_json(run_root / names["hostPost"], post_host)
        certification.validate_host_pair(pre_host, post_host, declaration["hostSnapshotContract"])
        state = {
            "schema": CAPTURE_STATE_SCHEMA, "schemaVersion": 1, "session": 19,
            "candidateId": declaration["candidateId"],
            "certificationRunId": declaration["certificationRunId"],
            "declaration": shipping.file_binding(declaration_path),
            "runStartedUtc": run_started, "runFinishedUtc": run_finished,
            "processId": process_id,
            "shippingExecutable": shipping.file_binding(executable),
            "sanitizedArguments": sanitized_game_arguments(),
            "rawPresentMon": shipping.file_binding(raw_present),
            "captureState": "CAPTURE_COMPLETE_REAL_UNREAL_TRACE_AND_TIMING_EXPORT_REQUIRED",
            "automatedUnrealTraceExportAttempted": False,
            "runManifestWritten": False,
            "claimBoundary": {
                "captureRecorded": True, "technicalValidationPassed": False,
                "humanPerformanceAcceptance": False, "releaseApproval": False,
                "releaseReady": False,
            },
        }
        shipping.write_exclusive_json(working / CAPTURE_STATE_NAME, state)
        return state
    except Exception as exc:
        failure = {
            "schema": "DiscGolfTour.Session19SustainedShippingCaptureFailure.v1",
            "candidateId": declaration["candidateId"],
            "certificationRunId": declaration["certificationRunId"],
            "failedUtc": utc_now(), "errorType": exc.__class__.__name__,
            "error": str(exc), "runManifestWritten": False,
            "humanPerformanceAcceptance": False, "releaseReady": False,
        }
        try:
            if not (working / FAILURE_NAME).exists():
                shipping.write_exclusive_json(working / FAILURE_NAME, failure)
        except (OSError, shipping.EvidenceError):
            pass
        raise
    finally:
        if sensor_handle is not None:
            sensor_handle.close()
        terminate_owned_process(present)
        terminate_owned_process(game)


def load_capture_state(path: Path, declaration: dict[str, Any], declaration_path: Path) -> dict[str, Any]:
    value = certification.load_json_stable(path, "capture state")
    certification.require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "declaration", "runStartedUtc", "runFinishedUtc", "processId",
        "shippingExecutable", "sanitizedArguments", "rawPresentMon", "captureState",
        "automatedUnrealTraceExportAttempted", "runManifestWritten", "claimBoundary",
    }, "capture state")
    require(value["schema"] == CAPTURE_STATE_SCHEMA and value["schemaVersion"] == 1,
            "capture state schema differs")
    require(value["candidateId"] == declaration["candidateId"]
            and value["certificationRunId"] == declaration["certificationRunId"],
            "capture state candidate/run differs")
    require(value["declaration"] == shipping.file_binding(declaration_path),
            "capture state declaration binding differs")
    require(value["captureState"] ==
            "CAPTURE_COMPLETE_REAL_UNREAL_TRACE_AND_TIMING_EXPORT_REQUIRED",
            "capture state is not ready for trace finalization")
    require(value["automatedUnrealTraceExportAttempted"] is False
            and value["runManifestWritten"] is False,
            "capture state overclaims trace export or manifest")
    require(value["sanitizedArguments"] == sanitized_game_arguments(),
            "capture state Shipping arguments differ")
    return value


def finalize(
    *,
    declaration_path: Path,
    declaration: dict[str, Any],
    paths: dict[str, Path],
    archive: Path,
    trace_source: Path,
    timing_source: Path,
    profile_started_utc: str,
    profile_finished_utc: str,
    unreal_insights: Path,
    trace_channels: Sequence[str],
) -> dict[str, Any]:
    run_root, working = paths["run"], paths["working"]
    require(run_root.is_dir() and working.is_dir(), "capture output is missing")
    state = load_capture_state(working / CAPTURE_STATE_NAME, declaration, declaration_path)
    names = declaration["requiredArtifacts"]
    expected_before = set(names.values()) - {
        names["runManifest"], names["threadProfile"], names["unrealInsightsTrace"],
        names["unrealInsightsTimingCsv"],
    }
    shipping.validate_exact_directory_entries(
        run_root, expected_before, set(), "capture-only sustained run"
    )
    trace_source = trace_source.resolve(strict=True)
    timing_source = timing_source.resolve(strict=True)
    for source in (trace_source, timing_source):
        require(source.is_file() and not shipping.is_reparse_point(source),
                f"external profile source is irregular: {source.name}")
        require(not shipping.is_same_or_under(source, ROOT)
                and not shipping.is_same_or_under(source, archive)
                and not shipping.is_same_or_under(source, run_root),
                f"external profile source is in a forbidden root: {source.name}")
    unreal_insights = resolve_regular_tool(
        unreal_insights,
        (Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealInsights.exe"),),
        "Unreal Insights",
    )
    require(unreal_insights.name == "UnrealInsights.exe",
            "Unreal Insights executable name differs")
    required_channels = declaration["profileContract"]["requiredTraceChannels"]
    require(list(trace_channels) == required_channels,
            "explicit observed trace channels must exactly match the declaration")
    started = parse_utc(profile_started_utc, "profile started UTC")
    finished = parse_utc(profile_finished_utc, "profile finished UTC")
    run_started = parse_utc(state["runStartedUtc"], "capture start")
    run_finished = parse_utc(state["runFinishedUtc"], "capture finish")
    require(run_started <= started <= finished <= run_finished,
            "profile window is outside the captured run")
    require((finished - started).total_seconds()
            >= declaration["profileContract"]["minimumProfileDurationSeconds"],
            "profile window is shorter than the declared minimum")
    certification.validate_trace(trace_source, declaration["profileContract"])
    role_summaries, _ = certification.validate_timing_csv(
        timing_source, declaration["profileContract"]
    )
    trace_target = run_root / names["unrealInsightsTrace"]
    timing_target = run_root / names["unrealInsightsTimingCsv"]
    copy_exclusive(trace_source, trace_target)
    copy_exclusive(timing_source, timing_target)
    profile = {
        "schema": declaration["schemas"]["threadProfile"],
        "schemaVersion": 1, "session": 19,
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "processId": state["processId"],
        "captureKind": declaration["profileContract"]["captureKind"],
        "startedUtc": profile_started_utc, "finishedUtc": profile_finished_utc,
        "traceChannels": list(trace_channels),
        "analysisTool": tool_binding(unreal_insights),
        "trace": certification.full_binding(run_root, trace_target),
        "timingCsv": certification.full_binding(run_root, timing_target),
        "roleSummaries": role_summaries,
    }
    certification.validate_thread_profile(
        profile, trace_target, timing_target,
        contract=declaration["profileContract"], schema=declaration["schemas"]["threadProfile"],
        candidate_id=declaration["candidateId"], run_id=declaration["certificationRunId"],
        process_id=state["processId"], run_started=run_started,
        run_finished=run_finished, run_root=run_root,
    )
    shipping.write_exclusive_json(run_root / names["threadProfile"], profile)
    archive_identity = declaration_tool.archive_identity(archive, declaration["candidateId"])
    require(archive_identity == declaration["archive"], "archive changed before manifest publication")
    artifacts = {
        role: certification.full_binding(run_root, run_root / name)
        for role, name in names.items() if role != "runManifest"
    }
    manifest = {
        "schema": declaration["schemas"]["runManifest"],
        "schemaVersion": 1, "session": 19,
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "declaration": shipping.file_binding(declaration_path),
        "startedUtc": state["runStartedUtc"], "finishedUtc": state["runFinishedUtc"],
        "processId": state["processId"], "generatedUtc": utc_now(),
        "artifacts": artifacts,
        "claimBoundary": certification.RUN_MANIFEST_CAPTURE_BOUNDARY,
    }
    certification.validate_run_manifest(manifest, declaration, declaration_path, run_root)
    shipping.write_exclusive_json(run_root / names["runManifest"], manifest)
    shipping.validate_exact_directory_entries(
        run_root, set(names.values()), set(), "finalized sustained run"
    )
    return {
        "state": "EXACT_FLAT_RUN_MANIFEST_PUBLISHED_INDEPENDENT_VALIDATION_REQUIRED",
        "candidateId": declaration["candidateId"],
        "certificationRunId": declaration["certificationRunId"],
        "runManifestWritten": True,
        "technicalValidationPassed": False,
        "humanPerformanceAcceptance": False,
        "releaseReady": False,
    }


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    with __import__("tempfile").TemporaryDirectory(prefix="dg-s19-sustained-runner-") as temporary:
        root = Path(temporary)
        user_dir = root / "UserDir"
        user_dir.mkdir()
        args = fixed_game_arguments(user_dir)
        check(args[:-1] == list(FIXED_GAME_ARGUMENTS), "fixed game arguments differ")
        check(args[-1].startswith("-UserDir="), "external UserDir argument is missing")
        check(not any(value.casefold().startswith("-trace") for value in args),
              "runner must never improvise Unreal trace flags")
        check("-log" not in [value.casefold() for value in args],
              "runner must not add project/default log flags")
        fields = [
            "uuid", "name", "driver_version", "temperature.gpu", "power.draw",
            "enforced.power.limit", "utilization.gpu", "clocks.current.graphics",
            "clocks.current.memory", "pstate",
        ]
        sensor_contract = {
            "effectivePowerLimitQueryField": "enforced.power.limit",
            "requiredNvidiaSmiQueryFields": fields,
        }
        row = parse_nvidia_rows(
            "GPU-1, Test GPU, 1.2, 70, 80, 120, 90, 2400, 8000, P0\n", fields
        )[0]
        check(row["uuid"] == "GPU-1", "nvidia UUID parsing differs")
        check(nvidia_row_to_sensor(
            row, 0, "2020-01-01T00:00:00.000Z", sensor_contract
        )[0] == 0,
              "sensor row index differs")
        check(effective_power_limit_field(sensor_contract) == "enforced.power.limit",
              "v2 enforced power-limit field differs")
        check(effective_power_limit_field({
            "requiredNvidiaSmiQueryFields": ["power.limit"]
        }) == "power.limit", "v1 power-limit field fallback differs")
        mixed_contract = {
            "effectivePowerLimitQueryField": "enforced.power.limit",
            "requiredNvidiaSmiQueryFields": ["power.limit", "enforced.power.limit"],
        }
        try:
            effective_power_limit_field(mixed_contract)
        except RunnerError:
            rejected = True
        else:
            rejected = False
        check(rejected, "mixed power-limit semantics must be rejected")
        raw = root / "raw.csv"
        output = root / "normalized.csv"
        columns = [
            "Application", "ProcessID", "SwapChainAddress", "PresentMode", "Dropped",
            "TimeInSeconds", "MsBetweenPresents",
        ]
        with raw.open("x", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle, lineterminator="\n")
            writer.writerow(columns)
            writer.writerow(["DiscGolfTour-Win64-Shipping.exe", "42", "0x1",
                             "Composed: Flip", "0", "0.0", "0.0"])
        contract = {
            "requiredCsvColumns": [
                "Application", "ProcessID", "TimeInSeconds", "MsBetweenPresents",
                "PresentMode", "Dropped",
            ],
            "requiredApplication": "DiscGolfTour-Win64-Shipping.exe",
        }
        check(normalized_presentmon_rows(raw, output, contract, 42) == 1,
              "one PresentMon row should normalize")
        normalized = certification.load_csv_stable(
            output, "normalized self-test PresentMon", contract["requiredCsvColumns"]
        )
        check(normalized[0]["ProcessID"] == "42", "normalized process ID differs")
        help_text = " ".join(PRESENTMON_REQUIRED_HELP)
        check(all(token in help_text for token in PRESENTMON_REQUIRED_HELP),
              "PresentMon capability token fixture differs")
        claim = certification.RUN_MANIFEST_CAPTURE_BOUNDARY
        check(claim["technicalValidationPassed"] is False,
              "runner manifest cannot claim technical validation")
        check(claim["humanPerformanceAcceptance"] is False,
              "runner manifest cannot grant human performance acceptance")
        check(claim["releaseReady"] is False,
              "runner manifest cannot grant release readiness")
        unversioned = root / "PresentMon_x64.exe"
        unversioned.write_bytes(b"synthetic unversioned self-test binary")
        check(tool_binding(unversioned)["version"].startswith("UNVERSIONED_BINARY_SHA256_"),
              "unversioned tools must remain hash-identified")
        check(CAPTURE_STATE_NAME != certification.declaration_tool.POLICY_PATH.name,
              "capture state must not target policy/evidence")
    print(
        f"Session 19 sustained performance runner self-test OK ({checks} assertions; "
        "no game, PresentMon capture, NVIDIA sampling loop, or Unreal trace launched)."
    )
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--preflight-only", action="store_true")
    modes.add_argument("--capture", action="store_true")
    modes.add_argument("--finalize-only", action="store_true")
    modes.add_argument("--self-test", action="store_true")
    parser.add_argument("--declaration", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--three-hole-receipt", type=Path)
    parser.add_argument("--three-hole-run-root", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--external-root", type=Path)
    parser.add_argument("--nvidia-smi", type=Path)
    parser.add_argument("--presentmon", type=Path)
    parser.add_argument("--unreal-insights", type=Path)
    parser.add_argument("--unreal-trace", type=Path)
    parser.add_argument("--unreal-timing-csv", type=Path)
    parser.add_argument("--profile-started-utc")
    parser.add_argument("--profile-finished-utc")
    parser.add_argument("--trace-channel", action="append", dest="trace_channels")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        extras = [
            args.declaration, args.archive, args.three_hole_receipt,
            args.three_hole_run_root, args.candidate_id, args.external_root,
            args.nvidia_smi, args.presentmon, args.unreal_insights,
            args.unreal_trace, args.unreal_timing_csv, args.profile_started_utc,
            args.profile_finished_utc, args.trace_channels,
        ]
        if any(value is not None for value in extras):
            print("--self-test accepts no capture/finalization arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (AssertionError, RunnerError, certification.CertificationError, OSError) as exc:
            print(f"Sustained performance runner self-test FAILED: {exc}", file=sys.stderr)
            return 1
    if not (args.preflight_only or args.capture or args.finalize_only):
        print("choose --preflight-only, --capture, --finalize-only, or --self-test",
              file=sys.stderr)
        return 2
    required = (
        args.declaration, args.archive, args.three_hole_receipt,
        args.three_hole_run_root, args.candidate_id, args.external_root,
    )
    if any(value is None for value in required):
        print("declaration/archive/prior receipt/prior run/candidate/external root are required",
              file=sys.stderr)
        return 2
    nvidia = args.nvidia_smi or Path(r"C:\Windows\System32\nvidia-smi.exe")
    presentmon = args.presentmon or Path(
        r"C:\Program Files\NVIDIA Corporation\FrameViewSDK\bin\PresentMon_x64.exe"
    )
    try:
        declaration, paths, inventory = preflight(
            declaration_path=args.declaration, archive=args.archive,
            receipt_path=args.three_hole_receipt,
            prior_run_root=args.three_hole_run_root, candidate_id=args.candidate_id,
            external_root=args.external_root, nvidia_smi=nvidia,
            presentmon=presentmon, for_capture=args.capture,
        )
        if args.preflight_only:
            print(json.dumps({
                "state": "PREFLIGHT_PASS_CAPTURE_NOT_STARTED",
                "candidateId": declaration["candidateId"],
                "certificationRunId": declaration["certificationRunId"],
                "declaredDurationSeconds": declaration["declaredDurationSeconds"],
                "tooling": inventory,
                "runRoot": str(paths["run"]),
                "automatedUnrealTraceExportAvailable": False,
                "captureExecuted": False, "releaseReady": False,
            }, indent=2, sort_keys=True))
            return 0
        if args.capture:
            state = capture(
                declaration_path=args.declaration, declaration=declaration,
                paths=paths, archive=args.archive.resolve(), nvidia_smi=nvidia.resolve(),
                presentmon=presentmon.resolve(),
            )
            print(json.dumps(state, indent=2, sort_keys=True))
            return 0
        final_required = (
            args.unreal_trace, args.unreal_timing_csv, args.profile_started_utc,
            args.profile_finished_utc, args.trace_channels,
        )
        if any(value is None for value in final_required):
            raise RunnerError(
                "--finalize-only requires Unreal trace, timing CSV, profile UTC window, "
                "and repeated --trace-channel values"
            )
        result = finalize(
            declaration_path=args.declaration.resolve(), declaration=declaration,
            paths=paths, archive=args.archive.resolve(), trace_source=args.unreal_trace,
            timing_source=args.unreal_timing_csv,
            profile_started_utc=args.profile_started_utc,
            profile_finished_utc=args.profile_finished_utc,
            unreal_insights=args.unreal_insights or Path(
                r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealInsights.exe"
            ),
            trace_channels=args.trace_channels,
        )
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (
        RunnerError, certification.CertificationError, declaration_tool.DeclarationError,
        readiness.AuditError, shipping.EvidenceError, OSError, subprocess.SubprocessError,
    ) as exc:
        print(f"Sustained performance runner failed closed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
