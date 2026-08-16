"""Prove that strict Session 6 outfit validation performs no content writes.

Run inside the UE 5.8 editor or editor commandlet after the proxy assets have
been authored.  This wrapper snapshots every owned Session 6 package sidecar
and every accepted protected package, invokes the strict catalog validator,
then hashes the same scope again.  It never calls an import, factory, Modify,
PostEditChange, package-save, source-control, rename, delete, or checkout API.
Only JSON reports below ``Saved`` are written.
"""

from __future__ import annotations

import hashlib
import json
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
AUTHOR_SCRIPT = PROJECT_ROOT / "Scripts/create_dg_character_session6_outfit_assets.py"
ASSET_VALIDATOR = PROJECT_ROOT / "Scripts/validate_dg_character_session6_outfit_assets.py"
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session6OutfitNoWriteValidation.json"
)
OWNED_ROOTS = (
    PROJECT_ROOT / "Content/DiscGolf/Outfits",
    PROJECT_ROOT / "Content/DiscGolf/Materials/Outfits",
)
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _tracked_files(protected_assets: dict[str, str]) -> list[Path]:
    paths = {PROJECT_ROOT / relative for relative in protected_assets}
    for root in OWNED_ROOTS:
        if root.is_dir():
            paths.update(
                path for path in root.rglob("*")
                if path.is_file() and path.suffix.casefold() in PACKAGE_SUFFIXES)
    return sorted(paths)


def _snapshot(protected_assets: dict[str, str]) -> dict[str, dict]:
    result = {}
    for path in _tracked_files(protected_assets):
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        if path.is_file():
            stat = path.stat()
            result[relative] = {
                "bytes": stat.st_size,
                "sha256": _sha256(path),
            }
        else:
            result[relative] = {"missing": True}
    return result


def main() -> None:
    author = runpy.run_path(
        str(AUTHOR_SCRIPT), run_name="dg_session6_no_write_author_contract")
    protected_assets = dict(author["PROTECTED_ASSETS"])
    before = _snapshot(protected_assets)
    asset_registry_before = tuple(sorted(
        str(path) for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Outfits", recursive=True, include_folder=False)))

    validator = runpy.run_path(
        str(ASSET_VALIDATOR), run_name="dg_session6_no_write_asset_validation")
    validation = validator["validate"]()

    asset_registry_after = tuple(sorted(
        str(path) for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Outfits", recursive=True, include_folder=False)))
    after = _snapshot(protected_assets)
    errors = list(validation.get("errors", []))
    if validation.get("status") != "PASS_NO_DISK_MUTATION":
        errors.append(
            f"Strict asset validator status was {validation.get('status')}, "
            "expected PASS_NO_DISK_MUTATION")
    if validation.get("asset_save_calls") != 0:
        errors.append("Strict asset validator reported a package-save call")
    if validation.get("asset_import_calls") != 0:
        errors.append("Strict asset validator reported an import call")
    if before != after:
        before_paths = set(before)
        after_paths = set(after)
        changed = sorted(
            path for path in before_paths & after_paths if before[path] != after[path])
        errors.append(
            "Content package snapshot changed: "
            f"added={sorted(after_paths - before_paths)} "
            f"removed={sorted(before_paths - after_paths)} changed={changed}")
    if asset_registry_before != asset_registry_after:
        errors.append("/Game/DiscGolf/Outfits registry membership changed during validation")

    result = {
        "schema": "DiscGolfTour.Session6OutfitNoWriteValidation.v1",
        "status": "PASS_NO_WRITE" if not errors else "FAIL",
        "validation_status": validation.get("status"),
        "catalog": validation.get("catalog"),
        "item_count": validation.get("item_count"),
        "tracked_content_file_count": len(after),
        "asset_registry_count": len(asset_registry_after),
        "content_snapshot_before": before,
        "content_snapshot_after": after,
        "disk_mutation": "NONE" if before == after else "DETECTED",
        "asset_registry_mutation": (
            "NONE" if asset_registry_before == asset_registry_after else "DETECTED"),
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
            "Session 6 outfit no-write validation failed: " + "; ".join(errors))
    unreal.log(
        "DG_SESSION6_OUTFIT_NO_WRITE: PASS_NO_WRITE "
        f"items={result['item_count']} files={result['tracked_content_file_count']}")


if __name__ == "__main__":
    main()
