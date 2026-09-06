"""Disable only the canonical Session 8B Root Motion retarget op transactionally."""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanRetargetRootMotionCorrection.v1"
CORRECT_SWITCH = "-DGSession8BMetaHumanRetargetRootMotionCorrection"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
EXPECTED_PROJECT_ROOT = Path(r"C:\DGTour").resolve()
EXPECTED_USER_ROOT = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanRetargetRootMotionCorrection.json"
)
WRAPPER_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset"
)
RETARGET_PACKAGE = "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman"
RETARGET_OBJECT = f"{RETARGET_PACKAGE}.RTG_DGMaster_To_MetaHuman"
RETARGET_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
)
PROFILE_RELATIVE = Path(
    "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
)
LEGACY_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"
EXPECTED_POLICY = (
    "PELVIS_FK_CURVES_ENABLED_RUN_IK_AND_ROOT_MOTION_DISABLED"
)
EXPECTED_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_OP_ENABLED_MASK_BEFORE = [True, True, False, True, True]
EXPECTED_OP_ENABLED_MASK = [True, True, False, False, True]
CONTENT_BEFORE = {
    "file_count": 953,
    "bytes": 2920024366,
    "manifest_sha256": (
        "95400E9857F222C4454587A4AB36E0D1993DEA64B9EF48EF24969E5F8320D651"
    ),
}
CONTENT_DIRECTORY_COUNT = 230
CANONICAL_BEFORE = {
    WRAPPER_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 225494,
        "sha256": (
            "C322CBFA8F9F3268D2A7F05E20FF942F5A3B51F2D2C6AF53ED74E24266DF3E2C"
        ),
    },
    RETARGET_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 22640,
        "sha256": (
            "6316D573F271EE40BD7D3C42258723F9C8669A5606A0FB0373016DF3EBFBEAE9"
        ),
    },
    PROFILE_RELATIVE.as_posix(): {
        "exists": True,
        "bytes": 2274,
        "sha256": (
            "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D"
        ),
    },
}
COUNTERFACTUAL_REPORT = Path(
    r"C:\DGTour_TestRuns\Session8B\4824f7b3-783d-437b-a248-bde0b9489858"
    r"\Saved\CharacterFramework\Session8BMetaHumanRootMotionCounterfactual.json"
)
COUNTERFACTUAL_REPORT_RECORD = {
    "exists": True,
    "bytes": 30010,
    "sha256": (
        "C2A83D7892CAF2D378579BB0DE348F3449A131D03D74F672F0F04F67FCE80205"
    ),
}
PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted"
    r"\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE = {
    "exists": True,
    "bytes": 5212,
    "sha256": (
        "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
    ),
}
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
TARGET_PELVIS_Z_CM = 93.164
TARGET_PELVIS_Z_TOLERANCE_CM = 0.25
TARGET_PELVIS_HORIZONTAL_TOLERANCE_CM = 0.10
OUTPUT_PELVIS_HEIGHT_TOLERANCE_CM = 0.10
OUTPUT_PELVIS_MOTION_TOLERANCE_CM = 0.10


class CorrectionError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {"exists": True, "bytes": path.stat().st_size, "sha256": sha256(path)}


def snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): record(path)
        for path in sorted(
            candidate for candidate in root.rglob("*") if candidate.is_file()
        )
    }


def snapshot_summary(files: dict[str, dict[str, Any]]) -> dict[str, Any]:
    digest = hashlib.sha256()
    total_bytes = 0
    for relative, identity in sorted(files.items()):
        size = int(identity["bytes"])
        total_bytes += size
        digest.update(
            f"{relative}\t{size}\t{identity['sha256']}\n".encode("utf-8")
        )
    return {
        "file_count": len(files),
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest().upper(),
    }


def directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_dir()
    }


def validate_finite_json(value: Any, label: str, location: str = "$") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise CorrectionError(f"non-finite JSON number in {label} at {location}")
    if isinstance(value, dict):
        for key, child in value.items():
            validate_finite_json(child, label, f"{location}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            validate_finite_json(child, label, f"{location}[{index}]")


def write_report(path: Path, state: dict[str, Any]) -> None:
    validate_finite_json(state, "transaction report")
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f"{path.name}.tmp")
    try:
        temp.write_text(
            json.dumps(state, indent=2, sort_keys=True, allow_nan=False) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        temp.replace(path)
    finally:
        if temp.exists():
            temp.unlink()


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise CorrectionError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    def reject_constant(value: str) -> None:
        raise CorrectionError(f"non-finite JSON constant {value!r} in {label}")

    try:
        value = json.loads(
            text,
            object_pairs_hook=no_duplicates,
            parse_constant=reject_constant,
        )
    except json.JSONDecodeError as exc:
        raise CorrectionError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise CorrectionError(f"{label} JSON root is not an object")
    validate_finite_json(value, label)
    return value


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(
        re.search(rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line)
    )


def command_values(command_line: str, name: str) -> list[str]:
    matches = re.findall(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))',
        command_line,
    )
    return [quoted or plain for quoted, plain in matches]


def validate_command_line(command_line: str, project_root: Path) -> Path:
    if project_root != EXPECTED_PROJECT_ROOT:
        raise CorrectionError(
            f"project root is {project_root}, expected {EXPECTED_PROJECT_ROOT}"
        )
    required = (CORRECT_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise CorrectionError(f"required command-line switches absent: {missing}")
    run_tokens = re.findall(r"(?i)(?:^|\s)-run=pythonscript(?=$|\s)", command_line)
    if len(run_tokens) != 1 or re.search(
        r"(?i)(?:^|\s)-executepythonscript(?:=|\s|$)", command_line
    ):
        raise CorrectionError("correction requires exactly one PythonScript commandlet")
    user_dir_values = command_values(command_line, "UserDir")
    if len(user_dir_values) != 1 or not Path(user_dir_values[0]).is_absolute():
        raise CorrectionError("exactly one absolute external -UserDir is required")
    user_dir_text = user_dir_values[0]
    user_dir = Path(user_dir_text).resolve()
    try:
        relative = user_dir.relative_to(EXPECTED_USER_ROOT)
    except ValueError as exc:
        raise CorrectionError(f"UserDir is outside Session8B test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise CorrectionError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise CorrectionError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise CorrectionError("UserDir UUID is not canonical")
    if not user_dir.is_dir():
        raise CorrectionError("UserDir UUID directory must already exist")
    if user_dir == COUNTERFACTUAL_REPORT.parents[2]:
        raise CorrectionError("correction UserDir must differ from counterfactual evidence UUID")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise CorrectionError("UserDir must be external to the project")
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


def resolve_method(names: tuple[str, ...], label: str) -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise CorrectionError(f"expected exactly one {label} spelling, got {matches}")
    return getattr(utility, matches[0])


def resolve_prepare_helper() -> Callable[[], str]:
    return resolve_method(
        (
            "prepare_session8_b_meta_human_root_motion_correction",
            "prepare_session8b_meta_human_root_motion_correction",
        ),
        "Root Motion prepare helper",
    )


def resolve_validate_helper() -> Callable[[], str]:
    return resolve_method(
        (
            "validate_session8_b_meta_human_assets",
            "validate_session8b_meta_human_assets",
        ),
        "validation helper",
    )


def require_exact(payload: dict[str, Any], expected: dict[str, Any], label: str) -> None:
    def exact_json_equal(actual: Any, wanted: Any) -> bool:
        if type(actual) is not type(wanted):
            return False
        if isinstance(wanted, list):
            return len(actual) == len(wanted) and all(
                exact_json_equal(actual_value, wanted_value)
                for actual_value, wanted_value in zip(actual, wanted)
            )
        if isinstance(wanted, dict):
            return set(actual) == set(wanted) and all(
                exact_json_equal(actual[key], wanted[key]) for key in wanted
            )
        return actual == wanted

    for key, value in expected.items():
        actual = payload.get(key)
        if not exact_json_equal(actual, value):
            raise CorrectionError(
                f"{label} field {key}={actual!r}, expected exact {value!r}"
            )


def finite_number(value: Any, label: str) -> float:
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or not math.isfinite(float(value))
    ):
        raise CorrectionError(f"{label} is not a finite number: {value!r}")
    return float(value)


def anchor_translation(
    anchors: dict[str, Any], name: str, label: str
) -> tuple[float, float, float]:
    anchor = anchors.get(name)
    if not isinstance(anchor, dict):
        raise CorrectionError(f"{label}.{name} anchor is absent or not an object")
    expected_fields = {
        "translation_x_cm",
        "translation_y_cm",
        "translation_z_cm",
        "rotation_pitch_degrees",
        "rotation_yaw_degrees",
        "rotation_roll_degrees",
        "scale_x",
        "scale_y",
        "scale_z",
    }
    if set(anchor) != expected_fields:
        raise CorrectionError(
            f"{label}.{name} transform field set differs: {set(anchor)!r}"
        )
    for field in expected_fields:
        finite_number(anchor[field], f"{label}.{name}.{field}")
    translation = tuple(
        float(anchor[f"translation_{axis}_cm"])
        for axis in ("x", "y", "z")
    )
    for axis in ("x", "y", "z"):
        scale = float(anchor[f"scale_{axis}"])
        if abs(scale - 1.0) > 0.001:
            raise CorrectionError(f"{label}.{name} scale is not neutral: {anchor!r}")
    return translation


def validate_corrected_pelvis_invariants(
    payload: dict[str, Any], label: str
) -> None:
    require_exact(
        payload,
        {
            "retarget_pelvis_motion_neutral_settings_confirmed": True,
            "retarget_target_pose_pelvis_expected_z_cm": TARGET_PELVIS_Z_CM,
            "retarget_target_pose_pelvis_z_tolerance_cm": (
                TARGET_PELVIS_Z_TOLERANCE_CM
            ),
            "retarget_output_pelvis_height_tolerance_cm": (
                OUTPUT_PELVIS_HEIGHT_TOLERANCE_CM
            ),
            "retarget_output_pelvis_motion_tolerance_cm": (
                OUTPUT_PELVIS_MOTION_TOLERANCE_CM
            ),
            "retarget_target_pose_pelvis_near_93_164_cm": True,
            "retarget_output_pelvis_restores_target_pose_height": True,
            "retarget_output_pelvis_matches_expected_pelvis_motion": True,
        },
        label,
    )
    target_z = finite_number(
        payload.get("retarget_target_pose_pelvis_z_cm"),
        f"{label}.retarget_target_pose_pelvis_z_cm",
    )
    output_z = finite_number(
        payload.get("retarget_output_pelvis_z_cm"),
        f"{label}.retarget_output_pelvis_z_cm",
    )
    expected_motion_z = finite_number(
        payload.get("retarget_expected_pelvis_motion_z_cm"),
        f"{label}.retarget_expected_pelvis_motion_z_cm",
    )
    target_height_delta = finite_number(
        payload.get("retarget_output_pelvis_height_from_target_pose_cm"),
        f"{label}.retarget_output_pelvis_height_from_target_pose_cm",
    )
    pelvis_motion_delta = finite_number(
        payload.get("retarget_output_pelvis_from_expected_pelvis_motion_cm"),
        f"{label}.retarget_output_pelvis_from_expected_pelvis_motion_cm",
    )
    if abs(target_z - TARGET_PELVIS_Z_CM) > TARGET_PELVIS_Z_TOLERANCE_CM:
        raise CorrectionError(f"{label} target-pose pelvis height differs: {target_z}")
    if abs(output_z - target_z) > OUTPUT_PELVIS_HEIGHT_TOLERANCE_CM:
        raise CorrectionError(f"{label} output pelvis did not restore target height")
    if abs(output_z - expected_motion_z) > OUTPUT_PELVIS_MOTION_TOLERANCE_CM:
        raise CorrectionError(f"{label} output pelvis differs from Pelvis Motion")
    if not 0.0 <= target_height_delta <= OUTPUT_PELVIS_HEIGHT_TOLERANCE_CM:
        raise CorrectionError(f"{label} target-height delta differs: {target_height_delta}")
    if not 0.0 <= pelvis_motion_delta <= OUTPUT_PELVIS_MOTION_TOLERANCE_CM:
        raise CorrectionError(f"{label} Pelvis Motion delta differs: {pelvis_motion_delta}")
    if abs(target_height_delta - abs(output_z - target_z)) > 0.001:
        raise CorrectionError(f"{label} target-height delta is internally inconsistent")


def validate_pose_evidence(payload: dict[str, Any], label: str) -> None:
    require_exact(
        payload,
        {
            "retarget_processor_initialized": True,
            "retarget_output_bone_count": 342,
            "retarget_output_pose_plausible": True,
            "retarget_output_translation_outlier_count": 0,
        },
        label,
    )
    max_abs = finite_number(
        payload.get("retarget_output_max_abs_translation_cm"),
        f"{label}.retarget_output_max_abs_translation_cm",
    )
    if not 0.0 <= max_abs <= 1000.0:
        raise CorrectionError(f"{label} max retarget translation is invalid: {max_abs}")
    anchors = payload.get("retarget_output_anchor_transforms")
    if not isinstance(anchors, dict) or set(anchors) != {
        "root",
        "pelvis",
        "spine_01",
        "head",
    }:
        raise CorrectionError(f"{label} retarget anchor set differs: {anchors!r}")
    root = anchor_translation(anchors, "root", label)
    pelvis = anchor_translation(anchors, "pelvis", label)
    spine = anchor_translation(anchors, "spine_01", label)
    head = anchor_translation(anchors, "head", label)
    if math.sqrt(sum(component * component for component in root)) > 0.10:
        raise CorrectionError(f"{label} named root moved away from zero: {root!r}")
    if abs(pelvis[2] - TARGET_PELVIS_Z_CM) > TARGET_PELVIS_Z_TOLERANCE_CM:
        raise CorrectionError(
            f"{label} pelvis height {pelvis[2]} does not restore {TARGET_PELVIS_Z_CM} cm"
        )
    if math.hypot(pelvis[0], pelvis[1]) > TARGET_PELVIS_HORIZONTAL_TOLERANCE_CM:
        raise CorrectionError(f"{label} pelvis horizontal offset is not neutral: {pelvis}")
    if not pelvis[2] < spine[2] < head[2]:
        raise CorrectionError(
            f"{label} pelvis/spine/head vertical continuity differs: "
            f"pelvis={pelvis}, spine={spine}, head={head}"
        )
    if not 0.1 <= spine[2] - pelvis[2] <= 25.0:
        raise CorrectionError(f"{label} pelvis-to-spine continuity is implausible")
    if not 20.0 <= head[2] - spine[2] <= 100.0:
        raise CorrectionError(f"{label} spine-to-head continuity is implausible")
    validate_corrected_pelvis_invariants(payload, label)


def validate_prepare(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "PREPARE_RETARGET_ROOT_MOTION_CORRECTION",
            "status": "PASS",
            "disk_mutation": "NONE_IN_MEMORY_ONLY",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "retarget_op_count": 5,
            "retarget_op_types": EXPECTED_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "root_motion_op_count": 1,
            "root_motion_op_index": 3,
            "root_motion_enabled": False,
            "root_motion_disabled_for_fixed_presentation": True,
            "root_motion_source_root_bone": "root",
            "root_motion_target_root_bone": "pelvis",
            "root_motion_target_pelvis_bone": "pelvis",
            "root_motion_source": "CopyFromSourceRoot",
            "root_motion_op_count_before_correction": 1,
            "root_motion_op_count_after_correction": 1,
            "root_motion_op_index_before_correction": 3,
            "root_motion_op_index_after_correction": 3,
            "root_motion_enabled_before_correction": True,
            "root_motion_enabled_after_correction": False,
            "retarget_op_enabled_mask_before_correction": (
                EXPECTED_OP_ENABLED_MASK_BEFORE
            ),
            "retarget_op_enabled_mask_after_correction": EXPECTED_OP_ENABLED_MASK,
            "retarget_runtime_policy_before_correction": LEGACY_POLICY,
            "retarget_runtime_policy_after_correction": EXPECTED_POLICY,
            "retarget_runtime_policy": EXPECTED_POLICY,
            "root_motion_correction_scope": (
                "UNIQUE_INDEX_3_ROOT_MOTION_OP_ENABLED_STATE_ONLY_VIA_CONTROLLER"
            ),
            "root_motion_toggle_via_controller_only": True,
            "root_motion_settings_verified_before_correction": True,
            "corrected_pelvis_numerical_invariants_verified": True,
            "root_motion_correction_prepared": True,
            "retarget_correction_prepared": True,
            "retargeter_dirty_for_exact_save": True,
        },
        "prepare helper",
    )
    validate_pose_evidence(payload, "prepare helper")


def validate_helper(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "VALIDATE",
            "status": "PASS",
            "disk_mutation": "NONE",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "reload_from_disk_verified": True,
            "canonical_packages_absent_before_reload": True,
            "reloaded_canonical_package_count": 3,
            "source_mhc_editor_only": True,
            "preferred_quality_profile_id": "GameplayPerformance",
            "retarget_op_count": 5,
            "retarget_op_types": EXPECTED_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "root_motion_op_count": 1,
            "root_motion_op_index": 3,
            "root_motion_enabled": False,
            "root_motion_disabled_for_fixed_presentation": True,
            "root_motion_source_root_bone": "root",
            "root_motion_target_root_bone": "pelvis",
            "root_motion_target_pelvis_bone": "pelvis",
            "root_motion_source": "CopyFromSourceRoot",
            "retarget_runtime_policy": EXPECTED_POLICY,
        },
        "validation helper",
    )
    validate_pose_evidence(payload, "validation helper")


def validate_counterfactual_evidence() -> dict[str, Any]:
    identity = record(COUNTERFACTUAL_REPORT)
    if identity != COUNTERFACTUAL_REPORT_RECORD:
        raise CorrectionError(
            f"accepted Root Motion counterfactual identity changed: {identity}"
        )
    payload = strict_json_text(
        COUNTERFACTUAL_REPORT.read_text(encoding="utf-8"),
        "accepted Root Motion counterfactual report",
    )
    require_exact(
        payload,
        {
            "schema": "DiscGolfTour.Session8BMetaHumanRootMotionEnvelope.v1",
            "status": "PASS_NO_WRITE",
            "phase": "COMPLETE",
            "canonical_disk_preserved": True,
            "dirty_package_baseline_preserved": True,
            "project_savegames_preserved": True,
            "external_user_dir_save_set_preserved": True,
            "errors": [],
        },
        "accepted Root Motion counterfactual report",
    )
    helper = payload.get("helper")
    if not isinstance(helper, dict):
        raise CorrectionError("accepted Root Motion counterfactual helper is absent")
    require_exact(
        helper,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "ROOT_MOTION_COUNTERFACTUAL_DIAGNOSTIC",
            "status": "PASS",
            "disk_mutation": "NONE",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "canonical_asset_mutation_attempted": False,
            "asset_save_attempted": False,
            "baseline_pelvis_near_zero": True,
            "root_off_pelvis_restores_target_retarget_pose_height": True,
            "root_off_pelvis_matches_expected_pelvis_motion": True,
            "pelvis_counterfactual_delta_exceeds_90_cm": True,
            "named_root_stable": True,
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
        "accepted Root Motion counterfactual helper",
    )
    deltas = helper.get("counterfactual_transform_deltas")
    if not isinstance(deltas, dict):
        raise CorrectionError("counterfactual transform deltas are absent")
    restoration = finite_number(
        deltas.get("pelvis_restoration_cm"), "counterfactual pelvis restoration"
    )
    restored_z = finite_number(
        deltas.get("root_off_pelvis_translation_z_cm"),
        "counterfactual restored pelvis height",
    )
    if restoration <= 90.0 or abs(restored_z - TARGET_PELVIS_Z_CM) > 0.25:
        raise CorrectionError("accepted Root Motion counterfactual geometry differs")
    return identity


def package_paths(asset_path: Path) -> list[Path]:
    base = asset_path.with_suffix("")
    return [asset_path, *(Path(f"{base}{suffix}") for suffix in PACKAGE_SUFFIXES[1:])]


def capture_package_state(
    asset_path: Path,
) -> dict[Path, tuple[bool, bytes | None, int | None]]:
    result: dict[Path, tuple[bool, bytes | None, int | None]] = {}
    for path in package_paths(asset_path):
        if path.is_file():
            result[path] = (True, path.read_bytes(), path.stat().st_mtime_ns)
        else:
            result[path] = (False, None, None)
    return result


def restore_package_state(
    package_state: dict[Path, tuple[bool, bytes | None, int | None]]
) -> list[str]:
    errors: list[str] = []
    for path, (existed, original_bytes, original_mtime_ns) in package_state.items():
        temporary = path.with_name(f"{path.name}.session8b-root-motion-rollback.tmp")
        try:
            if existed:
                if original_bytes is None or original_mtime_ns is None:
                    raise CorrectionError(f"rollback state is incomplete for {path}")
                if not path.is_file() or path.read_bytes() != original_bytes:
                    temporary.write_bytes(original_bytes)
                    temporary.replace(path)
                if path.stat().st_mtime_ns != original_mtime_ns:
                    os.utime(path, ns=(original_mtime_ns, original_mtime_ns))
            elif path.exists():
                if not path.is_file():
                    raise CorrectionError(f"rollback target is not a file: {path}")
                path.unlink()
        except Exception as exc:
            errors.append(f"rollback restore failed for {path}: {exc}")
        finally:
            if temporary.exists():
                try:
                    temporary.unlink()
                except OSError as exc:
                    errors.append(f"rollback temp cleanup failed for {temporary}: {exc}")
    return errors


def package_state_matches(
    package_state: dict[Path, tuple[bool, bytes | None, int | None]]
) -> bool:
    for path, (existed, original_bytes, original_mtime_ns) in package_state.items():
        if path.is_file() != existed:
            return False
        if existed and (
            path.read_bytes() != original_bytes
            or path.stat().st_mtime_ns != original_mtime_ns
        ):
            return False
    return True


def require_no_canonical_sidecars(
    project_root: Path, label: str
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for relative in (WRAPPER_RELATIVE, RETARGET_RELATIVE, PROFILE_RELATIVE):
        for sidecar in package_paths(project_root / relative)[1:]:
            identity = record(sidecar)
            result[sidecar.relative_to(project_root).as_posix()] = identity
            if identity["exists"]:
                raise CorrectionError(f"{label} canonical sidecar exists: {sidecar}")
    return result


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    report_path: Path | None = None
    report_writable = False
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "disk_mutation": "NONE",
        "errors": [],
        "writes": [],
        "attempted_writes": [],
        "asset_save_call_count": 0,
        "retarget_object": RETARGET_OBJECT,
        "legacy_policy": LEGACY_POLICY,
        "expected_policy": EXPECTED_POLICY,
        "expected_op_enabled_mask": EXPECTED_OP_ENABLED_MASK,
    }
    content_before: dict[str, dict[str, Any]] | None = None
    directories_before: set[str] | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    retarget_package_state: dict[
        Path, tuple[bool, bytes | None, int | None]
    ] | None = None
    retargeter: Any | None = None
    mutation_started = False
    user_dir: Path | None = None
    production: Path | None = None
    savegames_root: Path | None = None
    try:
        user_dir = validate_command_line(command_line, project_root)
        candidate_report_path = user_dir / REPORT_RELATIVE
        if candidate_report_path.exists():
            raise CorrectionError(
                f"fresh UUID already contains report: {candidate_report_path}"
            )
        if list(user_dir.rglob("*.sav")):
            raise CorrectionError("fresh UUID already contains a save file")
        report_path = candidate_report_path
        report_writable = True
        state["external_user_dir"] = str(user_dir)
        state["report_path"] = str(report_path)

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise CorrectionError(
                f"commandlet began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        counterfactual_identity = validate_counterfactual_evidence()
        content_before = snapshot(content_root)
        observed_summary = snapshot_summary(content_before)
        if observed_summary != CONTENT_BEFORE:
            raise CorrectionError(
                f"canonical Content baseline changed: {observed_summary}"
            )
        directories_before = directory_snapshot(content_root)
        if len(directories_before) != CONTENT_DIRECTORY_COUNT:
            raise CorrectionError(
                f"Content directory count is {len(directories_before)}, "
                f"expected {CONTENT_DIRECTORY_COUNT}"
            )
        for relative, expected in CANONICAL_BEFORE.items():
            if record(project_root / relative) != expected:
                raise CorrectionError(f"canonical baseline changed: {relative}")
        sidecars_before = require_no_canonical_sidecars(project_root, "before")
        retarget_path = project_root / RETARGET_RELATIVE
        retarget_package_state = capture_package_state(retarget_path)
        retarget_mtime_ns_before = retarget_path.stat().st_mtime_ns

        production = project_root / PRODUCTION_SAVE_RELATIVE
        savegames_root = production.parent
        savegames_before = snapshot(savegames_root)
        savegame_directories_before = directory_snapshot(savegames_root)
        if savegames_before != {production.name: ACCEPTED_SAVE}:
            raise CorrectionError("project SaveGames baseline differs")
        if savegame_directories_before:
            raise CorrectionError("project SaveGames unexpectedly contains directories")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise CorrectionError("accepted backup identity changed")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise CorrectionError("production save and accepted backup differ")
        state["before"] = {
            "content": CONTENT_BEFORE,
            "content_directory_count": len(directories_before),
            "canonical_assets": CANONICAL_BEFORE,
            "canonical_sidecars": sidecars_before,
            "retargeter_mtime_ns": retarget_mtime_ns_before,
            "project_savegames": savegames_before,
            "accepted_backup": record(ACCEPTED_BACKUP),
            "accepted_root_motion_counterfactual": counterfactual_identity,
        }

        state["phase"] = "PREPARE_EXACT_ROOT_MOTION_CHANGE"
        mutation_started = True
        prepare_payload = strict_json_text(
            resolve_prepare_helper()(), "C++ Root Motion prepare helper"
        )
        state["prepare_result"] = prepare_payload
        validate_prepare(prepare_payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content != [RETARGET_PACKAGE] or dirty_maps:
            raise CorrectionError(
                f"prepare dirtied unexpected packages: content={dirty_content}, "
                f"maps={dirty_maps}"
            )

        state["phase"] = "EXACT_RETARGETER_SAVE"
        retargeter = unreal.EditorAssetLibrary.load_asset(RETARGET_OBJECT)
        if retargeter is None or str(retargeter.get_path_name()) != RETARGET_OBJECT:
            raise CorrectionError("canonical retargeter did not load at the exact path")
        state["attempted_writes"] = [RETARGET_OBJECT]
        state["asset_save_call_count"] = 1
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            retargeter, only_if_is_dirty=True
        ):
            raise CorrectionError("UE refused the exact canonical retargeter save")
        state["writes"] = [RETARGET_OBJECT]
        retargeter = None

        state["phase"] = "UNLOAD_GC_RELOAD_STRICT_VALIDATE"
        unreal.SystemLibrary.collect_garbage()
        helper_payload = strict_json_text(
            resolve_validate_helper()(), "C++ strict reload validation helper"
        )
        state["helper_result"] = helper_payload
        validate_helper(helper_payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise CorrectionError(
                f"dirty packages remain: content={dirty_content}, maps={dirty_maps}"
            )

        content_after = snapshot(content_root)
        directories_after = directory_snapshot(content_root)
        changed = sorted(
            key
            for key in set(content_before) & set(content_after)
            if content_before[key] != content_after[key]
        )
        added = sorted(set(content_after) - set(content_before))
        removed = sorted(set(content_before) - set(content_after))
        expected_changed = [RETARGET_RELATIVE.relative_to("Content").as_posix()]
        if changed != expected_changed or added or removed:
            raise CorrectionError(
                f"Content delta differs: changed={changed}, added={added}, "
                f"removed={removed}"
            )
        if directories_after != directories_before:
            raise CorrectionError("Content directory topology changed")
        retarget_before_record = CANONICAL_BEFORE[RETARGET_RELATIVE.as_posix()]
        if record(retarget_path) == retarget_before_record:
            raise CorrectionError("retargeter bytes did not change")
        for relative, expected in CANONICAL_BEFORE.items():
            if relative != RETARGET_RELATIVE.as_posix():
                if record(project_root / relative) != expected:
                    raise CorrectionError(
                        f"non-retarget canonical asset changed: {relative}"
                    )
        sidecars_after = require_no_canonical_sidecars(project_root, "after")
        if sidecars_after != sidecars_before:
            raise CorrectionError("canonical sidecar identity set changed")

        savegames_after = snapshot(savegames_root)
        savegame_directories_after = directory_snapshot(savegames_root)
        external_saves = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav")
        )
        if (
            savegames_after != savegames_before
            or savegame_directories_after != savegame_directories_before
            or external_saves
            or record(ACCEPTED_BACKUP) != ACCEPTED_SAVE
            or production.read_bytes() != ACCEPTED_BACKUP.read_bytes()
            or validate_counterfactual_evidence() != counterfactual_identity
        ):
            raise CorrectionError(
                "protected save, backup, or counterfactual evidence boundary changed"
            )

        state.update(
            {
                "status": "PASS_RETARGET_ROOT_MOTION_DISABLED_AND_RELOAD_VALIDATED",
                "phase": "COMPLETE",
                "disk_mutation": "EXACT_ONE_RETARGETER_WRITE",
                "content_before": CONTENT_BEFORE,
                "content_after": snapshot_summary(content_after),
                "content_changed": changed,
                "content_added": added,
                "content_removed": removed,
                "content_directories_unchanged": True,
                "retargeter_before": retarget_before_record,
                "retargeter_before_mtime_ns": retarget_mtime_ns_before,
                "retargeter_after": record(retarget_path),
                "retargeter_after_mtime_ns": retarget_path.stat().st_mtime_ns,
                "wrapper_unchanged": True,
                "profile_unchanged": True,
                "canonical_sidecars_unchanged": True,
                "project_savegames_unchanged": True,
                "production_save_unchanged": True,
                "accepted_backup_unchanged": True,
                "accepted_root_motion_counterfactual_unchanged": True,
                "external_save_files": external_saves,
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback_verified = False
        rollback_errors: list[str] = []
        rollback_package_state_restored = False
        if (
            mutation_started
            and content_before is not None
            and directories_before is not None
            and retarget_package_state is not None
            and production is not None
            and savegames_root is not None
            and savegames_before is not None
            and savegame_directories_before is not None
            and user_dir is not None
        ):
            try:
                retargeter = None
                unreal.SystemLibrary.collect_garbage()
            except Exception as garbage_error:
                rollback_errors.append(
                    f"rollback garbage collection failed: {garbage_error}"
                )
            rollback_errors.extend(restore_package_state(retarget_package_state))
            try:
                rollback_package_state_restored = package_state_matches(
                    retarget_package_state
                )
                if not rollback_package_state_restored:
                    rollback_errors.append(
                        "retarget package bytes, mtimes, or sidecars did not restore"
                    )
                rollback_dirty_content, rollback_dirty_maps = dirty_names()
                rollback_verified = (
                    not rollback_errors
                    and rollback_package_state_restored
                    and not rollback_dirty_content
                    and not rollback_dirty_maps
                    and snapshot(content_root) == content_before
                    and directory_snapshot(content_root) == directories_before
                    and snapshot(savegames_root) == savegames_before
                    and directory_snapshot(savegames_root)
                    == savegame_directories_before
                    and record(ACCEPTED_BACKUP) == ACCEPTED_SAVE
                    and production.read_bytes() == ACCEPTED_BACKUP.read_bytes()
                    and record(COUNTERFACTUAL_REPORT)
                    == COUNTERFACTUAL_REPORT_RECORD
                    and not list(user_dir.rglob("*.sav"))
                    and all(
                        not sidecar.exists()
                        for relative in (
                            WRAPPER_RELATIVE,
                            RETARGET_RELATIVE,
                            PROFILE_RELATIVE,
                        )
                        for sidecar in package_paths(project_root / relative)[1:]
                    )
                )
                if rollback_dirty_content or rollback_dirty_maps:
                    rollback_errors.append(
                        "rollback left dirty packages: "
                        f"content={rollback_dirty_content}, maps={rollback_dirty_maps}"
                    )
                    rollback_verified = False
            except Exception as verification_error:
                rollback_errors.append(
                    f"rollback verification failed: {verification_error}"
                )
                rollback_verified = False
        state["rollback_errors"] = rollback_errors
        state["errors"].extend(rollback_errors)
        state["rollback_retarget_package_state_restored"] = (
            rollback_package_state_restored
        )
        state["rollback_verified"] = rollback_verified
        state["disk_mutation"] = (
            "ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
            if rollback_verified
            else ("ROLLBACK_INCOMPLETE" if mutation_started else "NONE")
        )
        state["status"] = (
            "FAIL_ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
            if rollback_verified
            else (
                "FAIL_ROLLBACK_INCOMPLETE"
                if mutation_started
                else "FAIL_BEFORE_MUTATION_NO_PERSISTENT_ASSET_WRITES"
            )
        )
        state["failed_utc"] = utc_now()
        if report_writable and report_path is not None:
            try:
                write_report(report_path, state)
            except Exception as report_error:
                unreal.log_error(
                    "DG_SESSION8B_RETARGET_ROOT_MOTION_CORRECTION: report write failed: "
                    f"{report_error}"
                )
        unreal.log_error(
            "DG_SESSION8B_RETARGET_ROOT_MOTION_CORRECTION: "
            f"{state['status']} phase={state['phase']} "
            f"report={report_path if report_path is not None else '<none>'}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_RETARGET_ROOT_MOTION_CORRECTION: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
