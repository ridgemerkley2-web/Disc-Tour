#!/usr/bin/env python3
"""Fail-closed Session 10 environment contract validator.

This is a source/evidence gate. It never launches Unreal, saves assets, cooks, or
promotes package evidence beyond the exact Development closure recorded here.
"""

from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session10EnvironmentContract.json")
REPORT_PATH = Path("Saved/EnvironmentReports/DG_Session10EnvironmentAudit.json")

NORMAL_STATUS = "PASS_TECHNICAL_ENVIRONMENT_CONTRACT_RELEASE_BLOCKED"
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_PROVENANCE_AND_ENVIRONMENT_READINESS"
S9_STATUS = "PASS_CONTRACT_RELEASE_BLOCKED"

S9_BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
]
S10_BLOCKERS = [
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
]

EXPECTED_STAGED_ENVIRONMENT_PACKAGES = [
    "/Game/Environment/Forest/DA_TemperateMountainForest",
    "/Game/Environment/Forest/DA_TemperateMountainForest_Assets",
    "/Game/Environment/Forest/Materials/MPC_EnvironmentWind",
    "/Game/Environment/Forest/PCG/PCG_TemperateMountainForest",
]

EXPECTED_PACKAGE_CONTAINERS = [
    {
        "path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak",
        "bytes": 11317153,
        "sha256": "59038C419FADEE64DC6A8B8AC69C37BC1AA3AF4C10F8415983A435C08895298F",
    },
    {
        "path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas",
        "bytes": 1038756432,
        "sha256": "0EBD10D81993F165AF2D488CE1BC2D8B6A7AB45B7D99A70D3FC5E0801077DFEE",
    },
    {
        "path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
        "bytes": 524578,
        "sha256": "76329E10AA34166C917E2E85DDF9E0DBDE85EF3D8ABDBB38D960AFFAFF945CA0",
    },
    {
        "path": "DiscGolfTour/Content/Paks/global.ucas",
        "bytes": 3521664,
        "sha256": "2277FDA0498C6C9C4BA81CC60B8CF41BF13276A43B34202CB14545FA75A4DF28",
    },
    {
        "path": "DiscGolfTour/Content/Paks/global.utoc",
        "bytes": 842,
        "sha256": "D0129C11FEC4FE3D5A255BB586210FDEC029830F109FF1B115C06D30CB78E6CE",
    },
]

EXPECTED_ASSETS = [
    {
        "asset_id": "temperate_mountain_forest_asset_set",
        "disk_path": "Content/Environment/Forest/DA_TemperateMountainForest_Assets.uasset",
        "package": "/Game/Environment/Forest/DA_TemperateMountainForest_Assets",
        "object": "/Game/Environment/Forest/DA_TemperateMountainForest_Assets.DA_TemperateMountainForest_Assets",
        "expected_bytes": 10388,
        "sha256": "ECE5EE0D5750585A702F333C93E71C82B88981C37D4FFF35AEFE288B9D63D9B9",
        "status": "DEVELOPMENT_ONLY_PENDING_BINDING_AND_PROVENANCE_CLOSURE",
        "release_use_allowed": False,
    },
    {
        "asset_id": "temperate_mountain_forest_preset",
        "disk_path": "Content/Environment/Forest/DA_TemperateMountainForest.uasset",
        "package": "/Game/Environment/Forest/DA_TemperateMountainForest",
        "object": "/Game/Environment/Forest/DA_TemperateMountainForest.DA_TemperateMountainForest",
        "expected_bytes": 2758,
        "sha256": "1E20257AAF6A43E6FBE67F4C39263D284FBA6D73ABDC3AEA4BDE037F7C84A4BC",
        "status": "DEVELOPMENT_ONLY_PENDING_BINDING_AND_PROVENANCE_CLOSURE",
        "release_use_allowed": False,
    },
    {
        "asset_id": "environment_wind_parameter_collection",
        "disk_path": "Content/Environment/Forest/Materials/MPC_EnvironmentWind.uasset",
        "package": "/Game/Environment/Forest/Materials/MPC_EnvironmentWind",
        "object": "/Game/Environment/Forest/Materials/MPC_EnvironmentWind.MPC_EnvironmentWind",
        "expected_bytes": 2201,
        "sha256": "38E994B9B79144269272A5D0A3E41314524DF931457446AEC39993FB9D5FA9C9",
        "status": "DEVELOPMENT_ONLY_PENDING_BINDING_AND_PROVENANCE_CLOSURE",
        "release_use_allowed": False,
    },
    {
        "asset_id": "temperate_mountain_forest_pcg_graph",
        "disk_path": "Content/Environment/Forest/PCG/PCG_TemperateMountainForest.uasset",
        "package": "/Game/Environment/Forest/PCG/PCG_TemperateMountainForest",
        "object": "/Game/Environment/Forest/PCG/PCG_TemperateMountainForest.PCG_TemperateMountainForest",
        "expected_bytes": 705957,
        "sha256": "86DF264918903F0F18832D6B078F4AFE032B3DFC31D64134F2B2CD2F3B17D159",
        "status": "DEVELOPMENT_ONLY_DORMANT_ON_DEMAND_PENDING_BINDING_REVIEW",
        "release_use_allowed": False,
    },
]

QUARANTINE_IMPORTS = [
    {
        "source_id": "fab_project_nature_spruce_forest",
        "disk_root": "Content/PN_interactiveSpruceForest",
        "package_root": "/Game/PN_interactiveSpruceForest",
        "release_use_allowed": False,
    },
    {
        "source_id": "fab_greenbuggames_stump_scanned",
        "disk_root": "Content/Stump_Scanned",
        "package_root": "/Game/Stump_Scanned",
        "release_use_allowed": False,
    },
    {
        "source_id": "fab_tharlevfx_water_materials",
        "disk_root": "Content/WaterMaterials",
        "package_root": "/Game/WaterMaterials",
        "release_use_allowed": False,
    },
]

LEGACY_REFERENCE_IDS = [
    "BrewsterRidgeTreeLine",
    "NorthwoodBlackCompression",
    "IdlewildLakeFrame",
]

REQUIRED_INTEGRATION_FILES = [
    "Source/DiscGolfTour/DiscGolfQualityAdapter.h",
    "Source/DiscGolfTour/DiscGolfQualityAdapter.cpp",
    "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.h",
    "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.cpp",
    "Source/DiscGolfTour/DevCourseBootstrap.h",
    "Source/DiscGolfTour/DevCourseBootstrap.cpp",
    "Source/DiscGolfTour/DiscGolfEnvironmentController.h",
    "Source/DiscGolfTour/DiscGolfEnvironmentController.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/DiscGolfTour.Build.cs",
    "Source/DiscGolfTour/Tests/DiscGolfQualityAdapterTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfBuiltInEnvironmentProviderTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession10EnvironmentContractTests.cpp",
    "Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.h",
    "Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfEnvironmentAssetBinderTests.cpp",
    "Scripts/run-environment-asset-binding-workflow.py",
]

FROZEN_INTEGRATION_HASHES = [
    {"path": "Source/DiscGolfTour/DiscGolfQualityAdapter.h", "sha256": "B0EC1C45BA9B49579B8DE933CC5385355D9A4DE84108B4F51D19D856FD45C16F"},
    {"path": "Source/DiscGolfTour/DiscGolfQualityAdapter.cpp", "sha256": "2B467179C891CB21AEE63F2AE1B65F6FADB144809BCDC1237E3227166E4F7BAC"},
    {"path": "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.h", "sha256": "630E975EA0CE8E4C5AFEDA070EDF1724E84DC9B9F43C5A5B71A60E2A1D164604"},
    {"path": "Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.cpp", "sha256": "F4066C96DE58992EE1D69177F0EB29307B08E5E6A2593A74C8D1554DAD49CD0D"},
    {"path": "Source/DiscGolfTour/Tests/DiscGolfQualityAdapterTests.cpp", "sha256": "70C8BC64839CBF307FE54738FBD242125A98144F51075601F3A23B6FA17D7C8F"},
    {"path": "Source/DiscGolfTour/Tests/DiscGolfBuiltInEnvironmentProviderTests.cpp", "sha256": "3283F60B05BF3E392DACDE93661468C36A29A874A5A36DFC5867EB4DD7577BF9"},
]

RUNTIME_TEST_NAMES = [
    "DiscGolfTour.Quality.Adapter.PlayerPresetMapping",
    "DiscGolfTour.Quality.Adapter.OmenCaptureContract",
    "DiscGolfTour.Quality.Adapter.CurrentProfileResolution",
    "DiscGolfTour.Quality.Adapter.PureResolutionNoGlobalMutation",
    "DiscGolfTour.Quality.Adapter.CollisionAuthorityInvariant",
    "DiscGolfTour.Environment.BuiltInProvider.ClearContract",
    "DiscGolfTour.Environment.BuiltInProvider.OvercastContract",
    "DiscGolfTour.Environment.BuiltInProvider.UnsupportedWeatherFailsSafe",
    "DiscGolfTour.Session10.Environment.ContractFailClosed",
]

BINDER_TEST_NAMES = [
    "DiscGolfTour.Environment.AssetBinder.ProvenanceGateFailsClosed",
    "DiscGolfTour.Environment.AssetBinder.ScanDoesNotMutate",
    "DiscGolfTour.Environment.AssetBinder.InvalidApprovalIsAtomic",
    "DiscGolfTour.Environment.AssetBinder.StructuralCompletenessRejectsInvalidCategories",
]


class StrictJsonError(ValueError):
    pass


def _reject_constant(value: str) -> None:
    raise StrictJsonError(f"non-finite JSON number is forbidden: {value}")


def _strict_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _validate_json_numbers(value: Any, label: str = "$") -> None:
    if type(value) is int and not (-(2**63) <= value <= (2**63 - 1)):
        raise StrictJsonError(f"integer outside signed 64-bit range at {label}")
    if type(value) is float and not math.isfinite(value):
        raise StrictJsonError(f"non-finite JSON number at {label}")
    if isinstance(value, dict):
        for key, child in value.items():
            _validate_json_numbers(child, f"{label}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _validate_json_numbers(child, f"{label}[{index}]")


def load_json(path: Path) -> Any:
    try:
        loaded = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_strict_pairs,
            parse_constant=_reject_constant,
        )
        _validate_json_numbers(loaded)
        return loaded
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _normalize_staged_manifest_path(value: str) -> str:
    """Normalize a staged manifest source path without weakening directory boundaries."""
    normalized = re.sub(r"/+", "/", value.split("\t", 1)[0].strip().strip('"').replace("\\", "/"))
    while normalized.startswith("../"):
        normalized = normalized[3:]
    if normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized.lstrip("/").casefold()


def _staged_entry_is_within_package(entry: str, package_root: str) -> bool:
    if not package_root.startswith("/Game/"):
        return False
    staged_root = (
        "discgolftour/content/" + package_root[len("/Game/"):]
    ).casefold().rstrip("/")
    normalized_entry = _normalize_staged_manifest_path(entry)
    return normalized_entry == staged_root or normalized_entry.startswith(staged_root + "/")


def _staged_environment_package(entry: str) -> str | None:
    """Convert one normalized forest .uasset manifest entry to a /Game package."""
    normalized_entry = _normalize_staged_manifest_path(entry)
    prefix = "discgolftour/content/"
    suffix = ".uasset"
    if not normalized_entry.startswith(prefix + "environment/forest/"):
        return None
    if not normalized_entry.endswith(suffix):
        return None
    relative = normalized_entry[len(prefix):-len(suffix)]
    return "/game/" + relative


def _same_json_value(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return set(actual) == set(expected) and all(
            _same_json_value(actual[key], child) for key, child in expected.items()
        )
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(
            _same_json_value(a, e) for a, e in zip(actual, expected)
        )
    return actual == expected


def _exact(actual: Any, expected: Any, label: str, errors: list[str]) -> None:
    if not _same_json_value(actual, expected):
        errors.append(f"{label}: expected {expected!r}, got {actual!r}")


def validate_contract_shape(contract: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(contract, dict):
        return ["contract: root must be an object"]

    expected_top_keys = {
        "schema", "schema_version", "contract_id", "audit_scope", "normal_status",
        "release_status", "authority", "source_environment_inventory",
        "presentation_contract", "runtime_pcg_contract", "assigned_readiness_contract",
        "runtime_statistics_contract", "runtime_integration_contract", "quarantine_contract", "session9_continuity",
        "staged_closure", "optional_adapters", "session10_release_blockers",
        "release_blockers",
    }
    _exact(set(contract), expected_top_keys, "contract keys", errors)
    _exact(contract.get("schema"), "DiscGolfTour.Session10EnvironmentContract.v1", "schema", errors)
    _exact(contract.get("schema_version"), 1, "schema_version", errors)
    _exact(contract.get("contract_id"), "session10_environment_fallback_v1", "contract_id", errors)
    _exact(contract.get("audit_scope"), "TECHNICAL_ENVIRONMENT_SOURCE_AND_EDITOR_AUTHORING_GATE_NOT_PRODUCTION_ART_OR_RELEASE_APPROVAL", "audit_scope", errors)
    _exact(contract.get("normal_status"), NORMAL_STATUS, "normal_status", errors)
    _exact(contract.get("release_status"), RELEASE_STATUS, "release_status", errors)
    _exact(contract.get("authority"), "ENVIRONMENT_PRESENTATION_ONLY_NO_GAMEPLAY_PHYSICS_RULES_OR_RELEASE_AUTHORITY", "authority", errors)

    inventory = contract.get("source_environment_inventory")
    expected_inventory = {
        "root": "Content/Environment/Forest",
        "expected_file_count": 4,
        "expected_total_bytes": 721304,
        "content_status": "DEVELOPMENT_FALLBACK_ENVIRONMENT_PENDING_PRODUCTION_READINESS",
        "production_environment_ready": False,
        "release_use_allowed": False,
        "exact_assets": EXPECTED_ASSETS,
    }
    _exact(inventory, expected_inventory, "source_environment_inventory", errors)

    expected_presentation = {
        "path": "Data/PineRidgePresentation.json",
        "sha256": "0B259E50B353B59BA2F940FB76C121CE69487BEB435D41E9CF21337219B2E8B0",
        "schema": "disc_golf_course_presentation",
        "schema_version": 1,
        "course_id": "PineRidgeChampionship",
        "layout_id": "Championship",
        "collision_profile_id": "PineRidgeCompetitiveV2_Fixtures",
        "collision_invariant_across_quality": True,
        "assets_ready_expected": False,
        "required_forest_reference_ids": [
            "OpeningBroadTreeLine", "NeedleCanopyCompression", "GalleryLakeFrame"
        ],
    }
    _exact(contract.get("presentation_contract"), expected_presentation, "presentation_contract", errors)

    expected_pcg = {
        "runtime_controller_source": "Source/DiscGolfTour/DiscGolfEnvironmentController.cpp",
        "editor_authoring_controller_source": "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp",
        "runtime_source_root": "Source/DiscGolfTour",
        "runtime_module_rules": "Source/DiscGolfTour/DiscGolfTour.Build.cs",
        "editor_module_rules": "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
        "generation_trigger": "GenerateOnDemand",
        "generate_on_drop_when_on_demand": False,
        "expected_runtime_generate_forest_callsite_count": 0,
        "expected_generated_instance_count": 0,
        "runtime_pcg_module_dependency_allowed": False,
        "runtime_pcg_component_or_generation_api_allowed": False,
        "editor_pcg_module_dependency_required": True,
        "plugin_target_allow_list": ["Editor"],
        "shipping_never_cook_directory": "/Game/Environment/Forest/PCG",
        "runtime_status": "AUTHORED_DATA_RUNTIME_EDITOR_ONLY_ON_DEMAND_PCG_PENDING_FINAL_ASSET_BINDINGS",
        "authored_environment_fallback_active": True,
        "release_use_allowed": False,
    }
    _exact(contract.get("runtime_pcg_contract"), expected_pcg, "runtime_pcg_contract", errors)

    expected_readiness = {
        "report_path": "Saved/Developer/EnvironmentAssetBindingReport.json",
        "report_sha256": "A79D1BA42AC422C7895E4A1017E815C987AA90DA6F1DA38F85E336DC58F4F403",
        "schema": "disc_golf_environment_asset_binding_report",
        "schema_version": 2,
        "readiness_evaluated": True,
        "structurally_complete": True,
        "production_ready": False,
        "expected_slot_count": 16,
        "expected_populated_slot_count": 3,
        "expected_missing_slot_count": 13,
        "expected_ready_slot_count": 0,
        "status": "BLOCKED_PENDING_APPROVED_VISUAL_COLLISION_INTERACTION_WIND_AND_LOD_BINDINGS",
    }
    _exact(contract.get("assigned_readiness_contract"), expected_readiness, "assigned_readiness_contract", errors)

    expected_stats = {
        "report_path": "Saved/EnvironmentReports/PineRidgeHole1EnvironmentStatistics.json",
        "report_sha256": "4C3DE78855CBF7A5F3264861EAA2039BFB492462D718517A42408420FD524E51",
        "asset_mode": "PROVISIONAL_CC0_AND_ENGINE_CONTENT",
        "visual_acceptance": "PENDING_FINAL_FAB_IMPORT",
        "pcg_generated_instances": 0,
        "pcg_generation_on_demand": True,
        "collision_quality_invariant": True,
    }
    _exact(contract.get("runtime_statistics_contract"), expected_stats, "runtime_statistics_contract", errors)

    expected_integration = {
        "authority": "SOURCE_SEMANTIC_GATE_FOR_PRESENTATION_ONLY_ENVIRONMENT_INTEGRATION",
        "release_use_allowed": False,
        "required_files": REQUIRED_INTEGRATION_FILES,
        "frozen_file_hashes": FROZEN_INTEGRATION_HASHES,
        "quality_adapter": {
            "player_profile_order": ["Performance", "Medium", "High", "Cinematic"],
            "course_visual_tier_order": ["Low", "Medium", "High", "High"],
            "environment_quality_order": ["Performance", "Performance", "High", "Cinematic"],
            "medium_to_performance_deliberately_frozen": True,
            "omen_profile_id": "OmenGameplay1080pHighFoliageV1",
            "omen_resolution_quality": 100.0,
            "omen_quality_two_fields": [
                "ViewDistanceQuality", "AntiAliasingQuality", "ShadowQuality",
                "GlobalIlluminationQuality", "ReflectionQuality", "PostProcessQuality",
                "TextureQuality", "EffectsQuality", "ShadingQuality",
            ],
            "omen_foliage_quality": 3,
            "omen_landscape_policy": "PRESERVE_CALLER_BASELINE",
            "omen_course_visual_tier": "High",
            "omen_environment_quality": "High",
            "mutates_global_scalability": False,
        },
        "built_in_provider": {
            "default_time_of_day_hours": 14.0,
            "default_sun_rotation": [-38.0, -35.0, 0.0],
            "default_sun_intensity": 1.25,
            "default_sky_light_intensity": 0.72,
            "supported_modes": ["Clear", "Overcast"],
            "precipitation_fallback_status": "UNSUPPORTED_PRECIPITATION_FALLBACK_OVERCAST_NO_GAMEPLAY_EFFECTS",
            "non_finite_input_policy": "REJECT",
            "gameplay_neutral": True,
            "tick_enabled": False,
        },
        "bootstrap_ownership": {
            "spawn_lighting_returns_success": True,
            "spawn_failure_blocks_course_build": True,
            "provider_owner_is_bootstrap": True,
            "provider_stored_only_after_apply_success": True,
            "end_play_destroys_provider": True,
            "end_play_clears_provider_and_lighting_state": True,
            "quality_snapshot_drives_course_and_environment_presentation": True,
        },
        "game_mode_integration": {
            "omen_profile_applied_before_course_assembly": True,
            "resolved_quality_snapshot_passed_to_persistent_course": True,
            "performance_profile_id_comes_from_adapter": True,
            "automatically_invokes_environment_wind_synchronization": False,
        },
        "controller_gameplay_neutrality": {
            "default_disc_flight_wind_synchronization_enabled": False,
            "bootstrapped_disc_flight_wind_synchronization_enabled": False,
            "production_required_binding_slot_count": 16,
            "production_requires_all_slots_populated": True,
            "editor_authoring_readiness_requires_graph": True,
            "shipping_runtime_readiness_uses_graph": False,
            "focused_test_name": "DiscGolfTour.Session10.Environment.ContractFailClosed",
            "focused_test_asserts_default_sync_disabled": True,
            "focused_test_asserts_incomplete_bindings_rejected": True,
            "focused_test_asserts_complete_bindings_with_graph_accepted": True,
        },
        "required_runtime_test_names": RUNTIME_TEST_NAMES,
        "binder_contract": {
            "approved_project_runtime_root": "/Game/Presentation/Course/PineRidge",
            "approved_engine_primitive_root": "/Engine/BasicShapes",
            "quarantined_roots": [
                "/Game/PN_interactiveSpruceForest", "/Game/Stump_Scanned", "/Game/WaterMaterials"
            ],
            "scan_roots_fail_closed_before_registry_query": True,
            "scanned_assets_rechecked_by_runtime_path_policy": True,
            "assigned_variants_rechecked_by_runtime_path_policy": True,
            "apply_validates_all_paths_before_mutation": True,
            "scan_and_proposal_are_nonmutating": True,
            "structural_category_count": 16,
            "structural_category_range_inclusive": [0, 15],
            "structural_categories_must_be_unique_and_complete": True,
            "required_test_names": BINDER_TEST_NAMES,
        },
        "workflow_contract": {
            "exact_approved_runtime_roots": ["/Game/Presentation/Course/PineRidge"],
            "environment_override_allowed": False,
            "scan_requires_provenance_accepted": True,
            "proposal_requires_provenance_accepted": True,
            "invokes_apply_approved_bindings": False,
        },
    }
    _exact(contract.get("runtime_integration_contract"), expected_integration, "runtime_integration_contract", errors)

    expected_quarantine = {
        "never_cook_config": "Config/DefaultGame.ini",
        "never_cook_policy": "REQUIRE_EACH_QUARANTINED_ROOT_EXACTLY_ONCE_ALLOW_ADDITIVE_LATER_SESSION_EXCLUSIONS",
        "runtime_reference_policy": "DENY_EXCEPT_EXPLICIT_NEVERCOOK_DECLARATIONS_AND_PRIVATE_AUDIT_EVIDENCE",
        "exact_imports": QUARANTINE_IMPORTS,
    }
    _exact(contract.get("quarantine_contract"), expected_quarantine, "quarantine_contract", errors)

    expected_s9 = {
        "contract_path": "Config/DG_BrandLicenseContract.json",
        "contract_sha256": "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001",
        "validator_path": "Scripts/validate_dg_session9_brand_license.py",
        "validator_sha256": "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550",
        "expected_release_ready": False,
        "expected_release_blocker_count": 8,
        "release_blockers": S9_BLOCKERS,
    }
    _exact(contract.get("session9_continuity"), expected_s9, "session9_continuity", errors)

    expected_stage = {
        "status": "ACCEPTED_FRESH_DEVELOPMENT_PACKAGE_CLOSURE",
        "closure_id": "5ea6982a-a89f-4948-a49c-212c4a889207",
        "build_configuration": "Development",
        "observed_stage_root": "C:/DGTour_Packages/S10_EnvironmentClosure_5ea6982a-a89f-4948-a49c-212c4a889207/Windows",
        "observed_nonufs_manifest": "C:/DGTour_Packages/S10_EnvironmentClosure_5ea6982a-a89f-4948-a49c-212c4a889207/Windows/Manifest_NonUFSFiles_Win64.txt",
        "observed_nonufs_manifest_bytes": 3322,
        "observed_nonufs_manifest_sha256": "DC946BC259C1E9680D6F17B4506D837A3D5E171923B64590D68686F34296BDB5",
        "observed_ufs_manifest": "C:/DGTour_Packages/S10_EnvironmentClosure_5ea6982a-a89f-4948-a49c-212c4a889207/Windows/Manifest_UFSFiles_Win64.txt",
        "observed_ufs_manifest_bytes": 300050,
        "observed_ufs_manifest_sha256": "FCDC543D72E550507C8E246445EC4CF41E5F2E931C7183E46CA0AC9A5229EC09",
        "observed_presentation_path": "C:/DGTour_Packages/S10_EnvironmentClosure_5ea6982a-a89f-4948-a49c-212c4a889207/Windows/DiscGolfTour/Data/PineRidgePresentation.json",
        "observed_presentation_bytes": 2184,
        "source_presentation_sha256": "0B259E50B353B59BA2F940FB76C121CE69487BEB435D41E9CF21337219B2E8B0",
        "observed_staged_presentation_sha256": "0B259E50B353B59BA2F940FB76C121CE69487BEB435D41E9CF21337219B2E8B0",
        "expected_environment_packages": EXPECTED_STAGED_ENVIRONMENT_PACKAGES,
        "observed_environment_packages": EXPECTED_STAGED_ENVIRONMENT_PACKAGES,
        "observed_staged_environment_asset_count": 4,
        "expected_fresh_environment_asset_count": 4,
        "observed_staged_contains_forbidden_legacy_reference_ids": False,
        "observed_quarantined_package_roots": [],
        "package_listing_evidence": "UFS_MANIFEST_CORROBORATED_BY_UNREALPAK_IOSTORE_LIST",
        "uat_log_path": "Saved/Logs/Session10/5ea6982a-a89f-4948-a49c-212c4a889207/UAT_Session10_EnvironmentClosure.log",
        "uat_log_bytes": 86126,
        "uat_log_sha256": "BB09C8D5C1D877853857DFB54EF7618C3675BEC14933719525973CFFF3F0C032",
        "cook_packages": 984,
        "cook_incrementally_skipped": 0,
        "cook_skipped_by_platform": 7,
        "cook_total_packages": 991,
        "container_files": EXPECTED_PACKAGE_CONTAINERS,
        "fresh_package_closure_accepted": True,
        "release_use_allowed": False,
    }
    _exact(contract.get("staged_closure"), expected_stage, "staged_closure", errors)

    expected_adapters = [
        {"adapter_id": "ultra_dynamic_sky", "status": "NOT_ACQUIRED_NOT_INTEGRATED", "runtime_required": False, "release_use_allowed": False},
        {"adapter_id": "brushify_forest", "status": "NOT_ACQUIRED_NOT_INTEGRATED", "runtime_required": False, "release_use_allowed": False},
    ]
    _exact(contract.get("optional_adapters"), expected_adapters, "optional_adapters", errors)
    _exact(contract.get("session10_release_blockers"), S10_BLOCKERS, "session10_release_blockers", errors)
    _exact(contract.get("release_blockers"), S9_BLOCKERS + S10_BLOCKERS, "release_blockers", errors)
    return errors


def _check(name: str, passed: bool, detail: str, checks: list[dict[str, Any]], errors: list[str]) -> None:
    checks.append({"check": name, "passed": passed, "detail": detail})
    if not passed:
        errors.append(f"{name}: {detail}")


def _load_evidence(path: Path, label: str, errors: list[str]) -> Any | None:
    try:
        return load_json(path)
    except StrictJsonError as exc:
        errors.append(f"{label}: {exc}")
        return None


def _automation_test_names(source: str) -> set[str]:
    return set(re.findall(r'"(DiscGolfTour\.[A-Za-z0-9_.]+)"', source))


def validate_project(contract: dict[str, Any]) -> tuple[list[dict[str, Any]], list[str]]:
    checks: list[dict[str, Any]] = []
    errors: list[str] = []

    inventory = contract["source_environment_inventory"]
    asset_root = ROOT / inventory["root"]
    actual_files = sorted(p for p in asset_root.rglob("*") if p.is_file()) if asset_root.is_dir() else []
    expected_paths = sorted(ROOT / item["disk_path"] for item in EXPECTED_ASSETS)
    _check(
        "environment_exact_inventory",
        actual_files == expected_paths,
        f"expected exactly 4 files under {inventory['root']}; observed {len(actual_files)}",
        checks, errors,
    )
    total_bytes = sum(p.stat().st_size for p in actual_files)
    _check(
        "environment_total_bytes",
        total_bytes == inventory["expected_total_bytes"],
        f"expected {inventory['expected_total_bytes']}; observed {total_bytes}",
        checks, errors,
    )
    for item in EXPECTED_ASSETS:
        path = ROOT / item["disk_path"]
        exists = path.is_file()
        size = path.stat().st_size if exists else None
        digest = sha256(path) if exists else None
        _check(
            f"asset:{item['asset_id']}",
            exists and size == item["expected_bytes"] and digest == item["sha256"],
            f"exists={exists} bytes={size} sha256={digest}",
            checks, errors,
        )

    quarantine_tokens = [entry["package_root"] for entry in QUARANTINE_IMPORTS]
    binary_hits: list[str] = []
    for path in actual_files:
        blob = path.read_bytes()
        for token in quarantine_tokens:
            if token.encode("utf-8") in blob or token.lstrip("/").encode("utf-8") in blob:
                binary_hits.append(f"{path.relative_to(ROOT).as_posix()}:{token}")
    _check(
        "environment_assets_deny_quarantine_references",
        not binary_hits,
        "no quarantined package roots found" if not binary_hits else "; ".join(binary_hits),
        checks, errors,
    )

    ini_path = ROOT / contract["quarantine_contract"]["never_cook_config"]
    ini_text = ini_path.read_text(encoding="utf-8") if ini_path.is_file() else ""
    never_cook_lines = re.findall(r'^\+DirectoriesToNeverCook=\(Path="([^"]+)"\)\s*$', ini_text, re.MULTILINE)
    never_cook_unique = len(never_cook_lines) == len(set(never_cook_lines))
    quarantine_counts = {token: never_cook_lines.count(token) for token in quarantine_tokens}
    _check(
        "quarantine_required_never_cook_roots_exactly_once",
        never_cook_unique and all(count == 1 for count in quarantine_counts.values()),
        f"required_counts={quarantine_counts!r} unique={never_cook_unique} observed={never_cook_lines!r}",
        checks, errors,
    )

    runtime_hits: list[str] = []
    runtime_text_files = list((ROOT / "Data").rglob("*.json"))
    for pattern in ("*.h", "*.cpp"):
        runtime_text_files.extend((ROOT / "Source/DiscGolfTour").rglob(pattern))
    for path in sorted(set(runtime_text_files)):
        text = path.read_text(encoding="utf-8", errors="replace")
        for token in quarantine_tokens:
            if token in text or token.lstrip("/") in text:
                runtime_hits.append(f"{path.relative_to(ROOT).as_posix()}:{token}")
    _check(
        "runtime_sources_deny_quarantine_references",
        not runtime_hits,
        "no quarantined package roots found" if not runtime_hits else "; ".join(runtime_hits),
        checks, errors,
    )

    presentation_contract = contract["presentation_contract"]
    presentation_path = ROOT / presentation_contract["path"]
    presentation = _load_evidence(presentation_path, "presentation", errors)
    presentation_hash = sha256(presentation_path) if presentation_path.is_file() else None
    forest_ids = [hole.get("forestReferenceId") for hole in presentation.get("holes", [])] if isinstance(presentation, dict) else []
    presentation_ok = isinstance(presentation, dict) and all([
        presentation_hash == presentation_contract["sha256"],
        presentation.get("schema") == presentation_contract["schema"],
        presentation.get("schemaVersion") == presentation_contract["schema_version"],
        presentation.get("courseId") == presentation_contract["course_id"],
        presentation.get("layoutId") == presentation_contract["layout_id"],
        presentation.get("collisionProfileId") == presentation_contract["collision_profile_id"],
        presentation.get("collisionInvariantAcrossQuality") is True,
        presentation.get("assetsReady") is False,
        forest_ids == presentation_contract["required_forest_reference_ids"],
        all(tier.get("affectsCollision") is False for tier in presentation.get("qualityTiers", [])),
        all(tier.get("collisionProfileId") == presentation_contract["collision_profile_id"] for tier in presentation.get("qualityTiers", [])),
    ])
    _check(
        "presentation_assets_not_ready_and_collision_invariant",
        presentation_ok,
        f"sha256={presentation_hash} assetsReady={presentation.get('assetsReady') if isinstance(presentation, dict) else None} forestReferenceIds={forest_ids!r}",
        checks, errors,
    )

    pcg_contract = contract["runtime_pcg_contract"]
    controller_path = ROOT / pcg_contract["runtime_controller_source"]
    controller_text = controller_path.read_text(encoding="utf-8") if controller_path.is_file() else ""
    authoring_path = ROOT / pcg_contract["editor_authoring_controller_source"]
    authoring_text = authoring_path.read_text(encoding="utf-8") if authoring_path.is_file() else ""
    pcg_config_ok = (
        "PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;" in authoring_text
        and "PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;" in authoring_text
        and "PCGComponent->GenerateLocal(" in authoring_text
        and "PCGComponent" not in controller_text
        and "GenerateForest" not in controller_text
    )
    _check(
        "editor_pcg_on_demand_runtime_pcg_absent_configuration",
        pcg_config_ok,
        "Editor authoring owns explicit GenerateOnDemand with generate-on-drop disabled; runtime owns no PCG component or generation API",
        checks, errors,
    )
    callsite_pattern = re.compile(r"(?:->|\.)\s*GenerateForest\s*\(")
    callsites: list[str] = []
    for pattern in ("*.h", "*.cpp"):
        for path in (ROOT / pcg_contract["runtime_source_root"]).rglob(pattern):
            text = path.read_text(encoding="utf-8", errors="replace")
            if callsite_pattern.search(text):
                callsites.append(path.relative_to(ROOT).as_posix())
    _check(
        "pcg_generate_forest_runtime_callsite_count",
        len(callsites) == pcg_contract["expected_runtime_generate_forest_callsite_count"],
        f"expected 0; observed {len(callsites)} in {callsites!r}",
        checks, errors,
    )
    runtime_pcg_hits: list[str] = []
    for pattern in ("*.h", "*.cpp"):
        for path in (ROOT / pcg_contract["runtime_source_root"]).rglob(pattern):
            if "Tests" in path.parts:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for token in ("UPCG", "PCGComponent", "GenerateForest", "GenerateLocal", "SetGraph"):
                if token in text:
                    runtime_pcg_hits.append(f"{path.relative_to(ROOT).as_posix()}:{token}")
    _check(
        "runtime_pcg_types_and_generation_apis_absent",
        not runtime_pcg_hits,
        "runtime production source contains no PCG type or generation API"
        if not runtime_pcg_hits else "; ".join(runtime_pcg_hits),
        checks, errors,
    )

    readiness_contract = contract["assigned_readiness_contract"]
    readiness_path = ROOT / readiness_contract["report_path"]
    readiness = _load_evidence(readiness_path, "assigned readiness report", errors)
    readiness_hash = sha256(readiness_path) if readiness_path.is_file() else None
    slots = readiness.get("slots", []) if isinstance(readiness, dict) else []
    populated = sum(
        1 for slot in slots
        if any(candidate.get("asset_path") for candidate in slot.get("candidates", []) if isinstance(candidate, dict))
    )
    missing = sum(1 for slot in slots if "MISSING" in slot.get("slot_statuses", []))
    ready = sum(1 for slot in slots if slot.get("slot_statuses") == ["READY"])
    readiness_ok = isinstance(readiness, dict) and all([
        readiness_hash == readiness_contract["report_sha256"],
        readiness.get("schema") == readiness_contract["schema"],
        readiness.get("schema_version") == readiness_contract["schema_version"],
        readiness.get("readiness_evaluated") is True,
        readiness.get("structurally_complete") is True,
        readiness.get("production_ready") is False,
        len(slots) == readiness_contract["expected_slot_count"],
        populated == readiness_contract["expected_populated_slot_count"],
        missing == readiness_contract["expected_missing_slot_count"],
        ready == readiness_contract["expected_ready_slot_count"],
    ])
    _check(
        "assigned_environment_readiness_false",
        readiness_ok,
        f"sha256={readiness_hash} slots={len(slots)} populated={populated} missing={missing} ready={ready} production_ready={readiness.get('production_ready') if isinstance(readiness, dict) else None}",
        checks, errors,
    )

    stats_contract = contract["runtime_statistics_contract"]
    stats_path = ROOT / stats_contract["report_path"]
    stats = _load_evidence(stats_path, "runtime statistics report", errors)
    stats_hash = sha256(stats_path) if stats_path.is_file() else None
    stats_ok = isinstance(stats, dict) and all([
        stats_hash == stats_contract["report_sha256"],
        stats.get("assetMode") == stats_contract["asset_mode"],
        stats.get("visualAcceptance") == stats_contract["visual_acceptance"],
        stats.get("pcgGeneratedInstances") == stats_contract["pcg_generated_instances"],
        stats.get("pcgGenerationOnDemand") is True,
        stats.get("collisionQualityInvariant") is True,
    ])
    _check(
        "runtime_statistics_confirm_dormant_pcg",
        stats_ok,
        f"sha256={stats_hash} generated={stats.get('pcgGeneratedInstances') if isinstance(stats, dict) else None}",
        checks, errors,
    )

    integration = contract["runtime_integration_contract"]
    missing_integration_files = [
        rel for rel in integration["required_files"] if not (ROOT / rel).is_file()
    ]
    _check(
        "runtime_integration_exact_required_file_inventory",
        not missing_integration_files,
        f"all {len(REQUIRED_INTEGRATION_FILES)} required integration files are present"
        if not missing_integration_files
        else f"missing={missing_integration_files!r}",
        checks, errors,
    )

    integration_hash_drift: list[str] = []
    for entry in integration["frozen_file_hashes"]:
        path = ROOT / entry["path"]
        actual_hash = sha256(path) if path.is_file() else None
        if actual_hash != entry["sha256"]:
            integration_hash_drift.append(
                f"{entry['path']}:expected={entry['sha256']}:actual={actual_hash}"
            )
    _check(
        "runtime_integration_frozen_quality_provider_hashes",
        not integration_hash_drift,
        "six stable quality/provider artifacts are byte-exact" if not integration_hash_drift
        else "; ".join(integration_hash_drift),
        checks, errors,
    )

    def source(rel: str) -> str:
        path = ROOT / rel
        return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""

    quality_h = source("Source/DiscGolfTour/DiscGolfQualityAdapter.h")
    quality_cpp = source("Source/DiscGolfTour/DiscGolfQualityAdapter.cpp")
    medium_start = quality_cpp.find("case 1:")
    medium_end = quality_cpp.find("case 3:", medium_start)
    medium_block = quality_cpp[medium_start:medium_end] if medium_start >= 0 and medium_end > medium_start else ""
    omen_required = [
        "Quality.ResolutionQuality = 100.0f;",
        "Quality.ViewDistanceQuality = 2;",
        "Quality.AntiAliasingQuality = 2;",
        "Quality.ShadowQuality = 2;",
        "Quality.GlobalIlluminationQuality = 2;",
        "Quality.ReflectionQuality = 2;",
        "Quality.PostProcessQuality = 2;",
        "Quality.TextureQuality = 2;",
        "Quality.EffectsQuality = 2;",
        "Quality.FoliageQuality = 3;",
        "Quality.ShadingQuality = 2;",
        'Result.ProfileId = TEXT("OmenGameplay1080pHighFoliageV1");',
        'Result.CourseVisualTierId = TEXT("High");',
        "Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::High;",
    ]
    quality_ok = all(token in quality_cpp for token in omen_required) and all([
        "Result.Profile = EDiscGolfRuntimeQualityProfile::Medium;" in medium_block,
        'Result.CourseVisualTierId = TEXT("Medium");' in medium_block,
        "Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::Performance;" in medium_block,
        "SetQualityLevels(" not in quality_cpp,
        "Scalability::FQualityLevels EngineQuality;" in quality_h,
    ])
    _check(
        "quality_adapter_medium_and_omen_semantics",
        quality_ok,
        "Medium->Performance frozen; Omen=100%, nine groups at 2, foliage 3, course High, environment High, baseline Landscape preserved",
        checks, errors,
    )

    provider_h = source("Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.h")
    provider_cpp = source("Source/DiscGolfTour/DiscGolfBuiltInEnvironmentProvider.cpp")
    provider_required = [
        "FMath::IsFinite(RequestedState.TimeOfDayHours)",
        "FMath::IsFinite(RequestedState.WeatherIntensity)",
        "FMath::IsFinite(RequestedState.Wetness)",
        "constexpr float DefaultTimeOfDayHours = 14.0f;",
        "constexpr float ClearSunIntensity = 1.25f;",
        "constexpr float ClearSkyIntensity = 0.72f;",
        'Resolved.Status = TEXT("BUILT_IN_CLEAR");',
        'Resolved.Status = TEXT("BUILT_IN_OVERCAST_LIGHTING_ONLY");',
        'Resolved.Status = TEXT("UNSUPPORTED_PRECIPITATION_FALLBACK_OVERCAST_NO_GAMEPLAY_EFFECTS");',
        "PrimaryActorTick.bCanEverTick = false;",
    ]
    provider_ok = all(token in provider_cpp for token in provider_required) and all([
        "return FRotator(-AltitudeDegrees, YawDegrees, 0.0f);" in provider_cpp,
        'bool IsGameplayNeutral() const { return true; }' in provider_h,
        "WindDirector" not in provider_cpp,
        "DiscFlight" not in provider_cpp,
        "Collision" not in provider_cpp,
        "Scoring" not in provider_cpp,
    ])
    _check(
        "built_in_provider_clear_default_and_gameplay_neutrality",
        provider_ok,
        "finite input gate, exact clear/overcast/fallback statuses and defaults, presentation-only no-tick provider",
        checks, errors,
    )

    bootstrap_h = source("Source/DiscGolfTour/DevCourseBootstrap.h")
    bootstrap_cpp = source("Source/DiscGolfTour/DevCourseBootstrap.cpp")
    apply_position = bootstrap_cpp.find("Candidate->ApplyEnvironmentState(InitialState, OutError)")
    store_position = bootstrap_cpp.find("BuiltInEnvironmentProvider = Candidate;")
    bootstrap_required = [
        "virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;",
        "bool SpawnLighting(FString& OutError);",
        "ADiscGolfBuiltInEnvironmentProvider* GetBuiltInEnvironmentProvider() const { return BuiltInEnvironmentProvider; }",
        "UPROPERTY() TObjectPtr<ADiscGolfBuiltInEnvironmentProvider> BuiltInEnvironmentProvider;",
    ]
    bootstrap_cpp_required = [
        "bool ADevCourseBootstrap::SpawnLighting(FString& OutError)",
        "Candidate->SetOwner(this);",
        "if (!Candidate->ApplyEnvironmentState(InitialState, OutError))",
        "void ADevCourseBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)",
        "BuiltInEnvironmentProvider->Destroy();",
        "BuiltInEnvironmentProvider = nullptr;",
        "bLightingSpawned = false;",
        "Super::EndPlay(EndPlayReason);",
        "EnvironmentController->Quality = QualityProfile.EnvironmentQuality;",
        "EnvironmentController->bSynchronizeDiscFlightWind = false;",
        "Candidate.TierId == QualityProfile.CourseVisualTierId",
    ]
    spawn_failure_guards = re.findall(
        r"if\s*\(!SpawnLighting\((?:LightingError|OutError)\)\)\s*"
        r"\{(?:(?!\}).)*?return nullptr;\s*\}",
        bootstrap_cpp,
        flags=re.DOTALL,
    )
    end_play_start = bootstrap_cpp.find(
        "void ADevCourseBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)"
    )
    end_play_end = bootstrap_cpp.find(
        "void ADevCourseBootstrap::DestroyGeneratedCourse()", end_play_start
    )
    end_play_body = (
        bootstrap_cpp[end_play_start:end_play_end]
        if end_play_start >= 0 and end_play_end > end_play_start else ""
    )
    end_play_order = [
        end_play_body.find("BuiltInEnvironmentProvider->Destroy();"),
        end_play_body.find("BuiltInEnvironmentProvider = nullptr;"),
        end_play_body.find("bLightingSpawned = false;"),
        end_play_body.find("Super::EndPlay(EndPlayReason);"),
    ]
    bootstrap_ok = all(token in bootstrap_h for token in bootstrap_required) and all(
        token in bootstrap_cpp for token in bootstrap_cpp_required
    ) and all([
        bootstrap_cpp.count("if (!SpawnLighting(") == 3,
        len(spawn_failure_guards) == 3,
        apply_position >= 0 and store_position > apply_position,
        "Candidate->Destroy();" in bootstrap_cpp[apply_position:store_position],
        all(position >= 0 for position in end_play_order),
        end_play_order == sorted(end_play_order),
    ])
    _check(
        "bootstrap_provider_fail_closed_ownership_and_quality_snapshot",
        bootstrap_ok,
        f"spawn_failure_guards={len(spawn_failure_guards)} apply_before_store={apply_position >= 0 and store_position > apply_position} ordered_end_play_cleanup={end_play_order == sorted(end_play_order) and all(position >= 0 for position in end_play_order)}",
        checks, errors,
    )

    game_mode_cpp = source("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
    game_mode_required = [
        "DiscGolfQualityAdapter::MakeOmenCaptureProfile(",
        "Scalability::SetQualityLevels(QualityProfile.EngineQuality, true);",
        "PerformanceCaptureProfile = QualityProfile.ProfileId.ToString();",
        "DiscGolfQualityAdapter::ResolveCurrent(Scalability::GetQualityLevels());",
        "DevBootstrap->BuildPersistentPineRidgeCourse(",
        "QualityProfile,",
    ]
    omen_apply_position = game_mode_cpp.find("ApplyOmenPerformanceCaptureProfile();")
    quality_resolve_position = game_mode_cpp.find(
        "DiscGolfQualityAdapter::ResolveCurrent(Scalability::GetQualityLevels());"
    )
    persistent_build_position = game_mode_cpp.find(
        "DevBootstrap->BuildPersistentPineRidgeCourse("
    )
    game_mode_ok = all(token in game_mode_cpp for token in game_mode_required) and all([
        omen_apply_position >= 0,
        quality_resolve_position > omen_apply_position,
        persistent_build_position > quality_resolve_position,
        "SynchronizeWindDirector(" not in game_mode_cpp,
    ])
    _check(
        "game_mode_omen_and_quality_snapshot_integration",
        game_mode_ok,
        "adapter applies Omen before assembly, passes one resolved quality snapshot into persistent course build, and never auto-synchronizes environment wind",
        checks, errors,
    )

    controller_h = source("Source/DiscGolfTour/DiscGolfEnvironmentController.h")
    controller_cpp = source("Source/DiscGolfTour/DiscGolfEnvironmentController.cpp")
    completeness_start = controller_cpp.find(
        "bool ADiscGolfEnvironmentController::IsProductionConfigurationComplete("
    )
    completeness_body = (
        controller_cpp[completeness_start:]
        if completeness_start >= 0 else ""
    )
    controller_ok = all([
        "bool bSynchronizeDiscFlightWind = false;" in controller_h,
        "static bool IsProductionConfigurationComplete(" in controller_h,
        "constexpr int32 RequiredBindingSlotCount = 16;" in completeness_body,
        "BindingSlotCount == RequiredBindingSlotCount" in completeness_body,
        "PopulatedSlotCount == RequiredBindingSlotCount" in completeness_body,
        "&& bHasAuthoringGraph;" in completeness_body,
        "Assets->Slots.Num()," in controller_cpp,
        "Assets->GetPopulatedSlotCount()," in controller_cpp,
        "!Preset->ForestGraph.IsNull());" in controller_cpp,
    ])
    _check(
        "environment_controller_gameplay_neutral_default_and_strict_readiness",
        controller_ok,
        "wind sync defaults off; production configuration requires exactly 16 slots, 16 populated bindings, and a forest graph",
        checks, errors,
    )

    build_rules = source("Source/DiscGolfTour/DiscGolfTour.Build.cs")
    editor_build_rules = source("Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs")
    project_descriptor = load_json(ROOT / "DiscGolfTour.uproject")
    pcg_plugins = [
        item for item in project_descriptor.get("Plugins", [])
        if isinstance(item, dict) and item.get("Name") == "PCG"
    ] if isinstance(project_descriptor, dict) else []
    _check(
        "runtime_editor_pcg_compile_and_plugin_boundary",
        '"Engine"' in build_rules
        and '"DiscGolfRuntimeFoundation"' in build_rules
        and '"PCG"' not in build_rules
        and editor_build_rules.count('"PCG"') == 1
        and pcg_plugins == [{"Name": "PCG", "Enabled": True, "TargetAllowList": ["Editor"]}]
        and never_cook_lines.count(pcg_contract["shipping_never_cook_directory"]) == 1,
        "runtime depends on Engine/foundation but not PCG; Editor owns PCG; plugin is Editor-only; graph is NeverCook",
        checks, errors,
    )

    quality_tests = source("Source/DiscGolfTour/Tests/DiscGolfQualityAdapterTests.cpp")
    provider_tests = source("Source/DiscGolfTour/Tests/DiscGolfBuiltInEnvironmentProviderTests.cpp")
    session10_tests = source("Source/DiscGolfTour/Tests/DiscGolfSession10EnvironmentContractTests.cpp")
    observed_runtime_tests = _automation_test_names(
        quality_tests + "\n" + provider_tests + "\n" + session10_tests
    )
    _check(
        "runtime_integration_exact_test_namespaces",
        observed_runtime_tests == set(integration["required_runtime_test_names"]),
        f"expected={integration['required_runtime_test_names']!r} observed={sorted(observed_runtime_tests)!r}",
        checks, errors,
    )

    compact_session10_tests = re.sub(r"\s+", " ", session10_tests)
    focused_test_required = [
        '"DiscGolfTour.Session10.Environment.ContractFailClosed"',
        'TestFalse(TEXT("Environment-to-flight wind synchronization is opt-in and dormant by default"), DefaultController->bSynchronizeDiscFlightWind);',
        'TestFalse(TEXT("An empty configuration cannot be production-ready"), ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 0, true));',
        'TestFalse(TEXT("Three of sixteen bindings cannot be production-ready"), ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 3, true));',
        'TestFalse(TEXT("Complete bindings without an authoring graph cannot be production-ready"), ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 16, false));',
        'TestTrue(TEXT("Authoring readiness requires exactly sixteen populated bindings and a graph"), ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 16, true));',
        'TestNull(TEXT("Runtime controller exposes no serialized PCG component"), DefaultController->GetClass()->FindPropertyByName(TEXT("PCGComponent")));',
    ]
    focused_test_ok = all(
        token in compact_session10_tests for token in focused_test_required
    )
    _check(
        "session10_focused_contract_test_exact_gameplay_neutrality_semantics",
        focused_test_ok,
        "exact focused test asserts default wind sync disabled, rejects incomplete authoring bindings, and proves the runtime controller has no PCG component",
        checks, errors,
    )

    binder_h = source("Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.h")
    binder_cpp = source("Source/DiscGolfTourEditor/DiscGolfEnvironmentAssetBinder.cpp")
    binder_tests = source("Source/DiscGolfTourEditor/Tests/DiscGolfEnvironmentAssetBinderTests.cpp")
    binder_contract = integration["binder_contract"]
    proposal_start = binder_cpp.find("UDiscGolfEnvironmentAssetBinder::ProposeBindingsToReport(")
    proposal_end = binder_cpp.find("UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssetReadiness(", proposal_start)
    proposal_body = binder_cpp[proposal_start:proposal_end] if proposal_start >= 0 and proposal_end > proposal_start else ""
    apply_start = binder_cpp.find("UDiscGolfEnvironmentAssetBinder::ApplyApprovedBindings(")
    apply_end = binder_cpp.find("UDiscGolfEnvironmentAssetBinder::WriteReport(", apply_start)
    apply_body = binder_cpp[apply_start:apply_end] if apply_start >= 0 and apply_end > apply_start else ""
    binder_required = [
        "bool bProvenanceAccepted = false;",
        "FString PolicyError;",
        "ValidateApprovedRuntimeAssetPath(",
        "ValidateApprovedScanRoots(",
    ]
    binder_cpp_required = [
        'TEXT("/Game/Presentation/Course/PineRidge")',
        'TEXT("/Engine/BasicShapes")',
        'TEXT("/Game/PN_interactiveSpruceForest")',
        'TEXT("/Game/Stump_Scanned")',
        'TEXT("/Game/WaterMaterials")',
        "bool IsQuarantinedPath(const FString& Path)",
        "ValidateVariantAssetPaths(",
        "EDiscGolfEnvironmentBindingStatus::BlockedByProvenance",
    ]
    binder_tests_observed = _automation_test_names(binder_tests)
    structural_start = binder_cpp.find("bool IsStructurallyComplete(")
    structural_end = binder_cpp.find("bool IsProductionReady(", structural_start)
    structural_body = (
        binder_cpp[structural_start:structural_end]
        if structural_start >= 0 and structural_end > structural_start else ""
    )
    binder_ok = all(token in binder_h for token in binder_required) and all(
        token in binder_cpp for token in binder_cpp_required
    ) and all([
        "if (!ValidateApprovedScanRoots(VendorContentRoots, Scan.PolicyError))" in proposal_body,
        "Scan.bProvenanceAccepted = true;" in proposal_body,
        proposal_body.find("ValidateApprovedScanRoots(") < proposal_body.find("Registry.GetAssetsByPath("),
        "if (!ValidateApprovedRuntimeAssetPath(" in proposal_body,
        "if (!ValidateApprovedRuntimeAssetPath(Path, OutError))" in apply_body,
        apply_body.find("ValidateApprovedRuntimeAssetPath(Path, OutError)") < apply_body.find("AssetSet->Modify();"),
        "if (Scan.Proposals.Num() != 16) return false;" in structural_body,
        "if (CategoryIndex >= 16" in structural_body,
        "|| Categories.Contains(Proposal.Category))" in structural_body,
        "for (int32 CategoryIndex = 0; CategoryIndex < 16; ++CategoryIndex)" in structural_body,
        "if (!Categories.Contains(" in structural_body,
        set(binder_contract["required_test_names"]).issubset(binder_tests_observed),
    ])
    _check(
        "editor_binder_provenance_and_atomic_apply_semantics",
        binder_ok,
        "project/Engine allowlist, three-root quarantine, pre-registry scan gate, per-asset gate, assigned-variant gate, pre-mutation apply gate, exact unique category set 0..15, focused tests",
        checks, errors,
    )

    workflow_path = ROOT / "Scripts/run-environment-asset-binding-workflow.py"
    workflow_source = source("Scripts/run-environment-asset-binding-workflow.py")
    workflow_roots: Any = None
    try:
        workflow_tree = ast.parse(workflow_source, filename=str(workflow_path))
        for node in workflow_tree.body:
            if (isinstance(node, ast.Assign) and len(node.targets) == 1
                    and isinstance(node.targets[0], ast.Name)
                    and node.targets[0].id == "APPROVED_RUNTIME_MESH_ROOTS"):
                workflow_roots = ast.literal_eval(node.value)
                break
    except (SyntaxError, ValueError) as exc:
        workflow_roots = f"INVALID:{exc}"
    workflow_contract = integration["workflow_contract"]
    workflow_ok = all([
        workflow_roots == workflow_contract["exact_approved_runtime_roots"],
        "if not scan.provenance_accepted:" in workflow_source,
        "if not proposal.provenance_accepted:" in workflow_source,
        "apply_approved_bindings" not in workflow_source.casefold(),
        "os.environ" not in workflow_source,
        "os.getenv" not in workflow_source,
        not any(root in workflow_source for root in binder_contract["quarantined_roots"]),
    ])
    _check(
        "nonmutating_workflow_exact_roots_and_provenance_gates",
        workflow_ok,
        f"approved_roots={workflow_roots!r}; scan/proposal fail closed; no apply/override/quarantine roots",
        checks, errors,
    )

    s9_contract = contract["session9_continuity"]
    s9_contract_path = ROOT / s9_contract["contract_path"]
    s9_validator_path = ROOT / s9_contract["validator_path"]
    s9_data = _load_evidence(s9_contract_path, "Session 9 contract", errors)
    s9_hash = sha256(s9_contract_path) if s9_contract_path.is_file() else None
    s9_validator_hash = sha256(s9_validator_path) if s9_validator_path.is_file() else None
    s9_ok = isinstance(s9_data, dict) and all([
        s9_hash == s9_contract["contract_sha256"],
        s9_validator_hash == s9_contract["validator_sha256"],
        s9_data.get("normal_status") == S9_STATUS,
        s9_data.get("release_blockers") == S9_BLOCKERS,
        len(s9_data.get("release_blockers", [])) == 8,
    ])
    _check(
        "session9_frozen_eight_blocker_continuity",
        s9_ok,
        f"contract_sha256={s9_hash} validator_sha256={s9_validator_hash} blockers={len(s9_data.get('release_blockers', [])) if isinstance(s9_data, dict) else None}",
        checks, errors,
    )

    stage_contract = contract["staged_closure"]
    stage_presentation_path = ROOT / stage_contract["observed_presentation_path"]
    ufs_path = ROOT / stage_contract["observed_ufs_manifest"]
    nonufs_path = ROOT / stage_contract["observed_nonufs_manifest"]
    uat_log_path = ROOT / stage_contract["uat_log_path"]
    stage_root = ROOT / stage_contract["observed_stage_root"]
    stage_presentation = _load_evidence(stage_presentation_path, "staged presentation", errors)
    stage_hash = sha256(stage_presentation_path) if stage_presentation_path.is_file() else None
    ufs_text = ufs_path.read_text(encoding="utf-8", errors="replace") if ufs_path.is_file() else ""
    nonufs_text = nonufs_path.read_text(encoding="utf-8", errors="replace") if nonufs_path.is_file() else ""
    uat_log_text = uat_log_path.read_text(encoding="utf-8", errors="replace") if uat_log_path.is_file() else ""
    ufs_entries = [
        _normalize_staged_manifest_path(line)
        for line in ufs_text.splitlines() if line.strip()
    ]
    nonufs_entries = [
        _normalize_staged_manifest_path(line)
        for line in nonufs_text.splitlines() if line.strip()
    ]
    env_packages = sorted(
        package for entry in ufs_entries
        if (package := _staged_environment_package(entry)) is not None
    )
    expected_env_packages = sorted(package.casefold() for package in EXPECTED_STAGED_ENVIRONMENT_PACKAGES)
    stage_blob = json.dumps(stage_presentation, sort_keys=True) if stage_presentation is not None else ""
    legacy_present = any(token in stage_blob for token in LEGACY_REFERENCE_IDS)
    staged_forest_ids = [
        hole.get("forestReferenceId")
        for hole in stage_presentation.get("holes", [])
    ] if isinstance(stage_presentation, dict) else []
    quarantine_stage_hits = [
        token for token in quarantine_tokens
        if any(
            _staged_entry_is_within_package(entry, token)
            for entry in ufs_entries + nonufs_entries
        )
    ]
    ufs_size = ufs_path.stat().st_size if ufs_path.is_file() else None
    nonufs_size = nonufs_path.stat().st_size if nonufs_path.is_file() else None
    uat_log_size = uat_log_path.stat().st_size if uat_log_path.is_file() else None
    ufs_hash = sha256(ufs_path) if ufs_path.is_file() else None
    nonufs_hash = sha256(nonufs_path) if nonufs_path.is_file() else None
    uat_log_hash = sha256(uat_log_path) if uat_log_path.is_file() else None
    container_observations: list[dict[str, Any]] = []
    containers_ok = True
    for item in stage_contract["container_files"]:
        container_path = stage_root / item["path"]
        exists = container_path.is_file()
        size = container_path.stat().st_size if exists else None
        digest = sha256(container_path) if exists else None
        passed = exists and size == item["bytes"] and digest == item["sha256"]
        containers_ok = containers_ok and passed
        container_observations.append({
            "path": item["path"], "exists": exists, "bytes": size, "sha256": digest,
        })
    _check(
        "staged_archive_container_hashes",
        containers_ok,
        f"closure_id={stage_contract['closure_id']} containers={container_observations!r}",
        checks, errors,
    )
    log_markers_ok = all(marker in uat_log_text for marker in [
        "FULL COOK:",
        "IsIterativeCook=false",
        "Packages Cooked: 984, Packages Incrementally Skipped: 0, Packages Skipped by Platform: 7, Total Packages: 991",
        "Success - 0 error(s), 0 warning(s)",
        "BUILD SUCCESSFUL",
        "AutomationTool exiting with ExitCode=0 (Success)",
    ])
    staged_presentation_ok = isinstance(stage_presentation, dict) and all([
        stage_presentation.get("schema") == presentation_contract["schema"],
        stage_presentation.get("schemaVersion") == presentation_contract["schema_version"],
        stage_presentation.get("courseId") == presentation_contract["course_id"],
        stage_presentation.get("layoutId") == presentation_contract["layout_id"],
        stage_presentation.get("collisionProfileId") == presentation_contract["collision_profile_id"],
        stage_presentation.get("collisionInvariantAcrossQuality") is True,
        stage_presentation.get("assetsReady") is False,
        staged_forest_ids == presentation_contract["required_forest_reference_ids"],
        all(tier.get("affectsCollision") is False for tier in stage_presentation.get("qualityTiers", [])),
        all(tier.get("collisionProfileId") == presentation_contract["collision_profile_id"] for tier in stage_presentation.get("qualityTiers", [])),
    ])
    stage_ok = all([
        stage_contract["build_configuration"] == "Development",
        stage_root.is_dir(),
        ufs_path.is_file(),
        nonufs_path.is_file(),
        uat_log_path.is_file(),
        ufs_size == stage_contract["observed_ufs_manifest_bytes"],
        nonufs_size == stage_contract["observed_nonufs_manifest_bytes"],
        uat_log_size == stage_contract["uat_log_bytes"],
        ufs_hash == stage_contract["observed_ufs_manifest_sha256"],
        nonufs_hash == stage_contract["observed_nonufs_manifest_sha256"],
        uat_log_hash == stage_contract["uat_log_sha256"],
        stage_presentation_path.stat().st_size == stage_contract["observed_presentation_bytes"] if stage_presentation_path.is_file() else False,
        stage_hash == stage_contract["observed_staged_presentation_sha256"],
        stage_hash == stage_contract["source_presentation_sha256"] == presentation_hash,
        env_packages == expected_env_packages,
        stage_contract["observed_environment_packages"] == EXPECTED_STAGED_ENVIRONMENT_PACKAGES,
        len(env_packages) == stage_contract["observed_staged_environment_asset_count"] == stage_contract["expected_fresh_environment_asset_count"] == 4,
        staged_presentation_ok,
        legacy_present is False,
        not quarantine_stage_hits,
        stage_contract["observed_quarantined_package_roots"] == [],
        log_markers_ok,
        stage_contract["cook_packages"] == 984,
        stage_contract["cook_incrementally_skipped"] == 0,
        stage_contract["cook_skipped_by_platform"] == 7,
        stage_contract["cook_total_packages"] == 991,
        containers_ok,
        stage_contract["fresh_package_closure_accepted"] is True,
        stage_contract["release_use_allowed"] is False,
    ])
    _check(
        "fresh_development_package_closure_exact_and_quarantine_clean",
        stage_ok,
        f"closure_id={stage_contract['closure_id']} staged_sha256={stage_hash} source_sha256={presentation_hash} environment_packages={env_packages!r} legacy_ids={legacy_present} quarantine_hits={quarantine_stage_hits!r} full_cook={log_markers_ok}",
        checks, errors,
    )
    return checks, errors


def validate_repository(
    root: Path = ROOT,
) -> tuple[list[str], list[dict[str, Any]], list[str]]:
    """Return (errors, checks, blockers) without writing reports or changing assets."""
    global ROOT
    previous_root = ROOT
    try:
        ROOT = Path(root).resolve()
        try:
            loaded = load_json(ROOT / CONTRACT_PATH)
        except StrictJsonError as exc:
            return [str(exc)], [], []
        if not isinstance(loaded, dict):
            return ["contract: root must be an object"], [], []
        shape_errors = validate_contract_shape(loaded)
        blockers = loaded.get("release_blockers", [])
        if shape_errors:
            return shape_errors, [], blockers if isinstance(blockers, list) else []
        checks, project_errors = validate_project(loaded)
        return project_errors, checks, blockers
    finally:
        ROOT = previous_root


def run_self_test() -> tuple[int, dict[str, Any]]:
    contract = load_json(ROOT / CONTRACT_PATH)
    cases: list[tuple[str, str | None]] = []

    def mutate(name: str, change: Any) -> None:
        candidate = copy.deepcopy(contract)
        change(candidate)
        found = validate_contract_shape(candidate)
        cases.append((name, None if found else "mutation was accepted"))

    cases.append(("baseline", None if not validate_contract_shape(contract) else "valid contract rejected"))
    mutate("unknown_top_level", lambda c: c.update({"unexpected": True}))
    mutate("normal_status_promotion", lambda c: c.update({"normal_status": "PASS_RELEASE_READY"}))
    mutate("schema_version_bool_coercion", lambda c: c.update({"schema_version": True}))
    mutate("asset_removed", lambda c: c["source_environment_inventory"]["exact_assets"].pop())
    mutate("asset_hash_changed", lambda c: c["source_environment_inventory"]["exact_assets"][0].update({"sha256": "0" * 64}))
    mutate("asset_release_enabled", lambda c: c["source_environment_inventory"]["exact_assets"][0].update({"release_use_allowed": True}))
    mutate("presentation_assets_ready", lambda c: c["presentation_contract"].update({"assets_ready_expected": True}))
    mutate("pcg_runtime_caller_added", lambda c: c["runtime_pcg_contract"].update({"expected_runtime_generate_forest_callsite_count": 1}))
    mutate("production_readiness_promoted", lambda c: c["assigned_readiness_contract"].update({"production_ready": True}))
    mutate("integration_required_file_removed", lambda c: c["runtime_integration_contract"]["required_files"].pop())
    mutate("integration_frozen_hash_changed", lambda c: c["runtime_integration_contract"]["frozen_file_hashes"][0].update({"sha256": "0" * 64}))
    mutate("medium_environment_mapping_promoted", lambda c: c["runtime_integration_contract"]["quality_adapter"]["environment_quality_order"].__setitem__(1, "High"))
    mutate("omen_environment_promoted", lambda c: c["runtime_integration_contract"]["quality_adapter"].update({"omen_environment_quality": "Cinematic"}))
    mutate("provider_gameplay_neutrality_removed", lambda c: c["runtime_integration_contract"]["built_in_provider"].update({"gameplay_neutral": False}))
    mutate("bootstrap_fail_closed_removed", lambda c: c["runtime_integration_contract"]["bootstrap_ownership"].update({"spawn_failure_blocks_course_build": False}))
    mutate("controller_default_wind_sync_enabled", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"default_disc_flight_wind_synchronization_enabled": True}))
    mutate("bootstrap_wind_sync_enabled", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"bootstrapped_disc_flight_wind_synchronization_enabled": True}))
    mutate("game_mode_auto_wind_sync_enabled", lambda c: c["runtime_integration_contract"]["game_mode_integration"].update({"automatically_invokes_environment_wind_synchronization": True}))
    mutate("controller_binding_count_weakened", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"production_required_binding_slot_count": 15}))
    mutate("controller_graph_requirement_removed", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"editor_authoring_readiness_requires_graph": False}))
    mutate("shipping_runtime_graph_enabled", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"shipping_runtime_readiness_uses_graph": True}))
    mutate("runtime_pcg_dependency_allowed", lambda c: c["runtime_pcg_contract"].update({"runtime_pcg_module_dependency_allowed": True}))
    mutate("plugin_game_target_allowed", lambda c: c["runtime_pcg_contract"].update({"plugin_target_allow_list": ["Editor", "Game"]}))
    mutate("focused_test_semantics_removed", lambda c: c["runtime_integration_contract"]["controller_gameplay_neutrality"].update({"focused_test_asserts_incomplete_bindings_rejected": False}))
    mutate("runtime_test_removed", lambda c: c["runtime_integration_contract"]["required_runtime_test_names"].pop())
    mutate("binder_structural_range_changed", lambda c: c["runtime_integration_contract"]["binder_contract"].update({"structural_category_range_inclusive": [0, 16]}))
    mutate("binder_test_removed", lambda c: c["runtime_integration_contract"]["binder_contract"]["required_test_names"].pop())
    mutate("workflow_apply_enabled", lambda c: c["runtime_integration_contract"]["workflow_contract"].update({"invokes_apply_approved_bindings": True}))
    mutate("session9_blocker_removed", lambda c: c["session9_continuity"]["release_blockers"].pop())
    mutate("session9_blockers_reordered", lambda c: c["session9_continuity"]["release_blockers"].reverse())
    mutate("fresh_stage_acceptance_removed", lambda c: c["staged_closure"].update({"fresh_package_closure_accepted": False}))
    mutate("fresh_stage_uuid_changed", lambda c: c["staged_closure"].update({"closure_id": "00000000-0000-0000-0000-000000000000"}))
    mutate("staged_presentation_hash_changed", lambda c: c["staged_closure"].update({"observed_staged_presentation_sha256": "0" * 64}))
    mutate("staged_environment_package_removed", lambda c: c["staged_closure"]["observed_environment_packages"].pop())
    mutate("staged_container_hash_changed", lambda c: c["staged_closure"]["container_files"][2].update({"sha256": "0" * 64}))
    mutate("development_closure_promoted_for_release", lambda c: c["staged_closure"].update({"release_use_allowed": True}))
    mutate("optional_adapter_acquired", lambda c: c["optional_adapters"][0].update({"status": "ACQUIRED"}))
    mutate("session10_blocker_removed", lambda c: c["session10_release_blockers"].pop())
    mutate("release_blocker_union_changed", lambda c: c["release_blockers"].pop())

    duplicate_text = '{"schema":1,"schema":2}'
    try:
        json.loads(duplicate_text, object_pairs_hook=_strict_pairs, parse_constant=_reject_constant)
        cases.append(("duplicate_json_key", "duplicate key was accepted"))
    except StrictJsonError:
        cases.append(("duplicate_json_key", None))
    try:
        json.loads('{"value":NaN}', object_pairs_hook=_strict_pairs, parse_constant=_reject_constant)
        cases.append(("non_finite_json_number", "NaN was accepted"))
    except StrictJsonError:
        cases.append(("non_finite_json_number", None))
    try:
        value = json.loads('{"value":1e309}', object_pairs_hook=_strict_pairs, parse_constant=_reject_constant)
        _validate_json_numbers(value)
        cases.append(("overflow_float", "overflowing float was accepted"))
    except StrictJsonError:
        cases.append(("overflow_float", None))
    try:
        value = json.loads('{"value":9223372036854775808}', object_pairs_hook=_strict_pairs, parse_constant=_reject_constant)
        _validate_json_numbers(value)
        cases.append(("overflow_integer", "overflowing integer was accepted"))
    except StrictJsonError:
        cases.append(("overflow_integer", None))

    traversal_entry = "..\\..\\..\\DiscGolfTour\\Content\\PN_interactiveSpruceForest\\Meshes\\Tree.uasset"
    cases.append((
        "staged_quarantine_windows_traversal_normalized",
        None if _staged_entry_is_within_package(
            traversal_entry, "/Game/PN_interactiveSpruceForest"
        ) else "normalized staged quarantine path was missed",
    ))
    near_match_entry = "DiscGolfTour/Content/PN_interactiveSpruceForest_Approved/Meshes/Tree.uasset"
    cases.append((
        "staged_quarantine_directory_boundary_preserved",
        "near-match staged path was falsely quarantined"
        if _staged_entry_is_within_package(
            near_match_entry, "/Game/PN_interactiveSpruceForest"
        ) else None,
    ))

    failures = [{"case": name, "error": error} for name, error in cases if error]
    result = {
        "schema": "DiscGolfTour.Session10EnvironmentValidatorSelfTest.v1",
        "passed": not failures,
        "case_count": len(cases),
        "failures": failures,
    }
    return (0 if not failures else 1), result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--report", type=Path, default=REPORT_PATH)
    parser.add_argument("--no-report", action="store_true", help="do not write an audit report")
    parser.add_argument("--require-release-ready", action="store_true", help="exit 2 unless every declared release blocker is closed")
    parser.add_argument("--self-test", action="store_true", help="run strict, nonwriting adversarial contract tests")
    args = parser.parse_args()

    if args.self_test:
        try:
            code, result = run_self_test()
        except StrictJsonError as exc:
            result = {"passed": False, "case_count": 0, "failures": [{"case": "load", "error": str(exc)}]}
            code = 1
        print(json.dumps(result, indent=2, sort_keys=True))
        return code

    contract_path = args.contract if args.contract.is_absolute() else ROOT / args.contract
    try:
        contract = load_json(contract_path)
    except StrictJsonError as exc:
        print(f"SESSION 10 ENVIRONMENT FAIL: {exc}", file=sys.stderr)
        return 1

    shape_errors = validate_contract_shape(contract)
    checks: list[dict[str, Any]] = []
    project_errors: list[str] = []
    if not shape_errors:
        checks, project_errors = validate_project(contract)
    errors = shape_errors + project_errors
    blockers = contract.get("release_blockers", []) if isinstance(contract, dict) else []
    contract_valid = not errors
    release_ready = contract_valid and not blockers
    status = NORMAL_STATUS if contract_valid else "FAIL_TECHNICAL_ENVIRONMENT_CONTRACT"
    result = {
        "schema": "DiscGolfTour.Session10EnvironmentAudit.v1",
        "contract": str(contract_path.relative_to(ROOT)).replace("\\", "/") if contract_path.is_relative_to(ROOT) else str(contract_path),
        "contract_sha256": sha256(contract_path),
        "contract_valid": contract_valid,
        "release_ready": release_ready,
        "status": status,
        "release_status": RELEASE_STATUS if not release_ready else "RELEASE_READY",
        "release_blocker_count": len(blockers),
        "release_blockers": blockers,
        "checks": checks,
        "errors": errors,
    }

    report_display = "DISABLED"
    if not args.no_report:
        report_path = args.report if args.report.is_absolute() else ROOT / args.report
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        report_display = str(report_path)

    if not contract_valid:
        print(f"SESSION 10 ENVIRONMENT FAIL: errors={len(errors)} report={report_display}", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"SESSION 10 ENVIRONMENT {NORMAL_STATUS}: contract_valid=true "
        f"release_ready=false blockers={len(blockers)} errors=0 report={report_display}"
    )
    if args.require_release_ready and not release_ready:
        print(f"SESSION 10 RELEASE GATE BLOCKED: status={RELEASE_STATUS} blockers={len(blockers)}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
