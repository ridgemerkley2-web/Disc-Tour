#!/usr/bin/env python3
"""Run the Session 5 pipeline montage through the three accepted body profiles.

Each profile owns one isolated Unreal process.  The guarded montage override is
available only to ``-unattended -Session5MocapPipelineSmokeTest``; normal play
and the separately-run accepted Session 3 smoke retain the prototype montage.
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
    PROJECT_ROOT
    / "Saved"
    / "CharacterFramework"
    / "Session5MocapProfileSmokeReport.json"
)
DEFAULT_LOG_DIR = PROJECT_ROOT / "Saved" / "Logs"
PROFILES = ("ShortCompact", "Baseline", "TallLongArms")
SESSION5_PASS_PREFIX = "SESSION 5 MOCAP PIPELINE SMOKE PASS:"
SESSION5_FAIL_PREFIX = "SESSION 5 MOCAP PIPELINE SMOKE FAIL:"
SESSION3_PASS_PREFIX = "SESSION 3 ONE-THROW SMOKE PASS:"
SESSION3_FAIL_PREFIX = "SESSION 3 ONE-THROW SMOKE FAIL:"
MONTAGE_OVERRIDE_PREFIX = "SESSION 5 MOCAP VALIDATION MONTAGE OVERRIDE:"
PIPELINE_MONTAGE = (
    "/Game/DiscGolf/Animation/Mocap/Production/"
    "AM_DG_RHBH_SyntheticPipelineTest_v001."
    "AM_DG_RHBH_SyntheticPipelineTest_v001"
)
REQUIRED_SESSION5_TOKENS = (
    "pipeline_montage=1",
    "cached_FThrowCommand=1",
    "cancel_before_release_zero_discs=1",
    "release_once=1",
    "authoritative_disc=1",
    "existing_flight=1",
    "follow_through=1",
    "recovery=1",
    "next_action=1",
    "default_route_untouched=1",
)
REQUIRED_SESSION3_TOKENS = (
    "aim=1",
    "animation=1",
    "release=1",
    "authoritative_disc=1",
    "existing_flight_solver=1",
    "nonzero_motion=1",
    "FollowThrough_phase=1",
    "Recovery_phase=1",
    "DG_Throw_Finished=1",
    "camera_return=1",
    "next_action_begin_and_cancel=1",
    "evaluated ReachBack cancellation recovered once",
    "launched 0 discs through the former release time",
)
RESERVED_EXTRA_PREFIXES = (
    "-session5mocappipelinesmoketest",
    "-session4profile",
    "-abslog",
)


def _bounded_timeout(value: str) -> float:
    try:
        timeout = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("timeout must be numeric") from exc
    if not 30.0 <= timeout <= 3600.0:
        raise argparse.ArgumentTypeError("timeout must be between 30 and 3600 seconds")
    return timeout


def _resolve_engine(value: str) -> Path:
    candidate = Path(value).expanduser().absolute()
    if candidate.is_dir():
        choices = (
            candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
            candidate / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
        )
        for choice in choices:
            if choice.is_file():
                return choice
    return candidate


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the Session 5 pipeline one-throw smoke across three profiles."
    )
    parser.add_argument(
        "--engine", default=os.environ.get("UE_EDITOR_CMD", str(DEFAULT_ENGINE))
    )
    parser.add_argument("--project", default=str(DEFAULT_PROJECT))
    parser.add_argument("--course", default="PineRidge")
    parser.add_argument("--timeout-seconds", type=_bounded_timeout, default=300.0)
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--extra-arg", action="append", default=[])
    return parser.parse_args(argv)


def _validate(args: argparse.Namespace) -> tuple[Path, Path, Path, Path]:
    engine = _resolve_engine(args.engine)
    project = Path(args.project).expanduser().absolute()
    log_dir = Path(args.log_dir).expanduser().absolute()
    report = Path(args.report).expanduser().absolute()
    if not engine.is_file():
        raise FileNotFoundError(f"Unreal command executable not found: {engine}")
    if not project.is_file() or project.suffix.lower() != ".uproject":
        raise FileNotFoundError(f"Unreal project not found: {project}")
    if not args.course.strip():
        raise ValueError("course must not be empty")
    for extra in args.extra_arg:
        folded = extra.strip().lower()
        if any(folded.startswith(prefix) for prefix in RESERVED_EXTRA_PREFIXES):
            raise ValueError(f"extra argument overrides an owned smoke argument: {extra}")
    return engine, project, log_dir, report


def _sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _git_head() -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def _terminate(process: subprocess.Popen[str]) -> None:
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


def _command(
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
        "-Session5MocapPipelineSmokeTest",
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


def _run_one(
    engine: Path,
    project: Path,
    course: str,
    profile: str,
    log_dir: Path,
    timeout: float,
    extra_args: Sequence[str],
) -> dict:
    log_path = log_dir / f"Session5MocapProfileSmoke_{profile}.log"
    if log_path.exists():
        log_path.unlink()
    command = _command(engine, project, course, profile, log_path, extra_args)
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
    output = ""
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        _terminate(process)
        tail, _ = process.communicate()
        output = partial + (tail or "")

    log_text = (
        log_path.read_text(encoding="utf-8-sig", errors="replace")
        if log_path.is_file()
        else ""
    )
    session5_pass = [line for line in log_text.splitlines() if SESSION5_PASS_PREFIX in line]
    session5_fail = [line for line in log_text.splitlines() if SESSION5_FAIL_PREFIX in line]
    session3_pass = [line for line in log_text.splitlines() if SESSION3_PASS_PREFIX in line]
    session3_fail = [line for line in log_text.splitlines() if SESSION3_FAIL_PREFIX in line]
    profile_token = f"SESSION 4 PROFILE OVERRIDE: {profile} (transient, save slot unchanged)."
    selected_line = session5_pass[0] if len(session5_pass) == 1 else ""
    base_line = session3_pass[0] if len(session3_pass) == 1 else ""
    validations = {
        "completed_before_timeout": not timed_out,
        "return_code_zero": process.returncode == 0,
        "separate_log_created": log_path.is_file(),
        "profile_override_once": log_text.count(profile_token) == 1,
        "pipeline_montage_override_once": log_text.count(MONTAGE_OVERRIDE_PREFIX) == 1,
        "pipeline_path_reported": PIPELINE_MONTAGE in log_text,
        "session5_pass_once": len(session5_pass) == 1,
        "session5_fail_absent": not session5_fail,
        "accepted_session3_contract_pass_once": len(session3_pass) == 1,
        "accepted_session3_fail_absent": not session3_fail,
        "session5_tokens_complete": all(
            token in selected_line for token in REQUIRED_SESSION5_TOKENS
        ),
        "single_authority_and_cancel_contract_complete": all(
            token in base_line for token in REQUIRED_SESSION3_TOKENS
        ),
    }
    failures = [name for name, passed in validations.items() if not passed]
    return {
        "profile": profile,
        "status": "PASS" if not failures else "FAIL",
        "command": command,
        "duration_seconds": round(time.monotonic() - started, 3),
        "timeout_seconds": timeout,
        "timed_out": timed_out,
        "return_code": process.returncode,
        "log_path": str(log_path),
        "log_sha256": _sha256(log_path),
        "validations": validations,
        "failed_validations": failures,
        "session5_pass_line": selected_line,
        "session3_pass_line": base_line,
        "failure_lines": (session5_fail + session3_fail)[-5:],
        "console_tail": output[-4000:],
    }


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    engine, project, log_dir, report_path = _validate(args)
    log_dir.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    results: list[dict] = []
    for index, profile in enumerate(PROFILES, start=1):
        print(f"[{index}/{len(PROFILES)}] Session 5 mocap smoke: {profile}", flush=True)
        try:
            result = _run_one(
                engine,
                project,
                args.course.strip(),
                profile,
                log_dir,
                args.timeout_seconds,
                args.extra_arg,
            )
        except Exception as exc:
            result = {
                "profile": profile,
                "status": "FAIL",
                "duration_seconds": 0.0,
                "return_code": None,
                "log_path": str(log_dir / f"Session5MocapProfileSmoke_{profile}.log"),
                "validations": {"process_launched_and_collected": False},
                "failed_validations": ["process_launched_and_collected"],
                "failure_lines": [f"{type(exc).__name__}: {exc}"],
            }
        results.append(result)
        print(f"[{index}/{len(PROFILES)}] {profile}: {result['status']}", flush=True)

    failed = [item["profile"] for item in results if item["status"] != "PASS"]
    report = {
        "schema": "DiscGolfTour.Session5MocapProfileSmokeReport.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_SESSION5_MOCAP_PROFILE_SMOKES" if not failed else "FAIL",
        "git_head": _git_head(),
        "engine": str(engine),
        "project": str(project),
        "course": args.course.strip(),
        "pipeline_montage": PIPELINE_MONTAGE,
        "pipeline_asset_status": "SYNTHETIC_TEST_DO_NOT_SHIP",
        "execution_policy": "SEQUENTIAL_ONE_UNREAL_PROCESS_PER_PROFILE",
        "profiles_requested": list(PROFILES),
        "profiles_passed": len(PROFILES) - len(failed),
        "profiles_failed": failed,
        "prototype_fallback_regression": {
            "owned_by": "existing -Session3OneThrowSmokeTest",
            "included_in_this_matrix": False,
            "reason": "The pipeline override and prototype fallback are intentionally separate processes.",
        },
        "results": results,
        "writes": [str(report_path), *(item["log_path"] for item in results)],
    }
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if failed:
        print("SESSION 5 MOCAP PROFILE SMOKES FAIL: " + ", ".join(failed), file=sys.stderr)
        return 1
    print(f"SESSION 5 MOCAP PROFILE SMOKES PASS: profiles=3 report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
