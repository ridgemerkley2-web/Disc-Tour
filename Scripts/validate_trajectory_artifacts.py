#!/usr/bin/env python3
"""Validate physics presets and versioned trajectory export artifacts."""

from __future__ import annotations

import csv
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRESETS = ROOT / "Data" / "PhysicsRegressionPresets.json"
LATEST_JSON = ROOT / "Saved" / "TrajectoryExports" / "LatestTrajectory.json"
LATEST_CSV = ROOT / "Saved" / "TrajectoryExports" / "LatestTrajectory.csv"
LATEST_REPORT = ROOT / "Saved" / "PhysicsRegressionReports" / "LatestPhysicsRegression.json"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def validate_presets() -> int:
    data = json.loads(PRESETS.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_physics_regression_presets", "wrong preset schema")
    require(data.get("schema_version") == 2, "unsupported preset schema version")
    presets = data.get("presets")
    require(isinstance(presets, list) and presets, "preset list is empty")

    ids: set[str] = set()
    calm_by_fps: dict[int, dict] = {}
    required_ranges = (
        "air_carry_m", "final_carry_m", "apex_m", "air_time_s",
        "lateral_m", "ground_contacts",
    )
    for preset in presets:
        preset_id = preset.get("id")
        require(isinstance(preset_id, str) and preset_id, "preset id is missing")
        require(preset_id not in ids, f"duplicate preset id: {preset_id}")
        ids.add(preset_id)
        fps = preset.get("render_fps")
        require(isinstance(fps, int) and 15 <= fps <= 240, f"unsafe render FPS in {preset_id}")
        require(0.0 <= preset.get("power", -1.0) <= 1.0, f"power outside [0,1] in {preset_id}")
        require(-1.0 <= preset.get("timing_error", -2.0) <= 1.0, f"timing error outside [-1,1] in {preset_id}")
        require(preset.get("shot_context") in {"Drive", "Circle1Putt", "Circle2Putt"},
                f"invalid shot context in {preset_id}")
        require(finite_number(preset.get("start_distance_m")) and 0.0 <= preset["start_distance_m"] <= 25.0,
                f"invalid start distance in {preset_id}")
        require(finite_number(preset.get("aim_offset_deg")) and abs(preset["aim_offset_deg"]) <= 45.0,
                f"invalid aim offset in {preset_id}")
        wind = preset.get("wind_mps")
        require(isinstance(wind, dict) and all(finite_number(wind.get(axis)) for axis in "xyz"),
                f"invalid wind vector in {preset_id}")
        expected = preset.get("expected")
        require(isinstance(expected, dict), f"missing expected envelope in {preset_id}")
        for field in required_ranges:
            bounds = expected.get(field)
            require(isinstance(bounds, list) and len(bounds) == 2 and all(finite_number(v) for v in bounds),
                    f"invalid {field} bounds in {preset_id}")
            require(bounds[0] <= bounds[1], f"reversed {field} bounds in {preset_id}")
        require(expected.get("final_ground_state") in {
            "Airborne", "Impact", "Skipping", "Sliding", "EdgeRolling", "Settled"
        }, f"invalid final state in {preset_id}")
        require(isinstance(expected.get("holed_out"), bool), f"missing holed-out result in {preset_id}")
        require(expected.get("basket_contact") in {
            "None", "Caught", "ChainDeflection", "BandRejection", "TrayRejection"
        }, f"invalid basket result in {preset_id}")
        if preset_id.startswith("ApexCalm"):
            calm_by_fps[fps] = preset

    require(set(calm_by_fps) == {30, 60, 120}, "calm frame-rate suite must contain 30/60/120 FPS")
    command_fields = (
        "mold_id", "plastic", "throw_style", "power", "hyzer_deg",
        "shot_context", "start_distance_m", "aim_offset_deg", "nose_deg", "launch_deg",
        "timing_error", "wind_mps", "expected",
    )
    reference = calm_by_fps[60]
    for fps in (30, 120):
        for field in command_fields:
            require(calm_by_fps[fps][field] == reference[field],
                    f"ApexCalm{fps} differs from the 60 FPS command/envelope at {field}")
    putting = {preset["id"]: preset for preset in presets if preset["shot_context"] != "Drive"}
    require(set(putting) == {"TouchCircle1Center", "TouchCircle2Center"},
            "putting suite must contain Circle 1 and Circle 2 center-chain scenarios")
    for preset_id, preset in putting.items():
        require(preset["mold_id"] == "Touch", f"{preset_id} must use the Touch putter")
        require(preset["expected"]["holed_out"] is True, f"{preset_id} must require a make")
        require(preset["expected"]["basket_contact"] == "Caught", f"{preset_id} must require center chains")
    return len(presets)


def validate_exports(json_path: Path, csv_path: Path) -> tuple[int, int]:
    require(json_path.is_file(), f"missing trajectory JSON: {json_path}")
    require(csv_path.is_file(), f"missing trajectory CSV: {csv_path}")
    data = json.loads(json_path.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_trajectory", "wrong trajectory schema")
    require(data.get("schema_version") == 3, "unsupported trajectory schema version")
    summary = data.get("summary")
    samples = data.get("samples")
    transitions = data.get("ground_transitions")
    require(isinstance(summary, dict), "trajectory summary is missing")
    require(isinstance(samples, list) and samples, "trajectory samples are empty")
    require(isinstance(transitions, list), "ground transitions are missing")
    require(summary.get("sample_count") == len(samples), "JSON sample count does not match summary")
    require(summary.get("ground_transition_count") == len(transitions),
            "JSON transition count does not match summary")
    require(summary.get("last_basket_contact") in {
        "None", "Caught", "ChainDeflection", "BandRejection", "TrayRejection"
    }, "trajectory summary has invalid basket result")
    require(summary.get("surface_at_rest") in {
        "Fairway", "TeePad", "LightRough", "DeepRough", "Dirt", "Rock", "OutOfBounds", "Hazard"
    }, "trajectory summary has invalid course surface")
    require(summary.get("playing_surface") in {
        "Fairway", "TeePad", "LightRough", "DeepRough", "Dirt", "Rock", "OutOfBounds", "Hazard"
    }, "trajectory summary has invalid playing surface")
    require(summary.get("resulting_lie_type") in {
        "Tee", "Fairway", "LightRough", "DeepRough", "Circle2", "Circle1", "Hazard"
    }, "trajectory summary has invalid resulting lie")
    require(summary.get("penalty_type") in {"None", "OutOfBounds", "Hazard"},
            "trajectory summary has invalid penalty type")
    require(summary.get("relief_rule") in {"PlayFromResult", "LastInBounds"},
            "trajectory summary has invalid relief rule")
    require(isinstance(summary.get("penalty_strokes"), int) and 0 <= summary["penalty_strokes"] <= 1,
            "trajectory summary has invalid penalty strokes")
    require(data.get("release", {}).get("shot_context") in {"Drive", "Circle1Putt", "Circle2Putt"},
            "trajectory release has invalid shot context")

    prior_time = -math.inf
    for index, sample in enumerate(samples):
        time_s = sample.get("time_s")
        require(finite_number(time_s) and time_s >= prior_time, f"sample time is not monotonic at {index}")
        prior_time = float(time_s)
        for vector_name in ("relative_position_m", "world_location_cm", "velocity_mps", "disc_normal_world", "wind_mps"):
            vector = sample.get(vector_name)
            require(isinstance(vector, dict) and all(finite_number(vector.get(axis)) for axis in "xyz"),
                    f"invalid {vector_name} at sample {index}")
        require(sample.get("course_surface") in {
            "Fairway", "TeePad", "LightRough", "DeepRough", "Dirt", "Rock", "OutOfBounds", "Hazard"
        }, f"invalid course surface at sample {index}")

    lines = csv_path.read_text(encoding="utf-8").splitlines()
    require("# schema=disc_golf_trajectory" in lines, "CSV schema metadata is missing")
    require("# schema_version=3" in lines, "CSV schema version is missing")
    data_lines = [line for line in lines if not line.startswith("#")]
    rows = list(csv.DictReader(data_lines))
    sample_rows = [row for row in rows if row.get("row_type") == "sample"]
    transition_rows = [row for row in rows if row.get("row_type") == "transition"]
    require(len(sample_rows) == len(samples), "CSV/JSON sample counts differ")
    require(len(transition_rows) == len(transitions), "CSV/JSON transition counts differ")
    return len(samples), len(transitions)


def validate_regression_report(path: Path) -> int:
    require(path.is_file(), f"missing physics regression report: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_physics_regression_report", "wrong regression report schema")
    require(data.get("schema_version") == 2, "unsupported regression report schema version")
    require(data.get("passed") is True, "latest physics regression report failed")
    require(data.get("comparison_failures") == [], "frame-rate comparisons contain failures")
    results = data.get("results")
    require(isinstance(results, list) and len(results) >= 6, "regression report is missing scenarios")
    by_id = {result.get("preset_id"): result for result in results}
    for preset_id in ("ApexCalm30", "ApexCalm60", "ApexCalm120", "ApexForehandCalm60"):
        require(preset_id in by_id, f"regression report is missing {preset_id}")
        require(by_id[preset_id].get("regression_passed") is True, f"{preset_id} failed its envelope")
    for preset_id in ("TouchCircle1Center", "TouchCircle2Center"):
        require(preset_id in by_id, f"regression report is missing {preset_id}")
        require(by_id[preset_id].get("regression_passed") is True, f"{preset_id} failed its envelope")
        require(by_id[preset_id].get("holed_out") is True, f"{preset_id} did not hole out")
        require(by_id[preset_id].get("last_basket_contact") == "Caught", f"{preset_id} missed center chains")
    reference = by_id["ApexCalm60"]
    for preset_id in ("ApexCalm30", "ApexCalm120"):
        result = by_id[preset_id]
        require(abs(result["final_carry_m"] - reference["final_carry_m"]) <= data["max_final_carry_delta_m"],
                f"{preset_id} final carry differs from 60 FPS")
        require(abs(result["apex_m"] - reference["apex_m"]) <= data["max_apex_delta_m"],
                f"{preset_id} apex differs from 60 FPS")
        require(abs(result["lateral_m"] - reference["lateral_m"]) <= data["max_lateral_delta_m"],
                f"{preset_id} lateral finish differs from 60 FPS")
        require(abs(result["ground_distance_m"] - reference["ground_distance_m"]) <= data["max_ground_distance_delta_m"],
                f"{preset_id} ground distance differs from 60 FPS")
        require(result["ground_contact_count"] == reference["ground_contact_count"],
                f"{preset_id} ground contacts differ from 60 FPS")
    return len(results)


def main() -> int:
    try:
        preset_count = validate_presets()
        json_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else LATEST_JSON
        csv_path = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else LATEST_CSV
        report_path = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else LATEST_REPORT
        if json_path.exists() or csv_path.exists() or len(sys.argv) > 1:
            sample_count, transition_count = validate_exports(json_path, csv_path)
            print(f"Trajectory artifacts OK: {sample_count} samples, {transition_count} transitions")
        else:
            print("Trajectory artifacts not present yet; preset contract only")
        if report_path.exists():
            result_count = validate_regression_report(report_path)
            print(f"Physics regression report OK: {result_count} scenarios")
        print(f"Physics regression presets OK: {preset_count}")
        return 0
    except (AssertionError, json.JSONDecodeError, KeyError, TypeError, ValueError) as exc:
        print(f"Trajectory validation FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
