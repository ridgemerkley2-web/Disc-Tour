#!/usr/bin/env python3
"""Fail-closed validation for partial or complete Needle Gate route telemetry."""

from __future__ import annotations

import argparse
import copy
from datetime import datetime
import json
import math
from pathlib import Path
import re
import sys
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPORT = ROOT / "Saved" / "RouteTelemetryReports" / "LatestRouteTelemetry.json"
ROUTES = {"NeedlePlacement", "LateCrosswindAttack", "LeftPitchOut"}
ROUTE_CONTRACT = {
    "NeedlePlacement": {
        "label": "NEEDLE-GATE PLACEMENT",
        "route_type": "Primary",
        "landing_zone_id": "GateLanding",
        "target_strokes": 3,
        "risk_rating": 3,
        "reward_rating": 4,
        "corridor_width_meters": 10.0,
    },
    "LateCrosswindAttack": {
        "label": "LATE-CROSSWIND ATTACK",
        "route_type": "RiskReward",
        "landing_zone_id": "GateLanding",
        "target_strokes": 3,
        "risk_rating": 5,
        "reward_rating": 5,
        "corridor_width_meters": 7.5,
    },
    "LeftPitchOut": {
        "label": "LEFT PITCH-OUT PAR",
        "route_type": "Bailout",
        "landing_zone_id": "PitchOutLanding",
        "target_strokes": 4,
        "risk_rating": 1,
        "reward_rating": 2,
        "corridor_width_meters": 12.5,
    },
}
MISS_SIDES = {"Inside", "Short", "Long", "Left", "Right", "Unclassified"}
PENALTY_TYPES = {"None", "OutOfBounds", "Hazard"}
FIXTURES = {"Unknown", "Tree", "DenseGrass", "Rock", "Sign"}
MOLDS = {"Apex", "Vector", "Line", "Compass", "Touch"}
PLASTICS = {"Base", "Tour", "Crystal"}
THROW_STYLES = {"Backhand", "Forehand"}
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z$")

ROOT_KEYS = {
    "schema", "schema_version", "session_id", "started_utc", "updated_utc",
    "course_id", "layout_id", "collision_profile_id", "hole_number",
    "target_attempts_per_route", "active_route_id", "complete", "routes",
    "attempts",
}
ROUTE_KEYS = {
    "route_id", "label", "route_type", "landing_zone_id", "target_strokes",
    "risk_rating", "reward_rating", "corridor_width_meters", "summary",
}
SUMMARY_KEYS = {
    "attempts", "target_attempts", "landing_zone_hits",
    "landing_zone_hit_rate_percent", "penalty_attempts", "penalty_rate_percent",
    "mean_corridor_adherence_percent", "mean_remaining_distance_meters",
    "basket_visible_rate_percent", "tradeoff_understood_rated",
    "tradeoff_understood_yes", "next_shot_clear_rated", "next_shot_clear_yes",
    "final_scores_recorded", "mean_final_score",
}
ATTEMPT_KEYS = {
    "attempt_number", "route_id", "recorded_utc", "release_location_cm",
    "raw_final_location_cm", "lie_location_cm", "landing_zone_hit", "miss_side",
    "corridor_sample_count", "corridor_samples_inside",
    "corridor_adherence_percent", "maximum_corridor_deviation_meters",
    "penalty_type", "penalty_strokes", "remaining_distance_meters",
    "basket_visible", "tradeoff_understood", "next_shot_clear",
    "final_hole_score", "holed_out_on_route_shot", "fixture_contact_count",
    "last_fixture_type", "flight_time_seconds", "final_carry_meters", "mold_id",
    "plastic", "throw_style", "release_speed_mps", "release_spin_rpm",
    "release_hyzer_deg", "release_nose_deg", "release_launch_deg",
    "release_quality_01",
}
INTEGER_SUMMARY_KEYS = {
    "attempts", "target_attempts", "landing_zone_hits", "penalty_attempts",
    "tradeoff_understood_rated", "tradeoff_understood_yes",
    "next_shot_clear_rated", "next_shot_clear_yes", "final_scores_recorded",
}


class DuplicateKeyError(ValueError):
    """Raised when a JSON object contains a duplicate member name."""


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise DuplicateKeyError(f"duplicate JSON key {key!r}")
        value[key] = item
    return value


def finite(value: object) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
    )


def exact_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def valid_utc(value: object) -> bool:
    if not isinstance(value, str) or UTC_RE.fullmatch(value) is None:
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def require_exact_keys(
    value: object,
    expected: set[str],
    label: str,
    errors: list[str],
) -> bool:
    if not isinstance(value, dict):
        errors.append(f"{label} is not an object")
        return False
    actual = set(value)
    if actual != expected:
        errors.append(
            f"{label} fields differ: missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
        return False
    return True


def close_number(actual: object, expected: float) -> bool:
    return finite(actual) and math.isclose(
        float(actual), float(expected), rel_tol=1.0e-6, abs_tol=1.0e-3
    )


def expected_summary(attempts: list[dict[str, Any]]) -> dict[str, int | float]:
    count = len(attempts)
    landing_hits = sum(attempt["landing_zone_hit"] is True for attempt in attempts)
    penalty_attempts = sum(attempt["penalty_strokes"] > 0 for attempt in attempts)
    visible_attempts = sum(attempt["basket_visible"] is True for attempt in attempts)
    understood = [
        attempt["tradeoff_understood"]
        for attempt in attempts
        if attempt["tradeoff_understood"] >= 0
    ]
    clear = [
        attempt["next_shot_clear"]
        for attempt in attempts
        if attempt["next_shot_clear"] >= 0
    ]
    scores = [
        attempt["final_hole_score"]
        for attempt in attempts
        if attempt["final_hole_score"] >= 0
    ]
    divisor = float(count)
    return {
        "attempts": count,
        "target_attempts": 20,
        "landing_zone_hits": landing_hits,
        "landing_zone_hit_rate_percent": landing_hits * 100.0 / divisor if count else 0.0,
        "penalty_attempts": penalty_attempts,
        "penalty_rate_percent": penalty_attempts * 100.0 / divisor if count else 0.0,
        "mean_corridor_adherence_percent": (
            sum(attempt["corridor_adherence_percent"] for attempt in attempts) / divisor
            if count else 0.0
        ),
        "mean_remaining_distance_meters": (
            sum(attempt["remaining_distance_meters"] for attempt in attempts) / divisor
            if count else 0.0
        ),
        "basket_visible_rate_percent": visible_attempts * 100.0 / divisor if count else 0.0,
        "tradeoff_understood_rated": len(understood),
        "tradeoff_understood_yes": sum(value > 0 for value in understood),
        "next_shot_clear_rated": len(clear),
        "next_shot_clear_yes": sum(value > 0 for value in clear),
        "final_scores_recorded": len(scores),
        "mean_final_score": sum(scores) / float(len(scores)) if scores else 0.0,
    }


def validate_attempt(
    attempt: object,
    index: int,
    errors: list[str],
) -> dict[str, Any] | None:
    label = f"attempt {index}"
    error_count_before = len(errors)
    if not require_exact_keys(attempt, ATTEMPT_KEYS, label, errors):
        return None
    assert isinstance(attempt, dict)

    attempt_number = attempt["attempt_number"]
    if not exact_int(attempt_number) or attempt_number != index:
        errors.append(f"{label} numbering is not an exact contiguous integer")
    if type(attempt["route_id"]) is not str or attempt["route_id"] not in ROUTES:
        errors.append(f"{label} has unknown route_id")
    if not valid_utc(attempt["recorded_utc"]):
        errors.append(f"{label} recorded_utc is not a valid UTC timestamp")

    for field in ("landing_zone_hit", "basket_visible", "holed_out_on_route_shot"):
        if type(attempt[field]) is not bool:
            errors.append(f"{label} {field} is not a boolean")
    if type(attempt["miss_side"]) is not str or attempt["miss_side"] not in MISS_SIDES:
        errors.append(f"{label} miss_side is invalid")
    elif type(attempt["landing_zone_hit"]) is bool and (
        attempt["landing_zone_hit"] != (attempt["miss_side"] == "Inside")
    ):
        errors.append(f"{label} landing hit and miss classification disagree")

    for field in (
        "corridor_sample_count", "corridor_samples_inside", "penalty_strokes",
        "tradeoff_understood", "next_shot_clear", "final_hole_score",
        "fixture_contact_count",
    ):
        if not exact_int(attempt[field]):
            errors.append(f"{label} {field} is not an exact integer")

    samples = attempt["corridor_sample_count"]
    inside = attempt["corridor_samples_inside"]
    if exact_int(samples) and samples <= 0:
        errors.append(f"{label} corridor_sample_count must be positive")
    if exact_int(inside) and inside < 0:
        errors.append(f"{label} corridor_samples_inside must be non-negative")
    if exact_int(samples) and exact_int(inside) and inside > samples:
        errors.append(f"{label} corridor sample counts are invalid")
    adherence = attempt["corridor_adherence_percent"]
    if not finite(adherence) or not 0.0 <= float(adherence) <= 100.0:
        errors.append(f"{label} corridor adherence is outside 0-100")
    elif exact_int(samples) and exact_int(inside) and samples > 0:
        expected_adherence = 100.0 * inside / float(samples)
        if not close_number(adherence, expected_adherence):
            errors.append(f"{label} corridor adherence does not match its sample counts")

    for field in (
        "maximum_corridor_deviation_meters", "remaining_distance_meters",
        "final_carry_meters", "release_spin_rpm",
    ):
        value = attempt[field]
        if not finite(value) or value < 0:
            errors.append(f"{label} {field} must be finite and non-negative")
    for field in ("flight_time_seconds", "release_speed_mps"):
        value = attempt[field]
        if not finite(value) or value <= 0:
            errors.append(f"{label} {field} must be finite and positive")
    for field in ("release_hyzer_deg", "release_nose_deg", "release_launch_deg"):
        if not finite(attempt[field]):
            errors.append(f"{label} {field} is not finite")
    quality = attempt["release_quality_01"]
    if not finite(quality) or not 0.0 <= float(quality) <= 1.0:
        errors.append(f"{label} release_quality_01 is outside 0-1")

    penalty_strokes = attempt["penalty_strokes"]
    if exact_int(penalty_strokes) and penalty_strokes < 0:
        errors.append(f"{label} penalty_strokes must be non-negative")
    if type(attempt["penalty_type"]) is not str or attempt["penalty_type"] not in PENALTY_TYPES:
        errors.append(f"{label} penalty_type is invalid")
    elif exact_int(penalty_strokes):
        if (attempt["penalty_type"] == "None") != (penalty_strokes == 0):
            errors.append(f"{label} penalty type and strokes disagree")

    for field in ("tradeoff_understood", "next_shot_clear"):
        if exact_int(attempt[field]) and attempt[field] not in (-1, 0, 1):
            errors.append(f"{label} {field} is not -1/0/1")
    final_score = attempt["final_hole_score"]
    if exact_int(final_score) and final_score != -1 and final_score < 1:
        errors.append(f"{label} final_hole_score must be -1 or a positive integer")
    fixture_count = attempt["fixture_contact_count"]
    if exact_int(fixture_count) and fixture_count < 0:
        errors.append(f"{label} fixture_contact_count must be non-negative")
    if type(attempt["last_fixture_type"]) is not str or attempt["last_fixture_type"] not in FIXTURES:
        errors.append(f"{label} fixture identity is invalid")

    if type(attempt["mold_id"]) is not str or attempt["mold_id"] not in MOLDS:
        errors.append(f"{label} mold_id is invalid")
    if type(attempt["plastic"]) is not str or attempt["plastic"] not in PLASTICS:
        errors.append(f"{label} plastic is invalid")
    if type(attempt["throw_style"]) is not str or attempt["throw_style"] not in THROW_STYLES:
        errors.append(f"{label} throw_style is invalid")

    for field in ("release_location_cm", "raw_final_location_cm", "lie_location_cm"):
        vector = attempt[field]
        if not require_exact_keys(vector, {"x", "y", "z"}, f"{label} {field}", errors):
            continue
        assert isinstance(vector, dict)
        if any(not finite(vector[axis]) for axis in "xyz"):
            errors.append(f"{label} {field} contains a non-finite coordinate")
    return attempt if len(errors) == error_count_before else None


def validate_report(
    report: object,
    require_complete: bool = False,
) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    counts = {route_id: 0 for route_id in ROUTES}
    if not require_exact_keys(report, ROOT_KEYS, "report", errors):
        return errors, counts
    assert isinstance(report, dict)

    if report["schema"] != "disc_golf_route_telemetry" or report["schema_version"] != 1:
        errors.append("schema identity/version is invalid")
    if not exact_int(report["schema_version"]):
        errors.append("schema_version is not an exact integer")
    if report["course_id"] != "PineRidgeChampionship" or report["layout_id"] != "Championship":
        errors.append("course/layout identity is invalid")
    if report["collision_profile_id"] != "PineRidgeCompetitiveV2_Fixtures":
        errors.append("collision profile is not the active fixture revision")
    if report["hole_number"] != 2 or report["target_attempts_per_route"] != 20:
        errors.append("Needle Gate hole/target contract is invalid")
    if not exact_int(report["hole_number"]) or not exact_int(report["target_attempts_per_route"]):
        errors.append("hole/target counts must be exact integers")
    if type(report["complete"]) is not bool:
        errors.append("complete is not a boolean")
    if not isinstance(report["session_id"], str) or not report["session_id"].strip():
        errors.append("session_id is empty or not a string")
    for field in ("started_utc", "updated_utc"):
        if not valid_utc(report[field]):
            errors.append(f"{field} is not a valid UTC timestamp")
    if type(report["active_route_id"]) is not str or report["active_route_id"] not in ROUTES:
        errors.append("active_route_id is invalid")

    routes = report["routes"]
    if not isinstance(routes, list):
        errors.append("routes is not an array")
        routes = []
    route_by_id: dict[str, dict[str, Any]] = {}
    for index, route in enumerate(routes):
        label = f"route {index + 1}"
        if not require_exact_keys(route, ROUTE_KEYS, label, errors):
            continue
        assert isinstance(route, dict)
        route_id = route["route_id"]
        if type(route_id) is not str or route_id not in ROUTE_CONTRACT:
            errors.append(f"{label} has an unknown route_id")
            continue
        if route_id in route_by_id:
            errors.append(f"{label} duplicates route_id {route_id}")
            continue
        route_by_id[route_id] = route
        contract = ROUTE_CONTRACT[route_id]
        for key, expected in contract.items():
            actual = route[key]
            if key in {"target_strokes", "risk_rating", "reward_rating"}:
                if not exact_int(actual) or actual != expected:
                    errors.append(f"{label} {key} differs from the route contract")
            elif key == "corridor_width_meters":
                if not close_number(actual, float(expected)):
                    errors.append(f"{label} {key} differs from the route contract")
            elif actual != expected:
                errors.append(f"{label} {key} differs from the route contract")
    if set(route_by_id) != ROUTES or len(routes) != 3:
        errors.append("exact three-route coverage is missing")

    attempts_value = report["attempts"]
    if not isinstance(attempts_value, list):
        errors.append("attempts is not an array")
        attempts_value = []
    valid_attempts: list[dict[str, Any]] = []
    for index, attempt in enumerate(attempts_value, 1):
        validated = validate_attempt(attempt, index, errors)
        if validated is not None:
            valid_attempts.append(validated)
            route_id = validated["route_id"]
            if route_id in counts:
                counts[route_id] += 1

    if any(count > 20 for count in counts.values()) or len(attempts_value) > 60:
        errors.append("a route or report exceeds the fixed 60-attempt evidence set")

    attempts_by_route = {
        route_id: [attempt for attempt in valid_attempts if attempt["route_id"] == route_id]
        for route_id in ROUTES
    }
    for route_id, route in route_by_id.items():
        summary = route["summary"]
        label = f"{route_id} summary"
        if not require_exact_keys(summary, SUMMARY_KEYS, label, errors):
            continue
        assert isinstance(summary, dict)
        expected = expected_summary(attempts_by_route[route_id])
        for key, expected_value in expected.items():
            actual = summary[key]
            if key in INTEGER_SUMMARY_KEYS:
                if not exact_int(actual) or actual != expected_value:
                    errors.append(f"{label} {key} does not match its attempts")
            elif not close_number(actual, float(expected_value)):
                errors.append(f"{label} {key} does not match its attempts")

    complete = len(attempts_value) == 60 and all(count == 20 for count in counts.values())
    if report["complete"] is not complete:
        errors.append("complete flag does not match exact route counts")
    if complete:
        for index, attempt in enumerate(valid_attempts, 1):
            if attempt["final_hole_score"] < 1:
                errors.append(f"attempt {index} complete evidence lacks a final hole score")
            if attempt["tradeoff_understood"] < 0:
                errors.append(f"attempt {index} complete evidence lacks a tradeoff rating")
            if attempt["next_shot_clear"] < 0:
                errors.append(f"attempt {index} complete evidence lacks a next-shot rating")
    if require_complete and not complete:
        errors.append(f"complete 60-attempt set required; counts={counts}")
    return errors, counts


def build_self_test_report(complete: bool) -> dict[str, Any]:
    attempts: list[dict[str, Any]] = []
    if complete:
        for route_id in ROUTE_CONTRACT:
            for route_index in range(20):
                attempt_number = len(attempts) + 1
                penalized = route_index % 5 == 0
                hit = route_index % 3 == 0
                attempts.append({
                    "attempt_number": attempt_number,
                    "route_id": route_id,
                    "recorded_utc": f"2026-08-26T12:{attempt_number // 60:02d}:{attempt_number % 60:02d}.000Z",
                    "release_location_cm": {"x": 0.0, "y": 0.0, "z": 100.0},
                    "raw_final_location_cm": {"x": 10000.0, "y": 200.0, "z": 30.0},
                    "lie_location_cm": {"x": 10000.0, "y": 200.0, "z": 30.0},
                    "landing_zone_hit": hit,
                    "miss_side": "Inside" if hit else "Short",
                    "corridor_sample_count": 10,
                    "corridor_samples_inside": 8,
                    "corridor_adherence_percent": 80.0,
                    "maximum_corridor_deviation_meters": 1.25,
                    "penalty_type": "Hazard" if penalized else "None",
                    "penalty_strokes": 1 if penalized else 0,
                    "remaining_distance_meters": 42.5,
                    "basket_visible": route_index % 2 == 0,
                    "tradeoff_understood": 1,
                    "next_shot_clear": route_index % 2,
                    "final_hole_score": ROUTE_CONTRACT[route_id]["target_strokes"],
                    "holed_out_on_route_shot": False,
                    "fixture_contact_count": 0,
                    "last_fixture_type": "Unknown",
                    "flight_time_seconds": 5.25,
                    "final_carry_meters": 74.0,
                    "mold_id": "Apex",
                    "plastic": "Tour",
                    "throw_style": "Backhand",
                    "release_speed_mps": 22.0,
                    "release_spin_rpm": 950.0,
                    "release_hyzer_deg": -5.0,
                    "release_nose_deg": -1.0,
                    "release_launch_deg": 10.0,
                    "release_quality_01": 0.85,
                })
    routes: list[dict[str, Any]] = []
    for route_id, contract in ROUTE_CONTRACT.items():
        route_attempts = [attempt for attempt in attempts if attempt["route_id"] == route_id]
        routes.append({"route_id": route_id, **copy.deepcopy(contract), "summary": expected_summary(route_attempts)})
    return {
        "schema": "disc_golf_route_telemetry",
        "schema_version": 1,
        "session_id": "20260826_120000",
        "started_utc": "2026-08-26T12:00:00.000Z",
        "updated_utc": "2026-08-26T12:01:00.000Z",
        "course_id": "PineRidgeChampionship",
        "layout_id": "Championship",
        "collision_profile_id": "PineRidgeCompetitiveV2_Fixtures",
        "hole_number": 2,
        "target_attempts_per_route": 20,
        "active_route_id": "NeedlePlacement",
        "complete": complete,
        "routes": routes,
        "attempts": attempts,
    }


def self_test() -> tuple[int, list[str]]:
    failures: list[str] = []
    count = 0
    complete = build_self_test_report(True)
    partial = build_self_test_report(False)

    def expect_pass(label: str, report: dict[str, Any], require_complete: bool) -> None:
        nonlocal count
        count += 1
        errors, _ = validate_report(report, require_complete)
        if errors:
            failures.append(f"{label} failed: {errors[:3]}")

    def expect_rejected(label: str, mutate: Callable[[dict[str, Any]], None]) -> None:
        nonlocal count
        count += 1
        candidate = copy.deepcopy(complete)
        mutate(candidate)
        errors, _ = validate_report(candidate, True)
        if not errors:
            failures.append(f"mutation survived: {label}")

    expect_pass("complete baseline", complete, True)
    expect_pass("partial baseline", partial, False)
    count += 1
    if not validate_report(partial, True)[0]:
        failures.append("require-complete accepted the partial baseline")

    attempt_mutations: tuple[tuple[str, Callable[[dict[str, Any]], None]], ...] = (
        ("fractional attempt number", lambda r: r["attempts"][0].__setitem__("attempt_number", 1.5)),
        ("fractional sample count", lambda r: r["attempts"][0].__setitem__("corridor_sample_count", 10.5)),
        ("fractional inside count", lambda r: r["attempts"][0].__setitem__("corridor_samples_inside", 7.5)),
        ("fractional penalty strokes", lambda r: r["attempts"][0].__setitem__("penalty_strokes", 1.5)),
        ("fractional final score", lambda r: r["attempts"][0].__setitem__("final_hole_score", 3.5)),
        ("fractional fixture count", lambda r: r["attempts"][0].__setitem__("fixture_contact_count", 0.5)),
        ("negative sample count", lambda r: r["attempts"][0].__setitem__("corridor_sample_count", -1)),
        ("negative inside count", lambda r: r["attempts"][0].__setitem__("corridor_samples_inside", -1)),
        ("negative penalty strokes", lambda r: r["attempts"][0].__setitem__("penalty_strokes", -1)),
        ("negative final score", lambda r: r["attempts"][0].__setitem__("final_hole_score", -2)),
        ("negative fixture count", lambda r: r["attempts"][0].__setitem__("fixture_contact_count", -1)),
        ("negative deviation", lambda r: r["attempts"][0].__setitem__("maximum_corridor_deviation_meters", -0.1)),
        ("negative remaining distance", lambda r: r["attempts"][0].__setitem__("remaining_distance_meters", -0.1)),
        ("negative flight time", lambda r: r["attempts"][0].__setitem__("flight_time_seconds", -0.1)),
        ("negative carry", lambda r: r["attempts"][0].__setitem__("final_carry_meters", -0.1)),
        ("negative release speed", lambda r: r["attempts"][0].__setitem__("release_speed_mps", -0.1)),
        ("negative spin", lambda r: r["attempts"][0].__setitem__("release_spin_rpm", -0.1)),
        ("quality above one", lambda r: r["attempts"][0].__setitem__("release_quality_01", 1.1)),
        ("adherence mismatch", lambda r: r["attempts"][0].__setitem__("corridor_adherence_percent", 79.0)),
        ("landing boolean integer", lambda r: r["attempts"][0].__setitem__("landing_zone_hit", 1)),
        ("route ID non-string", lambda r: r["attempts"][0].__setitem__("route_id", [])),
        ("speed non-number", lambda r: r["attempts"][0].__setitem__("release_speed_mps", "22")),
        ("coordinate non-finite", lambda r: r["attempts"][0]["release_location_cm"].__setitem__("x", math.inf)),
        ("penalty mismatch", lambda r: r["attempts"][0].__setitem__("penalty_type", "None")),
        ("missing final score", lambda r: r["attempts"][0].__setitem__("final_hole_score", -1)),
        ("missing tradeoff rating", lambda r: r["attempts"][0].__setitem__("tradeoff_understood", -1)),
        ("missing next-shot rating", lambda r: r["attempts"][0].__setitem__("next_shot_clear", -1)),
    )
    for label, mutate in attempt_mutations:
        expect_rejected(label, mutate)
    for summary_key in sorted(SUMMARY_KEYS):
        expect_rejected(
            f"summary mismatch {summary_key}",
            lambda report, key=summary_key: report["routes"][0]["summary"].__setitem__(
                key, report["routes"][0]["summary"][key] + 1
            ),
        )
    expect_rejected("route metadata mismatch", lambda r: r["routes"][0].__setitem__("risk_rating", 4))
    expect_rejected("summary integer fractional", lambda r: r["routes"][0]["summary"].__setitem__("attempts", 20.0))
    expect_rejected("extra attempt field", lambda r: r["attempts"][0].__setitem__("approved", True))
    expect_rejected("extra summary field", lambda r: r["routes"][0]["summary"].__setitem__("approved", True))
    count += 1
    try:
        json.loads('{"duplicate": 1, "duplicate": 2}', object_pairs_hook=reject_duplicate_keys)
    except DuplicateKeyError:
        pass
    else:
        failures.append("duplicate JSON member was accepted")
    return count, failures


def load_report(path: Path) -> object:
    return json.loads(
        path.read_text(encoding="utf-8-sig"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda token: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON constant {token}")
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", nargs="?", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--require-complete", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        count, failures = self_test()
        if failures:
            print(f"Route telemetry self-test FAILED: {len(failures)}/{count}")
            for failure in failures:
                print(" -", failure)
            return 1
        print(f"Route telemetry self-test PASS: {count}/{count}")
        return 0

    report_path = args.report if args.report.is_absolute() else ROOT / args.report
    try:
        report = load_report(report_path)
    except Exception as exc:
        print(f"Route telemetry validation FAILED: {exc}")
        return 1

    errors, counts = validate_report(report, args.require_complete)
    if errors:
        print("Route telemetry validation FAILED")
        for error in errors:
            print(" -", error)
        return 1
    assert isinstance(report, dict)
    attempts = report["attempts"]
    state = "complete" if report["complete"] else "partial"
    print(f"Route telemetry report OK: {state}, {len(attempts)}/60 attempts, counts={counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
