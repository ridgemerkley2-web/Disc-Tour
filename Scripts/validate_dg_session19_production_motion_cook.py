#!/usr/bin/env python3
"""Prove current production-motion presence and authoring-content exclusion.

This standalone Session 19 validator inventories the final IoStore container,
extracts the cooked AssetRegistry.bin with UnrealPak, and dumps that registry
with the matching Unreal editor commandlet.  Its source-only preflight also
proves that current v006 runtime motion is on an AlwaysCook root while mocap,
synthetic fixtures, and the reverse MetaHuman-to-DGMaster authoring retargeter
are on NeverCook roots and have no runtime-authority reference.  Passing is
technical boundary/cook evidence only; it is not animation, contact,
performance, owner, or release approval.
"""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "DiscGolfTour.uproject"
TARGET_ROOT = "/Game/DiscGolf/Animation/ProductionMotion"
CURRENT_MOTION_VERSION = "v6"
DEFAULT_GAME = ROOT / "Config/DefaultGame.ini"
RUNTIME_PATH_HEADER = ROOT / "Source/DiscGolfTour/DiscGolfProductionMotion.h"
RUNTIME_COOK_SPEC = ROOT / "Config/DG_RuntimeCookManifest.json"
RUNTIME_AUTHORITY_SOURCE_ROOTS = (
    ROOT / "Source/DiscGolfTour",
    ROOT / "Source/DiscGolfRuntimeFoundation",
)
REVERSE_AUTHORING_RETARGETER_PACKAGE = (
    "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster"
)
REVERSE_AUTHORING_RETARGETER_OBJECT = (
    REVERSE_AUTHORING_RETARGETER_PACKAGE + ".RTG_MetaHuman_To_DGMaster"
)
REVERSE_AUTHORING_RETARGETER_FILE = (
    ROOT
    / "Content/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster.uasset"
)
V1_EXPECTED_ASSETS = (
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v001.A_DG_RHBH_Drive_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v001.AM_DG_RHBH_Drive_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v001.A_DG_RHBH_Approach_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v001.AM_DG_RHBH_Approach_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v001.A_DG_RHBH_Putt_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v001.AM_DG_RHBH_Putt_Procedural_v001",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v001.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/"
        "DA_DG_ProductionMotionLibrary.DA_DG_ProductionMotionLibrary",
        "DiscGolf/Animation/ProductionMotion/DA_DG_ProductionMotionLibrary.uasset",
    ),
)
V2_EXPECTED_ASSETS = (
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v002.A_DG_RHBH_Drive_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v002.AM_DG_RHBH_Drive_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v002.A_DG_RHBH_Approach_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v002.AM_DG_RHBH_Approach_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v002.A_DG_RHBH_Putt_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v002.AM_DG_RHBH_Putt_Procedural_v002",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v002.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/"
        "DA_DG_ProductionMotionLibrary_v002.DA_DG_ProductionMotionLibrary_v002",
        "DiscGolf/Animation/ProductionMotion/"
        "DA_DG_ProductionMotionLibrary_v002.uasset",
    ),
)
V3_EXPECTED_ASSETS = (
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v003.A_DG_RHBH_Drive_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "A_DG_RHBH_Drive_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v003.AM_DG_RHBH_Drive_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Drive/"
        "AM_DG_RHBH_Drive_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v003.A_DG_RHBH_Approach_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "A_DG_RHBH_Approach_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v003.AM_DG_RHBH_Approach_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Approach/"
        "AM_DG_RHBH_Approach_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v003.A_DG_RHBH_Putt_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "A_DG_RHBH_Putt_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v003.AM_DG_RHBH_Putt_Procedural_v003",
        "DiscGolf/Animation/ProductionMotion/Putt/"
        "AM_DG_RHBH_Putt_Procedural_v003.uasset",
    ),
    (
        "/Game/DiscGolf/Animation/ProductionMotion/"
        "DA_DG_ProductionMotionLibrary_v003.DA_DG_ProductionMotionLibrary_v003",
        "DiscGolf/Animation/ProductionMotion/"
        "DA_DG_ProductionMotionLibrary_v003.uasset",
    ),
)
V4_EXPECTED_ASSETS = tuple(
    (
        object_path.replace("v003", "v004"),
        relative_path.replace("v003", "v004"),
    )
    for object_path, relative_path in V3_EXPECTED_ASSETS
)
V5_EXPECTED_ASSETS = tuple(
    (
        object_path.replace("v004", "v005"),
        relative_path.replace("v004", "v005"),
    )
    for object_path, relative_path in V4_EXPECTED_ASSETS
)
V6_EXPECTED_ASSETS = tuple(
    (
        object_path.replace("v005", "v006"),
        relative_path.replace("v005", "v006"),
    )
    for object_path, relative_path in V5_EXPECTED_ASSETS
)
MOTION_CONTRACTS = {
    "v1": V1_EXPECTED_ASSETS,
    "v2": V2_EXPECTED_ASSETS,
    "v3": V3_EXPECTED_ASSETS,
    "v4": V4_EXPECTED_ASSETS,
    "v5": V5_EXPECTED_ASSETS,
    "v6": V6_EXPECTED_ASSETS,
}
# Keep the historical name as the immutable v1 contract for callers which import
# this standalone validator. Receipt parsing retains v1-v5 compatibility, but a
# new live Shipping claim must use CURRENT_MOTION_VERSION.
EXPECTED_ASSETS = V1_EXPECTED_ASSETS
EDITOR_ONLY_ROOTS = (
    "/Game/DiscGolf/Animation/Mocap/",
    "/Game/DiscGolf/Animation/Throws/",
)
# Historical import name retained for callers and immutable receipt semantics.
PROTECTED_ROOTS = EDITOR_ONLY_ROOTS
PROTECTED_ASSETS = (
    "/Game/DiscGolf/Animation/Throws/"
    "A_DG_RHBH_Prototype.A_DG_RHBH_Prototype",
    "/Game/DiscGolf/Animation/Throws/"
    "AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype",
    "/Game/DiscGolf/Animation/Mocap/Production/"
    "A_DG_RHBH_SyntheticPipelineTest_v001.A_DG_RHBH_SyntheticPipelineTest_v001",
    "/Game/DiscGolf/Animation/Mocap/Production/"
    "AM_DG_RHBH_SyntheticPipelineTest_v001.AM_DG_RHBH_SyntheticPipelineTest_v001",
    "/Game/DiscGolf/Animation/Mocap/Production/"
    "DA_DG_AnimationLibrary.DA_DG_AnimationLibrary",
)
EDITOR_ONLY_ASSETS = (REVERSE_AUTHORING_RETARGETER_OBJECT,)
CLAIMS = {
    "technicalCookPresenceAccepted": True,
    "protectedSyntheticCookExclusionAccepted": True,
    "humanAnimationApproval": False,
    "humanDiscContactApproval": False,
    "metaHumanProfileMatrixApproval": False,
    "performanceAcceptance": False,
    "soakAcceptance": False,
    "ownerApproval": False,
    "releaseApproval": False,
    "releaseReady": False,
}
CANDIDATE_RE = re.compile(r"S19_WindowsShipping_[A-Za-z0-9_-]+\Z")


class ValidationError(ValueError):
    pass


def sha256_file(path: Path) -> str:
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(block)
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ValidationError(f"file changed while hashing: {path}")
    return digest.hexdigest().upper()


def identity(path: Path, *, include_path: str | None = None) -> dict[str, Any]:
    if not path.is_file() or path.is_symlink():
        raise ValidationError(f"required regular file is missing: {path}")
    value: dict[str, Any] = {
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }
    if include_path is not None:
        value = {"path": include_path, **value}
    return value


def require_current_motion_version(motion_version: str) -> None:
    """Prevent a new Shipping receipt from proving only a historical candidate."""
    if motion_version != CURRENT_MOTION_VERSION:
        raise ValidationError(
            "new Shipping production-motion evidence must target current "
            f"{CURRENT_MOTION_VERSION}, not historical {motion_version}"
        )


def _cook_directory_counts(config_text: str) -> dict[str, dict[str, int]]:
    counts = {"always": {}, "never": {}}
    pattern = re.compile(
        r'^\+(DirectoriesToAlwaysCook|DirectoriesToNeverCook)='
        r'\(Path="([^"]+)"\)\s*$',
        re.MULTILINE,
    )
    for directive, package_root in pattern.findall(config_text):
        group = "always" if directive == "DirectoriesToAlwaysCook" else "never"
        counts[group][package_root] = counts[group].get(package_root, 0) + 1
    return counts


def shipping_source_boundary_facts(
    default_game_text: str,
    runtime_header_text: str,
    runtime_cook_spec_text: str,
    runtime_authority_source_text: str = "",
) -> dict[str, Any]:
    """Validate the static runtime/editor-authoring partition without Unreal."""
    counts = _cook_directory_counts(default_game_text)
    required = (
        ("always", TARGET_ROOT),
        ("never", "/Game/DiscGolf/Animation/Mocap"),
        ("never", "/Game/DiscGolf/Animation/Throws"),
    )
    for group, package_root in required:
        if counts[group].get(package_root) != 1:
            raise ValidationError(
                f"packaging must declare exactly one {group}-cook root: {package_root}"
            )
    conflicting = (
        ("never", TARGET_ROOT),
        ("always", "/Game/DiscGolf/Animation/Mocap"),
        ("always", "/Game/DiscGolf/Animation/Throws"),
    )
    for group, package_root in conflicting:
        if counts[group].get(package_root, 0) != 0:
            raise ValidationError(
                f"packaging contains conflicting {group}-cook root: {package_root}"
            )

    active_aliases = (
        "inline constexpr const TCHAR* ActiveVersion = V6::Version;",
        "inline constexpr const TCHAR* ActiveAssetRevision = V6::AssetRevision;",
        "inline constexpr const TCHAR* DriveSequence = V6::DriveSequence;",
        "inline constexpr const TCHAR* DriveMontage = V6::DriveMontage;",
        "inline constexpr const TCHAR* ApproachSequence = V6::ApproachSequence;",
        "inline constexpr const TCHAR* ApproachMontage = V6::ApproachMontage;",
        "inline constexpr const TCHAR* PuttSequence = V6::PuttSequence;",
        "inline constexpr const TCHAR* PuttMontage = V6::PuttMontage;",
        "inline constexpr const TCHAR* Library = V6::Library;",
    )
    for token in active_aliases:
        if runtime_header_text.count(token) != 1:
            raise ValidationError(f"current-v006 runtime alias differs: {token}")
    for object_path, _relative in V6_EXPECTED_ASSETS:
        if runtime_header_text.count(object_path) != 1:
            raise ValidationError(
                f"runtime header must name exactly one current-v006 asset: {object_path}"
            )

    for authority_name, authority_text in (
        ("packaging configuration", default_game_text),
        ("runtime path header", runtime_header_text),
        ("runtime cook manifest", runtime_cook_spec_text),
        ("runtime C++ module source", runtime_authority_source_text),
    ):
        if REVERSE_AUTHORING_RETARGETER_PACKAGE in authority_text:
            raise ValidationError(
                "reverse MetaHuman-to-DGMaster retargeter became runtime authority "
                f"through {authority_name}"
            )

    try:
        cook_spec = json.loads(runtime_cook_spec_text)
    except (json.JSONDecodeError, UnicodeError) as exc:
        raise ValidationError(f"runtime cook manifest JSON is invalid: {exc}") from exc
    if not isinstance(cook_spec, dict):
        raise ValidationError("runtime cook manifest root must be an object")

    return {
        "schema": "DiscGolfTour.Session19MotionShippingBoundaryPreflight.v1",
        "status": "PASS_CURRENT_V006_RUNTIME_REVERSE_RETARGETER_EDITOR_ONLY",
        "diskMutation": "NONE",
        "runtime": {
            "motionVersion": CURRENT_MOTION_VERSION,
            "assetRevision": "v006",
            "targetRoot": TARGET_ROOT,
            "sourceAssetCount": len(V6_EXPECTED_ASSETS),
            "alwaysCookConfigured": True,
            "runtimeAliasesBoundToCurrentVersion": True,
        },
        "editorAuthoring": {
            "reverseRetargeterObject": REVERSE_AUTHORING_RETARGETER_OBJECT,
            "reverseRetargeterRoot": "/Game/DiscGolf/Animation/Mocap",
            "neverCookConfigured": True,
            "runtimeAuthorityReferenceCount": 0,
            "runtimeAuthorityScope": (
                "PACKAGING_CONFIG_RUNTIME_COOK_MANIFEST_AND_NON_TEST_RUNTIME_CPP"
            ),
            "shippingInclusionAllowed": False,
        },
        "claimBoundary": {
            "staticShippingBoundaryAccepted": True,
            "shippingCookPresenceAccepted": False,
            "animationQualityAccepted": False,
            "discContactQualityAccepted": False,
            "releaseReady": False,
        },
    }


def shipping_source_boundary_preflight() -> dict[str, Any]:
    for path in (DEFAULT_GAME, RUNTIME_PATH_HEADER, RUNTIME_COOK_SPEC):
        if not path.is_file() or path.is_symlink():
            raise ValidationError(f"required regular boundary source is missing: {path}")
    authority_paths = sorted(
        path
        for source_root in RUNTIME_AUTHORITY_SOURCE_ROOTS
        for path in source_root.rglob("*")
        if path.is_file()
        and not path.is_symlink()
        and path.suffix.casefold() in {".h", ".cpp", ".cs"}
        and "Tests" not in path.parts
    )
    if not authority_paths:
        raise ValidationError("runtime authority source inventory is empty")
    authority_source_text = "\n".join(
        path.read_text(encoding="utf-8") for path in authority_paths
    )
    facts = shipping_source_boundary_facts(
        DEFAULT_GAME.read_text(encoding="utf-8"),
        RUNTIME_PATH_HEADER.read_text(encoding="utf-8"),
        RUNTIME_COOK_SPEC.read_text(encoding="utf-8"),
        authority_source_text,
    )
    facts["editorAuthoring"]["runtimeAuthorityFileCount"] = len(authority_paths)

    source_assets = []
    for object_path, relative in V6_EXPECTED_ASSETS:
        path = ROOT / "Content" / PurePosixPath(relative)
        if not path.is_file() or path.is_symlink():
            raise ValidationError(f"current-v006 source asset is missing: {path}")
        source_assets.append({
            "objectPath": object_path,
            **identity(path, include_path=f"Content/{relative}"),
        })
    if (
        not REVERSE_AUTHORING_RETARGETER_FILE.is_file()
        or REVERSE_AUTHORING_RETARGETER_FILE.is_symlink()
    ):
        raise ValidationError(
            "reverse MetaHuman-to-DGMaster editor-authoring retargeter is missing"
        )
    facts["runtime"]["sourceAssets"] = source_assets
    facts["editorAuthoring"]["reverseRetargeterFile"] = identity(
        REVERSE_AUTHORING_RETARGETER_FILE,
        include_path=(
            "Content/DiscGolf/Animation/Mocap/Rigs/"
            "RTG_MetaHuman_To_DGMaster.uasset"
        ),
    )
    return facts


def run_tool(arguments: list[str], context: str, timeout: int = 240) -> str:
    try:
        completed = subprocess.run(
            arguments, capture_output=True, check=False, timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValidationError(f"{context} could not complete: {exc}") from exc
    output = (completed.stdout + b"\n" + completed.stderr).decode(
        "utf-8", errors="replace"
    )
    if completed.returncode != 0:
        raise ValidationError(
            f"{context} failed ({completed.returncode}): {output[-3000:]}"
        )
    return output


def normalize_container_name(value: str) -> str:
    return value.strip().replace("\\", "/")


def cooked_suffix(relative: str) -> str:
    return f"/DiscGolfTour/Content/{relative}"


def io_store_facts(
    names: list[str], expected_assets: tuple[tuple[str, str], ...] = EXPECTED_ASSETS,
) -> dict[str, Any]:
    normalized = [normalize_container_name(value) for value in names if value.strip()]
    folded = [value.casefold() for value in normalized]
    io_matches: list[dict[str, Any]] = []
    missing_io: list[str] = []
    for object_path, relative in expected_assets:
        suffix = cooked_suffix(relative).casefold()
        matching = [name for name, lower in zip(normalized, folded) if lower.endswith(suffix)]
        if len(matching) != 1:
            missing_io.append(object_path)
        io_matches.append({
            "objectPath": object_path,
            "cookedFileSuffix": cooked_suffix(relative),
            "matchCount": len(matching),
            "containerEntry": matching[0] if len(matching) == 1 else None,
        })

    editor_only_io = [
        name for name, lower in zip(normalized, folded)
        if any(
            f"/DiscGolfTour/Content/{root.removeprefix('/Game/')}".casefold()
            in lower
            for root in EDITOR_ONLY_ROOTS
        )
    ]
    if missing_io:
        raise ValidationError(
            "IoStore does not contain exactly one cooked file for: "
            + ", ".join(missing_io)
        )
    if editor_only_io:
        raise ValidationError(
            "IoStore contains editor-only motion/retarget authoring content: "
            + ", ".join(editor_only_io[:10])
        )
    return {
        "ioStoreTargetCount": sum(row["matchCount"] for row in io_matches),
        "protectedSyntheticIoStoreIdentityCount": 0,
        "ioStoreTargets": io_matches,
    }


def asset_registry_facts(
    registry_text: str,
    expected_assets: tuple[tuple[str, str], ...] = EXPECTED_ASSETS,
) -> dict[str, Any]:
    registry_matches = [
        {"objectPath": object_path, "present": object_path in registry_text}
        for object_path, _relative in expected_assets
    ]
    missing_registry = [
        item["objectPath"] for item in registry_matches if not item["present"]
    ]
    editor_only_registry = [
        value
        for value in (*EDITOR_ONLY_ROOTS, *EDITOR_ONLY_ASSETS, *PROTECTED_ASSETS)
        if value in registry_text
    ]
    if missing_registry:
        raise ValidationError(
            "cooked AssetRegistry is missing object(s): "
            + ", ".join(missing_registry)
        )
    if editor_only_registry:
        raise ValidationError(
            "cooked AssetRegistry contains editor-only motion/retarget "
            "authoring content: " + ", ".join(editor_only_registry)
        )
    return {
        "assetRegistryTargetCount": sum(row["present"] for row in registry_matches),
        "protectedSyntheticAssetRegistryIdentityCount": 0,
        "assetRegistryTargets": registry_matches,
    }


def inventory_facts(
    names: list[str], registry_text: str,
    expected_assets: tuple[tuple[str, str], ...] = EXPECTED_ASSETS,
) -> dict[str, Any]:
    return {
        "targetObjectCount": len(expected_assets),
        **io_store_facts(names, expected_assets),
        **asset_registry_facts(registry_text, expected_assets),
    }


def decode_page(path: Path) -> str:
    raw = path.read_bytes()
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        return raw.decode("utf-16")
    return raw.decode("utf-8-sig")


def snapshot_project_guard(
    expected_assets: tuple[tuple[str, str], ...] = EXPECTED_ASSETS,
) -> dict[str, dict[str, Any]]:
    paths = [
        ROOT / "Content" / PurePosixPath(relative)
        for _object_path, relative in expected_assets
    ]
    paths.append(ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav")
    result: dict[str, dict[str, Any]] = {}
    for path in paths:
        relative = path.relative_to(ROOT).as_posix()
        result[relative] = identity(path)
    return result


def audit_archive(
    candidate_id: str, archive: Path, unrealpak: Path, editor: Path,
    expected_assets: tuple[tuple[str, str], ...] = EXPECTED_ASSETS,
) -> dict[str, Any]:
    archive = archive.resolve(strict=True)
    if archive.is_symlink() or not archive.is_dir() or archive.name != "Windows":
        raise ValidationError("--archive must be a regular final Windows archive root")
    if archive.parent.name != candidate_id:
        raise ValidationError("--archive parent does not match --candidate-id")
    unrealpak = unrealpak.resolve(strict=True)
    editor = editor.resolve(strict=True)
    if not unrealpak.is_file() or unrealpak.name.casefold() != "unrealpak.exe":
        raise ValidationError("--unrealpak must name UnrealPak.exe")
    if not editor.is_file() or editor.name.casefold() != "unrealeditor-cmd.exe":
        raise ValidationError("--unreal-editor-cmd must name UnrealEditor-Cmd.exe")

    paks = archive / "DiscGolfTour/Content/Paks"
    main_pak = paks / "DiscGolfTour-Windows.pak"
    main_utoc = paks / "DiscGolfTour-Windows.utoc"
    main_ucas = paks / "DiscGolfTour-Windows.ucas"
    for path in (main_pak, main_utoc, main_ucas):
        identity(path)
    utocs = sorted(paks.glob("*.utoc"), key=lambda path: path.name.casefold())
    if not utocs or main_utoc not in utocs:
        raise ValidationError("candidate has no main IoStore container")

    before = snapshot_project_guard(expected_assets)
    reverse_authoring_before = identity(REVERSE_AUTHORING_RETARGETER_FILE)
    scratch_parent = Path(r"C:\DGTour_TestRuns")
    scratch_parent.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix="DGTourS19MotionCook_", dir=scratch_parent))
    try:
        names: list[str] = []
        chunk_count = 0
        container_rows: list[dict[str, Any]] = []
        for index, utoc in enumerate(utocs):
            csv_path = scratch / f"container_{index:03d}.csv"
            run_tool(
                [str(unrealpak), f"-ListContainer={utoc}", f"-CSV={csv_path}"],
                f"IoStore inventory {utoc.name}",
            )
            if not csv_path.is_file() or csv_path.is_symlink():
                raise ValidationError(f"UnrealPak emitted no CSV for {utoc.name}")
            with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
                reader = csv.DictReader(handle, skipinitialspace=True)
                if reader.fieldnames is None or "Filename" not in reader.fieldnames:
                    raise ValidationError("IoStore CSV lacks the Filename authority column")
                rows = list(reader)
            if not rows:
                raise ValidationError(f"IoStore CSV is empty for {utoc.name}")
            rows_names = [row.get("Filename") or "" for row in rows]
            names.extend(rows_names)
            chunk_count += len(rows)
            container_rows.append({
                "path": utoc.relative_to(archive).as_posix(),
                **identity(utoc),
                "chunkCount": len(rows),
            })

        # Fail before editor startup when the IoStore half is already incomplete.
        # This also makes a pre-integration candidate produce the useful blocker.
        io_preflight = io_store_facts(names, expected_assets)

        registry_extract = scratch / "Registry"
        run_tool(
            [
                str(unrealpak), str(main_pak), "-Extract", str(registry_extract),
                "-Filter=*AssetRegistry.bin",
            ],
            "AssetRegistry extraction",
        )
        registries = sorted(registry_extract.rglob("AssetRegistry.bin"))
        if len(registries) != 1:
            raise ValidationError(
                f"expected one extracted AssetRegistry.bin, got {len(registries)}"
            )
        registry = registries[0]
        dump = scratch / "Dump"
        user_dir = scratch / "UserDir"
        editor_output = run_tool(
            [
                str(editor), str(PROJECT), "-run=DumpAssetRegistry",
                f"-Path={registry}", f"-OutDir={dump}", "-All", "-unattended",
                "-nop4", "-nosplash", "-nullrhi", f"-UserDir={user_dir}",
            ],
            "DumpAssetRegistry",
        )
        if not re.search(
            r"Success\s*-\s*0 error\(s\),\s*0 warning\(s\)", editor_output,
        ):
            raise ValidationError("DumpAssetRegistry did not report a clean commandlet exit")
        pages = sorted(dump.glob("Page_*.txt"))
        if not pages:
            raise ValidationError("DumpAssetRegistry emitted no Page_*.txt authority")
        try:
            registry_text = "\n".join(decode_page(page) for page in pages)
        except (OSError, UnicodeError) as exc:
            raise ValidationError(f"AssetRegistry page decode failed: {exc}") from exc
        facts = {
            "targetObjectCount": len(expected_assets),
            **io_preflight,
            **asset_registry_facts(registry_text, expected_assets),
        }
        after = snapshot_project_guard(expected_assets)
        reverse_authoring_after = identity(REVERSE_AUTHORING_RETARGETER_FILE)
        if before != after or reverse_authoring_before != reverse_authoring_after:
            raise ValidationError(
                "project motion assets, reverse authoring retargeter, or "
                "protected save changed during audit"
            )
        return {
            "archiveHostPathRecorded": False,
            "containers": {
                "pak": {
                    "path": main_pak.relative_to(archive).as_posix(),
                    **identity(main_pak),
                },
                "utoc": {
                    "path": main_utoc.relative_to(archive).as_posix(),
                    **identity(main_utoc),
                },
                "ucas": {
                    "path": main_ucas.relative_to(archive).as_posix(),
                    **identity(main_ucas),
                },
            },
            "ioStoreContainerCount": len(utocs),
            "ioStoreChunkCount": chunk_count,
            "ioStoreContainers": container_rows,
            "assetRegistry": {
                **identity(registry),
                "dumpPageCount": len(pages),
                "commandletErrorCount": 0,
                "commandletWarningCount": 0,
            },
            "projectMutationGuard": {
                "guardedFileCount": len(before),
                "stable": True,
                "protectedSaveSha256": before[
                    "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
                ]["sha256"],
            },
            **facts,
        }
    finally:
        resolved = scratch.resolve()
        parent = scratch_parent.resolve()
        if resolved.parent == parent and resolved.name.startswith("DGTourS19MotionCook_"):
            shutil.rmtree(resolved, ignore_errors=True)


def make_receipt(
    candidate_id: str, archive_facts: dict[str, Any],
    unrealpak: Path, editor: Path,
    motion_version: str = "v1",
) -> dict[str, Any]:
    if motion_version not in MOTION_CONTRACTS:
        raise ValidationError(f"unsupported production-motion version: {motion_version}")
    return {
        "schema": "DiscGolfTour.Session19ProductionMotionCookPresenceEvidence.v1",
        "schemaVersion": 1,
        "session": 19,
        "milestone": "v0.5",
        "candidateId": candidate_id,
        "state": "PASS_SEVEN_PRODUCTION_MOTION_IDENTITIES_IN_IOSTORE_AND_ASSET_REGISTRY_SYNTHETIC_IDENTITIES_ABSENT_HUMAN_GATES_PENDING",
        "target": {
            "platform": "Windows",
            "configuration": "Shipping",
            "targetRoot": TARGET_ROOT,
            "requiredIdentityCount": 7,
            "motionVersion": motion_version,
        },
        "tools": {
            "unrealPak": identity(unrealpak),
            "unrealEditorCmd": identity(editor),
        },
        "archive": archive_facts,
        "claimBoundary": CLAIMS,
        "residualBlockers": [
            "PROFILE_MATRIX_META_HUMAN_RETARGET_AND_SINGLE_RELEASE_LIVE_VALIDATION_PENDING",
            "HUMAN_ANIMATION_DISC_CONTACT_AND_PRODUCT_ART_APPROVAL_PENDING",
            "PERFORMANCE_SOAK_OWNER_AND_RELEASE_APPROVAL_PENDING",
        ],
        "issues": [],
    }


def receipt_errors(value: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["receipt root must be an object"]
    expected_keys = {
        "schema", "schemaVersion", "session", "milestone", "candidateId",
        "state", "target", "tools", "archive", "claimBoundary",
        "residualBlockers", "issues",
    }
    if set(value) != expected_keys:
        errors.append("receipt keys differ")
        return errors
    for key, expected in {
        "schema": "DiscGolfTour.Session19ProductionMotionCookPresenceEvidence.v1",
        "schemaVersion": 1,
        "session": 19,
        "milestone": "v0.5",
        "state": "PASS_SEVEN_PRODUCTION_MOTION_IDENTITIES_IN_IOSTORE_AND_ASSET_REGISTRY_SYNTHETIC_IDENTITIES_ABSENT_HUMAN_GATES_PENDING",
        "claimBoundary": CLAIMS,
        "issues": [],
    }.items():
        if type(value.get(key)) is not type(expected) or value.get(key) != expected:
            errors.append(f"receipt.{key} differs")
    if not isinstance(value.get("candidateId"), str) or not CANDIDATE_RE.fullmatch(
        value["candidateId"]
    ):
        errors.append("receipt.candidateId differs")
    target = value.get("target")
    motion_version = "v1"
    expected_target = {
        "platform": "Windows", "configuration": "Shipping",
        "targetRoot": TARGET_ROOT, "requiredIdentityCount": 7,
    }
    if not isinstance(target, dict):
        errors.append("receipt.target differs")
    elif "motionVersion" not in target:
        # Immutable receipts emitted before the versioned contract are exactly
        # the v1/v001 contract and remain valid without being rewritten.
        if target != expected_target:
            errors.append("receipt.target differs")
    else:
        raw_motion_version = target.get("motionVersion")
        if not isinstance(raw_motion_version, str) or raw_motion_version not in MOTION_CONTRACTS:
            errors.append("receipt.target.motionVersion differs")
        else:
            motion_version = raw_motion_version
        versioned_target = {**expected_target, "motionVersion": raw_motion_version}
        if target != versioned_target:
            errors.append("receipt.target differs")
    expected_assets = MOTION_CONTRACTS[motion_version]
    sha_re = re.compile(r"[0-9A-F]{64}\Z")

    def valid_identity(item: Any) -> bool:
        return (
            isinstance(item, dict)
            and type(item.get("bytes")) is int
            and item["bytes"] > 0
            and isinstance(item.get("sha256"), str)
            and sha_re.fullmatch(item["sha256"]) is not None
        )

    tools = value.get("tools", {})
    if set(tools) != {"unrealPak", "unrealEditorCmd"} or not all(
        valid_identity(tools.get(key)) for key in ("unrealPak", "unrealEditorCmd")
    ):
        errors.append("receipt.tools differs")
    archive = value.get("archive", {})
    for key, expected in {
        "archiveHostPathRecorded": False,
        "targetObjectCount": len(expected_assets),
        "ioStoreTargetCount": len(expected_assets),
        "assetRegistryTargetCount": len(expected_assets),
        "protectedSyntheticIoStoreIdentityCount": 0,
        "protectedSyntheticAssetRegistryIdentityCount": 0,
    }.items():
        if type(archive.get(key)) is not type(expected) or archive.get(key) != expected:
            errors.append(f"receipt.archive.{key} differs")
    io_targets = archive.get("ioStoreTargets")
    registry_targets = archive.get("assetRegistryTargets")
    expected_objects = [object_path for object_path, _relative in expected_assets]
    if (
        not isinstance(io_targets, list)
        or len(io_targets) != len(expected_assets)
        or any(not isinstance(row, dict) for row in io_targets)
        or [row.get("objectPath") for row in io_targets] != expected_objects
        or any(row.get("matchCount") != 1 for row in io_targets)
        or any(not isinstance(row.get("containerEntry"), str) for row in io_targets)
    ):
        errors.append("receipt.archive.ioStoreTargets differs")
    if (
        not isinstance(registry_targets, list)
        or len(registry_targets) != len(expected_assets)
        or any(not isinstance(row, dict) for row in registry_targets)
        or [row.get("objectPath") for row in registry_targets] != expected_objects
        or any(row.get("present") is not True for row in registry_targets)
    ):
        errors.append("receipt.archive.assetRegistryTargets differs")
    containers = archive.get("containers", {})
    if set(containers) != {"pak", "utoc", "ucas"} or not all(
        valid_identity(containers.get(key))
        and isinstance(containers[key].get("path"), str)
        for key in ("pak", "utoc", "ucas")
    ):
        errors.append("receipt.archive.containers differs")
    registry = archive.get("assetRegistry", {})
    if (
        not valid_identity(registry)
        or type(registry.get("dumpPageCount")) is not int
        or registry["dumpPageCount"] < 1
        or registry.get("commandletErrorCount") != 0
        or registry.get("commandletWarningCount") != 0
    ):
        errors.append("receipt.archive.assetRegistry differs")
    guard = archive.get("projectMutationGuard", {})
    if (
        guard.get("guardedFileCount") != len(expected_assets) + 1
        or guard.get("stable") is not True
    ):
        errors.append("receipt.archive.projectMutationGuard differs")
    if value.get("residualBlockers") != [
        "PROFILE_MATRIX_META_HUMAN_RETARGET_AND_SINGLE_RELEASE_LIVE_VALIDATION_PENDING",
        "HUMAN_ANIMATION_DISC_CONTACT_AND_PRODUCT_ART_APPROVAL_PENDING",
        "PERFORMANCE_SOAK_OWNER_AND_RELEASE_APPROVAL_PENDING",
    ]:
        errors.append("receipt.residualBlockers differs")
    return errors


def self_test() -> tuple[int, list[str]]:
    failures: list[str] = []
    caught = 0
    default_game = DEFAULT_GAME.read_text(encoding="utf-8")
    runtime_header = RUNTIME_PATH_HEADER.read_text(encoding="utf-8")
    runtime_cook_spec = RUNTIME_COOK_SPEC.read_text(encoding="utf-8")
    try:
        boundary = shipping_source_boundary_preflight()
        if (
            boundary["runtime"]["motionVersion"] != CURRENT_MOTION_VERSION
            or boundary["runtime"]["sourceAssetCount"] != 7
            or len(boundary["runtime"].get("sourceAssets", [])) != 7
            or boundary["editorAuthoring"]["shippingInclusionAllowed"] is not False
            or boundary["editorAuthoring"]["runtimeAuthorityReferenceCount"] != 0
            or boundary["editorAuthoring"].get("runtimeAuthorityFileCount", 0) < 1
            or boundary["claimBoundary"]["shippingCookPresenceAccepted"] is not False
        ):
            failures.append("valid static Shipping boundary facts differ")
    except (OSError, UnicodeError, ValidationError) as exc:
        failures.append(f"valid static Shipping boundary failed: {exc}")

    cook_spec_with_reverse = json.dumps({
        **json.loads(runtime_cook_spec),
        "forbidden_reverse_runtime_authority": REVERSE_AUTHORING_RETARGETER_OBJECT,
    })
    boundary_mutations: list[tuple[str, Callable[[], None]]] = [
        (
            "missing ProductionMotion AlwaysCook",
            lambda: shipping_source_boundary_facts(
                default_game.replace(
                    '+DirectoriesToAlwaysCook=(Path="/Game/DiscGolf/Animation/ProductionMotion")',
                    "",
                ),
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "duplicate ProductionMotion AlwaysCook",
            lambda: shipping_source_boundary_facts(
                default_game
                + '\n+DirectoriesToAlwaysCook=(Path="/Game/DiscGolf/Animation/ProductionMotion")\n',
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "ProductionMotion NeverCook conflict",
            lambda: shipping_source_boundary_facts(
                default_game
                + '\n+DirectoriesToNeverCook=(Path="/Game/DiscGolf/Animation/ProductionMotion")\n',
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "missing Mocap NeverCook",
            lambda: shipping_source_boundary_facts(
                default_game.replace(
                    '+DirectoriesToNeverCook=(Path="/Game/DiscGolf/Animation/Mocap")',
                    "",
                ),
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "Mocap AlwaysCook conflict",
            lambda: shipping_source_boundary_facts(
                default_game
                + '\n+DirectoriesToAlwaysCook=(Path="/Game/DiscGolf/Animation/Mocap")\n',
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "missing Throws NeverCook",
            lambda: shipping_source_boundary_facts(
                default_game.replace(
                    '+DirectoriesToNeverCook=(Path="/Game/DiscGolf/Animation/Throws")',
                    "",
                ),
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "historical active runtime alias",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header.replace(
                    "ActiveVersion = V6::Version", "ActiveVersion = V5::Version"
                ),
                runtime_cook_spec,
            ),
        ),
        (
            "missing current-v006 runtime path",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header.replace(V6_EXPECTED_ASSETS[0][0], ""),
                runtime_cook_spec,
            ),
        ),
        (
            "reverse retargeter in runtime header",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header + "\n" + REVERSE_AUTHORING_RETARGETER_OBJECT,
                runtime_cook_spec,
            ),
        ),
        (
            "reverse retargeter in packaging configuration",
            lambda: shipping_source_boundary_facts(
                default_game + "\n" + REVERSE_AUTHORING_RETARGETER_OBJECT,
                runtime_header,
                runtime_cook_spec,
            ),
        ),
        (
            "reverse retargeter in runtime cook manifest",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header,
                cook_spec_with_reverse,
            ),
        ),
        (
            "reverse retargeter in another runtime source file",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header,
                runtime_cook_spec,
                REVERSE_AUTHORING_RETARGETER_OBJECT,
            ),
        ),
        (
            "invalid runtime cook manifest JSON",
            lambda: shipping_source_boundary_facts(
                default_game,
                runtime_header,
                "{",
            ),
        ),
    ]
    for historical_version in ("v1", "v2", "v3", "v4", "v5"):
        boundary_mutations.append((
            f"historical live motion selection {historical_version}",
            lambda value=historical_version: require_current_motion_version(value),
        ))
    for name, mutation in boundary_mutations:
        try:
            mutation()
        except ValidationError:
            caught += 1
        else:
            failures.append(f"Shipping boundary mutation escaped: {name}")

    names = ["../../../DiscGolfTour/Content/" + relative for _, relative in EXPECTED_ASSETS]
    registry = "\n".join(object_path for object_path, _ in EXPECTED_ASSETS)
    try:
        facts = inventory_facts(names, registry)
    except ValidationError as exc:
        return 0, [f"valid inventory fixture failed: {exc}"]

    inventory_mutations: list[tuple[str, Callable[[], None]]] = []
    for index in range(7):
        inventory_mutations.append((
            f"missing IoStore target {index}",
            lambda index=index: inventory_facts(names[:index] + names[index + 1:], registry),
        ))
        inventory_mutations.append((
            f"missing registry target {index}",
            lambda index=index: inventory_facts(
                names, registry.replace(EXPECTED_ASSETS[index][0], "")
            ),
        ))
    inventory_mutations.extend([
        (
            "duplicate IoStore target",
            lambda: inventory_facts(names + [names[0]], registry),
        ),
        (
            "protected IoStore root",
            lambda: inventory_facts(
                names + ["../../../DiscGolfTour/Content/DiscGolf/Animation/Throws/"
                         "A_DG_RHBH_Prototype.uasset"], registry,
            ),
        ),
        (
            "protected registry root",
            lambda: inventory_facts(names, registry + "\n" + PROTECTED_ASSETS[0]),
        ),
        (
            "reverse authoring retargeter in IoStore",
            lambda: inventory_facts(
                names + [
                    "../../../DiscGolfTour/Content/DiscGolf/Animation/Mocap/"
                    "Rigs/RTG_MetaHuman_To_DGMaster.uasset"
                ],
                registry,
            ),
        ),
        (
            "reverse authoring retargeter in registry",
            lambda: inventory_facts(
                names, registry + "\n" + REVERSE_AUTHORING_RETARGETER_OBJECT
            ),
        ),
    ])
    for name, mutation in inventory_mutations:
        try:
            mutation()
        except ValidationError:
            caught += 1
        else:
            failures.append(f"inventory mutation escaped: {name}")

    archive = {
        "archiveHostPathRecorded": False,
        "containers": {
            "pak": {"path": "DiscGolfTour/Content/Paks/Test.pak", "bytes": 1, "sha256": "A" * 64},
            "utoc": {"path": "DiscGolfTour/Content/Paks/Test.utoc", "bytes": 1, "sha256": "A" * 64},
            "ucas": {"path": "DiscGolfTour/Content/Paks/Test.ucas", "bytes": 1, "sha256": "A" * 64},
        },
        "ioStoreContainerCount": 1, "ioStoreChunkCount": 7,
        "ioStoreContainers": [],
        "assetRegistry": {
            "bytes": 1, "sha256": "A" * 64, "dumpPageCount": 1,
            "commandletErrorCount": 0, "commandletWarningCount": 0,
        },
        "projectMutationGuard": {
            "guardedFileCount": 8, "stable": True, "protectedSaveSha256": "A" * 64,
        },
        **facts,
    }
    fixture_tool = ROOT / "Scripts/validate_dg_session19_production_motion_cook.py"
    receipt = make_receipt(
        "S19_WindowsShipping_SELFTEST", archive, fixture_tool, fixture_tool,
    )
    receipt_mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("extra key", lambda v: v.__setitem__("releaseReady", True)),
        ("schema", lambda v: v.__setitem__("schema", "bad")),
        ("version", lambda v: v.__setitem__("schemaVersion", 2)),
        ("motion version", lambda v: v["target"].__setitem__("motionVersion", "v3")),
        ("candidate", lambda v: v.__setitem__("candidateId", "unsafe")),
        ("configuration", lambda v: v["target"].__setitem__("configuration", "Development")),
        ("target count", lambda v: v["archive"].__setitem__("targetObjectCount", 6)),
        ("IoStore count", lambda v: v["archive"].__setitem__("ioStoreTargetCount", 6)),
        ("registry count", lambda v: v["archive"].__setitem__("assetRegistryTargetCount", 6)),
        ("synthetic IoStore", lambda v: v["archive"].__setitem__("protectedSyntheticIoStoreIdentityCount", 1)),
        ("synthetic registry", lambda v: v["archive"].__setitem__("protectedSyntheticAssetRegistryIdentityCount", 1)),
        ("host path", lambda v: v["archive"].__setitem__("archiveHostPathRecorded", True)),
        ("mutation guard", lambda v: v["archive"]["projectMutationGuard"].__setitem__("stable", False)),
        ("human animation", lambda v: v["claimBoundary"].__setitem__("humanAnimationApproval", True)),
        ("human contact", lambda v: v["claimBoundary"].__setitem__("humanDiscContactApproval", True)),
        ("performance", lambda v: v["claimBoundary"].__setitem__("performanceAcceptance", True)),
        ("owner", lambda v: v["claimBoundary"].__setitem__("ownerApproval", True)),
        ("release", lambda v: v["claimBoundary"].__setitem__("releaseReady", True)),
        ("issues", lambda v: v["issues"].append("hidden")),
    ]
    for name, mutation in receipt_mutations:
        value = copy.deepcopy(receipt)
        mutation(value)
        if receipt_errors(value):
            caught += 1
        else:
            failures.append(f"receipt mutation escaped: {name}")
    if receipt_errors(receipt):
        failures.append("valid receipt fixture failed: " + "; ".join(receipt_errors(receipt)))

    legacy_receipt = copy.deepcopy(receipt)
    legacy_receipt["target"].pop("motionVersion")
    if receipt_errors(legacy_receipt):
        failures.append(
            "historical v1 receipt fixture failed: "
            + "; ".join(receipt_errors(legacy_receipt))
        )

    for motion_version, expected_assets in (
        ("v2", V2_EXPECTED_ASSETS),
        ("v3", V3_EXPECTED_ASSETS),
        ("v4", V4_EXPECTED_ASSETS),
        ("v5", V5_EXPECTED_ASSETS),
        ("v6", V6_EXPECTED_ASSETS),
    ):
        versioned_names = [
            "../../../DiscGolfTour/Content/" + relative
            for _, relative in expected_assets
        ]
        versioned_registry = "\n".join(
            object_path for object_path, _relative in expected_assets
        )
        try:
            versioned_facts = inventory_facts(
                versioned_names, versioned_registry, expected_assets
            )
        except ValidationError as exc:
            failures.append(
                f"valid {motion_version} inventory fixture failed: {exc}"
            )
            return caught, failures
        versioned_archive = copy.deepcopy(archive)
        for key in (
            "targetObjectCount", "ioStoreTargetCount", "ioStoreTargets",
            "protectedSyntheticIoStoreIdentityCount", "assetRegistryTargetCount",
            "assetRegistryTargets", "protectedSyntheticAssetRegistryIdentityCount",
        ):
            versioned_archive[key] = versioned_facts[key]
        versioned_receipt = make_receipt(
            "S19_WindowsShipping_SELFTEST", versioned_archive,
            fixture_tool, fixture_tool, motion_version,
        )
        if receipt_errors(versioned_receipt):
            failures.append(
                f"valid {motion_version} receipt fixture failed: "
                + "; ".join(receipt_errors(versioned_receipt))
            )
        versioned_mutations: list[tuple[str, Callable[[], None]]] = [
            (
                f"{motion_version} missing IoStore target",
                lambda assets=expected_assets, names=versioned_names,
                    registry=versioned_registry: inventory_facts(
                        names[:-1], registry, assets
                    ),
            ),
            (
                f"{motion_version} missing registry target",
                lambda assets=expected_assets, names=versioned_names,
                    registry=versioned_registry: inventory_facts(
                        names, registry.replace(assets[-1][0], ""), assets
                    ),
            ),
        ]
        for name, mutation in versioned_mutations:
            try:
                mutation()
            except ValidationError:
                caught += 1
            else:
                failures.append(f"inventory mutation escaped: {name}")
        for name, mutation in (
            (
                f"{motion_version} receipt mislabeled as v1",
                lambda value: value["target"].__setitem__(
                    "motionVersion", "v1"
                ),
            ),
            (
                f"{motion_version} receipt missing version",
                lambda value: value["target"].pop("motionVersion"),
            ),
        ):
            value = copy.deepcopy(versioned_receipt)
            mutation(value)
            if receipt_errors(value):
                caught += 1
            else:
                failures.append(f"receipt mutation escaped: {name}")
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--unreal-editor-cmd", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--motion-version", choices=tuple(MOTION_CONTRACTS))
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    live = (
        args.motion_version, args.candidate_id, args.archive, args.unrealpak,
        args.unreal_editor_cmd, args.output,
    )
    if args.self_test:
        if args.preflight or any(value is not None for value in live):
            parser.error("--self-test does not accept live candidate arguments")
        caught, failures = self_test()
        if failures or caught < 30:
            print(f"Production motion cook validator self-test: FAIL ({caught} caught)")
            for failure in failures:
                print(f"- {failure}")
            return 1
        print(f"Production motion cook validator self-test: PASS ({caught} caught)")
        return 0

    if args.preflight:
        if any(value is not None for value in live):
            parser.error("--preflight does not accept live candidate arguments")
        try:
            print(json.dumps(
                shipping_source_boundary_preflight(),
                indent=2,
                ensure_ascii=False,
                allow_nan=False,
            ))
            return 0
        except (OSError, UnicodeError, ValidationError) as exc:
            print(f"Production motion Shipping boundary preflight: FAIL\n- {exc}")
            return 1

    if any(value is None for value in live):
        parser.error(
            "live validation requires --motion-version --candidate-id --archive "
            "--unrealpak --unreal-editor-cmd --output"
        )
    assert args.motion_version is not None
    assert args.candidate_id is not None
    assert args.archive is not None
    assert args.unrealpak is not None
    assert args.unreal_editor_cmd is not None
    assert args.output is not None
    if not CANDIDATE_RE.fullmatch(args.candidate_id):
        parser.error("--candidate-id is malformed")
    expected_name = f"ProductionMotionCookPresence-{args.candidate_id}.json"
    evidence_root = (ROOT / "Evidence/Session19").resolve()
    output = args.output.resolve()
    if output.parent != evidence_root or output.name != expected_name:
        parser.error(f"--output must be Evidence/Session19/{expected_name}")
    if output.exists():
        print(f"Production motion cook validation: FAIL\n- immutable receipt exists: {output}")
        return 1

    try:
        require_current_motion_version(args.motion_version)
        shipping_source_boundary_preflight()
        expected_assets = MOTION_CONTRACTS[args.motion_version]
        facts = audit_archive(
            args.candidate_id, args.archive, args.unrealpak, args.unreal_editor_cmd,
            expected_assets,
        )
        receipt = make_receipt(
            args.candidate_id, facts,
            args.unrealpak.resolve(strict=True),
            args.unreal_editor_cmd.resolve(strict=True),
            args.motion_version,
        )
        errors = receipt_errors(receipt)
        if errors:
            raise ValidationError("; ".join(errors))
        rendered = json.dumps(
            receipt, indent=2, ensure_ascii=False, allow_nan=False,
        ) + "\n"
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8", newline="\n") as handle:
            handle.write(rendered)
            handle.flush()
            os.fsync(handle.fileno())
    except (OSError, UnicodeError, ValidationError) as exc:
        print(f"Production motion cook validation: FAIL\n- {exc}")
        return 1

    print(
        f"Production motion cook validation: PASS motion={args.motion_version} "
        "IoStore=7/7 AssetRegistry=7/7 editor_authoring=0 "
        "reverse_retargeter=excluded "
        "human_animation=false human_contact=false performance=false release_ready=false"
    )
    print(f"Receipt: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
