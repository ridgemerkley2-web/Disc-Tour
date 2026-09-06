#!/usr/bin/env python3
"""Convert canonical Unreal pose export JSON to the v008 biomechanics schema.

Required Unreal transform channels (world space, centimetres):
  pelvis, spine_03, head,
  thigh_l, calf_l, foot_l, DG_HeelContact_L, DG_ToeContact_L,
  thigh_r, calf_r, foot_r, DG_HeelContact_R, DG_ToeContact_R,
  upperarm_r, lowerarm_r, hand_r.

``DG_HeelContact_*`` and ``DG_ToeContact_*`` are evaluated sockets/virtual
markers, not inferred foot bounds. Every frame also requires Unreal-exported
``center_of_mass_world_cm`` and signed ``support_polygon_margin_cm``. Positive
margin is inside the active support polygon; negative is outside. This adapter
never fabricates missing channels or biomechanical values.
"""

from __future__ import annotations

import argparse
import copy
import json
import math
from pathlib import Path
import sys
from typing import Any, Callable

import validate_dg_v008_world_biomechanics as biomechanics


SOURCE_SCHEMA = "DiscGolfTour.UnrealWorldPoseExport.v1"
TARGET_SCHEMA = biomechanics.SCHEMA
CHANNEL_TO_JOINT = {
    "pelvis": "pelvis", "spine_03": "thorax", "head": "head",
    "thigh_l": "hip_l", "calf_l": "knee_l", "foot_l": "ankle_l",
    "DG_HeelContact_L": "heel_l", "DG_ToeContact_L": "toe_l",
    "thigh_r": "hip_r", "calf_r": "knee_r", "foot_r": "ankle_r",
    "DG_HeelContact_R": "heel_r", "DG_ToeContact_R": "toe_r",
    "upperarm_r": "shoulder_r", "lowerarm_r": "elbow_r", "hand_r": "hand_r",
}
REQUIRED_CHANNELS = tuple(CHANNEL_TO_JOINT)


class ContractError(ValueError):
    pass


def _number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ContractError(f"{label} must be a finite number")
    return float(value)


def _vec(value: Any, count: int, label: str) -> tuple[float, ...]:
    if not isinstance(value, list) or len(value) != count:
        raise ContractError(f"{label} must contain exactly {count} numbers")
    return tuple(_number(item, f"{label}[{index}]") for index, item in enumerate(value))


def _sub(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, ...]:
    return tuple(x - y for x, y in zip(a, b))


def _dot(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return sum(x * y for x, y in zip(a, b))


def _length(value: tuple[float, ...]) -> float:
    return math.sqrt(_dot(value, value))


def _joint_angle(a: tuple[float, ...], vertex: tuple[float, ...], c: tuple[float, ...], label: str) -> float:
    first, second = _sub(a, vertex), _sub(c, vertex)
    denominator = _length(first) * _length(second)
    if denominator <= 1e-6:
        raise ContractError(f"{label} has a zero-length limb segment")
    return math.degrees(math.acos(max(-1.0, min(1.0, _dot(first, second) / denominator))))


def _yaw_degrees(quat: tuple[float, ...]) -> float:
    x, y, z, w = quat
    # Unreal's quaternion storage is XYZW. This yields world Z yaw.
    return math.degrees(math.atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z)))


def adapt(source: Any) -> dict[str, Any]:
    if not isinstance(source, dict) or source.get("schema") != SOURCE_SCHEMA:
        raise ContractError(f"schema must equal {SOURCE_SCHEMA}")
    if source.get("space") != "WORLD" or source.get("distance_unit") != "CENTIMETER":
        raise ContractError("space/distance_unit must be WORLD/CENTIMETER")
    if source.get("quaternion_order") != "XYZW":
        raise ContractError("quaternion_order must be XYZW")
    rate = _number(source.get("frame_rate"), "frame_rate")
    if abs(rate - 60.0) > 1e-6:
        raise ContractError("frame_rate must be exactly 60")
    if source.get("handedness") != "RHBH":
        raise ContractError("handedness must be RHBH")
    phases = source.get("phases")
    if not isinstance(phases, dict) or set(phases) != set(biomechanics.PHASES):
        raise ContractError(f"phases must contain exactly {list(biomechanics.PHASES)}")
    phase_values = []
    for name in biomechanics.PHASES:
        value = phases[name]
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ContractError(f"phases.{name} must be a nonnegative integer")
        phase_values.append(value)
    if phase_values != sorted(phase_values) or len(set(phase_values)) != len(phase_values):
        raise ContractError("phase frames must be unique and ordered")
    axes = source.get("throw_axes_world")
    if not isinstance(axes, dict):
        raise ContractError("throw_axes_world must be an object")
    forward = list(_vec(axes.get("target_forward"), 3, "throw_axes_world.target_forward"))
    lateral = list(_vec(axes.get("target_lateral"), 3, "throw_axes_world.target_lateral"))
    if abs(_length(tuple(forward)) - 1.0) > 1e-3 or abs(_length(tuple(lateral)) - 1.0) > 1e-3:
        raise ContractError("throw axes must be normalized")
    if abs(_dot(tuple(forward), tuple(lateral))) > 1e-3:
        raise ContractError("throw axes must be orthogonal")
    ground = _number(source.get("ground_z_cm"), "ground_z_cm")
    raw_frames = source.get("frames")
    if not isinstance(raw_frames, list) or not raw_frames:
        raise ContractError("frames must be a non-empty array")

    output_frames = []
    for index, raw in enumerate(raw_frames):
        label = f"frames[{index}]"
        if not isinstance(raw, dict) or raw.get("frame") != index:
            raise ContractError(f"{label}.frame must equal its array index")
        time = _number(raw.get("time_seconds"), f"{label}.time_seconds")
        if abs(time - index / rate) > 1e-5:
            raise ContractError(f"{label}.time_seconds is not fixed-step 60 Hz")
        transforms = raw.get("transforms")
        if not isinstance(transforms, dict):
            raise ContractError(f"{label}.transforms must be an object")
        missing = [name for name in REQUIRED_CHANNELS if name not in transforms]
        if missing:
            raise ContractError(f"{label} missing required transform channels: {', '.join(missing)}")
        positions: dict[str, tuple[float, ...]] = {}
        rotations: dict[str, tuple[float, ...]] = {}
        for channel in REQUIRED_CHANNELS:
            transform = transforms[channel]
            if not isinstance(transform, dict) or set(transform) != {"translation_cm", "rotation_xyzw"}:
                raise ContractError(f"{label}.transforms.{channel} must contain exactly translation_cm and rotation_xyzw")
            positions[channel] = _vec(transform["translation_cm"], 3, f"{label}.{channel}.translation_cm")
            quat = _vec(transform["rotation_xyzw"], 4, f"{label}.{channel}.rotation_xyzw")
            norm = _length(quat)
            if abs(norm - 1.0) > 1e-3:
                raise ContractError(f"{label}.{channel} quaternion is not normalized ({norm:.6f})")
            rotations[channel] = tuple(value / norm for value in quat)
        joints = {joint: list(positions[channel]) for channel, joint in CHANNEL_TO_JOINT.items()}
        left_included = _joint_angle(positions["thigh_l"], positions["calf_l"], positions["foot_l"], f"{label}.left_knee")
        right_included = _joint_angle(positions["thigh_r"], positions["calf_r"], positions["foot_r"], f"{label}.right_knee")
        elbow_included = _joint_angle(positions["upperarm_r"], positions["lowerarm_r"], positions["hand_r"], f"{label}.right_elbow")
        output_frames.append({
            "frame": index,
            "time_seconds": time,
            "joints_world_cm": joints,
            "angles_degrees": {
                "pelvis_yaw": _yaw_degrees(rotations["pelvis"]),
                "thorax_yaw": _yaw_degrees(rotations["spine_03"]),
                "knee_l_flexion": 180.0 - left_included,
                "knee_r_flexion": 180.0 - right_included,
                "elbow_r": elbow_included,
            },
            "com_world_cm": list(_vec(raw.get("center_of_mass_world_cm"), 3, f"{label}.center_of_mass_world_cm")),
            "support_polygon_margin_cm": _number(raw.get("support_polygon_margin_cm"), f"{label}.support_polygon_margin_cm"),
        })
    return {
        "schema": TARGET_SCHEMA, "frame_rate": rate, "handedness": "RHBH",
        "target_forward_world": forward, "target_lateral_world": lateral,
        "ground_z_cm": ground, "phases": copy.deepcopy(phases), "frames": output_frames,
    }


def _quat_yaw(degrees: float) -> list[float]:
    half = math.radians(degrees) * 0.5
    return [0.0, 0.0, math.sin(half), math.cos(half)]


def _source_fixture() -> dict[str, Any]:
    # Re-express the validator's passing world fixture as canonical Unreal data.
    target = biomechanics._fixture()
    reverse = {joint: channel for channel, joint in CHANNEL_TO_JOINT.items()}
    frames = []
    for frame in target["frames"]:
        transforms = {}
        for joint, channel in reverse.items():
            yaw = 0.0
            if channel == "pelvis": yaw = frame["angles_degrees"]["pelvis_yaw"]
            if channel == "spine_03": yaw = frame["angles_degrees"]["thorax_yaw"]
            transforms[channel] = {"translation_cm": frame["joints_world_cm"][joint], "rotation_xyzw": _quat_yaw(yaw)}
        frames.append({"frame": frame["frame"], "time_seconds": frame["time_seconds"], "transforms": transforms,
                       "center_of_mass_world_cm": frame["com_world_cm"],
                       "support_polygon_margin_cm": frame["support_polygon_margin_cm"]})
    return {"schema": SOURCE_SCHEMA, "space": "WORLD", "distance_unit": "CENTIMETER", "quaternion_order": "XYZW",
            "frame_rate": 60, "handedness": "RHBH", "throw_axes_world": {"target_forward": [1.0, 0.0, 0.0], "target_lateral": [0.0, 1.0, 0.0]},
            "ground_z_cm": 0.0, "phases": target["phases"], "frames": frames}


def self_test() -> tuple[int, int]:
    def missing(x: dict[str, Any]) -> None: del x["frames"][0]["transforms"]["DG_HeelContact_L"]
    cases: list[tuple[str, Callable[[dict[str, Any]], None], str]] = [
        ("pass_end_to_end", lambda x: None, "PASS"),
        ("schema", lambda x: x.update(schema="wrong"), "schema must equal"),
        ("missing_channel", missing, "missing required transform channels"),
        ("bad_quaternion", lambda x: x["frames"][0]["transforms"]["pelvis"].__setitem__("rotation_xyzw", [0, 0, 0, 0]), "quaternion is not normalized"),
        ("missing_com", lambda x: x["frames"][0].pop("center_of_mass_world_cm"), "center_of_mass_world_cm"),
        ("nonfinite_margin", lambda x: x["frames"][0].__setitem__("support_polygon_margin_cm", float("nan")), "support_polygon_margin_cm"),
        ("bad_time", lambda x: x["frames"][1].__setitem__("time_seconds", 1.0), "not fixed-step"),
        ("bad_phases", lambda x: x["phases"].__setitem__("release", x["phases"]["plant"]), "unique and ordered"),
        ("bad_axes", lambda x: x["throw_axes_world"].__setitem__("target_lateral", [1.0, 0.0, 0.0]), "orthogonal"),
        ("zero_limb", lambda x: x["frames"][0]["transforms"]["calf_l"].__setitem__("translation_cm", x["frames"][0]["transforms"]["thigh_l"]["translation_cm"]), "zero-length limb"),
    ]
    passed = 0
    for name, mutate, expected in cases:
        fixture = _source_fixture()
        mutate(fixture)
        try:
            converted = adapt(fixture)
            result = biomechanics.validate(converted)
            actual = result["status"] + " " + " ".join(result["failures"])
        except ContractError as exc:
            actual = str(exc)
        if expected in actual:
            passed += 1
        else:
            print(f"SELF_TEST_FAIL {name}: expected {expected!r}, got {actual!r}", file=sys.stderr)
    return passed, len(cases)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unreal_export", nargs="?", type=Path)
    parser.add_argument("--output", type=Path, help="write converted JSON (stdout when omitted)")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        passed, total = self_test()
        print(f"SELF_TEST {passed}/{total}")
        return 0 if passed == total else 1
    if args.unreal_export is None:
        parser.error("unreal_export is required unless --self-test is used")
    try:
        source = json.loads(args.unreal_export.read_text(encoding="utf-8"))
        output = json.dumps(adapt(source), indent=2, sort_keys=True) + "\n"
    except (OSError, UnicodeError, json.JSONDecodeError, ContractError) as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
