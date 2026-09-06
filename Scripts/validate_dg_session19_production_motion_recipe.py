#!/usr/bin/env python3
"""Read-only, fail-closed validation for versioned production-motion recipes."""

from __future__ import annotations

import argparse
import copy
import json
import math
from pathlib import Path
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PHASES = [
    "Aim", "RunUp", "ReachBack", "Plant", "Acceleration",
    "FollowThrough", "Recovery",
]
CURVES = {
    "DG_FootPlant_L", "DG_FootPlant_R", "DG_ReachbackAlpha",
    "DG_BraceAlpha", "DG_ReleaseApproachAlpha", "DG_FollowThroughAlpha",
}
V3_CURVES = CURVES | {
    "DG_WeightShiftAlpha", "DG_BraceCompressionAlpha", "DG_HipDriveAlpha",
    "DG_TorsoDriveAlpha", "DG_ShoulderDriveAlpha", "DG_ElbowLeadAlpha",
    "DG_WristLagAlpha", "DG_FingerReleaseAlpha",
    "DG_OffArmCounterbalanceAlpha", "DG_GazeTargetAlpha",
    "DG_DiscPlaneAlpha", "DG_BraceExtensionAlpha", "DG_RecoveryBeatAlpha",
}
V3_EVENT_KEYS = [
    "disc_reachback_plane_frame", "weight_shift_frame",
    "brace_compression_frame", "hip_fire_frame", "torso_fire_frame",
    "off_arm_counterbalance_frame", "shoulder_fire_frame",
    "elbow_lead_frame", "wrist_lag_frame", "release_frame",
    "brace_extension_frame", "gaze_reacquire_frame",
    "recovery_deceleration_frame", "recovery_recenter_frame",
    "recovery_settle_frame",
]
V2_BONES = [
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04",
    "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "thigh_l", "calf_l",
    "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r", "index_01_r", "index_02_r",
    "index_03_r", "middle_01_r", "middle_02_r", "middle_03_r", "ring_01_r",
    "ring_02_r", "ring_03_r", "pinky_01_r", "pinky_02_r", "pinky_03_r",
]
V2_FAMILIES = {
    "Drive": {
        "frames": 144, "release": 84, "finish": 138, "min_poses": 18,
        "max_root": 110.0, "min_excursion": 90.0, "root_scale": 2.25,
        "power": (0.62, 1.0),
    },
    "Approach": {
        "frames": 108, "release": 62, "finish": 104, "min_poses": 16,
        "max_root": 35.0, "min_excursion": 24.0, "root_scale": 1.0,
        "power": (0.25, 0.72),
    },
    "Putt": {
        "frames": 72, "release": 38, "finish": 68, "min_poses": 17,
        "max_root": 12.0, "min_excursion": 7.0, "root_scale": 1.0,
        "power": (0.05, 0.45),
    },
}
V3_FAMILIES = {
    "Drive": {
        **V2_FAMILIES["Drive"], "min_poses": 35, "max_gap": 6,
        "min_recovery": 11, "min_rotation_channels": 23,
        "max_release_hand_geodesic_degrees_per_frame": 13.5,
    },
    "Approach": {
        **V2_FAMILIES["Approach"], "min_poses": 36, "max_gap": 5,
        "min_recovery": 10, "min_rotation_channels": 23,
        "max_release_hand_geodesic_degrees_per_frame": 13.0,
    },
    "Putt": {
        **V2_FAMILIES["Putt"], "min_poses": 30, "max_gap": 4,
        "min_recovery": 10, "min_rotation_channels": 23,
        "max_release_hand_geodesic_degrees_per_frame": 4.0,
    },
}
V3_RELEASE_HAND_WINDOW_RADIUS_FRAMES = 8
V5_COMPONENT_ROTATION_BONES = [
    "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "neck_01", "head",
]
V5_LOCAL_ROTATION_BONES = [
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l",
    "thigh_r", "calf_r", "foot_r", "ball_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r",
    "index_01_r", "index_02_r", "index_03_r",
    "middle_01_r", "middle_02_r", "middle_03_r",
    "ring_01_r", "ring_02_r", "ring_03_r",
    "pinky_01_r", "pinky_02_r", "pinky_03_r",
]
V5_ROTATION_SPACE = (
    "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE"
)
V5_ROOT_TRACK_POLICY = "TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE"
V5_MAX_GENERATED_DISTAL_FINGER_CURL_DEGREES = 24.0
V5_MAX_FINGER_RELEASE_DEGREES_PER_FRAME = 20.0
V5_NAMED_PHASE_KNEE_ANGLE_RANGE_DEGREES = (130.0, 176.0)
V5_GENERATED_FINGER_ROTATION_COEFFICIENTS = {
    "thumb_01_r": (-8.0, 4.0, 12.0),
    "thumb_02_r": (0.0, 0.0, -18.0),
    "thumb_03_r": (0.0, 0.0, -12.0),
    **{
        f"{digit}_{segment}_r": (
            0.0,
            0.0,
            -18.0 if segment == "01" else -24.0,
        )
        for digit in ("index", "middle", "ring", "pinky")
        for segment in ("01", "02", "03")
    },
}
V5_DISC_PLANE_PHASE_LIMITS_DEGREES = {
    "Drive": {"Aim": 25.0, "ReachBack": 25.0, "Plant": 35.0, "Release": 12.0},
    "Approach": {"Aim": 25.0, "ReachBack": 25.0, "Plant": 35.0, "Release": 12.0},
    "Putt": {"Aim": 15.0, "ReachBack": 15.0, "Plant": 35.0, "Release": 12.0},
}
V5_MAX_DISC_PLANE_FRAME_DELTA_DEGREES = 15.0
V5_MAX_WRIST_ADDITIVE_DEGREES = 25.0

V6_ARM_CHAINS = {
    "l": ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"),
    "r": ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"),
}
V6_ARM_BONES = tuple(
    bone for chain in V6_ARM_CHAINS.values() for bone in chain
)
V6_REFERENCE_LOCAL_TRANSLATIONS_CM = {
    "clavicle_l": (2.9999999329447746, -7.00000524520874, 0.0),
    "upperarm_l": (0.0, -16.00000113248825, 0.0),
    "lowerarm_l": (
        0.0000003601569487088341, -30.016660690307617,
        -0.00000034293905493143484,
    ),
    "hand_l": (
        -0.0000009622624475014163, -26.019230484962463,
        0.0000001468565047346715,
    ),
    "clavicle_r": (-2.9999999329447746, -7.00000524520874, 0.0),
    "upperarm_r": (0.0, -16.00000113248825, 0.0),
    "lowerarm_r": (
        -0.0000003601569487088341, -30.016660690307617,
        -0.00000034293905493143484,
    ),
    "hand_r": (
        0.0000009622624475014163, -26.019230484962463,
        0.0000001468565047346715,
    ),
}
V6_REFERENCE_LOCAL_ROTATIONS = {
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
V6_REFERENCE_COMPONENT_ROTATIONS = {
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
V6_MIN_REACHBACK_RADIAL_REACH_CM = 64.0
V6_MIN_NAMED_PHASE_VERTICAL_CM = 0.0
V6_MIN_RELEASE_FOLLOWTHROUGH_LATERAL_CLEARANCE_CM = 60.0
V6_MIN_DIRECTIONAL_DOMINANCE_MARGIN_CM = 0.0
V6_MIN_ELBOW_ANGLE_DEGREES = 110.0
V6_MIN_ARM_EXTENSION_RATIO = 0.82
V6_MAX_TORSO_RELATIVE_HAND_DIRECTION_DELTA_DEGREES = 8.0
V6_MAX_COMPONENT_INTENT_ERROR_DEGREES = 0.001
V6_DISC_PLANE_PHASE_TARGETS_DEGREES = {
    "Drive": {"Aim": 24.0, "ReachBack": 24.0, "Plant": 34.0, "Release": 11.0},
    "Approach": {"Aim": 24.0, "ReachBack": 24.0, "Plant": 34.0, "Release": 11.0},
    "Putt": {"Aim": 14.0, "ReachBack": 14.0, "Plant": 34.0, "Release": 11.0},
}
V6_DISC_PLANE_PHASE_TARGET_TOLERANCE_DEGREES = 0.01

# Exact DGMaster rotations from the accepted read-only skeleton probe. The held
# Cylinder keeps local +Z as its top/normal; its 180-degree relative yaw on the
# right grip does not change that normal.
V5_DISC_PLANE_REFERENCE_QUATERNIONS = {
    "spine_04_component": (
        0.7071067215818992, 0.0, 0.0, -0.7071068407911908,
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
    "disc_grip_r": (
        0.0000000005329638952389359, 0.000000000003781419621873283,
        0.00000000001809875339875422, -1.0,
    ),
}
# Exact, narrowly allowed v5 deviations from the immutable v4 arm payload.
# Each row is frame: (lowerarm_r, hand_r). All other arm keys remain v4-exact.
V5_DISC_PLANE_CORRECTIONS = {
    "Drive": {
        0: ((2.24, 33.295, -9.936), (2.044, 1.998, 6.093)),
        44: ((0.823, 8.775, -1.042), (-6.721, 8.19, 3.211)),
        64: ((-3.868, 23.682, -7.765), (-5.19, 4.518, 2.738)),
        66: ((-5.464, 28.809, -7.5), (-6.659, 2.963, 5.131)),
        72: ((-10.863, 45.074, -7.821), (-11.206, -1.991, 11.922)),
        76: ((-13.805, 63.298, -9.939), (-11.415, -8.678, 18.855)),
        77: ((-12.247, 59.83, -7.27), (-8.723, -9.163, 20.448)),
        78: ((-10.811, 58.321, -5.986), (-6.665, -9.312, 21.234)),
        79: ((-9.74, 58.121, -5.326), (-5.629, -8.739, 21.276)),
        81: ((-5.526, 49.082, -0.253), (-3.65, -3.72, 17.747)),
        83: ((1.397, 24.964, 3.245), (4.485, -6.722, 12.194)),
        84: ((3.962, 13.472, 0.912), (7.889, -15.864, 9.886)),
    },
    "Approach": {
        0: ((1.947, 29.527, -10.025), (1.765, 2.033, 2.986)),
        32: ((0.631, 13.656, -3.701), (-5.522, 6.088, 3.837)),
        48: ((-2.991, 25.222, -9.151), (-3.704, 2.316, 3.64)),
        50: ((-4.644, 31.409, -8.335), (-4.959, 0.223, 6.664)),
        52: ((-6.386, 37.697, -7.714), (-6.123, -1.926, 9.62)),
        55: ((-8.942, 48.936, -7.548), (-7.16, -5.805, 14.871)),
        56: ((-9.863, 54.631, -8.352), (-7.324, -7.437, 17.024)),
        57: ((-8.128, 50.341, -5.806), (-4.914, -7.719, 17.842)),
        58: ((-6.701, 47.883, -4.083), (-3.567, -6.963, 17.676)),
        60: ((-2.482, 37.284, 1.416), (-1.576, -1.767, 13.414)),
        61: ((0.408, 26.05, 2.764), (2.258, -4.259, 11.157)),
        62: ((2.882, 14.341, 1.199), (5.9, -12.87, 9.434)),
    },
    "Putt": {
        0: ((0.654, 42.988, 1.112), (2.1, 0.881, 9.685)),
        4: ((-0.439, 44.539, 1.788), (1.475, 0.876, 9.706)),
        8: ((-1.537, 45.999, 1.983), (0.827, 0.859, 9.889)),
        10: ((-2.057, 46.705, 1.83), (0.447, 0.842, 10.056)),
        12: ((-3.471, 48.765, 1.717), (-0.871, 1.167, 9.934)),
        16: ((-6.032, 53.535, 0.595), (-4.184, 1.864, 9.198)),
        20: ((-7.855, 58.717, -3.011), (-6.665, 2.914, 8.117)),
        24: ((-6.242, 57.688, -8.148), (-4.944, 2.172, 5.117)),
        28: ((-3.824, 52.448, -13.033), (-1.788, 0.188, 3.267)),
        30: ((-3.861, 45.403, -8.318), (-1.08, -2.383, 4.682)),
        32: ((-3.133, 38.589, -4.131), (0.009, -4.943, 5.801)),
        33: ((-2.465, 35.277, -2.645), (0.835, -5.936, 5.741)),
        34: ((-2.269, 33.488, -1.383), (1.297, -6.597, 5.657)),
        36: ((-1.892, 30.132, 1.158), (1.414, -7.194, 5.489)),
        37: ((-0.761, 24.919, 1.954), (1.979, -9.604, 5.736)),
        38: ((0.715, 18.453, 2.609), (2.472, -12.03, 5.84)),
    },
}
GRANULAR_VERSIONS = {"v3", "v4", "v5", "v6"}
MIXED_SPACE_VERSIONS = {"v5", "v6"}
VERSION_SPECS = {
    "v1": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v1.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v1",
        "schema_version": 1,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V1",
        "bone_count": 20,
        "revision": "v001",
    },
    "v2": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v2.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v2",
        "schema_version": 2,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V2",
        "bone_count": 39,
        "revision": "v002",
    },
    "v3": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v3.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v3",
        "schema_version": 3,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V3",
        "bone_count": 39,
        "revision": "v003",
    },
    "v4": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v4.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v4",
        "schema_version": 4,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V4",
        "bone_count": 39,
        "revision": "v004",
    },
    "v5": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v5.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v5",
        "schema_version": 5,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V5",
        "bone_count": 39,
        "revision": "v005",
    },
    "v6": {
        "path": Path("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"),
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v6",
        "schema_version": 6,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V6",
        "bone_count": 39,
        "revision": "v006",
    },
}


class RecipeError(ValueError):
    pass


def _pairs(items: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in items:
        if key in result:
            raise RecipeError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise RecipeError(f"non-finite JSON constant: {value}")


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_pairs,
            parse_constant=_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, RecipeError) as exc:
        raise RecipeError(f"{path}: {exc}") from exc
    if type(value) is not dict:
        raise RecipeError("recipe root must be an object")
    return value


def exact_keys(value: Any, expected: set[str], label: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{label} must be an object")
        return False
    if set(value) != expected:
        issues.append(
            f"{label} keys differ: missing={sorted(expected - set(value))} "
            f"extra={sorted(set(value) - expected)}"
        )
        return False
    return True


def finite_number(value: Any) -> bool:
    return type(value) in (int, float) and not isinstance(value, bool) and math.isfinite(value)


def vector(value: Any) -> tuple[float, float, float] | None:
    if type(value) is not list or len(value) != 3 or not all(finite_number(v) for v in value):
        return None
    return float(value[0]), float(value[1]), float(value[2])


def magnitude(value: tuple[float, float, float]) -> float:
    return math.sqrt(sum(component * component for component in value))


def unreal_rotator_quaternion(
    value: tuple[float, float, float],
) -> tuple[float, float, float, float]:
    """Match FRotator(Pitch, Yaw, Roll).Quaternion() for geodesic checks."""
    pitch, yaw, roll = (
        math.radians(component) * 0.5 for component in value
    )
    sp, cp = math.sin(pitch), math.cos(pitch)
    sy, cy = math.sin(yaw), math.cos(yaw)
    sr, cr = math.sin(roll), math.cos(roll)
    quaternion = (
        cr * sp * sy - sr * cp * cy,
        -cr * sp * cy - sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy,
    )
    length = math.sqrt(sum(component * component for component in quaternion))
    return tuple(component / length for component in quaternion)


def quaternion_geodesic_degrees(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> float:
    first_quaternion = unreal_rotator_quaternion(first)
    second_quaternion = unreal_rotator_quaternion(second)
    dot = abs(sum(
        first_component * second_component
        for first_component, second_component
        in zip(first_quaternion, second_quaternion)
    ))
    return math.degrees(2.0 * math.acos(min(1.0, max(-1.0, dot))))


def quaternion_multiply(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    """Hamilton product matching Unreal's FQuat multiplication."""
    x1, y1, z1, w1 = first
    x2, y2, z2, w2 = second
    return (
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
    )


def quaternion_rotate_vector(
    rotation: tuple[float, float, float, float],
    value: tuple[float, float, float],
) -> tuple[float, float, float]:
    conjugate = (-rotation[0], -rotation[1], -rotation[2], rotation[3])
    rotated = quaternion_multiply(
        quaternion_multiply(rotation, (*value, 0.0)), conjugate
    )
    return rotated[0], rotated[1], rotated[2]


def quaternion_normalize(
    value: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    length = math.sqrt(sum(component * component for component in value))
    if length <= 1e-12:
        raise RecipeError("degenerate quaternion")
    return tuple(component / length for component in value)


def quaternion_inverse(
    value: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    normalized = quaternion_normalize(value)
    return -normalized[0], -normalized[1], -normalized[2], normalized[3]


def quaternion_angular_distance_degrees(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> float:
    dot = abs(sum(
        first_component * second_component
        for first_component, second_component
        in zip(quaternion_normalize(first), quaternion_normalize(second))
    ))
    return math.degrees(2.0 * math.acos(min(1.0, max(-1.0, dot))))


def vector_add(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> tuple[float, float, float]:
    return tuple(a + b for a, b in zip(first, second))


def vector_subtract(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> tuple[float, float, float]:
    return tuple(a - b for a, b in zip(first, second))


def vector_dot(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> float:
    return sum(a * b for a, b in zip(first, second))


def vector_angle_degrees(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> float:
    denominator = magnitude(first) * magnitude(second)
    if denominator <= 1e-9:
        raise RecipeError("degenerate direction vector")
    cosine = vector_dot(first, second) / denominator
    return math.degrees(math.acos(min(1.0, max(-1.0, cosine))))


def mixed_space_arm_pose(
    rotations: dict[str, tuple[float, float, float]],
) -> tuple[
    dict[str, tuple[float, float, float]],
    dict[str, tuple[float, float, float, float]],
    tuple[float, float, float, float],
]:
    """Reconstruct DGMaster arm joints under the v5/v6 mixed-space contract."""
    spine_component = quaternion_multiply(
        unreal_rotator_quaternion(rotations["spine_04"]),
        V5_DISC_PLANE_REFERENCE_QUATERNIONS["spine_04_component"],
    )
    positions: dict[str, tuple[float, float, float]] = {
        "spine_04": (0.0, 0.0, 0.0),
    }
    components: dict[str, tuple[float, float, float, float]] = {
        "spine_04": spine_component,
    }
    for chain in V6_ARM_CHAINS.values():
        parent_position = positions["spine_04"]
        parent_component = spine_component
        for bone in chain:
            positions[bone] = vector_add(
                parent_position,
                quaternion_rotate_vector(
                    parent_component,
                    V6_REFERENCE_LOCAL_TRANSLATIONS_CM[bone],
                ),
            )
            local = quaternion_multiply(
                V6_REFERENCE_LOCAL_ROTATIONS[bone],
                unreal_rotator_quaternion(rotations[bone]),
            )
            components[bone] = quaternion_multiply(parent_component, local)
            parent_position = positions[bone]
            parent_component = components[bone]
    return positions, components, spine_component


def torso_relative_vector(
    spine_component: tuple[float, float, float, float],
    value: tuple[float, float, float],
) -> tuple[float, float, float]:
    """Return (throwing-side lateral, forward, vertical) torso coordinates."""
    local_x, local_y, local_z = quaternion_rotate_vector(
        quaternion_inverse(spine_component), value
    )
    return local_x, local_z, -local_y


def mixed_space_arm_spatial_observation(
    ordered_frames: list[int],
    rotations_by_frame: dict[int, dict[str, tuple[float, float, float]]],
    frame_count: int,
    phase_frames: list[int],
    release_frame: int,
) -> tuple[dict[str, Any], list[str]]:
    required_bones = ("spine_04", *V6_ARM_BONES)
    missing = sorted({
        f"{frame}:{bone}"
        for frame in ordered_frames
        for bone in required_bones
        if bone not in rotations_by_frame.get(frame, {})
    })
    if missing:
        return {}, [
            "mixed-space arm spatial reconstruction lacks explicit channels: "
            f"{missing[:8]}"
        ]
    if len(phase_frames) != len(PHASES):
        return {}, ["mixed-space arm spatial reconstruction lacks phase anchors"]

    sampled: dict[int, dict[str, Any]] = {}
    maximum_direction_delta = 0.0
    maximum_direction_delta_frame: int | None = None
    previous_direction: tuple[float, float, float] | None = None
    for frame in range(frame_count + 1):
        rotations = {
            bone: sampled_catmull_rotation(
                ordered_frames, rotations_by_frame, bone, frame
            )
            for bone in required_bones
        }
        positions, components, spine_component = mixed_space_arm_pose(rotations)
        hand_torso = torso_relative_vector(
            spine_component, positions["hand_r"]
        )
        sampled[frame] = {
            "hand_torso": hand_torso,
            "positions": positions,
            "components": components,
        }
        if previous_direction is not None:
            delta = vector_angle_degrees(previous_direction, hand_torso)
            if delta > maximum_direction_delta:
                maximum_direction_delta = delta
                maximum_direction_delta_frame = frame
        previous_direction = hand_torso

    named_frames = {
        "ReachBack": phase_frames[2],
        "Release": release_frame,
        "FollowThrough": phase_frames[5],
    }
    coordinates = {
        name: sampled[frame]["hand_torso"]
        for name, frame in named_frames.items()
    }
    reachback = coordinates["ReachBack"]
    release = coordinates["Release"]
    follow = coordinates["FollowThrough"]
    elbow_angles: dict[str, float] = {}
    extension_ratios: dict[str, float] = {}
    for phase_name, phase_frame in named_frames.items():
        phase_positions = sampled[phase_frame]["positions"]
        shoulder = phase_positions["upperarm_r"]
        elbow = phase_positions["lowerarm_r"]
        hand = phase_positions["hand_r"]
        proximal = vector_subtract(shoulder, elbow)
        distal = vector_subtract(hand, elbow)
        upper_length = magnitude(proximal)
        lower_length = magnitude(distal)
        if upper_length <= 1e-9 or lower_length <= 1e-9:
            raise RecipeError(
                f"{phase_name} throwing-arm segment is degenerate"
            )
        elbow_angles[phase_name] = vector_angle_degrees(proximal, distal)
        extension_ratios[phase_name] = (
            magnitude(vector_subtract(hand, shoulder))
            / (upper_length + lower_length)
        )
    directional_margins = {
        "ReachBack": abs(reachback[0]) - abs(reachback[1]),
        "FollowThrough": abs(follow[0]) - abs(follow[1]),
    }
    observation = {
        "torso_relative_hand_cm_by_phase": {
            name: [round(component, 6) for component in value]
            for name, value in coordinates.items()
        },
        "minimum_directional_dominance_margin_cm": min(
            directional_margins.values()
        ),
        "directional_dominance_margin_cm_by_phase": directional_margins,
        "reachback_radial_reach_cm": math.hypot(reachback[0], reachback[1]),
        "minimum_named_phase_vertical_cm": min(
            reachback[2], release[2], follow[2]
        ),
        "minimum_release_followthrough_lateral_clearance_cm": min(
            abs(release[0]), abs(follow[0])
        ),
        "throwing_elbow_angle_degrees_by_phase": elbow_angles,
        "throwing_arm_extension_ratio_by_phase": extension_ratios,
        "minimum_throwing_elbow_angle_degrees": min(elbow_angles.values()),
        "minimum_throwing_arm_extension_ratio": min(extension_ratios.values()),
        "maximum_torso_relative_hand_direction_delta_degrees": (
            maximum_direction_delta
        ),
        "maximum_torso_relative_hand_direction_delta_frame": (
            maximum_direction_delta_frame
        ),
    }
    gate_issues: list[str] = []
    if (
        observation["minimum_throwing_elbow_angle_degrees"]
        < V6_MIN_ELBOW_ANGLE_DEGREES - 1e-6
    ):
        gate_issues.append(
            "throwing elbow collapses below the "
            f"{V6_MIN_ELBOW_ANGLE_DEGREES:.1f}-degree phase gate"
        )
    if (
        observation["minimum_throwing_arm_extension_ratio"]
        < V6_MIN_ARM_EXTENSION_RATIO - 1e-6
    ):
        gate_issues.append(
            "throwing arm extension collapses below the "
            f"{V6_MIN_ARM_EXTENSION_RATIO:.2f} phase gate"
        )
    if (
        observation["minimum_directional_dominance_margin_cm"]
        < V6_MIN_DIRECTIONAL_DOMINANCE_MARGIN_CM - 1e-6
    ):
        gate_issues.append(
            "throwing-hand ReachBack/FollowThrough direction is not "
            "throwing-side dominant"
        )
    if (
        observation["reachback_radial_reach_cm"]
        < V6_MIN_REACHBACK_RADIAL_REACH_CM - 1e-6
    ):
        gate_issues.append(
            f"ReachBack radial reach is below {V6_MIN_REACHBACK_RADIAL_REACH_CM:.1f} cm"
        )
    if (
        observation["minimum_named_phase_vertical_cm"]
        < V6_MIN_NAMED_PHASE_VERTICAL_CM - 1e-6
    ):
        gate_issues.append(
            "throwing hand vertically collapses below the torso reference at "
            "ReachBack/Release/FollowThrough"
        )
    if (
        observation["minimum_release_followthrough_lateral_clearance_cm"]
        < V6_MIN_RELEASE_FOLLOWTHROUGH_LATERAL_CLEARANCE_CM - 1e-6
    ):
        gate_issues.append(
            "Release/FollowThrough lateral clearance permits torso self-occlusion"
        )
    if (
        observation["maximum_torso_relative_hand_direction_delta_degrees"]
        > V6_MAX_TORSO_RELATIVE_HAND_DIRECTION_DELTA_DEGREES + 1e-6
    ):
        gate_issues.append(
            "torso-relative throwing-hand direction exceeds the "
            f"{V6_MAX_TORSO_RELATIVE_HAND_DIRECTION_DELTA_DEGREES:.1f} "
            "degrees/frame angular smoothness gate"
        )
    return observation, gate_issues


def v5_disc_plane_normal(
    rotations: dict[str, tuple[float, float, float]],
) -> tuple[float, float, float]:
    """Reconstruct raw disc_grip_r local +Z under the v5 mixed-space contract."""
    component = quaternion_multiply(
        unreal_rotator_quaternion(rotations["spine_04"]),
        V5_DISC_PLANE_REFERENCE_QUATERNIONS["spine_04_component"],
    )
    for bone in ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
        local = quaternion_multiply(
            V5_DISC_PLANE_REFERENCE_QUATERNIONS[bone],
            unreal_rotator_quaternion(rotations[bone]),
        )
        component = quaternion_multiply(component, local)
    component = quaternion_multiply(
        component, V5_DISC_PLANE_REFERENCE_QUATERNIONS["disc_grip_r"]
    )
    normal = quaternion_rotate_vector(component, (0.0, 0.0, 1.0))
    normal_length = magnitude(normal)
    if normal_length <= 1e-9:
        raise RecipeError("disc_grip_r produced a degenerate plane normal")
    return tuple(component / normal_length for component in normal)


def disc_plane_tilt_degrees(normal: tuple[float, float, float]) -> float:
    # A disc plane is unoriented: +normal and -normal describe the same plane.
    cosine = min(1.0, max(-1.0, abs(normal[2])))
    return math.degrees(math.acos(cosine))


def disc_plane_delta_degrees(
    first: tuple[float, float, float], second: tuple[float, float, float]
) -> float:
    cosine = abs(sum(a * b for a, b in zip(first, second)))
    return math.degrees(math.acos(min(1.0, max(-1.0, cosine))))


def catmull_rom_scalar(p0: float, p1: float, p2: float, p3: float, alpha: float) -> float:
    """Match the authoring utility's clamped Catmull-Rom scalar sampling."""
    t = min(1.0, max(0.0, alpha))
    t2 = t * t
    t3 = t2 * t
    return 0.5 * (
        2.0 * p1
        + (-p0 + p2) * t
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
    )


def sampled_catmull_scalar(
    ordered_frames: list[int], values: dict[int, float], frame: int
) -> float:
    upper = 1
    while upper < len(ordered_frames) and ordered_frames[upper] < frame:
        upper += 1
    upper = min(max(upper, 1), len(ordered_frames) - 1)
    earlier_frame = ordered_frames[upper - 1]
    later_frame = ordered_frames[upper]
    alpha = (frame - earlier_frame) / max(1, later_frame - earlier_frame)
    return catmull_rom_scalar(
        values[ordered_frames[max(0, upper - 2)]],
        values[earlier_frame],
        values[later_frame],
        values[ordered_frames[min(len(ordered_frames) - 1, upper + 1)]],
        alpha,
    )


def sampled_catmull_rotation(
    ordered_frames: list[int],
    rotations_by_frame: dict[int, dict[str, tuple[float, float, float]]],
    bone: str,
    frame: int,
) -> tuple[float, float, float]:
    values = {
        pose_frame: rotations_by_frame.get(pose_frame, {}).get(
            bone, (0.0, 0.0, 0.0)
        )
        for pose_frame in ordered_frames
    }
    return tuple(
        sampled_catmull_scalar(
            ordered_frames,
            {pose_frame: value[axis] for pose_frame, value in values.items()},
            frame,
        )
        for axis in range(3)
    )


def normalize_v4_to_v3(value: Any) -> Any:
    if type(value) is str:
        return (
            value.replace(
                "ProjectAuthoredProceduralMotionRecipe.v4",
                "ProjectAuthoredProceduralMotionRecipe.v3",
            )
            .replace("_V4", "_V3")
            .replace("v004", "v003")
            .replace("v4", "v3")
        )
    if type(value) is list:
        return [normalize_v4_to_v3(item) for item in value]
    if type(value) is dict:
        return {
            key: normalize_v4_to_v3(item)
            for key, item in value.items()
            if key != "rotation_space"
        }
    return value


def normalize_v5_to_v4(value: Any) -> Any:
    if type(value) is str:
        return (
            value.replace(
                "ProjectAuthoredProceduralMotionRecipe.v5",
                "ProjectAuthoredProceduralMotionRecipe.v4",
            )
            .replace("_V5", "_V4")
            .replace("v005", "v004")
            .replace("v5", "v4")
        )
    if type(value) is list:
        return [normalize_v5_to_v4(item) for item in value]
    if type(value) is dict:
        return {
            key: normalize_v5_to_v4(item)
            for key, item in value.items()
            if key not in {
                "root_track_policy",
                "component_rotation_bones",
                "local_rotation_bones",
            }
        }
    return value


def normalize_v6_to_v5(value: Any) -> Any:
    if type(value) is str:
        return (
            value.replace(
                "ProjectAuthoredProceduralMotionRecipe.v6",
                "ProjectAuthoredProceduralMotionRecipe.v5",
            )
            .replace("_V6", "_V5")
            .replace("v006", "v005")
            .replace("v6", "v5")
        )
    if type(value) is list:
        return [normalize_v6_to_v5(item) for item in value]
    if type(value) is dict:
        return {
            key: normalize_v6_to_v5(item)
            for key, item in value.items()
        }
    return value


def validate_recipe(
    recipe: dict[str, Any], version: str, root: Path,
    *, check_source_seams: bool = True,
) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    spec = VERSION_SPECS.get(version)
    if spec is None:
        return [f"unsupported recipe version: {version!r}"], {}
    common_root_keys = {
        "schema", "schema_version", "recipe_id", "creator", "source_kind",
        "performer", "external_source_used", "derived_from_synthetic_fixture",
        "motion_capture_claim", "usage_status", "human_animation_approval",
        "human_disc_contact_approval", "target_skeleton", "frame_rate",
        "root_motion_enabled", "animated_bones", "families",
    }
    v2_root_keys = common_root_keys | {
        "recipe_version", "asset_revision", "world_motion_authority",
        "presentation_root_trajectory_enabled", "interpolation_mode",
    }
    v3_root_keys = v2_root_keys | {
        "biomechanical_model", "disc_contact_model", "recovery_model",
    }
    v4_root_keys = v3_root_keys | {"rotation_space"}
    v5_root_keys = v4_root_keys | {
        "root_track_policy", "component_rotation_bones", "local_rotation_bones",
    }
    expected_root_keys = (
        v5_root_keys if version in MIXED_SPACE_VERSIONS
        else v4_root_keys if version == "v4"
        else v3_root_keys if version == "v3"
        else v2_root_keys if version == "v2"
        else common_root_keys
    )
    if not exact_keys(
        recipe, expected_root_keys,
        "recipe", issues,
    ):
        return issues, {}
    expected_common = {
        "schema": spec["schema"],
        "schema_version": spec["schema_version"],
        "recipe_id": spec["recipe_id"],
        "creator": "DiscGolfTour project",
        "source_kind": "PROJECT_AUTHORED_PROCEDURAL",
        "performer": "NOT_APPLICABLE_PROCEDURAL_NO_CAPTURE",
        "external_source_used": False,
        "derived_from_synthetic_fixture": False,
        "motion_capture_claim": False,
        "usage_status": "PRODUCTION_CANDIDATE_HUMAN_REVIEW_REQUIRED",
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "target_skeleton": (
            "/Game/DiscGolf/Characters/Meshes/"
            "SKEL_DG_Master.SKEL_DG_Master"
        ),
        "frame_rate": 60,
        "root_motion_enabled": False,
    }
    for key, expected in expected_common.items():
        if recipe.get(key) != expected:
            issues.append(f"recipe.{key} differs")
    if version in {"v2", "v3", "v4", "v5", "v6"}:
        for key, expected in {
            "recipe_version": version,
            "asset_revision": spec["revision"],
            "world_motion_authority": "GAMEPLAY_PAWN_CAPSULE_PRESENTATION_ROOT_ONLY",
            "presentation_root_trajectory_enabled": True,
            "interpolation_mode": (
                "CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"
                if version in GRANULAR_VERSIONS else "CATMULL_ROM_PHASE_SHAPED"
            ),
        }.items():
            if recipe.get(key) != expected:
                issues.append(f"recipe.{key} differs")
    if version in GRANULAR_VERSIONS:
        for key, expected in {
            "biomechanical_model": "RHBH_KINETIC_CHAIN_GRANULAR_V1",
            "disc_contact_model": "REACHBACK_PLANE_WRIST_LAG_FINGER_RELEASE",
            "recovery_model": "THREE_BEAT_DECELERATION_RECENTER_SETTLE",
        }.items():
            if recipe.get(key) != expected:
                issues.append(f"recipe.{key} differs")
    if version == "v4" and recipe.get("rotation_space") != (
        "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"
    ):
        issues.append("recipe.rotation_space differs")
    if version in MIXED_SPACE_VERSIONS:
        if recipe.get("rotation_space") != V5_ROTATION_SPACE:
            issues.append("recipe.rotation_space differs")
        if recipe.get("root_track_policy") != V5_ROOT_TRACK_POLICY:
            issues.append("recipe.root_track_policy differs")
        if recipe.get("component_rotation_bones") != V5_COMPONENT_ROTATION_BONES:
            issues.append("recipe.component_rotation_bones differs from exact seven-bone axial contract")
        if recipe.get("local_rotation_bones") != V5_LOCAL_ROTATION_BONES:
            issues.append("recipe.local_rotation_bones differs from exact 31-bone appendicular contract")

    bones = recipe.get("animated_bones")
    if type(bones) is not list or len(bones) != spec["bone_count"]:
        issues.append(f"animated_bones must contain exactly {spec['bone_count']} entries")
        bones = []
    elif len(set(bones)) != len(bones) or not all(type(item) is str for item in bones):
        issues.append("animated_bones must be unique strings")
    if version in {"v2", "v3", "v4", "v5", "v6"} and bones != V2_BONES:
        issues.append(f"{version} animated_bones differs from the exact 39-bone contract")
    bone_set = set(bones)
    if version in MIXED_SPACE_VERSIONS:
        component_set = set(V5_COMPONENT_ROTATION_BONES)
        local_set = set(V5_LOCAL_ROTATION_BONES)
        if component_set & local_set:
            issues.append("v5 mixed-space partitions overlap")
        if component_set | local_set != bone_set - {"root"}:
            issues.append("v5 mixed-space partitions do not cover exactly every non-root bone")

    families = recipe.get("families")
    if type(families) is not list or len(families) != 3:
        issues.append("families must contain exactly Drive, Approach, and Putt")
        return issues, {}
    expected_family_names = ["Drive", "Approach", "Putt"]
    expected_paths: set[str] = set()
    family_observations: list[dict[str, Any]] = []
    common_family_keys = {
        "motion_id", "family", "sequence_object_path", "montage_object_path",
        "style_id", "frame_count", "duration_seconds", "release_frame",
        "finish_frame", "recommended_power_min", "recommended_power_max",
        "authored_coverage", "phases", "pose_keys", "curves",
    }
    v2_family_keys = common_family_keys | {
        "minimum_pose_key_count", "max_presentation_root_translation_cm",
        "minimum_forward_root_excursion_cm", "root_return_tolerance_cm",
        "presentation_root_scale",
    }
    v3_family_keys = v2_family_keys | {
        "maximum_pose_key_gap_frames", "minimum_recovery_pose_key_count",
        "minimum_authored_rotation_channels_per_pose", "biomechanical_events",
    }
    for index, family in enumerate(families):
        label = f"families[{index}]"
        if not exact_keys(
            family,
            v3_family_keys if version in GRANULAR_VERSIONS
            else v2_family_keys if version == "v2"
            else common_family_keys,
            label, issues,
        ):
            continue
        name = family["family"]
        if name != expected_family_names[index]:
            issues.append(f"{label}.family order differs")
            continue
        suffix = spec["revision"]
        base = f"/Game/DiscGolf/Animation/ProductionMotion/{name}"
        sequence = family["sequence_object_path"]
        montage = family["montage_object_path"]
        expected_prefix = f"A_DG_RHBH_{name}_Procedural_{suffix}"
        expected_sequence = f"{base}/{expected_prefix}.{expected_prefix}"
        expected_montage_name = f"AM_DG_RHBH_{name}_Procedural_{suffix}"
        expected_montage = f"{base}/{expected_montage_name}.{expected_montage_name}"
        if sequence != expected_sequence or montage != expected_montage:
            issues.append(f"{label} target paths differ from selected {version} contract")
        expected_paths.update((sequence, montage))
        frames = family["frame_count"]
        duration = family["duration_seconds"]
        release = family["release_frame"]
        finish = family["finish_frame"]
        power_min = family["recommended_power_min"]
        power_max = family["recommended_power_max"]
        if not all(finite_number(value) for value in (
            frames, duration, release, finish, power_min, power_max
        )):
            issues.append(f"{label} timing/power contains a non-finite value")
            continue
        if type(frames) is not int or type(release) is not int or type(finish) is not int:
            issues.append(f"{label} frame values must be integers")
            continue
        if not math.isclose(float(duration), frames / 60.0, abs_tol=0.001):
            issues.append(f"{label}.duration_seconds differs from 60 fps")
        if not 0 <= power_min < power_max <= 1:
            issues.append(f"{label} recommended power range is invalid")

        phase_rows = family["phases"]
        phase_frames: list[int] = []
        if type(phase_rows) is not list or len(phase_rows) != len(PHASES):
            issues.append(f"{label}.phases must contain exactly seven rows")
        else:
            for phase_index, phase in enumerate(phase_rows):
                if type(phase) is not dict or set(phase) != {"name", "frame"}:
                    issues.append(f"{label}.phases[{phase_index}] differs")
                    continue
                if phase["name"] != PHASES[phase_index] or type(phase["frame"]) is not int:
                    issues.append(f"{label}.phases[{phase_index}] name/frame differs")
                    continue
                phase_frames.append(phase["frame"])
        if len(phase_frames) == len(PHASES):
            if phase_frames != sorted(set(phase_frames)):
                issues.append(f"{label}.phases are not strictly ordered")
            elif not (
                phase_frames[4] < release < phase_frames[5]
                and phase_frames[6] < finish < frames
            ):
                issues.append(f"{label} release/follow-through/recovery/finish order differs")

        biomechanical_events: dict[str, int] = {}
        if version in GRANULAR_VERSIONS:
            raw_events = family["biomechanical_events"]
            if not exact_keys(
                raw_events, set(V3_EVENT_KEYS),
                f"{label}.biomechanical_events", issues,
            ):
                raw_events = {}
            elif not all(type(raw_events[key]) is int for key in V3_EVENT_KEYS):
                issues.append(f"{label}.biomechanical_events must be integer frames")
            else:
                biomechanical_events = {
                    key: raw_events[key] for key in V3_EVENT_KEYS
                }
                event_frames = [biomechanical_events[key] for key in V3_EVENT_KEYS]
                if len(phase_frames) != len(PHASES):
                    issues.append(f"{label} biomechanical events lack valid phase anchors")
                elif not (
                    biomechanical_events["disc_reachback_plane_frame"]
                        == phase_frames[2]
                    < biomechanical_events["weight_shift_frame"]
                    < biomechanical_events["brace_compression_frame"]
                        == phase_frames[3]
                    < biomechanical_events["hip_fire_frame"]
                    < biomechanical_events["torso_fire_frame"]
                        == phase_frames[4]
                    < biomechanical_events["off_arm_counterbalance_frame"]
                    < biomechanical_events["shoulder_fire_frame"]
                    < biomechanical_events["elbow_lead_frame"]
                    < biomechanical_events["wrist_lag_frame"]
                    < biomechanical_events["release_frame"] == release
                    < biomechanical_events["brace_extension_frame"]
                    < biomechanical_events["gaze_reacquire_frame"]
                        == phase_frames[5]
                    < biomechanical_events["recovery_deceleration_frame"]
                    < biomechanical_events["recovery_recenter_frame"]
                        == phase_frames[6]
                    < biomechanical_events["recovery_settle_frame"]
                    < finish
                ):
                    issues.append(f"{label} biomechanical event order/anchors differ")
                if len(event_frames) != len(V3_EVENT_KEYS):
                    issues.append(f"{label} biomechanical event cardinality differs")

        poses = family["pose_keys"]
        pose_frames: list[int] = []
        root_values: list[tuple[float, float, float]] = []
        grips: dict[int, float] = {}
        rotations_by_frame: dict[int, dict[str, tuple[float, float, float]]] = {}
        rotation_channel_counts: dict[int, int] = {}
        if type(poses) is not list:
            issues.append(f"{label}.pose_keys must be an array")
            poses = []
        for pose_index, pose in enumerate(poses):
            pose_label = f"{label}.pose_keys[{pose_index}]"
            pose_keys = {"frame", "translation_cm", "rotation_degrees"}
            if version in {"v2", "v3", "v4", "v5", "v6"}:
                pose_keys.add("throwing_hand_grip_alpha")
            if not exact_keys(pose, pose_keys, pose_label, issues):
                continue
            frame = pose["frame"]
            if type(frame) is not int:
                issues.append(f"{pose_label}.frame must be an integer")
                continue
            pose_frames.append(frame)
            translations = pose["translation_cm"]
            rotations = pose["rotation_degrees"]
            if type(translations) is not dict or type(rotations) is not dict:
                issues.append(f"{pose_label} transforms must be objects")
                continue
            if version in MIXED_SPACE_VERSIONS:
                if set(translations) != {"root"}:
                    issues.append(f"{pose_label}.translation_cm must contain root only")
                if "root" in rotations:
                    issues.append(f"{pose_label}.rotation_degrees must not author root")
                missing_component = set(V5_COMPONENT_ROTATION_BONES) - set(rotations)
                if missing_component:
                    issues.append(
                        f"{pose_label}.rotation_degrees omits explicit component bones: "
                        f"{sorted(missing_component)}"
                    )
                unexpected_rotations = set(rotations) - (
                    set(V5_COMPONENT_ROTATION_BONES) | set(V5_LOCAL_ROTATION_BONES)
                )
                if unexpected_rotations:
                    issues.append(
                        f"{pose_label}.rotation_degrees escapes mixed-space partition: "
                        f"{sorted(unexpected_rotations)}"
                    )
            parsed_rotations: dict[str, tuple[float, float, float]] = {}
            rotation_channel_counts[frame] = len(rotations)
            for bone, raw in translations.items():
                parsed = vector(raw)
                if bone not in bone_set or parsed is None:
                    issues.append(f"{pose_label}.translation_cm.{bone} is invalid")
                    continue
                if bone != "root" and magnitude(parsed) > 8.0:
                    issues.append(f"{pose_label}.{bone} translation exceeds 8 cm")
            for bone, raw in rotations.items():
                parsed = vector(raw)
                if bone not in bone_set or parsed is None:
                    issues.append(f"{pose_label}.rotation_degrees.{bone} is invalid")
                    continue
                parsed_rotations[bone] = parsed
                if version in MIXED_SPACE_VERSIONS and bone in {"calf_l", "calf_r"}:
                    if (
                        not math.isclose(parsed[0], 0.0, abs_tol=1e-9)
                        or not math.isclose(parsed[1], 0.0, abs_tol=1e-9)
                        or parsed[2] <= 0.0
                    ):
                        issues.append(
                            f"{pose_label}.{bone} must use positive local-X Roll "
                            "with zero Pitch/Yaw for DGMaster knee flexion"
                        )
                limit = 90.0
                if version in {"v2", "v3", "v4", "v5", "v6"} and bone in {"neck_01", "head"}:
                    limit = 25.0
                elif version in {"v2", "v3", "v4", "v5", "v6"} and bone in {"ball_l", "ball_r"}:
                    limit = 35.0
                if max(abs(component) for component in parsed) > limit:
                    issues.append(f"{pose_label}.{bone} rotation exceeds its limit")
            rotations_by_frame[frame] = parsed_rotations
            root_raw = vector(translations.get("root"))
            if root_raw is None:
                issues.append(f"{pose_label} omits a finite root translation")
            else:
                scale = float(family.get("presentation_root_scale", 1.0))
                root_values.append(tuple(component * scale for component in root_raw))
            if version in {"v2", "v3", "v4", "v5", "v6"}:
                grip = pose["throwing_hand_grip_alpha"]
                if not finite_number(grip) or not 0 <= grip <= 1:
                    issues.append(f"{pose_label}.throwing_hand_grip_alpha is invalid")
                else:
                    grips[frame] = float(grip)
        if pose_frames != sorted(set(pose_frames)) or not pose_frames:
            issues.append(f"{label}.pose_keys are empty, duplicated, or unordered")
        elif pose_frames[0] != 0 or pose_frames[-1] != frames:
            issues.append(f"{label}.pose_keys must span frame zero through the final frame")

        if version in {"v2", "v3", "v4", "v5", "v6"}:
            fspec = (V3_FAMILIES if version in GRANULAR_VERSIONS else V2_FAMILIES)[name]
            exact_versioned = {
                "frame_count": fspec["frames"],
                "release_frame": fspec["release"],
                "finish_frame": fspec["finish"],
                "minimum_pose_key_count": fspec["min_poses"],
                "max_presentation_root_translation_cm": fspec["max_root"],
                "minimum_forward_root_excursion_cm": fspec["min_excursion"],
                "root_return_tolerance_cm": 0.25,
                "presentation_root_scale": fspec["root_scale"],
                "recommended_power_min": fspec["power"][0],
                "recommended_power_max": fspec["power"][1],
            }
            if version in GRANULAR_VERSIONS:
                exact_versioned.update({
                    "maximum_pose_key_gap_frames": fspec["max_gap"],
                    "minimum_recovery_pose_key_count": fspec["min_recovery"],
                    "minimum_authored_rotation_channels_per_pose": (
                        fspec["min_rotation_channels"]
                    ),
                })
            for key, expected in exact_versioned.items():
                if family.get(key) != expected:
                    issues.append(f"{label}.{key} differs from {version} contract")
            if len(poses) < fspec["min_poses"]:
                issues.append(f"{label} has too few phase-shaped pose keys")
            required_pose_frames = set(phase_frames) | {release, finish}
            if not required_pose_frames.issubset(set(pose_frames)):
                issues.append(f"{label} omits a phase/release/finish pose key")
            if len(phase_frames) == len(PHASES):
                if grips.get(phase_frames[4], -1) < 0.65:
                    issues.append(f"{label} acceleration grip is too open")
                if grips.get(release, 2) > 0.25:
                    issues.append(f"{label} release grip does not open")
                if grips.get(phase_frames[5], 2) > 0.10:
                    issues.append(f"{label} follow-through grip does not remain open")
            if root_values:
                if max(magnitude(item) for item in root_values) > fspec["max_root"]:
                    issues.append(f"{label} effective presentation root exceeds its limit")
                excursion = max(item[0] for item in root_values) - min(
                    item[0] for item in root_values
                )
                if excursion < fspec["min_excursion"]:
                    issues.append(f"{label} effective forward root excursion is too small")
                return_distance = magnitude(tuple(
                    root_values[-1][axis] - root_values[0][axis] for axis in range(3)
                ))
                if return_distance > 0.25:
                    issues.append(f"{label} presentation root does not settle at finish")
            if version in GRANULAR_VERSIONS:
                maximum_gap = max(
                    (later - earlier for earlier, later in zip(
                        pose_frames, pose_frames[1:]
                    )),
                    default=frames,
                )
                recovery_pose_count = sum(
                    phase_frames[5] <= frame <= finish for frame in pose_frames
                ) if len(phase_frames) == len(PHASES) else 0
                minimum_rotation_channels = min(
                    rotation_channel_counts.values(), default=0
                )
                if maximum_gap > fspec["max_gap"]:
                    issues.append(f"{label} pose-key gap exceeds granular limit")
                if recovery_pose_count < fspec["min_recovery"]:
                    issues.append(f"{label} lacks multi-beat recovery pose density")
                if minimum_rotation_channels < fspec["min_rotation_channels"]:
                    issues.append(f"{label} authored rotation-channel density is too low")
                if biomechanical_events:
                    required_event_frames = set(biomechanical_events.values())
                    if not required_event_frames.issubset(set(pose_frames)):
                        issues.append(f"{label} omits a biomechanical event pose key")
                    wrist_frame = biomechanical_events["wrist_lag_frame"]
                    if grips.get(wrist_frame, -1) < 0.85:
                        issues.append(f"{label} wrist-lag grip is too open")
                    if grips.get(release, 2) > 0.15:
                        issues.append(f"{label} v3 release grip does not snap open")
                    if grips.get(phase_frames[5], 2) > 0.05:
                        issues.append(f"{label} v3 follow-through grip re-closes")

        v5_finger_release_peak_degrees_per_frame: float | None = None
        v5_finger_peak_bone: str | None = None
        v5_finger_peak_frame: int | None = None
        v5_named_phase_knee_angles: list[float] = []
        v5_disc_plane_tilts: dict[str, float] = {}
        v5_max_disc_plane_frame_delta: float | None = None
        v5_max_disc_plane_frame: int | None = None
        v5_max_wrist_additive: float | None = None
        v5_max_wrist_additive_frame: int | None = None
        if version in MIXED_SPACE_VERSIONS:
            sampled_grips = {
                frame: sampled_catmull_scalar(pose_frames, grips, frame)
                for frame in range(frames + 1)
            } if len(grips) == len(pose_frames) and len(pose_frames) >= 2 else {}
            for bone, coefficients in V5_GENERATED_FINGER_ROTATION_COEFFICIENTS.items():
                for frame in range(1, frames + 1):
                    if not sampled_grips:
                        break
                    earlier_rotation = tuple(
                        coefficient * sampled_grips[frame - 1]
                        for coefficient in coefficients
                    )
                    later_rotation = tuple(
                        coefficient * sampled_grips[frame]
                        for coefficient in coefficients
                    )
                    finger_delta = quaternion_geodesic_degrees(
                        earlier_rotation, later_rotation
                    )
                    if (
                        v5_finger_release_peak_degrees_per_frame is None
                        or finger_delta > v5_finger_release_peak_degrees_per_frame
                    ):
                        v5_finger_release_peak_degrees_per_frame = finger_delta
                        v5_finger_peak_bone = bone
                        v5_finger_peak_frame = frame
            if v5_finger_release_peak_degrees_per_frame is None:
                issues.append(f"{label} has no v5 finger-opening continuity samples")
            elif (
                v5_finger_release_peak_degrees_per_frame
                > V5_MAX_FINGER_RELEASE_DEGREES_PER_FRAME + 1e-6
            ):
                issues.append(
                    f"{label} synthesized finger {v5_finger_peak_bone} opening exceeds "
                    f"{V5_MAX_FINGER_RELEASE_DEGREES_PER_FRAME:.1f} degrees/frame "
                    f"at frame {v5_finger_peak_frame} "
                    f"(observed {v5_finger_release_peak_degrees_per_frame:.3f})"
                )
            for phase_frame in [*phase_frames, release]:
                phase_rotations = rotations_by_frame.get(phase_frame, {})
                for calf in ("calf_l", "calf_r"):
                    calf_rotation = phase_rotations.get(calf)
                    if calf_rotation is None:
                        issues.append(
                            f"{label} named phase frame {phase_frame} omits {calf}"
                        )
                        continue
                    v5_named_phase_knee_angles.append(
                        180.0 - abs(calf_rotation[2])
                    )
            if len(v5_named_phase_knee_angles) != 2 * (len(PHASES) + 1):
                issues.append(f"{label} v5 named-phase knee sample count differs")
            elif not (
                V5_NAMED_PHASE_KNEE_ANGLE_RANGE_DEGREES[0]
                    <= min(v5_named_phase_knee_angles)
                and max(v5_named_phase_knee_angles)
                    <= V5_NAMED_PHASE_KNEE_ANGLE_RANGE_DEGREES[1]
            ):
                issues.append(
                    f"{label} v5 named-phase geometric knee range is implausible: "
                    f"{min(v5_named_phase_knee_angles):.3f}.."
                    f"{max(v5_named_phase_knee_angles):.3f} degrees"
                )

            disc_chain_bones = (
                "spine_04", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
            )
            sampled_disc_normals: dict[int, tuple[float, float, float]] = {}
            if len(pose_frames) >= 2:
                try:
                    for frame in range(frames + 1):
                        sampled_rotations = {
                            bone: sampled_catmull_rotation(
                                pose_frames, rotations_by_frame, bone, frame
                            )
                            for bone in disc_chain_bones
                        }
                        sampled_disc_normals[frame] = v5_disc_plane_normal(
                            sampled_rotations
                        )
                        wrist_additive = quaternion_geodesic_degrees(
                            (0.0, 0.0, 0.0), sampled_rotations["hand_r"]
                        )
                        if (
                            v5_max_wrist_additive is None
                            or wrist_additive > v5_max_wrist_additive
                        ):
                            v5_max_wrist_additive = wrist_additive
                            v5_max_wrist_additive_frame = frame
                except (KeyError, RecipeError) as exc:
                    issues.append(f"{label} cannot reconstruct v5 disc plane: {exc}")
                    sampled_disc_normals = {}

            if sampled_disc_normals:
                for frame in range(1, frames + 1):
                    plane_delta = disc_plane_delta_degrees(
                        sampled_disc_normals[frame - 1], sampled_disc_normals[frame]
                    )
                    if (
                        v5_max_disc_plane_frame_delta is None
                        or plane_delta > v5_max_disc_plane_frame_delta
                    ):
                        v5_max_disc_plane_frame_delta = plane_delta
                        v5_max_disc_plane_frame = frame
                if (
                    v5_max_disc_plane_frame_delta is not None
                    and v5_max_disc_plane_frame_delta
                        > V5_MAX_DISC_PLANE_FRAME_DELTA_DEGREES + 1e-6
                ):
                    issues.append(
                        f"{label} raw disc plane changes "
                        f"{v5_max_disc_plane_frame_delta:.3f} degrees at frame "
                        f"{v5_max_disc_plane_frame}, above the "
                        f"{V5_MAX_DISC_PLANE_FRAME_DELTA_DEGREES:.1f} "
                        "degrees/frame continuity gate"
                    )
                required_disc_phases = {
                    "Aim": phase_frames[0],
                    "ReachBack": phase_frames[2],
                    "Plant": phase_frames[3],
                    "Release": release,
                } if len(phase_frames) == len(PHASES) else {}
                for phase_name, phase_frame in required_disc_phases.items():
                    observed_tilt = disc_plane_tilt_degrees(
                        sampled_disc_normals[phase_frame]
                    )
                    v5_disc_plane_tilts[phase_name] = observed_tilt
                    limit = V5_DISC_PLANE_PHASE_LIMITS_DEGREES[name][phase_name]
                    if observed_tilt > limit + 1e-6:
                        issues.append(
                            f"{label} raw disc plane tilt at {phase_name} is "
                            f"{observed_tilt:.3f} degrees, above {limit:.1f}"
                        )
                    if version == "v6":
                        target = V6_DISC_PLANE_PHASE_TARGETS_DEGREES[name][
                            phase_name
                        ]
                        if abs(observed_tilt - target) > (
                            V6_DISC_PLANE_PHASE_TARGET_TOLERANCE_DEGREES + 1e-6
                        ):
                            issues.append(
                                f"{label} v6 raw disc plane tilt at {phase_name} "
                                f"is {observed_tilt:.3f} degrees rather than the "
                                f"preserved v005 target {target:.1f}"
                            )
            if (
                v5_max_wrist_additive is not None
                and v5_max_wrist_additive > V5_MAX_WRIST_ADDITIVE_DEGREES + 1e-6
            ):
                issues.append(
                    f"{label} hand_r additive reaches "
                    f"{v5_max_wrist_additive:.3f} degrees at frame "
                    f"{v5_max_wrist_additive_frame}, above "
                    f"{V5_MAX_WRIST_ADDITIVE_DEGREES:.1f}"
                )

        arm_spatial_observation: dict[str, Any] = {}
        arm_spatial_gate_issues: list[str] = []
        if version in MIXED_SPACE_VERSIONS and len(pose_frames) >= 2:
            try:
                (
                    arm_spatial_observation,
                    arm_spatial_gate_issues,
                ) = mixed_space_arm_spatial_observation(
                    pose_frames, rotations_by_frame, frames,
                    phase_frames, release,
                )
            except (KeyError, RecipeError) as exc:
                arm_spatial_gate_issues = [
                    f"mixed-space arm spatial reconstruction failed: {exc}"
                ]
            # v005 remains an immutable historical recipe that must still
            # validate. Its failures are retained as comparator evidence;
            # only v006 claims and enforces the corrected spatial contract.
            if version == "v6":
                issues.extend(
                    f"{label} v6 spatial gate: {item}"
                    for item in arm_spatial_gate_issues
                )

        release_hand_peak_degrees_per_frame: float | None = None
        release_hand_interval_count = 0
        if version in GRANULAR_VERSIONS:
            fspec = V3_FAMILIES[name]
            window_start = release - V3_RELEASE_HAND_WINDOW_RADIUS_FRAMES
            window_end = release + V3_RELEASE_HAND_WINDOW_RADIUS_FRAMES
            for earlier_frame, later_frame in zip(pose_frames, pose_frames[1:]):
                if later_frame < window_start or earlier_frame > window_end:
                    continue
                earlier_hand = rotations_by_frame.get(earlier_frame, {}).get("hand_r")
                later_hand = rotations_by_frame.get(later_frame, {}).get("hand_r")
                if earlier_hand is None or later_hand is None:
                    issues.append(
                        f"{label} hand_r rotation is missing around release "
                        f"between frames {earlier_frame} and {later_frame}"
                    )
                    continue
                frame_delta = later_frame - earlier_frame
                if frame_delta <= 0:
                    continue
                release_hand_interval_count += 1
                degrees_per_frame = (
                    quaternion_geodesic_degrees(earlier_hand, later_hand)
                    / frame_delta
                )
                release_hand_peak_degrees_per_frame = max(
                    release_hand_peak_degrees_per_frame or 0.0,
                    degrees_per_frame,
                )
            if release_hand_interval_count == 0:
                issues.append(f"{label} has no hand_r continuity samples around release")
            elif release_hand_peak_degrees_per_frame is not None and (
                release_hand_peak_degrees_per_frame
                > fspec["max_release_hand_geodesic_degrees_per_frame"] + 1e-6
            ):
                issues.append(
                    f"{label} hand_r quaternion/geodesic continuity exceeds "
                    f"{fspec['max_release_hand_geodesic_degrees_per_frame']:.3f} "
                    "degrees/frame around release "
                    f"(observed {release_hand_peak_degrees_per_frame:.3f})"
                )

        curves = family["curves"]
        expected_curves = V3_CURVES if version in GRANULAR_VERSIONS else CURVES
        curve_values_by_frame: dict[str, dict[int, float]] = {}
        continuous_curve_min: float | None = None
        continuous_curve_max: float | None = None
        if type(curves) is not dict or set(curves) != expected_curves:
            issues.append(
                f"{label}.curves differs from the exact "
                f"{len(expected_curves)}-curve {version} contract"
            )
        else:
            for curve_name, keys in curves.items():
                if type(keys) is not list or len(keys) < 2:
                    issues.append(f"{label}.curves.{curve_name} has too few keys")
                    continue
                curve_frames: list[int] = []
                parsed_curve: dict[int, float] = {}
                for key in keys:
                    if (
                        type(key) is not list or len(key) != 2
                        or type(key[0]) is not int or not finite_number(key[1])
                        or not 0 <= key[1] <= 1
                    ):
                        issues.append(f"{label}.curves.{curve_name} has an invalid key")
                        continue
                    curve_frames.append(key[0])
                    parsed_curve[key[0]] = float(key[1])
                    if not 0 <= key[0] <= frames:
                        issues.append(
                            f"{label}.curves.{curve_name} frame is out of bounds"
                        )
                if curve_frames != sorted(set(curve_frames)):
                    issues.append(f"{label}.curves.{curve_name} frames are not ordered")
                curve_values_by_frame[curve_name] = parsed_curve
                if version in GRANULAR_VERSIONS and len(parsed_curve) >= 2:
                    ordered = sorted(parsed_curve.items())
                    for (frame_a, value_a), (frame_b, value_b) in zip(
                        ordered, ordered[1:]
                    ):
                        samples = max(1, (frame_b - frame_a) * 4)
                        for sample in range(samples + 1):
                            alpha = sample / samples
                            value = value_a + (value_b - value_a) * alpha
                            continuous_curve_min = (
                                value if continuous_curve_min is None
                                else min(continuous_curve_min, value)
                            )
                            continuous_curve_max = (
                                value if continuous_curve_max is None
                                else max(continuous_curve_max, value)
                            )
                            if not -0.001 <= value <= 1.001:
                                issues.append(
                                    f"{label}.curves.{curve_name} leaves [0,1] "
                                    "under bounded subframe interpolation"
                                )
                                break
        if version in GRANULAR_VERSIONS and biomechanical_events:
            required_curve_events = [
                ("DG_WeightShiftAlpha", "weight_shift_frame", 1.0),
                ("DG_BraceCompressionAlpha", "brace_compression_frame", 1.0),
                ("DG_HipDriveAlpha", "hip_fire_frame", 1.0),
                ("DG_TorsoDriveAlpha", "torso_fire_frame", 1.0),
                ("DG_ShoulderDriveAlpha", "shoulder_fire_frame", 1.0),
                ("DG_ElbowLeadAlpha", "elbow_lead_frame", 1.0),
                ("DG_WristLagAlpha", "wrist_lag_frame", 1.0),
                ("DG_FingerReleaseAlpha", "release_frame", 1.0),
                (
                    "DG_OffArmCounterbalanceAlpha",
                    "off_arm_counterbalance_frame", 1.0,
                ),
                ("DG_GazeTargetAlpha", "gaze_reacquire_frame", 1.0),
                ("DG_DiscPlaneAlpha", "disc_reachback_plane_frame", 1.0),
                ("DG_BraceExtensionAlpha", "brace_extension_frame", 1.0),
                (
                    "DG_RecoveryBeatAlpha",
                    "recovery_deceleration_frame", 0.35,
                ),
                ("DG_RecoveryBeatAlpha", "recovery_recenter_frame", 0.70),
                ("DG_RecoveryBeatAlpha", "recovery_settle_frame", 1.0),
            ]
            for curve_name, event_name, expected_value in required_curve_events:
                actual = curve_values_by_frame.get(curve_name, {}).get(
                    biomechanical_events[event_name]
                )
                if actual is None or not math.isclose(
                    actual, expected_value, abs_tol=0.001
                ):
                    issues.append(
                        f"{label}.curves.{curve_name} omits exact {event_name} key"
                    )

        family_observations.append({
            "family": name,
            "pose_key_count": len(poses),
            "maximum_pose_key_gap_frames": max(
                (later - earlier for earlier, later in zip(
                    pose_frames, pose_frames[1:]
                )),
                default=0,
            ),
            "recovery_pose_key_count": (
                sum(phase_frames[5] <= frame <= finish for frame in pose_frames)
                if len(phase_frames) == len(PHASES) else 0
            ),
            "minimum_authored_rotation_channels_per_pose": min(
                rotation_channel_counts.values(), default=0
            ),
            "curve_count": len(curves) if type(curves) is dict else 0,
            "continuous_curve_bounds_sampled": version in GRANULAR_VERSIONS,
            "continuous_curve_min": continuous_curve_min,
            "continuous_curve_max": continuous_curve_max,
            "release_hand_geodesic_window_radius_frames": (
                V3_RELEASE_HAND_WINDOW_RADIUS_FRAMES
                if version in GRANULAR_VERSIONS else None
            ),
            "release_hand_geodesic_interval_count": (
                release_hand_interval_count
                if version in GRANULAR_VERSIONS else None
            ),
            "release_hand_peak_geodesic_degrees_per_frame": (
                release_hand_peak_degrees_per_frame
                if version in GRANULAR_VERSIONS else None
            ),
            "finger_release_peak_degrees_per_frame": (
                v5_finger_release_peak_degrees_per_frame
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_generated_finger_frame_delta_degrees": (
                v5_finger_release_peak_degrees_per_frame
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_generated_finger_frame_delta_bone": (
                v5_finger_peak_bone if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_generated_finger_frame_delta_frame": (
                v5_finger_peak_frame if version in MIXED_SPACE_VERSIONS else None
            ),
            "named_phase_knee_sample_count": (
                len(v5_named_phase_knee_angles)
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "minimum_named_phase_knee_angle_degrees": (
                min(v5_named_phase_knee_angles)
                if version in MIXED_SPACE_VERSIONS
                and v5_named_phase_knee_angles else None
            ),
            "maximum_named_phase_knee_angle_degrees": (
                max(v5_named_phase_knee_angles)
                if version in MIXED_SPACE_VERSIONS
                and v5_named_phase_knee_angles else None
            ),
            "disc_plane_tilt_degrees_by_phase": (
                v5_disc_plane_tilts if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_disc_plane_frame_delta_degrees": (
                v5_max_disc_plane_frame_delta
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_disc_plane_frame_delta_frame": (
                v5_max_disc_plane_frame
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_wrist_additive_degrees": (
                v5_max_wrist_additive
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "maximum_wrist_additive_frame": (
                v5_max_wrist_additive_frame
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "arm_spatial_gate_pass": (
                not arm_spatial_gate_issues
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "arm_spatial_gate_issues": (
                arm_spatial_gate_issues
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "arm_spatial_metrics": (
                arm_spatial_observation
                if version in MIXED_SPACE_VERSIONS else None
            ),
            "presentation_root_excursion_cm": (
                max(item[0] for item in root_values) - min(item[0] for item in root_values)
                if root_values else 0.0
            ),
            "recommended_power_min": power_min,
            "recommended_power_max": power_max,
        })

    if len(expected_paths) != 6:
        issues.append("sequence/montage target paths are not six distinct assets")
    if version == "v4":
        try:
            v3_recipe = load_json(root / VERSION_SPECS["v3"]["path"])
        except RecipeError as exc:
            issues.append(f"immutable v3 recipe baseline is unreadable: {exc}")
        else:
            normalized_v4 = normalize_v4_to_v3(recipe)
            normalized_v4["schema_version"] = 3
            if normalized_v4 != v3_recipe:
                issues.append(
                    "v4 motion intent differs from the immutable v3 recipe; "
                    "only version/path identity and rotation_space may change"
                )
    if version == "v5":
        try:
            v4_recipe = load_json(root / VERSION_SPECS["v4"]["path"])
        except RecipeError as exc:
            issues.append(f"immutable v4 recipe baseline is unreadable: {exc}")
        else:
            normalized_v5 = normalize_v5_to_v4(recipe)
            normalized_v5["schema_version"] = 4
            normalized_v5["rotation_space"] = "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"
            v4_families = {
                family["family"]: family for family in v4_recipe.get("families", [])
            }
            for normalized_family in normalized_v5.get("families", []):
                family_name = normalized_family.get("family")
                baseline_family = v4_families.get(family_name, {})
                baseline_poses = {
                    pose.get("frame"): pose
                    for pose in baseline_family.get("pose_keys", [])
                }
                allowed_corrections = V5_DISC_PLANE_CORRECTIONS.get(
                    family_name, {}
                )
                for pose in normalized_family.get("pose_keys", []):
                    frame = pose.get("frame")
                    rotations = pose.get("rotation_degrees", {})
                    baseline_rotations = baseline_poses.get(frame, {}).get(
                        "rotation_degrees", {}
                    )
                    correction = allowed_corrections.get(frame)
                    for correction_index, bone in enumerate(
                        ("lowerarm_r", "hand_r")
                    ):
                        baseline_value = baseline_rotations.get(bone)
                        expected_value = (
                            correction[correction_index]
                            if correction is not None else baseline_value
                        )
                        actual_value = rotations.get(bone)
                        if (
                            type(actual_value) is not list
                            or tuple(actual_value) != tuple(expected_value or ())
                        ):
                            issues.append(
                                f"v5 {family_name} frame {frame} {bone} differs "
                                "from the exact disc-plane correction contract"
                            )
                        if baseline_value is not None:
                            # Restore the immutable baseline only after the exact
                            # v5 correction value has been checked above.
                            rotations[bone] = baseline_value
                    for calf in ("calf_l", "calf_r"):
                        value = rotations.get(calf)
                        if type(value) is list and len(value) == 3:
                            rotations[calf] = [value[2], value[1], value[0]]
            if normalized_v5 != v4_recipe:
                issues.append(
                    "v5 motion intent differs from the immutable v4 recipe outside "
                    "the exact mixed-space contract and calf Pitch-to-positive-Roll "
                    "axis migration plus enumerated lowerarm/hand disc-plane corrections"
                )
    if version == "v6":
        try:
            v5_recipe = load_json(root / VERSION_SPECS["v5"]["path"])
            v4_recipe = load_json(root / VERSION_SPECS["v4"]["path"])
        except RecipeError as exc:
            issues.append(f"immutable v004/v005 recipe baseline is unreadable: {exc}")
        else:
            normalized_v6 = normalize_v6_to_v5(recipe)
            normalized_v6["schema_version"] = 5
            v5_families = {
                family["family"]: family
                for family in v5_recipe.get("families", [])
            }
            v4_families = {
                family["family"]: family
                for family in v4_recipe.get("families", [])
            }
            observations_by_name = {
                item["family"]: item for item in family_observations
            }
            for normalized_family, authored_family in zip(
                normalized_v6.get("families", []), recipe.get("families", [])
            ):
                family_name = normalized_family.get("family")
                baseline_family = v5_families.get(family_name, {})
                v4_family = v4_families.get(family_name, {})
                baseline_poses = {
                    pose.get("frame"): pose
                    for pose in baseline_family.get("pose_keys", [])
                }
                v4_poses = {
                    pose.get("frame"): pose
                    for pose in v4_family.get("pose_keys", [])
                }
                maximum_component_error = 0.0
                maximum_component_error_bone: str | None = None
                maximum_component_error_frame: int | None = None
                for normalized_pose, authored_pose in zip(
                    normalized_family.get("pose_keys", []),
                    authored_family.get("pose_keys", []),
                ):
                    frame = normalized_pose.get("frame")
                    normalized_rotations = normalized_pose.get(
                        "rotation_degrees", {}
                    )
                    baseline_rotations = baseline_poses.get(frame, {}).get(
                        "rotation_degrees", {}
                    )
                    for bone in V6_ARM_BONES:
                        if bone in baseline_rotations:
                            normalized_rotations[bone] = baseline_rotations[bone]

                    authored_rotations = {
                        bone: vector(raw)
                        for bone, raw in authored_pose.get(
                            "rotation_degrees", {}
                        ).items()
                    }
                    if any(
                        authored_rotations.get(bone) is None
                        for bone in ("spine_04", *V6_ARM_BONES)
                    ):
                        issues.append(
                            f"v6 {family_name} frame {frame} lacks an arm "
                            "channel required for v004 component-intent proof"
                        )
                        continue
                    _, actual_components, _ = mixed_space_arm_pose(
                        authored_rotations  # type: ignore[arg-type]
                    )
                    v4_rotations = v4_poses.get(frame, {}).get(
                        "rotation_degrees", {}
                    )
                    # hand_r is the deliberate bounded disc-plane exception;
                    # its joint position is set by lowerarm_r and its local
                    # rotation is independently bounded by the exact v005
                    # plane targets plus the 25-degree wrist gate.
                    for bone in (*V6_ARM_CHAINS["l"], *V6_ARM_CHAINS["r"][:-1]):
                        raw_intent = vector(v4_rotations.get(bone))
                        if raw_intent is None:
                            issues.append(
                                f"immutable v004 {family_name} frame {frame} "
                                f"omits {bone}"
                            )
                            continue
                        expected_component = quaternion_multiply(
                            unreal_rotator_quaternion(raw_intent),
                            V6_REFERENCE_COMPONENT_ROTATIONS[bone],
                        )
                        component_error = quaternion_angular_distance_degrees(
                            actual_components[bone], expected_component
                        )
                        if component_error > maximum_component_error:
                            maximum_component_error = component_error
                            maximum_component_error_bone = bone
                            maximum_component_error_frame = frame
                observation = observations_by_name.get(family_name)
                if observation is not None:
                    observation[
                        "maximum_v004_arm_component_intent_error_degrees"
                    ] = maximum_component_error
                    observation[
                        "maximum_v004_arm_component_intent_error_bone"
                    ] = maximum_component_error_bone
                    observation[
                        "maximum_v004_arm_component_intent_error_frame"
                    ] = maximum_component_error_frame
                if maximum_component_error > (
                    V6_MAX_COMPONENT_INTENT_ERROR_DEGREES + 1e-9
                ):
                    issues.append(
                        f"v6 {family_name} component-intent conversion reaches "
                        f"{maximum_component_error:.6f} degrees at frame "
                        f"{maximum_component_error_frame} bone "
                        f"{maximum_component_error_bone}, above "
                        f"{V6_MAX_COMPONENT_INTENT_ERROR_DEGREES:.3f}"
                    )
            if normalized_v6 != v5_recipe:
                issues.append(
                    "v6 differs from immutable v005 outside version/path identity "
                    "and the exact eight-arm local-space regeneration contract"
                )
    if version in {"v2", "v3", "v4", "v5", "v6"} and check_source_seams:
        utility = root / "Source/DiscGolfTourEditor/DiscGolfProductionMotionAuthoringUtility.cpp"
        header = root / "Source/DiscGolfTour/DiscGolfProductionMotion.h"
        try:
            utility_text = utility.read_text(encoding="utf-8")
            header_text = header.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            issues.append(f"versioned native authoring seam is unreadable: {exc}")
        else:
            utility_tokens = [
                "Entry.MotionFamilyId = FName(*Recipe.Families[Index].Family);",
                "AuthorProductionMotionAssetsForVersion",
                "ValidateProductionMotionAssetsForVersion",
                "GAMEPLAY_PAWN_CAPSULE_PRESENTATION_ROOT_ONLY",
                (
                    "CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"
                    if version in GRANULAR_VERSIONS
                    else "CATMULL_ROM_PHASE_SHAPED"
                ),
            ]
            if version in GRANULAR_VERSIONS:
                utility_tokens.extend((
                    "Key.InterpMode = Recipe.Version.SchemaVersion >= 3",
                    "? RCIM_Linear : RCIM_Cubic",
                    "constexpr int32 SamplesPerFrame = 4",
                ))
            if version == "v4":
                utility_tokens.extend((
                    "BuildV4AuthoredLocalRotations",
                    "ValidateV4ComponentSpaceTracks",
                    "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE",
                    "COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS",
                ))
            if version in MIXED_SPACE_VERSIONS:
                utility_tokens.extend((
                    "BuildMixedSpaceAuthoredLocalRotations",
                    "ValidateMixedSpaceTracks",
                    V5_ROTATION_SPACE,
                    "MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS",
                    V5_ROOT_TRACK_POLICY,
                    "SchemaVersion >= 5 ? 24.0f : 35.0f",
                    "SchemaVersion >= 5 ? 24.0f : 25.0f",
                    "NamedPhaseFrames.Add(Family.ReleaseFrame);",
                    "maximum_finger_frame_delta_degrees",
                    "named_phase_knee_sample_count",
                ))
            if version == "v6":
                utility_tokens.extend((
                    "V6ArmSpatialThresholds",
                    "ReachBackThrowingHandRadialReachCm",
                    "MinimumReleaseFollowThrowingHandLateralClearanceCm",
                    "MaximumThrowingHandDirectionFrameDeltaDegrees",
                    'TEXT("arm_spatial_gate_applied")',
                    'TEXT("arm_spatial_gate_passed")',
                ))
            for token in utility_tokens:
                if token not in utility_text:
                    issues.append(f"versioned native authoring seam missing: {token}")
            header_tokens = [f"namespace {version.upper()}"]
            if version == "v6":
                header_tokens.extend((
                    "ActiveVersion = V6::Version",
                    "ActiveAssetRevision = V6::AssetRevision",
                ))
            for token in header_tokens:
                if token not in header_text:
                    issues.append(f"versioned runtime path seam missing: {token}")
    return issues, {
        "recipe_version": version,
        "asset_revision": spec["revision"],
        "animated_bone_count": len(bones),
        "family_count": len(families),
        "families": family_observations,
        "root_motion_enabled": recipe.get("root_motion_enabled"),
        "world_motion_authority": recipe.get(
            "world_motion_authority", "GAMEPLAY_PAWN_CAPSULE"
        ),
        "human_animation_approval": recipe.get("human_animation_approval"),
        "human_disc_contact_approval": recipe.get("human_disc_contact_approval"),
        "biomechanical_model": recipe.get("biomechanical_model"),
        "disc_contact_model": recipe.get("disc_contact_model"),
        "recovery_model": recipe.get("recovery_model"),
        "rotation_space": recipe.get("rotation_space"),
        "root_track_policy": recipe.get("root_track_policy"),
        "component_rotation_bone_count": len(
            recipe.get("component_rotation_bones", [])
        ),
        "local_rotation_bone_count": len(recipe.get("local_rotation_bones", [])),
    }


def validate_path(root: Path, version: str) -> tuple[list[str], dict[str, Any]]:
    spec = VERSION_SPECS[version]
    path = root / spec["path"]
    try:
        recipe = load_json(path)
    except RecipeError as exc:
        return [str(exc)], {}
    return validate_recipe(recipe, version, root)


def run_self_test(root: Path) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    caught = 0
    for version in VERSION_SPECS:
        issues, _ = validate_path(root, version)
        if issues:
            failures.append(f"{version} baseline failed: {'; '.join(issues[:5])}")
    v2 = load_json(root / VERSION_SPECS["v2"]["path"])
    mutations = [
        ("mocap-claim", lambda d: d.__setitem__("motion_capture_claim", True)),
        ("source", lambda d: d.__setitem__("source_kind", "MOCAP")),
        ("approval", lambda d: d.__setitem__("human_animation_approval", True)),
        ("root-motion", lambda d: d.__setitem__("root_motion_enabled", True)),
        ("authority", lambda d: d.__setitem__("world_motion_authority", "ANIMATION")),
        ("revision", lambda d: d.__setitem__("asset_revision", "v001")),
        ("bone", lambda d: d["animated_bones"].pop()),
        ("path", lambda d: d["families"][0].__setitem__("sequence_object_path", "/Game/Wrong.Asset")),
        ("motion-count", lambda d: d["families"].pop()),
        ("pose-density", lambda d: d["families"][0].__setitem__("pose_keys", d["families"][0]["pose_keys"][:8])),
        ("root-scale", lambda d: d["families"][0].__setitem__("presentation_root_scale", 1.0)),
        ("root-limit", lambda d: d["families"][0].__setitem__("max_presentation_root_translation_cm", 50.0)),
        ("release-order", lambda d: d["families"][0].__setitem__("release_frame", 100)),
        ("duplicate-pose", lambda d: d["families"][0]["pose_keys"].insert(1, copy.deepcopy(d["families"][0]["pose_keys"][0]))),
        ("release-grip", lambda d: d["families"][0]["pose_keys"][10].__setitem__("throwing_hand_grip_alpha", 0.9)),
    ]
    for name, mutate in mutations:
        candidate = copy.deepcopy(v2)
        mutate(candidate)
        issues, _ = validate_recipe(candidate, "v2", root, check_source_seams=False)
        if issues:
            caught += 1
        else:
            failures.append(f"v2 mutation survived: {name}")
    v3 = load_json(root / VERSION_SPECS["v3"]["path"])
    v3_mutations = [
        (
            "biomechanical-model",
            lambda d: d.__setitem__("biomechanical_model", "GENERIC"),
        ),
        (
            "interpolation",
            lambda d: d.__setitem__("interpolation_mode", "LINEAR"),
        ),
        (
            "declared-gap",
            lambda d: d["families"][0].__setitem__(
                "maximum_pose_key_gap_frames", 12
            ),
        ),
        (
            "actual-gap",
            lambda d: d["families"][0].__setitem__(
                "pose_keys",
                [p for p in d["families"][0]["pose_keys"] if p["frame"] != 6],
            ),
        ),
        (
            "recovery-density",
            lambda d: d["families"][0].__setitem__(
                "pose_keys",
                [p for p in d["families"][0]["pose_keys"] if p["frame"] != 96],
            ),
        ),
        (
            "rotation-density",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].pop("hand_l"),
        ),
        (
            "event-order",
            lambda d: d["families"][0]["biomechanical_events"].__setitem__(
                "weight_shift_frame", 65
            ),
        ),
        (
            "event-curve",
            lambda d: d["families"][0]["curves"]["DG_HipDriveAlpha"][2].__setitem__(
                1, 0.5
            ),
        ),
        (
            "curve-count",
            lambda d: d["families"][0]["curves"].pop("DG_WristLagAlpha"),
        ),
        (
            "wrist-lag-grip",
            lambda d: next(
                p for p in d["families"][0]["pose_keys"] if p["frame"] == 83
            ).__setitem__("throwing_hand_grip_alpha", 0.5),
        ),
        (
            "putt-release-hand-geodesic-pop",
            lambda d: next(
                p for p in d["families"][2]["pose_keys"] if p["frame"] == 38
            )["rotation_degrees"].__setitem__("hand_r", [0.0, 90.0, 90.0]),
        ),
    ]
    for name, mutate in v3_mutations:
        candidate = copy.deepcopy(v3)
        mutate(candidate)
        issues, _ = validate_recipe(candidate, "v3", root, check_source_seams=False)
        if issues:
            caught += 1
        else:
            failures.append(f"v3 mutation survived: {name}")
    v4 = load_json(root / VERSION_SPECS["v4"]["path"])
    v4_mutations = [
        (
            "rotation-space",
            lambda d: d.__setitem__("rotation_space", "LOCAL_SPACE_ADDITIVE"),
        ),
        (
            "motion-intent-drift",
            lambda d: next(
                p for p in d["families"][0]["pose_keys"] if p["frame"] == 84
            )["rotation_degrees"].__setitem__("hand_l", [1.0, 1.0, 1.0]),
        ),
    ]
    for name, mutate in v4_mutations:
        candidate = copy.deepcopy(v4)
        mutate(candidate)
        issues, _ = validate_recipe(candidate, "v4", root, check_source_seams=False)
        if issues:
            caught += 1
        else:
            failures.append(f"v4 mutation survived: {name}")
    v5 = load_json(root / VERSION_SPECS["v5"]["path"])
    v5_mutations = [
        (
            "rotation-space",
            lambda d: d.__setitem__("rotation_space", "COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"),
        ),
        (
            "root-policy",
            lambda d: d.__setitem__("root_track_policy", "ROOT_ROTATION_ALLOWED"),
        ),
        (
            "partition-overlap",
            lambda d: d["local_rotation_bones"].insert(0, "pelvis"),
        ),
        (
            "missing-component-pose",
            lambda d: d["families"][0]["pose_keys"][0]["rotation_degrees"].pop("pelvis"),
        ),
        (
            "root-rotation",
            lambda d: d["families"][0]["pose_keys"][0]["rotation_degrees"].__setitem__(
                "root", [0.0, 1.0, 0.0]
            ),
        ),
        (
            "non-root-translation",
            lambda d: d["families"][0]["pose_keys"][0]["translation_cm"].__setitem__(
                "pelvis", [0.0, 0.0, 0.0]
            ),
        ),
        (
            "finger-release-pop",
            lambda d: next(
                p for p in d["families"][0]["pose_keys"] if p["frame"] == 83
            ).__setitem__("throwing_hand_grip_alpha", 0.99),
        ),
        (
            "calf-pitch-axis",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("calf_l", [10.0, 0.0, 0.0]),
        ),
        (
            "calf-roll-sign",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("calf_l", [0.0, 0.0, -10.0]),
        ),
        (
            "straight-named-phase-knee",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("calf_l", [0.0, 0.0, 1.0]),
        ),
        (
            "numeric-intent-drift",
            lambda d: next(
                p for p in d["families"][0]["pose_keys"] if p["frame"] == 84
            )["rotation_degrees"].__setitem__("hand_r", [1.0, 2.0, 3.0]),
        ),
    ]
    for name, mutate in v5_mutations:
        candidate = copy.deepcopy(v5)
        mutate(candidate)
        issues, _ = validate_recipe(candidate, "v5", root, check_source_seams=False)
        if issues:
            caught += 1
        else:
            failures.append(f"v5 mutation survived: {name}")

    # V005 remains a valid immutable baseline, but its reconstructed arms must
    # provide negative comparator evidence for every new v006 spatial axis.
    v5_baseline_issues, v5_observations = validate_recipe(
        v5, "v5", root, check_source_seams=False
    )
    if v5_baseline_issues:
        failures.append(
            "v5 comparator baseline failed before spatial comparison: "
            + "; ".join(v5_baseline_issues[:3])
        )
    else:
        v5_spatial = {
            item["family"]: item for item in v5_observations["families"]
        }
        comparator_checks = {
            "torso-direction": any(
                item["arm_spatial_metrics"][
                    "minimum_directional_dominance_margin_cm"
                ] < V6_MIN_DIRECTIONAL_DOMINANCE_MARGIN_CM
                for item in v5_spatial.values()
            ),
            "vertical-collapse": all(
                item["arm_spatial_metrics"]["minimum_named_phase_vertical_cm"]
                < V6_MIN_NAMED_PHASE_VERTICAL_CM
                for item in v5_spatial.values()
            ),
            "reach": all(
                item["arm_spatial_metrics"]["reachback_radial_reach_cm"]
                < V6_MIN_REACHBACK_RADIAL_REACH_CM
                for item in v5_spatial.values()
            ),
            "clearance": all(
                item["arm_spatial_metrics"][
                    "minimum_release_followthrough_lateral_clearance_cm"
                ] < V6_MIN_RELEASE_FOLLOWTHROUGH_LATERAL_CLEARANCE_CM
                for item in v5_spatial.values()
            ),
            "angular-smoothness": any(
                item["arm_spatial_metrics"][
                    "maximum_torso_relative_hand_direction_delta_degrees"
                ] > V6_MAX_TORSO_RELATIVE_HAND_DIRECTION_DELTA_DEGREES
                for item in v5_spatial.values()
            ),
        }
        for gate_name, failed_as_expected in comparator_checks.items():
            if failed_as_expected:
                caught += 1
            else:
                failures.append(f"v5 comparator no longer fails {gate_name}")
        for family_name, item in v5_spatial.items():
            if item["arm_spatial_gate_pass"] is False and item[
                "arm_spatial_gate_issues"
            ]:
                caught += 1
            else:
                failures.append(
                    f"v5 {family_name} unexpectedly passes the v6 spatial contract"
                )

    v6 = load_json(root / VERSION_SPECS["v6"]["path"])
    v6_baseline_issues, v6_observations = validate_recipe(
        v6, "v6", root, check_source_seams=False
    )
    if v6_baseline_issues:
        failures.append(
            "v6 baseline failed before mutation tests: "
            + "; ".join(v6_baseline_issues[:3])
        )
    else:
        for item in v6_observations["families"]:
            if item["arm_spatial_gate_pass"] is True and not item[
                "arm_spatial_gate_issues"
            ]:
                caught += 1
            else:
                failures.append(
                    f"v6 {item['family']} does not pass its spatial contract"
                )

    def restore_v5_drive_arm_payload(candidate: dict[str, Any]) -> None:
        baseline_poses = {
            pose["frame"]: pose
            for pose in v5["families"][0]["pose_keys"]
        }
        for pose in candidate["families"][0]["pose_keys"]:
            baseline_rotations = baseline_poses[pose["frame"]][
                "rotation_degrees"
            ]
            for bone in V6_ARM_BONES:
                pose["rotation_degrees"][bone] = copy.deepcopy(
                    baseline_rotations[bone]
                )

    v6_mutations: list[tuple[str, Any, str]] = [
        (
            "non-arm-immutable-drift",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("pelvis", [1.0, 2.0, 3.0]),
            "outside version/path identity and the exact eight-arm",
        ),
        (
            "component-intent-arm-drift",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("clavicle_r", [3.0, 4.0, 5.0]),
            "component-intent conversion reaches",
        ),
        (
            "v005-spatial-regression",
            restore_v5_drive_arm_payload,
            "v6 spatial gate",
        ),
        (
            "disc-plane-target-drift",
            lambda d: d["families"][0]["pose_keys"][0][
                "rotation_degrees"
            ].__setitem__("hand_r", [0.0, 20.0, 20.0]),
            "preserved v005 target",
        ),
    ]
    for name, mutate, required_issue in v6_mutations:
        candidate = copy.deepcopy(v6)
        mutate(candidate)
        issues, _ = validate_recipe(candidate, "v6", root, check_source_seams=False)
        if any(required_issue in issue for issue in issues):
            caught += 1
        elif issues:
            failures.append(
                f"v6 mutation {name} was caught for the wrong reason: "
                + "; ".join(issues[:3])
            )
        else:
            failures.append(f"v6 mutation survived: {name}")

    identity_mismatch, _ = validate_recipe(
        v5, "v6", root, check_source_seams=False
    )
    if identity_mismatch:
        caught += 1
    else:
        failures.append("v5 payload survived validation as v6")
    with tempfile.TemporaryDirectory(prefix="dgt-motion-recipe-") as temp_name:
        duplicate = Path(temp_name) / "duplicate.json"
        duplicate.write_text('{"schema":1,"schema":2}', encoding="utf-8")
        try:
            load_json(duplicate)
        except RecipeError:
            caught += 1
        else:
            failures.append("duplicate JSON key survived")
    return not failures, caught, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--motion-version", choices=sorted(VERSION_SPECS))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.self_test:
        passed, caught, failures = run_self_test(root)
        if not passed:
            print("PRODUCTION MOTION RECIPE SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(f"PRODUCTION MOTION RECIPE SELF-TEST PASS caught={caught}")
        return 0
    if args.motion_version is None:
        parser.error("--motion-version is required outside --self-test")
    issues, observations = validate_path(root, args.motion_version)
    if issues:
        print("PRODUCTION MOTION RECIPE INVALID")
        for issue in issues:
            print(f" - {issue}")
        return 1
    print(
        "PRODUCTION MOTION RECIPE PASS "
        + json.dumps(observations, sort_keys=True, separators=(",", ":"))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
