#!/usr/bin/env python3
"""Preflight or predeclare one additive external WPR CPU/GPU ETW run.

Preflight is read-only: it revalidates the sealed candidate and its existing
three-hole performance receipt, inventories WPR/WPAExporter, lists the WPR
CPU/GPU profiles, and checks that WPR is idle.  It never starts WPR, launches
the game, creates an evidence directory, installs tooling, or grants approval.

Declaration emission is explicit and append-only.  A declaration may record
the current capture-ready/export-blocked host, but capture remains forbidden
until a real, version-bound WPAExporter is available.
"""

from __future__ import annotations

import argparse
import copy
import ctypes
from datetime import datetime, timezone
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Any, Optional, Sequence
import uuid

import audit_dg_session19_shipping_profile_lane as profile_lane
import validate_dg_session19_shipping_performance as shipping
import validate_dg_session19_sustained_etw_profile as etw


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = etw.POLICY_PATH
DEFAULT_ARCHIVE = etw.DEFAULT_ARCHIVE
DEFAULT_EXTERNAL_ROOT = etw.DEFAULT_EXTERNAL_ROOT
DEFAULT_ENGINE_ROOT = profile_lane.DEFAULT_ENGINE_ROOT
DECLARATION_SCHEMA = etw.DECLARATION_SCHEMA
PREFLIGHT_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwPreflight.v3"
DECLARATION_PREFIX = "SustainedEtwProfileDeclaration"
WPR_DEFAULT = Path(r"C:\Windows\System32\wpr.exe")
WPA_DEFAULT = Path(
    r"C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\WPAExporter.exe"
)
TRACERPT_DEFAULT = Path(r"C:\Windows\System32\tracerpt.exe")
UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
)


class DeclarationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise DeclarationError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def parse_run_id(value: str) -> str:
    require(UUID_RE.fullmatch(value) is not None,
            "profile run ID must be a lowercase RFC 4122 UUID v4")
    try:
        parsed = uuid.UUID(value)
    except ValueError:
        raise DeclarationError("profile run ID is malformed") from None
    require(parsed.version == 4 and str(parsed) == value,
            "profile run ID must be canonical UUID v4")
    return value


def safe_relative(value: Any, label: str) -> PurePosixPath:
    require(isinstance(value, str) and value and "\\" not in value,
            f"{label} must be a relative POSIX path")
    pure = PurePosixPath(value)
    require(not pure.is_absolute()
            and not any(part in ("", ".", "..") for part in pure.parts),
            f"{label} is unsafe")
    return pure


def windows_file_version(path: Path) -> str:
    if os.name != "nt":
        return "NON_WINDOWS_SELF_TEST"

    class FixedFileInfo(ctypes.Structure):
        _fields_ = [
            ("dwSignature", ctypes.c_uint32), ("dwStrucVersion", ctypes.c_uint32),
            ("dwFileVersionMS", ctypes.c_uint32), ("dwFileVersionLS", ctypes.c_uint32),
            ("dwProductVersionMS", ctypes.c_uint32),
            ("dwProductVersionLS", ctypes.c_uint32),
            ("dwFileFlagsMask", ctypes.c_uint32), ("dwFileFlags", ctypes.c_uint32),
            ("dwFileOS", ctypes.c_uint32), ("dwFileType", ctypes.c_uint32),
            ("dwFileSubtype", ctypes.c_uint32), ("dwFileDateMS", ctypes.c_uint32),
            ("dwFileDateLS", ctypes.c_uint32),
        ]

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
    require(size > 0, f"{path.name} has no readable Windows version resource")
    buffer = ctypes.create_string_buffer(size)
    require(bool(version.GetFileVersionInfoW(str(path), 0, size, buffer)),
            f"{path.name} version resource could not be read")
    pointer = ctypes.c_void_p()
    length = ctypes.c_uint()
    require(bool(version.VerQueryValueW(
        buffer, "\\", ctypes.byref(pointer), ctypes.byref(length)
    )), f"{path.name} fixed version resource is missing")
    info = ctypes.cast(pointer, ctypes.POINTER(FixedFileInfo)).contents
    require(info.dwSignature == 0xFEEF04BD,
            f"{path.name} version signature differs")
    return ".".join(str(value) for value in (
        info.dwFileVersionMS >> 16, info.dwFileVersionMS & 0xFFFF,
        info.dwFileVersionLS >> 16, info.dwFileVersionLS & 0xFFFF,
    ))


def versioned_tool_binding(path: Path) -> dict[str, Any]:
    binding = etw.stable_binding(path, path.name)
    try:
        version = windows_file_version(path)
    except DeclarationError:
        version = f"UNVERSIONED_BINARY_SHA256_{binding['sha256']}"
    return {**binding, "version": version}


def validate_external_root(external_root: Path, archive: Path) -> Path:
    try:
        external = external_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise DeclarationError(
            f"external root is missing ({exc.__class__.__name__})"
        ) from None
    require(external.is_dir() and not shipping.is_reparse_point(external),
            "external root must be a regular non-reparse directory")
    require(not shipping.is_same_or_under(external, ROOT)
            and not shipping.is_same_or_under(ROOT, external)
            and not shipping.is_same_or_under(external, archive)
            and not shipping.is_same_or_under(archive, external),
            "external root must be outside project/archive roots")
    return external


def validate_destination_chain(external: Path, relative: str) -> Path:
    pure = safe_relative(relative, "external destination")
    cursor = external
    for part in pure.parts:
        cursor /= part
        if os.path.lexists(cursor):
            require(not shipping.is_reparse_point(cursor),
                    "external destination chain contains a reparse point")
            try:
                cursor.resolve(strict=True).relative_to(external)
            except (OSError, RuntimeError, ValueError):
                raise DeclarationError(
                    "external destination escapes the declared root"
                ) from None
    return external / Path(*pure.parts)


def resolve_tools(
    *, wpr: Optional[Path], wpaexporter: Optional[Path],
    tracerpt: Optional[Path], unreal_insights: Optional[Path], engine_root: Path,
) -> tuple[Path, Path, Optional[Path], Optional[Path]]:
    wpr_path = profile_lane.resolve_regular_tool(wpr, (WPR_DEFAULT, "wpr"))
    require(wpr_path is not None, "wpr.exe is unavailable")
    tracerpt_path = profile_lane.resolve_regular_tool(
        tracerpt, (TRACERPT_DEFAULT, "tracerpt")
    )
    require(tracerpt_path is not None, "tracerpt.exe is unavailable")
    wpa_path = profile_lane.resolve_regular_tool(
        wpaexporter, (WPA_DEFAULT, "wpaexporter")
    )
    insights_path = profile_lane.resolve_regular_tool(
        unreal_insights, (engine_root / profile_lane.UNREAL_INSIGHTS_RELATIVE,)
    )
    return wpr_path, tracerpt_path, wpa_path, insights_path


def wpr_status(wpr: Path, idle_token: str) -> dict[str, Any]:
    code, stdout, stderr = profile_lane.run_command((str(wpr), "-status"), 15)
    combined = (stdout + "\n" + stderr).strip()
    return {
        "exitCode": code,
        "idleTokenObserved": idle_token in combined,
        "idle": code == 0 and idle_token in combined,
    }


def wpr_profile_details(
    wpr: Path, contract: dict[str, Any]
) -> dict[str, dict[str, Any]]:
    results: dict[str, dict[str, Any]] = {}
    for role in ("CPU", "GPU"):
        arguments = contract["profileDetailsArguments"][role]
        code, stdout, stderr = profile_lane.run_command(
            (str(wpr), *arguments), 15
        )
        combined = stdout + "\n" + stderr
        tokens = contract["requiredProfileDetailTokens"][role]
        results[role] = {
            "exitCode": code,
            "requiredTokensObserved": all(token in combined for token in tokens),
            "ready": code == 0 and all(token in combined for token in tokens),
        }
    return results


def validate_profile_audit(
    audit: dict[str, Any], policy: dict[str, Any],
    *, exporter_available: bool,
) -> None:
    require(audit.get("schema") ==
            "DiscGolfTour.Session19ShippingProfileLaneAudit.v1"
            and audit.get("candidateId") == policy["candidateId"]
            and audit.get("state") == profile_lane.STATE,
            "Shipping profile-lane audit identity differs")
    archive = audit.get("archive", {})
    expected = policy["candidateArchive"]
    audited_executable = archive.get("shippingExecutable", {})
    require(archive.get("inventorySha256") == expected["inventorySha256"]
            and archive.get("fileCount") == expected["fileCount"]
            and archive.get("totalBytes") == expected["bytes"]
            and {key: audited_executable.get(key)
                 for key in ("relativePath", "bytes", "sha256")}
            == expected["shippingExecutable"],
            "Shipping profile-lane audit archive differs")
    rules = audit.get("sourceRules", {})
    require(rules.get("projectTarget", {}).get("explicitFullTraceEnable") is False
            and rules.get("projectTarget", {}).get("ordinaryShippingDefaultApplies") is True
            and rules.get("engineTargetRules", {}).get(
                "ordinaryShippingFullTraceDefaultDisabled"
            ) is True
            and rules.get("engineTargetRules", {}).get(
                "fullShippingTraceUnsupportedGuardPresent"
            ) is True,
            "Shipping trace boundary audit differs")
    lane = audit.get("externalWindowsProfileLane", {})
    require(lane.get("cpuGpuCaptureToolingReady") is True
            and lane.get("candidateBoundCaptureExecuted") is False,
            "WPR CPU/GPU profile-lane readiness differs")
    require(lane.get("automatedAnalysisExportReady") is exporter_available,
            "WPAExporter availability differs within the profile-lane audit")
    claims = audit.get("claimBoundary", {})
    for key in (
        "gameLaunched", "traceCaptured", "profileExported",
        "sealedBinaryFullTraceOperationallyProven", "shippingPerformanceCertified",
        "humanPerformanceAcceptance", "productOwnerApproval", "releaseApproval",
        "releaseReady",
    ):
        require(claims.get(key) is False,
                f"Shipping profile-lane audit may not grant {key}")


def build_declaration(
    *, policy: dict[str, Any], policy_binding: dict[str, Any],
    profile_run_id: str, declared_utc: str, duration_seconds: int,
    archive: Path, receipt: dict[str, Any], performance_run_root: Path,
    external: Path, wpr_path: Path, wpr_binding: dict[str, Any],
    wpr_profile_result: dict[str, Any], status: dict[str, Any],
    profile_details: dict[str, dict[str, Any]],
    tracerpt_path: Path, tracerpt_binding: dict[str, Any],
    wpa_path: Optional[Path], wpa_binding: Optional[dict[str, Any]],
    wpa_profile_path: Optional[Path],
    wpa_profile_binding: Optional[dict[str, Any]],
) -> dict[str, Any]:
    profile_run_id = parse_run_id(profile_run_id)
    candidate = policy["candidateId"]
    declaration_relative = (
        f"SustainedEtwProfile/Declarations/{DECLARATION_PREFIX}-"
        f"{candidate}-{profile_run_id}.json"
    )
    run_relative = f"SustainedEtwProfile/Runs/{candidate}/{profile_run_id}"
    capture_ready = (
        wpr_profile_result.get("profilesExitCode") == 0
        and wpr_profile_result.get("cpuProfileAvailable") is True
        and wpr_profile_result.get("gpuProfileAvailable") is True
        and status.get("idle") is True
        and all(profile_details.get(role, {}).get("ready") is True
                for role in ("CPU", "GPU"))
        and tracerpt_binding["fileName"].casefold() == "tracerpt.exe"
    )
    export_ready = (wpa_path is not None and wpa_binding is not None
                    and wpa_profile_path is not None
                    and wpa_profile_binding is not None)
    capture_allowed = capture_ready and export_ready
    if capture_allowed:
        state = "PREDECLARED_CAPTURE_READY_NO_CAPTURE_STARTED"
    elif capture_ready and not export_ready:
        state = "PREDECLARED_CAPTURE_READY_EXPORT_BLOCKED_NO_CAPTURE_STARTED"
    else:
        state = "PREDECLARED_CAPTURE_BLOCKED_NO_CAPTURE_STARTED"
    return {
        "schema": DECLARATION_SCHEMA,
        "schemaVersion": 3,
        "session": 19,
        "profileRunId": profile_run_id,
        "declaredUtc": declared_utc,
        "state": state,
        "candidateId": candidate,
        "policy": policy_binding,
        "candidateArchive": {
            "path": str(archive),
            **copy.deepcopy(policy["candidateArchive"]),
        },
        "shippingPerformance": {
            "receipt": copy.deepcopy(policy["bindings"]["shippingPerformanceReceipt"]),
            "validatedRunId": receipt["runId"],
            "validatedRunRoot": str(performance_run_root),
            "independentlyRevalidated": True,
        },
        "requestedDurationSeconds": duration_seconds,
        "tooling": {
            "wpr": {
                "path": str(wpr_path),
                "binding": wpr_binding,
                "profilesExitCode": wpr_profile_result["profilesExitCode"],
                "cpuProfileAvailable": wpr_profile_result["cpuProfileAvailable"],
                "gpuProfileAvailable": wpr_profile_result["gpuProfileAvailable"],
                "profileDetails": profile_details,
                "status": status,
            },
            "wpaExporter": {
                "available": export_ready,
                "path": str(wpa_path) if wpa_path is not None else None,
                "binding": wpa_binding,
            },
            "tracerpt": {
                "path": str(tracerpt_path),
                "binding": tracerpt_binding,
            },
            "wpaProfile": {
                "available": wpa_profile_path is not None,
                "path": str(wpa_profile_path) if wpa_profile_path is not None else None,
                "binding": wpa_profile_binding,
            },
        },
        "externalLayout": {
            "externalRoot": str(external),
            "declarationRelativePath": declaration_relative,
            "runRelativePath": run_relative,
            "requiredArtifacts": copy.deepcopy(policy["requiredArtifacts"]),
            "appendOnly": True,
        },
        "contracts": {
            "shippingTraceBoundary": copy.deepcopy(policy["shippingTraceBoundary"]),
            "wpr": copy.deepcopy(policy["wprContract"]),
            "captureProvenance": copy.deepcopy(
                policy["captureProvenanceContract"]
            ),
            "etl": copy.deepcopy(policy["etlContract"]),
            "tracerpt": copy.deepcopy(policy["tracerptContract"]),
            "export": copy.deepcopy(policy["exportContract"]),
            "chronology": copy.deepcopy(policy["chronologyContract"]),
            "schemas": copy.deepcopy(policy["schemas"]),
        },
        "capability": {
            "captureToolingReady": capture_ready,
            "exportToolingReady": export_ready,
            "captureAllowed": capture_allowed,
            "wprStarted": False,
            "gameLaunched": False,
            "etlCaptured": False,
            "rawExportsCreated": False,
            "normalizedFourRoleExportCreated": False,
        },
        "claimBoundary": {
            "predeclarationOnly": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "cryptographicProcessAttestation": False,
            "hostileSameUserForgeryResistance": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }


def validate_declaration(
    value: dict[str, Any], policy: dict[str, Any],
    policy_binding: dict[str, Any],
) -> None:
    etw.require_keys(value, {
        "schema", "schemaVersion", "session", "profileRunId", "declaredUtc",
        "state", "candidateId", "policy", "candidateArchive",
        "shippingPerformance", "requestedDurationSeconds", "tooling",
        "externalLayout", "contracts", "capability", "claimBoundary",
    }, "ETW v3 declaration")
    require(value["schema"] == DECLARATION_SCHEMA
            and value["schemaVersion"] == 3 and value["session"] == 19,
            "ETW declaration schema differs")
    parse_run_id(value["profileRunId"])
    shipping.parse_utc(value["declaredUtc"], "ETW declaration declaredUtc")
    require(value["candidateId"] == policy["candidateId"],
            "ETW declaration candidate differs")
    require(value["policy"] == policy_binding,
            "ETW declaration policy binding differs")
    archive = value["candidateArchive"]
    require(isinstance(archive.get("path"), str) and archive.get("path")
            and {key: archive[key] for key in policy["candidateArchive"]}
            == policy["candidateArchive"],
            "ETW declaration archive binding differs")
    perf = value["shippingPerformance"]
    require(perf == {
        "receipt": policy["bindings"]["shippingPerformanceReceipt"],
        "validatedRunId": policy["bindings"]["shippingPerformanceReceipt"]["runId"],
        "validatedRunRoot": perf.get("validatedRunRoot"),
        "independentlyRevalidated": True,
    } and isinstance(perf.get("validatedRunRoot"), str)
            and perf["validatedRunRoot"],
            "ETW declaration performance receipt/run binding differs")
    duration = value["requestedDurationSeconds"]
    pre = policy["predeclaration"]
    require(type(duration) is int
            and pre["minimumDurationSeconds"] <= duration
            <= pre["maximumDurationSeconds"],
            "ETW declaration duration is outside policy")
    tooling = value["tooling"]
    etw.require_keys(tooling, {"wpr", "wpaExporter", "tracerpt", "wpaProfile"},
                     "ETW declaration tooling")
    wpr = tooling["wpr"]
    etw.require_keys(wpr, {
        "path", "binding", "profilesExitCode", "cpuProfileAvailable",
        "gpuProfileAvailable", "profileDetails", "status",
    }, "ETW declaration WPR")
    etw.require_keys(wpr["binding"], {"fileName", "bytes", "sha256", "version"},
                     "ETW declaration WPR binding")
    require(wpr["binding"]["fileName"].casefold() == "wpr.exe"
            and type(wpr["binding"]["bytes"]) is int and wpr["binding"]["bytes"] > 0
            and etw.SHA_RE.fullmatch(wpr["binding"]["sha256"])
            and isinstance(wpr["binding"]["version"], str)
            and wpr["binding"]["version"],
            "ETW declaration WPR binding differs")
    require({key: wpr["binding"][key]
             for key in ("fileName", "bytes", "sha256")}
            == policy["hostToolIdentities"]["wpr"],
            "ETW declaration WPR identity differs from pinned policy")
    require(type(wpr["profilesExitCode"]) is int
            and type(wpr["cpuProfileAvailable"]) is bool
            and type(wpr["gpuProfileAvailable"]) is bool,
            "ETW declaration WPR profile listing result differs")
    details = wpr["profileDetails"]
    etw.require_keys(details, {"CPU", "GPU"}, "ETW declaration WPR profile details")
    for role in ("CPU", "GPU"):
        etw.require_keys(details[role],
                         {"exitCode", "requiredTokensObserved", "ready"},
                         f"ETW declaration WPR {role} details")
        require(type(details[role]["exitCode"]) is int
                and type(details[role]["requiredTokensObserved"]) is bool
                and type(details[role]["ready"]) is bool
                and details[role]["ready"] is (
                    details[role]["exitCode"] == 0
                    and details[role]["requiredTokensObserved"] is True
                ), f"ETW declaration WPR {role} detail result differs")
    status = wpr["status"]
    require(status == {
        "exitCode": status.get("exitCode"),
        "idleTokenObserved": status.get("idleTokenObserved"),
        "idle": status.get("idle"),
    } and type(status["exitCode"]) is int
            and type(status["idleTokenObserved"]) is bool
            and type(status["idle"]) is bool,
            "ETW declaration WPR status differs")
    require(status["idle"] is (
        status["exitCode"] == 0 and status["idleTokenObserved"] is True
    ), "ETW declaration WPR idle status is incoherent")
    tracerpt = tooling["tracerpt"]
    etw.require_keys(tracerpt, {"path", "binding"}, "ETW declaration tracerpt")
    require(isinstance(tracerpt["path"], str) and tracerpt["path"],
            "ETW declaration tracerpt path is missing")
    etw.require_keys(tracerpt["binding"],
                     {"fileName", "bytes", "sha256", "version"},
                     "ETW declaration tracerpt binding")
    require(tracerpt["binding"]["fileName"].casefold() == "tracerpt.exe"
            and type(tracerpt["binding"]["bytes"]) is int
            and tracerpt["binding"]["bytes"] > 0
            and etw.SHA_RE.fullmatch(tracerpt["binding"]["sha256"])
            and isinstance(tracerpt["binding"]["version"], str)
            and tracerpt["binding"]["version"],
            "ETW declaration tracerpt binding differs")
    require({key: tracerpt["binding"][key]
             for key in ("fileName", "bytes", "sha256")}
            == policy["hostToolIdentities"]["tracerpt"],
            "ETW declaration tracerpt identity differs from pinned policy")
    exporter = tooling["wpaExporter"]
    etw.require_keys(exporter, {"available", "path", "binding"},
                     "ETW declaration WPAExporter")
    if exporter["available"]:
        require(isinstance(exporter["path"], str) and exporter["path"]
                and isinstance(exporter["binding"], dict),
                "available WPAExporter must be path- and identity-bound")
        etw.require_keys(exporter["binding"],
                         {"fileName", "bytes", "sha256", "version"},
                         "ETW declaration WPAExporter binding")
        require(exporter["binding"]["fileName"].casefold() == "wpaexporter.exe"
                and type(exporter["binding"]["bytes"]) is int
                and exporter["binding"]["bytes"] > 0
                and etw.SHA_RE.fullmatch(exporter["binding"]["sha256"])
                and isinstance(exporter["binding"]["version"], str)
                and exporter["binding"]["version"],
                "ETW declaration WPAExporter binding differs")
    else:
        require(exporter == {"available": False, "path": None, "binding": None},
                "blocked WPAExporter declaration differs")
    pinned_wpa = policy["hostToolIdentities"]["wpaExporter"]
    require((exporter["available"] is False and pinned_wpa == {
        "available": False, "binding": None,
    }) or (exporter["available"] is True and pinned_wpa == {
        "available": True,
        "binding": {key: exporter["binding"][key]
                    for key in ("fileName", "bytes", "sha256")},
    }), "ETW declaration WPAExporter differs from pinned policy")
    wpa_profile = tooling["wpaProfile"]
    etw.require_keys(wpa_profile, {"available", "path", "binding"},
                     "ETW declaration WPA profile")
    if wpa_profile["available"]:
        require(isinstance(wpa_profile["path"], str) and wpa_profile["path"]
                and Path(wpa_profile["path"]).suffix.casefold() == ".wpaprofile"
                and isinstance(wpa_profile["binding"], dict),
                "available WPA profile must be exact-path and identity-bound")
        etw.require_keys(wpa_profile["binding"], {"fileName", "bytes", "sha256"},
                         "ETW declaration WPA profile binding")
        require(type(wpa_profile["binding"]["bytes"]) is int
                and wpa_profile["binding"]["bytes"] > 0
                and etw.SHA_RE.fullmatch(wpa_profile["binding"]["sha256"]),
                "ETW declaration WPA profile binding differs")
    else:
        require(wpa_profile == {"available": False, "path": None, "binding": None},
                "blocked WPA profile declaration differs")
    layout = value["externalLayout"]
    etw.require_keys(layout, {
        "externalRoot", "declarationRelativePath", "runRelativePath",
        "requiredArtifacts", "appendOnly",
    }, "ETW declaration external layout")
    safe_relative(layout["declarationRelativePath"], "declaration path")
    safe_relative(layout["runRelativePath"], "run path")
    candidate = value["candidateId"]
    run_id = value["profileRunId"]
    require(layout["declarationRelativePath"] ==
            f"SustainedEtwProfile/Declarations/{DECLARATION_PREFIX}-{candidate}-{run_id}.json"
            and layout["runRelativePath"] ==
            f"SustainedEtwProfile/Runs/{candidate}/{run_id}"
            and layout["requiredArtifacts"] == policy["requiredArtifacts"]
            and layout["appendOnly"] is True,
            "ETW declaration external layout differs")
    require(value["contracts"] == {
        "shippingTraceBoundary": policy["shippingTraceBoundary"],
        "wpr": policy["wprContract"],
        "captureProvenance": policy["captureProvenanceContract"],
        "etl": policy["etlContract"],
        "tracerpt": policy["tracerptContract"],
        "export": policy["exportContract"],
        "chronology": policy["chronologyContract"],
        "schemas": policy["schemas"],
    }, "ETW declaration contract copy differs")
    capability = value["capability"]
    etw.require_keys(capability, {
        "captureToolingReady", "exportToolingReady", "captureAllowed",
        "wprStarted", "gameLaunched", "etlCaptured", "rawExportsCreated",
        "normalizedFourRoleExportCreated",
    }, "ETW declaration capability")
    capture_ready = (
        wpr["profilesExitCode"] == 0
        and wpr["cpuProfileAvailable"] is True
        and wpr["gpuProfileAvailable"] is True
        and status["idle"] is True
        and all(details[role]["ready"] is True for role in ("CPU", "GPU"))
        and tracerpt["binding"]["fileName"].casefold() == "tracerpt.exe"
    )
    export_ready = (exporter["available"] is True
                    and wpa_profile["available"] is True)
    require(capability["captureToolingReady"] is capture_ready
            and capability["exportToolingReady"] is export_ready
            and capability["captureAllowed"] is (capture_ready and export_ready)
            and all(capability[key] is False for key in (
                "wprStarted", "gameLaunched", "etlCaptured", "rawExportsCreated",
                "normalizedFourRoleExportCreated",
            )), "ETW declaration capability/state differs")
    expected_state = (
        "PREDECLARED_CAPTURE_READY_NO_CAPTURE_STARTED"
        if capture_ready and export_ready else
        "PREDECLARED_CAPTURE_READY_EXPORT_BLOCKED_NO_CAPTURE_STARTED"
        if capture_ready else "PREDECLARED_CAPTURE_BLOCKED_NO_CAPTURE_STARTED"
    )
    require(value["state"] == expected_state,
            "ETW declaration state differs")
    require(value["claimBoundary"] == {
        "predeclarationOnly": True,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "cryptographicProcessAttestation": False,
        "hostileSameUserForgeryResistance": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "ETW declaration claim boundary differs")


def preflight(
    *, policy_path: Path, archive: Path, external_root: Path,
    engine_root: Path, wpr: Optional[Path], wpaexporter: Optional[Path],
    tracerpt: Optional[Path], wpa_profile: Optional[Path],
    unreal_insights: Optional[Path], profile_run_id: str, duration_seconds: int,
) -> tuple[dict[str, Any], dict[str, Any], Path]:
    require(policy_path.resolve(strict=True) == POLICY_PATH.resolve(strict=True),
            "only the exact additive ETW v3 policy is supported")
    policy = etw.load_json(POLICY_PATH, "ETW v3 policy")
    etw.validate_policy(policy)
    archive, receipt, performance_run_root = etw.validate_baseline(
        policy, archive, external_root
    )
    external = validate_external_root(external_root, archive)
    pre = policy["predeclaration"]
    require(type(duration_seconds) is int
            and pre["minimumDurationSeconds"] <= duration_seconds
            <= pre["maximumDurationSeconds"],
            "requested duration is outside the 900-7200 second policy range")
    wpr_path, tracerpt_path, wpa_path, insights_path = resolve_tools(
        wpr=wpr, wpaexporter=wpaexporter, tracerpt=tracerpt,
        unreal_insights=unreal_insights,
        engine_root=engine_root,
    )
    if wpa_profile is not None:
        try:
            wpa_profile_path = wpa_profile.resolve(strict=True)
        except (OSError, RuntimeError) as exc:
            raise DeclarationError(
                f"WPA profile is missing ({exc.__class__.__name__})"
            ) from None
        require(wpa_profile_path.is_file()
                and not shipping.is_reparse_point(wpa_profile_path)
                and wpa_profile_path.suffix.casefold() == ".wpaprofile",
                "WPA profile must be a regular non-reparse .wpaProfile file")
        wpa_profile_binding = etw.stable_binding(wpa_profile_path, "WPA profile")
    else:
        wpa_profile_path = None
        wpa_profile_binding = None
    audit = profile_lane.audit(
        candidate_id=policy["candidateId"], archive=archive,
        engine_root=engine_root, wpr=wpr_path, wpaexporter=wpa_path,
        unreal_insights=insights_path,
    )
    validate_profile_audit(audit, policy, exporter_available=wpa_path is not None)
    lane_wpr = audit["externalWindowsProfileLane"]["wpr"]
    status = wpr_status(wpr_path, policy["wprContract"]["requiredIdleStatusToken"])
    profile_details = wpr_profile_details(wpr_path, policy["wprContract"])
    wpr_binding = versioned_tool_binding(wpr_path)
    tracerpt_binding = versioned_tool_binding(tracerpt_path)
    wpa_binding = versioned_tool_binding(wpa_path) if wpa_path is not None else None
    require({key: wpr_binding[key] for key in ("fileName", "bytes", "sha256")}
            == policy["hostToolIdentities"]["wpr"],
            "live WPR identity differs from the pinned v3 policy")
    require({key: tracerpt_binding[key]
             for key in ("fileName", "bytes", "sha256")}
            == policy["hostToolIdentities"]["tracerpt"],
            "live tracerpt identity differs from the pinned v3 policy")
    pinned_wpa = policy["hostToolIdentities"]["wpaExporter"]
    require((wpa_binding is None and pinned_wpa == {
        "available": False, "binding": None,
    }) or (wpa_binding is not None and pinned_wpa == {
        "available": True,
        "binding": {key: wpa_binding[key]
                    for key in ("fileName", "bytes", "sha256")},
    }), "live WPAExporter identity differs from the pinned v3 policy")
    policy_binding = etw.stable_binding(POLICY_PATH, "ETW v3 policy")
    declaration = build_declaration(
        policy=policy, policy_binding=policy_binding,
        profile_run_id=profile_run_id, declared_utc=utc_now(),
        duration_seconds=duration_seconds, archive=archive, receipt=receipt,
        performance_run_root=performance_run_root, external=external,
        wpr_path=wpr_path, wpr_binding=wpr_binding,
        wpr_profile_result=lane_wpr, status=status,
        profile_details=profile_details,
        tracerpt_path=tracerpt_path, tracerpt_binding=tracerpt_binding,
        wpa_path=wpa_path, wpa_binding=wpa_binding,
        wpa_profile_path=wpa_profile_path,
        wpa_profile_binding=wpa_profile_binding,
    )
    validate_declaration(declaration, policy, policy_binding)
    return declaration, audit, external


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
        policy_binding = etw.stable_binding(POLICY_PATH, "self-test ETW policy")
        run_id = "11111111-2222-4333-8444-555555555555"
        fake_wpr_binding = {
            **policy["hostToolIdentities"]["wpr"],
            "version": "1.0",
        }
        blocked = build_declaration(
            policy=policy, policy_binding=policy_binding,
            profile_run_id=run_id, declared_utc=utc_now(),
            duration_seconds=1800, archive=Path("C:/sealed/Windows"),
            receipt={"runId": policy["bindings"]["shippingPerformanceReceipt"]["runId"]},
            performance_run_root=Path("C:/external/performance/run"),
            external=Path("C:/external"), wpr_path=Path("C:/Windows/wpr.exe"),
            wpr_binding=fake_wpr_binding,
            wpr_profile_result={
                "profilesExitCode": 0, "cpuProfileAvailable": True,
                "gpuProfileAvailable": True,
            },
            status={"exitCode": 0, "idleTokenObserved": True, "idle": True},
            profile_details={
                role: {"exitCode": 0, "requiredTokensObserved": True, "ready": True}
                for role in ("CPU", "GPU")
            },
            tracerpt_path=Path("C:/Windows/tracerpt.exe"),
            tracerpt_binding={
                **policy["hostToolIdentities"]["tracerpt"], "version": "1.0",
            },
            wpa_path=None, wpa_binding=None,
            wpa_profile_path=None, wpa_profile_binding=None,
        )
        validate_declaration(blocked, policy, policy_binding)
        check("blocked-declaration-valid", True)
        check("blocked-state-explicit", blocked["state"] ==
              "PREDECLARED_CAPTURE_READY_EXPORT_BLOCKED_NO_CAPTURE_STARTED")
        check("blocked-capture-forbidden", blocked["capability"]["captureAllowed"] is False)
        check("no-process-start-claims", blocked["capability"]["wprStarted"] is False
              and blocked["capability"]["gameLaunched"] is False)
        ready = build_declaration(
            policy=policy, policy_binding=policy_binding,
            profile_run_id=run_id, declared_utc=utc_now(),
            duration_seconds=1800, archive=Path("C:/sealed/Windows"),
            receipt={"runId": policy["bindings"]["shippingPerformanceReceipt"]["runId"]},
            performance_run_root=Path("C:/external/performance/run"),
            external=Path("C:/external"), wpr_path=Path("C:/Windows/wpr.exe"),
            wpr_binding=fake_wpr_binding,
            wpr_profile_result={
                "profilesExitCode": 0, "cpuProfileAvailable": True,
                "gpuProfileAvailable": True,
            },
            status={"exitCode": 0, "idleTokenObserved": True, "idle": True},
            profile_details={
                role: {"exitCode": 0, "requiredTokensObserved": True, "ready": True}
                for role in ("CPU", "GPU")
            },
            tracerpt_path=Path("C:/Windows/tracerpt.exe"),
            tracerpt_binding={
                **policy["hostToolIdentities"]["tracerpt"], "version": "1.0",
            },
            wpa_path=Path("C:/WPT/WPAExporter.exe"),
            wpa_binding={
                "fileName": "WPAExporter.exe", "bytes": 2,
                "sha256": "B" * 64, "version": "2.0",
            },
            wpa_profile_path=Path("C:/WPT/DGTour.wpaProfile"),
            wpa_profile_binding={
                "fileName": "DGTour.wpaProfile", "bytes": 3,
                "sha256": "E" * 64,
            },
        )
        try:
            validate_declaration(ready, policy, policy_binding)
        except (DeclarationError, etw.EtwProfileError):
            unpinned_exporter_rejected = True
        else:
            unpinned_exporter_rejected = False
        check("unpinned-exporter-ready-declaration-rejected",
              unpinned_exporter_rejected)
        for name, mutate in (
            ("policy-binding-drift", lambda item: item["policy"].__setitem__("bytes", 2)),
            ("duration-weakened", lambda item: item.__setitem__("requestedDurationSeconds", 60)),
            ("capture-overclaim", lambda item: item["capability"].__setitem__("wprStarted", True)),
            ("incoherent-idle-status", lambda item: item["tooling"]["wpr"]["status"].__setitem__("exitCode", 1)),
            ("release-overclaim", lambda item: item["claimBoundary"].__setitem__("releaseReady", True)),
            ("path-traversal", lambda item: item["externalLayout"].__setitem__("runRelativePath", "../escape")),
        ):
            candidate = copy.deepcopy(blocked)
            mutate(candidate)
            try:
                validate_declaration(candidate, policy, policy_binding)
            except (DeclarationError, etw.EtwProfileError, shipping.EvidenceError):
                rejected = True
            else:
                rejected = False
            check(name, rejected)
    except (DeclarationError, etw.EtwProfileError, shipping.EvidenceError,
            OSError, KeyError, TypeError, ValueError) as exc:
        failures.append({"case": "self-test-exception", "error": str(exc)})
    result = {
        "schema": "DiscGolfTour.Session19SustainedShippingEtwDeclarationSelfTest.v3",
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
    mode.add_argument("--emit-declaration", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--external-root", type=Path, default=DEFAULT_EXTERNAL_ROOT)
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--wpr", type=Path)
    parser.add_argument("--tracerpt", type=Path)
    parser.add_argument("--wpaexporter", type=Path)
    parser.add_argument("--wpa-profile", type=Path)
    parser.add_argument("--unreal-insights", type=Path)
    parser.add_argument("--profile-run-id")
    parser.add_argument("--duration-seconds", type=int, default=1800)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        forbidden = (
            args.policy != POLICY_PATH or args.archive != DEFAULT_ARCHIVE
            or args.external_root != DEFAULT_EXTERNAL_ROOT
            or args.engine_root != DEFAULT_ENGINE_ROOT or args.wpr is not None
            or args.tracerpt is not None or args.wpaexporter is not None
            or args.wpa_profile is not None or args.unreal_insights is not None
            or args.profile_run_id is not None or args.duration_seconds != 1800
        )
        if forbidden:
            print("--self-test accepts no path/tool/run overrides", file=sys.stderr)
            return 2
        code, result = run_self_test()
        print(json.dumps(result, indent=2, sort_keys=True))
        return code
    if not args.preflight_only and not args.emit_declaration:
        print("select --preflight-only or --emit-declaration; no action taken",
              file=sys.stderr)
        return 2
    run_id = args.profile_run_id or str(uuid.uuid4())
    try:
        declaration, audit, external = preflight(
            policy_path=args.policy, archive=args.archive,
            external_root=args.external_root, engine_root=args.engine_root,
            wpr=args.wpr, tracerpt=args.tracerpt,
            wpaexporter=args.wpaexporter, wpa_profile=args.wpa_profile,
            unreal_insights=args.unreal_insights, profile_run_id=run_id,
            duration_seconds=args.duration_seconds,
        )
        destination = validate_destination_chain(
            external, declaration["externalLayout"]["declarationRelativePath"]
        )
        if args.emit_declaration:
            require(not destination.exists(),
                    "refusing to overwrite an existing declaration")
            shipping.write_exclusive_json(destination, declaration)
            emitted = etw.stable_binding(destination, "emitted ETW declaration")
        else:
            emitted = None
        result = {
            "schema": PREFLIGHT_SCHEMA,
            "schemaVersion": 3,
            "session": 19,
            "candidateId": declaration["candidateId"],
            "profileRunId": declaration["profileRunId"],
            "state": declaration["state"],
            "preflightOnly": args.preflight_only,
            "declarationEmitted": args.emit_declaration,
            "declarationPath": str(destination),
            "declarationBinding": emitted,
            "captureToolingReady": declaration["capability"]["captureToolingReady"],
            "exportToolingReady": declaration["capability"]["exportToolingReady"],
            "captureAllowed": declaration["capability"]["captureAllowed"],
            "wprStarted": False,
            "gameLaunched": False,
            "captureExecuted": False,
            "filesWritten": bool(args.emit_declaration),
            "blockers": [
                *audit["blockers"],
                *([] if declaration["tooling"]["wpaProfile"]["available"] else [
                    "no exact .wpaProfile is predeclared for WPAExporter raw outputs"
                ]),
                "no tracerpt-parsed provider proof exists for a real ETL",
                "no actual WPAExporter invocation/output inventory is bound",
            ],
            "claimBoundary": declaration["claimBoundary"],
        }
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if declaration["capability"]["captureAllowed"] else 2
    except (DeclarationError, etw.EtwProfileError, profile_lane.ProfileLaneError,
            shipping.EvidenceError, OSError, ValueError) as exc:
        print(f"ETW v3 declaration preflight failed closed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
