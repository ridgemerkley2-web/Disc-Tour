#!/usr/bin/env python3
"""Fail-closed, source-only Session 8B MetaHuman wiring validation.

This validator never launches Unreal, UBT, a browser, or a network request. It
accepts only the current source/static-asset checkpoint: DG master remains the
single gameplay/animation authority; the Optimized-Medium MetaHuman assembly,
wrapper, retargeter, and truthful GameplayPerformance profile are present; and
    post-retarget cook/package closure, runtime visuals, and packaged Omen
    performance remain explicitly blocked.
"""

from __future__ import annotations

import argparse
import ast
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Sequence


ROOT = Path(__file__).absolute().parents[1]
DEFAULT_REPORT = ROOT / "Saved/CharacterFramework/Session8BWiringValidation.json"
DEFAULT_AVAILABILITY_REPORT = (
    ROOT / "Saved/CharacterFramework/Session8BAvailabilityAudit.json"
)

SCHEMA = "DiscGolfTour.Session8BWiringValidation.v3"
DGMASTER_ID = "dg_master"
METAHUMAN_ID = "metahuman_assembled"
QUALITY_IDS = {"Prototype", "GameplayHigh", "GameplayPerformance", "Showcase"}

SOURCE_PACKAGE = "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default"
SOURCE_OBJECT = SOURCE_PACKAGE + ".MHC_DG_Golfer_Default"
SOURCE_FILE = (
    "Content/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default.uasset"
)
SOURCE_BYTES = 112982127
SOURCE_SHA256 = (
    "1AA1EFBFDD882308959D1429D562DD2D92F64317219660003063D0AEF17BCFAB"
)
GENERATED_BP_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "BP_MHC_DG_Golfer_Default"
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

ASSEMBLY_FILE_COUNT = 261
ASSEMBLY_BYTES = 376200391
ASSEMBLY_MANIFEST_SHA256 = (
    "42BD0396180C5BF3021FBB9B618BEE61F07CDF2CF084BA026B408D081076AA23"
)
EXPECTED_QUALITY_PROFILE = "GameplayPerformance"
CANONICAL_RUNTIME_ASSET_IDENTITIES = {
    METAHUMAN_ACTOR_PACKAGE: {
        "bytes": 225470,
        "sha256": "72704ECC288B7D0917C6062198B3B5161B966244A5A249632021A847BE253634",
    },
    METAHUMAN_RETARGET_PACKAGE: {
        "bytes": 22640,
        "sha256": "6316D573F271EE40BD7D3C42258723F9C8669A5606A0FB0373016DF3EBFBEAE9",
    },
    METAHUMAN_PROFILE_PACKAGE: {
        "bytes": 2274,
        "sha256": "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D",
    },
}

CURRENT_CONTENT = {
    "file_count": 953,
    "bytes": 2920024342,
    "manifest_sha256": "56DF1ED51ED32858141B6312BCD43E81E260C215137946E1F5395CED61762C0B",
}
PRE_RETARGET_CORRECTION_CONTENT = {
    "file_count": 953,
    "bytes": 2920025165,
    "manifest_sha256": "4306208A7347B8924DF8D1F3590B82FA605AC582529F4B2BC2F8ADED6E6194F2",
}
CURRENT_ASSEMBLY_PARTITION = {
    "file_count": 950,
    "bytes": 2919773958,
    "manifest_sha256": "CFA6EE7443BA141C770E02052460BE69989941C3F924F4FF54FA26B27D802E22",
}
CURRENT_ASSEMBLY_WITHOUT_COOK_MANIFEST = {
    "file_count": 949,
    "bytes": 2919652760,
    "manifest_sha256": "658627B46C343E83308D1407FC33F9F6518D9F5149E832AD754FB9183F4DDBFE",
}
CURRENT_COOK_MANIFEST_IDENTITY = {
    "bytes": 121198,
    "sha256": "438CA66DEC96AB7F7A19AC551C5FCFE9AEE6DDCDCC8E8432B475F0FD85AF30BE",
}
STALE_RETARGET_IDENTITY = {
    "bytes": 23463,
    "sha256": "51208403C91B72F490746AD2EDAA182F74B345CAD4B54D426A1C2C1441D74792",
}
EXPECTED_RETARGET_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_RETARGET_OP_ENABLED_MASK = [True, True, False, True, True]
EXPECTED_RETARGET_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"
EXPECTED_RETARGET_OUTPUT_MAX_ABS_TRANSLATION_CM = 164.71923439615676
EXPECTED_RETARGET_ANCHORS = {"root", "pelvis", "spine_01", "head"}
RETARGET_CORRECTION_RUN_ID = "5fcde026-faa0-4c6b-aeba-bfbb27f978e6"
RETARGET_CORRECTION_REPORT_IDENTITY = {
    "bytes": 18573,
    "sha256": "3B09DC2CDB0A0831F534CF3B21DAADF29237B2B84D8CA62149A913DF58074ACB",
}
RETARGET_CORRECTION_LOG_IDENTITY = {
    "bytes": 261088,
    "sha256": "7076641FB000BA32A2E386077B11CD7EE753637BD1BCBC9132A1857CBA4CD900",
}
CURRENT_VALIDATION_RUN_ID = "dc8962f0-bc36-409b-853e-54dfee62f75c"
CURRENT_VALIDATION_REPORT_IDENTITY = {
    "bytes": 11215,
    "sha256": "CD3BD3D09C748D6E907287129939D91033299E89FEE4D186B458839F7AC82FA8",
}
CURRENT_VALIDATION_LOG_IDENTITY = {
    "bytes": 259666,
    "sha256": "3D7322AF9E7963566B170F3E69B22BEC1F21224A98DE59D88C5673B77286050D",
}

AVAILABILITY_SCHEMA = "DiscGolfTour.Session8BAvailabilityAudit.v3"
AVAILABILITY_BLOCKERS = {
    "SESSION8B_POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE_NOT_ACCEPTED",
    "SESSION8B_RUNTIME_VISUAL_EVIDENCE_NOT_ACCEPTED",
    "SESSION8B_PACKAGED_OMEN_PERFORMANCE_NOT_MEASURED",
}
CORE_STATUS = "PRESENT_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT"

AUTHOR = "Scripts/author_dg_character_session8b_metahuman.py"
RUNTIME_AUTHOR = "Scripts/author_dg_character_session8b_runtime_assets.py"
RUNTIME_VALIDATOR = "Scripts/validate_dg_character_session8b_runtime_assets.py"
PROFILE_QUALITY_CORRECTOR = (
    "Scripts/reauthor_dg_character_session8b_profile_quality.py"
)
RETARGET_RUN_IK_CORRECTOR = (
    "Scripts/reauthor_dg_character_session8b_retarget_run_ik.py"
)
AVAILABILITY = "Scripts/audit_dg_character_session8b_availability.py"
S8B_COOK_VALIDATORS = (
    "Scripts/validate_dg_character_session8b_cook_assets.py",
    "Scripts/validate_dg_character_session8b_cook_no_write.py",
)
S8B_COOK_VALIDATOR_IDENTITIES = {
    S8B_COOK_VALIDATORS[0]: {
        "bytes": 32485,
        "sha256": "E0D2A23C1F05DF5D503B96585311BAD8EA6B1CAA78A693CE7DBA8A171D62D930",
    },
    S8B_COOK_VALIDATORS[1]: {
        "bytes": 16391,
        "sha256": "E17BB91DFFC7E30C3F287005C655149E8E6C6E1C97B6F51A77E03C20E20B9CF6",
    },
}
COOK_SPEC = "Config/DG_RuntimeCookManifest.json"
S8A_ACCEPTED_VALIDATORS = (
    "Scripts/audit_dg_character_session8_availability.py",
    "Scripts/validate_dg_character_session8_wiring.py",
    "Scripts/validate_dg_character_session8_brand_license.py",
    "Scripts/validate_dg_character_session8_cook_assets.py",
    "Scripts/validate_dg_character_session8_cook_no_write.py",
)

SAVE_HEADER = "Source/DiscGolfTour/DiscGolfSaveGame.h"
CUSTOMIZATION_TYPES = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfCustomizationTypes.h"
)
FULL_RUNTIME = "Source/DiscGolfTour/DiscGolfFullCharacterRuntime.cpp"
GAME_INSTANCE = "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp"
BACKEND_RUNTIME_H = "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.h"
BACKEND_RUNTIME_CPP = "Source/DiscGolfTour/DiscGolfAvatarBackendRuntime.cpp"
WIDGET = "Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp"
CONTROLLER = "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp"
PAWN_H = "Source/DiscGolfTour/DiscGolferPawn.h"
PAWN_CPP = "Source/DiscGolfTour/DiscGolferPawn.cpp"
VISUAL_CONTRACT = "Source/DiscGolfTour/DiscGolfMetaHumanVisualContract.h"
ADAPTER_H = "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.h"
ADAPTER_CPP = "Source/DiscGolfTour/DiscGolfMetaHumanAvatarBackendComponent.cpp"
RETARGET_H = "Source/DiscGolfTour/DiscGolfMetaHumanRetargetAnimInstance.h"
RETARGET_CPP = "Source/DiscGolfTour/DiscGolfMetaHumanRetargetAnimInstance.cpp"
BUILD_CS = "Source/DiscGolfTour/DiscGolfTour.Build.cs"
UPROJECT = "DiscGolfTour.uproject"
DEFAULT_ENGINE = "Config/DefaultEngine.ini"
PLUGIN_BACKEND_CPP = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Private/DiscGolfAvatarBackendComponent.cpp"
)
PLUGIN_BACKEND_H = (
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/"
    "Public/DiscGolfAvatarBackendComponent.h"
)
FULL_TESTS = "Source/DiscGolfTour/Tests/DiscGolfFullCharacterRuntimeTests.cpp"
BACKEND_TESTS = "Source/DiscGolfTour/Tests/DiscGolfAvatarBackendRuntimeTests.cpp"
SAVE_TESTS = "Source/DiscGolfTour/Tests/DiscGolfSaveSchemaTests.cpp"
EDITOR_HELPER_H = "Source/DiscGolfTourEditor/DiscGolfSession8BMetaHumanUtility.h"
EDITOR_HELPER_CPP = "Source/DiscGolfTourEditor/DiscGolfSession8BMetaHumanUtility.cpp"

BUILDKIT_BACKENDS = (
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_AvatarBackendSchema.json"
)
BUILDKIT_QUALITY = (
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Config/DG_QualityProfiles.json"
)

EXPECTED_PLUGIN_DIFF = {CUSTOMIZATION_TYPES, PLUGIN_BACKEND_CPP, PLUGIN_BACKEND_H}
EXPECTED_RENDERER_ADDITIONS = Counter(
    {
        "r.GPUSkin.Support16BitBoneIndex=True": 1,
        "r.GPUSkin.UnlimitedBoneInfluences=True": 1,
        "r.SkinCache.CompileShaders=True": 1,
    }
)


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
        self._texts: dict[str, str] = {}

    def text(self, relative: str) -> str:
        if relative not in self._texts:
            path = self.root / relative
            self._texts[relative] = (
                path.read_text(encoding="utf-8", errors="replace")
                if path.is_file()
                else ""
            )
        return self._texts[relative]

    def json(self, relative: str) -> dict[str, Any]:
        try:
            value = json.loads(self.text(relative))
            return value if isinstance(value, dict) else {}
        except (json.JSONDecodeError, OSError):
            return {}

    def add(self, check_id: str, passed: bool, summary: str, evidence: Any) -> None:
        self.checks.append(Check(check_id, bool(passed), summary, evidence))


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _record(path: Path) -> dict[str, Any]:
    return {
        "path": str(path),
        "exists": path.is_file(),
        "bytes": path.stat().st_size if path.is_file() else 0,
        "sha256": _sha256(path) if path.is_file() else None,
    }


def _identity_matches(record: Any, expected: dict[str, Any]) -> bool:
    return (
        isinstance(record, dict)
        and record.get("exists") is True
        and record.get("bytes") == expected["bytes"]
        and record.get("sha256") == expected["sha256"]
    )


def _retarget_policy_evidence_matches(evidence: Any) -> bool:
    if not isinstance(evidence, dict):
        return False
    anchors = evidence.get("retarget_output_anchor_transforms")
    return (
        evidence.get("retarget_op_types") == EXPECTED_RETARGET_OP_TYPES
        and evidence.get("retarget_op_enabled_mask")
        == EXPECTED_RETARGET_OP_ENABLED_MASK
        and evidence.get("retarget_runtime_policy") == EXPECTED_RETARGET_POLICY
        and evidence.get("retarget_output_pose_plausible") is True
        and evidence.get("retarget_output_max_abs_translation_cm")
        == EXPECTED_RETARGET_OUTPUT_MAX_ABS_TRANSLATION_CM
        and evidence.get("retarget_output_translation_outlier_count") == 0
        and isinstance(anchors, dict)
        and set(anchors) == EXPECTED_RETARGET_ANCHORS
    )


def _git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def _git_paths(root: Path, *args: str) -> set[str]:
    result = _git(root, *args)
    if result.returncode != 0:
        return set()
    return {line.strip().replace("\\", "/") for line in result.stdout.splitlines() if line.strip()}


def _cpp_function(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    brace = text.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    return ""


def _package_file(root: Path, package: str) -> Path:
    return root / "Content" / (package.removeprefix("/Game/") + ".uasset")


def _asset_partition_summary(roots: Sequence[Path], relative_to: Path) -> dict[str, Any]:
    files = sorted(
        (
            path
            for root in roots
            if root.is_dir()
            for path in root.rglob("*")
            if path.is_file()
        ),
        key=lambda path: path.relative_to(relative_to).as_posix(),
    )
    digest = hashlib.sha256()
    total_bytes = 0
    for path in files:
        relative = path.relative_to(relative_to).as_posix()
        size = path.stat().st_size
        total_bytes += size
        digest.update(f"{relative}\t{size}\t{_sha256(path)}\n".encode("utf-8"))
    return {
        "file_count": len(files),
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest().upper(),
    }


def _resolve_engine_root(root: Path, explicit: str | None) -> tuple[Path, str]:
    if explicit:
        return Path(explicit).expanduser().absolute(), "EXPLICIT"
    project = json.loads((root / UPROJECT).read_text(encoding="utf-8"))
    association = str(project.get("EngineAssociation", ""))
    if association == "5.8":
        return Path(r"C:\Program Files\Epic Games\UE_5.8"), "PROJECT_ASSOCIATION"
    return Path(), "UNRESOLVED"


def _added_removed_lines(diff: str) -> tuple[Counter[str], Counter[str]]:
    added: Counter[str] = Counter()
    removed: Counter[str] = Counter()
    for line in diff.splitlines():
        if line.startswith("+++") or line.startswith("---"):
            continue
        if line.startswith("+"):
            added[line[1:].strip()] += 1
        elif line.startswith("-"):
            removed[line[1:].strip()] += 1
    return added, removed


def _validate_required_files(v: Validator, availability_report: Path) -> None:
    required = (
        AUTHOR,
        RUNTIME_AUTHOR,
        RUNTIME_VALIDATOR,
        PROFILE_QUALITY_CORRECTOR,
        RETARGET_RUN_IK_CORRECTOR,
        AVAILABILITY,
        *S8B_COOK_VALIDATORS,
        COOK_SPEC,
        *S8A_ACCEPTED_VALIDATORS,
        SAVE_HEADER,
        CUSTOMIZATION_TYPES,
        FULL_RUNTIME,
        GAME_INSTANCE,
        BACKEND_RUNTIME_H,
        BACKEND_RUNTIME_CPP,
        WIDGET,
        CONTROLLER,
        PAWN_H,
        PAWN_CPP,
        VISUAL_CONTRACT,
        ADAPTER_H,
        ADAPTER_CPP,
        RETARGET_H,
        RETARGET_CPP,
        BUILD_CS,
        UPROJECT,
        DEFAULT_ENGINE,
        PLUGIN_BACKEND_CPP,
        PLUGIN_BACKEND_H,
        FULL_TESTS,
        BACKEND_TESTS,
        SAVE_TESTS,
        EDITOR_HELPER_H,
        EDITOR_HELPER_CPP,
        BUILDKIT_BACKENDS,
        BUILDKIT_QUALITY,
        SOURCE_FILE,
    )
    missing = [relative for relative in required if not (v.root / relative).is_file()]
    v.add(
        "required_files",
        not missing and availability_report.is_file(),
        "All frozen S8B source, contract, evidence, and accepted predecessor files exist",
        {"missing": missing, "availability_report": _record(availability_report)},
    )


def _validate_predecessor_and_plugin_scope(v: Validator) -> None:
    predecessor_dirty = [
        path
        for path in S8A_ACCEPTED_VALIDATORS
        if _git(v.root, "diff", "--quiet", "HEAD", "--", path).returncode != 0
    ]
    v.add(
        "accepted_s8a_validators_unchanged",
        not predecessor_dirty,
        "Session 8B does not weaken or overwrite any accepted Session 8A validator",
        {"expected_unchanged": list(S8A_ACCEPTED_VALIDATORS), "dirty": predecessor_dirty},
    )

    plugin_paths = _git_paths(v.root, "diff", "--name-only", "HEAD", "--", "Plugins")
    plugin_cpp = v.text(PLUGIN_BACKEND_CPP)
    plugin_h = v.text(PLUGIN_BACKEND_H)
    types_h = v.text(CUSTOMIZATION_TYPES)
    presentation_tokens = (
        "Session8BIsPresentationOnlyVisualActor",
        "!VisualActor->IsA<APawn>()",
        "!VisualActor->IsA<AController>()",
        "!VisualActor->FindComponentByClass<UMovementComponent>()",
        "!VisualActor->FindComponentByClass<UInputComponent>()",
        "if (!FinalizeVisualBackendActivation(",
        "DestroyVisualActor(CandidateActor);",
    )
    activation_header_tokens = (
        "virtual bool FinalizeVisualBackendActivation(",
        "AActor* VisualActor",
        "USkeletalMeshComponent* AnimationSourceMesh",
        "const FDGFullCharacterCustomization& Customization",
    )
    field_ok = bool(
        re.search(
            r"UPROPERTY\([^\)]*SaveGame[^\)]*\)\s*"
            r"FName\s+AvatarBackendId\s*=\s*TEXT\(\"dg_master\"\);",
            types_h,
        )
    )
    v.add(
        "exact_bounded_plugin_delta",
        plugin_paths == EXPECTED_PLUGIN_DIFF
        and all(token in plugin_cpp for token in presentation_tokens)
        and all(token in plugin_h for token in activation_header_tokens)
        and field_ok,
        "Plugin drift is exactly the S8B persisted backend field, presentation-only candidate hardening, and transactional render-activation seam",
        {
            "actual_paths": sorted(plugin_paths),
            "expected_paths": sorted(EXPECTED_PLUGIN_DIFF),
            "missing_presentation_tokens": [t for t in presentation_tokens if t not in plugin_cpp],
            "missing_activation_header_tokens": [
                token for token in activation_header_tokens if token not in plugin_h
            ],
            "savegame_backend_field": field_ok,
            "activation_header_changed_in_exact_scope": PLUGIN_BACKEND_H in plugin_paths,
        },
    )


def _validate_engine_and_project(v: Validator, engine_root: Path, engine_basis: str) -> None:
    project = v.json(UPROJECT)
    plugins = [entry for entry in project.get("Plugins", []) if isinstance(entry, dict)]
    creator_entries = [entry for entry in plugins if entry.get("Name") == "MetaHumanCharacter"]
    ik_entries = [entry for entry in plugins if entry.get("Name") == "IKRig"]
    build = v.text(BUILD_CS)
    v.add(
        "creator_and_runtime_modules",
        len(creator_entries) == 1
        and creator_entries[0].get("Enabled") is True
        and len(ik_entries) == 1
        and ik_entries[0].get("Enabled") is True
        and build.count('"IKRig"') == 1
        and build.count('"MetaHumanSDKRuntime"') == 1,
        "MetaHuman Creator is explicitly enabled and runtime code links only the typed IKRig/MetaHuman component modules it uses",
        {
            "creator_entries": creator_entries,
            "ikrig_entries": ik_entries,
            "ikrig_module_count": build.count('"IKRig"'),
            "metahuman_sdk_runtime_module_count": build.count('"MetaHumanSDKRuntime"'),
        },
    )

    optional_root = (
        engine_root
        / "Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Optional"
    )
    files = sorted(path for path in optional_root.rglob("*") if path.is_file()) if optional_root.is_dir() else []
    uassets = [path for path in files if path.suffix.casefold() == ".uasset"]
    umaps = [path for path in files if path.suffix.casefold() == ".umap"]
    v.add(
        "metahuman_creator_core_data_present",
        len(files) == 1816 and len(uassets) == 1783 and not umaps,
        "Installed UE 5.8.1 MetaHuman Creator Core Data is present at the optional-content contract root",
        {
            "engine_root": str(engine_root),
            "resolution": engine_basis,
            "optional_root": str(optional_root),
            "files": len(files),
            "uassets": len(uassets),
            "umaps": len(umaps),
        },
    )

    validation_source = (
        engine_root
        / "Engine/Plugins/MetaHuman/MetaHumanSDK/Source/MetaHumanSDKEditor/"
        "Private/Tests/MetaHumanValidationTests.cpp"
    )
    validation_text = validation_source.read_text(encoding="utf-8", errors="replace") if validation_source.is_file() else ""
    renderer_diff = _git(v.root, "diff", "--unified=0", "HEAD", "--", DEFAULT_ENGINE)
    added, removed = _added_removed_lines(renderer_diff.stdout if renderer_diff.returncode == 0 else "")
    source_pairs = (
        ('TEXT("r.GPUSkin.Support16BitBoneIndex")', '.Value = TEXT("True")'),
        ('TEXT("r.GPUSkin.UnlimitedBoneInfluences")', '.Value = TEXT("True")'),
        ('TEXT("r.SkinCache.CompileShaders")', '.Value = TEXT("True")'),
    )
    source_proves = all(key in validation_text and value in validation_text for key, value in source_pairs)
    v.add(
        "exact_metahuman_renderer_guideline_delta",
        added == EXPECTED_RENDERER_ADDITIONS
        and not removed
        and source_proves
        and validation_text.count('TEXT("/Config/DefaultEngine.ini")') >= 3,
        "The only renderer drift is three exact keys explicitly named by Epic's installed MetaHuman face Asset Guideline",
        {
            "added": dict(added),
            "removed": dict(removed),
            "expected_added": dict(EXPECTED_RENDERER_ADDITIONS),
            "provenance": _record(validation_source),
            "source_proves_all_three": source_proves,
            "classification": "METAHUMAN_FACE_ASSET_GUIDELINE_PROJECT_SETTINGS",
        },
    )


def _validate_schema_and_vocabulary(v: Validator) -> None:
    save_h = v.text(SAVE_HEADER)
    types_h = v.text(CUSTOMIZATION_TYPES)
    runtime = v.text(FULL_RUNTIME)
    gi = v.text(GAME_INSTANCE)
    backend_h = v.text(BACKEND_RUNTIME_H)
    tests = v.text(SAVE_TESTS) + v.text(FULL_TESTS)
    tokens = (
        "inline constexpr int32 CurrentVersion = 10;",
        'FName AvatarBackendId = TEXT("dg_master");',
        "CanonicalMetaHumanBackendId",
        "A.AvatarBackendId == B.AvatarBackendId",
        "Resolved.AvatarBackendId = NormalizedCurrent.AvatarBackendId;",
        "if (InOutProfile.SaveSchemaVersion < 10)",
        'InOutProfile.CharacterCustomization.AvatarBackendId = TEXT("dg_master");',
        'DiscGolfSaveSchema::CurrentVersion, 10',
    )
    combined = "\n".join((save_h, types_h, runtime, gi, tests))
    field_savegame = bool(
        re.search(r"UPROPERTY\([^\)]*SaveGame[^\)]*\)\s*FName\s+AvatarBackendId", types_h)
    )
    v.add(
        "schema10_backend_persistence",
        field_savegame and all(token in combined for token in tokens),
        "Schema 10 persists one normalized backend ID, migrates pre-10 saves to DGMaster, compares it, and preserves it through Randomize",
        {"missing_tokens": [token for token in tokens if token not in combined], "field_has_savegame": field_savegame},
    )

    backend_spec = v.json(BUILDKIT_BACKENDS)
    quality_spec = v.json(BUILDKIT_QUALITY)
    backend_ids = [entry.get("id") for entry in backend_spec.get("backends", []) if isinstance(entry, dict)]
    quality_ids = {entry.get("id") for entry in quality_spec.get("profiles", []) if isinstance(entry, dict)}
    header_tokens = (
        'TEXT("dg_master")',
        'TEXT("metahuman_assembled")',
        "DGMasterProfileObjectPath",
        "MetaHumanDefaultProfileObjectPath",
        "GetQualityProfileIds",
    )
    v.add(
        "canonical_backend_and_quality_vocabulary",
        backend_ids == [DGMASTER_ID, METAHUMAN_ID, "metahuman_collection_instance_experimental"]
        and backend_spec.get("shipping_default") == METAHUMAN_ID
        and quality_ids == QUALITY_IDS
        and quality_spec.get("shipping_default") == "GameplayHigh"
        and all(token in backend_h for token in header_tokens),
        "Runtime IDs and quality vocabulary exactly match the BuildKit contract; experimental collections remain outside the required path",
        {
            "backend_ids": backend_ids,
            "shipping_backend": backend_spec.get("shipping_default"),
            "quality_ids": sorted(quality_ids),
            "shipping_quality": quality_spec.get("shipping_default"),
            "missing_header_tokens": [token for token in header_tokens if token not in backend_h],
        },
    )

    profiles = {
        entry.get("id"): entry
        for entry in quality_spec.get("profiles", [])
        if isinstance(entry, dict) and isinstance(entry.get("id"), str)
    }
    performance_meta = profiles.get(EXPECTED_QUALITY_PROFILE, {}).get("metahuman", {})
    high_meta = profiles.get("GameplayHigh", {}).get("metahuman", {})
    helper = v.text(EDITOR_HELPER_CPP)
    runtime_tool_text = v.text(RUNTIME_AUTHOR) + "\n" + v.text(RUNTIME_VALIDATOR)
    v.add(
        "optimized_medium_quality_metadata_truth",
        performance_meta == {
            "assembly": "UE Optimized",
            "optimization_level": "Medium",
            "strand_hair_player": False,
        }
        and high_meta == {
            "assembly": "UE Optimized",
            "optimization_level": "High",
            "strand_hair_player": True,
        }
        and 'ExpectedQualityProfileId(TEXT("GameplayPerformance"))' in helper
        and 'TEXT("preferred_quality_profile_id")' in helper
        and 'TEXT("assembly_pipeline"), TEXT("UE_OPTIMIZED")' in helper
        and 'TEXT("assembly_optimization_level"), TEXT("MEDIUM")' in helper
        and runtime_tool_text.count(
            '"preferred_quality_profile_id": "GameplayPerformance"'
        ) == 2
        and runtime_tool_text.count('"assembly_pipeline": "UE_OPTIMIZED"') == 2
        and runtime_tool_text.count('"assembly_optimization_level": "MEDIUM"') == 2,
        "The persisted Optimized-Medium preset must identify as GameplayPerformance; GameplayHigh remains the distinct Optimized-High design target",
        {
            "gameplay_performance": performance_meta,
            "gameplay_high": high_meta,
            "helper_uses_gameplay_performance": (
                'ExpectedQualityProfileId(TEXT("GameplayPerformance"))' in helper
            ),
            "runtime_tool_quality_field_count": runtime_tool_text.count(
                '"preferred_quality_profile_id": "GameplayPerformance"'
            ),
        },
    )


def _validate_creator_semantics(v: Validator) -> None:
    widget = v.text(WIDGET)
    controller = v.text(CONTROLLER)
    tab_match = re.search(r"const TArray<FString> TabLabels\s*=\s*\{(.*?)\};", widget, re.S)
    tab_labels = re.findall(r'TEXT\("([^"]+)"\)', tab_match.group(1)) if tab_match else []
    selector_tokens = (
        "TSharedRef<SHorizontalBox> BackendSelector",
        "BackendSelector",
        "DGMASTER / PROXY",
        "METAHUMAN / CURATED",
        "METAHUMAN / UNAVAILABLE",
        "IsCharacterCreatorMetaHumanBackendAvailable",
        "HandleBackendSelection",
    )
    globals_ = (
        "RESET CURRENT TAB",
        "RESET ALL",
        "RANDOMIZE",
        "CANCEL",
        "APPLY / SAVE & CONTINUE",
        "ROTATE LEFT",
        "ROTATE RIGHT",
        "ZOOM IN",
        "ZOOM OUT",
        "LOCK TAB",
    )
    v.add(
        "seven_tabs_plus_global_backend_selector",
        tab_labels == ["IDENTITY", "BODY", "FACE", "HAIR", "APPEARANCE", "THROW STYLE", "OUTFIT"]
        and all(token in widget for token in selector_tokens)
        and all(token in widget for token in globals_)
        and widget.count("BuildBackendAwareProxyTab(Build") == 5
        and "BuildBackendAwareProxyTab(BuildIdentityTab())" not in widget
        and "BuildBackendAwareProxyTab(BuildThrowStyleTab())" not in widget
        and "proxy-only controls are preserved for DGMaster fallback" in widget,
        "Seven existing category tabs remain intact; backend choice is a separate global selector and only five proxy-specific tabs are disabled",
        {
            "tab_labels": tab_labels,
            "missing_selector_tokens": [t for t in selector_tokens if t not in widget],
            "missing_global_tokens": [t for t in globals_ if t not in widget],
            "proxy_wrapped_tab_count": widget.count("BuildBackendAwareProxyTab(Build"),
        },
    )

    select_fn = _cpp_function(controller, "bool ADiscGolfTourPlayerController::SelectCharacterCreatorBackend(")
    reset_tab = _cpp_function(controller, "bool ADiscGolfTourPlayerController::ResetCharacterCreatorCurrentTab(")
    reset_all = _cpp_function(controller, "bool ADiscGolfTourPlayerController::ResetCharacterCreatorAll(")
    randomize = _cpp_function(controller, "bool ADiscGolfTourPlayerController::RandomizeCharacterCreatorDraft(")
    cancel = _cpp_function(controller, "void ADiscGolfTourPlayerController::CancelCharacterCreator()")
    close = _cpp_function(controller, "void ADiscGolfTourPlayerController::CloseCharacterCreator(")
    ordered_select = (
        select_fn.find("BackendId != DGMasterId")
        < select_fn.find("!IsCharacterCreatorMetaHumanBackendAvailable()")
        < select_fn.find("Candidate.AvatarBackendId = BackendId")
        < select_fn.find("PreviewFullCharacterCreatorDraft(Candidate)")
    ) and all(select_fn.find(token) >= 0 for token in (
        "BackendId != DGMasterId", "!IsCharacterCreatorMetaHumanBackendAvailable()",
        "Candidate.AvatarBackendId = BackendId", "PreviewFullCharacterCreatorDraft(Candidate)"))
    v.add(
        "creator_backend_transaction_semantics",
        ordered_select
        and "AvatarBackendId" not in reset_tab
        and "MakeDefaultCustomization()" in reset_all
        and "DiscGolfFullCharacterRuntime::Randomize" in randomize
        and "CloseCharacterCreator(true)" in cancel
        and "CharacterCreatorOpeningCustomization" in close
        and "ApplyFullCharacterCustomizationTransactionally" in close
        and "the creator remains open" in close,
        "Selection verifies availability before draft mutation; tab reset/randomize preserve the global choice, reset-all uses schema defaults, and Cancel restores atomically",
        {
            "ordered_select": ordered_select,
            "tab_reset_mentions_backend": "AvatarBackendId" in reset_tab,
            "reset_all_defaults": "MakeDefaultCustomization()" in reset_all,
            "randomize_runtime": "DiscGolfFullCharacterRuntime::Randomize" in randomize,
            "cancel_snapshot_restore": "CharacterCreatorOpeningCustomization" in close,
            "cancel_failure_keeps_open": "the creator remains open" in close,
        },
    )


def _validate_retarget_and_visual_contract(v: Validator) -> None:
    retarget_h = v.text(RETARGET_H)
    retarget_cpp = v.text(RETARGET_CPP)
    adapter_h = v.text(ADAPTER_H)
    adapter_cpp = v.text(ADAPTER_CPP)
    contract = v.text(VISUAL_CONTRACT)
    tokens = (
        "UDiscGolfMetaHumanRetargetAnimInstance : public UAnimInstance",
        "FAnimNode_RetargetPoseFromMesh",
        "TObjectPtr<UIKRetargeter> VerifiedRetargeter",
        "ERetargetSourceMode::CustomSkeletalMeshComponent",
        "HasSourceIKRig()",
        "HasTargetIKRig()",
        "EnsureProcessorIsInitialized",
        "Processor->IsInitialized()",
        "WasInitializedWithTheseAssets",
        "bSuppressWarnings = false",
    )
    combined = retarget_h + "\n" + retarget_cpp
    v.add(
        "typed_native_retarget_instance",
        all(token in combined for token in tokens)
        and "UAnimBlueprint" not in combined
        and "TSoftObjectPtr<UObject>" not in combined,
        "The presentation body uses one typed native Retarget Pose From Mesh instance and verifies source rig, target rig, processor, and exact assets",
        {"missing_tokens": [token for token in tokens if token not in combined]},
    )

    contract_tokens = (
        "ConfigureFromDGAnimationSource",
        "ApplyMappedCustomization",
        "bool ConfigureVisualBackend_Implementation",
        "bool ApplyVisualCustomization_Implementation",
        "TInlineComponentArray<UMetaHumanComponentUE*>",
        "MetaHumanComponents.Num() != 1",
        "VisualActor->IsA<APawn>()",
        "VisualActor->IsA<AController>()",
        "FindComponentByClass<UMovementComponent>()",
        "Assembled visual must be a presentation-only actor.",
        "VisualRoot->IsAttachedTo(AnimationSourceMesh)",
    )
    combined_adapter = contract + "\n" + adapter_h + "\n" + adapter_cpp
    v.add(
        "actor_interface_verified_ready_contract",
        all(token in combined_adapter for token in contract_tokens)
        and "has no MetaHuman module dependency" not in adapter_h,
        "Project interface, adapter, one MetaHuman component, owner/world/root attachment, and presentation-only actor checks must all pass before ready",
        {
            "missing_tokens": [token for token in contract_tokens if token not in combined_adapter],
            "stale_no_dependency_claim": "has no MetaHuman module dependency" in adapter_h,
        },
    )


def _validate_atomic_fallback(v: Validator) -> None:
    plugin = v.text(PLUGIN_BACKEND_CPP)
    pawn = v.text(PAWN_CPP)
    build_fn = _cpp_function(plugin, "bool UDiscGolfAvatarBackendComponent::BuildVisualBackend(")
    apply_fn = _cpp_function(pawn, "bool ADiscGolferPawn::ApplyAvatarBackendForCustomization(")
    transaction_fn = _cpp_function(pawn, "bool ADiscGolferPawn::ApplyFullCharacterCustomizationTransactionally(")
    build_indices = {
        "spawn_deferred": build_fn.find("SpawnActorDeferred<AActor>"),
        "initial_hide": build_fn.find("CandidateActor->SetActorHiddenInGame(true)"),
        "finish_spawning": build_fn.find("CandidateActor->FinishSpawning"),
        "configure_while_hidden": build_fn.find(
            "const bool bConfigured = ConfigureVisualBackend"
        ),
        "structural_readiness": build_fn.find(
            "const bool bCandidateReady = bConfigured"
        ),
        "structural_failure_gate": build_fn.find("if (!bCandidateReady)"),
        "activation_show": build_fn.find(
            "CandidateActor->SetActorHiddenInGame(false)"
        ),
        "render_activation_gate": build_fn.find(
            "if (!FinalizeVisualBackendActivation("
        ),
    }
    build_indices["activation_failure_rehide"] = build_fn.find(
        "CandidateActor->SetActorHiddenInGame(true)",
        build_indices["activation_show"] + 1,
    )
    build_indices["activation_failure_destroy"] = build_fn.find(
        "DestroyVisualActor(CandidateActor);",
        build_indices["render_activation_gate"] + 1,
    )
    build_indices["commit_candidate"] = build_fn.find(
        "SpawnedVisualActor = CandidateActor"
    )
    build_indices["mark_visual_ready"] = build_fn.find(
        "ActiveBackendState.bVisualReady = true"
    )
    indices = list(build_indices.values())
    build_ordered = all(index >= 0 for index in indices) and indices == sorted(indices)
    fallback_index = apply_fn.find("if (!bReady || !AvatarBackendComponent->IsVisualBackendReady())")
    hide_index = apply_fn.find("SetDGProxyPresentationVisible(false)")
    fallback_tokens = (
        "SetDGProxyPresentationVisible(true)",
        "DestroyVisualBackend()",
        "return bAllowDGMasterFallback",
        "DGMaster remains visible",
    )
    v.add(
        "atomic_proxy_hide_fallback_and_cancel_foundation",
        build_ordered
        and all(token in apply_fn for token in fallback_tokens)
        and fallback_index >= 0
        and hide_index > fallback_index
        and "const FDGFullCharacterCustomization Previous" in transaction_fn
        and transaction_fn.count("CharacterCustomization->Current = Previous") >= 2
        and "ApplyAvatarBackendForCustomization(\n            Previous, true" in transaction_fn,
        "Candidate stays hidden through structural/configuration readiness, then passes a transactional render-activation gate before commit; every failure exposes DGMaster and restores the prior backend",
        {
            "build_ordered": build_ordered,
            "build_indices": build_indices,
            "missing_fallback_tokens": [t for t in fallback_tokens if t not in apply_fn],
            "proxy_hide_after_failure_branch": hide_index > fallback_index >= 0,
            "transaction_restore_count": transaction_fn.count("CharacterCustomization->Current = Previous"),
        },
    )

    pawn_component_count = pawn.count(
        "CreateDefaultSubobject<UDiscGolfMetaHumanAvatarBackendComponent>"
    )
    v.add(
        "one_metahuman_component_on_existing_pawn",
        pawn_component_count == 1
        and pawn.count('TEXT("MetaHumanVisualBackend")') == 1
        and "TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> AvatarBackendComponent" in v.text(PAWN_H),
        "The existing DG pawn owns exactly one optional MetaHuman backend component",
        {
            "constructor_count": pawn_component_count,
            "stable_name_count": pawn.count('TEXT("MetaHumanVisualBackend")'),
        },
    )


def _validate_single_authority(v: Validator) -> None:
    source_files = sorted((v.root / "Source/DiscGolfTour").rglob("*.h")) + sorted(
        (v.root / "Source/DiscGolfTour").rglob("*.cpp")
    )
    source_text = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in source_files)
    counts = {
        "pawn_subclasses": len(re.findall(r":\s*public\s+APawn\b", source_text)),
        "disc_bags": source_text.count("CreateDefaultSubobject<UDiscBagComponent>"),
        "disc_flights": source_text.count("CreateDefaultSubobject<UDiscFlightComponent>"),
        "framework_throws": source_text.count("CreateDefaultSubobject<UDiscGolfThrowComponent>"),
        "rhbh_adapters": source_text.count("CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>"),
        "metahuman_backends": source_text.count("CreateDefaultSubobject<UDiscGolfMetaHumanAvatarBackendComponent>"),
    }
    diff = _git(v.root, "diff", "--unified=0", "HEAD", "--", "Source", "Plugins")
    added, _ = _added_removed_lines(diff.stdout if diff.returncode == 0 else "")
    added_text = "\n".join(line for line, count in added.items() for _ in range(count))
    forbidden = (
        "CreateDefaultSubobject<UDiscBagComponent>",
        "CreateDefaultSubobject<UDiscFlightComponent>",
        "CreateDefaultSubobject<UDiscGolfThrowComponent>",
        "CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>",
        "SpawnActor<ADiscActor",
        "SaveGameToSlot",
        "LoadGameFromSlot",
        "ResolveThrowRelease",
        "AddStroke",
    )
    hits = [token for token in forbidden if token in added_text]
    request_throw_lines = [
        line
        for line, count in added.items()
        for _ in range(count)
        if "RequestThrowFromGrip" in line
    ]
    retained_authority_marker_ok = (
        len(request_throw_lines) == 1
        and "DG_SESSION8_COOK_CLOSURE_SMOKE: PASS" in request_throw_lines[0]
        and "release_authority=DiscGolfTourGameMode.RequestThrowFromGrip"
        in request_throw_lines[0]
        and "practice_snapshot_save_suppressed=1" in request_throw_lines[0]
    )
    if request_throw_lines and not retained_authority_marker_ok:
        hits.append("RequestThrowFromGrip_OUTSIDE_EXACT_COOK_EVIDENCE_MARKER")
    v.add(
        "single_dg_gameplay_authority",
        counts == {
            "pawn_subclasses": 1,
            "disc_bags": 1,
            "disc_flights": 1,
            "framework_throws": 1,
            "rhbh_adapters": 1,
            "metahuman_backends": 1,
        }
        and not hits
        and retained_authority_marker_ok,
        "Session 8B adds presentation only: no second pawn, bag, release path, flight component, inventory authority, or save path",
        {
            "authority_counts": counts,
            "forbidden_added_line_hits": hits,
            "retained_authority_marker_ok": retained_authority_marker_ok,
            "request_throw_added_lines": request_throw_lines,
        },
    )


def _validate_author_script(v: Validator) -> None:
    text = v.text(AUTHOR)
    try:
        tree = ast.parse(text, filename=AUTHOR)
        parse_error = None
    except SyntaxError as exc:
        tree = ast.Module(body=[], type_ignores=[])
        parse_error = str(exc)
    imports = {
        alias.name.split(".")[0]
        for node in ast.walk(tree)
        if isinstance(node, (ast.Import, ast.ImportFrom))
        for alias in node.names
    }
    save_calls = [
        node
        for node in ast.walk(tree)
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and node.func.attr == "save_packages"
    ]
    save_function = next(
        (node for node in tree.body if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "_save_exact_packages"),
        None,
    )
    save_calls_in_function = [
        node
        for node in ast.walk(save_function) if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute) and node.func.attr == "save_packages"
    ] if save_function else []
    forbidden_imports = sorted(imports & {"subprocess", "webbrowser", "selenium", "requests"})
    called_attrs = {
        node.func.attr.casefold()
        for node in ast.walk(tree)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
    }
    portal_calls = sorted(attr for attr in called_attrs if "portal" in attr or attr in {"open_url", "launch_url"})
    required = (
        'CHARACTER_ASSET_NAME = "MHC_DG_Golfer_Default"',
        'SOURCE_PACKAGE_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Source"',
        'GENERATED_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Generated"',
        'COMMON_ROOT = "/Game/DiscGolf/Characters/MetaHuman/Common"',
        'PREFLIGHT_SWITCH = "-DGSession8BMetaHumanPreflightOnly"',
        'AUTHOR_SWITCH = "-DGSession8BMetaHumanAuthor"',
        'NO_ACCOUNT_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"',
        'ALLOW_COMMANDLET_RENDERING_SWITCH = "-AllowCommandletRendering"',
        'RENDER_OFFSCREEN_SWITCH = "-RenderOffscreen"',
        'D3D12_SWITCH = "-d3d12"',
        '"-ini:Engine:[ConsoleVariables]:TextureGraph.AllowCommandlets=True"',
        'get_console_variable_bool_value(',
        '"TextureGraph.AllowCommandlets"',
        "unreal.MetaHumanRigType.JOINTS_ONLY",
        "unreal.MetaHumanDefaultPipelineType.OPTIMIZED",
        "unreal.MetaHumanQualityLevel.MEDIUM",
        "rig_request.blocking = True",
        "texture_request.blocking = True",
        "has_high_resolution_textures",
        "can_build_meta_human",
        "actual_dirty != expected_dirty",
        "EXPECTED_DISCARDED_TRANSIENT_CLASSES",
        "EXPECTED_PERSISTENT_GENERATED_OUTPUT_COUNT = 261",
        'TRANSIENT_DISPOSITION = "EPIC_DELETED_FACE_BAKE_INTERMEDIATE"',
        "_validate_exact_discarded_transients",
        "_clean_empty_rollback_residue",
        "intentionally_discarded_dirty_packages_at_exit",
        "commandlet_exit_discards_transients",
        "persistent_save_allowlist_count",
        "transient_tombstone_count",
        "discarded transient remains in the asset registry",
        "_restore_metahuman_tree",
        "runtime_profile_created",
        "retarget_asset_created",
        "credential_arguments_present",
        "external -UserDir",
    )
    v.add(
        "author_exact_modes_roots_and_fail_closed_save",
        parse_error is None
        and all(token in text for token in required)
        and len(save_calls) == 2
        and len(save_calls_in_function) == 2
        and not forbidden_imports
        and not portal_calls,
        "Authoring is confined to exact Source/Generated/Common roots, explicit preflight/author modes, blocking JointsOnly plus textures, Optimized Medium, exact save allowlist, and no unattended AccountPortal fallback",
        {
            "parse_error": parse_error,
            "missing_tokens": [token for token in required if token not in text],
            "save_calls_total": len(save_calls),
            "save_calls_in_exact_function": len(save_calls_in_function),
            "forbidden_imports": forbidden_imports,
            "account_portal_or_url_calls": portal_calls,
        },
    )

    runtime_author = v.text(RUNTIME_AUTHOR)
    runtime_validator = v.text(RUNTIME_VALIDATOR)
    profile_corrector = v.text(PROFILE_QUALITY_CORRECTOR)
    retarget_corrector = v.text(RETARGET_RUN_IK_CORRECTOR)
    helper_h = v.text(EDITOR_HELPER_H)
    helper_cpp = v.text(EDITOR_HELPER_CPP)
    tool_sources = {
        RUNTIME_AUTHOR: runtime_author,
        RUNTIME_VALIDATOR: runtime_validator,
        PROFILE_QUALITY_CORRECTOR: profile_corrector,
        RETARGET_RUN_IK_CORRECTOR: retarget_corrector,
    }
    tool_parse_errors: dict[str, str] = {}
    tool_trees: dict[str, ast.AST] = {}
    forbidden_tool_imports: dict[str, list[str]] = {}
    for relative, source in tool_sources.items():
        try:
            tool_trees[relative] = ast.parse(source, filename=relative)
        except SyntaxError as exc:
            tool_parse_errors[relative] = str(exc)
            tool_trees[relative] = ast.Module(body=[], type_ignores=[])
        imports_for_tool = {
            alias.name.split(".")[0]
            for node in ast.walk(tool_trees[relative])
            if isinstance(node, (ast.Import, ast.ImportFrom))
            for alias in node.names
        }
        forbidden_tool_imports[relative] = sorted(
            imports_for_tool
            & {"subprocess", "webbrowser", "selenium", "requests", "urllib"}
        )
    profile_corrector_save_calls = [
        node
        for node in ast.walk(tool_trees[PROFILE_QUALITY_CORRECTOR])
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and node.func.attr == "save_loaded_asset"
    ]
    retarget_corrector_save_calls = [
        node
        for node in ast.walk(tool_trees[RETARGET_RUN_IK_CORRECTOR])
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and node.func.attr == "save_loaded_asset"
    ]
    runtime_tokens = (
        'SCHEMA = "DiscGolfTour.Session8BMetaHumanRuntimeAssetAuthor.v1"',
        'AUTHOR_SWITCH = "-DGSession8BMetaHumanRuntimeAssetsAuthor"',
        "EXACT_THREE_ASSET_WRITES",
        "rollback_fresh_canonical_assets",
        "project SaveGames baseline is not the sole production slot",
        'SCHEMA = "DiscGolfTour.Session8BMetaHumanRuntimeAssetValidation.v2"',
        'VALIDATE_SWITCH = "-DGSession8BMetaHumanRuntimeAssetsValidate"',
        "PASS_NO_DISK_MUTATION",
        "fresh-process validation changed Content bytes/topology",
        "EXPECTED_RETARGET_OP_ENABLED_MASK = [True, True, False, True, True]",
        'EXPECTED_RETARGET_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"',
        '"retarget_output_pose_plausible": True',
        '"retarget_output_translation_outlier_count": 0',
        "retarget_output_max_abs_translation_cm",
    )
    profile_correction_tokens = (
        'SCHEMA = "DiscGolfTour.Session8BMetaHumanProfileQualityCorrection.v1"',
        'OLD_QUALITY = "GameplayHigh"',
        'EXPECTED_QUALITY = "GameplayPerformance"',
        'EXPECTED_PIPELINE = "UE_OPTIMIZED"',
        'EXPECTED_OPTIMIZATION = "MEDIUM"',
        "profile = None",
        "unreal.SystemLibrary.collect_garbage()",
        "restore_temp.replace(profile_path)",
        "EXACT_ONE_PROFILE_QUALITY_WRITE",
        "protected save boundary changed",
    )
    retarget_correction_tokens = (
        'SCHEMA = "DiscGolfTour.Session8BMetaHumanRetargetRunIKCorrection.v1"',
        'CORRECT_SWITCH = "-DGSession8BMetaHumanRetargetRunIKCorrection"',
        'EXPECTED_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"',
        "EXPECTED_OP_ENABLED_MASK = [True, True, False, True, True]",
        '"IKRetargetPelvisMotionOp"',
        '"IKRetargetFKChainsOp"',
        '"IKRetargetRunIKRigOp"',
        '"IKRetargetRootMotionOp"',
        '"IKRetargetCurveRemapOp"',
        "resolve_prepare_helper",
        "prepare_session8_b_meta_human_run_ik_correction",
        "retarget_output_pose_plausible",
        "retarget_output_translation_outlier_count",
        "retarget_output_max_abs_translation_cm",
        "state[\"attempted_writes\"] = [RETARGET_OBJECT]",
        "state[\"asset_save_call_count\"] = 1",
        "restore_temp.replace(retarget_path)",
        "EXACT_ONE_RETARGETER_WRITE",
        "protected save boundary changed",
    )
    helper_tokens = (
        "AuthorSession8BMetaHumanAssets",
        "ValidateSession8BMetaHumanAssets",
        "PrepareSession8BMetaHumanRunIKCorrection",
        "constexpr int32 SchemaVersion = 2;",
        "FIKRetargetPelvisMotionOp::StaticStruct()",
        "FIKRetargetFKChainsOp::StaticStruct()",
        "FIKRetargetRunIKRigOp::StaticStruct()",
        "FIKRetargetRootMotionOp::StaticStruct()",
        "FIKRetargetCurveRemapOp::StaticStruct()",
        "SetRetargetOpEnabled(RunIKOpIndex, false)",
        "MaxPlausibleTranslationCm",
        'TEXT("PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED")',
        'ExpectedQualityProfileId(TEXT("GameplayPerformance"))',
        'TEXT("NO_REFLECTED_PROPERTY_FIXED_ASSEMBLED_ACTOR")',
        'TEXT("preferred_quality_profile_id")',
        'TEXT("assembly_pipeline"), TEXT("UE_OPTIMIZED")',
        'TEXT("assembly_optimization_level"), TEXT("MEDIUM")',
        "ReloadCanonicalAssetsFromDisk",
        "canonical_packages_absent_before_reload",
        "VerifyPackageSnapshotsUnchanged",
        "ValidateNoUnexpectedNewDirtyPackages",
    )
    v.add(
        "canonical_runtime_asset_tools_fail_closed",
        not tool_parse_errors
        and not any(forbidden_tool_imports.values())
        and all(token in runtime_author + "\n" + runtime_validator for token in runtime_tokens)
        and all(token in profile_corrector for token in profile_correction_tokens)
        and all(token in retarget_corrector for token in retarget_correction_tokens)
        and len(profile_corrector_save_calls) == 1
        and len(retarget_corrector_save_calls) == 1
        and all(token in helper_h + "\n" + helper_cpp for token in helper_tokens),
        "Canonical author, Fresh3 validator, one-profile quality correction, one-retarget Run IK correction, and editor helper preserve exact roots, policy, reload evidence, write scope, rollback, and save isolation",
        {
            "parse_errors": tool_parse_errors,
            "forbidden_imports": forbidden_tool_imports,
            "missing_runtime_tokens": [
                token
                for token in runtime_tokens
                if token not in runtime_author + "\n" + runtime_validator
            ],
            "missing_profile_correction_tokens": [
                token
                for token in profile_correction_tokens
                if token not in profile_corrector
            ],
            "missing_retarget_correction_tokens": [
                token
                for token in retarget_correction_tokens
                if token not in retarget_corrector
            ],
            "profile_corrector_save_loaded_asset_calls": len(
                profile_corrector_save_calls
            ),
            "retarget_corrector_save_loaded_asset_calls": len(
                retarget_corrector_save_calls
            ),
            "missing_helper_tokens": [
                token for token in helper_tokens if token not in helper_h + "\n" + helper_cpp
            ],
        },
    )


def _validate_s8b_cook_successor_sources(v: Validator) -> None:
    asset_relative, no_write_relative = S8B_COOK_VALIDATORS
    sources = {
        asset_relative: v.text(asset_relative),
        no_write_relative: v.text(no_write_relative),
    }
    parse_errors: dict[str, str] = {}
    trees: dict[str, ast.AST] = {}
    forbidden_imports: dict[str, list[str]] = {}
    dangerous_calls: dict[str, list[str]] = {}
    report_write_counts: dict[str, int] = {}
    dangerous_names = {
        "save_asset",
        "save_loaded_asset",
        "save_packages",
        "save_packages_for_objects",
        "delete_asset",
        "rename_asset",
        "import_asset",
        "import_assets",
        "checkout_loaded_assets",
    }
    for relative, source in sources.items():
        try:
            tree = ast.parse(source, filename=relative)
        except SyntaxError as exc:
            parse_errors[relative] = str(exc)
            tree = ast.Module(body=[], type_ignores=[])
        trees[relative] = tree
        imports = {
            alias.name.split(".")[0]
            for node in ast.walk(tree)
            if isinstance(node, (ast.Import, ast.ImportFrom))
            for alias in node.names
        }
        forbidden_imports[relative] = sorted(
            imports & {"subprocess", "webbrowser", "requests", "urllib", "selenium"}
        )
        call_names = [
            node.func.attr
            for node in ast.walk(tree)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
        ]
        dangerous_calls[relative] = sorted(
            name for name in call_names if name in dangerous_names
        )
        report_write_counts[relative] = call_names.count("write_text")

    asset_tokens = (
        '"1B37EDF5A13EFDEA9243A97E2E733477AA4B6D6394AC82CAB6FA4FA6651BCB09"',
        '"003A7683069A2353819421CD71ECA82A2A0F8AE2E7786F56E04EA18DB11AB3B9"',
        'EXPECTED_INVENTORY_SEMANTICS = (',
        'EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT = 340',
        'EXPECTED_METAHUMAN_BACKEND_ID = "metahuman_assembled"',
        'EXPECTED_SOURCE_MHC_PACKAGE = (',
        '"GameplayPerformance"',
        'availability_gate = "SEPARATE_S8B_AUDIT_NOT_INVOKED"',
        '"DiscGolfTour.Session8BCookAssetValidation.v1"',
        '"status": "PASS_NO_DISK_MUTATION" if not errors else "FAIL"',
        '"runtime_package_count": len(runtime)',
        '"metahuman_runtime_package_count": len(metahuman_runtime)',
        '"backend_profile_count": 2',
        '"asset_save_calls": 0',
        '"cook_launched": False',
        '"ubt_launched": False',
    )
    no_write_tokens = (
        'PROJECT_ROOT / "Scripts/validate_dg_character_session8b_cook_assets.py"',
        'run_name="dg_session8b_cook_no_write_strict"',
        '"SEPARATE_S8B_AUDIT_NOT_INVOKED"',
        '"runtime_package_count": 69',
        '"metahuman_runtime_package_count": 264',
        '"DiscGolfTour.Session8BCookNoWriteValidation.v1"',
        '"status": "PASS_NO_WRITE" if not errors else "FAIL"',
        '"save_mutation": "NONE" if saves_before == saves_after else "DETECTED"',
        '"asset_save_calls": 0',
        '"cook_launched": False',
        '"ubt_launched": False',
    )
    asset_missing = [token for token in asset_tokens if token not in sources[asset_relative]]
    no_write_missing = [
        token for token in no_write_tokens if token not in sources[no_write_relative]
    ]
    source_identities = {
        relative: _record(v.root / relative) for relative in S8B_COOK_VALIDATORS
    }
    identities_ok = all(
        record["exists"]
        and record["bytes"] == expected["bytes"]
        and record["sha256"] == expected["sha256"]
        for relative, expected in S8B_COOK_VALIDATOR_IDENTITIES.items()
        for record in (source_identities[relative],)
    )
    v.add(
        "s8b_cook_successors_cross_bound_without_availability_recursion",
        identities_ok
        and not parse_errors
        and not any(forbidden_imports.values())
        and not any(dangerous_calls.values())
        and report_write_counts == {asset_relative: 1, no_write_relative: 1}
        and not asset_missing
        and not no_write_missing,
        "Cook successors independently freeze 69 legacy plus 264 MetaHuman packages and two profiles, exclude the editor source, preserve GameplayPerformance, and leave this intentionally BLOCKED availability audit as a separate gate",
        {
            "parse_errors": parse_errors,
            "source_identities": source_identities,
            "forbidden_imports": forbidden_imports,
            "dangerous_asset_calls": dangerous_calls,
            "report_write_counts": report_write_counts,
            "asset_missing_tokens": asset_missing,
            "no_write_missing_tokens": no_write_missing,
            "availability_gate": "SEPARATE_S8B_AUDIT_NOT_INVOKED",
        },
    )


def _validate_assets_and_availability(v: Validator, availability_report: Path) -> None:
    source = v.root / SOURCE_FILE
    meta_root = v.root / "Content/DiscGolf/Characters/MetaHuman"
    common_root = meta_root / "Common"
    generated_root = meta_root / "Generated"
    assembly = _asset_partition_summary((common_root, generated_root), meta_root)
    common_files = (
        sum(path.is_file() for path in common_root.rglob("*"))
        if common_root.is_dir()
        else 0
    )
    generated_files = (
        sum(path.is_file() for path in generated_root.rglob("*"))
        if generated_root.is_dir()
        else 0
    )
    meta_files = (
        sorted(path for path in meta_root.rglob("*") if path.is_file())
        if meta_root.is_dir()
        else []
    )
    meta_packages = sorted(
        "/Game/" + path.relative_to(v.root / "Content").with_suffix("").as_posix()
        for path in meta_files
        if path.suffix.casefold() == ".uasset"
    ) if meta_root.is_dir() else []
    runtime_assets = {
        package: _record(_package_file(v.root, package))
        for package in CANONICAL_RUNTIME_ASSET_IDENTITIES
    }
    runtime_sidecars = {
        package: [
            str(Path(f"{_package_file(v.root, package).with_suffix('')}{suffix}"))
            for suffix in (".uexp", ".ubulk", ".uptnl")
            if Path(f"{_package_file(v.root, package).with_suffix('')}{suffix}").is_file()
        ]
        for package in CANONICAL_RUNTIME_ASSET_IDENTITIES
    }
    runtime_identities_ok = all(
        record["exists"]
        and record["bytes"] == expected["bytes"]
        and record["sha256"] == expected["sha256"]
        for package, expected in CANONICAL_RUNTIME_ASSET_IDENTITIES.items()
        for record in (runtime_assets[package],)
    )
    exact_asset_checkpoint = (
        source.is_file()
        and source.stat().st_size == SOURCE_BYTES
        and _sha256(source) == SOURCE_SHA256
        and common_files == 205
        and generated_files == 56
        and assembly
        == {
            "file_count": ASSEMBLY_FILE_COUNT,
            "bytes": ASSEMBLY_BYTES,
            "manifest_sha256": ASSEMBLY_MANIFEST_SHA256,
        }
        and len(meta_files) == 264
        and len(meta_packages) == 264
        and all(path.suffix.casefold() == ".uasset" for path in meta_files)
        and SOURCE_PACKAGE in meta_packages
        and GENERATED_BP_PACKAGE in meta_packages
        and METAHUMAN_ACTOR_PACKAGE in meta_packages
        and METAHUMAN_RETARGET_PACKAGE in meta_packages
        and runtime_identities_ok
        and all(not sidecars for sidecars in runtime_sidecars.values())
    )
    v.add(
        "canonical_optimized_medium_assets_present",
        exact_asset_checkpoint,
        "Frozen editor source, exact 261-package Optimized-Medium assembly, and exact wrapper/retarget/GameplayPerformance profile are present without sidecars",
        {
            "source": _record(source),
            "common_files": common_files,
            "generated_files": generated_files,
            "assembly": assembly,
            "metahuman_tree_file_count": len(meta_files),
            "metahuman_package_count": len(meta_packages),
            "runtime_assets": runtime_assets,
            "runtime_sidecars": runtime_sidecars,
        },
    )

    try:
        report = json.loads(availability_report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        report = {}
    project_truth = report.get("project_asset_truth", {}) if isinstance(report, dict) else {}
    core = report.get("metahuman_creator_core_data", {}) if isinstance(report, dict) else {}
    assembly_truth = project_truth.get("optimized_medium_assembly", {})
    runtime_truth = project_truth.get("canonical_runtime_assets", {})
    quality_correction = report.get("quality_correction_evidence", {})
    retarget_correction = report.get("retarget_run_ik_correction_evidence", {})
    current = report.get("current_runtime_asset_validation", {})
    retarget_policy = project_truth.get("retarget_runtime_policy", {})
    quality = report.get("quality_metadata_contract", {})
    cook_prep = report.get("cook_prep_source", {})
    cook_inventory = report.get("cook_source_inventory", {})
    exact_retarget_object = (
        METAHUMAN_RETARGET_PACKAGE + ".RTG_DGMaster_To_MetaHuman"
    )
    exact_retarget_relative = (
        "DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
    )
    retarget_correction_ok = (
        retarget_correction.get("status")
        == "VERIFIED_EXACT_ONE_RETARGET_RUN_IK_POLICY_WRITE"
        and retarget_correction.get("run_id") == RETARGET_CORRECTION_RUN_ID
        and retarget_correction.get("report_schema")
        == "DiscGolfTour.Session8BMetaHumanRetargetRunIKCorrection.v1"
        and _identity_matches(
            retarget_correction.get("report_file"),
            RETARGET_CORRECTION_REPORT_IDENTITY,
        )
        and _identity_matches(
            retarget_correction.get("log_file"),
            RETARGET_CORRECTION_LOG_IDENTITY,
        )
        and retarget_correction.get("disk_mutation")
        == "EXACT_ONE_RETARGETER_WRITE"
        and retarget_correction.get("attempted_writes") == [exact_retarget_object]
        and retarget_correction.get("writes") == [exact_retarget_object]
        and retarget_correction.get("asset_save_call_count") == 1
        and _identity_matches(
            retarget_correction.get("retargeter_before"),
            STALE_RETARGET_IDENTITY,
        )
        and _identity_matches(
            retarget_correction.get("retargeter_after"),
            CANONICAL_RUNTIME_ASSET_IDENTITIES[METAHUMAN_RETARGET_PACKAGE],
        )
        and retarget_correction.get("content_before")
        == PRE_RETARGET_CORRECTION_CONTENT
        and retarget_correction.get("content_after") == CURRENT_CONTENT
        and retarget_correction.get("content_changed") == [exact_retarget_relative]
        and retarget_correction.get("content_added") == []
        and retarget_correction.get("content_removed") == []
        and retarget_correction.get("content_directories_unchanged") is True
        and retarget_correction.get("project_savegames_unchanged") is True
        and retarget_correction.get("production_save_unchanged") is True
        and retarget_correction.get("accepted_backup_unchanged") is True
        and retarget_correction.get("external_save_files") == []
        and retarget_correction.get("prepare_schema_version") == 2
        and retarget_correction.get("helper_schema_version") == 2
        and retarget_correction.get("success_markers_verified") is True
        and _identity_matches(
            retarget_correction.get("disk_record"),
            CANONICAL_RUNTIME_ASSET_IDENTITIES[METAHUMAN_RETARGET_PACKAGE],
        )
        and retarget_correction.get("sidecars") == []
        and _retarget_policy_evidence_matches(retarget_correction)
    )
    fresh3_validation_ok = (
        current.get("status")
        == "PRESENT_VERIFIED_FRESH_PROCESS_NO_DISK_MUTATION"
        and current.get("run_id") == CURRENT_VALIDATION_RUN_ID
        and current.get("report_schema")
        == "DiscGolfTour.Session8BMetaHumanRuntimeAssetValidation.v2"
        and current.get("helper_schema_version") == 2
        and _identity_matches(
            current.get("report_file"), CURRENT_VALIDATION_REPORT_IDENTITY
        )
        and _identity_matches(current.get("log_file"), CURRENT_VALIDATION_LOG_IDENTITY)
        and current.get("content") == CURRENT_CONTENT
        and current.get("content_directory_count_before") == 230
        and current.get("content_directory_count_after") == 230
        and current.get("accepted_assembly_partition")
        == CURRENT_ASSEMBLY_PARTITION
        and current.get("accepted_assembly_without_cook_manifest")
        == CURRENT_ASSEMBLY_WITHOUT_COOK_MANIFEST
        and _identity_matches(
            current.get("accepted_cook_manifest"),
            CURRENT_COOK_MANIFEST_IDENTITY,
        )
        and current.get("helper_writes") == []
        and current.get("helper_attempted_writes") == []
        and current.get("production_save_unchanged") is True
        and current.get("accepted_backup_unchanged") is True
        and current.get("external_save_files") == []
        and current.get("project_savegame_directories_before") == []
        and current.get("project_savegame_directories_after") == []
        and current.get("preferred_quality_profile_id")
        == EXPECTED_QUALITY_PROFILE
        and current.get("orientation_requires_runtime_visual_validation") is True
        and current.get("log_success_markers_verified") is True
        and current.get("disk_mismatches") == {}
        and _retarget_policy_evidence_matches(current)
    )
    policy_anchors = retarget_policy.get("output_anchor_transforms")
    project_policy_ok = (
        retarget_policy.get("classification")
        == "RUN_IK_DISABLED_FIXED_PRESENTATION_VERIFIED"
        and retarget_policy.get("retargeter") == exact_retarget_object
        and retarget_policy.get("op_types") == EXPECTED_RETARGET_OP_TYPES
        and retarget_policy.get("op_enabled_mask")
        == EXPECTED_RETARGET_OP_ENABLED_MASK
        and retarget_policy.get("policy") == EXPECTED_RETARGET_POLICY
        and retarget_policy.get("output_pose_plausible") is True
        and retarget_policy.get("output_max_abs_translation_cm")
        == EXPECTED_RETARGET_OUTPUT_MAX_ABS_TRANSLATION_CM
        and retarget_policy.get("output_translation_outlier_count") == 0
        and isinstance(policy_anchors, dict)
        and set(policy_anchors) == EXPECTED_RETARGET_ANCHORS
    )
    report_ok = (
        report.get("schema") == AVAILABILITY_SCHEMA
        and report.get("status") == "BLOCKED"
        and report.get("metahuman_integration_status") == "BLOCKED"
        and report.get("ready_claimed") is False
        and report.get("integrity_errors") == []
        and set(report.get("blockers", [])) == AVAILABILITY_BLOCKERS
        and report.get("blocker_count") == 3
        and core.get("status") == CORE_STATUS
        and core.get("file_type_counts")
        == {"files": 1816, "uassets": 1783, "umaps": 0, "other": 33}
        and project_truth.get("editor_only_source", {}).get("package")
        == SOURCE_PACKAGE
        and project_truth.get("editor_only_source", {}).get("classification")
        == "EDITOR_ONLY_AUTHORING_INPUT_NOT_RUNTIME_COOK_OUTPUT"
        and assembly_truth.get("classification") == "PRESENT_VERIFIED"
        and assembly_truth.get("common_files") == 205
        and assembly_truth.get("generated_files") == 56
        and assembly_truth.get("file_count") == ASSEMBLY_FILE_COUNT
        and assembly_truth.get("manifest_sha256") == ASSEMBLY_MANIFEST_SHA256
        and runtime_truth.get("classification") == "PRESENT_VERIFIED"
        and runtime_truth.get("objects")
        == [
            METAHUMAN_ACTOR_PACKAGE + ".BP_DG_MetaHuman_Default",
            METAHUMAN_RETARGET_PACKAGE + ".RTG_DGMaster_To_MetaHuman",
            METAHUMAN_PROFILE_PACKAGE
            + ".DA_DG_AvatarBackend_MetaHuman_Default",
        ]
        and project_truth.get("current_content") == CURRENT_CONTENT
        and retarget_correction_ok
        and fresh3_validation_ok
        and project_policy_ok
        and report.get("asset_authoring_status") == "PRESENT_VERIFIED"
        and quality_correction.get("status")
        == "VERIFIED_EXACT_ONE_PROFILE_QUALITY_WRITE"
        and quality_correction.get("preferred_quality_profile_id")
        == EXPECTED_QUALITY_PROFILE
        and quality
        == {
            "assembly_pipeline": "UE_OPTIMIZED",
            "assembly_optimization_level": "MEDIUM",
            "preferred_quality_profile_id": EXPECTED_QUALITY_PROFILE,
            "classification": "OPTIMIZED_MEDIUM_MATCHES_GAMEPLAY_PERFORMANCE",
        }
        and cook_prep.get("status")
        == "SOURCE_PREP_VERIFIED_PENDING_AUTHORED_ASSET_AND_PACKAGE_CLOSURE"
        and cook_prep.get("profile_quality") == EXPECTED_QUALITY_PROFILE
        and cook_prep.get("inventory_semantics")
        == "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH"
        and cook_prep.get("fresh_validation_transitive_runtime_dependency_count")
        == 340
        and cook_prep.get("transitive_dependency_evidence_status")
        == "FRESH2_SOURCE_VALIDATION_ONLY_REQUIRES_PACKAGED_IOSTORE_REPROOF"
        and cook_inventory
        == {
            "status": "SOURCE_AND_ASSET_PREVIOUSLY_VALIDATED_PENDING_POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE",
            "legacy_runtime_package_count": 69,
            "metahuman_explicit_cook_root_package_count": 264,
            "fresh_validation_transitive_runtime_dependency_count": 340,
            "inventory_semantics": "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH",
            "backend_profile_count": 2,
            "legacy_excluded_package_count": 12,
            "metahuman_excluded_package_count": 1,
            "metahuman_source_exclusion": SOURCE_PACKAGE,
            "packaged_iostore_reproof_required": True,
        }
        and report.get("cook_groundwork_status")
        == "SESSION8B_COOK_MANIFEST_SOURCE_AND_ASSET_PREVIOUSLY_VALIDATED_POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE_REQUIRED"
        and report.get("author_run_required") is False
        and report.get("post_retarget_recook_required") is True
        and report.get("blocker_state")
        == {
            "post_retarget_recook_and_packaged_closure": "NOT_YET_ACCEPTED",
            "runtime_visual_orientation_material_attachment_atomic_switching": "NOT_YET_ACCEPTED",
            "packaged_omen_performance": "NOT_YET_MEASURED",
        }
        and report.get("session8a_audit_reused_without_edit") is True
        and report.get("unreal_launched") is False
        and report.get("ubt_launched") is False
        and report.get("network_access") == "NONE"
    )
    v.add(
        "current_blocked_availability_evidence",
        report_ok,
        "Successor evidence binds the one-RTG Run IK correction and Fresh3 no-write validation while retaining exactly the post-retarget cook, visual, and performance blockers",
        {
            "report": _record(availability_report),
            "schema": report.get("schema"),
            "status": report.get("status"),
            "blockers": report.get("blockers"),
            "integrity_errors": report.get("integrity_errors"),
            "assembly_status": assembly_truth.get("classification"),
            "runtime_asset_status": runtime_truth.get("classification"),
            "retarget_correction_ok": retarget_correction_ok,
            "fresh3_validation_ok": fresh3_validation_ok,
            "project_retarget_policy_ok": project_policy_ok,
            "current_content": project_truth.get("current_content"),
            "quality": quality,
            "cook_prep_status": cook_prep.get("status"),
            "cook_inventory_status": cook_inventory.get("status"),
        },
    )


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root")
    parser.add_argument("--availability-report", default=str(DEFAULT_AVAILABILITY_REPORT))
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--no-report", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    root = ROOT
    availability_report = Path(args.availability_report).expanduser().absolute()
    report_path = Path(args.report).expanduser().absolute()
    v = Validator(root)
    try:
        engine_root, engine_basis = _resolve_engine_root(root, args.engine_root)
    except (OSError, json.JSONDecodeError) as exc:
        engine_root, engine_basis = Path(), f"ERROR: {exc}"

    _validate_required_files(v, availability_report)
    _validate_predecessor_and_plugin_scope(v)
    _validate_engine_and_project(v, engine_root, engine_basis)
    _validate_schema_and_vocabulary(v)
    _validate_creator_semantics(v)
    _validate_retarget_and_visual_contract(v)
    _validate_atomic_fallback(v)
    _validate_single_authority(v)
    _validate_author_script(v)
    _validate_s8b_cook_successor_sources(v)
    _validate_assets_and_availability(v, availability_report)

    failures = [check for check in v.checks if not check.passed]
    payload = {
        "schema": SCHEMA,
        "status": "PASS_SOURCE_ASSETS_PRESENT_BLOCKED_CLOSURE"
        if not failures
        else "FAIL_SOURCE_WIRING",
        "generated_utc": datetime.now(timezone.utc).astimezone().isoformat(),
        "project_root": str(root),
        "engine_root": str(engine_root),
        "determinism": "SOURCE_FILESYSTEM_GIT_AST_ONLY_NO_UNREAL_UBT_BROWSER_NETWORK",
        "unreal_launched": False,
        "ubt_launched": False,
        "browser_launched": False,
        "network_access": "NONE",
        "metahuman_integration_status": "BLOCKED",
        "static_asset_status": "PRESENT_VERIFIED",
        "proxy_authoritative": True,
        "ready_claimed": False,
        "schema_version": 11,
        "backend_ids": [DGMASTER_ID, METAHUMAN_ID],
        "availability_report": str(availability_report),
        "check_count": len(v.checks),
        "passed_count": len(v.checks) - len(failures),
        "failed_count": len(failures),
        "failed_check_ids": [check.check_id for check in failures],
        "checks": [asdict(check) for check in v.checks],
        "writes": [] if args.no_report else [str(report_path)],
    }
    if not args.no_report:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    print(
        f"SESSION 8B WIRING {payload['status']}: "
        f"{payload['passed_count']}/{payload['check_count']} checks; "
        f"metahuman=BLOCKED"
    )
    for check in failures:
        print(f"  FAIL {check.check_id}: {check.summary}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
