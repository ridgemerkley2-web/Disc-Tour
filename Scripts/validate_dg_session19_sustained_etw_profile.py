#!/usr/bin/env python3
"""Validate the additive Session 19 external WPR CPU/GPU ETW profile lane.

This validator is deliberately independent of the frozen sustained-performance
v1/v2 lane.  A technical pass requires a real candidate-bound ETL, raw CPU and
GPU exports, and one strict normalized export containing GPU, GameThread,
RenderThread, and RHIThread rows.  It never grants human or release approval.
"""

from __future__ import annotations

import argparse
import copy
import csv
from datetime import datetime, timedelta, timezone
import hashlib
import io
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any, Iterable, Optional, Sequence
import uuid

import validate_dg_session19_shipping_performance as shipping


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_ID = "S19_WindowsShipping_20260827T023919Z_51a1c132f837"
POLICY_PATH = ROOT / (
    "Config/DG_Session19SustainedShippingEtwProfilePolicyV3-"
    f"{CANDIDATE_ID}.json"
)
DEFAULT_ARCHIVE = Path("C:/DGTour_Packages") / CANDIDATE_ID / "Windows"
DEFAULT_EXTERNAL_ROOT = Path("C:/DGTourExternal")
POLICY_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwProfilePolicy.v3"
DECLARATION_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwProfileDeclaration.v3"
NORMALIZED_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwNormalizedRoleTiming.v3"
AUDIT_SCHEMA = "DiscGolfTour.Session19SustainedShippingEtwTechnicalAudit.v3"
PASS_STATE = (
    "PASS_CANDIDATE_BOUND_SUSTAINED_SHIPPING_EXTERNAL_WPR_CPU_GPU_ETW_"
    "FOUR_ROLE_PROFILE_HUMAN_APPROVALS_PENDING"
)
ROLE_ORDER = ["GPU", "GameThread", "RenderThread", "RHIThread"]
SHA_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_RE = re.compile(
    r"^S19_WindowsShipping_[0-9]{8}T[0-9]{6}Z_[0-9a-f]{12}$"
)


class EtwProfileError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise EtwProfileError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def valid_lower_uuid_v4(value: Any) -> bool:
    if not isinstance(value, str) or value != value.lower():
        return False
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError):
        return False
    return parsed.version == 4 and str(parsed) == value


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = shipping.load_json(path)
    except shipping.EvidenceError as exc:
        raise EtwProfileError(f"{label}: {exc}") from None
    validate_numbers(value, label)
    return value


def validate_numbers(value: Any, label: str) -> None:
    if value is None or isinstance(value, (bool, str)):
        return
    if isinstance(value, int):
        require(-(2**63) <= value <= 2**63 - 1,
                f"{label}: integer is outside signed 64-bit range")
        return
    if isinstance(value, float):
        require(math.isfinite(value), f"{label}: non-finite number")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            validate_numbers(item, f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            validate_numbers(item, f"{label}.{key}")
        return
    raise EtwProfileError(f"{label}: unsupported value type")


def require_keys(value: Any, expected: Iterable[str], label: str) -> None:
    require(isinstance(value, dict), f"{label} must be an object")
    expected_set = set(expected)
    actual = set(value)
    require(actual == expected_set,
            f"{label} keys differ: missing={sorted(expected_set - actual)!r} "
            f"extra={sorted(actual - expected_set)!r}")


def safe_relative(root: Path, value: Any, label: str) -> Path:
    require(isinstance(value, str) and value and "\\" not in value,
            f"{label} must be a normalized relative POSIX path")
    pure = PurePosixPath(value)
    require(not pure.is_absolute()
            and not any(part in ("", ".", "..") for part in pure.parts),
            f"{label} is unsafe")
    try:
        resolved_root = root.resolve(strict=True)
        resolved = (root / Path(*pure.parts)).resolve(strict=True)
        resolved.relative_to(resolved_root)
    except (OSError, RuntimeError, ValueError) as exc:
        raise EtwProfileError(
            f"{label} is missing or escapes its root ({exc.__class__.__name__})"
        ) from None
    return resolved


def stable_binding(path: Path, label: str) -> dict[str, Any]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EtwProfileError(f"{label} is missing ({exc.__class__.__name__})") from None
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            f"{label} must be a regular non-reparse file")
    before = resolved.stat()
    binding = shipping.file_binding(resolved)
    after = resolved.stat()
    fingerprint = lambda item: (
        item.st_size, item.st_mtime_ns, item.st_ctime_ns,
        item.st_dev, item.st_ino,
    )
    require(fingerprint(before) == fingerprint(after), f"{label} changed while hashed")
    return binding


def binding_without_name(path: Path, label: str) -> dict[str, Any]:
    binding = stable_binding(path, label)
    return {"bytes": binding["bytes"], "sha256": binding["sha256"]}


def validate_policy(policy: dict[str, Any], *, check_files: bool = True) -> None:
    require_keys(policy, {
        "schema", "schemaVersion", "session", "policyId", "candidateId",
        "authority", "state", "bindings", "hostToolIdentities", "candidateArchive",
        "shippingTraceBoundary", "predeclaration", "wprContract",
        "captureProvenanceContract", "etlContract", "tracerptContract",
        "exportContract", "chronologyContract", "requiredArtifacts", "schemas",
        "technicalPassState", "failState", "claimBoundary",
    }, "ETW v3 policy")
    require(policy["schema"] == POLICY_SCHEMA and policy["schemaVersion"] == 3,
            "ETW v3 policy schema differs")
    require(policy["session"] == 19 and type(policy["session"]) is int,
            "ETW v3 policy session differs")
    require(policy["candidateId"] == CANDIDATE_ID
            and CANDIDATE_RE.fullmatch(policy["candidateId"]) is not None,
            "ETW v3 candidate ID differs")
    require(policy["policyId"] ==
            "windows_shipping_external_wpr_cpu_gpu_etw_profile_v3",
            "ETW v3 policy ID differs")
    require(policy["authority"] ==
            "ADDITIVE_EXTERNAL_WINDOWS_ETW_PROFILE_LANE_NOT_A_MODIFICATION_"
            "OF_FROZEN_V1_V2_EVIDENCE_OR_HUMAN_RELEASE_APPROVAL",
            "ETW v3 authority differs")
    require(policy["state"] == "CAPTURE_READY_EXPORT_BLOCKED_WPAEXPORTER_UNAVAILABLE",
            "ETW v3 policy must retain the current export-blocked state")
    require(policy["hostToolIdentities"] == {
        "wpr": {
            "fileName": "wpr.exe", "bytes": 389120,
            "sha256": "64F6D9F83A8B9F8C720870109A6374F650E3786F0EA3F6E3A6C02463257BBFC2",
        },
        "tracerpt": {
            "fileName": "tracerpt.exe", "bytes": 430080,
            "sha256": "A83FE5CE5F57FAFDF5DCED6EF3BA8BE143B474658D6224ADB51C42BFCEC77464",
        },
        "wpaExporter": {"available": False, "binding": None},
    }, "pinned host tool identities differ")
    trace = policy["shippingTraceBoundary"]
    require(trace == {
        "ordinaryShippingFullUnrealTraceCompiled": False,
        "fullShippingTraceUnsupportedGuardPresent": True,
        "externalEtwRequired": True,
        "unrealTraceMayNotBeSubstitutedOrFabricated": True,
    }, "Shipping trace boundary differs")
    pre = policy["predeclaration"]
    require(pre == {
        "minimumDurationSeconds": 900,
        "maximumDurationSeconds": 7200,
        "defaultDurationSeconds": 1800,
        "declarationMustPrecedeCapture": True,
        "runIdKind": "LOWERCASE_RFC4122_UUID_V4",
        "externalOnly": True,
        "appendOnly": True,
        "captureStartsOnlyWithExplicitCaptureModeAndCandidateConfirmation": True,
        "preflightNeverStartsWprOrGame": True,
        "captureForbiddenWhileExportToolingIsBlocked": True,
    }, "ETW v3 predeclaration contract differs")
    wpr = policy["wprContract"]
    require(wpr["requiredExecutableName"] == "wpr.exe"
            and wpr["requiredProfiles"] == ["CPU", "GPU"]
            and wpr["profileListingArguments"] == ["-profiles"]
            and wpr["profileDetailsArguments"] == {
                "CPU": ["-profiledetails", "CPU.light"],
                "GPU": ["-profiledetails", "GPU.light"],
            }
            and wpr["requiredProfileDetailTokens"] == {
                "CPU": ["CPU.Light", "SampledProfile", "CSwitch"],
                "GPU": ["GPU.Light", "Microsoft-Windows-DxgKrnl", "DX"],
            }
            and wpr["statusArguments"] == ["-status"]
            and wpr["requiredIdleStatusToken"] == "WPR is not recording"
            and wpr["startArgumentsTemplate"] == [
                "-start", "CPU.light", "-start", "GPU.light", "-filemode",
                "-recordtempto", "<EXTERNAL_WORKING_DIRECTORY>",
            ]
            and wpr["stopArgumentsTemplate"] == ["-stop", "<EXTERNAL_ETL_PATH>"]
            and wpr["cancelArguments"] == ["-cancel"]
            and all(wpr[key] is True for key in (
                "requireNoPreexistingRecording", "requireStartExitCodeZero",
                "requireStopExitCodeZero",
            )), "WPR contract differs")
    require(policy["captureProvenanceContract"] == {
        "gameProcessMustBindPidCreationUtcExecutablePathBytesAndSha256": True,
        "gamePidMustMatchEveryRawCpuGpuRole": True,
        "gameProcessCreationMustFallInsideCaptureWindow": True,
        "wprStartAndStopMustRecordExactArgumentsStartFinishAndExitCode": True,
        "wprStartAndStopExitCodesMustBeZero": True,
        "wprStartMustPrecedeGameProcessCreation": True,
        "gameProcessExitMustPrecedeWprStop": True,
    }, "capture provenance contract differs")
    etl = policy["etlContract"]
    require(etl["artifactName"] == "SustainedCpuGpu.etl"
            and etl["minimumBytes"] >= 1024 * 1024
            and etl["maximumBytes"] <= 32 * 1024**3
            and etl["minimumDistinctByteValues"] >= 32
            and etl["minimumNonZeroRatio"] >= 0.05
            and all(etl[key] is True for key in (
                "mustBeRegularNonReparseFile", "mustBeCreatedInsideDeclaredRun",
                "mustBeBoundBySha256", "mustFollowWprStartAndPrecedeWprStopRecord",
                "textJsonZipPePlaceholdersForbidden",
                "byteEnvelopeNeverEstablishesRealEtl",
                "realEtlRequiresBoundTracerptProviderProof",
            )), "ETL contract differs")
    tracerpt = policy["tracerptContract"]
    require(tracerpt == {
        "requiredExecutableName": "tracerpt.exe",
        "argumentsTemplate": [
            "<EXTERNAL_ETL_PATH>", "-o", "<EXTERNAL_TRACERPT_CSV_PATH>",
            "-of", "CSV", "-summary", "<EXTERNAL_TRACERPT_SUMMARY_PATH>", "-y",
        ],
        "minimumParsedCsvBytes": 1024,
        "minimumParsedEventRows": 1000,
        "requiredParsedCsvColumnsSubset": [
            "Event Name", "PID", "TID", "Clock-Time", "User Data",
        ],
        "minimumSummaryBytes": 32,
        "requiredProviderTokens": ["DxgKrnl", "Process"],
        "inputEtlOutputAndToolMustBeSha256Bound": True,
        "invocationExitCodeMustBeZero": True,
        "gamePidMustAppearInParsedOutput": True,
        "invocationMustFollowWprStop": True,
        "highEntropyEnvelopeAloneNeverAuthorizesRealEtl": True,
    }, "tracerpt provider-proof contract differs")
    export = policy["exportContract"]
    require(export["requiredExporterExecutableName"] == "WPAExporter.exe"
            and export["exporterMustExistBeforeCapture"] is True
            and export["exporterMustBeBoundByBytesSha256AndVersion"] is True
            and export["wpaProfileMustBePredeclaredByPathBytesAndSha256"] is True
            and export["requiredProfileExtension"] == ".wpaProfile"
            and export["invocationArgumentsTemplate"] == [
                "-i", "<EXTERNAL_ETL_PATH>", "-profile",
                "<BOUND_WPA_PROFILE_PATH>", "-delimiter", ",",
                "-outputfolder", "<EXTERNAL_WPA_OUTPUT_DIRECTORY>",
            ]
            and export["invocationExitCodeMustBeZero"] is True
            and export[
                "invocationMustBindExactToolProfileInputAndOutputInventory"
            ] is True
            and export["invocationMustFollowTracerptProviderProof"] is True
            and export[
                "rawCpuAndGpuBindingsMustExistInInvocationOutputInventory"
            ] is True
            and export["normalizedSchema"] == NORMALIZED_SCHEMA
            and export["requiredProcessName"] == "DiscGolfTour-Win64-Shipping.exe"
            and export["requiredRoles"] == ROLE_ORDER
            and export["minimumRawArtifactBytes"] >= 256
            and export["maximumRawArtifactBytes"] <= 512 * 1024**2
            and export["maximumRawArtifactBytes"] >
            export["minimumRawArtifactBytes"]
            and export["minimumEventsPerRole"] >= 60
            and export["minimumObservedDurationSecondsPerRole"] >= 60.0
            and export["declaredDurationCoverageToleranceSeconds"] <= 2.0
            and export["everyRoleMustSpanDeclaredDuration"] is True
            and export["percentileMethod"] == "NEAREST_RANK_CEIL_0_95_N"
            and export["normalizedExportMustBindExactEtlAndRawArtifacts"] is True
            and export["rawEventsMustIndependentlyRecomputeEveryNormalizedRole"] is True
            and export["allFourRolesMustShareOnePositiveProcessId"] is True
            and export["normalizedExportMustBeStrictFiniteJson"] is True
            and export["technicalClaimForbiddenWithoutAllFourRoles"] is True,
            "ETW export contract differs")
    require(export["requiredRawCpuColumns"] == [
        "Process", "ProcessId", "Thread", "ThreadId", "StartTimeSeconds",
        "DurationMilliseconds",
    ] and export["requiredRawGpuColumns"] == [
        "Process", "ProcessId", "StartTimeSeconds", "DurationMilliseconds", "Engine",
    ], "raw export column contract differs")
    require(export["requiredRoleKeys"] == [
        "role", "sourceTable", "sourceArtifactSha256", "processName",
        "processId", "threadName", "threadId", "eventCount",
        "firstTimestampSeconds", "lastTimestampSeconds", "observedDurationSeconds",
        "averageDurationMilliseconds", "p95DurationMilliseconds",
        "maximumDurationMilliseconds",
    ], "normalized role key contract differs")
    require(policy["technicalPassState"] == PASS_STATE
            and policy["failState"] == "FAIL_CLOSED",
            "ETW technical states differ")
    claims = policy["claimBoundary"]
    for key in (
        "technicalEvidenceAccepted", "sustainedPerformanceCertified",
        "cryptographicProcessAttestation", "hostileSameUserForgeryResistance",
        "humanPerformanceAcceptance", "productOwnerApproval", "releaseApproval",
        "releaseReady",
    ):
        require(claims.get(key) is False, f"policy may not grant {key}")
    require(claims.get("candidateArchiveBound") is True
            and claims.get("threeHoleRenderedPerformanceReceiptBound") is True
            and claims.get("externalWprCpuGpuEtwRequired") is True
            and claims.get("realEtlRequired") is True
            and claims.get("trustedTracerptProviderProofRequired") is True
            and claims.get("actualWpaExporterInvocationRequired") is True
            and claims.get(
                "gameProcessPidExecutableAndWindowBindingRequired"
            ) is True
            and claims.get("normalizedGpuGameRenderRhiExportRequired") is True,
            "ETW required claim boundary differs")
    require(policy["chronologyContract"] == {
        "futureTimestampToleranceSeconds": 2,
        "declarationMustPrecedeCaptureStart": True,
        "wprStartMustNotFollowGameStart": True,
        "gameFinishMustNotFollowWprStop": True,
        "etlMustPrecedeExport": True,
        "exportMustPrecedeRunManifest": True,
        "archivePostMustFollowGameFinish": True,
    }, "ETW chronology contract differs")
    require(policy["requiredArtifacts"] == {
        "archivePre": "ArchiveSnapshot.Pre.json",
        "archivePost": "ArchiveSnapshot.Post.json",
        "captureRecord": "EtwCaptureRecord.json",
        "etl": "SustainedCpuGpu.etl",
        "tracerptCsv": "TracerptEvents.csv",
        "tracerptSummary": "TracerptSummary.txt",
        "providerProof": "TracerptProviderProof.json",
        "wpaInvocation": "WpaExporterInvocation.json",
        "rawCpu": "RawCpuTiming.csv",
        "rawGpu": "RawGpuTiming.csv",
        "normalizedRoles": "NormalizedRoleTiming.json",
        "exportRecord": "WpaExportRecord.json",
        "runManifest": "RunManifest.json",
    }, "required ETW artifact set differs")
    require(policy["schemas"] == {
        "declaration": DECLARATION_SCHEMA,
        "archiveSnapshot": "DiscGolfTour.Session19SustainedShippingEtwArchiveSnapshot.v3",
        "captureRecord": "DiscGolfTour.Session19SustainedShippingEtwCaptureRecord.v3",
        "providerProof": "DiscGolfTour.Session19SustainedShippingEtwProviderProof.v3",
        "wpaInvocation": "DiscGolfTour.Session19SustainedShippingEtwWpaInvocation.v3",
        "exportRecord": "DiscGolfTour.Session19SustainedShippingEtwExportRecord.v3",
        "runManifest": "DiscGolfTour.Session19SustainedShippingEtwRunManifest.v3",
        "technicalAudit": AUDIT_SCHEMA,
    }, "ETW schema set differs")
    require_keys(policy["bindings"], {
        "shippingProfileLaneAudit", "shippingPerformanceReceipt",
        "declarationGenerator", "runner", "validator",
    }, "ETW v3 bindings")
    audit_binding = policy["bindings"]["shippingProfileLaneAudit"]
    require_keys(audit_binding, {"path", "bytes", "sha256"},
                 "profile-lane audit binding")
    require(type(audit_binding["bytes"]) is int and audit_binding["bytes"] > 0
            and isinstance(audit_binding["sha256"], str)
            and SHA_RE.fullmatch(audit_binding["sha256"]),
            "profile-lane audit binding shape differs")
    tooling_paths = {
        "declarationGenerator":
            "Scripts/generate_dg_session19_sustained_etw_profile_declaration.py",
        "runner": "Scripts/run_dg_session19_sustained_etw_profile.py",
        "validator": "Scripts/validate_dg_session19_sustained_etw_profile.py",
    }
    for key, expected_path in tooling_paths.items():
        binding = policy["bindings"][key]
        require_keys(binding, {"path", "bytes", "sha256"},
                     f"ETW {key} binding")
        require(binding["path"] == expected_path
                and type(binding["bytes"]) is int and binding["bytes"] > 0
                and isinstance(binding["sha256"], str)
                and SHA_RE.fullmatch(binding["sha256"]),
                f"ETW {key} binding shape differs")
    receipt_binding = policy["bindings"]["shippingPerformanceReceipt"]
    require_keys(receipt_binding, {
        "externalRootRelativePath", "bytes", "sha256", "schema", "state",
        "runId", "runRootRelativePath",
    }, "Shipping performance receipt binding")
    require(type(receipt_binding["bytes"]) is int and receipt_binding["bytes"] > 0
            and isinstance(receipt_binding["sha256"], str)
            and SHA_RE.fullmatch(receipt_binding["sha256"])
            and receipt_binding["schema"] ==
            "DiscGolfTour.Session19ShippingPerformanceEvidence.v1"
            and receipt_binding["state"] ==
            "PASS_CANDIDATE_BOUND_THREE_HOLE_RENDERED_PERFORMANCE"
            and valid_lower_uuid_v4(receipt_binding["runId"]),
            "Shipping performance receipt binding shape differs")
    candidate_archive = policy["candidateArchive"]
    require_keys(candidate_archive, {
        "archiveRecoveryLocationToken", "fileCount", "bytes",
        "inventorySha256", "launcher", "shippingExecutable",
    }, "ETW candidate archive")
    require(candidate_archive["archiveRecoveryLocationToken"] ==
            "DGTOUR_PACKAGES/S19_WindowsShipping_20260827T023919Z_51a1c132f837/Windows"
            and type(candidate_archive["fileCount"]) is int
            and candidate_archive["fileCount"] > 0
            and type(candidate_archive["bytes"]) is int
            and candidate_archive["bytes"] > 0
            and SHA_RE.fullmatch(candidate_archive["inventorySha256"]),
            "ETW candidate archive identity differs")
    for label, expected_path in (
        ("launcher", "DiscGolfTour.exe"),
        ("shippingExecutable", shipping.EXECUTABLE_RELATIVE),
    ):
        binding = candidate_archive[label]
        require_keys(binding, {"relativePath", "bytes", "sha256"},
                     f"ETW candidate {label}")
        require(binding["relativePath"] == expected_path
                and type(binding["bytes"]) is int and binding["bytes"] > 0
                and isinstance(binding["sha256"], str)
                and SHA_RE.fullmatch(binding["sha256"]),
                f"ETW candidate {label} binding differs")
    if check_files:
        audit_path = safe_relative(ROOT, audit_binding["path"], "profile-lane audit")
        require(binding_without_name(audit_path, "profile-lane audit") == {
            "bytes": audit_binding["bytes"], "sha256": audit_binding["sha256"],
        }, "profile-lane audit binding differs")
        for key, expected_path in tooling_paths.items():
            path = safe_relative(ROOT, expected_path, f"ETW {key}")
            binding = policy["bindings"][key]
            require(binding_without_name(path, f"ETW {key}") == {
                "bytes": binding["bytes"], "sha256": binding["sha256"],
            }, f"ETW {key} binding differs")


def archive_identity(archive: Path, candidate_id: str) -> dict[str, Any]:
    value = shipping.build_archive_manifest(archive, candidate_id)
    return {
        "fileCount": value["fileCount"],
        "bytes": value["totalBytes"],
        "inventorySha256": value["inventorySha256"],
        "launcher": {
            "relativePath": value["launcher"]["relativePath"],
            "bytes": value["launcher"]["bytes"],
            "sha256": value["launcher"]["sha256"],
        },
        "shippingExecutable": {
            "relativePath": value["shippingExecutable"]["relativePath"],
            "bytes": value["shippingExecutable"]["bytes"],
            "sha256": value["shippingExecutable"]["sha256"],
        },
    }


def validate_baseline(
    policy: dict[str, Any], archive: Path, external_root: Path
) -> tuple[Path, dict[str, Any], Path]:
    candidate = policy["candidateId"]
    archive = shipping.validate_candidate_archive_path(archive, candidate)
    require(archive_identity(archive, candidate) == {
        key: policy["candidateArchive"][key]
        for key in ("fileCount", "bytes", "inventorySha256", "launcher", "shippingExecutable")
    }, "live candidate archive differs from ETW v3 policy")
    try:
        external = external_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EtwProfileError(
            f"external root is missing ({exc.__class__.__name__})"
        ) from None
    require(external.is_dir() and not shipping.is_reparse_point(external),
            "external root must be a regular non-reparse directory")
    require(not shipping.is_same_or_under(external, ROOT)
            and not shipping.is_same_or_under(ROOT, external)
            and not shipping.is_same_or_under(external, archive)
            and not shipping.is_same_or_under(archive, external),
            "external root must be outside project/archive roots")
    receipt_binding = policy["bindings"]["shippingPerformanceReceipt"]
    receipt_path = safe_relative(
        external, receipt_binding["externalRootRelativePath"],
        "Shipping performance receipt",
    )
    require(binding_without_name(receipt_path, "Shipping performance receipt") == {
        "bytes": receipt_binding["bytes"], "sha256": receipt_binding["sha256"],
    }, "Shipping performance receipt binding differs")
    receipt = load_json(receipt_path, "Shipping performance receipt")
    require(receipt.get("candidateId") == candidate
            and receipt.get("schema") == receipt_binding["schema"]
            and receipt.get("state") == receipt_binding["state"]
            and receipt.get("runId") == receipt_binding["runId"]
            and receipt.get("passed") is True,
            "Shipping performance receipt identity/state differs")
    run_root = safe_relative(
        external, receipt_binding["runRootRelativePath"],
        "Shipping performance run",
    )
    require(run_root.is_dir(), "Shipping performance run root is not a directory")
    recomputed = shipping.validate_run(run_root, archive, candidate)
    require(recomputed.get("passed") is True,
            "Shipping performance run failed independent revalidation")
    for key in (
        "schema", "schemaVersion", "session", "candidateId", "state", "passed",
        "runId", "archive", "runManifest", "runs", "claimBoundary", "errors", "tooling",
    ):
        require(receipt.get(key) == recomputed.get(key),
                f"Shipping performance receipt differs after revalidation: {key}")
    return archive, receipt, run_root


def validate_etl_envelope(path: Path, contract: dict[str, Any]) -> dict[str, Any]:
    """Validate only a binary/file envelope, never ETL authenticity."""
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EtwProfileError(f"ETL is missing ({exc.__class__.__name__})") from None
    require(resolved.name == contract["artifactName"], "ETL artifact name differs")
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            "ETL must be a regular non-reparse file")
    before = resolved.stat()
    require(contract["minimumBytes"] <= before.st_size <= contract["maximumBytes"],
            "ETL byte size is outside the declared range")
    digest = hashlib.sha256()
    distinct: set[int] = set()
    nonzero = 0
    total = 0
    prefix = b""
    try:
        with resolved.open("rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                if len(prefix) < 4096:
                    prefix += chunk[:4096 - len(prefix)]
                digest.update(chunk)
                distinct.update(chunk)
                nonzero += len(chunk) - chunk.count(0)
                total += len(chunk)
    except OSError as exc:
        raise EtwProfileError(f"ETL read failed ({exc.__class__.__name__})") from None
    after = resolved.stat()
    require((before.st_size, before.st_mtime_ns, before.st_ctime_ns)
            == (after.st_size, after.st_mtime_ns, after.st_ctime_ns)
            and total == before.st_size, "ETL changed while validated")
    require(len(distinct) >= contract["minimumDistinctByteValues"],
            "ETL byte diversity is too low")
    require(nonzero / total >= contract["minimumNonZeroRatio"],
            "ETL non-zero ratio is too low")
    lowered = prefix.lstrip().lower()
    require(not any(lowered.startswith(token) for token in (
        b"{", b"[", b"pk\x03\x04", b"mz", b"<?xml", b"<html", b"placeholder",
    )), "ETL is a forbidden text/archive/PE placeholder")
    printable = sum(32 <= byte <= 126 or byte in (9, 10, 13) for byte in prefix)
    require(not prefix or printable / len(prefix) < 0.95,
            "ETL prefix is implausibly plain text")
    return {
        "fileName": resolved.name,
        "bytes": total,
        "sha256": digest.hexdigest().upper(),
        "distinctByteValues": len(distinct),
        "nonZeroRatio": nonzero / total,
        "validationScope": "BYTE_ENVELOPE_ONLY_NOT_ETL_AUTHENTICITY",
        "realEtlValidated": False,
        "trustedProviderProofRequired": True,
    }


def validate_raw_csv(
    path: Path, required_columns: list[str], minimum_bytes: int,
    maximum_bytes: int, label: str,
) -> tuple[dict[str, Any], list[dict[str, str]]]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise EtwProfileError(f"{label} is missing ({exc.__class__.__name__})") from None
    require(resolved.is_file() and not shipping.is_reparse_point(resolved),
            f"{label} must be a regular non-reparse file")
    before = resolved.stat()
    require(minimum_bytes <= before.st_size <= maximum_bytes,
            f"{label} byte size is outside the declared range")
    try:
        payload = resolved.read_bytes()
        after = resolved.stat()
        require((before.st_size, before.st_mtime_ns, before.st_ctime_ns,
                 before.st_dev, before.st_ino) ==
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns,
                 after.st_dev, after.st_ino)
                and len(payload) == before.st_size,
                f"{label} changed while read")
        text = payload.decode("utf-8-sig")
        reader = csv.DictReader(io.StringIO(text, newline=""), strict=True)
        require(reader.fieldnames == required_columns, f"{label} columns differ")
        rows = list(reader)
    except (OSError, UnicodeDecodeError, csv.Error) as exc:
        raise EtwProfileError(f"{label} is malformed ({exc.__class__.__name__})") from None
    require(bool(rows), f"{label} contains no exported events")
    require(all(set(row) == set(required_columns)
                and all(isinstance(row[key], str) and row[key] != ""
                        for key in required_columns)
                for row in rows), f"{label} contains incomplete rows")
    return ({
        "fileName": resolved.name,
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
    }, rows)


def parse_nonnegative_float(value: str, label: str) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise EtwProfileError(f"{label} is not numeric") from None
    require(math.isfinite(result) and result >= 0.0,
            f"{label} must be finite and non-negative")
    return result


def parse_positive_int(value: str, label: str) -> int:
    require(isinstance(value, str) and re.fullmatch(r"[1-9][0-9]*", value) is not None,
            f"{label} must be a canonical positive integer")
    result = int(value)
    require(result <= 2**31 - 1, f"{label} is outside the supported range")
    return result


def nearest_rank_p95(values: list[float]) -> float:
    require(bool(values), "cannot compute P95 for an empty event set")
    ordered = sorted(values)
    return ordered[math.ceil(0.95 * len(ordered)) - 1]


def derive_role_metrics(
    cpu_rows: list[dict[str, str]], gpu_rows: list[dict[str, str]],
    contract: dict[str, Any], *, required_duration_seconds: int,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    require(type(required_duration_seconds) is int and required_duration_seconds > 0,
            "declared duration is invalid")
    process_name = contract["requiredProcessName"]
    process_ids: set[int] = set()
    cpu_groups: dict[str, list[tuple[float, float, int]]] = {
        role: [] for role in ROLE_ORDER[1:]
    }
    for index, row in enumerate(cpu_rows):
        require(row["Process"] == process_name,
                f"raw CPU row {index} process differs")
        process_id = parse_positive_int(row["ProcessId"],
                                        f"raw CPU row {index} ProcessId")
        process_ids.add(process_id)
        role = row["Thread"]
        require(role in cpu_groups, f"raw CPU row {index} thread is not a required role")
        thread_id = parse_positive_int(row["ThreadId"],
                                       f"raw CPU row {index} ThreadId")
        timestamp = parse_nonnegative_float(
            row["StartTimeSeconds"], f"raw CPU row {index} StartTimeSeconds"
        )
        duration = parse_nonnegative_float(
            row["DurationMilliseconds"],
            f"raw CPU row {index} DurationMilliseconds",
        )
        cpu_groups[role].append((timestamp, duration, thread_id))
    gpu_events: list[tuple[float, float]] = []
    for index, row in enumerate(gpu_rows):
        require(row["Process"] == process_name,
                f"raw GPU row {index} process differs")
        process_id = parse_positive_int(row["ProcessId"],
                                        f"raw GPU row {index} ProcessId")
        process_ids.add(process_id)
        require(row["Engine"].strip() != "", f"raw GPU row {index} engine is empty")
        gpu_events.append((
            parse_nonnegative_float(
                row["StartTimeSeconds"], f"raw GPU row {index} StartTimeSeconds"
            ),
            parse_nonnegative_float(
                row["DurationMilliseconds"],
                f"raw GPU row {index} DurationMilliseconds",
            ),
        ))
    require(len(process_ids) == 1, "raw CPU/GPU exports do not share one process ID")
    process_id = next(iter(process_ids))

    def summarize(
        role: str, events: list[tuple[float, float]], thread_id: Optional[int]
    ) -> dict[str, Any]:
        require(len(events) >= contract["minimumEventsPerRole"],
                f"raw {role} export has insufficient events")
        timestamps = [event[0] for event in events]
        durations = [event[1] for event in events]
        first = min(timestamps)
        last = max(timestamps)
        observed = last - first
        require(observed >= contract["minimumObservedDurationSecondsPerRole"],
                f"raw {role} export has insufficient observed duration")
        require(observed + contract["declaredDurationCoverageToleranceSeconds"]
                >= required_duration_seconds,
                f"raw {role} export does not span the declared capture duration")
        return {
            "role": role,
            "processName": process_name,
            "processId": process_id,
            "threadName": role,
            "threadId": thread_id,
            "eventCount": len(events),
            "firstTimestampSeconds": first,
            "lastTimestampSeconds": last,
            "observedDurationSeconds": observed,
            "averageDurationMilliseconds": sum(durations) / len(durations),
            "p95DurationMilliseconds": nearest_rank_p95(durations),
            "maximumDurationMilliseconds": max(durations),
        }

    derived = [summarize("GPU", gpu_events, 0)]
    for role in ROLE_ORDER[1:]:
        events = cpu_groups[role]
        thread_ids = {event[2] for event in events}
        require(len(thread_ids) == 1,
                f"raw {role} export contains multiple thread IDs")
        derived.append(summarize(
            role, [(event[0], event[1]) for event in events],
            next(iter(thread_ids)),
        ))
    return {"name": process_name, "processId": process_id}, derived


def validate_normalized_export(
    path: Path,
    *,
    policy: dict[str, Any],
    candidate_id: str,
    run_id: str,
    etl_binding: dict[str, Any],
    raw_cpu_binding: dict[str, Any],
    raw_gpu_binding: dict[str, Any],
    expected_exporter_binding: dict[str, Any],
    derived_roles: list[dict[str, Any]],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    value = load_json(path, "normalized ETW role export")
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "generatedUtc", "sourceEtl", "rawArtifacts", "process", "roles",
        "normalizationTool", "claimBoundary",
    }, "normalized ETW role export")
    export = policy["exportContract"]
    require(value["schema"] == export["normalizedSchema"]
            and value["schemaVersion"] == 3 and value["session"] == 19,
            "normalized ETW export schema differs")
    require(value["candidateId"] == candidate_id and value["profileRunId"] == run_id,
            "normalized ETW export candidate/run differs")
    shipping.parse_utc(value["generatedUtc"], "normalized export generatedUtc")
    require(value["sourceEtl"] == etl_binding,
            "normalized export ETL binding differs")
    require(value["rawArtifacts"] == {
        "cpu": raw_cpu_binding, "gpu": raw_gpu_binding,
    }, "normalized export raw-artifact bindings differ")
    process = value["process"]
    require_keys(process, {"name", "processId"}, "normalized process")
    require(process["name"] == export["requiredProcessName"]
            and type(process["processId"]) is int and process["processId"] > 0,
            "normalized process identity differs")
    roles = value["roles"]
    require(isinstance(roles, list) and len(roles) == 4,
            "normalized export must contain exactly four roles")
    require([row.get("role") for row in roles] == export["requiredRoles"],
            "normalized role order/identity differs")
    expected_keys = set(export["requiredRoleKeys"])
    require(len(derived_roles) == len(roles)
            and [row.get("role") for row in derived_roles] == ROLE_ORDER,
            "independently derived role set differs")
    for index, row in enumerate(roles):
        require_keys(row, expected_keys, f"normalized role {index}")
        role = row["role"]
        expected_source = raw_gpu_binding if role == "GPU" else raw_cpu_binding
        require(row["sourceArtifactSha256"] == expected_source["sha256"],
                f"normalized role {role} source hash differs")
        require(isinstance(row["sourceTable"], str) and row["sourceTable"].strip(),
                f"normalized role {role} source table is missing")
        require(row["processName"] == process["name"]
                and row["processId"] == process["processId"],
                f"normalized role {role} process differs")
        require(row["threadName"] == role,
                f"normalized role {role} thread/engine name differs")
        if role == "GPU":
            require(row["threadId"] in (0, None),
                    "GPU normalized role threadId must be null or zero")
        else:
            require(type(row["threadId"]) is int and row["threadId"] > 0,
                    f"normalized role {role} threadId is invalid")
        require(type(row["eventCount"]) is int
                and row["eventCount"] >= export["minimumEventsPerRole"],
                f"normalized role {role} has insufficient events")
        numbers: dict[str, float] = {}
        for key in (
            "firstTimestampSeconds", "lastTimestampSeconds", "observedDurationSeconds",
            "averageDurationMilliseconds", "p95DurationMilliseconds",
            "maximumDurationMilliseconds",
        ):
            raw = row[key]
            require(type(raw) in (int, float) and not isinstance(raw, bool)
                    and math.isfinite(float(raw)) and float(raw) >= 0.0,
                    f"normalized role {role}.{key} is invalid")
            numbers[key] = float(raw)
        require(numbers["lastTimestampSeconds"] >= numbers["firstTimestampSeconds"]
                and numbers["observedDurationSeconds"] >=
                export["minimumObservedDurationSecondsPerRole"]
                and abs(
                    numbers["observedDurationSeconds"]
                    - (numbers["lastTimestampSeconds"] - numbers["firstTimestampSeconds"])
                ) <= 0.001,
                f"normalized role {role} duration window differs")
        require(numbers["averageDurationMilliseconds"]
                <= numbers["p95DurationMilliseconds"]
                <= numbers["maximumDurationMilliseconds"],
                f"normalized role {role} duration ordering differs")
        independently_derived = derived_roles[index]
        for key in (
            "processName", "processId", "threadName", "threadId", "eventCount",
        ):
            require(row[key] == independently_derived[key],
                    f"normalized role {role}.{key} differs from raw exports")
        for key in (
            "firstTimestampSeconds", "lastTimestampSeconds",
            "observedDurationSeconds", "averageDurationMilliseconds",
            "p95DurationMilliseconds", "maximumDurationMilliseconds",
        ):
            require(abs(float(row[key]) - float(independently_derived[key])) <= 1e-6,
                    f"normalized role {role}.{key} differs from raw exports")
    tool = value["normalizationTool"]
    require_keys(tool, {"fileName", "bytes", "sha256", "version"},
                 "normalization tool")
    require(tool == expected_exporter_binding
            and tool["fileName"].casefold() ==
            export["requiredExporterExecutableName"].casefold()
            and type(tool["bytes"]) is int and tool["bytes"] > 0
            and isinstance(tool["sha256"], str) and SHA_RE.fullmatch(tool["sha256"])
            and isinstance(tool["version"], str) and tool["version"].strip(),
            "normalized export tool binding differs")
    claims = value["claimBoundary"]
    require(claims == {
        "normalizedTechnicalArtifactOnly": True,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "normalized export claim boundary differs")
    return value, roles


def scan_regular_file_tokens(
    path: Path, *, required_tokens: list[str], required_pid: int,
    minimum_bytes: int, label: str,
) -> tuple[dict[str, Any], dict[str, bool], bool]:
    binding = stable_binding(path, label)
    require(binding["bytes"] >= minimum_bytes, f"{label} is too small")
    encoded = {token: token.encode("utf-8") for token in required_tokens}
    observed = {token: False for token in required_tokens}
    pid_pattern = re.compile(rb"(?<![0-9])" + str(required_pid).encode("ascii")
                             + rb"(?![0-9])")
    pid_observed = False
    overlap = b""
    try:
        with path.open("rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                searchable = overlap + chunk
                for token, raw in encoded.items():
                    if raw in searchable:
                        observed[token] = True
                if pid_pattern.search(searchable):
                    pid_observed = True
                overlap = searchable[-256:]
    except OSError as exc:
        raise EtwProfileError(
            f"{label} read failed ({exc.__class__.__name__})"
        ) from None
    return binding, observed, pid_observed


def validate_tracerpt_csv(
    path: Path, *, contract: dict[str, Any], game_pid: int,
) -> tuple[dict[str, Any], dict[str, bool], bool, int]:
    binding = stable_binding(path, "tracerpt parsed CSV")
    require(binding["bytes"] >= contract["minimumParsedCsvBytes"],
            "tracerpt parsed CSV is too small")
    before = path.stat()
    observed = {token: False for token in contract["requiredProviderTokens"]}
    pid_observed = False
    row_count = 0
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle, strict=True)
            require(reader.fieldnames is not None
                    and set(contract["requiredParsedCsvColumnsSubset"])
                    <= set(reader.fieldnames),
                    "tracerpt CSV header lacks required event columns")
            for row in reader:
                require(None not in row and all(value is not None for value in row.values()),
                        "tracerpt CSV contains malformed rows")
                row_count += 1
                searchable = " ".join(row.values())
                for token in observed:
                    if token in searchable:
                        observed[token] = True
                raw_pid = row["PID"].strip()
                try:
                    parsed_pid = int(raw_pid, 0)
                except ValueError:
                    parsed_pid = -1
                if parsed_pid == game_pid:
                    pid_observed = True
    except (OSError, UnicodeDecodeError, csv.Error) as exc:
        raise EtwProfileError(
            f"tracerpt CSV is malformed ({exc.__class__.__name__})"
        ) from None
    after = path.stat()
    require((before.st_size, before.st_mtime_ns, before.st_ctime_ns)
            == (after.st_size, after.st_mtime_ns, after.st_ctime_ns),
            "tracerpt CSV changed while parsed")
    require(row_count >= contract["minimumParsedEventRows"],
            "tracerpt CSV has insufficient parsed event rows")
    require(all(observed.values()),
            "tracerpt CSV lacks required ETW provider tokens")
    require(pid_observed,
            "tracerpt CSV has no event row for the captured game PID")
    return binding, observed, pid_observed, row_count


def validate_invocation(
    value: Any, *, expected_arguments: list[str], label: str,
) -> tuple[datetime, datetime]:
    require_keys(value, {"arguments", "startedUtc", "finishedUtc", "exitCode"},
                 label)
    require(value["arguments"] == expected_arguments,
            f"{label} arguments differ")
    require(type(value["exitCode"]) is int and value["exitCode"] == 0,
            f"{label} did not exit successfully")
    started = shipping.parse_utc(value["startedUtc"], f"{label} startedUtc")
    finished = shipping.parse_utc(value["finishedUtc"], f"{label} finishedUtc")
    require(started <= finished, f"{label} chronology differs")
    return started, finished


def validate_provider_proof(
    path: Path, *, policy: dict[str, Any], candidate_id: str, run_id: str,
    etl_path: Path, etl_envelope: dict[str, Any], tracerpt_path: Path,
    tracerpt_binding: dict[str, Any], tracerpt_csv_path: Path,
    tracerpt_summary_path: Path, game_pid: int, game_executable_sha256: str,
    wpr_stopped_utc: datetime,
) -> tuple[dict[str, Any], dict[str, Any]]:
    value = load_json(path, "tracerpt provider proof")
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "state", "generatedUtc", "tracerpt", "invocation", "inputEtl",
        "outputs", "gameProcess", "providerTokensObserved", "gamePidObserved",
        "parsedEventRows", "recordedBy", "claimBoundary",
    }, "tracerpt provider proof")
    require(value["schema"] == policy["schemas"]["providerProof"]
            and value["schemaVersion"] == 3 and value["session"] == 19
            and value["candidateId"] == candidate_id
            and value["profileRunId"] == run_id
            and value["state"] ==
            "PASS_TRACERPT_PARSED_ETL_PROVIDER_EVIDENCE",
            "tracerpt provider proof identity/state differs")
    generated = shipping.parse_utc(value["generatedUtc"],
                                   "provider proof generatedUtc")
    require(value["tracerpt"] == {
        "path": str(tracerpt_path), "binding": tracerpt_binding,
    }, "tracerpt provider proof tool binding differs")
    expected_arguments = [
        str(tracerpt_path), str(etl_path), "-o", str(tracerpt_csv_path),
        "-of", "CSV", "-summary", str(tracerpt_summary_path), "-y",
    ]
    invocation_started, invocation_finished = validate_invocation(
        value["invocation"], expected_arguments=expected_arguments,
        label="tracerpt invocation",
    )
    require(invocation_started >= wpr_stopped_utc
            and generated >= invocation_finished,
            "tracerpt provider proof predates WPR stop or invocation completion")
    require(value["inputEtl"] == etl_envelope,
            "tracerpt provider proof ETL binding differs")
    contract = policy["tracerptContract"]
    csv_binding, observed, pid_observed, parsed_rows = validate_tracerpt_csv(
        tracerpt_csv_path, contract=contract, game_pid=game_pid,
    )
    summary_binding, _, _ = scan_regular_file_tokens(
        tracerpt_summary_path,
        required_tokens=[],
        required_pid=game_pid, minimum_bytes=contract["minimumSummaryBytes"],
        label="tracerpt summary",
    )
    require(value["outputs"] == {
        "csv": csv_binding, "summary": summary_binding,
    } and value["providerTokensObserved"] == observed
            and value["gamePidObserved"] is True,
            "tracerpt provider proof output facts differ")
    require(value["parsedEventRows"] == parsed_rows,
            "tracerpt provider proof parsed-event count differs")
    require(value["gameProcess"] == {
        "processId": game_pid,
        "executableSha256": game_executable_sha256,
    }, "tracerpt provider proof game process differs")
    require(value["recordedBy"] == policy["bindings"]["runner"],
            "tracerpt provider proof recorder binding differs")
    require(value["claimBoundary"] == {
        "trustedWindowsEtwParserEvidence": True,
        "byteEnvelopeOnly": False,
        "realEtlValidated": True,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "tracerpt provider proof claim boundary differs")
    return value, stable_binding(path, "tracerpt provider proof")


def validate_wpa_invocation_record(
    path: Path, *, policy: dict[str, Any], candidate_id: str, run_id: str,
    etl_path: Path, etl_envelope: dict[str, Any], wpa_path: Path,
    wpa_binding: dict[str, Any], wpa_profile_path: Path,
    wpa_profile_binding: dict[str, Any], output_directory: Path,
    raw_cpu_binding: dict[str, Any], raw_gpu_binding: dict[str, Any],
    game_process: dict[str, Any], provider_generated_utc: datetime,
) -> tuple[dict[str, Any], dict[str, Any], datetime]:
    value = load_json(path, "WPAExporter invocation record")
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "state", "generatedUtc", "tool", "profile", "inputEtl", "invocation",
        "outputDirectory", "outputInventory", "rawArtifacts", "gameProcess",
        "recordedBy", "claimBoundary",
    }, "WPAExporter invocation record")
    require(value["schema"] == policy["schemas"]["wpaInvocation"]
            and value["schemaVersion"] == 3 and value["session"] == 19
            and value["candidateId"] == candidate_id
            and value["profileRunId"] == run_id
            and value["state"] == "PASS_WPAEXPORTER_INVOCATION_RAW_OUTPUTS_BOUND",
            "WPAExporter invocation record identity/state differs")
    generated = shipping.parse_utc(value["generatedUtc"],
                                   "WPA invocation generatedUtc")
    require(value["tool"] == {"path": str(wpa_path), "binding": wpa_binding}
            and value["profile"] == {
                "path": str(wpa_profile_path), "binding": wpa_profile_binding,
            } and value["inputEtl"] == etl_envelope,
            "WPAExporter invocation immutable inputs differ")
    expected_arguments = [
        str(wpa_path), "-i", str(etl_path), "-profile", str(wpa_profile_path),
        "-delimiter", ",", "-outputfolder", str(output_directory),
    ]
    started, finished = validate_invocation(
        value["invocation"], expected_arguments=expected_arguments,
        label="WPAExporter invocation",
    )
    require(started >= provider_generated_utc and generated >= finished,
            "WPAExporter invocation predates trusted provider proof")
    require(value["outputDirectory"] == str(output_directory),
            "WPAExporter output directory differs")
    inventory = value["outputInventory"]
    require(isinstance(inventory, list) and inventory == [
        raw_cpu_binding, raw_gpu_binding,
    ], "WPAExporter output inventory differs from exact raw outputs")
    require(all(isinstance(item, dict)
                and set(item) == {"fileName", "bytes", "sha256"}
                for item in inventory),
            "WPAExporter output inventory binding shape differs")
    require(len({item["fileName"].casefold() for item in inventory}) == len(inventory),
            "WPAExporter output inventory contains duplicate names")
    require(value["rawArtifacts"] == {
        "cpu": raw_cpu_binding, "gpu": raw_gpu_binding,
    }, "WPAExporter invocation raw-artifact bindings differ")
    require(value["gameProcess"] == game_process,
            "WPAExporter invocation game process/window differs")
    require(value["recordedBy"] == policy["bindings"]["runner"],
            "WPAExporter invocation recorder binding differs")
    require(value["claimBoundary"] == {
        "actualExporterInvocationBound": True,
        "csvOnlyProvenanceSufficient": False,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "WPAExporter invocation claim boundary differs")
    return value, stable_binding(path, "WPAExporter invocation record"), generated


def validate_archive_snapshot_artifact(
    path: Path, *, policy: dict[str, Any], candidate_id: str,
    run_id: str, phase: str,
) -> tuple[dict[str, Any], datetime]:
    value = load_json(path, f"{phase} archive snapshot")
    require_keys(value, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "phase", "capturedUtc", "archive", "claimBoundary",
    }, f"{phase} archive snapshot")
    expected_archive = {
        key: policy["candidateArchive"][key]
        for key in ("fileCount", "bytes", "inventorySha256", "launcher",
                    "shippingExecutable")
    }
    require(value["schema"] == policy["schemas"]["archiveSnapshot"]
            and value["schemaVersion"] == 3 and value["session"] == 19
            and value["candidateId"] == candidate_id
            and value["profileRunId"] == run_id and value["phase"] == phase
            and value["archive"] == expected_archive,
            f"{phase} archive snapshot identity/binding differs")
    captured = shipping.parse_utc(value["capturedUtc"],
                                  f"{phase} archive capturedUtc")
    require(value["claimBoundary"] == {
        "archiveSnapshotOnly": True,
        "technicalEvidenceAccepted": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, f"{phase} archive snapshot claim boundary differs")
    return stable_binding(path, f"{phase} archive snapshot"), captured


def validate_completed_run(
    *, declaration_path: Path, run_root: Path, archive: Path = DEFAULT_ARCHIVE,
    external_root: Path = DEFAULT_EXTERNAL_ROOT,
) -> dict[str, Any]:
    """Pure, read-only validation of one exact completed v3 run."""
    import generate_dg_session19_sustained_etw_profile_declaration as declaration_tool

    require(POLICY_PATH.resolve(strict=True) ==
            (ROOT / POLICY_PATH.relative_to(ROOT)).resolve(strict=True),
            "ETW v3 policy path differs")
    policy = load_json(POLICY_PATH, "ETW v3 policy")
    validate_policy(policy)
    policy_binding = stable_binding(POLICY_PATH, "ETW v3 policy")
    archive, receipt, performance_run_root = validate_baseline(
        policy, archive, external_root
    )
    external = declaration_tool.validate_external_root(external_root, archive)
    declaration_resolved = declaration_path.resolve(strict=True)
    declaration = load_json(declaration_resolved, "ETW v3 declaration")
    declaration_tool.validate_declaration(declaration, policy, policy_binding)
    require(declaration_resolved == declaration_tool.validate_destination_chain(
        external, declaration["externalLayout"]["declarationRelativePath"]
    ), "declaration is not at its exact policy-bound path")
    expected_run = declaration_tool.validate_destination_chain(
        external, declaration["externalLayout"]["runRelativePath"]
    )
    run_resolved = run_root.resolve(strict=True)
    require(run_resolved == expected_run and run_resolved.is_dir()
            and not shipping.is_reparse_point(run_resolved),
            "completed run root is not the exact declaration-bound directory")
    artifacts = policy["requiredArtifacts"]
    paths = {key: run_resolved / name for key, name in artifacts.items()}
    for key, path in paths.items():
        require(path.is_file() and not shipping.is_reparse_point(path),
                f"completed run artifact is missing/invalid: {artifacts[key]}")

    tooling = declaration["tooling"]
    require(tooling["wpaExporter"]["available"] is True
            and tooling["wpaProfile"]["available"] is True,
            "completed run requires predeclared WPAExporter and WPA profile")
    wpr_path = Path(tooling["wpr"]["path"]).resolve(strict=True)
    tracerpt_path = Path(tooling["tracerpt"]["path"]).resolve(strict=True)
    wpa_path = Path(tooling["wpaExporter"]["path"]).resolve(strict=True)
    wpa_profile_path = Path(tooling["wpaProfile"]["path"]).resolve(strict=True)
    require(declaration_tool.versioned_tool_binding(wpr_path) ==
            tooling["wpr"]["binding"]
            and declaration_tool.versioned_tool_binding(tracerpt_path) ==
            tooling["tracerpt"]["binding"]
            and declaration_tool.versioned_tool_binding(wpa_path) ==
            tooling["wpaExporter"]["binding"]
            and stable_binding(wpa_profile_path, "WPA profile") ==
            tooling["wpaProfile"]["binding"],
            "completed run live tooling/profile differs from declaration")

    etl_envelope = validate_etl_envelope(paths["etl"], policy["etlContract"])
    require(etl_envelope["realEtlValidated"] is False,
            "ETL byte envelope may never independently grant real ETL status")
    capture = load_json(paths["captureRecord"], "ETW capture record")
    pre_binding, pre_captured = validate_archive_snapshot_artifact(
        paths["archivePre"], policy=policy, candidate_id=CANDIDATE_ID,
        run_id=declaration["profileRunId"], phase="PRE",
    )
    post_binding, post_captured = validate_archive_snapshot_artifact(
        paths["archivePost"], policy=policy, candidate_id=CANDIDATE_ID,
        run_id=declaration["profileRunId"], phase="POST",
    )
    require_keys(capture, {
        "schema", "schemaVersion", "session", "candidateId", "profileRunId",
        "state", "declaration", "requestedDurationSeconds", "wpr",
        "wprInvocations", "gameProcess", "archivePre", "archivePost",
        "etlEnvelope", "providerProof", "claimBoundary",
    }, "ETW capture record")
    require(capture["schema"] == policy["schemas"]["captureRecord"]
            and capture["schemaVersion"] == 3 and capture["session"] == 19
            and capture["candidateId"] == CANDIDATE_ID
            and capture["profileRunId"] == declaration["profileRunId"]
            and capture["state"] == "CAPTURE_COMPLETE_PROVIDER_PROOF_BOUND"
            and capture["declaration"] ==
            stable_binding(declaration_resolved, "ETW declaration")
            and capture["requestedDurationSeconds"] ==
            declaration["requestedDurationSeconds"]
            and capture["wpr"] == tooling["wpr"]["binding"]
            and capture["etlEnvelope"] == etl_envelope
            and capture["archivePre"] == pre_binding
            and capture["archivePost"] == post_binding,
            "ETW capture record identity/bindings differ")
    require(capture["claimBoundary"] == {
        "captureArtifactOnly": True,
        "technicalEvidenceAccepted": False,
        "sustainedPerformanceCertified": False,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }, "ETW capture record claim boundary differs")
    game = capture["gameProcess"]
    require_keys(game, {
        "processId", "creationUtc", "finishedUtc", "exitCode",
        "forcedTermination", "executable", "arguments",
    }, "captured game process")
    require(type(game["processId"]) is int and game["processId"] > 0
            and type(game["exitCode"]) is int
            and type(game["forcedTermination"]) is bool,
            "captured game process facts differ")
    executable_path = archive / shipping.EXECUTABLE_RELATIVE
    require(game["executable"] == {
        "path": str(executable_path),
        **policy["candidateArchive"]["shippingExecutable"],
    }, "captured game executable differs")
    expected_game_args = [
        "-Course=PineRidge", "-Hole=1", "-ResX=1920", "-ResY=1080",
        "-ForceRes", "-dx12", "-NoLoadExistingSave", "-DGNoProfileWrites",
        f"-UserDir={run_resolved / 'UserData'}",
    ]
    require(game["arguments"] == expected_game_args,
            "captured game arguments differ")
    wpr_working = run_resolved / "WprWorking"
    expected_start = [
        str(wpr_path), "-start", "CPU.light", "-start", "GPU.light",
        "-filemode", "-recordtempto", str(wpr_working),
    ]
    expected_stop = [str(wpr_path), "-stop", str(paths["etl"])]
    require_keys(capture["wprInvocations"], {"start", "stop"},
                 "WPR invocation records")
    wpr_start, wpr_start_finished = validate_invocation(
        capture["wprInvocations"]["start"], expected_arguments=expected_start,
        label="WPR start invocation",
    )
    wpr_stop, wpr_stop_finished = validate_invocation(
        capture["wprInvocations"]["stop"], expected_arguments=expected_stop,
        label="WPR stop invocation",
    )
    game_created = shipping.parse_utc(game["creationUtc"], "game creationUtc")
    game_finished = shipping.parse_utc(game["finishedUtc"], "game finishedUtc")
    declared = shipping.parse_utc(declaration["declaredUtc"], "declaredUtc")
    tolerance = policy["exportContract"]["declaredDurationCoverageToleranceSeconds"]
    require(declared <= wpr_start <= wpr_start_finished <= game_created
            < game_finished <= wpr_stop <= wpr_stop_finished,
            "WPR/game process capture chronology differs")
    require(pre_captured <= wpr_start and post_captured >= game_finished,
            "archive snapshot chronology differs from capture window")
    require((game_finished - game_created).total_seconds() + tolerance
            >= declaration["requestedDurationSeconds"],
            "captured game process does not span declared duration")

    provider, provider_binding = validate_provider_proof(
        paths["providerProof"], policy=policy, candidate_id=CANDIDATE_ID,
        run_id=declaration["profileRunId"], etl_path=paths["etl"],
        etl_envelope=etl_envelope, tracerpt_path=tracerpt_path,
        tracerpt_binding=tooling["tracerpt"]["binding"],
        tracerpt_csv_path=paths["tracerptCsv"],
        tracerpt_summary_path=paths["tracerptSummary"],
        game_pid=game["processId"],
        game_executable_sha256=game["executable"]["sha256"],
        wpr_stopped_utc=wpr_stop_finished,
    )
    require(capture["providerProof"] == provider_binding,
            "capture record provider-proof binding differs")

    raw_cpu_binding, cpu_rows = validate_raw_csv(
        paths["rawCpu"], policy["exportContract"]["requiredRawCpuColumns"],
        policy["exportContract"]["minimumRawArtifactBytes"],
        policy["exportContract"]["maximumRawArtifactBytes"], "raw CPU export",
    )
    raw_gpu_binding, gpu_rows = validate_raw_csv(
        paths["rawGpu"], policy["exportContract"]["requiredRawGpuColumns"],
        policy["exportContract"]["minimumRawArtifactBytes"],
        policy["exportContract"]["maximumRawArtifactBytes"], "raw GPU export",
    )
    process, derived_roles = derive_role_metrics(
        cpu_rows, gpu_rows, policy["exportContract"],
        required_duration_seconds=declaration["requestedDurationSeconds"],
    )
    require(process["processId"] == game["processId"],
            "raw CPU/GPU process ID differs from captured game PID")
    game_export_binding = {
        "processId": game["processId"],
        "creationUtc": game["creationUtc"],
        "finishedUtc": game["finishedUtc"],
        "executableSha256": game["executable"]["sha256"],
    }
    _, wpa_invocation_binding, wpa_generated = validate_wpa_invocation_record(
        paths["wpaInvocation"], policy=policy, candidate_id=CANDIDATE_ID,
        run_id=declaration["profileRunId"], etl_path=paths["etl"],
        etl_envelope=etl_envelope, wpa_path=wpa_path,
        wpa_binding=tooling["wpaExporter"]["binding"],
        wpa_profile_path=wpa_profile_path,
        wpa_profile_binding=tooling["wpaProfile"]["binding"],
        output_directory=run_resolved, raw_cpu_binding=raw_cpu_binding,
        raw_gpu_binding=raw_gpu_binding, game_process=game_export_binding,
        provider_generated_utc=shipping.parse_utc(
            provider["generatedUtc"], "provider generatedUtc"
        ),
    )
    normalized, roles = validate_normalized_export(
        paths["normalizedRoles"], policy=policy, candidate_id=CANDIDATE_ID,
        run_id=declaration["profileRunId"], etl_binding=etl_envelope,
        raw_cpu_binding=raw_cpu_binding, raw_gpu_binding=raw_gpu_binding,
        expected_exporter_binding=tooling["wpaExporter"]["binding"],
        derived_roles=derived_roles,
    )
    normalized_generated = shipping.parse_utc(
        normalized["generatedUtc"], "normalized generatedUtc"
    )
    require(normalized_generated >= wpa_generated,
            "normalized export predates actual WPAExporter invocation")
    normalized_binding = stable_binding(paths["normalizedRoles"],
                                        "normalized role export")
    export_record = load_json(paths["exportRecord"], "ETW export record")
    require(export_record == {
        "schema": policy["schemas"]["exportRecord"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": CANDIDATE_ID,
        "profileRunId": declaration["profileRunId"],
        "state": "PASS_ACTUAL_WPA_INVOCATION_AND_NORMALIZED_FOUR_ROLE_EXPORT_BOUND",
        "validatedUtc": export_record.get("validatedUtc"),
        "providerProof": provider_binding,
        "wpaInvocation": wpa_invocation_binding,
        "etlEnvelope": etl_envelope,
        "rawArtifacts": {"cpu": raw_cpu_binding, "gpu": raw_gpu_binding},
        "normalizedRoles": normalized_binding,
        "gameProcess": game_export_binding,
        "claimBoundary": {
            "actualExporterInvocationBound": True,
            "technicalEvidenceAccepted": False,
            "sustainedPerformanceCertified": False,
            "humanPerformanceAcceptance": False,
            "productOwnerApproval": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }, "ETW export record differs")
    export_validated = shipping.parse_utc(export_record["validatedUtc"],
                                          "export validatedUtc")
    require(export_validated >= normalized_generated,
            "export validation predates normalization")
    export_binding = stable_binding(paths["exportRecord"], "ETW export record")
    manifest = load_json(paths["runManifest"], "ETW run manifest")
    manifest_expected = {
        "schema": policy["schemas"]["runManifest"],
        "schemaVersion": 3,
        "session": 19,
        "candidateId": CANDIDATE_ID,
        "profileRunId": declaration["profileRunId"],
        "state": policy["technicalPassState"],
        "generatedUtc": manifest.get("generatedUtc"),
        "declaration": stable_binding(declaration_resolved, "ETW declaration"),
        "candidateArchive": copy.deepcopy(policy["candidateArchive"]),
        "threeHolePerformanceReceipt": copy.deepcopy(
            policy["bindings"]["shippingPerformanceReceipt"]
        ),
        "artifacts": {
            "archivePre": stable_binding(paths["archivePre"], "PRE archive snapshot"),
            "archivePost": stable_binding(paths["archivePost"], "POST archive snapshot"),
            "captureRecord": stable_binding(paths["captureRecord"], "capture record"),
            "etlEnvelope": etl_envelope,
            "tracerptCsv": stable_binding(paths["tracerptCsv"], "tracerpt CSV"),
            "tracerptSummary": stable_binding(paths["tracerptSummary"], "tracerpt summary"),
            "providerProof": provider_binding,
            "wpaInvocation": wpa_invocation_binding,
            "rawCpu": raw_cpu_binding,
            "rawGpu": raw_gpu_binding,
            "normalizedRoles": normalized_binding,
            "exportRecord": export_binding,
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
    require(manifest == manifest_expected, "ETW completed-run manifest differs")
    manifest_generated = shipping.parse_utc(manifest["generatedUtc"],
                                            "manifest generatedUtc")
    require(manifest_generated >= export_validated,
            "run manifest predates export validation")
    require(manifest_generated <= datetime.now(timezone.utc) +
            timedelta(seconds=policy["chronologyContract"][
                "futureTimestampToleranceSeconds"
            ]), "run manifest timestamp is implausibly in the future")
    return {
        "schema": AUDIT_SCHEMA,
        "schemaVersion": 3,
        "session": 19,
        "candidateId": CANDIDATE_ID,
        "profileRunId": declaration["profileRunId"],
        "state": PASS_STATE,
        "baselineValid": True,
        "threeHolePerformanceRunId": receipt["runId"],
        "threeHolePerformanceRunRoot": str(performance_run_root),
        "etlByteEnvelopeValidated": True,
        "trustedTracerptProviderProofValidated": True,
        "realEtlValidated": True,
        "actualWpaExporterInvocationValidated": True,
        "normalizedFourRoleExportValidated": True,
        "technicalEvidenceAccepted": True,
        "humanPerformanceAcceptance": False,
        "productOwnerApproval": False,
        "releaseApproval": False,
        "releaseReady": False,
    }


def run_self_test() -> tuple[int, dict[str, Any]]:
    checks = 0
    failures: list[dict[str, str]] = []

    def check(name: str, condition: bool) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            failures.append({"case": name, "error": "assertion failed"})

    try:
        policy = load_json(POLICY_PATH, "self-test ETW policy")
        validate_policy(policy)
        check("policy-baseline", True)
        for name, mutate in (
            ("candidate-drift", lambda item: item.__setitem__(
                "candidateId", "S19_WindowsShipping_20990101T000000Z_aaaaaaaaaaaa"
            )),
            ("recovery-token-drift", lambda item: item["candidateArchive"].__setitem__(
                "archiveRecoveryLocationToken", "DGTOUR_PACKAGES/other/Windows"
            )),
            ("tracerpt-tool-drift", lambda item: item["hostToolIdentities"]["tracerpt"].__setitem__(
                "sha256", "0" * 64
            )),
            ("trace-overclaim", lambda item: item["shippingTraceBoundary"].__setitem__("ordinaryShippingFullUnrealTraceCompiled", True)),
            ("duration-weakened", lambda item: item["predeclaration"].__setitem__("minimumDurationSeconds", 60)),
            ("gpu-profile-removed", lambda item: item["wprContract"]["requiredProfiles"].pop()),
            ("etl-minimum-weakened", lambda item: item["etlContract"].__setitem__("minimumBytes", 128)),
            ("role-removed", lambda item: item["exportContract"]["requiredRoles"].pop()),
            ("release-overclaim", lambda item: item["claimBoundary"].__setitem__("releaseReady", True)),
        ):
            candidate = copy.deepcopy(policy)
            mutate(candidate)
            try:
                validate_policy(candidate, check_files=False)
            except EtwProfileError:
                rejected = True
            else:
                rejected = False
            check(name, rejected)

        with tempfile.TemporaryDirectory(prefix="dg-s19-etw-validator-") as temporary:
            root = Path(temporary)
            etl = root / policy["etlContract"]["artifactName"]
            pattern = bytes(range(256))
            etl.write_bytes(pattern * ((1024 * 1024 // len(pattern)) + 1))
            etl_binding = validate_etl_envelope(etl, policy["etlContract"])
            check("synthetic-binary-etl", etl_binding["bytes"] >= 1024 * 1024)
            check("high-entropy-envelope-never-real-etl",
                  etl_binding["realEtlValidated"] is False
                  and etl_binding["validationScope"] ==
                  "BYTE_ENVELOPE_ONLY_NOT_ETL_AUTHENTICITY"
                  and etl_binding["trustedProviderProofRequired"] is True)
            placeholder = root / "placeholder.etl"
            placeholder.write_bytes(b"{" + b"a" * (1024 * 1024))
            try:
                validate_etl_envelope(placeholder, {**policy["etlContract"], "artifactName": placeholder.name})
            except EtwProfileError:
                placeholder_rejected = True
            else:
                placeholder_rejected = False
            check("text-etl-placeholder-rejected", placeholder_rejected)

            raw_cpu = root / policy["exportContract"]["rawCpuArtifactName"]
            raw_gpu = root / policy["exportContract"]["rawGpuArtifactName"]
            with raw_cpu.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle, lineterminator="\n")
                writer.writerow(policy["exportContract"]["requiredRawCpuColumns"])
                for role_index, role in enumerate(ROLE_ORDER[1:], start=1):
                    for event_index in range(80):
                        writer.writerow([
                            "DiscGolfTour-Win64-Shipping.exe", "42", role,
                            str(100 + role_index), str(event_index),
                            str(1.0 + role_index / 10.0),
                        ])
            with raw_gpu.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle, lineterminator="\n")
                writer.writerow(policy["exportContract"]["requiredRawGpuColumns"])
                for event_index in range(80):
                    writer.writerow([
                        "DiscGolfTour-Win64-Shipping.exe", "42", str(event_index),
                        "1.5", "3D",
                    ])
            cpu_binding, cpu_rows = validate_raw_csv(
                raw_cpu, policy["exportContract"]["requiredRawCpuColumns"],
                policy["exportContract"]["minimumRawArtifactBytes"],
                policy["exportContract"]["maximumRawArtifactBytes"], "raw CPU",
            )
            gpu_binding, gpu_rows = validate_raw_csv(
                raw_gpu, policy["exportContract"]["requiredRawGpuColumns"],
                policy["exportContract"]["minimumRawArtifactBytes"],
                policy["exportContract"]["maximumRawArtifactBytes"], "raw GPU",
            )
            process, derived_roles = derive_role_metrics(
                cpu_rows, gpu_rows, policy["exportContract"],
                required_duration_seconds=60,
            )
            check("raw-four-role-derivation", len(derived_roles) == 4)
            try:
                derive_role_metrics(
                    cpu_rows, gpu_rows, policy["exportContract"],
                    required_duration_seconds=900,
                )
            except EtwProfileError:
                short_role_span_rejected = True
            else:
                short_role_span_rejected = False
            check("sixty-second-role-span-rejected-for-900-second-run",
                  short_role_span_rejected)
            run_id = "11111111-2222-4333-8444-555555555555"
            tracerpt_csv = root / policy["requiredArtifacts"]["tracerptCsv"]
            tracerpt_summary = root / policy["requiredArtifacts"]["tracerptSummary"]
            with tracerpt_csv.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle, lineterminator="\n")
                writer.writerow(["Event Name", "PID", "TID", "Clock-Time",
                                 "User Data"])
                for index in range(1000):
                    writer.writerow([
                        "DxgKrnl" if index % 2 == 0 else "Process",
                        "42", "100", f"2026-01-01T00:00:{index % 60:02d}Z",
                        "DxgKrnl Process",
                    ])
            tracerpt_summary.write_text(
                "DxgKrnl Process captured PID 42\n", encoding="utf-8"
            )
            tracerpt_path = root / "tracerpt.exe"
            tracerpt_path.write_bytes(b"tool")
            tracerpt_binding = {
                **stable_binding(tracerpt_path, "self-test tracerpt"),
                "version": "1.0",
            }
            provider_path = root / policy["requiredArtifacts"]["providerProof"]
            provider = {
                "schema": policy["schemas"]["providerProof"],
                "schemaVersion": 3, "session": 19,
                "candidateId": CANDIDATE_ID, "profileRunId": run_id,
                "state": "PASS_TRACERPT_PARSED_ETL_PROVIDER_EVIDENCE",
                "generatedUtc": "2026-01-01T00:00:03.000Z",
                "tracerpt": {"path": str(tracerpt_path),
                              "binding": tracerpt_binding},
                "invocation": {
                    "arguments": [
                        str(tracerpt_path), str(etl), "-o", str(tracerpt_csv),
                        "-of", "CSV", "-summary", str(tracerpt_summary), "-y",
                    ],
                    "startedUtc": "2026-01-01T00:00:01.000Z",
                    "finishedUtc": "2026-01-01T00:00:02.000Z",
                    "exitCode": 0,
                },
                "inputEtl": etl_binding,
                "outputs": {
                    "csv": stable_binding(tracerpt_csv, "self-test tracerpt CSV"),
                    "summary": stable_binding(
                        tracerpt_summary, "self-test tracerpt summary"
                    ),
                },
                "gameProcess": {"processId": 42, "executableSha256": "F" * 64},
                "providerTokensObserved": {"DxgKrnl": True, "Process": True},
                "gamePidObserved": True,
                "parsedEventRows": 1000,
                "recordedBy": policy["bindings"]["runner"],
                "claimBoundary": {
                    "trustedWindowsEtwParserEvidence": True,
                    "byteEnvelopeOnly": False, "realEtlValidated": True,
                    "technicalEvidenceAccepted": False,
                    "sustainedPerformanceCertified": False,
                    "humanPerformanceAcceptance": False,
                    "productOwnerApproval": False, "releaseApproval": False,
                    "releaseReady": False,
                },
            }
            shipping.write_exclusive_json(provider_path, provider)
            validate_provider_proof(
                provider_path, policy=policy, candidate_id=CANDIDATE_ID,
                run_id=run_id, etl_path=etl, etl_envelope=etl_binding,
                tracerpt_path=tracerpt_path, tracerpt_binding=tracerpt_binding,
                tracerpt_csv_path=tracerpt_csv,
                tracerpt_summary_path=tracerpt_summary, game_pid=42,
                game_executable_sha256="F" * 64,
                wpr_stopped_utc=shipping.parse_utc(
                    "2026-01-01T00:00:00.000Z", "self-test WPR stop"
                ),
            )
            check("trusted-tracerpt-provider-proof-baseline", True)
            provider_bad = copy.deepcopy(provider)
            provider_bad["gamePidObserved"] = False
            provider_bad_path = root / "ProviderBad.json"
            shipping.write_exclusive_json(provider_bad_path, provider_bad)
            try:
                validate_provider_proof(
                    provider_bad_path, policy=policy, candidate_id=CANDIDATE_ID,
                    run_id=run_id, etl_path=etl, etl_envelope=etl_binding,
                    tracerpt_path=tracerpt_path,
                    tracerpt_binding=tracerpt_binding,
                    tracerpt_csv_path=tracerpt_csv,
                    tracerpt_summary_path=tracerpt_summary, game_pid=42,
                    game_executable_sha256="F" * 64,
                    wpr_stopped_utc=shipping.parse_utc(
                        "2026-01-01T00:00:00.000Z", "self-test WPR stop"
                    ),
                )
            except EtwProfileError:
                false_pid_claim_rejected = True
            else:
                false_pid_claim_rejected = False
            check("provider-proof-false-pid-claim-rejected",
                  false_pid_claim_rejected)
            provider_failed = copy.deepcopy(provider)
            provider_failed["invocation"]["exitCode"] = 1
            provider_failed_path = root / "ProviderFailed.json"
            shipping.write_exclusive_json(provider_failed_path, provider_failed)
            try:
                validate_provider_proof(
                    provider_failed_path, policy=policy,
                    candidate_id=CANDIDATE_ID, run_id=run_id, etl_path=etl,
                    etl_envelope=etl_binding, tracerpt_path=tracerpt_path,
                    tracerpt_binding=tracerpt_binding,
                    tracerpt_csv_path=tracerpt_csv,
                    tracerpt_summary_path=tracerpt_summary, game_pid=42,
                    game_executable_sha256="F" * 64,
                    wpr_stopped_utc=shipping.parse_utc(
                        "2026-01-01T00:00:00.000Z", "self-test WPR stop"
                    ),
                )
            except EtwProfileError:
                failed_tracerpt_rejected = True
            else:
                failed_tracerpt_rejected = False
            check("failed-tracerpt-invocation-rejected", failed_tracerpt_rejected)
            wpa_path = root / "WPAExporter.exe"
            wpa_path.write_bytes(b"exporter")
            wpa_binding = {
                **stable_binding(wpa_path, "self-test WPAExporter"),
                "version": "1.0",
            }
            profile_path = root / "DGTour.wpaProfile"
            profile_path.write_text("<Profile />", encoding="utf-8")
            profile_binding = stable_binding(profile_path, "self-test WPA profile")
            game_binding = {
                "processId": 42,
                "creationUtc": "2026-01-01T00:00:00.000Z",
                "finishedUtc": "2026-01-01T00:01:19.000Z",
                "executableSha256": "F" * 64,
            }
            wpa_invocation_path = root / policy["requiredArtifacts"]["wpaInvocation"]
            wpa_record = {
                "schema": policy["schemas"]["wpaInvocation"],
                "schemaVersion": 3, "session": 19,
                "candidateId": CANDIDATE_ID, "profileRunId": run_id,
                "state": "PASS_WPAEXPORTER_INVOCATION_RAW_OUTPUTS_BOUND",
                "generatedUtc": "2026-01-01T00:00:06.000Z",
                "tool": {"path": str(wpa_path), "binding": wpa_binding},
                "profile": {"path": str(profile_path),
                            "binding": profile_binding},
                "inputEtl": etl_binding,
                "invocation": {
                    "arguments": [
                        str(wpa_path), "-i", str(etl), "-profile",
                        str(profile_path), "-delimiter", ",", "-outputfolder",
                        str(root),
                    ],
                    "startedUtc": "2026-01-01T00:00:04.000Z",
                    "finishedUtc": "2026-01-01T00:00:05.000Z",
                    "exitCode": 0,
                },
                "outputDirectory": str(root),
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
                    "productOwnerApproval": False, "releaseApproval": False,
                    "releaseReady": False,
                },
            }
            shipping.write_exclusive_json(wpa_invocation_path, wpa_record)
            validate_wpa_invocation_record(
                wpa_invocation_path, policy=policy, candidate_id=CANDIDATE_ID,
                run_id=run_id, etl_path=etl, etl_envelope=etl_binding,
                wpa_path=wpa_path, wpa_binding=wpa_binding,
                wpa_profile_path=profile_path,
                wpa_profile_binding=profile_binding, output_directory=root,
                raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                game_process=game_binding,
                provider_generated_utc=shipping.parse_utc(
                    provider["generatedUtc"], "self-test provider generatedUtc"
                ),
            )
            check("actual-wpa-invocation-provenance-baseline", True)
            wpa_bad = copy.deepcopy(wpa_record)
            wpa_bad["invocation"]["exitCode"] = 1
            wpa_bad_path = root / "WpaBad.json"
            shipping.write_exclusive_json(wpa_bad_path, wpa_bad)
            try:
                validate_wpa_invocation_record(
                    wpa_bad_path, policy=policy, candidate_id=CANDIDATE_ID,
                    run_id=run_id, etl_path=etl, etl_envelope=etl_binding,
                    wpa_path=wpa_path, wpa_binding=wpa_binding,
                    wpa_profile_path=profile_path,
                    wpa_profile_binding=profile_binding, output_directory=root,
                    raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                    game_process=game_binding,
                    provider_generated_utc=shipping.parse_utc(
                        provider["generatedUtc"], "self-test provider generatedUtc"
                    ),
                )
            except EtwProfileError:
                failed_exporter_rejected = True
            else:
                failed_exporter_rejected = False
            check("failed-wpa-invocation-rejected", failed_exporter_rejected)
            wpa_wrong_game = copy.deepcopy(wpa_record)
            wpa_wrong_game["gameProcess"]["processId"] = 43
            wpa_wrong_game_path = root / "WpaWrongGame.json"
            shipping.write_exclusive_json(wpa_wrong_game_path, wpa_wrong_game)
            try:
                validate_wpa_invocation_record(
                    wpa_wrong_game_path, policy=policy,
                    candidate_id=CANDIDATE_ID, run_id=run_id, etl_path=etl,
                    etl_envelope=etl_binding, wpa_path=wpa_path,
                    wpa_binding=wpa_binding, wpa_profile_path=profile_path,
                    wpa_profile_binding=profile_binding, output_directory=root,
                    raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                    game_process=game_binding,
                    provider_generated_utc=shipping.parse_utc(
                        provider["generatedUtc"], "self-test provider generatedUtc"
                    ),
                )
            except EtwProfileError:
                wrong_game_binding_rejected = True
            else:
                wrong_game_binding_rejected = False
            check("wpa-game-process-binding-drift-rejected",
                  wrong_game_binding_rejected)
            roles = []
            for index, derived in enumerate(derived_roles):
                role = derived["role"]
                roles.append({**derived,
                    "sourceTable": "GPU Usage" if role == "GPU" else "CPU Usage Precise",
                    "sourceArtifactSha256": (
                        gpu_binding["sha256"] if role == "GPU" else cpu_binding["sha256"]
                    ),
                })
            exporter_binding = {
                "fileName": "WPAExporter.exe", "bytes": 1,
                "sha256": "A" * 64, "version": "1.0",
            }
            normalized = {
                "schema": NORMALIZED_SCHEMA, "schemaVersion": 3, "session": 19,
                "candidateId": CANDIDATE_ID, "profileRunId": run_id,
                "generatedUtc": "2099-01-01T00:00:00.000Z",
                "sourceEtl": etl_binding,
                "rawArtifacts": {"cpu": cpu_binding, "gpu": gpu_binding},
                "process": process,
                "roles": roles,
                "normalizationTool": exporter_binding,
                "claimBoundary": {
                    "normalizedTechnicalArtifactOnly": True,
                    "technicalEvidenceAccepted": False,
                    "sustainedPerformanceCertified": False,
                    "humanPerformanceAcceptance": False,
                    "productOwnerApproval": False,
                    "releaseApproval": False,
                    "releaseReady": False,
                },
            }
            normalized_path = root / policy["exportContract"]["normalizedArtifactName"]
            shipping.write_exclusive_json(normalized_path, normalized)
            validate_normalized_export(
                normalized_path, policy=policy, candidate_id=CANDIDATE_ID,
                run_id=run_id, etl_binding=etl_binding,
                raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                expected_exporter_binding=exporter_binding,
                derived_roles=derived_roles,
            )
            check("normalized-four-role-baseline", True)
            broken = copy.deepcopy(normalized)
            broken["roles"].pop()
            broken_path = root / "broken-normalized.json"
            shipping.write_exclusive_json(broken_path, broken)
            try:
                validate_normalized_export(
                    broken_path, policy=policy, candidate_id=CANDIDATE_ID,
                    run_id=run_id, etl_binding=etl_binding,
                    raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                    expected_exporter_binding=exporter_binding,
                    derived_roles=derived_roles,
                )
            except EtwProfileError:
                missing_role_rejected = True
            else:
                missing_role_rejected = False
            check("missing-role-rejected", missing_role_rejected)
            forged = copy.deepcopy(normalized)
            forged["roles"][0]["p95DurationMilliseconds"] += 0.25
            forged_path = root / "forged-normalized.json"
            shipping.write_exclusive_json(forged_path, forged)
            try:
                validate_normalized_export(
                    forged_path, policy=policy, candidate_id=CANDIDATE_ID,
                    run_id=run_id, etl_binding=etl_binding,
                    raw_cpu_binding=cpu_binding, raw_gpu_binding=gpu_binding,
                    expected_exporter_binding=exporter_binding,
                    derived_roles=derived_roles,
                )
            except EtwProfileError:
                forged_metric_rejected = True
            else:
                forged_metric_rejected = False
            check("forged-normalized-metric-rejected", forged_metric_rejected)
    except (EtwProfileError, shipping.EvidenceError, OSError, KeyError,
            TypeError, ValueError) as exc:
        failures.append({"case": "self-test-exception", "error": str(exc)})
    result = {
        "schema": "DiscGolfTour.Session19SustainedShippingEtwValidatorSelfTest.v3",
        "passed": not failures,
        "assertionCount": checks,
        "failures": failures,
        "wprStarted": False,
        "gameLaunched": False,
        "captureExecuted": False,
    }
    return (0 if not failures else 1), result


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--external-root", type=Path, default=DEFAULT_EXTERNAL_ROOT)
    parser.add_argument("--declaration", type=Path)
    parser.add_argument("--run-root", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        if args.policy != POLICY_PATH or args.archive != DEFAULT_ARCHIVE \
                or args.external_root != DEFAULT_EXTERNAL_ROOT \
                or args.declaration is not None or args.run_root is not None:
            print("--self-test accepts no path overrides", file=sys.stderr)
            return 2
        code, result = run_self_test()
        print(json.dumps(result, indent=2, sort_keys=True))
        return code
    try:
        require(args.policy.resolve(strict=True) == POLICY_PATH.resolve(strict=True),
                "only the exact pinned ETW v3 policy path is supported")
        require((args.declaration is None) is (args.run_root is None),
                "--declaration and --run-root must be supplied together")
        if args.declaration is not None:
            result = validate_completed_run(
                declaration_path=args.declaration, run_root=args.run_root,
                archive=args.archive.resolve(),
                external_root=args.external_root.resolve(),
            )
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0
        policy = load_json(POLICY_PATH, "ETW v3 policy")
        validate_policy(policy)
        _, receipt, run_root = validate_baseline(
            policy, args.archive.resolve(), args.external_root.resolve()
        )
    except (EtwProfileError, shipping.EvidenceError, OSError) as exc:
        print(f"ETW profile validation failed closed: {exc}", file=sys.stderr)
        return 1
    result = {
        "schema": AUDIT_SCHEMA,
        "schemaVersion": 3,
        "session": 19,
        "candidateId": policy["candidateId"],
        "generatedUtc": utc_now(),
        "state": "BASELINE_VALID_CAPTURE_AND_EXPORT_EVIDENCE_NOT_PRESENT",
        "baselineValid": True,
        "threeHolePerformanceRunId": receipt["runId"],
        "threeHolePerformanceRunRoot": str(run_root),
        "etlByteEnvelopeValidated": False,
        "trustedTracerptProviderProofValidated": False,
        "realEtlValidated": False,
        "actualWpaExporterInvocationValidated": False,
        "normalizedFourRoleExportValidated": False,
        "technicalEvidenceAccepted": False,
        "releaseReady": False,
        "blockers": [
            "candidate-bound ETW declaration is not selected",
            "real WPR CPU+GPU ETL byte envelope is not selected",
            "trusted tracerpt provider proof is not selected",
            "actual WPAExporter invocation/output inventory is not selected",
            "normalized GPU/GameThread/RenderThread/RHIThread export is not selected",
        ],
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
