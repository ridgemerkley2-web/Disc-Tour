#!/usr/bin/env python3
"""Fail-closed Session 18 Poly Haven source-to-runtime provenance gate."""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
ROOT_RESOLVED = ROOT.resolve()
CONTRACT_PATH = ROOT / "Config/DG_Session18PolyHavenProvenanceContract.json"
RECEIPT_PATH = ROOT / "SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"
REPORT_PATH = ROOT / "Saved/Provenance/LatestSession18PolyHavenProvenance.json"

SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z$")

SOURCE_MANIFEST_PATH = "SourceArt/PineRidge/PolyHaven/asset_manifest.json"
SOURCE_MANIFEST_SHA256 = "880C79DD7F9151DAD0D8344C0E505CBB1E3AC98D06C955E9DACD6E305BCBD250"
IMPORTER_PATH = "Scripts/import-pine-ridge-assets.py"
IMPORTER_BYTES = 108495
IMPORTER_SHA256 = "A0B2A0F39F0921ED0AC2E3E884C793428CA0FAF192635B125A0EF227A929A8D6"
RECEIPT_BYTES = 88601
RECEIPT_SHA256 = "13ACC125F5B442BE75DE405DBD76D43AA9381436624E12BDDBA3C7BA4F90D858"
ENGINE_BUILD_SHA256 = "D35D3D5F4DE0636FBB62386B63DEE8056889FB2BAF199D012DC32EE77F7A2997"
SEMANTIC_VALIDATOR_PATH = "Scripts/validate-pine-ridge-provenance-semantics.py"
SEMANTIC_VALIDATOR_BYTES = 7825
SEMANTIC_VALIDATOR_SHA256 = "D27C900033E2EA37A8429B37EFC044402AE8858B367DF030C2B48DD17B421150"
SEMANTIC_REPORT_PATH = "Evidence/Session18/PineRidgeSemanticValidation.json"
SEMANTIC_REPORT_BYTES = 34478
SEMANTIC_REPORT_SHA256 = "1C3D6960E342B58143D55B42D701B18D19E17BFE8D118862BE38A93A15B74035"
PACKAGE_IDENTITY_RECEIPT_PATH = "Evidence/Session18/PolyHavenPackageIdentityReceipt.json"
PACKAGE_IDENTITY_RECEIPT_BYTES = 3830
PACKAGE_IDENTITY_RECEIPT_SHA256 = "6413B0B72C9E2290FA82B03D1E1A075A182A5FA949D5F9A29E08175F1121CC27"
PACKAGE_UFS_MANIFEST_PATH = "Evidence/Session18/Manifest_UFSFiles_Win64.txt"
PACKAGE_UFS_MANIFEST_BYTES = 300128
PACKAGE_UFS_MANIFEST_SHA256 = "C94C9CE041B3E82813439818204B7A1DE2560120E6FEB845F864110EC612F747"
PACKAGE_BUILD_LOG_PATH = "Evidence/Session18/BuildCookRun.log"
PACKAGE_BUILD_LOG_BYTES = 329045
PACKAGE_BUILD_LOG_SHA256 = "72D74C2B1212EE3720CDBF5B799EE57337A949FB22845E1207421CC084791D27"
PACKAGE_BUILD_RAW_LOG_BYTES = 329318
PACKAGE_BUILD_RAW_LOG_SHA256 = "AC9A98A74DA333DEFA31C3C96D075450CB4E0184C26C928924E0D7DCE8516902"
PACKAGE_ARCHIVE_MANIFEST_PATH = "Evidence/Session18/ArchiveFiles.tsv"
PACKAGE_ARCHIVE_MANIFEST_BYTES = 6753
PACKAGE_ARCHIVE_MANIFEST_SHA256 = "0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870"
PACKAGE_SMOKE_LOG_PATH = "Evidence/Session18/PackagedPineRidgePlaySmoke.log"
PACKAGE_SMOKE_LOG_BYTES = 69652
PACKAGE_SMOKE_LOG_SHA256 = "5A5D13A776B9BF337B12FF1580E6E474799F6ADAE4AEF3E0B5E01EBB55176251"
PACKAGE_SMOKE_RAW_LOG_BYTES = 69655
PACKAGE_SMOKE_RAW_LOG_SHA256 = "8C7DC2123F7C054E05E57DF43536C6E95EDDD43FA8559E81A016148D3D4C5D15"

POLY_BLOCKER = "POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE"
HISTORICAL_BLOCKERS = [
    "FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED",
    "EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE",
    "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
    "DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE",
    "QUARANTINED_IMPORT_RECEIPTS_PENDING",
    POLY_BLOCKER,
    "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED",
    "MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED",
    "SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY",
    "SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING",
    "SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING",
    "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING",
    "SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING",
    "SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING",
]
REMAINING_BLOCKERS = [item for item in HISTORICAL_BLOCKERS if item != POLY_BLOCKER]

TEXTURE_DESTINATIONS = {
    "Textures/ForestGround01": "Content/Presentation/Course/PineRidge/Textures/ForestGround",
    "Textures/LeafyGrass": "Content/Presentation/Course/PineRidge/Textures/LeafyGrass",
    "Textures/GrassPath2": "Content/Presentation/Course/PineRidge/Textures/GrassPath",
    "Models/FirSapling/Textures": "Content/Presentation/Course/PineRidge/Textures/FirSapling",
    "Models/Boulder01/Textures": "Content/Presentation/Course/PineRidge/Textures/Boulder01",
    "Models/Shrub04/Textures": "Content/Presentation/Course/PineRidge/Textures/Shrub04",
    "Textures/WeatheredPlanks": "Content/Presentation/Course/PineRidge/Textures/WeatheredPlanks",
}
MESH_OUTPUTS = {
    "SourceArt/PineRidge/PolyHaven/Models/FirSapling/fir_sapling_1k.fbx": [
        "Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.uasset",
        "Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_b.uasset",
        "Content/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_c.uasset",
    ],
    "SourceArt/PineRidge/PolyHaven/Models/Boulder01/boulder_01_1k.fbx": [
        "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.uasset",
    ],
    "SourceArt/PineRidge/PolyHaven/Models/Shrub04/shrub_04_1k.fbx": [
        "Content/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.uasset",
    ],
}
MESH_MATERIAL_DEPENDENCIES = {
    "fir_sapling": [
        "Content/Presentation/Course/PineRidge/Materials/MI_FirSaplingBranches.uasset",
        "Content/Presentation/Course/PineRidge/Materials/MI_FirSaplingTwigs.uasset",
    ],
    "boulder_01": ["Content/Presentation/Course/PineRidge/Materials/MI_Boulder01.uasset"],
    "shrub_04": ["Content/Presentation/Course/PineRidge/Materials/MI_Shrub04.uasset"],
}
MATERIAL_SPECS = {
    "M_PineRidgeTerrain": ([('forrest_ground_01', None)], []),
    "M_PineRidgeFairwayBlend": ([('leafy_grass', None), ('forrest_ground_01', None)], []),
    "M_PineRidgeTrailBlend": ([('grass_path_2', None), ('forrest_ground_01', None)], []),
    "MI_PineRidgeFairway": ([('leafy_grass', None)], ["M_PineRidgeTerrain"]),
    "MI_PineRidgeForestFloor": ([('forrest_ground_01', None)], ["M_PineRidgeTerrain"]),
    "MI_PineRidgePath": ([('grass_path_2', None)], ["M_PineRidgeTerrain"]),
    "MI_PineRidgeTrailWear": ([('grass_path_2', None)], ["M_PineRidgeTerrain"]),
    "MI_PineRidgeShoreReeds": ([('leafy_grass', None)], ["M_PineRidgeTerrain"]),
    "M_PineRidgeBark": ([('fir_sapling', {
        "BranchesDiffuse1K", "BranchesNormalDX1K", "BranchesRoughness1K"
    })], []),
    "M_PineRidgeNeedles": ([('fir_sapling', {
        "TwigsDiffuse1K", "TwigsNormalDX1K", "TwigsRoughness1K", "TwigsOpacity1K"
    })], []),
    "MI_FirSaplingBranches": ([('fir_sapling', {
        "BranchesDiffuse1K", "BranchesNormalDX1K", "BranchesRoughness1K"
    })], ["M_PineRidgeBark"]),
    "MI_FirSaplingTwigs": ([('fir_sapling', {
        "TwigsDiffuse1K", "TwigsNormalDX1K", "TwigsRoughness1K", "TwigsOpacity1K"
    })], ["M_PineRidgeNeedles"]),
    "M_PineRidgeRock": ([('boulder_01', None)], []),
    "MI_Boulder01": ([('boulder_01', None)], ["M_PineRidgeRock"]),
    "M_PineRidgeBrush": ([('shrub_04', None)], []),
    "MI_Shrub04": ([('shrub_04', None)], ["M_PineRidgeBrush"]),
    "M_PineRidgeSignWood": ([('weathered_planks', None)], []),
    "MI_PineRidgeSignWood": ([('weathered_planks', None)], ["M_PineRidgeSignWood"]),
}
EXCLUDED_SPECS = {
    "Content/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater.uasset": {
        "assetClass": "Material", "provenanceKind": "PROJECT_ORIGINAL_PROCEDURAL_MATERIAL",
        "projectInputs": ["Scripts/import-pine-ridge-assets.py"],
        "directProjectSourceRelativePaths": [], "runtimeDependencies": [], "semanticSettings": {},
    },
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeGrassBlade.uasset": {
        "assetClass": "Material", "provenanceKind": "PROJECT_ORIGINAL_GENERATED_MASK_MATERIAL",
        "projectInputs": ["Scripts/import-pine-ridge-assets.py",
                          "SourceArt/PineRidge/Generated/pine_ridge_grass_card_alpha.png",
                          "SourceArt/PineRidge/Generated/pine_ridge_grass_card_alpha.png.base64"],
        "directProjectSourceRelativePaths": [],
        "runtimeDependencies": [
            "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_grass_card_alpha.uasset"],
        "semanticSettings": {},
    },
    "Content/Presentation/Course/PineRidge/Materials/M_PineRidgeLeafLitter.uasset": {
        "assetClass": "Material", "provenanceKind": "PROJECT_ORIGINAL_GENERATED_MASK_MATERIAL",
        "projectInputs": ["Scripts/import-pine-ridge-assets.py",
                          "SourceArt/PineRidge/Generated/pine_ridge_litter_card_alpha.png",
                          "SourceArt/PineRidge/Generated/pine_ridge_litter_card_alpha.png.base64"],
        "directProjectSourceRelativePaths": [],
        "runtimeDependencies": [
            "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_litter_card_alpha.uasset"],
        "semanticSettings": {},
    },
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_grass_card_alpha.uasset": {
        "assetClass": "Texture2D", "provenanceKind": "PROJECT_ORIGINAL_GENERATED_TEXTURE",
        "projectInputs": ["SourceArt/PineRidge/Generated/pine_ridge_grass_card_alpha.png",
                          "SourceArt/PineRidge/Generated/pine_ridge_grass_card_alpha.png.base64"],
        "directProjectSourceRelativePaths": [
            "SourceArt/PineRidge/Generated/pine_ridge_grass_card_alpha.png"],
        "runtimeDependencies": [],
        "semanticSettings": {
            "compression": "<TextureCompressionSettings.TC_MASKS: 2>", "srgb": False,
            "filter": "<TextureFilter.TF_NEAREST: 0>",
            "mipGenSettings": "<TextureMipGenSettings.TMGS_NO_MIPMAPS: 13>"},
    },
    "Content/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_litter_card_alpha.uasset": {
        "assetClass": "Texture2D", "provenanceKind": "PROJECT_ORIGINAL_GENERATED_TEXTURE",
        "projectInputs": ["SourceArt/PineRidge/Generated/pine_ridge_litter_card_alpha.png",
                          "SourceArt/PineRidge/Generated/pine_ridge_litter_card_alpha.png.base64"],
        "directProjectSourceRelativePaths": [
            "SourceArt/PineRidge/Generated/pine_ridge_litter_card_alpha.png"],
        "runtimeDependencies": [],
        "semanticSettings": {
            "compression": "<TextureCompressionSettings.TC_MASKS: 2>", "srgb": False,
            "filter": "<TextureFilter.TF_NEAREST: 0>",
            "mipGenSettings": "<TextureMipGenSettings.TMGS_NO_MIPMAPS: 13>"},
    },
}

EXPECTED_COUNTS = {
    "sourceFiles": 35,
    "directTextureImports": 32,
    "directStaticMeshImports": 5,
    "dependentDerivatives": 18,
    "polyHavenDerived": 55,
    "excludedProjectOriginal": 5,
    "runtimeRootTotal": 60,
}
EXPECTED_OPTIONS = {
    "assetTask": {
        "automated": True, "replaceExisting": True,
        "replaceExistingSettings": True, "save": True,
    },
    "polyTextures": {
        "color": {"compression": "TC_DEFAULT", "srgb": True},
        "normal": {"compression": "TC_NORMALMAP", "srgb": False},
        "data": {"compression": "TC_MASKS", "srgb": False},
    },
    "generatedMasks": {
        "compression": "TC_MASKS", "srgb": False, "filter": "TF_NEAREST",
        "mipGenSettings": "TMGS_NO_MIPMAPS",
    },
    "staticMeshes": {
        "importAsSkeletal": False, "importMesh": True,
        "importMaterials": False, "importTextures": False,
        "generateLightmapUVs": True, "autoGenerateCollision": False,
        "convertScene": True, "convertSceneUnit": True,
        "firCombineMeshes": False, "fixtureCombineMeshes": True,
        "firLods": [[1.0, 1.0], [0.2, 0.22], [0.05, 0.085], [0.0125, 0.03]],
        "boulderLods": [[1.0, 1.0], [0.18, 0.16], [0.035, 0.045]],
        "shrubLodCount": 1,
    },
    "materials": {"replaceExpressions": True, "saveAfterCompile": True},
}


class StrictJsonError(ValueError):
    pass


def _pairs_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_json_constant(value: str) -> Any:
    raise StrictJsonError(f"non-finite JSON number: {value}")


def _strict_json_float(value: str) -> float:
    parsed = float(value)
    if not math.isfinite(parsed):
        raise StrictJsonError(f"non-finite JSON number: {value}")
    return parsed


def _strict_json_int(value: str) -> int:
    parsed = int(value)
    if not -(2 ** 63) <= parsed <= 2 ** 63 - 1:
        raise StrictJsonError(f"JSON integer outside signed 64-bit range: {value}")
    return parsed


def load_json(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8-sig"), object_pairs_hook=_pairs_no_duplicates,
            parse_constant=_reject_json_constant, parse_float=_strict_json_float,
            parse_int=_strict_json_int,
        )
    except (OSError, UnicodeError, ValueError, OverflowError, json.JSONDecodeError,
            StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def md5(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def exact_keys(value: Any, expected: set[str], context: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{context} must be an object")
        return False
    actual = set(value)
    if actual != expected:
        issues.append(
            f"{context} keys differ: missing={sorted(expected - actual)} extra={sorted(actual - expected)}"
        )
        return False
    return True


def json_exact(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if type(expected) is dict:
        return set(actual) == set(expected) and all(
            json_exact(actual[key], value) for key, value in expected.items()
        )
    if type(expected) is list:
        return len(actual) == len(expected) and all(
            json_exact(left, right) for left, right in zip(actual, expected)
        )
    return actual == expected


def canonical_utc(value: Any) -> bool:
    if type(value) is not str or UTC_RE.fullmatch(value) is None:
        return False
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return parsed.tzinfo == timezone.utc and (
        parsed.isoformat(timespec="auto").replace("+00:00", "Z") == value
    )


def expect(condition: bool, message: str, issues: list[str]) -> None:
    if not condition:
        issues.append(message)


def repo_file(relative: Any, context: str, issues: list[str], root: Path = ROOT) -> Path | None:
    if type(relative) is not str or not relative or "\\" in relative or "\x00" in relative:
        issues.append(f"{context} is not a canonical repository-relative path")
        return None
    pure = PurePosixPath(relative)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        issues.append(f"{context} escapes the repository: {relative!r}")
        return None
    candidate = root.joinpath(*pure.parts)
    try:
        candidate.resolve().relative_to(root.resolve())
    except (OSError, ValueError):
        issues.append(f"{context} resolves outside the repository: {relative!r}")
        return None
    return candidate


def object_path(relative_path: str) -> str:
    package = "/Game/" + relative_path[len("Content/"):-len(".uasset")]
    return f"{package}.{Path(relative_path).stem}"


def texture_output(source_path: str) -> str:
    prefix = "SourceArt/PineRidge/PolyHaven/"
    inside = source_path[len(prefix):]
    parent = PurePosixPath(inside).parent.as_posix()
    return f"{TEXTURE_DESTINATIONS[parent]}/{PurePosixPath(source_path).stem}.uasset"


def expected_artifact_mappings(manifest_files: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    by_asset: dict[str, list[str]] = {}
    texture_outputs: dict[str, list[str]] = {}
    by_path = {item["relativePath"]: item for item in manifest_files}
    for item in manifest_files:
        by_asset.setdefault(item["assetId"], []).append(item["relativePath"])
        if item["role"] != "StaticMeshFBX1K":
            texture_outputs.setdefault(item["assetId"], []).append(
                texture_output(item["relativePath"])
            )
    for values in by_asset.values():
        values.sort()
    for values in texture_outputs.values():
        values.sort()

    expected: dict[str, dict[str, Any]] = {}
    for item in manifest_files:
        if item["role"] == "StaticMeshFBX1K":
            continue
        output = texture_output(item["relativePath"])
        expected[output] = {
            "assetClass": "Texture2D",
            "provenanceKind": "DIRECT_TEXTURE_IMPORT",
            "directSourceRelativePaths": [item["relativePath"]],
            "runtimeDependencies": [],
            "semanticSettings": {
                "compression": (
                    "<TextureCompressionSettings.TC_DEFAULT: 0>" if "Diffuse" in item["role"] else
                    "<TextureCompressionSettings.TC_NORMALMAP: 1>" if "Normal" in item["role"] else
                    "<TextureCompressionSettings.TC_MASKS: 2>"
                ),
                "srgb": "Diffuse" in item["role"],
            },
            "transitiveSourceRelativePaths": [item["relativePath"]],
        }

    for source_path, outputs in MESH_OUTPUTS.items():
        asset_id = by_path[source_path]["assetId"]
        for output in outputs:
            expected[output] = {
                "assetClass": "StaticMesh",
                "provenanceKind": "DIRECT_STATIC_MESH_IMPORT",
                "directSourceRelativePaths": [source_path],
                "runtimeDependencies": sorted(MESH_MATERIAL_DEPENDENCIES[asset_id]),
                "semanticSettings": {
                    "lodCount": 4 if asset_id == "fir_sapling" else 3 if asset_id == "boulder_01" else 1
                },
                "transitiveSourceRelativePaths": sorted(by_asset[asset_id]),
            }

    material_transitive_sources: dict[str, list[str]] = {}
    for name, (selectors, parents) in MATERIAL_SPECS.items():
        path = f"Content/Presentation/Course/PineRidge/Materials/{name}.uasset"
        selected = [
            item for asset_id, roles in selectors for item in manifest_files
            if item["assetId"] == asset_id and item["role"] != "StaticMeshFBX1K"
            and (roles is None or item["role"] in roles)
        ]
        sources = sorted(
            {item["relativePath"] for item in selected}.union(*(
                set(material_transitive_sources[parent]) for parent in parents
            ))
        )
        material_transitive_sources[name] = sources
        dependencies = sorted({
            texture_output(item["relativePath"]) for item in selected
        } | {
            f"Content/Presentation/Course/PineRidge/Materials/{parent}.uasset"
            for parent in parents
        })
        expected[path] = {
            "assetClass": "MaterialInstanceConstant" if name.startswith("MI_") else "Material",
            "provenanceKind": "AUTHORED_UNREAL_ASSET_WITH_POLY_HAVEN_DEPENDENCY",
            "directSourceRelativePaths": [],
            "runtimeDependencies": dependencies,
            "semanticSettings": {},
            "transitiveSourceRelativePaths": sources,
        }
    return expected


def validate_receipt(receipt: Any, root: Path = ROOT) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    summary: dict[str, Any] = {}
    root_keys = {
        "schema", "schemaVersion", "sourceId", "status", "completedUtc", "license",
        "sourceManifest", "importExecution", "sourceFiles", "projectInputs",
        "derivedRuntimeArtifacts", "excludedProjectOriginalRuntimeArtifacts",
        "expectedCounts", "closureAssertions",
    }
    if not exact_keys(receipt, root_keys, "receipt", issues):
        return issues, summary
    expect(receipt["schema"] == "DiscGolfTour.PineRidgeDerivedAssetReceipt.v1",
           "receipt schema differs", issues)
    expect(type(receipt["schemaVersion"]) is int and receipt["schemaVersion"] == 1,
           "receipt schemaVersion differs", issues)
    expect(receipt["sourceId"] == "poly_haven_pine_ridge_cc0", "sourceId differs", issues)
    expect(receipt["status"] == "COMPLETE_DURABLE_SOURCE_TO_UASSET_RECEIPT",
           "receipt status differs", issues)
    expect(canonical_utc(receipt["completedUtc"]),
           "completedUtc is not canonical UTC", issues)
    expect(json_exact(receipt["license"], {
        "spdxExpression": "CC0-1.0", "url": "https://polyhaven.com/license",
        "attributionRequired": False,
    }), "license declaration differs", issues)

    manifest_contract = receipt["sourceManifest"]
    if not exact_keys(manifest_contract, {"relativePath", "byteSize", "sha256", "fileCount"},
                      "sourceManifest", issues):
        return issues, summary
    expect(json_exact(manifest_contract, {
        "relativePath": SOURCE_MANIFEST_PATH,
        "byteSize": (root / SOURCE_MANIFEST_PATH).stat().st_size,
        "sha256": SOURCE_MANIFEST_SHA256,
        "fileCount": 35,
    }), "sourceManifest binding differs", issues)

    try:
        manifest_payload = load_json(root / SOURCE_MANIFEST_PATH)
    except StrictJsonError as exc:
        issues.append(str(exc))
        return issues, summary
    manifest_files = manifest_payload.get("files", []) if type(manifest_payload) is dict else []
    expect(len(manifest_files) == 35, "source manifest does not contain 35 files", issues)
    expected_source_records = []
    manifest_source_paths: set[str] = set()
    for index, item in enumerate(manifest_files):
        if type(item) is not dict:
            issues.append(f"manifest source[{index}] is not an object")
            continue
        path = repo_file(item.get("relativePath"), f"manifest source[{index}]", issues, root)
        if path is None or not path.is_file():
            issues.append(f"manifest source[{index}] is missing")
            continue
        source_path = item["relativePath"]
        expect(source_path.startswith("SourceArt/PineRidge/PolyHaven/"),
               f"manifest source leaves Poly Haven root: {source_path}", issues)
        expect(source_path.casefold() not in {value.casefold() for value in manifest_source_paths},
               f"duplicate/case-colliding source path: {source_path}", issues)
        manifest_source_paths.add(source_path)
        expect(path.stat().st_size == item.get("byteSize"), f"source size drifted: {source_path}", issues)
        expect(type(item.get("md5")) is str and MD5_RE.fullmatch(item["md5"]) is not None,
               f"source MD5 format differs: {source_path}", issues)
        expect(md5(path) == item.get("md5"), f"source MD5 drifted: {source_path}", issues)
        expected_source_records.append({
            "assetId": item.get("assetId"), "role": item.get("role"),
            "relativePath": source_path, "byteSize": item.get("byteSize"),
            "md5": item.get("md5"), "sha256": sha256(path),
        })
    expected_source_records.sort(key=lambda value: value["relativePath"].casefold())
    expect(json_exact(receipt["sourceFiles"], expected_source_records),
           "receipt sourceFiles differ from the live 35-file manifest closure", issues)

    execution = receipt["importExecution"]
    execution_keys = {
        "engineVersion", "engineChangelist", "engineCompatibleChangelist", "engineBranch",
        "engineBuildFileSha256", "importerRelativePath", "importerByteSize", "importerSha256",
        "contentRoot", "hostPathsRecorded", "secretsRecorded", "pinnedImportOptions",
    }
    if not exact_keys(execution, execution_keys, "importExecution", issues):
        return issues, summary
    expect(json_exact({key: execution.get(key) for key in execution_keys - {"pinnedImportOptions"}}, {
        "engineVersion": "5.8.1", "engineChangelist": 56057345,
        "engineCompatibleChangelist": 55116800, "engineBranch": "++UE5+Release-5.8",
        "engineBuildFileSha256": ENGINE_BUILD_SHA256, "importerRelativePath": IMPORTER_PATH,
        "importerByteSize": IMPORTER_BYTES, "importerSha256": IMPORTER_SHA256,
        "contentRoot": "/Game/Presentation/Course/PineRidge",
        "hostPathsRecorded": False, "secretsRecorded": False,
    }), "import execution identity differs", issues)
    expect(json_exact(execution.get("pinnedImportOptions"), EXPECTED_OPTIONS),
           "pinned import options differ", issues)
    importer = root / IMPORTER_PATH
    expect(importer.is_file() and importer.stat().st_size == IMPORTER_BYTES and
           sha256(importer) == IMPORTER_SHA256, "live importer bytes/hash drifted", issues)

    expected_project_inputs = []
    project_input_paths = sorted({
        path for spec in EXCLUDED_SPECS.values() for path in spec["projectInputs"]
    })
    for relative in project_input_paths:
        path = repo_file(relative, "project input", issues, root)
        if path is not None and path.is_file():
            expected_project_inputs.append({
                "relativePath": relative, "byteSize": path.stat().st_size, "sha256": sha256(path)
            })
    expect(json_exact(receipt["projectInputs"], expected_project_inputs),
           "projectInputs differ", issues)

    expected_mappings = expected_artifact_mappings(manifest_files)
    derived = receipt["derivedRuntimeArtifacts"]
    expect(type(derived) is list and len(derived) == 55,
           "derivedRuntimeArtifacts must contain exactly 55 entries", issues)
    derived_paths: list[str] = []
    mapped_sources: set[str] = set()
    if type(derived) is list:
        for index, item in enumerate(derived):
            item_keys = {
                "relativePath", "objectPath", "assetClass", "provenanceKind", "byteSize",
                "sha256", "directSourceRelativePaths", "runtimeDependencies",
                "semanticSettings", "transitiveSourceRelativePaths",
            }
            if not exact_keys(item, item_keys, f"derived[{index}]", issues):
                continue
            relative = item["relativePath"]
            path = repo_file(relative, f"derived[{index}].relativePath", issues, root)
            if type(relative) is str:
                derived_paths.append(relative)
            expected = expected_mappings.get(relative)
            expect(expected is not None, f"unexpected derived artifact: {relative}", issues)
            if expected is not None:
                for key, value in expected.items():
                    expect(json_exact(item.get(key), value),
                           f"derived mapping differs for {relative}.{key}", issues)
                expect(item.get("objectPath") == object_path(relative),
                       f"objectPath differs for {relative}", issues)
            expect(type(item.get("sha256")) is str and SHA256_RE.fullmatch(item["sha256"]) is not None,
                   f"derived SHA format differs: {relative}", issues)
            if path is None or not path.is_file():
                issues.append(f"derived artifact missing: {relative}")
            else:
                expect(path.stat().st_size == item.get("byteSize"),
                       f"derived size drifted: {relative}", issues)
                expect(sha256(path) == item.get("sha256"), f"derived SHA drifted: {relative}", issues)
            transitive = item.get("transitiveSourceRelativePaths")
            if type(transitive) is list:
                mapped_sources.update(value for value in transitive if type(value) is str)
    expect(derived_paths == sorted(derived_paths, key=str.casefold),
           "derived artifacts are not canonically ordered", issues)
    expect(len({value.casefold() for value in derived_paths}) == len(derived_paths),
           "derived artifact paths duplicate/case-collide", issues)
    expect(set(derived_paths) == set(expected_mappings), "derived artifact set is incomplete", issues)
    expect(mapped_sources == manifest_source_paths, "not every source file maps to runtime output", issues)

    excluded = receipt["excludedProjectOriginalRuntimeArtifacts"]
    expect(type(excluded) is list and len(excluded) == 5,
           "excludedProjectOriginalRuntimeArtifacts must contain five entries", issues)
    excluded_paths: list[str] = []
    if type(excluded) is list:
        for index, item in enumerate(excluded):
            item_keys = {
                "relativePath", "objectPath", "assetClass", "provenanceKind",
                "byteSize", "sha256", "directProjectSourceRelativePaths",
                "runtimeDependencies", "semanticSettings", "projectInputs",
            }
            if not exact_keys(item, item_keys, f"excluded[{index}]", issues):
                continue
            relative = item["relativePath"]
            path = repo_file(relative, f"excluded[{index}].relativePath", issues, root)
            if type(relative) is str:
                excluded_paths.append(relative)
            expected = EXCLUDED_SPECS.get(relative)
            expect(expected is not None, f"unexpected excluded artifact: {relative}", issues)
            if expected is not None:
                for key in ("assetClass", "provenanceKind", "directProjectSourceRelativePaths",
                            "runtimeDependencies", "semanticSettings"):
                    expect(json_exact(item.get(key), expected[key]),
                           f"excluded {key} differs: {relative}", issues)
                expect(item.get("objectPath") == object_path(relative),
                       f"excluded objectPath differs: {relative}", issues)
                expected_input_records = [
                    record for record in expected_project_inputs
                    if record["relativePath"] in expected["projectInputs"]
                ]
                expect(json_exact(item.get("projectInputs"), expected_input_records),
                       f"excluded projectInputs differ: {relative}", issues)
            expect(type(item.get("sha256")) is str and SHA256_RE.fullmatch(item["sha256"]) is not None,
                   f"excluded SHA format differs: {relative}", issues)
            if path is None or not path.is_file():
                issues.append(f"excluded runtime artifact missing: {relative}")
            else:
                expect(path.stat().st_size == item.get("byteSize"),
                       f"excluded size drifted: {relative}", issues)
                expect(sha256(path) == item.get("sha256"), f"excluded SHA drifted: {relative}", issues)
    expect(excluded_paths == sorted(excluded_paths, key=str.casefold),
           "excluded artifacts are not canonically ordered", issues)
    expect(set(excluded_paths) == set(EXCLUDED_SPECS), "excluded artifact set differs", issues)
    expect(set(derived_paths).isdisjoint(excluded_paths), "runtime partition overlaps", issues)

    runtime_root = root / "Content/Presentation/Course/PineRidge"
    observed = sorted(
        path.relative_to(root).as_posix() for path in runtime_root.rglob("*.uasset") if path.is_file()
    )
    expect(observed == sorted(derived_paths + excluded_paths),
           "60-file Pine Ridge runtime-root partition is incomplete", issues)
    expect(json_exact(receipt["expectedCounts"], EXPECTED_COUNTS), "expectedCounts differ", issues)
    expect(json_exact(receipt["closureAssertions"], {
        "allSourceFilesHashVerified": True,
        "allSourceFilesMapped": True,
        "allDerivedRuntimeArtifactsHashBound": True,
        "runtimeRootPartitionComplete": True,
        "technicalSourceToRuntimeClosureComplete": True,
        "actualStagedPackageProvenanceClosed": False,
    }), "closureAssertions differ", issues)
    summary.update({
        "source_file_count": len(expected_source_records),
        "derived_runtime_artifact_count": len(derived_paths),
        "excluded_project_original_count": len(excluded_paths),
        "runtime_root_total": len(observed),
        "derived_runtime_bytes": sum(
            item.get("byteSize", 0) for item in derived if type(item) is dict
        ) if type(derived) is list else 0,
    })
    return issues, summary


def validate_contract(contract: Any, root: Path = ROOT) -> list[str]:
    issues: list[str] = []
    keys = {
        "schema", "schemaVersion", "session", "status", "historicalReleaseBlockers",
        "resolvedReleaseBlockers", "remainingReleaseBlockers", "closureEvidence",
        "packageIdentityEvidence", "continuity", "unresolvedBoundaries", "exitGate",
    }
    if not exact_keys(contract, keys, "contract", issues):
        return issues
    expect(contract["schema"] == "DiscGolfTour.Session18PolyHavenProvenanceContract.v1",
           "contract schema differs", issues)
    expect(type(contract["schemaVersion"]) is int and contract["schemaVersion"] == 1,
           "contract schemaVersion differs", issues)
    expect(type(contract["session"]) is int and contract["session"] == 18,
           "contract session differs", issues)
    expect(contract["status"] == "TECHNICALLY_COMPLETE_RELEASE_BLOCKED",
           "contract status differs", issues)
    expect(contract["historicalReleaseBlockers"] == HISTORICAL_BLOCKERS,
           "historical 14-blocker ledger differs", issues)
    expect(contract["resolvedReleaseBlockers"] == [POLY_BLOCKER],
           "resolved blocker set differs", issues)
    expect(contract["remainingReleaseBlockers"] == REMAINING_BLOCKERS,
           "remaining 13-blocker ledger differs", issues)

    closure = contract["closureEvidence"]
    expected_closure = {
        "sourceId": "poly_haven_pine_ridge_cc0",
        "receiptPath": RECEIPT_PATH.relative_to(ROOT).as_posix(),
        "receiptBytes": RECEIPT_BYTES,
        "receiptSha256": RECEIPT_SHA256,
        "sourceManifestPath": SOURCE_MANIFEST_PATH,
        "sourceManifestSha256": SOURCE_MANIFEST_SHA256,
        "importerPath": IMPORTER_PATH,
        "importerBytes": IMPORTER_BYTES,
        "importerSha256": IMPORTER_SHA256,
        "engineVersion": "5.8.1",
        "engineChangelist": 56057345,
        "controlledReimportRunId": "1a0b2284-5196-487c-9a65-17b288ed0768",
        "controlledReimportLog": "Evidence/Session18/PolyHavenReimport.log",
        "controlledReimportLogBytes": 535486,
        "controlledReimportLogSha256": "9683A9518A9D034E0C4CCD2F705C54D330F8F312314D203FA9480C964BB40F01",
        "controlledReimportRawLogBytes": 535552,
        "controlledReimportRawLogSha256":
            "082FC4212BCA3FDCD3AF4019C8BEFE03F1BFD55CE375C1D40F2D4C3B04C8433A",
        "semanticValidatorPath": SEMANTIC_VALIDATOR_PATH,
        "semanticValidatorBytes": SEMANTIC_VALIDATOR_BYTES,
        "semanticValidatorSha256": SEMANTIC_VALIDATOR_SHA256,
        "semanticValidationRunId": "a4e81657-0ebc-42a2-a6e9-06858e10b1d3",
        "semanticReportPath": SEMANTIC_REPORT_PATH,
        "semanticReportBytes": SEMANTIC_REPORT_BYTES,
        "semanticReportSha256": SEMANTIC_REPORT_SHA256,
        "expectedCounts": EXPECTED_COUNTS,
        "contentRootPartitionComplete": True,
        "actualStagedPackageProvenanceClosed": False,
    }
    if not exact_keys(closure, set(expected_closure), "closureEvidence", issues):
        return issues
    expect(json_exact(closure, expected_closure),
           "closureEvidence differs from accepted reimport", issues)
    evidence_bindings = [
        ("receipt", "receiptPath", "receiptBytes", "receiptSha256"),
        ("sourceManifest", "sourceManifestPath", None, "sourceManifestSha256"),
        ("importer", "importerPath", "importerBytes", "importerSha256"),
        ("controlledReimportLog", "controlledReimportLog", "controlledReimportLogBytes",
         "controlledReimportLogSha256"),
        ("semanticValidator", "semanticValidatorPath", "semanticValidatorBytes",
         "semanticValidatorSha256"),
        ("semanticReport", "semanticReportPath", "semanticReportBytes", "semanticReportSha256"),
    ]
    for prefix, path_key, bytes_key, hash_key in evidence_bindings:
        path = repo_file(closure.get(path_key), f"closureEvidence.{prefix}", issues, root)
        if path is None or not path.is_file():
            issues.append(f"closure evidence missing: {prefix}")
            continue
        if bytes_key is not None:
            expect(path.stat().st_size == closure.get(bytes_key),
                   f"closure evidence bytes drifted: {prefix}", issues)
        expect(sha256(path) == closure.get(hash_key),
               f"closure evidence SHA drifted: {prefix}", issues)

    package_evidence = contract["packageIdentityEvidence"]
    expected_package_evidence = {
        "receiptPath": PACKAGE_IDENTITY_RECEIPT_PATH,
        "receiptBytes": PACKAGE_IDENTITY_RECEIPT_BYTES,
        "receiptSha256": PACKAGE_IDENTITY_RECEIPT_SHA256,
        "ufsManifestPath": PACKAGE_UFS_MANIFEST_PATH,
        "ufsManifestBytes": PACKAGE_UFS_MANIFEST_BYTES,
        "ufsManifestSha256": PACKAGE_UFS_MANIFEST_SHA256,
        "buildCookRunLogPath": PACKAGE_BUILD_LOG_PATH,
        "buildCookRunLogBytes": PACKAGE_BUILD_LOG_BYTES,
        "buildCookRunLogSha256": PACKAGE_BUILD_LOG_SHA256,
        "buildCookRunRawLogBytes": PACKAGE_BUILD_RAW_LOG_BYTES,
        "buildCookRunRawLogSha256": PACKAGE_BUILD_RAW_LOG_SHA256,
        "archiveFileManifestPath": PACKAGE_ARCHIVE_MANIFEST_PATH,
        "archiveFileManifestBytes": PACKAGE_ARCHIVE_MANIFEST_BYTES,
        "archiveFileManifestSha256": PACKAGE_ARCHIVE_MANIFEST_SHA256,
        "packagedRuntimeSmokeLogPath": PACKAGE_SMOKE_LOG_PATH,
        "packagedRuntimeSmokeLogBytes": PACKAGE_SMOKE_LOG_BYTES,
        "packagedRuntimeSmokeLogSha256": PACKAGE_SMOKE_LOG_SHA256,
        "packagedRuntimeSmokeRawLogBytes": PACKAGE_SMOKE_RAW_LOG_BYTES,
        "packagedRuntimeSmokeRawLogSha256": PACKAGE_SMOKE_RAW_LOG_SHA256,
        "ufsManifestPineRidgeIdentityPartitionComplete": True,
        "sourceUassetHashesBoundToCookedIoStoreBytes": False,
        "allStagedFilesDependenciesAndAnonymousChunksClassified": False,
        "actualStagedPackageProvenanceClosed": False,
    }
    if not exact_keys(package_evidence, set(expected_package_evidence),
                      "packageIdentityEvidence", issues):
        return issues
    expect(json_exact(package_evidence, expected_package_evidence),
           "packageIdentityEvidence differs from accepted package", issues)
    for prefix, path_key, bytes_key, hash_key in (
        ("packageIdentityReceipt", "receiptPath", "receiptBytes", "receiptSha256"),
        ("packageUfsManifest", "ufsManifestPath", "ufsManifestBytes", "ufsManifestSha256"),
        ("packageBuildCookRunLog", "buildCookRunLogPath", "buildCookRunLogBytes",
         "buildCookRunLogSha256"),
        ("packageArchiveFileManifest", "archiveFileManifestPath", "archiveFileManifestBytes",
         "archiveFileManifestSha256"),
        ("packagedRuntimeSmoke", "packagedRuntimeSmokeLogPath", "packagedRuntimeSmokeLogBytes",
         "packagedRuntimeSmokeLogSha256"),
    ):
        path = repo_file(package_evidence.get(path_key),
                         f"packageIdentityEvidence.{prefix}", issues, root)
        if path is None or not path.is_file():
            issues.append(f"package identity evidence missing: {prefix}")
            continue
        expect(path.stat().st_size == package_evidence.get(bytes_key),
               f"package identity evidence bytes drifted: {prefix}", issues)
        expect(sha256(path) == package_evidence.get(hash_key),
               f"package identity evidence SHA drifted: {prefix}", issues)

    continuity = contract["continuity"]
    expected_continuity = {
        "frozenFiles": [
            {"path": "Config/DG_Session16CorePlayabilityContract.json", "bytes": 23985,
             "sha256": "A30DC355EB098D2C9FF5F7CDD1BF3139FCE207EC4E9D79C57548DD961869B692"},
            {"path": "Scripts/validate_dg_session16_core_playability.py", "bytes": 55677,
             "sha256": "2364D916BDC5E62B0609F27ACB4BA3E936318B9A7985EC9DF939F4EB33182A9F"},
            {"path": "Docs/DG_SESSION17_INTERACTIVE_ROUND_FLOW_AUDIT.md", "bytes": 6208,
             "sha256": "19330DAAB484491CBC265025857CD4C016274076CEDF595F540A000B07FD1670"},
        ],
        "protectedProfile": {
            "path": "Saved/SaveGames/DiscGolfTour_Profile_0.sav", "bytes": 5212,
            "sha256": "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14",
        },
    }
    if not exact_keys(continuity, set(expected_continuity), "continuity", issues):
        return issues
    expect(json_exact(continuity, expected_continuity), "continuity differs", issues)
    for record in expected_continuity["frozenFiles"] + [expected_continuity["protectedProfile"]]:
        path = repo_file(record["path"], "continuity path", issues, root)
        if path is None or not path.is_file():
            issues.append(f"continuity file missing: {record['path']}")
        else:
            expect(path.stat().st_size == record["bytes"] and sha256(path) == record["sha256"],
                   f"continuity file drifted: {record['path']}", issues)

    expect(json_exact(contract["unresolvedBoundaries"], {
        "stagedPackageProvenance": "OPEN_NOT_IMPLEMENTED",
        "quarantinedFabReceipts": "OPEN",
        "developmentDoNotShipContent": "OPEN",
        "characterFrameworkDistributionRights": "OPEN",
        "legalAndManualReview": "OPEN",
    }), "unresolved boundaries differ", issues)
    expect(json_exact(contract["exitGate"], {
        "derivedRuntimeProvenanceComplete": True,
        "publicReleaseReady": False,
        "remainingReleaseBlockerCount": 13,
    }), "exitGate differs", issues)
    return issues


def sorted_path_digest(paths: list[str]) -> str:
    payload = ("\n".join(sorted(paths, key=str.casefold)) + "\n").encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def validate_package_identity_receipt(
    package_receipt: Any, source_receipt: Any, root: Path = ROOT,
) -> tuple[list[str], dict[str, Any]]:
    issues: list[str] = []
    summary: dict[str, Any] = {}
    if not exact_keys(package_receipt, {
        "schema", "schemaVersion", "session", "packageRunId", "platform", "configuration",
        "sourceReceipt", "buildCookRunEvidence", "ufsManifestEvidence", "archiveIdentity",
        "packagedRuntimeSmoke", "pineRidgePackageIdentityPartition", "boundary",
    }, "package identity receipt", issues):
        return issues, summary
    expect(package_receipt["schema"] ==
           "DiscGolfTour.Session18PolyHavenPackageIdentityReceipt.v1",
           "package identity receipt schema differs", issues)
    expect(type(package_receipt["schemaVersion"]) is int and
           package_receipt["schemaVersion"] == 1 and
           type(package_receipt["session"]) is int and package_receipt["session"] == 18,
           "package identity receipt version/session differs", issues)
    expect(package_receipt["packageRunId"] ==
           "63624166-aa24-49d0-a89f-74a125a36be2",
           "package identity run differs", issues)
    expect(package_receipt["platform"] == "Windows" and
           package_receipt["configuration"] == "Development",
           "package identity platform/configuration differs", issues)

    expected_source = {
        "path": RECEIPT_PATH.relative_to(ROOT).as_posix(),
        "bytes": RECEIPT_BYTES,
        "sha256": RECEIPT_SHA256,
    }
    expect(json_exact(package_receipt["sourceReceipt"], expected_source),
           "package identity source receipt binding differs", issues)
    expected_build = {
        "path": PACKAGE_BUILD_LOG_PATH,
        "bytes": PACKAGE_BUILD_LOG_BYTES,
        "sha256": PACKAGE_BUILD_LOG_SHA256,
        "rawSourceBytes": PACKAGE_BUILD_RAW_LOG_BYTES,
        "rawSourceSha256": PACKAGE_BUILD_RAW_LOG_SHA256,
        "sanitization": {
            "replacement": "<USER_HOME>",
            "windowsHomeMatchesReplaced": 60,
            "posixHomeMatchesReplaced": 31,
        },
        "fullCook": True,
        "cookedPackageCount": 985,
        "incrementallySkippedPackageCount": 0,
        "platformSkippedPackageCount": 7,
        "totalPackageCount": 992,
        "ioStoreOrderedPackageCount": 985,
        "ioStoreWrittenChunkCount": 2895,
        "buildSuccessful": True,
        "exitCode": 0,
    }
    expect(json_exact(package_receipt["buildCookRunEvidence"], expected_build),
           "package identity build/cook evidence differs", issues)
    expected_ufs = {
        "path": PACKAGE_UFS_MANIFEST_PATH,
        "bytes": PACKAGE_UFS_MANIFEST_BYTES,
        "sha256": PACKAGE_UFS_MANIFEST_SHA256,
        "lineCount": 3052,
    }
    expect(json_exact(package_receipt["ufsManifestEvidence"], expected_ufs),
           "package identity UFS evidence differs", issues)
    expect(json_exact(package_receipt["archiveIdentity"], {
        "label": "S18_PolyHavenProvenance_63624166-aa24-49d0-a89f-74a125a36be2/Windows",
        "fileManifestPath": PACKAGE_ARCHIVE_MANIFEST_PATH,
        "fileManifestBytes": PACKAGE_ARCHIVE_MANIFEST_BYTES,
        "fileManifestSha256": PACKAGE_ARCHIVE_MANIFEST_SHA256,
        "fileManifestLineCount": 54,
        "physicalFileCount": 54,
        "physicalByteSize": 1918214604,
        "sortedRelativePathSizeSha256ManifestDigest":
            "0877C04FF8911F0903077F8C68093BF8A783FB6939538D2BFD1E2888D35B1870",
        "bootstrapExecutable": {
            "relativePath": "DiscGolfTour.exe", "bytes": 171520,
            "sha256": "320484DEF09542EACF36B07B04F2D68040E64551C84342A34673BB22433DFF35",
        },
        "gameExecutable": {
            "relativePath": "DiscGolfTour/Binaries/Win64/DiscGolfTour.exe", "bytes": 341401088,
            "sha256": "6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346",
        },
    }), "package archive identity differs", issues)
    expect(json_exact(package_receipt["packagedRuntimeSmoke"], {
        "runId": "65f4b822-7c11-4ca3-a63e-caa4e10fb857",
        "path": PACKAGE_SMOKE_LOG_PATH,
        "bytes": PACKAGE_SMOKE_LOG_BYTES,
        "sha256": PACKAGE_SMOKE_LOG_SHA256,
        "rawSourceBytes": PACKAGE_SMOKE_RAW_LOG_BYTES,
        "rawSourceSha256": PACKAGE_SMOKE_RAW_LOG_SHA256,
        "canonicalization": "UTF8_NO_BOM",
        "exitCode": 0,
        "sampleCount": 1968,
        "finalCarryMeters": 98.1,
        "groundContactCount": 1,
        "authoredCameraActive": True,
        "localWindActive": True,
    }), "packaged runtime smoke declaration differs", issues)

    derived_rows = source_receipt.get("derivedRuntimeArtifacts") if type(source_receipt) is dict else None
    excluded_rows = (
        source_receipt.get("excludedProjectOriginalRuntimeArtifacts")
        if type(source_receipt) is dict else None
    )
    derived_paths = [
        "DiscGolfTour/" + row["relativePath"] for row in derived_rows
        if type(row) is dict and type(row.get("relativePath")) is str
    ] if type(derived_rows) is list else []
    excluded_paths = [
        "DiscGolfTour/" + row["relativePath"] for row in excluded_rows
        if type(row) is dict and type(row.get("relativePath")) is str
    ] if type(excluded_rows) is list else []
    expected_partition = {
        "derivedCount": 55,
        "derivedSortedPathDigest":
            "CA6FE3E1A167B42D4D72E522B6F7A232C71D3B275FC95A60E6F001CD2759DA69",
        "derivedMissingCount": 0,
        "excludedProjectOriginalCount": 5,
        "excludedProjectOriginalSortedPathDigest":
            "C593C2523F76EE9F7ED0519BD42EC65906075C44CE834E8582BD48319DD716B3",
        "excludedProjectOriginalMissingCount": 0,
        "unionCount": 60,
        "unionSortedPathDigest":
            "9EDB069BF5BDB40FC74234E85467C8941AF95B0D203DC31443D4A8E185713069",
        "unionMissingCount": 0,
        "pineRidgeUassetExtraCount": 0,
        "pineRidgeNonUassetSidecarCount": 37,
        "quarantinedImportRootMatchCount": 0,
    }
    expect(json_exact(package_receipt["pineRidgePackageIdentityPartition"], expected_partition),
           "package identity partition declaration differs", issues)
    expect(len(derived_paths) == 55 and len(excluded_paths) == 5,
           "package identity source receipt lists do not contain 55+5 paths", issues)
    expect(sorted_path_digest(derived_paths) == expected_partition["derivedSortedPathDigest"],
           "derived package identity digest differs", issues)
    expect(sorted_path_digest(excluded_paths) ==
           expected_partition["excludedProjectOriginalSortedPathDigest"],
           "excluded package identity digest differs", issues)
    expect(sorted_path_digest(derived_paths + excluded_paths) ==
           expected_partition["unionSortedPathDigest"],
           "Pine Ridge package identity union digest differs", issues)

    expect(json_exact(package_receipt["boundary"], {
        "ufsManifestPineRidgeIdentityPartitionComplete": True,
        "sourceUassetHashesBoundToCookedIoStoreBytes": False,
        "allStagedFilesDependenciesAndAnonymousChunksClassified": False,
        "actualStagedPackageProvenanceClosed": False,
        "publicReleaseReady": False,
    }), "package identity boundary differs", issues)

    archive_manifest_path = repo_file(
        PACKAGE_ARCHIVE_MANIFEST_PATH, "package archive file manifest", issues, root,
    )
    if archive_manifest_path is not None and archive_manifest_path.is_file():
        try:
            archive_lines = archive_manifest_path.read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeError) as exc:
            issues.append(f"package archive file manifest cannot be read: {exc}")
            archive_lines = []
        expect(len(archive_lines) == 54,
               "package archive file manifest line count differs", issues)
        archive_rows: dict[str, tuple[int, str]] = {}
        for index, line in enumerate(archive_lines, start=1):
            parts = line.split("\t")
            if len(parts) != 3:
                issues.append(f"package archive manifest line {index} is not three-column data")
                continue
            relative, byte_text, digest = parts
            pure = PurePosixPath(relative)
            if (not relative or "\\" in relative or pure.is_absolute() or
                    any(part in ("", ".", "..") for part in pure.parts)):
                issues.append(f"package archive manifest path is not canonical: {relative!r}")
                continue
            try:
                byte_size = int(byte_text)
            except ValueError:
                issues.append(f"package archive manifest byte count is invalid: {relative}")
                continue
            expect(byte_size >= 0, f"package archive byte count is negative: {relative}", issues)
            expect(SHA256_RE.fullmatch(digest) is not None,
                   f"package archive SHA-256 is malformed: {relative}", issues)
            folded = relative.casefold()
            expect(folded not in {value.casefold() for value in archive_rows},
                   f"package archive path duplicates/case-collides: {relative}", issues)
            archive_rows[relative] = (byte_size, digest)
        expect(list(archive_rows) == sorted(archive_rows, key=str.casefold),
               "package archive file manifest is not canonically sorted", issues)
        expect(sum(value[0] for value in archive_rows.values()) == 1918214604,
               "package archive byte total differs", issues)
        expect(archive_rows.get("DiscGolfTour.exe") == (
            171520, "320484DEF09542EACF36B07B04F2D68040E64551C84342A34673BB22433DFF35"
        ), "package bootstrap executable row differs", issues)
        expect(archive_rows.get("DiscGolfTour/Binaries/Win64/DiscGolfTour.exe") == (
            341401088, "6051C15D6DCF14D4EE92B34215B79B7AA7BCADC5D8F6F5CA8AB24807E89D4346"
        ), "package game executable row differs", issues)
        summary.update({
            "archive_physical_file_count": len(archive_rows),
            "archive_physical_byte_size": sum(value[0] for value in archive_rows.values()),
        })
    else:
        issues.append("package archive file manifest is missing")

    smoke_path = repo_file(PACKAGE_SMOKE_LOG_PATH, "packaged runtime smoke log", issues, root)
    if smoke_path is not None and smoke_path.is_file():
        try:
            smoke_log = smoke_path.read_text(encoding="utf-8", errors="strict")
        except (OSError, UnicodeError) as exc:
            issues.append(f"packaged runtime smoke log cannot be read: {exc}")
            smoke_log = ""
        pass_marker = (
            "PINE RIDGE PLAY SMOKE PASS: 1968 samples, 98.1 m final carry, 1 contacts, "
            "surface DEEP ROUGH, authored camera and local wind active."
        )
        expect(smoke_log.count(pass_marker) == 1,
               "packaged runtime smoke exact pass marker differs", issues)
        expect("PINE RIDGE PLAY SMOKE FAIL" not in smoke_log and "Fatal error" not in smoke_log,
               "packaged runtime smoke contains a failure marker", issues)
        summary["packaged_runtime_smoke_passed"] = smoke_log.count(pass_marker) == 1
    else:
        issues.append("packaged runtime smoke log is missing")

    manifest_path = repo_file(PACKAGE_UFS_MANIFEST_PATH, "package UFS manifest", issues, root)
    if manifest_path is not None and manifest_path.is_file():
        try:
            lines = manifest_path.read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeError) as exc:
            issues.append(f"package UFS manifest cannot be read: {exc}")
            lines = []
        expect(len(lines) == 3052, "package UFS manifest line count differs", issues)
        ufs_paths: list[str] = []
        for index, line in enumerate(lines, start=1):
            parts = line.split("\t")
            path_text = parts[0] if parts else ""
            pure = PurePosixPath(path_text)
            if (len(parts) != 2 or not path_text or "\\" in path_text or
                    path_text.startswith("/") or pure.as_posix() != path_text or
                    any(ord(character) < 32 for character in path_text)):
                issues.append(f"package UFS manifest line {index} is not canonical two-column data")
                continue
            ufs_paths.append(path_text)
        expect(len({item.casefold() for item in ufs_paths}) == len(ufs_paths),
               "package UFS manifest contains duplicate/case-colliding paths", issues)
        ufs_set = set(ufs_paths)
        pine_prefix = "DiscGolfTour/Content/Presentation/Course/PineRidge/"
        pine_uassets = {item for item in ufs_set
                        if item.startswith(pine_prefix) and item.endswith(".uasset")}
        pine_sidecars = {item for item in ufs_set
                         if item.startswith(pine_prefix) and not item.endswith(".uasset")}
        expected_union = set(derived_paths + excluded_paths)
        expect(set(derived_paths).issubset(ufs_set),
               "derived package identities are missing from the UFS manifest", issues)
        expect(set(excluded_paths).issubset(ufs_set),
               "excluded project-original identities are missing from the UFS manifest", issues)
        expect(pine_uassets == expected_union,
               "UFS Pine Ridge .uasset partition is not the exact declared 60", issues)
        expect(len(pine_sidecars) == 37,
               "UFS Pine Ridge non-.uasset sidecar count differs", issues)
        expect(all(item.endswith(".ubulk") and item[:-len(".ubulk")] + ".uasset" in expected_union
                   for item in pine_sidecars),
               "UFS Pine Ridge sidecar is not a .ubulk bound to a declared package identity", issues)
        quarantine_prefixes = (
            "DiscGolfTour/Content/PN_interactiveSpruceForest/",
            "DiscGolfTour/Content/Stump_Scanned/",
            "DiscGolfTour/Content/WaterMaterials/",
        )
        quarantine_matches = {
            item for item in ufs_set if item.startswith(quarantine_prefixes)
        }
        expect(not quarantine_matches,
               "quarantined import roots are present in the UFS manifest", issues)
        summary.update({
            "ufs_manifest_line_count": len(lines),
            "packaged_derived_identity_count": len(set(derived_paths) & ufs_set),
            "packaged_excluded_identity_count": len(set(excluded_paths) & ufs_set),
            "packaged_pine_ridge_uasset_count": len(pine_uassets),
            "packaged_pine_ridge_sidecar_count": len(pine_sidecars),
            "packaged_quarantine_match_count": len(quarantine_matches),
        })
    else:
        issues.append("package UFS manifest is missing")

    build_log_path = repo_file(PACKAGE_BUILD_LOG_PATH, "package build/cook log", issues, root)
    if build_log_path is not None and build_log_path.is_file():
        try:
            build_log = build_log_path.read_text(encoding="utf-8-sig", errors="strict")
        except (OSError, UnicodeError) as exc:
            issues.append(f"package build/cook log cannot be read: {exc}")
            build_log = ""
        for marker in (
            " -build -cook -stage -pak -iostore -archive ",
            " -clean -utf8output ",
            "FULL COOK:",
            "Packages Cooked: 985, Packages Incrementally Skipped: 0, "
            "Packages Skipped by Platform: 7, Total Packages: 992",
            "Ordered 985 packages using fallback bundle order",
            "2,895 chunks attempted to compress",
            "BUILD SUCCESSFUL",
            "AutomationTool exiting with ExitCode=0 (Success)",
        ):
            expect(marker in build_log, f"package build/cook marker missing: {marker}", issues)
    else:
        issues.append("package build/cook log is missing")
    return issues, summary


def validate_project(contract: Any, receipt: Any) -> tuple[list[str], dict[str, Any]]:
    issues = validate_contract(contract)
    receipt_issues, summary = validate_receipt(receipt)
    issues.extend(receipt_issues)
    if RECEIPT_PATH.is_file():
        expect(RECEIPT_PATH.stat().st_size == RECEIPT_BYTES, "durable receipt byte count drifted", issues)
        expect(sha256(RECEIPT_PATH) == RECEIPT_SHA256, "durable receipt SHA-256 drifted", issues)
    else:
        issues.append("durable receipt is missing")
    semantic_path = ROOT / SEMANTIC_REPORT_PATH
    if semantic_path.is_file():
        try:
            semantic = load_json(semantic_path)
        except StrictJsonError as exc:
            issues.append(str(exc))
        else:
            semantic_keys = {
                "schema", "receiptRelativePath", "receiptSha256", "engineVersion",
                "engineChangelist", "passed", "assetCount", "derivedAssetCount",
                "excludedProjectOriginalCount", "assets", "issues",
            }
            if not exact_keys(semantic, semantic_keys, "semantic report", issues):
                semantic = {}
            expect(semantic.get("schema") ==
                   "DiscGolfTour.PineRidgeProvenanceSemanticValidation.v1",
                   "semantic report schema differs", issues)
            expect(semantic.get("receiptRelativePath") == RECEIPT_PATH.relative_to(ROOT).as_posix()
                   and semantic.get("receiptSha256") == RECEIPT_SHA256,
                   "semantic report receipt binding differs", issues)
            expect(semantic.get("engineVersion") == "5.8.1" and
                   semantic.get("engineChangelist") == 56057345,
                   "semantic report engine identity differs", issues)
            expect(semantic.get("passed") is True and semantic.get("issues") == [],
                   "semantic report is not a clean pass", issues)
            expect(semantic.get("assetCount") == 60 and
                   semantic.get("derivedAssetCount") == 55 and
                   semantic.get("excludedProjectOriginalCount") == 5,
                   "semantic report counts differ", issues)
            rows = semantic.get("assets")
            expect(type(rows) is list and len(rows) == 60,
                   "semantic report must contain 60 asset rows", issues)
            if type(rows) is list:
                row_paths = [row.get("objectPath") for row in rows if type(row) is dict]
                derived_rows = (receipt.get("derivedRuntimeArtifacts")
                                if type(receipt) is dict else None)
                excluded_rows = (receipt.get("excludedProjectOriginalRuntimeArtifacts")
                                 if type(receipt) is dict else None)
                if type(derived_rows) is list and type(excluded_rows) is list and all(
                    type(item) is dict and type(item.get("objectPath")) is str
                    for item in derived_rows + excluded_rows
                ):
                    declared_paths = [item["objectPath"] for item in derived_rows + excluded_rows]
                    expect(row_paths == sorted(declared_paths),
                           "semantic report asset partition differs", issues)
                else:
                    issues.append("semantic report cannot bind malformed receipt asset rows")
    else:
        issues.append("semantic validation report is missing")
    package_receipt_path = ROOT / PACKAGE_IDENTITY_RECEIPT_PATH
    if package_receipt_path.is_file():
        try:
            package_receipt = load_json(package_receipt_path)
        except StrictJsonError as exc:
            issues.append(str(exc))
        else:
            package_issues, package_summary = validate_package_identity_receipt(
                package_receipt, receipt,
            )
            issues.extend(package_issues)
            summary.update(package_summary)
    else:
        issues.append("package identity receipt is missing")
    return issues, summary


def run_self_test(contract: Any, receipt: Any) -> tuple[int, int, list[str]]:
    failures: list[str] = []
    total = 0

    base_issues, _ = validate_project(contract, receipt)
    total += 1
    if base_issues:
        failures.append(f"valid baseline rejected: {base_issues[:3]}")

    contract_mutations = [
        ("contract schemaVersion type", lambda v: v.__setitem__("schemaVersion", True)),
        ("historical blocker removed", lambda v: v["historicalReleaseBlockers"].pop()),
        ("historical blocker reordered", lambda v: v["historicalReleaseBlockers"].reverse()),
        ("resolved blocker changed", lambda v: v["resolvedReleaseBlockers"].__setitem__(0, "OTHER")),
        ("remaining blocker removed", lambda v: v["remainingReleaseBlockers"].pop()),
        ("staged provenance blocker removed", lambda v: v["remainingReleaseBlockers"].remove(
            "ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED")),
        ("poly blocker restored", lambda v: v["remainingReleaseBlockers"].insert(5, POLY_BLOCKER)),
        ("receipt hash forged", lambda v: v["closureEvidence"].__setitem__("receiptSha256", "0" * 64)),
        ("staged closure forged", lambda v: v["closureEvidence"].__setitem__(
            "actualStagedPackageProvenanceClosed", True)),
        ("package evidence hash forged", lambda v: v["packageIdentityEvidence"].__setitem__(
            "receiptSha256", "0" * 64)),
        ("package staged closure forged", lambda v: v["packageIdentityEvidence"].__setitem__(
            "actualStagedPackageProvenanceClosed", True)),
        ("package partition bool type", lambda v: v["packageIdentityEvidence"].__setitem__(
            "ufsManifestPineRidgeIdentityPartitionComplete", 1)),
        ("closure object type", lambda v: v.__setitem__("closureEvidence", [])),
        ("release forged", lambda v: v["exitGate"].__setitem__("publicReleaseReady", True)),
        ("profile hash changed", lambda v: v["continuity"]["protectedProfile"].__setitem__(
            "sha256", "0" * 64)),
        ("contract extra key", lambda v: v.__setitem__("extra", True)),
    ]
    for name, mutate in contract_mutations:
        total += 1
        value = copy.deepcopy(contract)
        mutate(value)
        if not validate_contract(value):
            failures.append(f"contract mutation accepted: {name}")

    receipt_mutations = [
        ("receipt schemaVersion type", lambda v: v.__setitem__("schemaVersion", True)),
        ("license boolean type", lambda v: v["license"].__setitem__("attributionRequired", 0)),
        ("asset task boolean type", lambda v: v["importExecution"]["pinnedImportOptions"][
            "assetTask"].__setitem__("automated", 1)),
        ("invalid completed UTC", lambda v: v.__setitem__(
            "completedUtc", "2026-99-99T99:99:99Z")),
        ("source hash", lambda v: v["sourceFiles"][0].__setitem__("sha256", "0" * 64)),
        ("source removed", lambda v: v["sourceFiles"].pop()),
        ("source traversal", lambda v: v["sourceFiles"][0].__setitem__("relativePath", "../escape")),
        ("importer hash", lambda v: v["importExecution"].__setitem__("importerSha256", "0" * 64)),
        ("engine CL", lambda v: v["importExecution"].__setitem__("engineChangelist", 1)),
        ("import option", lambda v: v["importExecution"]["pinnedImportOptions"]["staticMeshes"].__setitem__(
            "autoGenerateCollision", True)),
        ("derived removed", lambda v: v["derivedRuntimeArtifacts"].pop()),
        ("derived duplicated", lambda v: v["derivedRuntimeArtifacts"].append(
            copy.deepcopy(v["derivedRuntimeArtifacts"][0]))),
        ("derived traversal", lambda v: v["derivedRuntimeArtifacts"][0].__setitem__(
            "relativePath", "Content/Presentation/../escape.uasset")),
        ("derived hash", lambda v: v["derivedRuntimeArtifacts"][0].__setitem__("sha256", "0" * 64)),
        ("derived class", lambda v: v["derivedRuntimeArtifacts"][0].__setitem__("assetClass", "Material")),
        ("direct source removed", lambda v: v["derivedRuntimeArtifacts"][0][
            "directSourceRelativePaths"].clear()),
        ("transitive source removed", lambda v: v["derivedRuntimeArtifacts"][0][
            "transitiveSourceRelativePaths"].clear()),
        ("dependency removed", lambda v: next(
            item for item in v["derivedRuntimeArtifacts"] if item["runtimeDependencies"]
        )["runtimeDependencies"].pop()),
        ("excluded removed", lambda v: v["excludedProjectOriginalRuntimeArtifacts"].pop()),
        ("excluded reclassified", lambda v: v["excludedProjectOriginalRuntimeArtifacts"][0].__setitem__(
            "provenanceKind", "POLY_HAVEN")),
        ("partition count", lambda v: v["expectedCounts"].__setitem__("runtimeRootTotal", 59)),
        ("source assertion", lambda v: v["closureAssertions"].__setitem__(
            "allSourceFilesMapped", False)),
        ("staged assertion", lambda v: v["closureAssertions"].__setitem__(
            "actualStagedPackageProvenanceClosed", True)),
        ("receipt extra key", lambda v: v.__setitem__("token", "forbidden")),
    ]
    for name, mutate in receipt_mutations:
        total += 1
        value = copy.deepcopy(receipt)
        mutate(value)
        if not validate_receipt(value)[0]:
            failures.append(f"receipt mutation accepted: {name}")

    try:
        package_receipt = load_json(ROOT / PACKAGE_IDENTITY_RECEIPT_PATH)
    except StrictJsonError as exc:
        failures.append(f"package identity receipt could not be loaded: {exc}")
        package_receipt = None
    package_mutations = [
        ("source binding", lambda v: v["sourceReceipt"].__setitem__("sha256", "0" * 64)),
        ("cooked count mislabeled", lambda v: v["buildCookRunEvidence"].__setitem__(
            "cookedPackageCount", 992)),
        ("derived count", lambda v: v["pineRidgePackageIdentityPartition"].__setitem__(
            "derivedCount", 54)),
        ("union digest", lambda v: v["pineRidgePackageIdentityPartition"].__setitem__(
            "unionSortedPathDigest", "0" * 64)),
        ("cooked bytes falsely bound", lambda v: v["boundary"].__setitem__(
            "sourceUassetHashesBoundToCookedIoStoreBytes", True)),
        ("UFS partition boolean type", lambda v: v["boundary"].__setitem__(
            "ufsManifestPineRidgeIdentityPartitionComplete", 1)),
        ("build success boolean type", lambda v: v["buildCookRunEvidence"].__setitem__(
            "buildSuccessful", 1)),
        ("smoke camera boolean type", lambda v: v["packagedRuntimeSmoke"].__setitem__(
            "authoredCameraActive", 1)),
        ("package receipt extra key", lambda v: v.__setitem__("extra", True)),
    ]
    if package_receipt is not None:
        for name, mutate in package_mutations:
            total += 1
            value = copy.deepcopy(package_receipt)
            mutate(value)
            if not validate_package_identity_receipt(value, receipt)[0]:
                failures.append(f"package identity mutation accepted: {name}")

    total += 1
    saved_root = ROOT / "Saved"
    saved_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="Session18StrictJson_", dir=saved_root) as temp:
        duplicate = Path(temp) / "duplicate.json"
        duplicate.write_text('{"schema":1,"schema":2}', encoding="utf-8")
        try:
            load_json(duplicate)
            failures.append("duplicate JSON key accepted")
        except StrictJsonError:
            pass

        total += 1
        overflow = Path(temp) / "overflow.json"
        overflow.write_text('{"value":1e309}', encoding="utf-8")
        try:
            load_json(overflow)
            failures.append("overflow JSON number accepted")
        except StrictJsonError:
            pass

    total += 1
    malformed_receipt = copy.deepcopy(receipt)
    malformed_receipt.pop("derivedRuntimeArtifacts")
    try:
        malformed_issues, _ = validate_project(contract, malformed_receipt)
        if not malformed_issues:
            failures.append("malformed top-level receipt accepted")
    except Exception as exc:  # self-test asserts fail-closed reporting, not a traceback
        failures.append(f"malformed top-level receipt raised {type(exc).__name__}: {exc}")

    return total - len(failures), total, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    parser.add_argument("--receipt", type=Path, default=RECEIPT_PATH)
    parser.add_argument("--report", type=Path, default=REPORT_PATH)
    parser.add_argument("--no-report", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-release-ready", action="store_true")
    args = parser.parse_args()

    try:
        contract = load_json(args.contract)
        receipt = load_json(args.receipt)
    except StrictJsonError as exc:
        print(f"SESSION 18 POLY HAVEN PROVENANCE FAIL: {exc}", file=sys.stderr)
        return 1

    if args.self_test:
        passed, total, failures = run_self_test(contract, receipt)
        if failures:
            for failure in failures:
                print(f"SELFTEST FAIL: {failure}", file=sys.stderr)
            print(f"SESSION 18 SELFTEST FAIL: {passed}/{total}", file=sys.stderr)
            return 1
        print(f"SESSION 18 SELFTEST PASS: {passed}/{total}")
        return 0

    issues, summary = validate_project(contract, receipt)
    report = {
        "schema": "DiscGolfTour.Session18PolyHavenProvenanceReport.v1",
        "generatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "contractValid": not issues,
        "derivedRuntimeProvenanceComplete": not issues,
        "publicReleaseReady": False,
        "resolvedReleaseBlockers": [POLY_BLOCKER] if not issues else [],
        "remainingReleaseBlockers": REMAINING_BLOCKERS if not issues else HISTORICAL_BLOCKERS,
        "summary": summary,
        "issues": issues,
    }
    if not args.no_report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if issues:
        for issue in issues:
            print(f"ERROR: {issue}", file=sys.stderr)
        print(f"SESSION 18 POLY HAVEN PROVENANCE FAIL: errors={len(issues)}", file=sys.stderr)
        return 1
    print(
        "SESSION 18 POLY HAVEN PROVENANCE PASS_TECHNICAL_RELEASE_BLOCKED: "
        f"sources={summary['source_file_count']} derived={summary['derived_runtime_artifact_count']} "
        f"excluded={summary['excluded_project_original_count']} blockers={len(REMAINING_BLOCKERS)}"
    )
    if args.require_release_ready:
        print(
            f"SESSION 18 RELEASE GATE BLOCKED: {len(REMAINING_BLOCKERS)} blockers remain",
            file=sys.stderr,
        )
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
