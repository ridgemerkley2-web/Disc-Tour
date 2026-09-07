#!/usr/bin/env python3
"""Fail-closed Session 7 installed-plugin integration audit.

The installed UE 5.8-compatible plugin is the runtime source of truth.  This
validator compares the working tree (or a later committed successor state)
against the accepted pre-outfit Session 5 checkpoint, so it remains useful
after the Session 7 checkpoint. Exactly the three reviewed Session 6/7
adaptations plus the two reviewed Session 8 avatar-backend adaptations are
permitted. A second comparison to the accepted Session 7 checkpoint prevents
the Session 8 allowance from becoming a broad plugin exemption. The BuildKit
copy must remain untouched.
"""

from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
PLUGIN_ROOT = ROOT / "Plugins/DiscGolfCharacterFramework"
BUILDKIT_ROOT = (
    ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/"
    "DiscGolfCharacterFramework"
)
REPORT = ROOT / "Saved/CharacterFramework/Session7PluginContractValidation.json"
SESSION5_CHECKPOINT = "e6e6a57727411a4cc50b890f4d8557bfb803e9e2"
SESSION7_CHECKPOINT = "2c54be19a119264d42f11db5470399e021d050cd"

OUTFIT_CPP = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfOutfitComponent.cpp"
)
CUSTOMIZATION_CPP = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfCharacterCustomizationComponent.cpp"
)
CHARACTER_TYPES_H = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfCharacterTypes.h"
)
AVATAR_BACKEND_CPP = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfAvatarBackendComponent.cpp"
)
AVATAR_BACKEND_H = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfAvatarBackendComponent.h"
)
SESSION6_SESSION7_INTEGRATION_DIFF = {
    OUTFIT_CPP, CUSTOMIZATION_CPP, CHARACTER_TYPES_H,
}
SESSION8_INTEGRATION_DIFF = {AVATAR_BACKEND_CPP, AVATAR_BACKEND_H}
EXPECTED_INTEGRATION_DIFF = (
    SESSION6_SESSION7_INTEGRATION_DIFF | SESSION8_INTEGRATION_DIFF
)

FACE_MORPHS = {
    "head_width": "DG_Face_HeadWidth",
    "head_height": "DG_Face_HeadHeight",
    "brow_height": "DG_Face_BrowHeight",
    "brow_depth": "DG_Face_BrowDepth",
    "eye_size": "DG_Face_EyeSize",
    "eye_spacing": "DG_Face_EyeSpacing",
    "eye_depth": "DG_Face_EyeDepth",
    "nose_width": "DG_Face_NoseWidth",
    "nose_length": "DG_Face_NoseLength",
    "nose_bridge": "DG_Face_NoseBridge",
    "cheek_width": "DG_Face_CheekWidth",
    "cheek_fullness": "DG_Face_CheekFullness",
    "jaw_width": "DG_Face_JawWidth",
    "jaw_height": "DG_Face_JawHeight",
    "chin_width": "DG_Face_ChinWidth",
    "chin_length": "DG_Face_ChinLength",
    "mouth_width": "DG_Face_MouthWidth",
    "lip_fullness": "DG_Face_LipFullness",
    "ear_size": "DG_Face_EarSize",
    "ear_angle": "DG_Face_EarAngle",
}


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def _git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=30.0, check=False,
    )


def _record(
    checks: list[dict], check_id: str, errors: list[str], evidence: dict
) -> None:
    checks.append({
        "id": check_id,
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "evidence": evidence,
    })


def _struct_block(source: str, struct_name: str, next_marker: str) -> str:
    marker = f"struct DISCGOLFCHARACTERFRAMEWORK_API {struct_name}"
    if marker not in source:
        return ""
    return source.split(marker, 1)[1].split(next_marker, 1)[0]


# Session 19 moved Plugins/DiscGolfCharacterFramework outside the project root
# (characterFrameworkExternalQuarantine, mustBeOutsideProjectRoot: true), so on any
# checkout this validator's inputs are absent by policy. Say so once, clearly,
# instead of dying with FileNotFoundError partway through. This asserts nothing
# about the contract itself -- it reports that the evidence cannot be reached here.
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


def main() -> int:
    _require_character_framework(ROOT)
    checks: list[dict] = []

    diff = _git(
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION5_CHECKPOINT, "--", "Plugins/DiscGolfCharacterFramework",
    )
    diff_paths = {
        line.strip().replace("\\", "/")
        for line in diff.stdout.splitlines() if line.strip()
    }
    session8_diff = _git(
        "diff", "--name-only", "--diff-filter=ACDMRTUXB",
        SESSION7_CHECKPOINT, "--", "Plugins/DiscGolfCharacterFramework",
    )
    session8_diff_paths = {
        line.strip().replace("\\", "/")
        for line in session8_diff.stdout.splitlines() if line.strip()
    }
    plugin_status = _git(
        "status", "--porcelain=v1", "--untracked-files=all", "--",
        "Plugins/DiscGolfCharacterFramework",
    )
    untracked_plugin = sorted(
        line[3:].replace("\\", "/") for line in plugin_status.stdout.splitlines()
        if line.startswith("?? ")
    )
    errors: list[str] = []
    if diff.returncode != 0:
        errors.append(f"git diff failed: {diff.stderr.strip()}")
    if diff_paths != EXPECTED_INTEGRATION_DIFF:
        errors.append(
            "installed-plugin integration diff is not the exact five-file "
            f"allowlist: actual={sorted(diff_paths)}"
        )
    if session8_diff.returncode != 0:
        errors.append(f"Session 8 git diff failed: {session8_diff.stderr.strip()}")
    if session8_diff_paths != SESSION8_INTEGRATION_DIFF:
        errors.append(
            "Session 8 plugin delta is not exactly the two avatar-backend files: "
            f"actual={sorted(session8_diff_paths)}"
        )
    if untracked_plugin:
        errors.append(f"untracked installed-plugin paths: {untracked_plugin}")
    whitespace = _git(
        "diff", "--check", SESSION5_CHECKPOINT, "--",
        "Plugins/DiscGolfCharacterFramework",
    )
    if whitespace.returncode != 0:
        errors.append("plugin integration diff has whitespace errors")
    _record(checks, "exact_five_file_layered_integration_diff", errors, {
        "baseline": SESSION5_CHECKPOINT,
        "session8_baseline": SESSION7_CHECKPOINT,
        "expected": sorted(EXPECTED_INTEGRATION_DIFF),
        "actual": sorted(diff_paths),
        "session8_expected": sorted(SESSION8_INTEGRATION_DIFF),
        "session8_actual": sorted(session8_diff_paths),
        "plugin_status": plugin_status.stdout.splitlines(),
        "untracked": untracked_plugin,
        "diff_check_return_code": whitespace.returncode,
    })

    buildkit_status = _git(
        "status", "--porcelain=v1", "--untracked-files=all", "--",
        "_BuildKit/DiscGolfCorePlayabilityKit_v1.5",
    )
    buildkit_entries = [
        line for line in buildkit_status.stdout.splitlines() if line.strip()
    ]
    _record(
        checks,
        "buildkit_copy_remains_pristine",
        ([f"BuildKit changed: {line}" for line in buildkit_entries]
         if buildkit_status.returncode == 0 else
         [f"BuildKit git status failed: {buildkit_status.stderr.strip()}"]),
        {"entries": buildkit_entries, "return_code": buildkit_status.returncode},
    )

    outfit = _read(OUTFIT_CPP)
    outfit_tokens = (
        "VariantId = Variant.VariantId;",
        "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "SetGenerateOverlapEvents(false)",
        "SetCanEverAffectNavigation(false)",
        "SetSimulatePhysics(false)",
        "SetAbsolute(false, false, true)",
    )
    missing = [token for token in outfit_tokens if token not in outfit]
    outfit_errors = [f"outfit component missing: {token}" for token in missing]
    if outfit.count(
        "Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);") != 2:
        outfit_errors.append("outfit skeletal/static collision disable count is not 2")
    if outfit.count("Comp->SetGenerateOverlapEvents(false);") != 2:
        outfit_errors.append("outfit skeletal/static overlap disable count is not 2")
    if outfit.count("Comp->SetCanEverAffectNavigation(false);") != 2:
        outfit_errors.append("outfit skeletal/static navigation disable count is not 2")
    _record(checks, "session6_outfit_safety_and_canonical_variant", outfit_errors, {
        "path": OUTFIT_CPP,
        "missing_tokens": missing,
        "collision_disable_count": outfit.count(
            "Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);"),
    })

    customization = _read(CUSTOMIZATION_CPP)
    customization_errors: list[str] = []
    missing_morphs = [
        f"{key}->{target}" for key, target in FACE_MORPHS.items()
        if f'{{TEXT("{key}"), TEXT("{target}")}}' not in customization
    ]
    if missing_morphs:
        customization_errors.append(f"missing face morph mappings: {missing_morphs}")
    full_contract_tokens = (
        "static void DGSetMorphTargetIfAvailable(",
        "GetSkeletalMeshAsset()",
        "FindMorphTarget(MorphName)",
        "for (const TPair<FName, FName>& Pair : MorphMap)",
        "Current.Face.MorphValues.FindRef(Pair.Key)",
        "DGSetMorphTargetIfAvailable(",
        "HeadMesh, Pair.Value",
        "DG_HairColor",
        "SetLeaderPoseComponent(HeadMesh)",
        "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "SetGenerateOverlapEvents(false)",
        "SetCanEverAffectNavigation(false)",
        "SetSimulatePhysics(false)",
        "SetAbsolute(false, false, true)",
    )
    missing_tokens = [
        token for token in full_contract_tokens if token not in customization
    ]
    customization_errors.extend(
        f"customization component missing: {token}" for token in missing_tokens
    )
    if customization.count("MeshComp->SetMorphTarget(MorphName, Value);") != 1:
        customization_errors.append(
            "all framework morph writes must converge on the one guarded helper"
        )
    direct_morph_write_lines = [
        {"line": number, "text": line.strip()}
        for number, line in enumerate(customization.splitlines(), 1)
        if "->SetMorphTarget(" in line
        and "MeshComp->SetMorphTarget(MorphName, Value);" not in line
    ]
    if direct_morph_write_lines:
        customization_errors.append(
            "unguarded morph writes remain outside DGSetMorphTargetIfAvailable: "
            f"{direct_morph_write_lines}"
        )
    for token, expected in (
        ("Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);", 2),
        ("Comp->SetGenerateOverlapEvents(false);", 2),
        ("Comp->SetCanEverAffectNavigation(false);", 2),
        ("Comp->SetSimulatePhysics(false);", 2),
        ("Comp->SetAbsolute(false, false, true);", 1),
    ):
        actual = customization.count(token)
        if actual != expected:
            customization_errors.append(
                f"customization safety token count {token!r}: {actual} != {expected}"
            )
    forbidden_authority = re.compile(
        r"SpawnActor\s*<\s*ADiscActor|NewObject\s*<\s*UDiscFlightComponent|"
        r"ResolveThrowRelease\s*\(|AddStroke\s*\(|SetWind",
        re.IGNORECASE,
    )
    authority_hits = [
        {"line": number, "text": line.strip()}
        for number, line in enumerate(customization.splitlines(), 1)
        if forbidden_authority.search(line)
    ]
    if authority_hits:
        customization_errors.append(
            f"customization component contains gameplay-authority tokens: {authority_hits}"
        )
    _record(checks, "session7_customization_component_repair", customization_errors, {
        "path": CUSTOMIZATION_CPP,
        "face_morph_count": len(FACE_MORPHS) - len(missing_morphs),
        "missing_morphs": missing_morphs,
        "missing_tokens": missing_tokens,
        "guarded_morph_helper_count": customization.count(
            "MeshComp->SetMorphTarget(MorphName, Value);"),
        "unguarded_morph_writes": direct_morph_write_lines,
        "authority_hits": authority_hits,
        "hair_coverage_owner": (
            "project Pawn transaction; plugin Current ID remains untouched"),
        "hair_style_id_assignment_count": customization.count(
            "Current.Hair.HairStyleId ="),
    })

    types = _read(CHARACTER_TYPES_H)
    body_block = _struct_block(types, "FDGBodyProfile", "USTRUCT(BlueprintType)")
    throw_block = _struct_block(types, "FDGThrowStyle", "USTRUCT(BlueprintType)")
    body_save_count = body_block.count("SaveGame")
    throw_save_count = throw_block.count("SaveGame")
    type_errors: list[str] = []
    if body_save_count != 7:
        type_errors.append(f"FDGBodyProfile SaveGame count {body_save_count} != 7")
    if throw_save_count != 8:
        type_errors.append(f"FDGThrowStyle SaveGame count {throw_save_count} != 8")
    if types.count("SaveGame") != 15:
        type_errors.append(
            "SaveGame was added outside the exact 7 body + 8 throw-style fields"
        )
    _record(checks, "nested_full_character_fields_serialize", type_errors, {
        "path": CHARACTER_TYPES_H,
        "body_savegame_fields": body_save_count,
        "throw_style_savegame_fields": throw_save_count,
        "whole_file_savegame_tokens": types.count("SaveGame"),
        "power_and_spin_are_serialized_but_must_remain_runtime_normalized": True,
    })

    avatar_header = _read(AVATAR_BACKEND_H)
    avatar_cpp = _read(AVATAR_BACKEND_CPP)
    avatar_errors: list[str] = []
    header_tokens = (
        "bool ApplyCustomizationToVisual(",
        "bool IsVisualBackendReady() const;",
        "FDGAvatarBackendState GetAvatarBackendState() const;",
        "AActor* GetActiveVisualActor() const;",
        "UDiscGolfAvatarBackendProfile* GetActiveBackendProfile() const;",
        "USkeletalMeshComponent* GetActiveAnimationSourceMesh() const;",
        "UFUNCTION(BlueprintNativeEvent, Category=\"Disc Golf|Avatar\")",
        "bool ConfigureVisualBackend(",
        "virtual bool ConfigureVisualBackend_Implementation(",
        "bool ApplyVisualCustomization(",
        "virtual bool ApplyVisualCustomization_Implementation(",
        "TObjectPtr<AActor> PendingVisualActor;",
        "TObjectPtr<UDiscGolfAvatarBackendProfile> ActiveBackendProfile;",
        "TObjectPtr<USkeletalMeshComponent> ActiveAnimationSourceMesh;",
        "FDGAvatarBackendState ActiveBackendState;",
        "bool bBuildInProgress = false;",
        "bool bApplyInProgress = false;",
    )
    cpp_tokens = (
        "TGuardValue<bool> BuildGuard(bBuildInProgress, true);",
        "TGuardValue<bool> ApplyGuard(bApplyInProgress, true);",
        "SpawnActorDeferred<AActor>",
        "CandidateActor->SetActorHiddenInGame(true);",
        "CandidateActor->SetActorEnableCollision(false);",
        "CandidateActor->FinishSpawning(SpawnTransform);",
        "FAttachmentTransformRules::SnapToTargetNotIncludingScale",
        "AddSourceTickPrerequisites(CandidateActor, AnimationSourceMesh);",
        "const bool bConfigured = ConfigureVisualBackend(",
        "const bool bCandidateReady = bConfigured",
        "CandidateRoot->IsAttachedTo(AnimationSourceMesh)",
        "ActiveBackendState.bVisualReady = true;",
        "const FDGAvatarBackendState ReadyState = GetAvatarBackendState();",
        "OnAvatarBackendReady.Broadcast(ReadyState);",
        "return IsVisualBackendReady();",
        "EnforcePresentationOnly(ActiveActor, bWasHidden);",
        "Primitive->SetSimulatePhysics(false);",
        "Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);",
        "Primitive->SetGenerateOverlapEvents(false);",
        "Component->SetCanEverAffectNavigation(false);",
        "TargetMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);",
        "ActiveBackendState = FDGAvatarBackendState();",
    )
    missing_header_tokens = [
        token for token in header_tokens if token not in avatar_header
    ]
    missing_cpp_tokens = [token for token in cpp_tokens if token not in avatar_cpp]
    avatar_errors.extend(
        f"avatar backend header missing: {token}" for token in missing_header_tokens
    )
    avatar_errors.extend(
        f"avatar backend implementation missing: {token}" for token in missing_cpp_tokens
    )
    base_configure_fails_closed = re.search(
        r"ConfigureVisualBackend_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        avatar_cpp,
        re.DOTALL,
    ) is not None
    base_apply_fails_closed = re.search(
        r"ApplyVisualCustomization_Implementation\([^)]*\)\s*\{\s*return false;\s*\}",
        avatar_cpp,
        re.DOTALL,
    ) is not None
    if not base_configure_fails_closed:
        avatar_errors.append("native ConfigureVisualBackend base no longer fails closed")
    if not base_apply_fails_closed:
        avatar_errors.append("native ApplyVisualCustomization base no longer fails closed")
    if avatar_cpp.count("SpawnActorDeferred<AActor>") != 1:
        avatar_errors.append("avatar backend must have exactly one deferred spawn site")
    if "SpawnActor<AActor>" in avatar_cpp:
        avatar_errors.append("avatar backend regained an immediate actor spawn site")
    if avatar_cpp.count("OnAvatarBackendReady.Broadcast(") != 1:
        avatar_errors.append("verified-ready delegate broadcast count is not exactly one")
    configure_index = avatar_cpp.find("const bool bConfigured = ConfigureVisualBackend(")
    readiness_index = avatar_cpp.find("const bool bCandidateReady = bConfigured")
    commit_index = avatar_cpp.find("SpawnedVisualActor = CandidateActor;")
    broadcast_index = avatar_cpp.find("OnAvatarBackendReady.Broadcast(ReadyState);")
    if not (0 <= configure_index < readiness_index < commit_index < broadcast_index):
        avatar_errors.append(
            "candidate configuration/readiness/commit/broadcast order is not fail closed"
        )
    forbidden_avatar_authority = re.compile(
        r"SpawnActor\s*<\s*ADiscActor|UDiscFlightComponent|UDiscBagComponent|"
        r"UThrowControllerComponent|ResolveThrowRelease\s*\(|RequestThrow\s*\(|"
        r"RequestThrowFromGrip\s*\(|AddStroke\s*\(|SetWind",
        re.IGNORECASE,
    )
    avatar_authority_hits = [
        {"path": path, "line": number, "text": line.strip()}
        for path, source in (
            (AVATAR_BACKEND_H, avatar_header),
            (AVATAR_BACKEND_CPP, avatar_cpp),
        )
        for number, line in enumerate(source.splitlines(), 1)
        if forbidden_avatar_authority.search(line)
    ]
    if avatar_authority_hits:
        avatar_errors.append(
            "avatar backend delta contains gameplay-authority tokens: "
            f"{avatar_authority_hits}"
        )
    _record(checks, "session8_avatar_backend_verified_ready_transaction", avatar_errors, {
        "paths": sorted(SESSION8_INTEGRATION_DIFF),
        "missing_header_tokens": missing_header_tokens,
        "missing_cpp_tokens": missing_cpp_tokens,
        "deferred_spawn_count": avatar_cpp.count("SpawnActorDeferred<AActor>"),
        "immediate_spawn_present": "SpawnActor<AActor>" in avatar_cpp,
        "ready_broadcast_count": avatar_cpp.count(
            "OnAvatarBackendReady.Broadcast("),
        "native_configure_fails_closed": base_configure_fails_closed,
        "native_apply_fails_closed": base_apply_fails_closed,
        "configuration_index": configure_index,
        "readiness_index": readiness_index,
        "commit_index": commit_index,
        "broadcast_index": broadcast_index,
        "authority_hits": avatar_authority_hits,
    })

    all_errors = [
        f"{check['id']}: {error}"
        for check in checks for error in check["errors"]
    ]
    report = {
        "schema": "DiscGolfTour.Session7PluginContractValidation.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not all_errors else "FAIL",
        "source_of_truth": str(PLUGIN_ROOT),
        "buildkit_copy": str(BUILDKIT_ROOT),
        "integration_baseline": SESSION5_CHECKPOINT,
        "session8_integration_baseline": SESSION7_CHECKPOINT,
        "allowed_plugin_paths": sorted(EXPECTED_INTEGRATION_DIFF),
        "session8_allowed_plugin_paths": sorted(SESSION8_INTEGRATION_DIFF),
        "checks": checks,
        "errors": all_errors,
        "ue_launched": False,
        "ubt_launched": False,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"SESSION 7 PLUGIN CONTRACT {report['status']}: "
        f"checks={len(checks)} errors={len(all_errors)} report={REPORT}"
    )
    for error in all_errors:
        print(f"  - {error}", file=sys.stderr)
    return 0 if not all_errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
