#!/usr/bin/env python3
"""Validate a partial or complete Needle Gate route-telemetry report."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPORT = ROOT / "Saved" / "RouteTelemetryReports" / "LatestRouteTelemetry.json"
ROUTES = {"NeedlePlacement", "LateCrosswindAttack", "LeftPitchOut"}
MISS_SIDES = {"Inside", "Short", "Long", "Left", "Right", "Unclassified"}
FIXTURES = {"Unknown", "Tree", "DenseGrass", "Rock", "Sign"}


def finite(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", nargs="?", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()
    report_path = args.report if args.report.is_absolute() else ROOT / args.report
    try:
        report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        print(f"Route telemetry validation FAILED: {exc}")
        return 1

    errors: list[str] = []
    if report.get("schema") != "disc_golf_route_telemetry" or report.get("schema_version") != 1:
        errors.append("schema identity/version is invalid")
    if report.get("course_id") != "PineRidgeChampionship" or report.get("layout_id") != "Championship":
        errors.append("course/layout identity is invalid")
    if report.get("collision_profile_id") != "PineRidgeCompetitiveV2_Fixtures":
        errors.append("collision profile is not the active fixture revision")
    if report.get("hole_number") != 2 or report.get("target_attempts_per_route") != 20:
        errors.append("Needle Gate hole/target contract is invalid")

    routes = report.get("routes", [])
    route_ids = {route.get("route_id") for route in routes if isinstance(route, dict)}
    if route_ids != ROUTES or len(routes) != 3:
        errors.append("exact three-route coverage is missing")
    attempts = report.get("attempts", [])
    if not isinstance(attempts, list):
        errors.append("attempts is not an array")
        attempts = []

    counts = {route_id: 0 for route_id in ROUTES}
    required_numeric = (
        "corridor_sample_count", "corridor_samples_inside", "corridor_adherence_percent",
        "maximum_corridor_deviation_meters", "penalty_strokes", "remaining_distance_meters",
        "tradeoff_understood", "next_shot_clear", "final_hole_score", "fixture_contact_count",
        "flight_time_seconds", "final_carry_meters", "release_speed_mps", "release_spin_rpm",
        "release_hyzer_deg", "release_nose_deg", "release_launch_deg", "release_quality_01",
    )
    for index, attempt in enumerate(attempts, 1):
        label = f"attempt {index}"
        if not isinstance(attempt, dict):
            errors.append(f"{label} is not an object")
            continue
        if attempt.get("attempt_number") != index:
            errors.append(f"{label} numbering is not contiguous")
        route_id = attempt.get("route_id")
        if route_id not in ROUTES:
            errors.append(f"{label} has unknown route_id")
        else:
            counts[route_id] += 1
        if attempt.get("miss_side") not in MISS_SIDES:
            errors.append(f"{label} miss_side is invalid")
        if attempt.get("last_fixture_type") not in FIXTURES:
            errors.append(f"{label} fixture identity is invalid")
        for field in required_numeric:
            if not finite(attempt.get(field)):
                errors.append(f"{label} {field} is not finite")
        samples = attempt.get("corridor_sample_count", -1)
        inside = attempt.get("corridor_samples_inside", -1)
        adherence = attempt.get("corridor_adherence_percent", -1)
        if finite(samples) and finite(inside) and not (samples >= inside >= 0):
            errors.append(f"{label} corridor sample counts are invalid")
        if finite(adherence) and not 0 <= adherence <= 100:
            errors.append(f"{label} corridor adherence is outside 0-100")
        for field in ("tradeoff_understood", "next_shot_clear"):
            if attempt.get(field) not in (-1, 0, 1):
                errors.append(f"{label} {field} is not -1/0/1")
        if attempt.get("landing_zone_hit") != (attempt.get("miss_side") == "Inside"):
            errors.append(f"{label} landing hit and miss classification disagree")
        for field in ("release_location_cm", "raw_final_location_cm", "lie_location_cm"):
            vector = attempt.get(field)
            if not isinstance(vector, dict) or any(not finite(vector.get(axis)) for axis in "xyz"):
                errors.append(f"{label} {field} is invalid")

    for route in routes:
        if not isinstance(route, dict) or route.get("route_id") not in ROUTES:
            continue
        route_id = route["route_id"]
        summary = route.get("summary", {})
        if summary.get("attempts") != counts[route_id] or summary.get("target_attempts") != 20:
            errors.append(f"{route_id} summary count is inconsistent")

    complete = all(count == 20 for count in counts.values())
    if report.get("complete") != complete:
        errors.append("complete flag does not match exact route counts")
    if any(count > 20 for count in counts.values()):
        errors.append("a route exceeds the fixed 20-attempt evidence set")
    if args.require_complete and not complete:
        errors.append(f"complete 60-attempt set required; counts={counts}")

    if errors:
        print("Route telemetry validation FAILED")
        for error in errors:
            print(" -", error)
        return 1
    state = "complete" if complete else "partial"
    print(f"Route telemetry report OK: {state}, {len(attempts)}/60 attempts, counts={counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
