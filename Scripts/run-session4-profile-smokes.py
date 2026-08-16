#!/usr/bin/env python3
"""Run the accepted Session 3 one-throw smoke across Session 4 profiles.

The five processes are deliberately sequential: each owns one Unreal process,
one transient ``-Session4Profile`` override, and one absolute log.  Marketplace
content, profile assets, and the player's save payload are never edited by this
orchestrator; the runtime override reports that its save slot is unchanged.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import Sequence


PROJECT_ROOT = Path(__file__).absolute().parents[1]
DEFAULT_PROJECT = PROJECT_ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
DEFAULT_REPORT = (
    PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session4ProfileSmokeReport.json"
)
DEFAULT_LOG_DIR = PROJECT_ROOT / "Saved" / "Logs"
PROFILES = ("ShortCompact", "Baseline", "TallLongArms", "SliderMin", "SliderMax")

PASS_PREFIX = "SESSION 3 ONE-THROW SMOKE PASS:"
FAIL_PREFIX = "SESSION 3 ONE-THROW SMOKE FAIL:"
REQUIRED_PASS_TOKENS = (
    "aim=1",
    "animation=1",
    "release=1",
    "authoritative_disc=1",
    "existing_flight_solver=1",
    "nonzero_motion=1",
    "FollowThrough_phase=1",
    "Recovery_phase=1",
    "DG_Throw_Finished=1",
    "recovery_callback=1",
    "original_player=1",
    "camera_return=1",
    "next_action_begin_and_cancel=1",
    "evaluated ReachBack cancellation recovered once",
    "launched 0 discs through the former release time",
)
RESERVED_EXTRA_ARG_PREFIXES = (
    "-session3onethrowsmoketest",
    "-session4profile",
    "-abslog",
)


def _bounded_timeout(value: str) -> float:
    try:
        timeout = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("timeout must be a number of seconds") from exc
    if not 30.0 <= timeout <= 3600.0:
        raise argparse.ArgumentTypeError("timeout must be between 30 and 3600 seconds")
    return timeout


def _resolve_engine(value: str) -> Path:
    candidate = Path(value).expanduser().absolute()
    if candidate.is_dir():
        rooted = candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
        engine_dir = candidate / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
        if rooted.is_file():
            return rooted
        if engine_dir.is_file():
            return engine_dir
    return candidate


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run five sequential Session 4 profile one-throw smokes."
    )
    parser.add_argument(
        "--engine",
        default=os.environ.get("UE_EDITOR_CMD", str(DEFAULT_ENGINE)),
        help="UnrealEditor-Cmd.exe or the UE installation/Engine directory.",
    )
    parser.add_argument(
        "--project",
        default=str(DEFAULT_PROJECT),
        help="Path to DiscGolfTour.uproject.",
    )
    parser.add_argument(
        "--course",
        default="PineRidge",
        help="Existing course command-line ID used by the accepted smoke.",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=_bounded_timeout,
        default=300.0,
        help="Per-profile hard timeout (30-3600, default 300).",
    )
    parser.add_argument(
        "--log-dir",
        default=str(DEFAULT_LOG_DIR),
        help="Directory for the five separate Unreal logs.",
    )
    parser.add_argument(
        "--report",
        default=str(DEFAULT_REPORT),
        help="Aggregate JSON report path.",
    )
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="Additional Unreal argument; repeat as needed. Reserved smoke arguments are rejected.",
    )
    return parser.parse_args(argv)


def _validate_configuration(args: argparse.Namespace) -> tuple[Path, Path, Path, Path]:
    engine = _resolve_engine(args.engine)
    project = Path(args.project).expanduser().absolute()
    log_dir = Path(args.log_dir).expanduser().absolute()
    report = Path(args.report).expanduser().absolute()
    if not engine.is_file():
        raise FileNotFoundError(f"Unreal command executable was not found: {engine}")
    if not project.is_file() or project.suffix.lower() != ".uproject":
        raise FileNotFoundError(f"Unreal project was not found: {project}")
    if not args.course.strip():
        raise ValueError("course must not be empty")
    for extra in args.extra_arg:
        folded = extra.strip().lower()
        if any(folded.startswith(prefix) for prefix in RESERVED_EXTRA_ARG_PREFIXES):
            raise ValueError(f"extra argument attempts to override an owned smoke argument: {extra}")
    return engine, project, log_dir, report


def _terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        process.kill()
    try:
        process.wait(timeout=10.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5.0)


def _read_log(log_path: Path) -> str:
    if not log_path.is_file():
        return ""
    return log_path.read_text(encoding="utf-8-sig", errors="replace")


def _sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _git_head(project_root: Path) -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=project_root,
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def _command_for(
    engine: Path,
    project: Path,
    course: str,
    profile: str,
    log_path: Path,
    extra_args: Sequence[str],
) -> list[str]:
    return [
        str(engine),
        str(project),
        "-game",
        f"-Course={course}",
        "-Session3OneThrowSmokeTest",
        f"-Session4Profile={profile}",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-NullRHI",
        "-NoSound",
        "-UTF8Output",
        "-stdout",
        "-FullStdOutLogOutput",
        f"-abslog={log_path}",
        *extra_args,
    ]


def _run_profile(
    engine: Path,
    project: Path,
    course: str,
    profile: str,
    log_dir: Path,
    timeout_seconds: float,
    extra_args: Sequence[str],
) -> dict:
    log_path = log_dir / f"Session4ProfileSmoke_{profile}.log"
    if log_path.exists():
        log_path.unlink()
    command = _command_for(engine, project, course, profile, log_path, extra_args)
    creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    started = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=project.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creation_flags,
    )
    timed_out = False
    console_output = ""
    try:
        console_output, _ = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        _terminate_process(process)
        remaining, _ = process.communicate()
        console_output = partial + (remaining or "")
    duration_seconds = time.monotonic() - started

    log_text = _read_log(log_path)
    pass_lines = [line.strip() for line in log_text.splitlines() if PASS_PREFIX in line]
    fail_lines = [line.strip() for line in log_text.splitlines() if FAIL_PREFIX in line]
    override_token = (
        f"SESSION 4 PROFILE OVERRIDE: {profile} (transient, save slot unchanged)."
    )
    override_count = log_text.count(override_token)
    pass_line = pass_lines[0] if len(pass_lines) == 1 else ""
    validations = {
        "process_completed_before_timeout": not timed_out,
        "process_return_code_zero": process.returncode == 0,
        "separate_log_created": log_path.is_file(),
        "profile_override_reported_once": override_count == 1,
        "one_throw_pass_reported_once": len(pass_lines) == 1,
        "one_throw_fail_not_reported": not fail_lines,
        "single_authority_release_and_flight": all(
            token in pass_line
            for token in (
                "release=1",
                "authoritative_disc=1",
                "existing_flight_solver=1",
                "nonzero_motion=1",
            )
        ),
        "pre_release_cancellation_zero_discs": all(
            token in pass_line
            for token in (
                "evaluated ReachBack cancellation recovered once",
                "launched 0 discs through the former release time",
            )
        ),
        "follow_through_and_recovery": all(
            token in pass_line
            for token in (
                "FollowThrough_phase=1",
                "Recovery_phase=1",
                "DG_Throw_Finished=1",
                "recovery_callback=1",
                "camera_return=1",
                "next_action_begin_and_cancel=1",
            )
        ),
        "complete_accepted_pass_contract": all(
            token in pass_line for token in REQUIRED_PASS_TOKENS
        ),
    }
    failed_validations = [name for name, passed in validations.items() if not passed]
    return {
        "profile": profile,
        "status": "PASS" if not failed_validations else "FAIL",
        "command": command,
        "duration_seconds": round(duration_seconds, 3),
        "timeout_seconds": timeout_seconds,
        "timed_out": timed_out,
        "return_code": process.returncode,
        "log_path": str(log_path),
        "log_sha256": _sha256(log_path),
        "override_count": override_count,
        "pass_banner_count": len(pass_lines),
        "fail_banner_count": len(fail_lines),
        "validations": validations,
        "failed_validations": failed_validations,
        "pass_line": pass_line,
        "failure_lines": fail_lines[-5:],
        "console_tail": console_output[-4000:],
    }


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    engine, project, log_dir, report_path = _validate_configuration(args)
    log_dir.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    profile_results: list[dict] = []
    for index, profile in enumerate(PROFILES, start=1):
        print(f"[{index}/{len(PROFILES)}] Running Session 4 profile smoke: {profile}", flush=True)
        started = time.monotonic()
        try:
            result = _run_profile(
                engine,
                project,
                args.course.strip(),
                profile,
                log_dir,
                args.timeout_seconds,
                args.extra_arg,
            )
        except Exception as exc:
            # One process-launch/pipe failure must not erase the aggregate
            # acceptance artifact or prevent the remaining isolated profiles
            # from being attempted.
            log_path = log_dir / f"Session4ProfileSmoke_{profile}.log"
            result = {
                "profile": profile,
                "status": "FAIL",
                "command": _command_for(
                    engine,
                    project,
                    args.course.strip(),
                    profile,
                    log_path,
                    args.extra_arg,
                ),
                "duration_seconds": round(time.monotonic() - started, 3),
                "timeout_seconds": args.timeout_seconds,
                "timed_out": False,
                "return_code": None,
                "log_path": str(log_path),
                "log_sha256": _sha256(log_path),
                "override_count": 0,
                "pass_banner_count": 0,
                "fail_banner_count": 0,
                "validations": {"process_launched_and_collected": False},
                "failed_validations": ["process_launched_and_collected"],
                "pass_line": "",
                "failure_lines": [f"{type(exc).__name__}: {exc}"],
                "console_tail": "",
            }
        profile_results.append(result)
        print(
            f"[{index}/{len(PROFILES)}] {profile}: {result['status']} "
            f"({result['duration_seconds']:.3f}s, rc={result['return_code']})",
            flush=True,
        )

    failed_profiles = [item["profile"] for item in profile_results if item["status"] != "PASS"]
    report = {
        "schema": "DiscGolfTour.Session4ProfileSmokeReport.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_SESSION4_PROFILE_SMOKES" if not failed_profiles else "FAIL",
        "git_head": _git_head(project.parent),
        "engine": str(engine),
        "project": str(project),
        "course": args.course.strip(),
        "execution_policy": "SEQUENTIAL_ONE_UNREAL_PROCESS_PER_PROFILE",
        "profiles_requested": list(PROFILES),
        "profiles_passed": len(PROFILES) - len(failed_profiles),
        "profiles_failed": failed_profiles,
        "results": profile_results,
        "writes": [str(report_path), *(item["log_path"] for item in profile_results)],
    }
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if failed_profiles:
        print(
            "SESSION 4 PROFILE SMOKES FAIL: " + ", ".join(failed_profiles),
            file=sys.stderr,
        )
        return 1
    print(
        f"SESSION 4 PROFILE SMOKES PASS: profiles={len(PROFILES)} report={report_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
