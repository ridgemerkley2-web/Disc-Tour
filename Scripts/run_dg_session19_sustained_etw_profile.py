#!/usr/bin/env python3
"""Guarded runner/finalizer for the additive Session 19 WPR ETW v3 lane.

No mode starts capture by default.  Read-only preflight validates an immutable
predeclaration plus the live candidate/tool identities.  Capture additionally
requires explicit mode and exact candidate/run confirmations; it is forbidden
unless both WPR CPU/GPU profiles and a version-bound WPAExporter are ready.

Finalize never captures or exports.  It independently validates the real ETL,
strict raw CPU/GPU CSVs, and a normalized four-role GPU/GameThread/RenderThread/
RHIThread export before writing an append-only technical manifest.  All human,
product-owner, legal, and release approvals remain false.
"""

from __future__ import annotations

import argparse
import copy
import ctypes
from datetime import datetime, timedelta, timezone
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any, Optional, Sequence

import audit_dg_session19_shipping_profile_lane as profile_lane
import generate_dg_session19_sustained_etw_profile_declaration as declaration_tool
import validate_dg_session19_shipping_performance as shipping
import validate_dg_session19_sustained_etw_profile as etw


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = etw.POLICY_PATH
DEFAULT_ARCHIVE = etw.DEFAULT_ARCHIVE
DEFAULT_EXTERNAL_ROOT = etw.DEFAULT_EXTERNAL_ROOT
PREFLIGHT_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwRunnerPreflight.v3"
FAILURE_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwCaptureFailure.v3"
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
    "-trace", "-stat", "-exec", "-benchmark", "-nullrhi",
    "-fixedseed", "-fps", "-usefixedtimestep", "-log", "-abslog",
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


def run_command(
    args: Sequence[str], timeout_seconds: int = 30
) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            list(args), check=False, capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=timeout_seconds,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", exc.__class__.__name__
    return completed.returncode, completed.stdout, completed.stderr


def fixed_game_arguments(user_dir: Path) -> list[str]:
    arguments = [*FIXED_GAME_ARGUMENTS, f"-UserDir={user_dir}"]
    lowered = [argument.casefold() for argument in arguments]
    require(not any(
        argument.startswith(prefix)
        for argument in lowered for prefix in FORBIDDEN_ARGUMENT_PREFIXES
    ), "fixed Shipping arguments contain a forbidden trace/stat/log switch")
    require(sum(argument.startswith("-userdir=") for argument in lowered) == 1,
            "exactly one external UserDir argument is required")
    return arguments


def windows_process_facts(process: subprocess.Popen[Any]) -> tuple[str, Path]:
    require(os.name == "nt", "Windows process facts require Windows")

    class FileTime(ctypes.Structure):
        _fields_ = [("low", ctypes.c_uint32), ("high", ctypes.c_uint32)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetProcessTimes.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
        ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
    ]
    kernel32.GetProcessTimes.restype = ctypes.c_int
    kernel32.QueryFullProcessImageNameW.argtypes = [
        ctypes.c_void_p, ctypes.c_uint32, ctypes.c_wchar_p,
        ctypes.POINTER(ctypes.c_uint32),
    ]
    kernel32.QueryFullProcessImageNameW.restype = ctypes.c_int
    creation = FileTime()
    exit_time = FileTime()
    kernel_time = FileTime()
    user_time = FileTime()
    handle = ctypes.c_void_p(int(process._handle))  # type: ignore[attr-defined]
    require(bool(kernel32.GetProcessTimes(
        handle, ctypes.byref(creation), ctypes.byref(exit_time),
        ctypes.byref(kernel_time), ctypes.byref(user_time),
    )), "GetProcessTimes failed for the launched Shipping process")
    ticks = (creation.high << 32) | creation.low
    unix_seconds = ticks / 10_000_000 - 11_644_473_600
    created = datetime.fromtimestamp(unix_seconds, timezone.utc).isoformat(
        timespec="milliseconds"
    ).replace("+00:00", "Z")
    capacity = ctypes.c_uint32(32768)
    buffer = ctypes.create_unicode_buffer(capacity.value)
    require(bool(kernel32.QueryFullProcessImageNameW(
        handle, 0, buffer, ctypes.byref(capacity)
    )), "QueryFullProcessImageNameW failed for the launched Shipping process")
    try:
        image = Path(buffer.value).resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise RunnerError(
            f"launched process image cannot be resolved ({exc.__class__.__name__})"
        ) from None
    return created, image


def windows_process_exit_utc(process: subprocess.Popen[Any]) -> str:
    require(os.name == "nt", "Windows process exit facts require Windows")

    class FileTime(ctypes.Structure):
        _fields_ = [("low", ctypes.c_uint32), ("high", ctypes.c_uint32)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetProcessTimes.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
        ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
    ]
    kernel32.GetProcessTimes.restype = ctypes.c_int
    values = [FileTime() for _ in range(4)]
    handle = ctypes.c_void_p(int(process._handle))  # type: ignore[attr-defined]
    require(bool(kernel32.GetProcessTimes(
        handle, *(ctypes.byref(value) for value in values)
    )), "GetProcessTimes failed after Shipping process exit")
    ticks = (values[1].high << 32) | values[1].low
    require(ticks > 0, "Shipping process exit time is unavailable")
    unix_seconds = ticks / 10_000_000 - 11_644_473_600
    return datetime.fromtimestamp(unix_seconds, timezone.utc).isoformat(
        timespec="milliseconds"
    ).replace("+00:00", "Z")


def resolve_declared_file(path_text: Any, label: str) -> Path:
    require(isinstance(path_text, str) and path_text,
            f"{label} path is missing")
    try:
        resolved = Path(path_text).resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise RunnerError(
            f"{label} is missing ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            f"{label} must be a regular non-reparse file")
    return resolved


def declaration_destination(
    declaration: dict[str, Any], external: Path
) -> Path:
    return declaration_tool.validate_destination_chain(
        external, declaration["externalLayout"]["declarationRelativePath"]
    )


def run_destination(declaration: dict[str, Any], external: Path) -> Path:
    return declaration_tool.validate_destination_chain(
        external, declaration["externalLayout"]["runRelativePath"]
    )


def validate_wpr_profiles(wpr: Path) -> dict[str, Any]:
    code, stdout, stderr = run_command((str(wpr), "-profiles"), 15)
    combined = stdout + "\n" + stderr
    return {
        "profilesExitCode": code,
        "cpuProfileAvailable": re.search(r"(?m)^\s*CPU\s+", combined) is not None,
        "gpuProfileAvailable": re.search(r"(?m)^\s*GPU\s+", combined) is not None,
    }


def load_context(
    declaration_path: Path, archive: Path, external_root: Path,
) -> dict[str, Any]:
    try:
        resolved_declaration = declaration_path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise RunnerError(
            f"declaration is missing ({exc.__class__.__name__})"
        ) from None
    require(resolved_declaration.is_file()
            and not shipping.is_reparse_point(resolved_declaration),
            "declaration must be a regular non-reparse file")
    policy = etw.load_json(POLICY_PATH, "ETW v3 policy")
    etw.validate_policy(policy)
    policy_binding = etw.stable_binding(POLICY_PATH, "ETW v3 policy")
    declaration = etw.load_json(resolved_declaration, "ETW v3 declaration")
    declaration_tool.validate_declaration(declaration, policy, policy_binding)
    archive, receipt, performance_run_root = etw.validate_baseline(
        policy, archive, external_root
    )
    external = declaration_tool.validate_external_root(external_root, archive)
    require(resolved_declaration == declaration_destination(declaration, external),
            "declaration is not at its exact predeclared external path")
    declared_archive = Path(declaration["candidateArchive"]["path"])
    require(declared_archive.resolve(strict=True) == archive,
            "declaration archive path differs from the selected archive")
    require(declaration["shippingPerformance"]["validatedRunId"] == receipt["runId"]
            and Path(declaration["shippingPerformance"]["validatedRunRoot"])
            .resolve(strict=True) == performance_run_root,
            "declaration performance run differs after revalidation")

    wpr_path = resolve_declared_file(declaration["tooling"]["wpr"]["path"], "WPR")
    live_wpr_binding = declaration_tool.versioned_tool_binding(wpr_path)
    require(live_wpr_binding == declaration["tooling"]["wpr"]["binding"],
            "live WPR identity/version differs from the declaration")
    profiles = validate_wpr_profiles(wpr_path)
    profile_details = declaration_tool.wpr_profile_details(
        wpr_path, policy["wprContract"]
    )
    status = declaration_tool.wpr_status(
        wpr_path, policy["wprContract"]["requiredIdleStatusToken"]
    )
    exporter_declaration = declaration["tooling"]["wpaExporter"]
    tracerpt_path = resolve_declared_file(
        declaration["tooling"]["tracerpt"]["path"], "tracerpt"
    )
    live_tracerpt_binding = declaration_tool.versioned_tool_binding(tracerpt_path)
    require(live_tracerpt_binding == declaration["tooling"]["tracerpt"]["binding"],
            "live tracerpt identity/version differs from the declaration")
    if exporter_declaration["available"]:
        wpa_path = resolve_declared_file(exporter_declaration["path"], "WPAExporter")
        live_wpa_binding = declaration_tool.versioned_tool_binding(wpa_path)
        require(live_wpa_binding == exporter_declaration["binding"],
                "live WPAExporter identity/version differs from the declaration")
    else:
        wpa_path = None
        live_wpa_binding = None
    profile_declaration = declaration["tooling"]["wpaProfile"]
    if profile_declaration["available"]:
        wpa_profile_path = resolve_declared_file(
            profile_declaration["path"], "WPA profile"
        )
        live_wpa_profile_binding = etw.stable_binding(
            wpa_profile_path, "WPA profile"
        )
        require(live_wpa_profile_binding == profile_declaration["binding"],
                "live WPA profile identity differs from the declaration")
    else:
        wpa_profile_path = None
        live_wpa_profile_binding = None
    capture_tooling_ready = (
        profiles["profilesExitCode"] == 0
        and profiles["cpuProfileAvailable"] is True
        and profiles["gpuProfileAvailable"] is True
        and all(profile_details[role]["ready"] is True for role in ("CPU", "GPU"))
        and status["idle"] is True
        and status["idle"] is (
            status["exitCode"] == 0 and status["idleTokenObserved"] is True
        )
    )
    export_tooling_ready = (
        wpa_path is not None and live_wpa_binding is not None
        and wpa_profile_path is not None and live_wpa_profile_binding is not None
    )
    capture_allowed = capture_tooling_ready and export_tooling_ready
    return {
        "policy": policy,
        "policyBinding": policy_binding,
        "declaration": declaration,
        "declarationPath": resolved_declaration,
        "declarationBinding": etw.stable_binding(
            resolved_declaration, "ETW v3 declaration"
        ),
        "archive": archive,
        "receipt": receipt,
        "performanceRunRoot": performance_run_root,
        "external": external,
        "runRoot": run_destination(declaration, external),
        "wpr": wpr_path,
        "wprBinding": live_wpr_binding,
        "profiles": profiles,
        "profileDetails": profile_details,
        "status": status,
        "wpaExporter": wpa_path,
        "wpaExporterBinding": live_wpa_binding,
        "wpaProfile": wpa_profile_path,
        "wpaProfileBinding": live_wpa_profile_binding,
        "tracerpt": tracerpt_path,
        "tracerptBinding": live_tracerpt_binding,
        "captureToolingReady": capture_tooling_ready,
        "exportToolingReady": export_tooling_ready,
        "captureAllowed": capture_allowed,
    }


def preflight_result(context: dict[str, Any]) -> dict[str, Any]:
    declaration = context["declaration"]
    blockers: list[str] = []
    if not context["captureToolingReady"]:
        if not context["status"]["idle"]:
            blockers.append("WPR is not idle; a preexisting recording may not be disturbed")
        if not context["profiles"]["cpuProfileAvailable"]:
            blockers.append("WPR CPU profile is unavailable")
        if not context["profiles"]["gpuProfileAvailable"]:
            blockers.append("WPR GPU profile is unavailable")
        for role in ("CPU", "GPU"):
            if not context["profileDetails"][role]["ready"]:
                blockers.append(f"WPR {role}.light profile details did not validate")
    if not context["exportToolingReady"]:
        blockers.append(
            "WPAExporter and an exact .wpaProfile are unavailable or not declaration-bound"
        )
    blockers.extend([
        "no real WPR CPU+GPU ETL has been captured for this declaration",
        "no tracerpt provider proof binds the ETL to trusted Windows parsing",
        "no actual WPAExporter invocation record binds the raw outputs",
        "no strict raw CPU/GPU exports have been validated",
        "no normalized GPU/GameThread/RenderThread/RHIThread export has been validated",
    ])
    return {
        "schema": PREFLIGHT_SCHEMA,
        "schemaVersion": 3,
        "session": 19,
        "candidateId": declaration["candidateId"],
        "profileRunId": declaration["profileRunId"],
        "state": (
            "CAPTURE_READY_NO_CAPTURE_STARTED"
            if context["captureAllowed"]
            else "CAPTURE_READY_EXPORT_BLOCKED_WPAEXPORTER_MISSING"
            if context["captureToolingReady"]
            else "CAPTURE_BLOCKED_WPR_PROFILE_OR_IDLE_CHECK_FAILED"
        ),
        "captureToolingReady": context["captureToolingReady"],
        "exportToolingReady": context["exportToolingReady"],
        "captureAllowed": context["captureAllowed"],
        "wprStarted": False,
        "gameLaunched": False,
        "captureExecuted": False,
        "filesWritten": False,
        "blockers": blockers,
        "claimBoundary": {
            "preflightOnly": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def archive_snapshot(
    context: dict[str, Any], phase: str, captured_utc: str
) -> dict[str, Any]:
    identity = etw.archive_identity(
        context["archive"], context["declaration"]["candidateId"]
    )
    require(identity == {
        key: context["policy"]["candidateArchive"][key]
        for key in ("fileCount", "bytes", "inventorySha256", "launcher",
                    "shippingExecutable")
    }, f"{phase} archive identity differs from policy")
    return {
        "schema": context["policy"]["schemas"]["archiveSnapshot"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": context["declaration"]["candidateId"],
        "profileRunId": context["declaration"]["profileRunId"],
        "phase": phase,
        "capturedUtc": captured_utc,
        "archive": identity,
        "claimBoundary": {
            "archiveSnapshotOnly": True,
            "technicalEvidenceAccepted": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def validate_archive_snapshot(
    value: dict[str, Any], context: dict[str, Any], phase: str
) -> datetime:
    etw.require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "phase", "capturedUtc", "archive", "claimBoundary",
    }, f"{phase} archive snapshot")
    declaration = context["declaration"]
    require(value["schema"] == context["policy"]["schemas"]["archiveSnapshot"]
            and value["schemaVersion"] == 3 and value["session"] == 19
            and value["candidateId"] == declaration["candidateId"]
            and value["profileRunId"] == declaration["profileRunId"]
            and value["phase"] == phase,
            f"{phase} archive snapshot identity differs")
    captured = parse_utc(value["capturedUtc"], f"{phase} capturedUtc")
    require(value["archive"] == {
        key: context["policy"]["candidateArchive"][key]
        for key in ("fileCount", "bytes", "inventorySha256", "launcher",
                    "shippingExecutable")
    }, f"{phase} archive snapshot binding differs")
    require(value["claimBoundary"] == {
        "archiveSnapshotOnly": True,
        "technicalEvidenceAccepted": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, f"{phase} archive snapshot claim boundary differs")
    return captured


def capture_record(
    context: dict[str, Any], *, start_invocation: dict[str, Any],
    stop_invocation: dict[str, Any], game_process: dict[str, Any],
    archive_pre_binding: dict[str, Any], archive_post_binding: dict[str, Any],
    etl_envelope: dict[str, Any], provider_proof_binding: dict[str, Any],
) -> dict[str, Any]:
    declaration = context["declaration"]
    return {
        "schema": context["policy"]["schemas"]["captureRecord"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": declaration["candidateId"],
        "profileRunId": declaration["profileRunId"],
        "state": "CAPTURE_COMPLETE_PROVIDER_PROOF_BOUND",
        "declaration": context["declarationBinding"],
        "requestedDurationSeconds": declaration["requestedDurationSeconds"],
        "wpr": context["wprBinding"],
        "wprInvocations": {"start": start_invocation, "stop": stop_invocation},
        "gameProcess": game_process,
        "archivePre": archive_pre_binding,
        "archivePost": archive_post_binding,
        "etlEnvelope": etl_envelope,
        "providerProof": provider_proof_binding,
        "claimBoundary": {
            "captureArtifactOnly": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def validate_capture_record(
    value: dict[str, Any], context: dict[str, Any],
    *, archive_pre_binding: dict[str, Any],
    archive_post_binding: dict[str, Any], etl_binding: dict[str, Any],
) -> dict[str, datetime]:
    etw.require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "state", "declaration", "requestedDurationSeconds", "wpr",
        "wprInvocations", "gameProcess", "archivePre", "archivePost",
        "etlEnvelope", "providerProof", "claimBoundary",
    }, "ETW capture record")
    declaration = context["declaration"]
    policy = context["policy"]
    require(value["schema"] == policy["schemas"]["captureRecord"]
            and value["schemaVersion"] == 3 and value["session"] == 19
            and value["candidateId"] == declaration["candidateId"]
            and value["profileRunId"] == declaration["profileRunId"]
            and value["state"] == "CAPTURE_COMPLETE_PROVIDER_PROOF_BOUND",
            "ETW capture record identity/state differs")
    require(value["declaration"] == context["declarationBinding"]
            and value["requestedDurationSeconds"] ==
            declaration["requestedDurationSeconds"]
            and value["wpr"] == context["wprBinding"],
            "ETW capture record immutable binding differs")
    expected_arguments = fixed_game_arguments(context["runRoot"] / "UserData")
    game = value["gameProcess"]
    require(game["arguments"] == expected_arguments,
            "ETW capture Shipping arguments differ")
    wpr_working = context["runRoot"] / "WprWorking"
    etl_path = context["runRoot"] / policy["requiredArtifacts"]["etl"]
    expected_start = [
        str(context["wpr"]), "-start", "CPU.light", "-start", "GPU.light",
        "-filemode", "-recordtempto", str(wpr_working),
    ]
    expected_stop = [str(context["wpr"]), "-stop", str(etl_path)]
    start_started, start_finished = etw.validate_invocation(
        value["wprInvocations"]["start"], expected_arguments=expected_start,
        label="self-test WPR start invocation",
    )
    stop_started, stop_finished = etw.validate_invocation(
        value["wprInvocations"]["stop"], expected_arguments=expected_stop,
        label="self-test WPR stop invocation",
    )
    require(value["archivePre"] == archive_pre_binding
            and value["archivePost"] == archive_post_binding
            and value["etlEnvelope"] == etl_binding
            and isinstance(value["providerProof"], dict),
            "ETW capture artifact bindings differ")
    require(type(game["processId"]) is int and game["processId"] > 0
            and type(game["exitCode"]) is int
            and type(game["forcedTermination"]) is bool,
            "ETW capture process result differs")
    game_created = parse_utc(game["creationUtc"], "game creationUtc")
    game_finished = parse_utc(game["finishedUtc"], "game finishedUtc")
    declared = parse_utc(declaration["declaredUtc"], "declaration declaredUtc")
    require(declared <= start_started <= start_finished <= game_created
            < game_finished <= stop_started <= stop_finished,
            "ETW capture chronology differs")
    observed = (game_finished - game_created).total_seconds()
    require(observed + 2.0 >= declaration["requestedDurationSeconds"],
            "ETW game capture duration is shorter than declared")
    require(value["claimBoundary"] == {
        "captureArtifactOnly": True,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "ETW capture claim boundary differs")
    return {
        "wprStartedUtc": start_started,
        "gameStartedUtc": game_created,
        "gameFinishedUtc": game_finished,
        "wprStoppedUtc": stop_finished,
    }


def create_provider_proof(
    context: dict[str, Any], *, etl_path: Path,
    etl_envelope: dict[str, Any], game_process: dict[str, Any],
    wpr_stopped_utc: datetime,
) -> tuple[dict[str, Any], dict[str, Any]]:
    policy = context["policy"]
    artifacts = policy["requiredArtifacts"]
    run_root = context["runRoot"]
    csv_path = run_root / artifacts["tracerptCsv"]
    summary_path = run_root / artifacts["tracerptSummary"]
    proof_path = run_root / artifacts["providerProof"]
    require(not any(path.exists() for path in (csv_path, summary_path, proof_path)),
            "refusing to overwrite tracerpt provider-proof artifacts")
    arguments = [
        str(context["tracerpt"]), str(etl_path), "-o", str(csv_path),
        "-of", "CSV", "-summary", str(summary_path), "-y",
    ]
    started = utc_now()
    exit_code, _, _ = run_command(arguments, 600)
    finished = utc_now()
    require(exit_code == 0, "tracerpt failed to parse the captured ETL")
    contract = policy["tracerptContract"]
    csv_binding, observed, pid_observed, parsed_rows = etw.validate_tracerpt_csv(
        csv_path, contract=contract, game_pid=game_process["processId"],
    )
    summary_binding, _, _ = etw.scan_regular_file_tokens(
        summary_path, required_tokens=[],
        required_pid=game_process["processId"],
        minimum_bytes=contract["minimumSummaryBytes"],
        label="tracerpt summary",
    )
    proof = {
        "schema": policy["schemas"]["providerProof"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": context["declaration"]["candidateId"],
        "profileRunId": context["declaration"]["profileRunId"],
        "state": "PASS_TRACERPT_PARSED_ETL_PROVIDER_EVIDENCE",
        "generatedUtc": utc_now(),
        "tracerpt": {
            "path": str(context["tracerpt"]),
            "binding": context["tracerptBinding"],
        },
        "invocation": {
            "arguments": arguments, "startedUtc": started,
            "finishedUtc": finished, "exitCode": exit_code,
        },
        "inputEtl": etl_envelope,
        "outputs": {"csv": csv_binding, "summary": summary_binding},
        "gameProcess": {
            "processId": game_process["processId"],
            "executableSha256": game_process["executable"]["sha256"],
        },
        "providerTokensObserved": observed,
        "gamePidObserved": True,
        "parsedEventRows": parsed_rows,
        "recordedBy": policy["bindings"]["runner"],
        "claimBoundary": {
            "trustedWindowsEtwParserEvidence": True,
            "byteEnvelopeOnly": False,
            "realEtlValidated": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    shipping.write_exclusive_json(proof_path, proof)
    validated, proof_binding = etw.validate_provider_proof(
        proof_path, policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"], etl_path=etl_path,
        etl_envelope=etl_envelope, tracerpt_path=context["tracerpt"],
        tracerpt_binding=context["tracerptBinding"],
        tracerpt_csv_path=csv_path, tracerpt_summary_path=summary_path,
        game_pid=game_process["processId"],
        game_executable_sha256=game_process["executable"]["sha256"],
        wpr_stopped_utc=wpr_stopped_utc,
    )
    return validated, proof_binding


def create_capture(context: dict[str, Any], confirm_candidate: str,
                   confirm_run: str) -> dict[str, Any]:
    declaration = context["declaration"]
    require(confirm_candidate == declaration["candidateId"],
            "--confirm-candidate-id does not match the declaration")
    require(confirm_run == declaration["profileRunId"],
            "--confirm-profile-run-id does not match the declaration")
    require(declaration["capability"]["captureAllowed"] is True
            and context["captureAllowed"] is True,
            "capture is forbidden: WPR CPU/GPU and exact WPAExporter tooling "
            "must both be predeclared and live")
    require(context["status"]["idle"] is True,
            "WPR is not idle; refusing to disturb a preexisting recording")
    run_root = context["runRoot"]
    require(not os.path.lexists(run_root),
            "refusing to overwrite/reuse an existing ETW run root")
    run_root.parent.mkdir(parents=True, exist_ok=True)
    require(not shipping.is_reparse_point(run_root.parent),
            "ETW run parent may not be a reparse point")
    require(declaration_tool.validate_destination_chain(
        context["external"],
        declaration["externalLayout"]["runRelativePath"],
    ) == run_root, "ETW run destination changed while prepared")
    run_root.mkdir(exist_ok=False)
    user_dir = run_root / "UserData"
    wpr_working = run_root / "WprWorking"
    user_dir.mkdir()
    wpr_working.mkdir()
    policy = context["policy"]
    artifacts = policy["requiredArtifacts"]
    pre_path = run_root / artifacts["archivePre"]
    post_path = run_root / artifacts["archivePost"]
    etl_path = run_root / artifacts["etl"]
    capture_path = run_root / artifacts["captureRecord"]
    failure_path = run_root / "CaptureFailure.json"
    start_command = [
        str(context["wpr"]), "-start", "CPU.light", "-start", "GPU.light",
        "-filemode", "-recordtempto", str(wpr_working),
    ]
    stop_command = [str(context["wpr"]), "-stop", str(etl_path)]
    cancel_command = [str(context["wpr"]), "-cancel"]
    arguments = fixed_game_arguments(user_dir)
    shipping.write_exclusive_json(
        pre_path, archive_snapshot(context, "PRE", utc_now())
    )
    wpr_owned = False
    game: Optional[subprocess.Popen[Any]] = None
    try:
        immediate_status = declaration_tool.wpr_status(
            context["wpr"], policy["wprContract"]["requiredIdleStatusToken"]
        )
        require(immediate_status["idle"] is True
                and immediate_status["exitCode"] == 0
                and immediate_status["idleTokenObserved"] is True,
                "WPR stopped being idle before capture start")
        start_invocation_started = utc_now()
        start_code, _, _ = run_command(start_command, 30)
        start_invocation_finished = utc_now()
        require(start_code == 0, "WPR CPU/GPU start failed")
        wpr_owned = True
        executable = context["archive"] / shipping.EXECUTABLE_RELATIVE
        game = subprocess.Popen(
            [str(executable), *arguments], cwd=str(context["archive"]),
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        game_started_utc, observed_process_image = windows_process_facts(game)
        require(observed_process_image == executable.resolve(strict=True),
                "launched process image differs from the bound Shipping executable")
        game_pid = game.pid
        deadline = time.monotonic() + declaration["requestedDurationSeconds"]
        while time.monotonic() < deadline:
            exit_code = game.poll()
            require(exit_code is None,
                    f"Shipping game exited early with code {exit_code}")
            time.sleep(min(1.0, max(0.05, deadline - time.monotonic())))
        game.terminate()
        forced = True
        try:
            game_exit_code = game.wait(timeout=20)
        except subprocess.TimeoutExpired:
            game.kill()
            game_exit_code = game.wait(timeout=10)
        game_finished_utc = windows_process_exit_utc(game)
        stop_invocation_started = utc_now()
        stop_code, _, _ = run_command(stop_command, 120)
        stop_invocation_finished = utc_now()
        require(stop_code == 0, "WPR stop/ETL flush failed")
        wpr_owned = False
        etl_binding = etw.validate_etl_envelope(etl_path, policy["etlContract"])
        require(etl_binding["realEtlValidated"] is False,
                "ETL byte envelope may not independently authorize evidence")
        game_process = {
            "processId": game_pid,
            "creationUtc": game_started_utc,
            "finishedUtc": game_finished_utc,
            "exitCode": game_exit_code,
            "forcedTermination": forced,
            "executable": {
                "path": str(executable),
                **policy["candidateArchive"]["shippingExecutable"],
            },
            "arguments": arguments,
        }
        _, provider_binding = create_provider_proof(
            context, etl_path=etl_path, etl_envelope=etl_binding,
            game_process=game_process,
            wpr_stopped_utc=parse_utc(
                stop_invocation_finished, "WPR stop finishedUtc"
            ),
        )
        shipping.write_exclusive_json(
            post_path, archive_snapshot(context, "POST", utc_now())
        )
        pre_binding = etw.stable_binding(pre_path, "PRE archive snapshot")
        post_binding = etw.stable_binding(post_path, "POST archive snapshot")
        record = capture_record(
            context,
            start_invocation={
                "arguments": start_command,
                "startedUtc": start_invocation_started,
                "finishedUtc": start_invocation_finished,
                "exitCode": start_code,
            },
            stop_invocation={
                "arguments": stop_command,
                "startedUtc": stop_invocation_started,
                "finishedUtc": stop_invocation_finished,
                "exitCode": stop_code,
            },
            game_process=game_process,
            archive_pre_binding=pre_binding, archive_post_binding=post_binding,
            etl_envelope=etl_binding, provider_proof_binding=provider_binding,
        )
        shipping.write_exclusive_json(capture_path, record)
        return {
            "state": "CAPTURE_COMPLETE_PROVIDER_PROOF_BOUND_EXPORT_REQUIRED",
            "runRoot": str(run_root),
            "captureRecord": etw.stable_binding(capture_path, "ETW capture record"),
            "etlEnvelope": etl_binding,
            "providerProof": provider_binding,
            "technicalEvidenceAccepted": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        }
    except BaseException as exc:
        if game is not None and game.poll() is None:
            game.terminate()
            try:
                game.wait(timeout=10)
            except subprocess.TimeoutExpired:
                game.kill()
                game.wait(timeout=10)
        if wpr_owned:
            run_command(cancel_command, 30)
        failure = {
            "schema": FAILURE_SCHEMA,
            "schemaVersion": 3,
            "session": 19,
            "candidateId": declaration["candidateId"],
            "profileRunId": declaration["profileRunId"],
            "failedUtc": utc_now(),
            "errorType": exc.__class__.__name__,
            "error": str(exc),
            "ownedWprCancelled": wpr_owned,
            "technicalEvidenceAccepted": False,
            "releaseReady": False,
        }
        try:
            shipping.write_exclusive_json(failure_path, failure)
        except (shipping.EvidenceError, OSError):
            pass
        raise


def create_wpa_export(context: dict[str, Any]) -> dict[str, Any]:
    require(context["exportToolingReady"] is True,
            "WPA export requires exact declaration-bound exporter and profile")
    policy = context["policy"]
    run_root = context["runRoot"].resolve(strict=True)
    artifacts = policy["requiredArtifacts"]
    paths = {key: run_root / name for key, name in artifacts.items()}
    for key in ("captureRecord", "etl", "providerProof", "tracerptCsv",
                "tracerptSummary"):
        require(paths[key].is_file() and not shipping.is_reparse_point(paths[key]),
                f"WPA export prerequisite is missing: {artifacts[key]}")
    for key in ("rawCpu", "rawGpu", "wpaInvocation"):
        require(not paths[key].exists(),
                f"refusing to overwrite WPA export artifact: {artifacts[key]}")
    capture = etw.load_json(paths["captureRecord"], "ETW capture record")
    game = capture["gameProcess"]
    etl_envelope = etw.validate_etl_envelope(paths["etl"], policy["etlContract"])
    pre_binding = etw.stable_binding(paths["archivePre"], "PRE archive snapshot")
    post_binding = etw.stable_binding(paths["archivePost"], "POST archive snapshot")
    capture_times = validate_capture_record(
        capture, context, archive_pre_binding=pre_binding,
        archive_post_binding=post_binding, etl_binding=etl_envelope,
    )
    provider, provider_binding = etw.validate_provider_proof(
        paths["providerProof"], policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"], etl_path=paths["etl"],
        etl_envelope=etl_envelope, tracerpt_path=context["tracerpt"],
        tracerpt_binding=context["tracerptBinding"],
        tracerpt_csv_path=paths["tracerptCsv"],
        tracerpt_summary_path=paths["tracerptSummary"],
        game_pid=game["processId"],
        game_executable_sha256=game["executable"]["sha256"],
        wpr_stopped_utc=capture_times["wprStoppedUtc"],
    )
    require(capture["providerProof"] == provider_binding,
            "capture record provider-proof binding differs")
    provider_generated = parse_utc(provider["generatedUtc"], "provider generatedUtc")
    before_files = {
        item.resolve() for item in run_root.iterdir() if item.is_file()
    }
    arguments = [
        str(context["wpaExporter"]), "-i", str(paths["etl"]), "-profile",
        str(context["wpaProfile"]), "-delimiter", ",", "-outputfolder",
        str(run_root),
    ]
    started = utc_now()
    exit_code, _, _ = run_command(arguments, 1800)
    finished = utc_now()
    require(exit_code == 0, "WPAExporter invocation failed")
    after_files = {
        item.resolve() for item in run_root.iterdir() if item.is_file()
    }
    created = after_files - before_files
    require(created == {paths["rawCpu"].resolve(), paths["rawGpu"].resolve()},
            "WPA profile must create exactly RawCpuTiming.csv and RawGpuTiming.csv")
    export = policy["exportContract"]
    cpu_binding, _ = etw.validate_raw_csv(
        paths["rawCpu"], export["requiredRawCpuColumns"],
        export["minimumRawArtifactBytes"], export["maximumRawArtifactBytes"],
        "raw CPU export",
    )
    gpu_binding, _ = etw.validate_raw_csv(
        paths["rawGpu"], export["requiredRawGpuColumns"],
        export["minimumRawArtifactBytes"], export["maximumRawArtifactBytes"],
        "raw GPU export",
    )
    game_binding = {
        "processId": game["processId"], "creationUtc": game["creationUtc"],
        "finishedUtc": game["finishedUtc"],
        "executableSha256": game["executable"]["sha256"],
    }
    record = {
        "schema": policy["schemas"]["wpaInvocation"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": context["declaration"]["candidateId"],
        "profileRunId": context["declaration"]["profileRunId"],
        "state": "PASS_WPAEXPORTER_INVOCATION_RAW_OUTPUTS_BOUND",
        "generatedUtc": utc_now(),
        "tool": {
            "path": str(context["wpaExporter"]),
            "binding": context["wpaExporterBinding"],
        },
        "profile": {
            "path": str(context["wpaProfile"]),
            "binding": context["wpaProfileBinding"],
        },
        "inputEtl": etl_envelope,
        "invocation": {
            "arguments": arguments, "startedUtc": started,
            "finishedUtc": finished, "exitCode": exit_code,
        },
        "outputDirectory": str(run_root),
        "outputInventory": [cpu_binding, gpu_binding],
        "rawArtifacts": {"cpu": cpu_binding, "gpu": gpu_binding},
        "gameProcess": game_binding,
        "recordedBy": policy["bindings"]["runner"],
        "claimBoundary": {
            "actualExporterInvocationBound": True,
            "csvOnlyProvenanceSufficient": False,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    shipping.write_exclusive_json(paths["wpaInvocation"], record)
    etw.validate_wpa_invocation_record(
        paths["wpaInvocation"], policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"],
        etl_path=paths["etl"], etl_envelope=etl_envelope,
        wpa_path=context["wpaExporter"],
        wpa_binding=context["wpaExporterBinding"],
        wpa_profile_path=context["wpaProfile"],
        wpa_profile_binding=context["wpaProfileBinding"],
        output_directory=run_root, raw_cpu_binding=cpu_binding,
        raw_gpu_binding=gpu_binding, game_process=game_binding,
        provider_generated_utc=provider_generated,
    )
    return {
        "state": "WPAEXPORTER_RAW_OUTPUTS_BOUND_NORMALIZATION_REQUIRED",
        "wpaInvocation": etw.stable_binding(
            paths["wpaInvocation"], "WPA invocation record"
        ),
        "technicalEvidenceAccepted": False,
        "releaseReady": False,
    }


def validate_and_finalize(context: dict[str, Any]) -> dict[str, Any]:
    require(context["declaration"]["capability"]["captureAllowed"] is True,
            "finalization requires a declaration that authorized capture")
    require(context["exportToolingReady"] is True,
            "finalization requires the exact declaration-bound WPAExporter")
    run_root = context["runRoot"]
    try:
        resolved_run = run_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise RunnerError(f"ETW run root is missing ({exc.__class__.__name__})") from None
    require(resolved_run.is_dir() and not shipping.is_reparse_point(resolved_run),
            "ETW run root must be a regular non-reparse directory")
    policy = context["policy"]
    artifacts = policy["requiredArtifacts"]
    paths = {key: resolved_run / name for key, name in artifacts.items()}
    require(not paths["exportRecord"].exists() and not paths["runManifest"].exists(),
            "refusing to overwrite an existing ETW export record/manifest")
    for key in ("archivePre", "archivePost", "captureRecord", "etl",
                "tracerptCsv", "tracerptSummary", "providerProof",
                "wpaInvocation", "rawCpu", "rawGpu", "normalizedRoles"):
        require(paths[key].is_file() and not shipping.is_reparse_point(paths[key]),
                f"required ETW artifact is missing or invalid: {artifacts[key]}")
    pre_binding = etw.stable_binding(paths["archivePre"], "PRE archive snapshot")
    post_binding = etw.stable_binding(paths["archivePost"], "POST archive snapshot")
    pre_value = etw.load_json(paths["archivePre"], "PRE archive snapshot")
    post_value = etw.load_json(paths["archivePost"], "POST archive snapshot")
    pre_time = validate_archive_snapshot(pre_value, context, "PRE")
    post_time = validate_archive_snapshot(post_value, context, "POST")
    etl_binding = etw.validate_etl_envelope(paths["etl"], policy["etlContract"])
    require(etl_binding["realEtlValidated"] is False,
            "ETL envelope alone may never establish real ETL status")
    capture_value = etw.load_json(paths["captureRecord"], "ETW capture record")
    capture_times = validate_capture_record(
        capture_value, context, archive_pre_binding=pre_binding,
        archive_post_binding=post_binding, etl_binding=etl_binding,
    )
    require(pre_time <= capture_times["wprStartedUtc"]
            and post_time >= capture_times["gameFinishedUtc"],
            "archive snapshot chronology differs from capture")
    game = capture_value["gameProcess"]
    provider, provider_binding = etw.validate_provider_proof(
        paths["providerProof"], policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"], etl_path=paths["etl"],
        etl_envelope=etl_binding, tracerpt_path=context["tracerpt"],
        tracerpt_binding=context["tracerptBinding"],
        tracerpt_csv_path=paths["tracerptCsv"],
        tracerpt_summary_path=paths["tracerptSummary"],
        game_pid=game["processId"],
        game_executable_sha256=game["executable"]["sha256"],
        wpr_stopped_utc=capture_times["wprStoppedUtc"],
    )
    require(capture_value["providerProof"] == provider_binding,
            "capture record provider-proof binding differs")
    export = policy["exportContract"]
    raw_cpu_binding, cpu_rows = etw.validate_raw_csv(
        paths["rawCpu"], export["requiredRawCpuColumns"],
        export["minimumRawArtifactBytes"], export["maximumRawArtifactBytes"],
        "raw CPU export",
    )
    raw_gpu_binding, gpu_rows = etw.validate_raw_csv(
        paths["rawGpu"], export["requiredRawGpuColumns"],
        export["minimumRawArtifactBytes"], export["maximumRawArtifactBytes"],
        "raw GPU export",
    )
    process, derived_roles = etw.derive_role_metrics(
        cpu_rows, gpu_rows, export,
        required_duration_seconds=context["declaration"]["requestedDurationSeconds"],
    )
    require(process["processId"] == game["processId"],
            "raw CPU/GPU PID differs from captured game PID")
    game_binding = {
        "processId": game["processId"], "creationUtc": game["creationUtc"],
        "finishedUtc": game["finishedUtc"],
        "executableSha256": game["executable"]["sha256"],
    }
    _, wpa_invocation_binding, wpa_generated = etw.validate_wpa_invocation_record(
        paths["wpaInvocation"], policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"],
        etl_path=paths["etl"], etl_envelope=etl_binding,
        wpa_path=context["wpaExporter"],
        wpa_binding=context["wpaExporterBinding"],
        wpa_profile_path=context["wpaProfile"],
        wpa_profile_binding=context["wpaProfileBinding"],
        output_directory=resolved_run, raw_cpu_binding=raw_cpu_binding,
        raw_gpu_binding=raw_gpu_binding, game_process=game_binding,
        provider_generated_utc=parse_utc(
            provider["generatedUtc"], "provider generatedUtc"
        ),
    )
    normalized, roles = etw.validate_normalized_export(
        paths["normalizedRoles"], policy=policy,
        candidate_id=context["declaration"]["candidateId"],
        run_id=context["declaration"]["profileRunId"],
        etl_binding=etl_binding, raw_cpu_binding=raw_cpu_binding,
        raw_gpu_binding=raw_gpu_binding,
        expected_exporter_binding=context["wpaExporterBinding"],
        derived_roles=derived_roles,
    )
    normalized_time = parse_utc(normalized["generatedUtc"],
                                "normalized export generatedUtc")
    require(normalized_time >= wpa_generated,
            "normalized export predates actual WPAExporter invocation")
    require(normalized_time <= datetime.now(timezone.utc) + timedelta(seconds=2),
            "normalized export timestamp is implausibly in the future")
    normalized_binding = etw.stable_binding(
        paths["normalizedRoles"], "normalized ETW role export"
    )
    export_record = {
        "schema": policy["schemas"]["exportRecord"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": context["declaration"]["candidateId"],
        "profileRunId": context["declaration"]["profileRunId"],
        "state": "PASS_ACTUAL_WPA_INVOCATION_AND_NORMALIZED_FOUR_ROLE_EXPORT_BOUND",
        "validatedUtc": utc_now(),
        "providerProof": provider_binding,
        "wpaInvocation": wpa_invocation_binding,
        "etlEnvelope": etl_binding,
        "rawArtifacts": {"cpu": raw_cpu_binding, "gpu": raw_gpu_binding},
        "normalizedRoles": normalized_binding,
        "gameProcess": game_binding,
        "claimBoundary": {
            "actualExporterInvocationBound": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    shipping.write_exclusive_json(paths["exportRecord"], export_record)
    export_record_binding = etw.stable_binding(
        paths["exportRecord"], "ETW export record"
    )
    manifest = {
        "schema": policy["schemas"]["runManifest"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": context["declaration"]["candidateId"],
        "profileRunId": context["declaration"]["profileRunId"],
        "state": policy["technicalPassState"],
        "generatedUtc": utc_now(),
        "declaration": context["declarationBinding"],
        "candidateArchive": copy.deepcopy(policy["candidateArchive"]),
        "threeHolePerformanceReceipt": copy.deepcopy(
            policy["bindings"]["shippingPerformanceReceipt"]
        ),
        "artifacts": {
            "archivePre": pre_binding,
            "archivePost": post_binding,
            "captureRecord": etw.stable_binding(
                paths["captureRecord"], "ETW capture record"
            ),
            "etlEnvelope": etl_binding,
            "tracerptCsv": etw.stable_binding(paths["tracerptCsv"], "tracerpt CSV"),
            "tracerptSummary": etw.stable_binding(
                paths["tracerptSummary"], "tracerpt summary"
            ),
            "providerProof": provider_binding,
            "wpaInvocation": wpa_invocation_binding,
            "rawCpu": raw_cpu_binding,
            "rawGpu": raw_gpu_binding,
            "normalizedRoles": normalized_binding,
            "exportRecord": export_record_binding,
        },
        "process": process,
        "roles": roles,
        "claimBoundary": {
            "candidateBoundExternalEtwTechnicalEvidence": True,
            "trustedTracerptProviderProofBound": True,
            "actualWpaExporterInvocationBound": True,
            "technicalEvidenceAccepted": True,
            "sustainedPerformanceCertified": False,
            "cryptographicProcessAttestation": False,
            "hostileSameUserForgeryResistance": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    shipping.write_exclusive_json(paths["runManifest"], manifest)
    return etw.validate_completed_run(
        declaration_path=context["declarationPath"], run_root=resolved_run,
        archive=context["archive"], external_root=context["external"],
    )


def run_self_test() -> tuple[int, dict[str, Any]]:
    checks = 0
    failures: list[dict[str, str]] = []

    def check(name: str, condition: bool) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            failures.append({"case": name, "error": "assertion failed"})

    try:
        policy = etw.load_json(POLICY_PATH, "self-test ETW policy")
        etw.validate_policy(policy)
        arguments = fixed_game_arguments(Path("C:/external/run/UserData"))
        check("fixed-argument-count", len(arguments) == len(FIXED_GAME_ARGUMENTS) + 1)
        check("no-unreal-trace-switch", not any(
            argument.casefold().startswith("-trace") for argument in arguments
        ))
        check("no-log-switch", not any(
            argument.casefold().startswith(("-log", "-abslog"))
            for argument in arguments
        ))
        blocked_context = {
            "declaration": {
                "candidateId": etw.CANDIDATE_ID,
                "profileRunId": "11111111-2222-4333-8444-555555555555",
            },
            "captureToolingReady": True,
            "exportToolingReady": False,
            "captureAllowed": False,
            "status": {"idle": True},
            "profiles": {
                "profilesExitCode": 0, "cpuProfileAvailable": True,
                "gpuProfileAvailable": True,
            },
            "profileDetails": {
                role: {"exitCode": 0, "requiredTokensObserved": True, "ready": True}
                for role in ("CPU", "GPU")
            },
        }
        result = preflight_result(blocked_context)
        check("export-blocked-state", result["state"] ==
              "CAPTURE_READY_EXPORT_BLOCKED_WPAEXPORTER_MISSING")
        check("blocked-no-wpr", result["wprStarted"] is False)
        check("blocked-no-game", result["gameLaunched"] is False)
        check("blocked-no-write", result["filesWritten"] is False)
        check("blocked-no-technical-claim",
              result["claimBoundary"]["technicalEvidenceAccepted"] is False)
        check("policy-four-roles", policy["exportContract"]["requiredRoles"] ==
              ["GPU", "GameThread", "RenderThread", "RHIThread"])
        check("policy-export-before-capture",
              policy["predeclaration"]["captureForbiddenWhileExportToolingIsBlocked"]
              is True)
        check("policy-explicit-capture",
              policy["predeclaration"][
                  "captureStartsOnlyWithExplicitCaptureModeAndCandidateConfirmation"
              ] is True)
        check("technical-pass-still-human-pending",
              policy["technicalPassState"].endswith("HUMAN_APPROVALS_PENDING"))
        fake_context = {
            "policy": policy,
            "declaration": {
                "candidateId": etw.CANDIDATE_ID,
                "profileRunId": "11111111-2222-4333-8444-555555555555",
                "requestedDurationSeconds": 900,
                "declaredUtc": "2026-01-01T00:00:00.000Z",
            },
            "declarationBinding": {
                "fileName": "Declaration.json", "bytes": 1, "sha256": "A" * 64,
            },
            "wpr": Path("C:/Windows/wpr.exe"),
            "wprBinding": {
                "fileName": "wpr.exe", "bytes": 1, "sha256": "B" * 64,
                "version": "1.0",
            },
            "runRoot": Path("C:/external/run"),
        }
        fake_pre = {"fileName": "ArchiveSnapshot.Pre.json", "bytes": 1,
                    "sha256": "C" * 64}
        fake_post = {"fileName": "ArchiveSnapshot.Post.json", "bytes": 1,
                     "sha256": "D" * 64}
        fake_etl = {"fileName": "SustainedCpuGpu.etl", "bytes": 1048576,
                    "sha256": "E" * 64, "distinctByteValues": 256,
                    "nonZeroRatio": 0.99,
                    "validationScope": "BYTE_ENVELOPE_ONLY_NOT_ETL_AUTHENTICITY",
                    "realEtlValidated": False,
                    "trustedProviderProofRequired": True}
        fake_arguments = fixed_game_arguments(fake_context["runRoot"] / "UserData")
        fake_start = [
            str(fake_context["wpr"]), "-start", "CPU.light", "-start", "GPU.light",
            "-filemode", "-recordtempto", str(fake_context["runRoot"] / "WprWorking"),
        ]
        fake_stop = [
            str(fake_context["wpr"]), "-stop",
            str(fake_context["runRoot"] / policy["requiredArtifacts"]["etl"]),
        ]
        record = capture_record(
            fake_context,
            start_invocation={
                "arguments": fake_start,
                "startedUtc": "2026-01-02T00:00:00.000Z",
                "finishedUtc": "2026-01-02T00:00:00.500Z", "exitCode": 0,
            },
            stop_invocation={
                "arguments": fake_stop,
                "startedUtc": "2026-01-02T00:15:01.500Z",
                "finishedUtc": "2026-01-02T00:15:02.000Z", "exitCode": 0,
            },
            game_process={
                "processId": 42,
                "creationUtc": "2026-01-02T00:00:01.000Z",
                "finishedUtc": "2026-01-02T00:15:01.000Z",
                "exitCode": 0, "forcedTermination": True,
                "executable": {
                    "path": "C:/sealed/DiscGolfTour-Win64-Shipping.exe",
                    **policy["candidateArchive"]["shippingExecutable"],
                },
                "arguments": fake_arguments,
            },
            archive_pre_binding=fake_pre, archive_post_binding=fake_post,
            etl_envelope=fake_etl,
            provider_proof_binding={
                "fileName": "TracerptProviderProof.json", "bytes": 1,
                "sha256": "F" * 64,
            },
        )
        validate_capture_record(
            record, fake_context, archive_pre_binding=fake_pre,
            archive_post_binding=fake_post, etl_binding=fake_etl,
        )
        check("capture-record-baseline", True)
        broken_record = copy.deepcopy(record)
        broken_record["gameProcess"]["finishedUtc"] = "2026-01-02T00:14:00.000Z"
        try:
            validate_capture_record(
                broken_record, fake_context, archive_pre_binding=fake_pre,
                archive_post_binding=fake_post, etl_binding=fake_etl,
            )
        except RunnerError:
            short_capture_rejected = True
        else:
            short_capture_rejected = False
        check("short-capture-record-rejected", short_capture_rejected)
        overclaim = copy.deepcopy(record)
        overclaim["claimBoundary"]["technicalEvidenceAccepted"] = True
        try:
            validate_capture_record(
                overclaim, fake_context, archive_pre_binding=fake_pre,
                archive_post_binding=fake_post, etl_binding=fake_etl,
            )
        except RunnerError:
            capture_overclaim_rejected = True
        else:
            capture_overclaim_rejected = False
        check("capture-record-overclaim-rejected", capture_overclaim_rejected)
    except (RunnerError, etw.EtwProfileError, shipping.EvidenceError,
            OSError, KeyError, TypeError, ValueError) as exc:
        failures.append({"case": "self-test-exception", "error": str(exc)})
    result = {
        "schema": "DiscGolfTour.Session19SustainedShippingEtwRunnerSelfTest.v3",
        "passed": not failures,
        "assertionCount": checks,
        "failures": failures,
        "wprStarted": False,
        "gameLaunched": False,
        "captureExecuted": False,
        "filesWritten": False,
    }
    return (0 if not failures else 1), result


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--preflight-only", action="store_true")
    mode.add_argument("--capture", action="store_true")
    mode.add_argument("--export", action="store_true")
    mode.add_argument("--finalize", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    parser.add_argument("--declaration", type=Path)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--external-root", type=Path, default=DEFAULT_EXTERNAL_ROOT)
    parser.add_argument("--confirm-candidate-id")
    parser.add_argument("--confirm-profile-run-id")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if (args.declaration is not None or args.archive != DEFAULT_ARCHIVE
                or args.external_root != DEFAULT_EXTERNAL_ROOT
                or args.confirm_candidate_id is not None
                or args.confirm_profile_run_id is not None):
            print("--self-test accepts no overrides", file=sys.stderr)
            return 2
        code, result = run_self_test()
        print(json.dumps(result, indent=2, sort_keys=True))
        return code
    if not any((args.preflight_only, args.capture, args.export, args.finalize)):
        print("select --preflight-only, --capture, --export, or --finalize; no action taken",
              file=sys.stderr)
        return 2
    if args.declaration is None:
        print("--declaration is required; no action taken", file=sys.stderr)
        return 2
    if not args.capture and (
        args.confirm_candidate_id is not None
        or args.confirm_profile_run_id is not None
    ):
        print("capture confirmations are accepted only with --capture", file=sys.stderr)
        return 2
    try:
        context = load_context(args.declaration, args.archive, args.external_root)
        if args.preflight_only:
            result = preflight_result(context)
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0 if context["captureAllowed"] else 2
        if args.capture:
            require(args.confirm_candidate_id is not None
                    and args.confirm_profile_run_id is not None,
                    "--capture requires exact candidate and profile-run confirmations")
            result = create_capture(
                context, args.confirm_candidate_id, args.confirm_profile_run_id
            )
            print(json.dumps(result, indent=2, sort_keys=True))
            return 2
        if args.export:
            result = create_wpa_export(context)
            print(json.dumps(result, indent=2, sort_keys=True))
            return 2
        result = validate_and_finalize(context)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (RunnerError, declaration_tool.DeclarationError,
            etw.EtwProfileError, shipping.EvidenceError, OSError,
            subprocess.SubprocessError, ValueError) as exc:
        print(f"ETW v3 runner failed closed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
