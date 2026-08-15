#!/usr/bin/env python3
"""Validate the latest rendered schema-v2 performance capture and recompute its budget result."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CAPTURE = ROOT / "Saved" / "PerformanceCaptures" / "LatestPerformance.json"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def number(value: object, name: str) -> float:
    require(isinstance(value, (int, float)) and math.isfinite(float(value)), f"{name} is not finite")
    return float(value)


def expected_result(summary: dict, budget: dict) -> str:
    samples = int(summary["sample_count"])
    if samples < int(budget["minimum_sample_count"]):
        return "WARMING"
    if (
        summary["p95_frame_ms"] > budget["fail_p95_frame_ms"]
        or summary["hitch_rate_percent"] > budget["max_hitch_rate_percent"]
        or summary["used_physical_bytes"] > budget["max_used_physical_bytes"]
    ):
        return "FAIL"
    if (
        summary["p95_frame_ms"] > budget["warning_p95_frame_ms"]
        or summary["hitch_count"] > 0
        or summary["used_physical_bytes"] > budget["warning_used_physical_bytes"]
    ):
        return "WARN"
    return "PASS"


def validate(path: Path) -> None:
    require(path.is_file(), f"performance capture not found: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    require(data.get("schema") == "disc_golf_performance_capture", "wrong performance schema")
    require(data.get("version") == 2, "unsupported performance schema version")
    require(data.get("result") in {"WARMING", "PASS", "WARN", "FAIL"}, "invalid result")
    require(isinstance(data.get("course_id"), str) and data["course_id"], "missing course identity")
    require(isinstance(data.get("hole_number"), int) and data["hole_number"] >= 0, "invalid hole number")
    require(data.get("capture_profile") == "OmenGameplay1080pHighFoliageV1",
            "capture did not use the Omen gameplay profile")
    require(data.get("camera_route") == "authored_flyover_continuous",
            "capture did not exercise the continuous authored flyover camera route")
    require(number(data.get("warmup_seconds"), "warmup_seconds") >= 10.0,
            "capture warm-up is shorter than 10 seconds")
    require(data.get("rendered") is True, "capture is not rendered evidence")
    require(isinstance(data.get("rhi"), str) and "null" not in data["rhi"].lower(),
            "capture used NullRHI")
    require(isinstance(data.get("gpu_brand"), str) and data["gpu_brand"], "missing GPU identity")
    require(data.get("resolution_x") == 1920 and data.get("resolution_y") == 1080,
            "capture resolution is not 1920x1080")
    require(5.0 <= number(data.get("requested_duration_seconds"), "requested_duration_seconds") <= 300.0,
            "capture duration is out of range")
    require(data.get("runtime_mode") in {"editor_game", "packaged"}, "invalid runtime mode")

    quality = data.get("quality")
    require(isinstance(quality, dict), "missing quality profile")
    expected_quality = {
        "view_distance": 2, "anti_aliasing": 2, "shadow": 2,
        "global_illumination": 2, "reflection": 2, "post_process": 2,
        "texture": 2, "effects": 2, "foliage": 3, "shading": 2,
    }
    require(math.isclose(number(quality.get("resolution_quality"), "quality.resolution_quality"),
                         100.0, abs_tol=0.01), "resolution quality is not 100%")
    for field, expected in expected_quality.items():
        require(number(quality.get(field), f"quality.{field}") == expected,
                f"quality.{field} is not {expected}")

    budget = data.get("budget")
    summary = data.get("summary")
    require(isinstance(budget, dict), "missing budget")
    require(isinstance(summary, dict), "missing summary")

    budget_fields = (
        "target_frame_ms", "warning_p95_frame_ms", "fail_p95_frame_ms",
        "hitch_frame_ms", "max_hitch_rate_percent", "warning_used_physical_bytes",
        "max_used_physical_bytes", "minimum_sample_count", "window_sample_count",
    )
    summary_fields = (
        "sample_count", "average_frame_ms", "p95_frame_ms", "max_frame_ms",
        "average_fps", "hitch_count", "hitch_rate_percent", "used_physical_bytes",
    )
    for field in budget_fields:
        budget[field] = number(budget.get(field), f"budget.{field}")
    for field in summary_fields:
        summary[field] = number(summary.get(field), f"summary.{field}")

    require(0 < budget["target_frame_ms"] <= budget["warning_p95_frame_ms"]
            <= budget["fail_p95_frame_ms"] <= budget["hitch_frame_ms"],
            "frame-time budget thresholds are not monotonic")
    require(0 <= budget["warning_used_physical_bytes"] <= budget["max_used_physical_bytes"],
            "memory thresholds are not monotonic")
    require(1 <= budget["minimum_sample_count"] <= budget["window_sample_count"],
            "sample thresholds are invalid")
    require(0 <= summary["sample_count"] <= budget["window_sample_count"], "sample window is unbounded")
    require(0 <= summary["p95_frame_ms"] <= summary["max_frame_ms"], "P95/max ordering is invalid")
    require(0 <= summary["hitch_count"] <= summary["sample_count"], "hitch count is invalid")
    require(summary["used_physical_bytes"] >= 0, "memory use is negative")
    if summary["sample_count"] > 0:
        expected_hitch_rate = 100.0 * summary["hitch_count"] / summary["sample_count"]
        require(math.isclose(summary["hitch_rate_percent"], expected_hitch_rate, abs_tol=0.01),
                "hitch-rate calculation is inconsistent")
        require(summary["average_frame_ms"] > 0, "average frame time must be positive")
        expected_fps = 1000.0 / summary["average_frame_ms"]
        require(math.isclose(summary["average_fps"], expected_fps, rel_tol=0.001),
                "average FPS is inconsistent with frame time")

    computed = expected_result(summary, budget)
    require(data["result"] == computed, f"reported result {data['result']} does not match {computed}")
    print(
        f"Performance capture OK: {computed}, {int(summary['sample_count'])} samples, "
        f"P95={summary['p95_frame_ms']:.2f} ms, {summary['average_fps']:.1f} FPS, "
        f"memory={summary['used_physical_bytes'] / 1073741824.0:.2f} GiB, "
        f"RHI={data['rhi']}, GPU={data['gpu_brand']}"
    )


def main() -> int:
    path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_CAPTURE
    try:
        validate(path)
    except (AssertionError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"Performance capture validation failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
