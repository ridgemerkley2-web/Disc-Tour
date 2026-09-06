#!/usr/bin/env python3
"""Fail-closed Session 12 presentation contract validator.

The validator is read-only unless --report is supplied. Normal mode requires
the exact hash-pinned all-capabilities technical closure while release remains
blocked; --require-release must fail with exit 2 until all eleven blockers close.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = Path("Config/DG_Session12PresentationContract.json")
REPORT_PATH = Path("Saved/PresentationReports/DG_Session12PresentationAudit.json")

NORMAL_STATUS = "PASS_SESSION12_PRESENTATION_CONTRACT_RELEASE_BLOCKED"
RELEASE_STATUS = "BLOCKED_PENDING_SESSION9_TO_SESSION12_PRODUCTION_CLOSURE"
SESSION12_BLOCKER = "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING"

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
    SESSION12_BLOCKER,
]

FROZEN_FILES = [
    ("Config/DG_BrandLicenseContract.json", 9267,
     "3EF3CBAD7E66D89D41CFB8C8FF59516484D8258995CAFAF28F8D58C4D8EB2001"),
    ("Scripts/validate_dg_session9_brand_license.py", 74292,
     "69867A9CE89D8F7720E2A124BEEB09CE4FFDC02D6F95BED8A11D281EF0EB7550"),
    ("Config/DG_Session10EnvironmentContract.json", 19976,
     "05931329CE877E80D4D36691D9091BBD6DDACE8F3F40612C5BE11B47744CDDC5"),
    ("Scripts/validate_dg_session10_environment.py", 76608,
     "13F3AECBB569466B6E3E8C49E0B3FA599EF57B13CE23604602CDC3158809112A"),
    ("Config/DG_Session11EquipmentThrowLabContract.json", 20213,
     "5306B7C1354775A896332474711B5E6E92C44B44F7EFDE0D9353EB4E9DBCA79D"),
    ("Scripts/validate_dg_session11_equipment_throw_lab.py", 36254,
     "E9D262E61122176EB4515D4AA2A4854D6354EADA2871B3A01299B17B13179D8A"),
]

PROTECTED_PROFILE = (
    "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
    5212,
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
)

DONOR_BINDINGS = [
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/12_CAMERA_REPLAY_INPUT_UI_AUDIO.md", 2037,
     "69D596AD0FB2998CD48C3D1DB99371A44DF765CF81705B8E65FC6658E9767E7D"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/38_CAMERA_REPLAY_TRACER.md", 1477,
     "CF0ADED53E2ADA05B72A76F4BF7798ECD35A381A9BB524FF4DB0E739F48D9B60"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/39_INPUT_COMMONUI_ACCESSIBILITY.md", 1490,
     "6D632DA6F534E6F36F175C3768E9A36703035B9F4AF854A8739FA5438CD02430"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/41_AUDIO_METASOUNDS.md", 1344,
     "A54750E99CDD730CC7E4C06BF240BB20AAC9E2517C40A5E42ED0239368861A66"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/47_OPTIONAL_ENGINE_FEATURE_SAFETY.md", 946,
     "FE0597A50FCBBC5C12CADD2E83EE28E37806E7B1CA479C745C8D9AC466B5D076"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_CameraReplaySchema.json", 558,
     "64BA9C8E4C12519F7F920263600E6B9328C08AABE74F85442CA910CBC7469E80"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_InputActionSchema.json", 1267,
     "4CCE36862032D770F7DFCBF1FCC3796CA5D7174E14B646FE996EFCA5984788B9"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_AudioParameterContract.json", 530,
     "631BE237BC59891CAA6C29B66BDA669695BF651648125DFF61B8B1F0822D7A88"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_AccessibilityDefaults.json", 315,
     "EA476E04CF54A7EAE089C98401C04FC1A586AFDB82E702A8DDA0148D74B244BD"),
    ("_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_OptionalEngineFeaturePolicy.json", 1477,
     "91A88850B5FB8752E81F462084C7D62038B8E3B4EF0E4098DD85370AE0EC761F"),
]

REQUIRED_FILES = [
    "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.h",
    "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp",
    "Source/DiscGolfTour/DiscGolfCameraViewContract.h",
    "Source/DiscGolfTour/DiscGolfCameraViewContract.cpp",
    "Source/DiscGolfTour/DiscGolfPresentationMath.h",
    "Source/DiscGolfTour/DiscGolfPresentationMath.cpp",
    "Source/DiscGolfTour/DiscReplayActor.h",
    "Source/DiscGolfTour/DiscReplayActor.cpp",
    "Source/DiscGolfTour/DiscGolfInputRoutePolicy.h",
    "Source/DiscGolfTour/DiscGolfInputConfig.h",
    "Source/DiscGolfTour/DiscGolfInputConfig.cpp",
    "Source/DiscGolfTour/DiscGolfPlayerExperience.h",
    "Source/DiscGolfTour/DiscGolfPlayerExperience.cpp",
    "Source/DiscGolfTour/ThrowControllerComponent.h",
    "Source/DiscGolfTour/ThrowControllerComponent.cpp",
    "Source/DiscGolfTour/DiscGolfTourPlayerController.h",
    "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp",
    "Source/DiscGolfTour/DiscGolferPawn.h",
    "Source/DiscGolfTour/DiscGolferPawn.cpp",
    "Source/DiscGolfTour/DiscGolfHUD.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession12AudioRouterTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession12CameraReplayTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession12InputUiTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSession12PresentationContractTests.cpp",
    "Config/DG_Session12PresentationContract.json",
    "Scripts/validate_dg_session12_presentation.py",
    "Docs/DG_SESSION12_PRESENTATION_AUDIT.md",
]

EXPECTED_CAPABILITIES = [
    {"id": "project_owned_audio_router", "status": "TECHNICAL_PASS"},
    {"id": "semantic_audio_router_runtime_wiring", "status": "TECHNICAL_PASS"},
    {"id": "project_owned_camera_lifecycle", "status": "TECHNICAL_PASS"},
    {"id": "immutable_sample_replay_presentation", "status": "TECHNICAL_PASS"},
    {"id": "explicit_input_context_and_remap_flow", "status": "TECHNICAL_PASS"},
    {"id": "read_only_ui_and_accessibility_presentation", "status": "TECHNICAL_PASS"},
    {"id": "live_editor_and_fresh_packaged_closure", "status": "TECHNICAL_PASS"},
]

EXPECTED_AUDIO_TESTS = [
    "DiscGolfTour.Session12.Audio.SemanticProviderMapping",
    "DiscGolfTour.Session12.Audio.PayloadSanitization",
    "DiscGolfTour.Session12.Audio.SilentFallbackAndDedupe",
    "DiscGolfTour.Session12.Audio.RouteTraceBound",
    "DiscGolfTour.Session12.Audio.InvalidEventIsAtomic",
]

EXPECTED_EXIT_GATE = {
    "status": "TECHNICAL_EXIT_ACCEPTED_PRODUCTION_READINESS_PENDING",
    "technical_audio_router_complete": True,
    "all_capabilities_technical_pass": True,
    "live_editor_game_pass": True,
    "fresh_package_pass": True,
    "packaged_live_pass": True,
    "production_readiness_approved": False,
}

EXPECTED_CLOSURE_EVIDENCE = {
    "editor_development_build": {
        "status": "PASS",
        "path": "Saved/Logs/Session12EditorBuildClosure_8f79bfcd-5783-4e2f-9bd2-fcf14359b4a5.log",
        "bytes": 9511,
        "sha256": "D31838DF690A592032A8E6A0C65C6FA7D5292BCB8F814DDEF803DD9937DBA32E",
    },
    "game_development_build": {
        "status": "PASS",
        "path": "Saved/Logs/Session12GameBuildClosure_8f79bfcd-5783-4e2f-9bd2-fcf14359b4a5.log",
        "bytes": 8996,
        "sha256": "1EFE34ECE2A7EFA3ABE24D98086AFBF63FBB241367C3FB7D5DB6A47F0C81BD3C",
    },
    "focused_automation": {
        "status": "PASS",
        "path": "Saved/Automation/Session12FocusedClosure_db263248-c975-4058-be62-87669bc17d00/index.json",
        "bytes": 7115,
        "sha256": "08B086F3EA66596825CC408A5D8772890B7E9AD6A93303E91C1B31A8FFEB8301",
        "succeeded": 16,
        "failed": 0,
        "not_run": 0,
    },
    "full_automation": {
        "status": "PASS",
        "path": "Saved/Automation/Session12FullClosure_67b0de32-7266-4b9d-ab18-539ac11fc267/index.json",
        "bytes": 72160,
        "sha256": "B5B32C9B4F7A62ADAC4148D19FB7620D05FE569936D0851DC591E3D60C7E3BB0",
        "succeeded": 180,
        "failed": 0,
        "not_run": 0,
    },
    "post_fix_camera_automation": {
        "status": "PASS",
        "path": "Saved/Automation/Session12CameraClosure_f2310155-9c8b-43ff-bd27-236d255a903c/index.json",
        "bytes": 3144,
        "sha256": "886D93BA381BF1099C6DDEB5524CD73A50C1FD1B95164B21BE512EDF485EACC8",
        "succeeded": 6,
        "failed": 0,
        "not_run": 0,
    },
    "live_editor_game": {
        "status": "PASS",
        "path": "Saved/Logs/Session12LiveClosure_d9a4b071-53e3-4804-b851-b0f8d98ca388.log",
        "bytes": 213848,
        "sha256": "44C8FC1AF72E3CB987F86941A39592ABA20AC600DD8AC59A4A641F79C5A1D57D",
        "solver_samples": 1968,
        "bounded_actual_samples": 492,
        "replay_progress": 0.168,
        "audio_routes": 7,
    },
    "fresh_package": {
        "status": "PASS",
        "archive_path": "C:/DGTour_Packages/S12_Presentation_Closure_cdb72579-35bf-4463-b3db-1dac5cfd7d3d",
        "path": "Saved/Logs/Session12PackageClosure_cdb72579-35bf-4463-b3db-1dac5cfd7d3d.log",
        "bytes": 319246,
        "sha256": "D097EE5AEA97348252742F24AB2D0C06B80DB824EAB7964A7C3CAF2524A5B940",
        "cooked_packages": 984,
        "incrementally_skipped": 0,
    },
    "packaged_live": {
        "status": "PASS",
        "path": "Saved/Logs/Session12PackagedClosureFinal_0c92b43a-266d-4c7f-920c-232b64c8befd.log",
        "bytes": 76872,
        "sha256": "7DFB350A2A45AB73857FFC0640DB51D7C1631A18FDAE837DCD60F99D8306DC24",
        "solver_samples": 1968,
        "bounded_actual_samples": 492,
        "replay_progress": 0.168,
        "audio_routes": 7,
    },
    "protected_profile_after_all_runs": {
        "status": "PASS",
        "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav",
        "bytes": 5212,
        "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
    },
    "reference_flight": {
        "status": "PASS",
        "preset": "Apex RHBH 82% / 3 deg hyzer",
        "carry_m": 84.5,
        "peak_m": 9.9,
        "flight_s": 7.42,
    },
}

FILE_BACKED_EVIDENCE = (
    "editor_development_build",
    "game_development_build",
    "focused_automation",
    "full_automation",
    "post_fix_camera_automation",
    "live_editor_game",
    "fresh_package",
    "packaged_live",
    "protected_profile_after_all_runs",
)

FORBIDDEN_COMPONENTS = [
    "UDiscGolfCameraDirectorComponent",
    "UDiscGolfReplayBridgeComponent",
    "UDiscGolfShotReplayComponent",
    "UDiscGolfShotTracerComponent",
    "UDiscGolfAudioBridgeComponent",
    "UDiscGolfSettingsSubsystem",
    "UDiscGolfUIFlowSubsystem",
]

FORBIDDEN_HARD_DEPENDENCIES = [
    '"GameplayCameras"',
    '"CommonUI"',
    '"MetaSoundBuilder"',
    '"MetasoundFrontend"',
    "StartRecordingReplay(",
    "PlayReplay(",
]

ROUTER_HEADER_TOKENS = [
    "UDiscGolfPresentationAudioRouterComponent",
    "FDGAudioEventPayload",
    "RoutePresentationAudioEvent",
    "MaximumRouteTrace = 64",
    "OutputGain01",
]

ROUTER_CPP_TOKENS = [
    "PrimaryComponentTick.bCanEverTick = false",
    "RequestAsyncLoad",
    "PlaySoundAtLocation",
    "SILENT ASSET FALLBACK",
    "Event.DedupeKey()",
    "Route.Payload.Intensity01 * Route.OutputGain01",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("contract root must be an object")
    return value


def require(condition: bool, message: str, issues: list[str]) -> None:
    if not condition:
        issues.append(message)


def validate_document(contract: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    require(contract.get("schema") == "dg.session12.presentation.contract",
            "unexpected schema", issues)
    require(contract.get("schema_version") == 1, "schema_version must be 1", issues)
    require(contract.get("normal_status") == NORMAL_STATUS, "normal_status mismatch", issues)
    require(contract.get("release_status") == RELEASE_STATUS, "release_status mismatch", issues)
    require(contract.get("releaseBlocked") is True, "releaseBlocked must be true", issues)
    require(contract.get("release_ready") is False, "release_ready must be false", issues)
    require(contract.get("release_use_allowed") is False,
            "release_use_allowed must be false", issues)
    require(contract.get("implementation_status") ==
            "TECHNICAL_RUNTIME_IMPLEMENTATION_ACCEPTED_PRODUCTION_READINESS_PENDING",
            "accepted technical implementation status is required", issues)
    require(contract.get("feature_complete") is True,
            "feature_complete must be true after technical closure", issues)
    require(contract.get("release_blockers") == EXPECTED_BLOCKERS,
            "release blockers must equal the ordered eleven-blocker set", issues)
    require(contract.get("session12_release_blocker") == SESSION12_BLOCKER,
            "Session 12 blocker mismatch", issues)

    authority = contract.get("authority", {})
    for key in ("camera", "replay", "input", "ui", "semantic_audio", "audio_router",
                "settings_and_save", "throw", "flight", "rules_scoring_course"):
        require(isinstance(authority.get(key), str) and authority[key],
                f"missing authority.{key}", issues)
    require(contract.get("forbidden_parallel_authorities") == FORBIDDEN_COMPONENTS,
            "forbidden component list mismatch", issues)

    optional = contract.get("optional_engine_policy", {})
    for key in ("hard_gameplay_cameras_dependency", "hard_common_ui_dependency",
                "engine_replay_is_authoritative", "metasound_builder_runtime_dependency"):
        require(optional.get(key) is False, f"{key} must be false", issues)
    for key in ("telemetry_replay_remains_default", "normal_umg_and_input_fallback_required",
                "player_camera_fallback_required", "normal_sound_or_silent_fallback_required"):
        require(optional.get(key) is True, f"{key} must be true", issues)

    router = contract.get("audio_router_contract", {})
    expected_router_values = {
        "owner": "UDiscGolfPresentationAudioRouterComponent",
        "semantic_trigger_owner": "DiscGolfPresentationAudio",
        "plugin_value_dto": "FDGAudioEventPayload",
        "plugin_audio_component_activated": False,
        "component_tick": False,
        "maximum_route_trace": 64,
        "duplicate_semantic_event_suppression": True,
        "synchronous_asset_load_during_routing": False,
        "fallback_preload": "ASYNC_ONLY",
        "missing_asset_result": "SUCCESSFUL_SILENT_ROUTE_WITH_EXPLICIT_STATUS",
        "blueprint_provider_event": "RoutePresentationAudioEvent",
        "semantic_id_preserved_outside_lossy_plugin_dto": True,
        "pitch_preserved_outside_lossy_plugin_dto": True,
        "gameplay_mutation_allowed": False,
        "production_audio_assets_present": False,
    }
    for key, expected in expected_router_values.items():
        require(router.get(key) == expected, f"audio_router_contract.{key} mismatch", issues)

    require(contract.get("required_files") == REQUIRED_FILES,
            "required_files mismatch", issues)
    require(contract.get("implementation_capabilities") == EXPECTED_CAPABILITIES,
            "all seven ordered implementation capabilities must be TECHNICAL_PASS", issues)

    test_contract = contract.get("test_contract", {})
    require(test_contract.get("focused_filter") == "DiscGolfTour.Session12.*",
            "focused Session 12 automation filter mismatch", issues)
    require(test_contract.get("audio_tests") == EXPECTED_AUDIO_TESTS,
            "focused audio automation inventory mismatch", issues)
    require(test_contract.get("contract_test") ==
            "DiscGolfTour.Session12.Contract.FailClosed",
            "fail-closed automation identity mismatch", issues)
    require(test_contract.get("full_filter") == "DiscGolfTour.",
            "full automation filter mismatch", issues)
    for key in (
            "editor_and_game_development_builds_required",
            "reference_flight_required",
            "live_editor_game_required",
            "fresh_non_iterative_package_required",
            "packaged_live_required",
            "protected_profile_hash_required_after_all_runs"):
        require(test_contract.get(key) is True,
                f"test_contract.{key} must be true", issues)

    require(contract.get("closure_evidence") == EXPECTED_CLOSURE_EVIDENCE,
            "closure evidence must equal the exact hash-pinned build/automation/runtime/package set",
            issues)
    require(contract.get("exit_gate") == EXPECTED_EXIT_GATE,
            "technical exit/live/package/packaged gate mismatch", issues)

    validation = contract.get("validation_contract", {})
    require(validation.get("validator_path") ==
            "Scripts/validate_dg_session12_presentation.py",
            "validator path mismatch", issues)
    require(validation.get("report_path") ==
            "Saved/PresentationReports/DG_Session12PresentationAudit.json",
            "validator report path mismatch", issues)
    require(validation.get("normal_mode_exit_code_when_technical_state_is_valid") == 0,
            "normal-mode exit code must be 0", issues)
    require(validation.get("expected_release_blocker_count") == 11,
            "validator blocker count must be 11", issues)
    require(validation.get("release_required_exit_code") == 2,
            "release-required exit code must be 2", issues)
    require(validation.get("normal_mode_requires_release_blocked") is True,
            "normal mode must require release blocked", issues)
    require(validation.get("pending_integration_is_reported_not_hidden") is False,
            "pending integration marker must be false after closure", issues)
    return issues


def validate_bound_files(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    continuity_entries = contract.get("continuity", {}).get("frozen_files")
    expected_entries = [
        {"path": path, "bytes": size, "sha256": digest}
        for path, size, digest in FROZEN_FILES
    ]
    require(continuity_entries == expected_entries, "contract frozen-file table mismatch", issues)
    donor_entries = contract.get("donor_reference", {}).get("bindings")
    expected_donors = [
        {"path": path, "bytes": size, "sha256": digest}
        for path, size, digest in DONOR_BINDINGS
    ]
    require(donor_entries == expected_donors, "contract donor binding table mismatch", issues)

    for path_text, expected_size, expected_hash in FROZEN_FILES + DONOR_BINDINGS + [PROTECTED_PROFILE]:
        path = root / path_text
        if not path.is_file():
            issues.append(f"missing frozen path: {path_text}")
            continue
        require(path.stat().st_size == expected_size,
                f"byte mismatch: {path_text}", issues)
        require(sha256(path) == expected_hash,
                f"hash mismatch: {path_text}", issues)

    profile = contract.get("continuity", {}).get("protected_profile", {})
    require(profile == {
        "path": PROTECTED_PROFILE[0],
        "bytes": PROTECTED_PROFILE[1],
        "sha256": PROTECTED_PROFILE[2],
        "writes_allowed": False,
    }, "protected profile contract mismatch", issues)

    evidence = contract.get("closure_evidence", {})
    if isinstance(evidence, dict):
        for evidence_id in FILE_BACKED_EVIDENCE:
            entry = evidence.get(evidence_id, {})
            if not isinstance(entry, dict):
                issues.append(f"invalid closure evidence object: {evidence_id}")
                continue
            path_text = entry.get("path")
            if not isinstance(path_text, str) or not path_text:
                issues.append(f"missing closure evidence path: {evidence_id}")
                continue
            path = root / path_text
            if not path.is_file():
                issues.append(f"missing closure evidence file: {path_text}")
                continue
            require(path.stat().st_size == entry.get("bytes"),
                    f"closure evidence byte mismatch: {evidence_id}", issues)
            require(sha256(path) == entry.get("sha256"),
                    f"closure evidence hash mismatch: {evidence_id}", issues)

        fresh_package = evidence.get("fresh_package", {})
        archive_path = fresh_package.get("archive_path") if isinstance(fresh_package, dict) else None
        require(isinstance(archive_path, str) and Path(archive_path).is_dir(),
                "fresh package archive directory is missing", issues)


def validate_sources(root: Path, contract: dict[str, Any], issues: list[str]) -> None:
    for path_text in REQUIRED_FILES:
        require((root / path_text).is_file(), f"missing required file: {path_text}", issues)

    header_path = root / REQUIRED_FILES[0]
    cpp_path = root / REQUIRED_FILES[1]
    if header_path.is_file():
        header = header_path.read_text(encoding="utf-8")
        for token in ROUTER_HEADER_TOKENS:
            require(token in header, f"router header missing token: {token}", issues)
    if cpp_path.is_file():
        cpp = cpp_path.read_text(encoding="utf-8")
        for token in ROUTER_CPP_TOKENS:
            require(token in cpp, f"router cpp missing token: {token}", issues)
        require("LoadSynchronous" not in cpp,
                "router must not synchronously load fallback assets", issues)

    project_source = root / "Source/DiscGolfTour"
    active_files = [
        path for path in project_source.rglob("*")
        if path.is_file() and path.suffix.lower() in {".h", ".cpp"}
        and "Tests" not in path.parts
    ]
    for path in active_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        relative = path.relative_to(root).as_posix()
        for forbidden in FORBIDDEN_COMPONENTS:
            require(forbidden not in text,
                    f"forbidden plugin component wired in {relative}: {forbidden}", issues)

    dependency_files = [root / "DiscGolfTour.uproject", root / "Source/DiscGolfTour/DiscGolfTour.Build.cs"]
    dependency_text = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in dependency_files if path.is_file()
    )
    for forbidden in FORBIDDEN_HARD_DEPENDENCIES[:4]:
        require(forbidden not in dependency_text,
                f"forbidden hard dependency found: {forbidden}", issues)
    all_source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="replace") for path in active_files
    )
    for forbidden in FORBIDDEN_HARD_DEPENDENCIES[4:]:
        require(forbidden not in all_source_text,
                f"forbidden engine replay authority call found: {forbidden}", issues)

    raw_audio_extensions = {".wav", ".ogg", ".mp3", ".flac", ".aif", ".aiff", ".wem", ".bnk"}
    raw_audio = []
    for folder in (root / "Content", root / "SourceArt"):
        if folder.is_dir():
            raw_audio.extend(path for path in folder.rglob("*")
                             if path.is_file() and path.suffix.lower() in raw_audio_extensions)
    production_assets = contract.get("audio_router_contract", {}).get("production_audio_assets_present")
    if production_assets is False:
        require(not raw_audio,
                "contract claims no production audio but raw audio files are present", issues)


def audit(root: Path, contract_path: Path) -> dict[str, Any]:
    issues: list[str] = []
    try:
        contract = read_json(root / contract_path)
    except Exception as exc:  # noqa: BLE001 - validator must report malformed input
        return {"technicalPass": False, "releaseReady": False,
                "releaseBlocked": True, "issues": [f"contract load failed: {exc}"]}
    issues.extend(validate_document(contract))
    validate_bound_files(root, contract, issues)
    validate_sources(root, contract, issues)
    capabilities = contract.get("implementation_capabilities", [])
    pending = [entry.get("id") for entry in capabilities
               if isinstance(entry, dict) and entry.get("status") != "TECHNICAL_PASS"]
    return {
        "schema": "dg.session12.presentation.audit",
        "schemaVersion": 1,
        "technicalPass": not issues,
        "releaseReady": False,
        "releaseBlocked": True,
        "normalStatus": NORMAL_STATUS,
        "releaseStatus": RELEASE_STATUS,
        "releaseBlockers": EXPECTED_BLOCKERS,
        "pendingCapabilities": pending,
        "issues": issues,
    }


def run_self_tests(root: Path, contract_path: Path) -> bool:
    baseline = audit(root, contract_path)
    cases = 0
    failures: list[str] = []
    cases += 1
    if not baseline.get("technicalPass"):
        failures.append("baseline audit did not pass")

    contract = read_json(root / contract_path)
    mutations = [
        ("schema", lambda value: value.__setitem__("schema", "wrong")),
        ("release ready", lambda value: value.__setitem__("release_ready", True)),
        ("release blocked", lambda value: value.__setitem__("releaseBlocked", False)),
        ("blocker order", lambda value: value["release_blockers"].reverse()),
        ("missing S12 blocker", lambda value: value["release_blockers"].pop()),
        ("plugin audio owner", lambda value: value["audio_router_contract"].__setitem__(
            "owner", "UDiscGolfAudioBridgeComponent")),
        ("router tick", lambda value: value["audio_router_contract"].__setitem__(
            "component_tick", True)),
        ("sync load", lambda value: value["audio_router_contract"].__setitem__(
            "synchronous_asset_load_during_routing", True)),
        ("hard CommonUI", lambda value: value["optional_engine_policy"].__setitem__(
            "hard_common_ui_dependency", True)),
        ("engine replay", lambda value: value["optional_engine_policy"].__setitem__(
            "engine_replay_is_authoritative", True)),
        ("duplicate capability", lambda value: value["implementation_capabilities"][1].__setitem__(
            "id", value["implementation_capabilities"][0]["id"])),
        ("incomplete capability", lambda value: value["implementation_capabilities"][1].__setitem__(
            "status", "PENDING_ROOT_INTEGRATION")),
        ("false feature complete", lambda value: value.__setitem__("feature_complete", False)),
        ("wrong exit status", lambda value: value["exit_gate"].__setitem__(
            "status", "PENDING")),
        ("missing live editor evidence", lambda value: value["exit_gate"].__setitem__(
            "live_editor_game_pass", False)),
        ("missing fresh package evidence", lambda value: value["exit_gate"].__setitem__(
            "fresh_package_pass", False)),
        ("missing packaged live evidence", lambda value: value["exit_gate"].__setitem__(
            "packaged_live_pass", False)),
        ("profile evidence not required", lambda value: value["test_contract"].__setitem__(
            "protected_profile_hash_required_after_all_runs", False)),
        ("missing closure evidence", lambda value: value["closure_evidence"].pop(
            "post_fix_camera_automation")),
        ("evidence hash drift", lambda value: value["closure_evidence"][
            "full_automation"].__setitem__("sha256", "0" * 64)),
        ("automation failure", lambda value: value["closure_evidence"][
            "focused_automation"].__setitem__("failed", 1)),
        ("runtime sample drift", lambda value: value["closure_evidence"][
            "live_editor_game"].__setitem__("solver_samples", 0)),
        ("runtime route drift", lambda value: value["closure_evidence"][
            "packaged_live"].__setitem__("audio_routes", 0)),
        ("incremental package", lambda value: value["closure_evidence"][
            "fresh_package"].__setitem__("incrementally_skipped", 1)),
        ("package count drift", lambda value: value["closure_evidence"][
            "fresh_package"].__setitem__("cooked_packages", 983)),
        ("reference flight drift", lambda value: value["closure_evidence"][
            "reference_flight"].__setitem__("carry_m", 0.0)),
        ("wrong blocker count", lambda value: value["validation_contract"].__setitem__(
            "expected_release_blocker_count", 10)),
    ]
    for name, mutate in mutations:
        candidate = copy.deepcopy(contract)
        mutate(candidate)
        cases += 1
        if not validate_document(candidate):
            failures.append(f"mutation was accepted: {name}")

    print(f"Session 12 validator self-test: {cases - len(failures)}/{cases} passed")
    for failure in failures:
        print(f"SELF-TEST FAIL: {failure}")
    return not failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--report", nargs="?", const=str(REPORT_PATH))
    parser.add_argument("--require-release", action="store_true")
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
        print("Session 12 presentation contract: FAIL")
        for issue in result["issues"]:
            print(f"- {issue}")
        return 1

    print(f"Session 12 presentation contract: {NORMAL_STATUS}")
    print(f"Pending capabilities: {len(result['pendingCapabilities'])}")
    print(f"Release blocked: yes ({len(EXPECTED_BLOCKERS)} blockers)")
    if args.require_release:
        print(f"Release gate: {RELEASE_STATUS}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
