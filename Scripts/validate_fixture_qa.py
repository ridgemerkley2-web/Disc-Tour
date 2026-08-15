#!/usr/bin/env python3
"""Validate the latest live disc-versus-course-fixture QA matrix."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LATEST_REPORT = ROOT / "Saved" / "FixtureQaReports" / "LatestFixtureQa.json"
FIXTURE_TYPES = {"Tree", "Rock", "Sign", "DenseGrass"}
SPEED_CLASSES = {"Low", "Medium", "Drive"}
CONTACT_ANGLES = {"HeadOn", "Glancing"}
SPIN_RETENTION = {"Tree": 0.56, "Rock": 0.82, "Sign": 0.68, "DenseGrass": 0.64}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def vector(value: object, label: str) -> tuple[float, float, float]:
    require(isinstance(value, dict), f"{label} is not a vector")
    require(all(finite_number(value.get(axis)) for axis in "xyz"), f"{label} is not finite")
    return tuple(float(value[axis]) for axis in "xyz")


def magnitude(value: tuple[float, float, float]) -> float:
    return math.sqrt(sum(component * component for component in value))


def distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return magnitude(tuple(left - right for left, right in zip(a, b)))


def validate_report(path: Path) -> int:
    require(path.is_file(), f"missing fixture QA report: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_fixture_qa", "wrong fixture QA schema")
    require(data.get("schema_version") == 1, "unsupported fixture QA schema version")
    require(data.get("collision_profile_id") == "PineRidgeCompetitiveV2_Fixtures",
            "unexpected collision profile")
    require(data.get("passed") is True, "fixture QA report failed")
    require(data.get("scenario_count") == 24, "fixture QA must contain 24 scenarios")
    require(data.get("passed_count") == 24, "not every fixture QA scenario passed")
    results = data.get("results")
    require(isinstance(results, list) and len(results) == 24, "fixture QA result count is not 24")

    combinations: set[tuple[str, str, str]] = set()
    for result in results:
        scenario_id = result.get("scenario_id", "<missing>")
        fixture_type = result.get("fixture_type")
        speed_class = result.get("speed_class")
        angle = result.get("contact_angle")
        require(fixture_type in FIXTURE_TYPES, f"invalid fixture type in {scenario_id}")
        require(speed_class in SPEED_CLASSES, f"invalid speed class in {scenario_id}")
        require(angle in CONTACT_ANGLES, f"invalid contact angle in {scenario_id}")
        combination = (fixture_type, speed_class, angle)
        require(combination not in combinations, f"duplicate fixture QA combination: {combination}")
        combinations.add(combination)

        require(result.get("passed") is True, f"scenario failed: {scenario_id}")
        require(result.get("failure_reason") == "", f"scenario has a failure reason: {scenario_id}")
        require(result.get("fixture_contact_count") == 1, f"contact count is not one: {scenario_id}")
        entry = vector(result.get("entry_velocity_mps"), f"{scenario_id} entry velocity")
        exit_velocity = vector(result.get("exit_velocity_mps"), f"{scenario_id} exit velocity")
        expected = vector(result.get("expected_exit_velocity_mps"), f"{scenario_id} expected velocity")
        normal = vector(result.get("impact_normal"), f"{scenario_id} impact normal")
        entry_speed = float(result.get("entry_speed_mps", -1.0))
        exit_speed = float(result.get("exit_speed_mps", -1.0))
        require(entry_speed > 0.0 and exit_speed > 0.0, f"non-positive speed in {scenario_id}")
        require(math.isclose(magnitude(entry), entry_speed, abs_tol=0.02),
                f"entry speed/vector mismatch in {scenario_id}")
        require(math.isclose(magnitude(exit_velocity), exit_speed, abs_tol=0.02),
                f"exit speed/vector mismatch in {scenario_id}")
        require(distance(exit_velocity, expected) <= 0.03,
                f"live and deterministic responses differ in {scenario_id}")
        require(0.0 < result.get("speed_retention", 0.0) < 1.0,
                f"invalid speed retention in {scenario_id}")
        require(math.isclose(result.get("spin_retention", -1.0), SPIN_RETENTION[fixture_type],
                             abs_tol=0.002), f"invalid spin retention in {scenario_id}")

        if fixture_type == "DenseGrass":
            require(magnitude(normal) <= 0.001, f"grass should use overlap response in {scenario_id}")
            require(math.isclose(result["speed_retention"], 0.56, abs_tol=0.002),
                    f"grass pass-through retention changed in {scenario_id}")
        else:
            require(math.isclose(magnitude(normal), 1.0, abs_tol=0.002),
                    f"blocking impact normal is not normalized in {scenario_id}")
            tangent_ratio = result.get("tangential_entry_ratio", -1.0)
            require(finite_number(tangent_ratio) and 0.0 <= tangent_ratio <= 1.0,
                    f"invalid contact-angle metric in {scenario_id}")
            require(tangent_ratio > 0.30 if angle == "Glancing" else tangent_ratio < 0.50,
                    f"contact-angle class was not achieved in {scenario_id}")

    expected_combinations = {
        (fixture_type, speed_class, angle)
        for fixture_type in FIXTURE_TYPES
        for speed_class in SPEED_CLASSES
        for angle in CONTACT_ANGLES
    }
    require(combinations == expected_combinations, "fixture QA matrix is incomplete")
    return len(results)


def main() -> int:
    try:
        path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else LATEST_REPORT
        scenario_count = validate_report(path)
        print(f"Fixture QA report OK: {scenario_count}/24 live scenarios")
        return 0
    except (AssertionError, json.JSONDecodeError, KeyError, TypeError, ValueError) as exc:
        print(f"Fixture QA validation FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
