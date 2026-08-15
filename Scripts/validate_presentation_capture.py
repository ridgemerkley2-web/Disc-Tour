#!/usr/bin/env python3
"""Validate that a trajectory export is safe for tracer and replay presentation."""

from __future__ import annotations

import json
import math
import statistics
import sys
from bisect import bisect_left
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CAPTURE = ROOT / "Saved" / "TrajectoryExports" / "LatestTrajectory.json"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def vector(sample: dict, field: str) -> tuple[float, float, float]:
    value = sample[field]
    result = tuple(float(value[axis]) for axis in "xyz")
    require(all(math.isfinite(component) for component in result), f"non-finite {field}")
    return result


def distance_sq(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return sum((left - right) ** 2 for left, right in zip(a, b))


def tracer_indices(samples: list[dict], max_points: int = 320) -> list[int]:
    if not samples or max_points <= 0:
        return []
    accepted = [0]
    if len(samples) == 1 or max_points == 1:
        return accepted
    last = 0
    for index in range(1, len(samples) - 1):
        if len(accepted) >= max_points - 1:
            break
        sample = samples[index]
        previous = samples[index - 1]
        state_boundary = (
            sample["ground_state"] != previous["ground_state"]
            or sample["ground_contact_count"] != previous["ground_contact_count"]
        )
        far_enough = distance_sq(
            vector(sample, "world_location_cm"), vector(samples[last], "world_location_cm")
        ) >= 75.0**2
        old_enough = float(sample["time_s"]) - float(samples[last]["time_s"]) >= 0.075
        if state_boundary or far_enough or old_enough:
            accepted.append(index)
            last = index
    if accepted[-1] != len(samples) - 1:
        accepted.append(len(samples) - 1)
    return accepted


def interpolate(samples: list[dict], playback_s: float) -> tuple[float, float, float]:
    times = [float(sample["time_s"]) for sample in samples]
    target = times[0] + max(playback_s, 0.0)
    upper = min(bisect_left(times, target), len(samples) - 1)
    lower = max(upper - 1, 0) if target > times[0] else 0
    if target >= times[-1]:
        lower = upper = len(samples) - 1
    a = vector(samples[lower], "world_location_cm")
    b = vector(samples[upper], "world_location_cm")
    segment = times[upper] - times[lower]
    alpha = 0.0 if segment <= 1e-9 else max(0.0, min(1.0, (target - times[lower]) / segment))
    return tuple(left + (right - left) * alpha for left, right in zip(a, b))


def main() -> int:
    path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_CAPTURE
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        require(data.get("schema") == "disc_golf_trajectory", "wrong trajectory schema")
        require(data.get("schema_version") == 3, "presentation requires trajectory schema v3")
        samples = data.get("samples")
        require(isinstance(samples, list) and len(samples) >= 2, "replay needs at least two samples")

        times = [float(sample["time_s"]) for sample in samples]
        deltas = [right - left for left, right in zip(times, times[1:]) if right > left]
        require(deltas, "trajectory has no positive time intervals")
        require(all(left <= right for left, right in zip(times, times[1:])), "sample time is not monotonic")
        duration = times[-1] - times[0]
        require(duration > 0.0, "capture duration must be positive")
        require(abs(duration - float(data["summary"]["duration_s"])) <= 0.02,
                "sample duration disagrees with summary")
        median_hz = 1.0 / statistics.median(deltas)
        require(220.0 <= median_hz <= 260.0, f"capture is not near the 240 Hz solver rate: {median_hz:.1f} Hz")

        for index, sample in enumerate(samples):
            normal = vector(sample, "disc_normal_world")
            magnitude = math.sqrt(sum(component * component for component in normal))
            require(0.97 <= magnitude <= 1.03, f"disc normal is not normalized at sample {index}")

        points = tracer_indices(samples)
        require(2 <= len(points) <= 320, "tracer decimation violated its point budget")
        require(points[0] == 0 and points[-1] == len(samples) - 1, "tracer lost a trajectory endpoint")
        require(all(left < right for left, right in zip(points, points[1:])), "tracer indices are not ordered")

        for index in range(1, len(samples)):
            boundary = (
                samples[index]["ground_state"] != samples[index - 1]["ground_state"]
                or samples[index]["ground_contact_count"] != samples[index - 1]["ground_contact_count"]
            )
            if boundary and len(points) < 320:
                require(index in points, f"tracer lost state boundary at sample {index}")

        for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
            replay_position = interpolate(samples, duration * fraction)
            require(all(math.isfinite(component) for component in replay_position),
                    f"replay interpolation failed at {fraction:.0%}")

        print(
            f"Presentation capture OK: {len(samples)} solver samples, {len(points)} tracer points, "
            f"{duration:.3f}s, median {median_hz:.1f} Hz"
        )
        return 0
    except (AssertionError, json.JSONDecodeError, KeyError, OSError, TypeError, ValueError) as exc:
        print(f"Presentation capture FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
