"""Static Session 3 authority/wiring acceptance.

This validator deliberately inspects source and package presence only. It never
loads or saves Unreal assets; the reflected asset validator owns notify/graph
inspection and the live smoke owns runtime behavior.
"""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import json
import re
import subprocess


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session3WiringValidation.json"

EXPECTED_ASSETS = (
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset",
    "Content/DiscGolf/Rigs/CR_DG_Master.uasset",
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset",
)


def _read(relative: str) -> str:
    return (PROJECT_ROOT / relative).read_text(encoding="utf-8")


def _require(checks: list[dict], name: str, condition: bool, detail: str) -> None:
    checks.append({"check": name, "passed": bool(condition), "detail": detail})


def main() -> None:
    pawn = _read("Source/DiscGolfTour/DiscGolferPawn.cpp")
    pawn_header = _read("Source/DiscGolfTour/DiscGolferPawn.h")
    adapter = _read("Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp")
    game_mode = _read("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
    game_mode_header = _read("Source/DiscGolfTour/DiscGolfTourGameMode.h")
    smoke = _read("Source/DiscGolfTour/DiscGolfSession3SmokeRunner.cpp")
    build_rules = _read("Source/DiscGolfTour/DiscGolfTour.Build.cs")
    project = json.loads(_read("DiscGolfTour.uproject"))

    checks: list[dict] = []
    _require(checks, "runtime_module_depends_on_installed_framework",
             '"DiscGolfCharacterFramework"' in build_rules,
             "DiscGolfTour.Build.cs declares the installed runtime plugin module")
    enabled_plugins = {
        item.get("Name"): item.get("Enabled") for item in project.get("Plugins", [])
    }
    _require(checks, "installed_framework_enabled",
             enabled_plugins.get("DiscGolfCharacterFramework") is True,
             "DiscGolfCharacterFramework is explicitly enabled in the uproject")
    _require(checks, "pawn_owns_single_animation_bridge",
             all(token in pawn for token in (
                 "CreateDefaultSubobject<UDiscGolfThrowComponent>",
                 "CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>",
                 "TryStartAnimatedRHBHThrow",
                 "HandleAnimatedRHBHRelease")),
             "player pawn owns framework presentation plus one project adapter")
    _require(checks, "provisional_disc_socket_contract",
             all(token in pawn for token in (
                 'TEXT("HeldDiscVisual")',
                 'TEXT("disc_grip_r")',
                 "FVector(0.21f, 0.21f, 0.015f)",
                 "FRotator(0.0f, 180.0f, 0.0f)",
                 "ECollisionEnabled::NoCollision")),
             "Cylinder visual is collision-free and socketed with the accepted provisional transform")
    _require(checks, "one_rhbh_drive_scope",
             "Command.ThrowStyle != EThrowStyle::Backhand" in pawn
             and "Command.ShotContext != EDiscShotContext::Drive" in pawn,
             "only RHBH Drive enters Session 3; other throws retain the immediate path")
    _require(checks, "animation_asset_paths",
             "A_DG_RHBH_Prototype" not in pawn
             and "/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype" in pawn,
             "runtime references the owned montage and does not couple to sequence internals")
    _require(checks, "release_bridge_uses_grip_entry",
             "RequestThrowFromGrip(AuthoritativeCommand, GripWorldTransform)" in pawn,
             "Pawn delegates exact command plus grip transform to GameMode")
    _require(checks, "grip_entry_is_cpp_only",
             "bool RequestThrowFromGrip(" in game_mode_header
             and "UFUNCTION(BlueprintCallable) bool RequestThrowFromGrip" not in game_mode_header,
             "the committed adapter seam is not exposed as a general Blueprint throw entry")
    _require(checks, "grip_entry_requires_committed_player_transaction",
             all(token in game_mode for token in (
                 "bRegressionActive || bLieTransitionActive",
                 "Golfer->GetRHBHThrowAdapter()",
                 "Adapter->IsThrowActive()",
                 "Adapter->HasCommittedRelease()",
                 "Adapter->GetReleaseCommitCountForAttempt() != 1",
                 "bMatchesCommittedCommand")),
             "the CanPlayerThrow bypass is limited to the exact committed player transaction")
    _require(checks, "existing_solver_is_authority",
             "DiscGolfMath::ResolveThrowRelease(AuthoredCommand)" in game_mode
             and "ActiveDisc->Throw(LastRelease)" in game_mode,
             "GameMode still resolves and launches through existing authoritative physics")
    _require(checks, "direct_request_semantics_preserved",
             re.search(r"void ADiscGolfTourGameMode::RequestThrow\([^)]*\)\s*\{\s*if \(!CanPlayerThrow\(\)\) return;\s*LaunchThrow\(Command\);\s*\}",
                       game_mode, re.DOTALL) is not None,
             "programmatic/direct RequestThrow remains synchronous")
    _require(checks, "grip_handoff_is_spawn_only",
             "ReleaseLocationOverrideCm" in game_mode
             and "? *ReleaseLocationOverrideCm" in game_mode,
             "animation handoff overrides spawn location only")

    release_fields = sorted(set(re.findall(r"ReleaseData\.([A-Za-z_][A-Za-z0-9_]*)", adapter)))
    _require(checks, "only_grip_transform_consumed",
             release_fields == ["GripWorldTransform"],
             f"adapter release-data fields: {release_fields}")
    forbidden_physics = (
        "SuggestedLaunchSpeedMps",
        "SuggestedSpinRpm",
        "GripLinearVelocityCmPerSec",
    )
    _require(checks, "no_parallel_physics_inputs",
             not any(token in adapter for token in forbidden_physics),
             "adapter contains no framework speed, spin, or grip-velocity input")
    _require(checks, "held_disc_hidden_before_launch",
             adapter.find("SetHeldDiscVisible(false);")
             < adapter.find("AuthoritativeLaunchDelegate.Execute"),
             "held presentation is hidden before the synchronous gameplay launch callback")
    _require(checks, "bounded_event_driven_adapter",
             "PrimaryComponentTick.bCanEverTick = false" in adapter
             and "WatchdogTimeoutSeconds" in adapter,
             "adapter is zero-Tick and has a bounded watchdog")
    _require(checks, "pawn_api_declared",
             all(token in pawn_header for token in (
                 "TryStartAnimatedRHBHThrow",
                 "CancelAnimatedThrowBeforeRelease",
                 "IsAnimatedThrowActive")),
             "real input and live smoke use the same high-level pawn seam")
    _require(checks, "live_smoke_proves_full_recovery",
             all(token in smoke for token in (
                 "EDGThrowPhase::FollowThrough",
                 "EDGThrowPhase::Recovery",
                 "CountWorldGolfers() == 1",
                 "PlayerController->GetViewTarget() == Golfer",
                 "the recovered player could not begin the next legal throw action",
                 "Golfer->CancelThrowPresentation()")),
             "live smoke observes both late phases, one pawn, camera return, and next-action cancellation")

    missing_assets = [relative for relative in EXPECTED_ASSETS
                      if not (PROJECT_ROOT / relative).is_file()]
    _require(checks, "exact_required_asset_set_present", not missing_assets,
             f"missing={missing_assets}; expected_count={len(EXPECTED_ASSETS)}")

    plugin_diff = subprocess.run(
        ["git", "diff", "--name-only", "HEAD", "--",
         "Plugins/DiscGolfCharacterFramework"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip().splitlines()
    _require(checks, "installed_plugin_unchanged", not plugin_diff,
             f"plugin diff entries={plugin_diff}")

    failed = [item for item in checks if not item["passed"]]
    report = {
        "schema": "DiscGolfTour.Session3WiringValidation.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_SESSION3_SINGLE_AUTHORITY_WIRING" if not failed else "FAIL",
        "scope": "RHBH_DRIVE_ONLY_NO_SESSION4",
        "checks": checks,
        "asset_count": len(EXPECTED_ASSETS),
        "release_data_fields_consumed": release_fields,
        "plugin_source_changes": plugin_diff,
        "failed_checks": [item["check"] for item in failed],
        "writes": [str(REPORT_PATH)],
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if failed:
        raise SystemExit(
            "SESSION3 WIRING VALIDATION FAIL: "
            + ", ".join(item["check"] for item in failed)
        )
    print(
        "SESSION3 WIRING VALIDATION PASS: "
        f"checks={len(checks)} assets={len(EXPECTED_ASSETS)} "
        "release_fields=GripWorldTransform plugin_changes=0"
    )


if __name__ == "__main__":
    main()
