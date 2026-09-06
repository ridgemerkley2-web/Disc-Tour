#!/usr/bin/env python3
"""Read-only readiness audit for measured-flight and route-telemetry capture.

This helper combines the two authoritative validators without generating,
accepting, or modifying evidence.  It is deliberately outside the Session 19
release-scope bindings so it can be used while candidate-bound evidence is in
progress without changing any bound validator hash.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import sys
from typing import Any

import validate_dg_physics_measured_reference as measured
import validate_route_telemetry as routes


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POLICY = ROOT / "Config" / "DG_PhysicsMeasuredReferencePolicy.json"
DEFAULT_DATASET = (
    ROOT / "Evidence" / "Session19" / "PhysicsMeasuredReferenceDataset.template.json"
)
DEFAULT_ROUTE_REPORT = (
    ROOT / "Saved" / "RouteTelemetryReports" / "LatestRouteTelemetry.json"
)
REVIEW_KEYS = (
    "captureOperatorAttested",
    "measurementQualityReviewed",
    "physicsOwnerApproved",
    "playtestOwnerApproved",
    "productOwnerApproved",
)
MEASUREMENT_SYSTEM_KEYS = (
    "releaseSpeed",
    "spin",
    "releaseAttitude",
    "trajectory",
    "wind",
)


def resolve_path(path: Path) -> Path:
    return path if path.is_absolute() else ROOT / path


def physics_progress(policy: dict[str, Any], dataset: dict[str, Any]) -> dict[str, Any]:
    throws_value = dataset.get("throws")
    throws = throws_value if isinstance(throws_value, list) else []
    plan_value = policy.get("samplePlan")
    plan = plan_value if isinstance(plan_value, dict) else {}
    required_total = plan.get("requiredTotalSampleCount")
    required_total = required_total if type(required_total) is int else 0

    cell_counts: Counter[tuple[str, str, str]] = Counter()
    for throw in throws:
        if not isinstance(throw, dict):
            continue
        partition = throw.get("partition")
        category = throw.get("equipmentCategory")
        launch_id = throw.get("launchId")
        if all(isinstance(value, str) for value in (partition, category, launch_id)):
            cell_counts[(partition, category, launch_id)] += 1

    equipment_value = policy.get("equipmentMatrix")
    equipment = equipment_value if isinstance(equipment_value, list) else []
    fit_value = plan.get("fitLaunchIds")
    holdout_value = plan.get("holdoutLaunchIds")
    fit_launches = fit_value if isinstance(fit_value, list) else []
    holdout_launches = holdout_value if isinstance(holdout_value, list) else []
    repeats = plan.get("minimumAcceptedRepeatsPerEquipmentLaunch")
    repeats = repeats if type(repeats) is int else 0

    cells: list[dict[str, Any]] = []
    for equipment_row in equipment:
        if not isinstance(equipment_row, dict) or not isinstance(
            equipment_row.get("category"), str
        ):
            continue
        category = equipment_row["category"]
        for partition, launch_ids in (("fit", fit_launches), ("holdout", holdout_launches)):
            for launch_id in launch_ids:
                if not isinstance(launch_id, str):
                    continue
                observed = cell_counts[(partition, category, launch_id)]
                cells.append(
                    {
                        "partition": partition,
                        "equipmentCategory": category,
                        "launchId": launch_id,
                        "observed": observed,
                        "required": repeats,
                        "remaining": max(0, repeats - observed),
                    }
                )

    review_value = dataset.get("review")
    review = review_value if isinstance(review_value, dict) else {}
    systems_value = dataset.get("measurementSystems")
    systems = systems_value if isinstance(systems_value, dict) else {}
    missing_metadata = [
        key
        for key in ("datasetId", "recordedUtc", "recorder", "location")
        if not measured.nonempty_string(dataset.get(key))
    ]
    if not isinstance(dataset.get("equipmentCalibration"), dict) or not dataset.get(
        "equipmentCalibration"
    ):
        missing_metadata.append("equipmentCalibration")

    return {
        "template": dataset.get("template") is True,
        "observedThrows": len(throws),
        "requiredThrows": required_total,
        "remainingThrows": max(0, required_total - len(throws)),
        "cells": cells,
        "incompleteCells": [cell for cell in cells if cell["remaining"] > 0],
        "missingMetadata": missing_metadata,
        "missingMeasurementSystems": [
            key for key in MEASUREMENT_SYSTEM_KEYS if not measured.nonempty_string(systems.get(key))
        ],
        "pendingReviews": [key for key in REVIEW_KEYS if review.get(key) is not True],
        "acceptedForCalibration": dataset.get("acceptedForCalibration") is True,
    }


def route_progress(report: dict[str, Any], counts: dict[str, int]) -> dict[str, Any]:
    required_per_route = report.get("target_attempts_per_route")
    required_per_route = required_per_route if type(required_per_route) is int else 20
    ordered_route_ids = tuple(routes.ROUTE_CONTRACT)
    per_route = {
        route_id: {
            "observed": counts.get(route_id, 0),
            "required": required_per_route,
            "remaining": max(0, required_per_route - counts.get(route_id, 0)),
        }
        for route_id in ordered_route_ids
    }
    observed = sum(counts.values())
    required = required_per_route * len(ordered_route_ids)
    return {
        "observedAttempts": observed,
        "requiredAttempts": required,
        "remainingAttempts": max(0, required - observed),
        "perRoute": per_route,
        "completeFlag": report.get("complete") is True,
    }


def build_audit(
    policy: dict[str, Any], dataset: dict[str, Any], route_report: dict[str, Any]
) -> dict[str, Any]:
    # The authoritative validator intentionally rejects an
    # acceptedForCalibration=true dataset unless complete mode is requested.
    # Use that same mode for the structural verdict so a valid accepted dataset
    # is not mislabeled invalid, while a premature acceptance claim still fails
    # every completeness requirement.
    physics_structure_errors = measured.validate(
        policy, dataset, dataset.get("acceptedForCalibration") is True
    )
    physics_complete_errors = measured.validate(policy, dataset, True)
    route_structure_errors, route_counts = routes.validate_report(route_report, False)
    route_complete_errors, _ = routes.validate_report(route_report, True)

    structurally_valid = not physics_structure_errors and not route_structure_errors
    capture_complete = not physics_complete_errors and not route_complete_errors
    status = (
        "INVALID_CAPTURE_EVIDENCE"
        if not structurally_valid
        else "CAPTURE_COMPLETE_RELEASE_STILL_REQUIRES_SEPARATE_AUTHORITY"
        if capture_complete
        else "CAPTURE_PENDING"
    )
    return {
        "schema": "DiscGolfTour.PhysicsCaptureReadinessAudit.v1",
        "status": status,
        "structurallyValid": structurally_valid,
        "captureEvidenceComplete": capture_complete,
        "releaseReady": False,
        "physics": {
            **physics_progress(policy, dataset),
            "structuralErrors": physics_structure_errors,
            "completeValidationErrors": physics_complete_errors,
        },
        "routeTelemetry": {
            **route_progress(route_report, route_counts),
            "structuralErrors": route_structure_errors,
            "completeValidationErrors": route_complete_errors,
        },
    }


def print_text(audit: dict[str, Any]) -> None:
    physics = audit["physics"]
    route = audit["routeTelemetry"]
    print(f"Physics capture readiness: {audit['status']}")
    print(
        " - measured throws: "
        f"{physics['observedThrows']}/{physics['requiredThrows']} "
        f"({physics['remainingThrows']} remaining)"
    )
    for cell in physics["incompleteCells"]:
        print(
            "   - "
            f"{cell['partition']} {cell['equipmentCategory']}/{cell['launchId']}: "
            f"{cell['observed']}/{cell['required']}"
        )
    if physics["missingMetadata"]:
        print(" - missing dataset metadata: " + ", ".join(physics["missingMetadata"]))
    if physics["missingMeasurementSystems"]:
        print(
            " - missing measurement-system identities: "
            + ", ".join(physics["missingMeasurementSystems"])
        )
    if physics["pendingReviews"]:
        print(" - pending measured-data reviews: " + ", ".join(physics["pendingReviews"]))
    print(
        " - route attempts: "
        f"{route['observedAttempts']}/{route['requiredAttempts']} "
        f"({route['remainingAttempts']} remaining)"
    )
    for route_id, progress in route["perRoute"].items():
        print(f"   - {route_id}: {progress['observed']}/{progress['required']}")
    for label, errors in (
        ("measured dataset", physics["structuralErrors"]),
        ("route report", route["structuralErrors"]),
    ):
        if errors:
            print(f" - {label} structural errors:")
            for error in errors:
                print("   - " + error)
    print(" - releaseReady: false (capture artifacts never grant release authority by themselves)")


def run_self_test(policy: dict[str, Any], template: dict[str, Any]) -> int:
    cases = 0
    failures: list[str] = []

    pending = build_audit(policy, template, routes.build_self_test_report(False))
    cases += 1
    if not (
        pending["status"] == "CAPTURE_PENDING"
        and pending["structurallyValid"] is True
        and pending["physics"]["remainingThrows"] == 100
        and pending["routeTelemetry"]["remainingAttempts"] == 60
        and pending["releaseReady"] is False
    ):
        failures.append("valid empty capture state was not reported as 100/60 pending")

    complete_dataset = measured.make_valid_complete_fixture(policy)
    complete = build_audit(policy, complete_dataset, routes.build_self_test_report(True))
    cases += 1
    if not (
        complete["status"]
        == "CAPTURE_COMPLETE_RELEASE_STILL_REQUIRES_SEPARATE_AUTHORITY"
        and complete["captureEvidenceComplete"] is True
        and complete["physics"]["remainingThrows"] == 0
        and complete["routeTelemetry"]["remainingAttempts"] == 0
        and complete["releaseReady"] is False
    ):
        failures.append("valid synthetic complete fixtures were not bounded correctly")

    invalid_dataset = dict(template)
    invalid_dataset["releaseReady"] = True
    invalid = build_audit(policy, invalid_dataset, routes.build_self_test_report(False))
    cases += 1
    if invalid["status"] != "INVALID_CAPTURE_EVIDENCE":
        failures.append("invalid releaseReady overclaim was not rejected")

    if failures:
        print(f"Physics capture readiness self-test FAILED: {len(failures)}/{cases}")
        for failure in failures:
            print(" - " + failure)
        return 1
    print(f"Physics capture readiness self-test PASS: {cases}/{cases}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--dataset", type=Path, default=DEFAULT_DATASET)
    parser.add_argument("--route-report", type=Path, default=DEFAULT_ROUTE_REPORT)
    parser.add_argument("--require-complete", action="store_true")
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    policy_path = resolve_path(args.policy)
    dataset_path = resolve_path(args.dataset)
    route_path = resolve_path(args.route_report)
    try:
        policy = measured.load_json(policy_path)
        dataset = measured.load_json(dataset_path)
    except Exception as exc:
        print(f"Physics capture readiness audit FAILED: {exc}")
        return 1

    if args.self_test:
        return run_self_test(policy, dataset)

    try:
        route_report = routes.load_report(route_path)
        if not isinstance(route_report, dict):
            raise ValueError(f"{route_path} is not a JSON object")
    except Exception as exc:
        print(f"Physics capture readiness audit FAILED: {exc}")
        return 1

    audit = build_audit(policy, dataset, route_report)
    audit["inputs"] = {
        "policy": str(policy_path),
        "dataset": str(dataset_path),
        "routeReport": str(route_path),
    }
    if args.as_json:
        print(json.dumps(audit, indent=2, sort_keys=True))
    else:
        print_text(audit)

    if not audit["structurallyValid"]:
        return 1
    if args.require_complete and not audit["captureEvidenceComplete"]:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
