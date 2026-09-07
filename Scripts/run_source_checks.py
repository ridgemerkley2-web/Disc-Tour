#!/usr/bin/env python3
"""Run every check that works without Unreal, and report all of them.

The previous runner stopped at the first non-zero exit. On a checkout without
`Saved/` that means `validate_project.py` fails and nothing after it ever runs, so
the checks that DO pass -- the whole physics lane -- are never reported. You cannot
tell "the flight model is fine, the evidence gates need the authoring machine" from
"everything is broken".

This runs all of them, prints a line per check, and still exits non-zero if any
failed. Nothing is skipped or excused: a failure is a failure, it is just reported
alongside the rest instead of hiding what comes after it.

    python Scripts/run_source_checks.py
    python Scripts/run_source_checks.py --fast     # skip the slow mutation harness
    python Scripts/run_source_checks.py --physics  # flight lane only
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]

# (label, script, args, group, slow)
CHECKS = (
    ("reference flight envelope", "reference_flight_check.py", [], "physics", False),
    ("reference flight step-independence self-test", "reference_flight_check.py",
     ["--self-test"], "physics", False),
    ("solver convention parity", "validate_solver_convention_parity.py", [],
     "physics", False),
    ("solver convention parity self-test", "validate_solver_convention_parity.py",
     ["--self-test"], "physics", False),
    ("reference flight mutation harness", "mutation_test_reference_flight.py", [],
     "physics", True),
    ("trajectory artifacts", "validate_trajectory_artifacts.py", [], "physics", False),

    ("C++ convention lint", "lint_cpp_conventions.py", [], "repository", False),
    ("C++ convention lint self-test", "lint_cpp_conventions.py", ["--self-test"],
     "repository", False),
    ("digest-pinned evidence", "audit_dg_digest_pinned_files.py", [], "repository", False),
    ("project validation", "validate_project.py", [], "repository", False),

    ("session 9 brand/license self-test", "validate_dg_session9_brand_license.py",
     ["--self-test"], "gates", False),
    ("session 13 course-authoring self-test",
     "validate_dg_session13_course_authoring_pcg.py", ["--self-test"], "gates", False),
    ("session 13 course-authoring", "validate_dg_session13_course_authoring_pcg.py",
     [], "gates", False),
    ("session 14 career/AI self-test", "validate_dg_session14_career_ai.py",
     ["--self-test"], "gates", False),
    ("session 14 career/AI", "validate_dg_session14_career_ai.py", [], "gates", False),
    ("session 15 vertical-slice self-test", "validate_dg_session15_vertical_slice.py",
     ["--self-test"], "gates", False),
    ("session 15 vertical-slice", "validate_dg_session15_vertical_slice.py", [],
     "gates", False),
    ("session 16 core-playability self-test",
     "validate_dg_session16_core_playability.py", ["--self-test"], "gates", False),
    ("session 16 core-playability", "validate_dg_session16_core_playability.py", [],
     "gates", False),
    ("session 18 provenance self-test",
     "validate_dg_session18_poly_haven_provenance.py", ["--self-test"], "gates", False),
    ("session 18 provenance", "validate_dg_session18_poly_haven_provenance.py", [],
     "gates", False),
    ("session 19 baked PCG self-test", "validate_dg_session19_baked_pcg.py",
     ["--self-test"], "gates", False),
    ("session 19 baked PCG", "validate_dg_session19_baked_pcg.py", [], "gates", False),
    ("session 19 release scope self-test", "validate_dg_session19_release_scope.py",
     ["--self-test"], "gates", False),
    ("session 19 release scope", "validate_dg_session19_release_scope.py", [],
     "gates", False),
)

# Only meaningful once the matching capture exists.
CONDITIONAL = (
    ("presentation capture", "validate_presentation_capture.py",
     "Saved/TrajectoryExports/LatestTrajectory.json"),
    ("performance capture", "validate_performance_capture.py",
     "Saved/PerformanceCaptures/LatestPerformance.json"),
)


def run_one(script: str, args: list[str]) -> tuple[int, str, float]:
    path = ROOT / "Scripts" / script
    if not path.is_file():
        return 127, f"script missing: {script}", 0.0
    started = time.monotonic()
    done = subprocess.run([sys.executable, str(path), *args],
                          capture_output=True, text=True, cwd=str(ROOT))
    elapsed = time.monotonic() - started
    lines = [l.strip() for l in (done.stdout + done.stderr).splitlines() if l.strip()]
    summary = ""
    if done.returncode == 0:
        summary = lines[0] if lines else ""
    else:
        for line in lines:
            if any(word in line for word in ("FAIL", "Error", "error", "missing")):
                summary = line
                break
        if not summary and lines:
            summary = lines[-1]
    return done.returncode, summary[:96], elapsed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fast", action="store_true",
                        help="skip checks marked slow")
    parser.add_argument("--physics", action="store_true",
                        help="run only the flight lane")
    args = parser.parse_args()

    selected = [c for c in CHECKS
                if not (args.fast and c[4])
                and not (args.physics and c[3] != "physics")]

    results = []
    current_group = None
    for label, script, argv, group, _slow in selected:
        if group != current_group:
            print(f"\n{group}")
            current_group = group
        code, summary, elapsed = run_one(script, argv)
        results.append((label, code, summary, group))
        mark = "PASS" if code == 0 else "FAIL"
        print(f"  {mark}  {label:44} {elapsed:5.1f}s  {summary}")

    if not args.physics:
        for label, script, required in CONDITIONAL:
            if (ROOT / required).is_file():
                code, summary, elapsed = run_one(script, [])
                results.append((label, code, summary, "captures"))
                mark = "PASS" if code == 0 else "FAIL"
                print(f"  {mark}  {label:44} {elapsed:5.1f}s  {summary}")
            else:
                print(f"  ....  {label:44}        no capture present")

    failed = [r for r in results if r[1] != 0]
    print(f"\n{len(results)-len(failed)}/{len(results)} checks passed")
    if failed:
        print("\nFailed:")
        for label, _code, summary, _group in failed:
            print(f"  - {label}: {summary}")
        print("\nOn a checkout without Saved/ and _BuildKit/, the evidence-bound "
              "gates cannot pass. Docs/FRESH_CHECKOUT.md explains which ones and "
              "why, and what would be needed to close them.")
        return 1
    print("\nAll source-only checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
