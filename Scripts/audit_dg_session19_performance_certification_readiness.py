#!/usr/bin/env python3
"""Read-only audit of the next Session 19 performance-certification step.

This tool revalidates an existing three-hole Shipping performance run, verifies
that an append-only receipt still matches the recomputed technical facts, and
inventories host capabilities that can support later certification work.  It
never writes evidence and never upgrades a rendered regression receipt into a
thermal, power, profiling, performance-acceptance, or release-ready claim.
"""

from __future__ import annotations

import argparse
import ctypes
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Any, Callable, Optional, Sequence

import validate_dg_session19_shipping_performance as shipping_performance


EXPECTED_RECEIPT_CLAIMS = {
    "candidateBoundRenderedPerformance": True,
    "gpuThreadProfiling": False,
    "thermalSoak": False,
    "powerModeRecorded": False,
    "wallPowerRecorded": False,
    "gpuDriverRecorded": False,
    "releaseReady": False,
}

EXPECTED_PASS_STATE = "PASS_CANDIDATE_BOUND_THREE_HOLE_RENDERED_PERFORMANCE"
RECEIPT_COMPARISON_FIELDS = (
    "schema",
    "schemaVersion",
    "session",
    "candidateId",
    "runId",
    "state",
    "passed",
    "archive",
    "runManifest",
    "tooling",
    "runs",
    "errors",
    "claimBoundary",
)


class AuditError(RuntimeError):
    pass


class SystemPowerStatus(ctypes.Structure):
    _fields_ = [
        ("ACLineStatus", ctypes.c_ubyte),
        ("BatteryFlag", ctypes.c_ubyte),
        ("BatteryLifePercent", ctypes.c_ubyte),
        ("SystemStatusFlag", ctypes.c_ubyte),
        ("BatteryLifeTime", ctypes.c_uint32),
        ("BatteryFullLifeTime", ctypes.c_uint32),
    ]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AuditError(message)


def command(
    args: Sequence[str], timeout_seconds: int = 10
) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            list(args),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", exc.__class__.__name__
    return completed.returncode, completed.stdout, completed.stderr


def parse_power_scheme(output: str) -> dict[str, Any]:
    match = re.search(
        r"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
        r"[0-9a-fA-F]{4}-[0-9a-fA-F]{12})(?:\s+\(([^\r\n()]*)\))?",
        output,
    )
    if match is None:
        return {"available": False, "schemeGuid": None, "schemeName": None}
    return {
        "available": True,
        "schemeGuid": match.group(1).lower(),
        "schemeName": match.group(2).strip() if match.group(2) else None,
    }


def query_power_scheme(
    runner: Callable[[Sequence[str], int], tuple[int, str, str]] = command,
) -> dict[str, Any]:
    exit_code, stdout, _ = runner(("powercfg", "/getactivescheme"), 10)
    parsed = parse_power_scheme(stdout)
    parsed["queryExitCode"] = exit_code
    if exit_code != 0:
        parsed["available"] = False
    return parsed


def query_system_power_status() -> dict[str, Any]:
    if os.name != "nt":
        return {
            "available": False,
            "acLineStatus": "UNAVAILABLE_NON_WINDOWS",
            "batteryPercent": None,
        }
    status = SystemPowerStatus()
    try:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        function = kernel32.GetSystemPowerStatus
        function.argtypes = [ctypes.POINTER(SystemPowerStatus)]
        function.restype = ctypes.c_int
        succeeded = function(ctypes.byref(status))
    except (AttributeError, OSError):
        succeeded = 0
    if not succeeded:
        return {
            "available": False,
            "acLineStatus": "UNKNOWN",
            "batteryPercent": None,
        }
    ac_names = {0: "OFFLINE", 1: "ONLINE", 255: "UNKNOWN"}
    battery_percent = None if status.BatteryLifePercent == 255 else int(
        status.BatteryLifePercent
    )
    return {
        "available": True,
        "acLineStatus": ac_names.get(int(status.ACLineStatus), "UNKNOWN"),
        "batteryPercent": battery_percent,
        "batteryFlag": int(status.BatteryFlag),
        "batterySaverOn": bool(status.SystemStatusFlag),
    }


def parse_nvidia_smi(output: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    keys = (
        "name",
        "driverVersion",
        "temperatureC",
        "powerDrawW",
        "utilizationPercent",
        "graphicsClockMhz",
        "memoryClockMhz",
        "performanceState",
    )
    numeric = {
        "temperatureC": float,
        "powerDrawW": float,
        "utilizationPercent": float,
        "graphicsClockMhz": float,
        "memoryClockMhz": float,
    }
    for line in output.splitlines():
        values = [value.strip() for value in line.split(",")]
        if len(values) != len(keys):
            continue
        row: dict[str, Any] = dict(zip(keys, values))
        for key, converter in numeric.items():
            raw = row[key]
            if raw in {"", "N/A", "[N/A]", "Not Supported"}:
                row[key] = None
                continue
            try:
                row[key] = converter(raw)
            except ValueError:
                row[key] = None
        rows.append(row)
    return rows


def resolve_tool(explicit: Optional[Path], defaults: Sequence[Path | str]) -> Optional[Path]:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(explicit)
    for default in defaults:
        if isinstance(default, Path):
            candidates.append(default)
        else:
            discovered = shutil.which(default)
            if discovered:
                candidates.append(Path(discovered))
    for candidate in candidates:
        try:
            resolved = candidate.resolve(strict=True)
        except (OSError, RuntimeError):
            continue
        if resolved.is_file() and not shipping_performance.is_reparse_point(resolved):
            return resolved
    return None


def tool_inventory(path: Optional[Path]) -> dict[str, Any]:
    if path is None:
        return {"available": False, "path": None, "binding": None}
    return {
        "available": True,
        "path": str(path),
        "binding": shipping_performance.file_binding(path),
    }


def query_nvidia_smi(
    path: Optional[Path],
    runner: Callable[[Sequence[str], int], tuple[int, str, str]] = command,
) -> dict[str, Any]:
    inventory = tool_inventory(path)
    inventory["gpus"] = []
    inventory["queryExitCode"] = None
    if path is None:
        return inventory
    fields = (
        "name,driver_version,temperature.gpu,power.draw,utilization.gpu,"
        "clocks.current.graphics,clocks.current.memory,pstate"
    )
    exit_code, stdout, _ = runner(
        (str(path), f"--query-gpu={fields}", "--format=csv,noheader,nounits"), 10
    )
    inventory["queryExitCode"] = exit_code
    inventory["gpus"] = parse_nvidia_smi(stdout) if exit_code == 0 else []
    return inventory


def validate_receipt_against_run(
    receipt_path: Path, run_root: Path, archive: Path, candidate_id: str
) -> tuple[dict[str, Any], dict[str, Any]]:
    try:
        receipt_path = receipt_path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise AuditError(
            f"receipt path cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(receipt_path.is_file(), "receipt is not a regular file")
    require(
        not shipping_performance.is_reparse_point(receipt_path),
        "receipt cannot be a reparse point",
    )
    for forbidden, label in (
        (shipping_performance.ROOT, "project"),
        (archive, "archive"),
        (run_root, "run"),
    ):
        require(
            not shipping_performance.is_same_or_under(receipt_path, forbidden),
            f"receipt must remain outside the {label} root",
        )

    receipt = shipping_performance.load_json(receipt_path)
    require(receipt.get("passed") is True, "receipt does not record a technical pass")
    require(receipt.get("state") == EXPECTED_PASS_STATE, "receipt pass state differs")
    require(receipt.get("candidateId") == candidate_id, "receipt candidate differs")
    require(
        receipt.get("claimBoundary") == EXPECTED_RECEIPT_CLAIMS,
        "receipt claim boundary differs or overclaims certification",
    )

    recomputed = shipping_performance.validate_run(
        run_root.resolve(), archive.resolve(), candidate_id
    )
    require(recomputed.get("passed") is True, "performance run no longer revalidates")
    for field in RECEIPT_COMPARISON_FIELDS:
        require(
            receipt.get(field) == recomputed.get(field),
            f"receipt no longer matches recomputed {field}",
        )
    return receipt, recomputed


def build_audit(
    candidate_id: str,
    receipt_path: Path,
    receipt: dict[str, Any],
    power_scheme: dict[str, Any],
    system_power: dict[str, Any],
    nvidia: dict[str, Any],
    unreal_insights: dict[str, Any],
    presentmon: dict[str, Any],
    free_system_drive_bytes: int,
) -> dict[str, Any]:
    receipt_gpus = sorted({row["gpuBrand"] for row in receipt["runs"]})
    observed_names = sorted(
        row["name"] for row in nvidia.get("gpus", []) if isinstance(row.get("name"), str)
    )
    matching_gpu = bool(set(receipt_gpus).intersection(observed_names))
    driver_snapshot = any(
        isinstance(row.get("driverVersion"), str) and bool(row["driverVersion"])
        for row in nvidia.get("gpus", [])
        if row.get("name") in receipt_gpus
    )
    temperature_snapshot = any(
        row.get("temperatureC") is not None
        for row in nvidia.get("gpus", [])
        if row.get("name") in receipt_gpus
    )

    return {
        "schema": "DiscGolfTour.Session19PerformanceCertificationReadinessAudit.v1",
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "observedUtc": utc_now(),
        "state": "READ_ONLY_READINESS_INVENTORY_RELEASE_CERTIFICATION_PENDING",
        "performanceReceipt": {
            "path": str(receipt_path.resolve()),
            "binding": shipping_performance.file_binding(receipt_path.resolve()),
            "runId": receipt["runId"],
            "revalidated": True,
            "gpuBrands": receipt_gpus,
        },
        "hostSnapshot": {
            "activePowerScheme": power_scheme,
            "systemPower": system_power,
            "nvidiaSmi": nvidia,
            "receiptGpuMatchesCurrentAdapter": matching_gpu,
            "unrealInsights": unreal_insights,
            "presentMon": presentmon,
            "systemDriveFreeBytes": free_system_drive_bytes,
        },
        "automationReadiness": {
            "driverSnapshotAvailable": driver_snapshot,
            "acLineStatusAvailable": system_power.get("available") is True,
            "activePowerSchemeAvailable": power_scheme.get("available") is True,
            "gpuTemperaturePointSnapshotAvailable": temperature_snapshot,
            "unrealInsightsInstalled": unreal_insights.get("available") is True,
            "presentMonInstalled": presentmon.get("available") is True,
            "candidateHasGuardedSustainedTraceSoakEntrypoint": False,
        },
        "remainingBlockers": [
            {
                "id": "SUSTAINED_THERMAL_SOAK_NOT_CAPTURED",
                "automatable": True,
                "currentFact": "one instantaneous sensor reading is not a sustained soak",
            },
            {
                "id": "GPU_THREAD_PROFILE_NOT_CAPTURED",
                "automatable": True,
                "currentFact": "installed tools do not substitute for a bound profile artifact",
            },
            {
                "id": "POWER_AND_WALL_POWER_NOT_BOUND_TO_PERFORMANCE_WINDOW",
                "automatable": True,
                "currentFact": "readiness-time state is not performance-window evidence",
            },
            {
                "id": "GPU_DRIVER_NOT_BOUND_TO_PERFORMANCE_WINDOW",
                "automatable": True,
                "currentFact": "readiness-time driver state is not performance-window evidence",
            },
            {
                "id": "PERFORMANCE_ACCEPTANCE_REVIEW_PENDING",
                "automatable": False,
                "currentFact": "technical measurements cannot grant owner acceptance",
            },
        ],
        "nextAutomatableStep": {
            "id": "PREDECLARE_AND_IMPLEMENT_EXTERNAL_CERTIFICATION_RUNNER",
            "requirements": [
                "bind the exact candidate archive and validated three-hole receipt before launch",
                "record GPU driver, active power scheme, AC-line state, and free space before and after the run",
                "sample GPU temperature, utilization, clocks, and available power telemetry over a predeclared sustained duration",
                "capture a real GPU/game/render/RHI-thread profile over a predeclared representative window",
                "hash-bind every external artifact and independently validate chronology, continuity, thresholds, and unchanged archive inventory",
                "retain performance acceptance and release readiness as false pending named human review",
            ],
            "candidateConstraint": (
                "the current guarded Shipping performance entrypoint allows only an exact "
                "30-second capture and no trace flags; a continuous trace/soak claim needs a "
                "separately predeclared guarded lane in a new candidate or another explicitly "
                "approved external profiling method"
            ),
        },
        "claimBoundary": {
            "readOnlyInventory": True,
            "candidateBoundRenderedPerformance": True,
            "gpuDriverRecordedForPerformanceWindow": False,
            "powerModeRecordedForPerformanceWindow": False,
            "wallPowerRecordedForPerformanceWindow": False,
            "sustainedThermalSoak": False,
            "gpuThreadProfile": False,
            "performanceAcceptance": False,
            "releaseReady": False,
        },
    }


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        require(condition, message)

    scheme = parse_power_scheme(
        "Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced)"
    )
    check(scheme["available"] is True, "power scheme should parse")
    check(scheme["schemeName"] == "Balanced", "power scheme name should parse")
    check(
        scheme["schemeGuid"] == "381b4222-f694-41f0-9685-ff5bb260df2e",
        "power scheme GUID should normalize",
    )
    check(not parse_power_scheme("unavailable")["available"], "bad power output should fail")

    rows = parse_nvidia_smi(
        "NVIDIA GeForce RTX 5060 Laptop GPU, 592.19, 39, 13.02, 0, 277, 405, P8\n"
    )
    check(len(rows) == 1, "one GPU row should parse")
    check(rows[0]["temperatureC"] == 39.0, "temperature should parse")
    check(rows[0]["powerDrawW"] == 13.02, "power should parse")
    check(rows[0]["driverVersion"] == "592.19", "driver should parse")
    check(not parse_nvidia_smi("malformed"), "malformed GPU rows should be ignored")
    na_rows = parse_nvidia_smi("GPU, 1.0, [N/A], N/A, 5, 1, 2, P0")
    check(na_rows[0]["temperatureC"] is None, "N/A temperature should remain unknown")
    check(na_rows[0]["powerDrawW"] is None, "N/A power should remain unknown")

    fake_receipt = {
        "runId": "11111111-2222-4333-8444-555555555555",
        "runs": [
            {"gpuBrand": "GPU"},
            {"gpuBrand": "GPU"},
            {"gpuBrand": "GPU"},
        ],
    }
    fake_nvidia = {
        "available": True,
        "gpus": [
            {
                "name": "GPU",
                "driverVersion": "1.0",
                "temperatureC": 40.0,
            }
        ],
    }
    # The self-test must not require or write a real receipt.  Supply this script
    # as a harmless existing file for the output-only binding shape.
    audit = build_audit(
        "S19_WindowsShipping_20260827T023919Z_51a1c132f837",
        Path(__file__),
        fake_receipt,
        {"available": True},
        {"available": True, "acLineStatus": "ONLINE"},
        fake_nvidia,
        {"available": True},
        {"available": True},
        1,
    )
    check(audit["performanceReceipt"]["revalidated"] is True, "receipt flag should remain technical")
    check(audit["hostSnapshot"]["receiptGpuMatchesCurrentAdapter"], "GPU identity should match")
    check(audit["automationReadiness"]["driverSnapshotAvailable"], "driver should be available")
    check(audit["automationReadiness"]["gpuTemperaturePointSnapshotAvailable"], "temperature should be available")
    check(not audit["automationReadiness"]["candidateHasGuardedSustainedTraceSoakEntrypoint"], "current lane must stay bounded")
    check(not audit["claimBoundary"]["sustainedThermalSoak"], "point sample cannot claim soak")
    check(not audit["claimBoundary"]["gpuThreadProfile"], "installed tool cannot claim profile")
    check(not audit["claimBoundary"]["performanceAcceptance"], "audit cannot approve performance")
    check(not audit["claimBoundary"]["releaseReady"], "audit cannot approve release")
    check(len(audit["remainingBlockers"]) == 5, "all blockers should remain explicit")
    check(
        EXPECTED_RECEIPT_CLAIMS["candidateBoundRenderedPerformance"] is True,
        "receipt may claim only the rendered candidate-bound fact",
    )
    check(
        sum(value is True for value in EXPECTED_RECEIPT_CLAIMS.values()) == 1,
        "receipt boundary must contain one positive claim",
    )
    print(f"Session 19 performance certification readiness self-test OK ({checks} assertions).")
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--run-root", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--nvidia-smi", type=Path)
    parser.add_argument("--unreal-insights", type=Path)
    parser.add_argument("--presentmon", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    evidence_args = (
        args.receipt,
        args.run_root,
        args.archive,
        args.candidate_id,
        args.nvidia_smi,
        args.unreal_insights,
        args.presentmon,
    )
    if args.self_test:
        if any(value is not None for value in evidence_args):
            print("--self-test accepts no evidence or tool arguments", file=sys.stderr)
            return 2
        try:
            return run_self_test()
        except (AuditError, OSError) as exc:
            print(f"Performance certification readiness self-test FAILED: {exc}", file=sys.stderr)
            return 1

    if any(value is None for value in evidence_args[:4]):
        print(
            "--receipt, --run-root, --archive, and --candidate-id are required",
            file=sys.stderr,
        )
        return 2

    try:
        receipt, _ = validate_receipt_against_run(
            args.receipt, args.run_root, args.archive, args.candidate_id
        )
        nvidia_path = resolve_tool(
            args.nvidia_smi,
            ("nvidia-smi.exe", Path(r"C:\Windows\System32\nvidia-smi.exe")),
        )
        insights_path = resolve_tool(
            args.unreal_insights,
            (
                Path(
                    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealInsights.exe"
                ),
            ),
        )
        presentmon_path = resolve_tool(
            args.presentmon,
            (
                "PresentMon.exe",
                Path(
                    r"C:\Program Files\NVIDIA Corporation\FrameViewSDK\bin\PresentMon_x64.exe"
                ),
            ),
        )
        system_drive = Path(os.environ.get("SystemDrive", "C:") + "\\")
        free_bytes = shutil.disk_usage(system_drive).free
        audit = build_audit(
            args.candidate_id,
            args.receipt,
            receipt,
            query_power_scheme(),
            query_system_power_status(),
            query_nvidia_smi(nvidia_path),
            tool_inventory(insights_path),
            tool_inventory(presentmon_path),
            free_bytes,
        )
    except (
        AuditError,
        shipping_performance.EvidenceError,
        OSError,
        ValueError,
    ) as exc:
        print(f"Performance certification readiness audit FAILED: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(audit, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
