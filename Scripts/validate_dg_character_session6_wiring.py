#!/usr/bin/env python3
"""Source-only Session 6 wiring and authority-boundary validation."""

from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).absolute().parents[1]
REPORT = ROOT / "Saved/CharacterFramework/Session6OutfitWiringReport.json"


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def exact_tokens(relative: str, tokens: tuple[str, ...]) -> tuple[list[str], dict]:
    path = ROOT / relative
    if not path.is_file():
        return [f"missing {relative}"], {"path": relative, "present": False}
    text = read(relative)
    missing = [token for token in tokens if token not in text]
    return [f"{relative} missing token: {token}" for token in missing], {
        "path": relative, "present": True, "tokens": len(tokens), "missing": missing,
    }


def main() -> int:
    checks: list[dict] = []

    def record(check_id: str, errors: list[str], evidence: dict) -> None:
        checks.append({
            "id": check_id,
            "status": "PASS" if not errors else "FAIL",
            "errors": errors,
            "evidence": evidence,
        })

    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolferPawn.h",
        (
            "GetOutfitComponent", "GetOutfitCatalog", "GetCurrentOutfitLoadout",
            "ApplyOutfitLoadoutTransactionally", "RefreshOutfitForCurrentBodyProfile",
        ),
    )
    record("actual_player_owns_single_outfit_component_api", errors, evidence)

    pawn_header = read("Source/DiscGolfTour/DiscGolferPawn.h")
    pawn_source = read("Source/DiscGolfTour/DiscGolferPawn.cpp")
    compact_pawn_source = re.sub(r"\s+", "", pawn_source)
    camera_errors = []
    if 'Camera/PlayerCameraManager.h' not in pawn_source:
        camera_errors.append("Pawn is missing the PlayerCameraManager include")
    if pawn_source.count("PlayerCameraManager->UpdateCamera(0.0f)") != 3:
        camera_errors.append(
            "Pawn must refresh the paused creator camera exactly once at begin, "
            "restore, and the Session 7 zoom successor seam")
    if pawn_source.count(
            "CameraBoom->SocketOffset = FVector(0.0f, -120.0f, 70.0f);") != 1:
        camera_errors.append("Pawn must use the exact Session 6 right-pane preview offset")
    saved_pcm_tick_member = "bSavedPlayerCameraManagerTickWhenPaused"
    pcm_tick_property = "PlayerCameraManager->PrimaryActorTick.bTickEvenWhenPaused"
    spring_arm_tick_call = (
        "CameraBoom->TickComponent(0.0f,"
        "ELevelTick::LEVELTICK_All,nullptr);")
    if compact_pawn_source.count(spring_arm_tick_call) != 3:
        camera_errors.append(
            "Pawn must deterministically propagate the spring arm exactly once "
            "at begin, restore, and the Session 7 zoom successor seam")
    if pawn_header.count(saved_pcm_tick_member) != 1:
        camera_errors.append(
            "Pawn must own exactly one saved PlayerCameraManager paused-tick flag")
    begin_preview = pawn_source.split(
        "void ADiscGolferPawn::BeginCharacterCreatorPreview()", 1
    )[-1].split("void ADiscGolferPawn::EndCharacterCreatorPreview", 1)[0]
    end_preview = pawn_source.split(
        "void ADiscGolferPawn::EndCharacterCreatorPreview", 1
    )[-1].split("void ADiscGolferPawn::RotateCharacterCreatorPreview", 1)[0]
    compact_begin_preview = re.sub(r"\s+", "", begin_preview)
    compact_end_preview = re.sub(r"\s+", "", end_preview)
    if ("RefreshCharacterProfilePresentation();" not in begin_preview
            or begin_preview.find("RefreshCharacterProfilePresentation();")
            > begin_preview.find("PlayerCameraManager->UpdateCamera(0.0f)")):
        camera_errors.append("Begin preview must refresh presentation before its camera cache")
    begin_save_tick = begin_preview.find(
        f"{saved_pcm_tick_member} =\n            {pcm_tick_property};")
    begin_enable_tick = begin_preview.find(f"{pcm_tick_property} = true;")
    begin_offset = begin_preview.find(
        "CameraBoom->SocketOffset = FVector(0.0f, -120.0f, 70.0f);")
    begin_rotation = begin_preview.find(
        "CameraBoom->SetRelativeRotation(FRotator(-4.0f, 180.0f, 0.0f));")
    begin_fov = begin_preview.find("Camera->SetFieldOfView(46.0f);")
    begin_refresh = begin_preview.find("RefreshCharacterProfilePresentation();")
    begin_spring_tick = begin_preview.find("CameraBoom->TickComponent(")
    begin_update = begin_preview.find("PlayerCameraManager->UpdateCamera(0.0f);")
    if (min(begin_save_tick, begin_enable_tick, begin_offset, begin_rotation,
            begin_fov, begin_refresh, begin_spring_tick, begin_update) < 0
            or not (begin_save_tick < begin_enable_tick < begin_offset
                    < begin_rotation < begin_fov < begin_refresh
                    < begin_spring_tick < begin_update)):
        camera_errors.append(
            "Begin preview must save and enable the camera-manager paused tick "
            "before composing, propagating, and refreshing the right-pane camera")
    if (compact_begin_preview.count(spring_arm_tick_call) != 1
            or compact_begin_preview.count(
                "if(CameraBoom->IsRegistered())") != 1):
        camera_errors.append(
            "Begin preview spring-arm propagation must be registered-gated")
    if ("if (bRestoreView)" not in end_preview
            or "PlayerCameraManager->UpdateCamera(0.0f)" not in end_preview):
        camera_errors.append("End preview must refresh the restored camera under bRestoreView")
    end_update = end_preview.find("PlayerCameraManager->UpdateCamera(0.0f);")
    end_restore_arm = end_preview.find(
        "CameraBoom->TargetArmLength = SavedPreviewCameraArmLength;")
    end_restore_socket = end_preview.find(
        "CameraBoom->SocketOffset = SavedPreviewCameraSocketOffset;")
    end_restore_rotation = end_preview.find(
        "CameraBoom->SetRelativeRotation(SavedPreviewCameraBoomRotation);")
    end_restore_fov = end_preview.find(
        "Camera->SetFieldOfView(SavedPreviewCameraFov);")
    end_spring_tick = end_preview.find("CameraBoom->TickComponent(")
    end_restore_tick = end_preview.find(
        f"{pcm_tick_property} =\n            {saved_pcm_tick_member};")
    if (min(end_restore_arm, end_restore_socket, end_restore_rotation,
            end_restore_fov, end_spring_tick, end_update, end_restore_tick) < 0
            or not (end_restore_arm < end_restore_socket < end_restore_rotation
                    < end_restore_fov < end_spring_tick < end_update
                    < end_restore_tick)):
        camera_errors.append(
            "End preview must restore and propagate the spring arm, refresh the "
            "camera, then restore the saved camera-manager paused-tick flag")
    if (compact_end_preview.count(spring_arm_tick_call) != 1
            or compact_end_preview.count(
                "if(bRestoreView&&CameraBoom&&CameraBoom->IsRegistered())") != 1):
        camera_errors.append(
            "End preview spring-arm restoration must be registered-gated")
    if begin_preview.count("if (PlayerCameraManager)") < 2:
        camera_errors.append(
            "Begin preview camera-manager save/enable and refresh must remain null-safe")
    if end_preview.count("if (PlayerCameraManager)") != 1:
        camera_errors.append(
            "End preview camera-manager paused-tick restoration must remain null-safe")
    zoom_preview = pawn_source.split(
        "void ADiscGolferPawn::ZoomCharacterCreatorPreview(float DeltaArmLength)", 1
    )[-1].split("void ADiscGolferPawn::SetupPlayerInputComponent", 1)[0]
    compact_zoom_preview = re.sub(r"\s+", "", zoom_preview)
    zoom_arm = compact_zoom_preview.find(
        "CameraBoom->TargetArmLength=FMath::Clamp("
        "CameraBoom->TargetArmLength+DeltaArmLength,300.0f,620.0f);")
    zoom_spring_tick = compact_zoom_preview.find(spring_arm_tick_call)
    zoom_update = compact_zoom_preview.find(
        "PlayerController->PlayerCameraManager->UpdateCamera(0.0f);")
    if (min(zoom_arm, zoom_spring_tick, zoom_update) < 0
            or not (zoom_arm < zoom_spring_tick < zoom_update)):
        camera_errors.append(
            "Session 7 zoom must clamp the accepted 300..620 cm arm, propagate "
            "the registered spring arm, then refresh PlayerCameraManager")
    if (compact_zoom_preview.count(spring_arm_tick_call) != 1
            or compact_zoom_preview.count(
                "if(CameraBoom->IsRegistered())") != 1
            or compact_zoom_preview.count(
                "PlayerController&&PlayerController->PlayerCameraManager") != 1
            or "!FMath::IsFinite(DeltaArmLength)" not in zoom_preview):
        camera_errors.append(
            "Session 7 zoom successor seam must remain finite, registered, "
            "controller-safe, and single-propagation")
    record(
        "real_paused_creator_camera_cache_is_refreshed_and_restored",
        camera_errors,
        {"update_camera_calls": pawn_source.count(
            "PlayerCameraManager->UpdateCamera(0.0f)"),
         "right_pane_offset": "FVector(0.0f, -120.0f, 70.0f)",
         "spring_arm_tick_calls": compact_pawn_source.count(spring_arm_tick_call),
         "spring_arm_tick_level": "ELevelTick::LEVELTICK_All",
         "saved_tick_member": saved_pcm_tick_member,
         "paused_tick_enabled_before_composition": (
             0 <= begin_enable_tick < begin_offset),
         "spring_arm_propagated_before_preview_camera_refresh": (
             0 <= begin_spring_tick < begin_update),
         "spring_arm_propagated_before_restore_camera_refresh": (
             0 <= end_spring_tick < end_update),
         "paused_tick_restored_after_camera_refresh": (
             0 <= end_update < end_restore_tick),
         "zoom_clamped_then_propagated_then_refreshed": (
             0 <= zoom_arm < zoom_spring_tick < zoom_update)},
    )

    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolfOutfitRuntime.h",
        (
            "GetOrderedSlots", "GetSlotDisplayName", "FindEntryForSlot",
            "SetSlotSelection", "ResolveCanonicalLoadout", "AreLoadoutsEquivalent",
            "GetOptionsForSlot",
            "/Game/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.DA_DG_OutfitCatalog",
        ),
    )
    record("project_adapter_canonicalizes_installed_plugin_model", errors, evidence)

    controller_path = "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp"
    errors, evidence = exact_tokens(
        controller_path,
        (
            "CharacterCreatorOpeningOutfit", "PreviewCharacterCreatorOutfitSelection",
            "Apply", "Cancel", "PrepareCharacterCreatorForSession6VisualEvidence",
        ),
    )
    controller_source = read(controller_path)
    zero_blend_sequence = (
        "SetViewTarget(Golfer);\n"
        "    Golfer->BeginCharacterCreatorPreview();"
    )
    open_creator = controller_source.split(
        "bool ADiscGolfTourPlayerController::OpenCharacterCreator()", 1
    )[-1].split(
        "bool ADiscGolfTourPlayerController::PreviewCharacterCreatorDraft", 1
    )[0]
    if controller_source.count("SetViewTarget(Golfer);") != 1:
        errors.append(
            "OpenCharacterCreator must target the existing golfer exactly once")
    if (controller_source.count(zero_blend_sequence) != 1
            or open_creator.count(zero_blend_sequence) != 1):
        errors.append(
            "OpenCharacterCreator must set the golfer view target immediately "
            "before beginning its preview")
    evidence["zero_blend_existing_golfer_sequence"] = (
        controller_source.count(zero_blend_sequence))
    record("creator_apply_cancel_and_live_outfit_preview", errors, evidence)

    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp",
        (
            "PrepareSession6VisualOutfitEvidence", "OutfitItemButtons",
            "OutfitVariantButtons", "SetKeyboardFocus",
        ),
    )
    record("creator_outfit_tab_draft_and_focus_capture_seam", errors, evidence)

    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolfSaveGame.h",
        ("FDGOutfitLoadout", "SaveGame", "Outfit"),
    )
    record("stable_outfit_save_payload", errors, evidence)

    smoke_tokens = (
        "Super::Start()", "Session6OutfitFixture", "MissingItem",
        "LeaderPoseComponent.Get()", "GetCollisionEnabled", "disc_grip_r",
        "authority_fields_unchanged=1", "one_authoritative_disc=1",
    )
    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolfSession6OutfitSmokeRunner.cpp", smoke_tokens,
    )
    record("end_to_end_outfit_throw_harness", errors, evidence)

    errors, evidence = exact_tokens(
        "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        ("Session6OutfitThrowSmokeTest", "ADiscGolfSession6OutfitSmokeRunner",
         "Session6OutfitVisualCapture", "Session6OutfitValidationNoSave",
         "bSession7FullCharacterValidationNoSave",
         "Session7FullCharacterThrowSmokeTest",
         "Session7FullCharacterVisualCapture"),
    )
    record("explicit_command_line_harness_hooks", errors, evidence)

    game_mode = read("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
    save_function = game_mode.split(
        "void ADiscGolfTourGameMode::SavePracticeRoundSnapshot()", 1
    )[-1].split("UDiscGolfTourGameInstance* GameInstance", 1)[0]
    no_save_errors = []
    if game_mode.count('TEXT("Session6OutfitValidationNoSave")') != 1:
        no_save_errors.append("validation no-save flag must occur exactly once in GameMode source")
    for token in (
        'TEXT("Session6OutfitValidationNoSave")',
        'TEXT("Session6OutfitThrowSmokeTest")',
        'TEXT("Session6OutfitVisualCapture")',
        'TEXT("Session7FullCharacterValidationNoSave")',
        'TEXT("Session7FullCharacterThrowSmokeTest")',
        'TEXT("Session7FullCharacterVisualCapture")',
        "&& (FParse::Param(",
        "|| FParse::Param(",
        "!= FParse::Param(",
    ):
        if token not in save_function:
            no_save_errors.append(f"SavePracticeRoundSnapshot missing fail-closed token: {token}")
    compact_save_function = re.sub(r"\s+", "", save_function)
    expected_combined_boundary = (
        "if(bRegressionActive||bSession6OutfitValidationNoSave"
        "||bSession7FullCharacterValidationNoSave)return;")
    if compact_save_function.count(expected_combined_boundary) != 1:
        no_save_errors.append(
            "practice snapshot boundary must have exactly one combined "
            "regression/Session6/Session7 early return")
    record(
        "session6_no_save_flag_is_fail_closed_at_practice_snapshot_boundary",
        no_save_errors,
        {"flag_occurrences_in_game_mode": game_mode.count(
            'TEXT("Session6OutfitValidationNoSave")'),
         "combined_boundary_occurrences": compact_save_function.count(
             expected_combined_boundary)},
    )

    visual_runner_path = (
        "Source/DiscGolfTour/DiscGolfSession6OutfitVisualCaptureRunner.cpp")
    errors, evidence = exact_tokens(
        visual_runner_path,
        (
            "PrepareCharacterCreatorForSession6VisualEvidence",
            "bCreatorOutfitTabPrepared", "ValidateCaptureScene",
            "manual_visual_review_required", "ProjectedKeyBoneCount",
            "KeyBonesInsideSafeMargin", "key_bone_projections",
            "PauseWorldForThrowCapture", "PausedThrowCaptureCount",
            "Session6ValidationCaptureFovDeg = 64.0f",
            "ApplyCharacterCreatorDraft", "LoadGameFromSlot",
            "DeleteGameInSlot", "creator_apply_saved",
            "creator_apply_reloaded", "validation_temp_slot_deleted",
            "creator_reload_reconstructed_outfit",
            "creator_cancel_restored_applied",
            "creator_view_target_is_golfer",
            "creator_preview_composed_in_right_pane",
            "ValidateVisibleSkeletalOutfitMaterials",
            "UMaterialInstanceDynamic", "FHashedMaterialParameterInfo",
            "/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy.M_DG_OutfitProxy",
            "DG_PrimaryColor", "DG_SecondaryColor", "DG_AccentColor",
            "DG_RoughnessBias", "GetVectorParameterValue",
            "GetScalarParameterValue",
            "GetBaseMaterial", "GetUsageByFlag(MATUSAGE_SkeletalMesh)",
            "all_visible_skeletal_outfit_materials_are_canonical_mids",
            "canonical_outfit_material_has_skeletal_mesh_usage",
            "all_selected_variant_material_parameters_match",
            "selected_skeletal_variants",
            "RightPaneMinX", "RightPaneMaxX",
            "Debug text would overlap the native Slate header",
        ),
    )
    visual_runner_source = read(visual_runner_path)
    compact_visual_runner_source = re.sub(r"\s+", "", visual_runner_source)
    override_only_read = (
        "FHashedMaterialParameterInfo(Pair.Key),Actual,true)")
    if compact_visual_runner_source.count(override_only_read) != 2:
        errors.append(
            "visual runner must read all vector/scalar MID parameters "
            "override-only")
    if "CheckMaterialUsage" in visual_runner_source:
        errors.append(
            "visual runner must not call material-usage APIs that can mutate assets")
    evidence["override_only_mid_parameter_reads"] = (
        compact_visual_runner_source.count(override_only_read))
    evidence["mutating_material_usage_checks"] = visual_runner_source.count(
        "CheckMaterialUsage")
    record("visual_creator_tab_sync_and_scene_gates", errors, evidence)

    errors, evidence = exact_tokens(
        "Scripts/run-session6-outfit-throw-matrix.py",
        tuple(name for name in (
            "BaselineCore", "BaselineLayered", "ShortFull", "TallFull",
            "SliderExtremeFull", "MissingItem", "no_persistent_writes",
            "-Session6OutfitValidationNoSave",
        )),
    )
    record("five_required_rows_plus_missing_reference", errors, evidence)

    errors, evidence = exact_tokens(
        "Scripts/run-session6-outfit-visual-capture.py",
        (
            "-Session6OutfitVisualCapture", "-Session6OutfitValidationNoSave",
            "-Session6OutfitValidationSaveSlot=",
            "DiscGolfTour_Automation_Session6Outfit_",
            "creator_apply_saved", "creator_apply_reloaded",
            "creator_reload_reconstructed_outfit",
            "validation_temp_slot_deleted", "creator_cancel_restored_applied",
            "creator_view_target_is_golfer",
            "creator_preview_composed_in_right_pane",
            "missing usage flag", "Default Material will be used in game",
            "runtime_canonical_skeletal_mids_and_variant_parameters",
            "frame_2_graphite_and_frame_3_teal_runtime_parameters",
            "TOP=Graphite", "TOP=Teal", "OUTERWEAR=Teal",
            "production_save_sha256_unchanged", "file_sha256_or_absent",
        ),
    )
    record("visual_launcher_requests_fail_closed_no_save_mode", errors, evidence)

    # Cosmetic integration may call the existing Pawn adapter but must never
    # add its own disc actor, flight component, release solver, wind, or score.
    outfit_sources = sorted((ROOT / "Source/DiscGolfTour").glob("*Outfit*.cpp"))
    forbidden = re.compile(
        r"SpawnActor\s*<\s*ADiscActor|NewObject\s*<\s*UDiscFlightComponent|"
        r"ResolveThrowRelease\s*\(|AddStroke\s*\(|SetWind",
        re.IGNORECASE,
    )
    authority_hits = []
    for path in outfit_sources:
        if path.name == "DiscGolfSession6OutfitSmokeRunner.cpp":
            continue
        for number, line in enumerate(path.read_text(
            encoding="utf-8", errors="replace").splitlines(), 1):
            if forbidden.search(line):
                authority_hits.append({
                    "file": path.relative_to(ROOT).as_posix(), "line": number,
                    "text": line.strip(),
                })
    record(
        "no_parallel_throw_flight_inventory_or_scoring_authority",
        [f"forbidden authority token: {hit}" for hit in authority_hits],
        {"outfit_source_count": len(outfit_sources), "hits": authority_hits},
    )

    slot_header = read(
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/DiscGolfOutfitTypes.h"
    )
    slots = (
        "Headwear", "Eyewear", "Top", "Outerwear", "Bottom", "Socks",
        "Footwear", "Glove", "Wrist", "Bag", "Accessory",
    )
    missing_slots = [slot for slot in slots if not re.search(rf"\b{slot}\b", slot_header)]
    record(
        "installed_plugin_frozen_eleven_slot_schema",
        [f"missing installed slot {slot}" for slot in missing_slots],
        {"slots": list(slots), "missing": missing_slots},
    )

    status = subprocess.run(
        ["git", "status", "--porcelain=v1", "--untracked-files=all"],
        cwd=ROOT, capture_output=True, text=True, check=False,
    )
    generated = ("Binaries/", "Intermediate/", "Saved/", "DerivedDataCache/")
    staged_generated = []
    if status.returncode == 0:
        for line in status.stdout.splitlines():
            state, path = line[:2], line[3:].replace("\\", "/")
            if state[0] not in (" ", "?") and path.startswith(generated):
                staged_generated.append(path)
    record(
        "generated_roots_not_staged",
        [f"staged generated path: {path}" for path in staged_generated],
        {"paths": staged_generated},
    )

    all_errors = [f"{check['id']}: {error}" for check in checks for error in check["errors"]]
    report = {
        "schema": "DiscGolfTour.Session6OutfitWiring.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not all_errors else "FAIL",
        "ue_launched": False,
        "ubt_launched": False,
        "checks": checks,
        "errors": all_errors,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"SESSION 6 OUTFIT WIRING {report['status']}: {len(checks)} checks report={REPORT}")
    for error in all_errors:
        print(f"  - {error}")
    return 0 if not all_errors else 1


if __name__ == "__main__":
    sys.exit(main())
