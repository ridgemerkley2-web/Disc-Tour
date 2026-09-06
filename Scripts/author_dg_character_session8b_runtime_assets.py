"""Guarded Session 8B authoring for the three canonical runtime entry assets."""

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


SCHEMA = "DiscGolfTour.Session8BMetaHumanRuntimeAssetAuthor.v1"
AUTHOR_SWITCH = "-DGSession8BMetaHumanRuntimeAssetsAuthor"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanRuntimeAssetAuthor.json"
)
ASSEMBLY_VALIDATION_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanAssemblyValidation.json"
)
ASSEMBLY_VALIDATION_SHA256 = (
    "CB0A274A2C56ED669CB645B40F8D3B0E3EEC9EEECDEE591F841476A263B8714B"
)
ASSEMBLY_VALIDATION_BYTES = 59875
SOURCE_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default.uasset"
)
SOURCE_BYTES = 112982127
SOURCE_SHA256 = "1AA1EFBFDD882308959D1429D562DD2D92F64317219660003063D0AEF17BCFAB"
ASSEMBLED_CONTENT_FILE_COUNT = 950
ASSEMBLED_CONTENT_BYTES = 2919678243
ASSEMBLED_CONTENT_MANIFEST_SHA256 = (
    "79EA7D1AC3B0A97ADD2C6D3AFD0A588C71A669DAB000DCEAE426A156497B6A34"
)
PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE_BYTES = 5212
ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
)
CANONICAL_OBJECTS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default",
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman",
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default",
)
CANONICAL_PACKAGES = tuple(path.split(".", 1)[0] for path in CANONICAL_OBJECTS)
EXPECTED_HELPER_SCHEMA = 2
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


class AuthorError(RuntimeError):
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


def canonical_disk_state(content_root: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for package_name in CANONICAL_PACKAGES:
        base = content_root / package_name.removeprefix("/Game/")
        for suffix in PACKAGE_SUFFIXES:
            path = Path(f"{base}{suffix}")
            result[f"{package_name}{suffix}"] = record(path)
    return result


def rollback_fresh_canonical_assets(
    content_root: Path,
    content_before: dict[str, dict[str, Any]],
    directories_before: set[str],
) -> dict[str, Any]:
    deleted_assets: list[str] = []
    deleted_files: list[str] = []
    errors: list[str] = []
    for package_name in reversed(CANONICAL_PACKAGES):
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(package_name):
                if not unreal.EditorAssetLibrary.delete_asset(package_name):
                    errors.append(f"EditorAssetLibrary refused delete: {package_name}")
                else:
                    deleted_assets.append(package_name)
        except Exception as exc:
            errors.append(f"asset delete failed for {package_name}: {exc}")
    for package_name in reversed(CANONICAL_PACKAGES):
        base = content_root / package_name.removeprefix("/Game/")
        for suffix in PACKAGE_SUFFIXES:
            path = Path(f"{base}{suffix}")
            if not path.exists():
                continue
            try:
                path.unlink()
                deleted_files.append(str(path))
            except OSError as exc:
                errors.append(f"disk delete failed for {path}: {exc}")
    new_directories = sorted(
        directory_snapshot(content_root) - directories_before,
        key=lambda value: (len(Path(value).parts), value),
        reverse=True,
    )
    removed_directories: list[str] = []
    for relative in new_directories:
        directory = content_root / relative
        try:
            directory.rmdir()
            removed_directories.append(relative)
        except OSError:
            pass
    content_after = snapshot(content_root)
    directories_after = directory_snapshot(content_root)
    return {
        "attempted": True,
        "deleted_assets": deleted_assets,
        "deleted_files": deleted_files,
        "removed_directories": removed_directories,
        "errors": errors,
        "content_files_restored": content_after == content_before,
        "content_directories_restored": directories_after == directories_before,
        "canonical_disk_state": canonical_disk_state(content_root),
    }


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise AuthorError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    try:
        value = json.loads(text, object_pairs_hook=no_duplicates)
    except json.JSONDecodeError as exc:
        raise AuthorError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise AuthorError(f"{label} JSON root is not an object")
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
    required = (AUTHOR_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise AuthorError(f"required command-line switches absent: {missing}")
    if "-run=pythonscript" not in command_line.casefold():
        raise AuthorError("runtime-asset author must use PythonScript commandlet")
    user_dir_text = command_value(command_line, "UserDir")
    if not user_dir_text:
        raise AuthorError("absolute external -UserDir is required")
    user_dir = Path(user_dir_text).resolve()
    expected_parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(expected_parent)
    except ValueError as exc:
        raise AuthorError(f"UserDir is outside Session8B test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise AuthorError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise AuthorError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise AuthorError("UserDir UUID is not canonical")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise AuthorError("UserDir must be external to the project")
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


def resolve_helper_method(name_options: tuple[str, ...]) -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    matches = [name for name in name_options if hasattr(utility, name)]
    if len(matches) != 1:
        raise AuthorError(
            f"expected exactly one reflected helper spelling {name_options}, got {matches}"
        )
    return getattr(utility, matches[0])


def validate_helper_result(payload: dict[str, Any], operation: str) -> None:
    if payload.get("schema_version") != EXPECTED_HELPER_SCHEMA:
        raise AuthorError(f"{operation} helper schema differs")
    if payload.get("session") != "8B" or payload.get("operation") != operation:
        raise AuthorError(f"{operation} helper identity differs")
    if payload.get("status") != "PASS" or payload.get("errors", []) != []:
        raise AuthorError(f"{operation} helper did not PASS: {payload}")
    if payload.get("chain_mapping") != EXPECTED_CHAIN_MAPPING:
        raise AuthorError(f"{operation} exact retarget mapping differs")
    exact_fields = {
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
        "reload_from_disk_verified": True,
    }
    for key, expected in exact_fields.items():
        if payload.get(key) != expected:
            raise AuthorError(
                f"{operation} helper field {key}={payload.get(key)!r}, expected {expected!r}"
            )
    max_abs = payload.get("retarget_output_max_abs_translation_cm")
    if not isinstance(max_abs, (int, float)) or not 0 <= float(max_abs) <= 1000:
        raise AuthorError(
            f"{operation} helper max retarget translation differs: {max_abs!r}"
        )
    pelvis_exact = {
        "retarget_target_pose_pelvis_expected_z_cm": 93.164,
        "retarget_target_pose_pelvis_z_tolerance_cm": 0.25,
        "retarget_output_pelvis_height_tolerance_cm": 0.10,
        "retarget_output_pelvis_motion_tolerance_cm": 0.10,
    }
    for key, expected in pelvis_exact.items():
        if payload.get(key) != expected:
            raise AuthorError(
                f"{operation} helper field {key}={payload.get(key)!r}, "
                f"expected {expected!r}"
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
        raise AuthorError(
            f"{operation} helper corrected pelvis numerics differ: "
            f"target_z={target_pelvis_z!r}, output_z={output_pelvis_z!r}, "
            f"height_delta={height_delta!r}, "
            f"pelvis_motion_delta={pelvis_motion_delta!r}"
        )
    anchors = payload.get("retarget_output_anchor_transforms")
    pelvis_anchor = anchors.get("pelvis") if isinstance(anchors, dict) else None
    anchor_pelvis_z = (
        pelvis_anchor.get("translation_z_cm")
        if isinstance(pelvis_anchor, dict)
        else None
    )
    if (
        not isinstance(anchor_pelvis_z, (int, float))
        or abs(float(anchor_pelvis_z) - float(output_pelvis_z)) > 1.0e-6
    ):
        raise AuthorError(
            f"{operation} helper pelvis anchor differs from corrected output: "
            f"anchor_z={anchor_pelvis_z!r}, output_z={output_pelvis_z!r}"
        )
    expected_rollback = operation == "AUTHOR"
    if payload.get("rollback_verified") != expected_rollback:
        raise AuthorError(
            f"{operation} helper rollback_verified={payload.get('rollback_verified')!r}, "
            f"expected {expected_rollback!r}"
        )


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    meta_root = content_root / "DiscGolf/Characters/MetaHuman"
    avatar_data_root = content_root / "DiscGolf/Characters/Avatar/Data"
    report_path = Path(tempfile.gettempdir()) / "Session8BMetaHumanRuntimeAssetAuthor.invalid.json"
    content_before: dict[str, dict[str, Any]] | None = None
    content_directories_before: set[str] | None = None
    author_transaction_started = False
    user_dir: Path | None = None
    production: Path | None = None
    savegames_root: Path | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    accepted_save: dict[str, Any] | None = None
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "canonical_objects": list(CANONICAL_OBJECTS),
        "errors": [],
    }
    try:
        user_dir = validate_command_line(command_line, project_root)
        report_path = user_dir / REPORT_RELATIVE
        if report_path.exists():
            raise AuthorError(f"fresh UUID already contains report: {report_path}")
        if list(user_dir.rglob("*.sav")):
            raise AuthorError("fresh UUID already contains a save file")
        state["external_user_dir"] = str(user_dir)
        state["report_path"] = str(report_path)

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise AuthorError(
                f"commandlet began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        assembly_validation = project_root / ASSEMBLY_VALIDATION_RELATIVE
        if record(assembly_validation) != {
            "exists": True,
            "bytes": ASSEMBLY_VALIDATION_BYTES,
            "sha256": ASSEMBLY_VALIDATION_SHA256,
        }:
            raise AuthorError("accepted assembly file-validation report changed")
        assembly_payload = strict_json_text(
            assembly_validation.read_text(encoding="utf-8"), "assembly validation"
        )
        if assembly_payload.get("status") != "PASS_NO_DISK_MUTATION":
            raise AuthorError("assembly file-validation report is not PASS")
        source = project_root / SOURCE_RELATIVE
        if record(source) != {
            "exists": True,
            "bytes": SOURCE_BYTES,
            "sha256": SOURCE_SHA256,
        }:
            raise AuthorError("accepted assembled source identity changed")
        accepted_save = {
            "exists": True,
            "bytes": ACCEPTED_SAVE_BYTES,
            "sha256": ACCEPTED_SAVE_SHA256,
        }
        production = project_root / PRODUCTION_SAVE_RELATIVE
        savegames_root = production.parent
        savegames_before = snapshot(savegames_root)
        savegame_directories_before = directory_snapshot(savegames_root)
        if savegames_before != {production.name: accepted_save}:
            raise AuthorError(
                f"project SaveGames baseline is not the sole production slot: {savegames_before}"
            )
        if record(production) != accepted_save or record(ACCEPTED_BACKUP) != accepted_save:
            raise AuthorError("production save or accepted backup changed")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise AuthorError("production save and accepted backup differ")
        canonical_before = canonical_disk_state(content_root)
        if any(identity["exists"] for identity in canonical_before.values()):
            raise AuthorError(
                f"canonical target or sidecar already exists: {canonical_before}"
            )
        content_before = snapshot(content_root)
        content_directories_before = directory_snapshot(content_root)
        content_before_summary = snapshot_summary(content_before)
        if content_before_summary != {
            "file_count": ASSEMBLED_CONTENT_FILE_COUNT,
            "bytes": ASSEMBLED_CONTENT_BYTES,
            "manifest_sha256": ASSEMBLED_CONTENT_MANIFEST_SHA256,
        }:
            raise AuthorError(
                f"assembled Content baseline changed: {content_before_summary}"
            )
        meta_before = {
            key.removeprefix("DiscGolf/Characters/MetaHuman/"): value
            for key, value in content_before.items()
            if key.startswith("DiscGolf/Characters/MetaHuman/")
        }
        avatar_before = {
            key.removeprefix("DiscGolf/Characters/Avatar/Data/"): value
            for key, value in content_before.items()
            if key.startswith("DiscGolf/Characters/Avatar/Data/")
        }
        state["before"] = {
            "content": content_before_summary,
            "content_directory_count": len(content_directories_before),
            "canonical_disk_state": canonical_before,
            "project_savegames": savegames_before,
            "project_savegame_directories": sorted(savegame_directories_before),
            "metahuman_file_count": len(meta_before),
            "avatar_data_file_count": len(avatar_before),
            "source": record(source),
            "production_save": record(production),
            "accepted_backup": record(ACCEPTED_BACKUP),
            "assembly_validation": record(assembly_validation),
        }

        state["phase"] = "AUTHOR_EXACT_THREE"
        author_transaction_started = True
        author_method = resolve_helper_method(
            (
                "author_session8_b_meta_human_assets",
                "author_session8b_meta_human_assets",
            )
        )
        author_result = strict_json_text(author_method(), "C++ author helper")
        state["author_result"] = author_result
        validate_helper_result(author_result, "AUTHOR")
        writes = author_result.get("writes")
        if writes != list(CANONICAL_OBJECTS):
            raise AuthorError(f"author helper write set differs: {writes}")
        if author_result.get("disk_mutation") != "EXACT_THREE_ASSET_WRITES":
            raise AuthorError("fresh author helper did not report exact three writes")

        state["phase"] = "HELPER_RELOAD_VALIDATE"
        validate_method = resolve_helper_method(
            (
                "validate_session8_b_meta_human_assets",
                "validate_session8b_meta_human_assets",
            )
        )
        validation_result = strict_json_text(validate_method(), "C++ validation helper")
        state["validation_result"] = validation_result
        validate_helper_result(validation_result, "VALIDATE")
        if validation_result.get("disk_mutation") != "NONE" or validation_result.get(
            "writes"
        ) != []:
            raise AuthorError("validation helper mutated disk")

        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise AuthorError(
                f"dirty packages remain: content={dirty_content}, maps={dirty_maps}"
            )
        content_after = snapshot(content_root)
        content_after_summary = snapshot_summary(content_after)
        content_added = sorted(set(content_after) - set(content_before))
        content_changed = sorted(
            key for key in set(content_before) & set(content_after)
            if content_before[key] != content_after[key]
        )
        content_removed = sorted(set(content_before) - set(content_after))
        expected_content_added = [
            "DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default.uasset",
            "DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset",
            "DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset",
        ]
        if content_added != expected_content_added or content_changed or content_removed:
            raise AuthorError(
                "full Content delta differs: "
                f"added={content_added}, changed={content_changed}, removed={content_removed}"
            )
        if directory_snapshot(content_root) != content_directories_before:
            raise AuthorError("full Content directory set changed")
        meta_after = {
            key.removeprefix("DiscGolf/Characters/MetaHuman/"): value
            for key, value in content_after.items()
            if key.startswith("DiscGolf/Characters/MetaHuman/")
        }
        avatar_after = {
            key.removeprefix("DiscGolf/Characters/Avatar/Data/"): value
            for key, value in content_after.items()
            if key.startswith("DiscGolf/Characters/Avatar/Data/")
        }
        meta_added = sorted(set(meta_after) - set(meta_before))
        avatar_added = sorted(set(avatar_after) - set(avatar_before))
        meta_changed = sorted(
            key for key in set(meta_before) & set(meta_after)
            if meta_before[key] != meta_after[key]
        )
        avatar_changed = sorted(
            key for key in set(avatar_before) & set(avatar_after)
            if avatar_before[key] != avatar_after[key]
        )
        if meta_added != [
            "BP_DG_MetaHuman_Default.uasset",
            "RTG_DGMaster_To_MetaHuman.uasset",
        ] or avatar_added != ["DA_DG_AvatarBackend_MetaHuman_Default.uasset"]:
            raise AuthorError(
                f"canonical added-file set differs: meta={meta_added}, avatar={avatar_added}"
            )
        if meta_changed or avatar_changed or set(meta_before) - set(meta_after) or set(
            avatar_before
        ) - set(avatar_after):
            raise AuthorError("preexisting MetaHuman/avatar files changed or disappeared")
        if record(source) != state["before"]["source"]:
            raise AuthorError("canonical assembled source changed")
        if record(production) != accepted_save or record(ACCEPTED_BACKUP) != accepted_save:
            raise AuthorError("protected save changed during canonical asset authoring")
        savegames_after = snapshot(savegames_root)
        savegame_directories_after = directory_snapshot(savegames_root)
        if (
            savegames_after != savegames_before
            or savegame_directories_after != savegame_directories_before
        ):
            raise AuthorError(
                f"project SaveGames directory changed: before={savegames_before}, "
                f"after={savegames_after}, dirs_before={savegame_directories_before}, "
                f"dirs_after={savegame_directories_after}"
            )
        external_saves = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav")
        )
        if external_saves:
            raise AuthorError(f"external UserDir contains save files: {external_saves}")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise AuthorError("protected saves ceased to be byte-identical")
        state.update(
            {
                "status": "PASS_AUTHORED_AND_HELPER_RELOAD_VALIDATED",
                "phase": "COMPLETE",
                "disk_delta": {
                    "content_before": content_before_summary,
                    "content_after": content_after_summary,
                    "content_added": content_added,
                    "content_changed": content_changed,
                    "content_removed": content_removed,
                    "project_savegames_before": savegames_before,
                    "project_savegames_after": savegames_after,
                    "project_savegame_directories_before": sorted(
                        savegame_directories_before
                    ),
                    "project_savegame_directories_after": sorted(
                        savegame_directories_after
                    ),
                    "external_save_files": external_saves,
                    "metahuman_added": meta_added,
                    "avatar_data_added": avatar_added,
                    "metahuman_changed": meta_changed,
                    "avatar_data_changed": avatar_changed,
                },
                "production_save_unchanged": True,
                "accepted_backup_unchanged": True,
                "fixed_preset_cosmetic_qualification": (
                    "CLEAN_SHAVEN_BEARD_MAPPING_UNSUPPORTED"
                ),
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback_complete = False
        if (
            author_transaction_started
            and content_before is not None
            and content_directories_before is not None
            and user_dir is not None
            and production is not None
            and savegames_root is not None
            and savegames_before is not None
            and savegame_directories_before is not None
            and accepted_save is not None
        ):
            rollback = rollback_fresh_canonical_assets(
                content_root, content_before, content_directories_before
            )
            external_saves_found = sorted(user_dir.rglob("*.sav"))
            deleted_external_saves: list[str] = []
            external_delete_errors: list[str] = []
            for save_path in external_saves_found:
                try:
                    save_path.unlink()
                    deleted_external_saves.append(str(save_path.relative_to(user_dir)))
                except OSError as delete_error:
                    external_delete_errors.append(f"{save_path}: {delete_error}")
            external_saves_after = sorted(
                str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav")
            )
            savegames_after_rollback = snapshot(savegames_root)
            savegame_directories_after_rollback = directory_snapshot(savegames_root)
            production_after_rollback = record(production)
            backup_after_rollback = record(ACCEPTED_BACKUP)
            protected_saves_restored = (
                savegames_after_rollback == savegames_before
                and savegame_directories_after_rollback
                    == savegame_directories_before
                and production_after_rollback == accepted_save
                and backup_after_rollback == accepted_save
                and production.read_bytes() == ACCEPTED_BACKUP.read_bytes()
            )
            rollback.update(
                {
                    "external_save_files_found": [
                        str(path.relative_to(user_dir)) for path in external_saves_found
                    ],
                    "external_save_files_deleted": deleted_external_saves,
                    "external_save_delete_errors": external_delete_errors,
                    "external_save_files_after": external_saves_after,
                    "project_savegames_after": savegames_after_rollback,
                    "project_savegame_directories_after": sorted(
                        savegame_directories_after_rollback
                    ),
                    "production_save_after": production_after_rollback,
                    "accepted_backup_after": backup_after_rollback,
                    "protected_saves_restored": protected_saves_restored,
                }
            )
            state["rollback"] = rollback
            rollback_complete = not (
                rollback["errors"]
                or external_delete_errors
                or external_saves_after
                or not rollback["content_files_restored"]
                or not rollback["content_directories_restored"]
                or not protected_saves_restored
                or any(
                    identity["exists"]
                    for identity in rollback["canonical_disk_state"].values()
                )
            )
            if not rollback_complete:
                state["errors"].append(
                    "rollback did not restore every Content/save/external boundary"
                )
        state["status"] = (
            "FAIL_ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
            if rollback_complete
            else (
                "FAIL_ROLLBACK_INCOMPLETE"
                if author_transaction_started
                else "FAIL_BEFORE_AUTHOR_NO_PERSISTENT_ASSET_WRITES"
            )
        )
        state["failed_utc"] = utc_now()
        try:
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                f"DG_SESSION8B_METAHUMAN_RUNTIME_ASSETS: report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            f"DG_SESSION8B_METAHUMAN_RUNTIME_ASSETS: {state['status']} "
            f"phase={state['phase']} "
            f"report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_METAHUMAN_RUNTIME_ASSETS: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
