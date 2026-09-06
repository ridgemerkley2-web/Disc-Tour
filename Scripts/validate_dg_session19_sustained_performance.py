#!/usr/bin/env python3
"""Independently validate a predeclared sustained Shipping performance run.

This validator is intentionally external-evidence only.  It revalidates the
existing three-hole rendered-performance receipt, recomputes the live candidate
archive identity, verifies pre/post host and archive snapshots, checks a
continuous one-second NVIDIA series and PresentMon continuity, and requires a
non-placeholder Unreal Insights trace with GPU/game/render/RHI timing exports.
A technical pass never grants human performance, product-owner, or release
approval.
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timedelta, timezone
import io
import json
import math
from pathlib import Path
import re
import sys
import tempfile
from typing import Any, Callable, Iterable, Optional, Sequence

import audit_dg_session19_performance_certification_readiness as readiness
import generate_dg_session19_sustained_performance_declaration as declaration_tool
import validate_dg_session19_shipping_performance as shipping


ROOT = Path(__file__).resolve().parents[1]
PASS_STATE = "PASS_SUSTAINED_EXTERNAL_SHIPPING_PERFORMANCE_EVIDENCE_HUMAN_APPROVALS_PENDING"
FAIL_STATE = "FAIL_CLOSED"
RUN_MANIFEST_CAPTURE_BOUNDARY = {
    "operationalCaptureComplete": True,
    "technicalValidationPassed": False,
    "humanPerformanceAcceptance": False,
    "productOwnerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}
UPPER_SHA_RE = re.compile(r"^[0-9A-F]{64}$")
POWER_GUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)


class CertificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CertificationError(message)


def require_keys(value: Any, keys: Iterable[str], label: str) -> None:
    require(type(value) is dict and set(value) == set(keys), f"{label} keys differ")


def parse_utc(value: Any, label: str) -> datetime:
    try:
        return shipping.parse_utc(value, label)
    except shipping.EvidenceError as exc:
        raise CertificationError(str(exc)) from None


def stable_bytes(path: Path, label: str, maximum_bytes: Optional[int] = None) -> bytes:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CertificationError(
            f"{label} cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            f"{label} is not a regular file")
    before = resolved.stat()
    require(maximum_bytes is None or before.st_size <= maximum_bytes,
            f"{label} exceeds its maximum size")
    try:
        payload = resolved.read_bytes()
    except OSError as exc:
        raise CertificationError(f"{label} could not be read ({exc.__class__.__name__})") from None
    after = resolved.stat()
    fingerprint = lambda value: (
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
        value.st_dev,
        value.st_ino,
    )
    require(fingerprint(before) == fingerprint(after) and len(payload) == after.st_size,
            f"{label} changed while read")
    return payload


def load_json_stable(path: Path, label: str, maximum_bytes: int = 4 * 1024 * 1024) -> dict[str, Any]:
    payload = stable_bytes(path, label, maximum_bytes)
    try:
        value = json.loads(
            payload.decode("utf-8"),
            object_pairs_hook=shipping.reject_duplicate_keys,
        )
    except (
        UnicodeDecodeError,
        json.JSONDecodeError,
        shipping.DuplicateKeyError,
    ) as exc:
        raise CertificationError(f"{label} is invalid JSON ({exc.__class__.__name__})") from None
    require(type(value) is dict, f"{label} JSON root is not an object")
    return value


def load_csv_stable(
    path: Path,
    label: str,
    required_columns: Sequence[str],
    maximum_bytes: int = 64 * 1024 * 1024,
) -> list[dict[str, str]]:
    payload = stable_bytes(path, label, maximum_bytes)
    require(not payload.startswith(b"\xef\xbb\xbf"), f"{label} must not contain a UTF-8 BOM")
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError:
        raise CertificationError(f"{label} is not UTF-8") from None
    require("\x00" not in text, f"{label} contains NUL bytes")
    try:
        reader = csv.DictReader(io.StringIO(text, newline=""), strict=True)
        require(reader.fieldnames == list(required_columns), f"{label} columns differ")
        require(len(set(reader.fieldnames or [])) == len(required_columns),
                f"{label} contains duplicate columns")
        rows = list(reader)
    except (csv.Error, CertificationError) as exc:
        if isinstance(exc, CertificationError):
            raise
        raise CertificationError(f"{label} is malformed CSV") from None
    require(all(None not in row for row in rows), f"{label} contains extra fields")
    require(all(set(row) == set(required_columns) for row in rows),
            f"{label} row columns differ")
    return rows


def strict_int(value: Any, label: str, minimum: Optional[int] = None) -> int:
    require(type(value) is int, f"{label} is not an integer")
    if minimum is not None:
        require(value >= minimum, f"{label} is below its minimum")
    return value


def csv_int(value: str, label: str, minimum: Optional[int] = None) -> int:
    require(re.fullmatch(r"-?(?:0|[1-9][0-9]*)", value or "") is not None,
            f"{label} is not a canonical integer")
    parsed = int(value)
    if minimum is not None:
        require(parsed >= minimum, f"{label} is below its minimum")
    return parsed


def finite_number(value: Any, label: str) -> float:
    require(type(value) in (int, float) and type(value) is not bool,
            f"{label} is not numeric")
    parsed = float(value)
    require(math.isfinite(parsed), f"{label} is not finite")
    return parsed


def csv_float(value: str, label: str) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        raise CertificationError(f"{label} is not numeric") from None
    require(math.isfinite(parsed), f"{label} is not finite")
    return parsed


def validate_sha_binding_shape(value: Any, label: str, include_version: bool = False) -> None:
    expected = {"fileName", "bytes", "sha256"}
    if include_version:
        expected.add("version")
    require_keys(value, expected, label)
    require(type(value["fileName"]) is str and bool(value["fileName"]),
            f"{label} file name is invalid")
    strict_int(value["bytes"], f"{label} bytes", 1)
    require(type(value["sha256"]) is str and UPPER_SHA_RE.fullmatch(value["sha256"]) is not None,
            f"{label} SHA-256 is invalid")
    if include_version:
        require(type(value["version"]) is str and bool(value["version"].strip()),
                f"{label} version is missing")


def full_binding(run_root: Path, path: Path) -> dict[str, Any]:
    return {"relativePath": path.relative_to(run_root).as_posix(), **shipping.file_binding(path)}


def validate_bound_artifact(
    binding: Any, path: Path, run_root: Path, expected_relative: str, label: str
) -> None:
    require(type(binding) is dict, f"{label} binding is missing")
    expected = {"relativePath": expected_relative, **shipping.file_binding(path)}
    require(binding == expected, f"{label} binding differs")
    try:
        path.resolve(strict=True).relative_to(run_root.resolve(strict=True))
    except (OSError, RuntimeError, ValueError):
        raise CertificationError(f"{label} escapes the run root") from None


ReceiptVerifier = Callable[[Path, Path, Path, str], tuple[dict[str, Any], dict[str, Any]]]


def validate_declaration(
    declaration_path: Path,
    archive: Path,
    receipt_path: Path,
    prior_run_root: Path,
    candidate_id: str,
    receipt_verifier: ReceiptVerifier,
) -> tuple[dict[str, Any], dict[str, Any]]:
    declaration = load_json_stable(declaration_path, "predeclaration")
    require_keys(declaration, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "declaredUtc", "declaredDurationSeconds", "state", "policy", "archive",
        "threeHolePerformanceReceipt", "requiredArtifacts", "schemas",
        "hostSnapshotContract", "nvidiaSensorContract", "presentMonContract",
        "profileContract", "chronologyContract", "technicalPassState",
        "futureTechnicalReceiptClaimBoundary", "claimBoundary",
    }, "predeclaration")
    policy_path = declaration_tool.resolve_bound_policy_path(declaration["policy"])
    policy = declaration_tool.load_policy(policy_path)
    policy_version = declaration_tool.policy_version(policy)
    require(declaration["schema"] == policy["schemas"]["declaration"],
            "predeclaration schema differs")
    require(declaration["schemaVersion"] == policy_version
            and declaration["session"] == 19,
            "predeclaration version/session differs")
    require(declaration["candidateId"] == candidate_id, "predeclaration candidate differs")
    require(declaration["state"] == declaration_tool.DECLARATION_STATE,
            "predeclaration state differs")
    require(declaration["claimBoundary"] == declaration_tool.declaration_claim_boundary(),
            "predeclaration overclaims capture or approval")
    require(declaration["policy"] == declaration_tool.stable_binding(
        policy_path, "certification policy"
    ), "predeclaration policy binding differs")
    for key in (
        "requiredArtifacts", "schemas", "hostSnapshotContract", "nvidiaSensorContract",
        "presentMonContract", "profileContract", "chronologyContract",
        "technicalPassState",
    ):
        require(declaration[key] == policy[key], f"predeclaration {key} differs from policy")
    require(declaration["futureTechnicalReceiptClaimBoundary"] == policy["claimBoundary"],
            "future technical claim boundary differs")
    require(declaration["technicalPassState"] == PASS_STATE,
            "predeclaration technical pass state differs")
    parse_utc(declaration["declaredUtc"], "predeclaration declaredUtc")
    duration = strict_int(declaration["declaredDurationSeconds"], "declared duration")
    require(
        policy["predeclaration"]["minimumDurationSeconds"]
        <= duration
        <= policy["predeclaration"]["maximumDurationSeconds"],
        "predeclared duration is outside policy",
    )
    require(shipping.canonical_uuid4(declaration["certificationRunId"]),
            "certification run ID is not lowercase UUIDv4")
    live_archive = declaration_tool.archive_identity(archive, candidate_id)
    require(declaration["archive"] == live_archive,
            "live archive differs from the predeclared identity")
    try:
        receipt, _ = receipt_verifier(receipt_path, prior_run_root, archive, candidate_id)
    except (readiness.AuditError, shipping.EvidenceError, OSError) as exc:
        raise CertificationError(f"three-hole receipt revalidation failed: {exc}") from None
    receipt_identity = declaration_tool.receipt_identity(receipt_path, receipt)
    require(declaration["threeHolePerformanceReceipt"] == receipt_identity,
            "three-hole performance receipt differs from the predeclaration")
    return declaration, policy


def validate_archive_snapshot(
    value: dict[str, Any],
    *,
    phase: str,
    candidate_id: str,
    run_id: str,
    archive_identity: dict[str, Any],
    schema: str,
) -> datetime:
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "phase", "capturedUtc", "archive",
    }, f"archive {phase} snapshot")
    require(value["schema"] == schema and value["schemaVersion"] == 1
            and value["session"] == 19, f"archive {phase} schema identity differs")
    require(value["candidateId"] == candidate_id and value["certificationRunId"] == run_id,
            f"archive {phase} candidate/run differs")
    require(value["phase"] == phase, f"archive {phase} phase differs")
    require(value["archive"] == archive_identity,
            f"archive {phase} identity differs from the declaration")
    return parse_utc(value["capturedUtc"], f"archive {phase} capturedUtc")


def validate_host_snapshot(
    value: dict[str, Any],
    *,
    phase: str,
    candidate_id: str,
    run_id: str,
    schema: str,
    contract: dict[str, Any],
) -> tuple[datetime, dict[str, Any]]:
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "phase", "capturedUtc", "gpu", "power", "storage",
    }, f"host {phase} snapshot")
    require(value["schema"] == schema and value["schemaVersion"] == 1
            and value["session"] == 19, f"host {phase} schema identity differs")
    require(value["candidateId"] == candidate_id and value["certificationRunId"] == run_id,
            f"host {phase} candidate/run differs")
    require(value["phase"] == phase, f"host {phase} phase differs")
    require_keys(value["gpu"], {
        "name", "uuid", "driverVersion", "powerDrawW", "powerLimitW",
    }, f"host {phase} GPU")
    gpu = value["gpu"]
    for key in ("name", "uuid", "driverVersion"):
        require(type(gpu[key]) is str and bool(gpu[key].strip()),
                f"host {phase} GPU {key} is missing")
    power_draw = finite_number(gpu["powerDrawW"], f"host {phase} GPU power draw")
    power_limit = finite_number(gpu["powerLimitW"], f"host {phase} GPU power limit")
    require(power_draw >= 0.0 and power_limit > 0.0,
            f"host {phase} GPU power values are invalid")
    require_keys(value["power"], {
        "activeSchemeGuid", "activeSchemeName", "acLineStatus", "batterySaverOn",
    }, f"host {phase} power")
    power = value["power"]
    require(type(power["activeSchemeGuid"]) is str
            and POWER_GUID_RE.fullmatch(power["activeSchemeGuid"]) is not None,
            f"host {phase} power scheme GUID is invalid")
    require(type(power["activeSchemeName"]) is str and bool(power["activeSchemeName"].strip()),
            f"host {phase} power scheme name is missing")
    if contract["requireAcOnline"]:
        require(power["acLineStatus"] == "ONLINE", f"host {phase} is not on AC power")
    if contract["requireBatterySaverOff"]:
        require(power["batterySaverOn"] is False,
                f"host {phase} battery saver must be off")
    require_keys(value["storage"], {"volumeId", "freeBytes"}, f"host {phase} storage")
    storage = value["storage"]
    require(type(storage["volumeId"]) is str and bool(storage["volumeId"].strip()),
            f"host {phase} storage volume is missing")
    strict_int(storage["freeBytes"], f"host {phase} free bytes", contract["minimumFreeBytes"])
    return parse_utc(value["capturedUtc"], f"host {phase} capturedUtc"), value


def validate_host_pair(pre: dict[str, Any], post: dict[str, Any], contract: dict[str, Any]) -> None:
    if contract["requireSameGpuUuid"]:
        require(pre["gpu"]["uuid"] == post["gpu"]["uuid"], "GPU UUID changed during capture")
    require(pre["gpu"]["name"] == post["gpu"]["name"], "GPU name changed during capture")
    if contract["requireSameGpuDriver"]:
        require(pre["gpu"]["driverVersion"] == post["gpu"]["driverVersion"],
                "GPU driver changed during capture")
    if contract["requireGpuPowerDrawAndLimit"]:
        require(
            finite_number(pre["gpu"]["powerLimitW"], "PRE GPU power limit")
            == finite_number(post["gpu"]["powerLimitW"], "POST GPU power limit"),
            "GPU power limit changed during capture",
        )
    if contract["requireSameActivePowerScheme"]:
        require(pre["power"]["activeSchemeGuid"] == post["power"]["activeSchemeGuid"],
                "active power scheme changed during capture")
    require(pre["storage"]["volumeId"] == post["storage"]["volumeId"],
            "storage volume changed during capture")


def validate_nvidia_sensors(
    path: Path,
    *,
    contract: dict[str, Any],
    duration_seconds: int,
    run_started: datetime,
    run_finished: datetime,
    boundary_tolerance_seconds: float,
    gpu: dict[str, Any],
) -> dict[str, Any]:
    rows = load_csv_stable(path, "NVIDIA sensor series", contract["requiredCsvColumns"])
    expected = math.floor(
        duration_seconds * 1000 / contract["nominalIntervalMilliseconds"]
    ) + 1
    minimum_samples = math.ceil(expected * contract["minimumSampleCoverageRatio"])
    require(len(rows) >= minimum_samples,
            "NVIDIA sensor series does not meet one-second sample coverage")
    times: list[datetime] = []
    temperatures: list[float] = []
    power_draws: list[float] = []
    for index, row in enumerate(rows):
        require(csv_int(row["sampleIndex"], f"sensor row {index} index", 0) == index,
                "NVIDIA sensor sample indexes are not contiguous")
        timestamp = parse_utc(row["timestampUtc"], f"sensor row {index} timestamp")
        require(row["gpuUuid"] == gpu["uuid"] and row["gpuName"] == gpu["name"],
                f"sensor row {index} GPU identity differs")
        require(row["driverVersion"] == gpu["driverVersion"],
                f"sensor row {index} GPU driver differs")
        temperature = csv_float(row["temperatureC"], f"sensor row {index} temperature")
        power_draw = csv_float(row["powerDrawW"], f"sensor row {index} power draw")
        power_limit = csv_float(row["powerLimitW"], f"sensor row {index} power limit")
        utilization = csv_float(
            row["utilizationGpuPercent"], f"sensor row {index} utilization"
        )
        graphics_clock = csv_float(
            row["graphicsClockMHz"], f"sensor row {index} graphics clock"
        )
        memory_clock = csv_float(
            row["memoryClockMHz"], f"sensor row {index} memory clock"
        )
        require(contract["minimumTemperatureC"] <= temperature <= contract["maximumTemperatureC"],
                f"sensor row {index} temperature is outside policy")
        require(power_draw >= 0.0 and power_limit > 0.0
                and power_draw <= power_limit * contract["maximumPowerOverLimitRatio"],
                f"sensor row {index} power telemetry is invalid")
        require(abs(power_limit - finite_number(gpu["powerLimitW"], "host GPU power limit"))
                <= 0.1, f"sensor row {index} GPU power limit differs from PRE snapshot")
        require(contract["minimumUtilizationPercent"] <= utilization
                <= contract["maximumUtilizationPercent"],
                f"sensor row {index} utilization is invalid")
        require(graphics_clock >= contract["minimumClockMHz"]
                and memory_clock >= contract["minimumClockMHz"],
                f"sensor row {index} clocks are invalid")
        require(bool(row["pstate"].strip()), f"sensor row {index} pstate is missing")
        times.append(timestamp)
        temperatures.append(temperature)
        power_draws.append(power_draw)
    interval_ms: list[float] = []
    for earlier, later in zip(times, times[1:]):
        milliseconds = (later - earlier).total_seconds() * 1000.0
        require(contract["minimumIntervalMilliseconds"] <= milliseconds
                <= contract["maximumIntervalMilliseconds"],
                "NVIDIA sensor series has a continuity gap")
        interval_ms.append(milliseconds)
    tolerance = timedelta(seconds=boundary_tolerance_seconds)
    require(run_started - tolerance <= times[0] <= run_started + tolerance,
            "NVIDIA sensor series does not begin at the run boundary")
    require(times[-1] >= run_started + timedelta(seconds=duration_seconds) - tolerance,
            "NVIDIA sensor series does not span the declared duration")
    require(times[-1] <= run_finished + tolerance,
            "NVIDIA sensor series extends beyond the run")
    return {
        "sampleCount": len(rows),
        "firstTimestampUtc": rows[0]["timestampUtc"],
        "lastTimestampUtc": rows[-1]["timestampUtc"],
        "maximumIntervalMilliseconds": max(interval_ms),
        "averageIntervalMilliseconds": sum(interval_ms) / len(interval_ms),
        "maximumTemperatureC": max(temperatures),
        "averagePowerDrawW": sum(power_draws) / len(power_draws),
    }


def validate_nvidia_capture(
    capture: dict[str, Any],
    csv_path: Path,
    *,
    contract: dict[str, Any],
    schema: str,
    candidate_id: str,
    run_id: str,
    duration_seconds: int,
    run_started: datetime,
    run_finished: datetime,
    boundary_tolerance_seconds: float,
    run_root: Path,
) -> None:
    require_keys(capture, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "startedUtc", "finishedUtc", "intervalMilliseconds", "tool", "queryFields", "csv",
    }, "NVIDIA sensor capture")
    require(capture["schema"] == schema and capture["schemaVersion"] == 1
            and capture["session"] == 19, "NVIDIA sensor capture schema identity differs")
    require(capture["candidateId"] == candidate_id and capture["certificationRunId"] == run_id,
            "NVIDIA sensor capture candidate/run differs")
    require(capture["intervalMilliseconds"] == contract["nominalIntervalMilliseconds"],
            "NVIDIA sensor capture interval differs")
    require(capture["queryFields"] == contract["requiredNvidiaSmiQueryFields"],
            "nvidia-smi query fields differ")
    validate_sha_binding_shape(capture["tool"], "nvidia-smi tool", include_version=True)
    require(capture["tool"]["fileName"].lower() == "nvidia-smi.exe",
            "nvidia-smi executable name differs")
    validate_bound_artifact(capture["csv"], csv_path, run_root, csv_path.name,
                            "NVIDIA sensor CSV")
    started = parse_utc(capture["startedUtc"], "NVIDIA sensor capture startedUtc")
    finished = parse_utc(capture["finishedUtc"], "NVIDIA sensor capture finishedUtc")
    tolerance = timedelta(seconds=boundary_tolerance_seconds)
    require(run_started - tolerance <= started <= run_started + tolerance,
            "NVIDIA sensor capture does not begin at the run boundary")
    require(finished >= started + timedelta(seconds=duration_seconds) - tolerance,
            "NVIDIA sensor capture is shorter than the declared duration")
    require(finished <= run_finished + tolerance,
            "NVIDIA sensor capture extends beyond the run")


def validate_presentmon(
    capture: dict[str, Any],
    csv_path: Path,
    *,
    contract: dict[str, Any],
    schema: str,
    candidate_id: str,
    run_id: str,
    process_id: int,
    duration_seconds: int,
    run_started: datetime,
    run_finished: datetime,
    boundary_tolerance_seconds: float,
    run_root: Path,
) -> dict[str, Any]:
    require_keys(capture, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "processId", "application", "startedUtc", "finishedUtc", "tool", "csv",
    }, "PresentMon capture")
    require(capture["schema"] == schema and capture["schemaVersion"] == 1
            and capture["session"] == 19, "PresentMon capture schema identity differs")
    require(capture["candidateId"] == candidate_id and capture["certificationRunId"] == run_id,
            "PresentMon candidate/run differs")
    require(capture["processId"] == process_id, "PresentMon process ID differs")
    require(capture["application"] == contract["requiredApplication"],
            "PresentMon application differs")
    validate_sha_binding_shape(capture["tool"], "PresentMon tool", include_version=True)
    require(re.fullmatch(r"(?i:PresentMon(?:_x64)?\.exe)", capture["tool"]["fileName"])
            is not None, "PresentMon tool executable name differs")
    validate_bound_artifact(
        capture["csv"], csv_path, run_root, csv_path.name, "PresentMon CSV"
    )
    started = parse_utc(capture["startedUtc"], "PresentMon startedUtc")
    finished = parse_utc(capture["finishedUtc"], "PresentMon finishedUtc")
    require(finished >= started + timedelta(seconds=duration_seconds),
            "PresentMon capture is shorter than the declared duration")
    tolerance = timedelta(seconds=boundary_tolerance_seconds)
    require(run_started - tolerance <= started <= run_started + tolerance,
            "PresentMon capture does not begin at the run boundary")
    require(finished <= run_finished + tolerance,
            "PresentMon capture extends beyond the run")
    rows = load_csv_stable(csv_path, "PresentMon CSV", contract["requiredCsvColumns"])
    minimum_rows = math.ceil(duration_seconds * contract["minimumPresentRateHz"])
    require(len(rows) >= minimum_rows, "PresentMon CSV has too few presented frames")
    times: list[float] = []
    gaps: list[float] = []
    dropped = 0
    for index, row in enumerate(rows):
        require(row["Application"] == contract["requiredApplication"],
                f"PresentMon row {index} application differs")
        require(csv_int(row["ProcessID"], f"PresentMon row {index} process ID", 1)
                == process_id, f"PresentMon row {index} process ID differs")
        timestamp = csv_float(row["TimeInSeconds"], f"PresentMon row {index} time")
        gap = csv_float(row["MsBetweenPresents"], f"PresentMon row {index} gap")
        require(timestamp >= 0.0 and gap >= 0.0,
                f"PresentMon row {index} contains a negative time")
        require(gap <= contract["maximumContinuityGapMilliseconds"],
                f"PresentMon row {index} exceeds the continuity gap")
        require(bool(row["PresentMode"].strip()),
                f"PresentMon row {index} present mode is missing")
        require(row["Dropped"] in {"0", "1", "false", "true", "False", "True"},
                f"PresentMon row {index} dropped value is invalid")
        dropped += int(row["Dropped"].lower() in {"1", "true"})
        if times:
            delta_ms = (timestamp - times[-1]) * 1000.0
            require(delta_ms > 0.0, "PresentMon times are not strictly increasing")
            require(abs(delta_ms - gap) <= contract["timeDeltaToleranceMilliseconds"],
                    f"PresentMon row {index} time and gap disagree")
        times.append(timestamp)
        gaps.append(gap)
    require(times[-1] - times[0] >= duration_seconds - boundary_tolerance_seconds,
            "PresentMon CSV does not span the declared duration")
    dropped_ratio = dropped / len(rows)
    require(dropped_ratio <= contract["maximumDroppedRatio"],
            "PresentMon dropped-frame ratio exceeds policy")
    return {
        "presentCount": len(rows),
        "csvSpanSeconds": times[-1] - times[0],
        "maximumGapMilliseconds": max(gaps),
        "droppedCount": dropped,
        "droppedRatio": dropped_ratio,
    }


def validate_trace(path: Path, contract: dict[str, Any]) -> dict[str, Any]:
    payload = stable_bytes(path, "Unreal Insights trace")
    require(len(payload) >= contract["minimumTraceBytes"],
            "Unreal Insights trace is too small to be a profile artifact")
    allowed_magic = {value.encode("ascii") for value in contract["requiredTraceMagicAscii"]}
    require(payload[:4] in allowed_magic,
            "Unreal Insights trace magic header is invalid")
    distinct = len(set(payload))
    non_zero_ratio = sum(byte != 0 for byte in payload) / len(payload)
    require(distinct >= contract["minimumDistinctTraceByteValues"],
            "Unreal Insights trace has placeholder-like byte diversity")
    require(non_zero_ratio >= contract["minimumTraceNonZeroRatio"],
            "Unreal Insights trace has placeholder-like zero content")
    return {
        "bytes": len(payload),
        "distinctByteValues": distinct,
        "nonZeroRatio": non_zero_ratio,
        "sha256": shipping.sha256_file(path),
    }


def validate_timing_csv(path: Path, contract: dict[str, Any]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    rows = load_csv_stable(path, "Unreal Insights timing CSV",
                           contract["requiredTimingCsvColumns"])
    by_role: dict[str, list[tuple[str, float, float]]] = {
        role: [] for role in contract["requiredThreadRoles"]
    }
    for index, row in enumerate(rows):
        role = row["ThreadRole"]
        require(role in by_role, f"timing row {index} thread role is unrecognized")
        require(bool(row["ThreadName"].strip()), f"timing row {index} thread name is missing")
        start = csv_float(row["StartSeconds"], f"timing row {index} start")
        duration_ms = csv_float(
            row["DurationMilliseconds"], f"timing row {index} duration"
        )
        require(start >= 0.0 and duration_ms > 0.0,
                f"timing row {index} contains invalid timing")
        by_role[role].append((row["ThreadName"], start, duration_ms))
    summaries: list[dict[str, Any]] = []
    for role in contract["requiredThreadRoles"]:
        values = by_role[role]
        require(len(values) >= contract["minimumEventsPerRole"],
                f"{role} profile has too few events")
        starts = [value[1] for value in values]
        require(all(later >= earlier for earlier, later in zip(starts, starts[1:])),
                f"{role} timing events are not chronological")
        first = min(starts)
        last_end = max(start + duration / 1000.0 for _, start, duration in values)
        require(last_end - first >= contract["minimumProfileDurationSeconds"] - 2.0,
                f"{role} timing span is shorter than the profile window")
        summaries.append({
            "role": role,
            "threadNames": sorted({value[0] for value in values}),
            "eventCount": len(values),
            "firstStartSeconds": round(first, 6),
            "lastEndSeconds": round(last_end, 6),
            "totalDurationMilliseconds": round(sum(value[2] for value in values), 6),
        })
    return summaries, {"timingEventCount": len(rows), "roles": summaries}


def validate_thread_profile(
    profile: dict[str, Any],
    trace_path: Path,
    timing_path: Path,
    *,
    contract: dict[str, Any],
    schema: str,
    candidate_id: str,
    run_id: str,
    process_id: int,
    run_started: datetime,
    run_finished: datetime,
    run_root: Path,
) -> dict[str, Any]:
    require_keys(profile, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "processId", "captureKind", "startedUtc", "finishedUtc", "traceChannels",
        "analysisTool", "trace", "timingCsv", "roleSummaries",
    }, "thread profile")
    require(profile["schema"] == schema and profile["schemaVersion"] == 1
            and profile["session"] == 19, "thread profile schema identity differs")
    require(profile["candidateId"] == candidate_id and profile["certificationRunId"] == run_id,
            "thread profile candidate/run differs")
    require(profile["processId"] == process_id, "thread profile process ID differs")
    require(profile["captureKind"] == contract["captureKind"],
            "thread profile capture kind differs")
    require(type(profile["traceChannels"]) is list
            and all(type(value) is str for value in profile["traceChannels"])
            and len(profile["traceChannels"]) == len(set(profile["traceChannels"]))
            and set(contract["requiredTraceChannels"]).issubset(profile["traceChannels"]),
            "thread profile required trace channels are missing")
    validate_sha_binding_shape(profile["analysisTool"], "Unreal Insights analysis tool",
                               include_version=True)
    require(profile["analysisTool"]["fileName"] == "UnrealInsights.exe",
            "Unreal Insights analysis executable name differs")
    validate_bound_artifact(profile["trace"], trace_path, run_root, trace_path.name,
                            "Unreal Insights trace")
    validate_bound_artifact(profile["timingCsv"], timing_path, run_root, timing_path.name,
                            "Unreal Insights timing CSV")
    started = parse_utc(profile["startedUtc"], "thread profile startedUtc")
    finished = parse_utc(profile["finishedUtc"], "thread profile finishedUtc")
    require(finished >= started + timedelta(seconds=contract["minimumProfileDurationSeconds"]),
            "thread profile window is too short")
    require(run_started <= started <= finished <= run_finished,
            "thread profile window is outside the sustained run")
    trace_summary = validate_trace(trace_path, contract)
    expected_roles, timing_summary = validate_timing_csv(timing_path, contract)
    require(profile["roleSummaries"] == expected_roles,
            "thread profile role summaries differ from the timing export")
    return {"trace": trace_summary, **timing_summary}


def validate_run_manifest(
    manifest: dict[str, Any], declaration: dict[str, Any], declaration_path: Path,
    run_root: Path,
) -> tuple[datetime, datetime, datetime, int]:
    require_keys(manifest, {
        "schema", "schemaVersion", "session", "candidateId", "certificationRunId",
        "declaration", "startedUtc", "finishedUtc", "processId", "generatedUtc",
        "artifacts", "claimBoundary",
    }, "run manifest")
    require(manifest["schema"] == declaration["schemas"]["runManifest"]
            and manifest["schemaVersion"] == 1 and manifest["session"] == 19,
            "run manifest schema identity differs")
    require(manifest["candidateId"] == declaration["candidateId"]
            and manifest["certificationRunId"] == declaration["certificationRunId"],
            "run manifest candidate/run differs")
    require(manifest["declaration"] == shipping.file_binding(declaration_path),
            "run manifest declaration binding differs")
    require(manifest["claimBoundary"] == RUN_MANIFEST_CAPTURE_BOUNDARY,
            "run manifest overclaims validation or approval")
    process_id = strict_int(manifest["processId"], "run process ID", 1)
    started = parse_utc(manifest["startedUtc"], "run startedUtc")
    finished = parse_utc(manifest["finishedUtc"], "run finishedUtc")
    generated = parse_utc(manifest["generatedUtc"], "run manifest generatedUtc")
    require(finished >= started + timedelta(seconds=declaration["declaredDurationSeconds"]),
            "run is shorter than the predeclared duration")
    require(generated >= finished, "run manifest predates run completion")
    expected_artifact_roles = set(declaration["requiredArtifacts"]) - {"runManifest"}
    require(type(manifest["artifacts"]) is dict
            and set(manifest["artifacts"]) == expected_artifact_roles,
            "run manifest artifact roles differ")
    for role in expected_artifact_roles:
        file_name = declaration["requiredArtifacts"][role]
        validate_bound_artifact(
            manifest["artifacts"][role], run_root / file_name, run_root, file_name,
            f"run manifest {role}",
        )
    return started, finished, generated, process_id


def validate_output_path(output: Path, external_root: Path, run_root: Path, archive: Path) -> Path:
    require(output.is_absolute() and external_root.is_absolute(),
            "receipt output and external root must be absolute")
    resolved_output = output.resolve(strict=False)
    resolved_external = external_root.resolve(strict=True)
    require(shipping.is_same_or_under(resolved_output, resolved_external),
            "receipt output must remain under the selected external root")
    require(not shipping.is_same_or_under(resolved_output, ROOT),
            "receipt output must remain outside the project")
    require(not shipping.is_same_or_under(resolved_output, archive),
            "receipt output must remain outside the archive")
    require(not shipping.is_same_or_under(resolved_output, run_root),
            "receipt output must remain outside the run root")
    require(not output.exists(), "refusing to overwrite an existing technical receipt")
    require(not declaration_tool.path_chain_has_reparse(output.parent),
            "receipt output parent contains a reparse point")
    return resolved_output


def validate_evidence(
    *,
    declaration_path: Path,
    run_root: Path,
    archive: Path,
    receipt_path: Path,
    prior_run_root: Path,
    candidate_id: str,
    receipt_verifier: ReceiptVerifier = readiness.validate_receipt_against_run,
) -> dict[str, Any]:
    archive = shipping.validate_candidate_archive_path(archive, candidate_id)
    try:
        declaration_path = declaration_path.resolve(strict=True)
        prior_run_root = prior_run_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CertificationError(
            f"declaration or three-hole run cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(declaration_path.is_file() and not shipping.is_reparse_point(declaration_path),
            "predeclaration is not a regular external file")
    require(prior_run_root.is_dir() and not shipping.is_reparse_point(prior_run_root),
            "three-hole run root is not a regular directory")
    require(not shipping.is_same_or_under(declaration_path, ROOT),
            "predeclaration must be outside the project")
    require(not shipping.is_same_or_under(declaration_path, archive),
            "predeclaration must be outside the archive")
    require(not shipping.is_same_or_under(declaration_path, prior_run_root),
            "predeclaration must be outside the three-hole performance run")
    try:
        run_root = run_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CertificationError(f"run root cannot be resolved ({exc.__class__.__name__})") from None
    require(run_root.is_dir() and not shipping.is_reparse_point(run_root),
            "run root is not a regular directory")
    require(not shipping.is_same_or_under(run_root, ROOT), "run root must be outside the project")
    require(not shipping.is_same_or_under(run_root, archive), "run root must be outside the archive")
    require(not shipping.is_same_or_under(run_root, prior_run_root),
            "sustained run must be separate from the three-hole performance run")
    require(not shipping.is_same_or_under(declaration_path, run_root),
            "predeclaration must be separate from the sustained run")
    declaration, policy = validate_declaration(
        declaration_path, archive, receipt_path, prior_run_root, candidate_id,
        receipt_verifier,
    )
    require(run_root.name == declaration["certificationRunId"],
            "run-root leaf must equal the predeclared certification run ID")
    expected_files = set(declaration["requiredArtifacts"].values())
    shipping.validate_exact_directory_entries(
        run_root, expected_files, set(), "sustained performance run"
    )
    artifacts = {
        role: run_root / name for role, name in declaration["requiredArtifacts"].items()
    }
    declaration_mtime_ns = declaration_path.stat().st_mtime_ns
    mtime_tolerance_ns = int(
        declaration["chronologyContract"]["futureTimestampToleranceSeconds"]
        * 1_000_000_000
    )
    for role, path in artifacts.items():
        require(path.stat().st_mtime_ns + mtime_tolerance_ns >= declaration_mtime_ns,
                f"{role} filesystem timestamp predates the predeclaration")
    manifest = load_json_stable(artifacts["runManifest"], "run manifest")
    run_started, run_finished, generated, process_id = validate_run_manifest(
        manifest, declaration, declaration_path, run_root
    )
    schema = declaration["schemas"]
    archive_pre = load_json_stable(artifacts["archivePre"], "archive PRE snapshot")
    archive_post = load_json_stable(artifacts["archivePost"], "archive POST snapshot")
    archive_pre_time = validate_archive_snapshot(
        archive_pre, phase="PRE", candidate_id=candidate_id,
        run_id=declaration["certificationRunId"], archive_identity=declaration["archive"],
        schema=schema["archiveSnapshot"],
    )
    archive_post_time = validate_archive_snapshot(
        archive_post, phase="POST", candidate_id=candidate_id,
        run_id=declaration["certificationRunId"], archive_identity=declaration["archive"],
        schema=schema["archiveSnapshot"],
    )
    host_pre = load_json_stable(artifacts["hostPre"], "host PRE snapshot")
    host_post = load_json_stable(artifacts["hostPost"], "host POST snapshot")
    host_pre_time, host_pre = validate_host_snapshot(
        host_pre, phase="PRE", candidate_id=candidate_id,
        run_id=declaration["certificationRunId"], schema=schema["hostSnapshot"],
        contract=declaration["hostSnapshotContract"],
    )
    host_post_time, host_post = validate_host_snapshot(
        host_post, phase="POST", candidate_id=candidate_id,
        run_id=declaration["certificationRunId"], schema=schema["hostSnapshot"],
        contract=declaration["hostSnapshotContract"],
    )
    validate_host_pair(host_pre, host_post, declaration["hostSnapshotContract"])
    chronology = declaration["chronologyContract"]
    boundary_seconds = chronology["captureBoundaryToleranceSeconds"]
    tolerance = timedelta(seconds=boundary_seconds)
    declared = parse_utc(declaration["declaredUtc"], "predeclaration declaredUtc")
    require(declared <= min(archive_pre_time, host_pre_time),
            "predeclaration does not precede PRE snapshots")
    require(archive_pre_time <= run_started + tolerance
            and host_pre_time <= run_started + tolerance,
            "PRE snapshots follow the run start")
    require(archive_post_time >= run_finished - tolerance
            and host_post_time >= run_finished - tolerance,
            "POST snapshots precede the run finish")
    require(generated >= max(archive_post_time, host_post_time),
            "run manifest predates POST snapshots")
    require(generated <= datetime.now(timezone.utc) + timedelta(
        seconds=chronology["futureTimestampToleranceSeconds"]
    ), "run chronology extends into the future")
    nvidia_capture = load_json_stable(
        artifacts["nvidiaSensorCapture"], "NVIDIA sensor capture"
    )
    validate_nvidia_capture(
        nvidia_capture, artifacts["nvidiaSensors"],
        contract=declaration["nvidiaSensorContract"],
        schema=schema["nvidiaSensorCapture"], candidate_id=candidate_id,
        run_id=declaration["certificationRunId"],
        duration_seconds=declaration["declaredDurationSeconds"],
        run_started=run_started, run_finished=run_finished,
        boundary_tolerance_seconds=boundary_seconds, run_root=run_root,
    )
    sensor_summary = validate_nvidia_sensors(
        artifacts["nvidiaSensors"], contract=declaration["nvidiaSensorContract"],
        duration_seconds=declaration["declaredDurationSeconds"],
        run_started=run_started, run_finished=run_finished,
        boundary_tolerance_seconds=boundary_seconds, gpu=host_pre["gpu"],
    )
    present_capture = load_json_stable(
        artifacts["presentMonCapture"], "PresentMon capture"
    )
    present_summary = validate_presentmon(
        present_capture, artifacts["presentMonCsv"],
        contract=declaration["presentMonContract"], schema=schema["presentMonCapture"],
        candidate_id=candidate_id, run_id=declaration["certificationRunId"],
        process_id=process_id, duration_seconds=declaration["declaredDurationSeconds"],
        run_started=run_started, run_finished=run_finished,
        boundary_tolerance_seconds=boundary_seconds, run_root=run_root,
    )
    thread_profile = load_json_stable(artifacts["threadProfile"], "thread profile")
    profile_summary = validate_thread_profile(
        thread_profile, artifacts["unrealInsightsTrace"],
        artifacts["unrealInsightsTimingCsv"], contract=declaration["profileContract"],
        schema=schema["threadProfile"], candidate_id=candidate_id,
        run_id=declaration["certificationRunId"], process_id=process_id,
        run_started=run_started, run_finished=run_finished, run_root=run_root,
    )
    live_archive_after = declaration_tool.archive_identity(archive, candidate_id)
    require(live_archive_after == declaration["archive"],
            "live archive changed by the end of validation")
    # Recheck every authority after the longest reads.  This closes accidental
    # mutation windows without pretending to resist a hostile same-user writer.
    require(load_json_stable(artifacts["runManifest"], "run manifest post-check") == manifest,
            "run manifest changed during validation")
    require(load_json_stable(declaration_path, "predeclaration post-check") == declaration,
            "predeclaration changed during validation")
    for role, binding in manifest["artifacts"].items():
        file_name = declaration["requiredArtifacts"][role]
        validate_bound_artifact(
            binding, artifacts[role], run_root, file_name, f"post-check {role}"
        )
    try:
        final_receipt, _ = receipt_verifier(
            receipt_path, prior_run_root, archive, candidate_id
        )
    except (readiness.AuditError, shipping.EvidenceError, OSError) as exc:
        raise CertificationError(
            f"three-hole receipt post-validation failed: {exc}"
        ) from None
    require(
        declaration_tool.receipt_identity(receipt_path, final_receipt)
        == declaration["threeHolePerformanceReceipt"],
        "three-hole performance receipt changed during validation",
    )
    return {
        "schema": schema["technicalReceipt"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": declaration["certificationRunId"],
        "verifiedUtc": datetime.now(timezone.utc).isoformat(
            timespec="milliseconds"
        ).replace("+00:00", "Z"),
        "state": PASS_STATE,
        "passed": True,
        "declaredDurationSeconds": declaration["declaredDurationSeconds"],
        "declaration": shipping.file_binding(declaration_path),
        "runManifest": full_binding(run_root, artifacts["runManifest"]),
        "archive": declaration["archive"],
        "threeHolePerformanceReceipt": declaration["threeHolePerformanceReceipt"],
        "measurements": {
            "hostPrePostIdentityStable": True,
            "archivePrePostIdentityStable": True,
            "nvidiaSensors": sensor_summary,
            "presentMon": present_summary,
            "threadProfile": profile_summary,
        },
        "errors": [],
        "claimBoundary": policy["claimBoundary"],
    }


def build_self_test_fixture(root: Path) -> dict[str, Any]:
    candidate_id = "S19_WindowsShipping_20200101T000000Z_abcdef123456"
    archive = root / candidate_id / "Windows"
    (archive / Path(shipping.EXECUTABLE_RELATIVE).parent).mkdir(parents=True)
    (archive / shipping.LAUNCHER_RELATIVE).write_bytes(b"launcher")
    (archive / shipping.EXECUTABLE_RELATIVE).write_bytes(b"shipping executable")
    prior_run = root / "PriorPerformance" / "11111111-2222-4333-8444-555555555555"
    prior_run.mkdir(parents=True)
    prior_receipt_path = root / "PriorReceipts" / "ShippingPerformance.json"
    prior_receipt_path.parent.mkdir()
    prior_receipt = {
        "schema": shipping.RECEIPT_SCHEMA,
        "state": readiness.EXPECTED_PASS_STATE,
        "runId": "11111111-2222-4333-8444-555555555555",
        "claimBoundary": readiness.EXPECTED_RECEIPT_CLAIMS,
    }
    shipping.write_exclusive_json(prior_receipt_path, prior_receipt)
    policy = declaration_tool.load_policy()
    run_id = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
    declaration = declaration_tool.compose_declaration(
        policy=policy,
        policy_binding=declaration_tool.stable_binding(
            declaration_tool.POLICY_PATH, "self-test policy"
        ),
        candidate_id=candidate_id,
        certification_run_id=run_id,
        duration_seconds=900,
        archive=declaration_tool.archive_identity(archive, candidate_id),
        performance_receipt=declaration_tool.receipt_identity(
            prior_receipt_path, prior_receipt
        ),
        declared_utc="2020-01-01T00:00:00.000Z",
    )
    declaration_path = root / "Declarations" / "Declaration.json"
    shipping.write_exclusive_json(declaration_path, declaration)
    run_root = root / "SustainedRuns" / run_id
    run_root.mkdir(parents=True)
    names = declaration["requiredArtifacts"]
    start = datetime(2020, 1, 1, 0, 0, 10, tzinfo=timezone.utc)
    finish = start + timedelta(seconds=900)
    iso = lambda value: value.isoformat(timespec="milliseconds").replace("+00:00", "Z")
    archive_snapshot = lambda phase, when: {
        "schema": declaration["schemas"]["archiveSnapshot"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "phase": phase,
        "capturedUtc": iso(when),
        "archive": declaration["archive"],
    }
    gpu = {
        "name": "NVIDIA GeForce RTX TEST",
        "uuid": "GPU-11111111-2222-3333-4444-555555555555",
        "driverVersion": "999.01",
        "powerDrawW": 80.0,
        "powerLimitW": 120.0,
    }
    host_snapshot = lambda phase, when: {
        "schema": declaration["schemas"]["hostSnapshot"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "phase": phase,
        "capturedUtc": iso(when),
        "gpu": dict(gpu),
        "power": {
            "activeSchemeGuid": "11111111-2222-3333-4444-555555555555",
            "activeSchemeName": "High performance",
            "acLineStatus": "ONLINE",
            "batterySaverOn": False,
        },
        "storage": {"volumeId": "C:", "freeBytes": 30 * 1024**3},
    }
    shipping.write_exclusive_json(run_root / names["archivePre"],
                                  archive_snapshot("PRE", start - timedelta(seconds=5)))
    shipping.write_exclusive_json(run_root / names["archivePost"],
                                  archive_snapshot("POST", finish + timedelta(seconds=1)))
    shipping.write_exclusive_json(run_root / names["hostPre"],
                                  host_snapshot("PRE", start - timedelta(seconds=4)))
    shipping.write_exclusive_json(run_root / names["hostPost"],
                                  host_snapshot("POST", finish + timedelta(seconds=1)))
    sensor_path = run_root / names["nvidiaSensors"]
    with sensor_path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(declaration["nvidiaSensorContract"]["requiredCsvColumns"])
        for index in range(901):
            writer.writerow([
                index, iso(start + timedelta(seconds=index)), gpu["uuid"], gpu["name"],
                gpu["driverVersion"], "70", "80", "120", "90", "2400", "8000", "P0",
            ])
    nvidia_capture = {
        "schema": declaration["schemas"]["nvidiaSensorCapture"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "startedUtc": iso(start),
        "finishedUtc": iso(finish),
        "intervalMilliseconds": 1000,
        "tool": {"fileName": "nvidia-smi.exe", "bytes": 1000, "sha256": "E" * 64,
                 "version": "test"},
        "queryFields": declaration["nvidiaSensorContract"]["requiredNvidiaSmiQueryFields"],
        "csv": full_binding(run_root, sensor_path),
    }
    shipping.write_exclusive_json(run_root / names["nvidiaSensorCapture"], nvidia_capture)
    present_csv = run_root / names["presentMonCsv"]
    with present_csv.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(declaration["presentMonContract"]["requiredCsvColumns"])
        for index in range(9001):
            writer.writerow([
                "DiscGolfTour-Win64-Shipping.exe", 4242, f"{index / 10:.1f}",
                "0" if index == 0 else "100", "Composed: Flip", "0",
            ])
    present_capture = {
        "schema": declaration["schemas"]["presentMonCapture"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "processId": 4242,
        "application": "DiscGolfTour-Win64-Shipping.exe",
        "startedUtc": iso(start),
        "finishedUtc": iso(finish),
        "tool": {"fileName": "PresentMon.exe", "bytes": 1000, "sha256": "C" * 64,
                 "version": "test"},
        "csv": full_binding(run_root, present_csv),
    }
    shipping.write_exclusive_json(run_root / names["presentMonCapture"], present_capture)
    trace_path = run_root / names["unrealInsightsTrace"]
    trace_path.write_bytes(b"2CRT" + (bytes(range(256)) * 4096))
    timing_path = run_root / names["unrealInsightsTimingCsv"]
    role_summaries: list[dict[str, Any]] = []
    with timing_path.open("x", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(declaration["profileContract"]["requiredTimingCsvColumns"])
        for role in declaration["profileContract"]["requiredThreadRoles"]:
            thread_name = role + " 1"
            for index in range(60):
                writer.writerow([role, thread_name, index, 1000])
            role_summaries.append({
                "role": role,
                "threadNames": [thread_name],
                "eventCount": 60,
                "firstStartSeconds": 0.0,
                "lastEndSeconds": 60.0,
                "totalDurationMilliseconds": 60000.0,
            })
    profile = {
        "schema": declaration["schemas"]["threadProfile"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "processId": 4242,
        "captureKind": declaration["profileContract"]["captureKind"],
        "startedUtc": iso(start + timedelta(seconds=60)),
        "finishedUtc": iso(start + timedelta(seconds=120)),
        "traceChannels": declaration["profileContract"]["requiredTraceChannels"],
        "analysisTool": {"fileName": "UnrealInsights.exe", "bytes": 1000,
                         "sha256": "D" * 64, "version": "test"},
        "trace": full_binding(run_root, trace_path),
        "timingCsv": full_binding(run_root, timing_path),
        "roleSummaries": role_summaries,
    }
    shipping.write_exclusive_json(run_root / names["threadProfile"], profile)
    artifacts = {
        role: full_binding(run_root, run_root / name)
        for role, name in names.items() if role != "runManifest"
    }
    manifest = {
        "schema": declaration["schemas"]["runManifest"],
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "certificationRunId": run_id,
        "declaration": shipping.file_binding(declaration_path),
        "startedUtc": iso(start),
        "finishedUtc": iso(finish),
        "processId": 4242,
        "generatedUtc": iso(finish + timedelta(seconds=2)),
        "artifacts": artifacts,
        "claimBoundary": RUN_MANIFEST_CAPTURE_BOUNDARY,
    }
    shipping.write_exclusive_json(run_root / names["runManifest"], manifest)
    return {
        "candidate_id": candidate_id,
        "archive": archive,
        "prior_run": prior_run,
        "receipt_path": prior_receipt_path,
        "receipt": prior_receipt,
        "declaration_path": declaration_path,
        "declaration": declaration,
        "run_root": run_root,
        "host_pre": host_snapshot("PRE", start - timedelta(seconds=4)),
        "host_post": host_snapshot("POST", finish + timedelta(seconds=1)),
        "start": start,
        "finish": finish,
        "sensor_path": sensor_path,
        "present_csv": present_csv,
        "present_capture": present_capture,
    }


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    with tempfile.TemporaryDirectory(prefix="dg-s19-sustained-validator-") as temporary:
        fixture = build_self_test_fixture(Path(temporary).resolve())
        verifier = lambda path, run, archive, candidate: (fixture["receipt"], fixture["receipt"])
        receipt = validate_evidence(
            declaration_path=fixture["declaration_path"],
            run_root=fixture["run_root"],
            archive=fixture["archive"],
            receipt_path=fixture["receipt_path"],
            prior_run_root=fixture["prior_run"],
            candidate_id=fixture["candidate_id"],
            receipt_verifier=verifier,
        )
        check(receipt["passed"] is True, "complete fixture should pass")
        check(receipt["state"] == PASS_STATE, "technical pass state differs")
        check(receipt["measurements"]["nvidiaSensors"]["sampleCount"] == 901,
              "sensor sample count differs")
        check(receipt["measurements"]["presentMon"]["presentCount"] == 9001,
              "PresentMon count differs")
        check(len(receipt["measurements"]["threadProfile"]["roles"]) == 4,
              "four profile roles are required")
        check(receipt["claimBoundary"]["humanPerformanceAcceptance"] is False,
              "technical receipt cannot grant human performance approval")
        check(receipt["claimBoundary"]["releaseReady"] is False,
              "technical receipt cannot grant release readiness")
        post = json.loads(json.dumps(fixture["host_post"]))
        post["gpu"]["driverVersion"] = "changed"
        try:
            validate_host_pair(fixture["host_pre"], post,
                               fixture["declaration"]["hostSnapshotContract"])
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "GPU driver drift must be rejected")
        placeholder = fixture["run_root"] / "placeholder.utrace"
        placeholder.write_bytes(b"\0" * 1048576)
        try:
            validate_trace(placeholder, fixture["declaration"]["profileContract"])
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "placeholder trace must be rejected")
        missing_role = fixture["run_root"] / "missing-role.csv"
        with missing_role.open("x", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle, lineterminator="\n")
            writer.writerow(fixture["declaration"]["profileContract"]["requiredTimingCsvColumns"])
            for index in range(60):
                writer.writerow(["GPU", "GPU", index, 1000])
        try:
            validate_timing_csv(missing_role, fixture["declaration"]["profileContract"])
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "missing game/render/RHI roles must be rejected")
        sensor_gap_path = fixture["run_root"] / "sensor-gap.csv"
        sensor_rows = load_csv_stable(
            fixture["sensor_path"], "self-test sensors",
            fixture["declaration"]["nvidiaSensorContract"]["requiredCsvColumns"],
        )
        sensor_rows[450]["timestampUtc"] = (
            fixture["start"] + timedelta(seconds=453)
        ).isoformat(timespec="milliseconds").replace("+00:00", "Z")
        with sensor_gap_path.open("x", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=fixture["declaration"]["nvidiaSensorContract"]["requiredCsvColumns"],
                lineterminator="\n",
            )
            writer.writeheader()
            writer.writerows(sensor_rows)
        try:
            validate_nvidia_sensors(
                sensor_gap_path,
                contract=fixture["declaration"]["nvidiaSensorContract"],
                duration_seconds=900,
                run_started=fixture["start"],
                run_finished=fixture["finish"],
                boundary_tolerance_seconds=2,
                gpu=fixture["host_pre"]["gpu"],
            )
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "NVIDIA sensor continuity gaps must be rejected")
        present_gap_path = fixture["run_root"] / "present-gap.csv"
        present_rows = load_csv_stable(
            fixture["present_csv"], "self-test PresentMon",
            fixture["declaration"]["presentMonContract"]["requiredCsvColumns"],
        )
        present_rows[500]["TimeInSeconds"] = "55.0"
        with present_gap_path.open("x", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=fixture["declaration"]["presentMonContract"]["requiredCsvColumns"],
                lineterminator="\n",
            )
            writer.writeheader()
            writer.writerows(present_rows)
        present_capture = json.loads(json.dumps(fixture["present_capture"]))
        present_capture["csv"] = full_binding(fixture["run_root"], present_gap_path)
        try:
            validate_presentmon(
                present_capture, present_gap_path,
                contract=fixture["declaration"]["presentMonContract"],
                schema=fixture["declaration"]["schemas"]["presentMonCapture"],
                candidate_id=fixture["candidate_id"],
                run_id=fixture["declaration"]["certificationRunId"],
                process_id=4242,
                duration_seconds=900,
                run_started=fixture["start"],
                run_finished=fixture["finish"],
                boundary_tolerance_seconds=2,
                run_root=fixture["run_root"],
            )
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "PresentMon chronology gaps must be rejected")
        for temporary_artifact in (
            placeholder, missing_role, sensor_gap_path, present_gap_path,
        ):
            temporary_artifact.unlink()
        extra = fixture["run_root"] / "Extra.bin"
        extra.write_bytes(b"unbound")
        try:
            validate_evidence(
                declaration_path=fixture["declaration_path"],
                run_root=fixture["run_root"],
                archive=fixture["archive"],
                receipt_path=fixture["receipt_path"],
                prior_run_root=fixture["prior_run"],
                candidate_id=fixture["candidate_id"],
                receipt_verifier=verifier,
            )
        except (CertificationError, shipping.EvidenceError):
            rejected = True
        else:
            rejected = False
        finally:
            extra.unlink()
        check(rejected, "unbound run artifacts must be rejected")
        try:
            validate_output_path(
                fixture["run_root"] / "forbidden.json",
                Path(temporary).resolve(), fixture["run_root"], fixture["archive"],
            )
        except CertificationError:
            rejected = True
        else:
            rejected = False
        check(rejected, "run-local technical receipt must be rejected")
    print(
        f"Session 19 sustained performance validator self-test OK "
        f"({checks} assertions; no game or profiling process launched)."
    )
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--declaration", type=Path)
    parser.add_argument("--run-root", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--three-hole-receipt", type=Path)
    parser.add_argument("--three-hole-run-root", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--external-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    evidence_args = (
        args.declaration, args.run_root, args.archive, args.three_hole_receipt,
        args.three_hole_run_root, args.candidate_id, args.external_root, args.output,
    )
    if args.self_test:
        if any(value is not None for value in evidence_args):
            print("--self-test accepts no evidence arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (
            AssertionError, CertificationError, declaration_tool.DeclarationError,
            shipping.EvidenceError, OSError,
        ) as exc:
            print(f"Sustained performance validator self-test FAILED: {exc}", file=sys.stderr)
            return 1
    required = evidence_args[:6]
    if any(value is None for value in required):
        print(
            "--declaration, --run-root, --archive, --three-hole-receipt, "
            "--three-hole-run-root, and --candidate-id are required",
            file=sys.stderr,
        )
        return 2
    if (args.output is None) != (args.external_root is None):
        print("--output and --external-root must be supplied together", file=sys.stderr)
        return 2
    try:
        receipt = validate_evidence(
            declaration_path=args.declaration,
            run_root=args.run_root,
            archive=args.archive,
            receipt_path=args.three_hole_receipt,
            prior_run_root=args.three_hole_run_root,
            candidate_id=args.candidate_id,
        )
        evidence_written = False
        if args.output is not None:
            output = validate_output_path(
                args.output, args.external_root, args.run_root, args.archive
            )
            shipping.write_exclusive_json(output, receipt)
            evidence_written = True
    except (
        CertificationError, declaration_tool.DeclarationError,
        shipping.EvidenceError, OSError,
    ) as exc:
        print(f"Sustained performance validation failed closed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({
        "state": receipt["state"],
        "passed": receipt["passed"],
        "candidateId": receipt["candidateId"],
        "certificationRunId": receipt["certificationRunId"],
        "technicalReceiptWritten": evidence_written,
        "humanPerformanceAcceptance": False,
        "releaseReady": False,
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
