"""Read-only source-asset audit for the canonical Session 8B MetaHuman."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


CHARACTER_OBJECT_PATH = (
    "/Game/DiscGolf/Characters/MetaHuman/Source/"
    "MHC_DG_Golfer_Default.MHC_DG_Golfer_Default"
)
REPORT_RELATIVE_PATH = "CharacterFramework/Session8BMetaHumanSourceAudit.json"


def _asset_paths(root: str, token: str) -> list[str]:
    return sorted(
        path
        for path in unreal.EditorAssetLibrary.list_assets(
            root, recursive=True, include_folder=False
        )
        if token.casefold() in path.casefold()
    )


def main() -> None:
    character = unreal.load_asset(CHARACTER_OBJECT_PATH)
    if not isinstance(character, unreal.MetaHumanCharacter):
        raise RuntimeError(f"Canonical MetaHuman source is absent: {CHARACTER_OBJECT_PATH}")

    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if not subsystem.try_add_object_to_edit(character=character):
        raise RuntimeError("MetaHuman source could not be registered for read-only audit")

    try:
        constraints = []
        for constraint in subsystem.get_body_constraints(character=character):
            constraints.append(
                {
                    "name": str(constraint.name),
                    "active": bool(constraint.is_active),
                    "target": float(constraint.target_measurement),
                    "minimum": float(constraint.min_measurement),
                    "maximum": float(constraint.max_measurement),
                }
            )

        landmarks = subsystem.get_face_landmarks(character=character)
        can_build = bool(
            subsystem.can_build_meta_human(character=character, log_error=False)
        )
        report = {
            "schema_version": 1,
            "status": "PASS_READ_ONLY_SOURCE_AUDIT",
            "character_object_path": CHARACTER_OBJECT_PATH,
            "character_class": character.get_class().get_path_name(),
            "has_high_resolution_textures": bool(
                character.get_editor_property("has_high_resolution_textures")
            ),
            "can_build": can_build,
            "face_landmark_count": len(landmarks),
            "body_constraint_count": len(constraints),
            "body_constraints": constraints,
            "available_hair_wardrobe_items": _asset_paths(
                "/MetaHumanCharacter/Optional/Grooms", "WI_Hair"
            ),
            "available_beard_wardrobe_items": _asset_paths(
                "/MetaHumanCharacter/Optional/Grooms", "WI_Beard"
            ),
            "available_clothing_wardrobe_items": _asset_paths(
                "/MetaHumanCharacter/Optional/Clothing", "WI_"
            ),
            "writes": [],
        }
    finally:
        if subsystem.is_object_added_for_editing(character=character):
            subsystem.remove_object_to_edit(character=character)

    report_path = (
        Path(unreal.Paths.project_saved_dir()) / REPORT_RELATIVE_PATH
    )
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    unreal.log(
        "DG_SESSION8B_METAHUMAN_SOURCE_AUDIT: PASS "
        f"landmarks={report['face_landmark_count']} "
        f"constraints={report['body_constraint_count']} "
        f"can_build={int(report['can_build'])}"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"DG_SESSION8B_METAHUMAN_SOURCE_AUDIT: FAIL: {exc}")
        raise
