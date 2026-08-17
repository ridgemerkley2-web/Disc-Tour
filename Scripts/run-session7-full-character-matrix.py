#!/usr/bin/env python3
"""Run the 12-case Session 7 full-character/RHBH matrix in isolation."""

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
import uuid


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
REPORT = ROOT / "Saved/CharacterFramework/Session7FullCharacterMatrix.json"
LOG_DIR = ROOT / "Saved/Logs"

# Exact directive order.  The final value states which accepted Session 4
# body-profile override the isolated process must report.
ROWS = (
    ("BaselineDefaultShortSimple", "Baseline"),
    ("BaselineSquareBeardHatFull", "Baseline"),
    ("ShortNarrowMedium", "ShortCompact"),
    ("TallRoundBeardFull", "TallLongArms"),
    ("BodyFaceExtremes", "SliderMax"),
    ("MissingHair", "Baseline"),
    ("MissingFacialHairEyebrow", "Baseline"),
    ("MissingScarTattooOutfit", "Baseline"),
    ("Schema8Migration", "Baseline"),
    ("RandomizeApplyReload", "Baseline"),
    ("RandomizeCancel", "Baseline"),
    ("CompleteCharacterRHBH", "Baseline"),
)

PASS_PREFIX = "SESSION 7 FULL CHARACTER THROW SMOKE PASS:"
FAIL_PREFIX = "SESSION 7 FULL CHARACTER THROW SMOKE FAIL:"
BASE_PASS_PREFIX = "SESSION 3 ONE-THROW SMOKE PASS:"
BASE_FAIL_PREFIX = "SESSION 3 ONE-THROW SMOKE FAIL:"
COMMON_PASS_TOKENS = (
    "runtime_profile_asserted=1",
    "full_customization=1",
    "schema9=1",
    "one_pawn=1",
    "one_customization_component=1",
    "one_outfit_component=1",
    "head_master_skeleton=1",
    "head_leader_pose=1",
    "cosmetics_attached=1",
    "cosmetics_collision_free=1",
    "disc_grip_r_contract_preserved=1",
    "appearance_authority_unchanged=1",
    "hair_hat_selection_preserved=1",
    "one_animation=1",
    "one_release=1",
    "one_authoritative_disc=1",
    "one_completed_flight=1",
    "one_recovery=1",
)
ROW_PASS_TOKENS = {
    "BaselineDefaultShortSimple": ("face_default=1", "hair_short=1", "simple_outfit=1"),
    "BaselineSquareBeardHatFull": (
        "face_square=1", "facialhair_beard=1", "hat_hides_hair=1", "full_outfit=1",
    ),
    "ShortNarrowMedium": ("face_narrow=1", "hair_medium=1"),
    "TallRoundBeardFull": ("face_round=1", "facialhair_beard=1", "full_outfit=1"),
    "BodyFaceExtremes": ("body_face_min_max=1", "full_outfit=1"),
    "MissingHair": ("missing_hair_fallback=1", "missing_fallback=1"),
    "MissingFacialHairEyebrow": (
        "missing_facialhair_fallback=1", "missing_eyebrow_fallback=1",
        "missing_fallback=1",
    ),
    "MissingScarTattooOutfit": (
        "missing_scar_tattoo_fallback=1", "missing_outfit_fallback=1",
        "missing_fallback=1",
    ),
    "Schema8Migration": (
        "schema8_migration=1", "migration_reload=1", "migration_preserved_schema8=1",
    ),
    "RandomizeApplyReload": (
        "randomize_catalog_valid=1", "randomize_locks_respected=1",
        "apply_reloaded=1", "validation_temp_slot_deleted=1",
    ),
    "RandomizeCancel": (
        "randomize_catalog_valid=1", "cancel_restored_exact=1",
        "cancel_wrote_no_save=1",
    ),
    "CompleteCharacterRHBH": (
        "complete_character=1", "release_components_stable=1",
        "followthrough_components_stable=1",
    ),
}

PACKAGE_SUFFIXES = {".uasset", ".umap", ".uexp", ".ubulk", ".uptnl"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=os.environ.get("UE_EDITOR_CMD", str(DEFAULT_ENGINE)))
    parser.add_argument("--project", default=str(PROJECT))
    parser.add_argument("--report", default=str(REPORT))
    parser.add_argument("--log-dir", default=str(LOG_DIR))
    parser.add_argument("--course", default="PineRidge")
    parser.add_argument("--timeout-seconds", type=float, default=360.0)
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


def file_sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def snapshot_tree(root: Path, suffixes: set[str] | None = None) -> dict[str, dict]:
    if not root.is_dir():
        return {}
    result: dict[str, dict] = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if suffixes is not None and path.suffix.casefold() not in suffixes:
            continue
        stat = path.stat()
        result[str(path.absolute())] = {
            "bytes": stat.st_size,
            "sha256": file_sha256(path),
        }
    return result


def diff_snapshots(before: dict[str, dict], after: dict[str, dict]) -> list[str]:
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
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
        )
    else:
        process.kill()
    try:
        process.wait(timeout=10.0)
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
    log_path = log_dir / f"Session7FullCharacter_{fixture}.log"
    log_path.unlink(missing_ok=True)
    slot = "DiscGolfTour_Automation_Session7FullCharacter_" + uuid.uuid4().hex
    slot_path = project.parent / "Saved/SaveGames" / f"{slot}.sav"
    production_save = (
        project.parent / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    )
    if slot_path.exists():
        raise RuntimeError(f"fresh GUID validation slot unexpectedly exists: {slot_path}")
    command = [
        str(engine), str(project), "-game", f"-Course={course}",
        "-Session7FullCharacterThrowSmokeTest",
        f"-Session7FullCharacterFixture={fixture}",
        "-Session7FullCharacterValidationNoSave",
        f"-Session7FullCharacterValidationSaveSlot={slot}",
        f"-Session4Profile={profile}",
        "-unattended", "-nop4", "-nosplash", "-NullRHI", "-NoSound",
        "-UTF8Output", "-stdout", "-FullStdOutLogOutput", f"-abslog={log_path}",
        *extra,
    ]
    packages_before = snapshot_tree(
        project.parent / "Content", PACKAGE_SUFFIXES)
    saves_before = snapshot_tree(project.parent / "Saved/SaveGames")
    production_before = file_sha256(production_save)
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

    text = log_path.read_text(encoding="utf-8-sig", errors="replace") if log_path.is_file() else console
    pass_lines = [line for line in text.splitlines() if PASS_PREFIX in line]
    fail_lines = [line for line in text.splitlines() if FAIL_PREFIX in line]
    base_pass = [line for line in text.splitlines() if BASE_PASS_PREFIX in line]
    base_fail = [line for line in text.splitlines() if BASE_FAIL_PREFIX in line]
    selected = pass_lines[0] if len(pass_lines) == 1 else ""
    expected_profile = f"SESSION 4 PROFILE OVERRIDE: {profile} (transient, save slot unchanged)."
    temp_deleted_by_runner = not slot_path.exists()
    production_after = file_sha256(production_save)
    packages_after = snapshot_tree(
        project.parent / "Content", PACKAGE_SUFFIXES)
    saves_after = snapshot_tree(project.parent / "Saved/SaveGames")
    changed_packages = diff_snapshots(packages_before, packages_after)
    changed_save_games = diff_snapshots(saves_before, saves_after)
    if not temp_deleted_by_runner:
        # Cleanup is defensive only.  The row remains failed because the C++
        # runner did not prove its required deletion/absence contract.
        slot_path.unlink(missing_ok=True)
    checks = {
        "completed_before_timeout": not timed_out,
        "return_code_zero": process.returncode == 0,
        "pass_once": len(pass_lines) == 1,
        "fail_absent": not fail_lines,
        "accepted_session3_pass_once": len(base_pass) == 1,
        "accepted_session3_fail_absent": not base_fail,
        "exact_runtime_profile_override_once": text.count(expected_profile) == 1,
        "common_full_character_and_authority_tokens": all(
            token in selected for token in COMMON_PASS_TOKENS
        ),
        "fixture_specific_tokens": all(
            token in selected for token in ROW_PASS_TOKENS[fixture]
        ),
        "validation_temp_slot_deleted_by_runner": temp_deleted_by_runner,
        "no_package_or_save_writes": (
            not changed_packages and not changed_save_games
        ),
        "production_save_sha256_unchanged": production_before == production_after,
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
        "log_sha256": file_sha256(log_path),
        "validation_slot": slot,
        "validation_temp_slot_deleted": temp_deleted_by_runner,
        "production_save": str(production_save),
        "production_save_sha256_before": production_before,
        "production_save_sha256_after": production_after,
        "production_save_sha256_unchanged": production_before == production_after,
        "changed_packages": changed_packages,
        "changed_save_games": changed_save_games,
        "checks": checks,
        "failed_checks": failures,
        "pass_line": selected,
        "failure_lines": (fail_lines + base_fail)[-5:],
        "console_tail": console[-4000:],
    }


def main() -> int:
    options = parse_args()
    engine = resolve_engine(options.engine)
    project = Path(options.project).expanduser().absolute()
    report_path = Path(options.report).expanduser().absolute()
    log_dir = Path(options.log_dir).expanduser().absolute()
    if not engine.is_file():
        raise FileNotFoundError(f"UnrealEditor-Cmd.exe not found: {engine}")
    if not project.is_file():
        raise FileNotFoundError(f"project not found: {project}")
    if not 30.0 <= options.timeout_seconds <= 3600.0:
        raise ValueError("timeout must be between 30 and 3600 seconds")
    reserved = (
        "-session7fullcharacter", "-session4profile", "-abslog",
    )
    if any(arg.casefold().startswith(reserved) for arg in options.extra_arg):
        raise ValueError("extra arguments may not override owned Session 7 arguments")
    if not options.course.strip():
        raise ValueError("course may not be empty")
    log_dir.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    print("Hashing Content and SaveGames before the isolated matrix...", flush=True)
    packages_before = snapshot_tree(project.parent / "Content", PACKAGE_SUFFIXES)
    saves_before = snapshot_tree(project.parent / "Saved/SaveGames")
    production_save = (
        project.parent / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    )
    production_before = file_sha256(production_save)

    results: list[dict] = []
    for index, (fixture, profile) in enumerate(ROWS, 1):
        print(f"[{index}/{len(ROWS)}] {fixture} / {profile}", flush=True)
        try:
            result = run_row(
                engine, project, log_dir, fixture, profile,
                options.course.strip(), options.timeout_seconds, options.extra_arg,
            )
        except Exception as exc:
            result = {
                "fixture": fixture,
                "profile": profile,
                "status": "FAIL",
                "failed_checks": ["process_launched_and_collected"],
                "failure_lines": [f"{type(exc).__name__}: {exc}"],
            }
        results.append(result)
        print(f"[{index}/{len(ROWS)}] {result['status']}", flush=True)

    print("Hashing Content and SaveGames after the isolated matrix...", flush=True)
    packages_after = snapshot_tree(project.parent / "Content", PACKAGE_SUFFIXES)
    saves_after = snapshot_tree(project.parent / "Saved/SaveGames")
    changed_packages = diff_snapshots(packages_before, packages_after)
    changed_save_games = diff_snapshots(saves_before, saves_after)
    production_after = file_sha256(production_save)
    no_persistent_writes = not changed_packages and not changed_save_games
    production_unchanged = production_before == production_after
    failed = [row["fixture"] for row in results if row.get("status") != "PASS"]
    if not no_persistent_writes:
        failed.append("PersistentWriteGuard")
    if not production_unchanged:
        failed.append("ProductionSaveGuard")
    git = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT,
        capture_output=True, text=True, check=False,
    )
    report = {
        "schema": "DiscGolfTour.Session7FullCharacterMatrix.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not failed else "FAIL",
        "git_head": git.stdout.strip() if git.returncode == 0 else None,
        "execution_policy": "SEQUENTIAL_ISOLATED_UNREAL_PROCESS_PER_ROW",
        "rows_requested": len(ROWS),
        "rows": [fixture for fixture, _ in ROWS],
        "no_persistent_writes": no_persistent_writes,
        "production_save": str(production_save),
        "production_save_sha256_before": production_before,
        "production_save_sha256_after": production_after,
        "production_save_sha256_unchanged": production_unchanged,
        "package_snapshot_count": len(packages_after),
        "save_snapshot_count": len(saves_after),
        "changed_packages": changed_packages,
        "changed_save_games": changed_save_games,
        "allowed_outputs": "Aggregate report and isolated logs under Saved only; GUID validation slots must be deleted before each process exits.",
        "failed_rows": failed,
        "results": results,
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"SESSION 7 FULL CHARACTER MATRIX {report['status']}: report={report_path}")
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
