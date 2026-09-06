#!/usr/bin/env python3
"""Deterministically derive the append-only v007 source-motion candidate.

v007 never rewrites v006 or an Unreal asset.  It preserves v006 timing,
events, curves, root motion, axial motion, legs, grip/fingers, and gameplay
identity, then replaces only the Drive family's bilateral clavicle/arm/hand
local additives.  The candidate uses the exact probed DGMaster reference
chain to remove the source T-pose silhouette, form a compact power pocket,
extend through release/follow-through, and recover the throwing hand toward
the hip.

The generated recipe deliberately remains a source-space candidate.  Static
joint geometry is not proof of MetaHuman deformation, disc contact, authored
Unreal assets, temporal visual quality, or human approval.  The companion
``validate_dg_session19_production_motion_v7.py`` therefore fails closed until
separate runtime evidence is supplied.
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

import generate_dg_session19_production_motion_v6 as v6


ROOT = Path(__file__).resolve().parents[1]
V6_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json"
V7_PATH = ROOT / "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json"

ARM_CHAINS = v6.ARM_CHAINS
ARM_BONES = v6.ARM_BONES

# Exact UE 5.8 DGMaster reference-pose translations captured read-only from
# SKEL_DG_Master.  They pair with v6.REFERENCE_LOCAL_ROTATIONS.
REFERENCE_LOCAL_TRANSLATIONS_CM = {
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

# Blender source local (-0.035, +0.060, -0.020) m after the FBX import's
# meter-to-centimeter and handed-axis conversion.  The resulting length
# (7.228416 cm) matches the live DGMaster source-hand-to-grip observation.
REFERENCE_DISC_GRIP_TRANSLATION_CM = (-3.5, -6.0, -2.0)

# A relaxed support arm removes the bilateral straight-arm silhouette.  Every
# v006 Drive key remains present; only its arm-chain rotations are replaced.
SUPPORT_ELBOW_DEGREES_BY_FRAME = {
    0: 105.0, 6: 100.0, 12: 95.0, 18: 92.0, 24: 90.0,
    30: 88.0, 36: 86.0, 42: 88.0, 44: 90.0, 48: 92.0,
    54: 94.0, 60: 93.0, 64: 90.0, 66: 88.0, 72: 86.0,
    76: 85.0, 77: 85.0, 78: 85.0, 79: 85.0, 81: 85.0,
    83: 88.0, 84: 90.0, 90: 94.0, 94: 95.0, 96: 95.0,
    102: 95.0, 104: 95.0, 108: 94.0, 114: 92.0, 118: 90.0,
    120: 90.0, 126: 92.0, 132: 95.0, 138: 98.0, 144: 100.0,
}
SUPPORT_TUCK_BLEND_BY_FRAME = {
    0: 0.40, 6: 0.45, 12: 0.50, 18: 0.55, 24: 0.60,
    30: 0.65, 36: 0.72, 42: 0.80, 44: 0.84, 48: 0.86,
    54: 0.88, 60: 0.90, 64: 0.92, 66: 0.94, 72: 0.96,
    76: 0.98, 77: 0.99, 78: 1.00, 79: 1.00, 81: 1.00,
    83: 1.00, 84: 1.00, 90: 0.96, 94: 0.92, 96: 0.92,
    102: 0.92, 104: 0.94, 108: 0.96, 114: 0.98, 118: 1.00,
    120: 1.00, 126: 0.96, 132: 0.90, 138: 0.84, 144: 0.80,
}

# The power-pocket and extension schedule is source-space design intent, not a
# claim of final target quality.  f80 is interpolated between explicit f79/f81
# keys and is independently sampled by the v007 validator.
THROWING_ELBOW_DEGREES_BY_FRAME = {
    72: 135.0,
    76: 118.0,
    77: 108.0,
    78: 101.0,
    79: 97.0,
    81: 105.0,
    83: 138.0,
    84: 145.0,
    90: 164.0,
    94: 165.0,
    96: 160.0,
    102: 140.0,
    104: 130.0,
    108: 116.0,
    114: 101.0,
    118: 92.0,
    120: 92.0,
    126: 96.0,
    132: 100.0,
    138: 104.0,
    144: 104.0,
}
THROWING_HIP_RECOVERY_BLEND_BY_FRAME = {
    72: 0.0, 76: 0.0, 77: 0.0, 78: 0.0, 79: 0.0, 81: 0.0,
    83: 0.0, 84: 0.0, 90: 0.0, 94: 0.0, 96: 0.0,
    102: 0.0, 104: 0.05, 108: 0.22, 114: 0.68, 118: 1.0,
    120: 1.0, 126: 0.92, 132: 0.82, 138: 0.76, 144: 0.76,
}
THROWING_COMPACT_POCKET_BLEND_BY_FRAME = {
    72: 0.00,
    76: 0.25,
    77: 0.45,
    78: 0.70,
    79: 1.00,
    81: 1.00,
    83: 0.00,
}

# Torso coordinates are (throwing-side lateral, forward, vertical).  This is a
# direction from shoulder, not an absolute hand point.
SUPPORT_TUCK_DIRECTION_TORSO = (0.15, -0.60, -0.79)
THROWING_COMPACT_POCKET_DIRECTION_TORSO = (-0.735, -0.665, -0.094)
THROWING_HIP_DIRECTION_TORSO = (-0.18, -0.12, -0.976)
MAX_THROWING_WRIST_PLANE_CORRECTION_DEGREES = 20.0
MAX_DENSE_THROWING_HAND_ADDITIVE_DEGREES = 24.5
# The immutable f84 grip and compact f81 pocket leave only three frames for
# extension.  These deterministic f83 tangents balance the Catmull path's
# full-path velocity/acceleration instead of optimizing key positions alone.
F83_TRANSITION_HAND_ALPHA = 0.66
F83_TRANSITION_COMPONENT_ALPHA = 0.70
F83_FIXED_HAND_SHOULDER_CIRCLE_INDEX: int | None = None
# The exact f84 grip and elbow constraints admit a shoulder circle.  Keep the
# selected sample explicit so a source-range review can change it without
# weakening any candidate-independent validation limit.
F84_FIXED_HAND_SHOULDER_CIRCLE_INDEX = 115
THROWING_CLAVICLE_FACTOR_OVERRIDE_BY_FRAME: dict[int, float] = {}

DENSE_THROWING_RESHAPE_START_FRAME = 66
DENSE_THROWING_RELEASE_FRAME = 84
DENSE_THROWING_RESHAPE_END_FRAME = 94
DENSE_THROWING_PRE_RELEASE_ANCHORS = (66, 72, 76, 77, 78, 79, 81)
DENSE_THROWING_BRIDGE = {
    82: {
        "hand_position_alpha": 0.36,
        "hand_component_alpha": 0.36,
    },
    83: {
        "hand_position_alpha": 0.73,
        "hand_component_alpha": 0.73,
    },
}


def _load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if type(value) is not dict:
        raise ValueError(f"{path} does not contain a JSON object")
    return value


def _add(first: tuple[float, ...], second: tuple[float, ...]) -> tuple[float, ...]:
    return tuple(a + b for a, b in zip(first, second))


def _subtract(
    first: tuple[float, ...], second: tuple[float, ...],
) -> tuple[float, ...]:
    return tuple(a - b for a, b in zip(first, second))


def _scale(value: tuple[float, ...], amount: float) -> tuple[float, ...]:
    return tuple(component * amount for component in value)


def _dot(first: tuple[float, ...], second: tuple[float, ...]) -> float:
    return sum(a * b for a, b in zip(first, second))


def _length(value: tuple[float, ...]) -> float:
    return math.sqrt(_dot(value, value))


def _unit(value: tuple[float, ...]) -> tuple[float, ...]:
    length = _length(value)
    if length <= 1e-9:
        raise ValueError("degenerate direction")
    return _scale(value, 1.0 / length)


def _nlerp_quaternion(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
    alpha: float,
) -> tuple[float, float, float, float]:
    """Shortest-path normalized quaternion interpolation."""
    if _dot(first, second) < 0.0:
        second = tuple(-value for value in second)
    return _unit(_add(_scale(first, 1.0 - alpha), _scale(second, alpha)))


def _slerp_quaternion(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
    alpha: float,
) -> tuple[float, float, float, float]:
    """Shortest-path spherical interpolation for component trajectories."""
    dot = _dot(first, second)
    if dot < 0.0:
        second = tuple(-value for value in second)
        dot = -dot
    dot = min(1.0, max(-1.0, dot))
    if dot > 0.9995:
        return _nlerp_quaternion(first, second, alpha)
    angle = math.acos(dot)
    sine = math.sin(angle)
    return tuple(
        (
            math.sin((1.0 - alpha) * angle) * first_component
            + math.sin(alpha * angle) * second_component
        ) / sine
        for first_component, second_component in zip(first, second)
    )


def _smoothstep(alpha: float) -> float:
    value = min(1.0, max(0.0, alpha))
    return value * value * (3.0 - 2.0 * value)


def _rotate_about_axis(
    value: tuple[float, float, float],
    axis: tuple[float, float, float],
    degrees: float,
) -> tuple[float, float, float]:
    direction = _unit(axis)
    radians = math.radians(degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    return _add(
        _add(
            _scale(value, cosine),
            _scale(_cross(direction, value), sine),
        ),
        _scale(direction, _dot(direction, value) * (1.0 - cosine)),
    )


def _catmull_rom_scalar(
    first: float, second: float, third: float, fourth: float, alpha: float,
) -> float:
    value = min(1.0, max(0.0, alpha))
    squared = value * value
    cubed = squared * value
    return 0.5 * (
        2.0 * second
        + (-first + third) * value
        + (2.0 * first - 5.0 * second + 4.0 * third - fourth) * squared
        + (-first + 3.0 * second - 3.0 * third + fourth) * cubed
    )


def _sampled_scalar(
    ordered_frames: list[int], values: dict[int, float], frame: int,
) -> float:
    upper = 1
    while upper < len(ordered_frames) and ordered_frames[upper] < frame:
        upper += 1
    upper = min(max(upper, 1), len(ordered_frames) - 1)
    earlier = ordered_frames[upper - 1]
    later = ordered_frames[upper]
    alpha = (frame - earlier) / max(1, later - earlier)
    return _catmull_rom_scalar(
        values[ordered_frames[max(0, upper - 2)]],
        values[earlier],
        values[later],
        values[ordered_frames[min(len(ordered_frames) - 1, upper + 1)]],
        alpha,
    )


def _densify_pose_keys(family: dict[str, Any]) -> list[dict[str, Any]]:
    """Bake the sparse Catmull source to exact integer-frame pose samples."""
    source_poses = family["pose_keys"]
    ordered_frames = [pose["frame"] for pose in source_poses]
    rotations = {
        pose["frame"]: pose["rotation_degrees"] for pose in source_poses
    }
    grips = {
        pose["frame"]: float(pose["throwing_hand_grip_alpha"])
        for pose in source_poses
    }
    roots = {
        pose["frame"]: tuple(float(value) for value in pose["translation_cm"]["root"])
        for pose in source_poses
    }
    bones = tuple(source_poses[0]["rotation_degrees"])
    dense: list[dict[str, Any]] = []
    for frame in range(family["frame_count"] + 1):
        dense.append({
            "frame": frame,
            "throwing_hand_grip_alpha": round(
                _sampled_scalar(ordered_frames, grips, frame), 6,
            ),
            "translation_cm": {
                "root": [
                    round(_sampled_scalar(
                        ordered_frames,
                        {
                            pose_frame: translation[axis]
                            for pose_frame, translation in roots.items()
                        },
                        frame,
                    ), 6)
                    for axis in range(3)
                ],
            },
            "rotation_degrees": {
                bone: [
                    round(_sampled_scalar(
                        ordered_frames,
                        {
                            pose_frame: float(values[bone][axis])
                            for pose_frame, values in rotations.items()
                        },
                        frame,
                    ), 6)
                    for axis in range(3)
                ]
                for bone in bones
            },
        })
    return dense


def _blend_direction(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
    alpha: float,
) -> tuple[float, float, float]:
    alpha = min(1.0, max(0.0, alpha))
    blended = _add(_scale(_unit(first), 1.0 - alpha), _scale(_unit(second), alpha))
    if _length(blended) <= 1e-6:
        raise ValueError("direction blend reached an antipodal singularity")
    return _unit(blended)


def _slerp_direction(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
    alpha: float,
) -> tuple[float, float, float]:
    """Spherical interpolation that preserves oriented bend-plane normals."""
    first = _unit(first)
    second = _unit(second)
    dot = min(1.0, max(-1.0, _dot(first, second)))
    angle = math.acos(dot)
    sine = math.sin(angle)
    if angle <= 1e-6:
        return first
    if abs(sine) <= 1e-6:
        raise ValueError("oriented bend-plane interpolation is antipodal")
    return _unit(tuple(
        (
            math.sin((1.0 - alpha) * angle) * first_component
            + math.sin(alpha * angle) * second_component
        ) / sine
        for first_component, second_component in zip(first, second)
    ))


def _bend_twist_degrees(
    hand_direction: tuple[float, float, float],
    bend_direction: tuple[float, float, float],
) -> float:
    """Encode the elbow bend about the hand ray in a stable vertical frame."""
    direction = _unit(hand_direction)
    vertical = (0.0, 0.0, 1.0)
    basis_zero = _subtract(
        vertical, _scale(direction, _dot(vertical, direction)),
    )
    if _length(basis_zero) <= 1e-6:
        raise ValueError("dense hand direction cannot define a vertical bend frame")
    basis_zero = _unit(basis_zero)
    basis_quarter = _cross(direction, basis_zero)
    bend = _unit(bend_direction)
    return math.degrees(math.atan2(
        _dot(bend, basis_quarter), _dot(bend, basis_zero),
    ))


def _bend_from_twist_degrees(
    hand_direction: tuple[float, float, float], degrees: float,
) -> tuple[float, float, float]:
    direction = _unit(hand_direction)
    vertical = (0.0, 0.0, 1.0)
    basis_zero = _subtract(
        vertical, _scale(direction, _dot(vertical, direction)),
    )
    if _length(basis_zero) <= 1e-6:
        raise ValueError("dense hand direction cannot define a vertical bend frame")
    basis_zero = _unit(basis_zero)
    basis_quarter = _cross(direction, basis_zero)
    radians = math.radians(degrees)
    return _unit(_add(
        _scale(basis_zero, math.cos(radians)),
        _scale(basis_quarter, math.sin(radians)),
    ))


def _shortest_angle_delta_degrees(first: float, second: float) -> float:
    return (second - first + 180.0) % 360.0 - 180.0


def _clamp_quaternion_angle(
    quaternion: tuple[float, float, float, float], maximum_degrees: float,
) -> tuple[float, float, float, float]:
    """Return the same shortest-arc axis with a bounded angular magnitude."""
    value = v6._normalize(quaternion)
    if value[3] < 0.0:
        value = tuple(-component for component in value)
    half_angle = math.acos(min(1.0, max(-1.0, value[3])))
    angle = 2.0 * half_angle
    maximum = math.radians(maximum_degrees)
    if angle <= maximum + 1e-12 or half_angle <= 1e-12:
        return value
    sine = math.sin(half_angle)
    axis = tuple(component / sine for component in value[:3])
    clamped_half = 0.5 * maximum
    return v6._normalize((
        axis[0] * math.sin(clamped_half),
        axis[1] * math.sin(clamped_half),
        axis[2] * math.sin(clamped_half),
        math.cos(clamped_half),
    ))


def _arm_pose(
    rotations: dict[str, list[float]],
) -> tuple[
    dict[str, tuple[float, float, float]],
    dict[str, tuple[float, float, float, float]],
    tuple[float, float, float, float],
]:
    spine_component = v6._multiply(
        v6._rotator_quaternion(rotations["spine_04"]),
        v6.REFERENCE_COMPONENT_ROTATIONS["spine_04"],
    )
    positions: dict[str, tuple[float, float, float]] = {
        "spine_04": (0.0, 0.0, 0.0),
    }
    components: dict[str, tuple[float, float, float, float]] = {
        "spine_04": spine_component,
    }
    for chain in ARM_CHAINS.values():
        parent_position = positions["spine_04"]
        parent_component = spine_component
        for bone in chain:
            positions[bone] = _add(
                parent_position,
                v6._rotate(parent_component, REFERENCE_LOCAL_TRANSLATIONS_CM[bone]),
            )
            local = v6._multiply(
                v6.REFERENCE_LOCAL_ROTATIONS[bone],
                v6._rotator_quaternion(rotations[bone]),
            )
            components[bone] = v6._multiply(parent_component, local)
            parent_position = positions[bone]
            parent_component = components[bone]
    return positions, components, spine_component


def _torso_direction_to_component(
    spine_component: tuple[float, float, float, float],
    direction: tuple[float, float, float],
) -> tuple[float, float, float]:
    lateral, forward, vertical = direction
    return _unit(v6._rotate(
        spine_component,
        (lateral, -vertical, forward),
    ))


def _write_component_rotation(
    rotations: dict[str, list[float]],
    bone: str,
    parent_component: tuple[float, float, float, float],
    target_component: tuple[float, float, float, float],
) -> None:
    target_local = v6._multiply(v6._inverse(parent_component), target_component)
    additive = v6._multiply(
        v6._inverse(v6.REFERENCE_LOCAL_ROTATIONS[bone]), target_local,
    )
    rotations[bone] = v6._round_degrees(v6._quaternion_rotator(additive))


def _target_joint_geometry(
    shoulder: tuple[float, float, float],
    original_elbow: tuple[float, float, float],
    hand_direction: tuple[float, float, float],
    upper_length: float,
    lower_length: float,
    elbow_degrees: float,
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    radians = math.radians(elbow_degrees)
    shoulder_hand_length = math.sqrt(max(
        0.0,
        upper_length * upper_length + lower_length * lower_length
        - 2.0 * upper_length * lower_length * math.cos(radians),
    ))
    direction = _unit(hand_direction)
    hand = _add(shoulder, _scale(direction, shoulder_hand_length))
    along = (
        upper_length * upper_length - lower_length * lower_length
        + shoulder_hand_length * shoulder_hand_length
    ) / (2.0 * shoulder_hand_length)
    bend_height = math.sqrt(max(0.0, upper_length * upper_length - along * along))
    original_offset = _subtract(original_elbow, shoulder)
    bend = _subtract(original_offset, _scale(direction, _dot(original_offset, direction)))
    if _length(bend) <= 1e-6:
        # Fail deterministically rather than choose a visually arbitrary elbow flip.
        raise ValueError("source elbow does not define a stable bend plane")
    elbow = _add(
        _add(shoulder, _scale(direction, along)),
        _scale(_unit(bend), bend_height),
    )
    return elbow, hand


def _nearest_sphere_intersection(
    first_center: tuple[float, float, float],
    first_radius: float,
    second_center: tuple[float, float, float],
    second_radius: float,
    preferred: tuple[float, float, float],
) -> tuple[float, float, float]:
    """Choose the sphere-intersection point nearest the preferred joint."""
    centers = _subtract(second_center, first_center)
    distance = _length(centers)
    if (
        distance <= 1e-9
        or distance > first_radius + second_radius + 1e-6
        or distance < abs(first_radius - second_radius) - 1e-6
    ):
        raise ValueError("fixed-hand release has no reachable shoulder solution")
    direction = _unit(centers)
    along = (
        first_radius * first_radius - second_radius * second_radius
        + distance * distance
    ) / (2.0 * distance)
    circle_radius = math.sqrt(max(0.0, first_radius * first_radius - along * along))
    circle_center = _add(first_center, _scale(direction, along))
    preferred_offset = _subtract(preferred, circle_center)
    circle_direction = _subtract(
        preferred_offset, _scale(direction, _dot(preferred_offset, direction)),
    )
    if circle_radius <= 1e-9:
        return circle_center
    if _length(circle_direction) <= 1e-6:
        raise ValueError("fixed-hand release shoulder circle lacks a stable preference")
    return _add(circle_center, _scale(_unit(circle_direction), circle_radius))


def _cross(
    first: tuple[float, float, float], second: tuple[float, float, float],
) -> tuple[float, float, float]:
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def _choose_fixed_hand_geometry(
    clavicle_origin: tuple[float, float, float],
    original_shoulder: tuple[float, float, float],
    fixed_hand: tuple[float, float, float],
    shoulder_radius: float,
    shoulder_hand_length: float,
    preferred_elbow: tuple[float, float, float],
    ideal_distal: tuple[float, float, float],
    upper_length: float,
    lower_length: float,
    target_elbow_degrees: float,
    shoulder_circle_index: int | None = None,
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Find the nearest shoulder whose exact hand solve keeps wrist intent <=20°."""
    centers = _subtract(fixed_hand, clavicle_origin)
    distance = _length(centers)
    direction = _unit(centers)
    along = (
        shoulder_radius * shoulder_radius
        - shoulder_hand_length * shoulder_hand_length
        + distance * distance
    ) / (2.0 * distance)
    circle_radius = math.sqrt(max(
        0.0, shoulder_radius * shoulder_radius - along * along,
    ))
    circle_center = _add(clavicle_origin, _scale(direction, along))
    preferred = _subtract(original_shoulder, circle_center)
    preferred = _subtract(
        preferred, _scale(direction, _dot(preferred, direction)),
    )
    if circle_radius <= 1e-9 or _length(preferred) <= 1e-6:
        raise ValueError("fixed-hand shoulder solve has no stable intersection circle")
    basis_a = _unit(preferred)
    basis_b = _unit(_cross(direction, basis_a))
    candidates = []
    for index in range(720):
        radians = 2.0 * math.pi * index / 720.0
        shoulder = _add(
            circle_center,
            _scale(_add(
                _scale(basis_a, math.cos(radians)),
                _scale(basis_b, math.sin(radians)),
            ), circle_radius),
        )
        elbow, solved_hand = _target_joint_geometry(
            shoulder, preferred_elbow, _subtract(fixed_hand, shoulder),
            upper_length, lower_length, target_elbow_degrees,
        )
        if _length(_subtract(solved_hand, fixed_hand)) > 0.0001:
            continue
        wrist_correction = math.degrees(math.acos(min(1.0, max(-1.0, _dot(
            _unit(ideal_distal), _unit(_subtract(fixed_hand, elbow)),
        )))))
        if wrist_correction <= MAX_THROWING_WRIST_PLANE_CORRECTION_DEGREES + 1e-6:
            candidates.append((
                _length(_subtract(shoulder, original_shoulder)),
                wrist_correction,
                index,
                shoulder,
                elbow,
            ))
    if not candidates:
        raise ValueError("fixed-hand solve cannot satisfy the 20-degree wrist bound")
    if shoulder_circle_index is not None:
        matches = [
            candidate for candidate in candidates
            if candidate[2] == shoulder_circle_index
        ]
        if len(matches) != 1:
            raise ValueError(
                "requested fixed-hand shoulder-circle sample violates the "
                "wrist bound"
            )
        return matches[0][3], matches[0][4]
    _, _, _, shoulder, elbow = min(candidates, key=lambda value: value[:3])
    return shoulder, elbow


def _solve_arm(
    rotations: dict[str, list[float]],
    side: str,
    target_elbow_degrees: float,
    direction_blend: float,
    target_direction_torso: tuple[float, float, float],
    clavicle_neutral_factor: float,
) -> None:
    clavicle, upperarm, lowerarm, hand = ARM_CHAINS[side]
    _, original_components, _ = _arm_pose(rotations)
    original_hand_component = original_components[hand]

    rotations[clavicle] = [
        round(float(value) * clavicle_neutral_factor, 6)
        for value in rotations[clavicle]
    ]
    positions, components, spine_component = _arm_pose(rotations)
    shoulder = positions[upperarm]
    elbow = positions[lowerarm]
    wrist = positions[hand]
    current_direction = _unit(_subtract(wrist, shoulder))
    desired_direction = _torso_direction_to_component(
        spine_component, target_direction_torso,
    )
    hand_direction = _blend_direction(
        current_direction, desired_direction, direction_blend,
    )
    upper_length = _length(_subtract(elbow, shoulder))
    lower_length = _length(_subtract(wrist, elbow))
    target_elbow, target_wrist = _target_joint_geometry(
        shoulder, elbow, hand_direction, upper_length, lower_length,
        target_elbow_degrees,
    )

    upper_correction = v6._shortest_arc(
        _subtract(elbow, shoulder), _subtract(target_elbow, shoulder),
    )
    target_upper_component = v6._multiply(
        upper_correction, components[upperarm],
    )
    _write_component_rotation(
        rotations, upperarm, components[clavicle], target_upper_component,
    )

    # Apply the original lower-arm local intent under the solved upper arm,
    # then take its shortest spatial correction to the target wrist segment.
    provisional_lower_component = v6._multiply(
        target_upper_component,
        v6._multiply(
            v6.REFERENCE_LOCAL_ROTATIONS[lowerarm],
            v6._rotator_quaternion(rotations[lowerarm]),
        ),
    )
    provisional_distal = v6._rotate(
        provisional_lower_component, REFERENCE_LOCAL_TRANSLATIONS_CM[hand],
    )
    lower_correction = v6._shortest_arc(
        provisional_distal, _subtract(target_wrist, target_elbow),
    )
    target_lower_component = v6._multiply(
        lower_correction, provisional_lower_component,
    )
    _write_component_rotation(
        rotations, lowerarm, target_upper_component, target_lower_component,
    )

    if side == "r":
        # Restore only the original disc-plane normal with the shortest wrist
        # correction.  Forearm twist is not silently pushed into a full hand
        # orientation match.
        neutral_hand_component = v6._multiply(
            target_lower_component, v6.REFERENCE_LOCAL_ROTATIONS[hand],
        )
        neutral_normal = v6._normal_from_hand(neutral_hand_component)
        target_normal = v6._normal_from_hand(original_hand_component)
        if _dot(neutral_normal, target_normal) < 0.0:
            target_normal = _scale(target_normal, -1.0)
        plane_correction = _clamp_quaternion_angle(
            v6._shortest_arc(neutral_normal, target_normal),
            MAX_THROWING_WRIST_PLANE_CORRECTION_DEGREES,
        )
        target_hand_component = v6._multiply(
            plane_correction, neutral_hand_component,
        )
        _write_component_rotation(
            rotations, hand, target_lower_component, target_hand_component,
        )
    else:
        # Relax the non-throwing wrist with the tucked arm instead of trying to
        # preserve its former straight-arm component orientation.
        rotations[hand] = [
            round(float(value) * 0.35, 6) for value in rotations[hand]
        ]


def _solve_arm_to_fixed_hand_transform(
    rotations: dict[str, list[float]],
    target_elbow_degrees: float,
    fixed_hand: tuple[float, float, float] | None = None,
    fixed_hand_component: tuple[float, float, float, float] | None = None,
    shoulder_circle_index: int | None = None,
) -> None:
    """Solve an elbow to an exact component-space hand transform.

    A fixed shoulder cannot satisfy both v006's compact hand point and the
    requested release elbow.  Move the shoulder on its exact 16 cm
    clavicle sphere, then solve the two arm segments back to the unchanged
    hand origin.  The original hand component rotation is restored exactly,
    so any fixed hand->disc_grip_r reference transform is also unchanged.
    """
    side = "r"
    clavicle, upperarm, lowerarm, hand = ARM_CHAINS[side]
    original_positions, original_components, spine_component = _arm_pose(rotations)
    clavicle_origin = original_positions[clavicle]
    original_shoulder = original_positions[upperarm]
    original_elbow = original_positions[lowerarm]
    if fixed_hand is None:
        fixed_hand = original_positions[hand]
    if fixed_hand_component is None:
        fixed_hand_component = original_components[hand]
    upper_length = _length(REFERENCE_LOCAL_TRANSLATIONS_CM[lowerarm])
    lower_length = _length(REFERENCE_LOCAL_TRANSLATIONS_CM[hand])
    elbow_radians = math.radians(target_elbow_degrees)
    shoulder_hand_length = math.sqrt(max(
        0.0,
        upper_length * upper_length + lower_length * lower_length
        - 2.0 * upper_length * lower_length * math.cos(elbow_radians),
    ))
    shoulder_radius = _length(_subtract(original_shoulder, clavicle_origin))
    ideal_lower_component = v6._multiply(
        fixed_hand_component,
        v6._inverse(v6.REFERENCE_LOCAL_ROTATIONS[hand]),
    )
    ideal_distal = v6._rotate(
        ideal_lower_component, REFERENCE_LOCAL_TRANSLATIONS_CM[hand],
    )
    preferred_elbow = _subtract(fixed_hand, ideal_distal)
    target_shoulder, target_elbow = _choose_fixed_hand_geometry(
        clavicle_origin,
        original_shoulder,
        fixed_hand,
        shoulder_radius,
        shoulder_hand_length,
        preferred_elbow,
        ideal_distal,
        upper_length,
        lower_length,
        target_elbow_degrees,
        shoulder_circle_index,
    )

    clavicle_correction = v6._shortest_arc(
        _subtract(original_shoulder, clavicle_origin),
        _subtract(target_shoulder, clavicle_origin),
    )
    target_clavicle_component = v6._multiply(
        clavicle_correction, original_components[clavicle],
    )
    _write_component_rotation(
        rotations, clavicle, spine_component, target_clavicle_component,
    )

    positions, components, _ = _arm_pose(rotations)
    shoulder = positions[upperarm]
    if _length(_subtract(shoulder, target_shoulder)) > 0.0001:
        raise ValueError("fixed-hand release shoulder reconstruction drifted")

    upper_correction = v6._shortest_arc(
        _subtract(positions[lowerarm], shoulder),
        _subtract(target_elbow, shoulder),
    )
    target_upper_component = v6._multiply(
        upper_correction, components[upperarm],
    )
    _write_component_rotation(
        rotations, upperarm, components[clavicle], target_upper_component,
    )
    lower_correction = v6._shortest_arc(
        v6._rotate(
            ideal_lower_component, REFERENCE_LOCAL_TRANSLATIONS_CM[hand],
        ),
        _subtract(fixed_hand, target_elbow),
    )
    target_lower_component = v6._multiply(
        lower_correction, ideal_lower_component,
    )
    _write_component_rotation(
        rotations, lowerarm, target_upper_component, target_lower_component,
    )
    _write_component_rotation(
        rotations, hand, target_lower_component, fixed_hand_component,
    )

    final_positions, final_components, _ = _arm_pose(rotations)
    hand_error = _length(_subtract(final_positions[hand], fixed_hand))
    hand_rotation_error = v6._angular_distance(
        final_components[hand], fixed_hand_component,
    )
    if hand_error > 0.0001 or hand_rotation_error > 0.0001:
        raise ValueError(
            "fixed-grip release reconstruction exceeded tolerance: "
            f"hand={hand_error:.9f}cm rotation={hand_rotation_error:.9f}deg"
        )


def _dense_arm_anchor(
    arm_pose: tuple[
        dict[str, tuple[float, float, float]],
        dict[str, tuple[float, float, float, float]],
        tuple[float, float, float, float],
    ],
) -> dict[str, Any]:
    positions, components, _ = arm_pose
    shoulder = positions["upperarm_r"]
    elbow = positions["lowerarm_r"]
    hand = positions["hand_r"]
    hand_direction = _unit(_subtract(hand, shoulder))
    elbow_offset = _subtract(elbow, shoulder)
    bend_direction = _subtract(
        elbow_offset,
        _scale(hand_direction, _dot(elbow_offset, hand_direction)),
    )
    return {
        "hand_direction": hand_direction,
        "bend_direction": _unit(bend_direction),
        "bend_twist_degrees": _bend_twist_degrees(
            hand_direction, bend_direction,
        ),
        "elbow_degrees": math.degrees(math.acos(min(1.0, max(-1.0, _dot(
            _unit(_subtract(shoulder, elbow)),
            _unit(_subtract(hand, elbow)),
        ))))),
        "hand_component": components["hand_r"],
    }


def _dense_anchor_segment(
    frame: int,
) -> tuple[int, int, float]:
    for index in range(1, len(DENSE_THROWING_PRE_RELEASE_ANCHORS)):
        later = DENSE_THROWING_PRE_RELEASE_ANCHORS[index]
        if frame <= later:
            earlier = DENSE_THROWING_PRE_RELEASE_ANCHORS[index - 1]
            return earlier, later, (frame - earlier) / (later - earlier)
    raise ValueError(f"dense throwing frame f{frame} is outside pre-release anchors")


def _solve_dense_throwing_arm(
    rotations: dict[str, list[float]],
    target_clavicle_component: tuple[float, float, float, float],
    hand_direction: tuple[float, float, float],
    bend_direction: tuple[float, float, float],
    target_elbow_degrees: float,
    target_hand_component: tuple[float, float, float, float],
    reference_upper_component: tuple[float, float, float, float],
    reference_lower_component: tuple[float, float, float, float],
) -> None:
    positions, components, spine_component = _arm_pose(rotations)
    _write_component_rotation(
        rotations,
        "clavicle_r",
        spine_component,
        target_clavicle_component,
    )
    positions, components, _ = _arm_pose(rotations)
    shoulder = positions["upperarm_r"]
    direction = _unit(hand_direction)
    projected_bend = _subtract(
        bend_direction,
        _scale(direction, _dot(bend_direction, direction)),
    )
    if _length(projected_bend) <= 1e-6:
        raise ValueError("dense throwing bend direction is parallel to the hand path")
    bend = _unit(projected_bend)
    upper_length = _length(REFERENCE_LOCAL_TRANSLATIONS_CM["lowerarm_r"])
    lower_length = _length(REFERENCE_LOCAL_TRANSLATIONS_CM["hand_r"])
    target_elbow, target_hand = _target_joint_geometry(
        shoulder,
        _add(shoulder, bend),
        direction,
        upper_length,
        lower_length,
        target_elbow_degrees,
    )

    target_upper_component = v6._multiply(
        v6._shortest_arc(
            v6._rotate(
                reference_upper_component,
                REFERENCE_LOCAL_TRANSLATIONS_CM["lowerarm_r"],
            ),
            _subtract(target_elbow, shoulder),
        ),
        reference_upper_component,
    )
    _write_component_rotation(
        rotations,
        "upperarm_r",
        components["clavicle_r"],
        target_upper_component,
    )
    target_lower_component = v6._multiply(
        v6._shortest_arc(
            v6._rotate(
                reference_lower_component,
                REFERENCE_LOCAL_TRANSLATIONS_CM["hand_r"],
            ),
            _subtract(target_hand, target_elbow),
        ),
        reference_lower_component,
    )
    _write_component_rotation(
        rotations,
        "lowerarm_r",
        target_upper_component,
        target_lower_component,
    )
    target_hand_local = v6._multiply(
        v6._inverse(target_lower_component), target_hand_component,
    )
    target_hand_additive = v6._multiply(
        v6._inverse(v6.REFERENCE_LOCAL_ROTATIONS["hand_r"]),
        target_hand_local,
    )
    bounded_hand_additive = _clamp_quaternion_angle(
        target_hand_additive, MAX_DENSE_THROWING_HAND_ADDITIVE_DEGREES,
    )
    target_hand_component = v6._multiply(
        target_lower_component,
        v6._multiply(
            v6.REFERENCE_LOCAL_ROTATIONS["hand_r"],
            bounded_hand_additive,
        ),
    )
    _write_component_rotation(
        rotations,
        "hand_r",
        target_lower_component,
        target_hand_component,
    )


def _reshape_dense_throwing_arm(family: dict[str, Any]) -> None:
    """Bake a smooth component-space f66-f94 throwing-arm trajectory."""
    poses = {pose["frame"]: pose for pose in family["pose_keys"]}
    base = {
        frame: _arm_pose(poses[frame]["rotation_degrees"])
        for frame in range(family["frame_count"] + 1)
    }
    anchor_frames = (
        *DENSE_THROWING_PRE_RELEASE_ANCHORS,
        DENSE_THROWING_RELEASE_FRAME,
        DENSE_THROWING_RESHAPE_END_FRAME,
    )
    anchors = {
        frame: _dense_arm_anchor(base[frame]) for frame in anchor_frames
    }
    target_elbows = {
        DENSE_THROWING_RESHAPE_START_FRAME: anchors[
            DENSE_THROWING_RESHAPE_START_FRAME
        ]["elbow_degrees"],
        72: 135.0,
        76: 118.0,
        77: 108.0,
        78: 101.0,
        79: 97.0,
        81: 105.0,
        DENSE_THROWING_RELEASE_FRAME: 145.0,
        DENSE_THROWING_RESHAPE_END_FRAME: 165.0,
    }
    release_hand_position = base[DENSE_THROWING_RELEASE_FRAME][0]["hand_r"]
    release_hand_component = base[DENSE_THROWING_RELEASE_FRAME][1]["hand_r"]
    solved_pocket_hand_position: tuple[float, float, float] | None = None

    for frame in range(
        DENSE_THROWING_RESHAPE_START_FRAME,
        DENSE_THROWING_RESHAPE_END_FRAME + 1,
    ):
        if frame <= DENSE_THROWING_PRE_RELEASE_ANCHORS[-1]:
            earlier, later, alpha = _dense_anchor_segment(frame)
            direction_alpha = alpha
            elbow_degrees = (
                target_elbows[earlier]
                + (target_elbows[later] - target_elbows[earlier]) * alpha
            )
            component_alpha = alpha
        elif frame < DENSE_THROWING_RELEASE_FRAME:
            earlier = DENSE_THROWING_PRE_RELEASE_ANCHORS[-1]
            later = DENSE_THROWING_RELEASE_FRAME
            bridge = DENSE_THROWING_BRIDGE[frame]
            direction_alpha = bridge["hand_position_alpha"]
            elbow_degrees = 0.0
            component_alpha = bridge["hand_component_alpha"]
        elif frame == DENSE_THROWING_RELEASE_FRAME:
            earlier = DENSE_THROWING_PRE_RELEASE_ANCHORS[-1]
            later = DENSE_THROWING_RELEASE_FRAME
            direction_alpha = 1.0
            elbow_degrees = target_elbows[frame]
            component_alpha = 1.0
        else:
            earlier = DENSE_THROWING_RELEASE_FRAME
            later = DENSE_THROWING_RESHAPE_END_FRAME
            direction_alpha = (frame - earlier) / (later - earlier)
            elbow_degrees = (
                target_elbows[earlier]
                + (target_elbows[later] - target_elbows[earlier])
                * direction_alpha
            )
            component_alpha = direction_alpha

        reference_alpha = _smoothstep(
            (frame - DENSE_THROWING_RESHAPE_START_FRAME)
            / (
                DENSE_THROWING_RESHAPE_END_FRAME
                - DENSE_THROWING_RESHAPE_START_FRAME
            )
        )
        reference_upper_component = _slerp_quaternion(
            base[DENSE_THROWING_RESHAPE_START_FRAME][1]["upperarm_r"],
            base[DENSE_THROWING_RESHAPE_END_FRAME][1]["upperarm_r"],
            reference_alpha,
        )
        reference_lower_component = _slerp_quaternion(
            base[DENSE_THROWING_RESHAPE_START_FRAME][1]["lowerarm_r"],
            base[DENSE_THROWING_RESHAPE_END_FRAME][1]["lowerarm_r"],
            reference_alpha,
        )
        if frame <= DENSE_THROWING_RELEASE_FRAME:
            clavicle_alpha = _smoothstep(
                (frame - DENSE_THROWING_RESHAPE_START_FRAME)
                / (
                    DENSE_THROWING_RELEASE_FRAME
                    - DENSE_THROWING_RESHAPE_START_FRAME
                )
            )
            clavicle_component = _slerp_quaternion(
                base[DENSE_THROWING_RESHAPE_START_FRAME][1]["clavicle_r"],
                base[DENSE_THROWING_RELEASE_FRAME][1]["clavicle_r"],
                clavicle_alpha,
            )
            bend_twist = (
                anchors[DENSE_THROWING_RESHAPE_START_FRAME][
                    "bend_twist_degrees"
                ]
                + _shortest_angle_delta_degrees(
                    anchors[DENSE_THROWING_RESHAPE_START_FRAME][
                        "bend_twist_degrees"
                    ],
                    anchors[DENSE_THROWING_RELEASE_FRAME][
                        "bend_twist_degrees"
                    ],
                ) * clavicle_alpha
            )
        else:
            clavicle_alpha = _smoothstep(
                (frame - DENSE_THROWING_RELEASE_FRAME)
                / (
                    DENSE_THROWING_RESHAPE_END_FRAME
                    - DENSE_THROWING_RELEASE_FRAME
                )
            )
            clavicle_component = _slerp_quaternion(
                base[DENSE_THROWING_RELEASE_FRAME][1]["clavicle_r"],
                base[DENSE_THROWING_RESHAPE_END_FRAME][1]["clavicle_r"],
                clavicle_alpha,
            )
            bend_twist = (
                anchors[DENSE_THROWING_RELEASE_FRAME][
                    "bend_twist_degrees"
                ]
                + _shortest_angle_delta_degrees(
                    anchors[DENSE_THROWING_RELEASE_FRAME][
                        "bend_twist_degrees"
                    ],
                    anchors[DENSE_THROWING_RESHAPE_END_FRAME][
                        "bend_twist_degrees"
                    ],
                ) * clavicle_alpha
            )
        hand_component = _slerp_quaternion(
            anchors[earlier]["hand_component"],
            anchors[later]["hand_component"],
            component_alpha,
        )
        if DENSE_THROWING_PRE_RELEASE_ANCHORS[-1] < frame < DENSE_THROWING_RELEASE_FRAME:
            if solved_pocket_hand_position is None:
                raise ValueError("dense release bridge lacks the solved f81 hand")
            target_hand = _add(
                _scale(
                    solved_pocket_hand_position,
                    1.0 - direction_alpha,
                ),
                _scale(release_hand_position, direction_alpha),
            )
            clavicle_origin = base[frame][0]["clavicle_r"]
            target_shoulder = _add(
                clavicle_origin,
                v6._rotate(
                    clavicle_component,
                    REFERENCE_LOCAL_TRANSLATIONS_CM["upperarm_r"],
                ),
            )
            hand_direction = _unit(_subtract(target_hand, target_shoulder))
            shoulder_hand_length = _length(
                _subtract(target_hand, target_shoulder)
            )
            upper_length = _length(
                REFERENCE_LOCAL_TRANSLATIONS_CM["lowerarm_r"]
            )
            lower_length = _length(
                REFERENCE_LOCAL_TRANSLATIONS_CM["hand_r"]
            )
            elbow_cosine = (
                upper_length * upper_length
                + lower_length * lower_length
                - shoulder_hand_length * shoulder_hand_length
            ) / (2.0 * upper_length * lower_length)
            if not -1.0 - 1e-6 <= elbow_cosine <= 1.0 + 1e-6:
                raise ValueError("dense release bridge hand target is unreachable")
            elbow_degrees = math.degrees(math.acos(min(
                1.0, max(-1.0, elbow_cosine),
            )))
        else:
            hand_direction = _blend_direction(
                anchors[earlier]["hand_direction"],
                anchors[later]["hand_direction"],
                direction_alpha,
            )
        bend_direction = _bend_from_twist_degrees(
            hand_direction, bend_twist,
        )
        _solve_dense_throwing_arm(
            poses[frame]["rotation_degrees"],
            clavicle_component,
            hand_direction,
            bend_direction,
            elbow_degrees,
            hand_component,
            reference_upper_component,
            reference_lower_component,
        )
        if frame == DENSE_THROWING_PRE_RELEASE_ANCHORS[-1]:
            solved_pocket_hand_position = _arm_pose(
                poses[frame]["rotation_degrees"]
            )[0]["hand_r"]

    final_positions, final_components, _ = _arm_pose(
        poses[DENSE_THROWING_RELEASE_FRAME]["rotation_degrees"]
    )
    hand_error = _length(_subtract(
        final_positions["hand_r"], release_hand_position,
    ))
    hand_rotation_error = v6._angular_distance(
        final_components["hand_r"], release_hand_component,
    )
    if hand_error > 0.0001 or hand_rotation_error > 0.0001:
        raise ValueError(
            "dense release reconstruction exceeded fixed-hand tolerance: "
            f"hand={hand_error:.9f}cm rotation={hand_rotation_error:.9f}deg"
        )


def _replace_version_identity(family: dict[str, Any]) -> None:
    family["motion_id"] = family["motion_id"].replace("_V6", "_V7")
    family["sequence_object_path"] = family["sequence_object_path"].replace(
        "v006", "v007"
    )
    family["montage_object_path"] = family["montage_object_path"].replace(
        "v006", "v007"
    )
    family["style_id"] = family["style_id"].replace("v006", "v007")


def build_v7() -> tuple[dict[str, Any], dict[str, Any]]:
    source = _load(V6_PATH)
    result = copy.deepcopy(source)
    for key, value in {
        "schema": "DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v7",
        "schema_version": 7,
        "recipe_id": "DG_PRODUCTION_PROCEDURAL_MOTION_V7",
        "recipe_version": "v7",
        "asset_revision": "v007",
    }.items():
        result[key] = value

    result["v7_source_motion_design_contract"] = {
        "schema": "DiscGolfTour.ProductionMotionV7SourceDesignContract.v1",
        "source_recipe": "SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json",
        "mutation_scope": (
            "DENSE_DRIVE_BILATERAL_CLAVICLE_UPPERARM_LOWERARM_HAND_ROTATIONS_ONLY"
        ),
        "source_metric_basis": (
            "EXACT_PROBED_DGMASTER_REFERENCE_CHAIN_STATIC_RECONSTRUCTION"
        ),
        "dense_drive_sampling_policy": {
            "frames": [0, 144],
            "frame_rate": 60,
            "required_pose_count": 145,
            "non_arm_source_sampling": (
                "V006_CATMULL_ROM_INTEGER_FRAME_SAMPLES_ROUNDED_6_DECIMALS"
            ),
            "throwing_component_reshape_frames": [66, 94],
            "release_shoulder_circle_sample": 115,
        },
        "support_arm_named_phase_gate": {
            "frames": [44, 64, 84, 118],
            "maximum_reach_ratio": 0.75,
            "minimum_elbow_degrees": 65.0,
            "maximum_elbow_degrees": 105.0,
        },
        "throwing_arm_power_pocket_gate": {
            "frames": [79, 80, 81],
            "minimum_reach_ratio": 0.70,
            "maximum_reach_ratio": 0.82,
            "minimum_elbow_degrees": 85.0,
            "maximum_elbow_degrees": 105.0,
            "maximum_absolute_torso_lateral_cm": 46.0,
            "minimum_torso_forward_cm": -45.0,
            "maximum_torso_forward_cm": -20.0,
            "minimum_torso_vertical_cm": 5.0,
            "maximum_torso_vertical_cm": 25.0,
        },
        "throwing_arm_release_gate": {
            "frame": 84,
            "minimum_reach_ratio": 0.95,
            "maximum_reach_ratio": 0.99,
            "minimum_elbow_degrees": 145.0,
            "maximum_elbow_degrees": 165.0,
        },
        "throwing_arm_followthrough_gate": {
            "frame": 94,
            "minimum_reach_ratio": 0.95,
            "maximum_reach_ratio": 1.0,
            "minimum_elbow_degrees": 155.0,
            "maximum_elbow_degrees": 175.0,
        },
        "throwing_arm_recovery_gate": {
            "frame": 118,
            "maximum_reach_ratio": 0.75,
            "minimum_elbow_degrees": 75.0,
            "maximum_elbow_degrees": 110.0,
            "maximum_absolute_torso_lateral_cm": 35.0,
            "maximum_torso_vertical_cm": -15.0,
        },
        "release_component_grip_invariant": {
            "frame": 84,
            "maximum_translation_error_cm": 0.1,
            "maximum_rotation_error_degrees": 0.1,
            "reference_local_translation_cm": list(
                REFERENCE_DISC_GRIP_TRANSLATION_CM
            ),
            "reference_local_rotation_xyzw": list(
                v6.REFERENCE_DISC_GRIP_ROTATION
            ),
        },
        "release_proximal_stability_gate": {
            "frame": 84,
            "maximum_shoulder_origin_delta_from_v006_cm": 11.5,
            "maximum_clavicle_relative_to_spine_delta_from_v006_degrees": 42.0,
            "maximum_fixed_hand_wrist_correction_degrees": 20.0,
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
            "maximum_hand_translation_cm_per_frame": 12.25,
            "maximum_grip_translation_cm_per_frame": 14.25,
            "maximum_hand_translation_acceleration_cm_per_frame_squared": 10.0,
            "maximum_grip_translation_acceleration_cm_per_frame_squared": 10.25,
            "maximum_torso_local_shoulder_translation_cm_per_frame": 2.25,
            "maximum_torso_local_shoulder_acceleration_cm_per_frame_squared": 3.0,
            "maximum_torso_local_elbow_translation_cm_per_frame": 10.0,
            "maximum_torso_local_elbow_acceleration_cm_per_frame_squared": 8.0,
            "maximum_clavicle_relative_to_spine_log_vector_rate_degrees_per_frame": 8.0,
            "maximum_clavicle_relative_to_spine_log_vector_acceleration_degrees_per_frame_squared": 6.0,
            "maximum_throwing_upperarm_component_log_vector_rate_degrees_per_frame": 18.0,
            "maximum_throwing_upperarm_component_log_vector_acceleration_degrees_per_frame_squared": 15.0,
            "maximum_throwing_lowerarm_component_log_vector_rate_degrees_per_frame": 24.0,
            "maximum_throwing_lowerarm_component_log_vector_acceleration_degrees_per_frame_squared": 18.0,
            "maximum_elbow_delta_degrees_per_frame": 45.0,
            "maximum_elbow_bend_plane_delta_degrees_per_frame": 60.0,
            "maximum_pre_release_disc_plane_delta_degrees_per_frame": 15.0,
            "maximum_throwing_wrist_additive_degrees": 25.0,
            "quaternion_log_vector_convention": (
                "SPATIAL_DELTA_CURRENT_TIMES_INVERSE_PREVIOUS_"
                "SHORTEST_W_NONNEGATIVE_LOG_VECTOR_DEGREES;_"
                "ACCELERATION_IS_NORM_OF_CONSECUTIVE_LOG_VECTOR_DIFFERENCE"
            ),
        },
        "static_source_geometry_approval": False,
        "unreal_assets_authored": False,
        "runtime_source_pose_approval": False,
        "metahuman_deformation_approval": False,
        "human_animation_approval": False,
        "human_disc_contact_approval": False,
        "release_approval": False,
        "status": "CANDIDATE_REQUIRES_FAIL_CLOSED_V7_VALIDATION",
    }

    changed_pose_count = 0
    changed_channel_count = 0
    for family in result["families"]:
        _replace_version_identity(family)
        if family["family"] != "Drive":
            continue
        source_family = next(
            value for value in source["families"] if value["family"] == "Drive"
        )
        source_release_pose = next(
            pose for pose in source_family["pose_keys"]
            if pose["frame"] == source_family["release_frame"]
        )
        source_release_positions, source_release_components, _ = _arm_pose(
            source_release_pose["rotation_degrees"]
        )
        source_release_hand = source_release_positions["hand_r"]
        source_release_hand_component = source_release_components["hand_r"]
        compact_pocket_hand: tuple[float, float, float] | None = None
        compact_pocket_hand_component: (
            tuple[float, float, float, float] | None
        ) = None
        for pose in family["pose_keys"]:
            frame = pose["frame"]
            rotations = pose["rotation_degrees"]
            before = {bone: list(rotations[bone]) for bone in ARM_BONES}
            _solve_arm(
                rotations,
                "l",
                SUPPORT_ELBOW_DEGREES_BY_FRAME[frame],
                SUPPORT_TUCK_BLEND_BY_FRAME[frame],
                SUPPORT_TUCK_DIRECTION_TORSO,
                0.82 - 0.15 * SUPPORT_TUCK_BLEND_BY_FRAME[frame],
            )
            if frame in THROWING_ELBOW_DEGREES_BY_FRAME:
                recovery_blend = THROWING_HIP_RECOVERY_BLEND_BY_FRAME[frame]
                if frame == family["release_frame"]:
                    _solve_arm_to_fixed_hand_transform(
                        rotations, THROWING_ELBOW_DEGREES_BY_FRAME[frame],
                        shoulder_circle_index=(
                            F84_FIXED_HAND_SHOULDER_CIRCLE_INDEX
                        ),
                    )
                elif frame == 83:
                    if (
                        compact_pocket_hand is None
                        or compact_pocket_hand_component is None
                    ):
                        raise ValueError("f83 transition lacks the solved f81 pocket")
                    transition_hand = _add(
                        _scale(
                            compact_pocket_hand,
                            1.0 - F83_TRANSITION_HAND_ALPHA,
                        ),
                        _scale(source_release_hand, F83_TRANSITION_HAND_ALPHA),
                    )
                    transition_hand_component = _nlerp_quaternion(
                        compact_pocket_hand_component,
                        source_release_hand_component,
                        F83_TRANSITION_COMPONENT_ALPHA,
                    )
                    _solve_arm_to_fixed_hand_transform(
                        rotations,
                        THROWING_ELBOW_DEGREES_BY_FRAME[frame],
                        fixed_hand=transition_hand,
                        fixed_hand_component=transition_hand_component,
                        shoulder_circle_index=(
                            F83_FIXED_HAND_SHOULDER_CIRCLE_INDEX
                        ),
                    )
                else:
                    pocket_blend = THROWING_COMPACT_POCKET_BLEND_BY_FRAME.get(
                        frame, 0.0,
                    )
                    if pocket_blend > 0.0:
                        direction_blend = pocket_blend
                        direction_torso = THROWING_COMPACT_POCKET_DIRECTION_TORSO
                    else:
                        direction_blend = recovery_blend
                        direction_torso = THROWING_HIP_DIRECTION_TORSO
                    _solve_arm(
                        rotations,
                        "r",
                        THROWING_ELBOW_DEGREES_BY_FRAME[frame],
                        direction_blend,
                        direction_torso,
                        THROWING_CLAVICLE_FACTOR_OVERRIDE_BY_FRAME.get(
                            frame,
                            0.92
                            - 0.10 * pocket_blend
                            - 0.22 * recovery_blend,
                        ),
                    )
                if frame == 81:
                    pocket_positions, pocket_components, _ = _arm_pose(rotations)
                    compact_pocket_hand = pocket_positions["hand_r"]
                    compact_pocket_hand_component = pocket_components["hand_r"]
            changed = sum(
                before[bone] != rotations[bone] for bone in ARM_BONES
            )
            if changed:
                changed_pose_count += 1
                changed_channel_count += changed

        source_dense_poses = _densify_pose_keys(source_family)
        family["pose_keys"] = _densify_pose_keys(family)
        _reshape_dense_throwing_arm(family)
        source_dense_by_frame = {
            pose["frame"]: pose for pose in source_dense_poses
        }
        changed_pose_count = 0
        changed_channel_count = 0
        for pose in family["pose_keys"]:
            frame = pose["frame"]
            changed = sum(
                pose["rotation_degrees"][bone]
                != source_dense_by_frame[frame]["rotation_degrees"][bone]
                for bone in ARM_BONES
            )
            if changed:
                changed_pose_count += 1
                changed_channel_count += changed

    summary = {
        "changed_family": "Drive",
        "changed_pose_count": changed_pose_count,
        "changed_arm_rotation_channel_count": changed_channel_count,
        "dense_drive_pose_count": 145,
        "preserved_families": ["Approach", "Putt"],
        "runtime_acceptance": False,
        "human_animation_approval": False,
    }
    return result, summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="fail unless the checked-in v007 file matches deterministic output",
    )
    args = parser.parse_args()
    recipe, summary = build_v7()
    encoded = (json.dumps(recipe, indent=2) + "\n").encode("utf-8")
    if args.check:
        if not V7_PATH.is_file() or V7_PATH.read_bytes() != encoded:
            print("v007 recipe differs from deterministic generation", file=sys.stderr)
            return 1
    else:
        V7_PATH.write_bytes(encoded)
    summary.update({
        "output": str(V7_PATH),
        "bytes": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest().upper(),
        "status": "PASS_CHECK" if args.check else "PASS_GENERATED",
    })
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
