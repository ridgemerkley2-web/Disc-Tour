#!/usr/bin/env python3
"""Emit a deterministic, fail-closed v008 telemetry calibration receipt.

This preflight is deliberately read-only with respect to Unreal content.  It
reads the accepted DGMaster proxy source through Blender, derives geometry
measurements, binds the current source packages by SHA-256, and writes one
UNREVIEWED JSON receipt outside the repository.  It never launches Unreal,
authors assets, adds sockets, creates PhysicsAsset bodies, bakes curves, or
asserts a human approval.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any


SCHEMA = "DiscGolfTour.V008TelemetryCalibrationPreflight.v1"
ROOT = Path(__file__).resolve().parents[1]
BLEND = ROOT / "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.blend"
SOURCE_FILES = {
    "proxy_blend": BLEND,
    "proxy_fbx": ROOT / "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.fbx",
    "dgmaster_mesh": ROOT / "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    "dgmaster_skeleton": ROOT / "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    "normalized_sequence": ROOT / (
        "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/"
        "AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED.uasset"
    ),
}
UE_ASSET_PATHS = {
    "dgmaster_mesh": "/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master",
    "dgmaster_skeleton": "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master",
    "normalized_sequence": (
        "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/"
        "AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED."
        "AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED"
    ),
}
BODY_NAMES = (
    "pelvis", "spine_03", "head",
    "upperarm_l", "lowerarm_l", "hand_l", "thigh_l", "calf_l", "foot_l",
    "upperarm_r", "lowerarm_r", "hand_r", "thigh_r", "calf_r", "foot_r",
)
PHASE_NAMES = (
    "reachback", "plant", "pocket", "release",
    "followthrough", "recovery", "settle",
)
SOCKET_NAMES = (
    "DG_HeelContact_L", "DG_ToeContact_L",
    "DG_HeelContact_R", "DG_ToeContact_R",
)
PLACEHOLDERS = {
    "", "UNASSIGNED", "REPLACE", "REPLACE_ME", "TBD", "TODO", "UNKNOWN",
    "PENDING", "NONE", "NULL", "N/A", "NA",
}
BLENDER_SENTINEL = "DG_V008_CALIBRATION_GEOMETRY="


class PreflightError(RuntimeError):
    pass


def _rounded(value: float, digits: int = 9) -> float:
    result = round(float(value), digits)
    return 0.0 if result == 0.0 else result


def _canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def _digest(value: Any) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest().upper()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _source_bindings(files: dict[str, Path] = SOURCE_FILES) -> dict[str, Any]:
    bindings: dict[str, Any] = {}
    for name in sorted(files):
        path = files[name]
        if not path.is_file():
            raise PreflightError(f"required source file is missing: {path}")
        item: dict[str, Any] = {
            "absolute_path": str(path.resolve()),
            "byte_count": path.stat().st_size,
            "sha256": _sha256_file(path),
        }
        if name in UE_ASSET_PATHS:
            item["unreal_asset_path"] = UE_ASSET_PATHS[name]
        bindings[name] = item
    return bindings


def _blender_executable(explicit: Path | None = None) -> Path:
    candidates = []
    if explicit is not None:
        candidates.append(explicit)
    configured = os.environ.get("DG_BLENDER_EXE", "").strip()
    if configured:
        candidates.append(Path(configured))
    candidates.extend((
        Path(r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"),
        Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"),
        Path(r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"),
    ))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise PreflightError("Blender executable was not found; set DG_BLENDER_EXE")


def _extract_blender_geometry(blender: Path, blend: Path = BLEND) -> dict[str, Any]:
    # All geometry is emitted to stdout. Blender is never asked to save a file.
    expression = r'''
import bpy, json
mesh = next((obj for obj in bpy.data.objects if obj.type == "MESH" and obj.name == "SK_DG_Master_Proxy"), None)
if mesh is None:
    raise RuntimeError("SK_DG_Master_Proxy mesh object is missing")
groups = {}
for group in mesh.vertex_groups:
    points = []
    for vertex in mesh.data.vertices:
        weights = [entry.weight for entry in vertex.groups if entry.group == group.index and entry.weight > 0.0]
        if weights:
            point = mesh.matrix_world @ vertex.co
            points.append({"position_m": [float(point.x), float(point.y), float(point.z)], "weight": float(max(weights))})
    if points:
        groups[group.name] = points
all_influences = []
for vertex in mesh.data.vertices:
    all_influences.append([{"group": mesh.vertex_groups[entry.group].name, "weight": float(entry.weight)} for entry in vertex.groups if entry.weight > 0.0])
payload = {"blender_version": bpy.app.version_string, "mesh_name": mesh.name, "vertex_count": len(mesh.data.vertices), "groups": groups, "vertex_influences": all_influences}
print("DG_V008_CALIBRATION_GEOMETRY=" + json.dumps(payload, sort_keys=True, separators=(",", ":")))
'''
    result = subprocess.run(
        [str(blender), "--background", str(blend), "--python-expr", expression],
        cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    if result.returncode != 0:
        raise PreflightError(f"Blender geometry probe failed with exit {result.returncode}:\n{result.stdout[-4000:]}")
    lines = [line for line in result.stdout.splitlines() if line.startswith(BLENDER_SENTINEL)]
    if len(lines) != 1:
        raise PreflightError("Blender geometry probe did not emit exactly one calibration payload")
    try:
        return json.loads(lines[0][len(BLENDER_SENTINEL):])
    except json.JSONDecodeError as exc:
        raise PreflightError(f"Blender calibration payload is invalid JSON: {exc}") from exc


def _blender_to_ue_cm(point_m: list[float]) -> list[float]:
    if len(point_m) != 3 or any(not math.isfinite(float(value)) for value in point_m):
        raise PreflightError("proxy point must be three finite Blender-metre values")
    # Verified import convention for SK_DG_Master: Blender X -> UE X,
    # Blender -Y -> UE Y, Blender Z -> UE Z, metres -> centimetres.
    return [
        _rounded(float(point_m[0]) * 100.0),
        _rounded(-float(point_m[1]) * 100.0),
        _rounded(float(point_m[2]) * 100.0),
    ]


def _bbox(points: list[list[float]]) -> tuple[list[float], list[float]]:
    return (
        [_rounded(min(point[axis] for point in points)) for axis in range(3)],
        [_rounded(max(point[axis] for point in points)) for axis in range(3)],
    )


def _derive_measurements(raw: dict[str, Any]) -> dict[str, Any]:
    if raw.get("mesh_name") != "SK_DG_Master_Proxy" or raw.get("vertex_count") != 120:
        raise PreflightError("proxy mesh identity or vertex count differs from the frozen audit contract")
    groups = raw.get("groups")
    influences = raw.get("vertex_influences")
    if not isinstance(groups, dict) or set(groups) != set(BODY_NAMES):
        raise PreflightError(f"proxy body groups differ: {sorted(groups) if isinstance(groups, dict) else groups}")
    if not isinstance(influences, list) or len(influences) != 120:
        raise PreflightError("proxy vertex influence list is missing or incomplete")
    for index, entries in enumerate(influences):
        if not isinstance(entries, list) or len(entries) != 1:
            raise PreflightError(f"proxy vertex {index} is not assigned to exactly one body group")
        if abs(float(entries[0].get("weight", 0.0)) - 1.0) > 1e-9:
            raise PreflightError(f"proxy vertex {index} is not weighted exactly 1.0")

    bodies: list[dict[str, Any]] = []
    for bone in BODY_NAMES:
        entries = groups[bone]
        if not isinstance(entries, list) or len(entries) != 8:
            raise PreflightError(f"proxy group {bone} does not contain exactly eight cuboid corners")
        if any(abs(float(entry.get("weight", 0.0)) - 1.0) > 1e-9 for entry in entries):
            raise PreflightError(f"proxy group {bone} contains a non-unit skin weight")
        points = [_blender_to_ue_cm(entry["position_m"]) for entry in entries]
        minimum, maximum = _bbox(points)
        center = [_rounded((minimum[i] + maximum[i]) * 0.5) for i in range(3)]
        size = [_rounded(maximum[i] - minimum[i]) for i in range(3)]
        volume = _rounded(size[0] * size[1] * size[2], 6)
        if min(size) <= 0.0 or volume <= 0.0:
            raise PreflightError(f"proxy group {bone} has a degenerate cuboid")
        expected = {
            tuple(_rounded(value) for value in corner)
            for corner in itertools.product(*zip(minimum, maximum))
        }
        actual = {tuple(point) for point in points}
        if actual != expected:
            raise PreflightError(f"proxy group {bone} is not an axis-aligned eight-corner cuboid")
        bodies.append({
            "bone": bone,
            "candidate_only": True,
            "source_vertex_count": 8,
            "source_skin_weight": 1.0,
            "bounds_min_ue_cm": minimum,
            "bounds_max_ue_cm": maximum,
            "center_ue_cm": center,
            "box_size_ue_cm": size,
            "box_volume_cm3": volume,
        })

    total_volume = _rounded(sum(body["box_volume_cm3"] for body in bodies), 6)
    uniform_com = [
        _rounded(sum(body["box_volume_cm3"] * body["center_ue_cm"][axis] for body in bodies) / total_volume)
        for axis in range(3)
    ]
    for body in bodies:
        body["uniform_density_mass_fraction"] = _rounded(body["box_volume_cm3"] / total_volume, 12)

    feet: dict[str, Any] = {}
    for side, bone in (("L", "foot_l"), ("R", "foot_r")):
        body = next(item for item in bodies if item["bone"] == bone)
        minimum = body["bounds_min_ue_cm"]
        maximum = body["bounds_max_ue_cm"]
        z = minimum[2]
        corners = []
        for x_label, x in (("x_min", minimum[0]), ("x_max", maximum[0])):
            for y_label, y in (("y_min", minimum[1]), ("y_max", maximum[1])):
                corners.append({
                    "candidate_id": f"{side}_{x_label}_{y_label}_sole",
                    "component_reference_ue_cm": [x, y, z],
                    "semantic": "UNASSIGNED",
                })
        feet[side] = {
            "bone": bone,
            "candidate_only": True,
            "sole_plane_reference_z_ue_cm": z,
            "sole_corners": corners,
            "longitudinal_edge_center_candidates": [
                {
                    "candidate_id": f"{side}_y_min_edge_center",
                    "component_reference_ue_cm": [
                        _rounded((minimum[0] + maximum[0]) * 0.5), minimum[1], z,
                    ],
                    "semantic": "UNASSIGNED",
                },
                {
                    "candidate_id": f"{side}_y_max_edge_center",
                    "component_reference_ue_cm": [
                        _rounded((minimum[0] + maximum[0]) * 0.5), maximum[1], z,
                    ],
                    "semantic": "UNASSIGNED",
                },
            ],
        }

    return {
        "measurement_space": "DGMASTER_REFERENCE_COMPONENT_SPACE_UE_CENTIMETERS",
        "coordinate_conversion": {
            "source": "BLENDER_OBJECT_WORLD_METERS",
            "target": "UNREAL_REFERENCE_COMPONENT_CENTIMETERS",
            "formula": "UE_CM=[BLENDER_X*100,-BLENDER_Y*100,BLENDER_Z*100]",
        },
        "source_mesh_name": raw["mesh_name"],
        "source_vertex_count": raw["vertex_count"],
        "body_candidate_count": len(bodies),
        "physics_body_candidates": bodies,
        "uniform_density_proxy_model": {
            "candidate_only": True,
            "not_performer_anatomical_com": True,
            "total_box_volume_cm3": total_volume,
            "reference_com_ue_cm": uniform_com,
        },
        "foot_contact_candidates": feet,
        "semantic_warning": (
            "These are exact proxy cuboid measurements, not approved heel/toe semantics, "
            "performer anatomy, contact state, or PhysicsAsset authoring instructions."
        ),
    }


def _blank_approvals() -> dict[str, Any]:
    return {
        "socket_semantics": {
            "reviewer_name": "",
            "reviewed_at_utc": "",
            "decision": "",
            "approved_parent_policy": "",
            "approved_component_reference_points_ue_cm": {
                name: None for name in SOCKET_NAMES
            },
        },
        "mass_model": {
            "reviewer_name": "",
            "reviewed_at_utc": "",
            "decision": "",
            "approved_model_id": "",
            "physics_body_set_approved": None,
            "performer_anatomical_claim_allowed": None,
        },
        "ground_and_stage_transform": {
            "reviewer_name": "",
            "reviewed_at_utc": "",
            "decision": "",
            "stage_frame_id": "",
            "component_to_stage_world_transform": None,
            "ground_plane_world": None,
            "target_forward_world": None,
        },
        "contact_thresholds": {
            "reviewer_name": "",
            "reviewed_at_utc": "",
            "decision": "",
            "ground_contact_tolerance_cm": None,
            "maximum_contact_vertical_speed_cm_per_second": None,
            "support_footprint_policy": "",
        },
        "seven_phase_frames": {
            "reviewer_name": "",
            "reviewed_at_utc": "",
            "decision": "",
            "phase_basis": "",
            "frames": {name: None for name in PHASE_NAMES},
        },
    }


def _placeholder(value: Any) -> bool:
    return not isinstance(value, str) or value.strip().upper() in PLACEHOLDERS


def _approval_blockers(approvals: Any) -> list[str]:
    blockers: list[str] = []
    if not isinstance(approvals, dict):
        return ["APPROVALS_OBJECT_MISSING"]
    expected_sections = set(_blank_approvals())
    if set(approvals) != expected_sections:
        blockers.append("APPROVAL_SECTION_SET_DIFFERS")
    for section_name in sorted(expected_sections):
        section = approvals.get(section_name)
        if not isinstance(section, dict):
            blockers.append(f"{section_name}:SECTION_MISSING")
            continue
        for field in ("reviewer_name", "reviewed_at_utc", "decision"):
            if _placeholder(section.get(field)):
                blockers.append(f"{section_name}:{field.upper()}_MISSING_OR_PLACEHOLDER")

    socket = approvals.get("socket_semantics", {})
    points = socket.get("approved_component_reference_points_ue_cm") if isinstance(socket, dict) else None
    if _placeholder(socket.get("approved_parent_policy") if isinstance(socket, dict) else None):
        blockers.append("socket_semantics:APPROVED_PARENT_POLICY_MISSING_OR_PLACEHOLDER")
    if not isinstance(points, dict) or set(points) != set(SOCKET_NAMES):
        blockers.append("socket_semantics:APPROVED_POINT_SET_DIFFERS")
    elif any(not isinstance(points[name], list) or len(points[name]) != 3 for name in SOCKET_NAMES):
        blockers.append("socket_semantics:APPROVED_POINTS_INCOMPLETE")

    mass = approvals.get("mass_model", {})
    if _placeholder(mass.get("approved_model_id") if isinstance(mass, dict) else None):
        blockers.append("mass_model:APPROVED_MODEL_ID_MISSING_OR_PLACEHOLDER")
    if not isinstance(mass, dict) or mass.get("physics_body_set_approved") is not True:
        blockers.append("mass_model:PHYSICS_BODY_SET_NOT_APPROVED")
    if not isinstance(mass, dict) or not isinstance(mass.get("performer_anatomical_claim_allowed"), bool):
        blockers.append("mass_model:ANATOMICAL_CLAIM_POLICY_MISSING")

    stage = approvals.get("ground_and_stage_transform", {})
    for field in (
        "stage_frame_id", "component_to_stage_world_transform",
        "ground_plane_world", "target_forward_world",
    ):
        value = stage.get(field) if isinstance(stage, dict) else None
        if value is None or (isinstance(value, str) and _placeholder(value)):
            blockers.append(f"ground_and_stage_transform:{field.upper()}_MISSING")

    contact = approvals.get("contact_thresholds", {})
    for field in ("ground_contact_tolerance_cm", "maximum_contact_vertical_speed_cm_per_second"):
        value = contact.get(field) if isinstance(contact, dict) else None
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)) or value < 0:
            blockers.append(f"contact_thresholds:{field.upper()}_MISSING_OR_INVALID")
    if _placeholder(contact.get("support_footprint_policy") if isinstance(contact, dict) else None):
        blockers.append("contact_thresholds:SUPPORT_FOOTPRINT_POLICY_MISSING_OR_PLACEHOLDER")

    phase = approvals.get("seven_phase_frames", {})
    frames = phase.get("frames") if isinstance(phase, dict) else None
    if phase.get("phase_basis") != "MANUALLY_REVIEWED_DGMASTER_NORMALIZED_FRAME_INDICES" if isinstance(phase, dict) else True:
        blockers.append("seven_phase_frames:PHASE_BASIS_MISSING_OR_INVALID")
    if not isinstance(frames, dict) or set(frames) != set(PHASE_NAMES):
        blockers.append("seven_phase_frames:FRAME_SET_DIFFERS")
    else:
        values = [frames[name] for name in PHASE_NAMES]
        if any(isinstance(value, bool) or not isinstance(value, int) or value < 0 for value in values):
            blockers.append("seven_phase_frames:FRAMES_INCOMPLETE_OR_INVALID")
        elif values != sorted(values) or len(set(values)) != len(values):
            blockers.append("seven_phase_frames:FRAMES_NOT_UNIQUE_ORDERED")
    return sorted(set(blockers))


def _build_receipt(
    bindings: dict[str, Any], measurements: dict[str, Any],
    blender: Path, blender_version: str,
) -> dict[str, Any]:
    approvals = _blank_approvals()
    blockers = _approval_blockers(approvals)
    if not blockers:
        raise PreflightError("blank approval template unexpectedly passed readiness validation")
    measurement_digest = _digest(measurements)
    return {
        "schema": SCHEMA,
        "status": "UNREVIEWED_CALIBRATION_NOT_READY_FOR_ASSET_AUTHORING",
        "ready_for_asset_authoring": False,
        "reviewed": False,
        "diagnostic_only": True,
        "production_promotion_allowed": False,
        "unreal_assets_authored": False,
        "animation_curves_baked": False,
        "unreal_was_launched": False,
        "source_bindings": bindings,
        "geometry_probe": {
            "executable_absolute_path": str(blender.resolve()),
            "blender_version": blender_version,
            "read_only_background_probe": True,
        },
        "measurements": measurements,
        "measurement_digest_sha256": measurement_digest,
        "approvals": approvals,
        "approval_blockers": blockers,
        "next_action": (
            "A named reviewer must replace every blank approval field using human/pose evidence. "
            "This receipt itself must never be edited into an approval or used for runtime promotion."
        ),
    }


def _output_is_outside_repo(output: Path, root: Path = ROOT) -> bool:
    resolved_output = output.resolve()
    resolved_root = root.resolve()
    try:
        return os.path.commonpath((str(resolved_output), str(resolved_root))) != str(resolved_root)
    except ValueError:
        return True


def _write_new_receipt(output: Path, receipt: dict[str, Any]) -> str:
    if not _output_is_outside_repo(output):
        raise PreflightError(f"receipt output must be outside the repository: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(receipt, indent=2, sort_keys=True, ensure_ascii=False, allow_nan=False) + "\n"
    try:
        with output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(payload)
    except FileExistsError as exc:
        raise PreflightError(f"append-only receipt target already exists: {output}") from exc
    return hashlib.sha256(payload.encode("utf-8")).hexdigest().upper()


def _geometry_fixture() -> dict[str, Any]:
    dimensions = {
        "pelvis": (24.0, 12.0, 10.0, (0.0, 0.0, 98.0)),
        "spine_03": (28.0, 13.0, 30.0, (0.0, 0.0, 132.0)),
        "head": (11.0, 10.0, 13.0, (0.0, 0.0, 170.0)),
        "upperarm_l": (15.0, 7.0, 7.0, (34.0, 0.0, 148.0)),
        "lowerarm_l": (13.0, 5.5, 5.5, (62.0, 0.0, 146.5)),
        "hand_l": (6.0, 5.0, 3.5, (81.0, 0.0, 146.0)),
        "thigh_l": (9.0, 9.0, 21.0, (10.0, 0.0, 76.0)),
        "calf_l": (7.0, 7.0, 21.0, (10.0, 0.0, 34.0)),
        "foot_l": (7.0, 14.0, 5.5, (10.0, 9.0, 8.0)),
        "upperarm_r": (15.0, 7.0, 7.0, (-34.0, 0.0, 148.0)),
        "lowerarm_r": (13.0, 5.5, 5.5, (-62.0, 0.0, 146.5)),
        "hand_r": (6.0, 5.0, 3.5, (-81.0, 0.0, 146.0)),
        "thigh_r": (9.0, 9.0, 21.0, (-10.0, 0.0, 76.0)),
        "calf_r": (7.0, 7.0, 21.0, (-10.0, 0.0, 34.0)),
        "foot_r": (7.0, 14.0, 5.5, (-10.0, 9.0, 8.0)),
    }
    groups: dict[str, Any] = {}
    influences = []
    for bone in BODY_NAMES:
        sx, sy, sz, center = dimensions[bone]
        points = []
        for x in (center[0] - sx / 2.0, center[0] + sx / 2.0):
            for y in (center[1] - sy / 2.0, center[1] + sy / 2.0):
                for z in (center[2] - sz / 2.0, center[2] + sz / 2.0):
                    # Invert the production conversion to create Blender metres.
                    points.append({"position_m": [x / 100.0, -y / 100.0, z / 100.0], "weight": 1.0})
                    influences.append([{"group": bone, "weight": 1.0}])
        groups[bone] = points
    return {
        "blender_version": "SELF_TEST",
        "mesh_name": "SK_DG_Master_Proxy",
        "vertex_count": 120,
        "groups": groups,
        "vertex_influences": influences,
    }


def _self_test() -> tuple[int, int]:
    checks: list[tuple[str, bool]] = []
    fixture = _geometry_fixture()
    first = _derive_measurements(fixture)
    second = _derive_measurements(fixture)
    checks.append(("schema_constant", SCHEMA == "DiscGolfTour.V008TelemetryCalibrationPreflight.v1"))
    checks.append(("measurement_determinism", _canonical_bytes(first) == _canonical_bytes(second)))
    checks.append(("measurement_hash_determinism", _digest(first) == _digest(second)))
    checks.append(("body_count", first["body_candidate_count"] == 15))
    checks.append(("proxy_volume", first["uniform_density_proxy_model"]["total_box_volume_cm3"] == 24234.5))
    checks.append(("sole_corner_count", all(len(first["foot_contact_candidates"][side]["sole_corners"]) == 4 for side in ("L", "R"))))

    blank = _blank_approvals()
    blank_blockers = _approval_blockers(blank)
    checks.append(("blank_approvals_blocked", bool(blank_blockers)))
    checks.append(("blank_named_reviewer_blocked", any("REVIEWER_NAME" in blocker for blocker in blank_blockers)))
    checks.append(("blank_phases_blocked", any("FRAMES_INCOMPLETE" in blocker for blocker in blank_blockers)))

    placeholder = _blank_approvals()
    for section in placeholder.values():
        section["reviewer_name"] = "UNASSIGNED"
        section["reviewed_at_utc"] = "TBD"
        section["decision"] = "REPLACE_ME"
    placeholder_blockers = _approval_blockers(placeholder)
    checks.append(("placeholder_approvals_blocked", bool(placeholder_blockers)))
    checks.append(("placeholder_named_reviewer_blocked", any("REVIEWER_NAME" in blocker for blocker in placeholder_blockers)))

    with tempfile.TemporaryDirectory() as folder:
        base = Path(folder)
        fake_files = {}
        for index, name in enumerate(sorted(SOURCE_FILES)):
            path = base / f"{name}.bin"
            path.write_bytes(f"fixture-{index}".encode("ascii"))
            fake_files[name] = path
        bindings_a = _source_bindings(fake_files)
        bindings_b = _source_bindings(fake_files)
        checks.append(("source_hash_determinism", _canonical_bytes(bindings_a) == _canonical_bytes(bindings_b)))
        receipt_a = _build_receipt(bindings_a, first, base / "blender.exe", "SELF_TEST")
        receipt_b = _build_receipt(bindings_b, second, base / "blender.exe", "SELF_TEST")
        checks.append(("receipt_determinism", _canonical_bytes(receipt_a) == _canonical_bytes(receipt_b)))
        checks.append(("receipt_never_ready", receipt_a["ready_for_asset_authoring"] is False and receipt_a["reviewed"] is False))
        checks.append(("receipt_never_promotes", receipt_a["production_promotion_allowed"] is False and receipt_a["unreal_assets_authored"] is False))
        checks.append(("repo_output_rejected", not _output_is_outside_repo(ROOT / "Saved/forbidden.json")))

    failed = [name for name, passed in checks if not passed]
    if failed:
        for name in failed:
            print(f"SELF_TEST_FAIL {name}", file=sys.stderr)
    return len(checks) - len(failed), len(checks)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--output", type=Path, help="new receipt path outside the repository")
    parser.add_argument("--blender", type=Path, help="explicit Blender executable")
    args = parser.parse_args()
    if args.self_test:
        passed, total = _self_test()
        print(f"SELF_TEST {passed}/{total}")
        return 0 if passed == total else 1
    if args.output is None:
        parser.error("--output is required unless --self-test is used")
    try:
        blender = _blender_executable(args.blender)
        raw = _extract_blender_geometry(blender)
        measurements = _derive_measurements(raw)
        bindings = _source_bindings()
        receipt = _build_receipt(bindings, measurements, blender, str(raw.get("blender_version", "")))
        receipt_hash = _write_new_receipt(args.output, receipt)
    except (OSError, ValueError, PreflightError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps({
        "status": receipt["status"],
        "ready_for_asset_authoring": False,
        "output": str(args.output.resolve()),
        "receipt_sha256": receipt_hash,
        "measurement_digest_sha256": receipt["measurement_digest_sha256"],
        "approval_blocker_count": len(receipt["approval_blockers"]),
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
