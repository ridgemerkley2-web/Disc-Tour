"""Correct only the canonical Session 8B wrapper's legacy +90 yaw defaults."""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanWrapperOrientationCorrection.v1"
CORRECT_SWITCH = "-DGSession8BMetaHumanWrapperOrientationCorrection"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanWrapperOrientationCorrection.json"
)
WRAPPER_PACKAGE = "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default"
WRAPPER_OBJECT = f"{WRAPPER_PACKAGE}.BP_DG_MetaHuman_Default"
WRAPPER_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset"
)
RETARGET_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
)
PROFILE_RELATIVE = Path(
    "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
)
LEGACY_POLICY = "DG_SOURCE_MINUS_90_YAW_COMPENSATED_BY_WRAPPER_PLUS_90_YAW_V1"
EXPECTED_POLICY = "DG_SOURCE_AUTHORITY_WRAPPER_IDENTITY_RELATIVE_TRANSFORM_V2"
CANONICAL_SEMANTICS = (
    "WRAPPER_COLOCATED_WITH_DG_ANIMATION_SOURCE;NO_RELATIVE_YAW;"
    "SOURCE_DISC_GRIP_REMAINS_AUTHORITATIVE"
)
CORRECTED_GRAPHS = [
    "ConfigureFromDGAnimationSource",
    "ApplyMappedCustomization",
]
LEGACY_TRANSFORM = {
    "translation_x_cm": 0.0,
    "translation_y_cm": 0.0,
    "translation_z_cm": 0.0,
    "rotation_pitch_degrees": 0.0,
    "rotation_yaw_degrees": 90.0,
    "rotation_roll_degrees": 0.0,
    "scale_x": 1.0,
    "scale_y": 1.0,
    "scale_z": 1.0,
}
IDENTITY_TRANSFORM = {
    **LEGACY_TRANSFORM,
    "rotation_yaw_degrees": 0.0,
}
IDENTITY_ROTATION = {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}
EXPECTED_RETARGET_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"
EXPECTED_RETARGET_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_RETARGET_OP_ENABLED_MASK = [True, True, False, True, True]
CONTENT_BEFORE = {
    "file_count": 953,
    "bytes": 2920024342,
    "manifest_sha256": (
        "56DF1ED51ED32858141B6312BCD43E81E260C215137946E1F5395CED61762C0B"
    ),
}
CONTENT_DIRECTORY_COUNT = 230
CANONICAL_BEFORE = {
    str(WRAPPER_RELATIVE).replace("\\", "/"): {
        "exists": True,
        "bytes": 225470,
        "sha256": (
            "72704ECC288B7D0917C6062198B3B5161B966244A5A249632021A847BE253634"
        ),
    },
    str(RETARGET_RELATIVE).replace("\\", "/"): {
        "exists": True,
        "bytes": 22640,
        "sha256": (
            "6316D573F271EE40BD7D3C42258723F9C8669A5606A0FB0373016DF3EBFBEAE9"
        ),
    },
    str(PROFILE_RELATIVE).replace("\\", "/"): {
        "exists": True,
        "bytes": 2274,
        "sha256": (
            "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D"
        ),
    },
}
WRAPPER_MTIME_NS_BEFORE = 1786994918684754000
FRESH_VALIDATION_REPORT = Path(
    r"C:\DGTour_TestRuns\Session8B\dc8962f0-bc36-409b-853e-54dfee62f75c"
    r"\Saved\CharacterFramework\Session8BMetaHumanRuntimeAssetValidation.json"
)
FRESH_VALIDATION_REPORT_RECORD = {
    "exists": True,
    "bytes": 11215,
    "sha256": (
        "CD3BD3D09C748D6E907287129939D91033299E89FEE4D186B458839F7AC82FA8"
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


def write_report(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f"{path.name}.tmp")
    try:
        temp.write_text(
            json.dumps(state, indent=2, sort_keys=True) + "\n",
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

    try:
        value = json.loads(text, object_pairs_hook=no_duplicates)
    except json.JSONDecodeError as exc:
        raise CorrectionError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise CorrectionError(f"{label} JSON root is not an object")
    return value


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(
        re.search(rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line)
    )


def command_value(command_line: str, name: str) -> str:
    match = re.search(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))',
        command_line,
    )
    return (match.group(1) or match.group(2)) if match else ""


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (CORRECT_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise CorrectionError(f"required command-line switches absent: {missing}")
    run_tokens = re.findall(r"(?i)(?:^|\s)-run=pythonscript(?=$|\s)", command_line)
    if len(run_tokens) != 1 or re.search(
        r"(?i)(?:^|\s)-executepythonscript(?:=|\s|$)", command_line
    ):
        raise CorrectionError("correction requires exactly one PythonScript commandlet")
    user_dir_text = command_value(command_line, "UserDir")
    if not user_dir_text:
        raise CorrectionError("absolute external -UserDir is required")
    user_dir = Path(user_dir_text).resolve()
    parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(parent)
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
            "prepare_session8_b_meta_human_wrapper_orientation_correction",
            "prepare_session8b_meta_human_wrapper_orientation_correction",
        ),
        "wrapper-orientation prepare helper",
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
    for key, value in expected.items():
        if payload.get(key) != value:
            raise CorrectionError(
                f"{label} field {key}={payload.get(key)!r}, expected {value!r}"
            )


def validate_numeric_mapping(
    value: Any,
    expected: dict[str, float],
    label: str,
    tolerance: float = 1.0e-9,
) -> None:
    if not isinstance(value, dict) or set(value) != set(expected):
        raise CorrectionError(
            f"{label} key set differs: "
            f"{sorted(value) if isinstance(value, dict) else value!r}"
        )
    for key, expected_value in expected.items():
        observed = value[key]
        if (
            isinstance(observed, bool)
            or not isinstance(observed, (int, float))
            or not math.isfinite(float(observed))
            or abs(float(observed) - expected_value) > tolerance
        ):
            raise CorrectionError(
                f"{label}.{key}={observed!r}, expected {expected_value!r} "
                f"within {tolerance}"
            )


def validate_orientation_contract(payload: dict[str, Any], label: str) -> None:
    require_exact(
        payload,
        {
            "orientation_policy": EXPECTED_POLICY,
            "orientation_canonical_semantics": CANONICAL_SEMANTICS,
            "orientation_requires_runtime_visual_validation": True,
            "wrapper_attachment_relative_transform_is_identity": True,
            "fixed_preset_semantics": True,
            "wrapper_contract_verified": True,
        },
        label,
    )
    validate_numeric_mapping(
        payload.get("wrapper_attachment_relative_transform"),
        IDENTITY_TRANSFORM,
        f"{label}.wrapper_attachment_relative_transform",
    )
    validate_numeric_mapping(
        payload.get("wrapper_relative_translation_cm"),
        {"x": 0.0, "y": 0.0, "z": 0.0},
        f"{label}.wrapper_relative_translation_cm",
    )
    validate_numeric_mapping(
        payload.get("wrapper_relative_rotation_degrees"),
        IDENTITY_ROTATION,
        f"{label}.wrapper_relative_rotation_degrees",
    )
    validate_numeric_mapping(
        payload.get("wrapper_relative_scale"),
        {"x": 1.0, "y": 1.0, "z": 1.0},
        f"{label}.wrapper_relative_scale",
    )


def validate_pose_evidence(payload: dict[str, Any], label: str) -> None:
    if payload.get("retarget_processor_initialized") is not True:
        raise CorrectionError(f"{label} did not initialize the retarget processor")
    if payload.get("retarget_output_bone_count") != 342:
        raise CorrectionError(
            f"{label} retarget output bone count differs: "
            f"{payload.get('retarget_output_bone_count')!r}"
        )
    if payload.get("retarget_output_pose_plausible") is not True:
        raise CorrectionError(f"{label} did not prove a plausible retarget output pose")
    if payload.get("retarget_output_translation_outlier_count") != 0:
        raise CorrectionError(f"{label} reported retarget translation outliers")
    max_abs = payload.get("retarget_output_max_abs_translation_cm")
    if (
        isinstance(max_abs, bool)
        or not isinstance(max_abs, (int, float))
        or not math.isfinite(float(max_abs))
        or not 0 <= float(max_abs) <= 1000
    ):
        raise CorrectionError(
            f"{label} max retarget translation is invalid: {max_abs!r}"
        )
    anchors = payload.get("retarget_output_anchor_transforms")
    if not isinstance(anchors, dict) or set(anchors) != {
        "root",
        "pelvis",
        "spine_01",
        "head",
    }:
        raise CorrectionError(f"{label} retarget anchor set differs: {anchors!r}")


def validate_prepare(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "PREPARE_WRAPPER_ORIENTATION_CORRECTION",
            "status": "PASS",
            "disk_mutation": "NONE_IN_MEMORY_ONLY",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "orientation_policy_before_correction": LEGACY_POLICY,
            "orientation_policy_after_correction": EXPECTED_POLICY,
            "orientation_correction_scope": (
                "CONFIGURE_AND_APPLY_MAKE_TRANSFORM_ROTATION_DEFAULTS_ONLY"
            ),
            "exact_legacy_wrapper_orientation_verified": True,
            "exact_legacy_wrapper_orientation_graphs_verified": CORRECTED_GRAPHS,
            "wrapper_orientation_graphs_corrected": CORRECTED_GRAPHS,
            "wrapper_rotation_default_change_count": 2,
            "wrapper_orientation_correction_prepared": True,
            "wrapper_dirty_for_exact_save": True,
            "retarget_op_count": 5,
            "retarget_op_types": EXPECTED_RETARGET_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_RETARGET_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "retarget_runtime_policy": EXPECTED_RETARGET_POLICY,
            "preferred_quality_profile_id": "GameplayPerformance",
        },
        "prepare helper",
    )
    validate_numeric_mapping(
        payload.get("wrapper_attachment_relative_transform_before_correction"),
        LEGACY_TRANSFORM,
        "prepare helper.wrapper_attachment_relative_transform_before_correction",
    )
    validate_numeric_mapping(
        payload.get("wrapper_attachment_relative_transform_after_correction"),
        IDENTITY_TRANSFORM,
        "prepare helper.wrapper_attachment_relative_transform_after_correction",
    )
    validate_orientation_contract(payload, "prepare helper")
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
            "retarget_op_types": EXPECTED_RETARGET_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_RETARGET_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "retarget_runtime_policy": EXPECTED_RETARGET_POLICY,
        },
        "validation helper",
    )
    validate_orientation_contract(payload, "validation helper")
    validate_pose_evidence(payload, "validation helper")


def package_sidecars(asset_path: Path) -> list[Path]:
    base = asset_path.with_suffix("")
    return [Path(f"{base}{suffix}") for suffix in PACKAGE_SUFFIXES[1:]]


def require_no_canonical_sidecars(
    project_root: Path, label: str
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for relative in (WRAPPER_RELATIVE, RETARGET_RELATIVE, PROFILE_RELATIVE):
        asset_path = project_root / relative
        for sidecar in package_sidecars(asset_path):
            identity = record(sidecar)
            result[sidecar.relative_to(project_root).as_posix()] = identity
            if identity["exists"]:
                raise CorrectionError(f"{label} canonical sidecar exists: {sidecar}")
    return result


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    invalid_report = (
        Path(tempfile.gettempdir()) / "Session8BWrapperOrientation.invalid.json"
    )
    report_path = invalid_report
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
        "wrapper_object": WRAPPER_OBJECT,
        "legacy_policy": LEGACY_POLICY,
        "expected_policy": EXPECTED_POLICY,
        "orientation_canonical_semantics": CANONICAL_SEMANTICS,
        "legacy_transform": LEGACY_TRANSFORM,
        "expected_transform": IDENTITY_TRANSFORM,
    }
    content_before: dict[str, dict[str, Any]] | None = None
    directories_before: set[str] | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    wrapper_bytes_before: bytes | None = None
    wrapper_mtime_ns_before: int | None = None
    wrapper: Any | None = None
    mutation_started = False
    user_dir: Path | None = None
    production: Path | None = None
    savegames_root: Path | None = None
    try:
        user_dir = validate_command_line(command_line, project_root)
        report_path = user_dir / REPORT_RELATIVE
        if report_path.exists():
            raise CorrectionError(f"fresh UUID already contains report: {report_path}")
        if list(user_dir.rglob("*.sav")):
            raise CorrectionError("fresh UUID already contains a save file")
        state["external_user_dir"] = str(user_dir)
        state["report_path"] = str(report_path)

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise CorrectionError(
                f"commandlet began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        if record(FRESH_VALIDATION_REPORT) != FRESH_VALIDATION_REPORT_RECORD:
            raise CorrectionError("accepted Fresh3 validation report identity changed")
        content_before = snapshot(content_root)
        if snapshot_summary(content_before) != CONTENT_BEFORE:
            raise CorrectionError(
                f"canonical Content baseline changed: {snapshot_summary(content_before)}"
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

        wrapper_path = project_root / WRAPPER_RELATIVE
        wrapper_bytes_before = wrapper_path.read_bytes()
        wrapper_mtime_ns_before = wrapper_path.stat().st_mtime_ns
        if wrapper_mtime_ns_before != WRAPPER_MTIME_NS_BEFORE:
            raise CorrectionError(
                "canonical wrapper mtime changed: "
                f"{wrapper_mtime_ns_before}, expected {WRAPPER_MTIME_NS_BEFORE}"
            )

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
            "wrapper": record(wrapper_path),
            "wrapper_mtime_ns": wrapper_mtime_ns_before,
            "canonical_assets": CANONICAL_BEFORE,
            "canonical_sidecars": sidecars_before,
            "project_savegames": savegames_before,
            "accepted_backup": record(ACCEPTED_BACKUP),
            "accepted_fresh_validation": record(FRESH_VALIDATION_REPORT),
        }

        state["phase"] = "PREPARE_EXACT_WRAPPER_ORIENTATION_CHANGE"
        mutation_started = True
        prepare_payload = strict_json_text(
            resolve_prepare_helper()(), "C++ wrapper-orientation prepare helper"
        )
        state["prepare_result"] = prepare_payload
        validate_prepare(prepare_payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content != [WRAPPER_PACKAGE] or dirty_maps:
            raise CorrectionError(
                f"prepare dirtied unexpected packages: content={dirty_content}, "
                f"maps={dirty_maps}"
            )

        state["phase"] = "EXACT_WRAPPER_SAVE"
        wrapper = unreal.EditorAssetLibrary.load_asset(WRAPPER_OBJECT)
        if wrapper is None or str(wrapper.get_path_name()) != WRAPPER_OBJECT:
            raise CorrectionError("canonical wrapper did not load at the exact path")
        state["attempted_writes"] = [WRAPPER_OBJECT]
        state["asset_save_call_count"] = 1
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            wrapper, only_if_is_dirty=True
        ):
            raise CorrectionError("UE refused the exact canonical wrapper save")
        state["writes"] = [WRAPPER_OBJECT]
        wrapper = None

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
        expected_changed = [WRAPPER_RELATIVE.relative_to("Content").as_posix()]
        if changed != expected_changed or added or removed:
            raise CorrectionError(
                f"Content delta differs: changed={changed}, added={added}, "
                f"removed={removed}"
            )
        if directories_after != directories_before:
            raise CorrectionError("Content directory topology changed")
        wrapper_before_record = CANONICAL_BEFORE[
            str(WRAPPER_RELATIVE).replace("\\", "/")
        ]
        if record(wrapper_path) == wrapper_before_record:
            raise CorrectionError("wrapper bytes did not change")
        for relative, expected in CANONICAL_BEFORE.items():
            if relative != str(WRAPPER_RELATIVE).replace("\\", "/"):
                if record(project_root / relative) != expected:
                    raise CorrectionError(
                        f"non-wrapper canonical asset changed: {relative}"
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
            or record(FRESH_VALIDATION_REPORT) != FRESH_VALIDATION_REPORT_RECORD
        ):
            raise CorrectionError("protected save or external evidence boundary changed")

        state.update(
            {
                "status": "PASS_WRAPPER_ORIENTATION_IDENTITY_AND_RELOAD_VALIDATED",
                "phase": "COMPLETE",
                "disk_mutation": "EXACT_ONE_WRAPPER_WRITE",
                "content_before": CONTENT_BEFORE,
                "content_after": snapshot_summary(content_after),
                "content_changed": changed,
                "content_added": added,
                "content_removed": removed,
                "content_directories_unchanged": True,
                "wrapper_before": wrapper_before_record,
                "wrapper_before_mtime_ns": wrapper_mtime_ns_before,
                "wrapper_after": record(wrapper_path),
                "wrapper_after_mtime_ns": wrapper_path.stat().st_mtime_ns,
                "retargeter_unchanged": True,
                "profile_unchanged": True,
                "canonical_sidecars_unchanged": True,
                "project_savegames_unchanged": True,
                "production_save_unchanged": True,
                "accepted_backup_unchanged": True,
                "accepted_fresh_validation_unchanged": True,
                "external_save_files": external_saves,
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback_verified = False
        rollback_errors: list[str] = []
        rollback_bytes_restored = False
        rollback_mtime_restored = False
        if (
            mutation_started
            and content_before is not None
            and directories_before is not None
            and wrapper_bytes_before is not None
            and wrapper_mtime_ns_before is not None
            and production is not None
            and savegames_root is not None
            and savegames_before is not None
            and savegame_directories_before is not None
            and user_dir is not None
        ):
            wrapper_path = project_root / WRAPPER_RELATIVE
            try:
                wrapper = None
                unreal.SystemLibrary.collect_garbage()
            except Exception as garbage_error:
                rollback_errors.append(
                    f"rollback garbage collection failed: {garbage_error}"
                )
            restore_temp = wrapper_path.with_name(
                f"{wrapper_path.name}.session8b-orientation-rollback.tmp"
            )
            try:
                disk_bytes_changed = (
                    not wrapper_path.is_file()
                    or wrapper_path.read_bytes() != wrapper_bytes_before
                )
                if disk_bytes_changed:
                    restore_temp.write_bytes(wrapper_bytes_before)
                    restore_temp.replace(wrapper_path)
                if (
                    disk_bytes_changed
                    or wrapper_path.stat().st_mtime_ns != wrapper_mtime_ns_before
                ):
                    os.utime(
                        wrapper_path,
                        ns=(wrapper_mtime_ns_before, wrapper_mtime_ns_before),
                    )
                rollback_mtime_restored = (
                    wrapper_path.stat().st_mtime_ns == wrapper_mtime_ns_before
                )
                rollback_bytes_restored = (
                    wrapper_path.read_bytes() == wrapper_bytes_before
                )
                if not rollback_bytes_restored:
                    rollback_errors.append("wrapper byte rollback did not match baseline")
                if not rollback_mtime_restored:
                    rollback_errors.append("wrapper mtime rollback did not match baseline")
                for sidecar in package_sidecars(wrapper_path):
                    if sidecar.exists():
                        sidecar.unlink()
            except OSError as rollback_error:
                rollback_errors.append(f"rollback file restore failed: {rollback_error}")
            finally:
                if restore_temp.exists():
                    try:
                        restore_temp.unlink()
                    except OSError as cleanup_error:
                        rollback_errors.append(
                            f"rollback temp cleanup failed: {cleanup_error}"
                        )
            try:
                rollback_verified = (
                    not rollback_errors
                    and rollback_bytes_restored
                    and rollback_mtime_restored
                    and snapshot(content_root) == content_before
                    and directory_snapshot(content_root) == directories_before
                    and snapshot(savegames_root) == savegames_before
                    and directory_snapshot(savegames_root)
                    == savegame_directories_before
                    and record(ACCEPTED_BACKUP) == ACCEPTED_SAVE
                    and production.read_bytes() == ACCEPTED_BACKUP.read_bytes()
                    and record(FRESH_VALIDATION_REPORT)
                    == FRESH_VALIDATION_REPORT_RECORD
                    and not list(user_dir.rglob("*.sav"))
                    and all(
                        not sidecar.exists()
                        for relative in (
                            WRAPPER_RELATIVE,
                            RETARGET_RELATIVE,
                            PROFILE_RELATIVE,
                        )
                        for sidecar in package_sidecars(project_root / relative)
                    )
                )
            except Exception as verification_error:
                rollback_errors.append(
                    f"rollback verification failed: {verification_error}"
                )
                rollback_verified = False
        state["rollback_errors"] = rollback_errors
        state["errors"].extend(rollback_errors)
        state["rollback_wrapper_bytes_restored"] = rollback_bytes_restored
        state["rollback_wrapper_mtime_restored"] = rollback_mtime_restored
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
        try:
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                "DG_SESSION8B_WRAPPER_ORIENTATION_CORRECTION: report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            "DG_SESSION8B_WRAPPER_ORIENTATION_CORRECTION: "
            f"{state['status']} phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_WRAPPER_ORIENTATION_CORRECTION: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
