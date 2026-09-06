#!/usr/bin/env python3
"""Strict, fail-closed Session 9 brand and provenance validation.

This is a build-time policy gate, not legal advice.  It intentionally keeps
license/receipt data out of runtime Content and writes only the canonical
Saved/BrandLicense report unless ``--no-report`` is supplied.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
ROOT_RESOLVED = ROOT.resolve()
CONTRACT_PATH = ROOT / "Config/DG_BrandLicenseContract.json"
REPORT_PATH = ROOT / "Saved/BrandLicense/LatestBrandLicenseAudit.json"

INT64_MIN = -(2**63)
INT64_MAX = 2**63 - 1
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
ID_RE = re.compile(r"^[a-z0-9]+(?:_[a-z0-9]+)*$")
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}

EXPECTED_POLY_HAVEN_SHA256 = (
    "880C79DD7F9151DAD0D8344C0E505CBB1E3AC98D06C955E9DACD6E305BCBD250"
)
EXPECTED_DEVELOPMENT_MANIFESTS = {
    "session6_proxy_outfits": {
        "path": "SourceArt/DiscGolf/Outfits/Proxy/proxy_outfit_source_manifest.json",
        "sha256": "551F5021720410879445675EA37BFC423E24DD99E69E98E0B2B3F539B47B6B27",
        "required_status": "DO_NOT_SHIP",
    },
    "session7_proxy_customization": {
        "path": "SourceArt/DiscGolf/Characters/Customization/Proxy/proxy_customization_source_manifest.json",
        "sha256": "4481300D0079B7B9E0360B82885E2665BF016362FBB2BECE0E708718C18C4711",
        "required_status": "DO_NOT_SHIP",
    },
    "motion_source_registry": {
        "path": "SourceArt/DiscGolf/Mocap/motion_source_registry.json",
        "sha256": "13301FF11E3E6024E1F64C1154D7AEBC232F71DECC7C143F12F319A929BD979E",
        "required_status": "NO_CLEARED_PRODUCTION_MOTION_PRESENT",
    },
}
EXPECTED_QUARANTINES = {
    "fab_project_nature_spruce_forest": {
        "publisher": "Project Nature",
        "listing_id": "f8044501-17a2-498f-b198-5f1bc71ee87a",
        "disk_root": "Content/PN_interactiveSpruceForest",
        "package_root": "/Game/PN_interactiveSpruceForest",
        "files": 363,
        "bytes": 1565612902,
    },
    "fab_greenbuggames_stump_scanned": {
        "publisher": "GreenBugGames",
        "listing_id": "5f433961-8d90-49ab-a991-e4c7a78acccf",
        "disk_root": "Content/Stump_Scanned",
        "package_root": "/Game/Stump_Scanned",
        "files": 63,
        "bytes": 680263631,
    },
    "fab_tharlevfx_water_materials": {
        "publisher": "tharlevfx",
        "listing_id": "063155ea-d9d2-4f29-b09f-33270b0bc861",
        "disk_root": "Content/WaterMaterials",
        "package_root": "/Game/WaterMaterials",
        "files": 107,
        "bytes": 91003274,
    },
}
EXPECTED_FOREST_IDS = [
    "OpeningBroadTreeLine",
    "NeedleCanopyCompression",
    "GalleryLakeFrame",
]
EXPECTED_NEVER_COOK = [
    "/Game/PN_interactiveSpruceForest",
    "/Game/Stump_Scanned",
    "/Game/WaterMaterials",
]
EXPECTED_FORBIDDEN_TOKENS = [
    "premium_disc_golf",
    "premium_default",
    "premium_bag_default",
    "BrewsterRidgeTreeLine",
    "NorthwoodBlackCompression",
    "IdlewildLakeFrame",
    "Brewster Ridge",
    "Northwood Black",
    "Idlewild",
    "Innova",
    "Discraft",
    "Discmania",
    "Dynamic Discs",
    "Latitude 64",
    "MVP Disc",
    "Prodigy Disc",
    "Westside Discs",
    "Nike",
    "Adidas",
    "Oakley",
    "Under Armour",
]
EXPECTED_FORBIDDEN_UI = [
    "CURATED METAHUMAN",
    "METAHUMAN /",
    "MetaHuman backend availability",
    "assembled MetaHuman preview",
    "Assembled MetaHuman",
    "Verified assembled MetaHuman",
]
EXPECTED_REQUIRED_UI = [
    "CURATED REALISTIC PRESET",
    "REALISTIC / CURATED",
    "REALISTIC / UNAVAILABLE",
    "Realistic character backend is available",
]
EXPECTED_PUBLIC_UI_FILES = [
    "Source/DiscGolfTour/DiscGolfCharacterCreatorWidget.cpp",
    "Source/DiscGolfTour/DiscGolfTourPlayerController.cpp",
    "Source/DiscGolfTour/DiscGolferPawn.cpp",
]
EXPECTED_RELEASE_BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE",
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
]
EXPECTED_GENERIC_SNIPPETS = {
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DGFrameworkCourseDefinition.h": ['FName BrandId = TEXT("dg_generic");'],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DiscGolfDiscDefinition.h": [
        'FName BrandId = TEXT("dg_generic");',
        'FName DefaultStampId = TEXT("dg_generic_default");',
    ],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DiscGolfPlasticDefinition.h": ['FName BrandId = TEXT("dg_generic");'],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DiscGolfTournamentDefinition.h": [
        'FName PresentingBrandId = TEXT("dg_generic");'
    ],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DiscGolfDiscTypes.h": [
        'FName StampId = TEXT("dg_generic_default");',
        'FName BagEquipmentId = TEXT("dg_generic_bag_default");',
    ],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Private/"
    "DiscGolfBagComponent.cpp": [
        'StampId.IsNone() ? TEXT("dg_generic_default") : StampId'
    ],
    "Plugins/DiscGolfCharacterFramework/Source/DiscGolfCharacterFramework/Public/"
    "DiscGolfBrandProfile.h": ["e.g. dg_generic."],
}


class StrictJsonError(ValueError):
    """Raised when JSON violates duplicate/numeric safety rules."""


def _reject_constant(raw: str) -> None:
    raise StrictJsonError(f"non-finite JSON number is forbidden: {raw}")


def _strict_int(raw: str) -> int:
    value = int(raw, 10)
    if value < INT64_MIN or value > INT64_MAX:
        raise StrictJsonError(f"JSON integer exceeds signed 64-bit range: {raw}")
    return value


def _strict_float(raw: str) -> float:
    try:
        decimal_value = Decimal(raw)
    except InvalidOperation as exc:
        raise StrictJsonError(f"invalid JSON number: {raw}") from exc
    if not decimal_value.is_finite():
        raise StrictJsonError(f"non-finite JSON number is forbidden: {raw}")
    value = float(decimal_value)
    if not math.isfinite(value):
        raise StrictJsonError(f"JSON floating-point number overflows: {raw}")
    return value


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON object key: {key!r}")
        result[key] = value
    return result


def _strict_json_text(text: str, label: str) -> Any:
    try:
        return json.loads(
            text,
            object_pairs_hook=_unique_object,
            parse_constant=_reject_constant,
            parse_int=_strict_int,
            parse_float=_strict_float,
        )
    except (json.JSONDecodeError, StrictJsonError, RecursionError) as exc:
        raise StrictJsonError(f"{label}: {exc}") from exc


def _strict_json_file(path: Path, label: str) -> Any:
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as exc:
        raise StrictJsonError(f"{label}: cannot read UTF-8 JSON: {exc}") from exc
    return _strict_json_text(text, label)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _md5(path: Path) -> str:
    # MD5 is used only to reproduce the upstream Poly Haven integrity manifest.
    digest = hashlib.md5(usedforsecurity=False)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _expect_keys(
    value: Any, expected: set[str], label: str, errors: list[str]
) -> dict[str, Any]:
    if type(value) is not dict:
        errors.append(f"{label} must be an object")
        return {}
    actual = set(value)
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing:
        errors.append(f"{label} missing fields: {', '.join(missing)}")
    if extra:
        errors.append(f"{label} has unknown fields: {', '.join(extra)}")
    return value


def _expect_exact(value: Any, expected: Any, label: str, errors: list[str]) -> None:
    # Exact type is intentional: bool is an int subclass in Python.
    if type(value) is not type(expected) or value != expected:
        errors.append(f"{label} must be exactly {expected!r}; got {value!r}")


def _expect_nonempty_string(value: Any, label: str, errors: list[str]) -> str:
    if type(value) is not str or not value:
        errors.append(f"{label} must be a non-empty string")
        return ""
    return value


def _expect_string_list(value: Any, label: str, errors: list[str]) -> list[str]:
    if type(value) is not list or any(type(item) is not str or not item for item in value):
        errors.append(f"{label} must be an array of non-empty strings")
        return []
    if len({item.casefold() for item in value}) != len(value):
        errors.append(f"{label} contains case-insensitive duplicates")
    return value


def _repo_path(raw: Any, label: str, errors: list[str]) -> Path | None:
    if type(raw) is not str or not raw:
        errors.append(f"{label} must be a non-empty repo-relative path")
        return None
    if "\\" in raw or re.match(r"^[A-Za-z]:", raw):
        errors.append(f"{label} must use repo-relative POSIX form: {raw!r}")
        return None
    posix = PurePosixPath(raw)
    if posix.is_absolute() or ".." in posix.parts:
        errors.append(f"{label} escapes the repository: {raw!r}")
        return None
    resolved = ROOT.joinpath(*posix.parts).resolve()
    try:
        resolved.relative_to(ROOT_RESOLVED)
    except ValueError:
        errors.append(f"{label} resolves outside the repository: {raw!r}")
        return None
    return resolved


def _validate_contract_shape(contract: Any) -> list[str]:
    errors: list[str] = []
    root = _expect_keys(
        contract,
        {
            "schema",
            "schema_version",
            "audit_scope",
            "normal_status",
            "release_status",
            "policy",
            "brands",
            "technical_dependencies",
            "approved_external_sources",
            "quarantined_imports",
            "development_only_content",
            "public_name_clearance",
            "scan_contract",
            "release_blockers",
        },
        "contract",
        errors,
    )
    _expect_exact(root.get("schema"), "DiscGolfTour.BrandLicenseContract.v1", "schema", errors)
    _expect_exact(root.get("schema_version"), 1, "schema_version", errors)
    _expect_exact(
        root.get("audit_scope"),
        "TECHNICAL_PROVENANCE_GATE_NOT_LEGAL_ADVICE",
        "audit_scope",
        errors,
    )
    _expect_exact(
        root.get("normal_status"),
        "PASS_CONTRACT_RELEASE_BLOCKED",
        "normal_status",
        errors,
    )
    _expect_exact(
        root.get("release_status"),
        "BLOCKED_PENDING_PROVENANCE_AND_CLEARANCE_CLOSURE",
        "release_status",
        errors,
    )

    policy = _expect_keys(
        root.get("policy"),
        {
            "active_brand_id",
            "unknown_brand_policy",
            "unknown_rights_policy",
            "unregistered_content_policy",
            "working_title_status",
            "receipt_secrets_forbidden",
            "manual_visual_brand_review_required",
            "distribution_builds_require_release_ready",
            "actual_staged_package_provenance_closure_status",
            "source_gate_covers_every_staged_file",
        },
        "policy",
        errors,
    )
    expected_policy = {
        "active_brand_id": "dg_generic",
        "unknown_brand_policy": "BLOCK",
        "unknown_rights_policy": "DO_NOT_SHIP",
        "unregistered_content_policy": "BLOCK",
        "working_title_status": "INTERNAL_ONLY_NOT_CLEARED_PUBLIC_TITLE",
        "receipt_secrets_forbidden": True,
        "manual_visual_brand_review_required": True,
        "distribution_builds_require_release_ready": True,
        "actual_staged_package_provenance_closure_status": (
            "NOT_IMPLEMENTED_SOURCE_GATE_ONLY"
        ),
        "source_gate_covers_every_staged_file": False,
    }
    for key, expected in expected_policy.items():
        _expect_exact(policy.get(key), expected, f"policy.{key}", errors)

    brands = root.get("brands")
    if type(brands) is not list or len(brands) != 2:
        errors.append("brands must contain exactly dg_generic and premium_disc_golf")
        brands = []
    brand_map: dict[str, dict[str, Any]] = {}
    for index, raw in enumerate(brands):
        item = _expect_keys(
            raw,
            {
                "brand_id",
                "display_name",
                "status",
                "public_runtime_allowed",
                "authorized_logo_count",
                "evidence",
            },
            f"brands[{index}]",
            errors,
        )
        brand_id = _expect_nonempty_string(item.get("brand_id"), f"brands[{index}].brand_id", errors)
        if brand_id and not ID_RE.fullmatch(brand_id):
            errors.append(f"brands[{index}].brand_id has invalid stable-ID form")
        key = brand_id.casefold()
        if key in brand_map:
            errors.append(f"duplicate brand_id (case-insensitive): {brand_id}")
        else:
            brand_map[key] = item
        _expect_nonempty_string(item.get("display_name"), f"brands[{index}].display_name", errors)
        evidence = _repo_path(item.get("evidence"), f"brands[{index}].evidence", errors)
        if evidence is not None and not evidence.is_file():
            errors.append(f"brands[{index}].evidence file is missing")
    if set(brand_map) != {"dg_generic", "premium_disc_golf"}:
        errors.append("brand IDs must be exactly dg_generic and premium_disc_golf")
    generic = brand_map.get("dg_generic", {})
    premium = brand_map.get("premium_disc_golf", {})
    for key, expected in {
        "display_name": "Generic project-original presentation",
        "status": "GENERIC_UNBRANDED_APPROVED",
        "public_runtime_allowed": True,
        "authorized_logo_count": 0,
        "evidence": "Docs/DG_GAME_BIBLE.md",
    }.items():
        _expect_exact(generic.get(key), expected, f"dg_generic.{key}", errors)
    for key, expected in {
        "display_name": "Premium Disc Golf",
        "status": "DORMANT_BLOCKED_PENDING_EXPLICIT_REAPPROVAL",
        "public_runtime_allowed": False,
        "authorized_logo_count": 0,
        "evidence": "Docs/DG_GAME_BIBLE.md",
    }.items():
        _expect_exact(premium.get(key), expected, f"premium_disc_golf.{key}", errors)

    dependencies = root.get("technical_dependencies")
    if type(dependencies) is not list or len(dependencies) != 2:
        errors.append("technical_dependencies must contain exactly two entries")
        dependencies = []
    dependency_map: dict[str, dict[str, Any]] = {}
    for index, raw in enumerate(dependencies):
        item = _expect_keys(
            raw,
            {
                "dependency_id",
                "display_name",
                "status",
                "public_runtime_branding_allowed",
                "release_use_allowed",
                "license_url",
                "content_roots",
                "expected_content_file_count",
                "restrictions",
            },
            f"technical_dependencies[{index}]",
            errors,
        )
        dependency_id = _expect_nonempty_string(
            item.get("dependency_id"), f"technical_dependencies[{index}].dependency_id", errors
        )
        key = dependency_id.casefold()
        if key in dependency_map:
            errors.append(f"duplicate dependency_id (case-insensitive): {dependency_id}")
        else:
            dependency_map[key] = item
        _expect_nonempty_string(item.get("display_name"), f"technical_dependencies[{index}].display_name", errors)
        _expect_string_list(item.get("content_roots"), f"technical_dependencies[{index}].content_roots", errors)
        _expect_string_list(item.get("restrictions"), f"technical_dependencies[{index}].restrictions", errors)
        if type(item.get("expected_content_file_count")) is not int or item["expected_content_file_count"] < 0:
            errors.append(f"technical_dependencies[{index}].expected_content_file_count must be a non-negative integer")
    if set(dependency_map) != {"epic_metahuman", "disc_golf_character_framework"}:
        errors.append("technical dependency IDs differ from the frozen pair")
    metahuman = dependency_map.get("epic_metahuman", {})
    framework = dependency_map.get("disc_golf_character_framework", {})
    expected_mh = {
        "display_name": "Epic MetaHuman",
        "status": "APPROVED_UE_ONLY_TECHNICAL_DEPENDENCY",
        "public_runtime_branding_allowed": False,
        "release_use_allowed": True,
        "license_url": "https://www.unrealengine.com/eula/content",
        "content_roots": [
            "Content/DiscGolf/Characters/MetaHuman/Common",
            "Content/DiscGolf/Characters/MetaHuman/Generated",
        ],
        "expected_content_file_count": 261,
        "restrictions": [
            "UE_ONLY_CONTENT",
            "NO_PUBLIC_TECHNOLOGY_BRANDING_WITHOUT_SEPARATE_APPROVAL",
            "NO_AI_TRAINING_OR_DATABASE_USE",
            "SOURCE_MHC_EDITOR_ONLY_EXCLUDED_FROM_RUNTIME_CLOSURE",
        ],
    }
    for key, expected in expected_mh.items():
        _expect_exact(metahuman.get(key), expected, f"epic_metahuman.{key}", errors)
    expected_framework = {
        "display_name": "Disc Golf Character Framework",
        "status": "PROJECT_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
        "public_runtime_branding_allowed": False,
        "release_use_allowed": False,
        "license_url": None,
        "content_roots": ["Plugins/DiscGolfCharacterFramework"],
        "expected_content_file_count": 96,
        "restrictions": [
            "NO_LICENSE_COPYING_OR_NOTICE_PRESENT",
            "NO_REDISTRIBUTION_RIGHT_INFERRED_FROM_LOCAL_POSSESSION",
            "OWNER_DECISION_OR_WRITTEN_GRANT_REQUIRED_BEFORE_DISTRIBUTION",
            "EXPECTED_FILE_COUNT_EXCLUDES_GENERATED_BINARIES_AND_INTERMEDIATE",
        ],
    }
    for key, expected in expected_framework.items():
        _expect_exact(framework.get(key), expected, f"disc_golf_character_framework.{key}", errors)

    sources = root.get("approved_external_sources")
    if type(sources) is not list or len(sources) != 1:
        errors.append("approved_external_sources must contain exactly Poly Haven")
        sources = []
    if sources:
        source = _expect_keys(
            sources[0],
            {
                "source_id",
                "publisher",
                "status",
                "license",
                "license_url",
                "manifest",
                "manifest_sha256",
                "expected_file_count",
                "release_use_allowed",
                "attribution_required",
                "derived_runtime_provenance_status",
                "release_derived_outputs_allowed",
            },
            "approved_external_sources[0]",
            errors,
        )
        expected_source = {
            "source_id": "poly_haven_pine_ridge_cc0",
            "publisher": "Poly Haven",
            "status": "LICENSED_EXTERNAL_APPROVED",
            "license": "CC0-1.0",
            "license_url": "https://polyhaven.com/license",
            "manifest": "SourceArt/PineRidge/PolyHaven/asset_manifest.json",
            "manifest_sha256": EXPECTED_POLY_HAVEN_SHA256,
            "expected_file_count": 35,
            "release_use_allowed": True,
            "attribution_required": False,
            "derived_runtime_provenance_status": (
                "INCOMPLETE_NO_DURABLE_SOURCE_TO_UASSET_RECEIPT"
            ),
            "release_derived_outputs_allowed": False,
        }
        for key, expected in expected_source.items():
            _expect_exact(source.get(key), expected, f"Poly Haven {key}", errors)

    quarantines = root.get("quarantined_imports")
    if type(quarantines) is not list or len(quarantines) != 3:
        errors.append("quarantined_imports must contain exactly three entries")
        quarantines = []
    quarantine_map: dict[str, dict[str, Any]] = {}
    seen_disk_roots: set[str] = set()
    seen_package_roots: set[str] = set()
    for index, raw in enumerate(quarantines):
        item = _expect_keys(
            raw,
            {
                "source_id",
                "publisher",
                "listing_id",
                "disk_root",
                "package_root",
                "expected_file_count",
                "expected_total_bytes",
                "status",
                "release_use_allowed",
                "evidence",
            },
            f"quarantined_imports[{index}]",
            errors,
        )
        source_id = _expect_nonempty_string(item.get("source_id"), f"quarantined_imports[{index}].source_id", errors)
        source_key = source_id.casefold()
        if source_key in quarantine_map:
            errors.append(f"duplicate quarantine source_id (case-insensitive): {source_id}")
        quarantine_map[source_key] = item
        disk_root = _expect_nonempty_string(item.get("disk_root"), f"quarantined_imports[{index}].disk_root", errors)
        package_root = _expect_nonempty_string(item.get("package_root"), f"quarantined_imports[{index}].package_root", errors)
        if disk_root.casefold() in seen_disk_roots:
            errors.append(f"duplicate quarantine disk_root (case-insensitive): {disk_root}")
        seen_disk_roots.add(disk_root.casefold())
        if package_root.casefold() in seen_package_roots:
            errors.append(f"duplicate quarantine package_root (case-insensitive): {package_root}")
        seen_package_roots.add(package_root.casefold())
        _repo_path(disk_root, f"quarantined_imports[{index}].disk_root", errors)
        evidence = _repo_path(item.get("evidence"), f"quarantined_imports[{index}].evidence", errors)
        if evidence is not None and not evidence.is_file():
            errors.append(f"quarantined_imports[{index}].evidence is missing")
        if not package_root.startswith("/Game/") or ".." in package_root.split("/"):
            errors.append(f"quarantined_imports[{index}].package_root is invalid")
        if type(item.get("expected_file_count")) is not int or item["expected_file_count"] < 0:
            errors.append(f"quarantined_imports[{index}].expected_file_count is invalid")
        if type(item.get("expected_total_bytes")) is not int or item["expected_total_bytes"] < 0:
            errors.append(f"quarantined_imports[{index}].expected_total_bytes is invalid")
        _expect_exact(item.get("status"), "QUARANTINED_PENDING_RECEIPT_EVIDENCE", f"quarantined_imports[{index}].status", errors)
        _expect_exact(item.get("release_use_allowed"), False, f"quarantined_imports[{index}].release_use_allowed", errors)
    if set(quarantine_map) != set(EXPECTED_QUARANTINES):
        errors.append("quarantine source IDs differ from the frozen set")
    for source_id, expected in EXPECTED_QUARANTINES.items():
        actual = quarantine_map.get(source_id, {})
        comparisons = {
            "publisher": expected["publisher"],
            "listing_id": expected["listing_id"],
            "disk_root": expected["disk_root"],
            "package_root": expected["package_root"],
            "expected_file_count": expected["files"],
            "expected_total_bytes": expected["bytes"],
        }
        for key, value in comparisons.items():
            _expect_exact(actual.get(key), value, f"{source_id}.{key}", errors)

    development = root.get("development_only_content")
    if type(development) is not list or len(development) != 3:
        errors.append("development_only_content must contain exactly three entries")
        development = []
    development_map: dict[str, dict[str, Any]] = {}
    for index, raw in enumerate(development):
        item = _expect_keys(
            raw,
            {"content_id", "manifest", "manifest_sha256", "required_status", "release_use_allowed"},
            f"development_only_content[{index}]",
            errors,
        )
        content_id = _expect_nonempty_string(item.get("content_id"), f"development_only_content[{index}].content_id", errors)
        key = content_id.casefold()
        if key in development_map:
            errors.append(f"duplicate development content_id: {content_id}")
        development_map[key] = item
        _repo_path(item.get("manifest"), f"development_only_content[{index}].manifest", errors)
        sha = item.get("manifest_sha256")
        if type(sha) is not str or not SHA256_RE.fullmatch(sha):
            errors.append(f"development_only_content[{index}].manifest_sha256 is invalid")
        _expect_exact(item.get("release_use_allowed"), False, f"development_only_content[{index}].release_use_allowed", errors)
    if set(development_map) != set(EXPECTED_DEVELOPMENT_MANIFESTS):
        errors.append("development content IDs differ from the frozen set")
    for content_id, expected in EXPECTED_DEVELOPMENT_MANIFESTS.items():
        actual = development_map.get(content_id, {})
        for key, expected_value in {
            "manifest": expected["path"],
            "manifest_sha256": expected["sha256"],
            "required_status": expected["required_status"],
        }.items():
            _expect_exact(actual.get(key), expected_value, f"{content_id}.{key}", errors)

    clearance = _expect_keys(
        root.get("public_name_clearance"),
        {
            "final_public_title",
            "final_public_title_status",
            "working_names",
            "equipment_display_names",
            "course_display_names",
        },
        "public_name_clearance",
        errors,
    )
    _expect_exact(clearance.get("final_public_title"), None, "final_public_title", errors)
    _expect_exact(clearance.get("final_public_title_status"), "NOT_LOCKED_NOT_CLEARED", "final_public_title_status", errors)
    _expect_exact(clearance.get("working_names"), ["DGTour", "DiscGolfTour", "Disc Golf Tour"], "working_names", errors)
    equipment = clearance.get("equipment_display_names")
    if type(equipment) is not list or len(equipment) != 5:
        errors.append("equipment_display_names must contain exactly five entries")
        equipment = []
    expected_equipment = ["Apex", "Vector", "Line", "Compass", "Touch"]
    for index, expected_name in enumerate(expected_equipment):
        item = _expect_keys(
            equipment[index] if index < len(equipment) else {},
            {"stable_id", "display_name", "status"},
            f"equipment_display_names[{index}]",
            errors,
        )
        _expect_exact(item.get("stable_id"), expected_name, f"equipment[{index}].stable_id", errors)
        _expect_exact(item.get("display_name"), expected_name, f"equipment[{index}].display_name", errors)
        _expect_exact(item.get("status"), "PENDING_PUBLIC_NAME_CLEARANCE_DEVELOPMENT_ONLY", f"equipment[{index}].status", errors)
    courses = clearance.get("course_display_names")
    if type(courses) is not list or len(courses) != 1:
        errors.append("course_display_names must contain exactly Pine Ridge")
        courses = []
    course = _expect_keys(
        courses[0] if courses else {},
        {"stable_id", "display_name", "status"},
        "course_display_names[0]",
        errors,
    )
    _expect_exact(course.get("stable_id"), "PineRidgeChampionship", "course stable_id", errors)
    _expect_exact(course.get("display_name"), "Pine Ridge Championship", "course display_name", errors)
    _expect_exact(course.get("status"), "PENDING_PUBLIC_NAME_CLEARANCE_DEVELOPMENT_ONLY", "course status", errors)

    scan = _expect_keys(
        root.get("scan_contract"),
        {
            "runtime_text_roots",
            "runtime_binary_roots",
            "forbidden_runtime_tokens",
            "forbidden_public_ui_fragments",
            "required_public_ui_fragments",
            "required_forest_reference_ids",
            "required_never_cook_roots",
        },
        "scan_contract",
        errors,
    )
    expected_scan = {
        "runtime_text_roots": [
            "Config/DefaultGame.ini",
            "Data",
            "Source/DiscGolfTour",
            "Plugins/DiscGolfCharacterFramework/Source",
        ],
        "runtime_binary_roots": [
            "Content/Data/Discs",
            "Content/DiscGolf/Characters/Customization",
            "Content/DiscGolf/Outfits",
            "Content/Presentation/Course/PineRidge",
        ],
        "forbidden_runtime_tokens": EXPECTED_FORBIDDEN_TOKENS,
        "forbidden_public_ui_fragments": EXPECTED_FORBIDDEN_UI,
        "required_public_ui_fragments": EXPECTED_REQUIRED_UI,
        "required_forest_reference_ids": EXPECTED_FOREST_IDS,
        "required_never_cook_roots": EXPECTED_NEVER_COOK,
    }
    for key, expected in expected_scan.items():
        _expect_exact(scan.get(key), expected, f"scan_contract.{key}", errors)
    text_roots = scan.get("runtime_text_roots")
    binary_roots = scan.get("runtime_binary_roots")
    safe_text_roots = text_roots if type(text_roots) is list else []
    safe_binary_roots = binary_roots if type(binary_roots) is list else []
    for index, raw in enumerate(safe_text_roots + safe_binary_roots):
        _repo_path(raw, f"scan root {index}", errors)

    _expect_exact(root.get("release_blockers"), EXPECTED_RELEASE_BLOCKERS, "release_blockers", errors)
    return errors


def _safe_files(root: Path, label: str, errors: list[str]) -> list[Path]:
    if not root.is_dir():
        errors.append(f"{label} directory is missing: {root.relative_to(ROOT)}")
        return []
    result: list[Path] = []
    for directory, directories, filenames in os.walk(root, followlinks=False):
        directory_path = Path(directory)
        for name in list(directories):
            candidate = directory_path / name
            if candidate.is_symlink():
                errors.append(f"{label} contains directory symlink: {candidate.relative_to(ROOT)}")
                directories.remove(name)
        for name in filenames:
            candidate = directory_path / name
            if candidate.is_symlink():
                errors.append(f"{label} contains file symlink: {candidate.relative_to(ROOT)}")
                continue
            try:
                candidate.resolve().relative_to(ROOT_RESOLVED)
            except (OSError, ValueError):
                errors.append(f"{label} file escapes repository: {candidate}")
                continue
            result.append(candidate)
    return sorted(result, key=lambda path: path.relative_to(ROOT).as_posix().casefold())


def _contains_token(path: Path, token: str) -> bool:
    payload = path.read_bytes().lower()
    return token.encode("utf-8").lower() in payload or token.encode("utf-16le").lower() in payload


def _scan_tokens(paths: Iterable[Path], tokens: Iterable[str]) -> dict[str, list[str]]:
    hits: dict[str, list[str]] = {}
    for path in paths:
        try:
            found = [token for token in tokens if _contains_token(path, token)]
        except OSError as exc:
            found = [f"<READ_ERROR:{exc}>"]
        if found:
            hits[path.relative_to(ROOT).as_posix()] = found
    return hits


def _scan_binary_tokens(paths: Iterable[Path], tokens: Iterable[str]) -> dict[str, list[str]]:
    """Scan serialized string runs without treating compressed bytes as text."""
    hits: dict[str, list[str]] = {}
    for path in paths:
        try:
            payload = path.read_bytes()
            ascii_strings = [
                match.group(0)[:-1].decode("ascii")
                for match in re.finditer(rb"[ -~]{4,}\x00", payload)
            ]
            utf16_strings = [
                match.group(0)[:-2].decode("utf-16le")
                for match in re.finditer(rb"(?:[ -~]\x00){4,}\x00\x00", payload)
            ]
            searchable = "\n".join(ascii_strings + utf16_strings).casefold()
            found = [token for token in tokens if token.casefold() in searchable]
        except (OSError, UnicodeError) as exc:
            found = [f"<READ_ERROR:{exc}>"]
        if found:
            hits[path.relative_to(ROOT).as_posix()] = found
    return hits


def _paths_from_roots(raw_roots: list[str], errors: list[str], binary_only: bool) -> list[Path]:
    result: list[Path] = []
    for index, raw in enumerate(raw_roots):
        path = _repo_path(raw, f"scan root {index}", errors)
        if path is None or not path.exists():
            if path is not None:
                errors.append(f"scan root is missing: {raw}")
            continue
        files = [path] if path.is_file() else _safe_files(path, f"scan root {raw}", errors)
        if binary_only:
            files = [file for file in files if file.suffix.casefold() in PACKAGE_SUFFIXES]
        result.extend(files)
    return sorted(set(result), key=lambda path: path.as_posix().casefold())


def _validate_generic_and_public_text(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    missing_snippets: dict[str, list[str]] = {}
    for relative, snippets in EXPECTED_GENERIC_SNIPPETS.items():
        path = ROOT / relative
        if not path.is_file():
            missing_snippets[relative] = ["<FILE_MISSING>"]
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            missing_snippets[relative] = [f"<READ_ERROR:{exc}>"]
            continue
        missing = [snippet for snippet in snippets if snippet not in text]
        if missing:
            missing_snippets[relative] = missing
    if missing_snippets:
        errors.append("generic framework defaults are missing or altered")

    scan = contract["scan_contract"]
    text_files = _paths_from_roots(scan["runtime_text_roots"], errors, False)
    binary_files = _paths_from_roots(scan["runtime_binary_roots"], errors, True)
    text_hits = _scan_tokens(text_files, scan["forbidden_runtime_tokens"])
    binary_hits = _scan_binary_tokens(binary_files, scan["forbidden_runtime_tokens"])
    if text_hits:
        errors.append("forbidden brand/real-course reference tokens remain in runtime text")
    if binary_hits:
        errors.append("forbidden brand/real-course reference tokens remain in runtime packages")

    ui_files: list[Path] = []
    ui_text: list[str] = []
    for relative in EXPECTED_PUBLIC_UI_FILES:
        path = ROOT / relative
        if not path.is_file():
            errors.append(f"required public UI source is missing: {relative}")
            continue
        ui_files.append(path)
        try:
            ui_text.append(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError) as exc:
            errors.append(f"cannot read public UI source {relative}: {exc}")
    combined = "\n".join(ui_text)
    forbidden_ui_hits = [fragment for fragment in scan["forbidden_public_ui_fragments"] if fragment.casefold() in combined.casefold()]
    missing_ui = [fragment for fragment in scan["required_public_ui_fragments"] if fragment not in combined]
    if forbidden_ui_hits:
        errors.append(f"public UI still exposes technical MetaHuman branding: {forbidden_ui_hits}")
    if missing_ui:
        errors.append(f"required generic public UI fragments are missing: {missing_ui}")

    return {
        "generic_default_files": len(EXPECTED_GENERIC_SNIPPETS),
        "missing_generic_snippets": missing_snippets,
        "runtime_text_file_count": len(text_files),
        "runtime_binary_file_count": len(binary_files),
        "forbidden_text_hits": text_hits,
        "forbidden_binary_hits": binary_hits,
        "forbidden_public_ui_hits": forbidden_ui_hits,
        "missing_required_public_ui_fragments": missing_ui,
        "public_ui_files": [path.relative_to(ROOT).as_posix() for path in ui_files],
    }


def _validate_forest_ids(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    path = ROOT / "Data/PineRidgePresentation.json"
    try:
        payload = _strict_json_file(path, "Pine Ridge presentation")
    except StrictJsonError as exc:
        errors.append(str(exc))
        return {"path": path.relative_to(ROOT).as_posix(), "actual": []}
    holes = payload.get("holes") if type(payload) is dict else None
    actual = [item.get("forestReferenceId") for item in holes if type(item) is dict] if type(holes) is list else []
    expected = contract["scan_contract"]["required_forest_reference_ids"]
    if actual != expected:
        errors.append(f"Pine Ridge forest reference IDs differ: {actual!r}")
    return {"path": path.relative_to(ROOT).as_posix(), "actual": actual, "expected": expected}


def _validate_never_cook(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    path = ROOT / "Config/DefaultGame.ini"
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"cannot read Config/DefaultGame.ini: {exc}")
        return {"never_cook_roots": [], "always_cook_roots": [], "overlap": []}
    never = re.findall(r'^\+DirectoriesToNeverCook=\(Path="([^"]+)"\)\s*$', text, re.MULTILINE)
    always = re.findall(r'^\+DirectoriesToAlwaysCook=\(Path="([^"]+)"\)\s*$', text, re.MULTILINE)
    expected = contract["scan_contract"]["required_never_cook_roots"]
    if never != expected:
        errors.append(f"DefaultGame never-cook roots must be exactly ordered {expected!r}; got {never!r}")
    overlap = sorted(set(never) & set(always))
    if overlap:
        errors.append(f"quarantine roots are both always-cooked and never-cooked: {overlap}")
    return {"never_cook_roots": never, "always_cook_roots": always, "overlap": overlap}


def _validate_quarantines(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    inventory: list[dict[str, Any]] = []
    quarantine_entries = {item["source_id"]: item for item in contract["quarantined_imports"]}
    for source_id, expected in EXPECTED_QUARANTINES.items():
        entry = quarantine_entries[source_id]
        root = ROOT / expected["disk_root"]
        files = _safe_files(root, source_id, errors)
        total_bytes = sum(path.stat().st_size for path in files)
        if len(files) != expected["files"] or total_bytes != expected["bytes"]:
            errors.append(
                f"{source_id} inventory differs: files={len(files)} bytes={total_bytes}"
            )
        inventory.append(
            {
                "source_id": source_id,
                "disk_root": entry["disk_root"],
                "package_root": entry["package_root"],
                "file_count": len(files),
                "total_bytes": total_bytes,
                "expected_file_count": expected["files"],
                "expected_total_bytes": expected["bytes"],
            }
        )

    reference_files: list[Path] = []
    for raw in [
        "Data",
        "Source/DiscGolfTour",
        "Plugins/DiscGolfCharacterFramework/Source",
        "Config/DG_RuntimeCookManifest.json",
        "Config/DefaultEngine.ini",
        "DiscGolfTour.uproject",
    ]:
        path = ROOT / raw
        reference_files.extend([path] if path.is_file() else _safe_files(path, f"runtime reference root {raw}", errors))
    reference_files.extend(
        _paths_from_roots(contract["scan_contract"]["runtime_binary_roots"], errors, True)
    )
    reference_files = sorted(set(reference_files), key=lambda path: path.as_posix().casefold())
    reference_tokens = [entry["package_root"] for entry in contract["quarantined_imports"]]
    references = _scan_tokens(reference_files, reference_tokens)
    if references:
        errors.append(f"quarantined package roots have runtime references: {references}")
    return {"inventories": inventory, "reference_scan_file_count": len(reference_files), "runtime_references": references}


def _validate_poly_haven(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    source = contract["approved_external_sources"][0]
    path = ROOT / source["manifest"]
    actual_sha = _sha256(path) if path.is_file() else None
    if actual_sha != source["manifest_sha256"] or actual_sha != EXPECTED_POLY_HAVEN_SHA256:
        errors.append(f"Poly Haven manifest SHA-256 differs: {actual_sha}")
    try:
        payload = _strict_json_file(path, "Poly Haven manifest")
    except StrictJsonError as exc:
        errors.append(str(exc))
        return {"manifest_sha256": actual_sha, "file_count": 0, "verified_file_count": 0}
    root = _expect_keys(
        payload,
        {"schema", "schemaVersion", "generatedUtc", "source", "license", "licenseUrl", "selection", "files"},
        "Poly Haven manifest",
        errors,
    )
    _expect_exact(root.get("schema"), "disc_golf_third_party_source_manifest", "Poly Haven schema", errors)
    _expect_exact(root.get("schemaVersion"), 1, "Poly Haven schemaVersion", errors)
    _expect_exact(root.get("source"), "Poly Haven", "Poly Haven source", errors)
    _expect_exact(root.get("license"), "CC0-1.0", "Poly Haven license", errors)
    _expect_exact(root.get("licenseUrl"), "https://polyhaven.com/license", "Poly Haven licenseUrl", errors)
    files = root.get("files")
    if type(files) is not list or len(files) != 35:
        errors.append(f"Poly Haven manifest must contain exactly 35 files; got {len(files) if type(files) is list else 'invalid'}")
        files = []
    verified = 0
    seen: set[str] = set()
    file_errors_before = len(errors)
    for index, raw in enumerate(files):
        item = _expect_keys(
            raw,
            {"assetId", "role", "relativePath", "sourceUrl", "byteSize", "md5", "license", "sourcePage"},
            f"Poly Haven files[{index}]",
            errors,
        )
        relative = _expect_nonempty_string(item.get("relativePath"), f"Poly Haven files[{index}].relativePath", errors)
        if relative.casefold() in seen:
            errors.append(f"duplicate Poly Haven relativePath: {relative}")
        seen.add(relative.casefold())
        if not relative.startswith("SourceArt/PineRidge/PolyHaven/"):
            errors.append(f"Poly Haven file leaves canonical source root: {relative}")
        local = _repo_path(relative, f"Poly Haven files[{index}].relativePath", errors)
        size = item.get("byteSize")
        checksum = item.get("md5")
        if type(size) is not int or size < 0:
            errors.append(f"Poly Haven files[{index}].byteSize is invalid")
        if type(checksum) is not str or not MD5_RE.fullmatch(checksum):
            errors.append(f"Poly Haven files[{index}].md5 is invalid")
        _expect_exact(item.get("license"), "CC0-1.0", f"Poly Haven files[{index}].license", errors)
        for url_field in ("sourceUrl", "sourcePage"):
            url = item.get(url_field)
            if type(url) is not str or not url.startswith("https://") or "polyhaven" not in url.casefold():
                errors.append(f"Poly Haven files[{index}].{url_field} is invalid")
        if local is None or not local.is_file():
            errors.append(f"Poly Haven source file is missing: {relative}")
            continue
        actual_size = local.stat().st_size
        actual_md5 = _md5(local)
        if size != actual_size:
            errors.append(f"Poly Haven size mismatch for {relative}: {actual_size}")
        if checksum != actual_md5:
            errors.append(f"Poly Haven MD5 mismatch for {relative}: {actual_md5}")
        if size == actual_size and checksum == actual_md5:
            verified += 1
    receipt_path = ROOT / "Saved/PineRidgeAssetImportReceipt.json"
    receipt_sha256 = _sha256(receipt_path) if receipt_path.is_file() else None
    receipt: Any = None
    receipt_parse_error: str | None = None
    if receipt_path.is_file():
        try:
            receipt = _strict_json_file(receipt_path, "Pine Ridge import receipt")
        except StrictJsonError as exc:
            receipt_parse_error = str(exc)
    source_manifest_hash_recorded = (
        type(receipt) is dict
        and receipt.get("sourceManifestSha256") == EXPECTED_POLY_HAVEN_SHA256
    )
    derived_runtime_artifacts = (
        receipt.get("derivedRuntimeArtifacts") if type(receipt) is dict else None
    )
    derived_hashes_recorded = (
        type(derived_runtime_artifacts) is list
        and bool(derived_runtime_artifacts)
        and all(
            type(item) is dict
            and type(item.get("relativePath")) is str
            and type(item.get("sha256")) is str
            and SHA256_RE.fullmatch(item["sha256"]) is not None
            and type(item.get("sourceRelativePaths")) is list
            and bool(item["sourceRelativePaths"])
            for item in derived_runtime_artifacts
        )
    )
    derived_runtime_provenance_complete = (
        source_manifest_hash_recorded and derived_hashes_recorded
    )
    return {
        "manifest": source["manifest"],
        "manifest_sha256": actual_sha,
        "file_count": len(files),
        "verified_file_count": verified,
        "file_error_count": len(errors) - file_errors_before,
        "derived_runtime_provenance_status": source[
            "derived_runtime_provenance_status"
        ],
        "release_derived_outputs_allowed": source[
            "release_derived_outputs_allowed"
        ],
        "import_receipt": receipt_path.relative_to(ROOT).as_posix(),
        "import_receipt_present": receipt_path.is_file(),
        "import_receipt_sha256": receipt_sha256,
        "import_receipt_parse_error": receipt_parse_error,
        "source_manifest_hash_recorded": source_manifest_hash_recorded,
        "derived_runtime_hashes_recorded": derived_hashes_recorded,
        "cryptographic_source_to_runtime_closure_complete": (
            derived_runtime_provenance_complete
        ),
    }


def _validate_staged_package_provenance(contract: dict[str, Any]) -> dict[str, Any]:
    # This source-tree audit cannot close the actual files selected by a later
    # staging/cook step.  A future release lane must validate an immutable
    # staged-package inventory and feed its evidence into this gate.
    policy = contract["policy"]
    status = policy["actual_staged_package_provenance_closure_status"]
    covers_every_staged_file = policy["source_gate_covers_every_staged_file"]
    return {
        "implemented": (
            status != "NOT_IMPLEMENTED_SOURCE_GATE_ONLY"
            and covers_every_staged_file is True
        ),
        "status": status,
        "source_gate_covers_every_staged_file": covers_every_staged_file,
        "reason": (
            "NO_IMMUTABLE_ACTUAL_STAGED_PACKAGE_INVENTORY_OR_PROVENANCE_RECEIPT"
        ),
    }


def _validate_development_manifests(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    entries = {item["content_id"]: item for item in contract["development_only_content"]}
    results: list[dict[str, Any]] = []
    for content_id, expected in EXPECTED_DEVELOPMENT_MANIFESTS.items():
        entry = entries[content_id]
        path = ROOT / expected["path"]
        actual_sha = _sha256(path) if path.is_file() else None
        if actual_sha != expected["sha256"] or actual_sha != entry["manifest_sha256"]:
            errors.append(f"{content_id} manifest SHA-256 differs: {actual_sha}")
        try:
            payload = _strict_json_file(path, content_id)
        except StrictJsonError as exc:
            errors.append(str(exc))
            results.append({"content_id": content_id, "sha256": actual_sha, "semantic_status": "INVALID_JSON"})
            continue
        semantic_errors_before = len(errors)
        if type(payload) is not dict:
            errors.append(f"{content_id} manifest root must be an object")
        elif content_id in {"session6_proxy_outfits", "session7_proxy_customization"}:
            _expect_exact(payload.get("shipping_status"), "DO_NOT_SHIP", f"{content_id}.shipping_status", errors)
            _expect_exact(payload.get("content_status"), "NON_PRODUCTION_PROXY", f"{content_id}.content_status", errors)
            _expect_exact(payload.get("brand_id"), "dg_generic", f"{content_id}.brand_id", errors)
            _expect_exact(payload.get("brand_status"), "GENERIC_UNBRANDED", f"{content_id}.brand_status", errors)
            _expect_exact(payload.get("external_sources"), [], f"{content_id}.external_sources", errors)
            _expect_exact(payload.get("logos"), [], f"{content_id}.logos", errors)
            _expect_exact(payload.get("production_license_claim"), "NONE", f"{content_id}.production_license_claim", errors)
        else:
            _expect_exact(payload.get("schema"), "disc_golf_motion_source_registry", "motion schema", errors)
            _expect_exact(payload.get("schema_version"), 1, "motion schema_version", errors)
            policy = payload.get("policy") if type(payload.get("policy")) is dict else {}
            _expect_exact(policy.get("external_production_sources_present"), False, "motion external production policy", errors)
            _expect_exact(policy.get("shipping_requires_usage_status"), "CLEARED_FOR_PRODUCTION", "motion shipping policy", errors)
            _expect_exact(policy.get("unknown_rights_policy"), "DO_NOT_SHIP", "motion unknown-rights policy", errors)
            sources = payload.get("sources")
            if type(sources) is not list or len(sources) != 9:
                errors.append("motion registry must contain exactly nine sources")
                sources = []
            seen_ids: set[str] = set()
            counts: dict[str, int] = {}
            for index, source in enumerate(sources):
                if type(source) is not dict:
                    errors.append(f"motion sources[{index}] must be an object")
                    continue
                source_id = source.get("source_id")
                if type(source_id) is not str or not source_id:
                    errors.append(f"motion sources[{index}] has no source_id")
                    continue
                if source_id.casefold() in seen_ids:
                    errors.append(f"duplicate motion source_id: {source_id}")
                seen_ids.add(source_id.casefold())
                usage = source.get("usage_status")
                if type(usage) is str:
                    counts[usage] = counts.get(usage, 0) + 1
                else:
                    errors.append(
                        f"motion source {source_id} usage_status must be a string"
                    )
                if type(usage) is not str or usage not in {
                    "DO_NOT_SHIP",
                    "DO_NOT_REPURPOSE",
                }:
                    errors.append(f"motion source {source_id} has cleared/unknown usage: {usage!r}")
                if "DO_NOT_SHIP" not in str(source.get("production_status", "")):
                    errors.append(f"motion source {source_id} production status does not block shipping")
                restrictions = source.get("restrictions")
                if type(restrictions) is not list or "DO_NOT_SHIP" not in restrictions:
                    errors.append(f"motion source {source_id} lacks DO_NOT_SHIP restriction")
                if source.get("external_production_claim") is not False:
                    errors.append(f"motion source {source_id} claims external production clearance")
            if counts != {"DO_NOT_SHIP": 2, "DO_NOT_REPURPOSE": 7}:
                errors.append(f"motion usage counts differ: {counts}")
        results.append(
            {
                "content_id": content_id,
                "manifest": expected["path"],
                "sha256": actual_sha,
                "required_status": entry["required_status"],
                "semantic_status": "PASS" if len(errors) == semantic_errors_before else "FAIL",
            }
        )
    return {"manifests": results}


def _package_name(path: Path) -> str:
    relative = path.relative_to(ROOT / "Content").with_suffix("").as_posix()
    return "/Game/" + relative


def _package_list_sha(packages: list[str]) -> str:
    payload = "".join(f"{package}\n" for package in sorted(packages)).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def _validate_metahuman_and_runtime_cook(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    common_root = ROOT / "Content/DiscGolf/Characters/MetaHuman/Common"
    generated_root = ROOT / "Content/DiscGolf/Characters/MetaHuman/Generated"
    common_files = _safe_files(common_root, "MetaHuman Common", errors)
    generated_files = _safe_files(generated_root, "MetaHuman Generated", errors)
    for label, files in (("Common", common_files), ("Generated", generated_files)):
        unexpected = [path.relative_to(ROOT).as_posix() for path in files if path.suffix.casefold() != ".uasset"]
        if unexpected:
            errors.append(f"MetaHuman {label} contains non-uasset files: {unexpected}")
    common_packages = [_package_name(path) for path in common_files if path.suffix.casefold() == ".uasset"]
    generated_packages = [_package_name(path) for path in generated_files if path.suffix.casefold() == ".uasset"]
    if len(common_packages) != 205:
        errors.append(f"MetaHuman Common must contain 205 packages; got {len(common_packages)}")
    if len(generated_packages) != 56:
        errors.append(f"MetaHuman Generated must contain 56 packages; got {len(generated_packages)}")
    dependency = next(item for item in contract["technical_dependencies"] if item["dependency_id"] == "epic_metahuman")
    if len(common_packages) + len(generated_packages) != dependency["expected_content_file_count"]:
        errors.append("MetaHuman technical dependency total does not equal 261")

    cook_path = ROOT / "Config/DG_RuntimeCookManifest.json"
    try:
        cook = _strict_json_file(cook_path, "runtime cook contract")
    except StrictJsonError as exc:
        errors.append(str(exc))
        return {"common_packages": len(common_packages), "generated_packages": len(generated_packages)}
    if type(cook) is not dict:
        errors.append("runtime cook contract root must be an object")
        return {
            "common_packages": len(common_packages),
            "generated_packages": len(generated_packages),
        }
    _expect_exact(cook.get("schema"), "DiscGolfTour.RuntimeCookManifestSource.v2", "runtime cook schema", errors)
    _expect_exact(cook.get("schema_version"), 2, "runtime cook schema_version", errors)
    _expect_exact(cook.get("expected_metahuman_runtime_package_count"), 264, "expected MetaHuman runtime package count", errors)
    _expect_exact(cook.get("expected_excluded_metahuman_package_count"), 1, "expected excluded MetaHuman count", errors)
    mh_contract = cook.get("metahuman_runtime_contract") if type(cook.get("metahuman_runtime_contract")) is dict else {}
    source_package = "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default"
    _expect_exact(mh_contract.get("source_mhc"), source_package, "source MHC package", errors)
    _expect_exact(
        mh_contract.get("source_mhc_runtime_status"),
        "EDITOR_ONLY_EXCLUDED_FROM_MANIFEST_AND_COOK_CLOSURE",
        "source MHC runtime status",
        errors,
    )
    _expect_exact(mh_contract.get("explicitly_excluded_packages"), [source_package], "source MHC exclusions", errors)
    source_file = ROOT / "Content/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default.uasset"
    if not source_file.is_file():
        errors.append("the editor-only source MHC package is missing")

    inventory = mh_contract.get("runtime_inventory") if type(mh_contract.get("runtime_inventory")) is dict else {}
    _expect_exact(inventory.get("expected_package_count"), 264, "MetaHuman inventory expected count", errors)
    explicit = inventory.get("explicit_packages")
    expected_explicit = [
        "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default",
        "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman",
        "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig",
    ]
    _expect_exact(explicit, expected_explicit, "MetaHuman explicit packages", errors)
    groups = inventory.get("directory_groups")
    if type(groups) is not list or len(groups) != 2:
        errors.append("MetaHuman inventory must contain exactly Common and Generated groups")
        groups = []
    group_map = {
        item.get("name"): item
        for item in groups
        if type(item) is dict and type(item.get("name")) is str
    }
    expected_groups = {
        "ACCEPTED_METAHUMAN_COMMON": (
            205,
            "90C3299AB7182FE1A6D60133936CDD1D6387B3509FF753F46C772579C20CCD1F",
            "/Game/DiscGolf/Characters/MetaHuman/Common",
            "Content/DiscGolf/Characters/MetaHuman/Common",
            common_packages,
        ),
        "ACCEPTED_METAHUMAN_GENERATED": (
            56,
            "0D545C7185BD7794E88E5AFECCAAEFA0BC9E0959B8F26004A560BF53A5AE1238",
            "/Game/DiscGolf/Characters/MetaHuman/Generated",
            "Content/DiscGolf/Characters/MetaHuman/Generated",
            generated_packages,
        ),
    }
    group_hashes: dict[str, str] = {}
    for name, (
        count,
        expected_hash,
        package_root,
        disk_relative_root,
        packages,
    ) in expected_groups.items():
        group = group_map.get(name, {})
        actual_hash = _package_list_sha(packages)
        group_hashes[name] = actual_hash
        _expect_exact(group.get("package_root"), package_root, f"{name} package root", errors)
        _expect_exact(
            group.get("disk_relative_root"),
            disk_relative_root,
            f"{name} disk root",
            errors,
        )
        _expect_exact(group.get("expected_package_count"), count, f"{name} expected count", errors)
        _expect_exact(group.get("package_list_sha256"), expected_hash, f"{name} frozen hash", errors)
        if actual_hash != expected_hash:
            errors.append(f"{name} actual package-list hash differs: {actual_hash}")
    all_packages = common_packages + generated_packages + expected_explicit
    if source_package in all_packages:
        errors.append("editor-only source MHC leaked into runtime package inventory")
    actual_all_hash = _package_list_sha(all_packages)
    _expect_exact(inventory.get("package_list_sha256"), "003A7683069A2353819421CD71ECA82A2A0F8AE2E7786F56E04EA18DB11AB3B9", "MetaHuman package-list hash", errors)
    if len(all_packages) != 264 or actual_all_hash != inventory.get("package_list_sha256"):
        errors.append(f"MetaHuman 264-package union differs: count={len(all_packages)} hash={actual_all_hash}")
    explicit_hash = _package_list_sha(expected_explicit)
    _expect_exact(inventory.get("explicit_package_list_sha256"), explicit_hash, "MetaHuman explicit package hash", errors)
    for relative in [
        "Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset",
        "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset",
    ]:
        if not (ROOT / relative).is_file():
            errors.append(f"MetaHuman explicit runtime package is missing: {relative}")

    runtime_groups = cook.get("runtime_package_groups")
    development_groups = []
    if type(runtime_groups) is not list:
        errors.append("runtime_package_groups must be an array")
    else:
        development_groups = [
            item.get("provenance")
            for item in runtime_groups
            if type(item) is dict
            and (
                "DO_NOT_SHIP" in str(item.get("shipping_status", ""))
                or "PROTOTYPE" in str(item.get("shipping_status", ""))
                or "NON_PRODUCTION" in str(item.get("shipping_status", ""))
            )
        ]
    if not development_groups:
        errors.append("runtime cook contract no longer truthfully identifies development/proxy closure")
    return {
        "common_packages": len(common_packages),
        "generated_packages": len(generated_packages),
        "common_package_list_sha256": group_hashes.get("ACCEPTED_METAHUMAN_COMMON"),
        "generated_package_list_sha256": group_hashes.get("ACCEPTED_METAHUMAN_GENERATED"),
        "runtime_union_count": len(all_packages),
        "runtime_union_sha256": actual_all_hash,
        "source_mhc": source_package,
        "source_mhc_excluded": source_package not in all_packages,
        "development_runtime_groups": development_groups,
    }


def _validate_framework_license(contract: dict[str, Any], errors: list[str]) -> dict[str, Any]:
    plugin_root = ROOT / "Plugins/DiscGolfCharacterFramework"
    files = _safe_files(plugin_root, "DiscGolfCharacterFramework", errors)
    content_files = [
        path
        for path in files
        if not {
            part.casefold()
            for part in path.relative_to(plugin_root).parts[:-1]
        }
        & {"binaries", "intermediate"}
    ]
    license_names = []
    pattern = re.compile(r"^(license|licence|notice|copying)(?:\.|$)", re.IGNORECASE)
    for path in files:
        if pattern.match(path.name):
            license_names.append(path.relative_to(ROOT).as_posix())
    if license_names:
        errors.append(f"framework contract says no license/notice, but files exist: {license_names}")
    dependency = next(
        item for item in contract["technical_dependencies"]
        if item["dependency_id"] == "disc_golf_character_framework"
    )
    expected_content_file_count = dependency["expected_content_file_count"]
    if len(content_files) != expected_content_file_count:
        errors.append(
            "framework content inventory differs after excluding Binaries and "
            f"Intermediate: files={len(content_files)} "
            f"expected={expected_content_file_count}"
        )
    blocker_present = "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED" in contract["release_blockers"]
    if dependency["license_url"] is not None or dependency["release_use_allowed"] is not False or not blocker_present:
        errors.append("framework no-license evidence is inconsistent with the release blocker")
    return {
        "plugin_file_count": len(files),
        "plugin_content_file_count_excluding_generated": len(content_files),
        "expected_content_file_count": expected_content_file_count,
        "license_or_notice_files": license_names,
        "contract_status": dependency["status"],
        "release_blocker_present": blocker_present,
    }


def _observed_blockers(
    contract: dict[str, Any],
    metahuman: dict[str, Any],
    framework: dict[str, Any],
    poly_haven: dict[str, Any],
    staged_package: dict[str, Any],
) -> list[str]:
    clearance = contract["public_name_clearance"]
    blockers: list[str] = []
    if clearance["final_public_title"] is None and clearance["final_public_title_status"] == "NOT_LOCKED_NOT_CLEARED":
        blockers.append("FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED")
    if all(item["status"] == "PENDING_PUBLIC_NAME_CLEARANCE_DEVELOPMENT_ONLY" for item in clearance["equipment_display_names"] + clearance["course_display_names"]):
        blockers.append("EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE")
    if not framework["license_or_notice_files"] and framework["contract_status"] == "PROJECT_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED":
        blockers.append("CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED")
    if metahuman.get("development_runtime_groups"):
        blockers.append("DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE")
    if all(item["status"] == "QUARANTINED_PENDING_RECEIPT_EVIDENCE" and item["release_use_allowed"] is False for item in contract["quarantined_imports"]):
        blockers.append("QUARANTINED_IMPORT_RECEIPTS_PENDING")
    poly_source = contract["approved_external_sources"][0]
    if (
        not poly_haven.get("cryptographic_source_to_runtime_closure_complete")
        and poly_source["derived_runtime_provenance_status"]
        == "INCOMPLETE_NO_DURABLE_SOURCE_TO_UASSET_RECEIPT"
        and poly_source["release_derived_outputs_allowed"] is False
    ):
        blockers.append("POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE")
    if staged_package.get("implemented") is False:
        blockers.append("ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED")
    if contract["policy"]["manual_visual_brand_review_required"] is True:
        blockers.append("MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED")
    return blockers


def validate_project(contract: dict[str, Any]) -> tuple[list[str], dict[str, Any], list[str]]:
    errors = _validate_contract_shape(contract)
    if errors:
        return errors, {}, []
    checks: dict[str, Any] = {}
    checks["generic_and_public_text"] = _validate_generic_and_public_text(contract, errors)
    checks["forest_reference_ids"] = _validate_forest_ids(contract, errors)
    checks["packaging_never_cook"] = _validate_never_cook(contract, errors)
    checks["quarantined_imports"] = _validate_quarantines(contract, errors)
    checks["poly_haven"] = _validate_poly_haven(contract, errors)
    checks["development_only_content"] = _validate_development_manifests(contract, errors)
    checks["metahuman_runtime_cook"] = _validate_metahuman_and_runtime_cook(contract, errors)
    checks["framework_license"] = _validate_framework_license(contract, errors)
    checks["staged_package_provenance"] = _validate_staged_package_provenance(contract)
    blockers = _observed_blockers(
        contract,
        checks["metahuman_runtime_cook"],
        checks["framework_license"],
        checks["poly_haven"],
        checks["staged_package_provenance"],
    )
    if blockers != contract["release_blockers"]:
        errors.append(
            "declared release blockers differ from observed blockers: "
            f"declared={contract['release_blockers']!r} observed={blockers!r}"
        )
    return errors, checks, blockers


def validate_repository(
    root: Path = ROOT,
) -> tuple[list[str], dict[str, Any], list[str]]:
    """Run the complete Session 9 gate without writing an audit report."""
    global ROOT, ROOT_RESOLVED, CONTRACT_PATH, REPORT_PATH
    previous = (ROOT, ROOT_RESOLVED, CONTRACT_PATH, REPORT_PATH)
    try:
        ROOT = Path(root).resolve()
        ROOT_RESOLVED = ROOT
        CONTRACT_PATH = ROOT / "Config/DG_BrandLicenseContract.json"
        REPORT_PATH = ROOT / "Saved/BrandLicense/LatestBrandLicenseAudit.json"
        try:
            loaded = _strict_json_file(CONTRACT_PATH, "Session 9 contract")
        except StrictJsonError as exc:
            return [str(exc)], {}, []
        if type(loaded) is not dict:
            return ["Session 9 contract root must be an object"], {}, []
        return validate_project(loaded)
    finally:
        ROOT, ROOT_RESOLVED, CONTRACT_PATH, REPORT_PATH = previous


def _run_self_tests() -> list[str]:
    failures: list[str] = []

    def expect_json_failure(name: str, text: str) -> None:
        try:
            _strict_json_text(text, name)
        except StrictJsonError:
            return
        failures.append(f"{name}: strict JSON unexpectedly accepted adversarial input")

    expect_json_failure("duplicate-key", '{"x":1,"x":2}')
    expect_json_failure("nan", '{"x":NaN}')
    expect_json_failure("infinity", '{"x":Infinity}')
    expect_json_failure("integer-overflow", '{"x":9223372036854775808}')
    expect_json_failure("float-overflow", '{"x":1e309}')

    try:
        canonical = _strict_json_file(CONTRACT_PATH, "Session 9 contract")
    except StrictJsonError as exc:
        return failures + [f"canonical contract could not seed self-tests: {exc}"]
    if _validate_contract_shape(canonical):
        failures.append("canonical contract does not pass shape validation")
        return failures

    mutations: list[tuple[str, Any]] = []
    value = copy.deepcopy(canonical)
    value["unexpected"] = True
    mutations.append(("unknown-field", value))
    value = copy.deepcopy(canonical)
    value["brands"][1]["brand_id"] = "DG_GENERIC"
    mutations.append(("casefold-duplicate-id", value))
    value = copy.deepcopy(canonical)
    value["quarantined_imports"][0]["disk_root"] = "../outside"
    mutations.append(("path-traversal", value))
    value = copy.deepcopy(canonical)
    value["quarantined_imports"][0]["expected_total_bytes"] = -1
    mutations.append(("negative-inventory", value))
    value = copy.deepcopy(canonical)
    value["brands"][1]["status"] = "APPROVED"
    mutations.append(("premium-reapproval-without-evidence", value))
    value = copy.deepcopy(canonical)
    value["scan_contract"]["required_never_cook_roots"].pop()
    mutations.append(("removed-never-cook-root", value))
    value = copy.deepcopy(canonical)
    value["approved_external_sources"][0]["release_derived_outputs_allowed"] = True
    mutations.append(("unsupported-derived-runtime-release", value))
    value = copy.deepcopy(canonical)
    value["policy"]["source_gate_covers_every_staged_file"] = True
    mutations.append(("false-staged-package-closure", value))
    value = copy.deepcopy(canonical)
    value["scan_contract"]["runtime_text_roots"] = 7
    mutations.append(("wrong-nested-type", value))
    value = copy.deepcopy(canonical)
    value["release_blockers"].remove(
        "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE"
    )
    mutations.append(("removed-derived-provenance-blocker", value))
    for name, mutated in mutations:
        try:
            mutation_errors = _validate_contract_shape(mutated)
        except Exception as exc:  # pragma: no cover - diagnostic guard
            failures.append(f"{name}: validator crashed on adversarial mutation: {exc}")
            continue
        if not mutation_errors:
            failures.append(f"{name}: contract shape unexpectedly accepted adversarial mutation")
    return failures


def _write_report(report: dict[str, Any]) -> None:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--require-release-ready",
        action="store_true",
        help="Return nonzero when the valid contract still has release blockers.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run strict-parser and contract-mutation adversarial tests only.",
    )
    parser.add_argument(
        "--no-report",
        action="store_true",
        help="Do not write Saved/BrandLicense/LatestBrandLicenseAudit.json.",
    )
    args = parser.parse_args(argv)

    if args.self_test:
        failures = _run_self_tests()
        if failures:
            print("SESSION 9 BRAND/LICENSE SELF-TEST FAIL", file=sys.stderr)
            for failure in failures:
                print(f"  - {failure}", file=sys.stderr)
            return 1
        print("SESSION 9 BRAND/LICENSE SELF-TEST PASS: adversarial_cases=15")
        return 0

    contract: dict[str, Any] = {}
    parse_errors: list[str] = []
    try:
        loaded = _strict_json_file(CONTRACT_PATH, "Session 9 contract")
        if type(loaded) is not dict:
            parse_errors.append("Session 9 contract root must be an object")
        else:
            contract = loaded
    except StrictJsonError as exc:
        parse_errors.append(str(exc))

    checks: dict[str, Any] = {}
    blockers: list[str] = []
    errors = parse_errors
    if not errors:
        errors, checks, blockers = validate_project(contract)
    release_ready = not errors and not blockers
    status = (
        "FAIL_CONTRACT"
        if errors
        else "PASS_RELEASE_READY"
        if release_ready
        else contract["normal_status"]
    )
    report = {
        "schema": "DiscGolfTour.Session9BrandLicenseAudit.v1",
        "schema_version": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": status,
        "audit_scope": contract.get("audit_scope"),
        "contract": CONTRACT_PATH.relative_to(ROOT).as_posix(),
        "contract_sha256": _sha256(CONTRACT_PATH) if CONTRACT_PATH.is_file() else None,
        "contract_valid": not errors,
        "release_ready": release_ready,
        "release_status": contract.get("release_status"),
        "require_release_ready": args.require_release_ready,
        "checks": checks,
        "release_blockers": blockers,
        "errors": errors,
    }
    if not args.no_report:
        _write_report(report)

    print(
        f"SESSION 9 BRAND/LICENSE {status}: "
        f"contract_valid={str(not errors).lower()} "
        f"release_ready={str(release_ready).lower()} "
        f"blockers={len(blockers)} errors={len(errors)} "
        f"report={'DISABLED' if args.no_report else REPORT_PATH}"
    )
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    if errors:
        return 1
    if args.require_release_ready and not release_ready:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
