#!/usr/bin/env python3
"""Static, fail-closed validation for the Session 5 mocap pipeline wiring.

This validator never imports Unreal or modifies project/content files. Its only
write is the JSON report under Saved/CharacterFramework.
"""

from __future__ import annotations

import ast
import copy
import difflib
import hashlib
import json
import re
import runpy
import subprocess
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


PROJECT_ROOT = Path(__file__).absolute().parents[1]
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session5WiringValidation.json"
)
BUILDKIT_ROOT = (
    PROJECT_ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5"
)
BUILDKIT_INVENTORY = BUILDKIT_ROOT / "FILE_INVENTORY.json"
BUILDKIT_INVENTORY_SHA256 = (
    "1C0F133DA80E9330AC28E5D9D20A8A60E8A79452BF4DB09E76C42DBF20813361"
)
BUILDKIT_INVENTORY_BYTES = 48821
BUILDKIT_INVENTORY_RECORDS = 251

PLUGIN_ROOT = "Plugins/DiscGolfCharacterFramework"
BUILDKIT_PLUGIN_ROOT = (
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Plugins/"
    "DiscGolfCharacterFramework"
)

PROTECTED_PACKAGE_HASHES: dict[str, tuple[int, str]] = {
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset": (
        59553,
        "5C461476D6877DFBE3E6DF08FF48CDF5BB940BC8C883D3E3F43058331DCC186F",
    ),
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset": (
        17681,
        "D40A0C4FE4BFE100A553910E01CCB5C00ACD9D1C927D4C1387F25894541493EA",
    ),
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset": (
        63490,
        "13D29A1D4B4F1E2A6F010D95E052D21A1E974A6789BBFE85AC5E4E260EE72958",
    ),
    "Content/DiscGolf/Rigs/CR_DG_Master.uasset": (
        205650,
        "21BAA6E6C0C885F4F18BFF077FFCE3043916051DFF6675317004B09B2D9E10CF",
    ),
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset": (
        58351,
        "833454B7FC2F5F759795FB641295E1F41DBF0F4F1AB888476FE222D2DFB9D379",
    ),
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset": (
        321080,
        "EA53E0460B958FFA7C8BC1DCACB5A4E6F1C6ABBACE9F177C68A1017C6783ECE6",
    ),
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset": (
        20451,
        "6BCD1C3256668D7F041FE6D33B6910052EE77FA4739A1EF4E60E689A787A8AF8",
    ),
    "Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset": (
        21687,
        "69D1879FA25B2480EC3A6FED461954E7E824DA58BFCE33A143F2B7CBB405F1B0",
    ),
    "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset": (
        1458,
        "A0EF3A40A6E1A25E96C1E7B0344681A40D22A672B650708B82ABD22DAEEB6A9B",
    ),
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset": (
        1900,
        "8F1421E28C286F42EE6881669648DF40A7EF0E4FCAC944198D1C9F17FF0842DF",
    ),
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset": (
        1900,
        "A57F8CF320B67FB48213517127F0D1AC35F1DA79D518DFBE7FE589B6E347327D",
    ),
}
PROTECTED_PACKAGES = tuple(PROTECTED_PACKAGE_HASHES)

CANONICAL_PACKAGES = {
    "master_mesh": "/Game/DiscGolf/Characters/Meshes/SK_DG_Master",
    "master_skeleton": "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master",
    "master_ik_rig": "/Game/DiscGolf/Rigs/IK_DG_Master",
    "master_control_rig": "/Game/DiscGolf/Rigs/CR_DG_Master",
    "player_anim_blueprint": "/Game/DiscGolf/Animation/ABP_DG_Player",
    "prototype_sequence": "/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype",
    "prototype_montage": "/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype",
    "source_mesh": "/Game/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource",
    "source_skeleton": "/Game/DiscGolf/Animation/Mocap/Source/SKEL_DG_RHBH_SyntheticSource",
    "raw_sequence": "/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW",
    "source_ik_rig": "/Game/DiscGolf/Animation/Mocap/Rigs/IK_DG_RHBH_SyntheticSource",
    "retargeter": "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_DG_RHBH_Synthetic_To_Master",
    "retargeted_sequence": "/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG",
    "cleaned_sequence": "/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN",
    "production_sequence": "/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001",
    "pipeline_montage": "/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001",
    "animation_library": "/Game/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary",
}


def _object_path(package_path: str) -> str:
    return f"{package_path}.{package_path.rsplit('/', 1)[-1]}"


HEADER_CONSTANTS = {
    "SourceSequence": _object_path(CANONICAL_PACKAGES["raw_sequence"]),
    "SourceSkeletalMesh": _object_path(CANONICAL_PACKAGES["source_mesh"]),
    "RetargetedSequence": _object_path(CANONICAL_PACKAGES["retargeted_sequence"]),
    "CleanedSequence": _object_path(CANONICAL_PACKAGES["cleaned_sequence"]),
    "PipelineTestMontage": _object_path(CANONICAL_PACKAGES["pipeline_montage"]),
    "ProductionSequence": _object_path(CANONICAL_PACKAGES["production_sequence"]),
    "PrototypeMontage": _object_path(CANONICAL_PACKAGES["prototype_montage"]),
    "TargetSkeletalMesh": _object_path(CANONICAL_PACKAGES["master_mesh"]),
}
UTILITY_CONSTANTS = {
    "MasterMeshPath": _object_path(CANONICAL_PACKAGES["master_mesh"]),
    "MasterSkeletonPath": _object_path(CANONICAL_PACKAGES["master_skeleton"]),
    "MasterIKRigPath": _object_path(CANONICAL_PACKAGES["master_ik_rig"]),
    "PrototypeSequencePath": _object_path(CANONICAL_PACKAGES["prototype_sequence"]),
    "PrototypeMontagePath": _object_path(CANONICAL_PACKAGES["prototype_montage"]),
    "SourceMeshPath": _object_path(CANONICAL_PACKAGES["source_mesh"]),
    "SourceSkeletonPath": _object_path(CANONICAL_PACKAGES["source_skeleton"]),
    "RawSequencePath": _object_path(CANONICAL_PACKAGES["raw_sequence"]),
    "SourceIKRigPath": _object_path(CANONICAL_PACKAGES["source_ik_rig"]),
    "RetargeterPath": _object_path(CANONICAL_PACKAGES["retargeter"]),
    "RetargetedSequencePath": _object_path(CANONICAL_PACKAGES["retargeted_sequence"]),
    "CleanedSequencePath": _object_path(CANONICAL_PACKAGES["cleaned_sequence"]),
    "FinalSequencePath": _object_path(CANONICAL_PACKAGES["production_sequence"]),
    "FinalMontagePath": _object_path(CANONICAL_PACKAGES["pipeline_montage"]),
    "LibraryPath": _object_path(CANONICAL_PACKAGES["animation_library"]),
}

FIXTURE_PACKAGE_KEYS = (
    "source_mesh",
    "source_skeleton",
    "raw_sequence",
    "source_ik_rig",
    "retargeter",
    "retargeted_sequence",
    "cleaned_sequence",
    "production_sequence",
    "pipeline_montage",
    "animation_library",
)


def _content_file(package_path: str) -> str:
    return f"Content/{package_path.removeprefix('/Game/')}.uasset"


FIXTURE_PACKAGES = tuple(
    _content_file(CANONICAL_PACKAGES[key]) for key in FIXTURE_PACKAGE_KEYS
)

NEW_SESSION5_FILES = {
    "Docs/DG_MOCAP_PIPELINE.md",
    "Scripts/create_dg_character_session5_mocap_fixture.py",
    "Scripts/probe_dg_character_session5_mocap_units.py",
    "Scripts/validate_dg_character_session5_mocap_fixture.py",
    "Scripts/validate_motion_source_registry.py",
    "Scripts/validate_dg_character_session5_wiring.py",
    "Scripts/run-session5-mocap-profile-smokes.py",
    "Scripts/run-session5-mocap-visual-capture.py",
    "Source/DiscGolfTour/DiscGolfSession5MocapSmokeRunner.cpp",
    "Source/DiscGolfTour/DiscGolfSession5MocapSmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfSession5MocapValidationPaths.h",
    "Source/DiscGolfTour/DiscGolfSession5MocapVisualCaptureRunner.cpp",
    "Source/DiscGolfTour/DiscGolfSession5MocapVisualCaptureRunner.h",
    "Source/DiscGolfTourEditor/DiscGolfSession5MocapUtility.cpp",
    "Source/DiscGolfTourEditor/DiscGolfSession5MocapUtility.h",
    "SourceArt/DiscGolf/Mocap/motion_source_registry.json",
}
INTENDED_SESSION5_UNTRACKED = NEW_SESSION5_FILES | set(FIXTURE_PACKAGES)

REQUIRED_EXISTING_WIRING_FILES = {
    "DiscGolfTour.uproject",
    "Source/DiscGolfTour/DiscGolfSession3SmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolferPawn.cpp",
    "Source/DiscGolfTour/DiscGolferPawn.h",
    "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
    f"{PLUGIN_ROOT}/DiscGolfCharacterFramework.uplugin",
    f"{PLUGIN_ROOT}/Source/DiscGolfCharacterFramework/Public/DiscGolfAnimationLibrary.h",
    f"{PLUGIN_ROOT}/Source/DiscGolfCharacterFramework/Public/AnimNotify_DiscRelease.h",
    f"{PLUGIN_ROOT}/Source/DiscGolfCharacterFramework/Public/AnimNotify_ThrowFinished.h",
    f"{PLUGIN_ROOT}/Source/DiscGolfCharacterFramework/Public/AnimNotify_ThrowPhase.h",
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/CODEX/05_MOCAP_PIPELINE.md",
    "_BuildKit/DiscGolfCorePlayabilityKit_v1.5/Docs/07_MOCAP_PIPELINE.md",
}

KNOWN_UNRELATED_UNTRACKED_EXACT = {"Config/DefaultEditor.ini"}
KNOWN_UNRELATED_UNTRACKED_PREFIXES = (
    "Content/Data/",
    "Content/Environment/",
    "Content/PN_interactiveSpruceForest/",
    "Content/Presentation/",
    "Content/Stump_Scanned/",
    "Content/WaterMaterials/",
    "SourceArt/PineRidge/",
)
EXPECTED_UNRELATED_UNTRACKED_COUNT = 647

MANDATED_REGISTRY_FIELDS = {
    "motion_id",
    "source_filename",
    "creator_or_source",
    "acquisition_method",
    "license_permission_status",
    "original_frame_rate",
    "handedness",
    "throw_type",
    "import_date",
    "raw_asset_path",
    "retargeted_asset_path",
    "cleaned_asset_path",
    "production_asset_path",
    "production_status",
}
REGISTRY_FIXTURE_PATHS = {
    "raw_asset_path": CANONICAL_PACKAGES["raw_sequence"],
    "retargeted_asset_path": CANONICAL_PACKAGES["retargeted_sequence"],
    "cleaned_asset_path": CANONICAL_PACKAGES["cleaned_sequence"],
    "production_asset_path": CANONICAL_PACKAGES["production_sequence"],
}

FORBIDDEN_SESSION5_SOURCE_PATTERNS = {
    "SuggestedLaunchSpeedMps": re.compile(r"\bSuggestedLaunchSpeedMps\b"),
    "SuggestedSpinRpm": re.compile(r"\bSuggestedSpinRpm\b"),
    "GripLinearVelocity": re.compile(r"\bGripLinearVelocity\b"),
    "ResolveThrowRelease": re.compile(r"\bResolveThrowRelease\b"),
    "SpawnActor<ADiscActor>": re.compile(r"SpawnActor\s*<\s*ADiscActor\s*>"),
    "InitializeDisc": re.compile(r"\bInitializeDisc\b"),
    "UDiscFlightComponent": re.compile(r"\bUDiscFlightComponent\b"),
    "FThrowRelease": re.compile(r"\bFThrowRelease\b"),
    "direct RequestThrowFromGrip": re.compile(r"\bRequestThrowFromGrip\s*\("),
}
TRACKED_SESSION5_INTEGRATION_FILES = (
    "Source/DiscGolfTour/DiscGolfSession3SmokeRunner.h",
    "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
    "Source/DiscGolfTour/DiscGolfTourGameMode.h",
    "Source/DiscGolfTour/DiscGolferPawn.cpp",
    "Source/DiscGolfTour/DiscGolferPawn.h",
    "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
)

GENERATED_PARTS = {
    "Binaries",
    "DerivedDataCache",
    "Intermediate",
    "Saved",
    ".vs",
    ".idea",
    ".vscode",
    "__pycache__",
}
GENERATED_PREFIXES = (
    "Build/Receipts/",
    "Build/Windows/FileOpenOrder/",
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _read_text(relative_path: str) -> str:
    return (PROJECT_ROOT / relative_path).read_text(encoding="utf-8")


def _git(args: Iterable[str], *, binary: bool = False) -> str | bytes:
    result = subprocess.run(
        ["git", "-c", "core.quotepath=false", *args],
        cwd=PROJECT_ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git {' '.join(args)} failed: {stderr}")
    if binary:
        return result.stdout
    return result.stdout.decode("utf-8", errors="surrogateescape")


def _git_status() -> list[dict[str, str]]:
    raw = _git(
        [
            "status",
            "--porcelain=v1",
            "-z",
            "--untracked-files=all",
            "--no-renames",
        ],
        binary=True,
    )
    assert isinstance(raw, bytes)
    entries: list[dict[str, str]] = []
    for record in raw.split(b"\0"):
        if not record:
            continue
        if len(record) < 4 or record[2:3] != b" ":
            raise RuntimeError(f"unrecognized git porcelain record: {record!r}")
        entries.append(
            {
                "status": record[:2].decode("ascii", errors="replace"),
                "path": record[3:].decode("utf-8", errors="surrogateescape"),
            }
        )
    return entries


def _literal_assignment(path: str, name: str) -> Any:
    tree = ast.parse(_read_text(path), filename=path)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if any(isinstance(target, ast.Name) and target.id == name for target in node.targets):
                return ast.literal_eval(node.value)
        if isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.target.id == name:
                return ast.literal_eval(node.value)
    raise KeyError(f"{name} not found as a literal assignment in {path}")


TCHAR_CONSTANT_RE = re.compile(
    r"(?:inline\s+constexpr\s+)?const\s+TCHAR\s*\*\s*"
    r"([A-Za-z_]\w*)\s*=\s*TEXT\(\s*\"([^\"]+)\"\s*\)\s*;",
    re.S,
)


def _tchar_constants(text: str) -> dict[str, str]:
    return dict(TCHAR_CONSTANT_RE.findall(text))


def _normalize_object_path(path: str) -> str:
    path = path.rstrip(".,;:)")
    leaf = path.rsplit("/", 1)[-1]
    if "." not in leaf:
        return path
    package_leaf, object_leaf = leaf.split(".", 1)
    if object_leaf in {package_leaf, f"{package_leaf}_C"}:
        return f"{path.rsplit('/', 1)[0]}/{package_leaf}"
    return path


def _added_lines(relative_path: str) -> str:
    current_path = PROJECT_ROOT / relative_path
    current = current_path.read_text(encoding="utf-8").splitlines()
    baseline_raw = _git(["show", f"HEAD:{relative_path}"])
    assert isinstance(baseline_raw, str)
    baseline = baseline_raw.splitlines()
    output: list[str] = []
    matcher = difflib.SequenceMatcher(a=baseline, b=current, autojunk=False)
    for tag, _old_start, _old_end, new_start, new_end in matcher.get_opcodes():
        if tag in {"insert", "replace"}:
            output.extend(current[new_start:new_end])
    return "\n".join(output)


def _is_known_unrelated_untracked(path: str) -> bool:
    return path in KNOWN_UNRELATED_UNTRACKED_EXACT or path.startswith(
        KNOWN_UNRELATED_UNTRACKED_PREFIXES
    )


def _is_generated_path(path: str) -> bool:
    parts = PurePosixPath(path).parts
    return any(part in GENERATED_PARTS for part in parts) or path.startswith(
        GENERATED_PREFIXES
    )


def _check_required_paths() -> tuple[list[str], dict[str, Any]]:
    required = sorted(NEW_SESSION5_FILES | REQUIRED_EXISTING_WIRING_FILES)
    present = [path for path in required if (PROJECT_ROOT / path).is_file()]
    missing = sorted(set(required) - set(present))
    errors = [f"missing required Session 5 wiring file: {path}" for path in missing]
    return errors, {
        "required_count": len(required),
        "present_count": len(present),
        "missing": missing,
    }


def _check_fixture_packages() -> tuple[list[str], dict[str, Any]]:
    root = PROJECT_ROOT / "Content/DiscGolf/Animation/Mocap"
    present = [path for path in FIXTURE_PACKAGES if (PROJECT_ROOT / path).is_file()]
    missing = sorted(set(FIXTURE_PACKAGES) - set(present))
    actual = (
        {
            path.relative_to(PROJECT_ROOT).as_posix()
            for path in root.rglob("*")
            if path.is_file()
        }
        if root.is_dir()
        else set()
    )
    unexpected = sorted(actual - set(FIXTURE_PACKAGES))
    errors = [f"missing required authored fixture package: {path}" for path in missing]
    errors.extend(f"unexpected file under the fixture content root: {path}" for path in unexpected)
    sizes = {
        path: (PROJECT_ROOT / path).stat().st_size
        for path in present
    }
    for path, size in sizes.items():
        if size <= 0:
            errors.append(f"fixture package is empty: {path}")
    return errors, {
        "expected_count": len(FIXTURE_PACKAGES),
        "present_count": len(present),
        "missing": missing,
        "unexpected": unexpected,
        "sizes": sizes,
    }


def _compare_constants(
    label: str,
    actual: dict[str, str],
    expected: dict[str, str],
    errors: list[str],
) -> None:
    for name, expected_value in expected.items():
        actual_value = actual.get(name)
        if actual_value != expected_value:
            errors.append(
                f"{label} {name} is {actual_value!r}, expected {expected_value!r}"
            )


def _check_canonical_paths() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    evidence: dict[str, Any] = {}
    try:
        header = _read_text("Source/DiscGolfTour/DiscGolfSession5MocapValidationPaths.h")
        header_constants = _tchar_constants(header)
        _compare_constants("runtime constant", header_constants, HEADER_CONSTANTS, errors)
        evidence["runtime_constants"] = {
            name: header_constants.get(name) for name in HEADER_CONSTANTS
        }
    except Exception as exc:
        errors.append(f"could not inspect runtime canonical constants: {type(exc).__name__}: {exc}")

    try:
        utility_path = "Source/DiscGolfTourEditor/DiscGolfSession5MocapUtility.cpp"
        utility = _read_text(utility_path)
        utility_constants = _tchar_constants(utility)
        _compare_constants("editor constant", utility_constants, UTILITY_CONSTANTS, errors)
        evidence["editor_constants"] = {
            name: utility_constants.get(name) for name in UTILITY_CONSTANTS
        }
        compatibility_guard_fragments = {
            "raw_guard_function": "ValidateRawNonRootTranslationsAreStatic",
            "source_reference_local_lookup": (
                "RefSkeleton.GetRefBonePose()[BoneIndex].GetTranslation()"
            ),
            "fail_closed_reference_mismatch": (
                "RAW non-root translation differs from the source reference local"
            ),
        }
        compatibility_guards = {
            name: fragment in utility
            for name, fragment in compatibility_guard_fragments.items()
        }
        for name, present in compatibility_guards.items():
            if not present:
                errors.append(
                    "editor compatibility normalization omits required RAW guard: "
                    f"{name}"
                )
        evidence["synthetic_raw_reference_local_guard"] = compatibility_guards
    except Exception as exc:
        errors.append(f"could not inspect editor canonical constants: {type(exc).__name__}: {exc}")

    expected_protected = PROTECTED_PACKAGES
    expected_fixture = FIXTURE_PACKAGES
    python_contracts: dict[str, Any] = {}
    for path, assignments in (
        (
            "Scripts/create_dg_character_session5_mocap_fixture.py",
            {"PROTECTED_PACKAGES": expected_protected},
        ),
        (
            "Scripts/validate_dg_character_session5_mocap_fixture.py",
            {
                "PROTECTED_PACKAGES": expected_protected,
                "FIXTURE_PACKAGES": expected_fixture,
            },
        ),
    ):
        python_contracts[path] = {}
        for name, expected in assignments.items():
            try:
                actual = tuple(_literal_assignment(path, name))
                python_contracts[path][name] = list(actual)
                if actual != tuple(expected):
                    errors.append(
                        f"{path} {name} differs from the exact canonical package tuple"
                    )
            except Exception as exc:
                errors.append(f"could not inspect {path} {name}: {type(exc).__name__}: {exc}")
    evidence["python_package_contracts"] = python_contracts

    try:
        profile_runner = "Scripts/run-session5-mocap-profile-smokes.py"
        runner_montage = _literal_assignment(profile_runner, "PIPELINE_MONTAGE")
        expected_runner_montage = _object_path(CANONICAL_PACKAGES["pipeline_montage"])
        if runner_montage != expected_runner_montage:
            errors.append(
                f"{profile_runner} PIPELINE_MONTAGE is {runner_montage!r}, "
                f"expected {expected_runner_montage!r}"
            )
        evidence["profile_runner_pipeline_montage"] = runner_montage
    except Exception as exc:
        errors.append(
            "could not inspect the profile-smoke canonical montage: "
            f"{type(exc).__name__}: {exc}"
        )

    try:
        doc_path = "Docs/DG_MOCAP_PIPELINE.md"
        doc = _read_text(doc_path)
        missing_doc_paths = [
            package for package in CANONICAL_PACKAGES.values() if package not in doc
        ]
        if missing_doc_paths:
            errors.append(
                "project mocap procedure omits canonical packages: "
                + ", ".join(missing_doc_paths)
            )
        required_policy_phrases = (
            "SYNTHETIC_TEST",
            "DO_NOT_SHIP",
            "The prototype remains the normal gameplay fallback.",
        )
        missing_phrases = [phrase for phrase in required_policy_phrases if phrase not in doc]
        if missing_phrases:
            errors.append(
                "project mocap procedure omits fail-closed policy text: "
                + ", ".join(missing_phrases)
            )
        evidence["documentation"] = {
            "canonical_paths_present": len(CANONICAL_PACKAGES) - len(missing_doc_paths),
            "canonical_paths_expected": len(CANONICAL_PACKAGES),
            "missing_paths": missing_doc_paths,
            "missing_policy_phrases": missing_phrases,
        }
    except Exception as exc:
        errors.append(f"could not inspect project mocap procedure: {type(exc).__name__}: {exc}")

    allowed_mocap_paths = set(CANONICAL_PACKAGES.values()) | {
        "/Game/DiscGolf/Animation/Mocap/Source",
        "/Game/DiscGolf/Animation/Mocap/Rigs",
        "/Game/DiscGolf/Animation/Mocap/Retargeted",
        "/Game/DiscGolf/Animation/Mocap/Cleaned",
        "/Game/DiscGolf/Animation/Mocap/Production",
    }
    divergent: list[dict[str, str]] = []
    scan_paths = (
        "Docs/DG_MOCAP_PIPELINE.md",
        "Source/DiscGolfTour/DiscGolfSession5MocapValidationPaths.h",
        "Source/DiscGolfTour/DiscGolfSession5MocapSmokeRunner.cpp",
        "Source/DiscGolfTour/DiscGolfSession5MocapVisualCaptureRunner.cpp",
        "Source/DiscGolfTourEditor/DiscGolfSession5MocapUtility.cpp",
    )
    game_path_re = re.compile(r"/Game/[A-Za-z0-9_./-]+")
    for path in scan_paths:
        file_path = PROJECT_ROOT / path
        if not file_path.is_file():
            continue
        for raw in game_path_re.findall(file_path.read_text(encoding="utf-8")):
            normalized = _normalize_object_path(raw)
            if (
                normalized.startswith("/Game/DiscGolf/Animation/Mocap")
                and normalized not in allowed_mocap_paths
            ):
                divergent.append({"file": path, "path": raw})
    if divergent:
        errors.append("non-canonical /Game/DiscGolf/Animation/Mocap path literals were found")
    evidence["divergent_mocap_literals"] = divergent
    return errors, evidence


def _check_protected_hashes() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    files: list[dict[str, Any]] = []
    for relative_path, (expected_bytes, expected_hash) in PROTECTED_PACKAGE_HASHES.items():
        path = PROJECT_ROOT / relative_path
        record: dict[str, Any] = {
            "path": relative_path,
            "expected_bytes": expected_bytes,
            "expected_sha256": expected_hash,
        }
        if not path.is_file():
            record["status"] = "MISSING"
            errors.append(f"frozen accepted package is missing: {relative_path}")
        else:
            actual_bytes = path.stat().st_size
            actual_hash = _sha256(path)
            record.update(
                {
                    "actual_bytes": actual_bytes,
                    "actual_sha256": actual_hash,
                    "status": "PASS"
                    if (actual_bytes, actual_hash) == (expected_bytes, expected_hash)
                    else "MISMATCH",
                }
            )
            if actual_bytes != expected_bytes:
                errors.append(
                    f"frozen accepted package size changed: {relative_path} "
                    f"({actual_bytes} != {expected_bytes})"
                )
            if actual_hash != expected_hash:
                errors.append(
                    f"frozen accepted package SHA-256 changed: {relative_path} "
                    f"({actual_hash} != {expected_hash})"
                )
        files.append(record)
    return errors, {"frozen_package_count": len(files), "files": files}


def _check_plugin_and_buildkit(status: list[dict[str, str]]) -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    evidence: dict[str, Any] = {}

    try:
        project = json.loads(_read_text("DiscGolfTour.uproject"))
        plugin_entries = [
            entry
            for entry in project.get("Plugins", [])
            if entry.get("Name") == "DiscGolfCharacterFramework"
        ]
        if len(plugin_entries) != 1 or plugin_entries[0].get("Enabled") is not True:
            errors.append("DiscGolfCharacterFramework must appear exactly once and enabled in the uproject")
        evidence["uproject_plugin_entries"] = plugin_entries
    except Exception as exc:
        errors.append(f"could not inspect uproject plugin entry: {type(exc).__name__}: {exc}")

    installed_manifest = PROJECT_ROOT / PLUGIN_ROOT / "DiscGolfCharacterFramework.uplugin"
    kit_manifest = PROJECT_ROOT / BUILDKIT_PLUGIN_ROOT / "DiscGolfCharacterFramework.uplugin"
    try:
        installed_json = json.loads(installed_manifest.read_text(encoding="utf-8"))
        if installed_json.get("VersionName") != "1.5.0":
            errors.append("installed DiscGolfCharacterFramework VersionName must be 1.5.0")
        if installed_manifest.read_bytes() != kit_manifest.read_bytes():
            errors.append("installed plugin manifest differs from the pristine BuildKit manifest")
        evidence["installed_plugin_version"] = installed_json.get("VersionName")
    except Exception as exc:
        errors.append(f"could not inspect installed plugin manifest: {type(exc).__name__}: {exc}")

    plugin_dirty = [entry for entry in status if entry["path"].startswith(f"{PLUGIN_ROOT}/")]
    if plugin_dirty:
        errors.append("installed plugin has modified, staged, or untracked source/content")
    try:
        tracked_raw = _git(["ls-files", "-z", "--", PLUGIN_ROOT], binary=True)
        assert isinstance(tracked_raw, bytes)
        tracked_count = len([part for part in tracked_raw.split(b"\0") if part])
        if tracked_count != 96:
            errors.append(f"installed plugin tracked-file count is {tracked_count}, expected 96")
        evidence["installed_plugin"] = {
            "tracked_file_count": tracked_count,
            "dirty_entries": plugin_dirty,
        }
    except Exception as exc:
        errors.append(f"could not inspect installed plugin git state: {type(exc).__name__}: {exc}")

    inventory_mismatches: list[dict[str, Any]] = []
    inventory_missing: list[str] = []
    inventory_unexpected: list[str] = []
    try:
        inventory_bytes = BUILDKIT_INVENTORY.stat().st_size
        inventory_hash = _sha256(BUILDKIT_INVENTORY)
        if inventory_bytes != BUILDKIT_INVENTORY_BYTES:
            errors.append(
                f"BuildKit inventory size changed: {inventory_bytes} != {BUILDKIT_INVENTORY_BYTES}"
            )
        if inventory_hash != BUILDKIT_INVENTORY_SHA256:
            errors.append(
                f"BuildKit inventory SHA-256 changed: {inventory_hash} != {BUILDKIT_INVENTORY_SHA256}"
            )
        inventory = json.loads(BUILDKIT_INVENTORY.read_text(encoding="utf-8"))
        records = inventory.get("files", [])
        if len(records) != BUILDKIT_INVENTORY_RECORDS:
            errors.append(
                f"BuildKit inventory has {len(records)} records, expected {BUILDKIT_INVENTORY_RECORDS}"
            )
        expected_paths = [record.get("path") for record in records]
        if len(expected_paths) != len(set(expected_paths)):
            errors.append("BuildKit inventory contains duplicate paths")
        expected_set = {path for path in expected_paths if isinstance(path, str)}
        actual_set = {
            path.relative_to(BUILDKIT_ROOT).as_posix()
            for path in BUILDKIT_ROOT.rglob("*")
            if path.is_file() and path != BUILDKIT_INVENTORY
        }
        inventory_missing = sorted(expected_set - actual_set)
        inventory_unexpected = sorted(actual_set - expected_set)
        for relative_path in inventory_missing:
            errors.append(f"BuildKit inventory file is missing: {relative_path}")
        for relative_path in inventory_unexpected:
            errors.append(f"unexpected file was added to the BuildKit: {relative_path}")
        for record in records:
            relative_path = record.get("path")
            if not isinstance(relative_path, str) or relative_path not in actual_set:
                continue
            path = BUILDKIT_ROOT / relative_path
            actual_bytes = path.stat().st_size
            actual_hash = _sha256(path).lower()
            if actual_bytes != record.get("bytes") or actual_hash != record.get("sha256"):
                inventory_mismatches.append(
                    {
                        "path": relative_path,
                        "expected_bytes": record.get("bytes"),
                        "actual_bytes": actual_bytes,
                        "expected_sha256": record.get("sha256"),
                        "actual_sha256": actual_hash,
                    }
                )
        if inventory_mismatches:
            errors.append(
                f"{len(inventory_mismatches)} BuildKit files differ from FILE_INVENTORY.json"
            )
        manifest = json.loads((BUILDKIT_ROOT / "MANIFEST.json").read_text(encoding="utf-8"))
        final_report = json.loads(
            (BUILDKIT_ROOT / "FINAL_BUILD_REPORT.json").read_text(encoding="utf-8")
        )
        version = (BUILDKIT_ROOT / "VERSION.txt").read_text(encoding="utf-8").strip()
        if version != "1.5.0" or manifest.get("version") != "1.5.0":
            errors.append("BuildKit version/manifest must remain 1.5.0")
        if final_report.get("static_validation") != "PASS":
            errors.append("BuildKit FINAL_BUILD_REPORT no longer records static_validation=PASS")
        evidence["buildkit"] = {
            "inventory_bytes": inventory_bytes,
            "inventory_sha256": inventory_hash,
            "inventory_records": len(records),
            "missing": inventory_missing,
            "unexpected": inventory_unexpected,
            "mismatches": inventory_mismatches,
            "version": version,
            "manifest_version": manifest.get("version"),
            "recorded_static_validation": final_report.get("static_validation"),
        }
    except Exception as exc:
        errors.append(f"could not validate pristine BuildKit inventory: {type(exc).__name__}: {exc}")
    return errors, evidence


def _check_default_route_and_gate() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    evidence: dict[str, Any] = {}
    try:
        header = _read_text("Source/DiscGolfTour/DiscGolfSession5MocapValidationPaths.h")
        compact = re.sub(r"\s+", "", header)
        exact_gate = (
            "returnFApp::IsUnattended()"
            "&&(FParse::Param(FCommandLine::Get(),TEXT(\"Session5MocapPipelineSmokeTest\"))"
            "||FParse::Param(FCommandLine::Get(),TEXT(\"Session5MocapVisualCapture\")));"
        )
        if exact_gate not in compact:
            errors.append(
                "pipeline montage gate is not exactly unattended AND (Session5 smoke OR visual)"
            )
        if compact.count("Session5MocapPipelineSmokeTest") != 1:
            errors.append("runtime gate must contain the Session5 smoke flag exactly once")
        if compact.count("Session5MocapVisualCapture") != 1:
            errors.append("runtime gate must contain the Session5 visual flag exactly once")
        evidence["gate_expression_exact"] = exact_gate in compact
    except Exception as exc:
        errors.append(f"could not inspect runtime gate: {type(exc).__name__}: {exc}")

    try:
        pawn = _read_text("Source/DiscGolfTour/DiscGolferPawn.cpp")
        prototype_object = _object_path(CANONICAL_PACKAGES["prototype_montage"])
        prototype_finder = re.compile(
            r"FObjectFinder\s*<\s*UAnimMontage\s*>\s+RHBHMontage\s*\(\s*"
            + re.escape(f'TEXT("{prototype_object}")')
            + r"\s*\)"
        )
        if not prototype_finder.search(pawn):
            errors.append("pawn constructor no longer loads the accepted prototype montage")
        if "RHBHThrowMontage = RHBHMontage.Succeeded() ? RHBHMontage.Object : nullptr;" not in pawn:
            errors.append("pawn constructor no longer assigns the prototype as the default route")
        if pawn.count("RHBHThrowMontage =") != 2:
            errors.append(
                "RHBHThrowMontage assignment count changed; expected default plus one gated override"
            )
        gate_call = "DiscGolfSession5MocapValidation::IsPipelineRuntimeValidationRequested()"
        if pawn.count(gate_call) != 1:
            errors.append("pawn must invoke the centralized Session5 unattended gate exactly once")
        gate_index = pawn.find(f"if ({gate_call})")
        load_index = pawn.find("RHBHThrowMontage = LoadObject<UAnimMontage>")
        next_profile_block = pawn.find("if (CharacterProfileTemplate && FrameworkThrowComponent)")
        if not (0 <= gate_index < load_index < next_profile_block):
            errors.append("pipeline montage assignment is not confined to the centralized gated block")
        if CANONICAL_PACKAGES["pipeline_montage"] in pawn:
            errors.append("pawn hard-codes the pipeline montage instead of using the canonical constant")
        evidence["default_route"] = {
            "prototype_object_path": prototype_object,
            "prototype_finder_present": bool(prototype_finder.search(pawn)),
            "montage_assignment_count": pawn.count("RHBHThrowMontage ="),
            "central_gate_call_count": pawn.count(gate_call),
            "pipeline_literal_in_pawn": CANONICAL_PACKAGES["pipeline_montage"] in pawn,
        }
    except Exception as exc:
        errors.append(f"could not inspect pawn montage routing: {type(exc).__name__}: {exc}")

    try:
        game_mode = _read_text("Source/DiscGolfTour/DiscGolfTourGameMode.cpp")
        required_fragments = (
            'TEXT("Session5MocapPipelineSmokeTest")',
            'TEXT("Session5MocapVisualCapture")',
            "SpawnActor<ADiscGolfSession5MocapSmokeRunner>",
            "SpawnActor<ADiscGolfSession5MocapVisualCaptureRunner>",
            "if (bSession5MocapVisualCaptureRequested)",
            "else if (bSession5MocapSmokeRequested)",
        )
        missing = [fragment for fragment in required_fragments if fragment not in game_mode]
        if missing:
            errors.append("GameMode Session5 command-line wiring is incomplete: " + ", ".join(missing))
        evidence["game_mode_missing_fragments"] = missing
    except Exception as exc:
        errors.append(f"could not inspect GameMode Session5 wiring: {type(exc).__name__}: {exc}")

    runner_gates: dict[str, dict[str, bool]] = {}
    for path, flag in (
        (
            "Scripts/run-session5-mocap-profile-smokes.py",
            "-Session5MocapPipelineSmokeTest",
        ),
        (
            "Scripts/run-session5-mocap-visual-capture.py",
            "-Session5MocapVisualCapture",
        ),
    ):
        try:
            text = _read_text(path)
            runner_gates[path] = {
                "owns_exact_session5_flag": flag in text,
                "forces_unattended": '"-unattended"' in text,
            }
            for key, passed in runner_gates[path].items():
                if not passed:
                    errors.append(f"{path} launch gate failed: {key}")
        except Exception as exc:
            errors.append(f"could not inspect {path} launch gate: {type(exc).__name__}: {exc}")
    evidence["runner_gates"] = runner_gates
    return errors, evidence


def _scan_forbidden(text: str, path: str) -> list[dict[str, Any]]:
    matches: list[dict[str, Any]] = []
    for label, pattern in FORBIDDEN_SESSION5_SOURCE_PATTERNS.items():
        for match in pattern.finditer(text):
            matches.append(
                {
                    "file": path,
                    "token": label,
                    "line": text.count("\n", 0, match.start()) + 1,
                }
            )
    return matches


def _check_authority_boundary() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    matches: list[dict[str, Any]] = []
    session5_sources = sorted(
        path.relative_to(PROJECT_ROOT).as_posix()
        for root in (PROJECT_ROOT / "Source/DiscGolfTour", PROJECT_ROOT / "Source/DiscGolfTourEditor")
        if root.is_dir()
        for path in root.glob("DiscGolfSession5*")
        if path.suffix in {".h", ".cpp"}
    )
    for path in session5_sources:
        matches.extend(_scan_forbidden(_read_text(path), path))
    session5_python_sources = sorted(
        path.relative_to(PROJECT_ROOT).as_posix()
        for path in (PROJECT_ROOT / "Scripts").glob("*session5*.py")
        if path.is_file() and path.resolve() != Path(__file__).resolve()
    )
    for path in session5_python_sources:
        matches.extend(_scan_forbidden(_read_text(path), path))
    for path in TRACKED_SESSION5_INTEGRATION_FILES:
        try:
            matches.extend(_scan_forbidden(_added_lines(path), f"{path} (added lines)"))
        except Exception as exc:
            errors.append(f"could not inspect added lines for {path}: {type(exc).__name__}: {exc}")
    if matches:
        errors.append(
            f"{len(matches)} forbidden flight/release authority token occurrence(s) found in Session5 changes"
        )

    delegation: dict[str, bool] = {}
    try:
        smoke_h = _read_text("Source/DiscGolfTour/DiscGolfSession5MocapSmokeRunner.h")
        smoke_cpp = _read_text("Source/DiscGolfTour/DiscGolfSession5MocapSmokeRunner.cpp")
        compact_h = re.sub(r"\s+", "", smoke_h)
        delegation = {
            "inherits_session3_runner": ":publicADiscGolfSession3SmokeRunner" in compact_h,
            "delegates_start": "Super::Start();" in smoke_cpp,
            "delegates_fail": "Super::Fail(Reason);" in smoke_cpp,
            "delegates_pass": "Super::Pass();" in smoke_cpp,
        }
        for key, passed in delegation.items():
            if not passed:
                errors.append(f"Session5 smoke authority delegation failed: {key}")
    except Exception as exc:
        errors.append(f"could not inspect Session5 smoke delegation: {type(exc).__name__}: {exc}")

    visual_isolation: dict[str, bool] = {}
    try:
        visual = _read_text("Source/DiscGolfTour/DiscGolfSession5MocapVisualCaptureRunner.cpp")
        visual_isolation = {
            "binds_validation_callback": (
                "GetAuthoritativeLaunchDelegate().BindUObject" in visual
                and "HandleFixtureRelease" in visual
            ),
            "checks_no_gameplay_mutation": "ValidateNoGameplayMutation" in visual,
            "does_not_spawn_gameplay_disc": not bool(
                FORBIDDEN_SESSION5_SOURCE_PATTERNS["SpawnActor<ADiscActor>"].search(visual)
            ),
            "does_not_call_throw_authority": not bool(
                FORBIDDEN_SESSION5_SOURCE_PATTERNS["direct RequestThrowFromGrip"].search(visual)
            ),
        }
        for key, passed in visual_isolation.items():
            if not passed:
                errors.append(f"Session5 visual callback isolation failed: {key}")
    except Exception as exc:
        errors.append(f"could not inspect Session5 visual isolation: {type(exc).__name__}: {exc}")

    return errors, {
        "scanned_session5_source_files": session5_sources,
        "scanned_session5_python_files": session5_python_sources,
        "scanned_tracked_integration_files": list(TRACKED_SESSION5_INTEGRATION_FILES),
        "forbidden_tokens": list(FORBIDDEN_SESSION5_SOURCE_PATTERNS),
        "matches": matches,
        "smoke_delegation": delegation,
        "visual_isolation": visual_isolation,
        "allowed_evidence": (
            "Inherited Session3 smoke behavior and read-only FThrowCommand/disc/stroke "
            "evidence are intentionally permitted."
        ),
    }


def _check_registry_contract() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    evidence: dict[str, Any] = {}
    registry_path = PROJECT_ROOT / "SourceArt/DiscGolf/Mocap/motion_source_registry.json"
    validator_path = PROJECT_ROOT / "Scripts/validate_motion_source_registry.py"
    try:
        registry = json.loads(registry_path.read_text(encoding="utf-8"))
        module = runpy.run_path(str(validator_path), run_name="session5_registry_contract")
        required_fields = set(module.get("REQUIRED_FIELDS", set()))
        missing_mandated = sorted(MANDATED_REGISTRY_FIELDS - required_fields)
        if missing_mandated:
            errors.append(
                "registry validator no longer requires mandated fields: "
                + ", ".join(missing_mandated)
            )
        if module.get("PIPELINE_FIXTURE_PATHS") != REGISTRY_FIXTURE_PATHS:
            errors.append("registry validator pipeline fixture stage constants differ")
        if module.get("PIPELINE_FIXTURE_MONTAGE") != CANONICAL_PACKAGES["pipeline_montage"]:
            errors.append("registry validator pipeline fixture montage constant differs")

        validator = module.get("_validate_registry")
        if not callable(validator):
            errors.append("registry validator does not expose its read-only validation contract")
            base_errors: list[str] = ["validator unavailable"]
            verified_files: list[dict[str, Any]] = []
        else:
            base_errors, verified_files = validator(registry)
            errors.extend(f"registry validator: {message}" for message in base_errors)
        expected_file_evidence = sum(
            len(source.get("files", []))
            for source in registry.get("sources", [])
            if isinstance(source, dict)
        )
        if len(verified_files) != expected_file_evidence:
            errors.append(
                f"registry verified {len(verified_files)} files, expected {expected_file_evidence}"
            )

        sources = registry.get("sources", [])
        if len(sources) != 9:
            errors.append(f"Session5 registry source count is {len(sources)}, expected 9")
        fixtures = [
            source for source in sources if source.get("pipeline_role") == "PIPELINE_FIXTURE"
        ]
        if len(fixtures) != 1:
            errors.append(f"registry has {len(fixtures)} PIPELINE_FIXTURE rows, expected exactly one")
        else:
            fixture = fixtures[0]
            for field, expected in REGISTRY_FIXTURE_PATHS.items():
                if fixture.get(field) != expected:
                    errors.append(
                        f"registry pipeline fixture {field} is {fixture.get(field)!r}, expected {expected!r}"
                    )
            if CANONICAL_PACKAGES["pipeline_montage"] not in fixture.get("content_assets", []):
                errors.append("registry pipeline fixture omits the exact pipeline montage")
            if fixture.get("source_kind") != "SYNTHETIC_TEST":
                errors.append("registry pipeline fixture must remain SYNTHETIC_TEST")
            if fixture.get("usage_status") != "DO_NOT_SHIP":
                errors.append("registry pipeline fixture must remain DO_NOT_SHIP")
            if fixture.get("external_production_claim") is not False:
                errors.append("registry pipeline fixture makes an external production claim")
        external_claims = [
            source.get("source_id")
            for source in sources
            if source.get("external_production_claim") is not False
        ]
        if external_claims:
            errors.append("registry contains external production claims: " + ", ".join(external_claims))
        if registry.get("policy", {}).get("external_production_sources_present") is not False:
            errors.append("registry policy must declare no external production sources")

        negative_guards: dict[str, bool] = {}
        if callable(validator) and not base_errors and fixtures:
            mutations: dict[str, Any] = {}
            missing_field = copy.deepcopy(registry)
            del missing_field["sources"][1]["motion_id"]
            mutations["missing_motion_id"] = missing_field
            duplicate_id = copy.deepcopy(registry)
            duplicate_id["sources"][1]["motion_id"] = duplicate_id["sources"][0]["motion_id"]
            mutations["duplicate_motion_id"] = duplicate_id
            external = copy.deepcopy(registry)
            external["sources"][1]["external_production_claim"] = True
            mutations["external_production_claim"] = external
            missing_stage = copy.deepcopy(registry)
            missing_stage["sources"][1]["raw_asset_path"] = None
            mutations["missing_pipeline_stage"] = missing_stage
            vendor_repurpose = copy.deepcopy(registry)
            vendor_repurpose["sources"][2]["usage_status"] = "CLEARED_FOR_PRODUCTION"
            mutations["vendor_repurpose"] = vendor_repurpose
            for label, mutated in mutations.items():
                mutation_errors, _ = validator(mutated)
                negative_guards[label] = bool(mutation_errors)
                if not mutation_errors:
                    errors.append(f"registry validator accepted forbidden mutation: {label}")
        else:
            errors.append("registry negative contract guards could not run against a clean baseline")
        evidence = {
            "source_count": len(sources),
            "pipeline_fixture_count": len(fixtures),
            "required_field_count": len(required_fields),
            "missing_mandated_fields": missing_mandated,
            "base_validation_errors": base_errors,
            "verified_file_count": len(verified_files),
            "expected_file_evidence": expected_file_evidence,
            "external_production_claims": external_claims,
            "negative_guards": negative_guards,
        }
    except Exception as exc:
        errors.append(f"could not execute registry validator contract: {type(exc).__name__}: {exc}")
    return errors, evidence


def _check_untracked_baseline(status: list[dict[str, str]]) -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    untracked = sorted(entry["path"] for entry in status if entry["status"] == "??")
    intended = sorted(path for path in untracked if path in INTENDED_SESSION5_UNTRACKED)
    unrelated = sorted(path for path in untracked if path not in INTENDED_SESSION5_UNTRACKED)
    outside_known_roots = sorted(
        path for path in unrelated if not _is_known_unrelated_untracked(path)
    )
    if len(unrelated) != EXPECTED_UNRELATED_UNTRACKED_COUNT:
        errors.append(
            f"unrelated untracked path count is {len(unrelated)}, "
            f"expected exactly {EXPECTED_UNRELATED_UNTRACKED_COUNT}"
        )
    if outside_known_roots:
        errors.append(
            f"{len(outside_known_roots)} unrelated untracked path(s) fall outside known roots"
        )
    by_root = Counter(path.split("/", 1)[0] for path in unrelated)
    return errors, {
        "total_untracked_count": len(untracked),
        "intended_session5_untracked_count": len(intended),
        "intended_session5_untracked": intended,
        "unrelated_untracked_count": len(unrelated),
        "expected_unrelated_untracked_count": EXPECTED_UNRELATED_UNTRACKED_COUNT,
        "unrelated_by_root": dict(sorted(by_root.items())),
        "outside_known_roots": outside_known_roots,
        "known_exact": sorted(KNOWN_UNRELATED_UNTRACKED_EXACT),
        "known_prefixes": list(KNOWN_UNRELATED_UNTRACKED_PREFIXES),
    }


def _check_generated_git_state() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    try:
        tracked_raw = _git(["ls-files", "-z"], binary=True)
        staged_raw = _git(["diff", "--cached", "--name-only", "-z"], binary=True)
        assert isinstance(tracked_raw, bytes) and isinstance(staged_raw, bytes)
        tracked = [
            path.decode("utf-8", errors="surrogateescape")
            for path in tracked_raw.split(b"\0")
            if path
        ]
        staged = [
            path.decode("utf-8", errors="surrogateescape")
            for path in staged_raw.split(b"\0")
            if path
        ]
        tracked_generated = sorted(path for path in tracked if _is_generated_path(path))
        staged_generated = sorted(path for path in staged if _is_generated_path(path))
        if tracked_generated:
            errors.append(f"{len(tracked_generated)} generated-root path(s) are tracked")
        if staged_generated:
            errors.append(f"{len(staged_generated)} generated-root path(s) are staged")
        evidence = {
            "tracked_generated": tracked_generated,
            "staged_generated": staged_generated,
            "generated_parts": sorted(GENERATED_PARTS),
            "generated_prefixes": list(GENERATED_PREFIXES),
        }
    except Exception as exc:
        errors.append(f"could not inspect generated-root git state: {type(exc).__name__}: {exc}")
        evidence = {}
    return errors, evidence


def _check_no_session6() -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    pattern = re.compile(r"(?i)(?:DiscGolfSession|Session[\s_-]*)6(?!\d)")
    self_path = Path(__file__).resolve()
    text_suffixes = {".h", ".cpp", ".cs", ".py", ".ps1", ".bat", ".ini", ".json", ".uplugin"}
    matches: list[dict[str, Any]] = []
    for root_name in ("Source", "Plugins", "Scripts", "Config"):
        root = PROJECT_ROOT / root_name
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or path.resolve() == self_path:
                continue
            relative = path.relative_to(PROJECT_ROOT).as_posix()
            if _is_generated_path(relative):
                continue
            if pattern.search(relative):
                matches.append({"file": relative, "kind": "path"})
            if path.suffix.lower() not in text_suffixes:
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (OSError, UnicodeError):
                continue
            match = pattern.search(text)
            if match:
                matches.append(
                    {
                        "file": relative,
                        "kind": "text",
                        "line": text.count("\n", 0, match.start()) + 1,
                    }
                )
    content_root = PROJECT_ROOT / "Content"
    if content_root.is_dir():
        for path in content_root.rglob("*"):
            if path.is_file():
                relative = path.relative_to(PROJECT_ROOT).as_posix()
                if pattern.search(relative):
                    matches.append({"file": relative, "kind": "content_path"})
    if matches:
        errors.append(f"{len(matches)} Session 6 implementation marker(s) found")
    return errors, {"matches": matches, "validator_self_excluded": Path(__file__).name}


def _record_check(
    checks: list[dict[str, Any]],
    check_id: str,
    errors: list[str],
    evidence: dict[str, Any],
) -> None:
    checks.append(
        {
            "id": check_id,
            "status": "PASS" if not errors else "FAIL",
            "errors": errors,
            "evidence": evidence,
        }
    )


def main() -> int:
    checks: list[dict[str, Any]] = []
    setup_errors: list[str] = []
    try:
        status = _git_status()
    except Exception as exc:
        status = []
        setup_errors.append(f"git status unavailable: {type(exc).__name__}: {exc}")
    _record_check(checks, "repository_state_available", setup_errors, {"status_entries": len(status)})

    for check_id, function in (
        ("required_source_tooling_docs_registry_paths", _check_required_paths),
        ("required_authored_fixture_packages", _check_fixture_packages),
        ("exact_canonical_asset_constants", _check_canonical_paths),
        ("frozen_accepted_uasset_sha256", _check_protected_hashes),
        ("installed_plugin_and_pristine_buildkit", lambda: _check_plugin_and_buildkit(status)),
        ("default_prototype_route_and_unattended_override_gate", _check_default_route_and_gate),
        ("no_new_flight_or_release_authority", _check_authority_boundary),
        ("motion_source_registry_validator_contract", _check_registry_contract),
        ("unrelated_untracked_baseline", lambda: _check_untracked_baseline(status)),
        ("no_generated_roots_tracked_or_staged", _check_generated_git_state),
        ("no_session6_implementation", _check_no_session6),
    ):
        try:
            errors, evidence = function()
        except Exception as exc:
            errors = [f"unhandled check failure: {type(exc).__name__}: {exc}"]
            evidence = {}
        _record_check(checks, check_id, errors, evidence)

    all_errors = [
        f"{check['id']}: {error}"
        for check in checks
        for error in check["errors"]
    ]
    report = {
        "schema": "dg_character_session5_static_wiring_validation",
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "project_root": str(PROJECT_ROOT),
        "status": "PASS" if not all_errors else "FAIL",
        "summary": {
            "check_count": len(checks),
            "passed_check_count": sum(check["status"] == "PASS" for check in checks),
            "failed_check_count": sum(check["status"] == "FAIL" for check in checks),
            "error_count": len(all_errors),
        },
        "checks": checks,
        "errors": all_errors,
        "report_path": str(REPORT_PATH),
        "ue_launched": False,
        "ubt_launched": False,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"{report['status']}: {report['summary']['passed_check_count']}/"
        f"{report['summary']['check_count']} checks; errors={len(all_errors)}; "
        f"report={REPORT_PATH}"
    )
    if all_errors:
        for error in all_errors:
            print(f"  - {error}")
    return 0 if not all_errors else 1


if __name__ == "__main__":
    sys.exit(main())
