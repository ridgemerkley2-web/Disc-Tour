#!/usr/bin/env python3
"""Read-only audit of Shipping-compatible sustained profiling capabilities.

The sealed Session 19 candidate is an ordinary Unreal Shipping build.  This
audit distinguishes three separate facts that must not be conflated:

* whether the project/engine build rules enable full Unreal trace in Shipping;
* whether the installed Unreal Insights build can headlessly export a real
  trace after one exists; and
* whether this Windows host has an external CPU/GPU ETW capture and export lane.

It never launches the game, starts WPR, creates a trace, writes evidence, or
claims performance/release acceptance.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Any, Optional, Sequence

import validate_dg_session19_shipping_performance as shipping


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CANDIDATE_ID = "S19_WindowsShipping_20260827T023919Z_51a1c132f837"
DEFAULT_ARCHIVE = (
    Path(r"C:\DGTour_Packages") / DEFAULT_CANDIDATE_ID / "Windows"
)
DEFAULT_ENGINE_ROOT = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine")
TARGET_RELATIVE = Path("Source/DiscGolfTour.Target.cs")
ENGINE_TARGET_RULES_RELATIVE = Path(
    "Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs"
)
TRACE_CONFIG_RELATIVE = Path("Source/Runtime/TraceLog/Public/Trace/Config.h")
INSIGHTS_EXPORT_TEST_RELATIVE = Path(
    "Source/Developer/TraceInsights/Private/Insights/Tests/FunctionalTests/"
    "ExportCommandsTests.cpp"
)
INSIGHTS_MANAGER_RELATIVE = Path(
    "Source/Developer/TraceInsights/Private/Insights/TimingProfiler/"
    "TimingProfilerManager.cpp"
)
INSIGHTS_EXPORTER_RELATIVE = Path(
    "Source/Developer/TraceInsights/Private/Insights/TimingProfiler/ViewModels/"
    "TimingExporter.cpp"
)
UNREAL_INSIGHTS_RELATIVE = Path("Binaries/Win64/UnrealInsights.exe")
STATE = "BLOCKED_SHIPPING_PROFILE_LANE_EXTERNAL_ETW_EXPORT_REQUIRED"


class ProfileLaneError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ProfileLaneError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def read_regular_text(path: Path, label: str) -> tuple[str, dict[str, Any]]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise ProfileLaneError(
            f"{label} cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            f"{label} is not a regular file")
    before = resolved.stat()
    try:
        payload = resolved.read_bytes()
    except OSError as exc:
        raise ProfileLaneError(
            f"{label} cannot be read ({exc.__class__.__name__})"
        ) from None
    after = resolved.stat()
    fingerprint = lambda value: (
        value.st_size, value.st_mtime_ns, value.st_ctime_ns,
        value.st_dev, value.st_ino,
    )
    require(fingerprint(before) == fingerprint(after) and len(payload) == after.st_size,
            f"{label} changed while read")
    try:
        text = payload.decode("utf-8-sig")
    except UnicodeDecodeError:
        raise ProfileLaneError(f"{label} is not UTF-8 text") from None
    return text, shipping.file_binding(resolved)


def project_shipping_trace_intent(target_text: str) -> dict[str, Any]:
    explicit_enable = re.search(
        r"\b(?:bEnableTrace|bForceEnableTrace)\s*=\s*true\s*;", target_text
    ) is not None
    explicit_disable = re.search(
        r"\b(?:bEnableTrace|bForceEnableTrace)\s*=\s*false\s*;", target_text
    ) is not None
    return {
        "explicitFullTraceEnable": explicit_enable,
        "explicitTraceDisable": explicit_disable,
        "ordinaryShippingDefaultApplies": not explicit_enable,
    }


def engine_shipping_trace_rules(
    target_rules_text: str, trace_config_text: str
) -> dict[str, Any]:
    default_disabled = re.search(
        r"public\s+virtual\s+bool\s+bEnableTrace\b.*?"
        r"if\s*\(\s*Configuration\s*==\s*UnrealTargetConfiguration\.Shipping\s*"
        r"\|\|\s*Type\s*==\s*TargetType\.Program\s*\)\s*\{\s*return\s+false\s*;",
        target_rules_text,
        flags=re.DOTALL,
    ) is not None
    full_shipping_unsupported = all(token in trace_config_text for token in (
        "Full tracing in shipping is not supported.",
        "static_assert(!TRACE_PRIVATE_FULL_ENABLED",
        "UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL",
    ))
    return {
        "ordinaryShippingFullTraceDefaultEnabled": not default_disabled,
        "ordinaryShippingFullTraceDefaultDisabled": default_disabled,
        "fullShippingTraceUnsupportedGuardPresent": full_shipping_unsupported,
    }


def insights_export_capability(
    export_test_text: str, manager_text: str, exporter_text: str = ""
) -> dict[str, Any]:
    launch_tokens = (
        "-OpenTraceFile=",
        "-AutoQuit",
        "-NoUI",
        "-ExecOnAnalysisCompleteCmd=",
    )
    command_tokens = (
        "TimingInsights.ExportThreads",
        "TimingInsights.ExportTimers",
        "TimingInsights.ExportTimingEvents",
    )
    filter_tokens = (
        "-columns=",
        "-threads=",
        "ThreadName",
        "StartTime",
        "Duration",
    )
    implementation_text = manager_text + "\n" + exporter_text
    return {
        "headlessAnalysisFlagsPresent": all(
            token in export_test_text for token in launch_tokens
        ),
        "timingExportCommandsPresent": all(
            token in manager_text for token in command_tokens
        ),
        "threadAndColumnFiltersPresent": all(
            token in implementation_text for token in filter_tokens
        ),
        "requiredLaunchTokens": list(launch_tokens),
        "requiredCommandTokens": list(command_tokens),
    }


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


def resolve_regular_tool(explicit: Optional[Path], defaults: Sequence[Path | str]) -> Optional[Path]:
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
        if resolved.is_file() and not shipping.is_reparse_point(resolved):
            return resolved
    return None


def tool_binding(path: Optional[Path]) -> dict[str, Any]:
    if path is None:
        return {"available": False, "path": None, "binding": None}
    return {
        "available": True,
        "path": str(path),
        "binding": shipping.file_binding(path),
    }


def audit(
    *, candidate_id: str, archive: Path, engine_root: Path,
    wpr: Optional[Path], wpaexporter: Optional[Path],
    unreal_insights: Optional[Path],
) -> dict[str, Any]:
    archive = shipping.validate_candidate_archive_path(archive, candidate_id)
    archive_identity = shipping.build_archive_manifest(archive, candidate_id)
    target_text, target_binding = read_regular_text(
        ROOT / TARGET_RELATIVE, "game target rules"
    )
    target_rules_text, target_rules_binding = read_regular_text(
        engine_root / ENGINE_TARGET_RULES_RELATIVE, "engine target rules"
    )
    trace_config_text, trace_config_binding = read_regular_text(
        engine_root / TRACE_CONFIG_RELATIVE, "engine trace configuration"
    )
    export_test_text, export_test_binding = read_regular_text(
        engine_root / INSIGHTS_EXPORT_TEST_RELATIVE, "Insights export test source"
    )
    manager_text, manager_binding = read_regular_text(
        engine_root / INSIGHTS_MANAGER_RELATIVE, "Insights timing manager source"
    )
    exporter_text, exporter_binding = read_regular_text(
        engine_root / INSIGHTS_EXPORTER_RELATIVE, "Insights timing exporter source"
    )

    target = project_shipping_trace_intent(target_text)
    engine = engine_shipping_trace_rules(target_rules_text, trace_config_text)
    insights_source = insights_export_capability(
        export_test_text, manager_text, exporter_text
    )
    wpr_inventory = tool_binding(wpr)
    if wpr is not None:
        code, stdout, stderr = run_command((str(wpr), "-profiles"))
        combined = stdout + "\n" + stderr
        wpr_inventory.update({
            "profilesExitCode": code,
            "cpuProfileAvailable": re.search(r"(?m)^\s*CPU\s+", combined) is not None,
            "gpuProfileAvailable": re.search(r"(?m)^\s*GPU\s+", combined) is not None,
        })
    else:
        wpr_inventory.update({
            "profilesExitCode": None,
            "cpuProfileAvailable": False,
            "gpuProfileAvailable": False,
        })

    full_trace_source_enabled = (
        target["explicitFullTraceEnable"]
        or engine["ordinaryShippingFullTraceDefaultEnabled"]
    )
    headless_export_ready = (
        unreal_insights is not None
        and insights_source["headlessAnalysisFlagsPresent"]
        and insights_source["timingExportCommandsPresent"]
        and insights_source["threadAndColumnFiltersPresent"]
    )
    external_capture_ready = (
        wpr_inventory["cpuProfileAvailable"]
        and wpr_inventory["gpuProfileAvailable"]
    )
    external_export_ready = wpaexporter is not None

    blockers: list[str] = []
    if not full_trace_source_enabled:
        blockers.append(
            "ordinary Shipping target does not compile full Unreal CPU/GPU trace"
        )
    if not external_capture_ready:
        blockers.append("Windows CPU/GPU ETW capture tooling is unavailable")
    if not external_export_ready:
        blockers.append("WPAExporter/xperf analysis tooling is unavailable")
    blockers.append(
        "no candidate-bound predeclared external ETW capture/export run exists"
    )

    return {
        "schema": "DiscGolfTour.Session19ShippingProfileLaneAudit.v1",
        "schemaVersion": 1,
        "session": 19,
        "generatedUtc": utc_now(),
        "candidateId": candidate_id,
        "state": STATE,
        "archive": {
            "inventorySha256": archive_identity["inventorySha256"],
            "fileCount": archive_identity["fileCount"],
            "totalBytes": archive_identity["totalBytes"],
            "shippingExecutable": archive_identity["shippingExecutable"],
        },
        "sourceRules": {
            "projectTarget": {"binding": target_binding, **target},
            "engineTargetRules": {"binding": target_rules_binding, **engine},
            "engineTraceConfig": {"binding": trace_config_binding},
        },
        "unrealInsightsExport": {
            "tool": tool_binding(unreal_insights),
            "exportTestSource": export_test_binding,
            "timingManagerSource": manager_binding,
            "timingExporterSource": exporter_binding,
            **insights_source,
            "headlessExportReadyAfterRealTraceExists": headless_export_ready,
        },
        "externalWindowsProfileLane": {
            "wpr": wpr_inventory,
            "wpaExporter": tool_binding(wpaexporter),
            "cpuGpuCaptureToolingReady": external_capture_ready,
            "automatedAnalysisExportReady": external_export_ready,
            "candidateBoundCaptureExecuted": False,
        },
        "blockers": blockers,
        "claimBoundary": {
            "readOnlyAudit": True,
            "gameLaunched": False,
            "traceCaptured": False,
            "profileExported": False,
            "sealedBinaryFullTraceOperationallyProven": False,
            "shippingPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    ordinary_target = "public class T : TargetRules { Type = TargetType.Game; }"
    traced_target = ordinary_target + " bEnableTrace = true;"
    disabled = project_shipping_trace_intent(ordinary_target)
    enabled = project_shipping_trace_intent(traced_target)
    check(disabled["ordinaryShippingDefaultApplies"] is True,
          "ordinary target must use Shipping default")
    check(disabled["explicitFullTraceEnable"] is False,
          "ordinary target must not claim trace enable")
    check(enabled["explicitFullTraceEnable"] is True,
          "explicit trace enable was not detected")
    engine = engine_shipping_trace_rules(
        "public virtual bool bEnableTrace { get { if (Configuration == "
        "UnrealTargetConfiguration.Shipping || Type == TargetType.Program) "
        "{ return false; } } }",
        "UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL "
        "static_assert(!TRACE_PRIVATE_FULL_ENABLED, "
        "\"Full tracing in shipping is not supported.\");",
    )
    check(engine["ordinaryShippingFullTraceDefaultDisabled"] is True,
          "Shipping trace default must be detected")
    check(engine["fullShippingTraceUnsupportedGuardPresent"] is True,
          "full Shipping trace guard must be detected")
    unrelated_false = engine_shipping_trace_rules(
        "public virtual bool bEnableTrace { get { return true; } } "
        "public bool OtherRule { get { return false; } }",
        "UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL "
        "static_assert(!TRACE_PRIVATE_FULL_ENABLED, "
        "\"Full tracing in shipping is not supported.\");",
    )
    check(unrelated_false["ordinaryShippingFullTraceDefaultDisabled"] is False,
          "an unrelated return false must not satisfy the Shipping trace rule")
    export = insights_export_capability(
        "-OpenTraceFile= -AutoQuit -NoUI -ExecOnAnalysisCompleteCmd=",
        "TimingInsights.ExportThreads TimingInsights.ExportTimers "
        "TimingInsights.ExportTimingEvents -columns= -threads= "
        "ThreadName StartTime Duration",
    )
    check(export["headlessAnalysisFlagsPresent"] is True,
          "headless flags must be detected")
    check(export["timingExportCommandsPresent"] is True,
          "timing commands must be detected")
    check(export["threadAndColumnFiltersPresent"] is True,
          "timing filters must be detected")
    missing = insights_export_capability("-OpenTraceFile=", "")
    check(missing["headlessAnalysisFlagsPresent"] is False,
          "partial headless flags must fail")
    check(missing["timingExportCommandsPresent"] is False,
          "missing timing commands must fail")
    check(STATE.startswith("BLOCKED_"), "audit state must remain blocked")
    print(
        f"Session 19 Shipping profile-lane audit self-test OK ({checks} assertions; "
        "no game, WPR, or Insights process launched)."
    )
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id", default=DEFAULT_CANDIDATE_ID)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--wpr", type=Path)
    parser.add_argument("--wpaexporter", type=Path)
    parser.add_argument("--unreal-insights", type=Path)
    parser.add_argument("--require-ready", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        try:
            return run_self_test()
        except (AssertionError, ProfileLaneError, OSError) as exc:
            print(f"Shipping profile-lane self-test FAILED: {exc}", file=sys.stderr)
            return 1
    wpr = resolve_regular_tool(args.wpr, (Path(r"C:\Windows\System32\wpr.exe"), "wpr"))
    wpa = resolve_regular_tool(args.wpaexporter, (
        Path(r"C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\wpaexporter.exe"),
        "wpaexporter",
    ))
    insights = resolve_regular_tool(
        args.unreal_insights,
        (args.engine_root / UNREAL_INSIGHTS_RELATIVE,),
    )
    try:
        result = audit(
            candidate_id=args.candidate_id, archive=args.archive,
            engine_root=args.engine_root, wpr=wpr, wpaexporter=wpa,
            unreal_insights=insights,
        )
    except (ProfileLaneError, shipping.EvidenceError, OSError) as exc:
        print(f"Shipping profile-lane audit failed closed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    if args.require_ready:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
