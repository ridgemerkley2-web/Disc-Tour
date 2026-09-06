"""UE 5.8 read-only extractor for the expected v008 DGMaster RAW sequence.

Run only through ``run_dg_v008_post_retarget_telemetry.py``.  The extractor
uses AnimSequence.get_anim_pose_at_time with RAW evaluation at exact 60 Hz.
It refuses alternate assets, missing bones/sockets, or absent COM/support
curves. It does not save or modify any Unreal package.
"""

import json
import math
import os
from pathlib import Path

import unreal


EXPECTED_ASSET_PATH = "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED"
MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SCHEMA = "DiscGolfTour.UnrealWorldPoseExport.v1"
BONES = (
    "pelvis", "spine_03", "head", "thigh_l", "calf_l", "foot_l",
    "thigh_r", "calf_r", "foot_r", "upperarm_r", "lowerarm_r", "hand_r",
)
SOCKETS = {
    "DG_HeelContact_L": "foot_l", "DG_ToeContact_L": "ball_l",
    "DG_HeelContact_R": "foot_r", "DG_ToeContact_R": "ball_r",
}
CURVES = ("DG_COM_X_CM", "DG_COM_Y_CM", "DG_COM_Z_CM", "DG_SupportPolygonMarginCm")


def _finite(value, label):
    result = float(value)
    if not math.isfinite(result):
        raise RuntimeError(f"{label} is not finite")
    return result


def _quat(value):
    values = [_finite(value.x, "quat.x"), _finite(value.y, "quat.y"),
              _finite(value.z, "quat.z"), _finite(value.w, "quat.w")]
    norm = math.sqrt(sum(item * item for item in values))
    if abs(norm - 1.0) > 1e-3:
        raise RuntimeError(f"evaluated quaternion is not normalized: {norm}")
    return [item / norm for item in values]


def _translation(value):
    return [_finite(value.x, "translation.x"), _finite(value.y, "translation.y"), _finite(value.z, "translation.z")]


def _rotate(q, v):
    x, y, z, w = q
    tx, ty, tz = 2.0 * (y * v[2] - z * v[1]), 2.0 * (z * v[0] - x * v[2]), 2.0 * (x * v[1] - y * v[0])
    return [v[0] + w * tx + (y * tz - z * ty),
            v[1] + w * ty + (z * tx - x * tz),
            v[2] + w * tz + (x * ty - y * tx)]


def _socket_local_location(socket, label):
    location = socket.get_editor_property("relative_location")
    return [_finite(location.x, f"{label}.x"), _finite(location.y, f"{label}.y"), _finite(location.z, f"{label}.z")]


def _curve(pose, name):
    getter = getattr(pose, "get_curve_weight", None)
    names_getter = getattr(pose, "get_curve_names", None)
    if getter is None or names_getter is None:
        raise RuntimeError("AnimPose curve API is unavailable; COM/support cannot be fabricated")
    names = {str(value) for value in names_getter()}
    if name not in names:
        raise RuntimeError(f"required evaluated curve is missing: {name}")
    return _finite(getter(name), name)


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    contract_path = Path(os.environ.get("DG_V008_EXTRACTION_CONTRACT", str(project / "SourceArt/DiscGolf/Motion/V008PostRetargetTelemetryContract.json")))
    output_path = Path(os.environ.get("DG_V008_UNREAL_EXPORT", str(project / "Saved/AnimationFluidity/v008_post_retarget_unreal.json")))
    if not contract_path.is_file():
        raise RuntimeError(f"BLOCKED_EXTRACTION_CONTRACT_MISSING: {contract_path}")
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    if contract.get("schema") != "DiscGolfTour.V008TelemetryExtractionContract.v1" or contract.get("asset_path") != EXPECTED_ASSET_PATH:
        raise RuntimeError("extraction contract schema or immutable asset_path differs")
    if contract.get("sample_rate_hz") != 60:
        raise RuntimeError("extraction contract sample_rate_hz must equal 60")
    review = contract.get("review")
    if not isinstance(review, dict) or review.get("status") != "APPROVED_FOR_TECHNICAL_DIAGNOSTIC_EXTRACTION":
        raise RuntimeError("extraction contract is not explicitly reviewed/approved")
    if not isinstance(review.get("reviewer"), str) or review["reviewer"].strip().upper() in ("", "UNASSIGNED", "REPLACE"):
        raise RuntimeError("extraction contract has no named reviewer")
    if not isinstance(review.get("reviewed_at_utc"), str) or not review["reviewed_at_utc"].endswith("Z"):
        raise RuntimeError("extraction contract has no UTC review timestamp")
    if review.get("phase_basis") != "MANUALLY_REVIEWED_DGMASTER_NORMALIZED_FRAME_INDICES":
        raise RuntimeError("extraction contract phase review basis differs")
    phases = contract.get("phases")
    phase_names = ("reachback", "plant", "pocket", "release", "followthrough", "recovery", "settle")
    if not isinstance(phases, dict) or set(phases) != set(phase_names):
        raise RuntimeError("extraction contract phase set differs")
    phase_values = [phases[name] for name in phase_names]
    if any(isinstance(value, bool) or not isinstance(value, int) or value < 0 for value in phase_values) or phase_values != sorted(phase_values) or len(set(phase_values)) != len(phase_values):
        raise RuntimeError("extraction contract phases must be unique ordered nonnegative frames")
    if contract.get("required_curves") != list(CURVES) or contract.get("required_sockets") != SOCKETS:
        raise RuntimeError("extraction contract required curves/sockets differ")

    asset_path = contract["asset_path"]
    sequence = unreal.load_asset(asset_path)
    mesh = unreal.load_asset(MESH_PATH)
    if sequence is None:
        raise RuntimeError(f"BLOCKED_EXPECTED_DGMASTER_NORMALIZED_ASSET_MISSING: {asset_path}")
    if mesh is None:
        raise RuntimeError(f"BLOCKED_DGMASTER_MESH_MISSING: {MESH_PATH}")
    skeleton = mesh.get_editor_property("skeleton")
    reference_names = {str(value) for value in skeleton.get_reference_pose().get_bone_names()}
    missing_bones = sorted((set(BONES) | set(SOCKETS.values())) - reference_names)
    if missing_bones:
        raise RuntimeError(f"required DGMaster bones missing: {missing_bones}")
    socket_data = {}
    for name, required_parent in SOCKETS.items():
        socket = mesh.find_socket(name)
        if socket is None:
            raise RuntimeError(f"required evaluated contact socket missing: {name}")
        parent = str(socket.get_editor_property("bone_name"))
        if parent != required_parent:
            raise RuntimeError(f"socket {name} parent {parent} != {required_parent}")
        socket_data[name] = (parent, _socket_local_location(socket, name))

    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    options.set_editor_property("optional_skeletal_mesh", mesh)
    options.set_editor_property("should_retarget", False)
    duration = _finite(sequence.get_play_length(), "sequence duration")
    count = int(math.floor(duration * 60.0 + 1e-6)) + 1
    frames = []
    for index in range(count):
        time_seconds = min(index / 60.0, duration)
        pose = sequence.get_anim_pose_at_time(time_seconds, options)
        if not pose.is_valid():
            raise RuntimeError(f"invalid RAW AnimPose at sample {index}")
        transforms = {}
        world = {}
        for bone in set(BONES) | set(SOCKETS.values()):
            value = pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD)
            scale = _translation(value.scale3d)
            if any(abs(component - 1.0) > 1e-4 for component in scale):
                raise RuntimeError(f"non-unit evaluated scale prevents exact socket reconstruction: {bone}={scale}")
            world[bone] = (_translation(value.translation), _quat(value.rotation))
        for bone in BONES:
            transforms[bone] = {"translation_cm": world[bone][0], "rotation_xyzw": world[bone][1]}
        for name, (parent, local) in socket_data.items():
            parent_t, parent_q = world[parent]
            rotated = _rotate(parent_q, local)
            transforms[name] = {"translation_cm": [parent_t[i] + rotated[i] for i in range(3)], "rotation_xyzw": parent_q}
        frames.append({
            "frame": index, "time_seconds": index / 60.0, "transforms": transforms,
            "center_of_mass_world_cm": [_curve(pose, CURVES[0]), _curve(pose, CURVES[1]), _curve(pose, CURVES[2])],
            "support_polygon_margin_cm": _curve(pose, CURVES[3]),
        })
    payload = {
        "schema": SCHEMA, "space": "WORLD", "distance_unit": "CENTIMETER", "quaternion_order": "XYZW",
        "frame_rate": 60, "handedness": "RHBH", "throw_axes_world": contract["throw_axes_world"],
        "ground_z_cm": contract["ground_z_cm"], "phases": contract["phases"], "frames": frames,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    unreal.log(f"DG_V008_POST_RETARGET_UNREAL_EXPORT={output_path}")


main()
