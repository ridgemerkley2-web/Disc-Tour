#!/usr/bin/env python3
"""Deterministically derive the append-only v006 production-motion recipe.

v004 authored the eight arm channels as component-space additives. v005
introduced the correct mixed-space contract, but copied those component values
into local channels. That changes their spatial meaning and folds the arms as
the torso turns. This generator converts v004's component targets into exact
DGMaster local additives under the unchanged v005 axial component targets.

The throwing wrist receives only the shortest component-space correction that
maps v004's disc plane onto the accepted v005 Aim/ReachBack/Plant/Release tilt
targets. This preserves those targets without importing v005's accidental
RunUp azimuth or moving the elbow/hand joint. Timing, root, axial, leg,
finger/grip, event, and curve payloads are copied from v005 byte-for-value at
the JSON value level.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
V4_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v4.json"
V5_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v5.json"
V6_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"

ARM_CHAINS = {
    "l": ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"),
    "r": ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"),
}
ARM_BONES = tuple(bone for chain in ARM_CHAINS.values() for bone in chain)

# Exact UE 5.8 DGMaster reference-pose values captured read-only from
# SKEL_DG_Master. Quaternions use Unreal/Hamilton (X,Y,Z,W) order.
REFERENCE_LOCAL_ROTATIONS = {
    "clavicle_l": (-0.5, 0.5, -0.5, -0.5),
    "upperarm_l": (
        -0.016657394849643768, 0.016657378085837865,
        0.0002775463144306596, -0.9997224544443182,
    ),
    "lowerarm_l": (
        -0.0025591447454608113, 0.002560780147899974,
        0.0000918660370052621, -0.9999934423504172,
    ),
    "hand_l": (
        0.01921654154163349, -0.019216537816343127,
        -0.0003694118997395486, -0.9996305881376281,
    ),
    "clavicle_r": (-0.5, -0.5, 0.5, -0.5),
    "upperarm_r": (
        -0.016657394849643768, -0.016657378085837865,
        -0.0002775463144306596, -0.9997224544443182,
    ),
    "lowerarm_r": (
        -0.0025591447454608113, -0.002560780147899974,
        -0.0000918660370052621, -0.9999934423504172,
    ),
    "hand_r": (
        0.01921654154163349, 0.019216537816343127,
        0.0003694118997395486, -0.9996305881376281,
    ),
}
REFERENCE_COMPONENT_ROTATIONS = {
    "spine_04": (
        0.7071067215818992, 0.0, 0.0, -0.7071068407911908,
    ),
    "clavicle_l": (
        5.960464577459135e-08, -5.960464577459135e-08,
        0.707106781186545, 0.707106781186545,
    ),
    "upperarm_l": (
        -0.023557161461014865, 4.771775888832985e-08,
        -0.706714271961008, -0.7071067797373463,
    ),
    "lowerarm_l": (
        0.02717633546350836, -4.4502745155010695e-08,
        0.7065843538895697, 0.7071067795101615,
    ),
    "hand_l": (
        -4.442658950963574e-08, 4.716855440435597e-08,
        -0.7071067811291694, -0.7071067812439229,
    ),
    "clavicle_r": (
        5.960464577459135e-08, 5.960464577459135e-08,
        -0.707106781186545, 0.707106781186545,
    ),
    "upperarm_r": (
        -0.023557161461014865, -4.771775888832985e-08,
        0.706714271961008, -0.7071067797373463,
    ),
    "lowerarm_r": (
        0.02717633546350836, 4.4502745155010695e-08,
        -0.7065843538895697, 0.7071067795101615,
    ),
    "hand_r": (
        -4.442658950963574e-08, -4.716855440435597e-08,
        0.7071067811291694, -0.7071067812439229,
    ),
}
REFERENCE_DISC_GRIP_ROTATION = (
    0.0000000005329638952389359, 0.000000000003781419621873283,
    0.00000000001809875339875422, -1.0,
)


def _load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if type(value) is not dict:
        raise ValueError(f"{path} does not contain a JSON object")
    return value


def _normalize(quaternion: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    length = math.sqrt(sum(component * component for component in quaternion))
    if length <= 1e-12:
        raise ValueError("degenerate quaternion")
    return tuple(component / length for component in quaternion)


def _multiply(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    x1, y1, z1, w1 = first
    x2, y2, z2, w2 = second
    return _normalize((
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
    ))


def _inverse(
    quaternion: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    normalized = _normalize(quaternion)
    return -normalized[0], -normalized[1], -normalized[2], normalized[3]


def _rotator_quaternion(values: list[float] | tuple[float, float, float]) -> tuple[float, float, float, float]:
    pitch, yaw, roll = (math.radians(float(value)) * 0.5 for value in values)
    sp, cp = math.sin(pitch), math.cos(pitch)
    sy, cy = math.sin(yaw), math.cos(yaw)
    sr, cr = math.sin(roll), math.cos(roll)
    return _normalize((
        cr * sp * sy - sr * cp * cy,
        -cr * sp * cy - sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy,
    ))


def _normalized_axis(degrees: float) -> float:
    value = math.fmod(degrees, 360.0)
    if value < 0.0:
        value += 360.0
    if value > 180.0:
        value -= 360.0
    return value


def _quaternion_rotator(
    quaternion: tuple[float, float, float, float],
) -> tuple[float, float, float]:
    """Match UE FQuat::Rotator's canonical Pitch/Yaw/Roll conversion."""
    x, y, z, w = _normalize(quaternion)
    singularity_test = z * x - w * y
    yaw_y = 2.0 * (w * z + x * y)
    yaw_x = 1.0 - 2.0 * (y * y + z * z)
    threshold = 0.4999995
    if singularity_test < -threshold:
        pitch = -90.0
        yaw = math.degrees(math.atan2(yaw_y, yaw_x))
        roll = _normalized_axis(-yaw - 2.0 * math.degrees(math.atan2(x, w)))
    elif singularity_test > threshold:
        pitch = 90.0
        yaw = math.degrees(math.atan2(yaw_y, yaw_x))
        roll = _normalized_axis(yaw - 2.0 * math.degrees(math.atan2(x, w)))
    else:
        pitch = math.degrees(math.asin(2.0 * singularity_test))
        yaw = math.degrees(math.atan2(yaw_y, yaw_x))
        roll = math.degrees(math.atan2(
            -2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y)
        ))
    result = pitch, _normalized_axis(yaw), _normalized_axis(roll)
    if _angular_distance(_rotator_quaternion(result), quaternion) > 1e-5:
        raise ValueError("quaternion-to-FRotator round trip exceeded tolerance")
    return result


def _rotate(
    quaternion: tuple[float, float, float, float],
    vector: tuple[float, float, float],
) -> tuple[float, float, float]:
    x, y, z, w = _normalize(quaternion)
    q_vector = (x, y, z)
    uv = (
        q_vector[1] * vector[2] - q_vector[2] * vector[1],
        q_vector[2] * vector[0] - q_vector[0] * vector[2],
        q_vector[0] * vector[1] - q_vector[1] * vector[0],
    )
    uuv = (
        q_vector[1] * uv[2] - q_vector[2] * uv[1],
        q_vector[2] * uv[0] - q_vector[0] * uv[2],
        q_vector[0] * uv[1] - q_vector[1] * uv[0],
    )
    return tuple(
        vector[index] + 2.0 * (w * uv[index] + uuv[index])
        for index in range(3)
    )


def _angular_distance(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> float:
    dot = abs(sum(a * b for a, b in zip(_normalize(first), _normalize(second))))
    return math.degrees(2.0 * math.acos(min(1.0, max(-1.0, dot))))


def _normal_from_hand(
    hand_component: tuple[float, float, float, float],
) -> tuple[float, float, float]:
    return _rotate(
        _multiply(hand_component, REFERENCE_DISC_GRIP_ROTATION),
        (0.0, 0.0, 1.0),
    )


def _plane_tilt(normal: tuple[float, float, float]) -> float:
    return math.degrees(math.acos(min(1.0, max(-1.0, abs(normal[2])))))


def _normal_with_tilt(
    normal: tuple[float, float, float], target_degrees: float,
) -> tuple[float, float, float]:
    # A plane is unoriented. Choose the normal hemisphere nearest the source,
    # then retain its azimuth while changing only elevation.
    sign = 1.0 if normal[2] >= 0.0 else -1.0
    oriented = tuple(sign * value for value in normal)
    horizontal_length = math.hypot(oriented[0], oriented[1])
    if horizontal_length <= 1e-9:
        horizontal = (1.0, 0.0)
    else:
        horizontal = (
            oriented[0] / horizontal_length,
            oriented[1] / horizontal_length,
        )
    radians = math.radians(target_degrees)
    target = (
        horizontal[0] * math.sin(radians),
        horizontal[1] * math.sin(radians),
        math.cos(radians),
    )
    return tuple(sign * value for value in target)


def _linear_anchor_value(
    anchors: list[tuple[int, float]], frame: int,
) -> float:
    for (first_frame, first_value), (second_frame, second_value) in zip(
        anchors, anchors[1:]
    ):
        if frame <= second_frame:
            alpha = (frame - first_frame) / max(1, second_frame - first_frame)
            return first_value + (second_value - first_value) * alpha
    return anchors[-1][1]


def _shortest_arc(
    source: tuple[float, float, float],
    target: tuple[float, float, float],
) -> tuple[float, float, float, float]:
    source_length = math.sqrt(sum(value * value for value in source))
    target_length = math.sqrt(sum(value * value for value in target))
    a = tuple(value / source_length for value in source)
    b = tuple(value / target_length for value in target)
    dot = min(1.0, max(-1.0, sum(x * y for x, y in zip(a, b))))
    if dot < -0.999999:
        seed = (1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0)
        axis = (
            a[1] * seed[2] - a[2] * seed[1],
            a[2] * seed[0] - a[0] * seed[2],
            a[0] * seed[1] - a[1] * seed[0],
        )
        length = math.sqrt(sum(value * value for value in axis))
        return axis[0] / length, axis[1] / length, axis[2] / length, 0.0
    cross = (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )
    return _normalize((*cross, 1.0 + dot))


def _v5_arm_components(
    rotations: dict[str, list[float]], side: str,
) -> dict[str, tuple[float, float, float, float]]:
    component = _multiply(
        _rotator_quaternion(rotations["spine_04"]),
        REFERENCE_COMPONENT_ROTATIONS["spine_04"],
    )
    result = {}
    for bone in ARM_CHAINS[side]:
        local = _multiply(
            REFERENCE_LOCAL_ROTATIONS[bone],
            _rotator_quaternion(rotations[bone]),
        )
        component = _multiply(component, local)
        result[bone] = component
    return result


def _v4_target_components(
    rotations: dict[str, list[float]], side: str,
) -> dict[str, tuple[float, float, float, float]]:
    return {
        bone: _multiply(
            _rotator_quaternion(rotations[bone]),
            REFERENCE_COMPONENT_ROTATIONS[bone],
        )
        for bone in ARM_CHAINS[side]
    }


def _round_degrees(values: tuple[float, float, float]) -> list[float]:
    return [0.0 if abs(value) < 0.0000005 else round(value, 6) for value in values]


def build_v6() -> tuple[dict[str, Any], dict[str, Any]]:
    v4 = _load(V4_PATH)
    v5 = _load(V5_PATH)
    result = copy.deepcopy(v5)
    for key, value in {
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v6",
        "schema_version": 6,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V6",
        "recipe_version": "v6",
        "asset_revision": "v006",
    }.items():
        result[key] = value

    v4_families = {family["family"]: family for family in v4["families"]}
    summary: dict[str, Any] = {"families": {}}
    for family in result["families"]:
        name = family["family"]
        family["motion_id"] = family["motion_id"].replace("_V5", "_V6")
        family["sequence_object_path"] = family["sequence_object_path"].replace(
            "v005", "v006"
        )
        family["montage_object_path"] = family["montage_object_path"].replace(
            "v005", "v006"
        )
        family["style_id"] = family["style_id"].replace("v005", "v006")
        v4_poses = {
            pose["frame"]: pose for pose in v4_families[name]["pose_keys"]
        }
        v5_poses = {pose["frame"]: pose for pose in family["pose_keys"]}
        phase_frames = {phase["name"]: phase["frame"] for phase in family["phases"]}
        plane_anchor_frames = (
            phase_frames["Aim"], phase_frames["ReachBack"],
            phase_frames["Plant"], family["release_frame"],
        )
        plane_anchors = []
        for anchor_frame in plane_anchor_frames:
            anchor_rotations = v5_poses[anchor_frame]["rotation_degrees"]
            anchor_hand = _v5_arm_components(anchor_rotations, "r")["hand_r"]
            plane_anchors.append((
                anchor_frame, _plane_tilt(_normal_from_hand(anchor_hand))
            ))
        follow_frame = phase_frames["FollowThrough"]
        follow_v4_hand = _v4_target_components(
            v4_poses[follow_frame]["rotation_degrees"], "r"
        )["hand_r"]
        plane_anchors.append((
            follow_frame, _plane_tilt(_normal_from_hand(follow_v4_hand))
        ))
        maximum_component_error = 0.0
        maximum_disc_tilt_error = 0.0
        maximum_wrist_plane_correction = 0.0
        for pose in family["pose_keys"]:
            frame = pose["frame"]
            v4_rotations = v4_poses[frame]["rotation_degrees"]
            v5_rotations = copy.deepcopy(pose["rotation_degrees"])
            axial_component = _multiply(
                _rotator_quaternion(v5_rotations["spine_04"]),
                REFERENCE_COMPONENT_ROTATIONS["spine_04"],
            )
            targets = {
                side: _v4_target_components(v4_rotations, side)
                for side in ARM_CHAINS
            }

            # Preserve the accepted v005 named-phase plane tilts while keeping
            # v004's azimuth. This avoids carrying v005's folded-chain RunUp
            # artifact into the corrected local-space arm.
            # Hand orientation does not move the wrist joint. Start from the
            # converted v004 forearm with zero wrist additive, then take the
            # shortest correction to the plane target. This retains v004's
            # full arm spatial path without encoding a 60+ degree wrist bend.
            neutral_hand = _multiply(
                targets["r"]["lowerarm_r"],
                REFERENCE_LOCAL_ROTATIONS["hand_r"],
            )
            neutral_normal = _normal_from_hand(neutral_hand)
            if frame <= follow_frame:
                target_tilt = _linear_anchor_value(plane_anchors, frame)
            else:
                target_tilt = _plane_tilt(neutral_normal)
            target_normal = _normal_with_tilt(neutral_normal, target_tilt)
            correction = _shortest_arc(neutral_normal, target_normal)
            corrected_hand = _multiply(correction, neutral_hand)
            maximum_wrist_plane_correction = max(
                maximum_wrist_plane_correction,
                _angular_distance(neutral_hand, corrected_hand),
            )
            targets["r"]["hand_r"] = corrected_hand

            for side, chain in ARM_CHAINS.items():
                parent_component = axial_component
                for bone in chain:
                    target_component = targets[side][bone]
                    target_local = _multiply(
                        _inverse(parent_component), target_component
                    )
                    local_intent = _multiply(
                        _inverse(REFERENCE_LOCAL_ROTATIONS[bone]), target_local
                    )
                    pose["rotation_degrees"][bone] = _round_degrees(
                        _quaternion_rotator(local_intent)
                    )
                    authored_local = _multiply(
                        REFERENCE_LOCAL_ROTATIONS[bone],
                        _rotator_quaternion(pose["rotation_degrees"][bone]),
                    )
                    authored_component = _multiply(parent_component, authored_local)
                    maximum_component_error = max(
                        maximum_component_error,
                        _angular_distance(authored_component, target_component),
                    )
                    parent_component = authored_component

            authored_right = _v5_arm_components(pose["rotation_degrees"], "r")
            authored_normal = _normal_from_hand(authored_right["hand_r"])
            maximum_disc_tilt_error = max(
                maximum_disc_tilt_error,
                abs(_plane_tilt(authored_normal) - target_tilt),
            )

        if maximum_component_error > 0.0001:
            raise ValueError(
                f"{name} component conversion error {maximum_component_error:.9f}"
            )
        if maximum_disc_tilt_error > 0.0001:
            raise ValueError(
                f"{name} disc-tilt error {maximum_disc_tilt_error:.9f}"
            )
        summary["families"][name] = {
            "pose_key_count": len(family["pose_keys"]),
            "maximum_component_round_trip_error_degrees": maximum_component_error,
            "maximum_disc_tilt_round_trip_error_degrees": maximum_disc_tilt_error,
            "maximum_wrist_plane_correction_degrees": maximum_wrist_plane_correction,
        }
    return result, summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="fail unless the checked-in v006 file matches deterministic output",
    )
    args = parser.parse_args()
    recipe, summary = build_v6()
    encoded = (json.dumps(recipe, indent=2) + "\n").encode("utf-8")
    if args.check:
        if not V6_PATH.is_file() or V6_PATH.read_bytes() != encoded:
            print("v006 recipe differs from deterministic generation", file=sys.stderr)
            return 1
    else:
        V6_PATH.write_bytes(encoded)
    summary.update({
        "output": str(V6_PATH),
        "bytes": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest().upper(),
        "status": "PASS_CHECK" if args.check else "PASS_GENERATED",
    })
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
