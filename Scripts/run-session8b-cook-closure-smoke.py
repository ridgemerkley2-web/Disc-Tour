#!/usr/bin/env python3
"""Run the Session 8B cook-closure proof in one isolated packaged process.

This launcher deliberately refuses editor/build-tool executables. It requires a
fresh absolute external UUID directory for Unreal's ``-UserDir`` and removes
that owned directory on every exit path after collecting the process result.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "Content"
PRODUCTION_SAVE = ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
DEFAULT_ACCEPTED_BACKUP = Path(
    "C:/DGTour_Backups/Session7_Accepted/"
    "DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
DEFAULT_REPORT = (
    ROOT / "Saved/CharacterFramework/Session8BCookClosureSmoke.json"
)
DEFAULT_LOG = ROOT / "Saved/Logs/Session8BCookClosureSmoke.log"
EXPECTED_ACCEPTED_SAVE_BYTES = 5212
EXPECTED_ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)
PASS_PREFIX = "DG_SESSION8_COOK_CLOSURE_SMOKE: PASS"
FAIL_PREFIX = "DG_SESSION8_COOK_CLOSURE_SMOKE: FAIL"
REQUIRED_PASS_TOKENS = (
    "runtime_packages=69",
    "runtime_exports=69",
    "direct_assets=66",
    "generated_classes=3",
    "metahuman_runtime_packages=264",
    "metahuman_runtime_exports=264",
    "metahuman_direct_assets=",
    "metahuman_generated_classes=",
    "backend_profiles=2",
    "excluded_absent=12",
    "editor_source_mhc_absent=1",
    "unrelated_metahuman_manifest_roots=0",
    "native_pawn=1",
    "outfit=1",
    "customization=1",
    "avatar_adapter=1",
    "release_authority=DiscGolfTourGameMode.RequestThrowFromGrip",
    "flight_authority=DiscActor.DiscFlightComponent",
    "practice_snapshot_save_suppressed=1",
)
ISOLATED_SAVE_RELATIVE_PATH = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
EXPECTED_EXTERNAL_USER_DIR_PARENT = Path(r"C:\DGTour_TestRuns\Session8B")
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--executable", required=True,
        help="Path to a packaged DiscGolfTour executable (never UnrealEditor).",
    )
    parser.add_argument(
        "--user-dir", required=True,
        help="Fresh absolute external directory whose final name is a UUID.",
    )
    parser.add_argument(
        "--accepted-backup", default=str(DEFAULT_ACCEPTED_BACKUP),
    )
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--log", default=str(DEFAULT_LOG))
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict:
    if not path.is_file():
        return {"present": False}
    stat = path.stat()
    return {
        "present": True,
        "bytes": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": sha256(path),
    }


def content_snapshot() -> dict[str, dict]:
    if not CONTENT.is_dir():
        return {}
    result: dict[str, dict] = {}
    for path in sorted(CONTENT.rglob("*")):
        if path.is_file():
            result[path.relative_to(ROOT).as_posix()] = file_record(path)
    return result


def directory_snapshot(root: Path) -> dict[str, dict]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): file_record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def directory_topology_snapshot(root: Path) -> list[str]:
    if not root.is_dir():
        return []
    return sorted(
        path.relative_to(root).as_posix()
        for path in root.rglob("*") if path.is_dir())


def changed_paths(before: dict[str, dict], after: dict[str, dict]) -> list[str]:
    return sorted(
        key for key in before.keys() | after.keys()
        if before.get(key) != after.get(key)
    )


def is_within(path: Path, directory: Path) -> bool:
    return path == directory or directory in path.parents


def terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if sys.platform == "win32":
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


def validate_paths(options: argparse.Namespace) -> tuple[Path, Path, Path, Path, Path]:
    executable = Path(options.executable).expanduser().resolve()
    user_dir_input = Path(options.user_dir).expanduser()
    if not user_dir_input.is_absolute():
        raise ValueError("--user-dir must be absolute")
    user_dir = user_dir_input.resolve()
    accepted_backup = Path(options.accepted_backup).expanduser().resolve()
    report = Path(options.report).expanduser().resolve()
    log = Path(options.log).expanduser().resolve()

    forbidden_executable_tokens = (
        "unrealeditor", "unrealbuildtool", "automationtool", "runuat",
    )
    executable_name = executable.name.casefold()
    if not executable.is_file() or executable.suffix.casefold() != ".exe":
        raise FileNotFoundError(f"packaged executable not found: {executable}")
    if not executable_name.startswith("discgolftour") \
            or any(token in executable_name for token in forbidden_executable_tokens):
        raise ValueError(
            "--executable must be a packaged DiscGolfTour game executable"
        )
    if is_within(executable, ROOT) \
            or executable.parent.name.casefold() != "win64" \
            or executable.parent.parent.name.casefold() != "binaries":
        raise ValueError(
            "--executable must be the packaged inner Binaries/Win64 game "
            "executable, not a source-tree binary or bootstrap"
        )
    packaged_game_root = executable.parents[2]
    packaged_paks = packaged_game_root / "Content/Paks"
    if not packaged_paks.is_dir() \
            or not any(packaged_paks.glob("*.utoc")) \
            or not any(packaged_paks.glob("*.ucas")):
        raise ValueError(
            "packaged executable is missing its adjacent IoStore containers"
        )
    try:
        parsed_uuid = uuid.UUID(user_dir.name)
    except ValueError as exc:
        raise ValueError("--user-dir final directory name must be a UUID") from exc
    if str(parsed_uuid) != user_dir.name.casefold():
        raise ValueError("--user-dir must use the canonical hyphenated UUID form")
    expected_user_parent = EXPECTED_EXTERNAL_USER_DIR_PARENT.resolve()
    if user_dir.parent != expected_user_parent:
        raise ValueError(
            "--user-dir must be one direct UUID child of "
            f"{expected_user_parent}")
    if user_dir.exists():
        raise FileExistsError(f"fresh --user-dir already exists: {user_dir}")
    if not user_dir.parent.is_dir():
        raise FileNotFoundError(
            f"external --user-dir parent must already exist: {user_dir.parent}"
        )
    if is_within(user_dir, ROOT) or is_within(user_dir, executable.parent):
        raise ValueError(
            "--user-dir must be external to both source and packaged executable trees"
        )
    if is_within(executable, user_dir):
        raise ValueError("packaged executable may not be inside --user-dir")
    if is_within(accepted_backup, ROOT):
        raise ValueError("accepted backup must remain external to the repository")
    for output in (report, log):
        if is_within(output, CONTENT) or is_within(output, ROOT / "Saved/SaveGames") \
                or is_within(output, user_dir):
            raise ValueError(
                "report/log outputs may not enter Content, SaveGames, or --user-dir"
            )
    if not 30.0 <= options.timeout_seconds <= 1800.0:
        raise ValueError("timeout must be between 30 and 1800 seconds")
    reserved_prefixes = (
        "-session8", "-userdir", "-abslog", "-run=", "-project=",
        "-editor", "-server", "-cook",
        "-nometahumanaccountportalloginfallback",
    )
    if any(
        argument.casefold().startswith(reserved_prefixes)
        for argument in options.extra_arg
    ):
        raise ValueError("extra arguments may not override owned packaged-smoke arguments")
    return executable, user_dir, accepted_backup, report, log


def main() -> int:
    options = parse_args()
    executable, user_dir, accepted_backup, report_path, log_path = (
        validate_paths(options)
    )
    production_before = file_record(PRODUCTION_SAVE)
    backup_before = file_record(accepted_backup)
    project_save_directory_before = directory_snapshot(
        ROOT / "Saved/SaveGames"
    )
    project_save_topology_before = directory_topology_snapshot(
        ROOT / "Saved/SaveGames")
    if production_before.get("bytes") != EXPECTED_ACCEPTED_SAVE_BYTES \
            or production_before.get("sha256") != EXPECTED_ACCEPTED_SAVE_SHA256:
        raise RuntimeError("production save is not the frozen accepted Session 7 A999 baseline")
    if backup_before.get("bytes") != EXPECTED_ACCEPTED_SAVE_BYTES \
            or backup_before.get("sha256") != EXPECTED_ACCEPTED_SAVE_SHA256:
        raise RuntimeError("external backup is not the frozen accepted Session 7 A999 baseline")

    print("Hashing Content before the packaged cook-closure smoke...", flush=True)
    content_before = content_snapshot()
    content_topology_before = directory_topology_snapshot(CONTENT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.unlink(missing_ok=True)
    command = [
        str(executable),
        "-Session8CookClosureSmokeTest",
        "-Session8ValidationNoSave",
        f"-UserDir={user_dir}",
        "-unattended",
        "-nop4",
        NO_PORTAL_SWITCH,
        "-nosplash",
        "-NullRHI",
        "-NoSound",
        "-UTF8Output",
        "-stdout",
        "-FullStdOutLogOutput",
        f"-abslog={log_path}",
        *options.extra_arg,
    ]

    process: subprocess.Popen[str] | None = None
    timed_out = False
    console = ""
    launch_error = ""
    cleanup_error = ""
    external_user_dir_files_before_cleanup: dict[str, dict] = {}
    external_save_files_before_cleanup: list[str] = []
    started = time.monotonic()
    owned_marker = user_dir / ".dg_session8b_cook_closure_owned"
    try:
        user_dir.mkdir()
        owned_marker.write_text(str(user_dir.name) + "\n", encoding="ascii")
        process = subprocess.Popen(
            command,
            cwd=executable.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            creationflags=(
                subprocess.CREATE_NEW_PROCESS_GROUP
                if sys.platform == "win32" else 0
            ),
        )
        try:
            console, _ = process.communicate(timeout=options.timeout_seconds)
        except subprocess.TimeoutExpired as exc:
            timed_out = True
            partial = exc.output or ""
            if isinstance(partial, bytes):
                partial = partial.decode("utf-8", errors="replace")
            terminate(process)
            tail, _ = process.communicate()
            console = partial + (tail or "")
    except Exception as exc:  # captured in the evidence report below
        launch_error = f"{type(exc).__name__}: {exc}"
        if process is not None:
            terminate(process)
    finally:
        try:
            external_user_dir_files_before_cleanup = directory_snapshot(user_dir)
            external_save_files_before_cleanup = sorted(
                path for path in external_user_dir_files_before_cleanup
                if Path(path).suffix.casefold() == ".sav"
            )
            if user_dir.is_dir() and owned_marker.is_file() \
                    and owned_marker.read_text(encoding="ascii").strip() == user_dir.name:
                shutil.rmtree(user_dir)
            elif user_dir.exists():
                cleanup_error = "owned marker missing or changed; refusing cleanup"
        except Exception as exc:
            cleanup_error = f"{type(exc).__name__}: {exc}"

    log_text = (
        log_path.read_text(encoding="utf-8-sig", errors="replace")
        if log_path.is_file() else console
    )
    pass_lines = [line for line in log_text.splitlines() if PASS_PREFIX in line]
    fail_lines = [line for line in log_text.splitlines() if FAIL_PREFIX in line]
    selected_pass = pass_lines[0] if len(pass_lines) == 1 else ""

    print("Hashing Content after the packaged cook-closure smoke...", flush=True)
    content_after = content_snapshot()
    production_after = file_record(PRODUCTION_SAVE)
    backup_after = file_record(accepted_backup)
    project_save_directory_after = directory_snapshot(
        ROOT / "Saved/SaveGames"
    )
    project_save_topology_after = directory_topology_snapshot(
        ROOT / "Saved/SaveGames")
    content_topology_after = directory_topology_snapshot(CONTENT)
    changed_content = changed_paths(content_before, content_after)
    isolated_save_record = external_user_dir_files_before_cleanup.get(
        ISOLATED_SAVE_RELATIVE_PATH, {}
    )
    checks = {
        "process_launched": not launch_error,
        "completed_before_timeout": not timed_out,
        "return_code_zero": process is not None and process.returncode == 0,
        "pass_once": len(pass_lines) == 1,
        "fail_absent": not fail_lines,
        "required_contract_tokens": all(
            token in selected_pass for token in REQUIRED_PASS_TOKENS
        ),
        "production_save_unchanged": production_before == production_after,
        "external_backup_unchanged": backup_before == backup_after,
        "project_save_directory_unchanged": (
            project_save_directory_before == project_save_directory_after
        ),
        "project_save_topology_unchanged": (
            project_save_topology_before == project_save_topology_after),
        "content_unchanged": not changed_content,
        "content_topology_unchanged": (
            content_topology_before == content_topology_after),
        "isolated_test_save_only": (
            external_save_files_before_cleanup
            == [ISOLATED_SAVE_RELATIVE_PATH]
            and isolated_save_record.get("present") is True
            and isolated_save_record.get("bytes", 0) > 0
            and bool(isolated_save_record.get("sha256"))
        ),
        "external_user_dir_removed": not user_dir.exists() and not cleanup_error,
    }
    failures = [name for name, passed in checks.items() if not passed]
    result = {
        "schema": "DiscGolfTour.Session8BCookClosureSmoke.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not failures else "FAIL",
        "metahuman_explicit_cook_root_package_count": 264,
        "metahuman_explicit_root_proof": "PACKAGED_EXPORTS_LOADED",
        "metahuman_transitive_dependency_graph_count_from_fresh2": 340,
        "metahuman_transitive_dependency_proof": (
            "PACKAGED_IOSTORE_HARD_REFERENCE_LOAD_REQUIRES_FINAL_AUDIT"),
        "packaged_executable": str(executable),
        "command": command,
        "duration_seconds": round(time.monotonic() - started, 3),
        "return_code": process.returncode if process is not None else None,
        "timed_out": timed_out,
        "log": str(log_path),
        "log_record": file_record(log_path),
        "pass_line": selected_pass,
        "failure_lines": fail_lines[-5:],
        "launch_error": launch_error,
        "cleanup_error": cleanup_error,
        "external_user_dir": str(user_dir),
        "external_user_dir_parent": str(EXPECTED_EXTERNAL_USER_DIR_PARENT),
        "no_portal_switch": NO_PORTAL_SWITCH,
        "external_user_dir_files_before_cleanup": (
            external_user_dir_files_before_cleanup
        ),
        "external_save_files_before_cleanup": external_save_files_before_cleanup,
        "isolated_test_save_record": isolated_save_record,
        "external_user_dir_residue": user_dir.exists(),
        "production_save": str(PRODUCTION_SAVE),
        "production_save_before": production_before,
        "production_save_after": production_after,
        "project_save_directory_before": project_save_directory_before,
        "project_save_directory_after": project_save_directory_after,
        "project_save_topology_before": project_save_topology_before,
        "project_save_topology_after": project_save_topology_after,
        "external_accepted_backup": str(accepted_backup),
        "external_backup_before": backup_before,
        "external_backup_after": backup_after,
        "content_file_count_before": len(content_before),
        "content_file_count_after": len(content_after),
        "content_topology_before": content_topology_before,
        "content_topology_after": content_topology_after,
        "changed_content": changed_content,
        "checks": checks,
        "failed_checks": failures,
        "console_tail": console[-4000:],
        "editor_or_ubt_launched_by_this_script": False,
        "packaged_game_launched": process is not None,
    }
    report_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        f"DG SESSION 8 PACKAGED COOK CLOSURE {result['status']}: "
        f"report={report_path}",
        flush=True,
    )
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
