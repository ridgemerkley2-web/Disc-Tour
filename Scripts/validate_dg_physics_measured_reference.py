#!/usr/bin/env python3
"""Validate the predeclared measured-flight reference protocol and datasets.

The source-controlled dataset is intentionally an empty capture template. Normal
validation proves that the protocol and template remain internally consistent.
``--require-complete`` additionally requires 100 measured throws and named human
approvals, so it must fail closed until real capture and review are complete.
"""

from __future__ import annotations

import argparse
from collections import Counter
import copy
import json
import math
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POLICY = ROOT / "Config" / "DG_PhysicsMeasuredReferencePolicy.json"
DEFAULT_DATASET = ROOT / "Evidence" / "Session19" / "PhysicsMeasuredReferenceDataset.template.json"
EXPECTED_UNCERTAINTY_CEILINGS = {
    "releaseSpeedMps": 0.5,
    "spinRpm": 75.0,
    "releaseAngleDegrees": 1.5,
    "trajectoryPositionMeters": 0.35,
    "landingPositionMeters": 0.5,
    "windMps": 0.5,
}
EXPECTED_UNITS = {
    "distance": "meters",
    "time": "seconds",
    "speed": "meters_per_second",
    "spin": "revolutions_per_minute",
    "angle": "degrees",
    "mass": "grams",
    "wind": "meters_per_second",
}
EXPECTED_CAPTURE_CONDITIONS = {
    "maximumMeanWindMps": 2.0,
    "maximumGustDeltaMps": 1.0,
    "maximumTerrainSlopeDegrees": 2.0,
    "minimumTrajectorySampleRateHz": 30.0,
    "minimumReleaseVideoFrameRateHz": 120.0,
    "releaseSpeedMeasurementRequired": True,
    "spinMeasurementRequired": True,
    "releaseAttitudeMeasurementRequired": True,
    "trajectoryOrReconstructedVideoRequired": True,
    "landingObservationRequired": True,
}
EXPECTED_EQUIPMENT_MATRIX = [
    {"category": "neutral_putter", "moldId": "Touch", "plastic": "Tour", "targetMassGrams": 175.0},
    {"category": "neutral_mid", "moldId": "Compass", "plastic": "Tour", "targetMassGrams": 175.0},
    {"category": "neutral_fairway", "moldId": "Line", "plastic": "Tour", "targetMassGrams": 175.0},
    {"category": "understable_control_driver", "moldId": "Vector", "plastic": "Tour", "targetMassGrams": 175.0},
    {"category": "overstable_distance_driver", "moldId": "Apex", "plastic": "Tour", "targetMassGrams": 175.0},
]
EXPECTED_LAUNCH_MATRIX = [
    {"launchId": "flat_baseline", "throwStyle": "Backhand", "targetHyzerDeg": 0.0, "targetNoseDeg": 0.0, "targetLaunchDeg": 10.0},
    {"launchId": "controlled_hyzer", "throwStyle": "Backhand", "targetHyzerDeg": 15.0, "targetNoseDeg": 0.0, "targetLaunchDeg": 10.0},
    {"launchId": "controlled_anhyzer", "throwStyle": "Backhand", "targetHyzerDeg": -15.0, "targetNoseDeg": 0.0, "targetLaunchDeg": 10.0},
    {"launchId": "forehand_flat_holdout", "throwStyle": "Forehand", "targetHyzerDeg": 0.0, "targetNoseDeg": 0.0, "targetLaunchDeg": 10.0},
]
EXPECTED_THROW_FIELDS = {
    "throwId", "partition", "equipmentCategory", "moldId", "plastic", "massGrams",
    "launchId", "throwStyle", "releaseSpeedMps", "spinRpm", "releaseHyzerDeg",
    "releaseNoseDeg", "releaseLaunchDeg", "meanWindMps", "gustDeltaMps",
    "trajectorySampleRateHz", "trajectoryPoints", "finalCarryMeters",
    "lateralDisplacementMeters", "apexMeters", "turnOnsetSeconds",
    "fadeOnsetSeconds", "landingSpeedMps", "landingAngleDeg", "measurementUncertainty",
}
EXPECTED_TRAJECTORY_POINT_FIELDS = {"timeSeconds", "xMeters", "yMeters", "zMeters"}

# The policy's 175 g equipment rows are capture targets, not merely labels. A
# one-gram allowance covers ordinary scale/display resolution without permitting
# a materially different weight class to masquerade as the predeclared matrix.
TARGET_MASS_TOLERANCE_GRAMS = 1.0
MAX_TRAJECTORY_GAP_PERIODS = 1.5
MAX_TRAJECTORY_DURATION_SECONDS = 60.0
MAX_TRAJECTORY_POSITION_DELTA_METERS = 500.0

# Broad sanity limits intentionally sit outside normal golf-disc operation. They
# reject corrupt units/signs and impossible values without becoming tuning
# envelopes for the simulator.
PHYSICAL_RANGES = {
    "massGrams": (100.0, 200.0),
    "releaseSpeedMps": (0.0, 80.0),
    "releaseHyzerDeg": (-90.0, 90.0),
    "releaseNoseDeg": (-45.0, 45.0),
    "releaseLaunchDeg": (-15.0, 60.0),
    "finalCarryMeters": (0.0, 300.0),
    "lateralDisplacementMeters": (-300.0, 300.0),
    "apexMeters": (0.0, 100.0),
    "turnOnsetSeconds": (0.0, MAX_TRAJECTORY_DURATION_SECONDS),
    "fadeOnsetSeconds": (0.0, MAX_TRAJECTORY_DURATION_SECONDS),
    "landingSpeedMps": (0.0, 80.0),
    "landingAngleDeg": (-90.0, 90.0),
}
EXPECTED_COMPARISON_TOLERANCES = {
    "finalCarryMetersAbsolute": 4.0,
    "finalCarryPercent": 5.0,
    "lateralDisplacementMetersAbsolute": 3.0,
    "apexMetersAbsolute": 2.0,
    "turnOnsetSecondsAbsolute": 0.5,
    "fadeOnsetSecondsAbsolute": 0.5,
    "landingSpeedMpsAbsolute": 2.0,
    "landingSpeedPercent": 10.0,
    "landingAngleDegreesAbsolute": 5.0,
    "requiredHoldoutPassPercent": 90.0,
}


def finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def nonempty_string(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def finite_in_range(value: object, lower: float, upper: float, *, lower_open: bool = False) -> bool:
    if not finite_number(value):
        return False
    lower_ok = value > lower if lower_open else value >= lower
    return lower_ok and value <= upper


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} is not a JSON object")
    return value


def expect(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def validate(policy: dict[str, Any], dataset: dict[str, Any], require_complete: bool) -> list[str]:
    errors: list[str] = []
    expect(errors, policy.get("schema") == "DiscGolfTour.PhysicsMeasuredReferencePolicy.v1", "policy schema is invalid")
    expect(errors, policy.get("schemaVersion") == 1, "policy schemaVersion is invalid")
    expect(errors, policy.get("authority") == "PREDECLARED_CAPTURE_AND_COMPARISON_PROTOCOL_NOT_ACCEPTED_MEASURED_DATA", "policy authority is invalid")
    expect(errors, policy.get("units") == EXPECTED_UNITS, "policy units differ from the predeclared protocol")

    conditions_value = policy.get("captureConditions")
    expect(errors, isinstance(conditions_value, dict), "captureConditions must be an object")
    conditions = conditions_value if isinstance(conditions_value, dict) else {}
    expect(errors, conditions == EXPECTED_CAPTURE_CONDITIONS, "capture conditions differ from the predeclared protocol")

    equipment = policy.get("equipmentMatrix")
    launches = policy.get("launchMatrix")
    expect(errors, isinstance(equipment, list) and len(equipment) == 5, "exact five-equipment calibration matrix is required")
    expect(errors, isinstance(launches, list) and len(launches) == 4, "exact four-launch calibration matrix is required")
    equipment = equipment if isinstance(equipment, list) else []
    launches = launches if isinstance(launches, list) else []
    expect(errors, equipment == EXPECTED_EQUIPMENT_MATRIX, "equipment matrix differs from the predeclared targets")
    expect(errors, launches == EXPECTED_LAUNCH_MATRIX, "launch matrix differs from the predeclared targets")
    equipment_by_category = {item.get("category"): item for item in equipment if isinstance(item, dict)}
    launches_by_id = {item.get("launchId"): item for item in launches if isinstance(item, dict)}
    expect(errors, len(equipment_by_category) == 5 and None not in equipment_by_category, "equipment categories must be unique and non-null")
    expect(errors, len(launches_by_id) == 4 and None not in launches_by_id, "launch IDs must be unique and non-null")

    plan_value = policy.get("samplePlan")
    expect(errors, isinstance(plan_value, dict), "samplePlan must be an object")
    plan = plan_value if isinstance(plan_value, dict) else {}
    fit_launches_value = plan.get("fitLaunchIds")
    holdout_launches_value = plan.get("holdoutLaunchIds")
    fit_launches = fit_launches_value if isinstance(fit_launches_value, list) else []
    holdout_launches = holdout_launches_value if isinstance(holdout_launches_value, list) else []
    repeat_count = plan.get("minimumAcceptedRepeatsPerEquipmentLaunch")
    expect(errors, set(fit_launches) == {"flat_baseline", "controlled_hyzer", "controlled_anhyzer"}, "fit launch partition is invalid")
    expect(errors, holdout_launches == ["forehand_flat_holdout"], "holdout launch partition is invalid")
    expect(errors, set(fit_launches).isdisjoint(holdout_launches), "fit and holdout launch IDs overlap")
    expect(errors, plan.get("fitAndHoldoutThrowIdsMustBeDisjoint") is True, "fit/holdout throw IDs must remain disjoint")
    expect(errors, repeat_count == 5, "repeat count must remain five")
    expected_fit = len(equipment_by_category) * len(fit_launches) * (repeat_count if isinstance(repeat_count, int) else 0)
    expected_holdout = len(equipment_by_category) * len(holdout_launches) * (repeat_count if isinstance(repeat_count, int) else 0)
    expect(errors, plan.get("requiredFitSampleCount") == expected_fit == 75, "fit sample count is inconsistent")
    expect(errors, plan.get("requiredHoldoutSampleCount") == expected_holdout == 25, "holdout sample count is inconsistent")
    expect(errors, plan.get("requiredTotalSampleCount") == expected_fit + expected_holdout == 100, "total sample count is inconsistent")

    route_value = policy.get("routeTelemetryMatrix")
    expect(errors, isinstance(route_value, dict), "routeTelemetryMatrix must be an object")
    route = route_value if isinstance(route_value, dict) else {}
    expect(errors, route.get("courseId") == "PineRidgeChampionship", "route course identity is invalid")
    expect(errors, route.get("layoutId") == "Championship", "route layout identity is invalid")
    expect(errors, route.get("collisionProfileId") == "PineRidgeCompetitiveV2_Fixtures", "route collision profile is invalid")
    expect(errors, route.get("holeNumber") == 2, "route hole identity is invalid")
    expect(errors, set(route.get("routeIds", [])) == {"NeedlePlacement", "LateCrosswindAttack", "LeftPitchOut"}, "route ID matrix is invalid")
    expect(errors, route.get("attemptsPerRoute") == 20 and route.get("requiredAttemptCount") == 60, "route attempt matrix is invalid")

    ceilings_value = policy.get("measurementUncertaintyCeilings")
    expect(errors, isinstance(ceilings_value, dict), "measurementUncertaintyCeilings must be an object")
    ceilings = ceilings_value if isinstance(ceilings_value, dict) else {}
    expect(errors, ceilings == EXPECTED_UNCERTAINTY_CEILINGS, "measurement uncertainty ceilings differ from the predeclared protocol")
    tolerances_value = policy.get("comparisonTolerances")
    expect(errors, isinstance(tolerances_value, dict), "comparisonTolerances must be an object")
    tolerances = tolerances_value if isinstance(tolerances_value, dict) else {}
    for key, expected in EXPECTED_COMPARISON_TOLERANCES.items():
        expect(errors, tolerances.get(key) == expected, f"comparison tolerance {key} differs from the predeclared protocol")
    expect(errors, tolerances.get("outliersRequireWrittenDisposition") is True, "outlier disposition must remain required")

    boundary_value = policy.get("claimBoundary")
    expect(errors, isinstance(boundary_value, dict), "claimBoundary must be an object")
    boundary = boundary_value if isinstance(boundary_value, dict) else {}
    for key in (
        "templateOrEngineeringRegressionIsAcceptedMeasuredCalibration", "physicsOwnerApproval",
        "playtestOwnerApproval", "productOwnerApproval", "releaseReady",
    ):
        expect(errors, boundary.get(key) is False, f"claim boundary {key} must remain false")
    expect(errors, boundary.get("acceptedMeasuredReferenceDatasetPath") is None, "policy must not claim an accepted dataset")

    expect(errors, dataset.get("schema") == "DiscGolfTour.PhysicsMeasuredReferenceDataset.v1", "dataset schema is invalid")
    expect(errors, dataset.get("schemaVersion") == 1, "dataset schemaVersion is invalid")
    expect(errors, dataset.get("policyId") == policy.get("policyId"), "dataset policyId is invalid")
    expect(errors, isinstance(dataset.get("template"), bool), "dataset template flag must be boolean")
    expect(errors, isinstance(dataset.get("acceptedForCalibration"), bool), "dataset acceptedForCalibration flag must be boolean")
    expect(errors, dataset.get("releaseReady") is False, "measured-reference datasets must not claim release readiness")
    if dataset.get("acceptedForCalibration") is True and not require_complete:
        errors.append("accepted calibration evidence requires --require-complete validation")
    throws = dataset.get("throws")
    expect(errors, isinstance(throws, list), "dataset throws must be an array")
    throws = throws if isinstance(throws, list) else []
    required_fields_value = policy.get("requiredThrowFields")
    point_fields_value = policy.get("trajectoryPointFields")
    required_fields = required_fields_value if isinstance(required_fields_value, list) else []
    point_fields = point_fields_value if isinstance(point_fields_value, list) else []
    expect(errors, set(required_fields) == EXPECTED_THROW_FIELDS, "required throw fields differ from the v1 schema")
    expect(errors, set(point_fields) == EXPECTED_TRAJECTORY_POINT_FIELDS, "trajectory point fields differ from the v1 schema")
    throw_ids: set[str] = set()
    fit_throw_ids: set[str] = set()
    holdout_throw_ids: set[str] = set()
    marked_outlier_ids: set[str] = set()
    counts: Counter[tuple[str, str, str]] = Counter()

    for index, throw in enumerate(throws, 1):
        label = f"throw {index}"
        if not isinstance(throw, dict):
            errors.append(f"{label} is not an object")
            continue
        missing = [field for field in required_fields if field not in throw]
        if missing:
            errors.append(f"{label} is missing fields: {', '.join(missing)}")
            continue
        throw_id = throw.get("throwId")
        expect(errors, nonempty_string(throw_id), f"{label} throwId is invalid")
        if nonempty_string(throw_id):
            expect(errors, throw_id not in throw_ids, f"{label} throwId is duplicated")
            throw_ids.add(throw_id)
        partition = throw.get("partition")
        launch_id = throw.get("launchId")
        expected_partition = "fit" if launch_id in fit_launches else "holdout" if launch_id in holdout_launches else None
        expect(errors, partition == expected_partition, f"{label} partition does not match launchId")
        equipment_row = equipment_by_category.get(throw.get("equipmentCategory"))
        launch_row = launches_by_id.get(launch_id)
        expect(errors, equipment_row is not None, f"{label} equipment category is invalid")
        expect(errors, launch_row is not None, f"{label} launchId is invalid")
        if equipment_row:
            expect(errors, throw.get("moldId") == equipment_row.get("moldId"), f"{label} moldId disagrees with equipment matrix")
            expect(errors, throw.get("plastic") == equipment_row.get("plastic"), f"{label} plastic disagrees with equipment matrix")
        if launch_row:
            expect(errors, throw.get("throwStyle") == launch_row.get("throwStyle"), f"{label} throwStyle disagrees with launch matrix")
        if isinstance(partition, str) and isinstance(launch_id, str) and isinstance(throw.get("equipmentCategory"), str):
            counts[(partition, throw["equipmentCategory"], launch_id)] += 1
        if nonempty_string(throw_id):
            if partition == "fit":
                fit_throw_ids.add(throw_id)
            elif partition == "holdout":
                holdout_throw_ids.add(throw_id)
            for marker in ("isOutlier", "excludedFromFit"):
                if marker in throw:
                    expect(errors, isinstance(throw[marker], bool), f"{label} {marker} must be boolean when present")
                    if throw[marker] is True:
                        marked_outlier_ids.add(throw_id)

        numeric_fields = (
            "massGrams", "releaseSpeedMps", "spinRpm", "releaseHyzerDeg", "releaseNoseDeg",
            "releaseLaunchDeg", "meanWindMps", "gustDeltaMps", "trajectorySampleRateHz",
            "finalCarryMeters", "lateralDisplacementMeters", "apexMeters", "turnOnsetSeconds",
            "fadeOnsetSeconds", "landingSpeedMps", "landingAngleDeg",
        )
        for key in numeric_fields:
            expect(errors, finite_number(throw.get(key)), f"{label} {key} is not finite")

        for key, (lower, upper) in PHYSICAL_RANGES.items():
            lower_open = key in {"releaseSpeedMps", "finalCarryMeters"}
            expect(
                errors,
                finite_in_range(throw.get(key), lower, upper, lower_open=lower_open),
                f"{label} {key} is outside the physical range {lower}{' < value' if lower_open else ''} to {upper}",
            )
        if finite_number(throw.get("spinRpm")):
            expect(errors, 0.0 < abs(throw["spinRpm"]) <= 3000.0, f"{label} spinRpm magnitude is outside the physical range (0, 3000]")

        if equipment_row and finite_number(throw.get("massGrams")) and finite_number(equipment_row.get("targetMassGrams")):
            expect(
                errors,
                abs(throw["massGrams"] - equipment_row["targetMassGrams"]) <= TARGET_MASS_TOLERANCE_GRAMS,
                f"{label} massGrams misses the equipment target by more than {TARGET_MASS_TOLERANCE_GRAMS:g} g",
            )
        if launch_row:
            angle_tolerance = ceilings.get("releaseAngleDegrees")
            for measured_key, target_key in (
                ("releaseHyzerDeg", "targetHyzerDeg"),
                ("releaseNoseDeg", "targetNoseDeg"),
                ("releaseLaunchDeg", "targetLaunchDeg"),
            ):
                measured = throw.get(measured_key)
                target = launch_row.get(target_key)
                if finite_number(measured) and finite_number(target) and finite_number(angle_tolerance):
                    expect(
                        errors,
                        abs(measured - target) <= angle_tolerance,
                        f"{label} {measured_key} misses {target_key} by more than {angle_tolerance:g} degrees",
                    )

        if finite_number(throw.get("meanWindMps")):
            expect(errors, abs(throw["meanWindMps"]) <= conditions.get("maximumMeanWindMps", -1), f"{label} mean wind exceeds policy")
        if finite_number(throw.get("gustDeltaMps")):
            expect(errors, abs(throw["gustDeltaMps"]) <= conditions.get("maximumGustDeltaMps", -1), f"{label} gust delta exceeds policy")
        if finite_number(throw.get("trajectorySampleRateHz")):
            expect(errors, throw["trajectorySampleRateHz"] >= conditions.get("minimumTrajectorySampleRateHz", math.inf), f"{label} trajectory sample rate is too low")

        uncertainty = throw.get("measurementUncertainty")
        expect(errors, isinstance(uncertainty, dict), f"{label} measurementUncertainty is invalid")
        if isinstance(uncertainty, dict):
            expect(errors, set(uncertainty) == set(ceilings), f"{label} measurementUncertainty fields differ from policy")
            for key, ceiling in ceilings.items():
                value = uncertainty.get(key)
                expect(errors, finite_number(value) and 0 <= value <= ceiling, f"{label} uncertainty {key} exceeds policy")

        points = throw.get("trajectoryPoints")
        expect(errors, isinstance(points, list) and len(points) >= 2, f"{label} trajectoryPoints requires at least two samples")
        last_time = -math.inf
        points_are_valid = isinstance(points, list) and len(points) >= 2
        times_are_strict = True
        valid_points: list[dict[str, Any]] = []
        if isinstance(points, list):
            for point_index, point in enumerate(points, 1):
                valid = isinstance(point, dict) and all(finite_number(point.get(field)) for field in EXPECTED_TRAJECTORY_POINT_FIELDS)
                expect(errors, valid, f"{label} trajectory point {point_index} is invalid")
                if valid:
                    time_is_strict = point["timeSeconds"] > last_time
                    expect(errors, time_is_strict, f"{label} trajectory times are not strictly increasing")
                    expect(errors, point["timeSeconds"] >= 0.0, f"{label} trajectory point {point_index} has negative time")
                    times_are_strict = times_are_strict and time_is_strict
                    last_time = point["timeSeconds"]
                    valid_points.append(point)
                else:
                    points_are_valid = False

        if points_are_valid and times_are_strict and len(valid_points) == len(points):
            first_point = valid_points[0]
            final_point = valid_points[-1]
            start_time = first_point["timeSeconds"]
            duration = final_point["timeSeconds"] - start_time
            expect(errors, abs(start_time) <= 1e-6, f"{label} trajectory must start at time zero")
            expect(errors, 0.0 < duration <= MAX_TRAJECTORY_DURATION_SECONDS, f"{label} trajectory duration is outside (0, {MAX_TRAJECTORY_DURATION_SECONDS:g}] seconds")

            declared_rate = throw.get("trajectorySampleRateHz")
            if duration > 0.0 and finite_number(declared_rate) and declared_rate > 0.0:
                gaps = [
                    valid_points[i]["timeSeconds"] - valid_points[i - 1]["timeSeconds"]
                    for i in range(1, len(valid_points))
                ]
                actual_rate = (len(valid_points) - 1) / duration
                minimum_rate = conditions.get("minimumTrajectorySampleRateHz")
                if finite_number(minimum_rate):
                    expect(errors, actual_rate + 1e-6 >= minimum_rate, f"{label} actual trajectory sample rate is below policy")
                expect(
                    errors,
                    abs(actual_rate - declared_rate) <= max(1.0, 0.1 * declared_rate),
                    f"{label} declared trajectory sample rate disagrees with timestamps",
                )
                expect(
                    errors,
                    max(gaps) <= MAX_TRAJECTORY_GAP_PERIODS / declared_rate + 1e-6,
                    f"{label} trajectory contains a sampling gap larger than {MAX_TRAJECTORY_GAP_PERIODS:g} declared periods",
                )

            origin = first_point
            for point_index, point in enumerate(valid_points, 1):
                position_is_local = all(
                    abs(point[axis] - origin[axis]) <= MAX_TRAJECTORY_POSITION_DELTA_METERS
                    for axis in ("xMeters", "yMeters", "zMeters")
                )
                expect(errors, position_is_local, f"{label} trajectory point {point_index} exceeds the physical position span")

            dx = final_point["xMeters"] - first_point["xMeters"]
            dy = final_point["yMeters"] - first_point["yMeters"]
            horizontal_displacement = math.hypot(dx, dy)
            max_z = max(point["zMeters"] for point in valid_points)
            trajectory_uncertainty = uncertainty.get("trajectoryPositionMeters") if isinstance(uncertainty, dict) else None
            landing_uncertainty = uncertainty.get("landingPositionMeters") if isinstance(uncertainty, dict) else None
            if not finite_number(trajectory_uncertainty):
                trajectory_uncertainty = EXPECTED_UNCERTAINTY_CEILINGS["trajectoryPositionMeters"]
            if not finite_number(landing_uncertainty):
                landing_uncertainty = EXPECTED_UNCERTAINTY_CEILINGS["landingPositionMeters"]
            summary_position_tolerance = max(landing_uncertainty, 2.0 * trajectory_uncertainty)

            release_speed = throw.get("releaseSpeedMps")
            mean_wind = throw.get("meanWindMps")
            gust_delta = throw.get("gustDeltaMps")
            if duration > 0.0 and all(finite_number(value) for value in (release_speed, mean_wind, gust_delta)):
                release_uncertainty = uncertainty.get("releaseSpeedMps") if isinstance(uncertainty, dict) else None
                wind_uncertainty = uncertainty.get("windMps") if isinstance(uncertainty, dict) else None
                release_uncertainty = release_uncertainty if finite_number(release_uncertainty) else 0.0
                wind_uncertainty = wind_uncertainty if finite_number(wind_uncertainty) else 0.0
                maximum_supported_travel = (
                    release_speed + abs(mean_wind) + abs(gust_delta) + release_uncertainty + wind_uncertainty
                ) * duration + summary_position_tolerance
                expect(
                    errors,
                    horizontal_displacement <= maximum_supported_travel,
                    f"{label} trajectory displacement is impossible for release speed and duration",
                )

            carry = throw.get("finalCarryMeters")
            if finite_number(carry):
                carry_candidates = (abs(dx), horizontal_displacement)
                expect(
                    errors,
                    min(abs(carry - candidate) for candidate in carry_candidates) <= summary_position_tolerance,
                    f"{label} finalCarryMeters disagrees with the trajectory endpoint",
                )
            lateral = throw.get("lateralDisplacementMeters")
            if finite_number(lateral):
                expect(
                    errors,
                    abs(lateral - dy) <= summary_position_tolerance,
                    f"{label} lateralDisplacementMeters disagrees with the trajectory endpoint",
                )
            apex = throw.get("apexMeters")
            if finite_number(apex):
                apex_candidates = (
                    max_z,
                    max_z - first_point["zMeters"],
                    max_z - min(first_point["zMeters"], final_point["zMeters"]),
                )
                expect(
                    errors,
                    min(abs(apex - candidate) for candidate in apex_candidates) <= trajectory_uncertainty,
                    f"{label} apexMeters disagrees with trajectory altitude",
                )
            turn_onset = throw.get("turnOnsetSeconds")
            fade_onset = throw.get("fadeOnsetSeconds")
            if finite_number(turn_onset):
                expect(errors, turn_onset <= final_point["timeSeconds"], f"{label} turn onset occurs after the trajectory ends")
            if finite_number(fade_onset):
                expect(errors, fade_onset <= final_point["timeSeconds"], f"{label} fade onset occurs after the trajectory ends")
            if finite_number(turn_onset) and finite_number(fade_onset):
                expect(errors, turn_onset <= fade_onset, f"{label} fade onset precedes turn onset")

    expect(errors, fit_throw_ids.isdisjoint(holdout_throw_ids), "fit and holdout throw IDs overlap")

    dispositions_value = dataset.get("outlierDispositions")
    expect(errors, isinstance(dispositions_value, list), "dataset outlierDispositions must be an array")
    dispositions = dispositions_value if isinstance(dispositions_value, list) else []
    disposition_throw_ids: set[str] = set()
    for index, disposition in enumerate(dispositions, 1):
        label = f"outlier disposition {index}"
        expect(errors, isinstance(disposition, dict), f"{label} is not an object")
        if not isinstance(disposition, dict):
            continue
        disposition_throw_id = disposition.get("throwId")
        written_disposition = disposition.get("disposition")
        expect(errors, nonempty_string(disposition_throw_id), f"{label} throwId is invalid")
        expect(errors, nonempty_string(written_disposition), f"{label} disposition text is missing")
        if nonempty_string(disposition_throw_id):
            expect(errors, disposition_throw_id in throw_ids, f"{label} references an unknown throwId")
            expect(errors, disposition_throw_id not in disposition_throw_ids, f"{label} duplicates a throwId")
            disposition_throw_ids.add(disposition_throw_id)
    for throw_id in marked_outlier_ids:
        expect(errors, throw_id in disposition_throw_ids, f"marked outlier {throw_id} lacks a written disposition")

    template_mode = dataset.get("template") is True
    if template_mode:
        expect(errors, len(throws) == 0, "template dataset must not contain measured throws")
        expect(errors, len(dispositions) == 0, "template dataset must not contain outlier dispositions")
        expect(errors, dataset.get("acceptedForCalibration") is False, "template must not be accepted for calibration")
        expect(errors, dataset.get("releaseReady") is False, "template must not be release ready")

    if require_complete:
        expect(errors, not template_mode, "complete validation rejects template datasets")
        expect(errors, len(throws) == plan.get("requiredTotalSampleCount"), "complete dataset must contain exactly 100 throws")
        for category in equipment_by_category:
            for launch_id in fit_launches:
                expect(errors, counts[("fit", category, launch_id)] == repeat_count, f"fit cell {category}/{launch_id} must contain exactly five throws")
            for launch_id in holdout_launches:
                expect(errors, counts[("holdout", category, launch_id)] == repeat_count, f"holdout cell {category}/{launch_id} must contain exactly five throws")
        for key in ("datasetId", "recordedUtc", "recorder", "location"):
            expect(errors, nonempty_string(dataset.get(key)), f"complete dataset {key} is missing or invalid")
        equipment_calibration = dataset.get("equipmentCalibration")
        expect(errors, isinstance(equipment_calibration, dict) and bool(equipment_calibration), "complete dataset equipmentCalibration is missing or invalid")
        systems_value = dataset.get("measurementSystems")
        expect(errors, isinstance(systems_value, dict), "complete dataset measurementSystems is invalid")
        systems = systems_value if isinstance(systems_value, dict) else {}
        for key in ("releaseSpeed", "spin", "releaseAttitude", "trajectory", "wind"):
            expect(errors, nonempty_string(systems.get(key)), f"complete dataset measurement system {key} is missing or invalid")
        review_value = dataset.get("review")
        expect(errors, isinstance(review_value, dict), "complete dataset review is invalid")
        review = review_value if isinstance(review_value, dict) else {}
        for key in (
            "captureOperatorAttested", "measurementQualityReviewed", "physicsOwnerApproved",
            "playtestOwnerApproved", "productOwnerApproved",
        ):
            expect(errors, review.get(key) is True, f"complete dataset review {key} is not approved")
        expect(errors, dataset.get("acceptedForCalibration") is True, "complete dataset is not accepted for calibration")
        expect(errors, dataset.get("releaseReady") is False, "measured dataset alone must not claim release readiness")

    return errors


def make_valid_complete_fixture(policy: dict[str, Any]) -> dict[str, Any]:
    """Build deterministic, internally coherent synthetic data for validator tests only."""
    throws: list[dict[str, Any]] = []
    equipment = policy["equipmentMatrix"]
    launches = policy["launchMatrix"]
    fit_launch_ids = set(policy["samplePlan"]["fitLaunchIds"])
    uncertainties = {
        key: value * 0.5 for key, value in policy["measurementUncertaintyCeilings"].items()
    }
    for equipment_row in equipment:
        for launch_row in launches:
            for repeat in range(policy["samplePlan"]["minimumAcceptedRepeatsPerEquipmentLaunch"]):
                lateral = 2.0 if launch_row["throwStyle"] == "Forehand" else -2.0
                points = []
                for sample in range(31):
                    time_seconds = sample / 30.0
                    z_meters = 1.5 * (1.0 - time_seconds) + 16.0 * time_seconds * (1.0 - time_seconds)
                    points.append({
                        "timeSeconds": time_seconds,
                        "xMeters": 30.0 * time_seconds,
                        "yMeters": lateral * time_seconds,
                        "zMeters": z_meters,
                    })
                throws.append({
                    "throwId": f"{equipment_row['category']}_{launch_row['launchId']}_{repeat + 1}",
                    "partition": "fit" if launch_row["launchId"] in fit_launch_ids else "holdout",
                    "equipmentCategory": equipment_row["category"],
                    "moldId": equipment_row["moldId"],
                    "plastic": equipment_row["plastic"],
                    "massGrams": equipment_row["targetMassGrams"],
                    "launchId": launch_row["launchId"],
                    "throwStyle": launch_row["throwStyle"],
                    "releaseSpeedMps": 35.0,
                    "spinRpm": -900.0 if launch_row["throwStyle"] == "Forehand" else 900.0,
                    "releaseHyzerDeg": launch_row["targetHyzerDeg"],
                    "releaseNoseDeg": launch_row["targetNoseDeg"],
                    "releaseLaunchDeg": launch_row["targetLaunchDeg"],
                    "meanWindMps": 0.0,
                    "gustDeltaMps": 0.0,
                    "trajectorySampleRateHz": 30.0,
                    "trajectoryPoints": points,
                    "finalCarryMeters": math.hypot(30.0, lateral),
                    "lateralDisplacementMeters": lateral,
                    "apexMeters": max(point["zMeters"] for point in points),
                    "turnOnsetSeconds": 0.25,
                    "fadeOnsetSeconds": 0.75,
                    "landingSpeedMps": 18.0,
                    "landingAngleDeg": 15.0,
                    "measurementUncertainty": copy.deepcopy(uncertainties),
                })
    return {
        "schema": "DiscGolfTour.PhysicsMeasuredReferenceDataset.v1",
        "schemaVersion": 1,
        "policyId": policy["policyId"],
        "datasetId": "self_test_complete_fixture",
        "recordedUtc": "2026-08-25T00:00:00Z",
        "recorder": "validator self-test",
        "location": "synthetic fixture",
        "equipmentCalibration": {"scale": "self-test calibrated"},
        "measurementSystems": {
            "releaseSpeed": "self-test radar",
            "spin": "self-test camera",
            "releaseAttitude": "self-test camera",
            "trajectory": "self-test reconstruction",
            "wind": "self-test anemometer",
        },
        "throws": throws,
        "outlierDispositions": [],
        "review": {
            "captureOperatorAttested": True,
            "measurementQualityReviewed": True,
            "physicsOwnerApproved": True,
            "playtestOwnerApproved": True,
            "productOwnerApproved": True,
        },
        "acceptedForCalibration": True,
        "releaseReady": False,
        "template": False,
    }


def run_self_test(policy: dict[str, Any], dataset: dict[str, Any]) -> int:
    cases: list[tuple[str, dict[str, Any], dict[str, Any], bool]] = []
    complete_fixture = make_valid_complete_fixture(policy)

    def add(name: str, mutate_policy=None, mutate_dataset=None, require_complete=False) -> None:
        p = copy.deepcopy(policy)
        d = copy.deepcopy(dataset)
        if mutate_policy:
            mutate_policy(p)
        if mutate_dataset:
            mutate_dataset(d)
        cases.append((name, p, d, require_complete))

    def add_complete(name: str, mutate_policy=None, mutate_dataset=None) -> None:
        p = copy.deepcopy(policy)
        d = copy.deepcopy(complete_fixture)
        if mutate_policy:
            mutate_policy(p)
        if mutate_dataset:
            mutate_dataset(d)
        cases.append((name, p, d, True))

    add("bad policy authority", lambda p: p.update(authority="INVALID"))
    add("bad policy units", lambda p: p["units"].update(speed="feet_per_second"))
    add("weakened capture sample rate", lambda p: p["captureConditions"].update(minimumTrajectorySampleRateHz=1.0))
    add("missing equipment row", lambda p: p["equipmentMatrix"].pop())
    add("changed equipment target", lambda p: p["equipmentMatrix"][0].update(targetMassGrams=150.0))
    add("changed launch target", lambda p: p["launchMatrix"][0].update(targetLaunchDeg=45.0))
    add("overlapping holdout", lambda p: p["samplePlan"].update(holdoutLaunchIds=["flat_baseline"]))
    add("holdout disjointness disabled", lambda p: p["samplePlan"].update(fitAndHoldoutThrowIdsMustBeDisjoint=False))
    add("bad route count", lambda p: p["routeTelemetryMatrix"].update(requiredAttemptCount=59))
    add("weakened carry tolerance", lambda p: p["comparisonTolerances"].update(finalCarryMetersAbsolute=40.0))
    add("accepted policy dataset", lambda p: p["claimBoundary"].update(acceptedMeasuredReferenceDatasetPath="fake.json"))
    add("template contains throw", mutate_dataset=lambda d: d["throws"].append({}))
    add("template claims acceptance", mutate_dataset=lambda d: d.update(acceptedForCalibration=True))
    add("template claims release", mutate_dataset=lambda d: d.update(releaseReady=True))
    add("complete rejects template", require_complete=True)

    add_complete("mass outside physical range", mutate_dataset=lambda d: d["throws"][0].update(massGrams=-999.0))
    add_complete("mass misses capture target", mutate_dataset=lambda d: d["throws"][0].update(massGrams=173.5))
    add_complete("negative release speed", mutate_dataset=lambda d: d["throws"][0].update(releaseSpeedMps=-100.0))
    add_complete("zero spin", mutate_dataset=lambda d: d["throws"][0].update(spinRpm=0.0))
    add_complete("impossible release angle", mutate_dataset=lambda d: d["throws"][0].update(releaseLaunchDeg=999.0))
    add_complete("release angle misses capture target", mutate_dataset=lambda d: d["throws"][0].update(releaseLaunchDeg=12.0))
    add_complete("negative carry", mutate_dataset=lambda d: d["throws"][0].update(finalCarryMeters=-500.0))
    add_complete("negative apex", mutate_dataset=lambda d: d["throws"][0].update(apexMeters=-100.0))
    add_complete("negative landing speed", mutate_dataset=lambda d: d["throws"][0].update(landingSpeedMps=-10.0))
    add_complete("negative uncertainty", mutate_dataset=lambda d: d["throws"][0]["measurementUncertainty"].update(windMps=-0.1))
    add_complete("missing uncertainty field", mutate_dataset=lambda d: d["throws"][0]["measurementUncertainty"].pop("spinRpm"))
    add_complete(
        "sparse trajectory lies about sample rate",
        mutate_dataset=lambda d: d["throws"][0].update(
            trajectoryPoints=[d["throws"][0]["trajectoryPoints"][0], d["throws"][0]["trajectoryPoints"][-1]]
        ),
    )
    add_complete("trajectory starts late", mutate_dataset=lambda d: d["throws"][0]["trajectoryPoints"][0].update(timeSeconds=0.01))
    add_complete("carry summary disagrees with points", mutate_dataset=lambda d: d["throws"][0].update(finalCarryMeters=80.0))
    add_complete("lateral summary disagrees with points", mutate_dataset=lambda d: d["throws"][0].update(lateralDisplacementMeters=40.0))
    add_complete("apex summary disagrees with points", mutate_dataset=lambda d: d["throws"][0].update(apexMeters=50.0))
    add_complete("turn onset follows fade onset", mutate_dataset=lambda d: d["throws"][0].update(turnOnsetSeconds=0.9, fadeOnsetSeconds=0.5))
    add_complete("onset occurs after landing", mutate_dataset=lambda d: d["throws"][0].update(fadeOnsetSeconds=2.0))
    add_complete("marked outlier lacks disposition", mutate_dataset=lambda d: d["throws"][0].update(isOutlier=True))
    add_complete(
        "outlier disposition references unknown throw",
        mutate_dataset=lambda d: d["outlierDispositions"].append({"throwId": "missing", "disposition": "Exclude"}),
    )
    add_complete(
        "outlier disposition lacks written text",
        mutate_dataset=lambda d: d["outlierDispositions"].append({"throwId": d["throws"][0]["throwId"], "disposition": ""}),
    )
    add_complete("invalid measurement system", mutate_dataset=lambda d: d["measurementSystems"].update(spin=True))
    add_complete("invalid equipment calibration", mutate_dataset=lambda d: d.update(equipmentCalibration="claimed"))

    failures = []
    baseline_errors = validate(policy, complete_fixture, True)
    if baseline_errors:
        print("Measured-reference validator self-test FAILED")
        print(" - valid complete fixture was rejected:")
        for error in baseline_errors:
            print("   -", error)
        return 1
    for name, p, d, require_complete in cases:
        if not validate(p, d, require_complete):
            failures.append(name)
    if failures:
        print("Measured-reference validator self-test FAILED")
        for failure in failures:
            print(" - mutation was accepted:", failure)
        return 1
    print(
        "Measured-reference validator self-test OK: valid complete fixture accepted; "
        f"{len(cases)}/{len(cases)} invalid mutations rejected"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--dataset", type=Path, default=DEFAULT_DATASET)
    parser.add_argument("--require-complete", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    policy_path = args.policy if args.policy.is_absolute() else ROOT / args.policy
    dataset_path = args.dataset if args.dataset.is_absolute() else ROOT / args.dataset
    try:
        policy = load_json(policy_path)
        dataset = load_json(dataset_path)
    except Exception as exc:
        print(f"Measured-reference validation FAILED: {exc}")
        return 1
    if args.self_test:
        return run_self_test(policy, dataset)
    errors = validate(policy, dataset, args.require_complete)
    if errors:
        print("Measured-reference validation FAILED")
        for error in errors:
            print(" -", error)
        return 2 if args.require_complete else 1
    if args.require_complete:
        print("Measured-reference dataset OK: complete, reviewed, and accepted for calibration; release readiness remains false")
    else:
        print("Measured-reference protocol/template OK: capture is prepared; measured data and named approvals remain pending")
    return 0


if __name__ == "__main__":
    sys.exit(main())
