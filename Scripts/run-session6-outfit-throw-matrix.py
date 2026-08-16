#!/usr/bin/env python3
"""Run the Session 6 outfit/RHBH matrix in isolated Unreal processes."""

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


ROOT = Path(__file__).absolute().parents[1]
PROJECT = ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
REPORT = ROOT / "Saved" / "CharacterFramework" / "Session6OutfitThrowMatrix.json"
LOG_DIR = ROOT / "Saved" / "Logs"
ROWS = (
    ("BaselineCore", "Baseline"),
    ("BaselineLayered", "Baseline"),
    ("ShortFull", "ShortCompact"),
    ("TallFull", "TallLongArms"),
    ("SliderExtremeFull", "SliderMax"),
    ("MissingItem", "Baseline"),
)
PASS_PREFIX = "SESSION 6 OUTFIT THROW SMOKE PASS:"
FAIL_PREFIX = "SESSION 6 OUTFIT THROW SMOKE FAIL:"
BASE_PASS_PREFIX = "SESSION 3 ONE-THROW SMOKE PASS:"
REQUIRED = (
    "runtime_profile_asserted=1",
    "leader_pose=1",
    "master_skeleton=1",
    "collision_free=1",
    "disc_grip_r_unchanged=1",
    "visual_bag_not_inventory=1",
    "authority_fields_unchanged=1",
    "one_animation=1",
    "one_release=1",
    "one_authoritative_disc=1",
    "one_completed_flight=1",
    "one_recovery=1",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=os.environ.get("UE_EDITOR_CMD", str(DEFAULT_ENGINE)))
    parser.add_argument("--project", default=str(PROJECT))
    parser.add_argument("--report", default=str(REPORT))
    parser.add_argument("--log-dir", default=str(LOG_DIR))
    parser.add_argument("--course", default="PineRidge")
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    return parser.parse_args()


def resolve_engine(value: str) -> Path:
    path = Path(value).expanduser().absolute()
    if path.is_dir():
        for candidate in (
            path / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe",
            path / "Binaries/Win64/UnrealEditor-Cmd.exe",
        ):
            if candidate.is_file():
                return candidate
    return path


def sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def snapshot_files(root: Path, package_only: bool = False) -> dict[str, tuple[int, int]]:
    if not root.is_dir():
        return {}
    result: dict[str, tuple[int, int]] = {}
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if package_only and path.suffix.lower() not in (".uasset", ".umap"):
            continue
        stat = path.stat()
        result[str(path.absolute())] = (stat.st_size, stat.st_mtime_ns)
    return result


def diff_snapshots(
    before: dict[str, tuple[int, int]], after: dict[str, tuple[int, int]]
) -> list[str]:
    return sorted(
        path for path in before.keys() | after.keys()
        if before.get(path) != after.get(path)
    )


def terminate(process: subprocess.Popen[str]) -> None:
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
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()


def run_row(
    engine: Path,
    project: Path,
    log_dir: Path,
    fixture: str,
    profile: str,
    course: str,
    timeout: float,
    extra: list[str],
) -> dict:
    log_path = log_dir / f"Session6OutfitThrow_{fixture}.log"
    log_path.unlink(missing_ok=True)
    command = [
        str(engine), str(project), "-game", f"-Course={course}",
        "-Session6OutfitThrowSmokeTest", f"-Session6OutfitFixture={fixture}",
        "-Session6OutfitValidationNoSave", f"-Session4Profile={profile}",
        "-unattended", "-nop4", "-nosplash",
        "-NullRHI", "-NoSound", "-UTF8Output", "-stdout",
        "-FullStdOutLogOutput", f"-abslog={log_path}", *extra,
    ]
    started = time.monotonic()
    process = subprocess.Popen(
        command, cwd=project.parent, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    timed_out = False
    console = ""
    try:
        console, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        terminate(process)
        tail, _ = process.communicate()
        console = partial + (tail or "")
    text = log_path.read_text(encoding="utf-8-sig", errors="replace") if log_path.is_file() else ""
    pass_lines = [line for line in text.splitlines() if PASS_PREFIX in line]
    fail_lines = [line for line in text.splitlines() if FAIL_PREFIX in line]
    base_lines = [line for line in text.splitlines() if BASE_PASS_PREFIX in line]
    selected = pass_lines[0] if len(pass_lines) == 1 else ""
    profile_override_token = (
        f"SESSION 4 PROFILE OVERRIDE: {profile} (transient, save slot unchanged)."
    )
    checks = {
        "completed_before_timeout": not timed_out,
        "return_code_zero": process.returncode == 0,
        "pass_once": len(pass_lines) == 1,
        "fail_absent": not fail_lines,
        "accepted_session3_contract_once": len(base_lines) == 1,
        "exact_runtime_profile_override_once": text.count(profile_override_token) == 1,
        "required_outfit_authority_tokens": all(token in selected for token in REQUIRED),
        "missing_fallback_proven": fixture != "MissingItem" or "missing_fallback=1" in selected,
    }
    failures = [name for name, passed in checks.items() if not passed]
    return {
        "fixture": fixture,
        "profile": profile,
        "status": "PASS" if not failures else "FAIL",
        "command": command,
        "duration_seconds": round(time.monotonic() - started, 3),
        "return_code": process.returncode,
        "timed_out": timed_out,
        "log": str(log_path),
        "log_sha256": sha256(log_path),
        "checks": checks,
        "failed_checks": failures,
        "pass_line": selected,
        "failure_lines": fail_lines[-5:],
        "console_tail": console[-4000:],
    }


def main() -> int:
    args = parse_args()
    engine = resolve_engine(args.engine)
    project = Path(args.project).expanduser().absolute()
    report_path = Path(args.report).expanduser().absolute()
    log_dir = Path(args.log_dir).expanduser().absolute()
    if not engine.is_file():
        raise FileNotFoundError(f"UnrealEditor-Cmd.exe not found: {engine}")
    if not project.is_file():
        raise FileNotFoundError(f"project not found: {project}")
    if not 30 <= args.timeout_seconds <= 3600:
        raise ValueError("timeout must be between 30 and 3600 seconds")
    reserved = (
        "-session6outfit", "-session6outfitvalidationnosave",
        "-session4profile", "-abslog",
    )
    if any(arg.lower().startswith(reserved) for arg in args.extra_arg):
        raise ValueError("extra arguments may not override owned Session 6 arguments")
    log_dir.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    packages_before = snapshot_files(project.parent / "Content", package_only=True)
    saves_before = snapshot_files(project.parent / "Saved/SaveGames")

    results = []
    for index, (fixture, profile) in enumerate(ROWS, 1):
        print(f"[{index}/{len(ROWS)}] {fixture} / {profile}", flush=True)
        results.append(run_row(
            engine, project, log_dir, fixture, profile, args.course,
            args.timeout_seconds, args.extra_arg,
        ))
        print(f"[{index}/{len(ROWS)}] {results[-1]['status']}", flush=True)
    changed_packages = diff_snapshots(
        packages_before,
        snapshot_files(project.parent / "Content", package_only=True),
    )
    changed_save_games = diff_snapshots(
        saves_before,
        snapshot_files(project.parent / "Saved/SaveGames"),
    )
    no_persistent_writes = not changed_packages and not changed_save_games
    failed = [row["fixture"] for row in results if row["status"] != "PASS"]
    if not no_persistent_writes:
        failed.append("PersistentWriteGuard")
    git = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=False,
    )
    report = {
        "schema": "DiscGolfTour.Session6OutfitThrowMatrix.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not failed else "FAIL",
        "git_head": git.stdout.strip() if git.returncode == 0 else None,
        "execution_policy": "SEQUENTIAL_ISOLATED_UNREAL_PROCESS_PER_ROW",
        "rows_requested": len(ROWS),
        "required_acceptance_rows": 5,
        "additional_missing_item_row": 1,
        "no_persistent_writes": no_persistent_writes,
        "changed_packages": changed_packages,
        "changed_save_games": changed_save_games,
        "allowed_outputs": "Matrix report and isolated logs under Saved only.",
        "failed_rows": failed,
        "results": results,
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"SESSION 6 OUTFIT THROW MATRIX {report['status']}: report={report_path}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
