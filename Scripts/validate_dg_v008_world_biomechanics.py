#!/usr/bin/env python3
"""Read-only, fail-closed validation of v008 world-space throw telemetry.

This validator does not infer biomechanics from animation curves.  It consumes
per-frame world transforms/angles exported from the evaluated skeletal pose and
checks the physical result.  Passing is a technical gate, never human animation,
contact, provenance, or release approval.
"""

from __future__ import annotations

import argparse
import copy
import json
import math
from pathlib import Path
import sys
from typing import Any, Callable


SCHEMA = "DiscGolfTour.WorldBiomechanicsTelemetry.v1"
REQUIRED_RATE = 60.0
JOINTS = (
    "pelvis", "thorax", "head", "hip_l", "knee_l", "ankle_l",
    "heel_l", "toe_l", "hip_r", "knee_r", "ankle_r", "heel_r",
    "toe_r", "shoulder_r", "elbow_r", "hand_r",
)
ANGLES = ("pelvis_yaw", "thorax_yaw", "knee_l_flexion", "knee_r_flexion", "elbow_r")
PHASES = ("reachback", "plant", "pocket", "release", "followthrough", "recovery", "settle")


class ValidationError(ValueError):
    pass


def _finite_number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValidationError(f"{label} must be a finite number")
    return float(value)


def _vector(value: Any, label: str) -> tuple[float, float, float]:
    if not isinstance(value, list) or len(value) != 3:
        raise ValidationError(f"{label} must be a three-number array")
    return tuple(_finite_number(v, f"{label}[{i}]") for i, v in enumerate(value))  # type: ignore[return-value]


def _sub(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, ...]:
    return tuple(x - y for x, y in zip(a, b))


def _dot(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return sum(x * y for x, y in zip(a, b))


def _length(a: tuple[float, ...]) -> float:
    return math.sqrt(_dot(a, a))


def _unit(a: tuple[float, ...], label: str) -> tuple[float, ...]:
    length = _length(a)
    if length < 0.999 or length > 1.001:
        raise ValidationError(f"{label} must be normalized (length={length:.6f})")
    return tuple(x / length for x in a)


def _angle_delta(current: float, previous: float) -> float:
    return (current - previous + 180.0) % 360.0 - 180.0


def _window(frames: list[dict[str, Any]], first: int, last: int) -> list[dict[str, Any]]:
    return frames[first:last + 1]


def validate(payload: Any) -> dict[str, Any]:
    failures: list[str] = []
    metrics: dict[str, Any] = {}
    try:
        if not isinstance(payload, dict) or payload.get("schema") != SCHEMA:
            raise ValidationError(f"schema must equal {SCHEMA}")
        rate = _finite_number(payload.get("frame_rate"), "frame_rate")
        if abs(rate - REQUIRED_RATE) > 1e-6:
            raise ValidationError("frame_rate must be exactly 60")
        if payload.get("handedness") != "RHBH":
            raise ValidationError("handedness must be RHBH")
        forward = _unit(_vector(payload.get("target_forward_world"), "target_forward_world"), "target_forward_world")
        lateral = _unit(_vector(payload.get("target_lateral_world"), "target_lateral_world"), "target_lateral_world")
        if abs(_dot(forward, lateral)) > 1e-3:
            raise ValidationError("target axes must be orthogonal")
        ground = _finite_number(payload.get("ground_z_cm"), "ground_z_cm")
        phases = payload.get("phases")
        if not isinstance(phases, dict) or set(phases) != set(PHASES):
            raise ValidationError(f"phases must contain exactly {list(PHASES)}")
        phase = {name: int(phases[name]) for name in PHASES if isinstance(phases[name], int) and not isinstance(phases[name], bool)}
        if len(phase) != len(PHASES) or list(phase.values()) != sorted(phase.values()) or len(set(phase.values())) != len(PHASES):
            raise ValidationError("phase frames must be unique ordered integers")
        raw_frames = payload.get("frames")
        if not isinstance(raw_frames, list) or len(raw_frames) < phase["settle"] + 1:
            raise ValidationError("frames do not cover settle")
        frames: list[dict[str, Any]] = []
        for index, raw in enumerate(raw_frames):
            if not isinstance(raw, dict) or raw.get("frame") != index:
                raise ValidationError(f"frames[{index}].frame must equal its array index")
            expected_time = index / rate
            if abs(_finite_number(raw.get("time_seconds"), f"frames[{index}].time_seconds") - expected_time) > 1e-5:
                raise ValidationError(f"frames[{index}] time does not match fixed 60 Hz")
            joints = raw.get("joints_world_cm")
            angles = raw.get("angles_degrees")
            if not isinstance(joints, dict) or set(joints) != set(JOINTS):
                raise ValidationError(f"frames[{index}] joints must contain exactly the required set")
            if not isinstance(angles, dict) or set(angles) != set(ANGLES):
                raise ValidationError(f"frames[{index}] angles must contain exactly the required set")
            frames.append({
                "j": {name: _vector(joints[name], f"frames[{index}].{name}") for name in JOINTS},
                "a": {name: _finite_number(angles[name], f"frames[{index}].{name}") for name in ANGLES},
                "com": _vector(raw.get("com_world_cm"), f"frames[{index}].com_world_cm"),
                "margin": _finite_number(raw.get("support_polygon_margin_cm"), f"frames[{index}].support_polygon_margin_cm"),
            })
    except (ValidationError, KeyError, TypeError, ValueError) as exc:
        return {"status": "FAIL", "failures": [f"SCHEMA: {exc}"], "metrics": {}}

    def fail(code: str, detail: str) -> None:
        failures.append(f"{code}: {detail}")

    # Plant foot: both contact landmarks must remain planted and on the ground.
    plant, release = phase["plant"], phase["release"]
    planted = _window(frames, plant, release)
    anchor = planted[0]
    max_slip = max(
        _length((_sub(frame["j"][bone], anchor["j"][bone])[0], _sub(frame["j"][bone], anchor["j"][bone])[1]))
        for frame in planted for bone in ("heel_l", "toe_l")
    )
    min_height = min(frame["j"][bone][2] - ground for frame in planted for bone in ("heel_l", "toe_l"))
    max_height = max(frame["j"][bone][2] - ground for frame in planted for bone in ("heel_l", "toe_l"))
    metrics.update(plant_slip_cm=max_slip, plant_min_height_cm=min_height, plant_max_height_cm=max_height)
    if max_slip > 1.5: fail("FOOT_SLIP", f"{max_slip:.3f} cm > 1.5 cm")
    if min_height < -0.5: fail("FOOT_PENETRATION", f"{min_height:.3f} cm < -0.5 cm")
    if max_height > 1.0: fail("FOOT_HOVER", f"{max_height:.3f} cm > 1.0 cm")

    # COM transfer and brace: move into the brace, then arrest translation.
    reach = frames[phase["reachback"]]["com"]
    plant_com = frames[plant]["com"]
    release_com = frames[release]["com"]
    transfer = _dot(_sub(plant_com, reach), forward)
    post_plant = _length(_sub(release_com, plant_com)[:2])
    min_margin = min(frame["margin"] for frame in planted)
    metrics.update(com_transfer_cm=transfer, post_plant_com_travel_cm=post_plant, minimum_brace_support_margin_cm=min_margin)
    if transfer < 8.0: fail("COM_TRANSFER", f"{transfer:.3f} cm < 8 cm")
    if post_plant > 3.0: fail("BRACE_ARREST", f"{post_plant:.3f} cm > 3 cm")
    if min_margin < 0.0: fail("COM_SUPPORT", f"margin {min_margin:.3f} cm is outside support polygon")

    knee_values = [frame["a"]["knee_l_flexion"] for frame in planted]
    metrics.update(plant_knee_flexion_min_degrees=min(knee_values), plant_knee_flexion_max_degrees=max(knee_values))
    if min(knee_values) < 10.0 or max(knee_values) > 60.0:
        fail("KNEE_BOUNDS", f"plant interval range {min(knee_values):.2f}..{max(knee_values):.2f} degrees outside 10..60")

    # Separation and transform-derived kinetic sequence.
    reach_sep = abs(_angle_delta(frames[phase["reachback"]]["a"]["thorax_yaw"], frames[phase["reachback"]]["a"]["pelvis_yaw"]))
    separations = [abs(_angle_delta(frames[i]["a"]["thorax_yaw"], frames[i]["a"]["pelvis_yaw"])) for i in range(release - 6, release + 1)]
    metrics.update(reachback_pelvis_thorax_separation_degrees=reach_sep, prerelease_separation_degrees=separations)
    if not 20.0 <= reach_sep <= 55.0: fail("SEPARATION_RANGE", f"reachback separation {reach_sep:.2f} outside 20..55")
    if any(separations[i + 1] - separations[i] > 3.0 for i in range(len(separations) - 1)):
        fail("SEPARATION_UNWIND", "separation increases by more than 3 degrees during final six frames")

    first, last = max(1, plant), release
    pelvis_rate = {i: abs(_angle_delta(frames[i]["a"]["pelvis_yaw"], frames[i - 1]["a"]["pelvis_yaw"])) for i in range(first, last + 1)}
    thorax_rate = {i: abs(_angle_delta(frames[i]["a"]["thorax_yaw"], frames[i - 1]["a"]["thorax_yaw"])) for i in range(first, last + 1)}
    elbow_rate = {i: abs(frames[i]["a"]["elbow_r"] - frames[i - 1]["a"]["elbow_r"]) for i in range(first, last + 1)}
    wrist_speed = {i: _length(_sub(frames[i]["j"]["hand_r"], frames[i - 1]["j"]["hand_r"])) for i in range(first, last + 1)}
    peaks = {"pelvis": max(pelvis_rate, key=pelvis_rate.get), "thorax": max(thorax_rate, key=thorax_rate.get), "elbow": max(elbow_rate, key=elbow_rate.get), "wrist": max(wrist_speed, key=wrist_speed.get)}
    metrics["kinetic_peak_frames"] = peaks
    if not (2 <= peaks["thorax"] - peaks["pelvis"] <= 8): fail("PELVIS_THORAX_ORDER", f"peak frames {peaks['pelvis']} -> {peaks['thorax']}")
    if not (1 <= peaks["elbow"] - peaks["thorax"] <= 6): fail("THORAX_ELBOW_ORDER", f"peak frames {peaks['thorax']} -> {peaks['elbow']}")
    if peaks["wrist"] < peaks["elbow"] or peaks["wrist"] > release: fail("ELBOW_WRIST_ORDER", f"peak frames {peaks['elbow']} -> {peaks['wrist']} with release {release}")

    # Reject the horizontal arm shelf and backward hand movement through pocket.
    shelf_run = max_run = 0
    for i in range(phase["pocket"], phase["followthrough"] + 1):
        elbow_height_delta = abs(frames[i]["j"]["elbow_r"][2] - frames[i]["j"]["shoulder_r"][2])
        upper_abduction_proxy = elbow_height_delta <= 5.0 and frames[i]["a"]["elbow_r"] >= 145.0
        shelf_run = shelf_run + 1 if upper_abduction_proxy else 0
        max_run = max(max_run, shelf_run)
    hand_steps = [_dot(_sub(frames[i]["j"]["hand_r"], frames[i - 1]["j"]["hand_r"]), forward) for i in range(phase["pocket"] + 1, release + 1)]
    metrics.update(horizontal_shelf_max_run_frames=max_run, pocket_release_forward_steps_cm=hand_steps)
    if max_run > 4: fail("ELBOW_SHELF", f"horizontal extended shelf lasts {max_run} frames")
    if sum(step > 0.0 for step in hand_steps[-5:]) < 4 or min(hand_steps) < -1.0:
        fail("HAND_PROGRESSION", "hand does not progress forward on at least 4/5 final intervals or reverses over 1 cm")

    # Follow-through must cross the torso, decay, and finish balanced.
    throw_side = _dot(_sub(frames[phase["pocket"]]["j"]["hand_r"], frames[phase["pocket"]]["j"]["thorax"]), lateral)
    follow_end = min(release + 18, phase["recovery"])
    follow_lateral = [_dot(_sub(frames[i]["j"]["hand_r"], frames[i]["j"]["thorax"]), lateral) for i in range(release + 1, follow_end + 1)]
    crossed = any(value * throw_side <= -2.0 * abs(throw_side) / max(abs(throw_side), 1e-6) for value in follow_lateral)
    recovery_rates = [_length(_sub(frames[i]["j"]["hand_r"], frames[i - 1]["j"]["hand_r"])) for i in range(phase["recovery"] + 1, phase["settle"] + 1)]
    release_speed = wrist_speed[peaks["wrist"]]
    settle_margin = frames[phase["settle"]]["margin"]
    settle_elbow = frames[phase["settle"]]["a"]["elbow_r"]
    metrics.update(follow_crossed_midline=crossed, recovery_max_hand_speed_cm_per_frame=max(recovery_rates), settle_support_margin_cm=settle_margin, settle_elbow_degrees=settle_elbow)
    if not crossed: fail("FOLLOW_CROSS", "throwing hand does not cross torso midline within 18 frames")
    if max(recovery_rates) >= 0.20 * release_speed: fail("RECOVERY_DECELERATION", "recovery hand speed is not below 20% of release peak")
    if settle_margin < 3.0: fail("RECOVERY_BALANCE", f"settle support margin {settle_margin:.2f} cm < 3 cm")
    if not 60.0 <= settle_elbow <= 130.0: fail("RECOVERY_ARM", f"settle elbow {settle_elbow:.2f} outside 60..130")

    return {"status": "PASS" if not failures else "FAIL", "failures": failures, "metrics": metrics}


def _fixture() -> dict[str, Any]:
    phases = dict(reachback=44, plant=64, pocket=79, release=84, followthrough=94, recovery=118, settle=132)
    frames = []
    pelvis_yaw = thorax_yaw = 0.0
    for i in range(145):
        pelvis_yaw += 8.0 * math.exp(-((i - 72.0) / 1.8) ** 2)
        thorax_yaw += 7.0 * math.exp(-((i - 76.0) / 1.8) ** 2)
        if i <= 44:
            thorax_yaw = pelvis_yaw + 30.0
        elif i < 78:
            thorax_yaw = max(thorax_yaw, pelvis_yaw + 30.0)
        elif i <= 84:
            thorax_yaw = pelvis_yaw + 30.0 * (84 - i) / 6.0
        elbow = 90.0 + 12.0 * sum(math.exp(-((k - 81.0) / 0.8) ** 2) for k in range(65, i + 1))
        hand_x = sum(5.0 * math.exp(-((k - 83.0) / 1.2) ** 2) for k in range(65, i + 1))
        if i > 84:
            hand_x += min(i - 84, 12) * 0.4
        hand_y = 20.0 if i <= 84 else 20.0 - min(i - 84, 12) * 2.5
        recovery_hand = (hand_x, hand_y, 130.0)
        if i > 118:
            recovery_hand = (hand_x + 0.02 * (i - 118), hand_y, 130.0)
        com_x = 10.0 * max(0.0, min(1.0, (i - 44) / 20.0)) + 2.0 * max(0.0, min(1.0, (i - 64) / 20.0))
        joints = {name: [0.0, 0.0, 0.0] for name in JOINTS}
        joints.update({
            "pelvis": [com_x, 0.0, 100.0], "thorax": [com_x, 0.0, 140.0], "head": [com_x, 0.0, 175.0],
            "heel_l": [10.0, -8.0, 0.0], "toe_l": [30.0, -8.0, 0.0],
            "heel_r": [-20.0, 8.0, 0.0], "toe_r": [0.0, 8.0, 0.0],
            "shoulder_r": [com_x, 20.0, 145.0], "elbow_r": [com_x + 10.0, 25.0, 125.0],
            "hand_r": list(recovery_hand), "hip_l": [com_x, -8.0, 100.0], "hip_r": [com_x, 8.0, 100.0],
            "knee_l": [com_x + 5.0, -8.0, 55.0], "knee_r": [com_x - 5.0, 8.0, 55.0],
            "ankle_l": [10.0, -8.0, 8.0], "ankle_r": [-20.0, 8.0, 8.0],
        })
        frames.append({"frame": i, "time_seconds": i / 60.0, "joints_world_cm": joints,
                       "angles_degrees": {"pelvis_yaw": pelvis_yaw, "thorax_yaw": thorax_yaw,
                                           "knee_l_flexion": 30.0, "knee_r_flexion": 30.0, "elbow_r": elbow if i < 118 else 90.0},
                       "com_world_cm": [com_x, 0.0, 95.0], "support_polygon_margin_cm": 5.0})
    return {"schema": SCHEMA, "frame_rate": 60, "handedness": "RHBH", "target_forward_world": [1.0, 0.0, 0.0],
            "target_lateral_world": [0.0, 1.0, 0.0], "ground_z_cm": 0.0, "phases": phases, "frames": frames}


def self_test() -> tuple[int, int]:
    cases: list[tuple[str, Callable[[dict[str, Any]], None], str]] = [
        ("pass", lambda x: None, "PASS"),
        ("schema", lambda x: x.update(schema="wrong"), "SCHEMA"),
        ("slip", lambda x: x["frames"][70]["joints_world_cm"]["heel_l"].__setitem__(0, 12.0), "FOOT_SLIP"),
        ("penetration", lambda x: x["frames"][70]["joints_world_cm"]["toe_l"].__setitem__(2, -1.0), "FOOT_PENETRATION"),
        ("hover", lambda x: x["frames"][70]["joints_world_cm"]["toe_l"].__setitem__(2, 2.0), "FOOT_HOVER"),
        ("transfer", lambda x: [f.__setitem__("com_world_cm", [0.0, 0.0, 95.0]) for f in x["frames"]], "COM_TRANSFER"),
        ("brace", lambda x: x["frames"][84].__setitem__("com_world_cm", [20.0, 0.0, 95.0]), "BRACE_ARREST"),
        ("support", lambda x: x["frames"][70].__setitem__("support_polygon_margin_cm", -1.0), "COM_SUPPORT"),
        ("knee", lambda x: x["frames"][70]["angles_degrees"].__setitem__("knee_l_flexion", 2.0), "KNEE_BOUNDS"),
        ("separation", lambda x: x["frames"][44]["angles_degrees"].__setitem__("thorax_yaw", x["frames"][44]["angles_degrees"]["pelvis_yaw"]), "SEPARATION_RANGE"),
        ("kinetic", lambda x: x["frames"][80]["angles_degrees"].__setitem__("pelvis_yaw", x["frames"][79]["angles_degrees"]["pelvis_yaw"] + 90.0), "PELVIS_THORAX_ORDER"),
        ("shelf", lambda x: [x["frames"][i]["joints_world_cm"]["elbow_r"].__setitem__(2, 145.0) or x["frames"][i]["angles_degrees"].__setitem__("elbow_r", 160.0) for i in range(79, 90)], "ELBOW_SHELF"),
        ("progression", lambda x: x["frames"][82]["joints_world_cm"]["hand_r"].__setitem__(0, -20.0), "HAND_PROGRESSION"),
        ("follow", lambda x: [x["frames"][i]["joints_world_cm"]["hand_r"].__setitem__(1, 20.0) for i in range(85, 103)], "FOLLOW_CROSS"),
        ("deceleration", lambda x: x["frames"][120]["joints_world_cm"]["hand_r"].__setitem__(0, x["frames"][119]["joints_world_cm"]["hand_r"][0] + 10.0), "RECOVERY_DECELERATION"),
        ("balance", lambda x: x["frames"][132].__setitem__("support_polygon_margin_cm", 1.0), "RECOVERY_BALANCE"),
        ("recovery_arm", lambda x: x["frames"][132]["angles_degrees"].__setitem__("elbow_r", 150.0), "RECOVERY_ARM"),
    ]
    passed = 0
    for name, mutate, expected in cases:
        fixture = _fixture()
        mutate(fixture)
        report = validate(fixture)
        text = report["status"] + " " + " ".join(report["failures"])
        if expected not in text:
            print(f"SELF_TEST_FAIL {name}: expected {expected}, got {text}", file=sys.stderr)
        else:
            passed += 1
    return passed, len(cases)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("telemetry", nargs="?", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        passed, total = self_test()
        print(f"SELF_TEST {passed}/{total}")
        return 0 if passed == total else 1
    if args.telemetry is None:
        parser.error("telemetry is required unless --self-test is used")
    try:
        payload = json.loads(args.telemetry.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        report = {"status": "FAIL", "failures": [f"INPUT: {exc}"], "metrics": {}}
    else:
        report = validate(payload)
    output = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output, encoding="utf-8")
    print(output, end="")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
