#!/usr/bin/env python3
"""Source-only Session 7 full-character wiring and authority audit.

This validator intentionally performs no Unreal, UBT, Blender, asset, or
network work.  It checks that Session 7 extends the accepted pawn, creator,
profile, outfit, and RHBH paths; freezes the seven-tab/schema/morph/catalog
contracts; and verifies that the dedicated runtime and rendered harnesses are
present without introducing gameplay authority.
"""

from __future__ import annotations

import ast
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "Saved/CharacterFramework/Session7FullCharacterWiringReport.json"
SESSION6_DOC_CHECKPOINT = "ec02860ddeec6cf59891d3c4aaaaf474d4b282fd"
SESSION7_CHECKPOINT = "2c54be19a119264d42f11db5470399e021d050cd"

FULL_SCHEMA = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/"
    "DG_FullCharacterCreatorSchema.json"
)
FACE_CONTRACT = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/"
    "DG_FaceMorphContract.json"
)
FACE_PRESETS = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Customization/"
    "face_presets.json"
)
SOURCE_SPEC = (
    ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy/"
    "proxy_customization_catalog_spec.json"
)

TABS = (
    "Identity", "Body", "Face", "Hair", "Appearance", "Throw Style", "Outfit",
)
EXPECTED_MATRIX_ROWS = (
    ("BaselineDefaultShortSimple", "Baseline"),
    ("BaselineSquareBeardHatFull", "Baseline"),
    ("ShortNarrowMedium", "ShortCompact"),
    ("TallRoundBeardFull", "TallLongArms"),
    ("BodyFaceExtremes", "SliderMax"),
    ("MissingHair", "Baseline"),
    ("MissingFacialHairEyebrow", "Baseline"),
    ("MissingScarTattooOutfit", "Baseline"),
    ("Schema8Migration", "Baseline"),
    ("RandomizeApplyReload", "Baseline"),
    ("RandomizeCancel", "Baseline"),
    ("CompleteCharacterRHBH", "Baseline"),
)
CAPTURE_FILENAMES = tuple(
    f"{index:02d}_{name}.png" for index, name in enumerate((
        "Full_Creator_Overview", "Identity_Tab", "Body_Tab", "Face_Tab",
        "Hair_Tab", "Appearance_Tab", "Throw_Style_Tab", "Outfit_Tab",
        "Face_Default", "Face_Square", "Face_Narrow", "Face_Round",
        "Hair_Short", "Hair_Medium", "Facial_Hair", "Multiple_Skin_Tones",
        "Multiple_Eye_Colors", "Hat_Hides_Hair",
        "Hat_Removed_Hair_Restored", "ShortCompact_Complete",
        "Baseline_Complete", "TallLongArms_Complete",
        "Complete_RHBH_Release", "Complete_RHBH_FollowThrough",
    ), 1)
)
EXPECTED_VISIBLE_CONTROLS = (
    ("DisplayName", "Handedness", "Voice", "Pronouns"),
    ("Height", "Wingspan", "ShoulderWidth", "TorsoLength", "LegLength",
     "HandScale", "Mass", "Muscularity", "BodyFat", "Chest", "Waist",
     "Hips", "Arms", "Legs", "BodyPresets"),
    ("FacePresets", "HeadWidth", "HeadHeight", "BrowHeight", "BrowDepth",
     "EyeSize", "EyeSpacing", "EyeDepth", "NoseWidth", "NoseLength",
     "NoseBridge", "CheekWidth", "CheekFullness", "JawWidth", "JawHeight",
     "ChinWidth", "ChinLength", "MouthWidth", "LipFullness", "EarSize",
     "EarAngle"),
    ("HairStyle", "FacialHair", "Eyebrow", "HairColor",
     "FacialHairColor", "EyebrowColor"),
    ("SkinTone", "EyeColor", "Complexion", "Freckles", "SunExposure",
     "Scar", "Tattoo"),
    ("RunUp", "ReachBack", "TorsoRotation", "Brace", "Explosiveness",
     "FollowThrough"),
    ("Headwear", "Eyewear", "Top", "Outerwear", "Bottom", "Socks",
     "Footwear", "Glove", "Wrist", "Bag", "Accessory", "Item", "Variant"),
)
VISIBLE_FACE_KEYS = {
    "head_width", "head_height", "cheek_fullness", "jaw_width", "chin_length",
}
STABLE_IDS = (
    "hair_none", "hair_short", "hair_medium", "hair_mohawk",
    "facialhair_none", "facialhair_stubble", "facialhair_beard",
    "brow_default", "brow_alt", "scar_none", "scar_proxy",
    "tattoo_none", "tattoo_proxy", "voice_default", "voice_alt",
    "pronouns_default", "pronouns_they_them",
)
CATALOG_IDS = set(STABLE_IDS)
CANONICAL_PATHS = {
    "catalog": (
        "/Game/DiscGolf/Characters/Customization/Data/"
        "DA_DG_CosmeticCatalog.DA_DG_CosmeticCatalog"
    ),
    "head": (
        "/Game/DiscGolf/Characters/Customization/Head/"
        "SK_DG_Head_Proxy.SK_DG_Head_Proxy"
    ),
    "head_material": (
        "/Game/DiscGolf/Materials/CharacterCustomization/"
        "M_DG_HeadProxy.M_DG_HeadProxy"
    ),
    "hair_material": (
        "/Game/DiscGolf/Materials/CharacterCustomization/"
        "M_DG_HairProxy.M_DG_HairProxy"
    ),
}

REQUIRED_SESSION7_PATHS = (
    "Source/DiscGolfTour/DiscGolfFullCharacterRuntime.h",
    "Source/DiscGolfTour/DiscGolfFullCharacterRuntime.cpp",
    "Source/DiscGolfTour/DiscGolfSession7FullCharacterSmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfSession7FullCharacterSmokeRunner.cpp",
    "Source/DiscGolfTour/DiscGolfSession7FullCharacterVisualCaptureRunner.h",
    "Source/DiscGolfTour/DiscGolfSession7FullCharacterVisualCaptureRunner.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfFullCharacterRuntimeTests.cpp",
    "Scripts/run-session7-full-character-matrix.py",
    "Scripts/run-session7-full-character-visual-capture.py",
    "Scripts/validate_dg_character_session7_plugin_contract.py",
)


def _read(relative: str) -> str:
    path = ROOT / relative
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def _python_assignment(source: str, name: str):
    try:
        module = ast.parse(source)
        for node in module.body:
            if (isinstance(node, ast.Assign)
                    and any(isinstance(target, ast.Name) and target.id == name
                            for target in node.targets)):
                return ast.literal_eval(node.value)
            if (isinstance(node, ast.AnnAssign)
                    and isinstance(node.target, ast.Name)
                    and node.target.id == name):
                return ast.literal_eval(node.value)
    except (SyntaxError, ValueError):
        return None
    return None


def _cpp_text_array(source: str, name: str) -> tuple[str, ...]:
    match = re.search(
        rf"\b{re.escape(name)}\s*\[\s*\]\s*=\s*\{{(?P<body>.*?)\}}\s*;",
        source, re.DOTALL,
    )
    return tuple(re.findall(r'TEXT\("([^"]+)"\)', match.group("body"))) \
        if match else ()


def _cpp_control_cases(
    source: str, begin: str, end: str
) -> tuple[tuple[str, ...], ...]:
    if begin not in source or end not in source.split(begin, 1)[-1]:
        return ()
    block = source.split(begin, 1)[-1].split(end, 1)[0]
    cases = {
        int(index): tuple(re.findall(r'TEXT\("([^"]+)"\)', body))
        for index, body in re.findall(
            r"case\s+(\d+)\s*:(.*?)break\s*;", block, re.DOTALL
        )
    }
    return tuple(cases.get(index, ()) for index in range(7))


def _record(
    checks: list[dict], check_id: str, errors: list[str], evidence: dict
) -> None:
    checks.append({
        "id": check_id,
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "evidence": evidence,
    })


def _git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=30.0, check=False,
    )


def _added_lines_since(checkpoint: str, relative: str) -> str:
    result = _git(
        "diff", "--unified=0", checkpoint, "--", relative,
    )
    if result.returncode != 0:
        return ""
    return "\n".join(
        line[1:] for line in result.stdout.splitlines()
        if line.startswith("+") and not line.startswith("+++")
    )


def _added_lines(relative: str) -> str:
    return _added_lines_since(SESSION6_DOC_CHECKPOINT, relative)


def main() -> int:
    checks: list[dict] = []

    missing_paths = [
        path for path in REQUIRED_SESSION7_PATHS if not (ROOT / path).is_file()
    ]
    _record(
        checks, "required_session7_source_and_harness_paths",
        [f"missing path: {path}" for path in missing_paths],
        {"required_count": len(REQUIRED_SESSION7_PATHS), "missing": missing_paths},
    )

    contract_errors: list[str] = []
    try:
        schema = json.loads(FULL_SCHEMA.read_text(encoding="utf-8"))
        face = json.loads(FACE_CONTRACT.read_text(encoding="utf-8"))
        presets = json.loads(FACE_PRESETS.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        schema = face = presets = {}
        contract_errors.append(f"BuildKit creator contract could not load: {exc}")
    schema_tabs = tuple(schema.get("creator_tabs", []))
    face_keys = set(face.get("morphs", {}))
    preset_ids = {
        row.get("id") for row in presets.get("presets", []) if isinstance(row, dict)
    }
    if schema_tabs != TABS:
        contract_errors.append(f"creator tab order differs: {schema_tabs}")
    if len(face_keys) != 20:
        contract_errors.append(f"face contract has {len(face_keys)} keys, expected 20")
    if not VISIBLE_FACE_KEYS <= face_keys:
        contract_errors.append("visible proxy face keys are outside the frozen contract")
    if preset_ids != {"face_default", "face_square", "face_narrow", "face_round"}:
        contract_errors.append(f"face preset IDs differ: {sorted(preset_ids)}")
    _record(checks, "installed_buildkit_creator_contract", contract_errors, {
        "tabs": list(schema_tabs),
        "face_key_count": len(face_keys),
        "visible_proxy_keys": sorted(VISIBLE_FACE_KEYS),
        "deferred_proxy_keys": sorted(face_keys - VISIBLE_FACE_KEYS),
        "preset_ids": sorted(value for value in preset_ids if value),
    })

    runtime_h = _read("Source/DiscGolfTour/DiscGolfFullCharacterRuntime.h")
    runtime_cpp = _read("Source/DiscGolfTour/DiscGolfFullCharacterRuntime.cpp")
    runtime = runtime_h + "\n" + runtime_cpp
    runtime_tokens = (
        "CosmeticCatalogObjectPath", "HeadMeshObjectPath",
        "MakeDefaultCustomization", "NormalizeForPersistence",
        "AreCustomizationsEquivalent", "ApplyFacePreset", "ResolveForRuntime",
        "GetCosmeticOptions", "SetCosmeticSelection", "Randomize",
        "FDiscGolfCharacterRandomizeLocks", "GetFaceMorphKeys",
        "GetVisibleProxyFaceMorphKeys", "PowerMultiplier", "SpinMultiplier",
        "hair_none", "facialhair_none", "brow_default", "scar_none",
        "tattoo_none", "voice_default", "pronouns_default",
    )
    runtime_missing = [token for token in runtime_tokens if token not in runtime]
    runtime_errors = [f"full-character runtime missing: {token}" for token in runtime_missing]
    for label in ("catalog", "head"):
        if CANONICAL_PATHS[label] not in runtime:
            runtime_errors.append(
                f"full-character runtime lacks canonical {label} path "
                f"{CANONICAL_PATHS[label]}"
            )
    if not all(key in runtime for key in face_keys):
        runtime_errors.append("runtime does not enumerate all 20 face keys")
    if not all(key in runtime for key in VISIBLE_FACE_KEYS):
        runtime_errors.append("runtime does not enumerate all five visible proxy keys")
    if not all(item_id in runtime for item_id in (
        "hair_none", "facialhair_none", "brow_default", "scar_none",
        "tattoo_none", "voice_default", "pronouns_default",
    )):
        runtime_errors.append("runtime fallback IDs are incomplete")
    _record(checks, "single_project_full_character_adapter", runtime_errors, {
        "missing_tokens": runtime_missing,
        "face_keys_present": sorted(key for key in face_keys if key in runtime),
        "visible_keys_present": sorted(key for key in VISIBLE_FACE_KEYS if key in runtime),
    })

    pawn_h = _read("Source/DiscGolfTour/DiscGolferPawn.h")
    pawn_cpp = _read("Source/DiscGolfTour/DiscGolferPawn.cpp")
    pawn = pawn_h + "\n" + pawn_cpp
    pawn_tokens = (
        "GetCharacterCustomizationComponent", "GetModularHeadMesh",
        "GetCosmeticCatalog", "GetCurrentFullCharacterCustomization",
        "ApplyFullCharacterCustomizationTransactionally",
        "PreviewFullCharacterCustomization",
        "IsHairHiddenByOutfitCoverage",
        "EDGBodyRegion::Hair",
        "hair_none",
        "UDiscGolfCharacterCustomizationComponent",
        "UDiscGolfOutfitComponent",
    )
    pawn_missing = [token for token in pawn_tokens if token not in pawn]
    pawn_errors = [f"pawn missing full-character seam: {token}" for token in pawn_missing]
    customization_creations = pawn_cpp.count(
        "CreateDefaultSubobject<UDiscGolfCharacterCustomizationComponent>")
    outfit_creations = pawn_cpp.count("CreateDefaultSubobject<UDiscGolfOutfitComponent>")
    if customization_creations != 1:
        pawn_errors.append(
            f"pawn customization-component creation count {customization_creations} != 1"
        )
    if outfit_creations != 1:
        pawn_errors.append(f"pawn outfit-component creation count {outfit_creations} != 1")
    hair_rebuild = pawn_cpp.split(
        "void ADiscGolferPawn::RebuildCustomizationHairForCoverage()", 1
    )[-1].split("void ADiscGolferPawn::RefreshCharacterProfilePresentation", 1)[0]
    selected_hair_save = hair_rebuild.find(
        "const FName SelectedHairStyleId =")
    temporary_none = hair_rebuild.find(
        'CharacterCustomization->Current.Hair.HairStyleId = TEXT("hair_none");')
    rebuild_hair = hair_rebuild.find(
        "CharacterCustomization->RebuildHair(ModularHeadMesh);")
    restore_hair = hair_rebuild.find(
        "CharacterCustomization->Current.Hair.HairStyleId = SelectedHairStyleId;")
    if (min(selected_hair_save, temporary_none, rebuild_hair, restore_hair) < 0
            or not (selected_hair_save < temporary_none < rebuild_hair < restore_hair)
            or hair_rebuild.count(
                'CharacterCustomization->Current.Hair.HairStyleId = TEXT("hair_none");'
            ) != 1
            or hair_rebuild.count(
                "CharacterCustomization->Current.Hair.HairStyleId = SelectedHairStyleId;"
            ) != 1):
        pawn_errors.append(
            "hair coverage must save the stable ID, substitute hair_none only "
            "for one RebuildHair call, then restore the exact selected ID")
    coverage_handler = pawn_cpp.split(
        "void ADiscGolferPawn::HandleOutfitCoverageChanged(", 1
    )[-1].split("void ADiscGolferPawn::ApplyFullCustomizationVisuals", 1)[0]
    if ("CoveredOutfitBodyRegions.Contains(EDGBodyRegion::Hair)"
            not in coverage_handler
            or "RebuildCustomizationHairForCoverage();" not in coverage_handler):
        pawn_errors.append(
            "outfit Hair coverage must be the sole trigger for the project-side "
            "hair visibility rebuild")
    _record(checks, "one_existing_pawn_one_customization_and_outfit_component", pawn_errors, {
        "missing_tokens": pawn_missing,
        "customization_component_creations": customization_creations,
        "outfit_component_creations": outfit_creations,
        "hair_coverage_save_temp_rebuild_restore": (
            0 <= selected_hair_save < temporary_none < rebuild_hair < restore_hair
        ),
    })

    save_h = _read("Source/DiscGolfTour/DiscGolfSaveGame.h")
    instance_h = _read("Source/DiscGolfTour/DiscGolfTourGameInstance.h")
    instance_cpp = _read("Source/DiscGolfTour/DiscGolfTourGameInstance.cpp")
    persistence = save_h + "\n" + instance_h + "\n" + instance_cpp
    persistence_tokens = (
        "CurrentVersion = 9", "FDGFullCharacterCustomization",
        "SaveGame", "MigrateToCurrent", "SaveSchemaVersion < 9",
        "GetFullCharacterCustomization", "UpdateFullCharacterCustomization",
        "TryResolveSession7FullCharacterValidationSaveSlot",
        "GetSession7FullCharacterValidationSaveSlot",
        "Session7FullCharacterValidationNoSave",
        "DiscGolfTour_Automation_Session7FullCharacter_",
    )
    persistence_missing = [
        token for token in persistence_tokens if token not in persistence
    ]
    persistence_errors = [
        f"schema-9 persistence missing: {token}" for token in persistence_missing
    ]
    if "UpdateCharacterProfileAndOutfit" not in persistence:
        persistence_errors.append("accepted schema-8 atomic update seam disappeared")
    _record(checks, "schema9_migration_and_isolated_validation_slot", persistence_errors, {
        "missing_tokens": persistence_missing,
        "keeps_schema8_atomic_seam": "UpdateCharacterProfileAndOutfit" in persistence,
    })

    controller_h = _read("Source/DiscGolfTour/DiscGolfTourPlayerController.h")
    controller_cpp = _read("Source/DiscGolfTour/DiscGolfTourPlayerController.cpp")
    widget_h = _read("Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.h")
    widget_cpp = _read("Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp")
    creator = controller_h + controller_cpp + widget_h + widget_cpp
    creator_tokens = (
        "PreviewFullCharacterCreatorDraft", "ApplyFullCharacterCreatorDraft",
        "ResetCharacterCreatorCurrentTab", "ResetCharacterCreatorAll",
        "RandomizeCharacterCreatorDraft", "GetCharacterCreatorDraftCustomization",
        "ZoomCharacterCreatorPreview", "PrepareSession7VisualEvidence",
        "RESET CURRENT TAB", "RESET ALL", "RANDOMIZE", "CANCEL",
        "SAVE", "ZOOM IN", "ZOOM OUT", "ROTATE LEFT", "ROTATE RIGHT",
    )
    creator_missing = [token for token in creator_tokens if token not in creator]
    creator_errors = [f"creator missing: {token}" for token in creator_missing]
    missing_tab_labels = [tab for tab in TABS if tab.upper() not in widget_cpp.upper()]
    if missing_tab_labels:
        creator_errors.append(f"creator missing tab labels: {missing_tab_labels}")
    if "TryStartAnimatedRHBHThrow" in widget_cpp:
        creator_errors.append("widget directly starts a full throw while editing")
    if "SpawnActor<ADiscGolferPawn>" in creator:
        creator_errors.append("creator spawns a competing player pawn")
    widget_control_cases = _cpp_control_cases(
        widget_cpp,
        "void UDiscGolfCharacterCreatorWidget::GetSession7VisibleControlIds(",
        "TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::RebuildWidget()",
    )
    if widget_control_cases != EXPECTED_VISIBLE_CONTROLS:
        creator_errors.append(
            "widget active-tab visible-control IDs/order differ from the frozen seven-tab contract")
    controller_control_forwarder = controller_cpp.split(
        "void ADiscGolfTourPlayerController::GetSession7CharacterCreatorVisibleControlIds(",
        1,
    )[-1].split(
        "void ADiscGolfTourPlayerController::CloseCharacterCreator", 1
    )[0]
    if not all(token in controller_control_forwarder for token in (
        "OutControlIds.Reset();",
        "if (bCharacterCreatorOpen && CharacterCreatorWidget)",
        "CharacterCreatorWidget->GetSession7VisibleControlIds(OutControlIds);",
    )):
        creator_errors.append(
            "controller visible-control forwarder is not closed-state-empty and widget-authoritative")
    _record(checks, "seven_tab_creator_and_global_actions", creator_errors, {
        "missing_tokens": creator_missing,
        "missing_tab_labels": missing_tab_labels,
        "expected_visible_control_counts": [
            len(controls) for controls in EXPECTED_VISIBLE_CONTROLS
        ],
        "actual_visible_control_counts": [
            len(controls) for controls in widget_control_cases
        ],
        "visible_control_contract_exact": (
            widget_control_cases == EXPECTED_VISIBLE_CONTROLS
        ),
        "widget_auto_throw_calls": widget_cpp.count("TryStartAnimatedRHBHThrow"),
        "creator_competing_pawn_spawns": creator.count("SpawnActor<ADiscGolferPawn>"),
    })

    plugin_customization = _read(
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Private/DiscGolfCharacterCustomizationComponent.cpp"
    )
    material_tokens = (
        "DG_SkinTone", "DG_EyeColor", "DG_Complexion", "DG_Freckles",
        "DG_SunExposure", "DG_HairColor", "DG_ScarProxy", "DG_TattooProxy",
    )
    appearance_sources = plugin_customization + "\n" + pawn_cpp
    material_missing = [
        token for token in material_tokens if token not in appearance_sources
    ]
    material_errors = [
        f"customization component missing material parameter: {token}"
        for token in material_missing
    ]
    _record(checks, "head_appearance_and_hair_parameter_contract", material_errors, {
        "head_material": CANONICAL_PATHS["head_material"],
        "hair_material": CANONICAL_PATHS["hair_material"],
        "missing_parameters": material_missing,
        "body_surface_visual_proof": "DEFERRED_ACCEPTED_MASTER_HAS_NO_USABLE_MATERIAL_SLOT",
    })

    game_mode_h = _read("Source/DiscGolfTour/DiscGolfTourGameMode.h")
    game_mode_cpp = _read("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
    game_mode = game_mode_h + game_mode_cpp
    game_mode_tokens = (
        "Session7FullCharacterThrowSmokeTest",
        "ADiscGolfSession7FullCharacterSmokeRunner",
        "Session7FullCharacterVisualCapture",
        "ADiscGolfSession7FullCharacterVisualCaptureRunner",
        "Session7FullCharacterValidationNoSave",
        "Session6OutfitValidationNoSave",
        "Session8ValidationNoSave",
        "Session8CookClosureSmokeTest",
    )
    game_mode_missing = [token for token in game_mode_tokens if token not in game_mode]
    game_mode_errors = [
        f"GameMode Session 7 harness wiring missing: {token}"
        for token in game_mode_missing
    ]
    save_function = game_mode_cpp.split(
        "void ADiscGolfTourGameMode::SavePracticeRoundSnapshot()", 1
    )[-1].split("UDiscGolfTourGameInstance* GameInstance", 1)[0]
    compact_save_function = re.sub(r"\s+", "", save_function)
    expected_guard_boundaries = (
        (
            "constboolbSession6OutfitValidationNoSave=FParse::Param("
            'FCommandLine::Get(),TEXT("Session6OutfitValidationNoSave"))'
            "&&(FParse::Param(FCommandLine::Get(),"
            'TEXT("Session6OutfitThrowSmokeTest"))||FParse::Param('
            'FCommandLine::Get(),TEXT("Session6OutfitVisualCapture")));'
        ),
        (
            "constboolbSession7FullCharacterValidationNoSave=FParse::Param("
            'FCommandLine::Get(),TEXT("Session7FullCharacterValidationNoSave"))'
            "&&(FParse::Param(FCommandLine::Get(),"
            'TEXT("Session7FullCharacterThrowSmokeTest"))!=FParse::Param('
            'FCommandLine::Get(),TEXT("Session7FullCharacterVisualCapture")));'
        ),
        (
            "constboolbSession8CookClosureValidationNoSave=FParse::Param("
            'FCommandLine::Get(),TEXT("Session8ValidationNoSave"))'
            "&&FParse::Param(FCommandLine::Get(),"
            'TEXT("Session8CookClosureSmokeTest"));'
        ),
    )
    for guard in expected_guard_boundaries:
        if compact_save_function.count(guard) != 1:
            game_mode_errors.append(
                "practice-save boundary must contain each exact Session6/"
                "Session7/Session8 guarded no-save conjunction exactly once")
    expected_combined_boundary = (
        "if(bRegressionActive||bSession6OutfitValidationNoSave"
        "||bSession7FullCharacterValidationNoSave"
        "||bSession8CookClosureValidationNoSave)return;")
    if compact_save_function.count(expected_combined_boundary) != 1:
        game_mode_errors.append(
            "practice-save boundary must contain the single combined "
            "regression/Session6/Session7/Session8 early return")
    if compact_save_function.count("return;") != 1:
        game_mode_errors.append(
            "practice-save guard prefix must contain exactly one early return")
    for flag in (
        "Session6OutfitValidationNoSave",
        "Session7FullCharacterValidationNoSave",
        "Session8ValidationNoSave",
    ):
        if game_mode_cpp.count(f'TEXT("{flag}")') != 1:
            game_mode_errors.append(
                f"GameMode must contain exactly one {flag} flag parse")
    _record(checks, "explicit_session7_harness_and_no_save_boundary", game_mode_errors, {
        "missing_tokens": game_mode_missing,
        "save_function_has_session7_guard": all(
            token in save_function for token in (
                "Session7FullCharacterValidationNoSave",
                "Session7FullCharacterThrowSmokeTest",
                "Session7FullCharacterVisualCapture",
            )
        ),
        "exact_guard_occurrences": {
            f"session{index + 6}": compact_save_function.count(guard)
            for index, guard in enumerate(expected_guard_boundaries)
        },
        "combined_boundary_occurrences": compact_save_function.count(
            expected_combined_boundary),
        "early_return_occurrences": compact_save_function.count("return;"),
    })

    smoke = _read("Source/DiscGolfTour/DiscGolfSession7FullCharacterSmokeRunner.cpp")
    matrix = _read("Scripts/run-session7-full-character-matrix.py")
    required_rows = (
        "BaselineDefaultShortSimple", "BaselineSquareBeardHatFull",
        "ShortNarrowMedium", "TallRoundBeardFull", "BodyFaceExtremes",
        "MissingHair", "MissingFacialHairEyebrow",
        "MissingScarTattooOutfit", "Schema8Migration",
        "RandomizeApplyReload", "RandomizeCancel", "CompleteCharacterRHBH",
    )
    smoke_tokens = (
        "Super::Start()", "Super::Pass()", "Super::Fail(",
        "disc_grip_r", "LeaderPoseComponent.Get()", "GetCollisionEnabled",
        "one_release=1", "one_authoritative_disc=1", "one_completed_flight=1",
        "one_recovery=1", "hair_hat_selection_preserved=1",
        "appearance_authority_unchanged=1", "schema8_migration=1",
        "cancel_restored_exact=1", "missing_fallback=1",
    )
    smoke_missing = [token for token in smoke_tokens if token not in smoke]
    matrix_missing = [row for row in required_rows if row not in matrix]
    harness_errors = [f"Session 7 smoke missing: {token}" for token in smoke_missing]
    harness_errors.extend(f"Session 7 matrix missing row: {row}" for row in matrix_missing)
    forbidden_harness_writes = (
        "SavePackage", "MarkPackageDirty", "CreatePackage(",
        "SaveProfile(", "DiscGolfTour_Profile_0",
    )
    smoke_forbidden_writes = [
        token for token in forbidden_harness_writes if token in smoke
    ]
    if smoke_forbidden_writes:
        harness_errors.append(
            f"smoke contains production/package write seams: {smoke_forbidden_writes}")
    if (smoke.count("UGameplayStatics::SaveGameToSlot(") != 2
            or smoke.count("ValidationSaveSlot, 0") < 4):
        harness_errors.append(
            "smoke must contain exactly two direct writes and route every direct save/load/delete through ValidationSaveSlot")
    matrix_rows_value = _python_assignment(matrix, "ROWS")
    matrix_rows = tuple(tuple(row) for row in matrix_rows_value) \
        if isinstance(matrix_rows_value, (tuple, list)) else ()
    if matrix_rows != EXPECTED_MATRIX_ROWS:
        harness_errors.append(
            f"matrix ROWS/order differ: actual={matrix_rows!r}")
    row_tokens = _python_assignment(matrix, "ROW_PASS_TOKENS")
    if not isinstance(row_tokens, dict) or tuple(row_tokens) != tuple(
            row[0] for row in EXPECTED_MATRIX_ROWS):
        harness_errors.append(
            "matrix per-row evidence-token keys/order do not match the exact 12 rows")
    for token in (
        "no_persistent_writes", "production_save_sha256_unchanged",
        "no_package_or_save_writes", "packages_before", "saves_before",
        "DiscGolfTour_Automation_Session7FullCharacter_",
        "Session7FullCharacterValidationNoSave",
    ):
        if token not in matrix:
            harness_errors.append(f"matrix missing isolation gate: {token}")
    _record(checks, "twelve_row_full_character_rhbh_matrix", harness_errors, {
        "rows": [list(row) for row in EXPECTED_MATRIX_ROWS],
        "matrix_rows_exact": matrix_rows == EXPECTED_MATRIX_ROWS,
        "smoke_direct_guid_slot_writes": smoke.count(
            "UGameplayStatics::SaveGameToSlot("),
        "smoke_forbidden_write_tokens": smoke_forbidden_writes,
        "missing_rows": matrix_missing,
        "missing_smoke_tokens": smoke_missing,
    })

    visual = _read(
        "Source/DiscGolfTour/DiscGolfSession7FullCharacterVisualCaptureRunner.cpp"
    )
    visual_launcher = _read("Scripts/run-session7-full-character-visual-capture.py")
    visual_tokens = (
        "manual_visual_review_required", "capture_count", "24",
        "Session7VisualTabLabels", "UE_ARRAY_COUNT(Session7VisualTabLabels)",
        "draft_preserved_across_tabs", "current_tab_reset_scoped",
        "reset_all_restored_defaults",
        "randomize_catalog_valid", "randomize_locks_respected",
        "schema8_migration_reloaded", "creator_apply_reloaded",
        "creator_cancel_restored_applied", "missing_cosmetic_fallbacks_resolved",
        "Captures[17].bHairHiddenByOutfitCoverage",
        "Captures[18].bHairComponentVisible",
        "Captures[17].HairStyleId == TEXT(\"hair_medium\")",
        "Captures[18].HairStyleId == TEXT(\"hair_medium\")",
        "head_leader_pose", "cosmetics_collision_free",
        "head_material_is_canonical_mid", "DG_SkinTone", "DG_EyeColor",
        "head_material_has_morph_target_usage",
        "GetUsageByFlag(MATUSAGE_MorphTargets)",
        "DG_Complexion", "DG_Freckles", "DG_SunExposure",
        "DG_ScarProxy", "DG_TattooProxy", "MODULAR_HEAD_ONLY",
        "visible_proxy_face_morphs", "Session7VisualDeferredMorphs",
        "visual_deferred_face_morphs", "Session7VisualVisibleMorphTargets",
        "GetMorphTarget(Target)", "visible_proxy_morphs_applied",
        "DG_HairColor", "presentation_materials_valid",
        "validation_release_callbacks",
        "outfit_attachments_and_materials_valid",
        "Session7VisualInitialUiSettleSeconds = 5.0",
        "Session7VisualHeadCloseupFovDeg = 50.0f",
        "camera_fov_valid",
        "PendingCaptureIndex == 0",
        "ClearOnScreenDebugMessages()",
        "no_persistent_writes", "DO_NOT_SHIP",
    )
    visual_missing = [token for token in visual_tokens if token not in visual]
    launcher_tokens = (
        "FILENAMES", "len(captures) == 24", "pixel_metrics", "sha1",
        "production_save_sha256_before", "production_save_sha256_after",
        "production_save_sha256_unchanged",
        "production_before == production_after", "validation_temp_slot_deleted",
        "manual_visual_review_required", "head_roi_pixels", "roi_difference",
        "FULL_BODY_FOV_DEG = 64.0", "HEAD_CLOSEUP_FOV_DEG = 50.0",
        "camera_fov_exact_all_24",
        "FACE_PRESET_ROI_DELTA_MIN = 0.13",
        "head_material_has_morph_target_usage_all_24",
        "material_usage_fallback_warnings_absent",
        "outfit_attachments_and_materials_valid", "pixel",
        "presentation_authority_all_24",
        "DiscGolfTour_Automation_Session7FullCharacter_", "uuid.uuid4().hex",
        "Session7FullCharacterValidationNoSave",
    )
    launcher_missing = [
        token for token in launcher_tokens if token not in visual_launcher
    ]
    visual_errors = [f"visual runner missing: {token}" for token in visual_missing]
    visual_errors.extend(
        f"visual launcher missing: {token}" for token in launcher_missing
    )
    visual_forbidden_writes = [
        token for token in (
            "SavePackage", "MarkPackageDirty", "CreatePackage(",
            "SaveProfile(", "DiscGolfTour_Profile_0",
        ) if token in visual
    ]
    if visual_forbidden_writes:
        visual_errors.append(
            f"visual runner contains production/package write seams: {visual_forbidden_writes}")
    if "CheckMaterialUsage" in visual:
        visual_errors.append(
            "visual runner must use read-only GetUsageByFlag, not mutating CheckMaterialUsage")
    if (visual.count("UGameplayStatics::SaveGameToSlot(") != 2
            or visual.count("ValidationSaveSlot, 0") < 4):
        visual_errors.append(
            "visual runner must contain exactly two direct writes and route every direct save/load/delete through ValidationSaveSlot")
    runner_filenames = _cpp_text_array(visual, "Session7VisualFilenames")
    runner_tabs = _cpp_text_array(visual, "Session7VisualTabLabels")
    runner_ids = _cpp_text_array(visual, "Session7VisualStableIds")
    runner_visible_keys = _cpp_text_array(
        visual, "Session7VisualVisibleMorphKeys")
    runner_visible_targets = _cpp_text_array(
        visual, "Session7VisualVisibleMorphTargets")
    runner_controls = _cpp_control_cases(
        visual,
        "void ADiscGolfSession7FullCharacterVisualCaptureRunner::GetExpectedControlIds(",
        "FString ADiscGolfSession7FullCharacterVisualCaptureRunner::",
    )
    launcher_filenames_value = _python_assignment(visual_launcher, "FILENAMES")
    launcher_visible_value = _python_assignment(visual_launcher, "VISIBLE_MORPHS")
    launcher_ids_value = _python_assignment(visual_launcher, "STABLE_IDS")
    launcher_tabs_value = _python_assignment(visual_launcher, "EXPECTED_TABS")
    launcher_indices_value = _python_assignment(
        visual_launcher, "EXPECTED_TAB_INDICES")
    launcher_controls_value = _python_assignment(
        visual_launcher, "EXPECTED_CONTROLS")
    launcher_filenames = tuple(launcher_filenames_value or ())
    if runner_filenames != CAPTURE_FILENAMES:
        visual_errors.append("runner filenames/order differ from the frozen exact 24")
    if launcher_filenames != CAPTURE_FILENAMES:
        visual_errors.append("launcher filenames/order differ from the frozen exact 24")
    if runner_tabs != TABS:
        visual_errors.append("runner seven-tab label order differs from the frozen contract")
    if runner_ids != STABLE_IDS:
        visual_errors.append("runner stable cosmetic ID order differs from the frozen 17")
    if tuple(launcher_ids_value or ()) != runner_ids:
        visual_errors.append("launcher and runner stable cosmetic ID orders differ")
    if tuple(launcher_visible_value or ()) != (
            "head_width", "head_height", "cheek_fullness", "jaw_width",
            "chin_length"):
        visual_errors.append("launcher visible proxy morph order differs from the frozen five")
    if runner_visible_keys != (
            "head_width", "head_height", "cheek_fullness", "jaw_width",
            "chin_length") or runner_visible_targets != (
            "DG_Face_HeadWidth", "DG_Face_HeadHeight",
            "DG_Face_CheekFullness", "DG_Face_JawWidth",
            "DG_Face_ChinLength"):
        visual_errors.append(
            "runner visible proxy key-to-target mapping differs from the frozen five")
    if tuple(launcher_tabs_value or ()) != (
            "Identity", "Identity", "Body", "Face", "Hair", "Appearance",
            "Throw Style", "Outfit") or tuple(launcher_indices_value or ()) != (
            0, 0, 1, 2, 3, 4, 5, 6):
        visual_errors.append("launcher overview/seven-tab capture mapping differs")
    expected_launcher_controls = (
        EXPECTED_VISIBLE_CONTROLS[0], *EXPECTED_VISIBLE_CONTROLS
    )
    if tuple(tuple(row) for row in (launcher_controls_value or ())) \
            != expected_launcher_controls:
        visual_errors.append(
            "launcher exact overview/seven-tab control arrays differ from the widget contract")
    if runner_controls != EXPECTED_VISIBLE_CONTROLS:
        visual_errors.append(
            "visual runner expected controls differ from the widget's frozen contract")
    _record(checks, "exact_24_frame_visual_manifest_and_independent_launcher", visual_errors, {
        "runner_missing": visual_missing,
        "launcher_missing": launcher_missing,
        "runner_capture_names_exact": runner_filenames == CAPTURE_FILENAMES,
        "launcher_capture_names_exact": launcher_filenames == CAPTURE_FILENAMES,
        "runner_tab_order_exact": runner_tabs == TABS,
        "runner_direct_guid_slot_writes": visual.count(
            "UGameplayStatics::SaveGameToSlot("),
        "runner_forbidden_write_tokens": visual_forbidden_writes,
        "runner_visible_morph_keys": list(runner_visible_keys),
        "runner_visible_morph_targets": list(runner_visible_targets),
        "runner_control_contract_exact": runner_controls == EXPECTED_VISIBLE_CONTROLS,
    })

    # Scan only lines added since the accepted Session 6 documentation
    # checkpoint.  Existing GameMode/pawn authority remains expected; Session
    # 7 presentation code may not add a second launch, solver, disc, score, or
    # inventory path.  Dedicated validation runners are excluded because they
    # intentionally observe the existing authority.
    changed = _git(
        "diff", "--name-only", SESSION6_DOC_CHECKPOINT, "--",
        "Source/DiscGolfTour", "Plugins/DiscGolfCharacterFramework",
    )
    changed_paths = [
        line.strip().replace("\\", "/")
        for line in changed.stdout.splitlines() if line.strip()
    ]
    forbidden = re.compile(
        r"SpawnActor\s*<\s*ADiscActor|NewObject\s*<\s*UDiscFlightComponent|"
        r"ResolveThrowRelease\s*\(|RequestThrowFromGrip\s*\(|AddStroke\s*\(|"
        r"SetWind\s*\(|SetDiscBag\s*\(",
        re.IGNORECASE,
    )
    authority_hits: list[dict] = []
    for path in changed_paths:
        folded = path.casefold()
        if ("session7fullcharacter" in folded
                or "/tests/" in folded
                or not path.endswith((".cpp", ".h"))):
            continue
        for number, line in enumerate(_added_lines(path).splitlines(), 1):
            if forbidden.search(line):
                authority_hits.append({"file": path, "added_line": number, "text": line})
    _record(
        checks, "no_new_gameplay_release_flight_score_or_inventory_authority",
        [f"forbidden authority addition: {hit}" for hit in authority_hits],
        {"changed_paths": changed_paths, "hits": authority_hits},
    )

    restricted_tokens = (
        "GroomComponent", "ForehandMontage", "PuttingMontage",
        "EquipmentInventory",
    )
    restricted_hits: list[dict] = []
    for path in changed_paths:
        if not path.endswith((".cpp", ".h", ".py", ".json")):
            continue
        added = _added_lines(path)
        for token in restricted_tokens:
            if token in added:
                restricted_hits.append({"file": path, "token": token})
    expected_session8_pawn_additions = {
        "Source/DiscGolfTour/DiscGolferPawn.cpp": (
            '#include "DiscGolfMetaHumanAvatarBackendComponent.h"',
            "AvatarBackendComponent =",
            "CreateDefaultSubobject<UDiscGolfMetaHumanAvatarBackendComponent>(",
            'TEXT("MetaHumanVisualBackend"));',
        ),
        "Source/DiscGolfTour/DiscGolferPawn.h": (
            "class UDiscGolfMetaHumanAvatarBackendComponent;",
            "UDiscGolfMetaHumanAvatarBackendComponent* "
            "GetAvatarBackendComponent() const",
            "{",
            "return AvatarBackendComponent;",
            "}",
            "UPROPERTY(VisibleAnywhere) "
            "TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> "
            "AvatarBackendComponent;",
        ),
    }
    actual_session8_pawn_additions = {
        path: tuple(
            line.strip() for line in _added_lines_since(
                SESSION7_CHECKPOINT, path
            ).splitlines() if line.strip()
        )
        for path in expected_session8_pawn_additions
    }
    successor_errors = [
        f"deferred marker added: {hit}" for hit in restricted_hits
    ]
    if actual_session8_pawn_additions != expected_session8_pawn_additions:
        successor_errors.append(
            "Session 8 Pawn delta is not exactly the one dormant avatar-backend "
            "component include/construction/getter/property contract")
    _record(
        checks, "session8_and_deferred_systems_not_started",
        successor_errors,
        {
            "remaining_deferred_hits": restricted_hits,
            "session8_baseline": SESSION7_CHECKPOINT,
            "expected_dormant_pawn_additions": (
                expected_session8_pawn_additions
            ),
            "actual_dormant_pawn_additions": actual_session8_pawn_additions,
            "dormant_pawn_delta_exact": (
                actual_session8_pawn_additions
                == expected_session8_pawn_additions
            ),
        },
    )

    status = _git("status", "--porcelain=v1", "--untracked-files=all")
    generated_prefixes = (
        "Binaries/", "Intermediate/", "Saved/", "DerivedDataCache/",
        ".vs/", ".idea/", ".vscode/", "__pycache__/",
    )
    staged_generated: list[str] = []
    if status.returncode == 0:
        for line in status.stdout.splitlines():
            state = line[:2]
            path = line[3:].replace("\\", "/")
            if state[0] not in (" ", "?") and path.startswith(generated_prefixes):
                staged_generated.append(path)
    _record(
        checks, "generated_roots_not_staged",
        [f"staged generated path: {path}" for path in staged_generated],
        {"paths": staged_generated},
    )

    spec_evidence: dict = {"path": str(SOURCE_SPEC), "present": SOURCE_SPEC.is_file()}
    spec_errors: list[str] = []
    if SOURCE_SPEC.is_file():
        try:
            spec = json.loads(SOURCE_SPEC.read_text(encoding="utf-8"))
            serialized = json.dumps(spec, sort_keys=True)
            found_ids = {
                row.get("item_id") for row in spec.get("items", [])
                if isinstance(row, dict)
            }
            if found_ids != CATALOG_IDS:
                spec_errors.append(
                    f"catalog stable-ID set differs: found={sorted(found_ids)}"
                )
            if spec.get("shipping_status") != "DO_NOT_SHIP":
                spec_errors.append("proxy spec lost DO_NOT_SHIP")
            if spec.get("external_sources") not in ([], None):
                spec_errors.append("proxy spec declares external sources")
            for path in CANONICAL_PATHS.values():
                if path.split(".", 1)[0] not in serialized and path not in serialized:
                    # The material paths may be recorded without object suffix.
                    spec_errors.append(f"proxy spec lacks canonical path: {path}")
            spec_evidence.update({
                "stable_ids": sorted(found_ids),
                "shipping_status": spec.get("shipping_status"),
            })
        except (OSError, json.JSONDecodeError) as exc:
            spec_errors.append(f"proxy spec could not load: {exc}")
    else:
        spec_errors.append("proxy catalog spec is missing")
    _record(checks, "exact_catalog_ids_paths_and_do_not_ship_boundary", spec_errors, spec_evidence)

    all_errors = [
        f"{check['id']}: {error}"
        for check in checks for error in check["errors"]
    ]
    report = {
        "schema": "DiscGolfTour.Session7FullCharacterWiring.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not all_errors else "FAIL",
        "tabs": list(TABS),
        "face_morph_count": 20,
        "visible_proxy_face_morph_count": 5,
        "deferred_proxy_face_morph_count": 15,
        "catalog_id_count": len(CATALOG_IDS),
        "canonical_paths": CANONICAL_PATHS,
        "checks": checks,
        "errors": all_errors,
        "ue_launched": False,
        "ubt_launched": False,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"SESSION 7 FULL CHARACTER WIRING {report['status']}: "
        f"checks={len(checks)} errors={len(all_errors)} report={REPORT}"
    )
    for error in all_errors:
        print(f"  - {error}", file=sys.stderr)
    return 0 if not all_errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
