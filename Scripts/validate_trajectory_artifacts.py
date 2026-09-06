#!/usr/bin/env python3
"""Validate physics presets and versioned trajectory export artifacts."""

from __future__ import annotations

import copy
import csv
import hashlib
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRESETS = ROOT / "Data" / "PhysicsRegressionPresets.json"
LATEST_JSON = ROOT / "Saved" / "TrajectoryExports" / "LatestTrajectory.json"
LATEST_CSV = ROOT / "Saved" / "TrajectoryExports" / "LatestTrajectory.csv"
LATEST_REPORT = ROOT / "Saved" / "PhysicsRegressionReports" / "LatestPhysicsRegression.json"
REQUIRED_PRESET_IDS = {
    "ApexCalm30",
    "ApexCalm60",
    "ApexCalm120",
    "ApexForehandCalm60",
    "TouchCircle1Center",
    "TouchCircle2Center",
}
EXPECTED_COMPARISON_LIMITS = {
    "max_final_carry_delta_m": 0.35,
    "max_apex_delta_m": 0.10,
    "max_lateral_delta_m": 0.25,
    "max_ground_distance_delta_m": 0.50,
}
EXPECTED_PRESET_FILE_SHA1 = "193C48DBCEDDBC629FA5873892F061C30BA4FF21"
THROW_COMMAND_SCALAR_LIMITS = {
    "power": (0.0, 1.0),
    "hyzer_deg": (-34.0, 34.0),
    "nose_deg": (-7.0, 11.0),
    "launch_deg": (-5.0, 35.0),
    "timing_error": (-1.0, 1.0),
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def finite_number(value: object) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(float(value))
    )


def current_preset_file_sha1() -> str:
    return hashlib.sha1(PRESETS.read_bytes()).hexdigest().upper()


def validate_preset_data(data: dict) -> int:
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
        require(isinstance(preset.get("mold_id"), str) and preset["mold_id"],
                f"mold identity is missing in {preset_id}")
        require(preset.get("plastic") in {"Base", "Tour", "Crystal"},
                f"invalid plastic in {preset_id}")
        fps = preset.get("render_fps")
        require(isinstance(fps, int) and 15 <= fps <= 240, f"unsafe render FPS in {preset_id}")
        require(preset.get("throw_style") in {"Backhand", "Forehand"},
                f"invalid throw style in {preset_id}")
        for field, (minimum, maximum) in THROW_COMMAND_SCALAR_LIMITS.items():
            value = preset.get(field)
            require(finite_number(value) and minimum <= value <= maximum,
                    f"{field} outside [{minimum:g},{maximum:g}] in {preset_id}")
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

    require(ids == REQUIRED_PRESET_IDS,
            f"preset identities differ from the canonical six: {sorted(ids)}")
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
    for fps, preset in calm_by_fps.items():
        require(preset["expected"]["lateral_m"][1] < 0.0,
                f"ApexCalm{fps} must fail a wrong-sign RHBH finish")
    forehand = next(preset for preset in presets if preset["id"] == "ApexForehandCalm60")
    require(forehand["expected"]["lateral_m"][0] > 0.0,
            "ApexForehandCalm60 must fail a wrong-sign RHFH finish")
    putting = {preset["id"]: preset for preset in presets if preset["shot_context"] != "Drive"}
    require(set(putting) == {"TouchCircle1Center", "TouchCircle2Center"},
            "putting suite must contain Circle 1 and Circle 2 center-chain scenarios")
    for preset_id, preset in putting.items():
        require(preset["mold_id"] == "Touch", f"{preset_id} must use the Touch putter")
        require(preset["expected"]["holed_out"] is True, f"{preset_id} must require a make")
        require(preset["expected"]["basket_contact"] == "Caught", f"{preset_id} must require center chains")
    return len(presets)


def validate_presets() -> int:
    digest = current_preset_file_sha1()
    require(
        digest == EXPECTED_PRESET_FILE_SHA1,
        f"preset file SHA-1 {digest} differs from frozen contract {EXPECTED_PRESET_FILE_SHA1}",
    )
    return validate_preset_data(json.loads(PRESETS.read_text(encoding="utf-8")))


def make_valid_regression_report_fixture(preset_file_sha1: str) -> dict:
    scenarios = (
        ("ApexCalm30", 30, False),
        ("ApexCalm60", 60, False),
        ("ApexCalm120", 120, False),
        ("ApexForehandCalm60", 60, False),
        ("TouchCircle1Center", 60, True),
        ("TouchCircle2Center", 60, True),
    )
    results = []
    for index, (preset_id, render_fps, must_hole_out) in enumerate(scenarios, start=1):
        results.append({
            "preset_id": preset_id,
            "render_fps": render_fps,
            "handedness": "Right",
            "wind_phase_origin_s": float(index * 37),
            "sample_count": 1201,
            "duration_s": 5.0,
            "air_time_s": 5.0,
            "air_carry_m": 80.0,
            "final_carry_m": 90.0,
            "apex_m": 10.0,
            "lateral_m": 2.0,
            "ground_distance_m": 8.0,
            "ground_contact_count": 2,
            "holed_out": must_hole_out,
            "last_basket_contact": "Caught" if must_hole_out else "None",
            "was_regression": True,
            "regression_passed": True,
            "regression_failures": [],
        })
    return {
        "schema": "disc_golf_physics_regression_report",
        "schema_version": 3,
        "run_state": "completed",
        "passed": True,
        "authoritative_presets_loaded": True,
        "presentation_trace_unchanged": True,
        "preset_file_sha1": preset_file_sha1,
        "comparison_failures": [],
        **EXPECTED_COMPARISON_LIMITS,
        "results": results,
    }


def self_test() -> int:
    baseline = json.loads(PRESETS.read_text(encoding="utf-8"))
    preset_digest = current_preset_file_sha1()
    require(preset_digest == EXPECTED_PRESET_FILE_SHA1,
            "current preset fixture does not match the frozen raw-file digest")
    require(validate_preset_data(copy.deepcopy(baseline)) == 6,
            "valid canonical preset fixture was rejected")

    mutations = (
        ("missing-mold", lambda d: d["presets"][3].__setitem__("mold_id", ""), "mold identity"),
        ("unknown-plastic", lambda d: d["presets"][3].__setitem__("plastic", "Unknown"), "invalid plastic"),
        ("power", lambda d: d["presets"][3].__setitem__("power", 1.001), "power outside"),
        ("hyzer", lambda d: d["presets"][3].__setitem__("hyzer_deg", 34.001), "hyzer_deg outside"),
        ("nose", lambda d: d["presets"][3].__setitem__("nose_deg", -7.001), "nose_deg outside"),
        ("launch", lambda d: d["presets"][3].__setitem__("launch_deg", 35.001), "launch_deg outside"),
        ("timing", lambda d: d["presets"][3].__setitem__("timing_error", -1.001), "timing_error outside"),
        ("throw-style", lambda d: d["presets"][3].__setitem__("throw_style", "Unknown"), "invalid throw style"),
        ("shot-context", lambda d: d["presets"][3].__setitem__("shot_context", "Unknown"), "invalid shot context"),
    )
    rejected = 0
    for label, mutate, expected_error in mutations:
        candidate = copy.deepcopy(baseline)
        mutate(candidate)
        try:
            validate_preset_data(candidate)
        except AssertionError as exc:
            require(expected_error in str(exc),
                    f"{label} failed for the wrong reason: {exc}")
            rejected += 1
        else:
            raise AssertionError(f"{label} mutation was accepted")

    report_fixture = make_valid_regression_report_fixture(preset_digest)
    require(validate_regression_report_data(copy.deepcopy(report_fixture), preset_digest) == 6,
            "valid schema-v3 regression report fixture was rejected")
    report_mutations = (
        ("old-report-schema", lambda d: d.__setitem__("schema_version", 2), "schema version"),
        ("in-progress-report", lambda d: d.__setitem__("run_state", "in_progress"), "terminal-complete"),
        ("missing-authority", lambda d: d.pop("authoritative_presets_loaded"), "authoritative preset"),
        ("false-authority", lambda d: d.__setitem__("authoritative_presets_loaded", False), "authoritative preset"),
        ("missing-presentation", lambda d: d.pop("presentation_trace_unchanged"), "presentation-isolated"),
        ("false-presentation", lambda d: d.__setitem__("presentation_trace_unchanged", False), "presentation-isolated"),
        ("wrong-preset-digest", lambda d: d.__setitem__("preset_file_sha1", "0" * 40), "current raw preset"),
        ("comparison-failure", lambda d: d["comparison_failures"].append("failure"), "comparisons contain failures"),
        ("scenario-failure", lambda d: d["results"][0]["regression_failures"].append("failure"), "contains regression failures"),
        ("invalid-sample-count", lambda d: d["results"][0].__setitem__("sample_count", 1), "sample count"),
        ("missed-touch-putt", lambda d: d["results"][4].__setitem__("holed_out", False), "did not hole out"),
        ("wrong-touch-contact", lambda d: d["results"][5].__setitem__("last_basket_contact", "ChainDeflection"), "missed center chains"),
    )
    rejected_reports = 0
    for label, mutate, expected_error in report_mutations:
        candidate = copy.deepcopy(report_fixture)
        mutate(candidate)
        try:
            validate_regression_report_data(candidate, preset_digest)
        except AssertionError as exc:
            require(expected_error in str(exc),
                    f"{label} failed for the wrong reason: {exc}")
            rejected_reports += 1
        else:
            raise AssertionError(f"{label} mutation was accepted")

    print(f"Trajectory validator self-test OK: valid preset/report contracts accepted; "
          f"{rejected}/{len(mutations)} invalid command mutations and "
          f"{rejected_reports}/{len(report_mutations)} invalid report mutations rejected")
    return 0


def validate_exports(json_path: Path, csv_path: Path) -> tuple[int, int]:
    require(json_path.is_file(), f"missing trajectory JSON: {json_path}")
    require(csv_path.is_file(), f"missing trajectory CSV: {csv_path}")
    data = json.loads(json_path.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_trajectory", "wrong trajectory schema")
    require(data.get("schema_version") == 5, "unsupported trajectory schema version")
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
    release = data.get("release")
    require(isinstance(release, dict), "trajectory release is missing")
    require(release.get("shot_context") in {"Drive", "Circle1Putt", "Circle2Putt"},
            "trajectory release has invalid shot context")
    require(release.get("handedness") in {"Right", "Left"},
            "trajectory release has invalid handedness")
    require(summary.get("handedness") in {"Right", "Left"},
            "trajectory summary has invalid handedness")
    require(summary["handedness"] == release["handedness"],
            "trajectory summary/release handedness disagree")
    phase_origin_s = release.get("wind_phase_origin_s")
    require(finite_number(phase_origin_s) and 0.0 <= float(phase_origin_s) < 4096.0,
            "trajectory release has invalid deterministic wind phase origin")
    require(finite_number(summary.get("wind_phase_origin_s"))
            and math.isclose(float(summary["wind_phase_origin_s"]), float(phase_origin_s), abs_tol=1e-6),
            "trajectory summary/release wind phase origins disagree")
    final_telemetry = data.get("final_telemetry")
    require(isinstance(final_telemetry, dict)
            and finite_number(final_telemetry.get("wind_phase_origin_s"))
            and math.isclose(float(final_telemetry["wind_phase_origin_s"]), float(phase_origin_s), abs_tol=1e-6),
            "trajectory telemetry/release wind phase origins disagree")

    prior_time = -math.inf
    for index, sample in enumerate(samples):
        time_s = sample.get("time_s")
        require(
            finite_number(time_s) and time_s > prior_time,
            f"sample time is not strictly increasing at {index}",
        )
        prior_time = float(time_s)
        for vector_name in ("relative_position_m", "world_location_cm", "velocity_mps", "disc_normal_world", "wind_mps"):
            vector = sample.get(vector_name)
            require(isinstance(vector, dict) and all(finite_number(vector.get(axis)) for axis in "xyz"),
                    f"invalid {vector_name} at sample {index}")
        normal = sample["disc_normal_world"]
        normal_magnitude = math.sqrt(sum(float(normal[axis]) ** 2 for axis in "xyz"))
        require(abs(normal_magnitude - 1.0) <= 1.0e-3,
                f"disc normal is not unit length at sample {index}")
        require(finite_number(sample.get("spin_rpm")) and sample["spin_rpm"] >= 0.0,
                f"invalid spin RPM at sample {index}")
        require(finite_number(sample.get("angle_of_attack_deg")),
                f"invalid angle of attack at sample {index}")
        require(sample.get("ground_state") in {
            "Airborne", "Impact", "Skipping", "Sliding", "EdgeRolling", "Settled"
        }, f"invalid ground state at sample {index}")
        require(sample.get("course_surface") in {
            "Fairway", "TeePad", "LightRough", "DeepRough", "Dirt", "Rock", "OutOfBounds", "Hazard"
        }, f"invalid course surface at sample {index}")

    lines = csv_path.read_text(encoding="utf-8").splitlines()
    require("# schema=disc_golf_trajectory" in lines, "CSV schema metadata is missing")
    require("# schema_version=5" in lines, "CSV schema version is missing")
    require(f"# handedness={release['handedness']}" in lines,
            "CSV handedness metadata is missing or disagrees with JSON")
    phase_metadata = [line for line in lines if line.startswith("# wind_phase_origin_s=")]
    require(len(phase_metadata) == 1,
            "CSV deterministic wind phase metadata is missing or duplicated")
    csv_phase_origin_s = float(phase_metadata[0].split("=", 1)[1])
    require(math.isfinite(csv_phase_origin_s)
            and math.isclose(csv_phase_origin_s, float(phase_origin_s), abs_tol=1e-6),
            "CSV deterministic wind phase disagrees with JSON")
    data_lines = [line for line in lines if not line.startswith("#")]
    rows = list(csv.DictReader(data_lines))
    sample_rows = [row for row in rows if row.get("row_type") == "sample"]
    transition_rows = [row for row in rows if row.get("row_type") == "transition"]
    require(len(sample_rows) == len(samples), "CSV/JSON sample counts differ")
    require(len(transition_rows) == len(transitions), "CSV/JSON transition counts differ")
    return len(samples), len(transitions)


def validate_regression_report_data(data: dict, preset_file_sha1: str) -> int:
    require(data.get("schema") == "disc_golf_physics_regression_report", "wrong regression report schema")
    require(data.get("schema_version") == 3, "unsupported regression report schema version")
    require(data.get("run_state") == "completed", "physics regression report is not terminal-complete")
    require(data.get("passed") is True, "latest physics regression report failed")
    require(data.get("authoritative_presets_loaded") is True,
            "physics regression report did not load the authoritative preset contract")
    require(data.get("presentation_trace_unchanged") is True,
            "physics regression report was not presentation-isolated")
    require(preset_file_sha1 == EXPECTED_PRESET_FILE_SHA1,
            "current preset file does not match the frozen SHA-1 contract")
    require(data.get("preset_file_sha1") == preset_file_sha1,
            "physics regression report is not bound to the current raw preset file")
    require(data.get("comparison_failures") == [], "frame-rate comparisons contain failures")
    for field, expected in EXPECTED_COMPARISON_LIMITS.items():
        require(finite_number(data.get(field)) and math.isclose(float(data[field]), expected),
                f"regression report changed the accepted {field}")
    results = data.get("results")
    require(isinstance(results, list) and len(results) == len(REQUIRED_PRESET_IDS),
            "regression report must contain exactly six scenarios")
    result_ids = [result.get("preset_id") for result in results if isinstance(result, dict)]
    require(len(result_ids) == len(results), "regression report contains a non-object result")
    require(len(set(result_ids)) == len(result_ids), "regression report contains duplicate scenario ids")
    require(set(result_ids) == REQUIRED_PRESET_IDS,
            f"regression report identities differ from the canonical six: {sorted(result_ids)}")
    by_id = {result.get("preset_id"): result for result in results}
    wind_phase_origins: set[float] = set()
    finite_result_fields = (
        "duration_s", "air_time_s", "air_carry_m", "final_carry_m",
        "apex_m", "lateral_m", "ground_distance_m",
    )
    for preset_id, result in by_id.items():
        require(result.get("handedness") == "Right",
                f"{preset_id} is not the canonical right-handed baseline")
        require(finite_number(result.get("wind_phase_origin_s"))
                and 0.0 <= float(result["wind_phase_origin_s"]) < 4096.0,
                f"{preset_id} has invalid deterministic wind phase provenance")
        phase_origin = float(result["wind_phase_origin_s"])
        require(phase_origin not in wind_phase_origins,
                f"{preset_id} reuses deterministic wind phase provenance")
        wind_phase_origins.add(phase_origin)
        require(all(finite_number(result.get(field)) for field in finite_result_fields),
                f"{preset_id} contains a non-finite regression metric")
        require(isinstance(result.get("sample_count"), int)
                and not isinstance(result.get("sample_count"), bool)
                and result["sample_count"] > 1,
                f"{preset_id} has an invalid sample count")
        require(result.get("regression_failures") == [],
                f"{preset_id} contains regression failures")
        require(result.get("was_regression") is True,
                f"{preset_id} was not captured as a regression scenario")
        require(result.get("regression_passed") is True,
                f"{preset_id} failed its envelope")
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


def validate_regression_report(path: Path) -> int:
    require(path.is_file(), f"missing physics regression report: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    return validate_regression_report_data(data, current_preset_file_sha1())


def main() -> int:
    try:
        if sys.argv[1:] == ["--self-test"]:
            return self_test()
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
