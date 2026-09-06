#!/usr/bin/env python3
"""Validate the bounded Session 19 normal-play Shipping I/O exclusions."""

from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "bag": ROOT / "Source/DiscGolfTour/DiscBagComponent.cpp",
    "course": ROOT / "Source/DiscGolfTour/DevCourseBootstrap.cpp",
    "trajectory": ROOT / "Source/DiscGolfTour/DiscTrajectorySubsystem.cpp",
    "game_instance": ROOT / "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
    "game_mode": ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "game_mode_header": ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.h",
}
BUILD_FILE = ROOT / "Source/DiscGolfTour/DiscGolfTour.Build.cs"


def evaluate_condition(expression: str, symbols: dict[str, bool]) -> bool:
    value = expression.split("//", 1)[0].strip()
    if value in ("0", "false"):
        return False
    if value in ("1", "true"):
        return True
    defined = re.fullmatch(r"defined\s*\(\s*([A-Za-z_]\w*)\s*\)", value)
    if defined:
        return symbols.get(defined.group(1), False)
    negated_defined = re.fullmatch(
        r"!\s*defined\s*\(\s*([A-Za-z_]\w*)\s*\)", value
    )
    if negated_defined:
        return not symbols.get(negated_defined.group(1), False)
    negated = re.fullmatch(r"!\s*([A-Za-z_]\w*)", value)
    if negated:
        return not symbols.get(negated.group(1), False)
    identifier = re.fullmatch(r"([A-Za-z_]\w*)", value)
    if identifier:
        return symbols.get(identifier.group(1), False)
    raise ValueError(f"unsupported preprocessor condition: {expression!r}")


def active_source(text: str, symbols: dict[str, bool]) -> str:
    output: list[str] = []
    # Each frame stores (parent active, original condition, current active,
    # else already seen). Only the simple compile definitions used by these
    # files are accepted; unexpected expressions fail the validator closed.
    stack: list[tuple[bool, bool, bool, bool]] = []
    active = True
    for line_number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("#if "):
            condition = evaluate_condition(stripped[4:], symbols)
            stack.append((active, condition, active and condition, False))
            active = active and condition
            continue
        if stripped.startswith("#ifdef "):
            name = stripped[7:].strip()
            condition = symbols.get(name, False)
            stack.append((active, condition, active and condition, False))
            active = active and condition
            continue
        if stripped.startswith("#ifndef "):
            name = stripped[8:].strip()
            condition = not symbols.get(name, False)
            stack.append((active, condition, active and condition, False))
            active = active and condition
            continue
        if stripped == "#else":
            if not stack:
                raise ValueError(f"orphan #else at line {line_number}")
            parent, condition, _, seen_else = stack[-1]
            if seen_else:
                raise ValueError(f"duplicate #else at line {line_number}")
            active = parent and not condition
            stack[-1] = (parent, condition, active, True)
            continue
        if stripped == "#endif":
            if not stack:
                raise ValueError(f"orphan #endif at line {line_number}")
            parent, _, _, _ = stack.pop()
            active = parent
            continue
        if stripped.startswith("#elif"):
            raise ValueError(f"unsupported #elif at line {line_number}")
        if active:
            output.append(line)
    if stack:
        raise ValueError("unterminated preprocessor branch")
    return "\n".join(output)


def run_self_tests() -> list[str]:
    failures: list[str] = []
    fixture = """before
#if DG_RELEASE_V05_SCOPE
release
#if DG_WITH_RELEASE_PERFORMANCE_CAPTURE
release_performance
#endif
#else
development
#if DG_WITH_DEVELOPMENT_CONTENT
devtools
#endif
#endif
after
"""
    release = active_source(fixture, {
        "DG_RELEASE_V05_SCOPE": True,
        "DG_WITH_DEVELOPMENT_CONTENT": False,
        "DG_WITH_RELEASE_PERFORMANCE_CAPTURE": True,
    })
    development = active_source(fixture, {
        "DG_RELEASE_V05_SCOPE": False,
        "DG_WITH_DEVELOPMENT_CONTENT": True,
        "DG_WITH_RELEASE_PERFORMANCE_CAPTURE": False,
    })
    if release.splitlines() != ["before", "release", "release_performance", "after"]:
        failures.append("release preprocessor fixture differed")
    if development.splitlines() != ["before", "development", "devtools", "after"]:
        failures.append("development preprocessor fixture differed")
    for expression, expected in (
        ("0", False), ("1", True), ("!DG_RELEASE_V05_SCOPE", False),
        ("defined(WITH_EDITOR)", False),
    ):
        actual = evaluate_condition(expression, {
            "DG_RELEASE_V05_SCOPE": True,
            "WITH_EDITOR": False,
        })
        if actual != expected:
            failures.append(f"condition fixture differed: {expression}")
    return failures


def main() -> int:
    failures = run_self_tests()
    sources = {name: path.read_text(encoding="utf-8-sig") for name, path in FILES.items()}
    release_symbols = {
        "DG_RELEASE_V05_SCOPE": True,
        "DG_WITH_DEVELOPMENT_CONTENT": False,
        "DG_WITH_RELEASE_PERFORMANCE_CAPTURE": True,
        "WITH_EDITOR": False,
        "WITH_DEV_AUTOMATION_TESTS": False,
    }
    development_symbols = {
        "DG_RELEASE_V05_SCOPE": False,
        "DG_WITH_DEVELOPMENT_CONTENT": True,
        "DG_WITH_RELEASE_PERFORMANCE_CAPTURE": False,
        "WITH_EDITOR": False,
        "WITH_DEV_AUTOMATION_TESTS": True,
    }
    try:
        release = {name: active_source(text, release_symbols) for name, text in sources.items()}
        development = {
            name: active_source(text, development_symbols) for name, text in sources.items()
        }
    except ValueError as exc:
        failures.append(str(exc))
        release = {}
        development = {}

    if release:
        combined_release = "\n".join(release.values())
        for token in (
            "DGT_Equipment_Dev_v1",
            "EnvironmentReports",
            "TrajectoryExports",
        ):
            if token in combined_release:
                failures.append(f"Shipping-active source retains forbidden output token: {token}")
        for token in ("SaveGameToSlot", "LoadGameFromSlot", "DoesSaveGameExist"):
            if token in release["bag"]:
                failures.append(f"Shipping-active equipment source retains I/O call: {token}")
        for token in (
            "In-memory trajectory summaries ready",
            "LastSummary = OutSummary;",
            "bHasLastCapture = true;",
        ):
            if token not in release["trajectory"]:
                failures.append(f"Shipping in-memory trajectory postcondition missing: {token}")
        for token in (
            "IN-MEMORY TRAJECTORY SUMMARY READY",
            "CAPTURED %d samples",
        ):
            if token not in release["game_mode"]:
                failures.append(f"Shipping HUD status boundary missing: {token}")
        for token in (
            "PerformanceCaptureSeconds=",
            "RELEASE PERFORMANCE CAPTURE REJECTED:",
            "ReleasePerformanceCapture.guard",
            "release performance capture requires a fresh external UUID UserDir",
            "release performance capture requires exact Pine Ridge hole, duration, resolution, offscreen, and D3D12 flags",
            "-NoLoadExistingSave",
            "-DGNoProfileWrites",
            "Arguments.Num() == 12",
            "bUnknownArgument",
            "CapturePerformanceSnapshotInternal",
            "ResetPerformanceTelemetryInternal",
            "Shipping public performance snapshot rejected.",
            "Shipping public performance telemetry reset rejected.",
        ):
            if token not in release["game_mode"]:
                failures.append(f"Shipping release-performance guard missing: {token}")
        for token in (
            "bReleasePerformanceAttempt",
            'TEXT("-PerformanceCapture"), ESearchCase::IgnoreCase',
            "!bReleasePerformanceAttempt",
            "DG_WITH_RELEASE_PERFORMANCE_CAPTURE != 0",
        ):
            if token not in release["game_instance"]:
                failures.append(
                    f"Shipping early profile-persistence guard missing: {token}"
                )
        if release["game_mode"].count(
                "IsPublicPerformanceCaptureMutationAllowed(DG_WITH_DEVELOPMENT_CONTENT != 0)"
        ) != 2:
            failures.append(
                "Shipping public performance mutation wrappers are not both fail-closed"
            )
        if release["game_mode"].count("CapturePerformanceSnapshotInternal();") != 2 \
                or release["game_mode"].count("ResetPerformanceTelemetryInternal();") != 2:
            failures.append(
                "Shipping private performance sequence authority differs"
            )
        for token in (
            'TEXT("RegressionSuiteSmokeTest")',
            'TEXT("NeedleGateRouteTelemetry")',
            'TEXT("VisualQAScreenshot=")',
            'TEXT("DGDeveloperToolAutomation")',
            'TEXT("Session16RulesSmokeTest=")',
        ):
            if token in release["game_mode"]:
                failures.append(
                    f"Shipping release-performance lane exposed developer entrypoint: {token}"
                )

    if development:
        combined_development = "\n".join(development.values())
        for token in (
            "DGT_Equipment_Dev_v1",
            "EnvironmentReports",
            "TrajectoryExports",
        ):
            if token not in combined_development:
                failures.append(f"development workflow token was not preserved: {token}")
        for token in ("SaveGameToSlot", "SaveStringToFile"):
            if token not in combined_development:
                failures.append(f"development I/O call was not preserved: {token}")

    build_text = BUILD_FILE.read_text(encoding="utf-8-sig")
    expected_build_lines = (
        'PublicDefinitions.Add("DG_WITH_DEVELOPMENT_CONTENT=" + (bReleaseShipping ? "0" : "1"));',
        'PublicDefinitions.Add("DG_WITH_RELEASE_PERFORMANCE_CAPTURE=" + (bReleaseShipping ? "1" : "0"));',
    )
    for line in expected_build_lines:
        if build_text.count(line) != 1:
            failures.append(f"Shipping compile-gate definition differs: {line}")

    if failures:
        print("SESSION 19 SHIPPING RUNTIME I/O VALIDATION FAILED")
        for failure in failures:
            print(" -", failure)
        return 1
    print(
        "SESSION 19 SHIPPING RUNTIME I/O VALIDATION PASS: "
        "equipment_dev_save=disabled environment_reports=disabled "
        "trajectory_files=disabled summaries=in_memory "
        "release_performance=guarded development_behavior=preserved"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
