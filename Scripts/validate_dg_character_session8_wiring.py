#!/usr/bin/env python3
"""Static, fail-closed Session 8 avatar-backend/cook wiring validation.

This checkpoint deliberately accepts only dormant MetaHuman groundwork. The
Session 7 DG proxy remains authoritative because MetaHuman Creator Core Data
and the canonical project MetaHuman assets are absent. The validator launches
no Unreal process or build tool and writes only its JSON report under Saved.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Sequence


DEFAULT_ROOT = Path(__file__).absolute().parents[1]
DEFAULT_REPORT = (
    DEFAULT_ROOT / "Saved/CharacterFramework/Session8WiringValidation.json"
)
SESSION5_CHECKPOINT = "e6e6a57727411a4cc50b890f4d8557bfb803e9e2"
SESSION7_CHECKPOINT = "2c54be19a119264d42f11db5470399e021d050cd"

SESSION6_SESSION7_PLUGIN_PATHS = {
    (
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Private/DiscGolfCharacterCustomizationComponent.cpp"
    ),
    (
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Private/DiscGolfOutfitComponent.cpp"
    ),
    (
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Public/DiscGolfCharacterTypes.h"
    ),
}
SESSION8_PLUGIN_PATHS = {
    (
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Private/DiscGolfAvatarBackendComponent.cpp"
    ),
    (
        "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
        "Public/DiscGolfAvatarBackendComponent.h"
    ),
}
EXPECTED_PLUGIN_PATHS = SESSION6_SESSION7_PLUGIN_PATHS | SESSION8_PLUGIN_PATHS

RUNTIME_HEADER = "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.h"
RUNTIME_CPP = "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.cpp"
VISUAL_INTERFACE = "Source/DiscGolfTour/DiscGolfMetaHumanVisualContract.h"
ADAPTER_HEADER = "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.h"
ADAPTER_CPP = "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.cpp"
COOK_HEADER = "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.h"
COOK_CPP = "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.cpp"
RUNTIME_TESTS = "Source/DiscGolfTour/Tests/DiscGolfAvatarBackendRuntimeTests.cpp"
PAWN_HEADER = "Source/DiscGolfTour/DiscGolferPawn.h"
PAWN_CPP = "Source/DiscGolfTour/DiscGolferPawn.cpp"
SAVE_HEADER = "Source/DiscGolfTour/DiscGolfSaveGame.h"
UPROJECT = "DiscGolfTour.uproject"
RUNTIME_BUILD = "Source/DiscGolfTour/DiscGolfTour.Build.cs"
EDITOR_BUILD = "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs"
PLUGIN_DESCRIPTOR = "Plugins/DiscGolfCharacterFramework/DiscGolfCharacterFramework.uplugin"
DEFAULT_GAME = "Config/DefaultGame.ini"
COOK_SPEC = "Config/DG_RuntimeCookManifest.json"
AVAILABILITY_AUDIT = "Scripts/audit_dg_character_session8_availability.py"
COOK_AUTHOR = "Scripts/create_dg_character_session8_cook_assets.py"
COOK_VALIDATOR = "Scripts/validate_dg_character_session8_cook_assets.py"
COOK_NO_WRITE = "Scripts/validate_dg_character_session8_cook_no_write.py"

BUILDKIT_AVATAR_SCHEMA = (
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/"
    "DG_AvatarBackendSchema.json"
)
BUILDKIT_QUALITY = (
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_QualityProfiles.json"
)

DGMASTER_ID = "dg_master"
METAHUMAN_ID = "metahuman_assembled"
QUALITY_IDS = {"Prototype", "GameplayHigh", "GameplayPerformance", "Showcase"}
COOK_MANIFEST_PACKAGE = "/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest"
DGMASTER_PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster"
)
METAHUMAN_PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default"
)
METAHUMAN_ACTOR_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default"
)
METAHUMAN_RETARGET_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman"
)
METAHUMAN_BLOCKED_STATUS = "BLOCKED"
METAHUMAN_CORE_DATA_STATUS = (
    "MISSING_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT"
)
METAHUMAN_CORE_DATA_BLOCKER = (
    "METAHUMAN_CREATOR_CORE_DATA_OPTIONAL_PAYLOAD_MISSING"
)
EXPECTED_AVAILABILITY_BLOCKERS = {
    "METAHUMAN_CREATOR_NOT_EXPLICITLY_ENABLED_IN_PROJECT",
    METAHUMAN_CORE_DATA_BLOCKER,
    "NO_PROJECT_METAHUMAN_NAMED_PACKAGES",
    "NO_CANONICAL_METAHUMAN_BACKEND_PROFILE",
    "NO_CANONICAL_ASSEMBLED_METAHUMAN_VISUAL_ACTOR",
    "NO_CANONICAL_DGMASTER_TO_METAHUMAN_RETARGET",
}

REQUIRED_FILES = (
    RUNTIME_HEADER,
    RUNTIME_CPP,
    VISUAL_INTERFACE,
    ADAPTER_HEADER,
    ADAPTER_CPP,
    COOK_HEADER,
    COOK_CPP,
    RUNTIME_TESTS,
    PAWN_HEADER,
    PAWN_CPP,
    SAVE_HEADER,
    UPROJECT,
    RUNTIME_BUILD,
    EDITOR_BUILD,
    PLUGIN_DESCRIPTOR,
    DEFAULT_GAME,
    COOK_SPEC,
    AVAILABILITY_AUDIT,
    COOK_AUTHOR,
    COOK_VALIDATOR,
    COOK_NO_WRITE,
    BUILDKIT_AVATAR_SCHEMA,
    BUILDKIT_QUALITY,
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    (
        "Content/DiscGolf/Characters/Customization/Head/"
        "SK_DG_Head_Proxy.uasset"
    ),
    (
        "Content/DiscGolf/Characters/Customization/Data/"
        "DA_DG_CosmeticCatalog.uasset"
    ),
    "Content/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.uasset",
)

PROTECTED_AUTHORITY_PATHS = (
    "Source/DiscGolfTour/DiscActor.h",
    "Source/DiscGolfTour/DiscActor.cpp",
    "Source/DiscGolfTour/DiscBagComponent.h",
    "Source/DiscGolfTour/DiscBagComponent.cpp",
    "Source/DiscGolfTour/DiscFlightComponent.h",
    "Source/DiscGolfTour/DiscFlightComponent.cpp",
    "Source/DiscGolfTour/ThrowControllerComponent.h",
    "Source/DiscGolfTour/ThrowControllerComponent.cpp",
    "Source/DiscGolfTour/DiscGolfSaveGame.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
)
SESSION8_COOK_RUNNER_GAMEMODE_PATHS = (
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
)
SESSION8_COOK_RUNNER_EXPECTED_ADDED_LINES = Counter({
    '{': 2,
    '}': 2,
    '&& FParse::Param(': 1,
    '&ADiscGolfSession8CookClosureRunner::Start),': 1,
    '#include "DiscGolfSession8CookClosureRunner.h"': 1,
    '|| bSession7FullCharacterValidationNoSave': 1,
    '|| bSession8CookClosureSmokeRequested': 1,
    '|| bSession8CookClosureValidationNoSave) return;': 1,
    '0.5f,': 1,
    'class ADiscGolfSession8CookClosureRunner;': 1,
    'const bool bSession8CookClosureSmokeRequested = FParse::Param(': 1,
    'const bool bSession8CookClosureValidationNoSave = FParse::Param(': 1,
    'else if (bSession7FullCharacterVisualCaptureRequested)': 1,
    'false);': 1,
    'FCommandLine::Get(), TEXT("Session8CookClosureSmokeTest"));': 2,
    'FCommandLine::Get(), TEXT("Session8ValidationNoSave"))': 1,
    'FPlatformMisc::RequestExitWithStatus(false, 1);': 1,
    'FTimerDelegate::CreateUObject(': 1,
    'FTimerHandle Session8CookClosureTimer;': 1,
    'GetWorld()->SpawnActor<ADiscGolfSession8CookClosureRunner>();': 1,
    'GetWorldTimerManager().SetTimer(': 1,
    'if (!Session8CookClosureRunner)': 1,
    'if (bSession8CookClosureSmokeRequested)': 1,
    'return;': 1,
    'Session8CookClosureRunner =': 1,
    'Session8CookClosureRunner,': 1,
    'Session8CookClosureTimer,': 1,
    'TEXT("DG_SESSION8_COOK_CLOSURE_SMOKE: FAIL reason=runner could not spawn"));': 1,
    'UE_LOG(LogDiscGolfTour, Error,': 1,
    'UPROPERTY() TObjectPtr<ADiscGolfSession8CookClosureRunner> Session8CookClosureRunner;': 1,
})
SESSION8_COOK_RUNNER_EXPECTED_REMOVED_LINES = Counter({
    '|| bSession7FullCharacterValidationNoSave) return;': 1,
    'if (bSession7FullCharacterVisualCaptureRequested)': 1,
})


@dataclass
class Check:
    check_id: str
    passed: bool
    summary: str
    evidence: Any


class Validator:
    def __init__(self, root: Path):
        self.root = root
        self.checks: list[Check] = []
        self._text_cache: dict[str, str] = {}

    def text(self, relative: str) -> str:
        if relative not in self._text_cache:
            path = self.root / relative
            self._text_cache[relative] = (
                path.read_text(encoding="utf-8", errors="replace")
                if path.is_file() else ""
            )
        return self._text_cache[relative]

    def json(self, relative: str) -> dict:
        try:
            value = json.loads(self.text(relative))
        except json.JSONDecodeError:
            return {}
        return value if isinstance(value, dict) else {}

    def add(
        self,
        check_id: str,
        passed: bool,
        summary: str,
        evidence: Any,
    ) -> None:
        self.checks.append(Check(check_id, bool(passed), summary, evidence))


def _git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30.0,
        check=False,
    )


def _git_paths(result: subprocess.CompletedProcess[str]) -> set[str]:
    return {
        line.strip().replace("\\", "/")
        for line in result.stdout.splitlines() if line.strip()
    }


def _git_changed_line_counters(
    result: subprocess.CompletedProcess[str],
) -> tuple[Counter[str], Counter[str]]:
    added: Counter[str] = Counter()
    removed: Counter[str] = Counter()
    for raw_line in result.stdout.splitlines():
        if raw_line.startswith("+++") or raw_line.startswith("---"):
            continue
        if raw_line.startswith("+"):
            added[raw_line[1:].strip()] += 1
        elif raw_line.startswith("-"):
            removed[raw_line[1:].strip()] += 1
    return added, removed


def _resolve_engine_root(project: dict, explicit: str | None) -> tuple[Path, str]:
    if explicit:
        return Path(explicit).expanduser().absolute(), "COMMAND_LINE"
    environment = os.environ.get("DGT_UE_ENGINE_ROOT")
    if environment:
        return Path(environment).expanduser().absolute(), "DGT_UE_ENGINE_ROOT"
    association = str(project.get("EngineAssociation", "5.8"))
    candidates = (
        Path(f"C:/Program Files/Epic Games/UE_{association}"),
        Path(f"D:/Epic Games/UE_{association}"),
    )
    for candidate in candidates:
        if (candidate / "Engine/Build/Build.version").is_file():
            return candidate, "PROJECT_ASSOCIATION_WELL_KNOWN_PATH"
    return candidates[0], "UNRESOLVED_PROJECT_ASSOCIATION_WELL_KNOWN_PATH"


def _package_file(root: Path, package: str) -> Path:
    return root / "Content" / (package.removeprefix("/Game/") + ".uasset")


def _flatten_groups(spec: dict, key: str) -> list[str]:
    return [
        package
        for group in spec.get(key, []) if isinstance(group, dict)
        for package in group.get("packages", []) if isinstance(package, str)
    ]


def _validate_required_files(v: Validator) -> None:
    missing = [relative for relative in REQUIRED_FILES if not (v.root / relative).is_file()]
    v.add(
        "source.required_files",
        not missing,
        "All frozen Session 8 source, cook-tool, and proxy authority inputs exist",
        {"required_count": len(REQUIRED_FILES), "missing": missing},
    )


def _validate_schema_and_authority(v: Validator) -> None:
    save_header = v.text(SAVE_HEADER)
    protected_diff = _git(
        v.root,
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION7_CHECKPOINT, "--", *PROTECTED_AUTHORITY_PATHS,
    )
    protected_status = _git(
        v.root,
        "status", "--porcelain=v1", "--untracked-files=all", "--",
        *PROTECTED_AUTHORITY_PATHS,
    )
    protected_changes = _git_paths(protected_diff)
    protected_worktree = [
        line for line in protected_status.stdout.splitlines() if line.strip()
    ]
    game_mode_paths_diff = _git(
        v.root,
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION7_CHECKPOINT, "--", *SESSION8_COOK_RUNNER_GAMEMODE_PATHS,
    )
    game_mode_line_diff = _git(
        v.root,
        "diff", "--no-ext-diff", "--unified=0",
        SESSION7_CHECKPOINT, "--", *SESSION8_COOK_RUNNER_GAMEMODE_PATHS,
    )
    game_mode_added, game_mode_removed = _git_changed_line_counters(
        game_mode_line_diff
    )
    game_mode_changed_paths = _git_paths(game_mode_paths_diff)
    game_mode_cpp = v.text(SESSION8_COOK_RUNNER_GAMEMODE_PATHS[1])
    game_mode_header = v.text(SESSION8_COOK_RUNNER_GAMEMODE_PATHS[0])
    no_save_conjunction = re.search(
        r"const bool bSession8CookClosureValidationNoSave\s*=\s*"
        r"FParse::Param\(\s*FCommandLine::Get\(\),\s*"
        r'TEXT\("Session8ValidationNoSave"\)\)\s*&&\s*'
        r"FParse::Param\(\s*FCommandLine::Get\(\),\s*"
        r'TEXT\("Session8CookClosureSmokeTest"\)\);',
        game_mode_cpp,
        re.DOTALL,
    )
    cook_runner_hook_ok = (
        game_mode_cpp.count(
            '#include "DiscGolfSession8CookClosureRunner.h"'
        ) == 1
        and game_mode_cpp.count(
            'TEXT("Session8CookClosureSmokeTest")'
        ) == 2
        and game_mode_cpp.count(
            'TEXT("Session8ValidationNoSave")'
        ) == 1
        and game_mode_cpp.count(
            "SpawnActor<ADiscGolfSession8CookClosureRunner>()"
        ) == 1
        and game_mode_cpp.count(
            "&ADiscGolfSession8CookClosureRunner::Start"
        ) == 1
        and game_mode_header.count(
            "class ADiscGolfSession8CookClosureRunner;"
        ) == 1
        and game_mode_header.count(
            "TObjectPtr<ADiscGolfSession8CookClosureRunner> "
            "Session8CookClosureRunner;"
        ) == 1
        and no_save_conjunction is not None
    )
    schema_ok = (
        "inline constexpr int32 CurrentVersion = 9;" in save_header
        and "SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion" in save_header
        and "CurrentVersion = 10" not in save_header
    )
    v.add(
        "authority.schema9_and_core_owners_unchanged",
        schema_ok
        and protected_diff.returncode == 0
        and protected_status.returncode == 0
        and not protected_changes
        and not protected_worktree
        and game_mode_paths_diff.returncode == 0
        and game_mode_line_diff.returncode == 0
        and game_mode_changed_paths == set(SESSION8_COOK_RUNNER_GAMEMODE_PATHS)
        and game_mode_added == SESSION8_COOK_RUNNER_EXPECTED_ADDED_LINES
        and game_mode_removed == SESSION8_COOK_RUNNER_EXPECTED_REMOVED_LINES
        and cook_runner_hook_ok,
        "Schema 9 and gameplay owners remain protected; GameMode has only the exact cook-runner/no-save hook",
        {
            "schema9": schema_ok,
            "baseline": SESSION7_CHECKPOINT,
            "protected_paths": list(PROTECTED_AUTHORITY_PATHS),
            "committed_changes": sorted(protected_changes),
            "worktree_changes": protected_worktree,
            "diff_return_code": protected_diff.returncode,
            "status_return_code": protected_status.returncode,
            "allowed_game_mode_paths": list(
                SESSION8_COOK_RUNNER_GAMEMODE_PATHS
            ),
            "actual_game_mode_paths": sorted(game_mode_changed_paths),
            "game_mode_added_lines": dict(sorted(game_mode_added.items())),
            "game_mode_removed_lines": dict(sorted(game_mode_removed.items())),
            "game_mode_added_lines_exact": (
                game_mode_added
                == SESSION8_COOK_RUNNER_EXPECTED_ADDED_LINES
            ),
            "game_mode_removed_lines_exact": (
                game_mode_removed
                == SESSION8_COOK_RUNNER_EXPECTED_REMOVED_LINES
            ),
            "cook_runner_hook_exact": cook_runner_hook_ok,
            "two_flag_no_save_conjunction": no_save_conjunction is not None,
        },
    )

    source_files = sorted(
        list((v.root / "Source/DiscGolfTour").rglob("*.h"))
        + list((v.root / "Source/DiscGolfTour").rglob("*.cpp"))
    )
    combined = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in source_files
    )
    owner_counts = {
        "pawn_classes": len(re.findall(r":\s*public\s+APawn\b", combined)),
        "disc_bag_subobjects": combined.count(
            "CreateDefaultSubobject<UDiscBagComponent>"
        ),
        "flight_subobjects": combined.count(
            "CreateDefaultSubobject<UDiscFlightComponent>"
        ),
        "framework_throw_subobjects": combined.count(
            "CreateDefaultSubobject<UDiscGolfThrowComponent>"
        ),
        "rhbh_adapter_subobjects": combined.count(
            "CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>"
        ),
    }
    expected_counts = {
        "pawn_classes": 1,
        "disc_bag_subobjects": 1,
        "flight_subobjects": 1,
        "framework_throw_subobjects": 1,
        "rhbh_adapter_subobjects": 1,
    }
    session8_sources = "\n".join(
        v.text(relative)
        for relative in (
            RUNTIME_HEADER, RUNTIME_CPP, VISUAL_INTERFACE,
            ADAPTER_HEADER, ADAPTER_CPP, COOK_HEADER, COOK_CPP,
        )
    )
    authority_pattern = re.compile(
        r"CreateDefaultSubobject\s*<\s*(?:UDiscBagComponent|"
        r"UDiscFlightComponent|UDiscGolfThrowComponent|"
        r"UDiscGolfRHBHThrowAdapterComponent)|"
        r"SpawnActor\s*<\s*ADiscActor|ResolveThrowRelease\s*\(|"
        r"RequestThrowFromGrip\s*\(|ActiveDisc\s*->\s*Throw\s*\(|"
        r"SaveGameToSlot\s*\(|LoadGameFromSlot\s*\(|AddStroke\s*\(",
        re.IGNORECASE,
    )
    authority_hits = [
        {"line": number, "text": line.strip()}
        for number, line in enumerate(session8_sources.splitlines(), 1)
        if authority_pattern.search(line)
    ]
    v.add(
        "authority.no_second_pawn_release_flight_or_inventory",
        owner_counts == expected_counts and not authority_hits,
        "Session 8 adds presentation/cook discovery only and no parallel gameplay owner",
        {
            "expected_owner_counts": expected_counts,
            "actual_owner_counts": owner_counts,
            "session8_authority_hits": authority_hits,
        },
    )


def _validate_plugin_isolation_and_delta(v: Validator) -> None:
    project = v.json(UPROJECT)
    enabled = {
        entry.get("Name")
        for entry in project.get("Plugins", [])
        if isinstance(entry, dict) and entry.get("Enabled") is True
    }
    forbidden_enabled = sorted(
        name for name in enabled if isinstance(name, str) and (
            name.casefold().startswith("metahuman")
            or name in {"HairStrands", "RigLogic"}
        )
    )
    build_sources = "\n".join(
        (v.text(RUNTIME_BUILD), v.text(EDITOR_BUILD), v.text(PLUGIN_DESCRIPTOR))
    )
    forbidden_module_tokens = sorted({
        token for token in ("MetaHuman", "HairStrands", "RigLogic")
        if re.search(rf'"{re.escape(token)}(?:[A-Za-z0-9_]*)?"', build_sources)
    })
    v.add(
        "dependency.no_metahuman_module_or_plugin_enablement",
        not forbidden_enabled and not forbidden_module_tokens,
        "Dormant adapter has no hard MetaHuman/HairStrands/RigLogic dependency or enablement",
        {
            "explicit_enabled_plugins": sorted(enabled),
            "forbidden_enabled_plugins": forbidden_enabled,
            "forbidden_module_tokens": forbidden_module_tokens,
        },
    )

    full_diff = _git(
        v.root,
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION5_CHECKPOINT, "--", "Plugins/DiscGolfCharacterFramework",
    )
    session8_diff = _git(
        v.root,
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION7_CHECKPOINT, "--", "Plugins/DiscGolfCharacterFramework",
    )
    plugin_status = _git(
        v.root,
        "status", "--porcelain=v1", "--untracked-files=all", "--",
        "Plugins/DiscGolfCharacterFramework",
    )
    full_paths = _git_paths(full_diff)
    session8_paths = _git_paths(session8_diff)
    untracked = [
        line for line in plugin_status.stdout.splitlines()
        if line.startswith("?? ")
    ]
    v.add(
        "plugin.exact_layered_five_file_delta",
        full_diff.returncode == 0
        and session8_diff.returncode == 0
        and plugin_status.returncode == 0
        and full_paths == EXPECTED_PLUGIN_PATHS
        and session8_paths == SESSION8_PLUGIN_PATHS
        and not untracked,
        "Plugin drift is exactly three accepted S6/S7 files plus two S8 avatar files",
        {
            "session5_baseline": SESSION5_CHECKPOINT,
            "session7_baseline": SESSION7_CHECKPOINT,
            "expected_all": sorted(EXPECTED_PLUGIN_PATHS),
            "actual_all": sorted(full_paths),
            "expected_session8": sorted(SESSION8_PLUGIN_PATHS),
            "actual_session8": sorted(session8_paths),
            "untracked": untracked,
        },
    )

    plugin_header = v.text(next(
        path for path in SESSION8_PLUGIN_PATHS if path.endswith(".h")
    ))
    plugin_cpp = v.text(next(
        path for path in SESSION8_PLUGIN_PATHS if path.endswith(".cpp")
    ))
    header_tokens = (
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
    cpp_tokens = (
        "SpawnActorDeferred<AActor>",
        "CandidateActor->SetActorHiddenInGame(true);",
        "CandidateActor->SetActorEnableCollision(false);",
        "CandidateActor->FinishSpawning(SpawnTransform);",
        "const bool bConfigured = ConfigureVisualBackend(",
        "const bool bCandidateReady = bConfigured",
        "CandidateRoot->IsAttachedTo(AnimationSourceMesh)",
        "ActiveBackendState.bVisualReady = true;",
        "OnAvatarBackendReady.Broadcast(ReadyState);",
        "EnforcePresentationOnly(ActiveActor, bWasHidden);",
        "Primitive->SetSimulatePhysics(false);",
        "Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);",
        "TargetMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);",
    )
    missing_header = [token for token in header_tokens if token not in plugin_header]
    missing_cpp = [token for token in cpp_tokens if token not in plugin_cpp]
    base_configure_false = re.search(
        r"ConfigureVisualBackend_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        plugin_cpp,
        re.DOTALL,
    ) is not None
    base_apply_false = re.search(
        r"ApplyVisualCustomization_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        plugin_cpp,
        re.DOTALL,
    ) is not None
    configure_index = plugin_cpp.find(
        "const bool bConfigured = ConfigureVisualBackend("
    )
    ready_index = plugin_cpp.find("const bool bCandidateReady = bConfigured")
    commit_index = plugin_cpp.find("SpawnedVisualActor = CandidateActor;")
    broadcast_index = plugin_cpp.find(
        "OnAvatarBackendReady.Broadcast(ReadyState);"
    )
    v.add(
        "plugin.verified_ready_transaction",
        not missing_header
        and not missing_cpp
        and base_configure_false
        and base_apply_false
        and plugin_cpp.count("SpawnActorDeferred<AActor>") == 1
        and plugin_cpp.count("OnAvatarBackendReady.Broadcast(") == 1
        and "SpawnActor<AActor>" not in plugin_cpp
        and 0 <= configure_index < ready_index < commit_index < broadcast_index,
        "Installed plugin commits and broadcasts only a verified hidden deferred candidate",
        {
            "missing_header_tokens": missing_header,
            "missing_cpp_tokens": missing_cpp,
            "native_configure_fails_closed": base_configure_false,
            "native_apply_fails_closed": base_apply_false,
            "deferred_spawn_count": plugin_cpp.count("SpawnActorDeferred<AActor>"),
            "ready_broadcast_count": plugin_cpp.count(
                "OnAvatarBackendReady.Broadcast("
            ),
            "order": [configure_index, ready_index, commit_index, broadcast_index],
        },
    )


def _validate_backend_policy(v: Validator) -> None:
    schema = v.json(BUILDKIT_AVATAR_SCHEMA)
    quality = v.json(BUILDKIT_QUALITY)
    backend_ids = [
        entry.get("id") for entry in schema.get("backends", [])
        if isinstance(entry, dict)
    ]
    quality_ids = [
        entry.get("id") for entry in quality.get("profiles", [])
        if isinstance(entry, dict)
    ]
    schema_ok = (
        schema.get("version") == 1
        and schema.get("shipping_default") == METAHUMAN_ID
        and backend_ids == [
            DGMASTER_ID,
            METAHUMAN_ID,
            "metahuman_collection_instance_experimental",
        ]
        and quality.get("version") == 1
        and set(quality_ids) == QUALITY_IDS
        and len(quality_ids) == len(QUALITY_IDS)
        and quality.get("shipping_default") == "GameplayHigh"
    )
    v.add(
        "policy.buildkit_backend_and_quality_vocabulary",
        schema_ok,
        "BuildKit backend IDs and four measured-before-CVar quality names remain exact",
        {
            "backend_ids": backend_ids,
            "backend_shipping_default": schema.get("shipping_default"),
            "quality_ids": quality_ids,
            "quality_shipping_default": quality.get("shipping_default"),
            "quality_rule": quality.get("rule"),
        },
    )

    runtime_header = v.text(RUNTIME_HEADER)
    runtime_cpp = v.text(RUNTIME_CPP)
    extracted_quality = re.findall(
        r'TEXT\("(Prototype|GameplayHigh|GameplayPerformance|Showcase)"\)',
        runtime_cpp,
    )
    policy_tokens = (
        'DGMasterBackendId = TEXT("dg_master")',
        'TEXT("metahuman_assembled")',
        DGMASTER_PROFILE_PACKAGE + ".DA_DG_AvatarBackend_DGMaster",
        METAHUMAN_PROFILE_PACKAGE + ".DA_DG_AvatarBackend_MetaHuman_Default",
    )
    contract_tokens = (
        "Profile->BackendId != FName(MetaHumanAssembledBackendId)",
        "Profile->Backend != EDGAvatarBackend::MetaHumanPreset",
        "EDGMetaHumanRuntimeMode::ShippingSafeAssembled",
        "Profile->VisualActorClass.IsNull()",
        "!Profile->bUseRuntimeRetargeting || Profile->RetargetAsset.IsNull()",
        "Profile->VisualBodyComponentTag == Profile->VisualHeadComponentTag",
        "!Session8ContainsQualityProfile(Profile->PreferredQualityProfileId)",
        "Profile->bAllowRuntimeFaceSculpting",
        "Result.bMetaHumanAttemptAllowed = true;",
    )
    v.add(
        "policy.project_resolution_is_canonical_and_fail_closed",
        all(token in runtime_header for token in policy_tokens)
        and all(token in runtime_cpp for token in contract_tokens)
        and set(extracted_quality) == QUALITY_IDS
        and len(extracted_quality) == len(QUALITY_IDS)
        and runtime_cpp.count("Result.bMetaHumanAttemptAllowed = true;") == 1,
        "Project policy recognizes only dg_master/metahuman_assembled and a complete shipping-safe profile",
        {
            "missing_header_tokens": [
                token for token in policy_tokens if token not in runtime_header
            ],
            "missing_contract_tokens": [
                token for token in contract_tokens if token not in runtime_cpp
            ],
            "quality_ids": extracted_quality,
            "attempt_authorization_count": runtime_cpp.count(
                "Result.bMetaHumanAttemptAllowed = true;"
            ),
        },
    )


def _validate_project_adapter(v: Validator) -> None:
    interface = v.text(VISUAL_INTERFACE)
    adapter_header = v.text(ADAPTER_HEADER)
    adapter_cpp = v.text(ADAPTER_CPP)
    pawn_header = v.text(PAWN_HEADER)
    pawn_cpp = v.text(PAWN_CPP)
    all_project_cpp = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in sorted((v.root / "Source/DiscGolfTour").rglob("*.cpp"))
    )
    all_project_headers = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in sorted((v.root / "Source/DiscGolfTour").rglob("*.h"))
    )

    interface_tokens = (
        "UDiscGolfMetaHumanVisualContract : public UInterface",
        "IDiscGolfMetaHumanVisualContract",
        "bool ConfigureFromDGAnimationSource(",
        "USkeletalMeshComponent* AnimationSourceMesh",
        "UDiscGolfAvatarBackendProfile* BackendProfile",
        "const FDGFullCharacterCustomization& Customization",
        "FString& OutStatus",
        "bool ApplyMappedCustomization(",
    )
    v.add(
        "adapter.actor_interface_contract",
        all(token in interface for token in interface_tokens)
        and interface.count("BlueprintImplementableEvent") == 2,
        "Future assembled actor must explicitly configure DG retarget and mapped customization",
        {
            "missing_tokens": [
                token for token in interface_tokens if token not in interface
            ],
            "blueprint_event_count": interface.count("BlueprintImplementableEvent"),
        },
    )

    header_tokens = (
        ": public UDiscGolfAvatarBackendComponent",
        "USkeletalMeshComponent* GetVerifiedVisualBody() const;",
        "USkeletalMeshComponent* GetVerifiedVisualHead() const;",
        "virtual bool ConfigureVisualBackend_Implementation(",
        "virtual bool ApplyVisualCustomization_Implementation(",
        "const UDiscGolfAvatarBackendProfile* ExpectedProfile",
        "TObjectPtr<AActor> VerifiedVisualActor;",
        "TObjectPtr<USkeletalMeshComponent> VerifiedVisualBody;",
        "TObjectPtr<USkeletalMeshComponent> VerifiedVisualHead;",
    )
    cpp_tokens = (
        "Session8FindUniqueTaggedSkeletalMesh(",
        "Candidate->ComponentHasTag(RequiredTag)",
        "UDiscGolfAvatarBackendProfile* CandidateProfile = BackendProfile.Get();",
        "ValidateMetaHumanProfileContract(",
        "UDiscGolfMetaHumanVisualContract::StaticClass()",
        "CandidateProfile->RetargetAsset.LoadSynchronous()",
        "Execute_ConfigureFromDGAnimationSource(",
        "UDiscGolfAvatarBackendProfile* ActiveProfile = GetActiveBackendProfile();",
        "Execute_ApplyMappedCustomization(",
        "VisualActor->GetOwner() != Owner",
        "VisualActor->GetWorld() != AnimationSourceMesh->GetWorld()",
        "!VisualRoot || !VisualRoot->IsAttachedTo(AnimationSourceMesh)",
        "bDuplicateBody || bDuplicateHead || !OutBody || !OutHead",
        "OutBody == OutHead",
        "!OutBody->IsRegistered() || !OutHead->IsRegistered()",
        "!OutBody->GetSkeletalMeshAsset() || !OutHead->GetSkeletalMeshAsset()",
        "!OutBody->GetAnimClass()",
        "Mesh->SetSimulatePhysics(false);",
        "Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);",
        "Mesh->SetGenerateOverlapEvents(false);",
        "Mesh->SetCanEverAffectNavigation(false);",
        "OutBody->AddTickPrerequisiteComponent(AnimationSourceMesh);",
        "OutHead->AddTickPrerequisiteComponent(AnimationSourceMesh);",
    )
    v.add(
        "adapter.verified_actor_body_head_retarget_contract",
        all(token in adapter_header for token in header_tokens)
        and all(token in adapter_cpp for token in cpp_tokens)
        and adapter_cpp.count("Execute_ConfigureFromDGAnimationSource(") == 1
        and adapter_cpp.count("Execute_ApplyMappedCustomization(") == 1
        and adapter_cpp.count("Session8FindUniqueTaggedSkeletalMesh(") == 3,
        "Project adapter verifies interface, profile, retarget, owner/world/root, tagged meshes, AnimBP, and safety",
        {
            "missing_header_tokens": [
                token for token in header_tokens if token not in adapter_header
            ],
            "missing_cpp_tokens": [
                token for token in cpp_tokens if token not in adapter_cpp
            ],
            "configure_contract_calls": adapter_cpp.count(
                "Execute_ConfigureFromDGAnimationSource("
            ),
            "apply_contract_calls": adapter_cpp.count(
                "Execute_ApplyMappedCustomization("
            ),
            "tag_lookup_definition_and_calls": adapter_cpp.count(
                "Session8FindUniqueTaggedSkeletalMesh("
            ),
        },
    )

    create_token = "CreateDefaultSubobject<UDiscGolfMetaHumanAvatarBackendComponent>"
    class_token = ": public UDiscGolfAvatarBackendComponent"
    pawn_diff = _git(
        v.root,
        "diff", "--no-ext-diff", "--unified=0",
        SESSION7_CHECKPOINT, "--", PAWN_HEADER, PAWN_CPP,
    )
    pawn_added, _ = _git_changed_line_counters(pawn_diff)
    session8_adapter_delta = "\n".join(pawn_added.elements()) + "\n" + adapter_cpp
    build_calls = re.findall(
        r"->\s*BuildVisualBackend\s*\(", session8_adapter_delta
    )
    profile_assignments = re.findall(
        r"AvatarBackendComponent\s*->\s*BackendProfile\s*=",
        session8_adapter_delta,
    )
    hide_proxy_hits = [
        line.strip() for line in session8_adapter_delta.splitlines()
        if re.search(
            r"(?:SkeletalMesh|ModularHeadMesh)\s*->\s*(?:SetHiddenInGame|SetVisibility)\s*\(\s*(?:true|false)",
            line,
        )
    ]
    v.add(
        "adapter.exactly_one_dormant_pawn_component",
        all_project_cpp.count(create_token) == 1
        and pawn_cpp.count(create_token) == 1
        and pawn_cpp.count('TEXT("MetaHumanVisualBackend")') == 1
        and all_project_headers.count(class_token) == 1
        and pawn_header.count(
            "TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> AvatarBackendComponent;"
        ) == 1
        and "GetAvatarBackendComponent() const" in pawn_header
        and not build_calls
        and not profile_assignments
        and not hide_proxy_hits,
        "Pawn owns one dormant adapter; no profile/build/hide action can displace the proxy",
        {
            "component_create_count_project": all_project_cpp.count(create_token),
            "component_create_count_pawn": pawn_cpp.count(create_token),
            "component_name_count": pawn_cpp.count('TEXT("MetaHumanVisualBackend")'),
            "project_adapter_subclass_count": all_project_headers.count(class_token),
            "build_calls": len(build_calls),
            "profile_assignments": len(profile_assignments),
            "new_proxy_visibility_writes": hide_proxy_hits,
            "pawn_diff_return_code": pawn_diff.returncode,
        },
    )


def _validate_cook_contract(v: Validator) -> None:
    spec = v.json(COOK_SPEC)
    runtime = _flatten_groups(spec, "runtime_package_groups")
    excluded = _flatten_groups(spec, "excluded_package_groups")
    runtime_set = set(runtime)
    excluded_set = set(excluded)
    missing_runtime = sorted(
        package for package in runtime if not _package_file(v.root, package).is_file()
    )
    missing_excluded = sorted(
        package for package in excluded if not _package_file(v.root, package).is_file()
    )
    outputs = spec.get("asset_outputs", {})
    manifest_output = outputs.get("runtime_cook_manifest", {})
    dgmaster_output = outputs.get("dg_master_backend_profile", {})
    reserved = spec.get("reserved_optional_metahuman_entry_points", {})
    contract_ok = (
        spec.get("schema") == "DiscGolfTour.RuntimeCookManifestSource.v1"
        and spec.get("schema_version") == 1
        and spec.get("manifest_id") == "dg_runtime_v1"
        and spec.get("expected_runtime_package_count") == 69
        and len(runtime) == 69
        and len(runtime_set) == 69
        and spec.get("expected_excluded_package_count") == 12
        and len(excluded) == 12
        and len(excluded_set) == 12
        and not (runtime_set & excluded_set)
        and all(package.startswith("/Game/DiscGolf/") for package in runtime + excluded)
        and not missing_runtime
        and not missing_excluded
        and manifest_output.get("package") == COOK_MANIFEST_PACKAGE
        and dgmaster_output.get("package") == DGMASTER_PROFILE_PACKAGE
        and dgmaster_output.get("backend_id") == DGMASTER_ID
        and dgmaster_output.get("backend") == "DGMaster"
        and dgmaster_output.get("preferred_quality_profile_id") == "Prototype"
        and dgmaster_output.get("visual_actor_class") is None
        and dgmaster_output.get("retarget_asset") is None
        and dgmaster_output.get("use_runtime_retargeting") is False
        and dgmaster_output.get("runtime_face_sculpting") is False
        and reserved.get("backend_profile")
            == METAHUMAN_PROFILE_PACKAGE + ".DA_DG_AvatarBackend_MetaHuman_Default"
        and reserved.get("assembled_visual_actor_class")
            == METAHUMAN_ACTOR_PACKAGE + ".BP_DG_MetaHuman_Default_C"
        and reserved.get("retarget_asset")
            == METAHUMAN_RETARGET_PACKAGE + ".RTG_DGMaster_To_MetaHuman"
        and reserved.get("authoring_status")
            == "BLOCKED_UNTIL_REAL_PROJECT_METAHUMAN_CONTENT_EXISTS"
        and reserved.get("created_by_session8_cook_author") is False
        and not any("/MetaHuman/" in package for package in runtime)
        and not any("/Tests/" in package or "/Mocap/" in package for package in runtime)
    )
    v.add(
        "cook.exact_69_runtime_12_excluded_source_manifest",
        contract_ok,
        "Cook discovery contains the exact accepted proxy closure and only a DGMaster backend profile",
        {
            "schema": spec.get("schema"),
            "runtime_count": len(runtime),
            "runtime_unique_count": len(runtime_set),
            "excluded_count": len(excluded),
            "excluded_unique_count": len(excluded_set),
            "overlap": sorted(runtime_set & excluded_set),
            "missing_runtime": missing_runtime,
            "missing_excluded": missing_excluded,
            "manifest_output": manifest_output,
            "dgmaster_output": dgmaster_output,
            "reserved_optional_metahuman": reserved,
        },
    )

    default_game = v.text(DEFAULT_GAME)
    cook_header = v.text(COOK_HEADER)
    cook_cpp = v.text(COOK_CPP)
    cook_author = v.text(COOK_AUTHOR)
    cook_validator = v.text(COOK_VALIDATOR)
    cook_no_write = v.text(COOK_NO_WRITE)
    config_tokens = (
        'PrimaryAssetType="DGRuntimeCookManifest"',
        'AssetBaseClass="/Script/DiscGolfTour.DiscGolfRuntimeCookManifest"',
        'Path="/Game/DiscGolf/Cook"',
        'PrimaryAssetType="DiscGolfAvatarBackendProfile"',
        (
            'AssetBaseClass="/Script/DiscGolfCharacterFramework.'
            'DiscGolfAvatarBackendProfile"'
        ),
        'Path="/Game/DiscGolf/Characters/Avatar/Data"',
    )
    source_tokens = (
        "UDiscGolfRuntimeCookManifest : public UPrimaryDataAsset",
        'meta=(AssetBundles="Runtime")',
        "TArray<TSoftObjectPtr<UObject>> RuntimeAssets;",
        "TArray<TSoftObjectPtr<UDiscGolfAvatarBackendProfile>> AvatarBackendProfiles;",
        "TArray<FString> ExplicitlyExcludedPackages;",
        'TEXT("DGRuntimeCookManifest")',
        "ValidateRuntimeContract",
        "ExpectedRuntimePackageCount != 69",
        "ExpectedExcludedPackageCount != 12",
        DGMASTER_PROFILE_PACKAGE,
        "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    )
    tool_tokens = {
        COOK_AUTHOR: (
            "PASS_CREATED_AND_SAVED",
            "PASS_ALREADY_CURRENT_NO_ASSET_WRITES",
            "METAHUMAN_ASSETS_CREATED = 0",
            "EXPECTED_MANIFEST_OBJECT",
            "EXPECTED_DGMASTER_PROFILE_OBJECT",
            'outputs = spec["asset_outputs"]',
            'manifest_output = outputs["runtime_cook_manifest"]',
            'profile_output = outputs["dg_master_backend_profile"]',
        ),
        COOK_VALIDATOR: (
            "PASS_NO_DISK_MUTATION",
            "EXPECTED_METAHUMAN_PROFILE_OBJECT",
            "EXPECTED_MANIFEST_OBJECT",
            "EXPECTED_DGMASTER_PROFILE_OBJECT",
        ),
        COOK_NO_WRITE: (
            "PASS_NO_WRITE",
            "asset_registry_mutation",
            "EXPECTED_MANIFEST_OBJECT",
            "EXPECTED_DGMASTER_PROFILE_OBJECT",
            'strict = runpy.run_path(',
            'validation = strict["validate"]()',
        ),
    }
    missing_tool_tokens = {
        path: [token for token in tokens if token not in source]
        for path, tokens, source in (
            (COOK_AUTHOR, tool_tokens[COOK_AUTHOR], cook_author),
            (COOK_VALIDATOR, tool_tokens[COOK_VALIDATOR], cook_validator),
            (COOK_NO_WRITE, tool_tokens[COOK_NO_WRITE], cook_no_write),
        )
    }
    missing_tool_tokens = {
        path: missing for path, missing in missing_tool_tokens.items() if missing
    }
    v.add(
        "cook.asset_manager_class_and_source_only_tools",
        all(token in default_game for token in config_tokens)
        and all(token in cook_header + "\n" + cook_cpp for token in source_tokens)
        and not missing_tool_tokens,
        "Primary-asset roots and source/author/validator/no-write contracts use the canonical paths",
        {
            "missing_config_tokens": [
                token for token in config_tokens if token not in default_game
            ],
            "missing_source_tokens": [
                token for token in source_tokens
                if token not in cook_header + "\n" + cook_cpp
            ],
            "missing_tool_tokens": missing_tool_tokens,
        },
    )


def _validate_tests(v: Validator) -> None:
    tests = v.text(RUNTIME_TESTS)
    tokens = (
        "DiscGolfTour.Character.Session8.AvatarBackend.FailClosedResolution",
        "DiscGolfTour.Character.Session8.AvatarBackend.ProfileContract",
        "ResolveBackend(NAME_None, nullptr)",
        "Unknown IDs fail closed to DG master",
        "Missing content cannot be reported ready",
        "MetaHumanAssembledBackendId",
        METAHUMAN_ACTOR_PACKAGE + ".BP_DG_MetaHuman_Default_C",
        METAHUMAN_RETARGET_PACKAGE + ".RTG_DGMaster_To_MetaHuman",
        'Profile->PreferredQualityProfileId = TEXT("GameplayHigh")',
        "Profile->bAllowRuntimeFaceSculpting = false;",
        "Metadata success permits only a later verified build attempt",
        "Unsupported runtime sculpting fails closed",
        "Unmeasured quality IDs fail closed",
    )
    v.add(
        "tests.fail_closed_resolution_and_profile_contract",
        all(token in tests for token in tokens)
        and tests.count("IMPLEMENT_SIMPLE_AUTOMATION_TEST") == 2,
        "Automation coverage proves fallback and metadata-attempt semantics without claiming visual readiness",
        {
            "missing_tokens": [token for token in tokens if token not in tests],
            "test_count": tests.count("IMPLEMENT_SIMPLE_AUTOMATION_TEST"),
        },
    )


def _validate_blocked_availability(
    v: Validator,
    engine_root: Path,
    engine_resolution: str,
) -> dict:
    plugin_root = engine_root / "Engine/Plugins/MetaHuman/MetaHumanCharacter"
    descriptor_path = plugin_root / "MetaHumanCharacter.uplugin"
    build_version_path = engine_root / "Engine/Build/Build.version"
    try:
        build_version = json.loads(build_version_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        build_version = {}
    try:
        descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        descriptor = {}

    optional = plugin_root / "Content/Optional"
    texture_synthesis = optional / "TextureSynthesis"
    body_textures = optional / "BodyTextures"
    ar_files = list(texture_synthesis.rglob("*.ar")) \
        if texture_synthesis.is_dir() else []
    body_uassets = list(body_textures.glob("*.uasset")) \
        if body_textures.is_dir() else []
    core_data_ready = (
        optional.is_dir()
        and texture_synthesis.is_dir()
        and body_textures.is_dir()
        and bool(ar_files)
        and bool(body_uassets)
    )

    project = v.json(UPROJECT)
    project_plugins = {
        entry.get("Name"): entry.get("Enabled")
        for entry in project.get("Plugins", []) if isinstance(entry, dict)
    }
    creator_explicitly_enabled = project_plugins.get("MetaHumanCharacter") is True
    canonical_presence = {
        "backend_profile": _package_file(v.root, METAHUMAN_PROFILE_PACKAGE).is_file(),
        "assembled_actor": _package_file(v.root, METAHUMAN_ACTOR_PACKAGE).is_file(),
        "retarget": _package_file(v.root, METAHUMAN_RETARGET_PACKAGE).is_file(),
    }
    project_assets_absent = not any(canonical_presence.values())
    blocked_truth = (
        (build_version.get("MajorVersion"), build_version.get("MinorVersion"))
            == (5, 8)
        and descriptor.get("FriendlyName") == "MetaHuman Creator"
        and descriptor.get("VersionName") == "1.0.0"
        and descriptor.get("EnabledByDefault") is False
        and descriptor.get("IsBetaVersion") is True
        and not core_data_ready
        and not creator_explicitly_enabled
        and project_assets_absent
    )
    availability_audit = v.text(AVAILABILITY_AUDIT)
    audit_tokens = (
        "DiscGolfTour.Session8AvailabilityAudit.v1",
        METAHUMAN_BLOCKED_STATUS,
        METAHUMAN_CORE_DATA_STATUS,
        METAHUMAN_CORE_DATA_BLOCKER,
        "Content/Optional",
        "TextureSynthesis",
        "BodyTextures",
        "metahuman_assets_created",
        "unreal_launched",
        "ubt_launched",
        "PREMIUM_DISC_GOLF_ART_OUT_OF_SESSION8_SCOPE_AND_NOT_BUNDLED",
        *sorted(EXPECTED_AVAILABILITY_BLOCKERS),
    )
    v.add(
        "availability.exact_missing_core_data_and_project_assets_block",
        blocked_truth and all(token in availability_audit for token in audit_tokens),
        "MetaHuman shell is installed, but official Optional Core Data, enablement, and canonical project assets remain absent",
        {
            "engine_root": str(engine_root),
            "engine_root_resolution": engine_resolution,
            "engine_build": build_version,
            "plugin_descriptor_present": descriptor_path.is_file(),
            "plugin_friendly_name": descriptor.get("FriendlyName"),
            "plugin_version": descriptor.get("VersionName"),
            "plugin_beta": descriptor.get("IsBetaVersion"),
            "plugin_enabled_by_default": descriptor.get("EnabledByDefault"),
            "core_data": {
                "optional_directory": optional.is_dir(),
                "texture_synthesis_directory": texture_synthesis.is_dir(),
                "body_textures_directory": body_textures.is_dir(),
                "texture_synthesis_ar_count": len(ar_files),
                "body_texture_uasset_count": len(body_uassets),
                "ready_by_ue58_module_contract": core_data_ready,
            },
            "creator_explicitly_enabled": creator_explicitly_enabled,
            "canonical_project_asset_presence": canonical_presence,
            "availability_audit_missing_tokens": [
                token for token in audit_tokens if token not in availability_audit
            ],
            "frozen_status": METAHUMAN_BLOCKED_STATUS,
            "frozen_core_data_status": METAHUMAN_CORE_DATA_STATUS,
            "frozen_blockers": sorted(EXPECTED_AVAILABILITY_BLOCKERS),
        },
    )
    return {
        "status": METAHUMAN_BLOCKED_STATUS,
        "core_data_status": METAHUMAN_CORE_DATA_STATUS,
        "blocker": METAHUMAN_CORE_DATA_BLOCKER,
        "expected_blockers": sorted(EXPECTED_AVAILABILITY_BLOCKERS),
        "core_data_ready": core_data_ready,
        "creator_explicitly_enabled": creator_explicitly_enabled,
        "canonical_project_asset_presence": canonical_presence,
    }


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Statically validate dormant Session 8 MetaHuman groundwork without "
            "launching Unreal or UBT."
        )
    )
    parser.add_argument("--project-root", default=str(DEFAULT_ROOT))
    parser.add_argument("--engine-root")
    parser.add_argument("--report")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    root = Path(args.project_root).expanduser().absolute()
    report_path = (
        Path(args.report).expanduser().absolute()
        if args.report else root / "Saved/CharacterFramework/Session8WiringValidation.json"
    )
    v = Validator(root)
    project = v.json(UPROJECT)
    engine_root, engine_resolution = _resolve_engine_root(project, args.engine_root)

    _validate_required_files(v)
    _validate_schema_and_authority(v)
    _validate_plugin_isolation_and_delta(v)
    _validate_backend_policy(v)
    _validate_project_adapter(v)
    _validate_cook_contract(v)
    _validate_tests(v)
    blocked = _validate_blocked_availability(v, engine_root, engine_resolution)

    failed = [check for check in v.checks if not check.passed]
    result = {
        "schema": "DiscGolfTour.Session8WiringValidation.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": (
            "PASS_SESSION8_BLOCKED_SCAFFOLD_WIRING" if not failed else "FAIL"
        ),
        "validation_mode": "STATIC_FILESYSTEM_ONLY_NO_UNREAL_OR_UBT",
        "save_schema_version": 9,
        "authoritative_visual_backend": "dg_master",
        "authoritative_visual_content": "SESSION7_NON_PRODUCTION_PROXY",
        "metahuman_readiness": blocked,
        "canonical_paths": {
            "runtime_cook_manifest": COOK_MANIFEST_PACKAGE,
            "dgmaster_backend_profile": DGMASTER_PROFILE_PACKAGE,
            "reserved_metahuman_backend_profile": METAHUMAN_PROFILE_PACKAGE,
            "reserved_metahuman_actor": METAHUMAN_ACTOR_PACKAGE,
            "reserved_metahuman_retarget": METAHUMAN_RETARGET_PACKAGE,
        },
        "allowed_plugin_paths_since_session5": sorted(EXPECTED_PLUGIN_PATHS),
        "allowed_plugin_paths_since_session7": sorted(SESSION8_PLUGIN_PATHS),
        "checks_total": len(v.checks),
        "checks_passed": len(v.checks) - len(failed),
        "checks_failed": len(failed),
        "failed_check_ids": [check.check_id for check in failed],
        "checks": [asdict(check) for check in v.checks],
        "writes": [str(report_path)],
        "network_access": "NONE",
        "asset_acquisition": "NONE",
        "unreal_launched": False,
        "ubt_launched": False,
        "errors": [check.summary for check in failed],
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        f"SESSION 8 WIRING {result['status']}: "
        f"checks={len(v.checks)} failed={len(failed)} "
        f"metahuman={blocked['status']} report={report_path}"
    )
    for check in failed:
        print(f"  - {check.check_id}: {check.summary}", file=sys.stderr)
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
