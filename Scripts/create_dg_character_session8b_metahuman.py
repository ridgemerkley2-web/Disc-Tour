"""Create or open the canonical Session 8B MetaHuman Character source asset.

This script is intentionally editor-only. It proves that the explicitly enabled
UE 5.8.1 MetaHuman Creator and Core Data can initialize a real project asset.
Cloud auto-rigging, texture-source download, and assembly are separate guarded
phases and are never silently inferred from successful source-asset creation.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


CHARACTER_PACKAGE_PATH = "/Game/DiscGolf/Characters/MetaHuman/Source"
CHARACTER_ASSET_NAME = "MHC_DG_Golfer_Default"
CHARACTER_OBJECT_PATH = (
    f"{CHARACTER_PACKAGE_PATH}/{CHARACTER_ASSET_NAME}.{CHARACTER_ASSET_NAME}"
)
REPORT_RELATIVE_PATH = "CharacterFramework/Session8BMetaHumanCreateOpen.json"
SUCCESS_MARKER = "DG_SESSION8B_METAHUMAN_CREATE_OPEN: PASS"


def _write_report(payload: dict[str, object]) -> Path:
    report_path = Path(unreal.Paths.project_saved_dir()) / REPORT_RELATIVE_PATH
    report_path.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    report_path.write_text(serialized, encoding="utf-8", newline="\n")
    return report_path


def _package_filename() -> Path:
    content_dir = Path(unreal.Paths.project_content_dir())
    relative = CHARACTER_OBJECT_PATH.removeprefix("/Game/").split(".", 1)[0]
    return content_dir / f"{relative}.uasset"


def main() -> None:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = False

    character = unreal.load_asset(CHARACTER_OBJECT_PATH)
    if character is None:
        character = asset_tools.create_asset(
            asset_name=CHARACTER_ASSET_NAME,
            package_path=CHARACTER_PACKAGE_PATH,
            asset_class=unreal.MetaHumanCharacter,
            factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew),
        )
        created = True

    if character is None:
        raise RuntimeError(f"Failed to create {CHARACTER_OBJECT_PATH}")
    if not isinstance(character, unreal.MetaHumanCharacter):
        raise RuntimeError(
            f"Canonical path resolved to {type(character).__name__}, not MetaHumanCharacter"
        )

    if created and not unreal.EditorAssetLibrary.save_loaded_asset(
        character, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Failed to save newly created {CHARACTER_OBJECT_PATH}")

    asset_editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    opened = bool(asset_editor_subsystem.open_editor_for_assets(assets=[character]))
    if not opened:
        raise RuntimeError(f"Failed to open {CHARACTER_OBJECT_PATH} in MetaHuman Creator")

    package_filename = _package_filename()
    if not package_filename.is_file():
        raise RuntimeError(f"Saved package is absent: {package_filename}")

    package_bytes = package_filename.read_bytes()
    report = {
        "schema_version": 1,
        "status": "PASS_CREATED_AND_OPENED" if created else "PASS_ALREADY_EXISTS_AND_OPENED",
        "character_object_path": CHARACTER_OBJECT_PATH,
        "character_class": character.get_class().get_path_name(),
        "created": created,
        "editor_open_requested": opened,
        "package_filename": str(package_filename),
        "package_size": len(package_bytes),
        "package_sha256": hashlib.sha256(package_bytes).hexdigest().upper(),
        "cloud_auto_rig_requested": False,
        "cloud_texture_sources_requested": False,
        "assembled": False,
        "qualification": (
            "SOURCE_ASSET_ONLY_NOT_RIGGED_NOT_TEXTURE_DOWNLOADED_NOT_ASSEMBLED"
        ),
    }
    report_path = _write_report(report)
    unreal.log(
        f"{SUCCESS_MARKER} created={int(created)} path={CHARACTER_OBJECT_PATH} "
        f"report={report_path}"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"DG_SESSION8B_METAHUMAN_CREATE_OPEN: FAIL: {exc}")
        raise
