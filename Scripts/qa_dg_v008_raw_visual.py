"""Read-only v008 authentic-motion visual QA and pose diagnostics.

Run this with Unreal Editor's PythonScript commandlet or from the editor Python
console.  It loads one quarantined v008 AnimSequence and its evaluation mesh,
samples the runtime-compressed pose at a fixed 60 Hz, and writes only external
QA artifacts under ``C:\\DGTour_TestRuns\\AnimationFluidity``.

The contact sheet is a deterministic front/side skeleton preview.  It is not a
surface render and is never evidence of human animation approval.  The script
does not create, modify, save, retarget, or promote any Unreal asset.

Environment overrides:
  DG_V008_QA_SEQUENCE  quarantined v008 AnimSequence package path
  DG_V008_QA_MESH      matching SkeletalMesh package path
  DG_V008_QA_EXPECTED_SEQUENCE_SHA256 optional fail-closed source binding
  DG_V008_QA_OUTPUT    external output root
  DG_V008_QA_SELF_TEST set to 1 to run pure helper tests before asset sampling
"""

from __future__ import annotations

import binascii
import datetime as _datetime
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import uuid
import zlib

import unreal


SCHEMA = "DiscGolfTour.V008RawVisualQA.v1"
DEFAULT_SEQUENCE = (
    "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/"
    "AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer"
)
DEFAULT_MESH = (
    "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic/"
    "SK_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer"
)
DEFAULT_OUTPUT = r"C:\DGTour_TestRuns\AnimationFluidity"
RATE = 60.0

SPINE = (
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04",
    "spine_05", "neck_01", "neck_02", "head",
)
LEFT_ARM = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l")
RIGHT_ARM = ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")
LEFT_LEG = ("pelvis", "thigh_l", "calf_l", "foot_l", "ball_l")
RIGHT_LEG = ("pelvis", "thigh_r", "calf_r", "foot_r", "ball_r")
SAMPLE_BONES = tuple(dict.fromkeys(SPINE + LEFT_ARM + RIGHT_ARM + LEFT_LEG + RIGHT_LEG))
MOTION_BONES = (
    "root", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r",
)
REQUIRED_BONES = (
    "root", "pelvis", "spine_03", "head", "upperarm_r", "lowerarm_r",
    "hand_r", "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r",
    "foot_r",
)

CONNECTIONS = (
    ("root", "pelvis", "center"),
    ("pelvis", "spine_01", "center"),
    ("spine_01", "spine_02", "center"),
    ("spine_02", "spine_03", "center"),
    ("spine_03", "spine_04", "center"),
    ("spine_04", "spine_05", "center"),
    ("spine_05", "neck_01", "center"),
    ("neck_01", "neck_02", "center"),
    ("neck_02", "head", "center"),
    ("spine_05", "clavicle_l", "left"),
    ("clavicle_l", "upperarm_l", "left"),
    ("upperarm_l", "lowerarm_l", "left"),
    ("lowerarm_l", "hand_l", "left"),
    ("spine_05", "clavicle_r", "right"),
    ("clavicle_r", "upperarm_r", "right"),
    ("upperarm_r", "lowerarm_r", "right"),
    ("lowerarm_r", "hand_r", "right"),
    ("pelvis", "thigh_l", "left"),
    ("thigh_l", "calf_l", "left"),
    ("calf_l", "foot_l", "left"),
    ("foot_l", "ball_l", "left"),
    ("pelvis", "thigh_r", "right"),
    ("thigh_r", "calf_r", "right"),
    ("calf_r", "foot_r", "right"),
    ("foot_r", "ball_r", "right"),
)

COLORS = {
    "background": (12, 17, 25),
    "panel": (21, 29, 41),
    "grid": (48, 61, 78),
    "center": (226, 232, 240),
    "left": (75, 157, 244),
    "right": (245, 142, 65),
    "root": (255, 214, 92),
    "text": (235, 241, 248),
    "muted": (137, 151, 169),
}


FONT = {
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "F": ("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01111"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("11111", "00100", "00100", "00100", "00100", "00100", "11111"),
    "J": ("00111", "00010", "00010", "00010", "10010", "10010", "01100"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "Q": ("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "10101", "01010"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    "6": ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
    "-": ("00000", "00000", "00000", "11111", "00000", "00000", "00000"),
    ".": ("00000", "00000", "00000", "00000", "00000", "00110", "00110"),
    ":": ("00000", "00110", "00110", "00000", "00110", "00110", "00000"),
    "/": ("00001", "00010", "00100", "01000", "10000", "00000", "00000"),
    "_": ("00000", "00000", "00000", "00000", "00000", "00000", "11111"),
    " ": ("00000",) * 7,
}


def _utc_now() -> str:
    return _datetime.datetime.now(_datetime.timezone.utc).isoformat()


def _distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _horizontal_distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def _percentile(values: list[float], fraction: float) -> float:
    if not values:
        raise ValueError("percentile requires values")
    ordered = sorted(values)
    location = max(0.0, min(1.0, fraction)) * (len(ordered) - 1)
    low = int(math.floor(location))
    high = int(math.ceil(location))
    alpha = location - low
    return ordered[low] * (1.0 - alpha) + ordered[high] * alpha


def _segments(mask: list[bool], minimum_frames: int, gap_frames: int = 0) -> list[tuple[int, int]]:
    raw: list[tuple[int, int]] = []
    start = None
    for index, value in enumerate(mask + [False]):
        if value and start is None:
            start = index
        elif not value and start is not None:
            raw.append((start, index - 1))
            start = None
    merged: list[tuple[int, int]] = []
    for first, last in raw:
        if merged and first - merged[-1][1] - 1 <= gap_frames:
            merged[-1] = (merged[-1][0], last)
        else:
            merged.append((first, last))
    return [(first, last) for first, last in merged if last - first + 1 >= minimum_frames]


def _asset_file(package_path: str) -> Path | None:
    if not package_path.startswith("/Game/"):
        return None
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
    return content / (package_path[len("/Game/"):] + ".uasset")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _quarantine_path(path: str, label: str) -> None:
    normalized = path.replace("\\", "/")
    if "/ProductionMotion/" in normalized or "v006" in normalized:
        raise RuntimeError(f"{label} points at an active/production asset: {path}")
    if label == "sequence":
        if not normalized.startswith("/Game/DiscGolf/Animation/Authentic/v008/"):
            raise RuntimeError(f"{label} is outside the quarantined v008 authentic root: {path}")
        return
    allowed_read_only_meshes = {
        "/Game/DiscGolf/Characters/Meshes/SK_DG_Master",
    }
    if (
        not normalized.startswith("/Game/DiscGolf/Animation/Authentic/v008/")
        and normalized not in allowed_read_only_meshes
    ):
        raise RuntimeError(f"{label} is not an approved read-only evaluation mesh: {path}")


def _pose_options(mesh: unreal.SkeletalMesh) -> unreal.AnimPoseEvaluationOptions:
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)
    options.set_editor_property("optional_skeletal_mesh", mesh)
    options.set_editor_property("should_retarget", False)
    # Keep authored root translation in the evaluated pose.  AnimPose WORLD is
    # component space, so no separate root translation is added later.
    options.set_editor_property("extract_root_motion", False)
    options.set_editor_property("incorporate_root_motion_into_pose", True)
    options.set_editor_property("evaluate_curves", False)
    return options


def _transform_record(transform: unreal.Transform) -> dict[str, list[float]]:
    location = transform.translation
    rotation = transform.rotation
    scale = transform.scale3d
    norm = math.sqrt(
        float(rotation.x) ** 2 + float(rotation.y) ** 2
        + float(rotation.z) ** 2 + float(rotation.w) ** 2
    )
    if norm <= 1e-8:
        raise RuntimeError("pose contained a zero quaternion")
    return {
        "translation_cm": [float(location.x), float(location.y), float(location.z)],
        "rotation_xyzw": [
            float(rotation.x) / norm, float(rotation.y) / norm,
            float(rotation.z) / norm, float(rotation.w) / norm,
        ],
        "scale_xyz": [float(scale.x), float(scale.y), float(scale.z)],
    }


def _sample(
    sequence: unreal.AnimSequence,
    mesh: unreal.SkeletalMesh,
) -> tuple[list[dict[str, object]], list[str], list[str], float]:
    duration = float(sequence.get_play_length())
    intervals = round(duration * RATE)
    if intervals <= 0 or abs(duration * RATE - intervals) > 1e-3:
        raise RuntimeError(
            f"sequence duration {duration:.9f}s is not endpoint-aligned to {RATE:g} Hz"
        )
    options = _pose_options(mesh)
    frames: list[dict[str, object]] = []
    known_bones: list[str] | None = None
    known_sockets: list[str] | None = None
    available_sample_bones: tuple[str, ...] | None = None
    for index in range(intervals + 1):
        time_seconds = index / RATE
        pose = sequence.get_anim_pose_at_time(time_seconds, options)
        if not pose.is_valid():
            raise RuntimeError(f"invalid evaluated pose at frame {index} ({time_seconds:.6f}s)")
        if known_bones is None:
            known_bones = sorted(str(value) for value in pose.get_bone_names())
            known_sockets = sorted(str(value) for value in pose.get_socket_names())
            missing = sorted(set(REQUIRED_BONES) - set(known_bones))
            if missing:
                raise RuntimeError(f"evaluation mesh lacks required bones: {missing}")
            available_sample_bones = tuple(name for name in SAMPLE_BONES if name in set(known_bones))
        transforms = {
            name: _transform_record(
                pose.get_bone_pose(name, unreal.AnimPoseSpaces.WORLD)
            )
            for name in available_sample_bones or ()
        }
        frames.append({"frame": index, "time_seconds": time_seconds, "transforms": transforms})
    return frames, known_bones or [], known_sockets or [], duration


def _positions(frame: dict[str, object]) -> dict[str, tuple[float, float, float]]:
    transforms = frame["transforms"]
    return {
        name: tuple(float(value) for value in record["translation_cm"])
        for name, record in transforms.items()
    }


def _speed_series(
    frames: list[dict[str, object]],
    bones: tuple[str, ...],
    horizontal: bool = False,
) -> list[float]:
    output = [0.0]
    prior = _positions(frames[0])
    for frame in frames[1:]:
        current = _positions(frame)
        values = []
        for bone in bones:
            if bone not in prior or bone not in current:
                continue
            distance = (
                _horizontal_distance(prior[bone], current[bone])
                if horizontal else _distance(prior[bone], current[bone])
            )
            values.append(distance * RATE)
        output.append(sum(values) / len(values) if values else 0.0)
        prior = current
    return output


def _infer_phases(
    frames: list[dict[str, object]],
) -> tuple[dict[str, int], dict[str, object], list[float]]:
    motion = _speed_series(frames, MOTION_BONES)
    if max(motion) <= 1e-6:
        raise RuntimeError("all sampled poses are stationary; throw phase inference is impossible")
    # A short centered envelope prevents one-frame retarget/compression spikes from
    # setting a threshold that the genuine throw cannot sustain.  P95 is used as
    # the high reference while the unsmoothed peak remains visible in diagnostics.
    radius = max(1, int(round(0.05 * RATE)))
    envelope = [
        sum(motion[max(0, index - radius):min(len(motion), index + radius + 1)])
        / (min(len(motion), index + radius + 1) - max(0, index - radius))
        for index in range(len(motion))
    ]
    baseline = _percentile(envelope, 0.20)
    high_reference = _percentile(envelope, 0.95)
    peak = max(motion)
    threshold = max(5.0, baseline + 0.15 * (high_reference - baseline))
    active_segments = _segments(
        [value >= threshold for value in envelope],
        minimum_frames=max(12, int(0.20 * RATE)),
        gap_frames=int(0.20 * RATE),
    )
    fallback_reason = None
    if not active_segments:
        # Preserve a useful, explicitly provisional preview for unusually sparse
        # motion by taking the interval containing 96% of motion energy.  This is
        # diagnostic-only and is promoted to a review flag below.
        energy = [max(0.0, value - baseline) for value in envelope]
        total_energy = sum(energy)
        if total_energy <= 1e-6:
            raise RuntimeError(
                f"no active-motion energy at threshold {threshold:.3f} cm/s"
            )
        lower_energy = total_energy * 0.02
        upper_energy = total_energy * 0.98
        cumulative = 0.0
        first = 0
        last = len(frames) - 1
        for index, value in enumerate(energy):
            cumulative += value
            if cumulative >= lower_energy:
                first = index
                break
        cumulative = 0.0
        for index, value in enumerate(energy):
            cumulative += value
            if cumulative >= upper_energy:
                last = index
                break
        fallback_reason = "NO_STABLE_PRIMARY_INTERVAL_USED_96_PERCENT_MOTION_ENERGY"
    else:
        first, last = max(
            active_segments,
            key=lambda item: sum(envelope[item[0]:item[1] + 1]),
        )
    first = max(0, first - int(0.20 * RATE))
    last = min(len(frames) - 1, last + int(0.35 * RATE))
    if last - first < int(0.75 * RATE):
        raise RuntimeError("active-motion interval is shorter than 0.75 seconds")

    span = last - first
    hand_speed = _speed_series(frames, ("hand_r",))
    foot_speed = _speed_series(frames, ("foot_r",), horizontal=True)
    reach_lo = first + int(0.18 * span)
    reach_hi = first + int(0.55 * span)
    reachback_raw = max(
        range(reach_lo, max(reach_lo + 1, reach_hi + 1)),
        key=lambda index: _horizontal_distance(
            _positions(frames[index])["hand_r"], _positions(frames[index])["pelvis"]
        ),
    )
    release_lo = min(last - 1, reachback_raw + 1)
    release_hi = max(release_lo, first + int(0.82 * span))
    release_raw = max(
        range(release_lo, min(last, release_hi) + 1),
        key=lambda index: hand_speed[index],
    )
    plant_lo = max(reachback_raw + 1, release_raw - max(2, int(0.18 * span)))
    plant_hi = max(plant_lo, release_raw - 1)
    plant_raw = min(
        range(plant_lo, plant_hi + 1),
        key=lambda index: foot_speed[index],
    )
    candidates = {
        "address": first,
        "runup": first + int(0.15 * span),
        "reachback": reachback_raw,
        "plant": plant_raw,
        "release": release_raw,
        "followthrough": min(last, release_raw + max(2, int(0.14 * span))),
        "recovery": last,
    }
    names = tuple(candidates)
    resolved: dict[str, int] = {}
    previous = first - 1
    for offset, name in enumerate(names):
        remaining = len(names) - offset - 1
        upper = last - remaining
        value = max(previous + 1, min(upper, candidates[name]))
        resolved[name] = value
        previous = value
    diagnostics = {
        "method": "PROVISIONAL_HEURISTIC_REVIEW_ONLY",
        "active_window": {"start_frame": first, "end_frame": last},
        "motion_threshold_cm_per_s": threshold,
        "motion_baseline_p20_cm_per_s": baseline,
        "motion_envelope_p95_cm_per_s": high_reference,
        "motion_peak_cm_per_s": peak,
        "motion_envelope_radius_frames": radius,
        "fallback_reason": fallback_reason,
        "raw_candidates": candidates,
        "resolved_ordered_frames": resolved,
        "release_hand_speed_cm_per_s": hand_speed[resolved["release"]],
        "plant_foot_horizontal_speed_cm_per_s": foot_speed[resolved["plant"]],
    }
    return resolved, diagnostics, motion


def _diagnostics(
    frames: list[dict[str, object]],
    phases: dict[str, int],
    phase_diagnostics: dict[str, object],
    motion: list[float],
    sequence: unreal.AnimSequence,
) -> dict[str, object]:
    active = phase_diagnostics["active_window"]
    first, last = int(active["start_frame"]), int(active["end_frame"])
    exact_step = []
    for index in range(1, len(frames)):
        prior, current = _positions(frames[index - 1]), _positions(frames[index])
        deltas = [
            _distance(prior[bone], current[bone])
            for bone in MOTION_BONES if bone in prior and bone in current
        ]
        exact_step.append(max(deltas) if deltas else 0.0)
    frozen_segments = _segments(
        [False] + [value <= 0.01 for value in exact_step],
        minimum_frames=max(3, int(0.10 * RATE)),
    )
    low_motion_segments = _segments(
        [value <= 3.0 for value in motion],
        minimum_frames=max(6, int(0.25 * RATE)),
        gap_frames=2,
    )

    root = [_positions(frame)["root"] for frame in frames]
    pelvis = [_positions(frame)["pelvis"] for frame in frames]
    active_root = root[first:last + 1]
    active_pelvis = pelvis[first:last + 1]
    root_path_cm = sum(
        _horizontal_distance(active_root[index - 1], active_root[index])
        for index in range(1, len(active_root))
    )
    root_displacement_cm = _horizontal_distance(active_root[0], active_root[-1])
    root_z_range_cm = max(value[2] for value in active_root) - min(value[2] for value in active_root)
    relative = [
        tuple(pelvis_value[axis] - root_value[axis] for axis in range(3))
        for pelvis_value, root_value in zip(active_pelvis, active_root)
    ]
    relative_drift_cm = max(
        _distance(relative[0], value) for value in relative
    )
    relative_distances_cm = [
        math.sqrt(sum(component * component for component in value))
        for value in relative
    ]
    relative_distance_min_cm = min(relative_distances_cm)
    relative_distance_max_cm = max(relative_distances_cm)
    relative_distance_mean_cm = sum(relative_distances_cm) / len(relative_distances_cm)
    relative_distance_range_cm = relative_distance_max_cm - relative_distance_min_cm

    root_motion_properties: dict[str, object] = {}
    unavailable_root_motion_properties = []
    for property_name in (
        "enable_root_motion",
        "root_motion_root_lock",
        "force_root_lock",
        "use_normalized_root_motion_scale",
    ):
        try:
            value = sequence.get_editor_property(property_name)
            root_motion_properties[property_name] = (
                value if isinstance(value, (bool, int, float, str)) or value is None
                else str(value)
            )
        except Exception:
            unavailable_root_motion_properties.append(property_name)
    root_speed = _speed_series(frames, ("root",))
    pelvis_speed = _speed_series(frames, ("pelvis",))

    limb_edges = (
        ("upperarm_l", "lowerarm_l"), ("lowerarm_l", "hand_l"),
        ("upperarm_r", "lowerarm_r"), ("lowerarm_r", "hand_r"),
        ("thigh_l", "calf_l"), ("calf_l", "foot_l"),
        ("thigh_r", "calf_r"), ("calf_r", "foot_r"),
    )
    limb_stability = {}
    for parent, child in limb_edges:
        lengths = [
            _distance(_positions(frame)[parent], _positions(frame)[child])
            for frame in frames[first:last + 1]
            if parent in _positions(frame) and child in _positions(frame)
        ]
        if not lengths:
            continue
        mean = sum(lengths) / len(lengths)
        deviation = max(abs(value - mean) for value in lengths) / mean if mean > 1e-6 else math.inf
        limb_stability[f"{parent}->{child}"] = {
            "mean_length_cm": mean,
            "maximum_relative_deviation": deviation,
        }
    maximum_limb_deviation = max(
        (value["maximum_relative_deviation"] for value in limb_stability.values()),
        default=0.0,
    )
    evaluated_vertical_extents = []
    evaluated_head_to_foot_lengths = []
    evaluated_upright_ratios = []
    evaluated_body_spans = []
    for frame in frames[first:last + 1]:
        pose = _positions(frame)
        if "head" not in pose:
            continue
        feet = [pose[bone] for bone in ("foot_l", "foot_r") if bone in pose]
        if feet:
            foot_center = tuple(
                sum(value[axis] for value in feet) / len(feet) for axis in range(3)
            )
            head_to_foot = _distance(pose["head"], foot_center)
            vertical_delta = pose["head"][2] - foot_center[2]
            evaluated_vertical_extents.append(vertical_delta)
            evaluated_head_to_foot_lengths.append(head_to_foot)
            evaluated_upright_ratios.append(
                abs(vertical_delta) / head_to_foot if head_to_foot > 1e-6 else 0.0
            )
        body_values = [value for bone, value in pose.items() if bone != "root"]
        if body_values:
            spans = [
                max(value[axis] for value in body_values)
                - min(value[axis] for value in body_values)
                for axis in range(3)
            ]
            evaluated_body_spans.append(math.sqrt(sum(value * value for value in spans)))
    transform_scales = [
        component
        for frame in frames[first:last + 1]
        for record in frame["transforms"].values()
        for component in record["scale_xyz"]
    ]
    review_flags = []
    if phase_diagnostics.get("fallback_reason"):
        review_flags.append(str(phase_diagnostics["fallback_reason"]))
    if any((last_frame - first_frame + 1) / RATE >= 0.20 for first_frame, last_frame in frozen_segments):
        review_flags.append("FROZEN_POSE_RUN_AT_LEAST_0_20S")
    if max(root_speed[first:last + 1]) > 1000.0:
        review_flags.append("ROOT_SPEED_EXCEEDS_1000_CM_PER_S")
    if relative_distance_range_cm > 2.0:
        review_flags.append("ROOT_TO_PELVIS_DISTANCE_RANGE_EXCEEDS_2_CM_REVIEW_LOCAL_TRANSLATION")
    if maximum_limb_deviation > 0.02:
        review_flags.append("LIMB_LENGTH_VARIATION_EXCEEDS_2_PERCENT")
    mean_body_length = (
        sum(evaluated_head_to_foot_lengths) / len(evaluated_head_to_foot_lengths)
        if evaluated_head_to_foot_lengths else 0.0
    )
    mean_upright_ratio = (
        sum(evaluated_upright_ratios) / len(evaluated_upright_ratios)
        if evaluated_upright_ratios else 0.0
    )
    if mean_body_length < 50.0 or mean_body_length > 300.0:
        review_flags.append("EVALUATED_BODY_SCALE_OUTSIDE_50_300_CM")
    if mean_upright_ratio < 0.50:
        review_flags.append("EVALUATED_BODY_AXIS_NOT_Z_UP")
    return {
        "classification": "TECHNICAL_DIAGNOSTIC_NOT_HUMAN_APPROVAL",
        "phase_inference": phase_diagnostics,
        "phases": {
            name: {"frame": frame, "time_seconds": frame / RATE}
            for name, frame in phases.items()
        },
        "held_frame_analysis": {
            "frozen_step_threshold_cm": 0.01,
            "low_motion_threshold_cm_per_s": 3.0,
            "frozen_segments": [
                {"start_frame": a, "end_frame": b, "duration_seconds": (b - a + 1) / RATE}
                for a, b in frozen_segments
            ],
            "low_motion_segments": [
                {"start_frame": a, "end_frame": b, "duration_seconds": (b - a + 1) / RATE}
                for a, b in low_motion_segments
            ],
        },
        "root_pelvis_drift": {
            "space": "ANIMPOSE_WORLD_IS_COMPONENT_SPACE",
            "root_horizontal_path_cm_active": root_path_cm,
            "root_horizontal_displacement_cm_active": root_displacement_cm,
            "root_vertical_range_cm_active": root_z_range_cm,
            "root_peak_speed_cm_per_s_active": max(root_speed[first:last + 1]),
            "pelvis_peak_speed_cm_per_s_active": max(pelvis_speed[first:last + 1]),
            "pelvis_relative_to_root_maximum_drift_cm_active": relative_drift_cm,
            "root_to_pelvis_component_vector_change": {
                "maximum_delta_from_active_start_cm": relative_drift_cm,
                "rotation_sensitive": True,
                "interpretation": (
                    "The component-space vector changes when the root rotates; this metric alone "
                    "is not evidence of duplicate translation authority."
                ),
            },
            "root_to_pelvis_distance_magnitude": {
                "minimum_cm_active": relative_distance_min_cm,
                "mean_cm_active": relative_distance_mean_cm,
                "maximum_cm_active": relative_distance_max_cm,
                "range_cm_active": relative_distance_range_cm,
                "rotation_invariant": True,
                "fixed_local_offset_expectation": (
                    "Magnitude remains constant under pure root rotation with a fixed local "
                    "root-to-pelvis offset; variation indicates pelvis local translation."
                ),
            },
            "translation_authority_context": {
                "desired_policy": (
                    "Root owns accumulated locomotion translation. Pelvis may rotate and carry "
                    "bounded local gait/body motion, but must not independently accumulate a "
                    "second world-translation track."
                ),
                "asset_root_motion_properties": root_motion_properties,
                "unavailable_asset_properties": unavailable_root_motion_properties,
                "classification": "REVIEW_CONTEXT_NOT_AUTOMATIC_DUPLICATE_TRANSLATION_FAILURE",
            },
            "root_track_caveat": (
                "Root extraction is disabled; authored root is already incorporated into every "
                "component-space bone. Do not add it a second time. In-place source data cannot "
                "recover absent world locomotion."
            ),
        },
        "limb_length_stability": {
            "maximum_relative_deviation": maximum_limb_deviation,
            "segments": limb_stability,
        },
        "evaluated_pose_scale": {
            "declared_units": "UNREAL_CENTIMETERS",
            "head_to_foot_distance_mean_cm_active": mean_body_length,
            "head_to_foot_distance_min_cm_active": min(evaluated_head_to_foot_lengths) if evaluated_head_to_foot_lengths else None,
            "head_to_foot_distance_max_cm_active": max(evaluated_head_to_foot_lengths) if evaluated_head_to_foot_lengths else None,
            "head_to_foot_vertical_delta_mean_cm_active": (
                sum(evaluated_vertical_extents) / len(evaluated_vertical_extents)
                if evaluated_vertical_extents else None
            ),
            "head_to_foot_z_upright_ratio_mean_active": mean_upright_ratio,
            "body_bounding_diagonal_mean_cm_active": (
                sum(evaluated_body_spans) / len(evaluated_body_spans)
                if evaluated_body_spans else None
            ),
            "transform_scale_component_min": min(transform_scales) if transform_scales else None,
            "transform_scale_component_max": max(transform_scales) if transform_scales else None,
            "contact_sheet_auto_fit": True,
        },
        "review_flags": review_flags,
        "automatic_acceptance": False,
    }


class _Canvas:
    def __init__(self, width: int, height: int, color: tuple[int, int, int]):
        self.width = width
        self.height = height
        self.pixels = bytearray(color * (width * height))

    def pixel(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            index = (y * self.width + x) * 3
            self.pixels[index:index + 3] = bytes(color)

    def rectangle(self, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]) -> None:
        for y in range(max(0, y0), min(self.height, y1)):
            start = (y * self.width + max(0, x0)) * 3
            count = max(0, min(self.width, x1) - max(0, x0))
            self.pixels[start:start + count * 3] = bytes(color) * count

    def circle(self, cx: int, cy: int, radius: int, color: tuple[int, int, int]) -> None:
        for y in range(cy - radius, cy + radius + 1):
            span = int(math.sqrt(max(0, radius * radius - (y - cy) ** 2)))
            for x in range(cx - span, cx + span + 1):
                self.pixel(x, y, color)

    def line(self, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int], width: int = 2) -> None:
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        error = dx + dy
        while True:
            self.circle(x0, y0, max(1, width // 2), color)
            if x0 == x1 and y0 == y1:
                break
            twice = 2 * error
            if twice >= dy:
                error += dy
                x0 += sx
            if twice <= dx:
                error += dx
                y0 += sy

    def text(self, x: int, y: int, value: str, color: tuple[int, int, int], scale: int = 2) -> None:
        cursor = x
        for character in value.upper():
            glyph = FONT.get(character, FONT[" "])
            for row, bits in enumerate(glyph):
                for column, bit in enumerate(bits):
                    if bit == "1":
                        self.rectangle(
                            cursor + column * scale,
                            y + row * scale,
                            cursor + (column + 1) * scale,
                            y + (row + 1) * scale,
                            color,
                        )
            cursor += 6 * scale

    def write_png(self, path: Path) -> None:
        def chunk(kind: bytes, payload: bytes) -> bytes:
            return (
                struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
            )
        rows = bytearray()
        stride = self.width * 3
        for y in range(self.height):
            rows.append(0)
            start = y * stride
            rows.extend(self.pixels[start:start + stride])
        data = b"\x89PNG\r\n\x1a\n"
        data += chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 2, 0, 0, 0))
        data += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
        data += chunk(b"IEND", b"")
        path.write_bytes(data)


def _contact_sheet(
    frames: list[dict[str, object]],
    phases: dict[str, int],
    path: Path,
) -> dict[str, object]:
    names = tuple(phases)
    cell_width, cell_height = 320, 470
    header = 64
    canvas = _Canvas(cell_width * len(names), header + cell_height * 2, COLORS["background"])
    canvas.text(16, 16, "V008 RAW POSE QA - FRONT YZ / SIDE XZ", COLORS["text"], 2)
    canvas.text(16, 38, "AUTO-FIT SKELETON PREVIEW - SEE SCALE DIAGNOSTIC", COLORS["muted"], 1)
    selected = [_positions(frames[phases[name]]) for name in names]
    centered: list[dict[str, tuple[float, float, float]]] = []
    for pose in selected:
        root = pose["root"]
        centered.append({
            bone: (value[0] - root[0], value[1] - root[1], value[2] - root[2])
            for bone, value in pose.items()
        })
    horizontal_extent = max(
        [0.01]
        + [abs(value[0]) for pose in centered for value in pose.values()]
        + [abs(value[1]) for pose in centered for value in pose.values()]
    )
    z_values = [value[2] for pose in centered for value in pose.values()]
    z_min, z_max = min(z_values), max(z_values)
    scale = min(
        (cell_width - 38) / (2.0 * horizontal_extent),
        (cell_height - 74) / max(0.01, z_max - z_min),
    )
    projections = ((1, "FRONT"), (0, "SIDE"))
    for row, (horizontal_axis, view_name) in enumerate(projections):
        for column, name in enumerate(names):
            x0, y0 = column * cell_width, header + row * cell_height
            canvas.rectangle(x0 + 3, y0 + 3, x0 + cell_width - 3, y0 + cell_height - 3, COLORS["panel"])
            canvas.text(x0 + 12, y0 + 12, name, COLORS["text"], 2)
            frame_index = phases[name]
            canvas.text(x0 + 12, y0 + 32, f"{view_name} F{frame_index}", COLORS["muted"], 1)
            pose = centered[column]
            center_x = x0 + cell_width // 2
            floor_y = y0 + cell_height - 34
            ground_relative = min(
                (pose[bone][2] for bone in ("foot_l", "foot_r", "ball_l", "ball_r") if bone in pose),
                default=z_min,
            )
            ground_y = int(floor_y - (ground_relative - z_min) * scale)
            canvas.line(x0 + 12, ground_y, x0 + cell_width - 12, ground_y, COLORS["grid"], 1)

            def point(bone: str) -> tuple[int, int]:
                value = pose[bone]
                return (
                    int(center_x + value[horizontal_axis] * scale),
                    int(floor_y - (value[2] - z_min) * scale),
                )

            for parent, child, group in CONNECTIONS:
                if parent in pose and child in pose:
                    a, b = point(parent), point(child)
                    canvas.line(a[0], a[1], b[0], b[1], COLORS[group], 4)
            for bone in pose:
                x, y = point(bone)
                color = COLORS["root"] if bone in ("root", "pelvis") else COLORS["center"]
                canvas.circle(x, y, 4 if bone in ("root", "pelvis", "head") else 3, color)
    canvas.write_png(path)
    return {
        "render_type": "DETERMINISTIC_SKELETON_CONTACT_SHEET",
        "surface_render": False,
        "auto_fit_scale": True,
        "width": canvas.width,
        "height": canvas.height,
        "views": ["FRONT_YZ", "SIDE_XZ"],
        "phase_order": list(names),
        "sha256": _sha256(path),
    }


def _write_json(path: Path, payload: object) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _self_test() -> None:
    assert abs(_percentile([0.0, 10.0], 0.5) - 5.0) < 1e-9
    assert _segments([False, True, True, False], 2) == [(1, 2)]
    assert _segments([True, False, True], 2, 1) == [(0, 2)]
    canvas = _Canvas(8, 8, (0, 0, 0))
    canvas.line(0, 0, 7, 7, (255, 255, 255), 1)
    assert any(canvas.pixels)


def main() -> None:
    if os.environ.get("DG_V008_QA_SELF_TEST") == "1":
        _self_test()
    sequence_path = os.environ.get("DG_V008_QA_SEQUENCE", DEFAULT_SEQUENCE)
    mesh_path = os.environ.get("DG_V008_QA_MESH", DEFAULT_MESH)
    expected_sequence_sha256 = os.environ.get(
        "DG_V008_QA_EXPECTED_SEQUENCE_SHA256", ""
    ).strip().lower()
    output_root = Path(os.environ.get("DG_V008_QA_OUTPUT", DEFAULT_OUTPUT))
    run_id = (
        "V008RawVisualQA-"
        + _datetime.datetime.now(_datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
        + str(uuid.uuid4())
    )
    run_dir = output_root / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    receipt_path = run_dir / "receipt.json"
    receipt: dict[str, object] = {
        "schema": SCHEMA,
        "run_id": run_id,
        "started_at_utc": _utc_now(),
        "status": "STARTED",
        "sequence_asset": sequence_path,
        "mesh_asset": mesh_path,
        "writes_are_external_only": True,
        "unreal_assets_modified": False,
        "promotion_authorized": False,
        "human_animation_approved": False,
    }
    _write_json(receipt_path, receipt)
    try:
        _quarantine_path(sequence_path, "sequence")
        _quarantine_path(mesh_path, "mesh")
        sequence_file = _asset_file(sequence_path)
        if sequence_file is None or not sequence_file.is_file():
            raise RuntimeError(f"cannot bind sequence package to a regular uasset: {sequence_path}")
        actual_sequence_sha256 = _sha256(sequence_file)
        if (
            expected_sequence_sha256
            and actual_sequence_sha256 != expected_sequence_sha256
        ):
            raise RuntimeError(
                "sequence SHA-256 mismatch: "
                f"expected={expected_sequence_sha256} actual={actual_sequence_sha256}"
            )
        sequence = unreal.load_asset(sequence_path)
        mesh = unreal.load_asset(mesh_path)
        if not isinstance(sequence, unreal.AnimSequence):
            raise RuntimeError(f"RAW AnimSequence missing or wrong class: {sequence_path}")
        if not isinstance(mesh, unreal.SkeletalMesh):
            raise RuntimeError(f"evaluation SkeletalMesh missing or wrong class: {mesh_path}")
        frames, bone_names, socket_names, duration = _sample(sequence, mesh)
        phases, phase_diagnostics, motion = _infer_phases(frames)
        diagnostics = _diagnostics(frames, phases, phase_diagnostics, motion, sequence)
        diagnostics.update({
            "schema": SCHEMA,
            "sequence_asset": sequence_path,
            "mesh_asset": mesh_path,
            "frame_rate": RATE,
            "sample_count": len(frames),
            "duration_seconds": duration,
            "bone_count": len(bone_names),
            "socket_names": socket_names,
        })
        diagnostics_path = run_dir / "pose_diagnostics.json"
        _write_json(diagnostics_path, diagnostics)
        sheet_path = run_dir / "contact_sheet.png"
        sheet = _contact_sheet(frames, phases, sheet_path)

        source_assets = []
        for label, package in (("sequence", sequence_path), ("mesh", mesh_path)):
            file_path = _asset_file(package)
            if file_path is None or not file_path.is_file():
                raise RuntimeError(f"cannot bind {label} package to a regular uasset: {package}")
            source_assets.append({
                "role": label,
                "package": package,
                "file": str(file_path),
                "bytes": file_path.stat().st_size,
                "sha256": _sha256(file_path),
            })
        receipt.update({
            "status": "PASS_TECHNICAL_QA_ARTIFACTS_WRITTEN",
            "finished_at_utc": _utc_now(),
            "source_assets": source_assets,
            "expected_sequence_sha256": expected_sequence_sha256 or None,
            "sampling": {
                "evaluation_type": "COMPRESSED",
                "frame_rate": RATE,
                "sample_count": len(frames),
                "duration_seconds": duration,
                "space": "ANIMPOSE_WORLD_IS_COMPONENT_SPACE",
                "extract_root_motion": False,
                "incorporate_root_motion_into_pose": True,
            },
            "phases": diagnostics["phases"],
            "contact_sheet": {"path": str(sheet_path), **sheet},
            "diagnostics": {
                "path": str(diagnostics_path),
                "bytes": diagnostics_path.stat().st_size,
                "sha256": _sha256(diagnostics_path),
                "review_flags": diagnostics["review_flags"],
            },
            "limitations": [
                "contact sheet is a skeleton preview, not a shaded character render",
                "phase labels are provisional heuristic review points",
                "technical output does not approve animation, contact, rights, or promotion",
            ],
        })
        _write_json(receipt_path, receipt)
        unreal.log(f"DG_V008_RAW_VISUAL_QA_PASS receipt={receipt_path}")
    except Exception as error:
        receipt.update({
            "status": "BLOCKED",
            "finished_at_utc": _utc_now(),
            "error_type": type(error).__name__,
            "error": str(error),
        })
        _write_json(receipt_path, receipt)
        unreal.log_error(f"DG_V008_RAW_VISUAL_QA_BLOCKED receipt={receipt_path}: {error}")
        raise


if __name__ == "__main__":
    main()
