#!/usr/bin/env python3
"""Run the accepted one-throw smoke across the current v006 gameplay matrix.

The five processes are deliberately sequential: each owns one Unreal process,
one transient ``-Session4Profile`` override, and one absolute log.  Marketplace
content, profile assets, and the player's save payload are never edited by this
orchestrator; the runtime override reports that its save slot is unchanged. A
run only passes when the runtime also reports the exact compiled v6/v006 Drive
montage for every profile, so older generic Session 4 evidence cannot be
mistaken for current-production coverage.
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
import uuid


PROJECT_ROOT = Path(__file__).absolute().parents[1]
DEFAULT_PROJECT = PROJECT_ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
DEFAULT_EVIDENCE_ROOT = (
    PROJECT_ROOT.parent
    / f"{PROJECT_ROOT.name}_TestRuns"
    / "AnimationFluidity"
    / f"ProfileSmoke-v006-{uuid.uuid4()}"
)
DEFAULT_REPORT = DEFAULT_EVIDENCE_ROOT / "Session4ProfileSmokeReport.json"
DEFAULT_LOG_DIR = DEFAULT_EVIDENCE_ROOT / "Logs"
DEFAULT_USER_DIR_ROOT = DEFAULT_EVIDENCE_ROOT / "UserDirs"
PROFILES = ("ShortCompact", "Baseline", "TallLongArms", "SliderMin", "SliderMax")
REQUIRED_PRODUCTION_PROFILES = ("ShortCompact", "Baseline", "TallLongArms")
CURRENT_MOTION_VERSION = "v6"
CURRENT_ASSET_REVISION = "v006"
CURRENT_DRIVE_MONTAGE = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
    "AM_DG_RHBH_Drive_Procedural_v006.AM_DG_RHBH_Drive_Procedural_v006"
)
GAMEPLAY_CLAIM_BOUNDARY = {
    "metahuman_target_profile_matrix_complete": False,
    "human_animation_approval": False,
    "human_contact_approval": False,
    "shipping_inclusion_verified": False,
    "release_ready": False,
}

PASS_PREFIX = "SESSION 3 ONE-THROW SMOKE PASS:"
FAIL_PREFIX = "SESSION 3 ONE-THROW SMOKE FAIL:"
PRODUCTION_PROOF_PREFIX = "SESSION 19 V006 PRODUCTION PROFILE PASS:"
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
    "-userdir",
    "-noloadexistingsave",
    "-dgnoprofilewrites",
    "-dgdevelopertoolnosave",
    "-session19v006productionprofileproof",
    "-nullrhi",
    "-d3d",
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
        "--user-dir-root",
        default=str(DEFAULT_USER_DIR_ROOT),
        help="External root for a fresh, isolated Unreal UserDir per run/profile.",
    )
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="Additional Unreal argument; repeat as needed. Reserved smoke arguments are rejected.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run parser/identity mutation tests without launching Unreal.",
    )
    return parser.parse_args(argv)


def _validate_configuration(
    args: argparse.Namespace,
) -> tuple[Path, Path, Path, Path, Path]:
    engine = _resolve_engine(args.engine)
    project = Path(args.project).expanduser().absolute()
    log_dir = Path(args.log_dir).expanduser().absolute()
    report = Path(args.report).expanduser().absolute()
    user_dir_root = Path(args.user_dir_root).expanduser().absolute()
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
    if user_dir_root == project.parent or project.parent in user_dir_root.parents:
        raise ValueError(
            "user-dir-root must be outside the project so smoke saves cannot reach production data"
        )
    return engine, project, log_dir, report, user_dir_root


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


def _file_identity(path: Path) -> dict:
    return {
        "path": str(path),
        "exists": path.is_file(),
        "size_bytes": path.stat().st_size if path.is_file() else None,
        "sha256": _sha256(path),
    }


def _production_proof_payload(profile: str) -> str:
    return (
        f"profile={profile} recipe={CURRENT_MOTION_VERSION} "
        f"revision={CURRENT_ASSET_REVISION} montage={CURRENT_DRIVE_MONTAGE}"
    )


def _production_proof_lines(log_text: str) -> list[str]:
    return [
        line.strip()
        for line in log_text.splitlines()
        if PRODUCTION_PROOF_PREFIX in line
    ]


def _has_exact_production_proof(log_text: str, profile: str) -> bool:
    proof_lines = _production_proof_lines(log_text)
    if len(proof_lines) != 1:
        return False
    payload = proof_lines[0].split(PRODUCTION_PROOF_PREFIX, 1)[1].strip()
    return payload == _production_proof_payload(profile)


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
    user_dir: Path,
    extra_args: Sequence[str],
) -> list[str]:
    return [
        str(engine),
        str(project),
        "-game",
        f"-Course={course}",
        "-Session3OneThrowSmokeTest",
        f"-Session4Profile={profile}",
        "-Session19V006ProductionProfileProof",
        "-NoLoadExistingSave",
        "-DGNoProfileWrites",
        "-DGDeveloperToolNoSave",
        f"-UserDir={user_dir}",
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
    user_dir: Path,
    timeout_seconds: float,
    extra_args: Sequence[str],
) -> dict:
    log_path = log_dir / f"Session4ProfileSmoke_{profile}.log"
    if log_path.exists():
        log_path.unlink()
    user_dir.mkdir(parents=True, exist_ok=False)
    command = _command_for(
        engine, project, course, profile, log_path, user_dir, extra_args
    )
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
    production_proof_lines = _production_proof_lines(log_text)
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
        "exact_current_v006_production_motion_proof": (
            _has_exact_production_proof(log_text, profile)
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
        "isolated_user_dir": str(user_dir),
        "log_sha256": _sha256(log_path),
        "override_count": override_count,
        "pass_banner_count": len(pass_lines),
        "fail_banner_count": len(fail_lines),
        "production_proof_banner_count": len(production_proof_lines),
        "production_proof_line": (
            production_proof_lines[0] if len(production_proof_lines) == 1 else ""
        ),
        "validations": validations,
        "failed_validations": failed_validations,
        "pass_line": pass_line,
        "failure_lines": fail_lines[-5:],
        "console_tail": console_output[-4000:],
    }


def _run_self_test() -> int:
    profile = "Baseline"
    canonical = (
        "[2026.08.30]LogDiscGolfTour: Display: "
        f"{PRODUCTION_PROOF_PREFIX} {_production_proof_payload(profile)}\n"
    )
    cases = {
        "canonical": (canonical, profile, True),
        "wrong_profile": (canonical, "ShortCompact", False),
        "wrong_recipe": (canonical.replace("recipe=v6", "recipe=v5"), profile, False),
        "wrong_revision": (canonical.replace("revision=v006", "revision=v005"), profile, False),
        "wrong_montage": (canonical.replace("_v006", "_v005"), profile, False),
        "extra_claim": (canonical.rstrip() + " approved=true\n", profile, False),
        "missing": ("", profile, False),
        "duplicate": (canonical + canonical, profile, False),
    }
    failures = [
        name
        for name, (log_text, candidate_profile, expected) in cases.items()
        if _has_exact_production_proof(log_text, candidate_profile) != expected
    ]

    command = _command_for(
        DEFAULT_ENGINE,
        DEFAULT_PROJECT,
        "PineRidge",
        profile,
        Path(r"C:\proof\profile.log"),
        Path(r"C:\proof\UserDir"),
        (),
    )
    required_owned_args = {
        "-Session3OneThrowSmokeTest",
        "-Session4Profile=Baseline",
        "-Session19V006ProductionProfileProof",
        "-NoLoadExistingSave",
        "-DGNoProfileWrites",
        "-DGDeveloperToolNoSave",
        r"-UserDir=C:\proof\UserDir",
    }
    if not required_owned_args.issubset(set(command)):
        failures.append("owned_command_contract")
    for override in ("-Session4Profile=Other", "-NullRHI", "-UserDir=C:\\unsafe"):
        folded = override.lower()
        if not any(folded.startswith(prefix) for prefix in RESERVED_EXTRA_ARG_PREFIXES):
            failures.append(f"owned_override:{override}")
    if any(GAMEPLAY_CLAIM_BOUNDARY.values()):
        failures.append("honest_non_metahuman_claim_boundary")

    if failures:
        print("SESSION 4 CURRENT V006 GAMEPLAY PROFILE SMOKE SELF-TEST FAIL: " + ", ".join(failures))
        return 1
    print(f"SESSION 4 CURRENT V006 GAMEPLAY PROFILE SMOKE SELF-TEST PASS: cases={len(cases) + 1}")
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    if args.self_test:
        return _run_self_test()
    engine, project, log_dir, report_path, user_dir_root = _validate_configuration(args)
    log_dir.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    run_user_dir = user_dir_root / str(uuid.uuid4())
    run_user_dir.mkdir(parents=True, exist_ok=False)

    protected_profile_path = (
        project.parent / "Saved" / "SaveGames" / "DiscGolfTour_Profile_0.sav"
    )
    protected_profile_before = _file_identity(protected_profile_path)

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
                run_user_dir / profile,
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
                    run_user_dir / profile,
                    args.extra_arg,
                ),
                "duration_seconds": round(time.monotonic() - started, 3),
                "timeout_seconds": args.timeout_seconds,
                "timed_out": False,
                "return_code": None,
                "log_path": str(log_path),
                "isolated_user_dir": str(run_user_dir / profile),
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
        protected_profile_after_profile = _file_identity(protected_profile_path)
        profile_unchanged = protected_profile_after_profile == protected_profile_before
        result["protected_profile_after"] = protected_profile_after_profile
        result["validations"]["production_profile_unchanged"] = profile_unchanged
        if not profile_unchanged:
            result["status"] = "FAIL"
            if "production_profile_unchanged" not in result["failed_validations"]:
                result["failed_validations"].append("production_profile_unchanged")
        profile_results.append(result)
        print(
            f"[{index}/{len(PROFILES)}] {profile}: {result['status']} "
            f"({result['duration_seconds']:.3f}s, rc={result['return_code']})",
            flush=True,
        )

    failed_profiles = [item["profile"] for item in profile_results if item["status"] != "PASS"]
    protected_profile_after = _file_identity(protected_profile_path)
    report = {
        "schema": "DiscGolfTour.Session4CurrentProductionGameplayProfileSmokeReport.v2",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_CURRENT_V006_GAMEPLAY_PROFILE_MATRIX" if not failed_profiles else "FAIL",
        "git_head": _git_head(project.parent),
        "engine": str(engine),
        "project": str(project),
        "course": args.course.strip(),
        "execution_policy": "SEQUENTIAL_ONE_UNREAL_PROCESS_PER_PROFILE",
        "production_motion_identity": {
            "recipe_version": CURRENT_MOTION_VERSION,
            "asset_revision": CURRENT_ASSET_REVISION,
            "drive_montage": CURRENT_DRIVE_MONTAGE,
            "proof_mode": "EXACT_COMPILED_RUNTIME_SELECTION_PER_PROFILE",
        },
        "claim_boundary": {
            "gameplay_profile_matrix_complete": not failed_profiles,
            **GAMEPLAY_CLAIM_BOUNDARY,
        },
        "save_isolation_policy": (
            "FRESH_EXTERNAL_USERDIR_PLUS_NO_LOAD_AND_NO_WRITE_FLAGS_PER_PROFILE"
        ),
        "isolated_user_dir_root": str(run_user_dir),
        "protected_profile_before": protected_profile_before,
        "protected_profile_after": protected_profile_after,
        "protected_profile_unchanged": protected_profile_after == protected_profile_before,
        "profiles_requested": list(PROFILES),
        "required_production_profiles": list(REQUIRED_PRODUCTION_PROFILES),
        "supplemental_boundary_profiles": ["SliderMin", "SliderMax"],
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
        f"SESSION 4 CURRENT V006 GAMEPLAY PROFILE SMOKES PASS: "
        f"profiles={len(PROFILES)} report={report_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
