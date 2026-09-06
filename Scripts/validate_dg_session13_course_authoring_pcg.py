#!/usr/bin/env python3
"""Fail-closed Session 13 course-authoring, validation, and PCG contract gate."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session13CourseAuthoringPcgContract.json")
REPORT_PATH = Path("Saved/CourseReports/DG_Session13CourseAuthoringPcgAudit.json")

NORMAL_STATUS = "PASS_TECHNICAL_COURSE_AUTHORING_PCG_CONTRACT_RELEASE_BLOCKED"
IMPLEMENTATION_STATUS = (
    "TECHNICAL_AUTHORING_VALIDATION_PCG_IMPLEMENTATION_ACCEPTED_"
    "PRODUCTION_READINESS_PENDING"
)
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_TO_SESSION13_PRODUCTION_CLOSURE"
SESSION13_BLOCKER = "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING"

EXPECTED_BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
    "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
    "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
    SESSION13_BLOCKER,
]

EXPECTED_CAPABILITIES = [
    {"id": "visible_editor_authoring_actors", "status": "TECHNICAL_PASS"},
    {"id": "deterministic_course_dto_conversion", "status": "TECHNICAL_PASS"},
    {"id": "plugin_and_project_strict_validation", "status": "TECHNICAL_PASS"},
    {"id": "fail_closed_atomic_asset_update", "status": "TECHNICAL_PASS"},
    {"id": "authored_zone_to_pcg_plan_seam", "status": "TECHNICAL_PASS"},
    {"id": "strict_runtime_course_json_ingest", "status": "TECHNICAL_PASS"},
    {"id": "validated_playable_pine_ridge_hole1", "status": "TECHNICAL_PASS"},
    {"id": "build_automation_and_package_regression", "status": "TECHNICAL_PASS"},
]

EXPECTED_EVIDENCE_KEYS = [
    "editor_development_build",
    "game_development_build",
    "focused_session13_automation",
    "focused_course_authoring_automation",
    "full_automation",
    "project_validation",
    "reference_flight",
    "fresh_package",
    "packaged_live",
    "protected_profile_after_all_runs",
]

EXPECTED_FROZEN_FILES = [
    {"path": "Config/DG_BrandLicenseContract.json", "bytes": 9267,
     "sha256": "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001"},
    {"path": "Scripts/validate_dg_session9_brand_license.py", "bytes": 74292,
     "sha256": "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550"},
    {"path": "Config/DG_Session10EnvironmentContract.json", "bytes": 19976,
     "sha256": "05931329CE877E80D4D36691D9091BBD6DDACE8F3F40612C5BE11B47744CDDC5"},
    {"path": "Scripts/validate_dg_session10_environment.py", "bytes": 76608,
     "sha256": "13F3AECBB569466B6E3E8C49E0B3FA599EF57B13CE23604602CDC3158809112A"},
    {"path": "Config/DG_Session11EquipmentThrowLabContract.json", "bytes": 20213,
     "sha256": "5306B7C1354775A896332474711B5E6E92C44B44F7EFDE0D9353EB4E9DBCA79D"},
    {"path": "Scripts/validate_dg_session11_equipment_throw_lab.py", "bytes": 36254,
     "sha256": "E9D262E61122176EB4515D4AA2A4854D6354EADA2871B3A01299B17B13179D8A"},
    {"path": "Config/DG_Session12PresentationContract.json", "bytes": 14978,
     "sha256": "B8385D58B102C06AC19A986B14E2858C1906F7BD79D28D48C9AB555D0EB4AB3F"},
    {"path": "Scripts/validate_dg_session12_presentation.py", "bytes": 30684,
     "sha256": "D2FCEE88310BF00B3A7F0EE1803E8F2216D5E42C15161B631E8A473A683B5388"},
]

EXPECTED_DONOR_BINDINGS = [
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/13_COURSE_AUTHORING_VALIDATOR_PCG.md",
     "bytes": 1567, "sha256": "56875611007AD4DA0A4867529A9B2568FCAC0D9EE6C4EAA63FEFAD9F03168065"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/40_COURSE_AUTHORING_PCG_VALIDATION.md",
     "bytes": 1581, "sha256": "DFD8DAF20AD17526A5DAA05B9C39E5D78B85C0CA61511D73AA77616F62181157"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/30_BRUSHIFY_FOREST_ADAPTER.md",
     "bytes": 1114, "sha256": "863BBDACEC5E258167259A098E6D5A583C4F359587DDAB270351FEBDC9A4F552"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/47_OPTIONAL_ENGINE_FEATURE_SAFETY.md",
     "bytes": 946, "sha256": "FE0597A50FCBBC5C12CADD2E83EE28E37806E7B1CA479C745C8D9AC466B5D076"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_CourseAuthoringSchema.json",
     "bytes": 628, "sha256": "14C9F98AE8285723C4A90DD72DFF84BA093F324626E89243A8085C7A585E43C7"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CourseAuthoring/hole_template.csv",
     "bytes": 163, "sha256": "0EA8ACD98036487A5636E627FAFC11854E1D5206C82635908AE696E89DBDD6E4"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CourseAuthoring/zone_template.csv",
     "bytes": 138, "sha256": "C2ACDC4E19D76F8F0CCE5746DD09A588EA015C7B4FD00260DCC4D4D9E7D95B55"},
]

EXPECTED_REQUIRED_FILES = [
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringValidation.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringValidation.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringExporter.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringExporter.cpp",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringPcgAdapter.h",
    "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringPcgAdapter.cpp",
    "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.h",
    "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringActorsTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringValidationTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringExporterTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfCourseAuthoringPcgAdapterTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfSession13CourseAuthoringFixtureTests.cpp",
    "Source/DiscGolfTourEditor/Tests/DiscGolfPcgShippingSeparationTests.cpp",
    "Source/DiscGolfTour/DiscGolfTour.Build.cs",
    "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
    "DiscGolfTour.uproject",
    "Config/DefaultGame.ini",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.h",
    "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfCourseDefinitionTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession13CourseJsonTests.cpp",
    "Data/PineRidgeCourse.json",
    "Data/PineRidgeHole1.json",
    "Saved/CourseReports/PineRidgeHole1Validation.json",
    "Saved/EnvironmentReports/PineRidgeHole1FlightRoutes.json",
    "Saved/EnvironmentReports/PineRidgeHole1EnvironmentStatistics.json",
    "Saved/Developer/EnvironmentAssetBindingReport.json",
    "Config/DG_Session13CourseAuthoringPcgContract.json",
    "Scripts/validate_dg_session13_course_authoring_pcg.py",
    "Docs/DG_SESSION13_COURSE_AUTHORING_PCG_AUDIT.md",
]


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path: Path) -> Any:
    return json.loads(
        path.read_text(encoding="utf-8-sig"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")),
    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def require(condition: bool, message: str, issues: list[str]) -> None:
    if not condition:
        issues.append(message)


def validate_binding(root: Path, binding: Any, label: str, issues: list[str]) -> None:
    if not isinstance(binding, dict):
        issues.append(f"{label} binding is not an object")
        return
    relative = binding.get("path")
    if not isinstance(relative, str) or not relative:
        issues.append(f"{label} binding has no relative path")
        return
    path = root / relative
    require(path.is_file(), f"{label} file is missing: {relative}", issues)
    if not path.is_file():
        return
    require(path.stat().st_size == binding.get("bytes"),
            f"{label} byte count drift: {relative}", issues)
    require(sha256(path) == binding.get("sha256"),
            f"{label} SHA-256 drift: {relative}", issues)


def validate_document(contract: Any) -> list[str]:
    issues: list[str] = []
    if not isinstance(contract, dict):
        return ["contract root must be an object"]
    require(contract.get("schema") == "dg.session13.course_authoring_pcg.contract",
            "wrong Session 13 contract schema", issues)
    require(contract.get("schema_version") == 1, "wrong schema version", issues)
    require(contract.get("contract_id") == "DG_SESSION13_COURSE_AUTHORING_VALIDATION_PCG_V1",
            "wrong contract id", issues)
    require(contract.get("audit_scope") ==
            "PROJECT_OWNED_EDITOR_ONLY_PCG_AUTHORING_AND_AUTHORED_JSON_SHIPPING_TECHNICAL_CLOSURE",
            "wrong audit scope", issues)
    require(contract.get("normal_status") == NORMAL_STATUS, "wrong normal status", issues)
    require(contract.get("implementation_status") == IMPLEMENTATION_STATUS,
            "wrong implementation status", issues)
    require(contract.get("release_status") == RELEASE_STATUS, "wrong release status", issues)
    require(contract.get("bounded_technical_feature_complete") is True,
            "bounded technical feature must be complete", issues)
    require(contract.get("full_production_pcg_workflow_complete") is False,
            "full production PCG workflow must remain explicitly incomplete", issues)
    require(contract.get("releaseBlocked") is True, "releaseBlocked must be true", issues)
    require(contract.get("release_ready") is False, "release_ready must be false", issues)
    require(contract.get("release_use_allowed") is False,
            "release use must remain disallowed", issues)
    require(contract.get("session13_release_blocker") == SESSION13_BLOCKER,
            "wrong Session 13 blocker", issues)
    require(contract.get("release_blockers") == EXPECTED_BLOCKERS,
            "release blockers must equal the ordered twelve-blocker set", issues)

    authority = contract.get("authority", {})
    require(authority.get("runtime_course") ==
            "FDiscGolfHoleBlockoutDefinition_AUTHORED_JSON_SHIPPING_WITH_DEVELOPMENT_CPP_FALLBACK",
            "project runtime course authority drifted", issues)
    require(authority.get("supplementary_editor_dto") == "UDiscGolfCourseDefinition",
            "supplementary editor DTO authority drifted", issues)
    require(authority.get("plugin_dto_is_runtime_authority") is False,
            "plugin course DTO may not become runtime authority", issues)
    require(authority.get("gameplay_zones_drive_pcg") is True,
            "authored gameplay zones must drive PCG", issues)
    require(authority.get("pcg_defines_gameplay") is False,
            "PCG may not define gameplay", issues)

    authoring = contract.get("authoring_contract", {})
    require(authoring.get("editor_only") is True, "authoring must be editor-only", issues)
    require(authoring.get("designer_world_coordinate_entry_required") is False,
            "designer world-coordinate entry must not be required", issues)
    require(authoring.get("invalid_update_mutates_target") is False,
            "invalid update must be atomic/no-mutation", issues)
    require(authoring.get("stable_id_sorting") is True,
            "stable-ID sorting must be required", issues)
    require(authoring.get("tick_enabled") is False and
            authoring.get("course_origin_relative_output") is True,
            "authoring tick/origin-relative policy drifted", issues)
    require(authoring.get("successful_update_marks_package_dirty") is True and
            authoring.get("automatic_asset_save") is False,
            "authoring commit/save policy drifted", issues)

    validation = contract.get("validation_contract", {})
    require(validation.get("portable_plugin_validator_required") is True,
            "portable plugin validator must be required", issues)
    require(validation.get("project_strict_validator_required") is True,
            "project strict validator must be required", issues)
    require(validation.get("silent_repair_allowed") is False,
            "validation may not silently repair input", issues)
    require(validation.get("errors_block_update") is True,
            "errors must block the asset update", issues)
    require(validation.get("hard_bounds") == {
        "course_holes": 36, "zones_per_hole": 128, "drop_zones_per_hole": 32,
        "mandos_per_hole": 32, "polygon_points": 256,
    }, "strict validation hard bounds drifted", issues)

    pcg = contract.get("pcg_contract", {})
    require(pcg.get("plugin_enabled") is True, "PCG Editor plugin must remain enabled", issues)
    require(pcg.get("plugin_target_allow_list") == ["Editor"],
            "PCG plugin target boundary must be Editor-only", issues)
    require(pcg.get("editor_authoring_controller") ==
            "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp",
            "PCG authoring controller path drifted", issues)
    require(pcg.get("runtime_module_rules") == "Source/DiscGolfTour/DiscGolfTour.Build.cs" and
            pcg.get("editor_module_rules") ==
            "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
            "PCG module-rule boundary paths drifted", issues)
    require(pcg.get("generation_trigger") == "GenerateOnDemand",
            "PCG must remain GenerateOnDemand", issues)
    require(pcg.get("expected_runtime_generate_forest_callsite_count") == 0,
            "runtime GenerateForest callsite count must remain zero", issues)
    require(pcg.get("expected_generated_instance_count") == 0,
            "generated instance count must remain zero", issues)
    require(pcg.get("runtime_pcg_module_dependency_allowed") is False and
            pcg.get("runtime_pcg_component_or_generation_api_allowed") is False and
            pcg.get("editor_pcg_module_dependency_required") is True,
            "runtime/editor PCG compile boundary drifted", issues)
    require(pcg.get("shipping_never_cook_directory") == "/Game/Environment/Forest/PCG" and
            pcg.get("shipping_graph_cooked") is False,
            "Shipping PCG graph cook boundary drifted", issues)
    require(pcg.get("shipping_runtime_authority") ==
            "ORDERED_EXPLICIT_TREES_ARRAYS_IN_AUTHORED_HOLE_JSON" and
            (pcg.get("shipping_tree_count"), pcg.get("shipping_tree_payload_bytes"),
             pcg.get("shipping_tree_payload_sha256")) ==
            (44, 2982, "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6"),
            "Shipping authored-tree authority identity drifted", issues)
    require(pcg.get("shipping_source_fallback_allowed") is False and
            pcg.get("runtime_random_placement_allowed") is False,
            "Shipping source fallback or runtime random placement was enabled", issues)
    require(pcg.get("fresh_shipping_cook_absence_proven") is False,
            "fresh Shipping cook absence may not be claimed by this legacy source gate", issues)
    require(pcg.get("hard_exclusions_quality_invariant") is True,
            "hard exclusions must be quality-invariant", issues)
    require(pcg.get("visual_foliage_collision_enabled") is False,
            "decorative PCG visual foliage collision must be disabled", issues)
    require(pcg.get("zone_plan_output_only") is True and
            pcg.get("authoring_plan_runtime_consumer_implemented") is False,
            "PCG handoff must remain an honest value-plan-only seam", issues)
    require(pcg.get("generated_content_profiled_and_visually_approved") is False,
            "generated content may not claim profiling/visual approval", issues)
    for claim in ("collision_visual_approved", "performance_accepted", "soak_accepted",
                  "manual_gameplay_accepted"):
        require(pcg.get(claim) is False, f"PCG claim must remain false: {claim}", issues)
    require(pcg.get("production_ready") is False,
            "PCG production readiness must remain false", issues)
    require((pcg.get("binding_slots"), pcg.get("populated_binding_slots"),
             pcg.get("missing_binding_slots"), pcg.get("ready_binding_slots")) ==
            (16, 3, 13, 0), "PCG binding readiness must remain exactly 16/3/13/0", issues)

    runtime_json = contract.get("runtime_json_contract", {})
    require(runtime_json.get("authority_unchanged") is True,
            "runtime JSON authority must remain unchanged", issues)
    for policy in ("unknown_enum_policy", "duplicate_key_policy", "unknown_field_policy",
                   "boolean_as_integer_policy", "non_finite_number_policy"):
        require(runtime_json.get(policy) == "REJECT",
                f"runtime JSON policy must reject: {policy}", issues)
    require(runtime_json.get("canonical_pine_ridge_semantics_unchanged") is True,
            "canonical Pine Ridge semantics must remain unchanged", issues)

    optional = contract.get("optional_integrations", {})
    for name in ("brushify", "ultra_dynamic_sky"):
        adapter = optional.get(name, {})
        require(adapter.get("status") == "NOT_ACQUIRED_NOT_INTEGRATED",
                f"{name} optional status drifted", issues)
        require(adapter.get("runtime_required") is False,
                f"{name} may not be runtime-required", issues)
        require(adapter.get("release_use_allowed") is False,
                f"{name} release use must remain disallowed", issues)

    require(contract.get("implementation_capabilities") == EXPECTED_CAPABILITIES,
            "implementation capabilities differ from the exact technical-pass set", issues)
    continuity = contract.get("continuity", {})
    require(continuity.get("frozen_files") == EXPECTED_FROZEN_FILES,
            "frozen Session 9-12 continuity list differs from the exact set", issues)
    require(continuity.get("protected_profile") == {
        "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "bytes": 5212,
        "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        "writes_allowed": False,
    }, "protected profile contract drifted", issues)
    donor = contract.get("donor_reference", {})
    require(donor.get("policy") == "READ_ONLY_REFERENCE_NEVER_RUNTIME_AUTHORITY",
            "donor policy drifted", issues)
    require(donor.get("bindings") == EXPECTED_DONOR_BINDINGS,
            "donor binding list differs from the exact seven-file set", issues)
    require(contract.get("required_files") == EXPECTED_REQUIRED_FILES,
            "required file list differs from the exact Session 13 set", issues)
    fixture = contract.get("validated_test_hole", {})
    expected_fixture = {
        "hole_number": 1, "par": 3, "surface_count": 11,
        "strategic_tree_count": 12, "collision_fixture_count": 3,
        "landing_zone_count": 2, "shot_route_count": 3,
        "camera_anchor_count": 3, "spectator_boundary_count": 3,
        "wind_zone_count": 2, "flyover_point_count": 6,
        "environment_zone_count": 6, "representative_routes_passed": 4,
        "representative_routes_total": 4,
    }
    for key, expected in expected_fixture.items():
        require(fixture.get(key) == expected,
                f"validated test-hole contract drifted: {key}", issues)
    require(math.isclose(fixture.get("exact_distance_feet", 0.0),
                         361.85507202148438, abs_tol=1e-6),
            "validated test-hole exact distance drifted", issues)
    require(fixture.get("runtime_authority") == "Data/PineRidgeHole1.json" and
            fixture.get("actual_playable_disc_collision") is True,
            "validated test-hole runtime/collision authority drifted", issues)
    require(fixture.get("integration_test") ==
            "DiscGolfTour.Session13.CourseAuthoring.ValidatedOneHoleWorkflow",
            "validated test-hole integration test drifted", issues)

    tests = contract.get("test_contract", {})
    require(tests.get("focused_session13_filter") == "DiscGolfTour.Session13.*",
            "wrong Session 13 focused filter", issues)
    require(tests.get("focused_course_authoring_filter") == "DiscGolfTour.CourseAuthoring.*",
            "wrong course-authoring focused filter", issues)
    require(tests.get("full_filter") == "DiscGolfTour.*", "wrong full filter", issues)
    for key in ("editor_and_game_development_builds_required", "reference_flight_required",
                "fresh_non_iterative_package_required", "packaged_pine_ridge_smoke_required",
                "protected_profile_hash_required_after_all_runs"):
        require(tests.get(key) is True, f"test contract must require {key}", issues)
    evidence = contract.get("closure_evidence", {})
    require(list(evidence) == EXPECTED_EVIDENCE_KEYS,
            "closure evidence keys/order differ from the exact Session 13 set", issues)
    for key in EXPECTED_EVIDENCE_KEYS:
        entry = evidence.get(key, {})
        require(entry.get("status") == "PASS", f"{key} is not PASS", issues)
        require(isinstance(entry.get("path"), str) and bool(entry.get("path")),
                f"{key} path is missing", issues)
        require(isinstance(entry.get("bytes"), int) and entry.get("bytes") > 0,
                f"{key} byte count is missing", issues)
        require(isinstance(entry.get("sha256"), str) and len(entry.get("sha256")) == 64,
                f"{key} SHA-256 is missing", issues)
        if key in ("focused_session13_automation", "focused_course_authoring_automation",
                   "full_automation"):
            require(isinstance(entry.get("succeeded"), int) and entry.get("succeeded") > 0,
                    f"{key} must record succeeded tests", issues)
            require(entry.get("failed") == 0 and entry.get("not_run") == 0,
                    f"{key} contains failed/not-run tests", issues)
    require(evidence.get("reference_flight", {}).get("carry_m") == 84.5,
            "reference flight carry drifted", issues)
    require(evidence.get("fresh_package", {}).get("incrementally_skipped") == 0,
            "fresh package must skip zero packages incrementally", issues)
    require(evidence.get("fresh_package", {}).get("cooked_packages", 0) > 0,
            "fresh package cooked count is missing", issues)
    require(isinstance(evidence.get("fresh_package", {}).get("archive_path"), str) and
            bool(evidence.get("fresh_package", {}).get("archive_path")),
            "fresh package archive path is missing", issues)
    require(isinstance(evidence.get("fresh_package", {}).get("archive_manifest"), dict),
            "fresh package archive manifest binding is missing", issues)
    require(evidence.get("packaged_live", {}).get("pine_ridge_smoke_pass") is True,
            "packaged Pine Ridge smoke is missing", issues)

    gate = contract.get("exit_gate", {})
    require(gate.get("status") == "TECHNICAL_EXIT_ACCEPTED_PRODUCTION_READINESS_PENDING",
            "wrong exit-gate status", issues)
    require(gate.get("all_capabilities_technical_pass") is True,
            "all technical capabilities must pass", issues)
    require(gate.get("production_readiness_approved") is False,
            "production readiness must remain unapproved", issues)
    return issues


def validate_sources(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    for relative in contract.get("required_files", []):
        require((root / relative).is_file(), f"required file is missing: {relative}", issues)

    uproject = read_json(root / "DiscGolfTour.uproject")
    modules = {item.get("Name"): item.get("Type") for item in uproject.get("Modules", [])}
    plugins = {item.get("Name"): item for item in uproject.get("Plugins", [])}
    require(modules.get("DiscGolfTour") == "Runtime", "DiscGolfTour must remain Runtime", issues)
    require(modules.get("DiscGolfTourEditor") == "Editor",
            "DiscGolfTourEditor must remain Editor-only", issues)
    require(plugins.get("PCG") == {
        "Name": "PCG", "Enabled": True, "TargetAllowList": ["Editor"]
    }, "PCG plugin is not enabled exclusively for Editor", issues)

    game_target = (root / "Source/DiscGolfTour.Target.cs").read_text(
        encoding="utf-8", errors="replace")
    require("DiscGolfTourEditor" not in game_target,
            "Game target references the editor authoring module", issues)

    actor_header = (root / "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.h").read_text(
        encoding="utf-8", errors="replace")
    actor_source = (root / "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringActors.cpp").read_text(
        encoding="utf-8", errors="replace")
    for token in (
        "ADiscGolfHoleAuthoringActor", "ADiscGolfTeeAuthoringActor",
        "ADiscGolfBasketAuthoringActor", "ADiscGolfDropZoneAuthoringActor",
        "ADiscGolfMandoGateAuthoringActor", "ADiscGolfGameplayZoneAuthoringActor",
    ):
        require(token in actor_header, f"missing visible authoring type: {token}", issues)
    require("bIsEditorOnlyActor = true" in actor_source,
            "authoring actors are not explicitly editor-only", issues)
    require("PrimaryActorTick.bCanEverTick = false" in actor_source,
            "authoring actors are not explicitly tick-free", issues)

    validator = (root / "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringValidation.cpp").read_text(
        encoding="utf-8", errors="replace")
    for token in (
        "UDiscGolfCourseValidatorLibrary::ValidateCourse", "ZONE_POLYGON_SELF_INTERSECTS",
        "TEE_FORBIDDEN_ZONE_CONFLICT", "FAIRWAY_OBSTRUCTION_EXCESSIVE",
        "MANDO_DROPZONE_INVALID", "ZONE_POINT_BUDGET_EXCEEDED",
        "constexpr int32 MaxCourseHoles = 36;",
        "constexpr int32 MaxZonesPerHole = 128;",
        "constexpr int32 MaxDropZonesPerHole = 32;",
        "constexpr int32 MaxMandosPerHole = 32;",
        "constexpr int32 MaxPolygonPoints = 256;",
        "Zone.PenaltyStrokes < 0",
        "IsHardClearanceZone(Zone.ZoneType) && !Zone.bAffectsVegetation",
    ):
        require(token in validator, f"strict validator gate missing: {token}", issues)

    exporter = (root / "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringExporter.cpp").read_text(
        encoding="utf-8", errors="replace")
    for token in ("ValidateCourse", "Modify()", "MarkPackageDirty"):
        require(token in exporter, f"fail-closed exporter gate missing: {token}", issues)
    require("SavePackage" not in exporter,
            "exporter must not silently save the target package", issues)

    adapter = (root / "Source/DiscGolfTourEditor/DiscGolfCourseAuthoringPcgAdapter.cpp").read_text(
        encoding="utf-8", errors="replace")
    for zone_type in (
        "TeeSafety", "FairwayPrimary", "FairwaySecondary", "Rough", "DeepRough",
        "Green", "OutOfBounds", "WaterHazard", "Spectator", "NoSpawn",
    ):
        require(adapter.count(f"case EDGCourseZoneType::{zone_type}:") == 1,
                f"PCG adapter does not map exactly one {zone_type} case", issues)
    require("bHardExclusion" in adapter,
            "PCG adapter has no explicit hard-exclusion output", issues)
    require("bExcludeTrees" in adapter,
            "PCG adapter has no explicit fairway/tree-flight-line protection", issues)
    require("Zone.PenaltyStrokes < 0" in adapter,
            "PCG adapter does not reject negative gameplay penalties", issues)
    require("IsHardClearanceType(Zone.ZoneType) && !Zone.bAffectsVegetation" in adapter,
            "PCG adapter hard-clearance vegetation policy is ambiguous", issues)

    environment = (root / contract["pcg_contract"]["editor_authoring_controller"]).read_text(
        encoding="utf-8", errors="replace")
    require("GenerateOnDemand" in environment, "PCG generation is no longer on-demand", issues)
    require("bGenerateOnDropWhenTriggerOnDemand = false" in environment,
            "PCG may generate when an on-demand actor is dropped", issues)
    for token in ("CreateDefaultSubobject<UPCGComponent>",
                  "PCGComponent->SetGraph(Graph)", "PCGComponent->GenerateLocal("):
        require(token in environment, f"Editor PCG authoring marker is missing: {token}", issues)

    runtime_pcg_violations: list[str] = []
    for path in (root / "Source/DiscGolfTour").rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".h", ".cpp", ".cs"}:
            continue
        if "Tests" in path.parts:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for token in ("UPCG", "PCGComponent", "GenerateForest", "GenerateLocal", "SetGraph"):
            if token in text:
                runtime_pcg_violations.append(f"{path.relative_to(root).as_posix()}:{token}")
    require(not runtime_pcg_violations,
            "runtime module contains PCG type/generation API: "
            + ", ".join(runtime_pcg_violations), issues)

    runtime_rules = (root / contract["pcg_contract"]["runtime_module_rules"]).read_text(
        encoding="utf-8", errors="replace")
    editor_rules = (root / contract["pcg_contract"]["editor_module_rules"]).read_text(
        encoding="utf-8", errors="replace")
    require('"PCG"' not in runtime_rules and editor_rules.count('"PCG"') == 1,
            "PCG module dependency must be absent from runtime and owned exactly once by Editor",
            issues)
    game_ini = (root / "Config/DefaultGame.ini").read_text(
        encoding="utf-8", errors="replace")
    never_cook_line = '+DirectoriesToNeverCook=(Path="/Game/Environment/Forest/PCG")'
    require(game_ini.splitlines().count(never_cook_line) == 1,
            "Shipping graph directory must be NeverCook exactly once", issues)

    course_parser = (root / "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp").read_text(
        encoding="utf-8", errors="replace")
    for token in ("RejectDuplicateJsonKeys", "ValidateAllowedFields", "RequireFiniteNumber",
                  "RequireInteger", "RequireKnownToken", "ValidateManifestJsonShape",
                  "ValidateHoleJsonShape", "ResolveAuthoredCourseLoadAction",
                  "UE_BUILD_SHIPPING != 0", "EDiscGolfAuthoredCourseLoadAction::FailClosed"):
        require(token in course_parser, f"runtime JSON hardening gate missing: {token}", issues)

    authority_holes: list[dict[str, Any]] = []
    authority_tree_count = 0
    for hole_number in (1, 2, 3):
        authored_hole = read_json(root / f"Data/PineRidgeHole{hole_number}.json")
        trees = authored_hole.get("trees") if isinstance(authored_hole, dict) else None
        require(isinstance(trees, list),
                f"Pine Ridge Hole {hole_number} trees must be an explicit array", issues)
        if not isinstance(trees, list):
            trees = []
        authority_tree_count += len(trees)
        authority_holes.append({
            "courseId": authored_hole.get("courseId"),
            "layoutId": authored_hole.get("layoutId"),
            "holeNumber": authored_hole.get("holeNumber"),
            "trees": trees,
        })
    authority_payload = json.dumps(
        {"holes": authority_holes}, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False, allow_nan=False).encode("utf-8")
    require((authority_tree_count, len(authority_payload),
             hashlib.sha256(authority_payload).hexdigest().upper()) ==
            (44, 2982, "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6"),
            "authored Shipping 44-tree authority differs", issues)

    runtime_authority_violations: list[str] = []
    for path in (root / "Source/DiscGolfTour").rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".h", ".cpp"}:
            continue
        if "Tests" in path.parts:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "UDiscGolfCourseDefinition" in text or "DGFrameworkCourseDefinition" in text:
            runtime_authority_violations.append(path.relative_to(root).as_posix())
    require(not runtime_authority_violations,
            "runtime module references the supplementary course DTO: "
            + ", ".join(runtime_authority_violations), issues)

    serialized_editor_refs: list[str] = []
    forbidden_names = (
        "ADiscGolfHoleAuthoringActor", "DiscGolfCourseAuthoringActors", "DiscGolfTourEditor",
        "UDiscGolfCourseDefinition", "DiscGolfCourseDefinition",
        "DGFrameworkCourseDefinition", "FDGHoleDefinition", "DiscGolfCourseTypes",
    )
    forbidden_asset_tokens = tuple(
        token for name in forbidden_names
        for token in (name.encode("ascii"), name.encode("utf-16-le")))
    content_roots = [root / "Content"]
    content_roots.extend(path for path in (root / "Plugins").glob("*/Content") if path.is_dir())
    for content_root in content_roots:
        for path in content_root.rglob("*.uasset"):
            payload = path.read_bytes()
            if any(token in payload for token in forbidden_asset_tokens):
                serialized_editor_refs.append(path.relative_to(root).as_posix())
    require(not serialized_editor_refs,
            "runtime Content serializes editor-authoring types: "
            + ", ".join(serialized_editor_refs), issues)


def validate_test_hole(root: Path, issues: list[str]) -> None:
    manifest = read_json(root / "Data/PineRidgeCourse.json")
    hole = read_json(root / "Data/PineRidgeHole1.json")
    report = read_json(root / "Saved/CourseReports/PineRidgeHole1Validation.json")
    routes = read_json(root / "Saved/EnvironmentReports/PineRidgeHole1FlightRoutes.json")
    environment = read_json(root / "Saved/EnvironmentReports/PineRidgeHole1EnvironmentStatistics.json")
    bindings = read_json(root / "Saved/Developer/EnvironmentAssetBindingReport.json")

    require(manifest.get("schema") == "disc_golf_course_manifest" and
            len(manifest.get("holes", [])) == 3, "Pine Ridge manifest drifted", issues)
    expected_counts = {
        "surfaces": 11, "trees": 12, "collisionFixtures": 3, "landingZones": 2,
        "shotRoutes": 3, "cameraAnchors": 3, "spectatorBoundaries": 3,
        "windZones": 2, "flyoverPointsCm": 6,
    }
    require(hole.get("holeNumber") == 1 and hole.get("par") == 3,
            "Pine Ridge Hole 1 identity/par drifted", issues)
    for key, expected in expected_counts.items():
        require(len(hole.get(key, [])) == expected,
                f"Pine Ridge Hole 1 {key} count drifted", issues)
    require(report.get("validationStatus") == "READY" and report.get("errors") == [],
            "Pine Ridge Hole 1 validation report is not technically ready", issues)
    require(math.isclose(report.get("exactTeeToBasketDistanceFeet", 0.0),
                         361.85507202148438, abs_tol=1e-6),
            "Pine Ridge Hole 1 distance drifted", issues)
    route_results = routes.get("results", [])
    require(routes.get("passed") is True and routes.get("routeCount") == 4 and
            len(route_results) == 4 and
            all(item.get("passed") is True for item in route_results),
            "Pine Ridge Hole 1 four-route acceptance is not passing", issues)
    require(report.get("environmentZones") == 6,
            "Pine Ridge Hole 1 environment-zone count drifted", issues)
    require(environment.get("pcgGeneratedInstances") == 0 and
            environment.get("pcgGenerationOnDemand") is True and
            environment.get("collisionQualityInvariant") is True and
            environment.get("collisionProxies", 0) > 0,
            "Hole 1 environment/collision/PCG boundary drifted", issues)
    slots = bindings.get("slots", [])
    populated = sum(any(candidate.get("asset_path")
                        for candidate in item.get("candidates", [])) for item in slots)
    missing = len(slots) - populated
    ready = sum("READY" in item.get("slot_statuses", []) for item in slots)
    require(len(slots) == 16 and populated == 3 and missing == 13 and ready == 0 and
            bindings.get("production_ready") is False,
            "environment readiness must remain exactly 16/3/13/0 and not production-ready", issues)


def validate_bound_files(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    continuity = contract.get("continuity", {})
    for binding in continuity.get("frozen_files", []):
        validate_binding(root, binding, "frozen", issues)
    validate_binding(root, continuity.get("protected_profile", {}), "protected profile", issues)
    for binding in contract.get("donor_reference", {}).get("bindings", []):
        validate_binding(root, binding, "donor", issues)
    evidence = contract.get("closure_evidence", {})
    for key, entry in evidence.items():
        validate_binding(root, entry, f"closure evidence {key}", issues)
    fresh_package = evidence.get("fresh_package", {})
    archive_path = fresh_package.get("archive_path")
    if isinstance(archive_path, str):
        require(bool(archive_path) and Path(archive_path).is_absolute() and
                Path(archive_path).is_dir(),
                "fresh package archive directory is missing or not absolute", issues)
    archive_manifest = fresh_package.get("archive_manifest", {})
    validate_binding(root, archive_manifest, "fresh package archive manifest", issues)
    if isinstance(archive_manifest, dict) and archive_manifest.get("path"):
        manifest_path = root / archive_manifest["path"]
        if manifest_path.is_file():
            try:
                manifest = read_json(manifest_path)
                require(manifest.get("archivePath") == archive_path,
                        "fresh package archive manifest path differs from contract", issues)
                require(manifest.get("fileCount") == fresh_package.get("archive_file_count") and
                        manifest.get("fileCount", 0) > 0,
                        "fresh package archive manifest file count differs", issues)
                require(isinstance(manifest.get("files"), list) and
                        len(manifest.get("files", [])) == manifest.get("fileCount"),
                        "fresh package archive manifest entries differ from file count", issues)
                require(all(isinstance(item, dict) and item.get("path") and
                            isinstance(item.get("bytes"), int) and item.get("bytes") >= 0 and
                            isinstance(item.get("sha256"), str) and len(item.get("sha256")) == 64
                            for item in manifest.get("files", [])),
                        "fresh package archive manifest contains an invalid file binding", issues)
            except Exception as exc:  # noqa: BLE001
                issues.append(f"fresh package archive manifest is invalid: {exc}")

    for key, count_key, path_prefix in (
        ("focused_session13_automation", "expected_session13_succeeded",
         "DiscGolfTour.Session13."),
        ("focused_course_authoring_automation", "expected_course_authoring_succeeded",
         "DiscGolfTour.CourseAuthoring."),
        ("full_automation", "expected_full_succeeded", "DiscGolfTour."),
    ):
        entry = evidence.get(key, {})
        path = root / entry.get("path", "")
        if not path.is_file():
            continue
        try:
            report = read_json(path)
            expected = contract.get("test_contract", {}).get(count_key)
            require(report.get("succeeded") == expected == entry.get("succeeded"),
                    f"{key} succeeded count differs from the exact contract/report", issues)
            require(report.get("failed") == entry.get("failed") == 0,
                    f"{key} report has failures", issues)
            require(report.get("notRun") == entry.get("not_run") == 0,
                    f"{key} report has not-run tests", issues)
            require(report.get("succeededWithWarnings") == 0,
                    f"{key} report has warning-success tests", issues)
            report_tests = report.get("tests", [])
            paths = [item.get("fullTestPath") for item in report_tests]
            require(len(report_tests) == expected and len(set(paths)) == expected,
                    f"{key} report test entries are not the exact unique expected count", issues)
            require(all(isinstance(path, str) and path.startswith(path_prefix)
                        for path in paths),
                    f"{key} report contains a test outside {path_prefix}", issues)
            require(all(item.get("state") == "Success" for item in report_tests),
                    f"{key} contains a non-success test entry", issues)
        except Exception as exc:  # noqa: BLE001
            issues.append(f"{key} automation report is invalid: {exc}")

    def evidence_text(key: str) -> str:
        relative = evidence.get(key, {}).get("path")
        if not isinstance(relative, str) or not relative:
            return ""
        path = root / relative
        if not path.is_file():
            return ""
        return path.read_text(encoding="utf-8", errors="replace")

    editor_build = evidence_text("editor_development_build")
    require("DiscGolfTourEditor Win64 Development" in editor_build and
            "Result: Succeeded" in editor_build,
            "Editor Development build log lacks exact success markers", issues)
    game_build = evidence_text("game_development_build")
    require("DiscGolfTour Win64 Development" in game_build and
            "Result: Succeeded" in game_build,
            "Game Development build log lacks exact success markers", issues)
    project_validation = evidence_text("project_validation")
    require("Project validation OK" in project_validation and
            "Project validation FAILED" not in project_validation,
            "project-validation log lacks exact success markers", issues)
    reference_flight = evidence_text("reference_flight")
    require("Reference flight envelope OK" in reference_flight and
            "carry=84.5m" in reference_flight,
            "reference-flight log lacks exact success/carry markers", issues)
    package_log = evidence_text("fresh_package")
    package_markers = (
        "FULL COOK:", "IsIterativeCook=false", "Success - 0 error(s), 0 warning(s)",
        "BUILD SUCCESSFUL", "AutomationTool exiting with ExitCode=0 (Success)",
    )
    require(all(marker in package_log for marker in package_markers),
            "fresh-package log lacks full-cook success markers", issues)
    package_counts = re.search(
        r"Packages Cooked: (\d+), Packages Incrementally Skipped: (\d+)", package_log)
    require(package_counts is not None and
            int(package_counts.group(1)) == fresh_package.get("cooked_packages") and
            int(package_counts.group(2)) == fresh_package.get("incrementally_skipped") == 0,
            "fresh-package log counts differ from contract", issues)
    packaged_live = evidence_text("packaged_live")
    require("Course manifest loaded: Pine Ridge Championship | AUTHORED JSON | 3 holes."
            in packaged_live and
            "Persistent Pine Ridge assembled: 3 holes coexist in one world." in packaged_live and
            "PINE RIDGE PLAY SMOKE PASS" in packaged_live and
            "RequestExitWithStatus(0, 0" in packaged_live and
            "Fatal error:" not in packaged_live,
            "packaged-live log lacks exact authored-course/smoke/exit markers", issues)


def audit(root: Path, contract_path: Path) -> dict[str, Any]:
    issues: list[str] = []
    try:
        contract = read_json(root / contract_path)
    except Exception as exc:  # noqa: BLE001 - malformed input is a reportable gate failure
        return {"technicalPass": False, "releaseReady": False, "releaseBlocked": True,
                "issues": [f"contract load failed: {exc}"]}
    issues.extend(validate_document(contract))
    try:
        validate_bound_files(root, contract, issues)
        validate_sources(root, contract, issues)
        validate_test_hole(root, issues)
    except Exception as exc:  # noqa: BLE001 - fail closed on missing/malformed project input
        issues.append(f"repository validation raised: {exc}")
    capabilities = contract.get("implementation_capabilities", [])
    pending = [item.get("id") for item in capabilities
               if isinstance(item, dict) and item.get("status") != "TECHNICAL_PASS"]
    return {
        "schema": "dg.session13.course_authoring_pcg.audit",
        "schemaVersion": 1,
        "technicalPass": not issues,
        "releaseReady": False,
        "releaseBlocked": True,
        "normalStatus": NORMAL_STATUS,
        "implementationStatus": IMPLEMENTATION_STATUS,
        "releaseStatus": RELEASE_STATUS,
        "releaseBlockers": EXPECTED_BLOCKERS,
        "pendingCapabilities": pending,
        "issues": issues,
    }


def run_self_tests(root: Path, contract_path: Path) -> bool:
    baseline = audit(root, contract_path)
    failures: list[str] = []
    if not baseline.get("technicalPass"):
        failures.append("baseline audit did not pass")
    contract = read_json(root / contract_path)
    mutations = [
        ("schema", lambda value: value.__setitem__("schema", "wrong")),
        ("audit scope", lambda value: value.__setitem__("audit_scope", "runtime PCG")),
        ("bounded feature incomplete", lambda value: value.__setitem__(
            "bounded_technical_feature_complete", False)),
        ("production PCG complete", lambda value: value.__setitem__(
            "full_production_pcg_workflow_complete", True)),
        ("release ready", lambda value: value.__setitem__("release_ready", True)),
        ("blocker order", lambda value: value["release_blockers"].reverse()),
        ("missing S13 blocker", lambda value: value["release_blockers"].pop()),
        ("missing frozen continuity", lambda value: value["continuity"][
            "frozen_files"].pop()),
        ("missing donor", lambda value: value["donor_reference"]["bindings"].pop()),
        ("missing required source", lambda value: value["required_files"].pop()),
        ("runtime authority", lambda value: value["authority"].__setitem__(
            "runtime_course", "UDiscGolfCourseDefinition")),
        ("plugin authority", lambda value: value["authority"].__setitem__(
            "plugin_dto_is_runtime_authority", True)),
        ("PCG gameplay", lambda value: value["authority"].__setitem__(
            "pcg_defines_gameplay", True)),
        ("silent repair", lambda value: value["validation_contract"].__setitem__(
            "silent_repair_allowed", True)),
        ("invalid mutation", lambda value: value["authoring_contract"].__setitem__(
            "invalid_update_mutates_target", True)),
        ("runtime PCG call", lambda value: value["pcg_contract"].__setitem__(
            "expected_runtime_generate_forest_callsite_count", 1)),
        ("generated PCG", lambda value: value["pcg_contract"].__setitem__(
            "expected_generated_instance_count", 1)),
        ("PCG game target", lambda value: value["pcg_contract"].__setitem__(
            "plugin_target_allow_list", ["Editor", "Game"])),
        ("runtime PCG dependency", lambda value: value["pcg_contract"].__setitem__(
            "runtime_pcg_module_dependency_allowed", True)),
        ("Shipping source fallback", lambda value: value["pcg_contract"].__setitem__(
            "shipping_source_fallback_allowed", True)),
        ("Shipping graph cooked", lambda value: value["pcg_contract"].__setitem__(
            "shipping_graph_cooked", True)),
        ("cook absence overclaim", lambda value: value["pcg_contract"].__setitem__(
            "fresh_shipping_cook_absence_proven", True)),
        ("tree authority hash", lambda value: value["pcg_contract"].__setitem__(
            "shipping_tree_payload_sha256", "0" * 64)),
        ("performance overclaim", lambda value: value["pcg_contract"].__setitem__(
            "performance_accepted", True)),
        ("PCG consumer claim", lambda value: value["pcg_contract"].__setitem__(
            "authoring_plan_runtime_consumer_implemented", True)),
        ("runtime unknown fields", lambda value: value["runtime_json_contract"].__setitem__(
            "unknown_field_policy", "IGNORE")),
        ("Brushify hard dependency", lambda value: value["optional_integrations"][
            "brushify"].__setitem__("runtime_required", True)),
        ("capability pending", lambda value: value["implementation_capabilities"][0].__setitem__(
            "status", "PENDING")),
        ("automation failure", lambda value: value["closure_evidence"][
            "focused_session13_automation"].__setitem__("failed", 1)),
        ("evidence hash drift", lambda value: value["closure_evidence"][
            "full_automation"].__setitem__("sha256", "0" * 64)),
        ("incremental package", lambda value: value["closure_evidence"][
            "fresh_package"].__setitem__("incrementally_skipped", 1)),
        ("production approved", lambda value: value["exit_gate"].__setitem__(
            "production_readiness_approved", True)),
    ]
    for name, mutate in mutations:
        candidate = copy.deepcopy(contract)
        mutate(candidate)
        mutation_issues = validate_document(candidate)
        validate_bound_files(root, candidate, mutation_issues)
        if not mutation_issues:
            failures.append(f"mutation was accepted: {name}")
    total = len(mutations) + 1
    print(f"Session 13 validator self-test: {total - len(failures)}/{total} passed")
    for failure in failures:
        print(f"SELF-TEST FAIL: {failure}")
    return not failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--report", nargs="?", const=str(REPORT_PATH))
    parser.add_argument("--require-release", "--require-release-ready",
                        dest="require_release", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()

    if args.self_test and not run_self_tests(root, args.contract):
        return 1
    result = audit(root, args.contract)
    if args.report:
        report_path = Path(args.report)
        if not report_path.is_absolute():
            report_path = root / report_path
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {report_path}")
    if not result["technicalPass"]:
        print("Session 13 course-authoring/PCG contract: FAIL")
        for issue in result["issues"]:
            print(f"- {issue}")
        return 1
    print(f"Session 13 course-authoring/PCG contract: {NORMAL_STATUS}")
    print(f"Pending capabilities: {len(result['pendingCapabilities'])}")
    print(f"Release blocked: yes ({len(EXPECTED_BLOCKERS)} blockers)")
    if args.require_release:
        print(f"Release gate: {RELEASE_STATUS}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
