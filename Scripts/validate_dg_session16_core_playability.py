#!/usr/bin/env python3
"""Fail-closed validation for the Session 16 core-playability source contract."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONTRACT = ROOT / "Config/DG_Session16CorePlayabilityContract.json"
PENDING_NORMAL_STATUS = "PASS_SOURCE_CONTRACT_CLOSURE_PENDING_RELEASE_BLOCKED"
COMPLETE_NORMAL_STATUS = "PASS_CORE_PLAYABILITY_TECHNICAL_CLOSURE_RELEASE_BLOCKED"
PENDING_CLOSURE_STATUS = "PENDING_RUNTIME_AND_PACKAGE_EVIDENCE"
COMPLETE_CLOSURE_STATUS = "COMPLETE_RUNTIME_AND_PACKAGE_EVIDENCE"
PENDING_RELEASE_STATUS = (
    "BLOCKED_PENDING_INHERITED_SESSION9_TO_SESSION15_PRODUCTION_CLOSURE_"
    "AND_SESSION16_PLAYABILITY_CLOSURE")
COMPLETE_RELEASE_STATUS = (
    "BLOCKED_PENDING_INHERITED_SESSION9_TO_SESSION15_PRODUCTION_CLOSURE")
SESSION16_BLOCKER = "SESSION16_CORE_PLAYABILITY_CLOSURE_PENDING"
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")

EXPECTED_TOP_LEVEL_KEYS = {
    "schema", "schema_version", "scope", "normal_status", "closure_status",
    "release_status", "policy", "buildkit_sources", "gate_contract",
    "authority_catalog", "prepared_checks", "focused_automation",
    "closure_evidence", "continuity", "session16_development_gate_blocker",
    "release_blockers", "exit_gate",
}

EXPECTED_POLICY_BASE = {
    "technical_only": True,
    "legal_advice": False,
    "production_art_approval": False,
    "public_release_approval": False,
    "prepared_check_count": 44,
    "all_checks_blocking": True,
    "smoke_or_core_failure_blocks_polish": True,
    "parallel_gameplay_authority_allowed": False,
    "historical_contract_rewrite_allowed": False,
    "closure_evidence_required": True,
}

EVIDENCE_KEYS = [
    "editor_development_build", "game_development_build",
    "focused_automation", "full_automation", "editor_live_acceptance",
    "fresh_package", "packaged_live_acceptance",
    "protected_profile_after_all_runs",
]

PENDING_EVIDENCE_RECORD = {
    "status": "PENDING", "path": None, "bytes": None, "sha256": None,
}

EXPECTED_COMPLETE_EVIDENCE = {
    "editor_development_build": {
        "status": "PASS",
        "path": "Binaries/Win64/UnrealEditor-DiscGolfTour.dll",
        "bytes": 5738496,
        "sha256": "1639670CB1C9B40B80DE154BA20319078D0DA4DB1C1C11BB7899F2E40754BDD8",
    },
    "game_development_build": {
        "status": "PASS",
        "path": "Binaries/Win64/DiscGolfTour.exe",
        "bytes": 341307904,
        "sha256": "FC459A2CF47AC5C4270363186692A378A5D78ACC82987BBAA69666DF7879D55D",
    },
    "focused_automation": {
        "status": "PASS",
        "path": "Saved/Automation/Session16Focused_PostPackage_5eeeebc6-969b-4d99-a342-e62aeff522f6/index.json",
        "bytes": 4225,
        "sha256": "2703FB605C565C68A00403E49F883F236749B39003B51C491CEACEB7C53D4272",
    },
    "full_automation": {
        "status": "PASS",
        "path": "Saved/Automation/FullRegression_Session16_PostPackage_d38a9e79-59d3-4852-bb95-81bd8f3097a6/index.json",
        "bytes": 96644,
        "sha256": "1D3B3E2F21C944F02BCF469B11FDB2FC5525C327D550C807226645F3F0A4F570",
    },
    "editor_live_acceptance": {
        "status": "PASS",
        "path": "Saved/Session16Reports/Acceptance_editor_d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2/PlayabilityReport.json",
        "bytes": 42321,
        "sha256": "303EE590BBDD8F3B3A3222782D4AE83B987528258440654D12E89FEB292CC4A4",
    },
    "fresh_package": {
        "status": "PASS",
        "path": "Saved/Session16Reports/Acceptance_packaged_07bb2b2a-be53-4686-bf67-088158e3da5c/RuntimeEvidence/PackageManifest.json",
        "bytes": 10251,
        "sha256": "5E02E82DA476744DE8779407A4C746E0AE7F9562E2EE0E1FD02012587A611BE9",
    },
    "packaged_live_acceptance": {
        "status": "PASS",
        "path": "Saved/Session16Reports/Acceptance_packaged_07bb2b2a-be53-4686-bf67-088158e3da5c/PlayabilityReport.json",
        "bytes": 42317,
        "sha256": "F438431E54349CB3B07B35C2F1744AB0083DA510EFAB6E8FA1B11CF0E835A1F1",
    },
    "protected_profile_after_all_runs": {
        "status": "PASS",
        "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "bytes": 5212,
        "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
    },
}

EXPECTED_SOURCES = [
    {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_CorePlayabilityGate.json",
        "bytes": 2587,
        "sha256": "86BEE84E959E94C369052BD6EFF18BC9611EC9600605FE25C959259DBDE33E5C",
    },
    {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/51_CORE_PLAYABILITY_GATE.md",
        "bytes": 1588,
        "sha256": "836A3AD2DCDDD2F1C782BCA21B93EA7AC224407F353A1EBA81DF841BD7367135",
    },
    {
        "path": "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/56_PLAYABILITY_RELEASE_CHECKLIST.md",
        "bytes": 1221,
        "sha256": "A4CDDFB0B9999DE638A43AA72D087B417E04D68CDC347E918C946423FD786554",
    },
]

GATE_CHECKS = {
    "Smoke": [
        "boot_game", "main_menu_available", "player_profile_available",
        "course_load", "spawn_at_tee", "bag_available", "select_disc",
        "enter_aim", "start_throw", "release_notify_fires", "disc_launches",
    ],
    "CoreLoop": [
        "physics_advances", "disc_settles_or_holes_out", "lie_updates",
        "next_throw_available", "ob_penalty_if_triggered",
        "water_penalty_if_triggered", "basket_detects_completion",
        "score_updates", "hole_completes", "pause_resume", "camera_recovers",
        "input_context_recovers", "replay_exits_cleanly", "no_duplicate_player",
        "no_duplicate_disc", "no_soft_lock",
    ],
    "Round": [
        "advance_to_next_hole", "spawn_next_hole",
        "scorecard_persists_between_holes", "multiple_holes_complete",
        "round_completes", "results_screen_available",
        "return_to_menu_or_continue",
    ],
    "Persistence": [
        "save_player", "save_bag", "save_settings", "save_round_or_career",
        "load_save", "loaded_character_matches", "loaded_bag_matches",
        "loaded_settings_match", "missing_cosmetic_falls_back",
        "missing_optional_asset_does_not_block_play",
    ],
}

CHECK_SPECS: list[tuple[str, str, str, tuple[str, ...]]] = [
    ("Smoke", "boot_game", "BootFailed", ("runtime_boot",)),
    ("Smoke", "main_menu_available", "MainMenuUnavailable", ("runtime_boot", "temporary_state_recovery")),
    ("Smoke", "player_profile_available", "PlayerProfileUnavailable", ("player_profile",)),
    ("Smoke", "course_load", "CourseLoadFailed", ("course_bootstrap",)),
    ("Smoke", "spawn_at_tee", "HoleSpawnFailed", ("course_bootstrap", "runtime_boot")),
    ("Smoke", "bag_available", "BagUnavailable", ("bag_equipment",)),
    ("Smoke", "select_disc", "DiscSelectionFailed", ("bag_equipment",)),
    ("Smoke", "enter_aim", "AimStateUnavailable", ("throw_input", "temporary_state_recovery")),
    ("Smoke", "start_throw", "ThrowCouldNotStart", ("throw_input", "lifecycle_guard")),
    ("Smoke", "release_notify_fires", "ReleaseNotifyMissing", ("release_authority",)),
    ("Smoke", "disc_launches", "DiscLaunchFailed", ("release_authority", "flight_authority")),
    ("CoreLoop", "physics_advances", "PhysicsDidNotAdvance", ("flight_authority",)),
    ("CoreLoop", "disc_settles_or_holes_out", "DiscNeverSettled", ("flight_authority", "basket_score")),
    ("CoreLoop", "lie_updates", "LieNotUpdated", ("lie_rules", "basket_score")),
    ("CoreLoop", "next_throw_available", "PlayerCouldNotContinue", ("lifecycle_guard", "lie_rules")),
    ("CoreLoop", "ob_penalty_if_triggered", "OutOfBoundsRuleFailed", ("lie_rules", "basket_score")),
    ("CoreLoop", "water_penalty_if_triggered", "WaterRuleFailed", ("lie_rules", "basket_score")),
    ("CoreLoop", "basket_detects_completion", "BasketDetectionFailed", ("basket_score", "flight_authority")),
    ("CoreLoop", "score_updates", "ScoreUpdateFailed", ("basket_score", "round_state")),
    ("CoreLoop", "hole_completes", "HoleCompletionFailed", ("basket_score", "round_state")),
    ("CoreLoop", "pause_resume", "ResumeFailed", ("runtime_boot", "temporary_state_recovery")),
    ("CoreLoop", "camera_recovers", "CameraStateStuck", ("temporary_state_recovery",)),
    ("CoreLoop", "input_context_recovers", "InputContextStuck", ("temporary_state_recovery",)),
    ("CoreLoop", "replay_exits_cleanly", "ReplayStateStuck", ("temporary_state_recovery", "lifecycle_guard")),
    ("CoreLoop", "no_duplicate_player", "DuplicatePlayerDetected", ("lifecycle_guard", "runtime_boot")),
    ("CoreLoop", "no_duplicate_disc", "DuplicateDiscDetected", ("lifecycle_guard", "release_authority")),
    ("CoreLoop", "no_soft_lock", "SoftLockDetected", ("lifecycle_guard", "temporary_state_recovery")),
    ("Round", "advance_to_next_hole", "NextHoleFailed", ("round_state", "course_bootstrap")),
    ("Round", "spawn_next_hole", "HoleSpawnFailed", ("course_bootstrap", "round_state")),
    ("Round", "scorecard_persists_between_holes", "ScoreUpdateFailed", ("round_state", "presentation_results")),
    ("Round", "multiple_holes_complete", "HoleCompletionFailed", ("round_state", "basket_score")),
    ("Round", "round_completes", "RoundCompletionFailed", ("round_state",)),
    ("Round", "results_screen_available", "ResultsScreenFailed", ("presentation_results", "round_state")),
    ("Round", "return_to_menu_or_continue", "PlayerCouldNotContinue", ("runtime_boot", "temporary_state_recovery", "round_state")),
    ("Persistence", "save_player", "SaveFailed", ("player_profile",)),
    ("Persistence", "save_bag", "SaveFailed", ("bag_equipment",)),
    ("Persistence", "save_settings", "SaveFailed", ("player_profile", "temporary_state_recovery")),
    ("Persistence", "save_round_or_career", "SaveFailed", ("player_profile", "round_state", "career_persistence")),
    ("Persistence", "load_save", "LoadFailed", ("player_profile", "bag_equipment", "career_persistence")),
    ("Persistence", "loaded_character_matches", "SaveMismatch", ("player_profile", "missing_content_recovery")),
    ("Persistence", "loaded_bag_matches", "SaveMismatch", ("bag_equipment",)),
    ("Persistence", "loaded_settings_match", "SaveMismatch", ("player_profile", "temporary_state_recovery")),
    ("Persistence", "missing_cosmetic_falls_back", "InvalidCosmeticRecoveryFailed", ("missing_content_recovery", "player_profile")),
    ("Persistence", "missing_optional_asset_does_not_block_play", "MissingAssetRecoveryFailed", ("missing_content_recovery", "runtime_boot")),
]

AUTHORITY_CATALOG = [
    ("runtime_boot", "ADiscGolfTourGameMode and ADiscGolfTourPlayerController startup", [
        "Source/DiscGolfTour/DiscGolfTourGameMode.h", "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/DiscGolfTourPlayerController.h", "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp"]),
    ("player_profile", "UDiscGolfTourGameInstance schema-10 profile", [
        "Source/DiscGolfTour/DiscGolfTourGameInstance.h", "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
        "Source/DiscGolfTour/DiscGolfSaveGame.h"]),
    ("course_bootstrap", "ADevCourseBootstrap and authored Pine Ridge course definitions", [
        "Source/DiscGolfTour/DevCourseBootstrap.h", "Source/DiscGolfTour/DevCourseBootstrap.cpp",
        "Source/DiscGolfTour/DiscGolfCourseDefinition.h", "Source/DiscGolfTour/DiscGolfCourseDefinition.cpp"]),
    ("bag_equipment", "UDiscBagComponent project-owned equipment inventory and selection", [
        "Source/DiscGolfTour/DiscBagComponent.h", "Source/DiscGolfTour/DiscBagComponent.cpp"]),
    ("throw_input", "ADiscGolferPawn input and UThrowControllerComponent command capture", [
        "Source/DiscGolfTour/DiscGolferPawn.h", "Source/DiscGolfTour/DiscGolferPawn.cpp",
        "Source/DiscGolfTour/ThrowControllerComponent.h", "Source/DiscGolfTour/ThrowControllerComponent.cpp"]),
    ("release_authority", "ADiscGolfTourGameMode::RequestThrow through the accepted animated release adapter", [
        "Source/DiscGolfTour/DiscGolfTourGameMode.h", "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/DiscGolferPawn.h", "Source/DiscGolfTour/DiscGolferPawn.cpp"]),
    ("flight_authority", "UDiscFlightComponent fixed-step 240 Hz solver", [
        "Source/DiscGolfTour/DiscFlightComponent.h", "Source/DiscGolfTour/DiscFlightComponent.cpp"]),
    ("lie_rules", "DiscGolfCourseRules and authoritative FDiscGolfLieState", [
        "Source/DiscGolfTour/DiscGolfCourseRules.h", "Source/DiscGolfTour/DiscGolfCourseRules.cpp",
        "Source/DiscGolfTour/DiscGolfTypes.h"]),
    ("basket_score", "ADiscGolfTourGameMode basket outcome, stroke, and hole completion path", [
        "Source/DiscGolfTour/DiscGolfTourGameMode.h", "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/DiscGolfMath.h"]),
    ("round_state", "FDiscGolfRoundState and DiscGolfRound progression", [
        "Source/DiscGolfTour/DiscGolfRoundState.h", "Source/DiscGolfTour/DiscGolfRoundState.cpp"]),
    ("temporary_state_recovery", "Project replay, camera-view token, and input-route policies", [
        "Source/DiscGolfTour/DiscReplayActor.h", "Source/DiscGolfTour/DiscReplayActor.cpp",
        "Source/DiscGolfTour/DiscGolfCameraViewContract.h", "Source/DiscGolfTour/DiscGolfCameraViewContract.cpp",
        "Source/DiscGolfTour/DiscGolfInputRoutePolicy.h"]),
    ("lifecycle_guard", "ADiscGolfTourGameMode single-player/single-disc lifecycle gate", [
        "Source/DiscGolfTour/DiscGolfTourGameMode.h", "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/DiscGolfGameplayGate.h"]),
    ("presentation_results", "ADiscGolfHUD scorecard and round-complete presentation", [
        "Source/DiscGolfTour/DiscGolfHUD.h", "Source/DiscGolfTour/DiscGolfHUD.cpp"]),
    ("career_persistence", "UDiscGolfCareerSubsystem isolated schema-v1 career domain", [
        "Source/DiscGolfTour/DiscGolfCareerSubsystem.h", "Source/DiscGolfTour/DiscGolfCareerSubsystem.cpp",
        "Source/DiscGolfTour/DiscGolfCareerProgressSaveGame.h"]),
    ("missing_content_recovery", "DiscGolfFullCharacter normalization and optional-backend fallback", [
        "Source/DiscGolfTour/DiscGolfFullCharacterRuntime.h", "Source/DiscGolfTour/DiscGolfFullCharacterRuntime.cpp",
        "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.h", "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.cpp"]),
]

EXPECTED_AUTOMATION_TESTS = [
    "DiscGolfTour.Session16.Catalog.ExactRequiredChecks",
    "DiscGolfTour.Session16.Gates.SmokePass",
    "DiscGolfTour.Session16.Gates.CoreLoopPass",
    "DiscGolfTour.Session16.Gates.RoundPass",
    "DiscGolfTour.Session16.Gates.PersistencePass",
    "DiscGolfTour.Session16.Gates.RejectIncompleteEvidence",
    "DiscGolfTour.Session16.Contract.FailureAndReportSemantics",
    "DiscGolfTour.Session16.Watchdog.ExplicitLifecycle",
    "DiscGolfTour.Session16.Persistence.OptionalAssetFallback",
]

EXPECTED_FROZEN_FILES = [
    ("Config/DG_BrandLicenseContract.json", 9267, "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001"),
    ("Scripts/validate_dg_session9_brand_license.py", 74292, "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550"),
    ("Config/DG_Session10EnvironmentContract.json", 19976, "05931329CE877E80D4D36691D9091BBD6DDACE8F3F40612C5BE11B47744CDDC5"),
    ("Scripts/validate_dg_session10_environment.py", 76608, "13F3AECBB569466B6E3E8C49E0B3FA599EF57B13CE23604602CDC3158809112A"),
    ("Config/DG_Session11EquipmentThrowLabContract.json", 20213, "5306B7C1354775A896332474711B5E6E92C44B44F7EFDE0D9353EB4E9DBCA79D"),
    ("Scripts/validate_dg_session11_equipment_throw_lab.py", 36254, "E9D262E61122176EB4515D4AA2A4854D6354EADA2871B3A01299B17B13179D8A"),
    ("Config/DG_Session12PresentationContract.json", 14978, "B8385D58B102C06AC19A986B14E2858C1906F7BD79D28D48C9AB555D0EB4AB3F"),
    ("Scripts/validate_dg_session12_presentation.py", 30684, "D2FCEE88310BF00B3A7F0EE1803E8F2216D5E42C15161B631E8A473A683B5388"),
    ("Config/DG_Session13CourseAuthoringPcgContract.json", 17700, "64554C71530F09CE4AC6F779A65A720C195112617DDEA7D08E574F4258A044F0"),
    ("Scripts/validate_dg_session13_course_authoring_pcg.py", 51940, "ABD65FA427ACB7099E603CC045C5333D3E3BD26C8C8E5EF6A07C3406B3A03E2F"),
    ("Config/DG_Session14CareerAiTestingContract.json", 16762, "5ADBA78E08C508C62C5BA71E7145C8939398F2819EF5B357F42D1E01EEF6C48D"),
    ("Scripts/validate_dg_session14_career_ai.py", 34967, "9A7E15D9D0F0F7BF36CD1960FFA0D8ED924376975CD84DC6208B4A1E288EF691"),
    ("Config/DG_Session15VerticalSliceContract.json", 17917, "DDE8DF325D5E6765D814C9B2656FBA0D544F79F8EE549BCA63FD8F4D64EA9142"),
    ("Scripts/validate_dg_session15_vertical_slice.py", 46943, "457BC09E5D223E4B5B91250073894A56F68E8157254776DEF798F305F7B44EC2"),
]

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
    "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING",
]


class StrictJsonError(ValueError):
    pass


def _pairs_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise StrictJsonError(f"non-finite JSON number: {value}")


def strict_json_loads(text: str) -> Any:
    try:
        return json.loads(
            text,
            object_pairs_hook=_pairs_no_duplicates,
            parse_constant=_reject_constant,
        )
    except (json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(str(exc)) from exc


def strict_json_load(path: Path) -> Any:
    return strict_json_loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def safe_project_path(root: Path, relative: Any) -> Path | None:
    if not isinstance(relative, str) or not relative or "\\" in relative:
        return None
    raw = Path(relative)
    if raw.is_absolute() or any(part in {"", ".", ".."} for part in raw.parts):
        return None
    candidate = (root / raw).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError:
        return None
    return candidate


def expect(condition: bool, message: str, issues: list[str]) -> None:
    if not condition:
        issues.append(message)


def exact_keys(value: Any, expected: set[str], context: str, issues: list[str]) -> bool:
    if not isinstance(value, dict):
        issues.append(f"{context} must be an object")
        return False
    actual = set(value)
    if actual != expected:
        issues.append(
            f"{context} keys differ: missing={sorted(expected - actual)!r} "
            f"extra={sorted(actual - expected)!r}")
        return False
    return True


def validate_hashed_records(
    records: Any,
    expected: list[dict[str, Any]],
    root: Path,
    context: str,
    issues: list[str],
) -> None:
    expect(records == expected, f"{context} records differ", issues)
    if not isinstance(records, list):
        return
    for index, record in enumerate(records):
        if not isinstance(record, dict):
            continue
        path = safe_project_path(root, record.get("path"))
        if path is None or not path.is_file():
            issues.append(f"{context}[{index}] path is unsafe or missing")
            continue
        expect(path.stat().st_size == record.get("bytes"),
               f"{context}[{index}] byte count differs", issues)
        expect(sha256(path) == record.get("sha256"),
               f"{context}[{index}] SHA-256 differs", issues)


def evidence_state(contract: Any) -> str:
    if not isinstance(contract, dict):
        return "INVALID"
    evidence = contract.get("closure_evidence")
    if not isinstance(evidence, dict) or set(evidence) != set(EVIDENCE_KEYS):
        return "INVALID"
    statuses = [record.get("status") if isinstance(record, dict) else None
                for record in evidence.values()]
    if statuses and all(status == "PENDING" for status in statuses):
        return "PENDING"
    if statuses and all(status == "PASS" for status in statuses):
        return "COMPLETE"
    return "MIXED"


def validate_evidence_binding(
    record: Any,
    expected: dict[str, Any],
    root: Path,
    context: str,
    issues: list[str],
    require_current_bytes: bool = True,
) -> Path | None:
    if not exact_keys(record, {"status", "path", "bytes", "sha256"}, context, issues):
        return None
    expect(record == expected, f"{context} differs from the accepted evidence binding", issues)
    relative = record.get("path")
    path = safe_project_path(root, relative)
    if path is None or not path.is_file():
        issues.append(f"{context} path is unsafe or missing: {relative!r}")
        return None
    size = record.get("bytes")
    digest = record.get("sha256")
    expect(type(size) is int and size > 0, f"{context} byte count is invalid", issues)
    expect(isinstance(digest, str) and SHA256_RE.fullmatch(digest) is not None,
           f"{context} SHA-256 is invalid", issues)
    # Build products under Binaries are a mutable development workspace, not
    # retained Session 16 artifacts. Their accepted metadata remains exact in
    # the historical contract and is independently bound by the retained
    # Session 16 package/acceptance evidence. Later clean builds must not
    # rewrite that history or make project validation fail merely because a
    # newer session produced new binaries. Immutable reports, manifests, and
    # the protected profile continue to require an exact live byte match.
    if require_current_bytes:
        expect(path.stat().st_size == size, f"{context} byte count drifted", issues)
        expect(sha256(path) == digest, f"{context} SHA-256 drifted", issues)
    return path


def load_evidence_json(path: Path | None, context: str, issues: list[str]) -> dict[str, Any] | None:
    if path is None:
        return None
    try:
        # Unreal Automation emits UTF-8 BOM reports. Strip only that encoding
        # marker while retaining the duplicate-key/non-finite strict parser.
        value = strict_json_loads(path.read_bytes().decode("utf-8-sig"))
    except (OSError, StrictJsonError) as exc:
        issues.append(f"{context} is not strict JSON: {exc}")
        return None
    if not isinstance(value, dict):
        issues.append(f"{context} root must be an object")
        return None
    return value


def validate_automation_report(
    report: dict[str, Any] | None,
    expected_succeeded: int,
    context: str,
    issues: list[str],
    require_exact_session16_tests: bool,
) -> None:
    if report is None:
        return
    expect(report.get("succeeded") == expected_succeeded,
           f"{context} succeeded count differs", issues)
    expect(report.get("succeededWithWarnings") == 0,
           f"{context} has warning results", issues)
    expect(report.get("failed") == 0, f"{context} has failures", issues)
    expect(report.get("notRun") == 0, f"{context} has not-run tests", issues)
    expect(report.get("inProcess") == 0, f"{context} has in-process tests", issues)
    tests = report.get("tests")
    if not isinstance(tests, list):
        issues.append(f"{context}.tests must be an array")
        return
    expect(len(tests) == expected_succeeded, f"{context} test count differs", issues)
    paths = [item.get("fullTestPath") if isinstance(item, dict) else None
             for item in tests]
    expect(len(paths) == len(set(paths)), f"{context} contains duplicate test paths", issues)
    expect(all(isinstance(item, dict) and item.get("state") == "Success" and
               item.get("warnings") == 0 and item.get("errors") == 0
               for item in tests), f"{context} contains a non-clean test result", issues)
    if require_exact_session16_tests:
        expect(set(paths) == set(EXPECTED_AUTOMATION_TESTS),
               f"{context} does not contain the exact Session 16 focused tests", issues)
    else:
        expect(set(EXPECTED_AUTOMATION_TESTS).issubset(set(paths)),
               f"{context} omits a required Session 16 test", issues)


def validate_acceptance_report(
    report: dict[str, Any] | None,
    expected_mode: str,
    expected_run_id: str,
    context: str,
    issues: list[str],
) -> None:
    if report is None:
        return
    expect(report.get("schema") == "DiscGolfTour.Session16CorePlayabilityAcceptance.v1",
           f"{context} schema differs", issues)
    expect(report.get("result") == "PASS_CORE_PLAYABILITY_GATE_RELEASE_BLOCKED",
           f"{context} result differs", issues)
    expect(report.get("run_id") == expected_run_id, f"{context} run id differs", issues)
    expect(report.get("mode") == expected_mode, f"{context} mode differs", issues)
    expect(report.get("overall_status") == "Passed", f"{context} did not pass", issues)
    expect([report.get("passed_checks"), report.get("failed_checks"),
            report.get("blocked_checks"), report.get("not_run_checks")] == [44, 0, 0, 0],
           f"{context} check totals are not exactly 44/0/0/0", issues)
    expect(report.get("blocking_failures") == 0,
           f"{context} contains blocking failures", issues)
    expect(report.get("core_playability_gate_pass") is True,
           f"{context} does not pass the core playability gate", issues)
    expect(report.get("project_boundaries_unchanged") is True,
           f"{context} changed project boundaries", issues)
    expect(report.get("bounded_technical_evidence_only") is True,
           f"{context} is not bounded technical evidence", issues)
    expect(report.get("release_ready") is False and
           report.get("release_use_allowed") is False and
           report.get("production_readiness_approved") is False,
           f"{context} improperly claims production/release approval", issues)
    expect(report.get("release_blockers_inherited_from_session15") == EXPECTED_BLOCKERS,
           f"{context} inherited blocker set/order differs", issues)
    if expected_mode == "packaged":
        expect(report.get("package_unchanged") is True,
               f"{context} did not preserve the package", issues)

    expected_gate_summaries = {
        gate_name: {
            "status": "Passed", "passed": len(check_ids), "failed": 0,
            "blocked": 0, "not_run": 0, "total": len(check_ids),
        }
        for gate_name, check_ids in GATE_CHECKS.items()
    }
    expect(report.get("gate_summaries") == expected_gate_summaries,
           f"{context} gate summaries differ", issues)
    checks = report.get("checks")
    if isinstance(checks, list):
        expected_ids = [check_id for _, check_id, _, _ in CHECK_SPECS]
        expect([item.get("check_id") if isinstance(item, dict) else None
                for item in checks] == expected_ids,
               f"{context} check identities/order differ", issues)
        expect(all(isinstance(item, dict) and item.get("status") == "Passed" and
                   item.get("blocking") is True and item.get("failure_code") == "None"
                   for item in checks), f"{context} contains a non-passing check", issues)
    else:
        issues.append(f"{context}.checks must be an array")

    focused = report.get("focused_automation")
    if isinstance(focused, dict):
        expect(focused.get("sha256") ==
               EXPECTED_COMPLETE_EVIDENCE["focused_automation"]["sha256"],
               f"{context} focused automation hash differs", issues)
        expect([focused.get("succeeded"), focused.get("failed"), focused.get("not_run"),
                focused.get("succeeded_with_warnings"), focused.get("in_process")] ==
               [9, 0, 0, 0, 0],
               f"{context} focused automation totals differ", issues)
        expect(set(focused.get("test_paths", [])) == set(EXPECTED_AUTOMATION_TESTS),
               f"{context} focused automation paths differ", issues)
    else:
        issues.append(f"{context}.focused_automation must be an object")


def validate_package_manifest(
    manifest: dict[str, Any] | None,
    context: str,
    issues: list[str],
) -> None:
    if manifest is None:
        return
    expected_root = (
        "C:\\DGTour_Packages\\S16_CorePlayability_22f2a5c2-583e-4823-b8c4-0c02b49afb57\\Windows")
    expected_executable = expected_root + "\\DiscGolfTour\\Binaries\\Win64\\DiscGolfTour.exe"
    expect(manifest.get("schema") == "DiscGolfTour.Session16PackageManifest.v1",
           f"{context} schema differs", issues)
    expect(manifest.get("package_root") == expected_root,
           f"{context} package root differs", issues)
    expect(manifest.get("executable") == expected_executable,
           f"{context} executable differs", issues)
    expect(manifest.get("unchanged") is True and
           manifest.get("manifest_sha256_before") == manifest.get("manifest_sha256_after") and
           isinstance(manifest.get("manifest_sha256_before"), str) and
           SHA256_RE.fullmatch(manifest["manifest_sha256_before"]) is not None,
           f"{context} does not prove an unchanged package", issues)
    files = manifest.get("files")
    expect(isinstance(files, dict) and manifest.get("file_count") == 54 and len(files) == 54,
           f"{context} file inventory differs", issues)
    if isinstance(files, dict):
        expect(files.get("DiscGolfTour/Binaries/Win64/DiscGolfTour.exe") == {
            "bytes": EXPECTED_COMPLETE_EVIDENCE["game_development_build"]["bytes"],
            "sha256": EXPECTED_COMPLETE_EVIDENCE["game_development_build"]["sha256"],
        }, f"{context} game executable binding differs", issues)
    package_root = Path(expected_root)
    executable = Path(expected_executable)
    expect(package_root.is_dir(), f"{context} package root is missing", issues)
    expect(executable.is_file(), f"{context} executable is missing", issues)
    if executable.is_file():
        expect(executable.stat().st_size ==
               EXPECTED_COMPLETE_EVIDENCE["game_development_build"]["bytes"],
               f"{context} executable byte count drifted", issues)
        expect(sha256(executable) ==
               EXPECTED_COMPLETE_EVIDENCE["game_development_build"]["sha256"],
               f"{context} executable SHA-256 drifted", issues)


def validate_complete_evidence(evidence: dict[str, Any], root: Path, issues: list[str]) -> None:
    paths: dict[str, Path | None] = {}
    for key in EVIDENCE_KEYS:
        paths[key] = validate_evidence_binding(
            evidence.get(key), EXPECTED_COMPLETE_EVIDENCE[key], root,
            f"closure_evidence.{key}", issues,
            require_current_bytes=key not in {
                "editor_development_build", "game_development_build"})

    focused = load_evidence_json(paths["focused_automation"],
                                 "focused automation evidence", issues)
    validate_automation_report(focused, 9, "focused automation evidence", issues, True)
    full = load_evidence_json(paths["full_automation"], "full automation evidence", issues)
    validate_automation_report(full, 240, "full automation evidence", issues, False)
    editor = load_evidence_json(paths["editor_live_acceptance"],
                                "editor acceptance evidence", issues)
    validate_acceptance_report(
        editor, "editor", "d64ee8c4-e1a6-4b49-9d73-d9bb76c834e2",
        "editor acceptance evidence", issues)
    packaged = load_evidence_json(paths["packaged_live_acceptance"],
                                  "packaged acceptance evidence", issues)
    validate_acceptance_report(
        packaged, "packaged", "07bb2b2a-be53-4686-bf67-088158e3da5c",
        "packaged acceptance evidence", issues)
    manifest = load_evidence_json(paths["fresh_package"], "fresh package evidence", issues)
    validate_package_manifest(manifest, "fresh package evidence", issues)


def validate_contract(contract: Any, root: Path = ROOT) -> list[str]:
    issues: list[str] = []
    if not exact_keys(contract, EXPECTED_TOP_LEVEL_KEYS, "contract", issues):
        return issues

    state = evidence_state(contract)
    expect(state in {"PENDING", "COMPLETE"},
           "closure evidence must be uniformly pending or uniformly complete", issues)
    complete = state == "COMPLETE"

    expect(contract.get("schema") == "DiscGolfTour.Session16CorePlayabilityContract.v1",
           "schema differs", issues)
    expect(type(contract.get("schema_version")) is int and contract["schema_version"] == 1,
           "schema_version differs", issues)
    expect(contract.get("scope") == "SOURCE_SIDE_CORE_PLAYABILITY_PREPARATION_TECHNICAL_ONLY",
           "scope differs", issues)
    expect(contract.get("normal_status") ==
           (COMPLETE_NORMAL_STATUS if complete else PENDING_NORMAL_STATUS),
           "normal_status differs for the closure state", issues)
    expect(contract.get("closure_status") ==
           (COMPLETE_CLOSURE_STATUS if complete else PENDING_CLOSURE_STATUS),
           "closure_status differs for the evidence state", issues)
    expect(contract.get("release_status") ==
           (COMPLETE_RELEASE_STATUS if complete else PENDING_RELEASE_STATUS),
           "release_status differs for the closure state", issues)

    policy = contract.get("policy")
    expected_policy = {
        **EXPECTED_POLICY_BASE,
        "closure_evidence_complete": complete,
        "technical_closure_accepted": complete,
        "release_ready": False,
    }
    expect(policy == expected_policy, "policy differs from the fail-closed closure state", issues)

    validate_hashed_records(
        contract.get("buildkit_sources"), EXPECTED_SOURCES, root,
        "buildkit_sources", issues)

    source_gate_path = root / EXPECTED_SOURCES[0]["path"]
    try:
        source_gate = strict_json_load(source_gate_path)
    except (OSError, StrictJsonError) as exc:
        source_gate = None
        issues.append(f"cannot load source playability gate: {exc}")
    if isinstance(source_gate, dict):
        expect(source_gate.get("version") == 1, "source gate version differs", issues)
        expect(source_gate.get("policy") == "BLOCK_POLISH_IF_CORE_PLAYABILITY_FAILS",
               "source gate policy differs", issues)
        levels = source_gate.get("levels")
        observed: dict[str, list[str]] = {}
        observed_order: list[str] = []
        if isinstance(levels, list):
            for level in levels:
                if isinstance(level, dict) and isinstance(level.get("id"), str):
                    observed_order.append(level["id"])
                    observed[level["id"]] = level.get("checks")
                    expect(level.get("blocking") is True,
                           f"source gate {level['id']} is not blocking", issues)
        expect(observed_order == list(GATE_CHECKS), "source gate order differs", issues)
        expect(observed == GATE_CHECKS, "source gate checks differ", issues)

    gate = contract.get("gate_contract")
    expected_gate = {
        "source_schema_version": 1,
        "source_policy": "BLOCK_POLISH_IF_CORE_PLAYABILITY_FAILS",
        "gate_order": list(GATE_CHECKS),
        "gate_check_counts": {key: len(value) for key, value in GATE_CHECKS.items()},
        "timeouts_seconds": {
            "boot_game": 30, "course_load": 30, "disc_launches": 5,
            "disc_settles_or_holes_out": 30, "hole_completes": 180,
            "round_completes": 1800,
        },
        "blocking_failure_examples": [
            "disc never settles", "input remains in replay mode",
            "camera cannot return to gameplay",
            "basket completion never advances state", "score fails to update",
            "player cannot take the next throw",
            "save load produces an unusable player",
        ],
        "polish_rule": "Do not spend integration time on visual polish while Smoke or CoreLoop has blocking failures.",
    }
    expect(gate == expected_gate, "gate_contract differs from the v1.5 source gate", issues)

    expected_authorities = [
        {"authority_id": item[0], "authority": item[1], "paths": item[2]}
        for item in AUTHORITY_CATALOG
    ]
    authorities = contract.get("authority_catalog")
    expect(authorities == expected_authorities, "authority_catalog differs", issues)
    authority_ids = {item[0] for item in AUTHORITY_CATALOG}
    if isinstance(authorities, list):
        for authority in authorities:
            if not isinstance(authority, dict):
                continue
            for relative in authority.get("paths", []):
                path = safe_project_path(root, relative)
                expect(path is not None and path.is_file() and path.stat().st_size > 0,
                       f"authority path is unsafe, missing, or empty: {relative!r}", issues)

    expected_checks = [
        {
            "gate": gate_name,
            "check_id": check_id,
            "failure_code": failure_code,
            "authority_ids": list(authorities_for_check),
            "prepared_status": "PREPARED_SOURCE_ASSERTION",
            "closure_status": "PASS" if complete else "PENDING",
        }
        for gate_name, check_id, failure_code, authorities_for_check in CHECK_SPECS
    ]
    checks = contract.get("prepared_checks")
    expect(checks == expected_checks, "prepared_checks differ from the exact 44-check mapping", issues)
    if isinstance(checks, list):
        expect(len(checks) == 44, "prepared_checks must contain exactly 44 entries", issues)
        identities = [(item.get("gate"), item.get("check_id"))
                      for item in checks if isinstance(item, dict)]
        expect(len(identities) == len(set(identities)),
               "prepared_checks contain duplicate identities", issues)
        observed_by_gate = {
            name: [item.get("check_id") for item in checks
                   if isinstance(item, dict) and item.get("gate") == name]
            for name in GATE_CHECKS
        }
        expect(observed_by_gate == GATE_CHECKS,
               "prepared_checks do not preserve exact gate order", issues)
        used_authorities: set[str] = set()
        for item in checks:
            if not isinstance(item, dict):
                continue
            refs = item.get("authority_ids")
            if isinstance(refs, list):
                used_authorities.update(value for value in refs if isinstance(value, str))
                expect(bool(refs) and len(refs) == len(set(refs)),
                       f"{item.get('check_id')} authority mapping is empty or duplicated", issues)
                expect(all(value in authority_ids for value in refs),
                       f"{item.get('check_id')} references an unknown authority", issues)
        expect(used_authorities == authority_ids,
               "authority catalog contains unused or unmapped authorities", issues)

    automation = contract.get("focused_automation")
    expected_automation = {
        "prefix": "DiscGolfTour.Session16.",
        "expected_test_count": 9,
        "expected_tests": EXPECTED_AUTOMATION_TESTS,
        "status": "PASS" if complete else "PENDING",
        "evidence_path": (EXPECTED_COMPLETE_EVIDENCE["focused_automation"]["path"]
                          if complete else None),
        "passed": 9 if complete else 0,
        "failed": 0,
        "not_run": 0 if complete else 9,
    }
    expect(automation == expected_automation,
           "focused_automation differs from the closure state", issues)

    evidence = contract.get("closure_evidence")
    expected_evidence_keys = set(EVIDENCE_KEYS)
    if exact_keys(evidence, expected_evidence_keys, "closure_evidence", issues):
        if complete:
            expect(evidence == EXPECTED_COMPLETE_EVIDENCE,
                   "closure_evidence differs from the accepted complete bindings", issues)
        else:
            for key in EVIDENCE_KEYS:
                expect(evidence.get(key) == PENDING_EVIDENCE_RECORD,
                       f"closure_evidence.{key} must be an exact pending record", issues)

    continuity = contract.get("continuity")
    if exact_keys(continuity, {"frozen_files", "protected_profile"}, "continuity", issues):
        expected_frozen = [
            {"path": path, "bytes": size, "sha256": digest}
            for path, size, digest in EXPECTED_FROZEN_FILES
        ]
        validate_hashed_records(
            continuity.get("frozen_files"), expected_frozen, root,
            "continuity.frozen_files", issues)
        expected_profile = {
            "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
            "bytes": 5212,
            "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
            "writes_allowed": False,
            "must_remain_only_project_savegame_file": True,
        }
        profile = continuity.get("protected_profile")
        expect(profile == expected_profile, "protected_profile binding differs", issues)
        if isinstance(profile, dict):
            profile_path = safe_project_path(root, profile.get("path"))
            expect(profile_path is not None and profile_path.is_file(),
                   "protected profile is missing", issues)
            if profile_path is not None and profile_path.is_file():
                expect(profile_path.stat().st_size == 5212,
                       "protected profile byte count differs", issues)
                expect(sha256(profile_path) == expected_profile["sha256"],
                       "protected profile SHA-256 differs", issues)
            save_root = root / "Saved/SaveGames"
            save_files = sorted(path for path in save_root.iterdir() if path.is_file()) \
                if save_root.is_dir() else []
            expect(save_files == [profile_path] if profile_path is not None else False,
                   "project SaveGames must contain only the protected profile", issues)

    expect(contract.get("session16_development_gate_blocker") ==
           (None if complete else SESSION16_BLOCKER),
           "session16_development_gate_blocker differs for the closure state", issues)
    expect(contract.get("release_blockers") == EXPECTED_BLOCKERS,
           "release_blockers differ from the exact ordered Session 9-16 set", issues)
    expected_exit = {
        "status": ("TECHNICAL_CLOSURE_ACCEPTED_RELEASE_BLOCKED" if complete else
                   "SOURCE_CONTRACT_PREPARED_CLOSURE_PENDING"),
        "prepared_source_contract_complete": True,
        "runtime_closure_complete": complete,
        "packaged_closure_complete": complete,
        "production_readiness_approved": False,
        "release_ready": False,
    }
    expect(contract.get("exit_gate") == expected_exit,
           "exit_gate differs from the closure state", issues)
    if complete and isinstance(evidence, dict) and not issues:
        validate_complete_evidence(evidence, root, issues)
    return issues


def contract_for_state(canonical: dict[str, Any], complete: bool) -> dict[str, Any]:
    value = copy.deepcopy(canonical)
    value["normal_status"] = COMPLETE_NORMAL_STATUS if complete else PENDING_NORMAL_STATUS
    value["closure_status"] = COMPLETE_CLOSURE_STATUS if complete else PENDING_CLOSURE_STATUS
    value["release_status"] = COMPLETE_RELEASE_STATUS if complete else PENDING_RELEASE_STATUS
    value["policy"] = {
        **EXPECTED_POLICY_BASE,
        "closure_evidence_complete": complete,
        "technical_closure_accepted": complete,
        "release_ready": False,
    }
    for check in value.get("prepared_checks", []):
        if isinstance(check, dict):
            check["closure_status"] = "PASS" if complete else "PENDING"
    value["focused_automation"].update({
        "status": "PASS" if complete else "PENDING",
        "evidence_path": (EXPECTED_COMPLETE_EVIDENCE["focused_automation"]["path"]
                          if complete else None),
        "passed": 9 if complete else 0,
        "failed": 0,
        "not_run": 0 if complete else 9,
    })
    value["closure_evidence"] = (copy.deepcopy(EXPECTED_COMPLETE_EVIDENCE) if complete else
                                 {key: copy.deepcopy(PENDING_EVIDENCE_RECORD)
                                  for key in EVIDENCE_KEYS})
    value["session16_development_gate_blocker"] = None if complete else SESSION16_BLOCKER
    value["exit_gate"] = {
        "status": ("TECHNICAL_CLOSURE_ACCEPTED_RELEASE_BLOCKED" if complete else
                   "SOURCE_CONTRACT_PREPARED_CLOSURE_PENDING"),
        "prepared_source_contract_complete": True,
        "runtime_closure_complete": complete,
        "packaged_closure_complete": complete,
        "production_readiness_approved": False,
        "release_ready": False,
    }
    return value


def run_self_test(canonical: Any) -> tuple[int, int, list[str]]:
    failures: list[str] = []
    total = 0

    baseline = validate_contract(canonical)
    total += 1
    if baseline:
        failures.append(f"baseline: {baseline}")

    pending = contract_for_state(canonical, False)
    total += 1
    pending_issues = validate_contract(pending)
    if pending_issues or evidence_state(pending) != "PENDING":
        failures.append(f"pending lifecycle baseline: {pending_issues}")

    complete = contract_for_state(canonical, True)
    total += 1
    complete_issues = validate_contract(complete)
    if complete_issues or evidence_state(complete) != "COMPLETE":
        failures.append(f"complete lifecycle baseline: {complete_issues}")

    mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("schema", lambda value: value.__setitem__("schema", "wrong")),
        ("schema-version-type", lambda value: value.__setitem__("schema_version", True)),
        ("scope", lambda value: value.__setitem__("scope", "release")),
        ("normal-status", lambda value: value.__setitem__("normal_status", "PASS")),
        ("closure-status", lambda value: value.__setitem__("closure_status", "COMPLETE")),
        ("release-status", lambda value: value.__setitem__("release_status", "READY")),
        ("extra-top-key", lambda value: value.__setitem__("unexpected", 1)),
        ("missing-top-key", lambda value: value.pop("exit_gate")),
        ("source-removed", lambda value: value["buildkit_sources"].pop()),
        ("source-hash", lambda value: value["buildkit_sources"][0].__setitem__("sha256", "0" * 64)),
        ("source-path", lambda value: value["buildkit_sources"][0].__setitem__("path", "../escape")),
        ("gate-order", lambda value: value["gate_contract"]["gate_order"].reverse()),
        ("gate-count", lambda value: value["gate_contract"]["gate_check_counts"].__setitem__("Smoke", 10)),
        ("timeout", lambda value: value["gate_contract"]["timeouts_seconds"].__setitem__("boot_game", 31)),
        ("failure-example", lambda value: value["gate_contract"]["blocking_failure_examples"].pop()),
        ("polish-rule", lambda value: value["gate_contract"].__setitem__("polish_rule", "polish first")),
        ("authority-removed", lambda value: value["authority_catalog"].pop()),
        ("authority-reordered", lambda value: value["authority_catalog"].reverse()),
        ("authority-path", lambda value: value["authority_catalog"][0]["paths"].append("../escape")),
        ("automation-prefix", lambda value: value["focused_automation"].__setitem__("prefix", "DiscGolfTour.")),
        ("automation-count", lambda value: value["focused_automation"].__setitem__("expected_test_count", 7)),
        ("automation-test", lambda value: value["focused_automation"]["expected_tests"].pop()),
        ("automation-pass-claim", lambda value: value["focused_automation"].update({"status": "PASS", "passed": 9, "not_run": 0})),
        ("closure-pass-claim", lambda value: value["closure_evidence"]["editor_development_build"].__setitem__("status", "PASS")),
        ("check-pass-claim", lambda value: value["prepared_checks"][0].__setitem__("closure_status", "PASS")),
        ("frozen-removed", lambda value: value["continuity"]["frozen_files"].pop()),
        ("frozen-hash", lambda value: value["continuity"]["frozen_files"][0].__setitem__("sha256", "0" * 64)),
        ("profile-bytes", lambda value: value["continuity"]["protected_profile"].__setitem__("bytes", 5213)),
        ("profile-write", lambda value: value["continuity"]["protected_profile"].__setitem__("writes_allowed", True)),
        ("session-blocker", lambda value: value.__setitem__("session16_development_gate_blocker", "NONE")),
        ("blocker-removed", lambda value: value["release_blockers"].pop()),
        ("blocker-order", lambda value: value["release_blockers"].reverse()),
        ("exit-runtime", lambda value: value["exit_gate"].__setitem__("runtime_closure_complete", True)),
        ("exit-package", lambda value: value["exit_gate"].__setitem__("packaged_closure_complete", True)),
        ("exit-release", lambda value: value["exit_gate"].__setitem__("release_ready", True)),
    ]
    for key in [*EXPECTED_POLICY_BASE, "closure_evidence_complete",
                "technical_closure_accepted", "release_ready"]:
        def mutate_policy(value: dict[str, Any], field: str = key) -> None:
            original = value["policy"][field]
            value["policy"][field] = not original if isinstance(original, bool) else original + 1
        mutations.append((f"policy-{key}", mutate_policy))
    for index in range(len(CHECK_SPECS)):
        def mutate_check_code(value: dict[str, Any], item_index: int = index) -> None:
            value["prepared_checks"][item_index]["failure_code"] = "UnexpectedError"
        def mutate_check_authority(value: dict[str, Any], item_index: int = index) -> None:
            current = value["prepared_checks"][item_index]["authority_ids"]
            value["prepared_checks"][item_index]["authority_ids"] = (
                ["player_profile"] if current == ["runtime_boot"] else ["runtime_boot"])
        mutations.append((f"check-{index}-failure", mutate_check_code))
        mutations.append((f"check-{index}-authority", mutate_check_authority))
    for evidence_key in EVIDENCE_KEYS:
        def mutate_evidence(value: dict[str, Any], field: str = evidence_key) -> None:
            value["closure_evidence"][field]["path"] = "Saved/Fake.json"
        mutations.append((f"evidence-{evidence_key}", mutate_evidence))

    for name, mutate in mutations:
        total += 1
        candidate = copy.deepcopy(pending)
        try:
            mutate(candidate)
            if not validate_contract(candidate):
                failures.append(f"mutation accepted: {name}")
        except Exception as exc:  # mutation harness failures are self-test failures
            failures.append(f"mutation raised unexpectedly: {name}: {exc}")

    complete_mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("complete-mixed-evidence", lambda value: value["closure_evidence"]
         ["editor_development_build"].__setitem__("status", "PENDING")),
        ("complete-check-regression", lambda value: value["prepared_checks"][0]
         .__setitem__("closure_status", "PENDING")),
        ("complete-automation-regression", lambda value: value["focused_automation"]
         .__setitem__("status", "PENDING")),
        ("complete-blocker-restored", lambda value: value.__setitem__(
            "session16_development_gate_blocker", SESSION16_BLOCKER)),
        ("complete-runtime-regression", lambda value: value["exit_gate"]
         .__setitem__("runtime_closure_complete", False)),
        ("complete-release-claim", lambda value: value["exit_gate"]
         .__setitem__("release_ready", True)),
    ]
    for name, mutate in complete_mutations:
        total += 1
        candidate = copy.deepcopy(complete)
        try:
            mutate(candidate)
            if not validate_contract(candidate):
                failures.append(f"complete mutation accepted: {name}")
        except Exception as exc:
            failures.append(f"complete mutation raised unexpectedly: {name}: {exc}")

    parser_cases = [
        ("duplicate-key", '{"a":1,"a":2}'),
        ("nan", '{"a":NaN}'),
        ("infinity", '{"a":Infinity}'),
        ("negative-infinity", '{"a":-Infinity}'),
        ("trailing-comma", '{"a":1,}'),
        ("unterminated", '{"a":'),
    ]
    for name, payload in parser_cases:
        total += 1
        try:
            strict_json_loads(payload)
            failures.append(f"strict parser accepted: {name}")
        except StrictJsonError:
            pass
    return total - len(failures), total, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-prepared", action="store_true")
    parser.add_argument("--require-closure", action="store_true")
    parser.add_argument("--require-release-ready", action="store_true")
    args = parser.parse_args()

    contract_path = args.contract.resolve()
    try:
        contract = strict_json_load(contract_path)
    except (OSError, StrictJsonError) as exc:
        print(f"FAIL: cannot load strict contract: {exc}")
        return 1

    issues = validate_contract(contract)
    if issues:
        print("FAIL_SESSION16_CORE_PLAYABILITY_CONTRACT")
        for issue in issues:
            print(f"- {issue}")
        return 1

    if args.self_test:
        passed, total, failures = run_self_test(contract)
        if failures:
            print(f"FAIL_SESSION16_SELF_TEST: {passed}/{total}")
            for failure in failures:
                print(f"- {failure}")
            return 1
        print(f"PASS_SESSION16_SELF_TEST: {passed}/{total}")

    state = evidence_state(contract)
    status = COMPLETE_NORMAL_STATUS if state == "COMPLETE" else PENDING_NORMAL_STATUS
    print(
        f"{status}: prepared_checks=44 gates=11/16/7/10 "
        f"focused_tests=9 closure_evidence={state.lower()} "
        f"blockers={len(EXPECTED_BLOCKERS)}")
    if args.require_closure:
        if state != "COMPLETE":
            print("BLOCKED: Session 16 runtime/package closure evidence is pending")
            return 2
        print("PASS_REQUIRE_CLOSURE: Session 16 runtime/package technical evidence is complete")
    if args.require_release_ready:
        print(f"BLOCKED: release gate has {len(EXPECTED_BLOCKERS)} ordered blockers")
        return 2
    if args.require_prepared:
        print("PASS_REQUIRE_PREPARED: exact 44-check source contract is complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
