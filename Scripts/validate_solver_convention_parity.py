#!/usr/bin/env python3
"""Check that the reference model and the authoritative C++ solver still agree.

`reference_flight_check.py` is a Python mirror of the Unreal solver, and every
invariant asserted there tests the mirror. If one side is edited and the other is
not, those assertions keep passing while the shipped game drifts away from the
thing being tested. On a machine without Unreal the C++ cannot be compiled or run,
so the only available cross-check is a structural one: read both sources and
verify they state the same conventions.

What this proves is narrow and worth stating plainly. It shows the two files SAY
the same thing. It does not show the compiled solver DOES it, and a defect inside
a function this script does not inspect passes unnoticed. It is worth running
because the alternative is no cross-check at all, not because it is strong.

Absence is a failure. If a refactor renames a convention this script looks for,
it reports the convention as missing rather than quietly finding nothing to
compare and passing.

    python Scripts/validate_solver_convention_parity.py
    python Scripts/validate_solver_convention_parity.py --self-test
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
MATH_HEADER = ROOT / "Source/DiscGolfTour/DiscGolfMath.h"
FLIGHT_COMPONENT = ROOT / "Source/DiscGolfTour/DiscFlightComponent.cpp"
REFERENCE = ROOT / "Scripts/reference_flight_check.py"


class Parity:
    """Collects findings so every convention is reported, not just the first."""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.checked = 0

    def found(self, label: str, pattern: str, text: str, source: Path):
        """Return the first capture of pattern, or record that it has gone."""
        match = re.search(pattern, text, re.MULTILINE)
        if match is None:
            self.errors.append(
                f"{label}: convention not found in {source.name}. It was either "
                "renamed or removed; this parity check cannot confirm anything "
                "until it is updated to match."
            )
            return None
        return match.group(1) if match.groups() else match.group(0)

    def same(self, label: str, cpp, python, note: str = "") -> None:
        self.checked += 1
        if cpp is None or python is None:
            return
        if cpp != python:
            detail = f" ({note})" if note else ""
            self.errors.append(
                f"{label}: C++ says {cpp!r}, reference model says {python!r}"
                f"{detail}"
            )


def check(math_source: str, flight_source: str, reference: str) -> Parity:
    parity = Parity()

    # Handedness and style are a sign convention shared by both solvers. Backhand
    # and right hand are the positive baseline; the product mirrors the other two.
    cpp_style = parity.found(
        "throw-style sign",
        r"StyleSign\s*=\s*ThrowStyle\s*==\s*EThrowStyle::Backhand\s*\?\s*"
        r"(-?[\d.]+)f",
        math_source, MATH_HEADER)
    py_style = parity.found(
        "throw-style sign",
        r'style_sign\s*=\s*(-?[\d.]+)\s*if\s*throw_style\s*==\s*"backhand"',
        reference, REFERENCE)
    parity.same("throw-style sign (backhand)", _num(cpp_style), _num(py_style))

    cpp_hand = parity.found(
        "handedness sign",
        r"HandednessSign\s*=\s*Handedness\s*==\s*EDGHandedness::Right\s*\?\s*"
        r"(-?[\d.]+)f",
        math_source, MATH_HEADER)
    py_hand = parity.found(
        "handedness sign",
        r'handedness_sign\s*=\s*(-?[\d.]+)\s*if\s*handedness\s*==\s*"right"',
        reference, REFERENCE)
    parity.same("handedness sign (right)", _num(cpp_hand), _num(py_hand))

    # Turn and fade must subtract, never add. This is the one convention whose
    # violation the reference check now catches behaviourally as well.
    cpp_stability = parity.found(
        "turn/fade opposition",
        r"return\s+HighSpeedTurnMomentNm\s*\*\s*TurnWeight\s*(-|\+)\s*"
        r"LowSpeedFadeMomentNm\s*\*\s*FadeWeight",
        math_source, MATH_HEADER)
    py_stability = parity.found(
        "turn/fade opposition",
        r"stability_moment\s*=\s*a\.turn_moment\*turn_w\s*(-|\+)\s*"
        r"a\.fade_moment\*fade_w",
        reference, REFERENCE)
    parity.same("turn/fade opposition operator", cpp_stability, py_stability,
                "they are opposing effects and must subtract")

    cpp_turn_band = parity.found(
        "turn weight band",
        r"TurnWeight\s*=\s*FMath::Clamp\(\(SpeedMps\s*-\s*TurnStartsAboveMps\)\s*"
        r"/\s*([\d.]+)f",
        math_source, MATH_HEADER)
    py_turn_band = parity.found(
        "turn weight band",
        r"turn_w\s*=\s*clamp\(\(speed-a\.turn_above\)/([\d.]+)",
        reference, REFERENCE)
    parity.same("turn weight band width", _num(cpp_turn_band), _num(py_turn_band))

    cpp_fade_band = parity.found(
        "fade weight band",
        r"FadeWeight\s*=\s*FMath::Clamp\(\(FadeStartsBelowMps\s*-\s*SpeedMps\)\s*"
        r"/\s*([\d.]+)f",
        math_source, MATH_HEADER)
    py_fade_band = parity.found(
        "fade weight band",
        r"fade_w\s*=\s*clamp\(\(a\.fade_below-speed\)/([\d.]+)",
        reference, REFERENCE)
    parity.same("fade weight band width", _num(cpp_fade_band), _num(py_fade_band))

    # Spin decay is a rate per second. Dropping the step from either side is the
    # frame-rate bug the step-independence guard exists to catch.
    cpp_decay = parity.found(
        "spin decay is per second",
        r"SpinRateRadPerSec\s*\*=\s*FMath::Exp\(-Disc\.Aero\.SpinDecayPerSecond"
        r"\s*\*\s*(\w+)\)",
        flight_source, FLIGHT_COMPONENT)
    py_decay = parity.found(
        "spin decay is per second",
        r"spin\s*\*=\s*math\.exp\(-a\.spin_decay\*(\w+)\)",
        reference, REFERENCE)
    if cpp_decay is not None and cpp_decay.lower() not in {"dt", "deltatime"}:
        parity.errors.append(
            f"spin decay is per second: C++ scales by {cpp_decay!r}, which is not "
            "the fixed step. A decay applied per step is frame-rate dependent.")
    if py_decay is not None and py_decay != "dt":
        parity.errors.append(
            f"spin decay is per second: reference model scales by {py_decay!r}, "
            "not the step.")

    # Relative airflow is disc velocity minus wind, in both solvers.
    parity.checked += 1
    if "InitialVelocityMps - InitialWindMps" not in flight_source:
        parity.errors.append(
            "relative airflow: C++ no longer computes velocity minus wind. A sign "
            "error here reverses every wind effect in the game.")
    if "air = sub(vel, wind)" not in reference:
        parity.errors.append(
            "relative airflow: reference model no longer computes velocity minus "
            "wind.")

    # Shared scalar constants.
    cpp_perfect = parity.found(
        "perfect-release band",
        r"constexpr\s+float\s+ReleasePerfectError\s*=\s*([\d.]+)f",
        math_source, MATH_HEADER)
    py_perfect = parity.found(
        "perfect-release band",
        r"RELEASE_PERFECT_ERROR\s*=\s*([\d.]+)",
        reference, REFERENCE)
    parity.same("perfect-release band", _num(cpp_perfect), _num(py_perfect))

    for label, cpp_name, py_pattern in (
        ("hyzer minimum", "ThrowCommandMinimumHyzerDeg",
         r"effective_hyzer_deg\s*=\s*clamp\([^,]+,\s*(-?[\d.]+),"),
        ("hyzer maximum", "ThrowCommandMaximumHyzerDeg",
         r"effective_hyzer_deg\s*=\s*clamp\([^,]+,\s*-?[\d.]+,\s*(-?[\d.]+)\)"),
        ("nose minimum", "ThrowCommandMinimumNoseAngleDeg",
         r"effective_nose_deg\s*=\s*clamp\([^,]+,\s*(-?[\d.]+),"),
        ("nose maximum", "ThrowCommandMaximumNoseAngleDeg",
         r"effective_nose_deg\s*=\s*clamp\([^,]+,\s*-?[\d.]+,\s*(-?[\d.]+)\)"),
        ("launch minimum", "ThrowCommandMinimumLaunchAngleDeg",
         r"effective_launch_deg\s*=\s*clamp\([^,]+,\s*(-?[\d.]+),"),
        ("launch maximum", "ThrowCommandMaximumLaunchAngleDeg",
         r"effective_launch_deg\s*=\s*clamp\([^,]+,\s*-?[\d.]+,\s*(-?[\d.]+)\)"),
    ):
        cpp_value = parity.found(
            label, rf"constexpr\s+float\s+{cpp_name}\s*=\s*(-?[\d.]+)f",
            math_source, MATH_HEADER)
        py_value = parity.found(label, py_pattern, reference, REFERENCE)
        parity.same(label, _num(cpp_value), _num(py_value))

    # The reference sweep must stay inside the step band the engine will accept,
    # or it is proving convergence at rates the game refuses to run.
    minimum = _num(parity.found(
        "minimum fixed step",
        r"constexpr\s+float\s+MinimumFixedStepSeconds\s*=\s*([\d.]+)f",
        flight_source, FLIGHT_COMPONENT))
    maximum_expr = parity.found(
        "maximum fixed step",
        r"constexpr\s+float\s+MaximumFixedStepSeconds\s*=\s*([\d.\s/f]+);",
        flight_source, FLIGHT_COMPONENT)
    maximum = None
    if maximum_expr is not None:
        cleaned = maximum_expr.replace("f", "").strip()
        try:
            maximum = eval(cleaned, {"__builtins__": {}})  # noqa: S307 - literal
        except Exception:
            parity.errors.append(
                f"maximum fixed step: cannot read {maximum_expr!r} from C++")

    rates = parity.found(
        "convergence sweep rates",
        r"CONVERGENCE_RATES_HZ\s*=\s*\(([\d,\s]+)\)", reference, REFERENCE)
    if rates is not None and minimum is not None and maximum is not None:
        parity.checked += 1
        for rate in [int(r) for r in rates.replace(" ", "").split(",") if r]:
            step = 1.0/rate
            if step < minimum - 1e-9 or step > maximum + 1e-9:
                parity.errors.append(
                    f"convergence sweep runs {rate} Hz (step {step:.6f} s), "
                    f"outside the step band the engine accepts "
                    f"[{minimum}, {maximum:.6f}]. The reference model would be "
                    "proving convergence at a rate the game refuses to run.")

    default_step = _num(parity.found(
        "reference default step",
        r"^DT\s*=\s*1\.0\s*/\s*([\d.]+)", reference, REFERENCE))
    if default_step is not None and minimum is not None and maximum is not None:
        parity.checked += 1
        step = 1.0/default_step
        if step < minimum - 1e-9 or step > maximum + 1e-9:
            parity.errors.append(
                f"reference default step {step:.6f} s is outside the engine band "
                f"[{minimum}, {maximum:.6f}]")

    return parity


def _num(value):
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return value


def run() -> Parity:
    return check(
        MATH_HEADER.read_text(encoding="utf-8"),
        FLIGHT_COMPONENT.read_text(encoding="utf-8"),
        REFERENCE.read_text(encoding="utf-8"),
    )


# Each entry breaks parity on one side only. The check must notice all of them,
# including the two that delete a convention rather than change it.
SELF_TEST_MUTATIONS = (
    ("turn/fade collapsed in C++", "math",
     "return HighSpeedTurnMomentNm * TurnWeight - LowSpeedFadeMomentNm * FadeWeight",
     "return HighSpeedTurnMomentNm * TurnWeight + LowSpeedFadeMomentNm * FadeWeight"),
    ("handedness sign flipped in C++", "math",
     "Handedness == EDGHandedness::Right ? 1.0f : -1.0f",
     "Handedness == EDGHandedness::Right ? -1.0f : 1.0f"),
    ("hyzer clamp widened in C++ only", "math",
     "constexpr float ThrowCommandMaximumHyzerDeg = 34.0f;",
     "constexpr float ThrowCommandMaximumHyzerDeg = 45.0f;"),
    ("perfect band changed in C++ only", "math",
     "constexpr float ReleasePerfectError = 0.12f;",
     "constexpr float ReleasePerfectError = 0.20f;"),
    ("spin decay no longer scaled by the step", "flight",
     "FMath::Exp(-Disc.Aero.SpinDecayPerSecond * Dt)",
     "FMath::Exp(-Disc.Aero.SpinDecayPerSecond)"),
    ("wind subtraction removed", "flight",
     "InitialVelocityMps - InitialWindMps",
     "InitialVelocityMps"),
    ("stability helper renamed away", "math",
     "return HighSpeedTurnMomentNm * TurnWeight - LowSpeedFadeMomentNm * FadeWeight",
     "return ComputeStability(HighSpeedTurnMomentNm, TurnWeight, LowSpeedFadeMomentNm, FadeWeight)"),
)


def self_test() -> int:
    math_source = MATH_HEADER.read_text(encoding="utf-8")
    flight_source = FLIGHT_COMPONENT.read_text(encoding="utf-8")
    reference = REFERENCE.read_text(encoding="utf-8")

    clean = check(math_source, flight_source, reference)
    if clean.errors:
        print("SELF-TEST ABORTED: parity is already broken:")
        for error in clean.errors:
            print(f"  - {error}")
        return 1

    survivors = []
    for name, side, old, new in SELF_TEST_MUTATIONS:
        source = math_source if side == "math" else flight_source
        if source.count(old) < 1:
            print(f"  {name}: ANCHOR GONE -- update this self-test")
            survivors.append(name)
            continue
        mutated = source.replace(old, new, 1)
        result = check(
            mutated if side == "math" else math_source,
            mutated if side == "flight" else flight_source,
            reference,
        )
        if result.errors:
            print(f"  {name}: caught")
        else:
            print(f"  {name}: SURVIVED")
            survivors.append(name)

    print(f"\nSOLVER CONVENTION PARITY SELF-TEST: "
          f"{len(SELF_TEST_MUTATIONS)-len(survivors)}/{len(SELF_TEST_MUTATIONS)} "
          "divergences caught")
    return 1 if survivors else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true",
                        help="prove the check notices a divergence")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    parity = run()
    if parity.errors:
        print("Solver convention parity FAILED")
        for error in parity.errors:
            print(f" - {error}")
        print(f"\n{len(parity.errors)} divergence(s) across {parity.checked} "
              "compared conventions.")
        return 1

    print("Solver convention parity OK")
    print(f"  {parity.checked} conventions agree between "
          f"{MATH_HEADER.name}/{FLIGHT_COMPONENT.name} and {REFERENCE.name}")
    print("  This compares what the sources state, not what the compiled "
          "solver does.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
