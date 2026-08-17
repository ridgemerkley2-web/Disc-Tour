"""Prove the Session 7 customization validator performs no content writes.

The wrapper snapshots the complete two owned content roots plus every frozen
Session 6 boundary package, runs strict validation, and compares bytes, hashes,
and Asset Registry membership.  Only JSON reports below ``Saved`` are written.
"""

from __future__ import annotations

import hashlib
import json
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
AUTHOR_SCRIPT = PROJECT_ROOT / "Scripts/create_dg_character_session7_customization_assets.py"
ASSET_VALIDATOR = (
    PROJECT_ROOT / "Scripts/validate_dg_character_session7_customization_assets.py"
)
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session7CustomizationNoWriteValidation.json"
)
OWNED_DISK_ROOTS = (
    PROJECT_ROOT / "Content/DiscGolf/Characters/Customization",
    PROJECT_ROOT / "Content/DiscGolf/Materials/CharacterCustomization",
)
OWNED_ASSET_ROOTS = (
    "/Game/DiscGolf/Characters/Customization",
    "/Game/DiscGolf/Materials/CharacterCustomization",
)
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _tracked_paths(protected_assets: dict[str, str]) -> list[Path]:
    paths = {PROJECT_ROOT / relative for relative in protected_assets}
    for root in OWNED_DISK_ROOTS:
        if root.is_dir():
            paths.update(
                path for path in root.rglob("*")
                if path.is_file() and path.suffix.casefold() in PACKAGE_SUFFIXES)
    return sorted(paths)


def _snapshot(protected_assets: dict[str, str]) -> dict[str, dict]:
    result = {}
    for path in _tracked_paths(protected_assets):
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        if path.is_file():
            stat = path.stat()
            result[relative] = {"bytes": stat.st_size, "sha256": _sha256(path)}
        else:
            result[relative] = {"missing": True}
    return result


def _registry_snapshot() -> dict[str, list[str]]:
    return {
        root: sorted(str(path) for path in unreal.EditorAssetLibrary.list_assets(
            root, recursive=True, include_folder=False))
        for root in OWNED_ASSET_ROOTS
    }


def main() -> None:
    author = runpy.run_path(
        str(AUTHOR_SCRIPT), run_name="dg_session7_no_write_author_contract")
    protected_assets = dict(author["PROTECTED_ASSETS"])
    before = _snapshot(protected_assets)
    registry_before = _registry_snapshot()

    validator = runpy.run_path(
        str(ASSET_VALIDATOR), run_name="dg_session7_no_write_asset_validation")
    validation = validator["validate"]()

    after = _snapshot(protected_assets)
    registry_after = _registry_snapshot()
    errors = list(validation.get("errors", []))
    if validation.get("status") != "PASS_NO_DISK_MUTATION":
        errors.append(
            f"Strict asset validator status was {validation.get('status')}, "
            "expected PASS_NO_DISK_MUTATION")
    for field in (
            "asset_save_calls", "asset_import_calls", "asset_factory_calls",
            "asset_delete_calls"):
        if validation.get(field) != 0:
            errors.append(f"Strict asset validator reported nonzero {field}")
    if before != after:
        before_paths = set(before)
        after_paths = set(after)
        changed = sorted(
            path for path in before_paths & after_paths if before[path] != after[path])
        errors.append(
            "Content package snapshot changed: "
            f"added={sorted(after_paths - before_paths)} "
            f"removed={sorted(before_paths - after_paths)} changed={changed}")
    if registry_before != registry_after:
        errors.append("Owned Session 7 Asset Registry membership changed during validation")

    result = {
        "schema": "DiscGolfTour.Session7CustomizationNoWriteValidation.v1",
        "status": "PASS_NO_WRITE" if not errors else "FAIL",
        "validation_status": validation.get("status"),
        "catalog": validation.get("catalog"),
        "catalog_item_count": validation.get("catalog_item_count"),
        "visible_morph_count": len(validation.get("visible_morphs", [])),
        "deferred_visual_morph_count": len(
            validation.get("deferred_visual_morphs", [])),
        "tracked_content_file_count": len(after),
        "asset_registry_counts": {
            root: len(paths) for root, paths in registry_after.items()},
        "content_snapshot_before": before,
        "content_snapshot_after": after,
        "asset_registry_before": registry_before,
        "asset_registry_after": registry_after,
        "disk_mutation": "NONE" if before == after else "DETECTED",
        "asset_registry_mutation": (
            "NONE" if registry_before == registry_after else "DETECTED"),
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "asset_factory_calls": 0,
        "asset_delete_calls": 0,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError(
            "Session 7 customization no-write validation failed: "
            + "; ".join(errors))
    unreal.log(
        "DG_SESSION7_CUSTOMIZATION_NO_WRITE: PASS_NO_WRITE "
        f"items={result['catalog_item_count']} files={result['tracked_content_file_count']}")


if __name__ == "__main__":
    main()
