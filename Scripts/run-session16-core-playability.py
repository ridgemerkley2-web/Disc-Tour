#!/usr/bin/env python3
"""Run the bounded Session 16 core-playability evidence lane.

This launcher deliberately orchestrates existing project authorities instead of
introducing a second gameplay state machine.  It runs the strict Session 15
four-process Hole 1 lane; the native main-menu, OB, water, temporary-state,
and three-hole lanes; and the existing mutation-safe missing-cosmetic recovery
fixture in one fresh external GUID UserDir.  Unobserved requirements remain
fail-closed Blocked results.

Exit codes:
  0  all 44 prepared playability checks passed (release is still not claimed)
  1  an attempted runtime check, boundary, or launcher invariant failed
  2  attempted evidence passed, but one or more required checks remain blocked
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid
from typing import Any


LEXICAL_ROOT = Path(__file__).absolute().parents[1]
ROOT = LEXICAL_ROOT.resolve()
DEFAULT_PROJECT = LEXICAL_ROOT / "DiscGolfTour.uproject"
SESSION15_WRAPPER = ROOT / "Scripts/run-session15-hole1-acceptance.py"
GATE_PATH = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/"
    "DG_CorePlayabilityGate.json"
)
FAILURE_CODES_PATH = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/"
    "DG_PlayabilityFailureCodes.json"
)

# The installed Session 15 runtime validates this exact parent internally.  A
# Session 16 launcher must reuse it until a later runtime explicitly generalizes
# that boundary; changing it here would fabricate a runnable capability.
RUN_PARENT = Path("C:/DGTour_TestRuns/Session15").resolve()
BACKUP_PARENT = Path("C:/DGTour_Backups").resolve()
CONTENT_ROOT = ROOT / "Content"
SAVEGAME_ROOT = ROOT / "Saved/SaveGames"
PROTECTED_PROFILE = SAVEGAME_ROOT / "DiscGolfTour_Profile_0.sav"
PROTECTED_BYTES = 5212
PROTECTED_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)

REPORT_SCHEMA = "DiscGolfTour.Session16CorePlayabilityAcceptance.v1"
PROCESS_SCHEMA = "DiscGolfTour.Session16PlayabilityProcess.v1"
BOUNDARY_SCHEMA = "DiscGolfTour.Session16BoundarySnapshot.v1"
PHASES = ("Setup", "Drive", "Finish", "Verify")
THREE_HOLE_PASS = (
    "THREE HOLE ROUND SMOKE PASS: 3/3 holes, 3 strokes, 11 par, -8 round; "
    "manifest transitions, scoring, scorecard state, and save snapshot active."
)
THREE_HOLE_FAIL = "THREE HOLE ROUND SMOKE FAIL:"
RESULTS_CONTINUE_PASS = "DG_SESSION16_RESULTS_CONTINUE: PASS"
MAIN_MENU_PASS = "DG_SESSION16_MAIN_MENU: PASS"
MAIN_MENU_FAIL = "DG_SESSION16_MAIN_MENU: FAIL"
RULES_OB_PASS = "DG_SESSION16_RULES_OB: PASS"
RULES_OB_FAIL = "DG_SESSION16_RULES_OB: FAIL"
RULES_WATER_PASS = "DG_SESSION16_RULES_WATER: PASS"
RULES_WATER_FAIL = "DG_SESSION16_RULES_WATER: FAIL"
SESSION12_PRESENTATION_PASS = "SESSION 12 PRESENTATION SMOKE PASS:"
SESSION12_PRESENTATION_FAIL = "SESSION 12 PRESENTATION SMOKE FAIL:"
TEMPORARY_STATE_RECOVERY_PASS = "DG_SESSION16_TEMPORARY_STATE_RECOVERY: PASS"
RECOVERY_PASS = "SESSION 7 FULL CHARACTER THROW SMOKE PASS:"
RECOVERY_FAIL = "SESSION 7 FULL CHARACTER THROW SMOKE FAIL:"
RECOVERY_REQUIRED_TOKENS = (
    "one_pawn=1",
    "one_authoritative_disc=1",
    "one_completed_flight=1",
    "one_recovery=1",
    "missing_scar_tattoo_fallback=1",
    "missing_outfit_fallback=1",
    "missing_fallback=1",
    "validation_temp_slot_deleted=1",
)
EXPECTED_FOCUSED_TESTS = frozenset(
    {
        "DiscGolfTour.Session16.Catalog.ExactRequiredChecks",
        "DiscGolfTour.Session16.Gates.SmokePass",
        "DiscGolfTour.Session16.Gates.CoreLoopPass",
        "DiscGolfTour.Session16.Gates.RoundPass",
        "DiscGolfTour.Session16.Gates.PersistencePass",
        "DiscGolfTour.Session16.Gates.RejectIncompleteEvidence",
        "DiscGolfTour.Session16.Contract.FailureAndReportSemantics",
        "DiscGolfTour.Session16.Watchdog.ExplicitLifecycle",
        "DiscGolfTour.Session16.Persistence.OptionalAssetFallback",
    }
)
FOCUSED_FRESHNESS_SOURCES = (
    ROOT / "Source/DiscGolfTour/DiscGolfSession16CorePlayabilityContract.h",
    ROOT / "Source/DiscGolfTour/DiscGolfSession16CorePlayabilityContract.cpp",
    ROOT / "Source/DiscGolfTour/Tests/DiscGolfSession16CorePlayabilityTests.cpp",
)
EDITOR_PROJECT_MODULE = ROOT / "Binaries/Win64/UnrealEditor-DiscGolfTour.dll"
CHECK_TIMEOUT_SECONDS = {
    "boot_game": 30.0,
    "course_load": 30.0,
    "disc_launches": 5.0,
    "disc_settles_or_holes_out": 30.0,
    "hole_completes": 180.0,
    "round_completes": 1800.0,
}
GATE_NUMERIC = {"Smoke": 0, "CoreLoop": 1, "Round": 2, "Persistence": 3}
RUNTIME_CHECK_ALIASES = {"soft_lock_watchdog": "no_soft_lock"}
FATAL_LOG_TOKENS = (
    "fatal error:",
    "exception_access_violation",
    "assertion failed:",
    "ensure condition failed",
    "logrhi: error:",
    "lowlevelfatalerror",
)

# Numeric values are the declaration order of EDGPlayabilityFailureCode.  The
# parser uses these when the runtime emits PLAYABILITY FAILURE with enum ints.
FAILURE_CODE_NAMES = (
    "None",
    "BootFailed",
    "MainMenuUnavailable",
    "PlayerProfileUnavailable",
    "CourseLoadFailed",
    "HoleSpawnFailed",
    "TeeStateInvalid",
    "BagUnavailable",
    "NoSelectableDisc",
    "DiscSelectionFailed",
    "AimStateUnavailable",
    "ThrowCouldNotStart",
    "ReleaseNotifyMissing",
    "DiscLaunchFailed",
    "PhysicsDidNotAdvance",
    "DiscNeverSettled",
    "DiscStateInvalid",
    "LieNotUpdated",
    "LieInvalid",
    "PlayerCouldNotContinue",
    "PenaltyRuleFailed",
    "OutOfBoundsRuleFailed",
    "WaterRuleFailed",
    "MandoRuleFailed",
    "BasketDetectionFailed",
    "HoleCompletionFailed",
    "ScoreUpdateFailed",
    "NextHoleFailed",
    "PauseFailed",
    "ResumeFailed",
    "CameraStateStuck",
    "InputContextStuck",
    "ReplayStateStuck",
    "DuplicatePlayerDetected",
    "DuplicateDiscDetected",
    "RoundCompletionFailed",
    "ResultsScreenFailed",
    "SaveFailed",
    "LoadFailed",
    "SaveMismatch",
    "MissingAssetRecoveryFailed",
    "InvalidCosmeticRecoveryFailed",
    "SoftLockDetected",
    "Timeout",
    "UnexpectedError",
)
FAILURE_CODE_SET = frozenset(FAILURE_CODE_NAMES)

CHECK_FAILURE_CODES = {
    "boot_game": "BootFailed",
    "main_menu_available": "MainMenuUnavailable",
    "player_profile_available": "PlayerProfileUnavailable",
    "course_load": "CourseLoadFailed",
    "spawn_at_tee": "HoleSpawnFailed",
    "bag_available": "BagUnavailable",
    "select_disc": "DiscSelectionFailed",
    "enter_aim": "AimStateUnavailable",
    "start_throw": "ThrowCouldNotStart",
    "release_notify_fires": "ReleaseNotifyMissing",
    "disc_launches": "DiscLaunchFailed",
    "physics_advances": "PhysicsDidNotAdvance",
    "disc_settles_or_holes_out": "DiscNeverSettled",
    "lie_updates": "LieNotUpdated",
    "next_throw_available": "PlayerCouldNotContinue",
    "ob_penalty_if_triggered": "OutOfBoundsRuleFailed",
    "water_penalty_if_triggered": "WaterRuleFailed",
    "basket_detects_completion": "BasketDetectionFailed",
    "score_updates": "ScoreUpdateFailed",
    "hole_completes": "HoleCompletionFailed",
    "pause_resume": "ResumeFailed",
    "camera_recovers": "CameraStateStuck",
    "input_context_recovers": "InputContextStuck",
    "replay_exits_cleanly": "ReplayStateStuck",
    "no_duplicate_player": "DuplicatePlayerDetected",
    "no_duplicate_disc": "DuplicateDiscDetected",
    "no_soft_lock": "SoftLockDetected",
    "advance_to_next_hole": "NextHoleFailed",
    "spawn_next_hole": "HoleSpawnFailed",
    "scorecard_persists_between_holes": "ScoreUpdateFailed",
    "multiple_holes_complete": "HoleCompletionFailed",
    "round_completes": "RoundCompletionFailed",
    "results_screen_available": "ResultsScreenFailed",
    "return_to_menu_or_continue": "PlayerCouldNotContinue",
    "save_player": "SaveFailed",
    "save_bag": "SaveFailed",
    "save_settings": "SaveFailed",
    "save_round_or_career": "SaveFailed",
    "load_save": "LoadFailed",
    "loaded_character_matches": "SaveMismatch",
    "loaded_bag_matches": "SaveMismatch",
    "loaded_settings_match": "SaveMismatch",
    "missing_cosmetic_falls_back": "InvalidCosmeticRecoveryFailed",
    "missing_optional_asset_does_not_block_play": "MissingAssetRecoveryFailed",
}

PLAYABILITY_FAILURE_RE = re.compile(
    r"PLAYABILITY FAILURE:\s*check=(?P<check>\S+)\s+gate=(?P<gate>\d+)\s+"
    r"code=(?P<code>\d+)\s+blocking=(?P<blocking>yes|no)\s+"
    r"state=(?P<state>.*?)\s+message=(?P<message>.*)$",
    re.IGNORECASE,
)
UE_LOG_TIMESTAMP_RE = re.compile(
    r"\[(?P<stamp>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]"
)


class ProcessFailure(RuntimeError):
    def __init__(self, failure_code: str, message: str) -> None:
        super().__init__(message)
        self.failure_code = failure_code


def load_session15_module() -> Any:
    spec = importlib.util.spec_from_file_location(
        "dg_session15_acceptance", SESSION15_WRAPPER
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load the Session 15 acceptance validator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def canonical_guid(value: str | None) -> str:
    parsed = uuid.uuid4() if value is None else uuid.UUID(value)
    if value is not None and str(parsed) != value.casefold():
        raise ValueError("--run-id must be a canonical hyphenated GUID")
    return str(parsed)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )


def validate_extra_args(args: list[str]) -> None:
    forbidden = (
        "-userdir",
        "-abslog",
        "-nullrhi",
        "-session15verticalslicesmoketest",
        "-session15verticalslicephase",
        "-session16mainmenusmoketest",
        "-session16rulessmoketest",
        "-session16coreplayabilitygate",
        "-session12presentationsmoketest",
        "-threeholeroundsmoketest",
        "-session7fullcharacterthrowsmoketest",
        "-session7fullcharacterfixture",
        "-session7fullcharactervalidation",
        "-resx",
        "-resy",
        "-forceres",
        "-d3d",
        "-vulkan",
        "-opengl",
    )
    for argument in args:
        folded = argument.casefold()
        if any(folded.startswith(prefix) for prefix in forbidden):
            raise ValueError(
                f"extra argument may not override playability authority: {argument}"
            )


def load_gate(s15: Any) -> tuple[list[str], dict[str, str]]:
    gate = s15.strict_json_load(GATE_PATH)
    levels = gate.get("levels")
    if not isinstance(levels, list):
        raise ValueError("playability gate levels are unavailable")
    ordered: list[str] = []
    level_by_check: dict[str, str] = {}
    for level in levels:
        if not isinstance(level, dict) or level.get("id") not in {
            "Smoke",
            "CoreLoop",
            "Round",
            "Persistence",
        }:
            raise ValueError("playability gate has an invalid level")
        checks = level.get("checks")
        if not isinstance(checks, list):
            raise ValueError("playability gate checks are unavailable")
        for check in checks:
            if not isinstance(check, str) or not check or check in level_by_check:
                raise ValueError(f"invalid or duplicate playability check: {check!r}")
            ordered.append(check)
            level_by_check[check] = level["id"]
    if len(ordered) != 44 or set(ordered) != set(CHECK_FAILURE_CODES):
        raise ValueError(
            "the prepared playability gate is not the exact expected 44-check set"
        )

    declared_codes = s15.strict_json_load(FAILURE_CODES_PATH)
    categories = declared_codes.get("categories")
    if not isinstance(categories, dict):
        raise ValueError("playability failure-code registry is unavailable")
    registry_names = {
        item
        for values in categories.values()
        if isinstance(values, list)
        for item in values
        if isinstance(item, str)
    }
    # PauseFailed and ResumeFailed are present in the source enum but omitted
    # from the kit category registry.  Everything used by this wrapper must be
    # declared by one of those two compatible sources.
    if not set(CHECK_FAILURE_CODES.values()) <= registry_names | {
        "PauseFailed",
        "ResumeFailed",
    }:
        raise ValueError("check-to-failure-code mapping drifted from the source enum")
    return ordered, level_by_check


def initial_results(
    ordered: list[str], level_by_check: dict[str, str]
) -> dict[str, dict[str, Any]]:
    return {
        check: {
            "check_id": check,
            "gate_level": level_by_check[check],
            "status": "Blocked",
            "failure_code": CHECK_FAILURE_CODES[check],
            "message": "Required runtime observation has not completed.",
            "duration_seconds": 0.0,
            "blocking": True,
            "evidence": [],
        }
        for check in ordered
    }


def set_result(
    results: dict[str, dict[str, Any]],
    check: str,
    status: str,
    message: str,
    evidence: list[dict[str, Any]],
    duration: float = 0.0,
    failure_code: str | None = None,
) -> None:
    if check not in results:
        raise ValueError(f"unknown playability check: {check}")
    if status not in {"Passed", "Failed", "Blocked", "NotRun"}:
        raise ValueError(f"invalid playability status: {status}")
    # The Session 16 contract fixes one failure code per prepared check.  A
    # process-level Timeout or a runtime monitor's observed code belongs in the
    # process/marker evidence, never in the canonical result metadata.
    code = "None" if status == "Passed" else CHECK_FAILURE_CODES[check]
    if code not in FAILURE_CODE_SET:
        raise ValueError(f"unknown EDGPlayabilityFailureCode: {code}")
    results[check].update(
        {
            "status": status,
            "failure_code": code,
            "message": message,
            "duration_seconds": round(max(0.0, duration), 6),
            "evidence": evidence,
        }
    )


def evidence_ref(process_id: str, manifest: Path, detail: str) -> dict[str, Any]:
    return {
        "source": "runtime_process",
        "process_id": process_id,
        "manifest": manifest.as_posix(),
        "detail": detail,
    }


def observed_process_failure(manifest: dict[str, Any]) -> dict[str, Any]:
    return {
        "source": "process_failure_observation",
        "observed_failure_code": manifest.get("failure_code", "UnexpectedError"),
        "return_code": manifest.get("return_code"),
        "timed_out": manifest.get("timed_out"),
        "errors": list(manifest.get("errors", [])),
    }


def terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    try:
        if sys.platform == "win32":
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        else:
            process.terminate()
    finally:
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()


def parse_playability_failures(text: str) -> list[dict[str, Any]]:
    failures: list[dict[str, Any]] = []
    for line in text.splitlines():
        match = PLAYABILITY_FAILURE_RE.search(line)
        if not match:
            continue
        numeric = int(match.group("code"))
        name = (
            FAILURE_CODE_NAMES[numeric]
            if 0 <= numeric < len(FAILURE_CODE_NAMES)
            else "UnexpectedError"
        )
        failures.append(
            {
                "check_id": match.group("check"),
                "gate_numeric": int(match.group("gate")),
                "failure_code_numeric": numeric,
                "failure_code": name,
                "blocking": match.group("blocking").casefold() == "yes",
                "state": match.group("state").strip(),
                "message": match.group("message").strip(),
                "line": line.strip(),
            }
        )
    return failures


def validate_runtime_failure_marker(
    runtime_failure: dict[str, Any],
    results: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    emitted_check = runtime_failure.get("check_id")
    catalog_check = RUNTIME_CHECK_ALIASES.get(emitted_check, emitted_check)
    if catalog_check not in results:
        raise ValueError(f"runtime marker used unknown check id {emitted_check!r}")
    expected_gate = GATE_NUMERIC[results[catalog_check]["gate_level"]]
    expected_code = CHECK_FAILURE_CODES[catalog_check]
    if runtime_failure.get("gate_numeric") != expected_gate:
        raise ValueError(
            f"runtime marker gate mismatch for {emitted_check!r}: "
            f"{runtime_failure.get('gate_numeric')} != {expected_gate}"
        )
    if runtime_failure.get("failure_code") != expected_code:
        raise ValueError(
            f"runtime marker code mismatch for {emitted_check!r}: "
            f"{runtime_failure.get('failure_code')!r} != {expected_code!r}"
        )
    if runtime_failure.get("blocking") is not True:
        raise ValueError(f"runtime marker for {emitted_check!r} was non-blocking")
    validated = dict(runtime_failure)
    validated["catalog_check_id"] = catalog_check
    validated["validation_status"] = "valid"
    return validated


def unique_runtime_failures(*texts: str) -> list[dict[str, Any]]:
    unique: dict[tuple[Any, ...], dict[str, Any]] = {}
    for source_name, value in zip(("abslog", "console"), texts):
        for marker in parse_playability_failures(value):
            key = (
                marker["check_id"],
                marker["gate_numeric"],
                marker["failure_code_numeric"],
                marker["blocking"],
                marker["state"],
                marker["message"],
            )
            if key not in unique:
                marker["observed_channels"] = [source_name]
                unique[key] = marker
            elif source_name not in unique[key]["observed_channels"]:
                unique[key]["observed_channels"].append(source_name)
    return list(unique.values())


def results_continue_failure_code(*texts: str) -> tuple[str | None, str]:
    for value in texts:
        for line in value.splitlines():
            if "DG_SESSION16_RESULTS_CONTINUE: FAIL" not in line:
                continue
            folded = line.casefold()
            code = (
                "PlayerCouldNotContinue"
                if "results=yes" in folded and "continue=no" in folded
                else "ResultsScreenFailed"
            )
            return code, line.strip()
    return None, ""


def marker_delta_seconds(
    process_dir: Path,
    process_id: str,
    start_token: str,
    end_token: str,
) -> tuple[float | None, str]:
    candidates = (
        ("abslog", process_dir / f"{process_id}.log", "utf-8-sig"),
        ("console", process_dir / f"{process_id}.console.log", "utf-8"),
    )
    for source_name, path, encoding in candidates:
        if not path.is_file():
            continue
        text_value = path.read_text(encoding=encoding, errors="replace")
        start_time: datetime | None = None
        for line in text_value.splitlines():
            timestamp_match = UE_LOG_TIMESTAMP_RE.search(line)
            if not timestamp_match:
                continue
            timestamp = datetime.strptime(
                timestamp_match.group("stamp"), "%Y.%m.%d-%H.%M.%S:%f"
            )
            if start_time is None and start_token in line:
                start_time = timestamp
                continue
            if start_time is not None and end_token in line:
                delta = (timestamp - start_time).total_seconds()
                return (delta if delta >= 0.0 else None), source_name
    return None, "unavailable"


def process_start_to_marker_seconds(
    process_dir: Path,
    process_id: str,
    started_utc: str,
    marker_token: str,
) -> tuple[float | None, str]:
    """Measure launch-to-functional-marker time without charging shutdown time."""
    try:
        process_start = datetime.fromisoformat(started_utc).astimezone(timezone.utc)
    except (TypeError, ValueError):
        return None, "unavailable"
    candidates = (
        ("abslog", process_dir / f"{process_id}.log", "utf-8-sig"),
        ("console", process_dir / f"{process_id}.console.log", "utf-8"),
    )
    for source_name, path, encoding in candidates:
        if not path.is_file():
            continue
        for line in path.read_text(encoding=encoding, errors="replace").splitlines():
            if marker_token not in line:
                continue
            timestamp_match = UE_LOG_TIMESTAMP_RE.search(line)
            if not timestamp_match:
                continue
            marker_time = datetime.strptime(
                timestamp_match.group("stamp"), "%Y.%m.%d-%H.%M.%S:%f"
            ).replace(tzinfo=timezone.utc)
            delta = (marker_time - process_start).total_seconds()
            return (delta if delta >= 0.0 else None), source_name
    return None, "unavailable"


def record_contract_time_bound(
    results: dict[str, dict[str, Any]],
    check: str,
    observed_seconds: float | None,
    basis: str,
    evidence: list[dict[str, Any]],
) -> None:
    limit = CHECK_TIMEOUT_SECONDS[check]
    timing_evidence = {
        "source": "contract_time_bound",
        "check_id": check,
        "limit_seconds": limit,
        "observed_seconds": (
            round(observed_seconds, 6) if observed_seconds is not None else None
        ),
        "basis": basis,
    }
    combined_evidence = [*evidence, timing_evidence]
    if observed_seconds is None or not math.isfinite(observed_seconds):
        set_result(
            results,
            check,
            "Blocked",
            "Functional evidence passed, but the required contract duration was not observed.",
            combined_evidence,
        )
        results[check]["time_bound"] = {**timing_evidence, "status": "NotObserved"}
        return
    if observed_seconds > limit:
        set_result(
            results,
            check,
            "Failed",
            f"Observed duration {observed_seconds:.3f}s exceeded the {limit:.3f}s contract bound.",
            combined_evidence,
            observed_seconds,
        )
        results[check]["time_bound"] = {**timing_evidence, "status": "Failed"}
        raise ProcessFailure(
            CHECK_FAILURE_CODES[check],
            f"{check} exceeded its {limit:.3f}s contract bound",
        )
    results[check]["duration_seconds"] = round(observed_seconds, 6)
    results[check]["evidence"] = combined_evidence
    results[check]["message"] += (
        f" Contract time bound passed at {observed_seconds:.3f}s/{limit:.3f}s."
    )
    results[check]["time_bound"] = {**timing_evidence, "status": "Passed"}


def base_command(
    executable: Path,
    project: Path,
    mode: str,
    user_dir: Path,
    log_path: Path,
) -> list[str]:
    command = [str(executable)]
    if mode == "editor":
        command.extend([str(project), "-game"])
    command.extend(
        [
            f"-UserDir={user_dir}",
            "-d3d12",
            "-ResX=1920",
            "-ResY=1080",
            "-ForceRes",
            "-RenderOffscreen",
            "-unattended",
            "-nop4",
            "-nosplash",
            "-UTF8Output",
            "-stdout",
            "-FullStdOutLogOutput",
            f"-abslog={log_path}",
        ]
    )
    return command


def run_process(
    s15: Any,
    results: dict[str, dict[str, Any]],
    process_id: str,
    command: list[str],
    process_dir: Path,
    timeout_seconds: int,
    pass_marker: str,
    fail_marker: str,
    required_pass_tokens: tuple[str, ...] = (),
    required_markers: tuple[str, ...] = (),
    default_failure_code: str = "UnexpectedError",
    required_marker_failure_code: str | None = None,
) -> tuple[dict[str, Any], Path]:
    process_dir.mkdir(parents=True, exist_ok=True)
    log_path = process_dir / f"{process_id}.log"
    console_path = process_dir / f"{process_id}.console.log"
    manifest_path = process_dir / "ProcessManifest.json"
    if sum(arg.casefold().startswith("-userdir=") for arg in command) != 1:
        raise ProcessFailure("UnexpectedError", "command did not contain one UserDir")
    if sum(arg.casefold().startswith("-abslog=") for arg in command) != 1:
        raise ProcessFailure("UnexpectedError", "command did not contain one abslog")

    started_utc = utc_now()
    started = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=Path(command[0]).parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=(
            subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0
        ),
    )
    timed_out = False
    try:
        console, _ = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        terminate_process(process)
        tail, _ = process.communicate()
        console = partial + (tail or "")
    duration = time.monotonic() - started
    console_path.write_text(console, encoding="utf-8")
    log_text = (
        log_path.read_text(encoding="utf-8-sig", errors="replace")
        if log_path.is_file()
        else ""
    )
    evidence_text = log_text if log_text else console
    raw_runtime_failures = unique_runtime_failures(log_text, console)
    runtime_failures: list[dict[str, Any]] = []
    marker_validation_errors: list[str] = []
    for runtime_failure in raw_runtime_failures:
        try:
            runtime_failures.append(
                validate_runtime_failure_marker(runtime_failure, results)
            )
        except ValueError as exc:
            invalid = dict(runtime_failure)
            invalid["validation_status"] = "invalid"
            invalid["validation_error"] = str(exc)
            runtime_failures.append(invalid)
            marker_validation_errors.append(str(exc))
    fatal_tokens = sorted(
        token
        for token in FATAL_LOG_TOKENS
        if token in log_text.casefold() or token in console.casefold()
    )
    pass_count = evidence_text.count(pass_marker)
    fail_count = max(log_text.count(fail_marker), console.count(fail_marker))
    selected_pass_line = next(
        (line for line in evidence_text.splitlines() if pass_marker in line), ""
    )
    missing_tokens = [
        token for token in required_pass_tokens if token not in selected_pass_line
    ]
    required_marker_counts = {
        marker: evidence_text.count(marker) for marker in required_markers
    }
    invalid_required_markers = {
        marker: count
        for marker, count in required_marker_counts.items()
        if count != 1
    }
    results_failure_code, results_failure_line = results_continue_failure_code(
        log_text, console
    )
    errors: list[str] = []
    failure_code = results_failure_code or "None"
    if marker_validation_errors:
        failure_code = "UnexpectedError"
        errors.append(
            "invalid PLAYABILITY FAILURE markers: "
            f"{marker_validation_errors}"
        )
    if timed_out:
        failure_code = "Timeout"
        errors.append(f"process timed out after {timeout_seconds} seconds")
    if process.returncode != 0:
        failure_code = failure_code if failure_code != "None" else default_failure_code
        errors.append(f"process returned {process.returncode}")
    if pass_count != 1:
        failure_code = failure_code if failure_code != "None" else default_failure_code
        errors.append(f"pass marker count was {pass_count}, expected 1")
    if fail_count:
        failure_code = failure_code if failure_code != "None" else default_failure_code
        errors.append(f"fail marker count was {fail_count}, expected 0")
    if missing_tokens:
        failure_code = failure_code if failure_code != "None" else default_failure_code
        errors.append(f"pass marker omitted tokens: {missing_tokens}")
    if invalid_required_markers:
        failure_code = failure_code if failure_code != "None" else (
            required_marker_failure_code or default_failure_code
        )
        errors.append(
            "required marker counts differed from 1: "
            f"{invalid_required_markers}"
        )
    if results_failure_code is not None:
        errors.append(
            "results/continue failure marker was emitted: "
            f"{results_failure_line}"
        )
    if fatal_tokens:
        failure_code = failure_code if failure_code != "None" else default_failure_code
        errors.append(f"fatal/error diagnostics were present: {fatal_tokens}")
    valid_runtime_failures = [
        marker
        for marker in runtime_failures
        if marker.get("validation_status") == "valid"
    ]
    if valid_runtime_failures and not marker_validation_errors:
        failure_code = valid_runtime_failures[0]["failure_code"]
        errors.append("one or more PLAYABILITY FAILURE markers were emitted")

    manifest = {
        "schema": PROCESS_SCHEMA,
        "process_id": process_id,
        "started_utc": started_utc,
        "completed_utc": utc_now(),
        "duration_seconds": round(duration, 6),
        "command": command,
        "return_code": process.returncode,
        "timed_out": timed_out,
        "pass_marker": pass_marker,
        "pass_marker_count": pass_count,
        "fail_marker": fail_marker,
        "fail_marker_count": fail_count,
        "required_pass_tokens": list(required_pass_tokens),
        "missing_pass_tokens": missing_tokens,
        "required_marker_counts": required_marker_counts,
        "playability_failure_markers": runtime_failures,
        "playability_failure_marker_validation_errors": marker_validation_errors,
        "results_continue_failure": (
            {
                "failure_code": results_failure_code,
                "line": results_failure_line,
            }
            if results_failure_code is not None
            else None
        ),
        "scanned_channels": {
            "abslog_present": bool(log_text),
            "console_present": bool(console),
            "abslog_pass_marker_count": log_text.count(pass_marker),
            "console_pass_marker_count": console.count(pass_marker),
            "abslog_fail_marker_count": log_text.count(fail_marker),
            "console_fail_marker_count": console.count(fail_marker),
        },
        "fatal_log_tokens": fatal_tokens,
        "log": s15.file_record(log_path),
        "console": s15.file_record(console_path),
        "status": "PASS" if not errors else "FAIL",
        "failure_code": failure_code,
        "errors": errors,
    }
    write_json(manifest_path, manifest)
    if errors:
        raise ProcessFailure(failure_code, f"{process_id}: {'; '.join(errors)}")
    return manifest, manifest_path


def apply_runtime_marker_failure(
    results: dict[str, dict[str, Any]],
    runtime_failure: dict[str, Any],
    evidence: list[dict[str, Any]],
) -> None:
    if runtime_failure.get("validation_status") != "valid":
        raise ValueError("refusing to map an invalid PLAYABILITY FAILURE marker")
    check = runtime_failure["catalog_check_id"]
    marker_evidence = {
        "source": "runtime_failure_marker",
        "emitted_check_id": runtime_failure["check_id"],
        "catalog_check_id": check,
        "observed_gate_numeric": runtime_failure["gate_numeric"],
        "observed_failure_code_numeric": runtime_failure["failure_code_numeric"],
        "observed_failure_code": runtime_failure["failure_code"],
        "observed_channels": runtime_failure.get("observed_channels", []),
        "state": runtime_failure["state"],
        "line": runtime_failure["line"],
    }
    set_result(
        results,
        check,
        "Failed",
        runtime_failure.get("message") or "runtime playability monitor failed",
        [*evidence, marker_evidence],
    )


def load_focused_automation(
    s15: Any, path: Path | None
) -> tuple[dict[str, Any] | None, dict[str, dict[str, Any]]]:
    if path is None:
        return None, {}
    resolved = path.resolve()
    report = s15.strict_json_load(resolved)
    expected_counters = {
        "succeeded": 9,
        "succeededWithWarnings": 0,
        "failed": 0,
        "notRun": 0,
        "inProcess": 0,
    }
    observed_counters = {key: report.get(key) for key in expected_counters}
    if observed_counters != expected_counters:
        raise ValueError(
            "focused Session 16 automation counters differ: "
            f"{observed_counters!r} != {expected_counters!r}"
        )
    tests = report.get("tests")
    if not isinstance(tests, list) or len(tests) != 9:
        raise ValueError("focused Session 16 automation must contain exactly 9 tests")
    successes: dict[str, dict[str, Any]] = {}
    for item in tests:
        if not isinstance(item, dict):
            raise ValueError("focused Session 16 automation has a malformed test")
        full_path = item.get("fullTestPath")
        if (
            not isinstance(full_path, str)
            or not full_path.startswith("DiscGolfTour.Session16.")
            or item.get("state") != "Success"
            or item.get("errors") != 0
            or item.get("warnings") != 0
        ):
            raise ValueError(
                "focused automation contains a non-Session16 or non-success test"
            )
        if full_path in successes:
            raise ValueError(
                f"focused Session 16 automation duplicates {full_path!r}"
            )
        successes[full_path] = item
    if set(successes) != EXPECTED_FOCUSED_TESTS:
        raise ValueError(
            "focused Session 16 automation is not the exact prepared nine-test set"
        )
    report_created_text = report.get("reportCreatedOn")
    if not isinstance(report_created_text, str):
        raise ValueError("focused Session 16 automation has no reportCreatedOn")
    try:
        # Unreal's automation index writes this timestamp in UTC.
        report_created_utc = datetime.strptime(
            report_created_text, "%Y.%m.%d-%H.%M.%S"
        ).replace(tzinfo=timezone.utc)
        report_created_epoch = report_created_utc.timestamp()
    except ValueError as exc:
        raise ValueError(
            "focused Session 16 automation reportCreatedOn is malformed"
        ) from exc
    freshness_sources = (*FOCUSED_FRESHNESS_SOURCES, EDITOR_PROJECT_MODULE)
    if not all(source.is_file() for source in freshness_sources):
        raise ValueError("Session 16 automation freshness sources are unavailable")
    latest_source_epoch = max(
        source.stat().st_mtime for source in freshness_sources
    )
    # FAT/zip/copy workflows and the automation index itself have whole-second
    # timestamps, so admit only a two-second resolution allowance.
    if report_created_epoch + 2.0 < latest_source_epoch:
        raise ValueError(
            "focused Session 16 automation predates its contract, test source, "
            "or built module"
        )
    if report_created_epoch > time.time() + 300.0:
        raise ValueError("focused Session 16 automation timestamp is in the future")
    summary = {
        "path": str(resolved),
        **s15.file_record(resolved),
        "succeeded": report.get("succeeded"),
        "failed": report.get("failed"),
        "not_run": report.get("notRun"),
        "succeeded_with_warnings": report.get("succeededWithWarnings"),
        "in_process": report.get("inProcess"),
        "report_created_on": report_created_text,
        "report_created_utc": report_created_utc.isoformat(),
        "freshness_source_latest_utc": datetime.fromtimestamp(
            latest_source_epoch, timezone.utc
        ).isoformat(),
        "freshness_built_module": str(EDITOR_PROJECT_MODULE),
        "freshness_built_module_record": s15.file_record(EDITOR_PROJECT_MODULE),
        "test_paths": sorted(successes),
    }
    return summary, successes


def automation_evidence(
    summary: dict[str, Any], full_path: str
) -> dict[str, Any]:
    return {
        "source": "focused_automation",
        "report": summary["path"],
        "copied_report": summary.get("copied_path"),
        "copied_record": summary.get("copied_record"),
        "sha256": summary["sha256"],
        "test": full_path,
        "detail": "fresh caller-supplied Session 16 focused automation success",
    }


def gate_summaries(
    ordered: list[str],
    level_by_check: dict[str, str],
    results: dict[str, dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    summaries: dict[str, dict[str, Any]] = {}
    for level in ("Smoke", "CoreLoop", "Round", "Persistence"):
        rows = [results[check] for check in ordered if level_by_check[check] == level]
        statuses = [row["status"] for row in rows]
        status = (
            "Failed"
            if "Failed" in statuses
            else "Blocked"
            if "Blocked" in statuses or "NotRun" in statuses
            else "Passed"
        )
        summaries[level] = {
            "status": status,
            "passed": sum(value == "Passed" for value in statuses),
            "failed": sum(value == "Failed" for value in statuses),
            "blocked": sum(value == "Blocked" for value in statuses),
            "not_run": sum(value == "NotRun" for value in statuses),
            "total": len(rows),
        }
    return summaries


def validate_result_catalog(
    ordered: list[str],
    level_by_check: dict[str, str],
    results: dict[str, dict[str, Any]],
) -> None:
    if list(results) != ordered:
        raise ValueError("result catalog order differs from the prepared 44 checks")
    for check in ordered:
        row = results[check]
        status = row.get("status")
        expected_code = "None" if status == "Passed" else CHECK_FAILURE_CODES[check]
        if (
            row.get("check_id") != check
            or row.get("gate_level") != level_by_check[check]
            or status not in {"Passed", "Failed", "Blocked", "NotRun"}
            or row.get("failure_code") != expected_code
            or row.get("blocking") is not True
            or not isinstance(row.get("message"), str)
            or not row["message"]
            or not isinstance(row.get("duration_seconds"), (int, float))
            or isinstance(row.get("duration_seconds"), bool)
            or not math.isfinite(float(row["duration_seconds"]))
            or row["duration_seconds"] < 0.0
            or not isinstance(row.get("evidence"), list)
        ):
            raise ValueError(f"result row violates the prepared contract: {check}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("editor", "packaged"), required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument(
        "--package-root",
        type=Path,
        help="Required in packaged mode; complete staged package root.",
    )
    parser.add_argument("--run-id")
    parser.add_argument(
        "--timeout",
        type=int,
        default=180,
        help="Per-process timeout in seconds (30-900).",
    )
    parser.add_argument(
        "--round-timeout",
        type=int,
        default=300,
        help="Three-hole process timeout in seconds (60-1800).",
    )
    parser.add_argument(
        "--focused-automation-report",
        type=Path,
        help=(
            "Optional fresh DiscGolfTour.Session16.* automation index. Only exact "
            "recognized recovery tests can satisfy otherwise unobserved checks."
        ),
    )
    parser.add_argument("--extra-arg", action="append", default=[])
    options = parser.parse_args()

    executable = options.executable.resolve()
    project_argument = options.project.absolute()
    project = project_argument.resolve()
    if not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    if options.mode == "editor" and not project.is_file():
        parser.error(f"project does not exist: {project}")
    if project != (ROOT / "DiscGolfTour.uproject").resolve():
        parser.error("--project must resolve to this DiscGolfTour workspace")
    if not 30 <= options.timeout <= 900:
        parser.error("--timeout must be between 30 and 900 seconds")
    if not 60 <= options.round_timeout <= 1800:
        parser.error("--round-timeout must be between 60 and 1800 seconds")
    try:
        validate_extra_args(options.extra_arg)
        run_id = canonical_guid(options.run_id)
    except (ValueError, TypeError) as exc:
        parser.error(str(exc))

    user_dir = (RUN_PARENT / run_id).resolve()
    backup_dir = (BACKUP_PARENT / f"Session16_{options.mode}_{run_id}").resolve()
    # Preserve the caller's lexical project path for report output.  Resolving
    # the C:\DGTour junction expands to a much longer workspace path and can
    # make shutil hit legacy Win32 path limits during evidence copying.
    project_report_root = project_argument.parent / "Saved/Session16Reports"
    stable_root = project_report_root / f"Acceptance_{options.mode}_{run_id}"
    if user_dir.parent != RUN_PARENT or user_dir.name != run_id:
        parser.error("resolved UserDir escaped the runtime-accepted GUID parent")
    if backup_dir.parent != BACKUP_PARENT:
        parser.error("resolved backup directory escaped its exact parent")
    if user_dir.exists() or backup_dir.exists() or stable_root.exists():
        parser.error("run-id is not fresh across run, backup, and report roots")

    package_root: Path | None = None
    if options.mode == "packaged":
        if options.package_root is None:
            parser.error("--package-root is required for packaged mode")
        package_root = options.package_root.resolve()
        if not package_root.is_dir() or package_root not in executable.parents:
            parser.error("packaged executable must be inside --package-root")

    s15 = load_session15_module()
    ordered, level_by_check = load_gate(s15)
    results = initial_results(ordered, level_by_check)
    focused_summary, focused_tests = load_focused_automation(
        s15, options.focused_automation_report
    )

    protected_before = s15.file_record(PROTECTED_PROFILE)
    if protected_before != {
        "present": True,
        "bytes": PROTECTED_BYTES,
        "sha256": PROTECTED_SHA256,
    }:
        parser.error("canonical project profile is not the protected A999 binding")

    print("Hashing project Content and SaveGames before Session 16...", flush=True)
    content_before = s15.tree_snapshot(CONTENT_ROOT)
    saves_before = s15.tree_snapshot(SAVEGAME_ROOT)
    expected_project_saves = {
        "DiscGolfTour_Profile_0.sav": {
            "bytes": PROTECTED_BYTES,
            "sha256": PROTECTED_SHA256,
        }
    }
    if saves_before != expected_project_saves:
        parser.error("project SaveGames must contain exactly the protected A999 profile")
    package_before = (
        s15.tree_snapshot(package_root) if package_root is not None else None
    )

    report_root = user_dir / "Saved/Session16Reports"
    session15_report_root = user_dir / "Saved/Session15Reports"
    processes_root = report_root / "Processes"
    owned_marker = user_dir / ".dg_session16_owned"
    started_utc = utc_now()
    process_records: list[dict[str, Any]] = []
    launcher_failures: list[dict[str, Any]] = []
    attempted_runtime_failed = False
    boundaries_unchanged = False
    package_unchanged: bool | None = None
    archived = False

    def remember_process(manifest: dict[str, Any], manifest_path: Path) -> None:
        process_records.append(
            {
                "process_id": manifest["process_id"],
                "status": manifest["status"],
                "failure_code": manifest["failure_code"],
                "duration_seconds": manifest["duration_seconds"],
                "manifest": manifest_path.relative_to(user_dir).as_posix(),
                "manifest_record": s15.file_record(manifest_path),
            }
        )

    phase_reports: dict[str, dict[str, Any]] = {}
    try:
        RUN_PARENT.mkdir(parents=True, exist_ok=True)
        user_dir.mkdir()
        owned_marker.write_text(run_id + "\n", encoding="ascii")

        if focused_summary is not None:
            focused_source = Path(focused_summary["path"])
            focused_copy = report_root / "FocusedAutomation/index.json"
            focused_copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(focused_source, focused_copy)
            copied_record = s15.file_record(focused_copy)
            if copied_record != {
                key: focused_summary[key]
                for key in ("present", "bytes", "sha256")
            }:
                raise ProcessFailure(
                    "UnexpectedError",
                    "focused automation evidence copy did not preserve exact bytes",
                )
            focused_summary["copied_path"] = focused_copy.relative_to(
                user_dir
            ).as_posix()
            focused_summary["copied_record"] = copied_record

        for phase in PHASES:
            phase_failure_check = {
                "Setup": "boot_game",
                "Drive": "start_throw",
                "Finish": "hole_completes",
                "Verify": "load_save",
            }[phase]
            phase_failure_code = CHECK_FAILURE_CODES[phase_failure_check]
            process_id = f"Session15_{phase}"
            process_dir = processes_root / process_id
            log_path = process_dir / f"{process_id}.log"
            command = base_command(
                executable, project, options.mode, user_dir, log_path
            )
            command.extend(
                [
                    "-Session15VerticalSliceSmokeTest",
                    f"-Session15VerticalSlicePhase={phase}",
                    "-Course=PineRidge",
                    "-Hole=1",
                    "-SkipHoleIntro",
                    *options.extra_arg,
                ]
            )
            print(f"Running Session 16 evidence process: {process_id}", flush=True)
            try:
                manifest, manifest_path = run_process(
                    s15,
                    results,
                    process_id,
                    command,
                    process_dir,
                    options.timeout,
                    s15.PASS_MARKERS[phase],
                    f"DG_SESSION15_VERTICAL_SLICE_{phase.upper()}: FAIL",
                    default_failure_code={
                        "Setup": "BootFailed",
                        "Drive": "ThrowCouldNotStart",
                        "Finish": "HoleCompletionFailed",
                        "Verify": "LoadFailed",
                    }[phase],
                )
                remember_process(manifest, manifest_path)
            except ProcessFailure as exc:
                manifest = s15.strict_json_load(process_dir / "ProcessManifest.json")
                remember_process(manifest, process_dir / "ProcessManifest.json")
                evidence = [
                    evidence_ref(
                        process_id,
                        (process_dir / "ProcessManifest.json").relative_to(user_dir),
                        "attempted process failed",
                    )
                ]
                evidence.append(observed_process_failure(manifest))
                valid_runtime_markers = [
                    marker
                    for marker in manifest.get("playability_failure_markers", [])
                    if marker.get("validation_status") == "valid"
                ]
                for marker in valid_runtime_markers:
                    apply_runtime_marker_failure(results, marker, evidence)
                if not valid_runtime_markers:
                    set_result(
                        results,
                        phase_failure_check,
                        "Failed",
                        str(exc),
                        evidence,
                        manifest.get("duration_seconds", 0.0),
                        exc.failure_code,
                    )
                raise

            phase_path = session15_report_root / phase / "PhaseReport.json"
            try:
                report = s15.strict_json_load(phase_path)
                s15.validate_phase_report(report, phase, user_dir, phase_reports)
            except Exception as exc:
                evidence = [
                    evidence_ref(
                        process_id,
                        manifest_path.relative_to(user_dir),
                        f"strict Session 15 {phase} report validation failed",
                    )
                ]
                set_result(
                    results,
                    phase_failure_check,
                    "Failed",
                    f"{type(exc).__name__}: {exc}",
                    evidence,
                    manifest.get("duration_seconds", 0.0),
                    phase_failure_code,
                )
                process_records[-1]["runtime_phase_report_validation"] = {
                    "status": "FAIL",
                    "failure_code": phase_failure_code,
                    "message": f"{type(exc).__name__}: {exc}",
                }
                raise ProcessFailure(
                    phase_failure_code,
                    f"{process_id}: strict PhaseReport validation failed: {exc}",
                ) from exc
            phase_reports[phase] = report
            process_records[-1]["runtime_phase_report"] = {
                "path": phase_path.relative_to(user_dir).as_posix(),
                **s15.file_record(phase_path),
            }
            process_records[-1]["manifest_record"] = s15.file_record(manifest_path)
            duration = manifest["duration_seconds"]
            ref = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    f"strict Session 15 {phase} report validation passed",
                )
            ]
            if phase == "Setup":
                for check, message in {
                    "boot_game": "Game process booted and emitted the Setup pass once.",
                    "player_profile_available": "Current schema-10 player profile reconstructed.",
                    "course_load": "Authored Pine Ridge Championship Hole 1 loaded.",
                    "spawn_at_tee": "Setup reported the legal authored Hole 1 tee state.",
                    "bag_available": "The project bag authority resolved a selected stable disc.",
                    "select_disc": "Apex/Tour selected through the existing bag authority.",
                }.items():
                    set_result(results, check, "Passed", message, ref, duration)
                setup_ready_seconds, setup_ready_channel = (
                    process_start_to_marker_seconds(
                        process_dir,
                        process_id,
                        manifest["started_utc"],
                        "DG_SESSION15_VERTICAL_SLICE_SETUP: PASS",
                    )
                )
                for timed_check in ("boot_game", "course_load"):
                    record_contract_time_bound(
                        results,
                        timed_check,
                        setup_ready_seconds,
                        (
                            "process launch to exact functional Setup pass marker "
                            f"from {setup_ready_channel}; shutdown time excluded"
                        ),
                        ref,
                    )
            elif phase == "Drive":
                for check, message in {
                    "enter_aim": "The existing golfer/throw path reached a legal drive release.",
                    "start_throw": "The existing animated RHBH request started successfully.",
                    "release_notify_fires": "Exactly one authoritative release commit was recorded.",
                    "disc_launches": "The selected gameplay disc launched with nonzero motion.",
                    "physics_advances": "The fixed-step flight produced actual trajectory samples.",
                    "disc_settles_or_holes_out": "The drive naturally settled in Circle 2.",
                    "lie_updates": "The resolved drive produced a finite legal Circle 2 lie.",
                    "replay_exits_cleanly": "Normal and Throw Lab replays exited before the phase continued.",
                }.items():
                    set_result(results, check, "Passed", message, ref, duration)
                launch_seconds, launch_channel = marker_delta_seconds(
                    process_dir,
                    process_id,
                    "Presentation.Throw.Release.",
                    "Presentation.Flight.Airborne",
                )
                record_contract_time_bound(
                    results,
                    "disc_launches",
                    launch_seconds,
                    f"release-to-airborne UE log delta from {launch_channel}",
                    ref,
                )
                settle_seconds, settle_channel = marker_delta_seconds(
                    process_dir,
                    process_id,
                    "Presentation.Throw.Release.",
                    "Ground settled on",
                )
                record_contract_time_bound(
                    results,
                    "disc_settles_or_holes_out",
                    settle_seconds,
                    f"release-to-settle UE log delta from {settle_channel}",
                    ref,
                )
            elif phase == "Finish":
                for check, message in {
                    "next_throw_available": "Two legal follow-up throws continued from resolved lies.",
                    "basket_detects_completion": "The third throw produced an actual caught basket outcome.",
                    "score_updates": "Exactly three strokes and zero penalties were recorded.",
                    "hole_completes": "Hole 1 completed once after the caught third throw.",
                }.items():
                    set_result(results, check, "Passed", message, ref, duration)
                record_contract_time_bound(
                    results,
                    "hole_completes",
                    duration,
                    "full Finish process wall time (conservative upper bound)",
                    ref,
                )
            else:
                for check, message in {
                    "save_player": "Setup player/character state survived fresh processes.",
                    "save_bag": "Stable selected equipment survived fresh-process reconstruction.",
                    "save_settings": "Normalized settings were written for isolated persistence.",
                    "save_round_or_career": "Completed Hole 1 round/lie state was saved in isolation.",
                    "load_save": "Verify loaded the exact isolated persisted state.",
                    "loaded_character_matches": "Verify matched the exact Setup character JSON.",
                    "loaded_bag_matches": "Verify retained the exact stable Touch/Base selection.",
                    "loaded_settings_match": "Verify matched the exact Setup settings JSON.",
                }.items():
                    set_result(results, check, "Passed", message, ref, duration)

        performance_path = session15_report_root / "Drive/PerformanceReport.json"
        canonical_path = session15_report_root / "Session15VerticalSliceReport.json"
        try:
            performance = s15.strict_json_load(performance_path)
            s15.validate_performance(performance, options.mode, phase_reports["Drive"])
        except Exception as exc:
            set_result(
                results,
                "physics_advances",
                "Failed",
                f"Session 15 performance validation failed: {type(exc).__name__}: {exc}",
                [
                    evidence_ref(
                        "Session15_Drive",
                        Path("Saved/Session16Reports/Processes/Session15_Drive/ProcessManifest.json"),
                        "strict performance validation failed",
                    )
                ],
                failure_code="PhysicsDidNotAdvance",
            )
            raise ProcessFailure(
                "PhysicsDidNotAdvance", f"Session15_Drive performance report: {exc}"
            ) from exc
        try:
            canonical = s15.strict_json_load(canonical_path)
            s15.validate_canonical(canonical, user_dir, phase_reports["Drive"])
        except Exception as exc:
            set_result(
                results,
                "load_save",
                "Failed",
                f"Session 15 canonical validation failed: {type(exc).__name__}: {exc}",
                [
                    evidence_ref(
                        "Session15_Verify",
                        Path("Saved/Session16Reports/Processes/Session15_Verify/ProcessManifest.json"),
                        "strict canonical validation failed",
                    )
                ],
                failure_code="LoadFailed",
            )
            raise ProcessFailure(
                "LoadFailed", f"Session15 canonical report: {exc}"
            ) from exc

        process_id = "MainMenu"
        process_dir = processes_root / process_id
        log_path = process_dir / f"{process_id}.log"
        command = base_command(executable, project, options.mode, user_dir, log_path)
        command.extend(
            [
                "-Session16MainMenuSmokeTest",
                "-Course=PineRidge",
                "-SkipHoleIntro",
                *options.extra_arg,
            ]
        )
        print(f"Running Session 16 evidence process: {process_id}", flush=True)
        try:
            manifest, manifest_path = run_process(
                s15,
                results,
                process_id,
                command,
                process_dir,
                options.timeout,
                MAIN_MENU_PASS,
                MAIN_MENU_FAIL,
                default_failure_code="MainMenuUnavailable",
            )
            remember_process(manifest, manifest_path)
        except ProcessFailure as exc:
            manifest_path = process_dir / "ProcessManifest.json"
            manifest = s15.strict_json_load(manifest_path)
            remember_process(manifest, manifest_path)
            evidence = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    "attempted native main-menu process failed",
                )
            ]
            evidence.append(observed_process_failure(manifest))
            runtime_markers = manifest.get("playability_failure_markers", [])
            valid_runtime_markers = [
                marker
                for marker in runtime_markers
                if marker.get("validation_status") == "valid"
            ]
            for marker in valid_runtime_markers:
                apply_runtime_marker_failure(results, marker, evidence)
            if not valid_runtime_markers:
                set_result(
                    results,
                    "main_menu_available",
                    "Failed",
                    str(exc),
                    evidence,
                    manifest.get("duration_seconds", 0.0),
                    "MainMenuUnavailable",
                )
            raise
        main_menu_ref = [
            evidence_ref(
                process_id,
                manifest_path.relative_to(user_dir),
                "native front end and playable continue path exact pass marker",
            )
        ]
        set_result(
            results,
            "main_menu_available",
            "Passed",
            "The native front end was visible and its existing continue path restored playable Hole 1 state.",
            main_menu_ref,
            manifest["duration_seconds"],
        )

        for rules_name, rules_value, check, pass_marker, fail_marker, code in (
            (
                "RulesOB",
                "OB",
                "ob_penalty_if_triggered",
                RULES_OB_PASS,
                RULES_OB_FAIL,
                "OutOfBoundsRuleFailed",
            ),
            (
                "RulesWater",
                "Water",
                "water_penalty_if_triggered",
                RULES_WATER_PASS,
                RULES_WATER_FAIL,
                "WaterRuleFailed",
            ),
        ):
            process_id = rules_name
            process_dir = processes_root / process_id
            log_path = process_dir / f"{process_id}.log"
            command = base_command(
                executable, project, options.mode, user_dir, log_path
            )
            command.extend(
                [
                    f"-Session16RulesSmokeTest={rules_value}",
                    "-Course=PineRidge",
                    "-SkipHoleIntro",
                    *options.extra_arg,
                ]
            )
            print(f"Running Session 16 evidence process: {process_id}", flush=True)
            try:
                manifest, manifest_path = run_process(
                    s15,
                    results,
                    process_id,
                    command,
                    process_dir,
                    options.timeout,
                    pass_marker,
                    fail_marker,
                    default_failure_code=code,
                )
                remember_process(manifest, manifest_path)
            except ProcessFailure as exc:
                manifest_path = process_dir / "ProcessManifest.json"
                manifest = s15.strict_json_load(manifest_path)
                remember_process(manifest, manifest_path)
                evidence = [
                    evidence_ref(
                        process_id,
                        manifest_path.relative_to(user_dir),
                        f"attempted {rules_value} rules process failed",
                    )
                ]
                evidence.append(observed_process_failure(manifest))
                runtime_markers = manifest.get("playability_failure_markers", [])
                valid_runtime_markers = [
                    marker
                    for marker in runtime_markers
                    if marker.get("validation_status") == "valid"
                ]
                for marker in valid_runtime_markers:
                    apply_runtime_marker_failure(results, marker, evidence)
                if not valid_runtime_markers:
                    set_result(
                        results,
                        check,
                        "Failed",
                        str(exc),
                        evidence,
                        manifest.get("duration_seconds", 0.0),
                        code,
                    )
                raise
            rules_ref = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    f"real {rules_value} trigger and penalty exact pass marker",
                )
            ]
            set_result(
                results,
                check,
                "Passed",
                f"The real {rules_value} trigger path applied and reported its authoritative penalty.",
                rules_ref,
                manifest["duration_seconds"],
            )

        process_id = "TemporaryStateRecovery"
        process_dir = processes_root / process_id
        log_path = process_dir / f"{process_id}.log"
        command = base_command(executable, project, options.mode, user_dir, log_path)
        command.extend(
            [
                "-Session12PresentationSmokeTest",
                "-Session16CorePlayabilityGate",
                "-Course=PineRidge",
                "-SkipHoleIntro",
                *options.extra_arg,
            ]
        )
        print(f"Running Session 16 evidence process: {process_id}", flush=True)
        try:
            manifest, manifest_path = run_process(
                s15,
                results,
                process_id,
                command,
                process_dir,
                options.timeout,
                SESSION12_PRESENTATION_PASS,
                SESSION12_PRESENTATION_FAIL,
                required_markers=(TEMPORARY_STATE_RECOVERY_PASS,),
                default_failure_code="ResumeFailed",
                required_marker_failure_code="ResumeFailed",
            )
            remember_process(manifest, manifest_path)
        except ProcessFailure as exc:
            manifest_path = process_dir / "ProcessManifest.json"
            manifest = s15.strict_json_load(manifest_path)
            remember_process(manifest, manifest_path)
            evidence = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    "attempted temporary-state recovery process failed",
                )
            ]
            evidence.append(observed_process_failure(manifest))
            runtime_markers = manifest.get("playability_failure_markers", [])
            valid_runtime_markers = [
                marker
                for marker in runtime_markers
                if marker.get("validation_status") == "valid"
            ]
            for marker in valid_runtime_markers:
                apply_runtime_marker_failure(results, marker, evidence)
            if not valid_runtime_markers:
                set_result(
                    results,
                    "pause_resume",
                    "Failed",
                    str(exc),
                    evidence,
                    manifest.get("duration_seconds", 0.0),
                    "ResumeFailed",
                )
            raise
        temporary_state_ref = [
            evidence_ref(
                process_id,
                manifest_path.relative_to(user_dir),
                "Session 12 presentation pass plus exact Session 16 recovery marker",
            )
        ]
        for check, message in {
            "pause_resume": "Pause and resume completed through the existing presentation controls.",
            "camera_recovers": "Replay teardown restored the authoritative gameplay camera target.",
            "input_context_recovers": "Replay teardown restored the gameplay input context without a stuck temporary route.",
        }.items():
            set_result(
                results,
                check,
                "Passed",
                message,
                temporary_state_ref,
                manifest["duration_seconds"],
            )

        process_id = "ThreeHoleRound"
        process_dir = processes_root / process_id
        log_path = process_dir / f"{process_id}.log"
        command = base_command(executable, project, options.mode, user_dir, log_path)
        command.extend(
            [
                "-ThreeHoleRoundSmokeTest",
                "-Session16CorePlayabilityGate",
                "-Course=PineRidge",
                "-SkipHoleIntro",
                *options.extra_arg,
            ]
        )
        print(f"Running Session 16 evidence process: {process_id}", flush=True)
        try:
            manifest, manifest_path = run_process(
                s15,
                results,
                process_id,
                command,
                process_dir,
                options.round_timeout,
                THREE_HOLE_PASS,
                THREE_HOLE_FAIL,
                required_markers=(RESULTS_CONTINUE_PASS,),
                default_failure_code="RoundCompletionFailed",
                required_marker_failure_code="ResultsScreenFailed",
            )
            remember_process(manifest, manifest_path)
        except ProcessFailure as exc:
            manifest_path = process_dir / "ProcessManifest.json"
            manifest = s15.strict_json_load(manifest_path)
            remember_process(manifest, manifest_path)
            evidence = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    "attempted three-hole process failed",
                )
            ]
            evidence.append(observed_process_failure(manifest))
            valid_runtime_markers = [
                marker
                for marker in manifest.get("playability_failure_markers", [])
                if marker.get("validation_status") == "valid"
            ]
            for marker in valid_runtime_markers:
                apply_runtime_marker_failure(results, marker, evidence)
            results_failure = manifest.get("results_continue_failure")
            fallback_check: str | None = None
            if isinstance(results_failure, dict):
                fallback_check = (
                    "return_to_menu_or_continue"
                    if results_failure.get("failure_code") == "PlayerCouldNotContinue"
                    else "results_screen_available"
                )
            elif not valid_runtime_markers:
                fallback_check = "round_completes"
            if fallback_check is not None:
                set_result(
                    results,
                    fallback_check,
                    "Failed",
                    str(exc),
                    evidence,
                    manifest.get("duration_seconds", 0.0),
                    exc.failure_code,
                )
            raise
        round_ref = [
            evidence_ref(
                process_id,
                manifest_path.relative_to(user_dir),
                "real ThreeHoleRoundSmokeTest exact pass marker",
            )
        ]
        for check, message in {
            "advance_to_next_hole": "Two ordered authored-hole transitions completed.",
            "spawn_next_hole": "Holes 1, 2, and 3 activated in manifest order.",
            "scorecard_persists_between_holes": "Scores progressed and final totals retained all holes.",
            "multiple_holes_complete": "Exactly three authored holes completed.",
            "round_completes": "The three-hole round completed at 3 strokes, par 11, score -8.",
            "results_screen_available": "The Session 16 gate observed completed-round scorecard/results state.",
            "return_to_menu_or_continue": "The existing completed-round continue path restarted Hole 1 in a playable state.",
        }.items():
            set_result(
                results,
                check,
                "Passed",
                message,
                round_ref,
                manifest["duration_seconds"],
            )
        record_contract_time_bound(
            results,
            "round_completes",
            manifest["duration_seconds"],
            "full ThreeHoleRound process wall time (conservative upper bound)",
            round_ref,
        )

        process_id = "MissingCosmeticRecovery"
        process_dir = processes_root / process_id
        log_path = process_dir / f"{process_id}.log"
        validation_slot = (
            "DiscGolfTour_Automation_Session7FullCharacter_"
            f"{run_id.replace('-', '')}"
        )
        command = base_command(executable, project, options.mode, user_dir, log_path)
        command.extend(
            [
                "-Session7FullCharacterThrowSmokeTest",
                "-Session7FullCharacterFixture=MissingScarTattooOutfit",
                "-Session7FullCharacterValidationNoSave",
                f"-Session7FullCharacterValidationSaveSlot={validation_slot}",
                "-Session4Profile=Baseline",
                "-Course=PineRidge",
                *options.extra_arg,
            ]
        )
        print(f"Running Session 16 evidence process: {process_id}", flush=True)
        try:
            manifest, manifest_path = run_process(
                s15,
                results,
                process_id,
                command,
                process_dir,
                options.timeout,
                RECOVERY_PASS,
                RECOVERY_FAIL,
                RECOVERY_REQUIRED_TOKENS,
                default_failure_code="InvalidCosmeticRecoveryFailed",
            )
            remember_process(manifest, manifest_path)
        except ProcessFailure as exc:
            manifest_path = process_dir / "ProcessManifest.json"
            manifest = s15.strict_json_load(manifest_path)
            remember_process(manifest, manifest_path)
            evidence = [
                evidence_ref(
                    process_id,
                    manifest_path.relative_to(user_dir),
                    "attempted missing-cosmetic process failed",
                )
            ]
            evidence.append(observed_process_failure(manifest))
            valid_runtime_markers = [
                marker
                for marker in manifest.get("playability_failure_markers", [])
                if marker.get("validation_status") == "valid"
            ]
            for marker in valid_runtime_markers:
                apply_runtime_marker_failure(results, marker, evidence)
            if not valid_runtime_markers:
                set_result(
                    results,
                    "missing_cosmetic_falls_back",
                    "Failed",
                    str(exc),
                    evidence,
                    manifest.get("duration_seconds", 0.0),
                    exc.failure_code,
                )
            raise
        recovery_ref = [
            evidence_ref(
                process_id,
                manifest_path.relative_to(user_dir),
                "live invalid scar/tattoo/outfit fallback plus completed throw/recovery",
            )
        ]
        set_result(
            results,
            "missing_cosmetic_falls_back",
            "Passed",
            "Missing scar, tattoo, and outfit IDs resolved to safe defaults and play continued.",
            recovery_ref,
            manifest["duration_seconds"],
        )
        set_result(
            results,
            "no_duplicate_player",
            "Passed",
            "The recovery process reported exactly one authoritative player pawn.",
            recovery_ref,
            manifest["duration_seconds"],
        )
        set_result(
            results,
            "no_duplicate_disc",
            "Passed",
            "The recovery process reported exactly one authoritative gameplay disc.",
            recovery_ref,
            manifest["duration_seconds"],
        )
        set_result(
            results,
            "no_soft_lock",
            "Passed",
            "All attempted bounded gameplay processes completed without a playability-failure marker.",
            recovery_ref,
            sum(item["duration_seconds"] for item in process_records),
        )

    except ProcessFailure as exc:
        attempted_runtime_failed = True
        launcher_failures.append(
            {"failure_code": exc.failure_code, "message": str(exc)}
        )
    except Exception as exc:
        attempted_runtime_failed = True
        launcher_failures.append(
            {
                "failure_code": "UnexpectedError",
                "message": f"{type(exc).__name__}: {exc}",
            }
        )

    # A fresh focused automation artifact may close only the explicitly
    # recognized optional-asset recovery check.  All other prepared IDs require direct
    # runtime observations or a future, separately reviewed mapping.
    if focused_summary is not None:
        automation_map = {
            "missing_optional_asset_does_not_block_play": (
                "DiscGolfTour.Session16.Persistence.OptionalAssetFallback"
            ),
        }
        for check, full_path in automation_map.items():
            if full_path in focused_tests and results[check]["status"] != "Failed":
                set_result(
                    results,
                    check,
                    "Passed",
                    f"Focused automation {full_path} passed without errors.",
                    [automation_evidence(focused_summary, full_path)],
                    float(focused_tests[full_path].get("duration", 0.0)),
                )

    # State explicitly why the current direct runtime does not satisfy these
    # checks.  This is intentional pending evidence, not an inferred failure.
    pending_messages = {
        "main_menu_available": "The dedicated native main-menu lane did not complete with its exact pass marker.",
        "ob_penalty_if_triggered": "The dedicated real OB-trigger lane did not complete with its exact pass marker.",
        "water_penalty_if_triggered": "The dedicated real water-trigger lane did not complete with its exact pass marker.",
        "pause_resume": "The Session 12 presentation lane did not emit the exact Session 16 temporary-state recovery marker.",
        "camera_recovers": "The Session 12 presentation lane did not emit the exact Session 16 temporary-state recovery marker.",
        "input_context_recovers": "The Session 12 presentation lane did not emit the exact Session 16 temporary-state recovery marker.",
        "results_screen_available": "The Session 16 results/continue runtime marker was not observed.",
        "return_to_menu_or_continue": "The Session 16 results/continue runtime marker was not observed.",
        "missing_optional_asset_does_not_block_play": "No live optional-premium-asset-absent lane or recognized focused automation evidence is available.",
    }
    for check, message in pending_messages.items():
        if results[check]["status"] == "Blocked":
            results[check]["message"] = message

    print("Hashing project boundaries after Session 16...", flush=True)
    try:
        content_after = s15.tree_snapshot(CONTENT_ROOT)
        saves_after = s15.tree_snapshot(SAVEGAME_ROOT)
        protected_after = s15.file_record(PROTECTED_PROFILE)
        content_unchanged = content_after == content_before
        saves_unchanged = saves_after == saves_before
        protected_unchanged = protected_after == protected_before
        boundaries_unchanged = (
            content_unchanged and saves_unchanged and protected_unchanged
        )
        if not boundaries_unchanged:
            attempted_runtime_failed = True
            launcher_failures.append(
                {
                    "failure_code": "UnexpectedError",
                    "message": "project Content, SaveGames, or protected profile mutated",
                }
            )
        package_after = None
        if package_root is not None and package_before is not None:
            print("Hashing staged package after Session 16...", flush=True)
            package_after = s15.tree_snapshot(package_root)
            package_unchanged = package_after == package_before
            if not package_unchanged:
                attempted_runtime_failed = True
                launcher_failures.append(
                    {
                        "failure_code": "UnexpectedError",
                        "message": "staged package mutated during acceptance",
                    }
                )
        boundary = {
            "schema": BOUNDARY_SCHEMA,
            "content": {
                "file_count": len(content_before),
                "manifest_sha256_before": s15.snapshot_hash(content_before),
                "manifest_sha256_after": s15.snapshot_hash(content_after),
                "unchanged": content_unchanged,
                "files_before": content_before,
                "files_after": content_after,
            },
            "project_savegames": {
                "file_count": len(saves_before),
                "manifest_sha256_before": s15.snapshot_hash(saves_before),
                "manifest_sha256_after": s15.snapshot_hash(saves_after),
                "unchanged": saves_unchanged,
                "files_before": saves_before,
                "files_after": saves_after,
            },
            "protected_profile_before": protected_before,
            "protected_profile_after": protected_after,
            "protected_profile_unchanged": protected_unchanged,
        }
    except Exception as exc:
        attempted_runtime_failed = True
        launcher_failures.append(
            {
                "failure_code": "UnexpectedError",
                "message": f"boundary validation failed: {type(exc).__name__}: {exc}",
            }
        )
        boundary = {
            "schema": BOUNDARY_SCHEMA,
            "unchanged": False,
            "error": launcher_failures[-1]["message"],
        }
        package_after = None

    try:
        validate_result_catalog(ordered, level_by_check, results)
    except Exception as exc:
        attempted_runtime_failed = True
        launcher_failures.append(
            {
                "failure_code": "UnexpectedError",
                "message": f"result catalog validation failed: {type(exc).__name__}: {exc}",
            }
        )
    summaries = gate_summaries(ordered, level_by_check, results)
    statuses = [results[check]["status"] for check in ordered]
    any_check_failed = "Failed" in statuses
    any_pending = "Blocked" in statuses or "NotRun" in statuses
    if attempted_runtime_failed or any_check_failed or not boundaries_unchanged:
        overall_status = "Failed"
        result_label = "FAILED_CORE_PLAYABILITY_RELEASE_BLOCKED"
        exit_code = 1
    elif any_pending:
        overall_status = "Blocked"
        result_label = "BLOCKED_CORE_PLAYABILITY_EVIDENCE_PENDING_RELEASE_BLOCKED"
        exit_code = 2
    else:
        overall_status = "Passed"
        result_label = "PASS_CORE_PLAYABILITY_GATE_RELEASE_BLOCKED"
        exit_code = 0

    report = {
        "schema": REPORT_SCHEMA,
        "result": result_label,
        "run_id": run_id,
        "mode": options.mode,
        "started_utc": started_utc,
        "finished_utc": utc_now(),
        "external_user_dir": str(user_dir),
        "external_user_dir_parent_reason": (
            "Existing Session 15 runtime accepts only C:/DGTour_TestRuns/Session15/<GUID>."
        ),
        "backup_dir": str(backup_dir),
        "stable_report_root": str(stable_root),
        "overall_status": overall_status,
        "gate_summaries": summaries,
        "checks": [results[check] for check in ordered],
        "contract_time_bounds": {
            check: results[check].get(
                "time_bound",
                {
                    "check_id": check,
                    "limit_seconds": limit,
                    "observed_seconds": None,
                    "basis": "no accepted duration evidence",
                    "status": "NotObserved",
                },
            )
            for check, limit in CHECK_TIMEOUT_SECONDS.items()
        },
        "passed_checks": sum(value == "Passed" for value in statuses),
        "failed_checks": sum(value == "Failed" for value in statuses),
        "blocked_checks": sum(value == "Blocked" for value in statuses),
        "not_run_checks": sum(value == "NotRun" for value in statuses),
        "blocking_failures": sum(
            value in {"Failed", "Blocked", "NotRun"} for value in statuses
        ),
        "processes": process_records,
        "focused_automation": focused_summary,
        "launcher_failures": launcher_failures,
        "project_boundaries_unchanged": boundaries_unchanged,
        "package_unchanged": package_unchanged,
        "core_playability_gate_pass": all(
            summary["status"] == "Passed" for summary in summaries.values()
        ),
        "polish_allowed_by_playability_rule": (
            summaries["Smoke"]["status"] == "Passed"
            and summaries["CoreLoop"]["status"] == "Passed"
        ),
        "bounded_technical_evidence_only": True,
        "release_ready": False,
        "release_use_allowed": False,
        "production_readiness_approved": False,
        "release_blockers_inherited_from_session15": list(s15.EXPECTED_BLOCKERS),
        "external_user_dir_moved_recoverably": False,
    }

    try:
        if user_dir.is_dir():
            write_json(report_root / "BoundarySnapshot.json", boundary)
            if package_root is not None and package_before is not None:
                write_json(
                    report_root / "PackageManifest.json",
                    {
                        "schema": "DiscGolfTour.Session16PackageManifest.v1",
                        "package_root": str(package_root),
                        "executable": str(executable),
                        "file_count": len(package_before),
                        "manifest_sha256_before": s15.snapshot_hash(package_before),
                        "manifest_sha256_after": (
                            s15.snapshot_hash(package_after)
                            if package_after is not None
                            else None
                        ),
                        "unchanged": package_unchanged,
                        "files": package_before,
                    },
                )
            write_json(report_root / "PlayabilityReport.json", report)

        stable_root.parent.mkdir(parents=True, exist_ok=True)
        stable_root.mkdir()
        if report_root.is_dir():
            shutil.copytree(report_root, stable_root / "RuntimeEvidence")
        write_json(stable_root / "BoundarySnapshot.json", boundary)
        write_json(stable_root / "PlayabilityReport.json", report)

        marker_ok = (
            owned_marker.is_file()
            and owned_marker.read_text(encoding="ascii").strip() == run_id
            and user_dir.parent == RUN_PARENT
            and user_dir.name == run_id
        )
        if not attempted_runtime_failed and boundaries_unchanged and marker_ok:
            BACKUP_PARENT.mkdir(parents=True, exist_ok=True)
            shutil.move(str(user_dir), str(backup_dir))
            archived = backup_dir.is_dir() and not user_dir.exists()
            if not archived:
                raise RuntimeError("recoverable external-run move did not complete")
            report["external_user_dir_moved_recoverably"] = True
            report["archived_utc"] = utc_now()
            write_json(
                backup_dir / "Saved/Session16Reports/PlayabilityReport.json",
                report,
            )
            write_json(stable_root / "PlayabilityReport.json", report)
            write_json(
                stable_root / "RuntimeEvidence/PlayabilityReport.json", report
            )
        elif user_dir.is_dir():
            report["failed_run_retained_for_diagnosis"] = True
            write_json(report_root / "PlayabilityReport.json", report)
            write_json(stable_root / "PlayabilityReport.json", report)
            if (stable_root / "RuntimeEvidence").is_dir():
                write_json(
                    stable_root / "RuntimeEvidence/PlayabilityReport.json", report
                )
    except Exception as exc:
        copy_failure = f"evidence finalization failed: {type(exc).__name__}: {exc}"
        launcher_failures.append(
            {"failure_code": "UnexpectedError", "message": copy_failure}
        )
        report.update(
            {
                "result": "FAILED_CORE_PLAYABILITY_RELEASE_BLOCKED",
                "overall_status": "Failed",
                "core_playability_gate_pass": False,
                "launcher_failures": launcher_failures,
                "evidence_finalization_failed": True,
                "failed_run_retained_for_diagnosis": user_dir.is_dir(),
                "external_user_dir_moved_recoverably": archived,
            }
        )
        # Best-effort fail-closed reports must replace any earlier optimistic
        # copy.  Never remove a partial copy or a retained diagnostic run.
        failure_report_paths = [stable_root / "PlayabilityReport.json"]
        if user_dir.is_dir():
            failure_report_paths.append(report_root / "PlayabilityReport.json")
        if backup_dir.is_dir():
            failure_report_paths.append(
                backup_dir / "Saved/Session16Reports/PlayabilityReport.json"
            )
        if (stable_root / "RuntimeEvidence").is_dir():
            failure_report_paths.append(
                stable_root / "RuntimeEvidence/PlayabilityReport.json"
            )
        for failure_report_path in failure_report_paths:
            try:
                write_json(failure_report_path, report)
            except Exception:
                pass
        print(
            f"Session 16 {copy_failure}",
            file=sys.stderr,
        )
        return 1

    print(f"{result_label}: {stable_root}", flush=True)
    if archived:
        print(f"Run root moved recoverably to: {backup_dir}", flush=True)
    elif user_dir.is_dir():
        print(f"Run root retained for diagnosis: {user_dir}", file=sys.stderr)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
