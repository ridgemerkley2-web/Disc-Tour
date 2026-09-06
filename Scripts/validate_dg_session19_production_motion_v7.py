#!/usr/bin/env python3
"""Fail-closed static validation for the append-only v007 motion candidate.

The source-only lane proves deterministic generation, v006 immutability,
non-arm identity, explicit DGMaster source-chain geometry gates, and exact
frame-84 grip/release preservation.  It cannot approve an Unreal asset or a
MetaHuman result.  Consequently the default invocation exits non-zero until a
future runtime-evidence validator replaces the explicit blocker; use
``--source-only`` only to verify this source scaffold.
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

import generate_dg_session19_production_motion_v6 as v6_generator
import generate_dg_session19_production_motion_v7 as v7_generator
import validate_dg_session19_production_motion_recipe as recipe_math


ROOT = Path(__file__).resolve().parents[1]
V6_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"
V7_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json"

FINGER_PREFIXES = ("thumb_", "index_", "middle_", "ring_", "pinky_")
CANONICAL_V006_BYTES = 294788
CANONICAL_V006_SHA256 = (
    "142E64A6BED93039C0C12389E9F7ABD1CEE5A7F4246C7F29E738DEEB45C29163"
)

# These are validator authority, not candidate-controlled metadata.
SUPPORT_GATE_FRAMES = (44, 64, 84, 118)
POWER_POCKET_GATE_FRAMES = (79, 80, 81)
RELEASE_GATE_FRAMES = (84,)
FOLLOWTHROUGH_GATE_FRAMES = (94,)
RECOVERY_GATE_FRAMES = (118,)
PINNED_FRAME_SETS = {
    "support": SUPPORT_GATE_FRAMES,
    "power_pocket": POWER_POCKET_GATE_FRAMES,
    "release": RELEASE_GATE_FRAMES,
    "followthrough": FOLLOWTHROUGH_GATE_FRAMES,
    "recovery": RECOVERY_GATE_FRAMES,
}
SUPPORT_REACH_RANGE = (None, 0.75)
SUPPORT_ELBOW_RANGE = (65.0, 105.0)
POWER_POCKET_REACH_RANGE = (0.70, 0.82)
POWER_POCKET_ELBOW_RANGE = (85.0, 105.0)
RELEASE_REACH_RANGE = (0.95, 0.99)
RELEASE_ELBOW_RANGE = (145.0, 165.0)
FOLLOWTHROUGH_REACH_RANGE = (0.95, 1.0)
FOLLOWTHROUGH_ELBOW_RANGE = (155.0, 175.0)
RECOVERY_REACH_RANGE = (None, 0.75)
RECOVERY_ELBOW_RANGE = (75.0, 110.0)
POWER_POCKET_MAX_ABS_LATERAL_CM = 46.0
POWER_POCKET_FORWARD_RANGE_CM = (-45.0, -20.0)
POWER_POCKET_VERTICAL_RANGE_CM = (5.0, 25.0)
RECOVERY_MAX_ABS_LATERAL_CM = 35.0
RECOVERY_MAX_VERTICAL_CM = -15.0
DENSE_DRIVE_FRAME_RANGE = (0, 144)
DENSE_DRIVE_POSE_COUNT = 145
DENSE_THROWING_RESHAPE_FRAME_RANGE = (66, 94)
PINNED_RELEASE_SHOULDER_CIRCLE_SAMPLE = 115

REFERENCE_DISC_GRIP_TRANSLATION_CM = (-3.5, -6.0, -2.0)
REFERENCE_DISC_GRIP_ROTATION_XYZW = v6_generator.REFERENCE_DISC_GRIP_ROTATION
RELEASE_GRIP_MAX_TRANSLATION_ERROR_CM = 0.1
RELEASE_GRIP_MAX_ROTATION_ERROR_DEGREES = 0.1
MAX_RELEASE_SHOULDER_DELTA_FROM_V006_CM = 11.5
MAX_RELEASE_CLAVICLE_RELATIVE_DELTA_FROM_V006_DEGREES = 42.0
MAX_RELEASE_FIXED_HAND_WRIST_CORRECTION_DEGREES = 20.0

# At 60 Hz, the translation caps are 7.35 m/s for the hand and 8.55 m/s
# for the grip.  They are tight against the geometric lower bounds imposed by
# a compact f81 pocket plus immutable v006 f84 grip (reported below), while the
# acceleration caps still permit the intentionally explosive release beat.
MAX_HAND_TRANSLATION_CM_PER_FRAME = 12.25
MAX_GRIP_TRANSLATION_CM_PER_FRAME = 14.25
MAX_HAND_ACCELERATION_CM_PER_FRAME_SQUARED = 10.0
MAX_GRIP_ACCELERATION_CM_PER_FRAME_SQUARED = 10.25
MAX_TORSO_LOCAL_SHOULDER_TRANSLATION_CM_PER_FRAME = 2.25
MAX_TORSO_LOCAL_SHOULDER_ACCELERATION_CM_PER_FRAME_SQUARED = 3.0
MAX_TORSO_LOCAL_ELBOW_TRANSLATION_CM_PER_FRAME = 10.0
MAX_TORSO_LOCAL_ELBOW_ACCELERATION_CM_PER_FRAME_SQUARED = 8.0
# These are 60 Hz procedural spike/style guards.  In particular the lower-arm
# 24/18 allowance accommodates the sampled release while remaining below the
# reported full-effort elbow-extension rate band; it is not an anatomy limit.
# High-rate capture, filtering, deformation, and human review remain required.
MAX_CLAVICLE_RELATIVE_LOG_RATE_DEGREES_PER_FRAME = 8.0
MAX_CLAVICLE_RELATIVE_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED = 6.0
MAX_UPPERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME = 18.0
MAX_UPPERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED = 15.0
MAX_LOWERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME = 24.0
MAX_LOWERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED = 18.0
MAX_ELBOW_DELTA_DEGREES_PER_FRAME = 45.0
MAX_BEND_PLANE_DELTA_DEGREES_PER_FRAME = 60.0
MAX_PRE_RELEASE_DISC_PLANE_DELTA_DEGREES_PER_FRAME = 15.0
MAX_WRIST_ADDITIVE_DEGREES = 25.0
# Native authoring keeps the default 90-degree per-Euler-component parser
# bound everywhere except this deliberately tucked v007 support-forearm Roll.
MAX_V7_DRIVE_SUPPORT_LOWERARM_ROLL_DEGREES = 92.0
QUATERNION_LOG_VECTOR_CONVENTION = (
    "SPATIAL_DELTA_CURRENT_TIMES_INVERSE_PREVIOUS_"
    "SHORTEST_W_NONNEGATIVE_LOG_VECTOR_DEGREES;_"
    "ACCELERATION_IS_NORM_OF_CONSECUTIVE_LOG_VECTOR_DIFFERENCE"
)

RUNTIME_BLOCKERS = [
    "v007 Unreal sequence/montage assets have not been authored or validated",
    "v007 runtime DGMaster source-pose capture has not been validated",
    "v007 MetaHuman target deformation/contact/temporal capture has not been validated",
    "v007 human animation and disc-contact approval remain false",
]


def _load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if type(value) is not dict:
        raise ValueError(f"{path} does not contain a JSON object")
    return value


def _encoded(value: dict[str, Any]) -> bytes:
    return (json.dumps(value, indent=2) + "\n").encode("utf-8")


def _sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


def _family(recipe: dict[str, Any], name: str) -> dict[str, Any]:
    matches = [value for value in recipe.get("families", []) if value.get("family") == name]
    if len(matches) != 1:
        raise ValueError(f"recipe has {len(matches)} {name!r} families")
    return matches[0]


def _poses(family: dict[str, Any]) -> dict[int, dict[str, Any]]:
    result = {pose["frame"]: pose for pose in family.get("pose_keys", [])}
    if len(result) != len(family.get("pose_keys", [])):
        raise ValueError(f"{family.get('family')} has duplicate pose frames")
    return result


def _restore_v6_identity(
    candidate: dict[str, Any], source: dict[str, Any],
) -> dict[str, Any]:
    normalized = copy.deepcopy(candidate)
    normalized.pop("v7_source_motion_design_contract", None)
    for key in ("schema", "schema_version", "recipe_id", "recipe_version", "asset_revision"):
        normalized[key] = source[key]
    source_families = {family["family"]: family for family in source["families"]}
    for family in normalized["families"]:
        source_family = source_families[family["family"]]
        for key in ("motion_id", "sequence_object_path", "montage_object_path", "style_id"):
            family[key] = source_family[key]
        if family["family"] == "Drive":
            # Dense v007 keys are validated independently below.  Restore the
            # complete sparse source payload here so the top-level identity
            # comparison is meaningful despite the append-only bake.
            family["pose_keys"] = copy.deepcopy(source_family["pose_keys"])
    return normalized


def _sampled_drive(
    recipe: dict[str, Any], frame: int,
) -> tuple[
    dict[str, tuple[float, float, float]],
    dict[str, tuple[float, float, float, float]],
    tuple[float, float, float, float],
]:
    drive = _family(recipe, "Drive")
    pose_frames = [pose["frame"] for pose in drive["pose_keys"]]
    rotations_by_frame = {
        pose["frame"]: {
            bone: tuple(values)
            for bone, values in pose["rotation_degrees"].items()
        }
        for pose in drive["pose_keys"]
    }
    bones = ("spine_04", *recipe_math.V6_ARM_BONES)
    rotations = {
        bone: recipe_math.sampled_catmull_rotation(
            pose_frames, rotations_by_frame, bone, frame,
        )
        for bone in bones
    }
    return recipe_math.mixed_space_arm_pose(rotations)


def _arm_metric(recipe: dict[str, Any], side: str, frame: int) -> dict[str, Any]:
    positions, _, spine_component = _sampled_drive(recipe, frame)
    shoulder = positions[f"upperarm_{side}"]
    elbow = positions[f"lowerarm_{side}"]
    hand = positions[f"hand_{side}"]
    proximal = recipe_math.vector_subtract(shoulder, elbow)
    distal = recipe_math.vector_subtract(hand, elbow)
    upper_length = recipe_math.magnitude(proximal)
    lower_length = recipe_math.magnitude(distal)
    reach = (
        recipe_math.magnitude(recipe_math.vector_subtract(hand, shoulder))
        / (upper_length + lower_length)
    )
    hand_torso = recipe_math.torso_relative_vector(spine_component, hand)
    return {
        "frame": frame,
        "elbow_degrees": recipe_math.vector_angle_degrees(proximal, distal),
        "reach_ratio": reach,
        "hand_torso_cm": list(hand_torso),
    }


def _drive_rotation_samples(
    recipe: dict[str, Any], bones: tuple[str, ...],
) -> tuple[dict[int, dict[str, tuple[float, float, float]]], int]:
    drive = _family(recipe, "Drive")
    pose_frames = [pose["frame"] for pose in drive["pose_keys"]]
    rotations_by_frame = {
        pose["frame"]: {
            bone: tuple(values)
            for bone, values in pose["rotation_degrees"].items()
        }
        for pose in drive["pose_keys"]
    }
    sampled = {
        frame: {
            bone: recipe_math.sampled_catmull_rotation(
                pose_frames, rotations_by_frame, bone, frame,
            )
            for bone in bones
        }
        for frame in range(drive["frame_count"] + 1)
    }
    return sampled, drive["release_frame"]


def _in_range(value: float, minimum: float | None, maximum: float | None) -> bool:
    return (
        (minimum is None or value >= minimum - 1e-6)
        and (maximum is None or value <= maximum + 1e-6)
    )


def _disc_grip_transform(
    recipe: dict[str, Any], frame: int,
) -> tuple[
    tuple[float, float, float], tuple[float, float, float, float],
    tuple[float, float, float], tuple[float, float, float, float],
]:
    positions, components, _ = _sampled_drive(recipe, frame)
    hand_position = positions["hand_r"]
    hand_rotation = components["hand_r"]
    grip_position = recipe_math.vector_add(
        hand_position,
        recipe_math.quaternion_rotate_vector(
            hand_rotation, REFERENCE_DISC_GRIP_TRANSLATION_CM,
        ),
    )
    grip_rotation = recipe_math.quaternion_multiply(
        hand_rotation, REFERENCE_DISC_GRIP_ROTATION_XYZW,
    )
    return hand_position, hand_rotation, grip_position, grip_rotation


def _vector_cross(
    first: tuple[float, float, float], second: tuple[float, float, float],
) -> tuple[float, float, float]:
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def _maximum_translation_step(
    values: list[tuple[float, float, float]],
) -> tuple[float, int]:
    return max(
        (
            recipe_math.magnitude(recipe_math.vector_subtract(values[frame], values[frame - 1])),
            frame,
        )
        for frame in range(1, len(values))
    )


def _maximum_translation_acceleration(
    values: list[tuple[float, float, float]],
) -> tuple[float, int]:
    return max(
        (
            recipe_math.magnitude(recipe_math.vector_subtract(
                recipe_math.vector_subtract(values[frame], values[frame - 1]),
                recipe_math.vector_subtract(values[frame - 1], values[frame - 2]),
            )),
            frame,
        )
        for frame in range(2, len(values))
    )


def _canonical_shortest_quaternion(
    quaternion: tuple[float, float, float, float],
) -> tuple[float, float, float, float]:
    value = recipe_math.quaternion_normalize(quaternion)
    negate = value[3] < 0.0
    if abs(value[3]) <= 1e-12:
        for component in value[:3]:
            if abs(component) > 1e-12:
                negate = component < 0.0
                break
    return tuple(-component for component in value) if negate else value


def _quaternion_log_vector_degrees(
    quaternion: tuple[float, float, float, float],
) -> tuple[float, float, float]:
    x, y, z, w = _canonical_shortest_quaternion(quaternion)
    vector_length = math.sqrt(x * x + y * y + z * z)
    if vector_length <= 1e-12:
        return (0.0, 0.0, 0.0)
    angle = math.degrees(2.0 * math.atan2(vector_length, max(0.0, w)))
    return (
        x * angle / vector_length,
        y * angle / vector_length,
        z * angle / vector_length,
    )


def _maximum_log_vector_rate_and_acceleration(
    values: list[tuple[float, float, float, float]],
) -> tuple[float, int, float, int]:
    increments = [
        _quaternion_log_vector_degrees(recipe_math.quaternion_multiply(
            values[frame], recipe_math.quaternion_inverse(values[frame - 1]),
        ))
        for frame in range(1, len(values))
    ]
    maximum_rate, rate_frame = max(
        (recipe_math.magnitude(value), frame)
        for frame, value in enumerate(increments, start=1)
    )
    maximum_acceleration, acceleration_frame = max(
        (
            recipe_math.magnitude(recipe_math.vector_subtract(
                increments[index], increments[index - 1],
            )),
            index + 1,
        )
        for index in range(1, len(increments))
    )
    return maximum_rate, rate_frame, maximum_acceleration, acceleration_frame


def _expected_contract_sections() -> dict[str, dict[str, Any]]:
    return {
        "dense_drive_sampling_policy": {
            "frames": list(DENSE_DRIVE_FRAME_RANGE),
            "frame_rate": 60,
            "required_pose_count": DENSE_DRIVE_POSE_COUNT,
            "non_arm_source_sampling": (
                "V006_CATMULL_ROM_INTEGER_FRAME_SAMPLES_ROUNDED_6_DECIMALS"
            ),
            "throwing_component_reshape_frames": list(
                DENSE_THROWING_RESHAPE_FRAME_RANGE
            ),
            "release_shoulder_circle_sample": (
                PINNED_RELEASE_SHOULDER_CIRCLE_SAMPLE
            ),
        },
        "support_arm_named_phase_gate": {
            "frames": list(SUPPORT_GATE_FRAMES),
            "maximum_reach_ratio": SUPPORT_REACH_RANGE[1],
            "minimum_elbow_degrees": SUPPORT_ELBOW_RANGE[0],
            "maximum_elbow_degrees": SUPPORT_ELBOW_RANGE[1],
        },
        "throwing_arm_power_pocket_gate": {
            "frames": list(POWER_POCKET_GATE_FRAMES),
            "minimum_reach_ratio": POWER_POCKET_REACH_RANGE[0],
            "maximum_reach_ratio": POWER_POCKET_REACH_RANGE[1],
            "minimum_elbow_degrees": POWER_POCKET_ELBOW_RANGE[0],
            "maximum_elbow_degrees": POWER_POCKET_ELBOW_RANGE[1],
            "maximum_absolute_torso_lateral_cm": POWER_POCKET_MAX_ABS_LATERAL_CM,
            "minimum_torso_forward_cm": POWER_POCKET_FORWARD_RANGE_CM[0],
            "maximum_torso_forward_cm": POWER_POCKET_FORWARD_RANGE_CM[1],
            "minimum_torso_vertical_cm": POWER_POCKET_VERTICAL_RANGE_CM[0],
            "maximum_torso_vertical_cm": POWER_POCKET_VERTICAL_RANGE_CM[1],
        },
        "throwing_arm_release_gate": {
            "frame": RELEASE_GATE_FRAMES[0],
            "minimum_reach_ratio": RELEASE_REACH_RANGE[0],
            "maximum_reach_ratio": RELEASE_REACH_RANGE[1],
            "minimum_elbow_degrees": RELEASE_ELBOW_RANGE[0],
            "maximum_elbow_degrees": RELEASE_ELBOW_RANGE[1],
        },
        "throwing_arm_followthrough_gate": {
            "frame": FOLLOWTHROUGH_GATE_FRAMES[0],
            "minimum_reach_ratio": FOLLOWTHROUGH_REACH_RANGE[0],
            "maximum_reach_ratio": FOLLOWTHROUGH_REACH_RANGE[1],
            "minimum_elbow_degrees": FOLLOWTHROUGH_ELBOW_RANGE[0],
            "maximum_elbow_degrees": FOLLOWTHROUGH_ELBOW_RANGE[1],
        },
        "throwing_arm_recovery_gate": {
            "frame": RECOVERY_GATE_FRAMES[0],
            "maximum_reach_ratio": RECOVERY_REACH_RANGE[1],
            "minimum_elbow_degrees": RECOVERY_ELBOW_RANGE[0],
            "maximum_elbow_degrees": RECOVERY_ELBOW_RANGE[1],
            "maximum_absolute_torso_lateral_cm": RECOVERY_MAX_ABS_LATERAL_CM,
            "maximum_torso_vertical_cm": RECOVERY_MAX_VERTICAL_CM,
        },
        "release_component_grip_invariant": {
            "frame": RELEASE_GATE_FRAMES[0],
            "maximum_translation_error_cm": RELEASE_GRIP_MAX_TRANSLATION_ERROR_CM,
            "maximum_rotation_error_degrees": RELEASE_GRIP_MAX_ROTATION_ERROR_DEGREES,
            "reference_local_translation_cm": list(REFERENCE_DISC_GRIP_TRANSLATION_CM),
            "reference_local_rotation_xyzw": list(REFERENCE_DISC_GRIP_ROTATION_XYZW),
        },
        "release_proximal_stability_gate": {
            "frame": RELEASE_GATE_FRAMES[0],
            "maximum_shoulder_origin_delta_from_v006_cm": (
                MAX_RELEASE_SHOULDER_DELTA_FROM_V006_CM
            ),
            "maximum_clavicle_relative_to_spine_delta_from_v006_degrees": (
                MAX_RELEASE_CLAVICLE_RELATIVE_DELTA_FROM_V006_DEGREES
            ),
            "maximum_fixed_hand_wrist_correction_degrees": (
                MAX_RELEASE_FIXED_HAND_WRIST_CORRECTION_DEGREES
            ),
        },
        "source_continuity_gate": {
            "frame_rate": 60,
            "gate_interpretation": (
                "PROCEDURAL_60HZ_ANTI_POP_REGRESSION_NOT_HUMAN_ANATOMICAL_MAXIMUM"
            ),
            "lowerarm_exception_basis": (
                "24_DEGREES_PER_FRAME_AND_18_DEGREES_PER_FRAME_SQUARED_"
                "ARE_SAMPLING_STYLE_CAPS_NOT_ANATOMY_MAXIMA"
            ),
            "high_rate_capture_validation_required": True,
            "maximum_hand_translation_cm_per_frame": MAX_HAND_TRANSLATION_CM_PER_FRAME,
            "maximum_grip_translation_cm_per_frame": MAX_GRIP_TRANSLATION_CM_PER_FRAME,
            "maximum_hand_translation_acceleration_cm_per_frame_squared": (
                MAX_HAND_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
            "maximum_grip_translation_acceleration_cm_per_frame_squared": (
                MAX_GRIP_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
            "maximum_torso_local_shoulder_translation_cm_per_frame": (
                MAX_TORSO_LOCAL_SHOULDER_TRANSLATION_CM_PER_FRAME
            ),
            "maximum_torso_local_shoulder_acceleration_cm_per_frame_squared": (
                MAX_TORSO_LOCAL_SHOULDER_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
            "maximum_torso_local_elbow_translation_cm_per_frame": (
                MAX_TORSO_LOCAL_ELBOW_TRANSLATION_CM_PER_FRAME
            ),
            "maximum_torso_local_elbow_acceleration_cm_per_frame_squared": (
                MAX_TORSO_LOCAL_ELBOW_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
            "maximum_clavicle_relative_to_spine_log_vector_rate_degrees_per_frame": (
                MAX_CLAVICLE_RELATIVE_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_clavicle_relative_to_spine_log_vector_acceleration_degrees_per_frame_squared": (
                MAX_CLAVICLE_RELATIVE_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
            "maximum_throwing_upperarm_component_log_vector_rate_degrees_per_frame": (
                MAX_UPPERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_throwing_upperarm_component_log_vector_acceleration_degrees_per_frame_squared": (
                MAX_UPPERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
            "maximum_throwing_lowerarm_component_log_vector_rate_degrees_per_frame": (
                MAX_LOWERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_throwing_lowerarm_component_log_vector_acceleration_degrees_per_frame_squared": (
                MAX_LOWERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
            "maximum_elbow_delta_degrees_per_frame": MAX_ELBOW_DELTA_DEGREES_PER_FRAME,
            "maximum_elbow_bend_plane_delta_degrees_per_frame": (
                MAX_BEND_PLANE_DELTA_DEGREES_PER_FRAME
            ),
            "maximum_pre_release_disc_plane_delta_degrees_per_frame": (
                MAX_PRE_RELEASE_DISC_PLANE_DELTA_DEGREES_PER_FRAME
            ),
            "maximum_throwing_wrist_additive_degrees": MAX_WRIST_ADDITIVE_DEGREES,
            "quaternion_log_vector_convention": QUATERNION_LOG_VECTOR_CONVENTION,
        },
    }


def validate_source_candidate(
    source: dict[str, Any], candidate: dict[str, Any],
) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    observations: dict[str, Any] = {}

    expected_v6, _ = v6_generator.build_v6()
    expected_v6_bytes = _encoded(expected_v6)
    actual_v6_bytes = V6_PATH.read_bytes() if V6_PATH.is_file() else b""
    observations["v006"] = {
        "path": str(V6_PATH),
        "sha256": _sha256_bytes(actual_v6_bytes),
        "bytes": len(actual_v6_bytes),
        "canonical_sha256": CANONICAL_V006_SHA256,
        "canonical_bytes": CANONICAL_V006_BYTES,
        "matches_pinned_canonical_identity": (
            len(actual_v6_bytes) == CANONICAL_V006_BYTES
            and _sha256_bytes(actual_v6_bytes) == CANONICAL_V006_SHA256
        ),
        "matches_deterministic_generator": actual_v6_bytes == expected_v6_bytes,
    }
    if (
        len(actual_v6_bytes) != CANONICAL_V006_BYTES
        or _sha256_bytes(actual_v6_bytes) != CANONICAL_V006_SHA256
    ):
        issues.append("checked-in v006 differs from pinned canonical bytes/SHA-256")
    if actual_v6_bytes != expected_v6_bytes:
        issues.append("checked-in v006 differs from its deterministic generator")
    if source != expected_v6:
        issues.append("loaded v006 JSON differs from deterministic v006 value")

    expected_v7, generation_summary = v7_generator.build_v7()
    expected_v7_bytes = _encoded(expected_v7)
    actual_v7_bytes = V7_PATH.read_bytes() if V7_PATH.is_file() else b""
    observations["v007"] = {
        "path": str(V7_PATH),
        "sha256": _sha256_bytes(actual_v7_bytes),
        "bytes": len(actual_v7_bytes),
        "matches_deterministic_generator": actual_v7_bytes == expected_v7_bytes,
        "generation_summary": generation_summary,
    }
    if actual_v7_bytes != expected_v7_bytes:
        issues.append("checked-in v007 differs from its deterministic generator")
    if candidate != expected_v7:
        issues.append("loaded v007 JSON differs from deterministic v007 value")

    expected_identity = {
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v7",
        "schema_version": 7,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V7",
        "recipe_version": "v7",
        "asset_revision": "v007",
    }
    for key, expected in expected_identity.items():
        if candidate.get(key) != expected:
            issues.append(f"v007 {key} differs from append-only identity")

    normalized = _restore_v6_identity(candidate, source)
    identity_preserved = normalized == source
    observations["v006_identity_preservation"] = {
        "passes_after_restoring_version_and_drive_arm_rotation_scope": (
            identity_preserved
        ),
        "allowed_mutation_scope": (
            "Drive bilateral clavicle/upperarm/lowerarm/hand rotations"
        ),
    }
    if not identity_preserved:
        issues.append("v007 differs from v006 outside version identity and Drive arm rotations")

    source_drive = _family(source, "Drive")
    expected_dense_poses = _poses({
        "family": "Drive",
        "pose_keys": v7_generator._densify_pose_keys(source_drive),
    })
    candidate_drive = _family(candidate, "Drive")
    candidate_pose_keys = candidate_drive.get("pose_keys", [])
    candidate_drive_poses = _poses(candidate_drive)
    exact_dense_frames = tuple(range(
        DENSE_DRIVE_FRAME_RANGE[0], DENSE_DRIVE_FRAME_RANGE[1] + 1,
    ))
    observed_dense_frames = tuple(
        pose.get("frame") for pose in candidate_pose_keys
    )
    dense_frame_set_exact = (
        len(candidate_pose_keys) == DENSE_DRIVE_POSE_COUNT
        and observed_dense_frames == exact_dense_frames
        and tuple(sorted(candidate_drive_poses)) == exact_dense_frames
    )
    if not dense_frame_set_exact:
        issues.append("v007 Drive dense frame set/order differs from exact f0-f144")

    non_arm_identity_mismatches: list[str] = []
    changed_channels: set[tuple[int, str]] = set()
    for frame in exact_dense_frames:
        source_pose = expected_dense_poses.get(frame)
        candidate_pose = candidate_drive_poses.get(frame)
        if source_pose is None or candidate_pose is None:
            continue
        if (
            candidate_pose.get("throwing_hand_grip_alpha")
            != source_pose["throwing_hand_grip_alpha"]
        ):
            non_arm_identity_mismatches.append(f"f{frame}:throwing_hand_grip_alpha")
        if candidate_pose.get("translation_cm") != source_pose["translation_cm"]:
            non_arm_identity_mismatches.append(f"f{frame}:translation_cm")
        candidate_rotations = candidate_pose.get("rotation_degrees", {})
        source_rotations = source_pose["rotation_degrees"]
        if set(candidate_rotations) != set(source_rotations):
            non_arm_identity_mismatches.append(f"f{frame}:rotation_channel_set")
        for bone in source_rotations:
            if bone in v7_generator.ARM_BONES:
                if candidate_rotations.get(bone) != source_rotations[bone]:
                    changed_channels.add((frame, bone))
            elif candidate_rotations.get(bone) != source_rotations[bone]:
                non_arm_identity_mismatches.append(f"f{frame}:{bone}")

    expected_v7_drive_poses = _poses(_family(expected_v7, "Drive"))
    expected_changed_channels = {
        (frame, bone)
        for frame in exact_dense_frames
        for bone in v7_generator.ARM_BONES
        if (
            expected_v7_drive_poses[frame]["rotation_degrees"][bone]
            != expected_dense_poses[frame]["rotation_degrees"][bone]
        )
    }
    observations["dense_drive_identity"] = {
        "required_frame_range": list(DENSE_DRIVE_FRAME_RANGE),
        "required_pose_count": DENSE_DRIVE_POSE_COUNT,
        "observed_pose_count": len(candidate_pose_keys),
        "frame_set_and_order_exact": dense_frame_set_exact,
        "non_arm_channels_equal_v006_integer_frame_catmull_samples": (
            not non_arm_identity_mismatches
        ),
        "non_arm_mismatch_count": len(non_arm_identity_mismatches),
        "approach_and_putt_payload_exact_after_version_identity": (
            identity_preserved
        ),
    }
    if non_arm_identity_mismatches:
        issues.append("v007 dense Drive differs from v006 integer samples outside arm rotations")
    observations["arm_mutation_scope"] = {
        "changed_channel_count": len(changed_channels),
        "expected_changed_channel_count": len(expected_changed_channels),
        "exact": changed_channels == expected_changed_channels,
    }
    if changed_channels != expected_changed_channels:
        issues.append("v007 Drive arm channel mutation set differs from exact scaffold")

    maximum_support_lowerarm_roll = 0.0
    maximum_support_lowerarm_roll_frame: int | None = None
    maximum_support_lowerarm_roll_signed = 0.0
    for frame in exact_dense_frames:
        pose = candidate_drive_poses.get(frame)
        rotations = pose.get("rotation_degrees", {}) if type(pose) is dict else {}
        lowerarm = rotations.get("lowerarm_l") if type(rotations) is dict else None
        if (
            type(lowerarm) is not list
            or len(lowerarm) != 3
            or type(lowerarm[2]) not in (int, float)
            or not math.isfinite(float(lowerarm[2]))
        ):
            issues.append(f"v007 Drive lowerarm_l Roll is malformed at f{frame}")
            continue
        signed_roll = float(lowerarm[2])
        absolute_roll = abs(signed_roll)
        if absolute_roll > maximum_support_lowerarm_roll:
            maximum_support_lowerarm_roll = absolute_roll
            maximum_support_lowerarm_roll_frame = frame
            maximum_support_lowerarm_roll_signed = signed_roll
        if absolute_roll > MAX_V7_DRIVE_SUPPORT_LOWERARM_ROLL_DEGREES:
            issues.append(
                "v007 Drive lowerarm_l Roll exceeds the 92-degree native "
                f"parser bound at f{frame}: {signed_roll:.6f}"
            )
    observations["native_parser_support_lowerarm_roll"] = {
        "bone": "lowerarm_l",
        "component": "Roll",
        "maximum_absolute_degrees": maximum_support_lowerarm_roll,
        "signed_degrees": maximum_support_lowerarm_roll_signed,
        "frame": maximum_support_lowerarm_roll_frame,
        "maximum_absolute_limit_degrees": (
            MAX_V7_DRIVE_SUPPORT_LOWERARM_ROLL_DEGREES
        ),
        "passed": (
            maximum_support_lowerarm_roll
            <= MAX_V7_DRIVE_SUPPORT_LOWERARM_ROLL_DEGREES
        ),
    }

    contract = candidate.get("v7_source_motion_design_contract")
    if type(contract) is not dict:
        issues.append("v007 source-motion design contract is absent or malformed")
        contract = {}
    false_claims = (
        "static_source_geometry_approval", "unreal_assets_authored",
        "runtime_source_pose_approval", "metahuman_deformation_approval",
        "human_animation_approval", "human_disc_contact_approval",
        "release_approval",
    )
    for key in false_claims:
        if contract.get(key) is not False:
            issues.append(f"v007 fail-closed claim {key} must remain false")

    expected_contract_sections = _expected_contract_sections()
    for key, expected in expected_contract_sections.items():
        if contract.get(key) != expected:
            issues.append(f"v007 contract section {key} differs from pinned validator authority")

    drive_frame_count = _family(candidate, "Drive")["frame_count"]
    observed_contract_frames = {
        "support": tuple(contract.get("support_arm_named_phase_gate", {}).get("frames", [])),
        "power_pocket": tuple(contract.get("throwing_arm_power_pocket_gate", {}).get("frames", [])),
        "release": (contract.get("throwing_arm_release_gate", {}).get("frame"),),
        "followthrough": (
            contract.get("throwing_arm_followthrough_gate", {}).get("frame"),
        ),
        "recovery": (contract.get("throwing_arm_recovery_gate", {}).get("frame"),),
    }
    for label, frames in PINNED_FRAME_SETS.items():
        if not frames:
            issues.append(f"pinned {label} frame set is empty")
        if observed_contract_frames.get(label) != frames:
            issues.append(f"candidate {label} frame set differs from exact pinned frames")
        if any(type(frame) is not int or not 0 <= frame <= drive_frame_count for frame in frames):
            issues.append(f"pinned {label} frame set is outside the Drive range")
    observations["pinned_frame_sets"] = {
        label: list(frames) for label, frames in PINNED_FRAME_SETS.items()
    }

    support_metrics = [
        _arm_metric(candidate, "l", frame) for frame in SUPPORT_GATE_FRAMES
    ]
    for metric in support_metrics:
        if not _in_range(
            metric["reach_ratio"], *SUPPORT_REACH_RANGE,
        ):
            issues.append(f"support-arm reach gate failed at f{metric['frame']}")
        if not _in_range(
            metric["elbow_degrees"], *SUPPORT_ELBOW_RANGE,
        ):
            issues.append(f"support-arm elbow gate failed at f{metric['frame']}")
    observations["support_arm_named_phases"] = support_metrics

    throwing_specs = [
        (
            "power_pocket", POWER_POCKET_GATE_FRAMES,
            POWER_POCKET_REACH_RANGE, POWER_POCKET_ELBOW_RANGE,
        ),
        ("release", RELEASE_GATE_FRAMES, RELEASE_REACH_RANGE, RELEASE_ELBOW_RANGE),
        (
            "followthrough", FOLLOWTHROUGH_GATE_FRAMES,
            FOLLOWTHROUGH_REACH_RANGE, FOLLOWTHROUGH_ELBOW_RANGE,
        ),
        (
            "recovery", RECOVERY_GATE_FRAMES,
            RECOVERY_REACH_RANGE, RECOVERY_ELBOW_RANGE,
        ),
    ]
    throwing_observations: dict[str, Any] = {}
    for label, frames, reach_range, elbow_range in throwing_specs:
        metrics = [
            _arm_metric(candidate, "r", frame) for frame in frames
        ]
        throwing_observations[label] = metrics
        for metric in metrics:
            if not _in_range(metric["reach_ratio"], *reach_range):
                issues.append(f"throwing-arm {label} reach gate failed at f{metric['frame']}")
            if not _in_range(metric["elbow_degrees"], *elbow_range):
                issues.append(f"throwing-arm {label} elbow gate failed at f{metric['frame']}")
            lateral, forward, vertical = metric["hand_torso_cm"]
            if label == "power_pocket":
                if abs(lateral) > POWER_POCKET_MAX_ABS_LATERAL_CM + 1e-6:
                    issues.append(
                        f"throwing-arm power-pocket hand is too lateral at f{metric['frame']}"
                    )
                if not _in_range(forward, *POWER_POCKET_FORWARD_RANGE_CM):
                    issues.append(
                        f"throwing-arm power-pocket forward position failed at f{metric['frame']}"
                    )
                if not _in_range(vertical, *POWER_POCKET_VERTICAL_RANGE_CM):
                    issues.append(
                        f"throwing-arm power-pocket vertical position failed at f{metric['frame']}"
                    )
            if label == "recovery":
                if abs(lateral) > RECOVERY_MAX_ABS_LATERAL_CM + 1e-6:
                    issues.append("throwing-arm recovery hand remains too far lateral")
                if vertical > RECOVERY_MAX_VERTICAL_CM + 1e-6:
                    issues.append("throwing-arm recovery hand is not below the sternum")
    observations["throwing_arm"] = throwing_observations

    # Preserve the release grip payload exactly while allowing the upstream arm
    # joint positions to change.  Disc plane normals are unoriented.
    source_drive = _family(source, "Drive")
    candidate_drive = _family(candidate, "Drive")
    source_release = _poses(source_drive)[source_drive["release_frame"]]
    candidate_release = _poses(candidate_drive)[candidate_drive["release_frame"]]
    source_release_rotations = {
        bone: tuple(values)
        for bone, values in source_release["rotation_degrees"].items()
    }
    candidate_release_rotations = {
        bone: tuple(values)
        for bone, values in candidate_release["rotation_degrees"].items()
    }
    disc_chain = ("spine_04", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")
    source_normal = recipe_math.v5_disc_plane_normal({
        bone: source_release_rotations[bone] for bone in disc_chain
    })
    candidate_normal = recipe_math.v5_disc_plane_normal({
        bone: candidate_release_rotations[bone] for bone in disc_chain
    })
    disc_normal_error = recipe_math.disc_plane_delta_degrees(
        source_normal, candidate_normal,
    )
    (
        source_hand_position, source_hand_rotation,
        source_grip_position, source_grip_rotation,
    ) = _disc_grip_transform(source, RELEASE_GATE_FRAMES[0])
    (
        candidate_hand_position, candidate_hand_rotation,
        candidate_grip_position, candidate_grip_rotation,
    ) = _disc_grip_transform(candidate, RELEASE_GATE_FRAMES[0])
    hand_translation_error = recipe_math.magnitude(
        recipe_math.vector_subtract(candidate_hand_position, source_hand_position)
    )
    hand_rotation_error = recipe_math.quaternion_angular_distance_degrees(
        candidate_hand_rotation, source_hand_rotation,
    )
    grip_translation_error = recipe_math.magnitude(
        recipe_math.vector_subtract(candidate_grip_position, source_grip_position)
    )
    grip_rotation_error = recipe_math.quaternion_angular_distance_degrees(
        candidate_grip_rotation, source_grip_rotation,
    )
    source_release_positions, source_release_components, source_release_spine = _sampled_drive(
        source, RELEASE_GATE_FRAMES[0]
    )
    (
        candidate_release_positions,
        candidate_release_components,
        candidate_release_spine,
    ) = _sampled_drive(
        candidate, RELEASE_GATE_FRAMES[0]
    )
    release_shoulder_origin_delta = recipe_math.magnitude(
        recipe_math.vector_subtract(
            candidate_release_positions["upperarm_r"],
            source_release_positions["upperarm_r"],
        )
    )
    source_release_clavicle_relative = recipe_math.quaternion_multiply(
        recipe_math.quaternion_inverse(source_release_spine),
        source_release_components["clavicle_r"],
    )
    candidate_release_clavicle_relative = recipe_math.quaternion_multiply(
        recipe_math.quaternion_inverse(candidate_release_spine),
        candidate_release_components["clavicle_r"],
    )
    release_clavicle_relative_delta = (
        recipe_math.quaternion_angular_distance_degrees(
            source_release_clavicle_relative,
            candidate_release_clavicle_relative,
        )
    )
    ideal_release_lower_component = recipe_math.quaternion_multiply(
        candidate_hand_rotation,
        recipe_math.quaternion_inverse(
            v6_generator.REFERENCE_LOCAL_ROTATIONS["hand_r"]
        ),
    )
    ideal_release_distal = recipe_math.quaternion_rotate_vector(
        ideal_release_lower_component,
        v7_generator.REFERENCE_LOCAL_TRANSLATIONS_CM["hand_r"],
    )
    actual_release_distal = recipe_math.vector_subtract(
        candidate_hand_position, candidate_release_positions["lowerarm_r"]
    )
    fixed_hand_wrist_correction = recipe_math.vector_angle_degrees(
        ideal_release_distal, actual_release_distal,
    )
    finger_bones = sorted({
        bone for bone in source_release["rotation_degrees"]
        if bone.startswith(FINGER_PREFIXES)
    })
    candidate_finger_bones = sorted({
        bone for bone in candidate_release["rotation_degrees"]
        if bone.startswith(FINGER_PREFIXES)
    })
    grip_evidence = {
        "frame": source_drive["release_frame"],
        "family_release_frame_exact": (
            source_drive["release_frame"] == candidate_drive["release_frame"] == 84
        ),
        "event_release_frame_exact": (
            source_drive["biomechanical_events"]["release_frame"]
            == candidate_drive["biomechanical_events"]["release_frame"]
            == 84
        ),
        "throwing_hand_grip_alpha_exact": (
            source_release["throwing_hand_grip_alpha"]
            == candidate_release["throwing_hand_grip_alpha"]
        ),
        "throwing_hand_grip_alpha": candidate_release["throwing_hand_grip_alpha"],
        "explicit_finger_rotation_payload_exact": (
            finger_bones == candidate_finger_bones
            and all(
                source_release["rotation_degrees"][bone]
                == candidate_release["rotation_degrees"][bone]
                for bone in finger_bones
            )
        ),
        "explicit_finger_rotation_channel_count_v006": len(finger_bones),
        "explicit_finger_rotation_channel_count_v007": len(candidate_finger_bones),
        "finger_generation_inputs_exact": (
            source_release["throwing_hand_grip_alpha"]
            == candidate_release["throwing_hand_grip_alpha"]
            and source_drive["curves"]["DG_FingerReleaseAlpha"]
            == candidate_drive["curves"]["DG_FingerReleaseAlpha"]
        ),
        "translation_payload_exact": (
            source_release["translation_cm"] == candidate_release["translation_cm"]
        ),
        "finger_release_curve_exact": (
            source_drive["curves"]["DG_FingerReleaseAlpha"]
            == candidate_drive["curves"]["DG_FingerReleaseAlpha"]
        ),
        "wrist_lag_curve_exact": (
            source_drive["curves"]["DG_WristLagAlpha"]
            == candidate_drive["curves"]["DG_WristLagAlpha"]
        ),
        "disc_plane_normal_error_degrees": disc_normal_error,
        "disc_plane_normal_exact_within_degrees": 0.0001,
        "reference_disc_grip_r_local_translation_cm": list(
            REFERENCE_DISC_GRIP_TRANSLATION_CM
        ),
        "reference_disc_grip_r_local_rotation_xyzw": list(
            REFERENCE_DISC_GRIP_ROTATION_XYZW
        ),
        "source_hand_r_component_translation_cm": list(source_hand_position),
        "candidate_hand_r_component_translation_cm": list(candidate_hand_position),
        "hand_origin_delta_cm": hand_translation_error,
        "hand_orientation_delta_degrees": hand_rotation_error,
        "hand_r_component_translation_error_cm": hand_translation_error,
        "hand_r_component_rotation_error_degrees": hand_rotation_error,
        "source_disc_grip_r_component_translation_cm": list(source_grip_position),
        "candidate_disc_grip_r_component_translation_cm": list(candidate_grip_position),
        "disc_grip_origin_delta_cm": grip_translation_error,
        "disc_grip_orientation_delta_degrees": grip_rotation_error,
        "disc_grip_r_component_translation_error_cm": grip_translation_error,
        "disc_grip_r_component_rotation_error_degrees": grip_rotation_error,
        "release_shoulder_origin_delta_from_v006_cm": (
            release_shoulder_origin_delta
        ),
        "release_clavicle_relative_to_spine_delta_from_v006_degrees": (
            release_clavicle_relative_delta
        ),
        "fixed_hand_wrist_correction_degrees": fixed_hand_wrist_correction,
        "maximum_release_shoulder_origin_delta_from_v006_cm": (
            MAX_RELEASE_SHOULDER_DELTA_FROM_V006_CM
        ),
        "maximum_release_clavicle_relative_to_spine_delta_from_v006_degrees": (
            MAX_RELEASE_CLAVICLE_RELATIVE_DELTA_FROM_V006_DEGREES
        ),
        "maximum_fixed_hand_wrist_correction_degrees": (
            MAX_RELEASE_FIXED_HAND_WRIST_CORRECTION_DEGREES
        ),
        "maximum_component_translation_error_cm": (
            RELEASE_GRIP_MAX_TRANSLATION_ERROR_CM
        ),
        "maximum_component_rotation_error_degrees": (
            RELEASE_GRIP_MAX_ROTATION_ERROR_DEGREES
        ),
    }
    observations["frame_84_grip_preservation"] = grip_evidence
    if not all(
        grip_evidence[key]
        for key in (
            "family_release_frame_exact", "event_release_frame_exact",
            "throwing_hand_grip_alpha_exact",
            "explicit_finger_rotation_payload_exact",
            "finger_generation_inputs_exact",
            "translation_payload_exact", "finger_release_curve_exact",
            "wrist_lag_curve_exact",
        )
    ):
        issues.append("frame-84 release/grip payload differs from v006")
    if disc_normal_error > 0.0001:
        issues.append("frame-84 disc-plane normal differs from v006")
    if (
        hand_translation_error > RELEASE_GRIP_MAX_TRANSLATION_ERROR_CM + 1e-6
        or hand_rotation_error > RELEASE_GRIP_MAX_ROTATION_ERROR_DEGREES + 1e-6
    ):
        issues.append("frame-84 component-space hand_r transform differs from v006")
    if (
        grip_translation_error > RELEASE_GRIP_MAX_TRANSLATION_ERROR_CM + 1e-6
        or grip_rotation_error > RELEASE_GRIP_MAX_ROTATION_ERROR_DEGREES + 1e-6
    ):
        issues.append("frame-84 component-space disc_grip_r transform differs from v006")
    if (
        release_shoulder_origin_delta
        > MAX_RELEASE_SHOULDER_DELTA_FROM_V006_CM + 1e-6
    ):
        issues.append("frame-84 release shoulder delta exceeds the pinned v006 bound")
    if (
        release_clavicle_relative_delta
        > MAX_RELEASE_CLAVICLE_RELATIVE_DELTA_FROM_V006_DEGREES + 1e-6
    ):
        issues.append("frame-84 release clavicle delta exceeds the pinned v006 bound")
    if (
        fixed_hand_wrist_correction
        > MAX_RELEASE_FIXED_HAND_WRIST_CORRECTION_DEGREES + 1e-6
    ):
        issues.append("frame-84 fixed-hand wrist correction exceeds the pinned bound")

    disc_bones = ("spine_04", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")
    sampled, release_frame = _drive_rotation_samples(candidate, disc_bones)
    normals = {
        frame: recipe_math.v5_disc_plane_normal(rotations)
        for frame, rotations in sampled.items()
    }
    maximum_pre_release_plane_delta = max(
        recipe_math.disc_plane_delta_degrees(normals[frame - 1], normals[frame])
        for frame in range(1, release_frame + 1)
    )
    maximum_wrist_additive, maximum_wrist_additive_frame = max(
        (
            recipe_math.quaternion_geodesic_degrees(
                (0.0, 0.0, 0.0), rotations["hand_r"],
            ),
            frame,
        )
        for frame, rotations in sampled.items()
    )
    drive_frame_count = _family(candidate, "Drive")["frame_count"]
    hand_positions: list[tuple[float, float, float]] = []
    grip_positions: list[tuple[float, float, float]] = []
    torso_local_shoulder_positions: list[tuple[float, float, float]] = []
    torso_local_elbow_positions: list[tuple[float, float, float]] = []
    clavicle_relative_rotations: list[tuple[float, float, float, float]] = []
    upperarm_component_rotations: list[tuple[float, float, float, float]] = []
    lowerarm_component_rotations: list[tuple[float, float, float, float]] = []
    elbow_angles: list[float] = []
    bend_planes: list[tuple[float, float, float]] = []
    for frame in range(drive_frame_count + 1):
        positions, components, spine_component = _sampled_drive(candidate, frame)
        shoulder = positions["upperarm_r"]
        elbow = positions["lowerarm_r"]
        hand = positions["hand_r"]
        spine_origin = positions["spine_04"]
        hand_positions.append(hand)
        grip_positions.append(recipe_math.vector_add(
            hand,
            recipe_math.quaternion_rotate_vector(
                components["hand_r"], REFERENCE_DISC_GRIP_TRANSLATION_CM,
            ),
        ))
        torso_local_shoulder_positions.append(
            recipe_math.torso_relative_vector(
                spine_component,
                recipe_math.vector_subtract(shoulder, spine_origin),
            )
        )
        torso_local_elbow_positions.append(
            recipe_math.torso_relative_vector(
                spine_component,
                recipe_math.vector_subtract(elbow, spine_origin),
            )
        )
        clavicle_relative_rotations.append(
            recipe_math.quaternion_multiply(
                recipe_math.quaternion_inverse(spine_component),
                components["clavicle_r"],
            )
        )
        upperarm_component_rotations.append(components["upperarm_r"])
        lowerarm_component_rotations.append(components["lowerarm_r"])
        elbow_angles.append(recipe_math.vector_angle_degrees(
            recipe_math.vector_subtract(shoulder, elbow),
            recipe_math.vector_subtract(hand, elbow),
        ))
        bend_plane = _vector_cross(
            recipe_math.vector_subtract(hand, shoulder),
            recipe_math.vector_subtract(elbow, shoulder),
        )
        bend_length = recipe_math.magnitude(bend_plane)
        if bend_length <= 1e-6:
            issues.append(f"throwing elbow bend plane degenerates at f{frame}")
            bend_planes.append((1.0, 0.0, 0.0))
        else:
            bend_planes.append(tuple(value / bend_length for value in bend_plane))

    maximum_hand_step, maximum_hand_step_frame = _maximum_translation_step(
        hand_positions
    )
    maximum_grip_step, maximum_grip_step_frame = _maximum_translation_step(
        grip_positions
    )
    maximum_hand_acceleration, maximum_hand_acceleration_frame = (
        _maximum_translation_acceleration(hand_positions)
    )
    maximum_grip_acceleration, maximum_grip_acceleration_frame = (
        _maximum_translation_acceleration(grip_positions)
    )
    maximum_shoulder_step, maximum_shoulder_step_frame = (
        _maximum_translation_step(torso_local_shoulder_positions)
    )
    maximum_shoulder_acceleration, maximum_shoulder_acceleration_frame = (
        _maximum_translation_acceleration(torso_local_shoulder_positions)
    )
    maximum_elbow_step, maximum_elbow_step_frame = (
        _maximum_translation_step(torso_local_elbow_positions)
    )
    maximum_elbow_acceleration, maximum_elbow_acceleration_frame = (
        _maximum_translation_acceleration(torso_local_elbow_positions)
    )
    (
        maximum_clavicle_log_rate,
        maximum_clavicle_log_rate_frame,
        maximum_clavicle_log_acceleration,
        maximum_clavicle_log_acceleration_frame,
    ) = _maximum_log_vector_rate_and_acceleration(
        clavicle_relative_rotations
    )
    (
        maximum_upperarm_log_rate,
        maximum_upperarm_log_rate_frame,
        maximum_upperarm_log_acceleration,
        maximum_upperarm_log_acceleration_frame,
    ) = _maximum_log_vector_rate_and_acceleration(
        upperarm_component_rotations
    )
    (
        maximum_lowerarm_log_rate,
        maximum_lowerarm_log_rate_frame,
        maximum_lowerarm_log_acceleration,
        maximum_lowerarm_log_acceleration_frame,
    ) = _maximum_log_vector_rate_and_acceleration(
        lowerarm_component_rotations
    )
    maximum_elbow_delta, maximum_elbow_delta_frame = max(
        (abs(elbow_angles[frame] - elbow_angles[frame - 1]), frame)
        for frame in range(1, len(elbow_angles))
    )
    maximum_bend_plane_delta, maximum_bend_plane_delta_frame = max(
        (
            recipe_math.vector_angle_degrees(
                bend_planes[frame - 1], bend_planes[frame]
            ),
            frame,
        )
        for frame in range(1, len(bend_planes))
    )
    pocket_to_release_frame_span = (
        RELEASE_GATE_FRAMES[0] - POWER_POCKET_GATE_FRAMES[-1]
    )
    hand_endpoint_lower_bound = (
        recipe_math.magnitude(recipe_math.vector_subtract(
            hand_positions[RELEASE_GATE_FRAMES[0]],
            hand_positions[POWER_POCKET_GATE_FRAMES[-1]],
        )) / pocket_to_release_frame_span
    )
    grip_endpoint_lower_bound = (
        recipe_math.magnitude(recipe_math.vector_subtract(
            grip_positions[RELEASE_GATE_FRAMES[0]],
            grip_positions[POWER_POCKET_GATE_FRAMES[-1]],
        )) / pocket_to_release_frame_span
    )
    observations["source_continuity_guards"] = {
        "frame_rate": 60,
        "quaternion_log_vector_convention": QUATERNION_LOG_VECTOR_CONVENTION,
        "torso_local_shoulder": {
            "maximum_translation_cm_per_frame": maximum_shoulder_step,
            "maximum_translation_frame": maximum_shoulder_step_frame,
            "maximum_translation_limit_cm_per_frame": (
                MAX_TORSO_LOCAL_SHOULDER_TRANSLATION_CM_PER_FRAME
            ),
            "maximum_acceleration_cm_per_frame_squared": (
                maximum_shoulder_acceleration
            ),
            "maximum_acceleration_frame": maximum_shoulder_acceleration_frame,
            "maximum_acceleration_limit_cm_per_frame_squared": (
                MAX_TORSO_LOCAL_SHOULDER_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
        },
        "torso_local_elbow": {
            "maximum_translation_cm_per_frame": maximum_elbow_step,
            "maximum_translation_frame": maximum_elbow_step_frame,
            "maximum_translation_limit_cm_per_frame": (
                MAX_TORSO_LOCAL_ELBOW_TRANSLATION_CM_PER_FRAME
            ),
            "maximum_acceleration_cm_per_frame_squared": (
                maximum_elbow_acceleration
            ),
            "maximum_acceleration_frame": maximum_elbow_acceleration_frame,
            "maximum_acceleration_limit_cm_per_frame_squared": (
                MAX_TORSO_LOCAL_ELBOW_ACCELERATION_CM_PER_FRAME_SQUARED
            ),
        },
        "clavicle_relative_to_spine": {
            "maximum_log_vector_rate_degrees_per_frame": (
                maximum_clavicle_log_rate
            ),
            "maximum_log_vector_rate_frame": maximum_clavicle_log_rate_frame,
            "maximum_log_vector_rate_limit_degrees_per_frame": (
                MAX_CLAVICLE_RELATIVE_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_log_vector_acceleration_degrees_per_frame_squared": (
                maximum_clavicle_log_acceleration
            ),
            "maximum_log_vector_acceleration_frame": (
                maximum_clavicle_log_acceleration_frame
            ),
            "maximum_log_vector_acceleration_limit_degrees_per_frame_squared": (
                MAX_CLAVICLE_RELATIVE_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
        },
        "throwing_upperarm_component": {
            "maximum_log_vector_rate_degrees_per_frame": (
                maximum_upperarm_log_rate
            ),
            "maximum_log_vector_rate_frame": maximum_upperarm_log_rate_frame,
            "maximum_log_vector_rate_limit_degrees_per_frame": (
                MAX_UPPERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_log_vector_acceleration_degrees_per_frame_squared": (
                maximum_upperarm_log_acceleration
            ),
            "maximum_log_vector_acceleration_frame": (
                maximum_upperarm_log_acceleration_frame
            ),
            "maximum_log_vector_acceleration_limit_degrees_per_frame_squared": (
                MAX_UPPERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
        },
        "throwing_lowerarm_component": {
            "maximum_log_vector_rate_degrees_per_frame": (
                maximum_lowerarm_log_rate
            ),
            "maximum_log_vector_rate_frame": maximum_lowerarm_log_rate_frame,
            "maximum_log_vector_rate_limit_degrees_per_frame": (
                MAX_LOWERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME
            ),
            "maximum_log_vector_acceleration_degrees_per_frame_squared": (
                maximum_lowerarm_log_acceleration
            ),
            "maximum_log_vector_acceleration_frame": (
                maximum_lowerarm_log_acceleration_frame
            ),
            "maximum_log_vector_acceleration_limit_degrees_per_frame_squared": (
                MAX_LOWERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
            ),
        },
        "maximum_hand_translation_cm_per_frame": maximum_hand_step,
        "maximum_hand_translation_frame": maximum_hand_step_frame,
        "maximum_hand_translation_limit_cm_per_frame": (
            MAX_HAND_TRANSLATION_CM_PER_FRAME
        ),
        "maximum_hand_translation_meters_per_second": maximum_hand_step * 0.6,
        "maximum_grip_translation_cm_per_frame": maximum_grip_step,
        "maximum_grip_translation_frame": maximum_grip_step_frame,
        "maximum_grip_translation_limit_cm_per_frame": (
            MAX_GRIP_TRANSLATION_CM_PER_FRAME
        ),
        "maximum_grip_translation_meters_per_second": maximum_grip_step * 0.6,
        "maximum_hand_acceleration_cm_per_frame_squared": (
            maximum_hand_acceleration
        ),
        "maximum_hand_acceleration_frame": maximum_hand_acceleration_frame,
        "maximum_hand_acceleration_limit_cm_per_frame_squared": (
            MAX_HAND_ACCELERATION_CM_PER_FRAME_SQUARED
        ),
        "maximum_grip_acceleration_cm_per_frame_squared": (
            maximum_grip_acceleration
        ),
        "maximum_grip_acceleration_frame": maximum_grip_acceleration_frame,
        "maximum_grip_acceleration_limit_cm_per_frame_squared": (
            MAX_GRIP_ACCELERATION_CM_PER_FRAME_SQUARED
        ),
        "maximum_elbow_delta_degrees_per_frame": maximum_elbow_delta,
        "maximum_elbow_delta_frame": maximum_elbow_delta_frame,
        "maximum_elbow_delta_limit_degrees_per_frame": (
            MAX_ELBOW_DELTA_DEGREES_PER_FRAME
        ),
        "maximum_elbow_bend_plane_delta_degrees_per_frame": (
            maximum_bend_plane_delta
        ),
        "maximum_elbow_bend_plane_delta_frame": maximum_bend_plane_delta_frame,
        "maximum_elbow_bend_plane_delta_limit_degrees_per_frame": (
            MAX_BEND_PLANE_DELTA_DEGREES_PER_FRAME
        ),
        "pocket_f81_to_release_f84_hand_geometric_lower_bound_cm_per_frame": (
            hand_endpoint_lower_bound
        ),
        "pocket_f81_to_release_f84_grip_geometric_lower_bound_cm_per_frame": (
            grip_endpoint_lower_bound
        ),
        "maximum_pre_release_disc_plane_delta_degrees_per_frame": (
            maximum_pre_release_plane_delta
        ),
        "maximum_pre_release_disc_plane_delta_limit_degrees": (
            MAX_PRE_RELEASE_DISC_PLANE_DELTA_DEGREES_PER_FRAME
        ),
        "maximum_throwing_wrist_additive_degrees": maximum_wrist_additive,
        "maximum_throwing_wrist_additive_frame": maximum_wrist_additive_frame,
        "maximum_throwing_wrist_additive_limit_degrees": MAX_WRIST_ADDITIVE_DEGREES,
    }
    if maximum_hand_step > MAX_HAND_TRANSLATION_CM_PER_FRAME + 1e-6:
        issues.append("throwing hand exceeds the full-path translation velocity gate")
    if maximum_grip_step > MAX_GRIP_TRANSLATION_CM_PER_FRAME + 1e-6:
        issues.append("disc_grip_r exceeds the full-path translation velocity gate")
    if (
        maximum_hand_acceleration
        > MAX_HAND_ACCELERATION_CM_PER_FRAME_SQUARED + 1e-6
    ):
        issues.append("throwing hand exceeds the full-path acceleration gate")
    if (
        maximum_grip_acceleration
        > MAX_GRIP_ACCELERATION_CM_PER_FRAME_SQUARED + 1e-6
    ):
        issues.append("disc_grip_r exceeds the full-path acceleration gate")
    if (
        maximum_shoulder_step
        > MAX_TORSO_LOCAL_SHOULDER_TRANSLATION_CM_PER_FRAME + 1e-6
    ):
        issues.append("throwing shoulder exceeds the torso-local translation-rate gate")
    if (
        maximum_shoulder_acceleration
        > MAX_TORSO_LOCAL_SHOULDER_ACCELERATION_CM_PER_FRAME_SQUARED + 1e-6
    ):
        issues.append("throwing shoulder exceeds the torso-local acceleration gate")
    if (
        maximum_elbow_step
        > MAX_TORSO_LOCAL_ELBOW_TRANSLATION_CM_PER_FRAME + 1e-6
    ):
        issues.append("throwing elbow exceeds the torso-local translation-rate gate")
    if (
        maximum_elbow_acceleration
        > MAX_TORSO_LOCAL_ELBOW_ACCELERATION_CM_PER_FRAME_SQUARED + 1e-6
    ):
        issues.append("throwing elbow exceeds the torso-local acceleration gate")
    if (
        maximum_clavicle_log_rate
        > MAX_CLAVICLE_RELATIVE_LOG_RATE_DEGREES_PER_FRAME + 1e-6
    ):
        issues.append("throwing clavicle exceeds the spine-relative log-vector rate gate")
    if (
        maximum_clavicle_log_acceleration
        > MAX_CLAVICLE_RELATIVE_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
        + 1e-6
    ):
        issues.append("throwing clavicle exceeds the spine-relative log-vector acceleration gate")
    if (
        maximum_upperarm_log_rate
        > MAX_UPPERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME + 1e-6
    ):
        issues.append("throwing upperarm exceeds the component log-vector rate gate")
    if (
        maximum_upperarm_log_acceleration
        > MAX_UPPERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
        + 1e-6
    ):
        issues.append("throwing upperarm exceeds the component log-vector acceleration gate")
    if (
        maximum_lowerarm_log_rate
        > MAX_LOWERARM_COMPONENT_LOG_RATE_DEGREES_PER_FRAME + 1e-6
    ):
        issues.append("throwing lowerarm exceeds the component log-vector rate gate")
    if (
        maximum_lowerarm_log_acceleration
        > MAX_LOWERARM_COMPONENT_LOG_ACCELERATION_DEGREES_PER_FRAME_SQUARED
        + 1e-6
    ):
        issues.append("throwing lowerarm exceeds the component log-vector acceleration gate")
    if maximum_elbow_delta > MAX_ELBOW_DELTA_DEGREES_PER_FRAME + 1e-6:
        issues.append("throwing elbow exceeds the per-frame angular-rate gate")
    if maximum_bend_plane_delta > MAX_BEND_PLANE_DELTA_DEGREES_PER_FRAME + 1e-6:
        issues.append("throwing elbow bend plane flips or changes too quickly")
    if (
        maximum_pre_release_plane_delta
        > MAX_PRE_RELEASE_DISC_PLANE_DELTA_DEGREES_PER_FRAME + 1e-6
    ):
        issues.append("pre-release disc plane exceeds 15 degrees/frame")
    if maximum_wrist_additive > MAX_WRIST_ADDITIVE_DEGREES + 1e-6:
        issues.append("throwing wrist additive exceeds 25 degrees")

    return issues, observations


def _run_self_test() -> int:
    source = _load(V6_PATH)
    candidate = _load(V7_PATH)
    issues, observations = validate_source_candidate(source, candidate)
    failures: list[str] = []
    if issues:
        failures.append(f"baseline source candidate failed: {issues[:4]}")
    if not RUNTIME_BLOCKERS:
        failures.append("default lane does not fail closed on runtime evidence")

    mutated = copy.deepcopy(candidate)
    mutated["v7_source_motion_design_contract"]["human_animation_approval"] = True
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("human_animation_approval" in issue for issue in mutated_issues):
        failures.append("false approval mutation was accepted")

    mutated = copy.deepcopy(candidate)
    _family(mutated, "Drive")["pose_keys"][0]["translation_cm"]["root"][0] += 0.01
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("outside arm rotations" in issue for issue in mutated_issues):
        failures.append("dense non-arm v006 integer-sample mutation was accepted")

    mutated = copy.deepcopy(candidate)
    release = _poses(_family(mutated, "Drive"))[84]
    release["throwing_hand_grip_alpha"] += 0.01
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("frame-84 release/grip payload" in issue for issue in mutated_issues):
        failures.append("frame-84 grip mutation was accepted")

    mutated = copy.deepcopy(candidate)
    release = _poses(_family(mutated, "Drive"))[84]
    release["rotation_degrees"]["hand_r"][0] += 1.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("component-space disc_grip_r" in issue for issue in mutated_issues):
        failures.append("frame-84 component grip transform mutation was accepted")

    mutated = copy.deepcopy(candidate)
    mutated["v7_source_motion_design_contract"][
        "throwing_arm_power_pocket_gate"
    ]["frames"] = [79]
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("power_pocket frame set" in issue for issue in mutated_issues):
        failures.append("candidate-controlled frame-set contraction was accepted")

    mutated = copy.deepcopy(candidate)
    pose83 = _poses(_family(mutated, "Drive"))[83]
    pose83["rotation_degrees"]["upperarm_r"][1] += 75.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any(
        "full-path" in issue or "bend plane" in issue or "angular-rate" in issue
        for issue in mutated_issues
    ):
        failures.append("full-path arm discontinuity mutation was accepted")

    mutated = copy.deepcopy(candidate)
    _family(mutated, "Drive")["pose_keys"].pop(1)
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("dense frame set/order" in issue for issue in mutated_issues):
        failures.append("dense required-frame deletion was accepted")

    mutated = copy.deepcopy(candidate)
    pose66 = _poses(_family(mutated, "Drive"))[66]
    pose66["rotation_degrees"]["lowerarm_l"][2] = -92.01
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any(
        "lowerarm_l Roll exceeds the 92-degree native parser bound" in issue
        for issue in mutated_issues
    ):
        failures.append("v007 support-lowerarm native parser bound mutation was accepted")

    mutated = copy.deepcopy(candidate)
    pose70 = _poses(_family(mutated, "Drive"))[70]
    pose70["rotation_degrees"]["clavicle_r"][0] += 90.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not all(any(fragment in issue for issue in mutated_issues) for fragment in (
        "shoulder exceeds the torso-local translation-rate",
        "shoulder exceeds the torso-local acceleration",
    )):
        failures.append("torso-local shoulder rate/acceleration mutation was accepted")
    if not all(any(fragment in issue for issue in mutated_issues) for fragment in (
        "clavicle exceeds the spine-relative log-vector rate",
        "clavicle exceeds the spine-relative log-vector acceleration",
    )):
        failures.append("spine-relative clavicle rate/acceleration mutation was accepted")

    mutated = copy.deepcopy(candidate)
    pose70 = _poses(_family(mutated, "Drive"))[70]
    pose70["rotation_degrees"]["upperarm_r"][0] += 90.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not all(any(fragment in issue for issue in mutated_issues) for fragment in (
        "upperarm exceeds the component log-vector rate",
        "upperarm exceeds the component log-vector acceleration",
    )):
        failures.append("upperarm component rate/acceleration mutation was accepted")

    mutated = copy.deepcopy(candidate)
    pose70 = _poses(_family(mutated, "Drive"))[70]
    pose70["rotation_degrees"]["lowerarm_r"][0] += 90.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not all(any(fragment in issue for issue in mutated_issues) for fragment in (
        "lowerarm exceeds the component log-vector rate",
        "lowerarm exceeds the component log-vector acceleration",
    )):
        failures.append("lowerarm component rate/acceleration mutation was accepted")

    mutated = copy.deepcopy(candidate)
    release = _poses(_family(mutated, "Drive"))[84]
    release["rotation_degrees"]["clavicle_r"][0] += 90.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any("release clavicle delta" in issue for issue in mutated_issues):
        failures.append("release proximal static-bound mutation was accepted")

    mutated = copy.deepcopy(candidate)
    mutated["v7_source_motion_design_contract"][
        "release_proximal_stability_gate"
    ]["maximum_shoulder_origin_delta_from_v006_cm"] = 99.0
    mutated_issues, _ = validate_source_candidate(source, mutated)
    if not any(
        "release_proximal_stability_gate" in issue
        for issue in mutated_issues
    ):
        failures.append("candidate-controlled release-bound weakening was accepted")

    result = {
        "schema": "DiscGolfTour.ProductionMotionV7SourceValidatorSelfTest.v1",
        "status": "PASS_SELF_TEST" if not failures else "FAIL_SELF_TEST",
        "cases": 14,
        "failures": failures,
        "baseline_v006_sha256": observations.get("v006", {}).get("sha256"),
        "baseline_v007_sha256": observations.get("v007", {}).get("sha256"),
    }
    print(json.dumps(result, indent=2))
    return 0 if not failures else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-only", action="store_true",
        help="pass on deterministic source-space gates without claiming runtime acceptance",
    )
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return _run_self_test()

    source = _load(V6_PATH)
    candidate = _load(V7_PATH)
    issues, observations = validate_source_candidate(source, candidate)
    source_status = "PASS_SOURCE_ONLY" if not issues else "FAIL_SOURCE_ONLY"
    overall_status = (
        "BLOCKED_RUNTIME_AND_HUMAN_EVIDENCE"
        if not issues else "FAIL_SOURCE_AND_BLOCKED_RUNTIME"
    )
    result = {
        "schema": "DiscGolfTour.ProductionMotionV7ValidationScaffold.v1",
        "source_status": source_status,
        "overall_status": overall_status,
        "source_issues": issues,
        "runtime_blockers": RUNTIME_BLOCKERS,
        "observations": observations,
        "claims": {
            "source_space_design_gates_pass": not issues,
            "unreal_asset_validation_complete": False,
            "metahuman_visual_validation_complete": False,
            "human_animation_approval": False,
            "human_disc_contact_approval": False,
            "release_approval": False,
        },
    }
    print(json.dumps(result, indent=2))
    if issues:
        return 1
    return 0 if args.source_only else 1


if __name__ == "__main__":
    sys.exit(main())
