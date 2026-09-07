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
EXPECTED_ACCEPTED_PLUGIN_CHANGES = (
    *SESSION6_SESSION7_PLUGIN_CHANGES,
    *SESSION8_PLUGIN_CHANGES,
)
OUTFIT_COMPONENT_PATH = SESSION6_SESSION7_PLUGIN_CHANGES[1]
AVATAR_BACKEND_CPP, AVATAR_BACKEND_H = SESSION8_PLUGIN_CHANGES


# Session 19 moved several development-only runners from the runtime module to
# Source/DiscGolfTourDeveloper, which is denied in Shipping. This validator is
# frozen and still names the original path, so resolve the move rather than
# failing to find a file that exists. The requirement is unchanged: the same
# tokens must still be present in the same file.
SESSION19_RUNTIME_PREFIX = "Source/DiscGolfTour/"
SESSION19_DEVELOPER_PREFIX = "Source/DiscGolfTourDeveloper/"


def _resolve_session19_relocation(relative: str):
    """Return the path to read, preferring the original location."""
    original = PROJECT_ROOT / relative
    if original.is_file() or not relative.startswith(SESSION19_RUNTIME_PREFIX):
        return original
    moved = PROJECT_ROOT / relative.replace(
        SESSION19_RUNTIME_PREFIX, SESSION19_DEVELOPER_PREFIX, 1)
    return moved if moved.is_file() else original


def _read(relative: str) -> str:
    return _resolve_session19_relocation(relative).read_text(encoding="utf-8")


def _require(checks: list[dict], name: str, condition: bool, detail: str) -> None:
    checks.append({"check": name, "passed": bool(condition), "detail": detail})


# Session 19 moved Plugins/DiscGolfCharacterFramework outside the project root
# (characterFrameworkExternalQuarantine, mustBeOutsideProjectRoot: true), so on any
# checkout this validator's inputs are absent by policy. Say so once, clearly,
# instead of dying with FileNotFoundError partway through. This asserts nothing
# about the wiring itself -- it reports that the evidence cannot be reached here.
def _require_character_framework(root) -> None:
    plugin = root / "Plugins/DiscGolfCharacterFramework/Source"
    if plugin.is_dir():
        return
    print("Character framework validator cannot run on this checkout.")
    print(f"  {plugin} is absent.")
    print("  Session 19 moved the plugin outside the project root by accepted "
          "policy; see Docs/FRESH_CHECKOUT.md. Run this on the authoring host, "
          "or restore the quarantined tree first.")
    raise SystemExit(2)


def main() -> None:
    _require_character_framework(PROJECT_ROOT)
    pawn = _read("Source/DiscGolfTour/DiscGolferPawn.cpp")
    pawn_header = _read("Source/DiscGolfTour/DiscGolferPawn.h")
    adapter = _read("Source/DiscGolfTour/DiscGolfRHBHThrowAdapterComponent.cpp")
    game_mode = _read("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
    game_mode_header = _read("Source/DiscGolfTour/DiscGolfTourGameMode.h")
    smoke = _read("Source/DiscGolfTour/DiscGolfSession3SmokeRunner.cpp")
    outfit_component = _read(OUTFIT_COMPONENT_PATH)
    avatar_backend_cpp = _read(AVATAR_BACKEND_CPP)
    avatar_backend_header = _read(AVATAR_BACKEND_H)
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

    # Compare to the accepted pre-outfit Session 5 checkpoint instead of
    # expecting a dirty working tree.  The old gate became stale as soon as
    # the accepted Session 6 outfit repair was committed: `git status` went
    # clean even though the installed plugin still contained the reviewed
    # adaptation.  The first comparison remains exact before and after the
    # successor commits.  The second independently proves that Session 8 adds
    # only the two reviewed avatar-backend files on top of accepted Session 7.
    plugin_diff_result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACDMRTUXB",
         SESSION5_PLUGIN_BASELINE, "--", "Plugins/DiscGolfCharacterFramework"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    plugin_diff = sorted({line.strip().replace("\\", "/")
                          for line in plugin_diff_result.stdout.splitlines()
                          if line.strip()})
    session8_plugin_diff_result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACDMRTUXB",
         SESSION7_PLUGIN_BASELINE, "--", "Plugins/DiscGolfCharacterFramework"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    session8_plugin_diff = sorted({line.strip().replace("\\", "/")
                                   for line in session8_plugin_diff_result.stdout.splitlines()
                                   if line.strip()})
    plugin_status = subprocess.run(
        ["git", "status", "--porcelain=v1", "--untracked-files=all", "--",
         "Plugins/DiscGolfCharacterFramework"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.splitlines()
    plugin_untracked = sorted({line[3:].replace("\\", "/")
                               for line in plugin_status
                               if line.startswith("?? ")})
    _require(checks, "installed_plugin_change_is_exact_accepted_integration_allowlist",
             plugin_diff == sorted(EXPECTED_ACCEPTED_PLUGIN_CHANGES)
             and session8_plugin_diff == sorted(SESSION8_PLUGIN_CHANGES)
             and not plugin_untracked,
             f"baseline={SESSION5_PLUGIN_BASELINE}; "
             f"allowed={sorted(EXPECTED_ACCEPTED_PLUGIN_CHANGES)}; "
             f"actual={plugin_diff}; session8_baseline={SESSION7_PLUGIN_BASELINE}; "
             f"session8_allowed={sorted(SESSION8_PLUGIN_CHANGES)}; "
             f"session8_actual={session8_plugin_diff}; untracked={plugin_untracked}")
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
        "FDGAvatarBackendState ActiveBackendState;",
    )
    avatar_cpp_tokens = (
        "SpawnActorDeferred<AActor>",
        "CandidateActor->SetActorHiddenInGame(true);",
        "CandidateActor->SetActorEnableCollision(false);",
        "CandidateActor->FinishSpawning(SpawnTransform);",
        "const bool bConfigured = ConfigureVisualBackend(",
        "const bool bCandidateReady = bConfigured",
        "CandidateRoot->IsAttachedTo(AnimationSourceMesh)",
        "ActiveBackendState.bVisualReady = true;",
        "OnAvatarBackendReady.Broadcast(ReadyState);",
        "Primitive->SetSimulatePhysics(false);",
        "Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);",
        "TargetMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);",
    )
    base_configure_fails_closed = re.search(
        r"ConfigureVisualBackend_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        avatar_backend_cpp,
        re.DOTALL,
    ) is not None
    base_apply_fails_closed = re.search(
        r"ApplyVisualCustomization_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        avatar_backend_cpp,
        re.DOTALL,
    ) is not None
    _require(checks, "session8_plugin_avatar_backend_is_transactional_and_fail_closed",
             all(token in avatar_backend_header for token in avatar_header_tokens)
             and all(token in avatar_backend_cpp for token in avatar_cpp_tokens)
             and avatar_backend_cpp.count("SpawnActorDeferred<AActor>") == 1
             and avatar_backend_cpp.count("OnAvatarBackendReady.Broadcast(") == 1
             and avatar_backend_cpp.find("const bool bCandidateReady = bConfigured")
             < avatar_backend_cpp.find("SpawnedVisualActor = CandidateActor;")
             and base_configure_fails_closed
             and base_apply_fails_closed,
             "Session 8 plugin delta is limited to deferred hidden candidate setup, "
             "verified-ready commit, presentation safety, and read-only state accessors")
    _require(checks, "session6_plugin_variant_resolution_is_canonical",
             all(token in outfit_component for token in (
                 "const bool bHasVariant = Item->FindVariant(VariantId, Variant);",
                 "if (bHasVariant)",
                 "VariantId = Variant.VariantId;",
                 "VariantId = NAME_None;")),
             "resolved fallback variants persist the catalog's stable canonical ID")
    _require(checks, "session6_plugin_cosmetics_are_collision_free",
             outfit_component.count(
                 "Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);") == 2
             and outfit_component.count(
                 "Comp->SetGenerateOverlapEvents(false);") == 2
             and outfit_component.count(
                 "Comp->SetCanEverAffectNavigation(false);") == 2,
             "skeletal and static cosmetics both disable collision, overlap, and navigation")
    _require(checks, "session6_plugin_static_cosmetics_are_scale_and_physics_isolated",
             outfit_component.count("Comp->SetAbsolute(false, false, true);") == 1
             and outfit_component.count("Comp->SetSimulatePhysics(false);") == 1,
             "static socket cosmetics inherit translation/rotation only and explicitly disable physics")

    failed = [item for item in checks if not item["passed"]]
    report = {
        "schema": "DiscGolfTour.Session3WiringValidation.v2",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS_SESSION3_SINGLE_AUTHORITY_WIRING" if not failed else "FAIL",
        "scope": "RHBH_DRIVE_ONLY_NO_SESSION4",
        "checks": checks,
        "asset_count": len(EXPECTED_ASSETS),
        "release_data_fields_consumed": release_fields,
        "accepted_successor_plugin_change_allowlist": list(
            EXPECTED_ACCEPTED_PLUGIN_CHANGES
        ),
        "session8_plugin_change_allowlist": list(SESSION8_PLUGIN_CHANGES),
        "plugin_source_changes": plugin_diff,
        "session8_plugin_source_changes": session8_plugin_diff,
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
        "release_fields=GripWorldTransform plugin_changes=5 "
        "session6_session7_session8_plugin_safety_allowlist=1"
    )


if __name__ == "__main__":
    main()
