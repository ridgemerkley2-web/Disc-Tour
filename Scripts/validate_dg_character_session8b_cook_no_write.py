"""Independently prove Session 8B strict cook validation performs no writes.

The wrapper snapshots every project Content file/directory, Asset Registry
membership, every project SaveGames file/directory, the accepted production
save, its external Session 7 backup, and the isolated external UserDir save set.
"""

from __future__ import annotations

import hashlib
import json
import re
import runpy
import uuid
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
STRICT_VALIDATOR = (
    PROJECT_ROOT / "Scripts/validate_dg_character_session8b_cook_assets.py")
SCHEMA = "DiscGolfTour.Session8BCookNoWriteValidation.v1"
NO_WRITE_SWITCH = "-DGSession8BCookNoWriteValidate"
STRICT_VALIDATE_SWITCH = "-DGSession8BCookAssetValidate"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BCookNoWriteValidation.json")
PRODUCTION_SAVE = (
    PROJECT_ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_EXTERNAL_BACKUP = Path(
    "C:/DGTour_Backups/Session7_Accepted/"
    "DiscGolfTour_Profile_0_Session7_Accepted.sav")
EXPECTED_ACCEPTED_SAVE_BYTES = 5212
EXPECTED_ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14")
EXPECTED_MANIFEST_OBJECT = (
    "/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest.DA_DG_RuntimeCookManifest")
EXPECTED_DGMASTER_PROFILE_OBJECT = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster."
    "DA_DG_AvatarBackend_DGMaster")
EXPECTED_METAHUMAN_BACKEND_ID = "metahuman_assembled"
EXPECTED_METAHUMAN_PROFILE_OBJECT = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default")
EXPECTED_METAHUMAN_ACTOR_CLASS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default_C")
EXPECTED_METAHUMAN_RETARGET_OBJECT = (
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman")
EXPECTED_METAHUMAN_TARGET_IK_OBJECT = (
    "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig")
EXPECTED_SOURCE_MHC_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default")
def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _file_record(path: Path) -> dict:
    if not path.is_file():
        return {"present": False}
    return {
        "present": True,
        "bytes": path.stat().st_size,
        "sha256": _sha256(path),
    }


def _content_snapshot() -> dict[str, dict]:
    root = PROJECT_ROOT / "Content"
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): _file_record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def _directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*") if path.is_dir()
    }


def _registry_snapshot() -> list[str]:
    return sorted(str(path) for path in unreal.EditorAssetLibrary.list_assets(
        "/Game/DiscGolf", recursive=True, include_folder=False))


def _save_directory_snapshot() -> dict[str, dict]:
    root = PROJECT_ROOT / "Saved/SaveGames"
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): _file_record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def _exact_switch(command_line: str, switch: str) -> bool:
    return bool(re.search(
        rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line))


def _command_values(command_line: str, name: str) -> list[str]:
    pattern = re.compile(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))')
    return [quoted or plain for quoted, plain in pattern.findall(command_line)]


def _validate_command_line(command_line: str) -> Path:
    required = (
        NO_WRITE_SWITCH, STRICT_VALIDATE_SWITCH, NO_PORTAL_SWITCH,
        "-unattended", "-nop4")
    missing = [switch for switch in required
               if not _exact_switch(command_line, switch)]
    if missing:
        raise RuntimeError(f"Required no-write switches absent: {missing}")
    run_values = _command_values(command_line, "run")
    if len(run_values) != 1 or run_values[0].casefold() != "pythonscript":
        raise RuntimeError(
            "Cook no-write proof requires exactly one -run=PythonScript")
    values = _command_values(command_line, "UserDir")
    if len(values) != 1:
        raise RuntimeError(
            "Command line must contain exactly one absolute -UserDir=<UUID path>")
    user_dir = Path(values[0]).resolve()
    expected_parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(expected_parent)
    except ValueError as exc:
        raise RuntimeError(
            f"UserDir is outside the Session8B external test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise RuntimeError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise RuntimeError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold() or not user_dir.is_dir():
        raise RuntimeError("UserDir UUID is not canonical and active")
    try:
        user_dir.relative_to(PROJECT_ROOT)
    except ValueError:
        pass
    else:
        raise RuntimeError("UserDir must remain external to the project")
    return user_dir


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir = _validate_command_line(command_line)
    report_path = user_dir / REPORT_RELATIVE
    if report_path.exists():
        raise RuntimeError(f"Fresh UUID already contains no-write report: {report_path}")
    external_saves_before = sorted(
        str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
    if external_saves_before:
        raise RuntimeError(
            f"Fresh external UserDir already contains saves: {external_saves_before}")
    content_root = PROJECT_ROOT / "Content"
    savegames_root = PROJECT_ROOT / "Saved/SaveGames"
    content_before = _content_snapshot()
    content_directories_before = _directory_snapshot(content_root)
    registry_before = _registry_snapshot()
    saves_before = _save_directory_snapshot()
    savegame_directories_before = _directory_snapshot(savegames_root)
    production_before = _file_record(PRODUCTION_SAVE)
    backup_before = _file_record(ACCEPTED_EXTERNAL_BACKUP)

    try:
        strict = runpy.run_path(
            str(STRICT_VALIDATOR), run_name="dg_session8b_cook_no_write_strict")
        validation = strict["validate"]()
    except Exception as exc:
        validation = {
            "status": "EXCEPTION",
            "errors": [f"Nested strict validator raised {type(exc).__name__}: {exc}"],
        }

    content_after = _content_snapshot()
    content_directories_after = _directory_snapshot(content_root)
    registry_after = _registry_snapshot()
    saves_after = _save_directory_snapshot()
    savegame_directories_after = _directory_snapshot(savegames_root)
    production_after = _file_record(PRODUCTION_SAVE)
    backup_after = _file_record(ACCEPTED_EXTERNAL_BACKUP)
    external_saves_after = sorted(
        str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
    errors = list(validation.get("errors", []))
    if validation.get("status") != "PASS_NO_DISK_MUTATION":
        errors.append(
            f"Strict validator status was {validation.get('status')}, "
            "expected PASS_NO_DISK_MUTATION")
    if validation.get("availability_gate") != "SEPARATE_S8B_AUDIT_NOT_INVOKED":
        errors.append("Strict validator availability separation gate differs")
    if validation.get("validation_switch") != STRICT_VALIDATE_SWITCH \
            or validation.get("no_portal_switch") != NO_PORTAL_SWITCH \
            or validation.get("external_user_dir") != str(user_dir):
        errors.append("Strict validator invocation boundary differs")
    if validation.get("metahuman_preferred_quality_profile_id") \
            != "GameplayPerformance":
        errors.append("Strict validator MetaHuman quality mapping differs")
    expected_entry_points = {
        "manifest_object": EXPECTED_MANIFEST_OBJECT,
        "dg_master_backend_profile_object": EXPECTED_DGMASTER_PROFILE_OBJECT,
        "metahuman_backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "metahuman_profile_object": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "metahuman_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "metahuman_retarget_object": EXPECTED_METAHUMAN_RETARGET_OBJECT,
        "metahuman_target_ik_object": EXPECTED_METAHUMAN_TARGET_IK_OBJECT,
        "source_mhc_package": EXPECTED_SOURCE_MHC_PACKAGE,
    }
    for field, expected in expected_entry_points.items():
        if validation.get(field) != expected:
            errors.append(
                f"Strict validator {field}={validation.get(field)!r}, "
                f"expected frozen {expected!r}")
    expected_counts = {
        "runtime_package_count": 69,
        "metahuman_runtime_package_count": 264,
        "metahuman_explicit_cook_root_package_count": 264,
        "backend_profile_count": 2,
        "excluded_package_count": 12,
        "excluded_metahuman_package_count": 1,
    }
    for field, expected in expected_counts.items():
        if validation.get(field) != expected:
            errors.append(
                f"Strict validator {field}={validation.get(field)!r}, "
                f"expected {expected}")
    if validation.get("metahuman_inventory_semantics") != \
            "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH" \
            or validation.get("fresh_validation_transitive_runtime_dependency_count") \
                != 340 \
            or validation.get("packaged_transitive_dependency_reproof") != "REQUIRED":
        errors.append("Explicit-root/transitive-dependency proof semantics differ")
    for field in (
            "asset_save_calls", "asset_import_calls", "asset_factory_calls",
            "asset_delete_calls"):
        if validation.get(field) != 0:
            errors.append(f"Strict validator reported nonzero {field}")
    if content_before != content_after:
        errors.append("Full project Content file snapshot changed")
    if content_directories_before != content_directories_after:
        errors.append("Full project Content directory topology changed")
    if registry_before != registry_after:
        errors.append("/Game/DiscGolf Asset Registry membership changed")
    if saves_before != saves_after:
        errors.append("Project SaveGames file set or bytes changed")
    if savegame_directories_before != savegame_directories_after:
        errors.append("Project SaveGames directory topology changed")
    if production_before != production_after:
        errors.append("Accepted production save changed")
    if backup_before != backup_after:
        errors.append("Accepted external Session 7 backup changed")
    if external_saves_after:
        errors.append(
            f"External UserDir contains save files: {external_saves_after}")
    for label, record in (
            ("production save", production_before),
            ("external accepted backup", backup_before)):
        if record.get("bytes") != EXPECTED_ACCEPTED_SAVE_BYTES \
                or record.get("sha256") != EXPECTED_ACCEPTED_SAVE_SHA256:
            errors.append(
                f"{label} is not the frozen 5212-byte A999 Session 7 baseline")
    if PRODUCTION_SAVE.resolve() == ACCEPTED_EXTERNAL_BACKUP.resolve() \
            or PROJECT_ROOT in ACCEPTED_EXTERNAL_BACKUP.resolve().parents:
        errors.append("Accepted backup is not independent and outside the repository")

    result = {
        "schema": SCHEMA,
        "status": "PASS_NO_WRITE" if not errors else "FAIL",
        "no_write_switch": NO_WRITE_SWITCH,
        "strict_validate_switch": STRICT_VALIDATE_SWITCH,
        "no_portal_switch": NO_PORTAL_SWITCH,
        "external_user_dir": str(user_dir),
        "report_path": str(report_path),
        "validation_status": validation.get("status"),
        "availability_gate": validation.get("availability_gate"),
        "metahuman_preferred_quality_profile_id": validation.get(
            "metahuman_preferred_quality_profile_id"),
        "runtime_package_count": validation.get("runtime_package_count"),
        "metahuman_runtime_package_count": validation.get(
            "metahuman_runtime_package_count"),
        "metahuman_explicit_cook_root_package_count": validation.get(
            "metahuman_explicit_cook_root_package_count"),
        "metahuman_inventory_semantics": validation.get(
            "metahuman_inventory_semantics"),
        "fresh_validation_transitive_runtime_dependency_count": validation.get(
            "fresh_validation_transitive_runtime_dependency_count"),
        "packaged_transitive_dependency_reproof": validation.get(
            "packaged_transitive_dependency_reproof"),
        "backend_profile_count": validation.get("backend_profile_count"),
        "excluded_package_count": validation.get("excluded_package_count"),
        "excluded_metahuman_package_count": validation.get(
            "excluded_metahuman_package_count"),
        "core_data_status": validation.get("core_data_status"),
        **expected_entry_points,
        "tracked_content_file_count": len(content_after),
        "asset_registry_count": len(registry_after),
        "content_snapshot_before": content_before,
        "content_snapshot_after": content_after,
        "content_directories_before": sorted(content_directories_before),
        "content_directories_after": sorted(content_directories_after),
        "asset_registry_before": registry_before,
        "asset_registry_after": registry_after,
        "save_directory_before": saves_before,
        "save_directory_after": saves_after,
        "savegame_directories_before": sorted(savegame_directories_before),
        "savegame_directories_after": sorted(savegame_directories_after),
        "production_save_before": production_before,
        "production_save_after": production_after,
        "external_backup_path": str(ACCEPTED_EXTERNAL_BACKUP),
        "external_backup_before": backup_before,
        "external_backup_after": backup_after,
        "external_save_files_before": external_saves_before,
        "external_save_files_after": external_saves_after,
        "content_mutation": "NONE" if content_before == content_after else "DETECTED",
        "content_directory_mutation": (
            "NONE" if content_directories_before == content_directories_after
            else "DETECTED"),
        "asset_registry_mutation": (
            "NONE" if registry_before == registry_after else "DETECTED"),
        "save_mutation": "NONE" if saves_before == saves_after else "DETECTED",
        "save_directory_mutation": (
            "NONE" if savegame_directories_before == savegame_directories_after
            else "DETECTED"),
        "external_backup_mutation": (
            "NONE" if backup_before == backup_after else "DETECTED"),
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "asset_factory_calls": 0,
        "asset_delete_calls": 0,
        "ubt_launched": False,
        "cook_launched": False,
        "errors": errors,
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8", newline="\n")
    if errors:
        raise RuntimeError(
            "Session 8B cook no-write validation failed: " + "; ".join(errors))
    unreal.log(
        "DG_SESSION8B_COOK_NO_WRITE: PASS_NO_WRITE "
        f"runtime={result['runtime_package_count']} "
        f"metahuman_runtime={result['metahuman_runtime_package_count']} "
        f"profiles={result['backend_profile_count']} "
        f"excluded={result['excluded_package_count']} "
        f"files={result['tracked_content_file_count']}")


if __name__ == "__main__":
    main()
