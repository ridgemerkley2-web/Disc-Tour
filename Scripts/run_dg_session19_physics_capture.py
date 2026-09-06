#!/usr/bin/env python3
"""Guard one candidate-bound, append-only throw-physics capture campaign.

The tool predeclares all 100 real-world measured-flight slots and all 60 human
Needle Gate route-attempt slots.  It can snapshot externally produced measured
datasets and launch the unchanged Shipping game for interactive route capture.
It never synthesizes measurements, route attempts, annotations, or approvals.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
from typing import Any, Iterable, Optional, Sequence
import uuid

import validate_dg_physics_measured_reference as measured
import validate_dg_session19_shipping_performance as shipping
import validate_route_telemetry as routes


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config" / "DG_Session19PhysicsCaptureCoordinationPolicy.json"
MEASURED_POLICY_PATH = ROOT / "Config" / "DG_PhysicsMeasuredReferencePolicy.json"
MEASURED_VALIDATOR_PATH = ROOT / "Scripts" / "validate_dg_physics_measured_reference.py"
READINESS_AUDIT_PATH = ROOT / "Scripts" / "audit_dg_physics_capture_readiness.py"
ROUTE_VALIDATOR_PATH = ROOT / "Scripts" / "validate_route_telemetry.py"
RUNNER_PATH = Path(__file__).resolve()
PROJECT_PATH = ROOT / "DiscGolfTour.uproject"
RUNTIME_MODULE_PATH = ROOT / "Binaries" / "Win64" / "UnrealEditor-DiscGolfTour.dll"
DEVELOPER_MODULE_PATH = ROOT / "Binaries" / "Win64" / "UnrealEditor-DiscGolfTourDeveloper.dll"
FOUNDATION_MODULE_PATH = ROOT / "Binaries" / "Win64" / "UnrealEditor-DiscGolfRuntimeFoundation.dll"
EDITOR_TARGET_PATH = ROOT / "Binaries" / "Win64" / "DiscGolfTourEditor.target"
ROUTE_DATA_PATHS = (
    ROOT / "Data" / "PineRidgeCourse.json",
    ROOT / "Data" / "PineRidgeHole2.json",
    ROOT / "Data" / "PineRidgePresentation.json",
)
ROUTE_CAPABILITY_MARKERS = (
    "NeedleGateRouteTelemetry",
    "DGT_SelectTelemetryRoute",
    "DGT_TelemetryTradeoffUnderstood",
    "DGT_TelemetryNextShotClear",
)

POLICY_SCHEMA = "DiscGolfTour.Session19PhysicsCaptureCoordinationPolicy.v1"
DECLARATION_SCHEMA = "DiscGolfTour.Session19PhysicsCaptureDeclaration.v1"
ROUTE_INTENT_SCHEMA = "DiscGolfTour.Session19PhysicsRouteLaunchIntent.v1"
ROUTE_RESULT_SCHEMA = "DiscGolfTour.Session19PhysicsRouteLaunchResult.v1"
ROUTE_RECEIPT_SCHEMA = "DiscGolfTour.Session19PhysicsRouteSnapshotReceipt.v1"
MEASURED_RECEIPT_SCHEMA = "DiscGolfTour.Session19PhysicsMeasuredSnapshotReceipt.v1"
DECLARATION_STATE = "PREDECLARED_CANDIDATE_BOUND_CAPTURE_PLAN_NO_MEASUREMENTS_NO_APPROVALS"
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_\d{8}T\d{6}Z_[0-9a-f]{12}$")
UPPER_SHA_RE = re.compile(r"^[0-9A-F]{64}$")


class CaptureError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CaptureError(message)


def require_keys(value: Any, expected: Iterable[str], label: str) -> None:
    require(type(value) is dict and set(value) == set(expected), f"{label} keys differ")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def parse_utc(value: Any, label: str) -> datetime:
    require(isinstance(value, str) and value.endswith("Z"), f"{label} is not UTC")
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        raise CaptureError(f"{label} is not UTC") from None
    require(parsed.tzinfo is not None, f"{label} lacks timezone authority")
    return parsed


def canonical_uuid4(value: Any) -> bool:
    if not isinstance(value, str) or value.lower() != value:
        return False
    try:
        parsed = uuid.UUID(value)
    except (AttributeError, ValueError):
        return False
    return (
        str(parsed) == value
        and parsed.version == 4
        and parsed.variant == uuid.RFC_4122
    )


def is_reparse(path: Path) -> bool:
    if path.is_symlink():
        return True
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError:
        return False
    return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def path_chain_has_reparse(path: Path) -> bool:
    absolute = path.absolute()
    if not absolute.parts:
        return False
    cursor = Path(absolute.parts[0])
    for part in absolute.parts[1:]:
        cursor /= part
        if os.path.lexists(cursor) and is_reparse(cursor):
            return True
    return False


def same_or_under(path: Path, parent: Path) -> bool:
    return shipping.is_same_or_under(path, parent)


def file_fingerprint(path: Path) -> tuple[int, int, int, int, int]:
    info = path.stat()
    return (
        info.st_size,
        info.st_mtime_ns,
        info.st_ctime_ns,
        info.st_dev,
        info.st_ino,
    )


def stable_file_binding(path: Path, label: str, relative: Optional[str] = None) -> dict[str, Any]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CaptureError(
            f"{label} cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not is_reparse(resolved), f"{label} is not a regular file")
    before = file_fingerprint(resolved)
    try:
        binding = shipping.file_binding(resolved)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"{label} binding failed: {exc}") from None
    require(before == file_fingerprint(resolved), f"{label} changed while hashed")
    if relative is not None:
        return {"path": relative, **binding}
    return {"path": str(resolved), **binding}


def write_exclusive_bytes(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError:
        raise CaptureError(f"refusing to overwrite {path.name}") from None


def stable_read_bytes(path: Path, label: str) -> tuple[bytes, dict[str, Any]]:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CaptureError(
            f"{label} cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_file() and not is_reparse(resolved), f"{label} is not a regular file")
    before = file_fingerprint(resolved)
    try:
        payload = resolved.read_bytes()
    except OSError as exc:
        raise CaptureError(f"{label} could not be read ({exc.__class__.__name__})") from None
    require(before == file_fingerprint(resolved), f"{label} changed while read")
    digest = hashlib.sha256(payload).hexdigest().upper()
    return payload, {
        "path": str(resolved),
        "fileName": resolved.name,
        "bytes": len(payload),
        "sha256": digest,
    }


def canonical_claim_boundary() -> dict[str, Any]:
    return {
        "predeclaredPlanOnly": True,
        "measurementsSynthesized": False,
        "routeAttemptsSynthesized": False,
        "shippingCandidateEmittedRouteTelemetry": False,
        "routeCaptureRuntimeIsDevelopmentEditorGame": True,
        "captureOperatorAttested": False,
        "measurementQualityReviewed": False,
        "physicsOwnerApproved": False,
        "playtestOwnerApproved": False,
        "productOwnerApproved": False,
        "calibrationAccepted": False,
        "releaseReady": False,
    }


def snapshot_claim_boundary() -> dict[str, Any]:
    return {
        "snapshotIdentityOnly": True,
        "humanApprovalGrantedByTool": False,
        "calibrationAcceptedByTool": False,
        "productAcceptanceGrantedByTool": False,
        "releaseReady": False,
    }


def load_coordination_policy() -> dict[str, Any]:
    try:
        policy = shipping.load_json(POLICY_PATH)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"coordination policy is invalid: {exc}") from None
    require(policy.get("schema") == POLICY_SCHEMA, "coordination policy schema differs")
    require(policy.get("schemaVersion") == 1 and policy.get("session") == 19,
            "coordination policy identity differs")
    require(
        policy.get("authority")
        == "PREDECLARED_EXTERNAL_CAPTURE_COORDINATION_NOT_MEASUREMENTS_NOT_APPROVAL",
        "coordination policy authority differs",
    )
    target = policy.get("target")
    require(type(target) is dict, "coordination target is missing")
    require(
        target.get("platform") == "Windows"
        and target.get("configuration") == "Shipping"
        and target.get("candidateIdPattern")
        == r"^S19_WindowsShipping_[0-9]{8}T[0-9]{6}Z_[0-9a-f]{12}$"
        and target.get("archiveLeaf") == "Windows"
        and target.get("launcherRelativePath") == shipping.LAUNCHER_RELATIVE
        and target.get("shippingExecutableRelativePath") == shipping.EXECUTABLE_RELATIVE,
        "coordination Shipping target differs",
    )
    measured_contract = policy.get("measuredReference")
    require(type(measured_contract) is dict, "measured-reference contract is missing")
    require(
        measured_contract.get("policyPath") == "Config/DG_PhysicsMeasuredReferencePolicy.json"
        and measured_contract.get("validatorPath")
        == "Scripts/validate_dg_physics_measured_reference.py"
        and measured_contract.get("readinessAuditPath")
        == "Scripts/audit_dg_physics_capture_readiness.py"
        and measured_contract.get("requiredThrowCount") == 100
        and measured_contract.get("requiredFitThrowCount") == 75
        and measured_contract.get("requiredHoldoutThrowCount") == 25
        and measured_contract.get("minimumAcceptedRepeatsPerEquipmentLaunch") == 5
        and measured_contract.get("snapshotsMustBeExternalAndAppendOnly") is True
        and measured_contract.get("partialSnapshotsMayNotClaimCalibrationAcceptance") is True,
        "measured-reference coordination contract differs",
    )
    route = policy.get("routeTelemetry")
    require(type(route) is dict, "route coordination contract is missing")
    require(
        route.get("validatorPath") == "Scripts/validate_route_telemetry.py"
        and route.get("courseId") == "PineRidgeChampionship"
        and route.get("layoutId") == "Championship"
        and route.get("collisionProfileId") == "PineRidgeCompetitiveV2_Fixtures"
        and route.get("holeNumber") == 2
        and route.get("routeIds") == list(routes.ROUTE_CONTRACT)
        and route.get("attemptsPerRoute") == 20
        and route.get("requiredAttemptCount") == 60
        and route.get("captureRuntime") == "DevelopmentEditorGame"
        and route.get("shippingCandidateRouteTelemetryAvailable") is False
        and route.get("shippingCandidateBindingPurpose")
        == "IMMUTABLE_RELEASE_COMPARISON_CONTEXT_NOT_ROUTE_TELEMETRY_EMITTER"
        and route.get("requiredDevelopmentCapabilityMarkers")
        == list(ROUTE_CAPABILITY_MARKERS)
        and route.get("runtimeFlag") == "-NeedleGateRouteTelemetry"
        and route.get("reportRelativePathUnderUserDir")
        == "Saved/RouteTelemetryReports/LatestRouteTelemetry.json"
        and route.get("humanPlayAndAnnotationsRequired") is True
        and route.get("automationMayNotSynthesizeAttempts") is True,
        "route coordination contract differs",
    )
    guard = policy.get("guardedLaunch")
    require(type(guard) is dict, "guarded-launch contract is missing")
    require(
        guard.get("executableKind") == "UnrealEditor.exe"
        and guard.get("boundProjectDescriptor") == "DiscGolfTour.uproject"
        and
        guard.get("requiredArgumentsBeforeUserDir")
        == [
            "-game",
            "-Course=PineRidge",
            "-Hole=2",
            "-NeedleGateRouteTelemetry",
            "-ResX=1920",
            "-ResY=1080",
            "-ForceRes",
            "-dx12",
            "-NoLoadExistingSave",
            "-DGNoProfileWrites",
            "-log",
        ],
        "guarded route arguments differ",
    )
    for key in (
        "interactiveVisibleWindowRequired",
        "unattendedForbidden",
        "renderOffscreenForbidden",
        "benchmarkOrFixedTimeForbidden",
        "profileSaveFilesForbidden",
        "candidateArchiveMustMatchBeforeAndAfter",
        "launchIntentMustPrecedeProcessStart",
        "routeSnapshotMustFollowProcessExit",
    ):
        require(guard.get(key) is True, f"guarded-launch requirement {key} was weakened")
    schemas = policy.get("schemas")
    require(
        schemas
        == {
            "declaration": DECLARATION_SCHEMA,
            "routeLaunchIntent": ROUTE_INTENT_SCHEMA,
            "routeLaunchResult": ROUTE_RESULT_SCHEMA,
            "routeSnapshotReceipt": ROUTE_RECEIPT_SCHEMA,
            "measuredSnapshotReceipt": MEASURED_RECEIPT_SCHEMA,
        },
        "coordination artifact schemas differ",
    )
    boundary = policy.get("claimBoundary")
    require(type(boundary) is dict, "coordination claim boundary is missing")
    for key in (
        "declarationIsCapturedEvidence",
        "deterministicOrSyntheticDataIsMeasuredReference",
        "routeAutomationMayReplaceHumanAttempts",
        "shippingCandidateEmittedRouteTelemetry",
        "captureOperatorAttested",
        "measurementQualityReviewed",
        "physicsOwnerApproved",
        "playtestOwnerApproved",
        "productOwnerApproved",
        "calibrationAccepted",
        "releaseReady",
    ):
        require(boundary.get(key) is False, f"coordination policy may not grant {key}")
    require(boundary.get("routeCaptureRuntimeIsDevelopmentEditorGame") is True,
            "coordination policy route-capture runtime boundary differs")
    return policy


def load_measured_policy() -> dict[str, Any]:
    try:
        policy = measured.load_json(MEASURED_POLICY_PATH)
    except Exception as exc:
        raise CaptureError(
            f"measured-reference policy is invalid ({exc.__class__.__name__})"
        ) from None
    require(
        policy.get("schema") == "DiscGolfTour.PhysicsMeasuredReferencePolicy.v1"
        and policy.get("policyId") == "physics_measured_reference_capture_v1",
        "measured-reference policy identity differs",
    )
    return policy


def source_bindings() -> dict[str, Any]:
    return {
        "coordinationPolicy": stable_file_binding(
            POLICY_PATH, "coordination policy", "Config/DG_Session19PhysicsCaptureCoordinationPolicy.json"
        ),
        "measuredReferencePolicy": stable_file_binding(
            MEASURED_POLICY_PATH, "measured-reference policy", "Config/DG_PhysicsMeasuredReferencePolicy.json"
        ),
        "measuredReferenceValidator": stable_file_binding(
            MEASURED_VALIDATOR_PATH,
            "measured-reference validator",
            "Scripts/validate_dg_physics_measured_reference.py",
        ),
        "captureReadinessAudit": stable_file_binding(
            READINESS_AUDIT_PATH,
            "capture-readiness audit",
            "Scripts/audit_dg_physics_capture_readiness.py",
        ),
        "routeTelemetryValidator": stable_file_binding(
            ROUTE_VALIDATOR_PATH,
            "route-telemetry validator",
            "Scripts/validate_route_telemetry.py",
        ),
        "captureRunner": stable_file_binding(
            RUNNER_PATH, "physics-capture runner", "Scripts/run_dg_session19_physics_capture.py"
        ),
    }


def marker_presence(path: Path, markers: Sequence[str], label: str) -> dict[str, bool]:
    payload, _ = stable_read_bytes(path, label)
    return {
        marker: (
            marker.encode("ascii") in payload
            or marker.encode("utf-16le") in payload
        )
        for marker in markers
    }


def route_runtime_bindings(editor_executable: Path, archive: Path) -> dict[str, Any]:
    require(editor_executable.is_absolute(), "Editor executable path must be absolute")
    try:
        editor_executable = editor_executable.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CaptureError(
            f"Editor executable cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(
        editor_executable.name.casefold() == "unrealeditor.exe"
        and editor_executable.is_file()
        and not is_reparse(editor_executable)
        and not path_chain_has_reparse(editor_executable),
        "route capture requires one regular UnrealEditor.exe",
    )
    require(not same_or_under(editor_executable, ROOT),
            "Editor executable must remain outside the project")
    require(not same_or_under(editor_executable, archive),
            "Editor executable must remain outside the candidate archive")
    engine_build_version = editor_executable.parents[2] / "Build" / "Build.version"
    development_presence = marker_presence(
        DEVELOPER_MODULE_PATH, ROUTE_CAPABILITY_MARKERS, "Developer route module"
    )
    require(all(development_presence.values()),
            "Development route module lacks required human telemetry commands")
    runtime_presence = marker_presence(
        RUNTIME_MODULE_PATH, ("NeedleGateRouteTelemetry",), "runtime route module"
    )
    require(runtime_presence["NeedleGateRouteTelemetry"],
            "Development runtime lacks the route telemetry launch hook")
    shipping_executable = archive / shipping.EXECUTABLE_RELATIVE
    shipping_presence = marker_presence(
        shipping_executable, ROUTE_CAPABILITY_MARKERS, "Shipping executable"
    )
    require(not any(shipping_presence.values()),
            "Shipping candidate unexpectedly contains Development route tooling")
    return {
        "runtimeType": "DevelopmentEditorGame",
        "shippingCandidateIsEmitter": False,
        "editorExecutable": stable_file_binding(
            editor_executable, "Unreal Editor executable"
        ),
        "engineBuildVersion": stable_file_binding(
            engine_build_version, "Unreal Engine build version"
        ),
        "projectDescriptor": stable_file_binding(
            PROJECT_PATH, "project descriptor", "DiscGolfTour.uproject"
        ),
        "runtimeModule": stable_file_binding(
            RUNTIME_MODULE_PATH,
            "Editor runtime module",
            "Binaries/Win64/UnrealEditor-DiscGolfTour.dll",
        ),
        "developerModule": stable_file_binding(
            DEVELOPER_MODULE_PATH,
            "Editor DeveloperTool module",
            "Binaries/Win64/UnrealEditor-DiscGolfTourDeveloper.dll",
        ),
        "foundationModule": stable_file_binding(
            FOUNDATION_MODULE_PATH,
            "Editor runtime-foundation module",
            "Binaries/Win64/UnrealEditor-DiscGolfRuntimeFoundation.dll",
        ),
        "editorTarget": stable_file_binding(
            EDITOR_TARGET_PATH,
            "Editor target receipt",
            "Binaries/Win64/DiscGolfTourEditor.target",
        ),
        "routeData": {
            "course": stable_file_binding(
                ROUTE_DATA_PATHS[0], "route course data", "Data/PineRidgeCourse.json"
            ),
            "hole2": stable_file_binding(
                ROUTE_DATA_PATHS[1], "route hole data", "Data/PineRidgeHole2.json"
            ),
            "presentation": stable_file_binding(
                ROUTE_DATA_PATHS[2],
                "route presentation data",
                "Data/PineRidgePresentation.json",
            ),
        },
        "requiredCapabilityMarkers": list(ROUTE_CAPABILITY_MARKERS),
        "developmentCapabilityMarkersPresent": [
            marker for marker in ROUTE_CAPABILITY_MARKERS if development_presence[marker]
        ],
        "shippingCandidateCapabilityMarkersAbsent": [
            marker for marker in ROUTE_CAPABILITY_MARKERS if not shipping_presence[marker]
        ],
        "sourceToBinaryAttestation": False,
    }


def archive_identity(archive: Path, candidate_id: str) -> dict[str, Any]:
    try:
        manifest = shipping.build_archive_manifest(archive, candidate_id)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"candidate archive validation failed: {exc}") from None
    return {
        "inventorySha256": manifest["inventorySha256"],
        "fileCount": manifest["fileCount"],
        "totalBytes": manifest["totalBytes"],
        "launcher": manifest["launcher"],
        "shippingExecutable": manifest["shippingExecutable"],
    }


def build_throw_slots(policy: dict[str, Any]) -> list[dict[str, Any]]:
    equipment = policy.get("equipmentMatrix")
    launches = policy.get("launchMatrix")
    plan = policy.get("samplePlan")
    require(isinstance(equipment, list) and isinstance(launches, list) and type(plan) is dict,
            "measured-reference matrix is malformed")
    repeats = plan.get("minimumAcceptedRepeatsPerEquipmentLaunch")
    require(type(repeats) is int and repeats == 5, "measured-reference repeat count differs")
    fit_ids = plan.get("fitLaunchIds")
    holdout_ids = plan.get("holdoutLaunchIds")
    require(isinstance(fit_ids, list) and isinstance(holdout_ids, list),
            "measured-reference partition matrix is malformed")
    slots: list[dict[str, Any]] = []
    for equipment_row in equipment:
        require(type(equipment_row) is dict, "measured equipment row is malformed")
        for launch in launches:
            require(type(launch) is dict, "measured launch row is malformed")
            launch_id = launch.get("launchId")
            partition = "fit" if launch_id in fit_ids else "holdout" if launch_id in holdout_ids else None
            require(partition is not None, f"launch {launch_id} has no partition")
            for repeat in range(1, repeats + 1):
                slot_id = f"M-{equipment_row['category']}-{launch_id}-{repeat:02d}"
                slots.append(
                    {
                        "slotId": slot_id,
                        "partition": partition,
                        "equipmentCategory": equipment_row["category"],
                        "moldId": equipment_row["moldId"],
                        "plastic": equipment_row["plastic"],
                        "targetMassGrams": equipment_row["targetMassGrams"],
                        "launchId": launch_id,
                        "throwStyle": launch["throwStyle"],
                        "targetHyzerDeg": launch["targetHyzerDeg"],
                        "targetNoseDeg": launch["targetNoseDeg"],
                        "targetLaunchDeg": launch["targetLaunchDeg"],
                        "repeat": repeat,
                        "status": "UNRECORDED",
                        "artifact": None,
                    }
                )
    require(len(slots) == 100 and len({slot["slotId"] for slot in slots}) == 100,
            "measured slot plan is not exactly 100 unique slots")
    require(sum(slot["partition"] == "fit" for slot in slots) == 75,
            "measured fit slot plan is not exactly 75")
    require(sum(slot["partition"] == "holdout" for slot in slots) == 25,
            "measured holdout slot plan is not exactly 25")
    return slots


def build_route_slots(policy: dict[str, Any]) -> list[dict[str, Any]]:
    route = policy["routeTelemetry"]
    slots = [
        {
            "slotId": f"R-{route_id}-{attempt:02d}",
            "routeId": route_id,
            "attempt": attempt,
            "status": "UNRECORDED",
            "artifact": None,
        }
        for route_id in route["routeIds"]
        for attempt in range(1, route["attemptsPerRoute"] + 1)
    ]
    require(len(slots) == 60 and len({slot["slotId"] for slot in slots}) == 60,
            "route slot plan is not exactly 60 unique slots")
    return slots


def guarded_arguments(policy: dict[str, Any], user_dir: Path, token: str,
                      sanitized: bool) -> list[str]:
    value = f"<EXTERNAL_UUID:{token}>" if sanitized else str(user_dir)
    return [
        str(PROJECT_PATH.resolve()),
        *policy["guardedLaunch"]["requiredArgumentsBeforeUserDir"],
        f"-UserDir={value}",
    ]


def validate_external_root(external_root: Path, archive: Path, create: bool) -> Path:
    require(external_root.is_absolute() and ".." not in external_root.parts,
            "external root must be absolute without parent traversal")
    require(not path_chain_has_reparse(external_root), "external root path contains a reparse point")
    prospective = external_root.resolve(strict=False)
    require(not same_or_under(prospective, ROOT), "external root must remain outside the project")
    require(not same_or_under(prospective, archive), "external root must remain outside the archive")
    if create:
        external_root.mkdir(parents=True, exist_ok=True)
    try:
        resolved = external_root.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CaptureError(
            f"external root cannot be resolved ({exc.__class__.__name__})"
        ) from None
    require(resolved.is_dir() and not is_reparse(resolved), "external root is not a regular directory")
    return resolved


def compose_declaration(
    *,
    policy: dict[str, Any],
    measured_policy: dict[str, Any],
    candidate_id: str,
    capture_id: str,
    route_token: str,
    declared_utc: str,
    archive: Path,
    archive_binding: dict[str, Any],
    external_root: Path,
    declaration_path: Path,
    bindings: dict[str, Any],
    route_runtime: dict[str, Any],
) -> dict[str, Any]:
    require(CANDIDATE_RE.fullmatch(candidate_id) is not None, "candidate ID is invalid")
    require(canonical_uuid4(capture_id), "capture ID is not a canonical UUIDv4")
    require(canonical_uuid4(route_token), "route UserDir token is not a canonical UUIDv4")
    route_user_dir = external_root / "UserDirs" / route_token
    route_report = route_user_dir / Path(
        policy["routeTelemetry"]["reportRelativePathUnderUserDir"]
    )
    run_root = external_root / "Runs" / capture_id
    return {
        "schema": DECLARATION_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "captureId": capture_id,
        "declaredUtc": declared_utc,
        "state": DECLARATION_STATE,
        "sourceBindings": bindings,
        "candidateArchive": {"path": str(archive), **archive_binding},
        "routeCaptureRuntime": route_runtime,
        "externalLayout": {
            "externalRoot": str(external_root),
            "declarationPath": str(declaration_path),
            "runRoot": str(run_root),
            "routeUserDirToken": route_token,
            "routeUserDir": str(route_user_dir),
            "routeReport": str(route_report),
        },
        "measuredReferencePlan": {
            "policyId": measured_policy["policyId"],
            "requiredThrowCount": measured_policy["samplePlan"]["requiredTotalSampleCount"],
            "requiredFitThrowCount": measured_policy["samplePlan"]["requiredFitSampleCount"],
            "requiredHoldoutThrowCount": measured_policy["samplePlan"]["requiredHoldoutSampleCount"],
            "slots": build_throw_slots(measured_policy),
        },
        "routeTelemetryPlan": {
            "courseId": policy["routeTelemetry"]["courseId"],
            "layoutId": policy["routeTelemetry"]["layoutId"],
            "collisionProfileId": policy["routeTelemetry"]["collisionProfileId"],
            "holeNumber": policy["routeTelemetry"]["holeNumber"],
            "requiredAttemptCount": policy["routeTelemetry"]["requiredAttemptCount"],
            "slots": build_route_slots(policy),
        },
        "sanitizedGuardedRouteArguments": guarded_arguments(
            policy, route_user_dir, route_token, True
        ),
        "claimBoundary": canonical_claim_boundary(),
    }


def validate_binding_shape(binding: Any, label: str) -> None:
    require_keys(binding, {"path", "fileName", "bytes", "sha256"}, label)
    require(isinstance(binding["path"], str) and binding["path"], f"{label} path is invalid")
    require(isinstance(binding["fileName"], str) and binding["fileName"],
            f"{label} fileName is invalid")
    require(type(binding["bytes"]) is int and binding["bytes"] >= 0,
            f"{label} bytes are invalid")
    require(isinstance(binding["sha256"], str) and UPPER_SHA_RE.fullmatch(binding["sha256"]),
            f"{label} sha256 is invalid")


def validate_route_runtime_shape(runtime: Any) -> Path:
    require_keys(
        runtime,
        {
            "runtimeType", "shippingCandidateIsEmitter", "editorExecutable",
            "engineBuildVersion", "projectDescriptor", "runtimeModule",
            "developerModule", "foundationModule", "editorTarget", "routeData",
            "requiredCapabilityMarkers", "developmentCapabilityMarkersPresent",
            "shippingCandidateCapabilityMarkersAbsent", "sourceToBinaryAttestation",
        },
        "routeCaptureRuntime",
    )
    require(
        runtime["runtimeType"] == "DevelopmentEditorGame"
        and runtime["shippingCandidateIsEmitter"] is False
        and runtime["requiredCapabilityMarkers"] == list(ROUTE_CAPABILITY_MARKERS)
        and runtime["developmentCapabilityMarkersPresent"] == list(ROUTE_CAPABILITY_MARKERS)
        and runtime["shippingCandidateCapabilityMarkersAbsent"] == list(ROUTE_CAPABILITY_MARKERS)
        and runtime["sourceToBinaryAttestation"] is False,
        "route-capture runtime authority boundary differs",
    )
    for label in (
        "editorExecutable", "engineBuildVersion", "projectDescriptor",
        "runtimeModule", "developerModule", "foundationModule", "editorTarget",
    ):
        validate_binding_shape(runtime[label], f"route runtime {label}")
    route_data = runtime["routeData"]
    require_keys(route_data, {"course", "hole2", "presentation"}, "route runtime data")
    for label, binding in route_data.items():
        validate_binding_shape(binding, f"route runtime data {label}")
    editor_path = runtime["editorExecutable"]["path"]
    require(isinstance(editor_path, str) and Path(editor_path).is_absolute(),
            "route runtime Editor path is invalid")
    return Path(editor_path)


def validate_declaration(
    declaration: dict[str, Any],
    declaration_path: Path,
    policy: dict[str, Any],
    measured_policy: dict[str, Any],
    *,
    live: bool,
) -> dict[str, Any]:
    require_keys(
        declaration,
        {
            "schema", "schemaVersion", "session", "candidateId", "captureId",
            "declaredUtc", "state", "sourceBindings", "candidateArchive",
            "routeCaptureRuntime",
            "externalLayout", "measuredReferencePlan", "routeTelemetryPlan",
            "sanitizedGuardedRouteArguments", "claimBoundary",
        },
        "declaration",
    )
    require(
        declaration["schema"] == DECLARATION_SCHEMA
        and declaration["schemaVersion"] == 1
        and declaration["session"] == 19
        and declaration["state"] == DECLARATION_STATE,
        "declaration identity/state differs",
    )
    candidate_id = declaration["candidateId"]
    capture_id = declaration["captureId"]
    require(isinstance(candidate_id, str) and CANDIDATE_RE.fullmatch(candidate_id),
            "declaration candidate ID is invalid")
    require(canonical_uuid4(capture_id), "declaration capture ID is invalid")
    declared = parse_utc(declaration["declaredUtc"], "declaration declaredUtc")
    require(declared <= datetime.now(timezone.utc), "declaration is future-dated")

    bindings = declaration["sourceBindings"]
    require_keys(
        bindings,
        {
            "coordinationPolicy", "measuredReferencePolicy",
            "measuredReferenceValidator", "captureReadinessAudit",
            "routeTelemetryValidator", "captureRunner",
        },
        "sourceBindings",
    )
    for label, binding in bindings.items():
        validate_binding_shape(binding, f"source binding {label}")
    if live:
        require(bindings == source_bindings(), "live capture source bindings differ")

    archive_value = declaration["candidateArchive"]
    require_keys(
        archive_value,
        {"path", "inventorySha256", "fileCount", "totalBytes", "launcher", "shippingExecutable"},
        "candidateArchive",
    )
    archive = Path(archive_value["path"]) if isinstance(archive_value["path"], str) else Path()
    require(archive.is_absolute(), "candidate archive path is not absolute")
    require(archive.name == "Windows" and archive.parent.name == candidate_id,
            "candidate archive path identity differs")
    require(isinstance(archive_value["inventorySha256"], str)
            and UPPER_SHA_RE.fullmatch(archive_value["inventorySha256"]),
            "candidate archive inventory hash is invalid")
    require(type(archive_value["fileCount"]) is int and archive_value["fileCount"] > 0,
            "candidate archive file count is invalid")
    require(type(archive_value["totalBytes"]) is int and archive_value["totalBytes"] > 0,
            "candidate archive byte count is invalid")
    if live:
        live_archive = archive.resolve(strict=True)
        expected_archive = {"path": str(live_archive), **archive_identity(live_archive, candidate_id)}
        require(archive_value == expected_archive, "live candidate archive differs from declaration")
        archive = live_archive

    editor_executable = validate_route_runtime_shape(declaration["routeCaptureRuntime"])
    if live:
        require(
            declaration["routeCaptureRuntime"]
            == route_runtime_bindings(editor_executable, archive),
            "live Development route-capture runtime differs from declaration",
        )

    layout = declaration["externalLayout"]
    require_keys(
        layout,
        {"externalRoot", "declarationPath", "runRoot", "routeUserDirToken", "routeUserDir", "routeReport"},
        "externalLayout",
    )
    token = layout["routeUserDirToken"]
    require(canonical_uuid4(token), "route UserDir token is invalid")
    external_root = Path(layout["externalRoot"]) if isinstance(layout["externalRoot"], str) else Path()
    declared_path = Path(layout["declarationPath"]) if isinstance(layout["declarationPath"], str) else Path()
    run_root = Path(layout["runRoot"]) if isinstance(layout["runRoot"], str) else Path()
    user_dir = Path(layout["routeUserDir"]) if isinstance(layout["routeUserDir"], str) else Path()
    route_report = Path(layout["routeReport"]) if isinstance(layout["routeReport"], str) else Path()
    for path, label in (
        (external_root, "external root"), (declared_path, "declared path"),
        (run_root, "run root"), (user_dir, "route UserDir"),
        (route_report, "route report"),
    ):
        require(path.is_absolute() and ".." not in path.parts, f"{label} is not an absolute safe path")
    require(declared_path.resolve(strict=False) == declaration_path.resolve(strict=False),
            "declaration path differs from its bound path")
    require(user_dir.resolve(strict=False)
            == (external_root / "UserDirs" / token).resolve(strict=False),
            "route UserDir layout differs")
    require(run_root.resolve(strict=False)
            == (external_root / "Runs" / capture_id).resolve(strict=False),
            "capture run-root layout differs")
    require(route_report.resolve(strict=False)
            == (user_dir / Path(policy["routeTelemetry"]["reportRelativePathUnderUserDir"])).resolve(strict=False),
            "route report layout differs")
    for path, label in ((external_root, "external root"), (declared_path, "declaration"),
                        (run_root, "run root"), (user_dir, "route UserDir")):
        require(not same_or_under(path, ROOT), f"{label} overlaps the project")
        require(not same_or_under(path, archive), f"{label} overlaps the candidate archive")
        require(same_or_under(path, external_root), f"{label} escapes the external root")
    if live:
        require(not path_chain_has_reparse(external_root), "external root contains a reparse point")

    measured_plan = declaration["measuredReferencePlan"]
    require_keys(
        measured_plan,
        {"policyId", "requiredThrowCount", "requiredFitThrowCount", "requiredHoldoutThrowCount", "slots"},
        "measuredReferencePlan",
    )
    require(
        measured_plan["policyId"] == measured_policy["policyId"]
        and measured_plan["requiredThrowCount"] == 100
        and measured_plan["requiredFitThrowCount"] == 75
        and measured_plan["requiredHoldoutThrowCount"] == 25
        and measured_plan["slots"] == build_throw_slots(measured_policy),
        "measured-reference slot plan differs",
    )
    route_plan = declaration["routeTelemetryPlan"]
    require_keys(
        route_plan,
        {"courseId", "layoutId", "collisionProfileId", "holeNumber", "requiredAttemptCount", "slots"},
        "routeTelemetryPlan",
    )
    require(
        route_plan["courseId"] == "PineRidgeChampionship"
        and route_plan["layoutId"] == "Championship"
        and route_plan["collisionProfileId"] == "PineRidgeCompetitiveV2_Fixtures"
        and route_plan["holeNumber"] == 2
        and route_plan["requiredAttemptCount"] == 60
        and route_plan["slots"] == build_route_slots(policy),
        "route slot plan differs",
    )
    require(
        declaration["sanitizedGuardedRouteArguments"]
        == guarded_arguments(policy, user_dir, token, True),
        "guarded route arguments differ",
    )
    require(declaration["claimBoundary"] == canonical_claim_boundary(),
            "declaration claim boundary differs")
    return {
        "candidateId": candidate_id,
        "captureId": capture_id,
        "archive": archive,
        "externalRoot": external_root,
        "runRoot": run_root,
        "userDir": user_dir,
        "routeReport": route_report,
        "routeUserDirToken": token,
        "editorExecutable": editor_executable,
    }


def load_declaration(path: Path) -> dict[str, Any]:
    require(path.is_absolute(), "declaration path must be absolute")
    require(path.is_file() and not is_reparse(path),
            "declaration path is missing, redirected, or not a regular file")
    try:
        value = shipping.load_json(path)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"declaration is invalid: {exc}") from None
    return value


def create_declaration(
    candidate_id: str,
    archive: Path,
    editor_executable: Path,
    external_root: Path,
    output: Optional[Path],
) -> tuple[Path, dict[str, Any]]:
    policy = load_coordination_policy()
    measured_policy = load_measured_policy()
    require(CANDIDATE_RE.fullmatch(candidate_id) is not None, "candidate ID is invalid")
    try:
        archive = shipping.validate_candidate_archive_path(archive, candidate_id)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"candidate archive path is invalid: {exc}") from None
    external_root = validate_external_root(external_root, archive, True)
    capture_id = str(uuid.uuid4())
    route_token = str(uuid.uuid4())
    if output is None:
        output = (
            external_root
            / "Declarations"
            / f"PhysicsCaptureDeclaration-{candidate_id}-{capture_id}.json"
        )
    require(output.is_absolute() and ".." not in output.parts,
            "declaration output must be an absolute safe path")
    output = output.resolve(strict=False)
    require(same_or_under(output, external_root), "declaration output escapes the external root")
    require(not same_or_under(output, ROOT) and not same_or_under(output, archive),
            "declaration output overlaps protected content")
    require(not output.exists(), "refusing to overwrite an existing declaration")
    require(not path_chain_has_reparse(output.parent), "declaration parent contains a reparse point")
    before_archive = archive_identity(archive, candidate_id)
    before_bindings = source_bindings()
    before_route_runtime = route_runtime_bindings(editor_executable, archive)
    declaration = compose_declaration(
        policy=policy,
        measured_policy=measured_policy,
        candidate_id=candidate_id,
        capture_id=capture_id,
        route_token=route_token,
        declared_utc=utc_now(),
        archive=archive,
        archive_binding=before_archive,
        external_root=external_root,
        declaration_path=output,
        bindings=before_bindings,
        route_runtime=before_route_runtime,
    )
    require(archive_identity(archive, candidate_id) == before_archive,
            "candidate archive changed during declaration")
    require(source_bindings() == before_bindings,
            "capture authority changed during declaration")
    require(route_runtime_bindings(editor_executable, archive) == before_route_runtime,
            "Development route-capture runtime changed during declaration")
    try:
        shipping.write_exclusive_json(output, declaration)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"declaration publish failed: {exc}") from None
    published = load_declaration(output)
    validate_declaration(published, output, policy, measured_policy, live=True)
    return output, published


def secure_external_input(path: Path, context: dict[str, Any], label: str) -> Path:
    require(path.is_absolute() and ".." not in path.parts, f"{label} path must be absolute")
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise CaptureError(f"{label} cannot be resolved ({exc.__class__.__name__})") from None
    require(resolved.is_file() and not is_reparse(resolved), f"{label} is not a regular file")
    require(not same_or_under(resolved, ROOT), f"{label} must remain outside the project")
    require(not same_or_under(resolved, context["archive"]),
            f"{label} must remain outside the candidate archive")
    require(not path_chain_has_reparse(resolved), f"{label} path contains a reparse point")
    return resolved


def inspect_route_report(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {
            "present": False,
            "valid": True,
            "complete": False,
            "attempts": 0,
            "counts": {route_id: 0 for route_id in routes.ROUTE_CONTRACT},
            "report": None,
            "errors": [],
        }
    require(path.is_file() and not is_reparse(path), "route report is not a regular file")
    try:
        report = routes.load_report(path)
    except Exception as exc:
        return {
            "present": True, "valid": False, "complete": False, "attempts": 0,
            "counts": {route_id: 0 for route_id in routes.ROUTE_CONTRACT},
            "report": None, "errors": [f"route report load failed ({exc.__class__.__name__})"],
        }
    errors, counts = routes.validate_report(report, False)
    complete_errors, _ = routes.validate_report(report, True)
    attempts = len(report.get("attempts", [])) if type(report) is dict and isinstance(report.get("attempts"), list) else 0
    return {
        "present": True,
        "valid": not errors,
        "complete": not complete_errors,
        "attempts": attempts,
        "counts": counts,
        "report": report,
        "errors": errors,
    }


def measured_progress(policy: dict[str, Any], dataset: dict[str, Any]) -> dict[str, Any]:
    throws = dataset.get("throws") if isinstance(dataset.get("throws"), list) else []
    expected_slots = build_throw_slots(policy)
    limits: dict[tuple[str, str, str], int] = {}
    for slot in expected_slots:
        key = (slot["partition"], slot["equipmentCategory"], slot["launchId"])
        limits[key] = limits.get(key, 0) + 1
    counts = {key: 0 for key in limits}
    for row in throws:
        if type(row) is dict:
            key = (row.get("partition"), row.get("equipmentCategory"), row.get("launchId"))
            if key in counts:
                counts[key] += 1
    overfilled = [
        {"partition": key[0], "equipmentCategory": key[1], "launchId": key[2],
         "observed": count, "limit": limits[key]}
        for key, count in counts.items() if count > limits[key]
    ]
    return {
        "observedThrows": len(throws),
        "requiredThrows": 100,
        "remainingThrows": max(0, 100 - len(throws)),
        "fitThrows": sum(count for key, count in counts.items() if key[0] == "fit"),
        "holdoutThrows": sum(count for key, count in counts.items() if key[0] == "holdout"),
        "overfilledCells": overfilled,
    }


def inspect_measured_dataset(path: Path, context: dict[str, Any],
                             policy: dict[str, Any]) -> dict[str, Any]:
    path = secure_external_input(path, context, "measured dataset")
    try:
        dataset = measured.load_json(path)
    except Exception as exc:
        return {"present": True, "valid": False, "errors": [f"dataset load failed ({exc.__class__.__name__})"]}
    complete_mode = dataset.get("acceptedForCalibration") is True
    errors = measured.validate(policy, dataset, complete_mode)
    progress = measured_progress(policy, dataset)
    if dataset.get("template") is True:
        errors.append("template is not captured measured evidence")
    if progress["observedThrows"] <= 0:
        errors.append("measured snapshot contains no throws")
    if progress["observedThrows"] > 100:
        errors.append("measured snapshot exceeds the predeclared 100 slots")
    if progress["overfilledCells"]:
        errors.append("measured snapshot overfills a predeclared equipment/launch cell")
    complete_errors = measured.validate(policy, dataset, True)
    return {
        "present": True,
        "valid": not errors,
        "complete": not complete_errors,
        "errors": errors,
        "completeErrors": complete_errors,
        "progress": progress,
        "dataset": dataset,
        "path": path,
    }


def preflight(
    declaration_path: Path, measured_dataset: Optional[Path] = None
) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]]:
    policy = load_coordination_policy()
    measured_policy = load_measured_policy()
    declaration = load_declaration(declaration_path)
    context = validate_declaration(
        declaration, declaration_path, policy, measured_policy, live=True
    )
    user_dir = context["userDir"]
    user_dir_errors: list[str] = []
    save_files: list[str] = []
    if user_dir.exists():
        if not user_dir.is_dir() or is_reparse(user_dir) or path_chain_has_reparse(user_dir):
            user_dir_errors.append("route UserDir is redirected or not a directory")
        else:
            save_files = [
                str(path.relative_to(user_dir))
                for path in user_dir.rglob("*.sav")
                if path.is_file()
            ]
            if save_files:
                user_dir_errors.append("route UserDir contains forbidden profile save files")
    route_state = inspect_route_report(context["routeReport"])
    measured_state: Optional[dict[str, Any]] = None
    if measured_dataset is not None:
        measured_state = inspect_measured_dataset(measured_dataset, context, measured_policy)
    route_launch_allowed = not user_dir_errors and route_state["valid"] and not route_state["complete"]
    inputs_valid = measured_state is None or measured_state["valid"]
    output = {
        "schema": "DiscGolfTour.Session19PhysicsCapturePreflight.v1",
        "candidateId": context["candidateId"],
        "captureId": context["captureId"],
        "status": (
            "READY_FOR_GUARDED_DEVELOPMENT_ROUTE_CAPTURE"
            if route_launch_allowed and inputs_valid
            else "CAPTURE_REQUIRES_ATTENTION"
        ),
        "candidateArchiveExact": True,
        "shippingCandidateIsRouteTelemetryEmitter": False,
        "routeCaptureRuntime": "DevelopmentEditorGame",
        "developmentRouteRuntimeExact": True,
        "sourceBindingsExact": True,
        "declarationExact": True,
        "routeLaunchAllowed": route_launch_allowed,
        "routeCaptureComplete": route_state["complete"],
        "routeAttempts": route_state["attempts"],
        "routeCounts": route_state["counts"],
        "routeErrors": [*user_dir_errors, *route_state["errors"]],
        "forbiddenSaveFiles": save_files,
        "measuredDatasetInspected": measured_state is not None,
        "measuredDatasetValid": measured_state["valid"] if measured_state else False,
        "measuredCaptureComplete": measured_state["complete"] if measured_state else False,
        "measuredProgress": measured_state.get("progress") if measured_state else None,
        "measuredErrors": measured_state["errors"] if measured_state else [
            "no external measured dataset supplied; no measurement claim was inspected"
        ],
        "releaseReady": False,
    }
    return output, declaration, context, policy


def declaration_binding(path: Path) -> dict[str, Any]:
    return stable_file_binding(path, "capture declaration")


def ensure_snapshot_parent(context: dict[str, Any], category: str) -> Path:
    root = context["runRoot"] / category
    require(same_or_under(root, context["externalRoot"]), "snapshot root escapes external evidence")
    require(not same_or_under(root, ROOT) and not same_or_under(root, context["archive"]),
            "snapshot root overlaps protected content")
    require(not path_chain_has_reparse(root), "snapshot root contains a reparse point")
    root.mkdir(parents=True, exist_ok=True)
    require(root.is_dir() and not is_reparse(root), "snapshot root is invalid")
    return root


def seal_measured_snapshot(declaration_path: Path, source: Path) -> tuple[Path, dict[str, Any]]:
    preflight_result, declaration, context, _ = preflight(declaration_path, source)
    require(preflight_result["measuredDatasetValid"],
            "measured dataset failed structural capture validation: "
            + "; ".join(preflight_result["measuredErrors"][:5]))
    measured_policy = load_measured_policy()
    inspected = inspect_measured_dataset(source, context, measured_policy)
    require(inspected["valid"], "measured dataset changed before snapshot")
    source_path = inspected["path"]
    payload, source_binding = stable_read_bytes(source_path, "measured dataset")
    snapshot_id = str(uuid.uuid4())
    parent = ensure_snapshot_parent(context, "MeasuredSnapshots")
    snapshot_dir = parent / snapshot_id
    snapshot_dir.mkdir(exist_ok=False)
    destination = snapshot_dir / "PhysicsMeasuredReferenceDataset.json"
    write_exclusive_bytes(destination, payload)
    copied = inspect_measured_dataset(destination, context, measured_policy)
    require(copied["valid"], "published measured snapshot failed revalidation")
    destination_binding = stable_file_binding(destination, "published measured snapshot")
    require(destination_binding["sha256"] == source_binding["sha256"],
            "published measured snapshot differs from source")
    receipt = {
        "schema": MEASURED_RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": context["candidateId"],
        "captureId": context["captureId"],
        "snapshotId": snapshot_id,
        "snapshottedUtc": utc_now(),
        "state": "PASS_APPEND_ONLY_MEASURED_DATASET_SNAPSHOT_IDENTITY_NO_APPROVAL_CLAIM",
        "declaration": declaration_binding(declaration_path),
        "sourceDataset": source_binding,
        "snapshotDataset": destination_binding,
        "progress": copied["progress"],
        "completeValidatorPassed": copied["complete"],
        "datasetClaimsCalibrationAcceptance": copied["dataset"].get("acceptedForCalibration") is True,
        "claimBoundary": snapshot_claim_boundary(),
    }
    receipt_path = snapshot_dir / "MeasuredSnapshotReceipt.json"
    try:
        shipping.write_exclusive_json(receipt_path, receipt)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"measured receipt publish failed: {exc}") from None
    require(shipping.load_json(receipt_path) == receipt, "measured receipt did not round-trip")
    return receipt_path, receipt


def route_report_is_append_only(before: Optional[dict[str, Any]], after: dict[str, Any]) -> bool:
    if before is None:
        return True
    before_attempts = before.get("attempts") if isinstance(before.get("attempts"), list) else []
    after_attempts = after.get("attempts") if isinstance(after.get("attempts"), list) else []
    return (
        after.get("session_id") == before.get("session_id")
        and after.get("started_utc") == before.get("started_utc")
        and len(after_attempts) >= len(before_attempts)
        and after_attempts[: len(before_attempts)] == before_attempts
    )


def launch_route_session(declaration_path: Path) -> tuple[Path, dict[str, Any], int]:
    preflight_result, declaration, context, policy = preflight(declaration_path)
    require(preflight_result["routeLaunchAllowed"],
            "route launch preflight failed: " + "; ".join(preflight_result["routeErrors"][:5]))
    before_route = inspect_route_report(context["routeReport"])
    before_report = before_route["report"] if before_route["valid"] else None
    before_attempts = before_route["attempts"]
    user_dir = context["userDir"]
    user_dir.parent.mkdir(parents=True, exist_ok=True)
    user_dir.mkdir(exist_ok=True)
    require(user_dir.is_dir() and not is_reparse(user_dir) and not path_chain_has_reparse(user_dir),
            "route UserDir is invalid after creation")
    launch_id = str(uuid.uuid4())
    launch_parent = ensure_snapshot_parent(context, "RouteLaunches")
    launch_dir = launch_parent / launch_id
    launch_dir.mkdir(exist_ok=False)
    archive_before = archive_identity(context["archive"], context["candidateId"])
    route_runtime_before = declaration["routeCaptureRuntime"]
    sanitized_args = guarded_arguments(
        policy, user_dir, context["routeUserDirToken"], True
    )
    intent = {
        "schema": ROUTE_INTENT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": context["candidateId"],
        "captureId": context["captureId"],
        "launchId": launch_id,
        "declaredUtc": utc_now(),
        "state": "PREDECLARED_INTERACTIVE_ROUTE_LAUNCH_NOT_STARTED",
        "declaration": declaration_binding(declaration_path),
        "candidateArchive": archive_before,
        "routeCaptureRuntime": declaration["routeCaptureRuntime"],
        "routeAttemptsBefore": before_attempts,
        "routeCountsBefore": before_route["counts"],
        "sanitizedArguments": sanitized_args,
        "claimBoundary": canonical_claim_boundary(),
    }
    intent_path = launch_dir / "LaunchIntent.json"
    try:
        shipping.write_exclusive_json(intent_path, intent)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"route launch intent publish failed: {exc}") from None

    executable = context["editorExecutable"]
    actual_args = guarded_arguments(
        policy, user_dir, context["routeUserDirToken"], False
    )
    started_utc = utc_now()
    pid: Optional[int] = None
    exit_code: Optional[int] = None
    launch_error: Optional[str] = None
    try:
        process = subprocess.Popen(
            [str(executable), *actual_args],
            cwd=str(ROOT),
        )
        pid = process.pid
        exit_code = process.wait()
    except OSError as exc:
        launch_error = f"process launch/wait failed ({exc.__class__.__name__})"
    finished_utc = utc_now()

    archive_post: Optional[dict[str, Any]] = None
    archive_unchanged = False
    route_runtime_post: Optional[dict[str, Any]] = None
    route_runtime_unchanged = False
    try:
        archive_post = archive_identity(context["archive"], context["candidateId"])
        archive_unchanged = archive_post == archive_before
    except CaptureError as exc:
        launch_error = launch_error or str(exc)
    try:
        route_runtime_post = route_runtime_bindings(
            context["editorExecutable"], context["archive"]
        )
        route_runtime_unchanged = route_runtime_post == route_runtime_before
    except CaptureError as exc:
        launch_error = launch_error or str(exc)
    after_route = inspect_route_report(context["routeReport"])
    after_report = after_route["report"] if after_route["valid"] else None
    logical_append_only = (
        after_report is not None and route_report_is_append_only(before_report, after_report)
    )
    attempts_advanced = after_route["attempts"] > before_attempts
    save_files = [
        str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav") if path.is_file()
    ]
    accepted = (
        launch_error is None
        and exit_code == 0
        and archive_unchanged
        and route_runtime_unchanged
        and after_route["valid"]
        and logical_append_only
        and attempts_advanced
        and not save_files
    )
    snapshot_binding: Optional[dict[str, Any]] = None
    receipt_binding: Optional[dict[str, Any]] = None
    if accepted:
        payload, report_source_binding = stable_read_bytes(
            context["routeReport"], "route report"
        )
        snapshot_path = launch_dir / "RouteTelemetrySnapshot.json"
        write_exclusive_bytes(snapshot_path, payload)
        copied = inspect_route_report(snapshot_path)
        require(copied["valid"] and copied["attempts"] == after_route["attempts"],
                "published route snapshot failed revalidation")
        snapshot_binding = stable_file_binding(snapshot_path, "published route snapshot")
        require(snapshot_binding["sha256"] == report_source_binding["sha256"],
                "published route snapshot differs from source")
        route_receipt = {
            "schema": ROUTE_RECEIPT_SCHEMA,
            "schemaVersion": 1,
            "session": 19,
            "candidateId": context["candidateId"],
            "captureId": context["captureId"],
            "launchId": launch_id,
            "snapshottedUtc": utc_now(),
            "state": "PASS_CANDIDATE_CONTEXT_BOUND_DEVELOPMENT_HUMAN_ROUTE_PROGRESS_SNAPSHOT_NO_APPROVAL_CLAIM",
            "declaration": declaration_binding(declaration_path),
            "launchIntent": stable_file_binding(intent_path, "route launch intent"),
            "sourceReport": report_source_binding,
            "snapshotReport": snapshot_binding,
            "attemptsBefore": before_attempts,
            "attemptsAfter": after_route["attempts"],
            "countsAfter": after_route["counts"],
            "routeEvidenceComplete": after_route["complete"],
            "claimBoundary": snapshot_claim_boundary(),
        }
        receipt_path = launch_dir / "RouteSnapshotReceipt.json"
        try:
            shipping.write_exclusive_json(receipt_path, route_receipt)
        except shipping.EvidenceError as exc:
            raise CaptureError(f"route snapshot receipt publish failed: {exc}") from None
        receipt_binding = stable_file_binding(receipt_path, "route snapshot receipt")

    log_binding: Optional[dict[str, Any]] = None
    log_path = user_dir / "Saved" / "Logs" / "DiscGolfTour.log"
    if log_path.is_file() and not is_reparse(log_path):
        log_payload, log_source_binding = stable_read_bytes(log_path, "route process log")
        copied_log = launch_dir / "DiscGolfTour.log"
        write_exclusive_bytes(copied_log, log_payload)
        log_binding = stable_file_binding(copied_log, "published route process log")
        require(log_binding["sha256"] == log_source_binding["sha256"],
                "published route log differs from source")

    result = {
        "schema": ROUTE_RESULT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": context["candidateId"],
        "captureId": context["captureId"],
        "launchId": launch_id,
        "startedUtc": started_utc,
        "finishedUtc": finished_utc,
        "state": (
            "PASS_CANDIDATE_CONTEXT_BOUND_DEVELOPMENT_ROUTE_PROGRESS_SNAPSHOT_CAPTURED"
            if accepted else "RECORDED_ROUTE_LAUNCH_NO_ACCEPTED_PROGRESS_SNAPSHOT"
        ),
        "pid": pid,
        "exitCode": exit_code,
        "launchError": launch_error,
        "sanitizedArguments": sanitized_args,
        "archiveBefore": archive_before,
        "archiveAfter": archive_post,
        "archiveUnchanged": archive_unchanged,
        "routeCaptureRuntimeAfter": route_runtime_post,
        "routeCaptureRuntimeUnchanged": route_runtime_unchanged,
        "routeReportValid": after_route["valid"],
        "routeReportAppendOnly": logical_append_only,
        "attemptsAdvanced": attempts_advanced,
        "attemptsBefore": before_attempts,
        "attemptsAfter": after_route["attempts"],
        "countsAfter": after_route["counts"],
        "routeEvidenceComplete": after_route["complete"],
        "forbiddenSaveFiles": save_files,
        "snapshotReport": snapshot_binding,
        "snapshotReceipt": receipt_binding,
        "processLog": log_binding,
        "claimBoundary": snapshot_claim_boundary(),
    }
    result_path = launch_dir / "LaunchResult.json"
    try:
        shipping.write_exclusive_json(result_path, result)
    except shipping.EvidenceError as exc:
        raise CaptureError(f"route launch result publish failed: {exc}") from None
    return result_path, result, 0 if accepted else 2


def run_self_test() -> int:
    checks = 0

    def check(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    policy = load_coordination_policy()
    measured_policy = load_measured_policy()
    slots = build_throw_slots(measured_policy)
    route_slots = build_route_slots(policy)
    check(len(slots) == 100, "throw slot count differs")
    check(sum(slot["partition"] == "fit" for slot in slots) == 75, "fit count differs")
    check(sum(slot["partition"] == "holdout" for slot in slots) == 25, "holdout count differs")
    check(len(route_slots) == 60, "route slot count differs")
    check(
        {slot["routeId"] for slot in route_slots} == set(routes.ROUTE_CONTRACT),
        "route identities differ",
    )

    with tempfile.TemporaryDirectory(prefix="dgt_physics_capture_selftest_") as temporary:
        temp_root = Path(temporary).resolve()
        candidate = "S19_WindowsShipping_20990101T000000Z_abcdef123456"
        archive = temp_root / candidate / "Windows"
        external = temp_root / "External"
        declaration_path = external / "Declarations" / "declaration.json"
        capture_id = "11111111-2222-4333-8444-555555555555"
        route_token = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
        fake_file = {
            "fileName": "fake.bin", "bytes": 10, "sha256": "A" * 64,
        }
        def fake_binding(path: str, name: str = "fake.bin") -> dict[str, Any]:
            return {
                "path": path,
                "fileName": name,
                "bytes": 10,
                "sha256": "A" * 64,
            }
        fake_archive = {
            "inventorySha256": "B" * 64,
            "fileCount": 31,
            "totalBytes": 1000,
            "launcher": {"relativePath": shipping.LAUNCHER_RELATIVE, **fake_file},
            "shippingExecutable": {"relativePath": shipping.EXECUTABLE_RELATIVE, **fake_file},
        }
        fake_route_runtime = {
            "runtimeType": "DevelopmentEditorGame",
            "shippingCandidateIsEmitter": False,
            "editorExecutable": fake_binding(str(temp_root / "Engine" / "UnrealEditor.exe"), "UnrealEditor.exe"),
            "engineBuildVersion": fake_binding(str(temp_root / "Engine" / "Build.version"), "Build.version"),
            "projectDescriptor": fake_binding("DiscGolfTour.uproject", "DiscGolfTour.uproject"),
            "runtimeModule": fake_binding("Binaries/Win64/UnrealEditor-DiscGolfTour.dll", "UnrealEditor-DiscGolfTour.dll"),
            "developerModule": fake_binding("Binaries/Win64/UnrealEditor-DiscGolfTourDeveloper.dll", "UnrealEditor-DiscGolfTourDeveloper.dll"),
            "foundationModule": fake_binding("Binaries/Win64/UnrealEditor-DiscGolfRuntimeFoundation.dll", "UnrealEditor-DiscGolfRuntimeFoundation.dll"),
            "editorTarget": fake_binding("Binaries/Win64/DiscGolfTourEditor.target", "DiscGolfTourEditor.target"),
            "routeData": {
                "course": fake_binding("Data/PineRidgeCourse.json", "PineRidgeCourse.json"),
                "hole2": fake_binding("Data/PineRidgeHole2.json", "PineRidgeHole2.json"),
                "presentation": fake_binding("Data/PineRidgePresentation.json", "PineRidgePresentation.json"),
            },
            "requiredCapabilityMarkers": list(ROUTE_CAPABILITY_MARKERS),
            "developmentCapabilityMarkersPresent": list(ROUTE_CAPABILITY_MARKERS),
            "shippingCandidateCapabilityMarkersAbsent": list(ROUTE_CAPABILITY_MARKERS),
            "sourceToBinaryAttestation": False,
        }
        declaration = compose_declaration(
            policy=policy,
            measured_policy=measured_policy,
            candidate_id=candidate,
            capture_id=capture_id,
            route_token=route_token,
            declared_utc="2026-08-26T00:00:00.000Z",
            archive=archive,
            archive_binding=fake_archive,
            external_root=external,
            declaration_path=declaration_path,
            bindings=source_bindings(),
            route_runtime=fake_route_runtime,
        )
        validate_declaration(
            declaration, declaration_path, policy, measured_policy, live=False
        )
        check(True, "valid declaration was rejected")

        mutations = (
            ("missing measured slot", lambda d: d["measuredReferencePlan"]["slots"].pop()),
            ("captured slot claim", lambda d: d["measuredReferencePlan"]["slots"][0].update(status="CAPTURED")),
            ("missing route slot", lambda d: d["routeTelemetryPlan"]["slots"].pop()),
            ("unattended argument", lambda d: d["sanitizedGuardedRouteArguments"].append("-unattended")),
            ("release claim", lambda d: d["claimBoundary"].update(releaseReady=True)),
            ("wrong UserDir token", lambda d: d["externalLayout"].update(routeUserDirToken=capture_id)),
            ("malformed archive hash", lambda d: d["candidateArchive"].update(inventorySha256="C" * 63)),
            ("Shipping emitter claim", lambda d: d["routeCaptureRuntime"].update(shippingCandidateIsEmitter=True)),
        )
        for label, mutate in mutations:
            candidate_declaration = copy.deepcopy(declaration)
            mutate(candidate_declaration)
            rejected = False
            try:
                validate_declaration(
                    candidate_declaration,
                    declaration_path,
                    policy,
                    measured_policy,
                    live=False,
                )
            except CaptureError:
                rejected = True
            check(rejected, f"mutation survived: {label}")

        complete_dataset = measured.make_valid_complete_fixture(measured_policy)
        progress = measured_progress(measured_policy, complete_dataset)
        check(progress["observedThrows"] == 100, "complete fixture count differs")
        check(not progress["overfilledCells"], "complete fixture overfilled a cell")

        before_report = routes.build_self_test_report(False)
        after_report = routes.build_self_test_report(True)
        before_report["session_id"] = after_report["session_id"]
        before_report["started_utc"] = after_report["started_utc"]
        check(route_report_is_append_only(before_report, after_report),
              "valid route extension was rejected")
        changed = copy.deepcopy(after_report)
        changed["attempts"][0]["release_speed_mps"] += 1.0
        check(not route_report_is_append_only(after_report, changed),
              "historical route-attempt mutation was accepted")

        payload_path = temp_root / "exclusive.bin"
        write_exclusive_bytes(payload_path, b"evidence")
        overwrite_rejected = False
        try:
            write_exclusive_bytes(payload_path, b"changed")
        except CaptureError:
            overwrite_rejected = True
        check(overwrite_rejected, "exclusive evidence write allowed overwrite")

    print(f"Physics capture coordinator self-test PASS: {checks}/{checks}")
    return 0


def print_preflight(result: dict[str, Any]) -> None:
    print(f"Physics capture preflight: {result['status']}")
    print(" - candidate/archive/source/declaration exact: true")
    print(" - route runtime: exact Development Editor game; Shipping candidate is context only")
    print(
        f" - route attempts: {result['routeAttempts']}/60; "
        f"counts={result['routeCounts']}; launchAllowed={str(result['routeLaunchAllowed']).lower()}"
    )
    if result["routeErrors"]:
        for error in result["routeErrors"]:
            print("   - " + error)
    if result["measuredDatasetInspected"]:
        progress = result["measuredProgress"]
        observed = progress["observedThrows"] if progress else 0
        print(
            f" - measured throws: {observed}/100; "
            f"valid={str(result['measuredDatasetValid']).lower()}; "
            f"complete={str(result['measuredCaptureComplete']).lower()}"
        )
        for error in result["measuredErrors"][:10]:
            print("   - " + error)
    else:
        print(" - measured throws: not inspected (no measurement claim inferred)")
    print(" - releaseReady: false")


def main() -> int:
    parser = argparse.ArgumentParser()
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--create-declaration", action="store_true")
    actions.add_argument("--preflight-only", action="store_true")
    actions.add_argument("--launch-route-session", action="store_true")
    actions.add_argument("--seal-measured-snapshot", type=Path)
    actions.add_argument("--self-test", action="store_true")
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--editor-executable", type=Path)
    parser.add_argument("--external-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--declaration", type=Path)
    parser.add_argument("--measured-dataset", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    try:
        if args.self_test:
            return run_self_test()
        if args.create_declaration:
            require(args.candidate_id is not None, "--candidate-id is required")
            require(args.archive is not None and args.archive.is_absolute(),
                    "absolute --archive is required")
            require(
                args.editor_executable is not None
                and args.editor_executable.is_absolute(),
                "absolute --editor-executable is required",
            )
            require(args.external_root is not None and args.external_root.is_absolute(),
                    "absolute --external-root is required")
            if args.output is not None:
                require(args.output.is_absolute(), "--output must be absolute")
            path, declaration = create_declaration(
                args.candidate_id,
                args.archive,
                args.editor_executable,
                args.external_root,
                args.output,
            )
            print(f"Physics capture declaration created: {path}")
            print(f" - captureId: {declaration['captureId']}")
            print(" - measured slots: 100 (75 fit / 25 holdout)")
            print(" - route slots: 60 (20 per route)")
            print(" - route runtime: bound Development Editor game; Shipping is context only")
            print(" - capture executed: false; approvals/releaseReady: false")
            return 0
        require(args.declaration is not None and args.declaration.is_absolute(),
                "absolute --declaration is required")
        if args.preflight_only:
            if args.measured_dataset is not None:
                require(args.measured_dataset.is_absolute(), "--measured-dataset must be absolute")
            result, _, _, _ = preflight(args.declaration, args.measured_dataset)
            if args.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            else:
                print_preflight(result)
            route_ok = result["routeLaunchAllowed"] or result["routeCaptureComplete"]
            measured_ok = (
                not result["measuredDatasetInspected"]
                or result["measuredDatasetValid"]
            )
            return 0 if route_ok and measured_ok else 2
        if args.seal_measured_snapshot is not None:
            require(args.seal_measured_snapshot.is_absolute(),
                    "--seal-measured-snapshot path must be absolute")
            path, receipt = seal_measured_snapshot(
                args.declaration, args.seal_measured_snapshot
            )
            print(f"Measured dataset snapshot sealed: {path}")
            print(
                f" - throws: {receipt['progress']['observedThrows']}/100; "
                f"completeValidatorPassed={str(receipt['completeValidatorPassed']).lower()}"
            )
            print(" - approvals/releaseReady granted by tool: false")
            return 0
        if args.launch_route_session:
            path, result, code = launch_route_session(args.declaration)
            print(f"Route launch result: {path}")
            print(
                f" - state: {result['state']}; attempts "
                f"{result['attemptsBefore']} -> {result['attemptsAfter']}"
            )
            print(" - approvals/releaseReady granted by tool: false")
            return code
        raise CaptureError("no action selected")
    except (CaptureError, OSError, RuntimeError, ValueError) as exc:
        print(f"Physics capture coordinator FAILED: {exc}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
