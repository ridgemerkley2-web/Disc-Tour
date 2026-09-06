"""Fresh-process, no-write validation for Session 8B canonical runtime assets."""

from __future__ import annotations

import hashlib
import json
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanRuntimeAssetValidation.v2"
VALIDATE_SWITCH = "-DGSession8BMetaHumanRuntimeAssetsValidate"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanRuntimeAssetValidation.json"
)
SOURCE_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default.uasset"
)
SOURCE_BYTES = 112982127
SOURCE_SHA256 = "1AA1EFBFDD882308959D1429D562DD2D92F64317219660003063D0AEF17BCFAB"
ASSEMBLED_CONTENT_FILE_COUNT = 950
ASSEMBLED_CONTENT_BYTES = 2919773958
ASSEMBLED_CONTENT_MANIFEST_SHA256 = (
    "CFA6EE7443BA141C770E02052460BE69989941C3F924F4FF54FA26B27D802E22"
)
COOK_MANIFEST_RELATIVE = "DiscGolf/Cook/DA_DG_RuntimeCookManifest.uasset"
COOK_MANIFEST_RECORD = {
    "exists": True,
    "bytes": 121198,
    "sha256": "438CA66DEC96AB7F7A19AC551C5FCFE9AEE6DDCDCC8E8432B475F0FD85AF30BE",
}
ASSEMBLY_WITHOUT_COOK_MANIFEST = {
    "file_count": 949,
    "bytes": 2919652760,
    "manifest_sha256": (
        "658627B46C343E83308D1407FC33F9F6518D9F5149E832AD754FB9183F4DDBFE"
    ),
}
PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE = {
    "exists": True,
    "bytes": 5212,
    "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
}
CANONICAL_OBJECTS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default",
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman",
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default",
)
EXPECTED_CHAIN_MAPPING = {
    "Head": "Neck",
    "LeftArm": "Arm_L",
    "LeftLeg": "Leg_L",
    "Neck": "Neck",
    "RightArm": "Arm_R",
    "RightLeg": "Leg_R",
    "Root": "Root",
    "Spine": "Spine",
}
EXPECTED_RETARGET_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_RETARGET_OP_ENABLED_MASK = [True, True, False, False, True]
EXPECTED_RETARGET_POLICY = (
    "PELVIS_FK_CURVES_ENABLED_RUN_IK_AND_ROOT_MOTION_DISABLED"
)
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")


class ValidationError(RuntimeError):
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
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file())
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


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValidationError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    try:
        value = json.loads(text, object_pairs_hook=no_duplicates)
    except json.JSONDecodeError as exc:
        raise ValidationError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise ValidationError(f"{label} JSON root is not an object")
    return value


def write_report(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(state, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(re.search(rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line))


def command_value(command_line: str, name: str) -> str:
    match = re.search(rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))', command_line)
    return (match.group(1) or match.group(2)) if match else ""


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (VALIDATE_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise ValidationError(f"required command-line switches absent: {missing}")
    if "-run=pythonscript" not in command_line.casefold():
        raise ValidationError("runtime-asset validator must use PythonScript commandlet")
    user_dir_text = command_value(command_line, "UserDir")
    if not user_dir_text:
        raise ValidationError("absolute external -UserDir is required")
    user_dir = Path(user_dir_text).resolve()
    parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(parent)
    except ValueError as exc:
        raise ValidationError(f"UserDir is outside Session8B test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise ValidationError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise ValidationError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise ValidationError("UserDir UUID is not canonical")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise ValidationError("UserDir must be external to the project")
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


def resolve_validate_method() -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    names = (
        "validate_session8_b_meta_human_assets",
        "validate_session8b_meta_human_assets",
    )
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise ValidationError(f"expected exactly one helper spelling, got {matches}")
    return getattr(utility, matches[0])


def validate_helper(payload: dict[str, Any]) -> None:
    exact = {
        "schema_version": 2,
        "session": "8B",
        "operation": "VALIDATE",
        "status": "PASS",
        "disk_mutation": "NONE",
        "writes": [],
        "attempted_writes": [],
        "chain_mapping": EXPECTED_CHAIN_MAPPING,
        "tagged_body_count": 1,
        "tagged_head_count": 1,
        "metahuman_component_count": 1,
        "wrapper_contract_verified": True,
        "retarget_processor_initialized": True,
        "retarget_op_count": 5,
        "retarget_op_types": EXPECTED_RETARGET_OP_TYPES,
        "retarget_op_enabled_mask": EXPECTED_RETARGET_OP_ENABLED_MASK,
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
        "retarget_runtime_policy": EXPECTED_RETARGET_POLICY,
        "retarget_pelvis_motion_neutral_settings_confirmed": True,
        "retarget_target_pose_pelvis_near_93_164_cm": True,
        "retarget_output_pelvis_restores_target_pose_height": True,
        "retarget_output_pelvis_matches_expected_pelvis_motion": True,
        "retarget_output_pose_plausible": True,
        "retarget_output_translation_outlier_count": 0,
        "profile_contract_verified": True,
        "preferred_quality_profile_id": "GameplayPerformance",
        "assembly_pipeline": "UE_OPTIMIZED",
        "assembly_optimization_level": "MEDIUM",
        "fixed_preset_semantics": True,
        "clean_shaven": True,
        "beard_groom_asset_count": 0,
        "mustache_groom_asset_count": 0,
        "hair_s_clean_present": True,
        "outfit_present": True,
        "source_mhc_editor_only": True,
        "rollback_verified": False,
        "rollback_performed": False,
        "reload_from_disk_verified": True,
        "errors": [],
    }
    for key, expected in exact.items():
        if payload.get(key) != expected:
            raise ValidationError(
                f"helper field {key}={payload.get(key)!r}, expected {expected!r}"
            )
    max_abs = payload.get("retarget_output_max_abs_translation_cm")
    if not isinstance(max_abs, (int, float)) or not 0 <= float(max_abs) <= 1000:
        raise ValidationError(
            f"helper retarget_output_max_abs_translation_cm is invalid: {max_abs!r}"
        )
    pelvis_exact = {
        "retarget_target_pose_pelvis_expected_z_cm": 93.164,
        "retarget_target_pose_pelvis_z_tolerance_cm": 0.25,
        "retarget_output_pelvis_height_tolerance_cm": 0.10,
        "retarget_output_pelvis_motion_tolerance_cm": 0.10,
    }
    for key, expected in pelvis_exact.items():
        if payload.get(key) != expected:
            raise ValidationError(
                f"helper field {key}={payload.get(key)!r}, expected {expected!r}"
            )
    target_pelvis_z = payload.get("retarget_target_pose_pelvis_z_cm")
    output_pelvis_z = payload.get("retarget_output_pelvis_z_cm")
    height_delta = payload.get(
        "retarget_output_pelvis_height_from_target_pose_cm"
    )
    pelvis_motion_delta = payload.get(
        "retarget_output_pelvis_from_expected_pelvis_motion_cm"
    )
    if (
        not isinstance(target_pelvis_z, (int, float))
        or abs(float(target_pelvis_z) - 93.164) > 0.25
        or not isinstance(output_pelvis_z, (int, float))
        or abs(float(output_pelvis_z) - float(target_pelvis_z)) > 0.10
        or not isinstance(height_delta, (int, float))
        or not 0 <= float(height_delta) <= 0.10
        or not isinstance(pelvis_motion_delta, (int, float))
        or not 0 <= float(pelvis_motion_delta) <= 0.10
    ):
        raise ValidationError(
            "helper corrected pelvis numerics differ: "
            f"target_z={target_pelvis_z!r}, output_z={output_pelvis_z!r}, "
            f"height_delta={height_delta!r}, "
            f"pelvis_motion_delta={pelvis_motion_delta!r}"
        )
    anchors = payload.get("retarget_output_anchor_transforms")
    if not isinstance(anchors, dict) or set(anchors) != {
        "root",
        "pelvis",
        "spine_01",
        "head",
    }:
        raise ValidationError(f"helper retarget output anchors differ: {anchors!r}")
    pelvis_anchor = anchors.get("pelvis")
    anchor_pelvis_z = (
        pelvis_anchor.get("translation_z_cm")
        if isinstance(pelvis_anchor, dict)
        else None
    )
    if (
        not isinstance(anchor_pelvis_z, (int, float))
        or abs(float(anchor_pelvis_z) - float(output_pelvis_z)) > 1.0e-6
    ):
        raise ValidationError(
            "helper pelvis anchor differs from corrected output: "
            f"anchor_z={anchor_pelvis_z!r}, output_z={output_pelvis_z!r}"
        )


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    report_path = Path(tempfile.gettempdir()) / "Session8BMetaHumanRuntimeAssetValidation.invalid.json"
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "errors": [],
    }
    try:
        user_dir = validate_command_line(command_line, project_root)
        report_path = user_dir / REPORT_RELATIVE
        if report_path.exists():
            raise ValidationError(f"fresh UUID already contains report: {report_path}")
        if list(user_dir.rglob("*.sav")):
            raise ValidationError("fresh UUID already contains a save file")

        state["phase"] = "DISK_BASELINE"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise ValidationError(
                f"validator began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        source = project_root / SOURCE_RELATIVE
        if record(source) != {
            "exists": True,
            "bytes": SOURCE_BYTES,
            "sha256": SOURCE_SHA256,
        }:
            raise ValidationError("accepted assembled source identity changed")
        production = project_root / PRODUCTION_SAVE_RELATIVE
        savegames_before = snapshot(production.parent)
        savegame_directories_before = directory_snapshot(production.parent)
        if savegames_before != {production.name: ACCEPTED_SAVE}:
            raise ValidationError("project SaveGames is not the sole accepted production slot")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise ValidationError("accepted external backup changed")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise ValidationError("production save and accepted backup differ")
        content_before = snapshot(content_root)
        content_directories_before = directory_snapshot(content_root)
        content_summary = snapshot_summary(content_before)
        if content_summary["file_count"] != 953:
            raise ValidationError(
                f"canonical Content file count is not assembly 950 plus three: {content_summary}"
            )
        canonical_relative_files = {
            f"{object_path.split('.', 1)[0].removeprefix('/Game/')}.uasset"
            for object_path in CANONICAL_OBJECTS
        }
        assembly_partition = {
            relative: identity
            for relative, identity in content_before.items()
            if relative not in canonical_relative_files
        }
        assembly_partition_summary = snapshot_summary(assembly_partition)
        if assembly_partition_summary != {
            "file_count": ASSEMBLED_CONTENT_FILE_COUNT,
            "bytes": ASSEMBLED_CONTENT_BYTES,
            "manifest_sha256": ASSEMBLED_CONTENT_MANIFEST_SHA256,
        }:
            raise ValidationError(
                "pre-canonical Content partition differs from the accepted assembly: "
                f"{assembly_partition_summary}"
            )
        if assembly_partition.get(COOK_MANIFEST_RELATIVE) != COOK_MANIFEST_RECORD:
            raise ValidationError("accepted cook manifest identity changed")
        assembly_without_cook_manifest = dict(assembly_partition)
        assembly_without_cook_manifest.pop(COOK_MANIFEST_RELATIVE)
        assembly_without_cook_manifest_summary = snapshot_summary(
            assembly_without_cook_manifest
        )
        if assembly_without_cook_manifest_summary != ASSEMBLY_WITHOUT_COOK_MANIFEST:
            raise ValidationError(
                "assembly partition below the owned cook manifest changed: "
                f"{assembly_without_cook_manifest_summary}"
            )
        canonical_disk: dict[str, dict[str, Any]] = {}
        for object_path in CANONICAL_OBJECTS:
            package_name = object_path.split(".", 1)[0]
            base = content_root / package_name.removeprefix("/Game/")
            for suffix in PACKAGE_SUFFIXES:
                identity = record(Path(f"{base}{suffix}"))
                canonical_disk[f"{package_name}{suffix}"] = identity
                if suffix == ".uasset":
                    if not identity["exists"] or identity["bytes"] <= 0:
                        raise ValidationError(f"canonical asset missing: {package_name}")
                elif identity["exists"]:
                    raise ValidationError(
                        f"unexpected canonical sidecar exists: {package_name}{suffix}"
                    )

        state["phase"] = "FRESH_PROCESS_VALIDATE"
        payload = strict_json_text(resolve_validate_method()(), "C++ validation helper")
        state["helper_result"] = payload
        validate_helper(payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise ValidationError(
                f"validator left dirty packages: content={dirty_content}, maps={dirty_maps}"
            )
        content_after = snapshot(content_root)
        content_directories_after = directory_snapshot(content_root)
        savegames_after = snapshot(production.parent)
        savegame_directories_after = directory_snapshot(production.parent)
        external_saves = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav")
        )
        if (
            content_after != content_before
            or content_directories_after != content_directories_before
        ):
            raise ValidationError("fresh-process validation changed Content bytes/topology")
        if (
            savegames_after != savegames_before
            or savegame_directories_after != savegame_directories_before
        ):
            raise ValidationError("fresh-process validation changed project SaveGames")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise ValidationError("fresh-process validation changed accepted backup")
        if external_saves:
            raise ValidationError(f"validator created external saves: {external_saves}")
        state.update(
            {
                "status": "PASS_NO_DISK_MUTATION",
                "phase": "COMPLETE",
                "content": content_summary,
                "content_directory_count_before": len(content_directories_before),
                "content_directory_count_after": len(content_directories_after),
                "accepted_assembly_partition": assembly_partition_summary,
                "accepted_assembly_without_cook_manifest": (
                    assembly_without_cook_manifest_summary
                ),
                "accepted_cook_manifest": COOK_MANIFEST_RECORD,
                "canonical_disk": canonical_disk,
                "project_savegames_before": savegames_before,
                "project_savegames_after": savegames_after,
                "project_savegame_directories_before": sorted(
                    savegame_directories_before
                ),
                "project_savegame_directories_after": sorted(
                    savegame_directories_after
                ),
                "external_save_files": external_saves,
                "production_save_unchanged": True,
                "accepted_backup_unchanged": True,
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        state["status"] = "FAIL"
        state["failed_utc"] = utc_now()
        try:
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                "DG_SESSION8B_METAHUMAN_RUNTIME_ASSET_VALIDATION: "
                f"report write failed: {report_error}"
            )
        unreal.log_error(
            f"DG_SESSION8B_METAHUMAN_RUNTIME_ASSET_VALIDATION: FAIL "
            f"phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_METAHUMAN_RUNTIME_ASSET_VALIDATION: "
        f"{state['status']} report={report_path}"
    )


main()
