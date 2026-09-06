#!/usr/bin/env python3
"""Run and validate the four-process Session 15 Hole 1 acceptance lane.

The launcher owns one new GUID UserDir for Setup, Drive, Finish, and Verify.
On success it copies immutable evidence into the project report area and moves
the exact owned run directory to C:/DGTour_Backups. Failed runs are left in
place for diagnosis. Project Content and project SaveGames are read-only gates.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import uuid
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT / "DiscGolfTour.uproject"
RUN_PARENT = Path("C:/DGTour_TestRuns/Session15").resolve()
BACKUP_PARENT = Path("C:/DGTour_Backups").resolve()
CONTENT_ROOT = ROOT / "Content"
SAVEGAME_ROOT = ROOT / "Saved/SaveGames"
PROJECT_REPORT_ROOT = ROOT / "Saved/Session15Reports"
PROTECTED_PROFILE = SAVEGAME_ROOT / "DiscGolfTour_Profile_0.sav"
PROTECTED_BYTES = 5212
PROTECTED_SHA256 = "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"

PHASES = ("Setup", "Drive", "Finish", "Verify")
PHASE_SCHEMA = "DiscGolfTour.Session15VerticalSlicePhase.v1"
CANONICAL_SCHEMA = "DiscGolfTour.Session15VerticalSliceAcceptance.v1"
PERFORMANCE_SCHEMA = "DiscGolfTour.Session15VerticalSlicePerformance.v1"
CANONICAL_RESULT = "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED"
EXPECTED_BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
    "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
    "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
    "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
    "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING",
]
PASS_MARKERS = {
    phase: f"DG_SESSION15_VERTICAL_SLICE_{phase.upper()}: PASS" for phase in PHASES
}
CANONICAL_MARKER = (
    "DG_SESSION15_VERTICAL_SLICE_ACCEPTANCE: "
    "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED"
)
FAIL_MARKER = "DG_SESSION15_VERTICAL_SLICE_"
FATAL_LOG_TOKENS = (
    "fatal error:", "exception_access_violation", "assertion failed:",
    "ensure condition failed", "logrhi: error:", "lowlevelfatalerror",
)
COMMON_PHASE_FIELDS = {
    "schema", "phase", "passed", "failure_reason", "external_user_dir",
    "course_id", "hole_number", "strokes", "penalty_strokes", "hole_complete",
    "character_json", "outfit_json", "settings_json", "selected_disc_json",
    "round_json", "lie_json", "blocker_summary", "release_blockers",
}
SHA256_RE = re.compile(r"[0-9A-F]{64}\Z")


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_nonfinite(value: str) -> Any:
    raise ValueError(f"non-finite JSON number: {value}")


def require_finite(value: Any, context: str = "$") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError(f"non-finite number at {context}")
    if isinstance(value, dict):
        for key, child in value.items():
            require_finite(child, f"{context}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            require_finite(child, f"{context}[{index}]")


def strict_json_load(path: Path) -> dict[str, Any]:
    value = json.loads(
        path.read_text(encoding="utf-8-sig"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=reject_nonfinite,
    )
    if not isinstance(value, dict):
        raise ValueError(f"JSON root is not an object: {path}")
    require_finite(value)
    return value


def strict_json_string(value: Any, field: str) -> dict[str, Any]:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} is not a populated JSON string")
    parsed = json.loads(
        value, object_pairs_hook=reject_duplicate_keys, parse_constant=reject_nonfinite)
    if not isinstance(parsed, dict):
        raise ValueError(f"{field} JSON root is not an object")
    require_finite(parsed, field)
    return parsed


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"present": False}
    return {
        "present": True,
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def tree_snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    result: dict[str, dict[str, Any]] = {}
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        result[relative] = {
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }
    return result


def snapshot_hash(snapshot: dict[str, dict[str, Any]]) -> str:
    payload = json.dumps(snapshot, sort_keys=True, separators=(",", ":"), allow_nan=False)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest().upper()


def normalize_path(path: Path | str) -> str:
    return str(Path(path).resolve()).replace("/", "\\").casefold()


def integer(value: Any, field: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{field} is not an integer")
    return value


def number(value: Any, field: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value):
        raise ValueError(f"{field} is not a finite number")
    return float(value)


def bool_field(value: Any, field: str) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{field} is not a boolean")
    return value


def string_field(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} is not a populated string")
    return value


def nested_casefold_get(value: dict[str, Any], field: str) -> Any:
    target = field.casefold()
    for key, child in value.items():
        if key.casefold() == target:
            return child
    return None


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"screenshot is not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def validate_disc_json(value: dict[str, Any], mold: str, plastic: str, field: str) -> None:
    if nested_casefold_get(value, "DiscDefinitionId") != mold:
        raise ValueError(f"{field} mold is not {mold}")
    if nested_casefold_get(value, "PlasticId") != plastic:
        raise ValueError(f"{field} plastic is not {plastic}")
    instance = nested_casefold_get(value, "InstanceId")
    if instance in (None, "", "00000000-0000-0000-0000-000000000000"):
        raise ValueError(f"{field} stable instance identity is empty")


def validate_phase_report(
    report: dict[str, Any], phase: str, user_dir: Path, previous: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    missing = sorted(COMMON_PHASE_FIELDS - set(report))
    if missing:
        raise ValueError(f"{phase} report omitted fields: {missing}")
    if report.get("schema") != PHASE_SCHEMA or report.get("phase") != phase:
        raise ValueError(f"{phase} report schema/phase mismatch")
    if report.get("passed") is not True or report.get("failure_reason") != "":
        raise ValueError(f"{phase} report did not pass cleanly")
    if normalize_path(report.get("external_user_dir", "")) != normalize_path(user_dir):
        raise ValueError(f"{phase} report used a different external UserDir")
    if report.get("course_id") != "PineRidgeChampionship" or report.get("hole_number") != 1:
        raise ValueError(f"{phase} did not run authored Pine Ridge Hole 1")
    if not isinstance(report.get("blocker_summary"), str) or not report["blocker_summary"]:
        raise ValueError(f"{phase} blocker summary is empty")
    if report.get("release_blockers") != EXPECTED_BLOCKERS:
        raise ValueError(f"{phase} release blockers are not the exact ordered 14")

    decoded = {
        field: strict_json_string(report.get(field), f"{phase}.{field}")
        for field in ("character_json", "outfit_json", "settings_json",
                      "selected_disc_json", "round_json", "lie_json")
    }
    if phase != "Setup":
        setup = previous["Setup"]
        for field in ("character_json", "outfit_json", "settings_json"):
            if report[field] != setup[field]:
                raise ValueError(f"{phase} did not reconstruct exact Setup {field}")

    expected_score = {
        "Setup": (0, 0, False),
        "Drive": (1, 0, False),
        "Finish": (3, 0, True),
        "Verify": (3, 0, True),
    }[phase]
    score = (integer(report.get("strokes"), f"{phase}.strokes"),
             integer(report.get("penalty_strokes"), f"{phase}.penalty_strokes"),
             bool_field(report.get("hole_complete"), f"{phase}.hole_complete"))
    if score != expected_score:
        raise ValueError(f"{phase} score/completion is {score}, expected {expected_score}")

    if phase in {"Setup", "Drive"}:
        validate_disc_json(decoded["selected_disc_json"], "Apex", "Tour",
                           f"{phase}.selected_disc_json")
    else:
        validate_disc_json(decoded["selected_disc_json"], "Touch", "Base",
                           f"{phase}.selected_disc_json")
    if phase == "Drive" and report["selected_disc_json"] != previous["Setup"]["selected_disc_json"]:
        raise ValueError("Drive Apex/Tour stable instance identity differs from Setup")
    if phase == "Verify":
        finish = previous["Finish"]
        for field in ("selected_disc_json", "round_json", "lie_json"):
            if report[field] != finish[field]:
                raise ValueError(f"Verify did not restore exact Finish {field}")

    if phase == "Drive":
        if integer(report.get("release_count_delta"), "Drive.release_count_delta") != 1:
            raise ValueError("Drive did not commit exactly one authoritative release")
        if integer(report.get("flight_sample_count"), "Drive.flight_sample_count") <= 0:
            raise ValueError("Drive did not record actual flight samples")
        if report.get("replay_exercised") is not True or report.get(
                "throw_lab_replay_exercised") is not True or report.get("tracer_enabled") is not True:
            raise ValueError("Drive did not exercise flight replay, Throw Lab replay, and tracer")
        if report.get("camera_seen") is not True or report.get("natural_circle2_lie") is not True:
            raise ValueError("Drive did not observe camera and natural Circle 2 lie")
        if integer(report.get("throw_lab_count"), "Drive.throw_lab_count") != 1:
            raise ValueError("Drive did not record exactly one Throw Lab shot")
        if integer(report.get("audio_event_delta"), "Drive.audio_event_delta") < 1:
            raise ValueError("Drive did not route semantic audio")
        p95 = number(report.get("p95_frame_ms"), "Drive.p95_frame_ms")
        if not (0.0 < p95 <= 22.0) or report.get("meets_22ms_gate") is not True \
                or report.get("performance_budget_pass") is not True:
            raise ValueError("Drive failed the p95 performance budget")
        if report.get("rhi") != "D3D12" or not isinstance(report.get("rhi_detail"), str) \
                or not report.get("rhi_detail"):
            raise ValueError("Drive was not rendered through real D3D12")
        if integer(report.get("width"), "Drive.width") != 1920 or \
                integer(report.get("height"), "Drive.height") != 1080:
            raise ValueError("Drive was not rendered at 1920x1080")
        if integer(report.get("performance_sample_count"), "Drive.performance_sample_count") < 120:
            raise ValueError("Drive performance sample count is below 120")
        if integer(report.get("hitch_count"), "Drive.hitch_count") != 0:
            raise ValueError("Drive reported one or more hitches")
        memory = integer(report.get("memory_bytes"), "Drive.memory_bytes")
        if memory <= 0 or memory > 3758096384:
            raise ValueError("Drive memory budget failed")
        expected_60 = p95 <= (1000.0 / 60.0) + 1e-3
        if report.get("meets_60fps_diagnostic") is not expected_60:
            raise ValueError("Drive 60fps diagnostic is not computed honestly")
        if report.get("screenshot_requested") is not True:
            raise ValueError("Drive did not request its screenshot")
    elif phase == "Finish":
        if report.get("circle2_settled_to_circle1") is not True or report.get("caught_holeout") is not True:
            raise ValueError("Finish did not naturally settle to Circle 1 and complete a caught holeout")
        if integer(report.get("total_strokes"), "Finish.total_strokes") != 3 or \
                integer(report.get("completed_holes"), "Finish.completed_holes") != 1:
            raise ValueError("Finish did not complete exactly one hole in three strokes")
        if report.get("replay_exercised") is not True:
            raise ValueError("Finish did not exercise completed-hole replay")
        if integer(report.get("throw_lab_count"), "Finish.throw_lab_count") != 3:
            raise ValueError("Finish did not retain exactly all three Throw Lab shots")
        if integer(report.get("audio_event_delta"), "Finish.audio_event_delta") < 6:
            raise ValueError("Finish did not route the expected audio lifecycle")
        if report.get("screenshot_requested") is not True:
            raise ValueError("Finish did not request its screenshot")
    return decoded


def validate_performance(report: dict[str, Any], mode: str, drive: dict[str, Any]) -> None:
    if report.get("schema") != PERFORMANCE_SCHEMA:
        raise ValueError("Drive performance schema mismatch")
    if report.get("phase") != "Drive" or report.get("passed") is not True:
        raise ValueError("Drive performance phase/pass fields are invalid")
    if report.get("quality_profile") != "GameplayPerformance" or report.get(
            "runtime_profile") != "OmenGameplay1080pHighFoliageV1":
        raise ValueError("Drive performance quality/runtime profile drifted")
    sample_count = integer(report.get("sample_count"), "performance.sample_count")
    p95 = number(report.get("p95_frame_ms"), "performance.p95_frame_ms")
    if sample_count < 120 or not (0.0 < p95 <= 22.0):
        raise ValueError("Drive performance sample count/p95 budget failed")
    if abs(p95 - number(drive.get("p95_frame_ms"), "Drive.p95_frame_ms")) > 0.01:
        raise ValueError("Drive phase and performance report p95 differ")
    if report.get("performance_budget_pass") is not True:
        raise ValueError("performance report did not pass the budget")
    meets_60 = p95 <= (1000.0 / 60.0) + 1e-3
    if report.get("meets_60fps_diagnostic") is not meets_60:
        raise ValueError("performance 60fps diagnostic is not computed honestly")
    warmup = number(report.get("warmup_seconds"), "performance.warmup_seconds")
    if warmup < 10.0:
        raise ValueError("performance residency warmup was less than 10 seconds")
    if report.get("rhi") != "D3D12" or not isinstance(report.get("rhi_detail"), str) \
            or not report.get("rhi_detail"):
        raise ValueError(f"{mode} performance was not measured on real D3D12")
    if integer(report.get("width"), "performance.width") != 1920 or \
            integer(report.get("height"), "performance.height") != 1080:
        raise ValueError(f"{mode} performance was not 1920x1080")
    if integer(report.get("hitch_count"), "performance.hitch_count") != 0:
        raise ValueError(f"{mode} performance reported hitches")
    memory = integer(report.get("memory_bytes"), "performance.memory_bytes")
    if memory <= 0 or memory > 3758096384:
        raise ValueError(f"{mode} performance memory budget failed")


def validate_canonical(report: dict[str, Any], user_dir: Path, drive: dict[str, Any]) -> None:
    if report.get("schema") != CANONICAL_SCHEMA or report.get("result") != CANONICAL_RESULT:
        raise ValueError("canonical report schema/result mismatch")
    if report.get("bounded_technical_vertical_slice_complete") is not True or \
            report.get("release_ready") is not False or report.get("release_use_allowed") is not False:
        raise ValueError("canonical report overclaims or omits bounded technical completion")
    if normalize_path(report.get("external_user_dir", "")) != normalize_path(user_dir):
        raise ValueError("canonical report used a different external UserDir")
    if report.get("phase_pass") != {phase: True for phase in PHASES}:
        raise ValueError("canonical phase-pass map is not exact")
    p95 = number(report.get("p95_frame_ms"), "canonical.p95_frame_ms")
    if abs(p95 - number(drive.get("p95_frame_ms"), "Drive.p95_frame_ms")) > 0.01:
        raise ValueError("canonical and Drive p95 differ")
    if report.get("performance_budget_pass") is not True:
        raise ValueError("canonical performance budget did not pass")
    expected_60 = p95 <= (1000.0 / 60.0) + 1e-3
    if report.get("meets_60fps_diagnostic") is not expected_60:
        raise ValueError("canonical 60fps diagnostic is not computed honestly")
    for field in ("technical_substitution", "art_blocker", "calibration_blocker",
                  "audio_blocker", "footing_blocker"):
        string_field(report.get(field), f"canonical.{field}")
    if report.get("release_blockers") != EXPECTED_BLOCKERS:
        raise ValueError("canonical release blockers are not the exact ordered 14")


def validate_screenshot(path: Path, mode: str) -> dict[str, Any]:
    if not path.is_file() or path.stat().st_size < 1024:
        raise FileNotFoundError(f"screenshot missing or undersized: {path}")
    width, height = png_dimensions(path)
    if mode == "packaged" and (width, height) != (1920, 1080):
        raise ValueError(f"packaged screenshot is {width}x{height}, not 1920x1080")
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": sha256(path),
            "width": width, "height": height}


def terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    try:
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            process.terminate()
    finally:
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()


def validate_extra_args(args: list[str]) -> None:
    forbidden = (
        "-userdir", "-nullrhi", "-session15verticalslicesmoketest",
        "-session15verticalslicephase", "-abslog", "-resx", "-resy",
        "-forceres", "-d3d", "-vulkan", "-opengl",
    )
    for argument in args:
        folded = argument.casefold()
        if any(folded.startswith(prefix) for prefix in forbidden):
            raise ValueError(f"extra argument may not override acceptance authority: {argument}")


def canonical_guid(value: str | None) -> str:
    parsed = uuid.uuid4() if value is None else uuid.UUID(value)
    if value is not None and str(parsed) != value.casefold():
        raise ValueError("--run-id must be a canonical hyphenated GUID")
    return str(parsed)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Session 15 Setup/Drive/Finish/Verify in one fresh external GUID UserDir.")
    parser.add_argument("--mode", choices=("editor", "packaged"), required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--package-root", type=Path,
                        help="Required for packaged mode; the complete fresh staged package root.")
    parser.add_argument("--run-id")
    parser.add_argument("--timeout", type=int, default=180,
                        help="Per-phase timeout in seconds (30-900).")
    parser.add_argument("--extra-arg", action="append", default=[])
    options = parser.parse_args()

    executable = options.executable.resolve()
    project = options.project.resolve()
    if not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    if options.mode == "editor" and not project.is_file():
        parser.error(f"project does not exist: {project}")
    if not 30 <= options.timeout <= 900:
        parser.error("--timeout must be between 30 and 900 seconds")
    validate_extra_args(options.extra_arg)
    run_id = canonical_guid(options.run_id)
    user_dir = (RUN_PARENT / run_id).resolve()
    backup_dir = (BACKUP_PARENT / f"Session15_{options.mode}_{run_id}").resolve()
    if user_dir.parent != RUN_PARENT or user_dir.name != run_id:
        parser.error("resolved UserDir escaped the exact Session 15 run parent")
    if backup_dir.parent != BACKUP_PARENT or backup_dir.name != f"Session15_{options.mode}_{run_id}":
        parser.error("resolved backup path escaped the exact backup parent")
    if user_dir.exists() or backup_dir.exists():
        parser.error("run-id is not fresh in both run and backup parents")

    package_root: Path | None = None
    package_before: dict[str, dict[str, Any]] | None = None
    if options.mode == "packaged":
        if options.package_root is None:
            parser.error("--package-root is required for packaged mode")
        package_root = options.package_root.resolve()
        if not package_root.is_dir() or package_root not in executable.parents:
            parser.error("packaged executable must be inside --package-root")

    protected_before = file_record(PROTECTED_PROFILE)
    if protected_before != {"present": True, "bytes": PROTECTED_BYTES,
                            "sha256": PROTECTED_SHA256}:
        parser.error("canonical project profile is not the protected A999 binding")

    print("Hashing project Content and SaveGames before acceptance...", flush=True)
    content_before = tree_snapshot(CONTENT_ROOT)
    saves_before = tree_snapshot(SAVEGAME_ROOT)
    if package_root is not None:
        print("Hashing staged package before acceptance...", flush=True)
        package_before = tree_snapshot(package_root)

    report_root = user_dir / "Saved/Session15Reports"
    owned_marker = user_dir / ".dg_session15_owned"
    stable_root = PROJECT_REPORT_ROOT / f"Acceptance_{options.mode}_{run_id}"
    started = datetime.now(timezone.utc).isoformat()
    phase_records: list[dict[str, Any]] = []
    phase_reports: dict[str, dict[str, Any]] = {}
    decoded_reports: dict[str, dict[str, Any]] = {}
    screenshots: list[dict[str, Any]] = []
    errors: list[str] = []
    validated = False
    archived = False

    try:
        RUN_PARENT.mkdir(parents=True, exist_ok=True)
        user_dir.mkdir()
        owned_marker.write_text(run_id + "\n", encoding="ascii")
        for phase in PHASES:
            phase_dir = report_root / phase
            phase_dir.mkdir(parents=True, exist_ok=True)
            log_path = phase_dir / f"Session15_{phase}.log"
            console_path = phase_dir / f"LauncherConsole_{phase}.log"
            command = [str(executable)]
            if options.mode == "editor":
                command.extend([str(project), "-game"])
            command.extend([
                "-Session15VerticalSliceSmokeTest",
                f"-Session15VerticalSlicePhase={phase}",
                f"-UserDir={user_dir}",
                "-Course=PineRidge", "-Hole=1", "-SkipHoleIntro",
                "-d3d12", "-ResX=1920", "-ResY=1080", "-ForceRes",
                "-RenderOffscreen",
                "-unattended", "-nop4", "-nosplash", "-UTF8Output",
                "-stdout", "-FullStdOutLogOutput", f"-abslog={log_path}",
            ])
            command.extend(options.extra_arg)
            if sum(arg.casefold().startswith("-userdir=") for arg in command) != 1:
                raise RuntimeError("command did not contain exactly one UserDir")
            print(f"Running Session 15 {options.mode} phase: {phase}", flush=True)
            process = subprocess.Popen(
                command, cwd=executable.parent, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
                creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0),
            )
            timed_out = False
            try:
                console, _ = process.communicate(timeout=options.timeout)
            except subprocess.TimeoutExpired as exc:
                timed_out = True
                partial = exc.output or ""
                if isinstance(partial, bytes):
                    partial = partial.decode("utf-8", errors="replace")
                terminate_process(process)
                tail, _ = process.communicate()
                console = partial + (tail or "")
            console_path.write_text(console, encoding="utf-8")
            log_text = log_path.read_text(encoding="utf-8", errors="replace") \
                if log_path.is_file() else ""
            # -stdout mirrors the absolute log into the captured console stream.
            # Count markers in one canonical channel so a single runtime emission
            # is not mistaken for two launcher emissions.
            evidence_text = log_text if log_text else console
            record = {
                "phase": phase,
                "return_code": process.returncode,
                "timed_out": timed_out,
                "command": command,
                "log": file_record(log_path),
                "console": file_record(console_path),
            }
            phase_records.append(record)
            if timed_out:
                raise TimeoutError(f"{phase} timed out after {options.timeout}s")
            if process.returncode != 0:
                raise RuntimeError(f"{phase} returned {process.returncode}")
            if evidence_text.count(PASS_MARKERS[phase]) != 1:
                raise ValueError(f"{phase} did not emit exactly one pass marker")
            fail_prefix = f"DG_SESSION15_VERTICAL_SLICE_{phase.upper()}: FAIL"
            if fail_prefix in evidence_text:
                raise ValueError(f"{phase} emitted its fail marker")
            if any(token in evidence_text.casefold() for token in FATAL_LOG_TOKENS):
                raise ValueError(f"{phase} log contains fatal/error diagnostics")
            if phase == "Verify" and evidence_text.count(CANONICAL_MARKER) != 1:
                raise ValueError("Verify did not emit exactly one canonical pass marker")

            phase_path = phase_dir / "PhaseReport.json"
            if not phase_path.is_file():
                raise FileNotFoundError(f"phase report is missing: {phase_path}")
            phase_report = strict_json_load(phase_path)
            decoded = validate_phase_report(phase_report, phase, user_dir, phase_reports)
            phase_reports[phase] = phase_report
            decoded_reports[phase] = decoded
            record["phase_report"] = file_record(phase_path)
            if phase == "Drive":
                screenshots.append(validate_screenshot(
                    phase_dir / "Session15_Drive.png", options.mode))
            elif phase == "Finish":
                screenshots.append(validate_screenshot(
                    phase_dir / "Session15_HoleComplete.png", options.mode))

        performance_path = report_root / "Drive/PerformanceReport.json"
        canonical_path = report_root / "Session15VerticalSliceReport.json"
        if not performance_path.is_file() or not canonical_path.is_file():
            raise FileNotFoundError("performance or canonical report is missing")
        performance = strict_json_load(performance_path)
        canonical = strict_json_load(canonical_path)
        validate_performance(performance, options.mode, phase_reports["Drive"])
        validate_canonical(canonical, user_dir, phase_reports["Drive"])

        print("Hashing project boundaries after acceptance...", flush=True)
        content_after = tree_snapshot(CONTENT_ROOT)
        saves_after = tree_snapshot(SAVEGAME_ROOT)
        if content_after != content_before:
            raise RuntimeError("project Content changed during acceptance")
        if saves_after != saves_before:
            raise RuntimeError("project SaveGames changed during acceptance")
        protected_after = file_record(PROTECTED_PROFILE)
        if protected_after != protected_before:
            raise RuntimeError("protected canonical profile changed during acceptance")

        package_after: dict[str, dict[str, Any]] | None = None
        if package_root is not None and package_before is not None:
            print("Hashing staged package after acceptance...", flush=True)
            package_after = tree_snapshot(package_root)
            if package_after != package_before:
                raise RuntimeError("staged package changed during acceptance")

        if stable_root.exists():
            raise RuntimeError(f"stable evidence root already exists: {stable_root}")
        stable_root.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(report_root, stable_root / "RuntimeReports")
        boundary = {
            "schema": "DiscGolfTour.Session15BoundarySnapshot.v1",
            "content": {"file_count": len(content_before),
                        "manifest_sha256_before": snapshot_hash(content_before),
                        "manifest_sha256_after": snapshot_hash(content_after),
                        "unchanged": True,
                        "files_before": content_before,
                        "files_after": content_after},
            "project_savegames": {"file_count": len(saves_before),
                                  "manifest_sha256_before": snapshot_hash(saves_before),
                                  "manifest_sha256_after": snapshot_hash(saves_after),
                                  "unchanged": True,
                                  "files_before": saves_before,
                                  "files_after": saves_after},
            "protected_profile_before": protected_before,
            "protected_profile_after": protected_after,
        }
        (stable_root / "BoundarySnapshot.json").write_text(
            json.dumps(boundary, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        package_manifest = None
        if package_root is not None and package_before is not None and package_after is not None:
            package_manifest = {
                "schema": "DiscGolfTour.Session15PackageManifest.v1",
                "package_root": str(package_root),
                "executable": str(executable),
                "file_count": len(package_before),
                "manifest_sha256_before": snapshot_hash(package_before),
                "manifest_sha256_after": snapshot_hash(package_after),
                "unchanged": True,
                "files": package_before,
            }
            (stable_root / "PackageManifest.json").write_text(
                json.dumps(package_manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        aggregate = {
            "schema": "DiscGolfTour.Session15Hole1AcceptanceLauncher.v1",
            "result": CANONICAL_RESULT,
            "mode": options.mode,
            "run_id": run_id,
            "started_utc": started,
            "completed_utc": datetime.now(timezone.utc).isoformat(),
            "external_user_dir": str(user_dir),
            "backup_dir": str(backup_dir),
            "one_absolute_user_dir": True,
            "phase_order": list(PHASES),
            "phases": phase_records,
            "canonical_report": file_record(canonical_path),
            "performance_report": file_record(performance_path),
            "screenshots": screenshots,
            "project_boundaries_unchanged": True,
            "package_unchanged": package_manifest is not None if options.mode == "packaged" else None,
            "protected_profile_sha256": PROTECTED_SHA256,
            "release_ready": False,
            "release_blockers": EXPECTED_BLOCKERS,
            "external_user_dir_moved_recoverably": False,
            "errors": [],
        }
        (stable_root / "AcceptanceManifest.json").write_text(
            json.dumps(aggregate, indent=2, allow_nan=False) + "\n", encoding="utf-8")

        marker_ok = (owned_marker.is_file() and
                     owned_marker.read_text(encoding="ascii").strip() == run_id and
                     user_dir.parent == RUN_PARENT and user_dir.name == run_id)
        if not marker_ok:
            raise RuntimeError("owned run marker/path changed; refusing recoverable move")
        BACKUP_PARENT.mkdir(parents=True, exist_ok=True)
        shutil.move(str(user_dir), str(backup_dir))
        archived = backup_dir.is_dir() and not user_dir.exists()
        if not archived:
            raise RuntimeError("recoverable backup move did not complete")
        aggregate["external_user_dir_moved_recoverably"] = True
        aggregate["archived_utc"] = datetime.now(timezone.utc).isoformat()
        (stable_root / "AcceptanceManifest.json").write_text(
            json.dumps(aggregate, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        validated = True
        print(f"{CANONICAL_RESULT}: {stable_root}", flush=True)
        print(f"Run root moved recoverably to: {backup_dir}", flush=True)
    except Exception as exc:
        errors.append(f"{type(exc).__name__}: {exc}")
        print(f"Session 15 acceptance FAILED: {errors[-1]}", file=sys.stderr)
        if user_dir.exists():
            print(f"Failed run retained for diagnosis: {user_dir}", file=sys.stderr)
    return 0 if validated and archived and not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
