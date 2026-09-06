#!/usr/bin/env python3
"""Run the fail-closed current-v006 fixed-MetaHuman Baseline proof.

The retained assembled MetaHuman is one fixed curated preset with no runtime
DG-body-profile mapping.  Baseline is therefore the only acceptance-eligible
source profile.  Four other source profiles may be run explicitly as
non-claiming compatibility observations, but they can never become acceptance
passes.  Every launched profile owns a fresh external ``-UserDir`` and Editor
process.  The runner never packages, cooks, saves content, loads an existing
player save, or asserts human/release approval.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any, Sequence
import uuid


PROJECT_ROOT = Path(__file__).absolute().parents[1]
DEFAULT_PROJECT = PROJECT_ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
DEFAULT_OUTPUT_ROOT = (
    PROJECT_ROOT.parent
    / f"{PROJECT_ROOT.name}_TestRuns"
    / "AnimationFluidity"
    / f"FixedMetaHumanBaseline-v006-{uuid.uuid4()}"
)
PROFILES = ("ShortCompact", "Baseline", "TallLongArms", "SliderMin", "SliderMax")
ACCEPTANCE_PROFILE = "Baseline"
UNSUPPORTED_PROBE_PROFILES = (
    "ShortCompact", "TallLongArms", "SliderMin", "SliderMax"
)
UNSUPPORTED_REASON_CODE = "FIXED_CURATED_PRESET_HAS_NO_DG_BODY_PROFILE_MAPPING"
EXPECTED_SCHEMA = "DiscGolfTour.Session19MetaHumanProductionMotionVisualEvidence.v6"
REPORT_SCHEMA = "DiscGolfTour.Session19V006FixedMetaHumanPresetAcceptanceReport.v1"
REPORT_NAME = "Session19V006FixedMetaHumanPresetAcceptanceReport.json"
PASS_STATUS = "PASS_FIXED_PRESET_BASELINE_ACCEPTANCE"
FAIL_STATUS = "FAIL_FIXED_PRESET_BASELINE_ACCEPTANCE"
PROBE_ERROR_STATUS = "ERROR_FIXED_PRESET_COMPATIBILITY_PROBE_COLLECTION"
METAHUMAN_BACKEND_PROFILE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default"
)
METAHUMAN_VISUAL_ACTOR_CLASS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default_C"
)
UNIT_SCALE_FIELDS = (
    "metahuman_visual_root_world_scale",
    "metahuman_body_world_scale",
    "metahuman_head_world_scale",
    "metahuman_outfit_world_scale",
)
UNIT_SCALE_TOLERANCE = 0.001
EXPECTED_MONTAGE = (
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
    "AM_DG_RHBH_Drive_Procedural_v006.AM_DG_RHBH_Drive_Procedural_v006"
)
EXPECTED_CHECKPOINTS = ("ReachBack", "Plant", "Release", "FollowThrough", "Recovery")
CAPTURE_RELATIVE = Path(
    "Saved/CharacterFramework/Screenshots/Session19_MetaHumanProductionMotion"
)
MANIFEST_NAME = "Session19_MetaHumanProductionMotion_CaptureManifest.json"
PASS_PREFIX = "DG_SESSION3_VISUAL_CAPTURE: PASS mode=metahuman_production"
FAIL_PREFIX = "DG_SESSION3_VISUAL_CAPTURE: FAIL"
UNREAL_ERROR_PATTERNS = (
    re.compile(r"\bLog[^:\r\n]+:\s+Error:", re.IGNORECASE),
    re.compile(r"\bFatal error:", re.IGNORECASE),
    re.compile(r"\bUnhandled Exception\b", re.IGNORECASE),
    re.compile(r"\bAssertion failed:", re.IGNORECASE),
    re.compile(r"\bEnsure condition failed:", re.IGNORECASE),
)
RESERVED_EXTRA_PREFIXES = (
    "-userdir",
    "-abslog",
    "-session3visualcapture",
    "-session19metahumanproductionvisualcapture",
    "-session19expectedproductionmotionrevision",
    "-session4profile",
    "-resx",
    "-resy",
    "-forceres",
    "-d3d",
    "-nullrhi",
    "-noloadexistingsave",
    "-dgnoprofilewrites",
    "-dgdevelopertoolnosave",
)


def _sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _identity(path: Path) -> dict[str, Any]:
    return {
        "path": str(path),
        "exists": path.is_file(),
        "size_bytes": path.stat().st_size if path.is_file() else None,
        "sha256": _sha256(path),
    }


def _path_key(path: Path) -> str:
    return os.path.normcase(str(path.resolve(strict=False)))


def _unreal_error_lines(log_text: str) -> list[str]:
    return [
        line.strip()
        for line in log_text.splitlines()
        if any(pattern.search(line) for pattern in UNREAL_ERROR_PATTERNS)
    ]


def _bounded_timeout(value: str) -> float:
    try:
        timeout = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("timeout must be a number") from exc
    if not 60.0 <= timeout <= 1800.0:
        raise argparse.ArgumentTypeError("timeout must be between 60 and 1800 seconds")
    return timeout


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run strict current-v006 fixed-MetaHuman Baseline acceptance and, "
            "when requested, four non-claiming compatibility probes."
        )
    )
    parser.add_argument("--engine", default=os.environ.get("UE_EDITOR_CMD", str(DEFAULT_ENGINE)))
    parser.add_argument("--project", default=str(DEFAULT_PROJECT))
    parser.add_argument("--output-root", default=str(DEFAULT_OUTPUT_ROOT))
    parser.add_argument("--timeout-seconds", type=_bounded_timeout, default=600.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    parser.add_argument(
        "--include-unsupported-probes",
        action="store_true",
        help=(
            "Also collect non-acceptance observations for ShortCompact, "
            "TallLongArms, SliderMin, and SliderMax."
        ),
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def _validate_extra_args(extra_args: Sequence[str]) -> None:
    for extra in extra_args:
        folded = extra.strip().lower()
        if any(folded.startswith(prefix) for prefix in RESERVED_EXTRA_PREFIXES):
            raise ValueError(f"extra argument overrides an owned proof argument: {extra}")


def _validate_configuration(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    engine = Path(args.engine).expanduser().absolute()
    if engine.is_dir():
        engine = engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    project = Path(args.project).expanduser().absolute()
    output_root = Path(args.output_root).expanduser().absolute()
    if not engine.is_file():
        raise FileNotFoundError(f"Unreal command executable was not found: {engine}")
    if not project.is_file() or project.suffix.lower() != ".uproject":
        raise FileNotFoundError(f"Unreal project was not found: {project}")
    if output_root.exists():
        raise FileExistsError(f"output-root must be fresh: {output_root}")
    if output_root == project.parent or project.parent in output_root.parents:
        raise ValueError("output-root must be outside the project")
    _validate_extra_args(args.extra_arg)
    return engine, project, output_root


def _command(
    engine: Path,
    project: Path,
    profile: str,
    user_dir: Path,
    log_path: Path,
    extra_args: Sequence[str],
) -> list[str]:
    return [
        str(engine),
        str(project),
        "-game",
        "-Course=PineRidge",
        "-Hole=1",
        "-Session3VisualCapture",
        "-Session19MetaHumanProductionVisualCapture",
        "-Session19ExpectedProductionMotionRevision=v006",
        f"-Session4Profile={profile}",
        "-windowed",
        "-ResX=1920",
        "-ResY=1080",
        "-ForceRes",
        "-d3d12",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-NoSound",
        "-SkipHoleIntro",
        "-NoLoadExistingSave",
        "-DGNoProfileWrites",
        "-DGDeveloperToolNoSave",
        "-NoMetaHumanAccountPortalLoginFallback",
        "-UTF8Output",
        "-stdout",
        "-FullStdOutLogOutput",
        f"-UserDir={user_dir}",
        f"-abslog={log_path}",
        *extra_args,
    ]


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
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def _expect_equal(issues: list[str], label: str, actual: Any, expected: Any) -> None:
    if actual != expected:
        issues.append(f"{label} expected {expected!r}, got {actual!r}")


def _validate_unit_scale_vector(
    issues: list[str], label: str, value: Any
) -> None:
    if not isinstance(value, dict) or set(value) != {"x", "y", "z"}:
        issues.append(f"{label} must be an exact x/y/z object")
        return
    for axis in ("x", "y", "z"):
        component = value[axis]
        if (
            isinstance(component, bool)
            or not isinstance(component, (int, float))
            or not math.isfinite(float(component))
        ):
            issues.append(f"{label}.{axis} must be finite")
        elif abs(float(component) - 1.0) > UNIT_SCALE_TOLERANCE:
            issues.append(
                f"{label}.{axis} must be within {UNIT_SCALE_TOLERANCE} of 1.0, "
                f"got {component!r}"
            )


def _validate_no_write_contract(payload: Any, issues: list[str]) -> None:
    writes = payload.get("writes") if isinstance(payload, dict) else None
    if not isinstance(writes, dict):
        issues.append("writes is not an object")
        return
    expected_writes = {
        "png_and_manifest_only": True,
        "trajectory_json_csv_written_by_capture_lane": False,
        "existing_profile_loaded": False,
        "profile_writes_allowed": False,
        "practice_snapshot_writes_allowed": False,
        "uasset_writes": [],
        "level_save_calls": [],
        "content_save_calls": [],
    }
    for key, expected in expected_writes.items():
        _expect_equal(issues, f"writes.{key}", writes.get(key), expected)


def _validate_manifest(
    payload: Any,
    manifest_path: Path,
    *,
    check_files: bool,
) -> tuple[list[str], list[dict[str, Any]]]:
    issues: list[str] = []
    images: list[dict[str, Any]] = []
    if not isinstance(payload, dict):
        return ["manifest root is not an object"], images
    _expect_equal(issues, "schema", payload.get("schema"), EXPECTED_SCHEMA)
    _expect_equal(issues, "status", payload.get("status"), "PASS")
    _expect_equal(
        issues,
        "scope",
        payload.get("scope"),
        "SESSION_19_METAHUMAN_ACTIVE_PRODUCTION_MOTION_VISUAL_EVIDENCE_ONLY",
    )
    if check_files:
        _expect_equal(
            issues,
            "output_directory",
            _path_key(Path(str(payload.get("output_directory", "")))),
            _path_key(manifest_path.parent),
        )
    _expect_equal(issues, "capture_count", payload.get("capture_count"), 5)
    _expect_equal(issues, "expected_capture_count", payload.get("expected_capture_count"), 5)
    _expect_equal(issues, "error", payload.get("error"), "")

    proof = payload.get("proof_policy")
    if not isinstance(proof, dict):
        issues.append("proof_policy is not an object")
    else:
        _expect_equal(issues, "fixed_frame_rate_hz", proof.get("fixed_frame_rate_hz"), 60)
        _expect_equal(
            issues,
            "montage_position_source",
            proof.get("montage_position_source"),
            "EXACT_COMMITTED_FANIMMONTAGEINSTANCE_ID_POSITION_AND_BLEND_WEIGHTS",
        )
        _expect_equal(
            issues,
            "montage_full_weight_gate_applies_to_all_checkpoints",
            proof.get("montage_full_weight_gate_applies_to_all_checkpoints"),
            True,
        )

    captures = payload.get("captures")
    if not isinstance(captures, list) or len(captures) != 5:
        issues.append("captures must contain exactly five rows")
    else:
        filenames: list[str] = []
        image_hashes: list[str] = []
        for index, (capture, checkpoint) in enumerate(zip(captures, EXPECTED_CHECKPOINTS)):
            label = f"captures[{index}]"
            if not isinstance(capture, dict):
                issues.append(f"{label} is not an object")
                continue
            _expect_equal(issues, f"{label}.capture_index", capture.get("capture_index"), index)
            _expect_equal(issues, f"{label}.checkpoint_name", capture.get("checkpoint_name"), checkpoint)
            _expect_equal(
                issues,
                f"{label}.montage_instance_montage_path",
                capture.get("montage_instance_montage_path"),
                EXPECTED_MONTAGE,
            )
            for key in (
                "checkpoint_timing_valid",
                "montage_instance_matches_committed_throw",
                "montage_full_weight_gate_valid",
                "retarget_pose_telemetry_valid",
                "target_pose_spatial_gate_valid",
                "motion_curves_finite_and_normalized",
                "checkpoint_curve_gate_valid",
                "bone_lengths_invariant",
                "subject_readable_scale",
                "metahuman_proof_telemetry_valid",
            ):
                _expect_equal(issues, f"{label}.{key}", capture.get(key), True)
            if capture.get("metahuman_visual_root_uses_absolute_scale") is not True:
                issues.append(
                    f"{label}.metahuman_visual_root_uses_absolute_scale "
                    "must be boolean true"
                )
            for scale_field in UNIT_SCALE_FIELDS:
                _validate_unit_scale_vector(
                    issues, f"{label}.{scale_field}", capture.get(scale_field)
                )
            _expect_equal(issues, f"{label}.width", capture.get("width"), 1920)
            _expect_equal(issues, f"{label}.height", capture.get("height"), 1080)
            filename = capture.get("filename")
            if (
                not isinstance(filename, str)
                or Path(filename).name != filename
                or Path(filename).suffix.casefold() != ".png"
            ):
                issues.append(f"{label}.filename is not a PNG basename")
                continue
            filenames.append(filename)
            image_path = manifest_path.parent / filename
            if check_files:
                identity = _identity(image_path)
                if not identity["exists"] or not identity["size_bytes"]:
                    issues.append(f"{label} PNG is missing or empty")
                _expect_equal(
                    issues,
                    f"{label}.bytes",
                    capture.get("bytes"),
                    identity["size_bytes"],
                )
                if identity["sha256"]:
                    image_hashes.append(identity["sha256"])
                if _path_key(Path(str(capture.get("path", "")))) != _path_key(image_path):
                    issues.append(f"{label}.path does not resolve to its manifest-owned PNG")
                images.append(identity)
        if len(set(filenames)) != 5:
            issues.append("captures must name five unique PNG basenames")
        if check_files and len(set(image_hashes)) != 5:
            issues.append("captures must contain five byte-distinct PNG files")

    live = payload.get("live_throw")
    if not isinstance(live, dict):
        issues.append("live_throw is not an object")
    else:
        expected_live = {
            "montage": EXPECTED_MONTAGE,
            "expected_production_motion_revision": "v006",
            "verified_metahuman_gameplay_presentation": True,
            "release_callback_count": 1,
            "stroke_delta": 1,
            "follow_through_reached": True,
            "recovered": True,
            "recovery_reason": "ThrowFinished",
            "authoritative_flight_validation_passed": True,
            "trajectory_export_deferral_requested": True,
            "trajectory_export_deferral_discarded": True,
        }
        for key, expected in expected_live.items():
            _expect_equal(issues, f"live_throw.{key}", live.get(key), expected)

    _validate_no_write_contract(payload, issues)
    return issues, images


def _validate_probe_failure_manifest(
    payload: Any,
    manifest_path: Path,
    *,
    check_files: bool,
) -> list[str]:
    """Validate a bounded failure artifact without treating it as acceptance."""
    issues: list[str] = []
    if not isinstance(payload, dict):
        return ["manifest root is not an object"]
    _expect_equal(issues, "schema", payload.get("schema"), EXPECTED_SCHEMA)
    _expect_equal(issues, "status", payload.get("status"), "FAIL")
    _expect_equal(
        issues,
        "scope",
        payload.get("scope"),
        "SESSION_19_METAHUMAN_ACTIVE_PRODUCTION_MOTION_VISUAL_EVIDENCE_ONLY",
    )
    if check_files:
        _expect_equal(
            issues,
            "output_directory",
            _path_key(Path(str(payload.get("output_directory", "")))),
            _path_key(manifest_path.parent),
        )
    _expect_equal(issues, "expected_capture_count", payload.get("expected_capture_count"), 5)
    error = payload.get("error")
    if not isinstance(error, str) or not error.strip():
        issues.append("error must be a non-empty string")
    capture_count = payload.get("capture_count")
    if (
        isinstance(capture_count, bool)
        or not isinstance(capture_count, int)
        or not 0 <= capture_count <= 5
    ):
        issues.append("capture_count must be an integer from 0 through 5")
    captures = payload.get("captures")
    if not isinstance(captures, list):
        issues.append("captures is not an array")
    elif isinstance(capture_count, int) and not isinstance(capture_count, bool):
        _expect_equal(issues, "captures length", len(captures), capture_count)
    _validate_no_write_contract(payload, issues)
    return issues


def _probe_failure_manifest_issues_for_payload(
    payload: Any,
    manifest_path: Path,
    *,
    check_files: bool,
) -> list[str]:
    """Return failed-probe diagnostics only for a manifest claiming FAIL."""
    if not isinstance(payload, dict) or payload.get("status") != "FAIL":
        return []
    return _validate_probe_failure_manifest(
        payload, manifest_path, check_files=check_files
    )


def _run_profile(
    engine: Path,
    project: Path,
    output_root: Path,
    profile: str,
    timeout_seconds: float,
    extra_args: Sequence[str],
) -> dict[str, Any]:
    user_dir = output_root / "UserDirs" / profile
    log_path = output_root / "Logs" / f"MetaHumanProfile_{profile}.log"
    user_dir.mkdir(parents=True, exist_ok=False)
    command = _command(engine, project, profile, user_dir, log_path, extra_args)
    started = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=project.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    timed_out = False
    console = ""
    try:
        console, _ = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        _terminate(process)
        tail, _ = process.communicate()
        console = partial + (tail or "")

    log_text = log_path.read_text(encoding="utf-8-sig", errors="replace") if log_path.is_file() else ""
    manifest_path = user_dir / CAPTURE_RELATIVE / MANIFEST_NAME
    payload: Any = None
    parse_error = ""
    if manifest_path.is_file():
        try:
            payload = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError) as exc:
            parse_error = f"{type(exc).__name__}: {exc}"
    manifest_issues, images = _validate_manifest(
        payload, manifest_path, check_files=True
    ) if payload is not None else (["manifest missing or invalid"], [])
    # A PASS manifest is acceptance evidence, not a failed-probe envelope.
    # Only attach the bounded-failure diagnostics when the payload actually
    # claims FAIL; otherwise the report can misleadingly make a clean
    # Baseline acceptance row look internally contradictory.
    probe_failure_manifest_issues = _probe_failure_manifest_issues_for_payload(
        payload, manifest_path, check_files=True
    )
    override = f"SESSION 4 PROFILE OVERRIDE: {profile} (transient, save slot unchanged)."
    pass_lines = [line for line in log_text.splitlines() if PASS_PREFIX in line]
    fail_lines = [line for line in log_text.splitlines() if FAIL_PREFIX in line]
    unreal_error_lines = _unreal_error_lines(log_text)
    unexpected_unreal_error_lines = [
        line for line in unreal_error_lines if FAIL_PREFIX not in line
    ]
    manifest_error = payload.get("error") if isinstance(payload, dict) else None
    fail_line_matches_manifest_error = (
        isinstance(manifest_error, str)
        and bool(manifest_error)
        and len(fail_lines) == 1
        and manifest_error in fail_lines[0]
    )
    validations = {
        "process_completed_before_timeout": not timed_out,
        "process_return_code_zero": process.returncode == 0,
        "separate_log_created": log_path.is_file(),
        "profile_override_reported_once": log_text.count(override) == 1,
        "visual_pass_reported_once": len(pass_lines) == 1,
        "visual_fail_not_reported": not fail_lines,
        "unreal_error_markers_absent": not unreal_error_lines,
        "schema_v6_manifest_passed": not manifest_issues,
    }
    failed = [name for name, passed in validations.items() if not passed]
    return {
        "profile": profile,
        "status": "PASS" if not failed else "FAIL",
        "command": command,
        "duration_seconds": round(time.monotonic() - started, 3),
        "timed_out": timed_out,
        "return_code": process.returncode,
        "user_dir": str(user_dir),
        "log": _identity(log_path),
        "manifest": _identity(manifest_path),
        "images": images,
        "validations": validations,
        "failed_validations": failed,
        "manifest_issues": manifest_issues,
        "manifest_payload_status": (
            payload.get("status") if isinstance(payload, dict) else None
        ),
        "manifest_error": manifest_error,
        "manifest_parse_error": parse_error,
        "pass_banner_count": len(pass_lines),
        "fail_banner_count": len(fail_lines),
        "probe_failure_manifest_issues": probe_failure_manifest_issues,
        "fail_line_matches_manifest_error": fail_line_matches_manifest_error,
        "failure_lines": fail_lines[-5:],
        "unreal_error_lines": unreal_error_lines[-10:],
        "unexpected_unreal_error_lines": unexpected_unreal_error_lines[-10:],
        "console_tail": console[-4000:],
    }


def _profiles_to_run(include_unsupported_probes: bool) -> tuple[str, ...]:
    if include_unsupported_probes:
        return (ACCEPTANCE_PROFILE, *UNSUPPORTED_PROBE_PROFILES)
    return (ACCEPTANCE_PROFILE,)


def _unrequested_probe_row(profile: str) -> dict[str, Any]:
    return {
        "dg_source_profile": profile,
        "role": "UNSUPPORTED_COMPATIBILITY_PROBE",
        "acceptance_eligible": False,
        "support_status": "UNSUPPORTED_BY_FIXED_PRESET_CONTRACT",
        "unsupported_reason_code": UNSUPPORTED_REASON_CODE,
        "observation_status": "NOT_REQUESTED",
        "strict_visual_contract_status": "NOT_RUN",
        "collection_issues": [],
        "evidence": None,
    }


def _classify_probe_result(
    profile: str, result: dict[str, Any]
) -> dict[str, Any]:
    validations = result.get("validations")
    collection_issues: list[str] = []
    for key in (
        "process_completed_before_timeout",
        "process_return_code_zero",
        "separate_log_created",
        "profile_override_reported_once",
    ):
        if not isinstance(validations, dict) or validations.get(key) is not True:
            collection_issues.append(key)
    if result.get("production_profile_unchanged") is not True:
        collection_issues.append("production_profile_unchanged")

    strict_status = result.get("status")
    pass_count = result.get("pass_banner_count")
    fail_count = result.get("fail_banner_count")
    if pass_count == 1 and fail_count == 0:
        if strict_status != "PASS":
            collection_issues.append("pass_banner_without_strict_schema_v6_pass")
        observation_status = "OBSERVED_STRICT_PASS_OUTSIDE_SUPPORT_CONTRACT"
    elif pass_count == 0 and fail_count == 1:
        if strict_status != "FAIL":
            collection_issues.append("fail_banner_with_strict_pass")
        if result.get("manifest_payload_status") != "FAIL":
            collection_issues.append("failure_manifest_status")
        if result.get("probe_failure_manifest_issues"):
            collection_issues.append("failure_manifest_envelope")
        if result.get("fail_line_matches_manifest_error") is not True:
            collection_issues.append("failure_banner_manifest_binding")
        if result.get("unexpected_unreal_error_lines"):
            collection_issues.append("unexpected_unreal_error_markers")
        observation_status = "OBSERVED_STRICT_FAIL_OUTSIDE_SUPPORT_CONTRACT"
    else:
        collection_issues.append("exactly_one_terminal_visual_banner")
        observation_status = "PROBE_COLLECTION_ERROR"

    if collection_issues:
        observation_status = "PROBE_COLLECTION_ERROR"

    evidence = dict(result)
    evidence.pop("status", None)
    return {
        "dg_source_profile": profile,
        "role": "UNSUPPORTED_COMPATIBILITY_PROBE",
        "acceptance_eligible": False,
        "support_status": "UNSUPPORTED_BY_FIXED_PRESET_CONTRACT",
        "unsupported_reason_code": UNSUPPORTED_REASON_CODE,
        "observation_status": observation_status,
        "strict_visual_contract_status": (
            strict_status if strict_status in {"PASS", "FAIL"} else "UNAVAILABLE"
        ),
        "collection_issues": collection_issues,
        "evidence": evidence,
    }


def _build_report(
    *,
    baseline_result: dict[str, Any],
    probe_rows: list[dict[str, Any]],
    probes_requested: bool,
    protected_before: dict[str, Any],
    protected_after: dict[str, Any],
    report_path: Path,
) -> dict[str, Any]:
    baseline_passed = (
        baseline_result.get("status") == "PASS"
        and baseline_result.get("production_profile_unchanged") is True
        and protected_after == protected_before
    )
    if not probes_requested:
        probe_collection_status = "NOT_REQUESTED"
    elif all(
        row.get("observation_status") != "PROBE_COLLECTION_ERROR"
        for row in probe_rows
    ):
        probe_collection_status = "COMPLETE"
    else:
        probe_collection_status = "INCOMPLETE"

    if not baseline_passed:
        status = FAIL_STATUS
    elif probe_collection_status == "INCOMPLETE":
        status = PROBE_ERROR_STATUS
    else:
        status = PASS_STATUS

    return {
        "schema": REPORT_SCHEMA,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": status,
        "motion_identity": {
            "recipe_version": "v6",
            "asset_revision": "v006",
            "drive_montage": EXPECTED_MONTAGE,
            "capture_manifest_schema": EXPECTED_SCHEMA,
        },
        "target_contract": {
            "kind": "FIXED_CURATED_METAHUMAN_PRESET",
            "backend_profile_object": METAHUMAN_BACKEND_PROFILE,
            "visual_actor_class": METAHUMAN_VISUAL_ACTOR_CLASS,
            "target_scale_contract": "ABSOLUTE_UNIT_WORLD_SCALE",
            "runtime_body_profile_mapping": "NONE",
            "accepted_dg_source_profile": ACCEPTANCE_PROFILE,
        },
        "lane_separation": {
            "dgmaster_gameplay_profile_matrix_authority": (
                "Scripts/run-session4-profile-smokes.py"
            ),
            "dgmaster_gameplay_profile_matrix_evaluated_by_this_report": False,
            "dgmaster_gameplay_profiles": list(PROFILES),
        },
        "execution_policy": (
            "SEQUENTIAL_FRESH_EXTERNAL_USERDIR_PER_LAUNCHED_SOURCE_PROFILE"
        ),
        "acceptance": {
            "eligible_profiles": [ACCEPTANCE_PROFILE],
            "profiles_requested": [ACCEPTANCE_PROFILE],
            "profiles_passed": [ACCEPTANCE_PROFILE] if baseline_passed else [],
            "profiles_failed": [] if baseline_passed else [ACCEPTANCE_PROFILE],
            "complete": baseline_passed,
            "result": baseline_result,
        },
        "compatibility_probes": {
            "requested": probes_requested,
            "profiles": list(UNSUPPORTED_PROBE_PROFILES),
            "support_status": "UNSUPPORTED_BY_FIXED_PRESET_CONTRACT",
            "unsupported_reason_code": UNSUPPORTED_REASON_CODE,
            "collection_status": probe_collection_status,
            "results": probe_rows,
        },
        "protected_profile_before": protected_before,
        "protected_profile_after": protected_after,
        "protected_profile_unchanged": protected_after == protected_before,
        "claim_boundary": {
            "automated_fixed_preset_baseline_acceptance_complete": baseline_passed,
            "automated_metahuman_profile_matrix_complete": False,
            "unsupported_profiles_counted_as_acceptance_passes": False,
            "dgmaster_gameplay_profile_matrix_evaluated_by_this_report": False,
            "human_animation_approval": False,
            "human_contact_approval": False,
            "cloth_hair_approval": False,
            "temporal_continuity_approval": False,
            "shipping_inclusion_verified": False,
            "release_ready": False,
        },
        "report_write": str(report_path),
    }


def _self_test() -> int:
    checks: list[tuple[str, bool]] = []

    def check(name: str, passed: bool) -> None:
        checks.append((name, passed))

    unit_scale = {"x": 1.0, "y": 1.0, "z": 1.0}
    captures = []
    for index, checkpoint in enumerate(EXPECTED_CHECKPOINTS):
        captures.append({
            "capture_index": index,
            "checkpoint_name": checkpoint,
            "montage_instance_montage_path": EXPECTED_MONTAGE,
            "checkpoint_timing_valid": True,
            "montage_instance_matches_committed_throw": True,
            "montage_full_weight_gate_valid": True,
            "retarget_pose_telemetry_valid": True,
            "target_pose_spatial_gate_valid": True,
            "motion_curves_finite_and_normalized": True,
            "checkpoint_curve_gate_valid": True,
            "bone_lengths_invariant": True,
            "subject_readable_scale": True,
            "metahuman_proof_telemetry_valid": True,
            "metahuman_visual_root_uses_absolute_scale": True,
            "metahuman_visual_root_world_scale": copy.deepcopy(unit_scale),
            "metahuman_body_world_scale": copy.deepcopy(unit_scale),
            "metahuman_head_world_scale": copy.deepcopy(unit_scale),
            "metahuman_outfit_world_scale": copy.deepcopy(unit_scale),
            "width": 1920,
            "height": 1080,
            "filename": f"{index}.png",
        })
    baseline = {
        "schema": EXPECTED_SCHEMA,
        "status": "PASS",
        "scope": "SESSION_19_METAHUMAN_ACTIVE_PRODUCTION_MOTION_VISUAL_EVIDENCE_ONLY",
        "capture_count": 5,
        "expected_capture_count": 5,
        "error": "",
        "proof_policy": {
            "fixed_frame_rate_hz": 60,
            "montage_position_source": "EXACT_COMMITTED_FANIMMONTAGEINSTANCE_ID_POSITION_AND_BLEND_WEIGHTS",
            "montage_full_weight_gate_applies_to_all_checkpoints": True,
        },
        "captures": captures,
        "live_throw": {
            "montage": EXPECTED_MONTAGE,
            "expected_production_motion_revision": "v006",
            "verified_metahuman_gameplay_presentation": True,
            "release_callback_count": 1,
            "stroke_delta": 1,
            "follow_through_reached": True,
            "recovered": True,
            "recovery_reason": "ThrowFinished",
            "authoritative_flight_validation_passed": True,
            "trajectory_export_deferral_requested": True,
            "trajectory_export_deferral_discarded": True,
        },
        "writes": {
            "png_and_manifest_only": True,
            "trajectory_json_csv_written_by_capture_lane": False,
            "existing_profile_loaded": False,
            "profile_writes_allowed": False,
            "practice_snapshot_writes_allowed": False,
            "uasset_writes": [],
            "level_save_calls": [],
            "content_save_calls": [],
        },
    }
    issues, _ = _validate_manifest(baseline, Path("C:/proof/manifest.json"), check_files=False)
    check("baseline_schema_v6", not issues)
    mutations = (
        ("schema", lambda p: p.__setitem__("schema", "v5")),
        ("scope", lambda p: p.__setitem__("scope", "wrong")),
        ("revision", lambda p: p["live_throw"].__setitem__("expected_production_motion_revision", "v005")),
        ("montage", lambda p: p["captures"][2].__setitem__("montage_instance_montage_path", "v005")),
        ("blend", lambda p: p["captures"][0].__setitem__("montage_full_weight_gate_valid", False)),
        ("backend", lambda p: p["live_throw"].__setitem__("verified_metahuman_gameplay_presentation", False)),
        ("write", lambda p: p["writes"].__setitem__("profile_writes_allowed", True)),
        ("duplicate_capture", lambda p: p["captures"].append(copy.deepcopy(p["captures"][0]))),
        ("duplicate_filename", lambda p: p["captures"][4].__setitem__("filename", "0.png")),
        ("traversal_filename", lambda p: p["captures"][4].__setitem__("filename", "../4.png")),
        (
            "root_absolute_false",
            lambda p: p["captures"][0].__setitem__(
                "metahuman_visual_root_uses_absolute_scale", False
            ),
        ),
        (
            "root_absolute_not_boolean",
            lambda p: p["captures"][0].__setitem__(
                "metahuman_visual_root_uses_absolute_scale", 1
            ),
        ),
        (
            "body_scale_082_axis",
            lambda p: p["captures"][1]["metahuman_body_world_scale"].__setitem__(
                "x", 0.82
            ),
        ),
        (
            "head_scale_non_finite",
            lambda p: p["captures"][2]["metahuman_head_world_scale"].__setitem__(
                "z", float("nan")
            ),
        ),
        (
            "outfit_scale_missing_axis",
            lambda p: p["captures"][3]["metahuman_outfit_world_scale"].pop("y"),
        ),
        (
            "root_scale_extra_axis",
            lambda p: p["captures"][4]["metahuman_visual_root_world_scale"].__setitem__(
                "w", 1.0
            ),
        ),
    )
    for name, mutate in mutations:
        candidate = copy.deepcopy(baseline)
        mutate(candidate)
        candidate_issues, _ = _validate_manifest(
            candidate, Path("C:/proof/manifest.json"), check_files=False
        )
        check(f"mutation:{name}", bool(candidate_issues))

    failure_manifest = copy.deepcopy(baseline)
    failure_manifest["status"] = "FAIL"
    failure_manifest["capture_count"] = 1
    failure_manifest["captures"] = failure_manifest["captures"][:1]
    failure_manifest["error"] = "capture 1 rejected fixed-preset retarget telemetry"
    check(
        "bounded_probe_failure_manifest",
        not _validate_probe_failure_manifest(
            failure_manifest, Path("C:/proof/manifest.json"), check_files=False
        ),
    )
    bad_failure_manifest = copy.deepcopy(failure_manifest)
    bad_failure_manifest["writes"]["profile_writes_allowed"] = True
    check(
        "probe_failure_manifest_rejects_write",
        bool(_validate_probe_failure_manifest(
            bad_failure_manifest, Path("C:/proof/manifest.json"), check_files=False
        )),
    )
    check(
        "pass_manifest_omits_probe_failure_diagnostics",
        not _probe_failure_manifest_issues_for_payload(
            baseline, Path("C:/proof/manifest.json"), check_files=False
        ),
    )

    check(
        "profile_partition",
        len((ACCEPTANCE_PROFILE, *UNSUPPORTED_PROBE_PROFILES)) == len(PROFILES)
        and set((ACCEPTANCE_PROFILE, *UNSUPPORTED_PROBE_PROFILES)) == set(PROFILES)
        and ACCEPTANCE_PROFILE not in UNSUPPORTED_PROBE_PROFILES,
    )
    check("default_plan_baseline_only", _profiles_to_run(False) == ("Baseline",))
    check(
        "opt_in_plan_exact",
        _profiles_to_run(True) == ("Baseline", *UNSUPPORTED_PROBE_PROFILES),
    )
    check(
        "cli_default_excludes_probes",
        _parse_args([]).include_unsupported_probes is False,
    )
    check(
        "cli_opt_in_includes_probes",
        _parse_args(["--include-unsupported-probes"]).include_unsupported_probes is True,
    )

    for profile in PROFILES:
        command = _command(
            DEFAULT_ENGINE,
            DEFAULT_PROJECT,
            profile,
            Path(rf"C:\proof\UserDirs\{profile}"),
            Path(rf"C:\proof\Logs\{profile}.log"),
            (),
        )
        for owned in (
            "-Session19ExpectedProductionMotionRevision=v006",
            f"-Session4Profile={profile}",
            "-Session19MetaHumanProductionVisualCapture",
            "-d3d12",
        ):
            check(f"owned_command:{profile}:{owned}", command.count(owned) == 1)
    for override in ("-Session4Profile=Other", "-NullRHI", "-ResX=640"):
        try:
            _validate_extra_args([override])
        except ValueError:
            rejected = True
        else:
            rejected = False
        check(f"owned_override:{override}", rejected)
    check(
        "unreal_error_detection",
        _unreal_error_lines("LogTemp: Error: bad\n") == ["LogTemp: Error: bad"],
    )
    check(
        "unreal_error_false_positive",
        not _unreal_error_lines("LogTemp: Display: clean\n"),
    )

    common_probe = {
        "validations": {
            "process_completed_before_timeout": True,
            "process_return_code_zero": True,
            "separate_log_created": True,
            "profile_override_reported_once": True,
        },
        "production_profile_unchanged": True,
        "manifest_payload_status": "PASS",
        "probe_failure_manifest_issues": [],
        "fail_line_matches_manifest_error": False,
        "unexpected_unreal_error_lines": [],
    }
    passing_probe = {
        **common_probe,
        "profile": "ShortCompact",
        "status": "PASS",
        "pass_banner_count": 1,
        "fail_banner_count": 0,
    }
    failing_probe = {
        **common_probe,
        "profile": "TallLongArms",
        "status": "FAIL",
        "pass_banner_count": 0,
        "fail_banner_count": 1,
        "manifest_payload_status": "FAIL",
        "fail_line_matches_manifest_error": True,
    }
    passing_probe_row = _classify_probe_result("ShortCompact", passing_probe)
    failing_probe_row = _classify_probe_result("TallLongArms", failing_probe)
    check(
        "unsupported_strict_pass_is_observation",
        passing_probe_row["observation_status"]
        == "OBSERVED_STRICT_PASS_OUTSIDE_SUPPORT_CONTRACT"
        and passing_probe_row["acceptance_eligible"] is False,
    )
    check(
        "unsupported_strict_fail_is_observation",
        failing_probe_row["observation_status"]
        == "OBSERVED_STRICT_FAIL_OUTSIDE_SUPPORT_CONTRACT"
        and failing_probe_row["acceptance_eligible"] is False,
    )
    broken_probe = copy.deepcopy(failing_probe)
    broken_probe["unexpected_unreal_error_lines"] = ["LogTemp: Error: unrelated"]
    broken_probe_row = _classify_probe_result("SliderMin", broken_probe)
    check(
        "probe_extra_error_is_collection_error",
        broken_probe_row["observation_status"] == "PROBE_COLLECTION_ERROR",
    )

    baseline_result = {
        "profile": ACCEPTANCE_PROFILE,
        "status": "PASS",
        "production_profile_unchanged": True,
    }
    probe_rows = [
        passing_probe_row,
        failing_probe_row,
        _classify_probe_result("SliderMin", failing_probe),
        _classify_probe_result("SliderMax", passing_probe),
    ]
    report_path = Path("C:/proof/Session19V006FixedMetaHumanPresetAcceptanceReport.json")
    report = _build_report(
        baseline_result=baseline_result,
        probe_rows=probe_rows,
        probes_requested=True,
        protected_before={"sha256": "A"},
        protected_after={"sha256": "A"},
        report_path=report_path,
    )
    check("report_schema", report["schema"] == REPORT_SCHEMA)
    check("report_baseline_pass_status", report["status"] == PASS_STATUS)
    check(
        "report_only_baseline_accepted",
        report["acceptance"]["profiles_passed"] == [ACCEPTANCE_PROFILE]
        and all(
            row["acceptance_eligible"] is False
            for row in report["compatibility_probes"]["results"]
        ),
    )
    check(
        "report_never_claims_metahuman_matrix",
        report["claim_boundary"]["automated_metahuman_profile_matrix_complete"]
        is False
        and report["claim_boundary"][
            "unsupported_profiles_counted_as_acceptance_passes"
        ] is False,
    )
    baseline_failure = dict(baseline_result)
    baseline_failure["status"] = "FAIL"
    failed_report = _build_report(
        baseline_result=baseline_failure,
        probe_rows=[_unrequested_probe_row(p) for p in UNSUPPORTED_PROBE_PROFILES],
        probes_requested=False,
        protected_before={"sha256": "A"},
        protected_after={"sha256": "A"},
        report_path=report_path,
    )
    check("baseline_failure_status", failed_report["status"] == FAIL_STATUS)
    protected_mutation_report = _build_report(
        baseline_result=baseline_result,
        probe_rows=[_unrequested_probe_row(p) for p in UNSUPPORTED_PROBE_PROFILES],
        probes_requested=False,
        protected_before={"sha256": "A"},
        protected_after={"sha256": "B"},
        report_path=report_path,
    )
    check(
        "protected_profile_mutation_revokes_acceptance",
        protected_mutation_report["status"] == FAIL_STATUS
        and protected_mutation_report["claim_boundary"][
            "automated_fixed_preset_baseline_acceptance_complete"
        ] is False,
    )
    probe_error_report = _build_report(
        baseline_result=baseline_result,
        probe_rows=[broken_probe_row, *probe_rows[1:]],
        probes_requested=True,
        protected_before={"sha256": "A"},
        protected_after={"sha256": "A"},
        report_path=report_path,
    )
    check(
        "probe_collection_error_status",
        probe_error_report["status"] == PROBE_ERROR_STATUS
        and probe_error_report["claim_boundary"][
            "automated_fixed_preset_baseline_acceptance_complete"
        ] is True,
    )
    not_requested_report = _build_report(
        baseline_result=baseline_result,
        probe_rows=[_unrequested_probe_row(p) for p in UNSUPPORTED_PROBE_PROFILES],
        probes_requested=False,
        protected_before={"sha256": "A"},
        protected_after={"sha256": "A"},
        report_path=report_path,
    )
    check(
        "probes_not_requested_do_not_block",
        not_requested_report["status"] == PASS_STATUS
        and not_requested_report["compatibility_probes"]["collection_status"]
        == "NOT_REQUESTED",
    )

    failures = [name for name, passed in checks if not passed]
    if failures:
        print(
            "V006 FIXED METAHUMAN BASELINE SELF-TEST FAIL: "
            + ", ".join(failures)
        )
        return 1
    print(f"V006 FIXED METAHUMAN BASELINE SELF-TEST PASS: cases={len(checks)}")
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    if args.self_test:
        return _self_test()
    engine, project, output_root = _validate_configuration(args)
    (output_root / "Logs").mkdir(parents=True, exist_ok=False)
    (output_root / "UserDirs").mkdir(parents=True, exist_ok=False)
    protected_profile = project.parent / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    protected_before = _identity(protected_profile)
    planned_profiles = _profiles_to_run(args.include_unsupported_probes)
    collected_results: dict[str, dict[str, Any]] = {}
    for index, profile in enumerate(planned_profiles, start=1):
        role = (
            "fixed-preset acceptance"
            if profile == ACCEPTANCE_PROFILE
            else "unsupported compatibility probe"
        )
        print(
            f"[{index}/{len(planned_profiles)}] {role}: DG source profile {profile}",
            flush=True,
        )
        try:
            result = _run_profile(
                engine, project, output_root, profile,
                args.timeout_seconds, args.extra_arg,
            )
        except Exception as exc:
            result = {
                "profile": profile,
                "status": "FAIL",
                "validations": {"process_launched_and_collected": False},
                "failed_validations": ["process_launched_and_collected"],
                "exception": f"{type(exc).__name__}: {exc}",
            }
        unchanged = _identity(protected_profile) == protected_before
        result["production_profile_unchanged"] = unchanged
        result.setdefault("validations", {})["production_profile_unchanged"] = unchanged
        if not unchanged:
            result["status"] = "FAIL"
            result.setdefault("failed_validations", []).append("production_profile_unchanged")
        collected_results[profile] = result
        if profile == ACCEPTANCE_PROFILE:
            outcome = f"acceptance={result['status']}"
        else:
            outcome = (
                "observation="
                + _classify_probe_result(profile, result)["observation_status"]
            )
        print(
            f"[{index}/{len(planned_profiles)}] {profile}: {outcome}",
            flush=True,
        )

    baseline_result = collected_results[ACCEPTANCE_PROFILE]
    if args.include_unsupported_probes:
        probe_rows = [
            _classify_probe_result(profile, collected_results[profile])
            for profile in UNSUPPORTED_PROBE_PROFILES
        ]
    else:
        probe_rows = [
            _unrequested_probe_row(profile)
            for profile in UNSUPPORTED_PROBE_PROFILES
        ]

    report_path = output_root / REPORT_NAME
    report = _build_report(
        baseline_result=baseline_result,
        probe_rows=probe_rows,
        probes_requested=args.include_unsupported_probes,
        protected_before=protected_before,
        protected_after=_identity(protected_profile),
        report_path=report_path,
    )
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if report["status"] == FAIL_STATUS:
        print(
            f"V006 FIXED METAHUMAN BASELINE ACCEPTANCE FAIL: report={report_path}",
            file=sys.stderr,
        )
        return 1
    if report["status"] == PROBE_ERROR_STATUS:
        failed_probes = [
            row["dg_source_profile"]
            for row in probe_rows
            if row["observation_status"] == "PROBE_COLLECTION_ERROR"
        ]
        print(
            "V006 FIXED METAHUMAN BASELINE PASSED BUT PROBE COLLECTION FAILED: "
            + ", ".join(failed_probes)
            + f" report={report_path}",
            file=sys.stderr,
        )
        return 1
    print(
        "V006 FIXED METAHUMAN BASELINE ACCEPTANCE PASS: "
        f"probes={report['compatibility_probes']['collection_status']} "
        f"report={report_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
