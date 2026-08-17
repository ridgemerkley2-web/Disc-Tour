"""Independently prove Session 8 strict cook validation performs no writes.

The wrapper snapshots every /Game/DiscGolf package, Asset Registry membership,
all project save-game files, the accepted production save, and its external
Session 7 backup. The nested validator may write only JSON reports under Saved.
"""

from __future__ import annotations

import hashlib
import json
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
STRICT_VALIDATOR = (
    PROJECT_ROOT / "Scripts/validate_dg_character_session8_cook_assets.py")
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session8CookNoWriteValidation.json")
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
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}


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
    root = PROJECT_ROOT / "Content/DiscGolf"
    result = {}
    if not root.is_dir():
        return result
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.suffix.casefold() in PACKAGE_SUFFIXES:
            result[path.relative_to(PROJECT_ROOT).as_posix()] = {
                "bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
    return result


def _registry_snapshot() -> list[str]:
    return sorted(str(path) for path in unreal.EditorAssetLibrary.list_assets(
        "/Game/DiscGolf", recursive=True, include_folder=False))


def _save_directory_snapshot() -> dict[str, dict]:
    root = PROJECT_ROOT / "Saved/SaveGames"
    if not root.is_dir():
        return {}
    return {
        path.relative_to(PROJECT_ROOT).as_posix(): _file_record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def main() -> None:
    content_before = _content_snapshot()
    registry_before = _registry_snapshot()
    saves_before = _save_directory_snapshot()
    production_before = _file_record(PRODUCTION_SAVE)
    backup_before = _file_record(ACCEPTED_EXTERNAL_BACKUP)

    strict = runpy.run_path(
        str(STRICT_VALIDATOR), run_name="dg_session8_cook_no_write_strict")
    validation = strict["validate"]()

    content_after = _content_snapshot()
    registry_after = _registry_snapshot()
    saves_after = _save_directory_snapshot()
    production_after = _file_record(PRODUCTION_SAVE)
    backup_after = _file_record(ACCEPTED_EXTERNAL_BACKUP)
    errors = list(validation.get("errors", []))
    if validation.get("status") != "PASS_NO_DISK_MUTATION":
        errors.append(
            f"Strict validator status was {validation.get('status')}, "
            "expected PASS_NO_DISK_MUTATION")
    expected_entry_points = {
        "manifest_object": EXPECTED_MANIFEST_OBJECT,
        "dg_master_backend_profile_object": EXPECTED_DGMASTER_PROFILE_OBJECT,
        "metahuman_backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "reserved_metahuman_profile_object": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "reserved_metahuman_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "reserved_metahuman_retarget_object": EXPECTED_METAHUMAN_RETARGET_OBJECT,
    }
    for field, expected in expected_entry_points.items():
        if validation.get(field) != expected:
            errors.append(
                f"Strict validator {field}={validation.get(field)!r}, "
                f"expected frozen {expected!r}")
    for field in (
            "asset_save_calls", "asset_import_calls", "asset_factory_calls",
            "asset_delete_calls"):
        if validation.get(field) != 0:
            errors.append(f"Strict validator reported nonzero {field}")
    if content_before != content_after:
        errors.append("/Game/DiscGolf content package snapshot changed")
    if registry_before != registry_after:
        errors.append("/Game/DiscGolf Asset Registry membership changed")
    if saves_before != saves_after:
        errors.append("Project SaveGames file set or bytes changed")
    if production_before != production_after:
        errors.append("Accepted production save changed")
    if backup_before != backup_after:
        errors.append("Accepted external Session 7 backup changed")
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
        "schema": "DiscGolfTour.Session8CookNoWriteValidation.v1",
        "status": "PASS_NO_WRITE" if not errors else "FAIL",
        "validation_status": validation.get("status"),
        "runtime_package_count": validation.get("runtime_package_count"),
        "excluded_package_count": validation.get("excluded_package_count"),
        "core_data_status": validation.get("core_data_status"),
        **expected_entry_points,
        "tracked_content_file_count": len(content_after),
        "asset_registry_count": len(registry_after),
        "content_snapshot_before": content_before,
        "content_snapshot_after": content_after,
        "asset_registry_before": registry_before,
        "asset_registry_after": registry_after,
        "save_directory_before": saves_before,
        "save_directory_after": saves_after,
        "production_save_before": production_before,
        "production_save_after": production_after,
        "external_backup_path": str(ACCEPTED_EXTERNAL_BACKUP),
        "external_backup_before": backup_before,
        "external_backup_after": backup_after,
        "content_mutation": "NONE" if content_before == content_after else "DETECTED",
        "asset_registry_mutation": (
            "NONE" if registry_before == registry_after else "DETECTED"),
        "save_mutation": "NONE" if saves_before == saves_after else "DETECTED",
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
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError(
            "Session 8 cook no-write validation failed: " + "; ".join(errors))
    unreal.log(
        "DG_SESSION8_COOK_NO_WRITE: PASS_NO_WRITE "
        f"runtime={result['runtime_package_count']} "
        f"excluded={result['excluded_package_count']} "
        f"files={result['tracked_content_file_count']}")


if __name__ == "__main__":
    main()
