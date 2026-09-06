#!/usr/bin/env python3
"""Prove the reference flight check has teeth, by trying to slip faults past it.

`reference_flight_check.py` passing means nothing on its own: a check that asserts
only what the code already does is indistinguishable from no check at all. This
harness injects known-bad solver behaviour into a scratch copy and requires the
reference check to reject every one of them. A survivor is a hole in the envelope,
not a pass.

The faults are drawn from the failure modes the project's own rules name:
frame-rate dependence, collapsing turn and fade into one same-sign curve,
handedness that does more than mirror through spin, world-frame assumptions in
contact resolution, wind sign errors, and release penalties that reward a worse
throw. Each is a single-line edit to a copy under the system temp directory; the
repository is never modified.

Run after any change to the flight, release or ground models:

    python Scripts/mutation_test_reference_flight.py

Exit code 0 means every fault was caught.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / "Scripts/reference_flight_check.py"

# (name, exact source to replace, replacement). Each must be a real behavioural
# change: a mutant that alters nothing proves nothing, so the harness verifies the
# anchor exists and that the edit actually changed the file.
MUTANTS = (
    ("frame-rate dependence: spin decays per step, not per second",
     "spin *= math.exp(-a.spin_decay*dt)",
     "spin *= math.exp(-a.spin_decay)"),
    ("turn and fade collapse into one same-sign curve",
     "stability_moment = a.turn_moment*turn_w - a.fade_moment*fade_w",
     "stability_moment = a.turn_moment*turn_w + a.fade_moment*fade_w"),
    ("hyzer attitude ignores handedness",
     "normal, forward, rotation_sign*math.radians(effective_hyzer_deg)), normal)",
     "normal, forward, math.radians(effective_hyzer_deg)), normal)"),
    ("handedness does more than mirror: left-handed spin scaled 2%",
     "spin = rotation_sign * release.spin_rpm * 2.0*math.pi/60.0",
     "spin = rotation_sign * release.spin_rpm * 2.0*math.pi/60.0"
     " * (1.02 if handedness == 'left' else 1.0)"),
    ("timing error reaches the solver without going through the release",
     "spin = rotation_sign * release.spin_rpm * 2.0*math.pi/60.0",
     "spin = rotation_sign * release.spin_rpm * 2.0*math.pi/60.0"
     " * (1.0 - 0.05*abs(timing_error))"),
    ("wind sign flipped",
     "air = sub(vel, wind)",
     "air = add(vel, wind)"),
    ("wind ignored entirely",
     "air = sub(vel, wind)",
     "air = vel"),
    ("contact measured against world Z, not the surface normal",
     "normal = norm(surface_normal, (0.0, 0.0, 1.0))",
     "normal = (0.0, 0.0, 1.0)"),
    ("normal/tangent split carries a scale error",
     "tangent_velocity = sub(velocity, mul(normal, signed_normal_speed))",
     "tangent_velocity = sub(velocity, mul(normal, signed_normal_speed*1.02))"),
    ("release penalty is non-monotone: a worse throw is rewarded",
     "speed_multiplier = 1.0-0.16*severity**1.25",
     "speed_multiplier = 1.0-0.16*severity**1.25+0.05*math.sin(6.0*severity)"),
    ("release penalty reshaped by throw style",
     "nose_offset_deg=signed_severity*3.0,",
     "nose_offset_deg=signed_severity*3.0"
     "*(-1.0 if throw_style == 'forehand' else 1.0),"),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verbose", action="store_true",
                        help="print the failure message each mutant produced")
    args = parser.parse_args()

    source = TARGET.read_text(encoding="utf-8")
    workspace = Path(tempfile.mkdtemp(prefix="dgtour_mutation_"))
    survivors, results = [], []
    try:
        for index, (name, old, new) in enumerate(MUTANTS):
            if source.count(old) < 1:
                results.append((name, "ANCHOR GONE", "the mutated line no longer "
                                "exists; update this harness"))
                survivors.append(name)
                continue
            mutated = source.replace(old, new, 1)
            if mutated == source:
                results.append((name, "INERT", "the edit changed nothing"))
                survivors.append(name)
                continue
            path = workspace / f"mutant_{index:02d}.py"
            path.write_text(mutated, encoding="utf-8")
            done = subprocess.run([sys.executable, str(path)],
                                  capture_output=True, text=True)
            detail = ""
            for line in (done.stdout + done.stderr).splitlines():
                if "FAILED" in line or "Error" in line:
                    detail = line.strip()
                    break
            if done.returncode == 0:
                survivors.append(name)
                results.append((name, "SURVIVED",
                                "the check passed a solver that is wrong"))
            else:
                results.append((name, "caught", detail))
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    width = max(len(name) for name, _, _ in results)
    for name, verdict, detail in results:
        line = f"  {name:{width}}  {verdict}"
        if args.verbose or verdict != "caught":
            line += f"  {detail[:100]}"
        print(line)

    caught = len(results) - len(survivors)
    print(f"\nREFERENCE FLIGHT MUTATION TEST: {caught}/{len(results)} faults caught")
    if survivors:
        print("Survivors are holes in the envelope check, not passes:")
        for name in survivors:
            print(f"  - {name}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
