"""Repair the one retained avatar profile after the clean-room migration.

The legacy profile serialized ``ShippingSafeAssembled`` as byte 0. An early
Foundation enum revision interpreted that byte as ``Disabled`` and the bounded
Session 19 resave persisted the wrong value. Three legacy defaults that were
not serialized also adopted the clean-room class defaults. This commandlet
restores exactly those four fields on exactly that retained data asset and
emits a local receipt.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default"
)
PROFILE_FILE = (
    PROJECT_ROOT
    / "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
)
REPORT_FILE = PROJECT_ROOT / "Saved/Session19/RetainedAvatarProfileRepair.json"
PROTECTED_SAVE = (
    PROJECT_ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
)
PROTECTED_SAVE_IDENTITY = {
    "bytes": 5212,
    "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
}


def identity(path: Path) -> dict[str, object] | None:
    if not path.is_file():
        return None
    return {
        "bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
    }


def main() -> None:
    before_profile = identity(PROFILE_FILE)
    before_save = identity(PROTECTED_SAVE)
    if before_profile is None:
        raise RuntimeError(f"Retained profile is missing: {PROFILE_FILE}")
    if before_save != PROTECTED_SAVE_IDENTITY:
        raise RuntimeError(
            f"Protected production save identity differs: {before_save}"
        )

    profile = unreal.EditorAssetLibrary.load_asset(PROFILE_PACKAGE)
    if not isinstance(profile, unreal.DiscGolfAvatarBackendProfile):
        raise RuntimeError("Retained profile did not load as DiscGolfAvatarBackendProfile")

    observed = {
        name: str(profile.get_editor_property(name))
        for name in (
            "backend_id",
            "backend",
            "meta_human_runtime_mode",
            "visual_actor_class",
            "use_runtime_retargeting",
            "retarget_asset",
            "visual_body_component_tag",
            "visual_head_component_tag",
            "preferred_quality_profile_id",
            "allow_runtime_face_sculpting",
        )
    }
    unreal.log("SESSION19_RETAINED_AVATAR_PROFILE_OBSERVED=" + json.dumps(observed))

    if str(profile.get_editor_property("backend_id")) != "metahuman_assembled":
        raise RuntimeError("Retained profile backend ID is not canonical")
    backend_value = str(profile.get_editor_property("backend"))
    normalized_backend = "".join(character for character in backend_value.upper() if character.isalnum())
    if "METAHUMANPRESET" not in normalized_backend:
        raise RuntimeError(
            "Retained profile backend kind is not MetaHumanPreset: "
            f"{backend_value}"
        )
    visual_class = observed["visual_actor_class"]
    if "BP_DG_MetaHuman_Default" not in visual_class:
        raise RuntimeError(f"Retained profile visual class differs: {visual_class}")
    retarget_asset = observed["retarget_asset"]
    if "RTG_DGMaster_To_MetaHuman" not in retarget_asset:
        raise RuntimeError(f"Retained profile retarget asset differs: {retarget_asset}")
    if str(profile.get_editor_property("preferred_quality_profile_id")) != "GameplayPerformance":
        raise RuntimeError("Retained profile quality ID differs")
    if profile.get_editor_property("allow_runtime_face_sculpting"):
        raise RuntimeError("Retained profile unexpectedly permits runtime face sculpting")

    profile.modify()
    previous_values = {
        "MetaHumanRuntimeMode": observed["meta_human_runtime_mode"],
        "bUseRuntimeRetargeting": observed["use_runtime_retargeting"],
        "VisualBodyComponentTag": observed["visual_body_component_tag"],
        "VisualHeadComponentTag": observed["visual_head_component_tag"],
    }
    enum_type = getattr(unreal, "DGMetaHumanRuntimeMode", None)
    accepted_mode = (
        enum_type.SHIPPING_SAFE_ASSEMBLED
        if enum_type is not None
        else 0
    )
    profile.set_editor_property("meta_human_runtime_mode", accepted_mode)
    profile.set_editor_property("use_runtime_retargeting", True)
    profile.set_editor_property("visual_body_component_tag", "DGVisualBody")
    profile.set_editor_property("visual_head_component_tag", "DGVisualHead")
    if not unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False):
        raise RuntimeError("Failed to save the retained avatar profile")

    after_mode = str(profile.get_editor_property("meta_human_runtime_mode"))
    if "SHIPPING_SAFE_ASSEMBLED" not in after_mode.upper():
        raise RuntimeError(f"Retained profile mode did not persist in memory: {after_mode}")
    accepted_values = {
        "MetaHumanRuntimeMode": after_mode,
        "bUseRuntimeRetargeting": str(
            profile.get_editor_property("use_runtime_retargeting")
        ),
        "VisualBodyComponentTag": str(
            profile.get_editor_property("visual_body_component_tag")
        ),
        "VisualHeadComponentTag": str(
            profile.get_editor_property("visual_head_component_tag")
        ),
    }
    expected_values = {
        "bUseRuntimeRetargeting": "True",
        "VisualBodyComponentTag": "DGVisualBody",
        "VisualHeadComponentTag": "DGVisualHead",
    }
    for name, expected in expected_values.items():
        if accepted_values[name] != expected:
            raise RuntimeError(
                f"Retained profile {name} did not persist in memory: "
                f"{accepted_values[name]}"
            )
    after_profile = identity(PROFILE_FILE)
    after_save = identity(PROTECTED_SAVE)
    if after_profile is None or after_save != PROTECTED_SAVE_IDENTITY:
        raise RuntimeError("Protected production save changed during profile repair")

    report = {
        "schema": "DiscGolfTour.Session19.RetainedAvatarProfileRepair.v1",
        "status": "PASS_EXACT_PROFILE_RUNTIME_MODE_REPAIRED",
        "profilePackage": PROFILE_PACKAGE,
        "properties": list(previous_values),
        "previousValues": previous_values,
        "acceptedValues": accepted_values,
        "before": before_profile,
        "after": after_profile,
        "protectedSave": after_save,
    }
    REPORT_FILE.parent.mkdir(parents=True, exist_ok=True)
    REPORT_FILE.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log("SESSION19_RETAINED_AVATAR_PROFILE_REPAIR=" + json.dumps(report))


main()
