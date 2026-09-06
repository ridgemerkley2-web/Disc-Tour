"""Run the isolated no-save Session 8B Root Motion counterfactual."""

from __future__ import annotations

import hashlib
import json
import math
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanRootMotionEnvelope.v1"
DIAGNOSTIC_SWITCH = "-DGSession8BMetaHumanRootMotionCounterfactual"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
EXPECTED_PROJECT_ROOT = Path(r"C:\DGTour").resolve()
EXPECTED_USER_ROOT = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanRootMotionCounterfactual.json"
)
CANONICAL_RELATIVES = (
    Path("Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset"),
    Path(
        "Content/DiscGolf/Characters/MetaHuman/"
        "RTG_DGMaster_To_MetaHuman.uasset"
    ),
    Path(
        "Content/DiscGolf/Characters/Avatar/Data/"
        "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
    ),
)
PRODUCTION_SAVE_ROOT = Path("Saved/SaveGames")
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
ANCHORS = {"root", "pelvis", "spine_01", "head"}


class DiagnosticError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "mtime_ns": 0, "sha256": ""}
    stat = path.stat()
    return {
        "exists": True,
        "bytes": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": sha256(path),
    }


def tree_snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): file_record(path)
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


def directory_snapshot(root: Path) -> list[str]:
    if not root.is_dir():
        return []
    return sorted(
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_dir()
    )


def canonical_snapshot(project_root: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for relative in CANONICAL_RELATIVES:
        main_path = project_root / relative
        base = main_path.with_suffix("")
        for suffix in PACKAGE_SUFFIXES:
            candidate = base.with_suffix(suffix)
            if candidate.is_file():
                result[candidate.relative_to(project_root).as_posix()] = (
                    file_record(candidate)
                )
    return result


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(
        re.search(rf'(?i)(?:^|\s|"){re.escape(switch)}(?=$|\s|")', command_line)
    )


def command_values(command_line: str, name: str) -> list[str]:
    pattern = re.compile(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))'
    )
    return [quoted or plain for quoted, plain in pattern.findall(command_line)]


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (DIAGNOSTIC_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise DiagnosticError(f"required command-line switches absent: {missing}")
    run_values = command_values(command_line, "run")
    if len(run_values) != 1 or run_values[0].casefold() != "pythonscript":
        raise DiagnosticError("diagnostic requires exactly one -run=PythonScript")
    if re.search(r"(?i)(?:^|\s)-executepythonscript(?:=|\s|$)", command_line):
        raise DiagnosticError("ExecutePythonScript is outside the isolated runner contract")
    user_values = command_values(command_line, "UserDir")
    if len(user_values) != 1:
        raise DiagnosticError("exactly one absolute -UserDir=<UUID path> is required")
    user_dir = Path(user_values[0]).resolve()
    try:
        relative = user_dir.relative_to(EXPECTED_USER_ROOT)
    except ValueError as exc:
        raise DiagnosticError(f"UserDir is outside Session8B root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise DiagnosticError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise DiagnosticError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold() or not user_dir.is_dir():
        raise DiagnosticError("UserDir UUID is not canonical and active")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise DiagnosticError("UserDir must remain external to the project")
    return user_dir


def dirty_names() -> tuple[list[str], list[str]]:
    content = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    maps = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return content, maps


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise DiagnosticError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    def no_constant(value: str) -> Any:
        raise DiagnosticError(f"non-finite JSON number {value!r} in {label}")

    try:
        value = json.loads(
            text,
            object_pairs_hook=no_duplicates,
            parse_constant=no_constant,
        )
    except json.JSONDecodeError as exc:
        raise DiagnosticError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise DiagnosticError(f"{label} JSON root is not an object")
    return value


def resolve_helper() -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    names = (
        "diagnose_session8_b_meta_human_root_motion_counterfactual",
        "diagnose_session8b_meta_human_root_motion_counterfactual",
    )
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise DiagnosticError(f"expected exactly one diagnostic spelling: {matches}")
    return getattr(utility, matches[0])


def require_exact(payload: dict[str, Any], expected: dict[str, Any]) -> None:
    for key, value in expected.items():
        if payload.get(key) != value:
            raise DiagnosticError(
                f"helper.{key}={payload.get(key)!r}, expected {value!r}"
            )


def finite_number(value: Any, label: str) -> float:
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or not math.isfinite(float(value))
    ):
        raise DiagnosticError(f"{label} is not finite: {value!r}")
    return float(value)


def validate_transform(value: Any, label: str) -> None:
    if not isinstance(value, dict):
        raise DiagnosticError(f"{label} is not an object")
    required = {
        "translation_x_cm",
        "translation_y_cm",
        "translation_z_cm",
        "rotation_pitch_degrees",
        "rotation_yaw_degrees",
        "rotation_roll_degrees",
        "rotation_quaternion_x",
        "rotation_quaternion_y",
        "rotation_quaternion_z",
        "rotation_quaternion_w",
        "scale_x",
        "scale_y",
        "scale_z",
        "bone_index",
    }
    if set(value) != required:
        raise DiagnosticError(f"{label} transform field set differs: {set(value)}")
    for field in required:
        finite_number(value[field], f"{label}.{field}")


def validate_helper(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "ROOT_MOTION_COUNTERFACTUAL_DIAGNOSTIC",
            "status": "PASS",
            "disk_mutation": "NONE",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "diagnostic_schema": (
                "DiscGolfTour.Session8BMetaHumanRootMotionCounterfactual.v1"
            ),
            "counterfactual_delta": (
                "TRANSIENT_PROFILE_ONLY_ROOT_MOTION_BENABLED_TRUE_TO_FALSE"
            ),
            "processor_asset_scope": (
                "TWO_INDEPENDENT_TRANSIENT_DUPLICATES_OF_CANONICAL_RETARGETER"
            ),
            "asset_save_attempted": False,
            "canonical_asset_mutation_attempted": False,
            "editor_retargeter_controller_acquired": False,
            "cleanup_requires_asset_restore": False,
            "transient_profile_only_root_enabled_changed": True,
            "transient_retargeter_duplicates_created": True,
            "pelvis_motion_executes_before_root_motion": True,
            "pelvis_scale_vertical_precedes_late_root_overwrite": True,
            "pelvis_motion_neutral_settings_confirmed": True,
            "pelvis_motion_neutral_horizontal_policy": (
                "BLEND_TO_ABSOLUTE_OFFSET_ZERO_USES_HEIGHT_NORMALIZED_"
                "SOURCE_COMPONENT_POSITION"
            ),
            "target_retarget_pose_lateral_offset_is_not_neutral_output_invariant": True,
            "source_root_near_zero": True,
            "target_retarget_pelvis_near_93_164_cm": True,
            "baseline_pelvis_near_zero": True,
            "baseline_pelvis_matches_source_root": True,
            "root_off_pelvis_restores_target_retarget_pose_height": True,
            "root_off_pelvis_matches_expected_pelvis_motion": True,
            "pelvis_counterfactual_delta_exceeds_90_cm": True,
            "named_root_stable": True,
            "named_root_near_zero": True,
            "spine_01_stable": True,
            "head_stable": True,
            "canonical_in_memory_settings_preserved": True,
            "canonical_disk_bytes_preserved": True,
            "dirty_package_baseline_preserved": True,
            "savable_dirty_package_baseline_preserved": True,
            "diagnostic_completed": True,
            (
                "root_motion_target_root_equals_pelvis_"
                "causal_counterfactual_confirmed"
            ): True,
        },
    )
    rig = payload.get("ik_rig_retarget_definition")
    if not isinstance(rig, dict) or rig != {
        "source_root": "root",
        "source_pelvis": "pelvis",
        "target_root": "pelvis",
        "target_pelvis": "pelvis",
        "target_root_equals_target_pelvis": True,
    }:
        raise DiagnosticError(f"IK rig root/pelvis identity differs: {rig!r}")
    if payload.get("canonical_target_retarget_pose_name") != "DGMasterAligned":
        raise DiagnosticError("target retarget pose is not DGMasterAligned")

    for field in (
        "source_retarget_pose_anchor_transforms",
        "scaled_source_input_anchor_transforms",
        "target_retarget_pose_anchor_transforms",
        "baseline_output_anchor_transforms",
        "root_off_output_anchor_transforms",
    ):
        anchors = payload.get(field)
        if not isinstance(anchors, dict) or set(anchors) != ANCHORS:
            raise DiagnosticError(f"{field} anchor set differs: {anchors!r}")
        for bone_name, transform in anchors.items():
            validate_transform(transform, f"{field}.{bone_name}")

    deltas = payload.get("counterfactual_transform_deltas")
    if not isinstance(deltas, dict):
        raise DiagnosticError("counterfactual_transform_deltas is absent")
    for field, value in deltas.items():
        finite_number(value, f"counterfactual_transform_deltas.{field}")
    if abs(float(deltas["baseline_pelvis_translation_z_cm"])) > 0.10:
        raise DiagnosticError("baseline pelvis is not within 0.10 cm of zero")
    if abs(float(deltas["target_retarget_pelvis_translation_z_cm"]) - 93.164) > 0.25:
        raise DiagnosticError("target retarget pelvis is not near 93.164 cm")
    if abs(
        float(deltas["root_off_pelvis_translation_z_cm"])
        - float(deltas["target_retarget_pelvis_translation_z_cm"])
    ) > 0.10:
        raise DiagnosticError("Root-off pelvis does not restore target pose height")
    if float(
        deltas["root_off_pelvis_height_from_target_retarget_pose_cm"]
    ) > 0.10:
        raise DiagnosticError("reported Root-off pelvis height delta exceeds tolerance")
    if float(
        deltas["root_off_pelvis_from_expected_pelvis_motion_cm"]
    ) > 0.10:
        raise DiagnosticError("Root-off pelvis differs from neutral Pelvis Motion")
    full_target_delta = float(
        deltas["root_off_pelvis_from_target_retarget_pose_cm"]
    )
    lateral_target_delta = float(
        deltas["root_off_pelvis_lateral_from_target_retarget_pose_cm"]
    )
    if abs(full_target_delta - lateral_target_delta) > 0.01:
        raise DiagnosticError("target-pose delta is not isolated to the lateral plane")

    expected_pelvis = payload.get(
        "pelvis_motion_expected_neutral_output_translation_cm"
    )
    if not isinstance(expected_pelvis, dict) or set(expected_pelvis) != {
        "x",
        "y",
        "z",
    }:
        raise DiagnosticError("expected neutral Pelvis Motion translation is absent")
    for axis, value in expected_pelvis.items():
        finite_number(value, f"expected_neutral_pelvis.{axis}")
    if abs(float(expected_pelvis["y"])) > 0.10:
        raise DiagnosticError("neutral Pelvis Motion does not predict lateral Y near zero")
    if float(deltas["spine_01_translation_delta_cm"]) > 0.10 or float(
        deltas["spine_01_rotation_delta_degrees"]
    ) > 0.10 or float(deltas["spine_01_scale_maximum_delta"]) > 0.001:
        raise DiagnosticError("spine_01 changed outside the frozen tolerance")
    if (
        float(deltas["named_root_translation_delta_cm"]) > 0.10
        or float(deltas["named_root_rotation_delta_degrees"]) > 0.10
        or float(deltas["named_root_scale_maximum_delta"]) > 0.001
    ):
        raise DiagnosticError("named root changed outside the frozen tolerance")
    if (
        float(deltas["head_translation_delta_cm"]) > 0.10
        or float(deltas["head_rotation_delta_degrees"]) > 0.10
        or float(deltas["head_scale_maximum_delta"]) > 0.001
    ):
        raise DiagnosticError("head changed outside the frozen tolerance")

    baseline = payload.get("baseline_processor_root_motion_settings")
    counterfactual = payload.get("counterfactual_processor_root_motion_settings")
    if not isinstance(baseline, dict) or not isinstance(counterfactual, dict):
        raise DiagnosticError("processor Root Motion settings evidence is absent")
    if baseline.get("enabled") is not True or counterfactual.get("enabled") is not False:
        raise DiagnosticError("processor Root Motion enabled A/B differs")
    for settings in (baseline, counterfactual):
        if (
            settings.get("source_root_bone") != "root"
            or settings.get("target_root_bone") != "pelvis"
            or settings.get("target_pelvis_bone") != "pelvis"
            or settings.get("root_motion_source") != "CopyFromSourceRoot"
            or settings.get("root_height_source") != "CopyHeightFromSource"
        ):
            raise DiagnosticError(f"Root Motion settings identity differs: {settings}")


def write_report(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.tmp")
    temporary.write_text(
        json.dumps(state, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(path)


def main() -> None:
    project_root = Path(unreal.Paths.project_dir()).resolve()
    if project_root != EXPECTED_PROJECT_ROOT:
        raise DiagnosticError(
            f"project root {project_root} is not {EXPECTED_PROJECT_ROOT}"
        )
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir = validate_command_line(command_line, project_root)
    report_path = user_dir / REPORT_RELATIVE
    if report_path.exists():
        raise DiagnosticError(f"fresh UUID already contains report: {report_path}")
    if list(user_dir.rglob("*.sav")):
        raise DiagnosticError("fresh external UserDir already contains save files")

    state: dict[str, Any] = {
        "schema": SCHEMA,
        "status": "FAIL",
        "phase": "PRE_SNAPSHOT",
        "started_utc": utc_now(),
        "project_root": str(project_root),
        "external_user_dir": str(user_dir),
        "report_path": str(report_path),
        "diagnostic_switch": DIAGNOSTIC_SWITCH,
        "no_portal_switch": NO_PORTAL_SWITCH,
        "errors": [],
    }
    try:
        canonical_before = canonical_snapshot(project_root)
        missing_main_files = [
            relative.as_posix()
            for relative in CANONICAL_RELATIVES
            if relative.as_posix() not in canonical_before
        ]
        if missing_main_files:
            raise DiagnosticError(
                f"canonical package main files are absent: {missing_main_files}"
            )
        saves_root = project_root / PRODUCTION_SAVE_ROOT
        saves_before = tree_snapshot(saves_root)
        save_directories_before = directory_snapshot(saves_root)
        dirty_before = dirty_names()
        external_saves_before = sorted(
            path.relative_to(user_dir).as_posix()
            for path in user_dir.rglob("*.sav")
        )

        state["phase"] = "HELPER"
        helper = resolve_helper()
        payload = strict_json_text(helper(), "root-motion helper")
        state["helper"] = payload
        validate_helper(payload)

        state["phase"] = "POST_SNAPSHOT"
        canonical_after = canonical_snapshot(project_root)
        saves_after = tree_snapshot(saves_root)
        save_directories_after = directory_snapshot(saves_root)
        dirty_after = dirty_names()
        external_saves_after = sorted(
            path.relative_to(user_dir).as_posix()
            for path in user_dir.rglob("*.sav")
        )
        if canonical_after != canonical_before:
            raise DiagnosticError("canonical package file set or bytes changed")
        if saves_after != saves_before or save_directories_after != save_directories_before:
            raise DiagnosticError("project SaveGames file set, bytes, or topology changed")
        if dirty_after != dirty_before:
            raise DiagnosticError("editor dirty-package baseline changed")
        if external_saves_after != external_saves_before:
            raise DiagnosticError("external UserDir save set changed")

        state.update(
            {
                "status": "PASS_NO_WRITE",
                "phase": "COMPLETE",
                "completed_utc": utc_now(),
                "canonical_before": canonical_before,
                "canonical_after": canonical_after,
                "canonical_disk_preserved": True,
                "project_savegames_preserved": True,
                "external_user_dir_save_set_preserved": True,
                "dirty_package_baseline_preserved": True,
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["completed_utc"] = utc_now()
        state["errors"] = [f"{type(exc).__name__}: {exc}"]
        try:
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                "DG_SESSION8B_ROOT_MOTION report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            "DG_SESSION8B_ROOT_MOTION: "
            f"FAIL phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_ROOT_MOTION: "
        f"PASS_NO_WRITE report={report_path}"
    )


if __name__ == "__main__":
    main()
