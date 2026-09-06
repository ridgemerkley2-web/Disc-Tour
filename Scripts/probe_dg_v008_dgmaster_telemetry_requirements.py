"""UE-only, read-only probe for v008 DGMaster telemetry prerequisites.

The probe writes diagnostics under Saved only. It never creates, saves, or
modifies Unreal assets and never substitutes computed contact/COM data.
"""

import json
import os
from pathlib import Path

import unreal


ASSET_PATH = "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED"
MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SOCKETS = {"DG_HeelContact_L": "foot_l", "DG_ToeContact_L": "ball_l", "DG_HeelContact_R": "foot_r", "DG_ToeContact_R": "ball_r"}
CURVES = ("DG_COM_X_CM", "DG_COM_Y_CM", "DG_COM_Z_CM", "DG_SupportPolygonMarginCm")


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    output = Path(os.environ.get("DG_V008_REQUIREMENTS_PROBE", str(project / "Saved/AnimationFluidity/V008DGMasterTelemetryRequirements.json")))
    blockers = []
    sequence = unreal.load_asset(ASSET_PATH)
    mesh = unreal.load_asset(MESH_PATH)
    report = {"schema": "DiscGolfTour.V008DGMasterTelemetryRequirementsProbe.v1", "asset_path": ASSET_PATH,
              "mesh_path": MESH_PATH, "read_only": True, "sockets": {}, "curves": {}, "physics_asset": {}}
    if sequence is None: blockers.append(f"EXPECTED_NORMALIZED_ASSET_MISSING: {ASSET_PATH}")
    if mesh is None: blockers.append(f"DGMASTER_MESH_MISSING: {MESH_PATH}")
    if mesh is not None:
        physics_asset = mesh.get_editor_property("physics_asset")
        report["physics_asset"] = {"available": physics_asset is not None, "path": physics_asset.get_path_name() if physics_asset else None}
        if physics_asset is None: blockers.append("DGMASTER_PHYSICS_ASSET_MISSING")
        finder = getattr(mesh, "find_socket", None)
        if finder is None: blockers.append("SKELETAL_MESH_SOCKET_API_UNAVAILABLE")
        else:
            for name, expected_parent in SOCKETS.items():
                socket = finder(name)
                parent = str(socket.get_editor_property("bone_name")) if socket else None
                valid = socket is not None and parent == expected_parent
                report["sockets"][name] = {"available": socket is not None, "parent": parent, "expected_parent": expected_parent, "valid": valid}
                if not valid: blockers.append(f"CONTACT_SOCKET_MISSING_OR_WRONG_PARENT: {name}")
    if sequence is not None and mesh is not None:
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
        options.set_editor_property("optional_skeletal_mesh", mesh)
        options.set_editor_property("should_retarget", False)
        pose = sequence.get_anim_pose_at_time(0.0, options)
        names_getter = getattr(pose, "get_curve_names", None)
        weight_getter = getattr(pose, "get_curve_weight", None)
        if not pose.is_valid(): blockers.append("RAW_ANIMPOSE_INVALID_AT_TIME_ZERO")
        if names_getter is None or weight_getter is None: blockers.append("ANIMPOSE_CURVE_API_UNAVAILABLE")
        else:
            names = {str(value) for value in names_getter()}
            for name in CURVES:
                available = name in names
                report["curves"][name] = {"available": available, "sample_zero": float(weight_getter(name)) if available else None}
                if not available: blockers.append(f"MEASURED_TELEMETRY_CURVE_MISSING: {name}")
    report["blockers"] = blockers
    report["status"] = "PASS_REQUIREMENTS_AVAILABLE" if not blockers else "BLOCKED_REQUIREMENTS_MISSING"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"DG_V008_REQUIREMENTS_PROBE={output} status={report['status']}")


main()
