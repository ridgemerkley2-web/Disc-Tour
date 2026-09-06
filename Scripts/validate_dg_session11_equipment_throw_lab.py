#!/usr/bin/env python3
"""Fail-closed Session 11 equipment and Throw Lab source-contract validator.

The validator is deliberately non-writing unless report output is requested. It
does not launch Unreal, mutate a player profile, cook, stage, or claim release
readiness. Session 11 adapts project-owned authorities; plugin equipment DTOs
may be reused, but plugin bag, telemetry, and replay components must stay
unwired.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session11EquipmentThrowLabContract.json")
REPORT_PATH = Path("Saved/EquipmentReports/DG_Session11EquipmentThrowLabAudit.json")

NORMAL_STATUS = "PASS_TECHNICAL_EQUIPMENT_THROW_LAB_CONTRACT_RELEASE_BLOCKED"
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_SESSION10_AND_SESSION11_PRODUCTION_CLOSURE"

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
S10_BLOCKERS = ["SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY"]
S11_BLOCKERS = ["SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING"]
EXPECTED_BLOCKERS = S9_BLOCKERS + S10_BLOCKERS + S11_BLOCKERS

FROZEN_CONTINUITY = [
    {
        "path": "Config/DG_BrandLicenseContract.json",
        "bytes": 9267,
        "sha256": "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001",
    },
    {
        "path": "Scripts/validate_dg_session9_brand_license.py",
        "bytes": 74292,
        "sha256": "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550",
    },
    {
        "path": "Config/DG_Session10EnvironmentContract.json",
        "bytes": 19976,
        "sha256": "05931329CE877E80D4D36691D9091BBD6DDACE8F3F40612C5BE11B47744CDDC5",
    },
    {
        "path": "Scripts/validate_dg_session10_environment.py",
        "bytes": 76608,
        "sha256": "13F3AECBB569466B6E3E8C49E0B3FA599EF57B13CE23604602CDC3158809112A",
    },
]

EXPECTED_OWNERS = {
    "catalog": "UDiscCatalogSubsystem",
    "bag": "UDiscBagComponent",
    "release": "DiscGolfMath::ResolveThrowRelease",
    "launch": "ADiscGolfTourGameMode::LaunchThrow",
    "gameplay_disc": "ADiscActor",
    "flight": "UDiscFlightComponent",
    "trajectory_and_regression": "UDiscTrajectorySubsystem",
    "replay": "ADiscReplayActor",
    "save": "UDiscGolfTourGameInstance/UDiscGolfSaveGame",
}

EXPECTED_MOLDS = ["Apex", "Vector", "Line", "Compass", "Touch"]
EXPECTED_PLASTICS = ["Base", "Tour", "Crystal"]

EXPECTED_DONOR_BINDINGS = {
    "session_spec": {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/11_DISC_EQUIPMENT_THROW_LAB.md",
        "bytes": 2266,
        "sha256": "7B990AC275A9B8F531BCB567B568D1DA3CCDFE71C073350CA80833C285C17FEB",
    },
    "equipment_schema": {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_DiscEquipmentSchema.json",
        "bytes": 760,
        "sha256": "F7667FFDF676048EE157E9FDFCC801E1E548AA2B8A45DDE475DF81675058CF46",
    },
    "telemetry_schema": {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_ThrowLabTelemetrySchema.json",
        "bytes": 667,
        "sha256": "421EA0810E30D90FBA5C9A12D38FBD8CA31F705FAA596142053B35972F45F410",
    },
}

EXPECTED_CATALOG_ASSETS = [
    {
        "path": "Content/Data/Discs/Molds/DA_Mold_Apex.uasset",
        "bytes": 2009,
        "sha256": "C280DFAB44AA0EDDBC996F2C321DEE541D0B23C9E4879706F14346B05718AEAA",
    },
    {
        "path": "Content/Data/Discs/Molds/DA_Mold_Compass.uasset",
        "bytes": 2073,
        "sha256": "8499F1D6E2B1B440E7E994E61CB87EA9EF52583EB451D06AF24281BC9FC77E8C",
    },
    {
        "path": "Content/Data/Discs/Molds/DA_Mold_Line.uasset",
        "bytes": 1904,
        "sha256": "50FE515D59783D797FAA5488548F3F5F4630256C00CDF9254BD247BADDF23E39",
    },
    {
        "path": "Content/Data/Discs/Molds/DA_Mold_Touch.uasset",
        "bytes": 2102,
        "sha256": "AFCFB8A80C3D5BA6C37B8C280E4C3593D04D44D5CF41FFE88C10C189ED1BF9D6",
    },
    {
        "path": "Content/Data/Discs/Molds/DA_Mold_Vector.uasset",
        "bytes": 2082,
        "sha256": "7EAA0C21D998B98E5753187E722BB22E42E124B2C2F9DE9746B3F6D74780086B",
    },
    {
        "path": "Content/Data/Discs/Plastics/DA_Plastic_Base.uasset",
        "bytes": 1897,
        "sha256": "E4BD3EE047C002CA53231312E5AA3E850317702F42BAA810A22D89B883A5285D",
    },
    {
        "path": "Content/Data/Discs/Plastics/DA_Plastic_Crystal.uasset",
        "bytes": 1918,
        "sha256": "C19030DA0EEE0AA73DDD18BC37278D19E007CFA2C8A08095D8C7F5B3FEFC2FD3",
    },
    {
        "path": "Content/Data/Discs/Plastics/DA_Plastic_Tour.uasset",
        "bytes": 1460,
        "sha256": "70012C7B71125B0BC52D66A6BD590078925E04E72503CFCEF692CB9139FE7827",
    },
]

IMPLEMENTATION_FILES = [
    "Source/DiscGolfTour/DiscEquipmentSaveGame.h",
    "Source/DiscGolfTour/DiscThrowLabTypes.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.h",
    "Source/DiscGolfTour/DiscThrowLabSubsystem.cpp",
    "Source/DiscGolfTour/DiscThrowLabSaveGame.h",
    "Source/DiscGolfTour/Tests/DiscEquipmentSession11Tests.cpp",
    "Source/DiscGolfTour/Tests/DiscThrowLabSession11Tests.cpp",
]

AUTHORITY_TOKEN_GROUPS: dict[str, list[tuple[str, ...]]] = {
    "Source/DiscGolfTour/DiscCatalogSubsystem.cpp": [
        ("UDiscCatalogSubsystem::ResolveDisc",),
        ("GetPrimaryAssetIdList",),
        ("DiscMold",),
        ("DiscPlastic",),
    ],
    "Source/DiscGolfTour/DiscBagComponent.h": [
        ("class DISCGOLFTOUR_API UDiscBagComponent",),
        ("GetSelectedMoldId",),
        ("GetSelectedPlastic",),
    ],
    "Source/DiscGolfTour/DiscFlightComponent.cpp": [
        ("FixedStepSeconds",),
        ("SimulateFixedStep",),
        ("Accumulator",),
    ],
    "Source/DiscGolfTour/DiscTrajectorySubsystem.h": [
        ("UDiscTrajectorySubsystem",),
        ("CompleteCapture",),
        ("BuildJson",),
    ],
    "Source/DiscGolfTour/DiscReplayActor.cpp": [
        ("ADiscReplayActor::InitializeReplay",),
        ("EvaluateReplayFrame",),
    ],
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp": [
        ("ADiscGolfTourGameMode::LaunchThrow",),
        ("Catalog->ResolveDisc",),
        ("ADiscGolfTourGameMode::CompleteDiscCapture",),
        ("Trajectories->CompleteCapture",),
    ],
}

IMPLEMENTATION_TOKEN_GROUPS: dict[str, list[tuple[str, ...]]] = {
    "Source/DiscGolfTour/DiscGolfTypes.h": [
        ("DiscInstanceId",),
        ("DiscMassGrams", "MassGrams"),
        ("DiscWear01", "Wear01"),
        ("bWearAffectsPhysics",),
        ("FDiscTrajectorySample",),
        ("WorldLocationCm",),
        ("VelocityMps",),
        ("SpinRpm",),
    ],
    "Source/DiscGolfTour/DiscBagComponent.h": [
        ("FDGDiscInstance",),
        ("FDGDiscBagLoadout",),
        ("ValidateDiscInstance",),
        ("ValidateLoadout",),
        ("BuildDefaultLoadout",),
        ("ResolveSelectedDiscInstance",),
        ("SaveEquipmentToSlot",),
        ("LoadEquipmentFromSlot",),
    ],
    "Source/DiscGolfTour/DiscBagComponent.cpp": [
        ("UDiscBagComponent::ValidateDiscInstance",),
        ("UDiscBagComponent::ValidateLoadout",),
        ("UDiscBagComponent::BuildDefaultLoadout",),
        ("UDiscBagComponent::ResolveDiscInstance",),
        ("175.0",),
        ("bWearAffectsPhysics",),
        ("UDiscBagComponent::ApplyEquipmentLoadout",),
        ("UDiscBagComponent::SaveEquipmentToSlot",),
        ("UDiscBagComponent::LoadEquipmentFromSlot",),
    ],
    "Source/DiscGolfTour/DiscEquipmentSaveGame.h": [
        ("UDiscEquipmentSaveGame",),
        ("CurrentSchemaVersion = 1",),
        ("FDGDiscBagLoadout",),
        ("SaveGame",),
    ],
    "Source/DiscGolfTour/DiscGolferPawn.cpp": [
        ("GetSelectedDiscInstance", "GetSelectedDiscInstanceId"),
        ("DiscInstanceId",),
    ],
    "Source/DiscGolfTour/DiscGolfTourGameMode.h": [
        ("DGT_ToggleThrowLab",),
        ("DGT_ThrowLabPrevious",),
        ("DGT_ThrowLabNext",),
        ("DGT_ThrowLabPin",),
        ("DGT_ThrowLabDelete",),
        ("DGT_ThrowLabCompareLatest",),
        ("DGT_ThrowLabReplay",),
        ("DGT_ThrowLabSave",),
        ("DGT_ThrowLabLoad",),
        ("DGT_ToggleDiscFavorite",),
        ("GetThrowLabRecordCount",),
        ("GetThrowLabComparisonText",),
    ],
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp": [
        ("ResolveSelectedDiscInstance", "UDiscBagComponent::ResolveDiscInstance"),
        ("DiscInstanceId",),
        ("ThrowLab", "DiscThrowLab"),
        ("CompleteDiscCapture",),
    ],
    "Source/DiscGolfTour/DiscThrowLabTypes.h": [
        ("FDiscThrowLab", "FDGThrowLab"),
        ("FDiscTrajectorySample",),
        ("DiscInstance", "FResolvedDiscDefinition"),
        ("Release",),
        ("Summary",),
        ("bPinned", "Pinned"),
        ("centimeter",),
        ("meter_per_second",),
        ("revolution_per_minute",),
    ],
    "Source/DiscGolfTour/DiscThrowLabSubsystem.h": [
        ("UDiscThrowLabSubsystem", "UDiscGolfThrowLabSubsystem"),
        ("Record", "Capture"),
        ("Pin",),
        ("Delete", "Remove"),
        ("Compare",),
        ("Replay", "Samples"),
    ],
    "Source/DiscGolfTour/DiscThrowLabSubsystem.cpp": [
        ("MaxSamplesPerRecord", "MaxReplaySamples", "2400"),
        ("MaxReplaySampleRateHz", "60.0", "60.f", "60.0f"),
        ("Pin",),
        ("Delete", "Remove"),
        ("Compare",),
        ("FDiscTrajectorySample",),
    ],
    "Source/DiscGolfTour/DiscThrowLabSaveGame.h": [
        ("USaveGame",),
        ("ThrowLab",),
        ("Record",),
    ],
    "Source/DiscGolfTour/DiscGolfHUD.cpp": [
        ("THROW LAB // DEVELOPMENT",),
        ("GetThrowLabStatusText",),
        ("GetThrowLabComparisonText",),
        ("DGT_ThrowLab",),
    ],
    "Source/DiscGolfTour/Tests/DiscEquipmentSession11Tests.cpp": [
        ("DiscGolfTour.Session11.",),
        ("ValidateLoadout",),
        ("Wear",),
        ("Mass",),
    ],
    "Source/DiscGolfTour/Tests/DiscThrowLabSession11Tests.cpp": [
        ("DiscGolfTour.Session11.",),
        ("Pin",),
        ("Delete", "Remove"),
        ("Compare", "Comparison"),
        ("Replay", "Samples"),
    ],
}

FORBIDDEN_PROJECT_WIRING = [
    "UDiscGolfBagComponent",
    "UDiscGolfTelemetryComponent",
    "UDiscGolfShotReplayComponent",
    '#include "DiscGolfBagComponent.h"',
    '#include "DiscGolfTelemetryComponent.h"',
    '#include "DiscGolfShotReplayComponent.h"',
]

FORBIDDEN_ACTIVE_BRAND_TOKENS = [
    "premium_disc_golf",
    "premium_default",
    "premium_bag_default",
    "premium_proto_p1",
    "premium_proto_m1",
    "premium_proto_f1",
    "premium_proto_d1",
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


def _at(value: Any, dotted_path: str) -> Any:
    current = value
    for part in dotted_path.split("."):
        if not isinstance(current, dict) or part not in current:
            return None
        current = current[part]
    return current


def _expect(errors: list[str], contract: dict[str, Any], path: str, expected: Any) -> None:
    actual = _at(contract, path)
    if actual != expected:
        errors.append(f"contract {path} must be {expected!r}; found {actual!r}")


def _validate_contract(contract: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    exact_values = {
        "schema": "DiscGolfTour.Session11EquipmentThrowLabContract.v1",
        "schema_version": 1,
        "contract_id": "session11_equipment_throw_lab_source_first_v1",
        "audit_scope": "TECHNICAL_DEVELOPMENT_EQUIPMENT_AND_THROW_LAB_GATE_NOT_PRODUCTION_OR_RELEASE_APPROVAL",
        "normal_status": NORMAL_STATUS,
        "release_status": RELEASE_STATUS,
        "authority": "PROJECT_OWNED_CATALOG_BAG_RELEASE_FLIGHT_TRAJECTORY_AND_REPLAY_REMAIN_AUTHORITATIVE",
        "implementation_status": "TECHNICAL_RUNTIME_IMPLEMENTATION_ACCEPTED_PRODUCTION_READINESS_PENDING",
        "feature_complete": True,
        "releaseBlocked": True,
        "release_ready": False,
        "release_use_allowed": False,
        "donor_reference.policy": "REFERENCE_ONLY_ADAPT_DO_NOT_COPY_OR_REPLACE_INSTALLED_PLUGIN_OR_PROJECT_AUTHORITIES",
        "donor_reference.session_spec": EXPECTED_DONOR_BINDINGS["session_spec"],
        "donor_reference.equipment_schema": EXPECTED_DONOR_BINDINGS["equipment_schema"],
        "donor_reference.telemetry_schema": EXPECTED_DONOR_BINDINGS["telemetry_schema"],
        "donor_reference.plugin_bag_component_policy": "DORMANT_NO_PARALLEL_BAG_AUTHORITY",
        "donor_reference.plugin_telemetry_component_policy": "DORMANT_NO_PARALLEL_FLIGHT_OR_RECORDING_AUTHORITY",
        "brand_and_identity_contract.active_brand_id": "dg_generic",
        "brand_and_identity_contract.premium_disc_golf_status": "DORMANT_BLOCKED_NOT_ADOPTED_FROM_DONOR",
        "brand_and_identity_contract.unknown_brand_policy": "BLOCK",
        "brand_and_identity_contract.real_manufacturer_or_product_data_allowed": False,
        "brand_and_identity_contract.generic_development_equipment_allowed": True,
        "brand_and_identity_contract.forbidden_active_runtime_tokens": [
            "premium_disc_golf", "premium_default", "premium_bag_default",
            "premium_proto_p1", "premium_proto_m1", "premium_proto_f1", "premium_proto_d1",
        ],
        "current_authority_inventory.owners": EXPECTED_OWNERS,
        "current_authority_inventory.parallel_authority_allowed": False,
        "generic_catalog_contract.stable_mold_ids": EXPECTED_MOLDS,
        "generic_catalog_contract.stable_plastic_ids": EXPECTED_PLASTICS,
        "generic_catalog_contract.exact_assets": EXPECTED_CATALOG_ASSETS,
        "generic_catalog_contract.expected_mold_count": 5,
        "generic_catalog_contract.expected_plastic_count": 3,
        "generic_catalog_contract.existing_aerodynamic_coefficients_remain_authoritative": True,
        "generic_catalog_contract.release_use_allowed": False,
        "equipment_instance_contract.default_bag_capacity": 24,
        "equipment_instance_contract.one_selected_disc": True,
        "equipment_instance_contract.selected_instance_must_exist_in_bag": True,
        "equipment_instance_contract.instance_ids_must_be_unique": True,
        "equipment_instance_contract.definition_and_plastic_ids_must_resolve": True,
        "equipment_instance_contract.invalid_or_missing_content_policy": "ATOMIC_CANONICAL_FALLBACK_NO_PARTIAL_SELECTION",
        "equipment_instance_contract.default_reference_mass_grams": 175.0,
        "equipment_instance_contract.default_reference_wear01": 0.0,
        "equipment_instance_contract.default_mapping_must_preserve_reference_physics": True,
        "equipment_instance_contract.non_default_mass_and_wear_status": "UNCALIBRATED_DEVELOPMENT_ONLY",
        "equipment_instance_contract.selected_instance_snapshot_point": "SUCCESSFUL_EXISTING_LAUNCH_TRANSACTION",
        "equipment_instance_contract.mutates_post_release_physics": False,
        "telemetry_contract.record_origin": "SUCCESSFUL_EXISTING_GAME_MODE_LAUNCH_NOT_ANIMATION_NOTIFY_ALONE",
        "telemetry_contract.finish_origins": [
            "EXISTING_SETTLED_AUTHORITY", "EXISTING_HOLED_OUT_AUTHORITY",
            "EXISTING_OUT_OF_BOUNDS_LIE_AUTHORITY",
        ],
        "telemetry_contract.impact_history_required": True,
        "telemetry_contract.source_trajectory_sampling_hz": 240.0,
        "telemetry_contract.stored_throw_lab_max_sampling_hz": 60.0,
        "telemetry_contract.max_samples_per_record": 2400,
        "telemetry_contract.downsample_policy": "DETERMINISTIC_RETAIN_FIRST_LAST_AND_DISCRETE_TRANSITIONS_OR_IMPACTS",
        "telemetry_contract.world_position_unit": "CENTIMETERS",
        "telemetry_contract.linear_velocity_unit": "METERS_PER_SECOND",
        "telemetry_contract.spin_unit": "RPM",
        "telemetry_contract.angular_velocity_unit": "RADIANS_PER_SECOND",
        "telemetry_contract.telemetry_changes_simulation": False,
        "telemetry_contract.exactly_one_record_per_completed_authoritative_throw": True,
        "telemetry_contract.cancelled_or_rejected_throw_record_count": 0,
        "development_ui_contract.shipping_ui_claimed": False,
        "development_ui_contract.fake_post_release_curve_allowed": False,
        "development_ui_contract.active_throw_or_replay_mutation_allowed": False,
        "persistence_contract.project_save_remains_authoritative": True,
        "persistence_contract.stable_ids_not_display_names": True,
        "persistence_contract.schema_migration_required_for_new_bag_payload": True,
        "persistence_contract.missing_content_fallback_required": True,
        "persistence_contract.atomic_save_failure_rollback_required": True,
        "persistence_contract.production_save_automation_mutation_allowed": False,
        "persistence_contract.protected_save_path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "persistence_contract.protected_save_bytes": 5212,
        "persistence_contract.protected_save_sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        "regression_contract.create_parallel_regression_authority": False,
        "regression_contract.uncalibrated_cases_enforced": False,
        "regression_contract.new_mass_or_wear_ranges_status": "UNENFORCED_UNTIL_APPROVED_REPEATED_AND_TOLERANCED",
        "regression_contract.reference_flight_must_remain_green": True,
        "regression_contract.default_equipment_reference_envelopes_must_remain_green": True,
        "exit_gate.status": "SATISFIED_TECHNICAL_RELEASE_BLOCKED",
        "exit_gate.production_equipment_claimed": False,
        "exit_gate.calibrated_weight_or_wear_claimed": False,
        "exit_gate.shipping_ui_claimed": False,
        "exit_gate.public_release_claimed": False,
        "validation_contract.validator_path": "Scripts/validate_dg_session11_equipment_throw_lab.py",
        "validation_contract.report_path": "Saved/EquipmentReports/DG_Session11EquipmentThrowLabAudit.json",
        "validation_contract.normal_mode_exit_code": 0,
        "validation_contract.release_required_exit_code": 2,
        "validation_contract.pending_implementation_artifact_count": 0,
        "validation_contract.pending_artifacts_are_technical_errors_during_scaffold_phase": False,
        "validation_contract.normal_mode_requires_release_blocked": True,
        "session11_release_blockers": S11_BLOCKERS,
        "release_blockers": EXPECTED_BLOCKERS,
    }
    for path, expected in exact_values.items():
        _expect(errors, contract, path, expected)

    _expect(errors, contract, "continuity.session9.release_blockers", S9_BLOCKERS)
    _expect(errors, contract, "continuity.session9.expected_release_blocker_count", 8)
    _expect(errors, contract, "continuity.session10.expected_release_blocker_count", 9)
    _expect(errors, contract, "continuity.session10.session10_release_blockers", S10_BLOCKERS)
    _expect(
        errors,
        contract,
        "continuity.session10.preserved_boundaries",
        {
            "environment_presentation_only": True,
            "automatic_environment_to_flight_wind_sync": False,
            "assets_ready": False,
            "pcg_runtime_generation": False,
            "quarantined_roots": [
                "/Game/PN_interactiveSpruceForest",
                "/Game/Stump_Scanned",
                "/Game/WaterMaterials",
            ],
        },
    )

    fields = _at(contract, "equipment_instance_contract.required_fields")
    if fields != [
        "InstanceId", "DiscDefinitionId", "PlasticId", "MassGrams", "Wear01",
        "Color", "StampId", "Nickname", "Favorite",
    ]:
        errors.append("contract equipment instance fields must remain exact and ordered")

    capabilities = _at(contract, "implementation_capabilities")
    if not isinstance(capabilities, list) or len(capabilities) != 7:
        errors.append("contract must retain exactly seven explicit Session 11 technical capabilities")
    elif any(
        not isinstance(entry, dict)
        or entry.get("status") != "TECHNICAL_PASS"
        or entry.get("required_for_session11_completion") is not True
        for entry in capabilities
    ):
        errors.append("all Session 11 capabilities must remain explicit, completion-required technical passes")

    exact_assets = _at(contract, "generic_catalog_contract.exact_assets")
    if not isinstance(exact_assets, list) or len(exact_assets) != 8:
        errors.append("contract generic catalog must bind exactly five mold and three plastic assets")
    return errors


def _check_file_binding(root: Path, binding: dict[str, Any], errors: list[str], label: str) -> None:
    relative_value = binding.get("path")
    expected_bytes = binding.get("bytes")
    expected_hash = binding.get("sha256")
    if (
        not isinstance(relative_value, str)
        or not relative_value
        or Path(relative_value).is_absolute()
        or ".." in Path(relative_value).parts
        or type(expected_bytes) is not int
        or expected_bytes < 0
        or not isinstance(expected_hash, str)
        or len(expected_hash) != 64
    ):
        errors.append(f"invalid {label} file binding: {binding!r}")
        return
    path = root / relative_value
    if not path.is_file():
        errors.append(f"missing {label}: {relative_value}")
        return
    actual_bytes = path.stat().st_size
    if actual_bytes != expected_bytes:
        errors.append(
            f"{label} byte mismatch for {relative_value}: expected {expected_bytes}, found {actual_bytes}"
        )
    actual_hash = sha256(path)
    if actual_hash != expected_hash:
        errors.append(
            f"{label} hash mismatch for {relative_value}: expected {expected_hash}, found {actual_hash}"
        )


def _read_text(root: Path, relative: str, errors: list[str]) -> str:
    path = root / relative
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"cannot read required source {relative}: {exc}")
        return ""


def _require_token_groups(
    root: Path,
    groups_by_path: dict[str, list[tuple[str, ...]]],
    errors: list[str],
    label: str,
) -> None:
    for relative, groups in groups_by_path.items():
        if label == "Session 11 implementation" and not (root / relative).is_file():
            # The implementation artifact inventory emits the more useful single
            # missing-file diagnostic for new Session 11 files.
            continue
        text = _read_text(root, relative, errors)
        if not text:
            continue
        for alternatives in groups:
            if not any(token in text for token in alternatives):
                errors.append(
                    f"{label} token group missing in {relative}: one of {list(alternatives)!r}"
                )


def _project_runtime_sources(root: Path) -> Iterable[Path]:
    source_root = root / "Source/DiscGolfTour"
    if not source_root.is_dir():
        return []
    return (
        path
        for path in source_root.rglob("*")
        if path.is_file()
        and path.suffix.casefold() in {".h", ".cpp", ".cs"}
        and "Tests" not in path.parts
    )


def _validate_forbidden_wiring_text(relative: str, text: str) -> list[str]:
    return [
        f"parallel plugin authority wiring is forbidden in {relative}: {token}"
        for token in FORBIDDEN_PROJECT_WIRING
        if token in text
    ]


def _validate_sources(root: Path, contract: dict[str, Any]) -> tuple[list[str], list[str], bool]:
    errors: list[str] = []
    warnings: list[str] = []

    for binding in FROZEN_CONTINUITY:
        _check_file_binding(root, binding, errors, "frozen continuity artifact")

    for section in ("session9", "session10"):
        continuity = _at(contract, f"continuity.{section}")
        if not isinstance(continuity, dict):
            continue
        for prefix in ("contract", "validator"):
            expected = next(
                (item for item in FROZEN_CONTINUITY if item["path"] == continuity.get(f"{prefix}_path")),
                None,
            )
            if expected is None:
                errors.append(f"contract continuity {section}.{prefix} does not name the frozen artifact")
            elif continuity.get(f"{prefix}_bytes") != expected["bytes"] or continuity.get(f"{prefix}_sha256") != expected["sha256"]:
                errors.append(f"contract continuity binding changed for {section}.{prefix}")

    donor = _at(contract, "donor_reference")
    if isinstance(donor, dict):
        for key in ("session_spec", "equipment_schema", "telemetry_schema"):
            binding = donor.get(key)
            if isinstance(binding, dict):
                _check_file_binding(root, binding, errors, f"reference-only donor {key}")

    required = _at(contract, "current_authority_inventory.required_files")
    if not isinstance(required, list):
        errors.append("current authority required file inventory is malformed")
    else:
        for relative in required:
            if not isinstance(relative, str) or not (root / relative).is_file():
                errors.append(f"missing project-owned authority artifact: {relative!r}")

    assets = _at(contract, "generic_catalog_contract.exact_assets")
    if isinstance(assets, list):
        for binding in assets:
            if isinstance(binding, dict):
                _check_file_binding(root, binding, errors, "generic catalog asset")

    protected = _at(contract, "persistence_contract")
    if isinstance(protected, dict):
        _check_file_binding(
            root,
            {
                "path": protected.get("protected_save_path"),
                "bytes": protected.get("protected_save_bytes"),
                "sha256": protected.get("protected_save_sha256"),
            },
            errors,
            "protected production profile",
        )

    _require_token_groups(root, AUTHORITY_TOKEN_GROUPS, errors, "project authority")

    for path in _project_runtime_sources(root):
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"cannot scan runtime source {path.relative_to(root).as_posix()}: {exc}")
            continue
        relative = path.relative_to(root).as_posix()
        errors.extend(_validate_forbidden_wiring_text(relative, text))
        folded = text.casefold()
        for token in FORBIDDEN_ACTIVE_BRAND_TOKENS:
            if token.casefold() in folded:
                errors.append(f"forbidden dormant brand token is active in project runtime source {relative}: {token}")

    activation_tokens = _read_text(root, "Source/DiscGolfTour/DiscBagComponent.h", errors)
    implementation_active = any((root / relative).is_file() for relative in IMPLEMENTATION_FILES)
    implementation_active = implementation_active or "FDGDiscInstance" in activation_tokens

    if implementation_active:
        for relative in IMPLEMENTATION_FILES:
            if not (root / relative).is_file():
                errors.append(f"Session 11 implementation is partial; missing implementation artifact: {relative}")
        _require_token_groups(root, IMPLEMENTATION_TOKEN_GROUPS, errors, "Session 11 implementation")
    else:
        warnings.append(
            "Session 11 runtime implementation is not present; contract scaffold is internally valid but the exit gate remains pending"
        )

    default_game = _read_text(root, "Config/DefaultGame.ini", errors)
    for token in FORBIDDEN_ACTIVE_BRAND_TOKENS:
        if token.casefold() in default_game.casefold():
            errors.append(f"forbidden dormant brand token is active in DefaultGame.ini: {token}")
    for asset_type in ("DiscMold", "DiscPlastic"):
        if asset_type not in default_game or "/Game/Data/Discs" not in default_game:
            errors.append(f"DefaultGame.ini does not retain the project-owned {asset_type} primary asset scan")

    catalog = _read_text(root, "Source/DiscGolfTour/DiscCatalogSubsystem.cpp", errors)
    for stable_id in EXPECTED_MOLDS:
        if f'TEXT("{stable_id}")' not in catalog:
            errors.append(f"generic catalog source is missing stable mold id {stable_id}")
    for stable_id in EXPECTED_PLASTICS:
        if f'TEXT("{stable_id}")' not in catalog:
            errors.append(f"generic catalog source is missing stable plastic id {stable_id}")

    return errors, warnings, implementation_active


def validate(root: Path, contract_path: Path) -> dict[str, Any]:
    technical_errors: list[str] = []
    warnings: list[str] = []
    try:
        contract = load_json(root / contract_path)
    except StrictJsonError as exc:
        contract = {}
        technical_errors.append(str(exc))

    if contract:
        technical_errors.extend(_validate_contract(contract))
        source_errors, source_warnings, implementation_active = _validate_sources(root, contract)
        technical_errors.extend(source_errors)
        warnings.extend(source_warnings)
    else:
        implementation_active = False

    technical_pass = not technical_errors
    return {
        "schema": "DiscGolfTour.Session11EquipmentThrowLabAudit.v1",
        "status": NORMAL_STATUS if technical_pass else "FAIL_TECHNICAL_EQUIPMENT_THROW_LAB_CONTRACT",
        "technicalPass": technical_pass,
        "implementationAuditActive": implementation_active,
        "releaseStatus": RELEASE_STATUS,
        "releaseBlocked": True,
        "releaseReady": False,
        "releaseUseAllowed": False,
        "authority": "PROJECT_OWNED_CATALOG_BAG_RELEASE_FLIGHT_TRAJECTORY_AND_REPLAY_REMAIN_AUTHORITATIVE",
        "inheritedReleaseBlockerCount": len(S9_BLOCKERS) + len(S10_BLOCKERS),
        "releaseBlockerCount": len(EXPECTED_BLOCKERS),
        "releaseBlockers": EXPECTED_BLOCKERS,
        "technicalErrors": technical_errors,
        "warnings": warnings,
    }


def _self_test(root: Path, contract_path: Path) -> tuple[bool, list[str]]:
    failures: list[str] = []
    try:
        baseline = load_json(root / contract_path)
    except StrictJsonError as exc:
        return False, [f"baseline contract cannot be loaded: {exc}"]

    if _validate_contract(baseline):
        failures.append("baseline contract semantic validation failed")

    mutations = [
        ("releaseBlocked false", "releaseBlocked", False),
        ("parallel authority", "current_authority_inventory.parallel_authority_allowed", True),
        ("plugin bag enabled", "donor_reference.plugin_bag_component_policy", "ACTIVE"),
        ("wrong telemetry unit", "telemetry_contract.linear_velocity_unit", "CENTIMETERS_PER_SECOND"),
        ("telemetry mutates simulation", "telemetry_contract.telemetry_changes_simulation", True),
        ("parallel regression", "regression_contract.create_parallel_regression_authority", True),
        ("shipping UI claim", "development_ui_contract.shipping_ui_claimed", True),
        ("calibrated wear claim", "exit_gate.calibrated_weight_or_wear_claimed", True),
        ("release blockers weakened", "release_blockers", EXPECTED_BLOCKERS[:-1]),
        ("donor reference rebound", "donor_reference.session_spec.sha256", "0" * 64),
        ("protected profile rebound", "persistence_contract.protected_save_sha256", "0" * 64),
    ]
    for label, path, replacement in mutations:
        candidate = copy.deepcopy(baseline)
        current: dict[str, Any] = candidate
        parts = path.split(".")
        for part in parts[:-1]:
            current = current[part]
        current[parts[-1]] = replacement
        if not _validate_contract(candidate):
            failures.append(f"mutation was not rejected: {label}")

    try:
        _strict_pairs([("duplicate", 1), ("duplicate", 2)])
        failures.append("duplicate JSON key was not rejected")
    except StrictJsonError:
        pass

    for token in FORBIDDEN_PROJECT_WIRING:
        if not _validate_forbidden_wiring_text("Synthetic.cpp", f"prefix {token} suffix"):
            failures.append(f"forbidden plugin wiring token was not rejected: {token}")

    if _validate_forbidden_wiring_text("Synthetic.cpp", "FDGDiscInstance reusable DTO only"):
        failures.append("reusable plugin disc-instance DTO was incorrectly rejected")

    if _exit_code([], False) != 0 or _exit_code([], True) != 2 or _exit_code(["x"], True) != 1:
        failures.append("normal/release-required exit-code contract is incorrect")

    return not failures, failures


def _exit_code(technical_errors: list[str], release_required: bool) -> int:
    if technical_errors:
        return 1
    return 2 if release_required else 0


def _write_report(root: Path, report_path: Path, report: dict[str, Any]) -> None:
    target = root / report_path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--report", type=Path, default=REPORT_PATH)
    parser.add_argument("--no-report", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument(
        "--release-required",
        "--require-release-ready",
        dest="release_required",
        action="store_true",
        help="require public release readiness; exits 2 while the exact blockers remain",
    )
    args = parser.parse_args()

    if args.self_test:
        passed, failures = _self_test(ROOT, args.contract)
        payload = {
            "selfTest": "PASS" if passed else "FAIL",
            "caseCount": 11 + len(FORBIDDEN_PROJECT_WIRING) + 4,
            "failures": failures,
        }
        print(json.dumps(payload, indent=2))
        return 0 if passed else 1

    report = validate(ROOT, args.contract)
    if not args.no_report:
        _write_report(ROOT, args.report, report)
    print(json.dumps(report, indent=2))
    return _exit_code(report["technicalErrors"], args.release_required)


if __name__ == "__main__":
    sys.exit(main())
