#!/usr/bin/env python3
"""Static Session 4 character-creator wiring and authority validation.

This script deliberately does not launch Unreal or mutate project content.  It
checks the project-owned persistence/UI/rig seams, the accepted Session 3 throw
authority boundary, the installed creator schema, and the expected content
paths.  Binary Control Rig/Anim Blueprint graph inspection remains the job of
``validate_dg_character_session4_assets.py`` once the editor-authored assets are
present.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Iterable, Sequence


DEFAULT_ROOT = Path(__file__).absolute().parents[1]
DEFAULT_REPORT = (
    DEFAULT_ROOT / "Saved" / "CharacterFramework" / "Session4WiringValidation.json"
)

SESSION5_PLUGIN_BASELINE = "e6e6a57727411a4cc50b890f4d8557bfb803e9e2"
SESSION7_PLUGIN_BASELINE = "2c54be19a119264d42f11db5470399e021d050cd"
SESSION6_SESSION7_PLUGIN_CHANGES = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfCharacterCustomizationComponent.cpp",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfOutfitComponent.cpp",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfCharacterTypes.h",
)
SESSION8_PLUGIN_CHANGES = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfAvatarBackendComponent.cpp",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfAvatarBackendComponent.h",
)
EXPECTED_SUCCESSOR_PLUGIN_CHANGES = (
    *SESSION6_SESSION7_PLUGIN_CHANGES,
    *SESSION8_PLUGIN_CHANGES,
)

SCHEMA_RELATIVE_PATH = Path(
    "Plugins/DiscGolfCharacterFramework/Config/DG_CharacterCreatorSchema.json"
)
EXPECTED_SCHEMA_SHA256 = "D95F2B5F4771A506D1BCB09F83177B696268964531521DEC68413B7124E49C24"

EXPECTED_BODY_SCHEMA: dict[str, tuple[float, float, float]] = {
    "height_cm": (150.0, 210.0, 183.0),
    "wingspan_scale": (0.92, 1.08, 1.0),
    "shoulder_width_scale": (0.92, 1.08, 1.0),
    "torso_length_scale": (0.94, 1.06, 1.0),
    "leg_length_scale": (0.94, 1.06, 1.0),
    "hand_scale": (0.94, 1.06, 1.0),
    "mass_kg": (45.0, 160.0, 82.0),
}

EXPECTED_STYLE_SCHEMA: dict[str, tuple[float, float, float]] = {
    "run_up_intensity": (0.0, 1.0, 0.65),
    "reach_back_amount": (0.0, 1.0, 0.80),
    "torso_rotation": (0.0, 1.0, 0.75),
    "brace_intensity": (0.0, 1.0, 0.75),
    "explosiveness": (0.0, 1.0, 0.60),
    "follow_through": (0.0, 1.0, 0.80),
}

GAMEPLAY_INPUTS = (
    "Power01",
    "HyzerDegrees",
    "NoseDegrees",
    "AimYawDegrees",
    "TimingQuality01",
)

DTO_FIELDS: tuple[tuple[str, str], ...] = (
    ("bool", "bLeftHanded"),
    ("float", "HeightCm"),
    ("float", "WingspanScale"),
    ("float", "ShoulderWidthScale"),
    ("float", "TorsoLengthScale"),
    ("float", "LegLengthScale"),
    ("float", "HandScale"),
    ("float", "MassKg"),
    ("float", "Muscularity"),
    ("float", "BodyFat"),
    ("float", "Chest"),
    ("float", "Waist"),
    ("float", "Hips"),
    ("float", "Arms"),
    ("float", "Legs"),
    ("float", "RunUpIntensity"),
    ("float", "ReachBackAmount"),
    ("float", "TorsoRotation"),
    ("float", "BraceIntensity"),
    ("float", "Explosiveness"),
    ("float", "FollowThrough"),
)

CONTENT_ASSETS: tuple[tuple[str, str], ...] = (
    ("master_mesh", "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset"),
    ("master_skeleton", "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset"),
    ("master_ik_rig", "Content/DiscGolf/Rigs/IK_DG_Master.uasset"),
    ("master_control_rig", "Content/DiscGolf/Rigs/CR_DG_Master.uasset"),
    ("player_anim_blueprint", "Content/DiscGolf/Animation/ABP_DG_Player.uasset"),
    ("rhbh_sequence", "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset"),
    ("rhbh_montage", "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset"),
    (
        "baseline_profile",
        "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset",
    ),
    (
        "short_compact_profile",
        "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset",
    ),
    (
        "tall_long_arms_profile",
        "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset",
    ),
    ("character_creator_widget", "Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset"),
)

RUNTIME_ASSET_REFERENCES = (
    "/Game/DiscGolf/Characters/Meshes/SK_DG_Master",
    "/Game/DiscGolf/Animation/ABP_DG_Player",
    "/Game/DiscGolf/Animation/ProductionMotion/Drive/AM_DG_RHBH_Drive_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Approach/AM_DG_RHBH_Approach_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/Putt/AM_DG_RHBH_Putt_Procedural_v003",
    "/Game/DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary_v003",
    "/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter",
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact",
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms",
    "/Game/DiscGolf/UI/WBP_DG_CharacterCreator",
)

PROFILE_NAMES = ("ShortCompact", "Baseline", "TallLongArms", "SliderMin", "SliderMax")


@dataclass(frozen=True)
class Check:
    check_id: str
    category: str
    passed: bool
    summary: str
    evidence: Any = None


class Validator:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.checks: list[Check] = []
        self._text_cache: dict[str, str] = {}

    def path(self, relative: str | Path) -> Path:
        return self.root / Path(relative)

    def text(self, relative: str | Path) -> str:
        key = str(relative).replace("\\", "/")
        if key not in self._text_cache:
            path = self.path(relative)
            self._text_cache[key] = (
                path.read_text(encoding="utf-8-sig", errors="replace")
                if path.is_file()
                else ""
            )
        return self._text_cache[key]

    def add(
        self,
        check_id: str,
        category: str,
        passed: bool,
        summary: str,
        evidence: Any = None,
    ) -> None:
        self.checks.append(Check(check_id, category, bool(passed), summary, evidence))

    def file_exists(self, check_id: str, category: str, relative: str | Path) -> None:
        path = self.path(relative)
        self.add(
            check_id,
            category,
            path.is_file(),
            f"Required file exists: {relative}",
            {"path": str(path), "exists": path.is_file()},
        )

    def contains_all(
        self,
        check_id: str,
        category: str,
        relative: str | Path,
        tokens: Iterable[str],
        summary: str,
    ) -> None:
        content = self.text(relative)
        token_list = list(tokens)
        missing = [token for token in token_list if token not in content]
        self.add(
            check_id,
            category,
            bool(content) and not missing,
            summary,
            {"path": str(self.path(relative)), "missing_tokens": missing},
        )


def _close(actual: Any, expected: float) -> bool:
    return isinstance(actual, (int, float)) and math.isclose(
        float(actual), expected, rel_tol=0.0, abs_tol=1.0e-6
    )


def _extract_constant(source: str, name: str) -> float | None:
    match = re.search(
        rf"\b{name}\s*=\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+))f?\s*;", source
    )
    return float(match.group(1)) if match else None


def _validate_schema(validator: Validator) -> dict[str, Any] | None:
    schema_path = validator.path(SCHEMA_RELATIVE_PATH)
    validator.file_exists("schema.file", "schema", SCHEMA_RELATIVE_PATH)
    if not schema_path.is_file():
        return None
    try:
        schema_bytes = schema_path.read_bytes()
        schema_sha256 = hashlib.sha256(schema_bytes).hexdigest().upper()
        schema = json.loads(schema_bytes.decode("utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        validator.add("schema.json", "schema", False, "Creator schema parses as JSON", str(exc))
        return None

    validator.add(
        "schema.json", "schema", True, "Creator schema parses as JSON", {"version": schema.get("version")}
    )
    validator.add(
        "schema.installed_plugin_hash",
        "schema",
        schema_sha256 == EXPECTED_SCHEMA_SHA256,
        "Installed UE 5.8 plugin creator schema matches the validated source-of-truth hash",
        {"actual_sha256": schema_sha256, "expected_sha256": EXPECTED_SCHEMA_SHA256},
    )
    validator.add(
        "schema.version",
        "schema",
        schema.get("version") == 1,
        "Installed plugin creator schema remains version 1",
        {"actual": schema.get("version"), "expected": 1},
    )

    for group_name, expected_group in (
        ("body", EXPECTED_BODY_SCHEMA),
        ("throw_style", EXPECTED_STYLE_SCHEMA),
    ):
        actual_group = schema.get(group_name, {})
        for field, expected_values in expected_group.items():
            actual = actual_group.get(field, {})
            actual_values = (actual.get("min"), actual.get("max"), actual.get("default"))
            passed = all(_close(value, expected) for value, expected in zip(actual_values, expected_values))
            validator.add(
                f"schema.{group_name}.{field}",
                "schema",
                passed,
                f"{group_name}.{field} range/default matches the installed schema contract",
                {"actual": actual_values, "expected": expected_values},
            )

    body = schema.get("body", {})
    control_rig_fields = set(EXPECTED_BODY_SCHEMA) - {"mass_kg"}
    validator.add(
        "schema.body_control_rig_flags",
        "schema",
        all(body.get(name, {}).get("control_rig") is True for name in control_rig_fields),
        "Six proportion fields are explicitly Control Rig inputs",
        {"expected_fields": sorted(control_rig_fields)},
    )
    validator.add(
        "schema.mass_visual_only",
        "schema",
        body.get("mass_kg", {}).get("visual_only_v1") is True,
        "Mass remains explicitly visual-only in schema v1",
        body.get("mass_kg", {}),
    )
    actual_gameplay_inputs = schema.get("gameplay_inputs_not_character_creator")
    validator.add(
        "schema.gameplay_exclusions",
        "schema",
        actual_gameplay_inputs == list(GAMEPLAY_INPUTS),
        "Creator schema excludes all five authoritative shot inputs",
        {"actual": actual_gameplay_inputs, "expected": list(GAMEPLAY_INPUTS)},
    )
    return schema


def _validate_runtime_schema_and_dto(validator: Validator) -> None:
    header_path = "Source/DiscGolfTour/DiscGolfCharacterProfileRuntime.h"
    cpp_path = "Source/DiscGolfTour/DiscGolfCharacterProfileRuntime.cpp"
    validator.file_exists("dto.header", "persistence", header_path)
    validator.file_exists("dto.source", "persistence", cpp_path)
    header = validator.text(header_path)
    cpp = validator.text(cpp_path)

    expected_constants = {
        "MinHeightCm": 150.0,
        "MaxHeightCm": 210.0,
        "DefaultHeightCm": 183.0,
        "MinWingspanScale": 0.92,
        "MaxWingspanScale": 1.08,
        "MinShoulderWidthScale": 0.92,
        "MaxShoulderWidthScale": 1.08,
        "MinTorsoLengthScale": 0.94,
        "MaxTorsoLengthScale": 1.06,
        "MinLegLengthScale": 0.94,
        "MaxLegLengthScale": 1.06,
        "MinHandScale": 0.94,
        "MaxHandScale": 1.06,
        "DefaultBodyScale": 1.0,
        "MinMassKg": 45.0,
        "MaxMassKg": 160.0,
        "DefaultMassKg": 82.0,
        "MinStyleValue": 0.0,
        "MaxStyleValue": 1.0,
        "DefaultRunUpIntensity": 0.65,
        "DefaultReachBackAmount": 0.80,
        "DefaultTorsoRotation": 0.75,
        "DefaultBraceIntensity": 0.75,
        "DefaultExplosiveness": 0.60,
        "DefaultFollowThrough": 0.80,
        "DefaultMuscularity": 0.35,
        "DefaultBodyFat": 0.35,
        "DefaultBodyShape": 0.0,
    }
    actual_constants = {name: _extract_constant(header, name) for name in expected_constants}
    bad_constants = {
        name: {"actual": actual_constants[name], "expected": expected}
        for name, expected in expected_constants.items()
        if not _close(actual_constants[name], expected)
    }
    validator.add(
        "dto.schema_constants",
        "persistence",
        not bad_constants,
        "Runtime sanitation constants exactly match creator schema v1",
        {"mismatches": bad_constants},
    )

    customization_header = validator.text(
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfCustomizationTypes.h"
    )
    expected_build_defaults = {
        "Muscularity": 0.35,
        "BodyFat": 0.35,
        "Chest": 0.0,
        "Waist": 0.0,
        "Hips": 0.0,
        "Arms": 0.0,
        "Legs": 0.0,
    }
    actual_build_defaults = {
        name: _extract_constant(customization_header, name)
        for name in expected_build_defaults
    }
    bad_build_defaults = {
        name: {"actual": actual_build_defaults[name], "expected": expected}
        for name, expected in expected_build_defaults.items()
        if not _close(actual_build_defaults[name], expected)
    }
    validator.add(
        "dto.body_build_plugin_defaults",
        "persistence",
        bool(customization_header) and not bad_build_defaults,
        "Persisted body-build foundation defaults match installed plugin types",
        {"mismatches": bad_build_defaults},
    )

    save_fields = re.findall(
        r"UPROPERTY\([^)]*\bSaveGame\b[^)]*\)\s*(bool|float)\s+(\w+)\s*=",
        header,
        flags=re.MULTILINE,
    )
    validator.add(
        "dto.primitive_shape",
        "persistence",
        tuple(save_fields) == DTO_FIELDS,
        "Profile SaveGame DTO has the exact project-owned primitive field set",
        {"actual": save_fields, "expected": list(DTO_FIELDS)},
    )
    dto_block_match = re.search(
        r"struct\s+DISCGOLFTOUR_API\s+FDiscGolfCharacterProfileSaveData\b(?P<body>[\s\S]*?)\n};",
        header,
    )
    dto_block = dto_block_match.group("body") if dto_block_match else ""
    forbidden_reflected_types = (
        "UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame", "FDGBodyProfile",
        "FDGThrowStyle", "FDGBodyBuildProfile", "EDGHandedness",
    )
    # The UPROPERTY prefix is expected; only inspect declarations after it for
    # framework types. Conversion method signatures intentionally use them.
    reflected_framework_field = re.search(
        r"UPROPERTY\([^)]*SaveGame[^)]*\)\s*(?:FDG\w+|EDGHandedness)\b", dto_block
    )
    validator.add(
        "dto.no_framework_fields",
        "persistence",
        dto_block_match is not None and reflected_framework_field is None,
        "DTO stores no plugin UObject/struct/enum field",
        {"forbidden_reflected_field": reflected_framework_field.group(0) if reflected_framework_field else None},
    )
    validator.contains_all(
        "dto.conversion_api",
        "persistence",
        header_path,
        (
            "void Sanitize();",
            "FDGBodyProfile ToBodyProfile() const;",
            "FDGBodyBuildProfile ToBodyBuildProfile() const;",
            "FDGThrowStyle ToThrowStyle() const;",
            "EDGHandedness GetHandedness() const;",
            "FromFramework(",
        ),
        "DTO exposes the one-way plugin conversion boundary and sanitation",
    )
    validator.contains_all(
        "dto.sanitize_all_fields",
        "persistence",
        cpp_path,
        tuple(f"{field} = SanitizeFinite(" for _, field in DTO_FIELDS if field != "bLeftHanded"),
        "Every numeric creator value is finite-sanitized",
    )
    validator.add(
        "dto.style_multipliers_neutral",
        "authority",
        cpp.count("ThrowStyle.PowerMultiplier = 1.0f;") == 1
        and cpp.count("ThrowStyle.SpinMultiplier = 1.0f;") == 1,
        "Plugin style power/spin multipliers are forced to neutral exactly once",
        {
            "power_assignments": cpp.count("ThrowStyle.PowerMultiplier = 1.0f;"),
            "spin_assignments": cpp.count("ThrowStyle.SpinMultiplier = 1.0f;"),
        },
    )
    forbidden_dto_inputs = [token for token in GAMEPLAY_INPUTS if token in header or token in cpp]
    validator.add(
        "dto.no_gameplay_inputs",
        "authority",
        not forbidden_dto_inputs,
        "Profile DTO does not persist or synthesize authoritative shot inputs",
        {"forbidden_tokens_found": forbidden_dto_inputs},
    )

    save_header = validator.text("Source/DiscGolfTour/DiscGolfSaveGame.h")
    gi_header = validator.text("Source/DiscGolfTour/DiscGolfTourGameInstance.h")
    gi_cpp = validator.text("Source/DiscGolfTour/DiscGolfTourGameInstance.cpp")
    validator.add(
        "save.schema_v9_successor_preserves_schema8_mirrors",
        "persistence",
        re.search(r"CurrentVersion\s*=\s*9\s*;", save_header) is not None
        and "SaveGame) FDGFullCharacterCustomization CharacterCustomization;" in save_header
        and "SaveGame) FDiscGolfCharacterProfileSaveData CharacterProfile;" in save_header
        and "SaveGame) FDGOutfitLoadout OutfitLoadout;" in save_header
        and "SaveSchemaVersion < 9" in gi_cpp
        and "InOutProfile.SaveSchemaVersion = 9;" in gi_cpp
        and "InOutProfile.CharacterCustomization" in gi_cpp,
        "Schema 9 owns the complete character while retaining exact schema-8 migration mirrors",
    )
    validator.contains_all(
        "save.game_instance_api",
        "persistence",
        "Source/DiscGolfTour/DiscGolfTourGameInstance.h",
        (
            "FDiscGolfCharacterProfileSaveData GetCharacterProfile() const;",
            "bool UpdateCharacterProfile(const FDiscGolfCharacterProfileSaveData& CharacterProfile);",
            "FDGBodyProfile& OutBody",
            "FDGThrowStyle& OutThrowStyle",
            "const FDGBodyProfile& Body",
            "const FDGThrowStyle& ThrowStyle",
        ),
        "GameInstance exposes DTO and framework convenience profile APIs",
    )
    future_guard = gi_cpp.find("SaveSchemaVersion > DiscGolfSaveSchema::CurrentVersion")
    first_migration = gi_cpp.find("SaveSchemaVersion < 4")
    validator.add(
        "save.future_schema_guard",
        "persistence",
        future_guard >= 0
        and first_migration >= 0
        and future_guard < first_migration
        and "FutureSchemaRejected" in gi_cpp,
        "Future-schema saves are rejected before any migration mutation",
        {"future_guard_offset": future_guard, "first_migration_offset": first_migration},
    )
    validator.add(
        "save.v6_to_v7_migration",
        "persistence",
        "SaveSchemaVersion < 7" in gi_cpp
        and "CharacterProfile.Sanitize();" in gi_cpp
        and "SaveSchemaVersion = 7;" in gi_cpp,
        "Legacy saves migrate deterministically from v6 to v7",
    )
    validator.add(
        "save.v7_to_v8_outfit_migration",
        "persistence",
        "SaveSchemaVersion < 8" in gi_cpp
        and "OutfitLoadout.Equipped.Reset();" in gi_cpp
        and "SaveSchemaVersion = 8;" in gi_cpp,
        "Schema-7 saves migrate deterministically to an empty Session-6 outfit",
    )
    validator.add(
        "save.transactional_update",
        "persistence",
        "const FDiscGolfCharacterProfileSaveData PreviousCharacter" in gi_cpp
        and "const FDGOutfitLoadout PreviousOutfit" in gi_cpp
        and "UpdateCharacterProfileAndOutfit" in gi_cpp
        and "if (!SaveProfileInternal())" in gi_cpp
        and "Profile->CharacterProfile = PreviousCharacter;" in gi_cpp
        and "Profile->OutfitLoadout = PreviousOutfit;" in gi_cpp,
        "Failed atomic profile/outfit writes restore both previous in-memory payloads",
    )


def _validate_transient_runtime_profile(validator: Validator) -> None:
    pawn_path = "Source/DiscGolfTour/DiscGolferPawn.cpp"
    pawn_header_path = "Source/DiscGolfTour/DiscGolferPawn.h"
    pawn = validator.text(pawn_path)
    pawn_header = validator.text(pawn_header_path)
    validator.contains_all(
        "runtime.transient_profile_clone",
        "runtime_profile",
        pawn_path,
        (
            "DuplicateObject<UDiscGolfCharacterProfile>",
            "RuntimeCharacterProfile->SetFlags(RF_Transient);",
            "FrameworkThrowComponent->CharacterProfile = RuntimeCharacterProfile;",
            "ApplyCharacterProfileUnchecked",
        ),
        "Pawn edits a transient profile clone and never a preset asset",
    )
    validator.add(
        "runtime.profile_property_transient",
        "runtime_profile",
        "UPROPERTY(Transient) TObjectPtr<UDiscGolfCharacterProfile> RuntimeCharacterProfile;"
        in pawn_header,
        "Runtime profile pointer is explicitly transient",
    )
    missing_profiles = [name for name in PROFILE_NAMES if name.lower() not in pawn.lower()]
    validator.add(
        "runtime.smoke_overrides",
        "runtime_profile",
        not missing_profiles
        and "Session4Profile=" in pawn
        and "FApp::IsUnattended()" in pawn
        and "transient, save slot unchanged" in pawn,
        "All five smoke profiles use unattended transient overrides",
        {"missing_profiles": missing_profiles},
    )
    validator.add(
        "runtime.slider_extremes_use_schema",
        "runtime_profile",
        all(
            token in pawn
            for token in (
                "DiscGolfCharacterCreatorSchema::MinHeightCm",
                "DiscGolfCharacterCreatorSchema::MaxHeightCm",
                "DiscGolfCharacterCreatorSchema::MinWingspanScale",
                "DiscGolfCharacterCreatorSchema::MaxWingspanScale",
                "Minimum.ToThrowStyle()",
                "Maximum.ToThrowStyle()",
            )
        ),
        "SliderMin/SliderMax are built through the shared schema/DTO boundary",
    )
    validator.add(
        "runtime.no_profile_asset_mutation",
        "runtime_profile",
        "CharacterProfileTemplate->Body =" not in pawn
        and "CharacterProfileTemplate->ThrowStyle =" not in pawn
        and "CharacterProfileTemplate->Handedness =" not in pawn,
        "Pawn does not assign editable values into the profile template asset",
    )


def _validate_authority(validator: Validator) -> None:
    adapter_path = "Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp"
    pawn_path = "Source/DiscGolfTour/DiscGolferPawn.cpp"
    game_mode_path = "Source/DiscGolfTour/DiscGolfTourGameMode.cpp"
    adapter = validator.text(adapter_path)
    pawn = validator.text(pawn_path)
    game_mode = validator.text(game_mode_path)

    release_fields = sorted(set(re.findall(r"\bReleaseData\.(\w+)", adapter)))
    validator.add(
        "authority.release_data_boundary",
        "authority",
        release_fields == ["GripWorldTransform"],
        "Animation adapter consumes only FDGReleaseData::GripWorldTransform",
        {"fields_consumed": release_fields, "expected": ["GripWorldTransform"]},
    )
    validator.contains_all(
        "authority.single_launch_chain",
        "authority",
        game_mode_path,
        (
            "bool ADiscGolfTourGameMode::RequestThrowFromGrip(",
            "DiscGolfMath::ResolveThrowRelease(AuthoredCommand)",
            "ActiveDisc->Throw(CandidateRelease)",
        ),
        "Release adapter reaches the existing authoritative release solver and disc launch",
    )
    validator.add(
        "authority.pawn_forwards_exact_command",
        "authority",
        "RequestThrowFromGrip(AuthoritativeCommand, GripWorldTransform)" in pawn,
        "Pawn forwards the cached command and grip transform without recomputation",
    )
    validator.add(
        "authority.montage_rate_presentation_only",
        "authority",
        all(
            token in pawn
            for token in (
                "ComputeThrowMontagePlayRate(AuthoritativeCommand)",
                "MinimumThrowMontagePlayRate = 0.90f",
                "MaximumThrowMontagePlayRate = 1.10f",
                "It never feeds back into release speed, spin, angle, or direction.",
            )
        ),
        "Bounded montage cadence remains presentation-only and cannot rewrite the command",
    )

    physics_files = (
        "Source/DiscGolfTour/DiscGolfMath.h",
        "Source/DiscGolfTour/DiscFlightComponent.h",
        "Source/DiscGolfTour/DiscFlightComponent.cpp",
        "Source/DiscGolfTour/ThrowControllerComponent.h",
        "Source/DiscGolfTour/ThrowControllerComponent.cpp",
        "Source/DiscGolfTour/DiscGolfTourGameMode.h",
        "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.h",
        "Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp",
    )
    forbidden_style_members = (
        "RunUpIntensity",
        "ReachBackAmount",
        "TorsoRotation",
        "BraceIntensity",
        "Explosiveness",
        "FollowThrough",
    )
    violations: list[dict[str, Any]] = []
    for relative in physics_files:
        content = validator.text(relative)
        found = [token for token in forbidden_style_members if re.search(rf"\b{token}\b", content)]
        if "FDGThrowStyle" in content or found:
            violations.append(
                {"path": str(validator.path(relative)), "FDGThrowStyle": "FDGThrowStyle" in content, "fields": found}
            )
    validator.add(
        "authority.no_visual_style_in_physics",
        "authority",
        not violations,
        "Creator throw-style values do not enter authoritative throw/flight files",
        {"violations": violations},
    )

    rig_paths = (
        "Source/DiscGolfTour/DiscGolfCharacterRigUnits.h",
        "Source/DiscGolfTour/DiscGolfCharacterRigUnits.cpp",
    )
    rig_combined = "\n".join(validator.text(path) for path in rig_paths)
    forbidden_rig_authority = (
        "FThrowCommand",
        "FThrowRelease",
        "ResolveThrowRelease",
        "ActiveDisc->Throw",
        "SuggestedLaunchSpeedMps",
        "SuggestedSpinRpm",
        "PowerMultiplier",
        "SpinMultiplier",
        "Montage_Play",
        "Montage_SetPlayRate",
    )
    rig_violations = [token for token in forbidden_rig_authority if token in rig_combined]
    validator.add(
        "authority.rig_presentation_only",
        "authority",
        bool(rig_combined) and not rig_violations,
        "Profile rig unit has no throw command, flight, multiplier, or montage authority",
        {"forbidden_tokens_found": rig_violations},
    )


def _validate_left_boundary(validator: Validator) -> None:
    pawn = validator.text("Source/DiscGolfTour/DiscGolferPawn.cpp")
    widget = validator.text("Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp")
    throw_start = pawn.find("bool ADiscGolferPawn::TryStartAnimatedRHBHThrow")
    montage_play = pawn.find("AnimInstance->Montage_Play(", throw_start)
    right_gate = pawn.find("Handedness != EDGHandedness::Right", throw_start)
    validator.add(
        "left.rhb_hard_boundary",
        "handedness",
        throw_start >= 0 and right_gate >= throw_start and montage_play > right_gate,
        "Non-right-handed profiles are rejected before the accepted RHBH montage plays",
        {"throw_start": throw_start, "right_gate": right_gate, "montage_play": montage_play},
    )
    validator.add(
        "left.right_grip_attachment",
        "handedness",
        "HeldDiscVisual->SetupAttachment(SkeletalMesh, TEXT(\"disc_grip_r\"));" in pawn,
        "Session 4 held disc remains explicitly attached to the accepted right-hand grip",
    )
    validator.add(
        "left.ui_disclosure",
        "handedness",
        "ANIMATED LHBH IS NOT AVAILABLE IN SESSION 4" in widget
        and "NO MIRRORED MONTAGE IS CLAIMED" in widget,
        "Creator clearly discloses the LHBH animation boundary",
    )


def _validate_ui(validator: Validator) -> None:
    widget_header_path = "Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.h"
    widget_cpp_path = "Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp"
    controller_header_path = "Source/DiscGolfTour/DiscGolfTourPlayerController.h"
    controller_cpp_path = "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp"
    widget_h = validator.text(widget_header_path)
    widget_cpp = validator.text(widget_cpp_path)
    controller_h = validator.text(controller_header_path)
    controller_cpp = validator.text(controller_cpp_path)
    hud_cpp = validator.text("Source/DiscGolfTour/DiscGolfHUD.cpp")
    game_mode_cpp = validator.text("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")

    validator.add(
        "ui.native_widget_class",
        "ui",
        "class DISCGOLFTOUR_API UDiscGolfCharacterCreatorWidget : public UUserWidget" in widget_h
        and "RebuildWidget()" in widget_h
        and "UDiscGolfCharacterCreatorWidget::RebuildWidget()" in widget_cpp,
        "Native UMG fallback widget is implemented",
    )
    body_fields = (
        "HeightCm", "WingspanScale", "ShoulderWidthScale", "TorsoLengthScale",
        "LegLengthScale", "HandScale", "MassKg",
    )
    style_fields = (
        "RunUpIntensity", "ReachBackAmount", "TorsoRotation", "BraceIntensity",
        "Explosiveness", "FollowThrough",
    )
    missing_fields = [field for field in (*body_fields, *style_fields) if field not in widget_cpp]
    validator.add(
        "ui.complete_slider_set",
        "ui",
        not missing_fields and "BODY" in widget_cpp and "THROW STYLE  //  PRESENTATION ONLY" in widget_cpp,
        "Creator exposes all seven body and six visual throw-style controls",
        {"missing_fields": missing_fields},
    )
    validator.contains_all(
        "ui.actions_and_presets",
        "ui",
        widget_cpp_path,
        (
            "BASELINE", "SHORT COMPACT", "TALL / LONG ARMS",
            "RESET CURRENT TAB", "RESET ALL", "RANDOMIZE",
            "APPLY / SAVE & CONTINUE", "CANCEL", "RIGHT", "LEFT",
            "ROTATE LEFT", "ROTATE RIGHT", "ZOOM IN", "ZOOM OUT",
        ),
        "Successor creator retains presets and handedness while exposing the frozen global transaction controls",
    )
    controller_api = (
        "OpenCharacterCreator",
        "IsCharacterCreatorOpen",
        "PreviewCharacterCreatorDraft",
        "LoadCharacterCreatorPreset",
        "ResetCharacterCreatorDraft",
        "ApplyCharacterCreatorDraft",
        "CancelCharacterCreator",
        "RotateCharacterCreatorPreview",
    )
    validator.add(
        "ui.controller_api",
        "ui",
        all(token in controller_h and token in controller_cpp for token in controller_api if token != "IsCharacterCreatorOpen")
        and "IsCharacterCreatorOpen" in controller_h,
        "PlayerController exposes the complete creator lifecycle API",
        {"expected_api": list(controller_api)},
    )
    validator.add(
        "ui.shared_sanitation",
        "ui",
        all(
            token in controller_cpp
            for token in (
                "FDiscGolfCharacterProfileSaveData::FromFramework",
                ".Sanitize();",
                ".ToBodyProfile()",
                ".ToThrowStyle()",
                ".GetHandedness()",
            )
        ),
        "UI preview/apply uses the shared DTO sanitation boundary",
    )
    validator.add(
        "ui.transactional_cancel",
        "ui",
        all(
            token in controller_cpp
            for token in (
                "CharacterCreatorOpeningBody",
                "CharacterCreatorOpeningThrowStyle",
                "CharacterCreatorOpeningHandedness",
                "CloseCharacterCreator(true)",
                "PreviewCharacterCreatorProfile(",
                "EndCharacterCreatorPreview(true)",
            )
        ),
        "Cancel restores the opening profile and camera preview transaction",
    )
    validator.add(
        "ui.apply_persists_through_gi",
        "ui",
        "Instance->UpdateCharacterProfileAndOutfit(" in controller_cpp
        and "SaveData, CharacterCreatorDraftOutfit" in controller_cpp
        and "CloseCharacterCreator(false)" in controller_cpp,
        "Apply atomically persists profile and outfit through GameInstance before closing",
    )
    validator.add(
        "ui.safe_open_gate",
        "ui",
        "IsCharacterProfileChangeSafe()" in controller_cpp
        and "CanOpenCharacterCreator" in game_mode_cpp,
        "Creator opening/preview respects gameplay transition safety",
    )
    validator.add(
        "ui.asset_plus_native_fallback",
        "ui",
        "/Game/DiscGolf/UI/WBP_DG_CharacterCreator" in controller_cpp
        and "UDiscGolfCharacterCreatorWidget::StaticClass()" in controller_cpp,
        "Optional authored WBP path has a fully functional native fallback",
    )
    validator.add(
        "ui.hud_suppression",
        "ui",
        "IsCharacterCreatorOpen()" in hud_cpp,
        "Existing Canvas HUD is suppressed while the creator is open",
    )
    validator.add(
        "ui.event_driven",
        "performance",
        "PrimaryActorTick.bCanEverTick = true" not in widget_cpp
        and "NativeTick(" not in widget_cpp
        and "Tick(" not in widget_h,
        "Creator widget is event-driven and adds no permanent UI Tick",
    )


def _validate_rig_seams(validator: Validator) -> None:
    rig_h_path = "Source/DiscGolfTour/DiscGolfCharacterRigUnits.h"
    rig_cpp_path = "Source/DiscGolfTour/DiscGolfCharacterRigUnits.cpp"
    rig_h = validator.text(rig_h_path)
    rig_cpp = validator.text(rig_cpp_path)
    validator.file_exists("rig.header", "rig", rig_h_path)
    validator.file_exists("rig.source", "rig", rig_cpp_path)
    validator.add(
        "rig.unit_signature",
        "rig",
        "FRigUnit_DGApplyCharacterProfile : public FRigUnitMutable" in rig_h
        and 'DisplayName="DG Apply Character Profile"' in rig_h
        and "RIGVM_METHOD()" in rig_h,
        "Project-owned mutable DG Apply Character Profile rig unit is registered",
    )
    rig_inputs = (
        "BodyProfile",
        "ThrowStyle",
        "Handedness",
        "ThrowPhase",
        "bThrowActive",
        "ThrowIntent",
    )
    missing_inputs = [name for name in rig_inputs if name not in rig_h]
    validator.add(
        "rig.six_inputs",
        "rig",
        not missing_inputs and rig_h.count("UPROPERTY(meta=(Input))") == 6,
        "Rig unit exposes exactly the six expected ABP/Control Rig inputs",
        {"missing_inputs": missing_inputs, "input_property_count": rig_h.count("UPROPERTY(meta=(Input))")},
    )
    rig_ranges = (
        "150.0f, 210.0f", "0.92f, 1.08f", "0.94f, 1.06f",
        "45.0f, 160.0f", "0.0f, 1.0f",
    )
    validator.add(
        "rig.schema_bounds",
        "rig",
        all(token in rig_cpp for token in rig_ranges),
        "Rig unit clamps body/style inputs to schema-compatible bounds",
        {"missing_tokens": [token for token in rig_ranges if token not in rig_cpp]},
    )
    validator.contains_all(
        "rig.preserved_controls",
        "rig",
        rig_cpp_path,
        (
            "ctrl_foot_l", "ctrl_foot_r", "ctrl_hand_l", "ctrl_hand_r",
            "dg_hand_ik_alpha_l", "dg_hand_ik_alpha_r", "dg_height_cm",
            "dg_wingspan_scale", "dg_shoulder_width_scale", "dg_torso_length_scale",
            "dg_leg_length_scale", "dg_hand_scale",
        ),
        "Rig implementation retains accepted IK controls and six diagnostic profile controls",
    )
    left_idle_reset = 'SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_l"), 0.0f);'
    right_idle_reset = 'SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"), 0.0f);'
    active_branch = "if (bThrowActive)"
    left_reset_index = rig_cpp.find(left_idle_reset)
    right_reset_index = rig_cpp.find(right_idle_reset)
    active_branch_index = rig_cpp.find(active_branch, max(left_reset_index, right_reset_index, 0))
    validator.add(
        "rig.idle_hand_ik_reset_order",
        "rig",
        left_reset_index >= 0
        and right_reset_index >= 0
        and active_branch_index > max(left_reset_index, right_reset_index),
        "Both hand IK weights are explicitly zeroed before active-throw weighting",
        {
            "left_reset_index": left_reset_index,
            "right_reset_index": right_reset_index,
            "active_branch_index": active_branch_index,
        },
    )
    validator.add(
        "rig.no_actor_scale_shortcut",
        "rig",
        "SetActorScale3D" not in rig_cpp
        and "SetWorldScale3D" not in rig_cpp
        and "SetRelativeScale3D" not in rig_cpp,
        "Proportion work does not use whole-actor/component scaling shortcuts",
    )
    build_cs = validator.text("Source/DiscGolfTour/DiscGolfTour.Build.cs")
    validator.add(
        "rig.runtime_dependency",
        "rig",
        re.search(r'"ControlRig"', build_cs) is not None,
        "Runtime module declares ControlRig dependency for the custom unit",
    )

    editor_files = (
        "Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.h",
        "Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.cpp",
        "Scripts/create_dg_character_session4_assets.py",
        "Scripts/validate_dg_character_session4_assets.py",
    )
    for index, path in enumerate(editor_files):
        validator.file_exists(f"rig.asset_tool_{index + 1}", "rig_assets", path)
    editor_combined = "\n".join(validator.text(path) for path in editor_files)
    expected_editor_tokens = (
        "CR_DG_Master",
        "ABP_DG_Player",
        "AM_DG_RHBH_Prototype",
        "WBP_DG_CharacterCreator",
        "BodyProfile",
        "ThrowStyle",
        "Handedness",
        "ThrowPhase",
        "bThrowActive",
        "ThrowIntent",
        "DGApplyCharacterProfile",
        "DGFullBodyIK",
    )
    missing_editor_tokens = [token for token in expected_editor_tokens if token not in editor_combined]
    validator.add(
        "rig.asset_tool_contract",
        "rig_assets",
        all(validator.path(path).is_file() for path in editor_files) and not missing_editor_tokens,
        "Editor asset creator/validator encode the expected CR, ABP, montage, and WBP seams",
        {"missing_tokens": missing_editor_tokens},
    )

    asset_utility = validator.text("Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.cpp")
    author_index = asset_utility.find("UDiscGolfSession4AssetUtility::AuthorSession4Assets")
    variable_loop_index = asset_utility.find(
        "for (const FVariableSpec& Spec : RequiredVariables())", author_index
    )
    generated_compile_index = asset_utility.find(
        "CompileRigBlueprintAndValidateVariables(Rig, Error)", variable_loop_index
    )
    graph_author_index = asset_utility.find("EnsureRigGraph(Rig", generated_compile_index)
    validator.add(
        "rig.asset_variable_full_compile_order",
        "rig_assets",
        author_index >= 0
        and variable_loop_index > author_index
        and generated_compile_index > variable_loop_index
        and graph_author_index > generated_compile_index,
        "UE 5.8 host variables receive a full synchronous Blueprint compile before RigVM getter authoring",
        {
            "author_index": author_index,
            "variable_loop_index": variable_loop_index,
            "generated_compile_index": generated_compile_index,
            "graph_author_index": graph_author_index,
        },
    )
    generated_contract_tokens = (
        "GetPublicVariableByName",
        "PublicVariable.GetProperty()",
        "PublicVariable.GetGuid() != Spec.Guid",
        "GetExtendedCPPType",
        "GetCPPTypeObject",
        "IsBlueprintCompileStatusHealthy",
        "HasCompletePublicAssetVariableSet",
    )
    missing_generated_contract_tokens = [
        token for token in generated_contract_tokens if token not in asset_utility
    ]
    validator.add(
        "rig.generated_variable_contract",
        "rig_assets",
        not missing_generated_contract_tokens,
        "Session 4 authoring verifies generated properties and live public Control Rig variables",
        {"missing_tokens": missing_generated_contract_tokens},
    )


def _validate_assets_and_paths(validator: Validator) -> None:
    for asset_id, relative in CONTENT_ASSETS:
        validator.file_exists(f"asset.{asset_id}", "assets", relative)

    source_paths = (
        "Source/DiscGolfTour/DiscGolferPawn.cpp",
        "Source/DiscGolfTour/DiscGolfProductionMotion.h",
        "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp",
        "Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.cpp",
        "Scripts/create_dg_character_session4_assets.py",
        "Scripts/validate_dg_character_session4_assets.py",
    )
    combined = "\n".join(validator.text(path) for path in source_paths)
    missing_references = [path for path in RUNTIME_ASSET_REFERENCES if path not in combined]
    validator.add(
        "asset.canonical_references",
        "assets",
        not missing_references,
        "Runtime/editor tooling references the canonical Session 4 content paths",
        {"missing_references": missing_references},
    )


def _validate_plugin_immutability(validator: Validator) -> None:
    customization_path, outfit_path, types_path = SESSION6_SESSION7_PLUGIN_CHANGES
    avatar_cpp_path, avatar_header_path = SESSION8_PLUGIN_CHANGES
    customization_component = validator.text(customization_path)
    outfit_component = validator.text(outfit_path)
    character_types = validator.text(types_path)
    avatar_cpp = validator.text(avatar_cpp_path)
    avatar_header = validator.text(avatar_header_path)
    required_outfit_tokens = (
        "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "SetGenerateOverlapEvents(false)",
        "SetCanEverAffectNavigation(false)",
        "SetAbsolute(false, false, true)",
        "VariantId = Variant.VariantId",
    )
    required_customization_tokens = (
        "static void DGSetMorphTargetIfAvailable(",
        "GetSkeletalMeshAsset()",
        "FindMorphTarget(MorphName)",
        "for (const TPair<FName, FName>& Pair : MorphMap)",
        "Current.Face.MorphValues.FindRef(Pair.Key)",
        "SetLeaderPoseComponent(HeadMesh)",
        "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "SetAbsolute(false, false, true)",
    )
    try:
        diff_result = subprocess.run(
            [
                "git", "diff", "--name-only", "--diff-filter=ACDMRTUXB",
                SESSION5_PLUGIN_BASELINE, "--",
                "Plugins/DiscGolfCharacterFramework",
            ],
            cwd=validator.root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=15.0,
            check=False,
        )
        session8_diff_result = subprocess.run(
            [
                "git", "diff", "--name-only", "--diff-filter=ACDMRTUXB",
                SESSION7_PLUGIN_BASELINE, "--",
                "Plugins/DiscGolfCharacterFramework",
            ],
            cwd=validator.root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=15.0,
            check=False,
        )
        status_result = subprocess.run(
            [
                "git", "status", "--porcelain=v1", "--untracked-files=all", "--",
                "Plugins/DiscGolfCharacterFramework",
            ],
            cwd=validator.root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=15.0,
            check=False,
        )
        changed_paths = sorted({
            line.strip().replace("\\", "/")
            for line in diff_result.stdout.splitlines() if line.strip()
        })
        session8_changed_paths = sorted({
            line.strip().replace("\\", "/")
            for line in session8_diff_result.stdout.splitlines() if line.strip()
        })
        status_entries = [
            line for line in status_result.stdout.splitlines() if line.strip()
        ]
        untracked_paths = sorted({
            line[3:].strip().replace("\\", "/")
            for line in status_entries if line.startswith("?? ")
        })
        customization_morph_writes = customization_component.count(
            "MeshComp->SetMorphTarget(MorphName, Value);")
        body_block = character_types.split("struct DISCGOLFCHARACTERFRAMEWORK_API FDGBodyProfile", 1)[-1].split(
            "USTRUCT(BlueprintType)", 1
        )[0]
        throw_block = character_types.split("struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowStyle", 1)[-1].split(
            "USTRUCT(BlueprintType)", 1
        )[0]
        avatar_header_tokens = (
            "bool ApplyCustomizationToVisual(",
            "bool IsVisualBackendReady() const;",
            "FDGAvatarBackendState GetAvatarBackendState() const;",
            "AActor* GetActiveVisualActor() const;",
            "UDiscGolfAvatarBackendProfile* GetActiveBackendProfile() const;",
            "USkeletalMeshComponent* GetActiveAnimationSourceMesh() const;",
            "UFUNCTION(BlueprintNativeEvent, Category=\"Disc Golf|Avatar\")",
            "virtual bool ConfigureVisualBackend_Implementation(",
            "virtual bool ApplyVisualCustomization_Implementation(",
            "TObjectPtr<AActor> PendingVisualActor;",
            "TObjectPtr<UDiscGolfAvatarBackendProfile> ActiveBackendProfile;",
            "TObjectPtr<USkeletalMeshComponent> ActiveAnimationSourceMesh;",
            "FDGAvatarBackendState ActiveBackendState;",
        )
        avatar_cpp_tokens = (
            "SpawnActorDeferred<AActor>",
            "CandidateActor->SetActorHiddenInGame(true);",
            "CandidateActor->SetActorEnableCollision(false);",
            "CandidateActor->FinishSpawning(SpawnTransform);",
            "CandidateActor->AttachToComponent(",
            "AnimationSourceMesh,",
            "FAttachmentTransformRules::SnapToTargetNotIncludingScale",
            "const bool bConfigured = ConfigureVisualBackend(",
            "const bool bCandidateReady = bConfigured",
            "CandidateRoot->IsAttachedTo(AnimationSourceMesh)",
            "ActiveBackendState.bVisualReady = true;",
            "OnAvatarBackendReady.Broadcast(ReadyState);",
            "Primitive->SetSimulatePhysics(false);",
            "Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);",
            "Primitive->SetGenerateOverlapEvents(false);",
            "TargetMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);",
        )
        base_configure_fails_closed = re.search(
            r"ConfigureVisualBackend_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
            avatar_cpp,
            re.DOTALL,
        ) is not None
        base_apply_fails_closed = re.search(
            r"ApplyVisualCustomization_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
            avatar_cpp,
            re.DOTALL,
        ) is not None
        passed = (
            diff_result.returncode == 0
            and session8_diff_result.returncode == 0
            and status_result.returncode == 0
            and changed_paths == sorted(EXPECTED_SUCCESSOR_PLUGIN_CHANGES)
            and session8_changed_paths == sorted(SESSION8_PLUGIN_CHANGES)
            and not untracked_paths
            and all(token in outfit_component for token in required_outfit_tokens)
            and all(token in customization_component for token in required_customization_tokens)
            and customization_morph_writes == 1
            and body_block.count("SaveGame") == 7
            and throw_block.count("SaveGame") == 8
            and character_types.count("SaveGame") == 15
            and all(token in avatar_header for token in avatar_header_tokens)
            and all(token in avatar_cpp for token in avatar_cpp_tokens)
            and avatar_cpp.count("SpawnActorDeferred<AActor>") == 1
            and avatar_cpp.count("OnAvatarBackendReady.Broadcast(") == 1
            and avatar_cpp.find("const bool bCandidateReady = bConfigured")
            < avatar_cpp.find("SpawnedVisualActor = CandidateActor;")
            and base_configure_fails_closed
            and base_apply_fails_closed
        )
        evidence: Any = {
            "diff_return_code": diff_result.returncode,
            "session8_diff_return_code": session8_diff_result.returncode,
            "status_return_code": status_result.returncode,
            "baseline": SESSION5_PLUGIN_BASELINE,
            "session8_baseline": SESSION7_PLUGIN_BASELINE,
            "allowed_paths": list(EXPECTED_SUCCESSOR_PLUGIN_CHANGES),
            "session8_allowed_paths": list(SESSION8_PLUGIN_CHANGES),
            "changed_paths": changed_paths,
            "session8_changed_paths": session8_changed_paths,
            "untracked_paths": untracked_paths,
            "missing_outfit_tokens": [
                token for token in required_outfit_tokens if token not in outfit_component
            ],
            "missing_customization_tokens": [
                token for token in required_customization_tokens
                if token not in customization_component
            ],
            "guarded_morph_write_count": customization_morph_writes,
            "body_savegame_fields": body_block.count("SaveGame"),
            "throw_style_savegame_fields": throw_block.count("SaveGame"),
            "whole_file_savegame_tokens": character_types.count("SaveGame"),
            "missing_avatar_header_tokens": [
                token for token in avatar_header_tokens if token not in avatar_header
            ],
            "missing_avatar_cpp_tokens": [
                token for token in avatar_cpp_tokens if token not in avatar_cpp
            ],
            "avatar_deferred_spawn_count": avatar_cpp.count(
                "SpawnActorDeferred<AActor>"),
            "avatar_ready_broadcast_count": avatar_cpp.count(
                "OnAvatarBackendReady.Broadcast("),
            "avatar_base_configure_fails_closed": base_configure_fails_closed,
            "avatar_base_apply_fails_closed": base_apply_fails_closed,
        }
    except (OSError, subprocess.TimeoutExpired) as exc:
        passed = False
        evidence = {"error": str(exc)}
    validator.add(
        "plugin.installed_source_unchanged",
        "plugin",
        passed,
        "Installed UE 5.8 plugin differs from Session 5 in exactly five reviewed files, with Session 8 limited to two fail-closed avatar-backend files",
        evidence,
    )


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Statically validate Session 4 character-creator wiring without launching Unreal."
    )
    parser.add_argument(
        "--project-root",
        default=str(DEFAULT_ROOT),
        help="DiscGolfTour repository root (default: inferred from this script).",
    )
    parser.add_argument(
        "--report",
        default=None,
        help="JSON report path (default: <root>/Saved/CharacterFramework/Session4WiringValidation.json).",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    root = Path(args.project_root).expanduser().absolute()
    report_path = (
        Path(args.report).expanduser().absolute()
        if args.report
        else root / "Saved" / "CharacterFramework" / "Session4WiringValidation.json"
    )
    validator = Validator(root)
    validator.add(
        "project.uproject",
        "project",
        (root / "DiscGolfTour.uproject").is_file(),
        "DiscGolfTour project root is valid",
        {"root": str(root)},
    )

    _validate_schema(validator)
    _validate_runtime_schema_and_dto(validator)
    _validate_transient_runtime_profile(validator)
    _validate_authority(validator)
    _validate_left_boundary(validator)
    _validate_ui(validator)
    _validate_rig_seams(validator)
    _validate_assets_and_paths(validator)
    _validate_plugin_immutability(validator)

    failed = [check for check in validator.checks if not check.passed]
    categories: dict[str, dict[str, int]] = {}
    for check in validator.checks:
        counts = categories.setdefault(check.category, {"passed": 0, "failed": 0})
        counts["passed" if check.passed else "failed"] += 1

    report = {
        "schema": "DiscGolfTour.Session4WiringValidation.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_SESSION4_CHARACTER_CREATOR_WIRING" if not failed else "FAIL",
        "validation_mode": "STATIC_NO_UNREAL_LAUNCH",
        "project_root": str(root),
        "checks_total": len(validator.checks),
        "checks_passed": len(validator.checks) - len(failed),
        "checks_failed": len(failed),
        "category_counts": categories,
        "failed_check_ids": [check.check_id for check in failed],
        "checks": [asdict(check) for check in validator.checks],
        "notes": [
            "This report does not replace Unreal reflection, strict rig, asset-graph, build, automation, or visual validation.",
            "Binary CR/ABP/WBP graph semantics are delegated to validate_dg_character_session4_assets.py.",
            "Creator throw-style values are presentation-only; existing project throw/flight authority remains unchanged.",
        ],
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    for check in failed:
        print(f"FAIL {check.check_id}: {check.summary}", file=sys.stderr)
    if failed:
        print(
            f"SESSION 4 CHARACTER CREATOR WIRING FAIL: failed={len(failed)} "
            f"total={len(validator.checks)} report={report_path}",
            file=sys.stderr,
        )
        return 1
    print(
        f"SESSION 4 CHARACTER CREATOR WIRING PASS: checks={len(validator.checks)} "
        f"report={report_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
