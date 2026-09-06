#!/usr/bin/env python3
"""Fail-closed Session 14 career, AI, settings, and automation contract gate."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session14CareerAiTestingContract.json")
REPORT_PATH = Path("Saved/CareerReports/DG_Session14CareerAiTestingAudit.json")

NORMAL_STATUS = "PASS_TECHNICAL_CAREER_AI_TESTING_CONTRACT_RELEASE_BLOCKED"
IMPLEMENTATION_STATUS = (
    "BOUNDED_CAREER_AI_SETTINGS_AUTOMATION_FOUNDATION_ACCEPTED_"
    "PRODUCTION_READINESS_PENDING"
)
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_TO_SESSION14_PRODUCTION_CLOSURE"
SESSION14_BLOCKER = "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING"

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
    SESSION14_BLOCKER,
]

EXPECTED_CAPABILITIES = [
    {"id": "generic_single_event_stroke_play_contract", "status": "TECHNICAL_PASS"},
    {"id": "project_round_to_framework_scorecard_conversion", "status": "TECHNICAL_PASS"},
    {"id": "isolated_schema_v1_career_persistence", "status": "TECHNICAL_PASS"},
    {"id": "existing_profile_and_settings_authority_preserved", "status": "TECHNICAL_PASS"},
    {"id": "measured_real_flight_ai_candidates", "status": "TECHNICAL_PASS"},
    {"id": "framework_planner_deterministic_selection", "status": "TECHNICAL_PASS"},
    {"id": "selected_ai_command_existing_release_flight_execution", "status": "TECHNICAL_PASS"},
    {"id": "fast_data_score_course_save_smoke_foundation", "status": "TECHNICAL_PASS"},
    {"id": "build_automation_and_package_regression", "status": "TECHNICAL_PASS"},
]

EXPECTED_EVIDENCE_KEYS = [
    "editor_development_build",
    "game_development_build",
    "focused_session14_automation",
    "focused_competition_career_automation",
    "focused_ai_automation",
    "full_automation",
    "project_validation",
    "reference_flight",
    "editor_live_smoke",
    "live_smoke_report",
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
    {"path": "Config/DG_Session13CourseAuthoringPcgContract.json", "bytes": 17700,
     "sha256": "64554C71530F09CE4AC6F779A65A720C195112617DDEA7D08E574F4258A044F0"},
    {"path": "Scripts/validate_dg_session13_course_authoring_pcg.py", "bytes": 51940,
     "sha256": "ABD65FA427ACB7099E603CC045C5333D3E3BD26C8C8E5EF6A07C3406B3A03E2F"},
]

EXPECTED_DONOR_BINDINGS = [
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/14_CAREER_AI_SETTINGS_TESTING.md",
     "bytes": 1455, "sha256": "FE205223186C2F35A8FED0B2671301623C383D1AFB98C32C37ABBC133CE0E58A"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/42_AUTOMATION_AND_FLIGHT_REGRESSION.md",
     "bytes": 1333, "sha256": "D989306A6D700976735EFD511A668337477E0A8AE04D59AD63A4D3A9382720AC"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/43_CAREER_TOURNAMENT_CONTRACTS.md",
     "bytes": 1072, "sha256": "FA195708C5FAE52F230ABDBBA1C655E4512A8E193B0A736BA512CD13D017934A"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/44_AI_GOLFER_FRAMEWORK.md",
     "bytes": 1248, "sha256": "8013C9104B29237E6BA854239C97C3E8687CA004BA30EF688121C6FA0AF7E38F"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/45_SAVE_ARCHITECTURE.md",
     "bytes": 943, "sha256": "600AC3D11E909455310C31D6531744D83794DC5A49E3079F613F7B9EB708534A"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_CareerTournamentSchema.json",
     "bytes": 371, "sha256": "DFFA214B99E7985032E5552AD9DFEA33841DCEBE84E732F73B5528B0BCABF5E0"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_AIGolferSchema.json",
     "bytes": 536, "sha256": "5DA1BCB55F5F2CB24E8215FDB5F63995B5F8ED2117BE4565B1C53DD7BF7FDEEA"},
    {"path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_FlightRegressionSchema.json",
     "bytes": 460, "sha256": "50246664BB1C723E0B9B6F947DCB2357B7EE3E2A4117DF44027E9B1F5740739A"},
]

EXPECTED_REQUIRED_FILES = [
    "Source/DiscGolfTour/DiscGolfCompetitionRuntime.h",
    "Source/DiscGolfTour/DiscGolfCompetitionRuntime.cpp",
    "Source/DiscGolfTour/DiscGolfCareerProgressSaveGame.h",
    "Source/DiscGolfTour/DiscGolfCareerSubsystem.h",
    "Source/DiscGolfTour/DiscGolfCareerSubsystem.cpp",
    "Source/DiscGolfTour/DiscGolfAIPlannerAdapter.h",
    "Source/DiscGolfTour/DiscGolfAIPlannerAdapter.cpp",
    "Source/DiscGolfTour/DiscGolfSession14AISmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfSession14AISmokeRunner.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14CompetitionCareerTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14AITests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession14SmokeFoundationTests.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/DiscGolfSaveGame.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
    "Source/DiscGolfTour/DiscBagComponent.h",
    "Source/DiscGolfTour/DiscBagComponent.cpp",
    "Source/DiscGolfTour/DiscGolfRoundState.h",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfCompetitionTypes.h",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfTournamentDefinition.h",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfAIGolferProfile.h",
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfAIShotPlannerComponent.h",
    "Config/DG_Session14CareerAiTestingContract.json",
    "Scripts/validate_dg_session14_career_ai.py",
    "Docs/DG_SESSION14_CAREER_AI_TESTING_AUDIT.md",
]


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


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
    require(contract.get("schema") == "dg.session14.career_ai_testing.contract",
            "wrong Session 14 contract schema", issues)
    require(contract.get("schema_version") == 1, "wrong schema version", issues)
    require(contract.get("normal_status") == NORMAL_STATUS, "wrong normal status", issues)
    require(contract.get("implementation_status") == IMPLEMENTATION_STATUS,
            "wrong implementation status", issues)
    require(contract.get("release_status") == RELEASE_STATUS, "wrong release status", issues)
    require(contract.get("bounded_technical_feature_complete") is True,
            "bounded Session 14 feature must be complete", issues)
    require(contract.get("full_career_mode_complete") is False,
            "full career mode must remain explicitly incomplete", issues)
    require(contract.get("production_ai_complete") is False,
            "production AI must remain explicitly incomplete", issues)
    require(contract.get("releaseBlocked") is True and
            contract.get("release_ready") is False and
            contract.get("release_use_allowed") is False,
            "release status must remain blocked/not-ready/not-allowed", issues)
    require(contract.get("session14_release_blocker") == SESSION14_BLOCKER,
            "wrong Session 14 blocker", issues)
    require(contract.get("release_blockers") == EXPECTED_BLOCKERS,
            "release blockers must equal the ordered thirteen-blocker set", issues)

    authority = contract.get("authority", {})
    require(authority.get("player_profile") == "UDiscGolfSaveGame_SCHEMA_10_UNCHANGED",
            "production profile authority drifted", issues)
    require(authority.get("settings") == "FDiscGolfPlayerSettings_EXISTING_NORMALIZED_PERSISTENCE",
            "settings authority drifted", issues)
    require(authority.get("career") == "UDiscGolfCareerProgressSaveGame_ISOLATED_SCHEMA_1",
            "career save authority drifted", issues)
    require(authority.get("score") == "FDiscGolfRoundState_WITH_FRAMEWORK_VALUE_CONVERSION",
            "score authority drifted", issues)
    require(authority.get("ai_selector") == "UDiscGolfAIShotPlannerComponent",
            "framework AI selector must remain active", issues)
    require(authority.get("release_execution") == "ADiscGolfTourGameMode_RequestThrow",
            "selected AI release must use GameMode RequestThrow", issues)
    require(authority.get("flight") == "UDiscFlightComponent_FIXED_STEP_240_HZ_SI",
            "flight authority drifted", issues)

    tournament = contract.get("tournament_proof", {})
    require(tournament.get("event_id") == "PineRidgeChampionship" and
            tournament.get("course_id") == "PineRidgeChampionship",
            "Session 14 event/course identity drifted", issues)
    require(tournament.get("presenting_brand_id") == "dg_generic",
            "Session 14 event must use the generic presenting brand", issues)
    require(tournament.get("format") == "StrokePlay" and
            tournament.get("round_count") == 1 and
            tournament.get("hole_count") == 3 and
            tournament.get("hole_pars") == [3, 4, 4],
            "bounded one-round three-hole StrokePlay contract drifted", issues)
    require(tournament.get("project_strokes_include_penalties") is True and
            tournament.get("framework_base_strokes_subtract_penalties") is True,
            "scorecard penalty conversion policy drifted", issues)
    require(tournament.get("result_persisted") is True,
            "tournament result persistence proof is not closed", issues)

    career = contract.get("career_save_contract", {})
    require(career.get("schema_version") == 1 and
            career.get("slot") == "DGT_Career_Dev_v1" and
            career.get("profile_slot_touched") is False,
            "isolated schema-v1 career save policy drifted", issues)
    require(career.get("atomic_failed_load") is True and
            career.get("future_schema_rejected") is True and
            career.get("duplicate_results_rejected") is True,
            "career validation/failure-atomicity contract drifted", issues)
    require(career.get("max_completed_events") == 128 and
            career.get("max_round_history") == 128 and
            career.get("max_sponsorships") == 32,
            "career persistence bounds drifted", issues)

    ai = contract.get("ai_proof", {})
    require(ai.get("profile_count") == 1 and ai.get("finite_skill_count") == 6,
            "bounded AI profile contract drifted", issues)
    require(ai.get("minimum_measured_candidates") == 3 and
            ai.get("candidate_source") == "DETACHED_ADiscActor_ACTUAL_FLIGHT_OUTCOMES" and
            ai.get("analytic_alternate_solver_allowed") is False,
            "AI measured-candidate contract drifted", issues)
    require(ai.get("deterministic_stable_ties") is True and
            ai.get("invalid_input_mutates_output") is False,
            "AI determinism/failure-atomicity contract drifted", issues)
    require(ai.get("authoritative_request_count") == 1 and
            ai.get("state_tree_status") == "OPTIONAL_NOT_INTEGRATED",
            "AI authoritative execution/StateTree boundary drifted", issues)

    testing = contract.get("test_contract", {})
    for key in ("expected_session14_succeeded", "expected_career_succeeded",
                "expected_ai_succeeded", "expected_full_succeeded"):
        require(isinstance(testing.get(key), int) and testing.get(key) > 0,
                f"{key} must be a positive exact count", issues)
    require(testing.get("uncalibrated_flight_ranges_enforced") is False,
            "uncalibrated donor flight ranges may not be enforced", issues)
    require(testing.get("fresh_non_iterative_package_required") is True and
            testing.get("packaged_ai_career_smoke_required") is True and
            testing.get("protected_profile_hash_required_after_all_runs") is True,
            "Session 14 closure test requirements drifted", issues)

    continuity = contract.get("continuity", {})
    require(continuity.get("frozen_files") == EXPECTED_FROZEN_FILES,
            "frozen Session 9-13 bindings drifted", issues)
    require(continuity.get("protected_profile") == {
        "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "bytes": 5212,
        "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        "writes_allowed": False,
    }, "protected profile binding drifted", issues)
    require(contract.get("donor_reference", {}).get("policy") ==
            "READ_ONLY_REFERENCE_NEVER_RUNTIME_AUTHORITY",
            "donor authority policy drifted", issues)
    require(contract.get("donor_reference", {}).get("bindings") == EXPECTED_DONOR_BINDINGS,
            "Session 14 donor bindings drifted", issues)
    require(contract.get("required_files") == EXPECTED_REQUIRED_FILES,
            "required Session 14 file set drifted", issues)
    require(contract.get("implementation_capabilities") == EXPECTED_CAPABILITIES,
            "Session 14 capability set drifted", issues)
    evidence = contract.get("closure_evidence", {})
    require(isinstance(evidence, dict) and list(evidence) == EXPECTED_EVIDENCE_KEYS,
            "closure evidence keys/order drifted", issues)
    require(all(isinstance(evidence.get(key), dict) and
                evidence[key].get("status") == "PASS"
                for key in EXPECTED_EVIDENCE_KEYS),
            "all closure evidence must pass", issues)
    require(contract.get("exit_gate") == {
        "status": "TECHNICAL_EXIT_ACCEPTED_PRODUCTION_READINESS_PENDING",
        "all_capabilities_technical_pass": True,
        "production_readiness_approved": False,
    }, "exit gate drifted", issues)
    return issues


def validate_sources(root: Path, issues: list[str]) -> None:
    for relative in EXPECTED_REQUIRED_FILES:
        require((root / relative).is_file(), f"required file is missing: {relative}", issues)
    if issues:
        return
    competition = (root / "Source/DiscGolfTour/DiscGolfCompetitionRuntime.cpp").read_text(
        encoding="utf-8")
    career_header = (root / "Source/DiscGolfTour/DiscGolfCareerSubsystem.h").read_text(
        encoding="utf-8")
    career_source = (root / "Source/DiscGolfTour/DiscGolfCareerSubsystem.cpp").read_text(
        encoding="utf-8")
    adapter = (root / "Source/DiscGolfTour/DiscGolfAIPlannerAdapter.cpp").read_text(
        encoding="utf-8")
    runner = (root / "Source/DiscGolfTour/DiscGolfSession14AISmokeRunner.cpp").read_text(
        encoding="utf-8")
    game_mode = (root / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp").read_text(
        encoding="utf-8")
    build_rules = (root / "Source/DiscGolfTour/DiscGolfTour.Build.cs").read_text(
        encoding="utf-8")
    session14_sources = "\n".join((competition, career_header, career_source, adapter, runner))

    for marker in ("PineRidgeChampionship", "dg_generic", "StrokePlay",
                   "BuildCompletedScorecard", "CalculateTotalStrokes", "CalculateToPar"):
        require(marker in competition, f"competition source lacks {marker}", issues)
    require(re.search(r"Strokes\s*-\s*Source\.PenaltyStrokes", competition) is not None,
            "scorecard conversion does not visibly subtract included project penalties", issues)
    require("DGT_Career_Dev_v1" in career_source and
            "SaveGameToSlot" in career_source and "LoadGameFromSlot" in career_source,
            "career source lacks isolated save/load implementation", issues)
    require("CurrentSchemaVersion" in career_source and
            "MaxRoundHistory" in career_source and "ValidateProgress" in career_source,
            "career source lacks schema/bounds/validation", issues)
    require("UDiscGolfAIShotPlannerComponent" in adapter,
            "AI adapter does not invoke the framework planner", issues)
    require("FDiscFlightTelemetry" in adapter and "FThrowCommand" in adapter,
            "AI adapter does not bind measured flight outcomes to existing commands", issues)
    require("ADiscActor" in runner and "ResolveThrowRelease" in runner and
            "RequestThrow" in runner and "OnDiscSettled" in runner,
            "live runner lacks actual preview flight and authoritative request seams", issues)
    require("Session14AIGolferSmokeTest" in game_mode,
            "GameMode lacks the Session 14 live-smoke command-line gate", issues)
    require("StateTree" not in build_rules,
            "StateTree may not become a hard module dependency for the bounded proof", issues)
    for forbidden in ("premium_disc_golf", "Premium Disc Golf",
                      "DiscGolfTour_Profile_0", "UDiscGolfPlayerProfileSaveGame"):
        require(forbidden not in session14_sources,
                f"Session 14 source contains forbidden authority/brand token: {forbidden}", issues)

    save_header = (root / "Source/DiscGolfTour/DiscGolfSaveGame.h").read_text(encoding="utf-8")
    require(re.search(r"CurrentVersion\s*=\s*10", save_header) is not None,
            "production profile schema is no longer 10", issues)
    require("FDiscGolfPlayerSettings" in
            (root / "Source/DiscGolfTour/DiscGolfTourGameInstance.h").read_text(encoding="utf-8"),
            "existing player-settings authority is missing", issues)


def validate_automation_report(root: Path, entry: dict[str, Any], expected: int,
                               prefix: str, label: str, issues: list[str]) -> None:
    relative = entry.get("path")
    if not isinstance(relative, str) or not (root / relative).is_file():
        return
    try:
        report = read_json(root / relative)
        require(report.get("succeeded") == entry.get("succeeded") == expected,
                f"{label} succeeded count drifted", issues)
        require(report.get("failed") == entry.get("failed") == 0 and
                report.get("notRun") == entry.get("not_run") == 0 and
                report.get("succeededWithWarnings") == 0,
                f"{label} is not an exact clean pass", issues)
        tests = report.get("tests", [])
        paths = [item.get("fullTestPath") for item in tests]
        require(len(tests) == expected and len(set(paths)) == expected,
                f"{label} test paths are not exact and unique", issues)
        require(all(isinstance(path, str) and path.startswith(prefix) for path in paths),
                f"{label} contains a path outside {prefix}", issues)
        require(all(item.get("state") == "Success" for item in tests),
                f"{label} contains a non-success entry", issues)
    except Exception as exc:  # noqa: BLE001
        issues.append(f"{label} report is invalid: {exc}")


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

    tests = contract.get("test_contract", {})
    validate_automation_report(root, evidence.get("focused_session14_automation", {}),
                               tests.get("expected_session14_succeeded"),
                               "DiscGolfTour.Session14.", "focused Session 14", issues)
    validate_automation_report(root, evidence.get("focused_competition_career_automation", {}),
                               tests.get("expected_career_succeeded"),
                               "DiscGolfTour.Session14.CompetitionCareer.",
                               "focused competition/career", issues)
    validate_automation_report(root, evidence.get("focused_ai_automation", {}),
                               tests.get("expected_ai_succeeded"),
                               "DiscGolfTour.Session14.AI.", "focused AI", issues)
    validate_automation_report(root, evidence.get("full_automation", {}),
                               tests.get("expected_full_succeeded"),
                               "DiscGolfTour.", "full automation", issues)

    def evidence_text(key: str) -> str:
        relative = evidence.get(key, {}).get("path")
        path = root / relative if isinstance(relative, str) else root / "__missing__"
        return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""

    require("DiscGolfTourEditor Win64 Development" in evidence_text("editor_development_build") and
            "Result: Succeeded" in evidence_text("editor_development_build"),
            "Editor Development build log lacks exact success markers", issues)
    require("DiscGolfTour Win64 Development" in evidence_text("game_development_build") and
            "Result: Succeeded" in evidence_text("game_development_build"),
            "Game Development build log lacks exact success markers", issues)
    require("Project validation OK" in evidence_text("project_validation") and
            "Project validation FAILED" not in evidence_text("project_validation"),
            "project-validation log lacks exact success markers", issues)
    require("Reference flight envelope OK" in evidence_text("reference_flight") and
            "carry=84.5m" in evidence_text("reference_flight"),
            "reference-flight log lacks exact success markers", issues)
    for key in ("editor_live_smoke", "packaged_live"):
        log = evidence_text(key)
        require("SESSION 14 AI GOLFER SMOKE PASS" in log and
                "SESSION 14 CAREER TOURNAMENT SMOKE PASS" in log and
                "RequestExitWithStatus(0, 0" in log and "Fatal error:" not in log,
                f"{key} lacks exact AI/career/exit markers", issues)

    live_binding = evidence.get("live_smoke_report", {})
    live_path = root / live_binding.get("path", "")
    if live_path.is_file():
        try:
            live = read_json(live_path)
            require(live.get("schema") == "disc_golf_session14_ai_golfer_smoke" and
                    live.get("schema_version") == 1 and live.get("result") == "PASS",
                    "live smoke report identity/status drifted", issues)
            require(live.get("candidate_count", 0) >= 3 and
                    live.get("authoritative_request_count") == 1 and
                    live.get("authoritative_stroke_delta_at_launch") == 1 and
                    live.get("final_stroke_delta") == 1,
                    "live smoke report AI execution proof drifted", issues)
            require(live.get("event_id") == "PineRidgeChampionship" and
                    live.get("presenting_brand_id") == "dg_generic" and
                    live.get("career_round_history_count") == 1 and
                    live.get("career_round_trip_equal") is True and
                    live.get("career_isolated_slot_deleted") is True,
                    "live smoke report career persistence proof drifted", issues)
        except Exception as exc:  # noqa: BLE001
            issues.append(f"live smoke report is invalid: {exc}")

    fresh_package = evidence.get("fresh_package", {})
    package_log = evidence_text("fresh_package")
    require(all(marker in package_log for marker in (
        "FULL COOK:", "IsIterativeCook=false", "Success - 0 error(s), 0 warning(s)",
        "BUILD SUCCESSFUL", "AutomationTool exiting with ExitCode=0 (Success)")),
        "fresh-package log lacks full-cook success markers", issues)
    package_counts = re.search(
        r"Packages Cooked: (\d+), Packages Incrementally Skipped: (\d+)", package_log)
    require(package_counts is not None and
            int(package_counts.group(1)) == fresh_package.get("cooked_packages") and
            int(package_counts.group(2)) == fresh_package.get("incrementally_skipped") == 0,
            "fresh-package counts differ from contract", issues)
    archive_path = fresh_package.get("archive_path")
    require(isinstance(archive_path, str) and Path(archive_path).is_absolute() and
            Path(archive_path).is_dir(), "fresh package archive is missing", issues)
    manifest_binding = fresh_package.get("archive_manifest", {})
    validate_binding(root, manifest_binding, "fresh package archive manifest", issues)
    if isinstance(manifest_binding, dict) and manifest_binding.get("path"):
        manifest_path = root / manifest_binding["path"]
        if manifest_path.is_file():
            manifest = read_json(manifest_path)
            require(manifest.get("archivePath") == archive_path and
                    manifest.get("fileCount") == fresh_package.get("archive_file_count") and
                    len(manifest.get("files", [])) == manifest.get("fileCount"),
                    "fresh package archive manifest drifted", issues)


def audit(root: Path, contract_path: Path) -> dict[str, Any]:
    issues: list[str] = []
    try:
        contract = read_json(root / contract_path)
    except Exception as exc:  # noqa: BLE001
        return {"technicalPass": False, "releaseReady": False, "releaseBlocked": True,
                "issues": [f"contract load failed: {exc}"]}
    issues.extend(validate_document(contract))
    try:
        validate_bound_files(root, contract, issues)
        validate_sources(root, issues)
    except Exception as exc:  # noqa: BLE001
        issues.append(f"repository validation raised: {exc}")
    capabilities = contract.get("implementation_capabilities", [])
    pending = [item.get("id") for item in capabilities
               if isinstance(item, dict) and item.get("status") != "TECHNICAL_PASS"]
    return {
        "schema": "dg.session14.career_ai_testing.audit",
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
        ("bounded incomplete", lambda value: value.__setitem__(
            "bounded_technical_feature_complete", False)),
        ("full career", lambda value: value.__setitem__("full_career_mode_complete", True)),
        ("production AI", lambda value: value.__setitem__("production_ai_complete", True)),
        ("release ready", lambda value: value.__setitem__("release_ready", True)),
        ("blocker order", lambda value: value["release_blockers"].reverse()),
        ("missing blocker", lambda value: value["release_blockers"].pop()),
        ("profile authority", lambda value: value["authority"].__setitem__(
            "player_profile", "UDiscGolfPlayerProfileSaveGame")),
        ("score authority", lambda value: value["authority"].__setitem__(
            "score", "FDGRoundScorecard")),
        ("premium brand", lambda value: value["tournament_proof"].__setitem__(
            "presenting_brand_id", "premium_disc_golf")),
        ("double penalty", lambda value: value["tournament_proof"].__setitem__(
            "framework_base_strokes_subtract_penalties", False)),
        ("profile touched", lambda value: value["career_save_contract"].__setitem__(
            "profile_slot_touched", True)),
        ("future schema", lambda value: value["career_save_contract"].__setitem__(
            "future_schema_rejected", False)),
        ("analytic solver", lambda value: value["ai_proof"].__setitem__(
            "analytic_alternate_solver_allowed", True)),
        ("multiple requests", lambda value: value["ai_proof"].__setitem__(
            "authoritative_request_count", 2)),
        ("StateTree", lambda value: value["ai_proof"].__setitem__(
            "state_tree_status", "REQUIRED")),
        ("missing frozen", lambda value: value["continuity"]["frozen_files"].pop()),
        ("missing donor", lambda value: value["donor_reference"]["bindings"].pop()),
        ("missing required", lambda value: value["required_files"].pop()),
        ("pending capability", lambda value: value["implementation_capabilities"][0].__setitem__(
            "status", "PENDING")),
        ("automation failure", lambda value: value["closure_evidence"][
            "focused_session14_automation"].__setitem__("failed", 1)),
        ("evidence drift", lambda value: value["closure_evidence"][
            "full_automation"].__setitem__("sha256", "0" * 64)),
        ("incremental cook", lambda value: value["closure_evidence"][
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
    print(f"Session 14 validator self-test: {total - len(failures)}/{total} passed")
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
        print("Session 14 career/AI/testing contract: FAIL")
        for issue in result["issues"]:
            print(f"- {issue}")
        return 1
    print(f"Session 14 career/AI/testing contract: {NORMAL_STATUS}")
    print(f"Pending capabilities: {len(result['pendingCapabilities'])}")
    print(f"Release blocked: yes ({len(EXPECTED_BLOCKERS)} blockers)")
    if args.require_release:
        print(f"Release gate: {RELEASE_STATUS}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
