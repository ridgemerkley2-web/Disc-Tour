#!/usr/bin/env python3
"""Fail-closed Session 15 vertical-slice contract and evidence validator."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session15VerticalSliceContract.json")
REPORT_PATH = Path("Saved/Session15Reports/DG_Session15VerticalSliceAudit.json")

INCOMPLETE_STATUS = "INCOMPLETE_SESSION15_VERTICAL_SLICE_EVIDENCE_PENDING"
COMPLETE_STATUS = "PASS_TECHNICAL_GENERIC_VERTICAL_SLICE_CONTRACT_RELEASE_BLOCKED"
INCOMPLETE_IMPLEMENTATION = "SESSION15_IMPLEMENTATION_AND_CLOSURE_EVIDENCE_PENDING"
COMPLETE_IMPLEMENTATION = (
    "BOUNDED_GENERIC_VERTICAL_SLICE_INTEGRATION_ACCEPTED_"
    "PRODUCTION_READINESS_PENDING"
)
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_TO_SESSION15_PRODUCTION_CLOSURE"
SESSION15_BLOCKER = "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING"

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
    "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
    SESSION15_BLOCKER,
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
    {"path": "Config/DG_Session13CourseAuthoringPcgContract.json", "bytes": 17700,
     "sha256": "64554C71530F09CE4AC6F779A65A720C195112617DDEA7D08E574F4258A044F0"},
    {"path": "Scripts/validate_dg_session13_course_authoring_pcg.py", "bytes": 51940,
     "sha256": "ABD65FA427ACB7099E603CC045C5333D3E3BD26C8C8E5EF6A07C3406B3A03E2F"},
    {"path": "Config/DG_Session14CareerAiTestingContract.json", "bytes": 16762,
     "sha256": "5ADBA78E08C508C62C5BA71E7145C8939398F2819EF5B357F42D1E01EEF6C48D"},
    {"path": "Scripts/validate_dg_session14_career_ai.py", "bytes": 34967,
     "sha256": "9A7E15D9D0F0F7BF36CD1960FFA0D8ED924376975CD84DC6208B4A1E288EF691"},
]

EXPECTED_DONORS = [
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/15_FINAL_VERTICAL_SLICE.md",
     "bytes": 1239, "sha256": "BF2056D7DEFBFC71B54E5F10102AF0DDEB43AAFB6CA2C667EEFA3D39001918C2"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/46_VERTICAL_SLICE_GATE.md",
     "bytes": 971, "sha256": "B95C9FB6ED70B5D08529C997A7168AA1DA289FDB433EF922D6ED48169B70E7C6"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_VerticalSliceGate.json",
     "bytes": 638, "sha256": "105F28CB93D89262C1BE9B4559877991A1BFEF96C28AF776EF58DE3A6F950493"},
]

EXPECTED_REQUIRED_FILES = [
    "Config/DG_Session15VerticalSliceContract.json",
    "Scripts/validate_dg_session15_vertical_slice.py",
    "Scripts/run-session15-hole1-acceptance.py",
    "Docs/DG_SESSION15_VERTICAL_SLICE_AUDIT.md",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
    "Source/DiscGolfTour/DiscBagComponent.h",
    "Source/DiscGolfTour/DiscBagComponent.cpp",
    "Source/DiscGolfTour/DiscGolfRoundState.h",
    "Source/DiscGolfTour/DiscFlightComponent.h",
    "Source/DiscGolfTour/DiscReplayActor.h",
    "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.h",
    "Data/PineRidgeHole1.json",
]

EXPECTED_PLANNED_FILES = [
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceContract.h",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceContract.cpp",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceRunner.h",
    "Source/DiscGolfTour/DiscGolfSession15VerticalSliceRunner.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession15VerticalSliceTests.cpp",
]

EXPECTED_CAPABILITY_IDS = [
    "four_phase_external_userdir_runtime",
    "verified_character_creator_outfit",
    "stable_generic_disc_identity",
    "single_authoritative_rhbh_release",
    "fixed_step_flight_and_natural_lie",
    "follow_landing_camera_and_tracer",
    "throw_lab_actual_trajectory",
    "replay_actual_trajectory",
    "presentation_audio_route_or_silent_fallback",
    "circle2_circle1_caught_holeout",
    "three_stroke_one_hole_completion",
    "settings_accessibility_save_roundtrip",
    "focused_and_full_automation",
    "editor_live_acceptance",
    "fresh_package_live_acceptance",
    "rendered_performance_budget",
]

EXPECTED_EVIDENCE_KEYS = [
    "editor_development_build",
    "game_development_build",
    "inherited_validation",
    "focused_session15_automation",
    "full_automation",
    "project_validation",
    "reference_flight",
    "editor_live_acceptance",
    "editor_live_report",
    "fresh_package",
    "package_manifest",
    "packaged_live_acceptance",
    "packaged_live_report",
    "visual_manifest",
    "performance_report",
    "protected_profile_after_all_runs",
]

EXPECTED_PHASES = ["Setup", "Drive", "Finish", "Verify"]
SHA256_RE = re.compile(r"[0-9A-F]{64}\Z")


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_nonfinite(value: str) -> Any:
    raise ValueError(f"non-finite JSON number: {value}")


def require_recursive_finiteness(value: Any, context: str = "$") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError(f"non-finite JSON number at {context}")
    if isinstance(value, dict):
        for key, child in value.items():
            require_recursive_finiteness(child, f"{context}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            require_recursive_finiteness(child, f"{context}[{index}]")


def read_json(path: Path) -> Any:
    value = json.loads(
        path.read_text(encoding="utf-8-sig"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=reject_nonfinite,
    )
    require_recursive_finiteness(value)
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def require(condition: bool, message: str, issues: list[str]) -> None:
    if not condition:
        issues.append(message)


def is_plain_relative_path(value: Any) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts and ":" not in value


def validate_binding(root: Path, binding: Any, label: str, issues: list[str]) -> None:
    if not isinstance(binding, dict):
        issues.append(f"{label} binding must be an object")
        return
    if set(binding) != {"path", "bytes", "sha256"}:
        issues.append(f"{label} binding has unexpected fields")
        return
    relative = binding.get("path")
    require(is_plain_relative_path(relative), f"{label} path must be a safe project-relative path", issues)
    require(isinstance(binding.get("bytes"), int) and not isinstance(binding.get("bytes"), bool)
            and binding["bytes"] >= 0, f"{label} byte count is invalid", issues)
    require(isinstance(binding.get("sha256"), str) and
            SHA256_RE.fullmatch(binding["sha256"]) is not None,
            f"{label} SHA-256 is invalid", issues)
    if not is_plain_relative_path(relative):
        return
    path = root / relative
    require(path.is_file(), f"{label} file is missing: {relative}", issues)
    if not path.is_file():
        return
    require(path.stat().st_size == binding.get("bytes"),
            f"{label} byte count drift: {relative}", issues)
    require(sha256(path) == binding.get("sha256"),
            f"{label} SHA-256 drift: {relative}", issues)


def evidence_state(contract: dict[str, Any]) -> str:
    evidence = contract.get("closure_evidence")
    capabilities = contract.get("implementation_capabilities")
    if not isinstance(evidence, dict) or not isinstance(capabilities, list):
        return "INVALID"
    evidence_statuses = [item.get("status") if isinstance(item, dict) else None
                         for item in evidence.values()]
    capability_statuses = [item.get("status") if isinstance(item, dict) else None
                           for item in capabilities]
    if evidence_statuses and capability_statuses:
        if all(status == "PENDING" for status in evidence_statuses + capability_statuses):
            return "PENDING"
        if all(status == "PASS" for status in evidence_statuses) and all(
                status == "TECHNICAL_PASS" for status in capability_statuses):
            return "COMPLETE"
    return "MIXED"


def validate_document(contract: Any) -> list[str]:
    issues: list[str] = []
    if not isinstance(contract, dict):
        return ["contract root must be an object"]

    require(contract.get("schema") == "dg.session15.vertical_slice.contract",
            "wrong Session 15 contract schema", issues)
    require(contract.get("schema_version") == 1, "wrong Session 15 schema version", issues)
    require(contract.get("contract_id") == "DG_SESSION15_GENERIC_HOLE1_VERTICAL_SLICE_V1",
            "wrong Session 15 contract id", issues)
    require(contract.get("audit_scope") ==
            "BOUNDED_TECHNICAL_GENERIC_PINE_RIDGE_HOLE1_VERTICAL_SLICE",
            "wrong Session 15 audit scope", issues)
    require(contract.get("normal_status_incomplete") == INCOMPLETE_STATUS,
            "wrong incomplete status", issues)
    require(contract.get("normal_status_complete") == COMPLETE_STATUS,
            "wrong complete status", issues)
    require(contract.get("implementation_status_incomplete") == INCOMPLETE_IMPLEMENTATION,
            "wrong incomplete implementation status", issues)
    require(contract.get("implementation_status_complete") == COMPLETE_IMPLEMENTATION,
            "wrong complete implementation status", issues)
    require(contract.get("release_status") == RELEASE_STATUS, "wrong release status", issues)
    require(contract.get("releaseBlocked") is True and
            contract.get("release_ready") is False and
            contract.get("release_use_allowed") is False and
            contract.get("polished_vertical_slice_complete") is False and
            contract.get("production_readiness_approved") is False,
            "release/polish/production state must remain explicitly blocked", issues)
    require(contract.get("session15_release_blocker") == SESSION15_BLOCKER,
            "wrong Session 15 release blocker", issues)
    require(contract.get("release_blockers") == EXPECTED_BLOCKERS,
            "release blockers must equal the ordered fourteen-blocker set", issues)

    identity = contract.get("active_identity", {})
    require(identity == {
        "presenting_brand_id": "dg_generic",
        "generic_substitution_required": True,
        "premium_presenting_brand_runtime_active": False,
        "premium_content_activation_allowed": False,
        "commercial_brand_or_license_claim_added": False,
    }, "generic active identity or dormant premium policy drifted", issues)

    adaptation = contract.get("donor_adaptation", {})
    require(adaptation.get("policy") == "READ_ONLY_REFERENCE_NEVER_RUNTIME_AUTHORITY",
            "donor reference gained runtime authority", issues)
    require(adaptation.get("donor_premium_presentation_requirement") ==
            "EXPLICITLY_SUBSTITUTED_BY_DG_GENERIC",
            "generic substitution for donor presentation is not explicit", issues)
    require(adaptation.get("donor_character_requirement") ==
            "SATISFIED_ONLY_BY_CURRENT_VERIFIED_TECHNICAL_METAHUMAN_BINDING",
            "technical character substitution drifted", issues)
    require(adaptation.get("donor_throw_requirement") ==
            "SATISFIED_BY_ORIGINAL_PROJECT_PROCEDURAL_RHBH_TECHNICAL_MOTION",
            "technical throw substitution drifted", issues)
    require(adaptation.get("donor_course_requirement") ==
            "SATISFIED_BY_PINE_RIDGE_HOLE1_TECHNICAL_BLOCKOUT_WITH_PRODUCTION_ART_PENDING",
            "technical course substitution drifted", issues)
    require(adaptation.get("donor_flight_requirement") ==
            "REFERENCE_ENVELOPE_DIAGNOSTIC_ONLY_UNTIL_CALIBRATED",
            "uncalibrated flight may only be diagnostic", issues)
    require(adaptation.get("donor_audio_requirement") ==
            "PROJECT_ROUTER_OR_EXPLICIT_SILENT_FALLBACK_NO_LICENSE_CLAIM",
            "technical audio substitution drifted", issues)
    require(adaptation.get("donor_quality_label") == "GAMEPLAY_HIGH_NOT_ADOPTED" and
            adaptation.get("project_quality_label") == "GameplayPerformance",
            "project quality authority drifted", issues)

    require(contract.get("authority") == {
        "player_profile": "UDiscGolfSaveGame_SCHEMA_10_UNCHANGED",
        "settings": "FDiscGolfPlayerSettings_EXISTING_NORMALIZED_PERSISTENCE",
        "round": "FDiscGolfRoundState",
        "bag": "UDiscBagComponent",
        "release": "ADiscGolfTourGameMode_RequestThrow",
        "flight": "UDiscFlightComponent_FIXED_STEP_240_HZ_SI",
        "throw_lab": "UDiscThrowLabSubsystem_ACTUAL_TRAJECTORY_SAMPLES",
        "replay": "ADiscReplayActor_ACTUAL_FLIGHT_SAMPLES",
        "presentation_audio": "UDiscGolfPresentationAudioRouterComponent",
        "quality": "GameplayPerformance",
    }, "Session 15 authority map drifted", issues)

    gates = contract.get("vertical_slice_gate")
    require(isinstance(gates, list) and
            [item.get("id") for item in gates if isinstance(item, dict)] ==
            EXPECTED_CAPABILITY_IDS[:0] + [
                "actual_project_build", "verified_technical_metahuman",
                "project_character_creator", "generic_outfit", "stable_generic_bag_disc",
                "original_procedural_rhbh_motion", "exactly_one_authoritative_visible_release",
                "fixed_step_reference_flight", "pine_ridge_hole1",
                "follow_and_landing_camera", "flight_tracer", "throw_lab_actual_samples",
                "replay_actual_samples", "audio_routing", "settings_accessibility_save_load",
                "rendered_performance_diagnostic",
            ], "vertical-slice gate set/order drifted", issues)
    if isinstance(gates, list):
        require(all(isinstance(item, dict) and set(item) ==
                    {"id", "required", "technical_substitution"} and
                    item.get("required") is True and
                    isinstance(item.get("technical_substitution"), bool)
                    for item in gates), "vertical-slice gate entries are malformed", issues)

    tests = contract.get("test_contract", {})
    require(tests.get("focused_filter") == "DiscGolfTour.Session15.*" and
            tests.get("full_filter") == "DiscGolfTour.*",
            "automation filters drifted", issues)
    require([tests.get(key) for key in (
        "expected_focused_succeeded", "expected_focused_failed", "expected_focused_not_run",
        "expected_full_succeeded", "expected_full_failed", "expected_full_not_run")]
            == [8, 0, 0, 231, 0, 0], "exact automation counts drifted", issues)
    for key in ("editor_and_game_development_builds_required", "reference_flight_required",
                "fresh_non_iterative_package_required",
                "editor_and_packaged_live_acceptance_required",
                "protected_profile_hash_required_after_all_runs"):
        require(tests.get(key) is True, f"test requirement {key} must remain true", issues)

    live = contract.get("live_acceptance", {})
    require(live.get("runtime_flag") == "-Session15VerticalSliceSmokeTest" and
            live.get("phase_argument") == "-Session15VerticalSlicePhase" and
            live.get("phase_order") == EXPECTED_PHASES,
            "live flag or phase order drifted", issues)
    require(live.get("external_user_dir_parent") == "C:/DGTour_TestRuns/Session15" and
            live.get("recoverable_backup_parent") == "C:/DGTour_Backups",
            "external run/backup parent drifted", issues)
    for key in ("one_fresh_external_guid_user_dir", "same_absolute_user_dir_all_phases",
                "project_savegames_and_content_snapshots_required",
                "successful_run_moved_after_validation"):
        require(live.get(key) is True, f"live invariant {key} must remain true", issues)
    require(live.get("project_savegames_or_content_mutation_allowed") is False and
            live.get("failed_run_moved_to_backup") is False,
            "live failure/mutation policy drifted", issues)
    require(live.get("report_root") == "Saved/Session15Reports" and
            live.get("phase_report_relative_pattern") ==
            "Saved/Session15Reports/{Phase}/PhaseReport.json" and
            live.get("canonical_report_relative_path") ==
            "Saved/Session15Reports/Session15VerticalSliceReport.json" and
            live.get("drive_performance_relative_path") ==
            "Saved/Session15Reports/Drive/PerformanceReport.json" and
            live.get("drive_screenshot_relative_path") ==
            "Saved/Session15Reports/Drive/Session15_Drive.png" and
            live.get("finish_screenshot_relative_path") ==
            "Saved/Session15Reports/Finish/Session15_HoleComplete.png",
            "live artifact paths drifted", issues)
    require(live.get("phase_schema") == "DiscGolfTour.Session15VerticalSlicePhase.v1" and
            live.get("canonical_schema") ==
            "DiscGolfTour.Session15VerticalSliceAcceptance.v1" and
            live.get("performance_schema") ==
            "DiscGolfTour.Session15VerticalSlicePerformance.v1" and
            live.get("canonical_result") ==
            "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED",
            "live report schemas/result drifted", issues)
    require(live.get("phase_log_markers") == [
        "DG_SESSION15_VERTICAL_SLICE_SETUP: PASS",
        "DG_SESSION15_VERTICAL_SLICE_DRIVE: PASS",
        "DG_SESSION15_VERTICAL_SLICE_FINISH: PASS",
        "DG_SESSION15_VERTICAL_SLICE_VERIFY: PASS",
    ] and live.get("canonical_log_marker") ==
            "DG_SESSION15_VERTICAL_SLICE_ACCEPTANCE: "
            "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED",
            "live log markers drifted", issues)

    package = contract.get("package_contract", {})
    require(package == {
        "fresh_package_required": True,
        "iterative_cook_allowed": False,
        "packaged_rhi": "D3D12",
        "packaged_resolution": {"width": 1920, "height": 1080},
        "null_rhi_allowed": False,
        "package_manifest_required": True,
        "manifest_hashes_all_files": True,
    }, "fresh rendered package contract drifted", issues)
    performance = contract.get("performance_contract", {})
    require(performance == {
        "quality_profile": "GameplayPerformance",
        "rendered_rhi": "D3D12",
        "resolution": {"width": 1920, "height": 1080},
        "warmup_seconds_min": 10,
        "sample_count_min": 120,
        "p95_frame_ms_max": 22.0,
        "hitch_count_max": 0,
        "memory_bytes_max": 3758096384,
        "target_frame_ms": 16.667,
        "target_is_diagnostic_not_release_claim": True,
    }, "rendered performance contract drifted", issues)

    continuity = contract.get("continuity", {})
    require(continuity.get("frozen_files") == EXPECTED_FROZEN_FILES,
            "frozen Session 9-14 bindings drifted", issues)
    require(continuity.get("protected_profile") == {
        "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "bytes": 5212,
        "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        "writes_allowed": False,
    }, "protected canonical profile binding drifted", issues)
    donor = contract.get("donor_reference", {})
    require(donor.get("policy") == "READ_ONLY_REFERENCE_NEVER_RUNTIME_AUTHORITY" and
            donor.get("bindings") == EXPECTED_DONORS,
            "Session 15 donor bindings/policy drifted", issues)
    require(contract.get("required_files") == EXPECTED_REQUIRED_FILES,
            "required Session 15 file set drifted", issues)
    require(contract.get("planned_implementation_files") == EXPECTED_PLANNED_FILES,
            "planned implementation file set drifted", issues)

    capabilities = contract.get("implementation_capabilities")
    require(isinstance(capabilities, list) and
            [item.get("id") for item in capabilities if isinstance(item, dict)] ==
            EXPECTED_CAPABILITY_IDS, "implementation capability set/order drifted", issues)
    if isinstance(capabilities, list):
        require(all(isinstance(item, dict) and set(item) == {"id", "status"}
                    for item in capabilities), "capability entries are malformed", issues)
    evidence = contract.get("closure_evidence")
    require(isinstance(evidence, dict) and list(evidence) == EXPECTED_EVIDENCE_KEYS,
            "closure evidence keys/order drifted", issues)

    state = evidence_state(contract)
    require(state in {"PENDING", "COMPLETE"},
            "capabilities/evidence must be uniformly pending or uniformly complete", issues)
    if state == "PENDING":
        require(contract.get("bounded_technical_feature_complete") is False,
                "pending evidence cannot claim bounded completion", issues)
        require(all(evidence.get(key) == {"status": "PENDING"}
                    for key in EXPECTED_EVIDENCE_KEYS),
                "pending evidence records must contain no fabricated fields", issues)
        require(contract.get("exit_gate") == {
            "status": "TECHNICAL_EVIDENCE_PENDING",
            "all_capabilities_technical_pass": False,
            "production_readiness_approved": False,
        }, "pending exit gate drifted", issues)
    elif state == "COMPLETE":
        require(contract.get("bounded_technical_feature_complete") is True,
                "complete evidence must close the bounded technical feature", issues)
        require(contract.get("exit_gate") == {
            "status": "TECHNICAL_EXIT_ACCEPTED_PRODUCTION_READINESS_PENDING",
            "all_capabilities_technical_pass": True,
            "production_readiness_approved": False,
        }, "complete exit gate drifted", issues)
        validate_complete_evidence_shape(evidence, issues)
    return issues


def validate_complete_evidence_shape(evidence: dict[str, Any], issues: list[str]) -> None:
    for key in EXPECTED_EVIDENCE_KEYS:
        record = evidence.get(key)
        if not isinstance(record, dict):
            issues.append(f"{key} evidence is not an object")
            continue
        require(record.get("status") == "PASS", f"{key} evidence did not pass", issues)
        require(is_plain_relative_path(record.get("path")),
                f"{key} evidence path must be project-relative", issues)
        require(isinstance(record.get("bytes"), int) and
                not isinstance(record.get("bytes"), bool) and record["bytes"] > 0,
                f"{key} evidence byte count is invalid", issues)
        require(isinstance(record.get("sha256"), str) and
                SHA256_RE.fullmatch(record["sha256"]) is not None,
                f"{key} evidence SHA-256 is invalid", issues)
    focused = evidence.get("focused_session15_automation", {})
    require([focused.get("succeeded"), focused.get("failed"), focused.get("not_run")]
            == [8, 0, 0], "focused automation result is not exactly 8/8", issues)
    full = evidence.get("full_automation", {})
    require([full.get("succeeded"), full.get("failed"), full.get("not_run")]
            == [231, 0, 0], "full automation result is not exactly 231/231", issues)
    fresh = evidence.get("fresh_package", {})
    require(fresh.get("fresh_non_iterative") is True and
            fresh.get("incrementally_skipped") == 0 and
            isinstance(fresh.get("archive_file_count"), int) and
            fresh.get("archive_file_count", 0) > 0,
            "fresh package evidence is not non-iterative and populated", issues)
    for key, mode in (("editor_live_report", "editor"),
                      ("packaged_live_report", "packaged")):
        record = evidence.get(key, {})
        require(record.get("mode") == mode and record.get("result") ==
                "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED" and
                isinstance(record.get("run_id"), str) and bool(record.get("run_id")),
                f"{key} mode/result/run id is invalid", issues)
    performance = evidence.get("performance_report", {})
    p95 = performance.get("p95_frame_ms")
    require(isinstance(p95, (int, float)) and not isinstance(p95, bool) and
            math.isfinite(p95) and 0.0 < p95 <= 22.0 and
            performance.get("rhi") == "D3D12" and
            performance.get("width") == 1920 and performance.get("height") == 1080 and
            performance.get("sample_count", 0) >= 120 and
            performance.get("hitch_count") == 0 and
            0 < performance.get("memory_bytes", 0) <= 3758096384,
            "rendered performance evidence does not meet the technical budget", issues)
    protected = evidence.get("protected_profile_after_all_runs", {})
    require(protected.get("path") == "Saved/SaveGames/DiscGolfTour_Profile_0.sav" and
            protected.get("bytes") == 5212 and protected.get("sha256") ==
            "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
            "final protected profile evidence is not canonical A999", issues)


def validate_bound_files(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    continuity = contract.get("continuity", {})
    for index, binding in enumerate(continuity.get("frozen_files", [])
                                    if isinstance(continuity, dict) else []):
        validate_binding(root, binding, f"frozen[{index}]", issues)
    donor = contract.get("donor_reference", {})
    for index, binding in enumerate(donor.get("bindings", [])
                                    if isinstance(donor, dict) else []):
        validate_binding(root, binding, f"donor[{index}]", issues)

    protected = continuity.get("protected_profile", {}) if isinstance(continuity, dict) else {}
    validate_binding(root, {key: protected.get(key) for key in ("path", "bytes", "sha256")},
                     "protected profile", issues)
    for relative in contract.get("required_files", []) if isinstance(
            contract.get("required_files"), list) else []:
        require(is_plain_relative_path(relative), "required file path is unsafe", issues)
        if is_plain_relative_path(relative):
            require((root / relative).is_file(), f"required file is missing: {relative}", issues)

    state = evidence_state(contract)
    if state == "COMPLETE":
        for relative in contract.get("planned_implementation_files", []):
            require((root / relative).is_file(),
                    f"completed implementation file is missing: {relative}", issues)
        evidence = contract.get("closure_evidence", {})
        for key in EXPECTED_EVIDENCE_KEYS:
            record = evidence.get(key, {})
            if isinstance(record, dict) and is_plain_relative_path(record.get("path")):
                path = root / record["path"]
                require(path.is_file(), f"{key} evidence file is missing", issues)
                if path.is_file():
                    require(path.stat().st_size == record.get("bytes"),
                            f"{key} evidence byte count drifted", issues)
                    require(sha256(path) == record.get("sha256"),
                            f"{key} evidence SHA-256 drifted", issues)


def validate_closed_source_seams(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    if evidence_state(contract) != "COMPLETE":
        return
    runner_path = root / "Source/DiscGolfTour/DiscGolfSession15VerticalSliceRunner.cpp"
    game_mode_path = root / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp"
    tests_path = root / "Source/DiscGolfTour/Tests/DiscGolfSession15VerticalSliceTests.cpp"
    if not all(path.is_file() for path in (runner_path, game_mode_path, tests_path)):
        return
    runner = runner_path.read_text(encoding="utf-8")
    game_mode = game_mode_path.read_text(encoding="utf-8")
    tests = tests_path.read_text(encoding="utf-8")
    for marker in ("Session15VerticalSlicePhase", "Setup", "Drive", "Finish", "Verify",
                   "Session15Reports", "PhaseReport.json", "PerformanceReport.json",
                   "Session15VerticalSliceReport.json", "RequestThrow", "D3D12"):
        require(marker in runner, f"Session 15 runner lacks {marker}", issues)
    require("Session15VerticalSliceSmokeTest" in game_mode,
            "GameMode lacks Session 15 runtime flag", issues)
    require(tests.count('"DiscGolfTour.Session15.') >= 8,
            "Session 15 source does not visibly declare eight focused tests", issues)


def audit(root: Path = ROOT, contract_path: Path = CONTRACT_PATH) -> dict[str, Any]:
    root = root.resolve()
    path = contract_path if contract_path.is_absolute() else root / contract_path
    issues: list[str] = []
    try:
        contract = read_json(path)
    except Exception as exc:
        contract = {}
        issues.append(f"contract parse failed: {exc}")
    if not issues:
        issues.extend(validate_document(contract))
        validate_bound_files(root, contract, issues)
        validate_closed_source_seams(root, contract, issues)
    state = evidence_state(contract) if isinstance(contract, dict) else "INVALID"
    valid = not issues and state in {"PENDING", "COMPLETE"}
    pending = [item.get("id") for item in contract.get("implementation_capabilities", [])
               if isinstance(item, dict) and item.get("status") == "PENDING"]
    return {
        "schema": "dg.session15.vertical_slice.audit.v1",
        "contractValid": valid,
        "technicalIncomplete": valid and state == "PENDING",
        "technicalPass": valid and state == "COMPLETE",
        "status": (INCOMPLETE_STATUS if state == "PENDING" else
                   COMPLETE_STATUS if state == "COMPLETE" else "INVALID"),
        "implementationStatus": (INCOMPLETE_IMPLEMENTATION if state == "PENDING" else
                                 COMPLETE_IMPLEMENTATION if state == "COMPLETE" else "INVALID"),
        "releaseStatus": RELEASE_STATUS,
        "releaseReady": False,
        "releaseBlocked": True,
        "releaseBlockers": EXPECTED_BLOCKERS,
        "pendingCapabilities": pending,
        "evidenceState": state,
        "issues": issues,
    }


def run_self_tests(root: Path, contract_path: Path) -> bool:
    path = contract_path if contract_path.is_absolute() else root / contract_path
    failures: list[str] = []
    try:
        base = read_json(path)
    except Exception as exc:
        print(f"Session 15 validator self-test: contract parse failed: {exc}")
        return False

    base_issues = validate_document(base)
    validate_bound_files(root, base, base_issues)
    checked_in_state = evidence_state(base)
    if base_issues or checked_in_state not in {"PENDING", "COMPLETE"}:
        failures.append(
            "canonical checked-in contract was not accepted in a valid closure state")

    # Keep lifecycle mutations anchored to the strict pre-evidence shape even after
    # the checked-in contract advances to COMPLETE. This prevents the pending-state
    # mutations below from becoming no-ops against an already-complete contract.
    mutation_base = copy.deepcopy(base)
    if checked_in_state == "COMPLETE":
        for capability in mutation_base.get("implementation_capabilities", []):
            if isinstance(capability, dict):
                capability["status"] = "PENDING"
        mutation_base["closure_evidence"] = {
            key: {"status": "PENDING"} for key in EXPECTED_EVIDENCE_KEYS
        }
        mutation_base["bounded_technical_feature_complete"] = False
        mutation_base["exit_gate"] = {
            "status": "TECHNICAL_EVIDENCE_PENDING",
            "all_capabilities_technical_pass": False,
            "production_readiness_approved": False,
        }

    mutation_base_issues = validate_document(mutation_base)
    validate_bound_files(root, mutation_base, mutation_base_issues)
    if mutation_base_issues or evidence_state(mutation_base) != "PENDING":
        failures.append(
            "canonical pre-evidence mutation baseline was not accepted as technical incomplete")

    mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("schema", lambda v: v.__setitem__("schema", "wrong")),
        ("schema version", lambda v: v.__setitem__("schema_version", 2)),
        ("contract id", lambda v: v.__setitem__("contract_id", "wrong")),
        ("audit scope", lambda v: v.__setitem__("audit_scope", "wrong")),
        ("incomplete status", lambda v: v.__setitem__("normal_status_incomplete", "PASS")),
        ("complete status", lambda v: v.__setitem__("normal_status_complete", "PASS")),
        ("implementation status", lambda v: v.__setitem__("implementation_status_complete", "PASS")),
        ("release status", lambda v: v.__setitem__("release_status", "READY")),
        ("release ready", lambda v: v.__setitem__("release_ready", True)),
        ("release use", lambda v: v.__setitem__("release_use_allowed", True)),
        ("production approved", lambda v: v.__setitem__("production_readiness_approved", True)),
        ("polished claim", lambda v: v.__setitem__("polished_vertical_slice_complete", True)),
        ("generic identity", lambda v: v["active_identity"].__setitem__("presenting_brand_id", "other")),
        ("premium active", lambda v: v["active_identity"].__setitem__("premium_presenting_brand_runtime_active", True)),
        ("premium allowed", lambda v: v["active_identity"].__setitem__("premium_content_activation_allowed", True)),
        ("license claim", lambda v: v["active_identity"].__setitem__("commercial_brand_or_license_claim_added", True)),
        ("donor authority", lambda v: v["donor_reference"].__setitem__("policy", "RUNTIME")),
        ("donor adaptation", lambda v: v["donor_adaptation"].__setitem__("donor_premium_presentation_requirement", "ACTIVE")),
        ("uncalibrated flight", lambda v: v["donor_adaptation"].__setitem__("donor_flight_requirement", "AUTHORITATIVE")),
        ("quality", lambda v: v["authority"].__setitem__("quality", "Other")),
        ("release authority", lambda v: v["authority"].__setitem__("release", "Other")),
        ("gate missing", lambda v: v["vertical_slice_gate"].pop()),
        ("gate optional", lambda v: v["vertical_slice_gate"][0].__setitem__("required", False)),
        ("focused count", lambda v: v["test_contract"].__setitem__("expected_focused_succeeded", 7)),
        ("full count", lambda v: v["test_contract"].__setitem__("expected_full_succeeded", 230)),
        ("focused failures", lambda v: v["test_contract"].__setitem__("expected_focused_failed", 1)),
        ("runtime flag", lambda v: v["live_acceptance"].__setitem__("runtime_flag", "-Other")),
        ("phase order", lambda v: v["live_acceptance"]["phase_order"].reverse()),
        ("run parent", lambda v: v["live_acceptance"].__setitem__("external_user_dir_parent", "C:/DGTour")),
        ("same userdir", lambda v: v["live_acceptance"].__setitem__("same_absolute_user_dir_all_phases", False)),
        ("failed move", lambda v: v["live_acceptance"].__setitem__("failed_run_moved_to_backup", True)),
        ("project mutation", lambda v: v["live_acceptance"].__setitem__("project_savegames_or_content_mutation_allowed", True)),
        ("canonical path", lambda v: v["live_acceptance"].__setitem__("canonical_report_relative_path", "other")),
        ("canonical schema", lambda v: v["live_acceptance"].__setitem__("canonical_schema", "other")),
        ("marker", lambda v: v["live_acceptance"]["phase_log_markers"].pop()),
        ("iterative cook", lambda v: v["package_contract"].__setitem__("iterative_cook_allowed", True)),
        ("NullRHI", lambda v: v["package_contract"].__setitem__("null_rhi_allowed", True)),
        ("resolution", lambda v: v["package_contract"]["packaged_resolution"].__setitem__("width", 1280)),
        ("performance p95", lambda v: v["performance_contract"].__setitem__("p95_frame_ms_max", 30.0)),
        ("performance diagnostic", lambda v: v["performance_contract"].__setitem__("target_is_diagnostic_not_release_claim", False)),
        ("frozen removed", lambda v: v["continuity"]["frozen_files"].pop()),
        ("frozen hash", lambda v: v["continuity"]["frozen_files"][0].__setitem__("sha256", "0" * 64)),
        ("A999 hash", lambda v: v["continuity"]["protected_profile"].__setitem__("sha256", "0" * 64)),
        ("A999 writes", lambda v: v["continuity"]["protected_profile"].__setitem__("writes_allowed", True)),
        ("donor removed", lambda v: v["donor_reference"]["bindings"].pop()),
        ("required removed", lambda v: v["required_files"].pop()),
        ("planned removed", lambda v: v["planned_implementation_files"].pop()),
        ("capability removed", lambda v: v["implementation_capabilities"].pop()),
        ("mixed capability", lambda v: v["implementation_capabilities"][0].__setitem__("status", "TECHNICAL_PASS")),
        ("fabricated pending", lambda v: v["closure_evidence"]["editor_development_build"].__setitem__("path", "fake")),
        ("mixed evidence", lambda v: v["closure_evidence"]["editor_development_build"].__setitem__("status", "PASS")),
        ("exit claim", lambda v: v["exit_gate"].__setitem__("all_capabilities_technical_pass", True)),
        ("blocker removed", lambda v: v["release_blockers"].pop()),
        ("blocker reordered", lambda v: v["release_blockers"].reverse()),
        ("blocker id", lambda v: v.__setitem__("session15_release_blocker", "other")),
    ]
    for name, mutate in mutations:
        candidate = copy.deepcopy(mutation_base)
        mutate(candidate)
        mutation_issues = validate_document(candidate)
        validate_bound_files(root, candidate, mutation_issues)
        if not mutation_issues:
            failures.append(f"mutation was accepted: {name}")

    complete_evidence: dict[str, dict[str, Any]] = {
        key: {"status": "PASS", "path": "AGENTS.md", "bytes": 1,
              "sha256": "0" * 64}
        for key in EXPECTED_EVIDENCE_KEYS
    }
    complete_evidence["focused_session15_automation"].update(
        {"succeeded": 8, "failed": 0, "not_run": 0})
    complete_evidence["full_automation"].update(
        {"succeeded": 231, "failed": 0, "not_run": 0})
    complete_evidence["fresh_package"].update(
        {"fresh_non_iterative": True, "incrementally_skipped": 0,
         "archive_file_count": 1})
    complete_evidence["editor_live_report"].update(
        {"mode": "editor", "result": "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED",
         "run_id": "00000000-0000-0000-0000-000000000001"})
    complete_evidence["packaged_live_report"].update(
        {"mode": "packaged", "result": "PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED",
         "run_id": "00000000-0000-0000-0000-000000000002"})
    complete_evidence["performance_report"].update(
        {"p95_frame_ms": 20.0, "rhi": "D3D12", "width": 1920, "height": 1080,
         "sample_count": 120, "hitch_count": 0, "memory_bytes": 1})
    complete_evidence["protected_profile_after_all_runs"].update(
        {"path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav", "bytes": 5212,
         "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"})
    complete_shape_issues: list[str] = []
    validate_complete_evidence_shape(complete_evidence, complete_shape_issues)
    if complete_shape_issues:
        failures.append("canonical synthetic complete-evidence shape was rejected")

    complete_mutations: list[tuple[str, Callable[[dict[str, dict[str, Any]]], None]]] = [
        ("evidence traversal", lambda v: v["editor_development_build"].__setitem__("path", "../fake")),
        ("evidence lowercase hash", lambda v: v["editor_development_build"].__setitem__("sha256", "a" * 64)),
        ("evidence zero bytes", lambda v: v["editor_development_build"].__setitem__("bytes", 0)),
        ("evidence focused count", lambda v: v["focused_session15_automation"].__setitem__("succeeded", 7)),
        ("evidence focused failure", lambda v: v["focused_session15_automation"].__setitem__("failed", 1)),
        ("evidence full count", lambda v: v["full_automation"].__setitem__("succeeded", 230)),
        ("evidence iterative package", lambda v: v["fresh_package"].__setitem__("incrementally_skipped", 1)),
        ("evidence empty package", lambda v: v["fresh_package"].__setitem__("archive_file_count", 0)),
        ("evidence editor mode", lambda v: v["editor_live_report"].__setitem__("mode", "packaged")),
        ("evidence live result", lambda v: v["packaged_live_report"].__setitem__("result", "PASS")),
        ("evidence p95", lambda v: v["performance_report"].__setitem__("p95_frame_ms", 22.1)),
        ("evidence RHI", lambda v: v["performance_report"].__setitem__("rhi", "Other")),
        ("evidence resolution", lambda v: v["performance_report"].__setitem__("width", 1280)),
        ("evidence samples", lambda v: v["performance_report"].__setitem__("sample_count", 119)),
        ("evidence hitch", lambda v: v["performance_report"].__setitem__("hitch_count", 1)),
        ("evidence memory", lambda v: v["performance_report"].__setitem__("memory_bytes", 3758096385)),
        ("evidence A999", lambda v: v["protected_profile_after_all_runs"].__setitem__("sha256", "0" * 64)),
    ]
    for name, mutate in complete_mutations:
        candidate = copy.deepcopy(complete_evidence)
        mutate(candidate)
        mutation_issues = []
        validate_complete_evidence_shape(candidate, mutation_issues)
        if not mutation_issues:
            failures.append(f"complete-evidence mutation was accepted: {name}")

    parser_cases = [
        ("duplicate key", '{"schema":1,"schema":2}'),
        ("NaN", '{"value":NaN}'),
        ("Infinity", '{"value":Infinity}'),
        ("negative Infinity", '{"value":-Infinity}'),
        ("overflow", '{"value":1e999}'),
    ]
    with tempfile.TemporaryDirectory(prefix="dg_s15_validator_") as temp_dir:
        for name, text in parser_cases:
            candidate_path = Path(temp_dir) / f"{name.replace(' ', '_')}.json"
            candidate_path.write_text(text, encoding="utf-8")
            try:
                read_json(candidate_path)
            except (ValueError, json.JSONDecodeError):
                continue
            failures.append(f"strict parser accepted {name}")

    total = 3 + len(mutations) + len(complete_mutations) + len(parser_cases)
    print(f"Session 15 validator self-test: {total - len(failures)}/{total} passed")
    for failure in failures:
        print(f"SELF-TEST FAIL: {failure}")
    return not failures


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the Session 15 vertical-slice contract and closure evidence.")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--report", nargs="?", const=str(REPORT_PATH))
    parser.add_argument("--require-technical", action="store_true",
                        help="Return 3 while valid closure evidence remains pending.")
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

    if not result["contractValid"]:
        print("Session 15 vertical-slice contract: FAIL")
        for issue in result["issues"]:
            print(f"- {issue}")
        return 1
    print(f"Session 15 vertical-slice contract: {result['status']}")
    print(f"Pending capabilities: {len(result['pendingCapabilities'])}")
    print(f"Release blocked: yes ({len(EXPECTED_BLOCKERS)} blockers)")
    if args.require_release:
        print(f"Release gate: {RELEASE_STATUS}")
        return 2
    if args.require_technical and result["technicalIncomplete"]:
        print("Technical gate: closure evidence remains pending")
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
