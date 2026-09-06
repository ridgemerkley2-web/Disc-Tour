#!/usr/bin/env python3
"""Inventory and fail-closed audit a Session 19 Windows Shipping archive.

This tool never approves public release.  A passing receipt means only that the
supplied candidate was exhaustively inventoried and matched an independently
reviewed exact-identity classification manifest.
"""

from __future__ import annotations

import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Any, Callable, Iterable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19StagedProvenancePolicy.json"
PLUGIN_CAPABILITY_POLICY_PATH = ROOT / "Config/DG_Session19ShippingPluginCapabilityPolicy.json"
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
SHA1_RE = re.compile(r"^[0-9A-Fa-f]{40}$")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
AUTHORITY_RE = re.compile(r"^[A-Z0-9][A-Z0-9._-]{1,127}$")
LICENSE_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9.+_-]{0,127}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{3})?Z$")
ANONYMOUS_ID_RE = re.compile(
    r"^<[^<>]+>#[0-9A-F]{24}@\d{16}:\d{16}:[0-9A-F]{40}$"
)
REQUIRED_ENGINE_DEVELOPMENT_ROOTS = (
    "/Engine/Content/VREditor/",
    "/Engine/Content/EditorMeshes/",
    "/Engine/Content/Slate/Testing/",
    "/Engine/Plugins/Developer/Concert/",
    "/Engine/Plugins/ChaosVD/",
)
PROTECTED_RUNTIME_SLATE_IDENTITIES = (
    "Engine/Content/Slate/Fonts/Roboto-Regular.ttf",
    "Engine/Content/Slate/Common/Selection.png",
    "Engine/Content/Slate/Starship/Common/Window/WindowButton_Close.png",
)
REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS = (
    "Engine/Plugins/Editor/BlueprintMaterialTextureNodes/BlueprintMaterialTextureNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetDataflowNodes/ChaosClothAssetDataflowNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetEditorCore/ChaosClothAssetEditorCore.uplugin",
    "Engine/Plugins/Experimental/ChaosSolverPlugin/ChaosSolverPlugin.uplugin",
    "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin",
    "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin",
    "Engine/Plugins/Experimental/EditorDataStorageFeatures/EditorDataStorageFeatures.uplugin",
    "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin",
    "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin",
    "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
    "Engine/Plugins/MovieScene/LevelSequenceEditor/LevelSequenceEditor.uplugin",
    "Engine/Plugins/Animation/LiveLinkDevice/LiveLinkDevice.uplugin",
    "Engine/Plugins/Developer/PluginUtils/PluginUtils.uplugin",
    "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin",
    "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin",
)
REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS = (
    "Engine/Plugins/ChaosClothAssetDataflowNodes/ChaosClothAssetDataflowNodes.uplugin",
    "Engine/Plugins/ChaosClothAssetEditorCore/ChaosClothAssetEditorCore.uplugin",
    "Engine/Plugins/Experimental/ChaosSolverPlugin/ChaosSolverPlugin.uplugin",
    "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin",
    "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin",
    "Engine/Plugins/Experimental/EditorDataStorageFeatures/EditorDataStorageFeatures.uplugin",
    "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin",
    "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin",
    "Engine/Plugins/MovieScene/LevelSequenceEditor/LevelSequenceEditor.uplugin",
    "Engine/Plugins/Animation/LiveLinkDevice/LiveLinkDevice.uplugin",
    "Engine/Plugins/Developer/PluginUtils/PluginUtils.uplugin",
    "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin",
    "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin",
)
DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES = (
    "Binaries", "Config", "Content", "Resources", "Source",
)
REVIEWED_RESIDUAL_METADATA_EXCLUSIONS = (
    "Engine/Plugins/ChaosClothAssetEditorCore/Config/DefaultChaosClothAssetEditorCore.ini",
    "Engine/Plugins/Editor/EditorScriptingUtilities/Config/DefaultEditorScriptingUtilities.ini",
)
EXACT_DEVELOPMENT_PATH_EXCEPTIONS = (
    "Engine/Plugins/Editor/BlueprintMaterialTextureNodes/BlueprintMaterialTextureNodes.uplugin",
    "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin",
    "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin",
    "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin",
    "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin",
    "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin",
)
LANDMASS_DESCRIPTOR = "Engine/Plugins/Experimental/Landmass/Landmass.uplugin"
LANDMASS_FINAL_PAYLOAD_TOKENS = tuple(
    f"/Engine/Plugins/Experimental/Landmass/{subdirectory}/"
    for subdirectory in DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES
)
IOSTORE_CHUNK_ID_RE = re.compile(r"^[0-9A-Fa-f]{24}$")
IOSTORE_PACKAGE_ID_RE = re.compile(r"^0x[0-9A-Fa-f]{1,16}$")
IOSTORE_CHUNK_TYPE_RE = re.compile(r"^[A-Za-z][A-Za-z0-9]*$")
UNSIGNED_INTEGER_RE = re.compile(r"^(?:0|[1-9][0-9]*)$")
IOSTORE_CSV_HEADER = (
    "OrderInContainer", "ChunkId", "PackageId", "PackageName", "Filename",
    "ContainerName", "Offset", "OffsetOnDisk", "Size", "CompressedSize",
    "Hash", "ChunkType", "ClassType", "PakChunk", "Platform", "InstallType",
    "DeliveryType", "PartitionIndex", "OffsetInPartition",
)

PAK_ENTRY_RE = re.compile(
    r'^LogPakFile: Display: "(?P<path>.+)" offset: (?P<offset>\d+), '
    r'size: (?P<size>\d+) bytes, sha1: (?P<hash>[0-9A-Fa-f]{40}), '
    r'compression: (?P<compression>.+)\.$'
)
PAK_SUMMARY_RE = re.compile(
    r"^LogPakFile: Display: (?P<count>\d+) files \((?P<bytes>\d+) bytes\), "
    r"\((?P<filtered>\d+) filtered bytes\)\.$"
)

POLICY_ROOT_KEYS = {
    "schema", "schemaVersion", "session", "policyId", "state", "releaseReady",
    "closesReleaseBlockerWithoutAcceptedReceipt", "target", "requiredArchiveFiles",
    "requiredLooseDataFiles", "manifestFiles", "distributionIdentityAuthority",
    "containerPolicy",
    "forbiddenArchiveExtensions", "forbiddenPathTokens",
    "forbiddenDevelopmentPathTokens", "forbiddenBinaryMarkers",
    "pluginDependencyClosure", "uatLogRequirements", "classificationManifest",
    "receipt", "negativeFixture",
}
CLASSIFICATION_ROOT_KEYS = {
    "schema", "schemaVersion", "candidateId", "state", "records",
}
CLASSIFICATION_RECORD_KEYS = {
    "scope", "container", "identity", "classification", "authorityId",
    "licenseId", "shippingApproved", "evidencePaths",
}


class StrictJsonError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value: str) -> Any:
    raise StrictJsonError(f"non-finite JSON number: {value}")


def load_json_text_strict(text: str) -> Any:
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=reject_constant,
    )


def load_json_strict(path: Path) -> Any:
    try:
        return load_json_text_strict(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"strict JSON load failed: {exc}") from exc


def issue(code: str, message: str, identity: str | None = None) -> dict[str, Any]:
    value: dict[str, Any] = {"code": code, "message": message}
    if identity is not None:
        value["identity"] = identity
    return value


def typed_equal(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if type(expected) is dict:
        return set(actual) == set(expected) and all(
            typed_equal(actual[key], expected[key]) for key in expected
        )
    if type(expected) is list:
        return len(actual) == len(expected) and all(
            typed_equal(left, right) for left, right in zip(actual, expected)
        )
    return actual == expected


def canonical_relative(value: Any) -> str | None:
    if type(value) is not str:
        return None
    if not value or value != value.strip() or "\\" in value or "\x00" in value or ":" in value:
        return None
    pure = PurePosixPath(value)
    if (
            pure.is_absolute() or value.startswith("/")
            or any(part in ("", ".", "..") for part in pure.parts)
            or pure.as_posix() != value):
        return None
    return value


def project_evidence_path(value: Any, project_root: Path) -> Path | None:
    canonical = canonical_relative(value)
    if canonical is None:
        return None
    candidate = project_root.joinpath(*PurePosixPath(canonical).parts)
    resolved_root = project_root.resolve()
    try:
        resolved = candidate.resolve(strict=True)
    except OSError:
        return None
    if resolved_root not in resolved.parents or not candidate.is_file() or candidate.is_symlink():
        return None
    return candidate


def unique_string_list(value: Any) -> bool:
    return (
        type(value) is list
        and all(type(item) is str and item for item in value)
        and len({item.casefold() for item in value}) == len(value)
    )


def evidence_binding(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {
        "path": path.relative_to(ROOT).as_posix(),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def development_path_token(identity: str, policy: dict[str, Any]) -> str | None:
    normalized = "/" + identity + "/"
    return next(
        (
            token for token in policy["forbiddenDevelopmentPathTokens"]
            if token.casefold() in normalized.casefold()
        ),
        None,
    )


def development_path_problems(
        identity: str, policy: dict[str, Any], message: str,
) -> list[dict[str, Any]]:
    token = development_path_token(identity, policy)
    if token is None:
        return []
    exceptions = policy["distributionIdentityAuthority"]["exactDevelopmentPathExceptions"]
    if identity in exceptions:
        return []
    problems: list[dict[str, Any]] = []
    if identity.casefold() in {value.casefold() for value in exceptions}:
        problems.append(issue(
            "DEVELOPMENT_EXCEPTION_CASE_MISMATCH",
            "development-path exception must match exact canonical case",
            identity,
        ))
    problems.append(issue("DEVELOPMENT_ARTIFACT", message, identity))
    return problems


def metadata_dependency_companion_problems(
        identity: str, policy: dict[str, Any], message: str,
) -> list[dict[str, Any]]:
    descriptors = policy["pluginDependencyClosure"][
        "reviewedMetadataOnlyDependencyDescriptors"
    ]
    folded = identity.casefold()
    for descriptor in descriptors:
        root = descriptor.rsplit("/", 1)[0] + "/"
        if folded.startswith(root.casefold()) and identity != descriptor:
            return [issue(
                "METADATA_ONLY_DEPENDENCY_PLUGIN_COMPANION",
                message,
                identity,
            )]
    return []


def validate_policy(policy: Any) -> list[dict[str, Any]]:
    problems: list[dict[str, Any]] = []
    if type(policy) is not dict or set(policy) != POLICY_ROOT_KEYS:
        return [issue("POLICY_MALFORMED", "policy root keys or type differ")]
    expected_scalars = {
        "schema": "DiscGolfTour.Session19StagedProvenancePolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "windows_shipping_whole_archive_provenance_v1",
        "state": "IMPLEMENTED_PENDING_FRESH_WINDOWS_SHIPPING_CANDIDATE",
        "releaseReady": False,
        "closesReleaseBlockerWithoutAcceptedReceipt": False,
    }
    for key, expected in expected_scalars.items():
        if not typed_equal(policy[key], expected):
            problems.append(issue("POLICY_MALFORMED", f"policy {key} differs"))

    expected_target = {
        "platform": "Win64", "configuration": "Shipping", "milestone": "v0.5",
        "holes": [1, 2, 3], "cleanBuildRequired": True,
        "nonIterativeCookRequired": True, "pakRequired": True,
        "ioStoreRequired": True, "archiveRequired": True,
    }
    if not typed_equal(policy["target"], expected_target):
        problems.append(issue("POLICY_MALFORMED", "policy target differs"))
    expected_manifests = {
        "ufs": "Manifest_UFSFiles_Win64.txt",
        "nonUfs": "Manifest_NonUFSFiles_Win64.txt",
    }
    if not typed_equal(policy["manifestFiles"], expected_manifests):
        problems.append(issue("POLICY_MALFORMED", "manifest file authority differs"))

    expected_distribution_authority = {
        "ufsDevelopmentPathRows": "DIAGNOSTIC_NON_AUTHORITATIVE_FOR_DISTRIBUTION",
        "malformedStageManifestsFail": True,
        "authoritativeFinalIdentityScopes": [
            "ARCHIVE_FILE", "NONUFS_ENTRY", "CONTAINER_NAMED_ENTRY",
            "CONTAINER_ANONYMOUS_CHUNK",
        ],
        "exactDevelopmentPathExceptions": list(EXACT_DEVELOPMENT_PATH_EXCEPTIONS),
        "exceptionMatchMode": "EXACT_CASE_SENSITIVE_IDENTITY",
        "allOtherDevelopmentPathMatchesFail": True,
    }
    if not typed_equal(
            policy["distributionIdentityAuthority"], expected_distribution_authority):
        problems.append(issue(
            "POLICY_MALFORMED", "distribution identity authority differs"
        ))

    for key in (
        "requiredArchiveFiles", "requiredLooseDataFiles", "forbiddenArchiveExtensions",
        "forbiddenPathTokens", "forbiddenDevelopmentPathTokens", "forbiddenBinaryMarkers",
    ):
        if not unique_string_list(policy[key]):
            problems.append(issue("POLICY_MALFORMED", f"policy {key} is not a unique string array"))
    if unique_string_list(policy["forbiddenPathTokens"]):
        forbidden_folded = {token.casefold() for token in policy["forbiddenPathTokens"]}
        for token in LANDMASS_FINAL_PAYLOAD_TOKENS:
            if token.casefold() not in forbidden_folded:
                problems.append(issue(
                    "POLICY_MALFORMED",
                    f"project-specific Landmass final-payload subtree is missing: {token}",
                ))
        if "/engine/plugins/experimental/landmass/" in forbidden_folded:
            problems.append(issue(
                "POLICY_MALFORMED",
                "Landmass whole-root exclusion would reject its required descriptor",
            ))
    development_tokens = policy["forbiddenDevelopmentPathTokens"]
    if unique_string_list(development_tokens):
        configured = {token.casefold() for token in development_tokens}
        for root in REQUIRED_ENGINE_DEVELOPMENT_ROOTS:
            if root.casefold() not in configured:
                problems.append(issue(
                    "POLICY_MALFORMED", f"required engine development root is not classified: {root}"
                ))
        for identity in PROTECTED_RUNTIME_SLATE_IDENTITIES:
            normalized = "/" + identity + "/"
            if any(token.casefold() in normalized.casefold() for token in development_tokens):
                problems.append(issue(
                    "POLICY_MALFORMED", f"required runtime Slate identity is over-classified: {identity}"
                ))
        exceptions = policy["distributionIdentityAuthority"].get(
            "exactDevelopmentPathExceptions", []
        ) if type(policy["distributionIdentityAuthority"]) is dict else []
        if not unique_string_list(exceptions):
            problems.append(issue(
                "POLICY_MALFORMED", "development-path exceptions are malformed or case-collide"
            ))
        else:
            for identity in exceptions:
                if canonical_relative(identity) is None:
                    problems.append(issue(
                        "POLICY_MALFORMED", "development-path exception is not canonical", identity
                    ))
                elif development_path_token(identity, policy) is None:
                    problems.append(issue(
                        "POLICY_MALFORMED",
                        "development-path exception does not match a development path token",
                        identity,
                    ))

    expected_plugin_closure = {
        "capabilityPolicy": evidence_binding(PLUGIN_CAPABILITY_POLICY_PATH),
        "requiredFinalPayloadDescriptors": list(
            REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS
        ),
        "reviewedMetadataOnlyDependencyDescriptors": list(
            REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS
        ),
        "descriptorOnlyForbiddenSubdirectories": list(
            DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES
        ),
        "reviewedResidualMetadataExclusions": list(
            REVIEWED_RESIDUAL_METADATA_EXCLUSIONS
        ),
        "landmassDescriptor": LANDMASS_DESCRIPTOR,
        "landmassNeverCookVirtualRoot": "/Landmass",
        "landmassForbiddenFinalPayloadSubtrees": list(
            LANDMASS_FINAL_PAYLOAD_TOKENS
        ),
    }
    if not typed_equal(policy["pluginDependencyClosure"], expected_plugin_closure):
        problems.append(issue(
            "POLICY_MALFORMED", "plugin dependency-closure authority differs"
        ))
    try:
        plugin_policy = load_json_strict(PLUGIN_CAPABILITY_POLICY_PATH)
    except (OSError, UnicodeError, StrictJsonError, json.JSONDecodeError):
        plugin_policy = None
        problems.append(issue(
            "POLICY_MALFORMED", "plugin capability policy cannot be read as strict JSON"
        ))
    if type(plugin_policy) is dict:
        expected_metadata_records = [
            {"name": PurePosixPath(descriptor).stem, "descriptor": descriptor}
            for descriptor in REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS
        ]
        if (
                plugin_policy.get("schema")
                != "DiscGolfTour.Session19ShippingPluginCapabilityPolicy.v3"
                or plugin_policy.get("schemaVersion") != 3
                or len(plugin_policy.get("reviewedDisabledPlugins", [])) != 31
                or plugin_policy.get("requiredDependencyClosureFinalPayloadDescriptors")
                != list(REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS)
                or plugin_policy.get("reviewedMetadataOnlyDependencyDescriptors")
                != expected_metadata_records
                or plugin_policy.get("reviewedResidualMetadataExclusions")
                != list(REVIEWED_RESIDUAL_METADATA_EXCLUSIONS)):
            problems.append(issue(
                "POLICY_MALFORMED", "plugin capability policy v3 contract differs"
            ))

    container = policy["containerPolicy"]
    expected_container = {
        "extensionsInventoriedByUnrealPak": [".pak", ".utoc"],
        "pakInventoryMethod": "UnrealPak positional -List",
        "utocInventoryMethod": "UnrealPak -ListContainer=<utoc> -CSV=<csv>",
        "utocCompanionExtension": ".ucas",
        "requireEveryContainerInventorySuccess": True,
        "requirePakListSummaryCountMatch": True,
        "requireIoStoreCsvHeaderExact": True,
        "requireIoStoreChunkOrderContiguous": True,
        "requireEveryNamedEntryClassified": True,
        "requireEveryAnonymousChunkClassified": True,
        "allowDuplicateNamedEntriesWithinContainer": False,
        "allowDuplicatePhysicalPaths": False,
    }
    if not typed_equal(container, expected_container):
        problems.append(issue("POLICY_MALFORMED", "container policy differs"))

    uat = policy["uatLogRequirements"]
    if (
            type(uat) is not dict
            or set(uat) != {"requiredCaseInsensitiveTokens", "forbiddenCaseInsensitiveTokens"}
            or not unique_string_list(uat.get("requiredCaseInsensitiveTokens"))
            or not unique_string_list(uat.get("forbiddenCaseInsensitiveTokens"))):
        problems.append(issue("POLICY_MALFORMED", "UAT log requirements are malformed"))

    classification = policy["classificationManifest"]
    expected_classification_keys = {
        "schema", "schemaVersion", "requiredState", "recordScopes",
        "allowedClassifications", "requireExactIdentityRecords",
        "requireShippingApprovedTrue", "requireAuthorityId", "requireLicenseId",
        "requireExistingProjectEvidence", "allowUnclassified", "allowExtraRecords",
        "allowDuplicateRecords",
    }
    if type(classification) is not dict or set(classification) != expected_classification_keys:
        problems.append(issue("POLICY_MALFORMED", "classification policy keys differ"))
    else:
        if classification["schema"] != "DiscGolfTour.Session19StagedProvenanceClassification.v1":
            problems.append(issue("POLICY_MALFORMED", "classification schema differs"))
        if type(classification["schemaVersion"]) is not int or classification["schemaVersion"] != 1:
            problems.append(issue("POLICY_MALFORMED", "classification schemaVersion differs"))
        if classification["requiredState"] != "REVIEWED_FOR_WINDOWS_SHIPPING":
            problems.append(issue("POLICY_MALFORMED", "classification state differs"))
        if not unique_string_list(classification["recordScopes"]):
            problems.append(issue("POLICY_MALFORMED", "classification scopes are malformed"))
        if not unique_string_list(classification["allowedClassifications"]):
            problems.append(issue("POLICY_MALFORMED", "classification catalog is malformed"))
        for key in (
            "requireExactIdentityRecords", "requireShippingApprovedTrue",
            "requireAuthorityId", "requireLicenseId", "requireExistingProjectEvidence",
        ):
            if classification.get(key) is not True:
                problems.append(issue("POLICY_MALFORMED", f"classification {key} must be true"))
        for key in ("allowUnclassified", "allowExtraRecords", "allowDuplicateRecords"):
            if classification.get(key) is not False:
                problems.append(issue("POLICY_MALFORMED", f"classification {key} must be false"))

    receipt = policy["receipt"]
    expected_receipt = {
        "schema": "DiscGolfTour.Session19StagedProvenanceReceipt.v1",
        "schemaVersion": 1,
        "writeOnlyOnExplicitOutputPath": True,
        "recordHostPaths": False,
        "releaseReadyOnPass": False,
        "blockerClosedOnPass": False,
    }
    if not typed_equal(receipt, expected_receipt):
        problems.append(issue("POLICY_MALFORMED", "receipt boundary differs"))
    negative = policy["negativeFixture"]
    if (
            type(negative) is not dict
            or negative.get("expectedResult") != "REJECT"
            or negative.get("mayNeverSatisfyPositiveEvidence") is not True
            or not unique_string_list(negative.get("requiredReasonCodes"))):
        problems.append(issue("POLICY_MALFORMED", "negative fixture boundary differs"))
    return problems


def hash_file(path: Path, markers: Iterable[str] = ()) -> tuple[int, str, list[str], bool]:
    encoded_markers: list[tuple[str, bytes]] = []
    for marker in markers:
        encoded_markers.append((marker, marker.encode("utf-8").lower()))
        encoded_markers.append((marker, marker.encode("utf-16-le").lower()))
    maximum = max((len(value) for _, value in encoded_markers), default=1)
    found: set[str] = set()
    before = path.stat()
    digest = hashlib.sha256()
    carry = b""
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
            lowered = (carry + chunk).lower()
            for label, encoded in encoded_markers:
                if encoded in lowered:
                    found.add(label)
            carry = lowered[-(maximum - 1):] if maximum > 1 else b""
    after = path.stat()
    stable = before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns
    return after.st_size, digest.hexdigest().upper(), sorted(found), stable


def is_reparse(stat_result: os.stat_result) -> bool:
    attributes = getattr(stat_result, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & reparse_flag)


def collect_archive(
        archive_root: Path, policy: dict[str, Any]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    problems: list[dict[str, Any]] = []
    records: list[dict[str, Any]] = []
    if not archive_root.is_dir() or archive_root.is_symlink():
        return [], [issue("ARCHIVE_ROOT_INVALID", "archive root must be a regular directory")]
    root_resolved = archive_root.resolve()
    seen: set[str] = set()
    markers = policy["forbiddenBinaryMarkers"]
    for current, directories, filenames in os.walk(archive_root, followlinks=False):
        current_path = Path(current)
        safe_directories: list[str] = []
        for name in directories:
            directory = current_path / name
            try:
                info = directory.lstat()
            except OSError:
                problems.append(issue("ARCHIVE_ENTRY_UNREADABLE", "archive directory cannot be inspected"))
                continue
            if directory.is_symlink() or is_reparse(info):
                relative = directory.relative_to(archive_root).as_posix()
                problems.append(issue("ARCHIVE_REPARSE_ENTRY", "archive contains a reparse directory", relative))
                continue
            safe_directories.append(name)
        directories[:] = safe_directories
        for name in filenames:
            path = current_path / name
            relative = path.relative_to(archive_root).as_posix()
            canonical = canonical_relative(relative)
            if canonical is None:
                problems.append(issue("MALFORMED_ARCHIVE_PATH", "archive path is not canonical", relative))
                continue
            collision_key = canonical.casefold()
            if collision_key in seen:
                problems.append(issue("DUPLICATE_ARCHIVE_PATH", "archive path duplicates or case-collides", canonical))
                continue
            seen.add(collision_key)
            try:
                info = path.lstat()
            except OSError:
                problems.append(issue("ARCHIVE_ENTRY_UNREADABLE", "archive file cannot be inspected", canonical))
                continue
            if path.is_symlink() or is_reparse(info) or not stat.S_ISREG(info.st_mode):
                problems.append(issue("ARCHIVE_REPARSE_ENTRY", "archive entry is not a regular file", canonical))
                continue
            try:
                resolved = path.resolve(strict=True)
            except OSError:
                problems.append(issue("ARCHIVE_ENTRY_UNREADABLE", "archive file cannot be resolved", canonical))
                continue
            if root_resolved not in resolved.parents:
                problems.append(issue("ARCHIVE_PATH_ESCAPE", "archive file resolves outside archive", canonical))
                continue
            try:
                byte_count, digest, marker_hits, stable = hash_file(path, markers)
            except OSError:
                problems.append(issue("ARCHIVE_ENTRY_UNREADABLE", "archive file cannot be hashed", canonical))
                continue
            if not stable:
                problems.append(issue("ARCHIVE_CHANGED_DURING_HASH", "archive file changed during hashing", canonical))
            record = {
                "path": canonical,
                "bytes": byte_count,
                "sha256": digest,
                "distributionAuthoritative": True,
                "provenance": None,
            }
            records.append(record)
            extension = PurePosixPath(canonical).suffix.casefold()
            if extension in {value.casefold() for value in policy["forbiddenArchiveExtensions"]}:
                code = "FORBIDDEN_PDB" if extension == ".pdb" else "FORBIDDEN_ARCHIVE_EXTENSION"
                problems.append(issue(code, "archive contains a forbidden file extension", canonical))
            normalized = "/" + canonical + "/"
            for token in policy["forbiddenPathTokens"]:
                if token.casefold() in normalized.casefold():
                    code = (
                        "FORBIDDEN_FRAMEWORK_ARTIFACT"
                        if "characterframework" in token.casefold()
                        or "characterframework" in canonical.casefold()
                        else "FORBIDDEN_PATH"
                    )
                    problems.append(issue(code, "archive path matches a forbidden policy token", canonical))
                    break
            problems.extend(development_path_problems(
                canonical, policy, "archive path is development-only"
            ))
            problems.extend(metadata_dependency_companion_problems(
                canonical, policy, "archive contains a metadata-only plugin companion"
            ))
            if marker_hits:
                problems.append(issue(
                    "FORBIDDEN_FRAMEWORK_ARTIFACT",
                    "archive bytes contain a forbidden framework marker",
                    canonical,
                ))
    records.sort(key=lambda value: (value["path"].casefold(), value["path"]))
    return records, problems


def validate_utc(value: str) -> bool:
    if not UTC_RE.fullmatch(value):
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def parse_stage_manifest(
        path: Path, scope: str, policy: dict[str, Any]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    problems: list[dict[str, Any]] = []
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError):
        return [], [issue("MALFORMED_STAGE_MANIFEST", f"{scope} manifest is not strict UTF-8")]
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for line_number, line in enumerate(text.splitlines(), 1):
        fields = line.split("\t")
        if len(fields) != 2:
            problems.append(issue(
                "MALFORMED_STAGE_MANIFEST", f"{scope} row {line_number} must have two TSV fields"
            ))
            continue
        raw_path, timestamp = fields
        canonical = canonical_relative(raw_path)
        if canonical is None:
            problems.append(issue(
                "MALFORMED_STAGE_MANIFEST", f"{scope} row {line_number} path is not canonical"
            ))
            continue
        key = canonical.casefold()
        if key in seen:
            problems.append(issue(
                "DUPLICATE_STAGE_MANIFEST_ENTRY", f"{scope} contains a duplicate or case collision", canonical
            ))
            continue
        seen.add(key)
        if not validate_utc(timestamp):
            problems.append(issue(
                "MALFORMED_STAGE_MANIFEST", f"{scope} row {line_number} timestamp is invalid", canonical
            ))
        normalized = "/" + canonical + "/"
        forbidden_path_diagnostic = None
        for token in policy["forbiddenPathTokens"]:
            if token.casefold() in normalized.casefold():
                forbidden_path_diagnostic = token
                if scope != "UFS":
                    code = (
                        "FORBIDDEN_FRAMEWORK_ARTIFACT"
                        if "characterframework" in token.casefold()
                        else "FORBIDDEN_PATH"
                    )
                    problems.append(issue(
                        code, f"{scope} entry matches a forbidden policy token", canonical
                    ))
                break
        development_diagnostic = development_path_token(canonical, policy)
        if scope != "UFS":
            problems.extend(development_path_problems(
                canonical, policy, f"{scope} entry is development-only"
            ))
            problems.extend(metadata_dependency_companion_problems(
                canonical, policy, f"{scope} entry is a metadata-only plugin companion"
            ))
        records.append({
            "path": canonical,
            "timestampUtc": timestamp,
            "distributionAuthoritative": scope != "UFS",
            "developmentPathDiagnostic": (
                development_diagnostic if scope == "UFS" else None
            ),
            "forbiddenPathDiagnostic": (
                forbidden_path_diagnostic if scope == "UFS" else None
            ),
            "provenance": None,
        })
    if not records:
        problems.append(issue("MALFORMED_STAGE_MANIFEST", f"{scope} manifest contains no valid rows"))
    records.sort(key=lambda value: (value["path"].casefold(), value["path"]))
    return records, problems


def normalize_container_path(raw_path: str) -> tuple[str | None, bool]:
    if raw_path.startswith("<") and raw_path.endswith(">") and "/" not in raw_path and "\\" not in raw_path:
        return raw_path, True
    value = raw_path.replace("\\", "/")
    if value.startswith("../../../"):
        value = value[len("../../../"):]
    elif value.startswith("../"):
        return None, False
    canonical = canonical_relative(value)
    return canonical, False


def parse_unrealpak_listing(
        text: str, extension: str, container_path: str, policy: dict[str, Any]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, Any] | None]:
    problems: list[dict[str, Any]] = []
    entries: list[dict[str, Any]] = []
    if extension != ".pak":
        return [], [issue(
            "MALFORMED_CONTAINER_LIST",
            "positional UnrealPak listing is authorized only for .pak containers",
            container_path,
        )], None
    entry_regex = PAK_ENTRY_RE
    summary_regex = PAK_SUMMARY_RE
    summaries: list[re.Match[str]] = []
    named_seen: set[str] = set()
    physical_seen: set[tuple[int, int, str]] = set()
    anonymous_ordinal = 0
    for line in text.splitlines():
        match = entry_regex.fullmatch(line)
        if match is not None:
            normalized, anonymous = normalize_container_path(match.group("path"))
            if normalized is None:
                problems.append(issue(
                    "MALFORMED_CONTAINER_LIST", "container entry path is not canonical", container_path
                ))
                continue
            offset = int(match.group("offset"))
            byte_count = int(match.group("size"))
            digest = match.group("hash").upper()
            physical_key = (offset, byte_count, digest)
            if physical_key in physical_seen:
                problems.append(issue(
                    "DUPLICATE_CONTAINER_ENTRY", "container repeats an offset/size/hash identity", container_path
                ))
            physical_seen.add(physical_key)
            if anonymous:
                anonymous_ordinal += 1
                identity = (
                    f"{normalized}#{anonymous_ordinal:06d}@{offset:016d}:"
                    f"{byte_count:016d}:{digest}"
                )
                scope = "CONTAINER_ANONYMOUS_CHUNK"
            else:
                identity = normalized
                scope = "CONTAINER_NAMED_ENTRY"
                collision = identity.casefold()
                if collision in named_seen:
                    problems.append(issue(
                        "DUPLICATE_CONTAINER_ENTRY", "container named entry duplicates or case-collides", identity
                    ))
                named_seen.add(collision)
            normalized_for_scan = "/" + identity + "/"
            for token in policy["forbiddenPathTokens"]:
                if token.casefold() in normalized_for_scan.casefold():
                    code = "FORBIDDEN_FRAMEWORK_ARTIFACT" if "characterframework" in token.casefold() else "FORBIDDEN_PATH"
                    problems.append(issue(code, "container entry matches a forbidden policy token", identity))
                    break
            problems.extend(development_path_problems(
                identity, policy, "container entry is development-only"
            ))
            problems.extend(metadata_dependency_companion_problems(
                identity, policy, "container entry is a metadata-only plugin companion"
            ))
            entries.append({
                "scope": scope,
                "identity": identity,
                "offset": offset,
                "bytes": byte_count,
                "sha1": digest,
                "compression": match.group("compression"),
                "distributionAuthoritative": True,
                "provenance": None,
            })
            continue
        summary_match = summary_regex.fullmatch(line)
        if summary_match is not None:
            summaries.append(summary_match)
            continue
        if line.startswith("LogPakFile: Display:") and " offset:" in line:
            problems.append(issue(
                "MALFORMED_CONTAINER_LIST", "UnrealPak emitted an unparseable entry row", container_path
            ))
    if len(summaries) != 1:
        problems.append(issue(
            "MALFORMED_CONTAINER_LIST", "UnrealPak list must contain exactly one file summary", container_path
        ))
        return entries, problems, None
    summary_match = summaries[0]
    summary = {
        "entryCount": int(summary_match.group("count")),
        "entryBytes": int(summary_match.group("bytes")),
    }
    if summary["entryCount"] != len(entries):
        problems.append(issue(
            "CONTAINER_LIST_COUNT_MISMATCH", "UnrealPak summary count differs from parsed entries", container_path
        ))
    observed_bytes = sum(entry["bytes"] for entry in entries)
    if summary["entryBytes"] != observed_bytes:
        problems.append(issue(
            "CONTAINER_LIST_BYTES_MISMATCH", "UnrealPak summary bytes differ from parsed entries", container_path
        ))
    return entries, problems, summary


def parse_iostore_chunk_csv(
        text: str, container_path: str, expected_container_name: str,
        policy: dict[str, Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, Any] | None]:
    """Strictly parse UnrealPak's FIoStoreReader chunk-enumeration CSV."""
    problems: list[dict[str, Any]] = []
    entries: list[dict[str, Any]] = []
    if not text or not text.endswith(("\n", "\r")):
        return [], [issue(
            "MALFORMED_IOSTORE_CHUNK_CSV",
            "IoStore chunk CSV is empty or lacks a complete terminal row",
            container_path,
        )], None
    try:
        rows = list(csv.reader(text.splitlines(), skipinitialspace=True, strict=True))
    except csv.Error:
        return [], [issue(
            "MALFORMED_IOSTORE_CHUNK_CSV", "IoStore chunk CSV syntax is invalid", container_path
        )], None
    if not rows or tuple(rows[0]) != IOSTORE_CSV_HEADER:
        return [], [issue(
            "MALFORMED_IOSTORE_CHUNK_CSV",
            "IoStore chunk CSV header differs from the engine authority",
            container_path,
        )], None
    if len(rows) == 1:
        return [], [issue(
            "MALFORMED_IOSTORE_CHUNK_CSV",
            "IoStore chunk enumeration produced no chunk rows",
            container_path,
        )], None

    chunk_ids_seen: set[str] = set()
    named_seen: set[str] = set()
    physical_seen: set[tuple[int, int, int]] = set()
    for expected_order, row in enumerate(rows[1:]):
        if len(row) != len(IOSTORE_CSV_HEADER):
            problems.append(issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                f"IoStore chunk row {expected_order} has the wrong field count",
                container_path,
            ))
            continue
        values = dict(zip(IOSTORE_CSV_HEADER, row))
        numeric_names = (
            "OrderInContainer", "Offset", "OffsetOnDisk", "Size", "CompressedSize",
            "PartitionIndex", "OffsetInPartition",
        )
        if any(not UNSIGNED_INTEGER_RE.fullmatch(values[name]) for name in numeric_names):
            problems.append(issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                f"IoStore chunk row {expected_order} has a malformed integer",
                container_path,
            ))
            continue
        numeric = {name: int(values[name]) for name in numeric_names}
        if numeric["OrderInContainer"] != expected_order:
            problems.append(issue(
                "IOSTORE_CHUNK_ORDER_MISMATCH",
                "IoStore chunk order is not unique and contiguous from zero",
                container_path,
            ))
        if any(value > 0xFFFFFFFFFFFFFFFF for value in numeric.values()):
            problems.append(issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                f"IoStore chunk row {expected_order} exceeds unsigned 64-bit range",
                container_path,
            ))
            continue
        chunk_id = values["ChunkId"]
        package_id = values["PackageId"]
        chunk_hash_value = values["Hash"]
        chunk_type = values["ChunkType"]
        if (
                not IOSTORE_CHUNK_ID_RE.fullmatch(chunk_id)
                or not IOSTORE_PACKAGE_ID_RE.fullmatch(package_id)
                or not chunk_hash_value.startswith("0x")
                or not SHA1_RE.fullmatch(chunk_hash_value[2:])
                or not IOSTORE_CHUNK_TYPE_RE.fullmatch(chunk_type)
                or values["ContainerName"] != expected_container_name):
            problems.append(issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                f"IoStore chunk row {expected_order} has malformed identity metadata",
                container_path,
            ))
            continue
        chunk_key = chunk_id.casefold()
        if chunk_key in chunk_ids_seen:
            problems.append(issue(
                "DUPLICATE_CONTAINER_ENTRY", "IoStore container repeats a chunk ID", chunk_id
            ))
        chunk_ids_seen.add(chunk_key)
        physical_key = (
            numeric["PartitionIndex"], numeric["OffsetInPartition"], numeric["CompressedSize"]
        )
        if physical_key in physical_seen:
            problems.append(issue(
                "DUPLICATE_CONTAINER_ENTRY",
                "IoStore container repeats a partition/offset/compressed-size identity",
                chunk_id,
            ))
        physical_seen.add(physical_key)

        normalized, anonymous = normalize_container_path(values["Filename"])
        if normalized is None:
            problems.append(issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                f"IoStore chunk row {expected_order} filename is not canonical",
                container_path,
            ))
            continue
        digest = chunk_hash_value[2:].upper()
        if anonymous:
            identity = (
                f"{normalized}#{chunk_id.upper()}@{numeric['Offset']:016d}:"
                f"{numeric['Size']:016d}:{digest}"
            )
            scope = "CONTAINER_ANONYMOUS_CHUNK"
        else:
            identity = normalized
            scope = "CONTAINER_NAMED_ENTRY"
            collision = identity.casefold()
            if collision in named_seen:
                problems.append(issue(
                    "DUPLICATE_CONTAINER_ENTRY",
                    "IoStore named entry duplicates or case-collides",
                    identity,
                ))
            named_seen.add(collision)

        normalized_for_scan = "/" + identity + "/"
        for token in policy["forbiddenPathTokens"]:
            if token.casefold() in normalized_for_scan.casefold():
                code = (
                    "FORBIDDEN_FRAMEWORK_ARTIFACT"
                    if "characterframework" in token.casefold() else "FORBIDDEN_PATH"
                )
                problems.append(issue(code, "IoStore chunk matches a forbidden policy token", identity))
                break
        problems.extend(development_path_problems(
            identity, policy, "IoStore chunk is development-only"
        ))
        problems.extend(metadata_dependency_companion_problems(
            identity, policy, "IoStore chunk is a metadata-only plugin companion"
        ))
        entries.append({
            "scope": scope,
            "identity": identity,
            "chunkId": chunk_id.upper(),
            "chunkType": chunk_type,
            "offset": numeric["Offset"],
            "offsetOnDisk": numeric["OffsetOnDisk"],
            "bytes": numeric["Size"],
            "compressedBytes": numeric["CompressedSize"],
            "ioHash": digest,
            "partitionIndex": numeric["PartitionIndex"],
            "offsetInPartition": numeric["OffsetInPartition"],
            "distributionAuthoritative": True,
            "provenance": None,
        })
    summary = {
        "entryCount": len(entries),
        "entryBytes": sum(entry["bytes"] for entry in entries),
        "compressedBytes": sum(entry["compressedBytes"] for entry in entries),
    }
    if len(entries) != len(rows) - 1:
        problems.append(issue(
            "IOSTORE_CHUNK_COUNT_MISMATCH",
            "not every IoStore CSV data row produced an inventory entry",
            container_path,
        ))
    return entries, problems, summary


def find_unrealpak(explicit: Path | None = None) -> Path | None:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(explicit)
    environment = os.environ.get("UE_UNREALPAK")
    if environment:
        candidates.append(Path(environment))
    found = shutil.which("UnrealPak.exe") or shutil.which("UnrealPak")
    if found:
        candidates.append(Path(found))
    epic_root = Path("C:/Program Files/Epic Games")
    if epic_root.is_dir():
        candidates.extend(sorted(
            epic_root.glob("UE_*/Engine/Binaries/Win64/UnrealPak.exe"), reverse=True
        ))
    for candidate in candidates:
        try:
            if candidate.is_file() and not candidate.is_symlink():
                return candidate.resolve()
        except OSError:
            continue
    return None


def inventory_containers(
        archive_root: Path,
        archive_files: list[dict[str, Any]],
        unrealpak: Path | None,
        policy: dict[str, Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, Any] | None]:
    problems: list[dict[str, Any]] = []
    container_paths = [
        item["path"] for item in archive_files
        if PurePosixPath(item["path"]).suffix.casefold() in {".pak", ".utoc"}
    ]
    ucas_paths = {
        item["path"] for item in archive_files
        if PurePosixPath(item["path"]).suffix.casefold() == ".ucas"
    }
    pak_count = sum(PurePosixPath(value).suffix.casefold() == ".pak" for value in container_paths)
    utoc_paths = [value for value in container_paths if PurePosixPath(value).suffix.casefold() == ".utoc"]
    if pak_count == 0:
        problems.append(issue("REQUIRED_CONTAINER_MISSING", "archive contains no .pak container"))
    if not utoc_paths or not ucas_paths:
        problems.append(issue("REQUIRED_CONTAINER_MISSING", "archive lacks an IoStore .utoc/.ucas pair"))
    for utoc in utoc_paths:
        expected_ucas = str(PurePosixPath(utoc).with_suffix(".ucas"))
        if expected_ucas not in ucas_paths:
            problems.append(issue("IOSTORE_COMPANION_MISSING", ".utoc lacks its exact .ucas companion", utoc))
    for ucas in sorted(ucas_paths, key=str.casefold):
        expected_utoc = str(PurePosixPath(ucas).with_suffix(".utoc"))
        if expected_utoc not in utoc_paths:
            problems.append(issue("IOSTORE_COMPANION_MISSING", ".ucas lacks its exact .utoc companion", ucas))

    if unrealpak is None:
        problems.append(issue("UNREALPAK_UNAVAILABLE", "UnrealPak executable was not found"))
        return [], problems, None
    try:
        tool_bytes, tool_hash, _, stable = hash_file(unrealpak)
    except OSError:
        problems.append(issue("UNREALPAK_UNAVAILABLE", "UnrealPak executable cannot be hashed"))
        return [], problems, None
    if not stable:
        problems.append(issue("UNREALPAK_UNSTABLE", "UnrealPak executable changed during hashing"))
    tool_binding = {"name": unrealpak.name, "bytes": tool_bytes, "sha256": tool_hash}

    containers: list[dict[str, Any]] = []
    archive_by_path = {item["path"]: item for item in archive_files}
    for relative in sorted(container_paths, key=lambda value: (value.casefold(), value)):
        extension = PurePosixPath(relative).suffix.casefold()
        absolute_container = archive_root.joinpath(*PurePosixPath(relative).parts)
        method = "PAK_POSITIONAL_LIST" if extension == ".pak" else "IOSTORE_CHUNK_CSV"
        output = ""
        csv_text: str | None = None
        try:
            if extension == ".pak":
                command = [str(unrealpak), str(absolute_container), "-List"]
                completed = subprocess.run(
                    command,
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    timeout=300,
                )
            else:
                with tempfile.TemporaryDirectory(prefix="dgtour-iostore-") as temporary_name:
                    csv_path = Path(temporary_name) / "chunks.csv"
                    command = [
                        str(unrealpak),
                        f"-ListContainer={absolute_container}",
                        f"-CSV={csv_path}",
                    ]
                    completed = subprocess.run(
                        command,
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        timeout=300,
                    )
                    if csv_path.is_file() and not csv_path.is_symlink():
                        csv_text = csv_path.read_text(encoding="utf-8-sig", errors="strict")
        except (OSError, UnicodeError, subprocess.TimeoutExpired):
            problems.append(issue(
                "UNREALPAK_INVENTORY_FAILED",
                "UnrealPak container inventory could not complete or emit strict UTF-8",
                relative,
            ))
            containers.append({
                "path": relative, "extension": extension, "inventoryMethod": method,
                "inventoryExitCode": None, "summary": None, "entries": [],
            })
            continue
        try:
            output = completed.stdout.decode("utf-8", errors="strict")
        except UnicodeError:
            problems.append(issue(
                "MALFORMED_CONTAINER_INVENTORY",
                "UnrealPak process output is not strict UTF-8",
                relative,
            ))
        if completed.returncode != 0:
            problems.append(issue(
                "UNREALPAK_INVENTORY_FAILED",
                "UnrealPak container inventory returned a nonzero exit code",
                relative,
            ))
        if extension == ".pak":
            entries, parse_problems, summary = parse_unrealpak_listing(
                output, extension, relative, policy
            )
        elif csv_text is None:
            entries, summary = [], None
            parse_problems = [issue(
                "MALFORMED_IOSTORE_CHUNK_CSV",
                "UnrealPak did not create a regular IoStore chunk CSV",
                relative,
            )]
        else:
            entries, parse_problems, summary = parse_iostore_chunk_csv(
                csv_text, relative, absolute_container.stem, policy
            )
        problems.extend(parse_problems)
        containers.append({
            "path": relative,
            "extension": extension,
            "bytes": archive_by_path[relative]["bytes"],
            "sha256": archive_by_path[relative]["sha256"],
            "inventoryMethod": method,
            "inventoryExitCode": completed.returncode,
            "summary": summary,
            "entries": entries,
        })
    return containers, problems, tool_binding


def parse_uat_log(path: Path, policy: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    problems: list[dict[str, Any]] = []
    try:
        byte_count, digest, _, stable = hash_file(path)
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError):
        return {"bytes": 0, "sha256": "0" * 64}, [
            issue("UAT_LOG_INVALID", "UAT log cannot be read and hashed as strict UTF-8")
        ]
    if not stable:
        problems.append(issue("UAT_LOG_INVALID", "UAT log changed during hashing"))
    lowered = text.casefold()
    requirements = policy["uatLogRequirements"]
    shipping_marker_present = "-clientconfig=shipping" in lowered
    for token in requirements["requiredCaseInsensitiveTokens"]:
        if token.casefold() not in lowered:
            code = "NOT_SHIPPING_BUILD" if token.casefold() == "-clientconfig=shipping" else "UAT_REQUIRED_MARKER_MISSING"
            problems.append(issue(code, f"UAT log lacks required marker: {token}"))
    for token in requirements["forbiddenCaseInsensitiveTokens"]:
        if token.casefold() in lowered:
            code = (
                "NOT_SHIPPING_BUILD"
                if "development" in token.casefold()
                else "ITERATIVE_OR_FAILED_BUILD"
            )
            problems.append(issue(code, f"UAT log contains forbidden marker: {token}"))
    return {
        "bytes": byte_count,
        "sha256": digest,
        "shippingConfigurationObserved": shipping_marker_present,
    }, problems


IdentityKey = tuple[str, str | None, str]


def identity_key(scope: str, container: str | None, identity: str) -> IdentityKey:
    return scope, container.casefold() if container is not None else None, identity.casefold()


def discovered_identities(
        archive_files: list[dict[str, Any]],
        ufs_entries: list[dict[str, Any]],
        nonufs_entries: list[dict[str, Any]],
        containers: list[dict[str, Any]],
) -> tuple[dict[IdentityKey, tuple[str, str | None, str]], list[dict[str, Any]]]:
    result: dict[IdentityKey, tuple[str, str | None, str]] = {}
    problems: list[dict[str, Any]] = []
    values: list[tuple[str, str | None, str]] = []
    values.extend(("ARCHIVE_FILE", None, item["path"]) for item in archive_files)
    values.extend(("UFS_ENTRY", None, item["path"]) for item in ufs_entries)
    values.extend(("NONUFS_ENTRY", None, item["path"]) for item in nonufs_entries)
    for container in containers:
        values.extend(
            (entry["scope"], container["path"], entry["identity"])
            for entry in container["entries"]
        )
    for scope, container, identity in values:
        key = identity_key(scope, container, identity)
        if key in result:
            problems.append(issue(
                "DUPLICATE_DISCOVERED_IDENTITY",
                "the exhaustive inventory contains a duplicate classification identity",
                identity,
            ))
        else:
            result[key] = (scope, container, identity)
    return result, problems


def required_dependency_descriptor_problems(
        final_named_identities: Iterable[str], policy: dict[str, Any],
) -> list[dict[str, Any]]:
    exact = set(final_named_identities)
    folded = {identity.casefold(): identity for identity in exact}
    problems: list[dict[str, Any]] = []
    for required in policy["pluginDependencyClosure"]["requiredFinalPayloadDescriptors"]:
        if required in exact:
            continue
        problems.append(issue(
            "REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTOR_MISSING",
            "required final-payload plugin dependency descriptor is absent",
            required,
        ))
        observed = folded.get(required.casefold())
        if observed is not None:
            problems.append(issue(
                "DEPENDENCY_CLOSURE_DESCRIPTOR_CASE_MISMATCH",
                f"required descriptor was observed with non-authoritative case: {observed}",
                required,
            ))
    return problems


def validate_classification_manifest(
        document: Any,
        candidate_id: str,
        policy: dict[str, Any],
        project_root: Path,
        discovered: dict[IdentityKey, tuple[str, str | None, str]],
        *,
        check_evidence: bool,
) -> tuple[dict[IdentityKey, dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    problems: list[dict[str, Any]] = []
    if type(document) is not dict or set(document) != CLASSIFICATION_ROOT_KEYS:
        return {}, [issue("CLASSIFICATION_MANIFEST_MALFORMED", "classification root keys or type differ")], []
    classification_policy = policy["classificationManifest"]
    expected_scalars = {
        "schema": classification_policy["schema"],
        "schemaVersion": classification_policy["schemaVersion"],
        "candidateId": candidate_id,
        "state": classification_policy["requiredState"],
    }
    for key, expected in expected_scalars.items():
        if not typed_equal(document[key], expected):
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"classification {key} differs"))
    records = document["records"]
    if type(records) is not list:
        return {}, problems + [issue("CLASSIFICATION_MANIFEST_MALFORMED", "classification records must be an array")], []
    scopes = set(classification_policy["recordScopes"])
    classifications = set(classification_policy["allowedClassifications"])
    mapping: dict[IdentityKey, dict[str, Any]] = {}
    for index, record in enumerate(records):
        if type(record) is not dict or set(record) != CLASSIFICATION_RECORD_KEYS:
            problems.append(issue(
                "CLASSIFICATION_MANIFEST_MALFORMED", f"classification record {index} keys or type differ"
            ))
            continue
        scope = record["scope"]
        container = record["container"]
        identity = record["identity"]
        if type(scope) is not str or scope not in scopes:
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} scope is invalid"))
            continue
        if scope.startswith("CONTAINER_"):
            if canonical_relative(container) is None:
                problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} container is invalid"))
                continue
        elif container is not None:
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} container must be null"))
            continue
        if scope == "CONTAINER_ANONYMOUS_CHUNK":
            if type(identity) is not str or not ANONYMOUS_ID_RE.fullmatch(identity):
                problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} anonymous identity is invalid"))
                continue
        elif canonical_relative(identity) is None:
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} identity is invalid"))
            continue
        if type(record["classification"]) is not str or record["classification"] not in classifications:
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} classification is invalid"))
        if type(record["authorityId"]) is not str or not AUTHORITY_RE.fullmatch(record["authorityId"]):
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} authorityId is invalid"))
        if type(record["licenseId"]) is not str or not LICENSE_RE.fullmatch(record["licenseId"]):
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} licenseId is invalid"))
        if record["shippingApproved"] is not True:
            problems.append(issue("CLASSIFICATION_NOT_APPROVED", f"record {index} is not Shipping-approved", identity if type(identity) is str else None))
        evidence_paths = record["evidencePaths"]
        if not unique_string_list(evidence_paths) or not evidence_paths:
            problems.append(issue("CLASSIFICATION_MANIFEST_MALFORMED", f"record {index} evidencePaths are invalid"))
        elif check_evidence:
            for evidence_path in evidence_paths:
                if project_evidence_path(evidence_path, project_root) is None:
                    problems.append(issue(
                        "CLASSIFICATION_EVIDENCE_MISSING",
                        f"record {index} evidence path is absent, noncanonical, or escapes the project",
                        evidence_path,
                    ))
        if type(identity) is not str:
            continue
        key = identity_key(scope, container, identity)
        if key in mapping:
            problems.append(issue("DUPLICATE_CLASSIFICATION", "classification record duplicates or case-collides", identity))
        else:
            mapping[key] = record

    missing_keys = sorted(set(discovered) - set(mapping))
    extra_keys = sorted(set(mapping) - set(discovered))
    template_records: list[dict[str, Any]] = []
    for key in missing_keys:
        scope, container, identity = discovered[key]
        template_records.append({
            "scope": scope,
            "container": container,
            "identity": identity,
            "classification": "UNCLASSIFIED",
            "authorityId": "PENDING",
            "licenseId": "PENDING",
            "shippingApproved": False,
            "evidencePaths": ["Config/DG_Session19StagedProvenancePolicy.json"],
        })
    if missing_keys:
        problems.append(issue(
            "UNCLASSIFIED_IDENTITY", f"{len(missing_keys)} exhaustive identities lack exact classification"
        ))
    if extra_keys:
        problems.append(issue(
            "EXTRA_CLASSIFICATION_IDENTITY", f"{len(extra_keys)} classification records do not exist in the candidate"
        ))
    return mapping, problems, template_records


def attach_provenance(
        mapping: dict[IdentityKey, dict[str, Any]],
        archive_files: list[dict[str, Any]],
        ufs_entries: list[dict[str, Any]],
        nonufs_entries: list[dict[str, Any]],
        containers: list[dict[str, Any]],
) -> None:
    def public(record: dict[str, Any] | None) -> dict[str, Any] | None:
        if record is None:
            return None
        return {
            "classification": record["classification"],
            "authorityId": record["authorityId"],
            "licenseId": record["licenseId"],
            "shippingApproved": record["shippingApproved"],
            "evidencePaths": record["evidencePaths"],
        }

    for item in archive_files:
        item["provenance"] = public(mapping.get(identity_key("ARCHIVE_FILE", None, item["path"])))
    for item in ufs_entries:
        item["provenance"] = public(mapping.get(identity_key("UFS_ENTRY", None, item["path"])))
    for item in nonufs_entries:
        item["provenance"] = public(mapping.get(identity_key("NONUFS_ENTRY", None, item["path"])))
    for container in containers:
        for entry in container["entries"]:
            entry["provenance"] = public(mapping.get(identity_key(
                entry["scope"], container["path"], entry["identity"]
            )))


def canonical_archive_manifest_hash(archive_files: list[dict[str, Any]]) -> str:
    payload = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n"
        for item in archive_files
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def audit_candidate(
        *,
        archive_root: Path,
        uat_log: Path,
        classification_path: Path | None,
        candidate_id: str,
        policy: dict[str, Any],
        project_root: Path,
        unrealpak: Path | None,
) -> tuple[dict[str, Any], dict[str, Any]]:
    problems = validate_policy(policy)
    if not CANDIDATE_ID_RE.fullmatch(candidate_id):
        problems.append(issue("CANDIDATE_ID_INVALID", "candidate ID is not a sanitized token"))

    archive_files, archive_problems = collect_archive(archive_root, policy)
    problems.extend(archive_problems)
    archive_by_path = {item["path"]: item for item in archive_files}
    for required in policy["requiredArchiveFiles"] + policy["requiredLooseDataFiles"]:
        if required not in archive_by_path:
            problems.append(issue("REQUIRED_ARCHIVE_FILE_MISSING", "required archive file is absent", required))

    ufs_path = archive_root / policy["manifestFiles"]["ufs"]
    nonufs_path = archive_root / policy["manifestFiles"]["nonUfs"]
    ufs_entries, ufs_problems = parse_stage_manifest(ufs_path, "UFS", policy)
    nonufs_entries, nonufs_problems = parse_stage_manifest(nonufs_path, "NONUFS", policy)
    problems.extend(ufs_problems)
    problems.extend(nonufs_problems)
    overlap = {item["path"].casefold() for item in ufs_entries} & {
        item["path"].casefold() for item in nonufs_entries
    }
    if overlap:
        problems.append(issue(
            "DUPLICATE_STAGE_MANIFEST_ENTRY",
            f"{len(overlap)} paths appear in both UFS and NonUFS manifests",
        ))

    containers, container_problems, tool_binding = inventory_containers(
        archive_root, archive_files, unrealpak, policy
    )
    problems.extend(container_problems)
    uat_binding, uat_problems = parse_uat_log(uat_log, policy)
    problems.extend(uat_problems)

    discovered, discovered_problems = discovered_identities(
        archive_files, ufs_entries, nonufs_entries, containers
    )
    problems.extend(discovered_problems)
    final_named_identities = {
        entry[2]
        for entry in discovered.values()
        if entry[0] == "CONTAINER_NAMED_ENTRY"
    }
    problems.extend(required_dependency_descriptor_problems(
        final_named_identities, policy
    ))
    classification_binding: dict[str, Any] | None = None
    mapping: dict[IdentityKey, dict[str, Any]] = {}
    template_records: list[dict[str, Any]] = []
    if classification_path is None:
        problems.append(issue(
            "CLASSIFICATION_MANIFEST_MISSING", "an exact reviewed classification manifest is required"
        ))
        template_records = [
            {
                "scope": scope,
                "container": container,
                "identity": identity,
                "classification": "UNCLASSIFIED",
                "authorityId": "PENDING",
                "licenseId": "PENDING",
                "shippingApproved": False,
                "evidencePaths": ["Config/DG_Session19StagedProvenancePolicy.json"],
            }
            for scope, container, identity in sorted(discovered.values(), key=lambda value: (
                value[0], (value[1] or "").casefold(), value[2].casefold(), value[2]
            ))
        ]
        if template_records:
            problems.append(issue(
                "UNCLASSIFIED_IDENTITY",
                f"{len(template_records)} exhaustive identities lack exact classification",
            ))
    else:
        try:
            classification_bytes, classification_hash, _, stable = hash_file(classification_path)
            document = load_json_strict(classification_path)
        except (OSError, StrictJsonError):
            problems.append(issue(
                "CLASSIFICATION_MANIFEST_MALFORMED", "classification manifest cannot be strictly loaded"
            ))
            document = None
        else:
            classification_binding = {
                "bytes": classification_bytes,
                "sha256": classification_hash,
            }
            if not stable:
                problems.append(issue(
                    "CLASSIFICATION_MANIFEST_MALFORMED", "classification manifest changed while hashing"
                ))
            mapping, classification_problems, template_records = validate_classification_manifest(
                document, candidate_id, policy, project_root, discovered,
                check_evidence=True,
            )
            problems.extend(classification_problems)
    attach_provenance(mapping, archive_files, ufs_entries, nonufs_entries, containers)

    unique_codes = sorted({item["code"] for item in problems})
    passed = not problems
    receipt = {
        "schema": policy["receipt"]["schema"],
        "schemaVersion": policy["receipt"]["schemaVersion"],
        "session": 19,
        "candidateId": candidate_id,
        "generatedUtc": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "status": "PASS_EXHAUSTIVE_TECHNICAL_INVENTORY" if passed else "FAIL_CLOSED",
        "target": copy.deepcopy(policy["target"]),
        "archive": {
            "fileCount": len(archive_files),
            "bytes": sum(item["bytes"] for item in archive_files),
            "canonicalManifestSha256": canonical_archive_manifest_hash(archive_files),
            "files": archive_files,
        },
        "uatLog": uat_binding,
        "stageManifests": {
            "ufs": {
                "entryCount": len(ufs_entries),
                "developmentPathAuthority": (
                    policy["distributionIdentityAuthority"]["ufsDevelopmentPathRows"]
                ),
                "malformedRowsFailClosed": True,
                "developmentPathDiagnosticCount": sum(
                    item["developmentPathDiagnostic"] is not None for item in ufs_entries
                ),
                "forbiddenPathDiagnosticCount": sum(
                    item["forbiddenPathDiagnostic"] is not None for item in ufs_entries
                ),
                "entries": ufs_entries,
            },
            "nonUfs": {
                "entryCount": len(nonufs_entries),
                "developmentPathAuthority": "POLICY_ENFORCED",
                "malformedRowsFailClosed": True,
                "developmentPathDiagnosticCount": 0,
                "forbiddenPathDiagnosticCount": 0,
                "entries": nonufs_entries,
            },
        },
        "unrealPak": tool_binding,
        "containers": containers,
        "classification": {
            "manifest": classification_binding,
            "discoveredIdentityCount": len(discovered),
            "classifiedIdentityCount": len(mapping),
            "unclassifiedIdentityCount": len(template_records),
        },
        "reasonCodes": unique_codes,
        "issues": problems,
        "releaseBoundary": {
            "technicalInventoryComplete": passed,
            "releaseReady": False,
            "releaseUseAllowed": False,
            "blockerClosed": False,
            "remainingEvidence": [
                "BIND_PASSING_RECEIPT_TO_ADDITIVE_RELEASE_AUTHORITY",
                "INDEPENDENT_PROVENANCE_REVIEW",
                "ALL_OTHER_SESSION19_RELEASE_GATES",
            ],
        },
    }
    template = {
        "schema": policy["classificationManifest"]["schema"],
        "schemaVersion": policy["classificationManifest"]["schemaVersion"],
        "candidateId": candidate_id,
        "state": "DRAFT_UNREVIEWED_NOT_SHIPPING_APPROVED",
        "records": template_records,
    }
    return receipt, template


def write_json_explicit(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise FileExistsError(
            f"explicit evidence output already exists; refusing overwrite: {path}"
        ) from exc


def run_unit_self_tests(policy: dict[str, Any], project_root: Path) -> tuple[int, list[str]]:
    failures: list[str] = []
    count = 0

    parser_cases = [
        '{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}', '{"a":-Infinity}',
    ]
    for text in parser_cases:
        count += 1
        try:
            load_json_text_strict(text)
        except (StrictJsonError, json.JSONDecodeError):
            pass
        else:
            failures.append("strict JSON adversarial case survived")

    path_cases = {
        "ok/file.bin": True,
        "": False,
        "../escape": False,
        "ok/../escape": False,
        "/absolute": False,
        "C:/host": False,
        "back\\slash": False,
        "dot/./file": False,
        " trailing": False,
        "double//slash": False,
    }
    for value, expected in path_cases.items():
        count += 1
        if (canonical_relative(value) is not None) is not expected:
            failures.append(f"canonical path case differed: {value!r}")

    valid_manifest = (
        "DiscGolfTour/A.bin\t2026-08-25T01:02:03.004Z\n"
        "Engine/B.bin\t2026-08-25T01:02:03Z\n"
    )
    manifest_mutations = [
        valid_manifest,
        valid_manifest.replace("\t", " ", 1),
        valid_manifest.replace("DiscGolfTour/A.bin", "../escape", 1),
        valid_manifest.replace("2026-08-25T01:02:03.004Z", "not-utc", 1),
        valid_manifest + "DiscGolfTour/A.bin\t2026-08-25T01:02:03Z\n",
    ]
    with tempfile.TemporaryDirectory() as temporary_name:
        temporary = Path(temporary_name)
        for index, content in enumerate(manifest_mutations):
            count += 1
            path = temporary / f"manifest-{index}.txt"
            path.write_text(content, encoding="utf-8")
            _, problems = parse_stage_manifest(path, "SELFTEST", policy)
            expected_valid = index == 0
            if (not problems) is not expected_valid:
                failures.append(f"stage manifest mutation {index} result differed")
        diagnostic_identity = (
            "Engine/Plugins/Editor/AssetManagerEditor/AssetManagerEditor.uplugin"
        )
        diagnostic_path = temporary / "manifest-development-diagnostic.txt"
        diagnostic_path.write_text(
            f"{diagnostic_identity}\t2026-08-25T01:02:03Z\n", encoding="utf-8"
        )
        count += 1
        diagnostic_records, diagnostic_problems = parse_stage_manifest(
            diagnostic_path, "UFS", policy
        )
        if (
                diagnostic_problems
                or len(diagnostic_records) != 1
                or diagnostic_records[0]["distributionAuthoritative"] is not False
                or diagnostic_records[0]["developmentPathDiagnostic"] != "/Editor/"):
            failures.append("UFS development path was not diagnostic-only")
        count += 1
        _, nonufs_development_problems = parse_stage_manifest(
            diagnostic_path, "NONUFS", policy
        )
        if "DEVELOPMENT_ARTIFACT" not in {
                item["code"] for item in nonufs_development_problems}:
            failures.append("NonUFS development path incorrectly became diagnostic-only")

        landmass_identity = (
            "Engine/Plugins/Experimental/Landmass/Landmass.uplugin"
        )
        landmass_path = temporary / "manifest-landmass-diagnostic.txt"
        landmass_path.write_text(
            f"{landmass_identity}\t2026-08-25T01:02:03Z\n", encoding="utf-8"
        )
        count += 1
        landmass_ufs_records, landmass_ufs_problems = parse_stage_manifest(
            landmass_path, "UFS", policy
        )
        if (
                landmass_ufs_problems
                or len(landmass_ufs_records) != 1
                or landmass_ufs_records[0]["distributionAuthoritative"] is not False
                or landmass_ufs_records[0]["forbiddenPathDiagnostic"] is not None):
            failures.append("required Landmass descriptor was not allowed as UFS diagnostic metadata")
        count += 1
        _, landmass_nonufs_problems = parse_stage_manifest(
            landmass_path, "NONUFS", policy
        )
        if landmass_nonufs_problems:
            failures.append("required Landmass descriptor was rejected in NonUFS")

        landmass_content_identity = (
            "Engine/Plugins/Experimental/Landmass/Content/Leak.uasset"
        )
        landmass_content_path = temporary / "manifest-landmass-content.txt"
        landmass_content_path.write_text(
            f"{landmass_content_identity}\t2026-08-25T01:02:03Z\n", encoding="utf-8"
        )
        count += 1
        _, landmass_content_nonufs_problems = parse_stage_manifest(
            landmass_content_path, "NONUFS", policy
        )
        if "FORBIDDEN_PATH" not in {
                item["code"] for item in landmass_content_nonufs_problems}:
            failures.append("NonUFS Landmass content was not rejected")

    sha1_a = "A" * 40
    sha1_b = "B" * 40
    valid_pak = (
        f'LogPakFile: Display: "DiscGolfTour/A.bin" offset: 0, size: 2 bytes, sha1: {sha1_a}, compression: None.\n'
        "LogPakFile: Display: 1 files (2 bytes), (0 filtered bytes).\n"
    )
    listing_cases = [
        (valid_pak, ".pak", True),
        (valid_pak.replace("1 files", "2 files"), ".pak", False),
        (valid_pak.replace("2 bytes),", "3 bytes),"), ".pak", False),
        (valid_pak.replace("DiscGolfTour/A.bin", "../escape"), ".pak", False),
        (valid_pak.replace("offset: 0", "offset: x"), ".pak", False),
        (valid_pak, ".utoc", False),
    ]
    for text, extension, expected_valid in listing_cases:
        count += 1
        _, problems, _ = parse_unrealpak_listing(text, extension, "Content/Test" + extension, policy)
        if (not problems) is not expected_valid:
            failures.append(f"UnrealPak listing case for {extension} differed")

    def one_entry_pak(identity: str) -> str:
        return (
            f'LogPakFile: Display: "{identity}" offset: 0, size: 2 bytes, '
            f'sha1: {sha1_a}, compression: None.\n'
            "LogPakFile: Display: 1 files (2 bytes), (0 filtered bytes).\n"
        )

    final_identity_cases = [
        (identity, True)
        for identity in EXACT_DEVELOPMENT_PATH_EXCEPTIONS
    ] + [
        ("Engine/Plugins/Editor/AssetManagerEditor/AssetManagerEditor.uplugin", False),
        ("Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin", False),
        ("Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin", False),
        (EXACT_DEVELOPMENT_PATH_EXCEPTIONS[0].lower(), False),
    ]
    for identity, expected_valid in final_identity_cases:
        count += 1
        entries, problems, _ = parse_unrealpak_listing(
            one_entry_pak(identity), ".pak", "Content/FinalIdentity.pak", policy
        )
        if (
                (not problems) is not expected_valid
                or (expected_valid and (
                    len(entries) != 1
                    or entries[0]["distributionAuthoritative"] is not True
                ))):
            failures.append(f"final Pak development disposition differed: {identity}")

    landmass_identity = LANDMASS_DESCRIPTOR
    count += 1
    _, landmass_pak_problems, _ = parse_unrealpak_listing(
        one_entry_pak(landmass_identity), ".pak", "Content/Landmass.pak", policy
    )
    if landmass_pak_problems:
        failures.append("required final Pak Landmass descriptor was rejected")
    count += 1
    _, landmass_content_pak_problems, _ = parse_unrealpak_listing(
        one_entry_pak("Engine/Plugins/Experimental/Landmass/Content/Leak.uasset"),
        ".pak",
        "Content/LandmassContent.pak",
        policy,
    )
    if "FORBIDDEN_PATH" not in {
            item["code"] for item in landmass_content_pak_problems}:
        failures.append("final Pak Landmass content was not rejected")

    metadata_descriptor = REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS[3]
    metadata_companion = metadata_descriptor.rsplit("/", 1)[0] + "/Content/Leak.uasset"
    count += 1
    _, metadata_companion_problems, _ = parse_unrealpak_listing(
        one_entry_pak(metadata_companion), ".pak", "Content/MetadataLeak.pak", policy
    )
    if "METADATA_ONLY_DEPENDENCY_PLUGIN_COMPANION" not in {
            item["code"] for item in metadata_companion_problems}:
        failures.append("metadata-only dependency companion was not rejected")

    collision_pak = (
        f'LogPakFile: Display: "{EXACT_DEVELOPMENT_PATH_EXCEPTIONS[0]}" '
        f'offset: 0, size: 2 bytes, sha1: {sha1_a}, compression: None.\n'
        f'LogPakFile: Display: "{EXACT_DEVELOPMENT_PATH_EXCEPTIONS[0].lower()}" '
        f'offset: 2, size: 2 bytes, sha1: {sha1_b}, compression: None.\n'
        "LogPakFile: Display: 2 files (4 bytes), (0 filtered bytes).\n"
    )
    count += 1
    _, collision_problems, _ = parse_unrealpak_listing(
        collision_pak, ".pak", "Content/CaseCollision.pak", policy
    )
    if "DUPLICATE_CONTAINER_ENTRY" not in {
            item["code"] for item in collision_problems}:
        failures.append("case-colliding final Pak identities survived")

    valid_iostore_csv = (
        ", ".join(IOSTORE_CSV_HEADER) + "\n"
        f"0, 000000000000000000000005, 0x0, , <ScriptObjects>, global, "
        f"0, 0, 2, 2, 0x{sha1_a}, ScriptObjects, , global, Unknown, Base, Installed, 0, 0\n"
        f"1, 111111111111111111111108, 0x1111111111111111, , "
        f"../../../DiscGolfTour/Content/A.uasset, global, 2, 2, 3, 3, 0x{sha1_b}, "
        f"ExportBundleData, , global, Unknown, Base, Installed, 0, 2\n"
    )
    iostore_cases = [
        (valid_iostore_csv, True),
        (valid_iostore_csv.replace("OrderInContainer", "Order"), False),
        (valid_iostore_csv.replace("\n1, ", "\n2, "), False),
        (valid_iostore_csv.replace("111111111111111111111108", "000000000000000000000005"), False),
        (valid_iostore_csv.replace(f"0x{sha1_a}", "0xBAD", 1), False),
        (valid_iostore_csv.replace("../../../DiscGolfTour/Content/A.uasset", "../escape"), False),
        (", ".join(IOSTORE_CSV_HEADER) + "\n", False),
        (valid_iostore_csv.rstrip("\n"), False),
    ]
    for text, expected_valid in iostore_cases:
        count += 1
        entries, problems, summary = parse_iostore_chunk_csv(
            text, "DiscGolfTour/Content/Paks/global.utoc", "global", policy
        )
        if (not problems) is not expected_valid:
            failures.append("IoStore chunk CSV mutation result differed")
        if expected_valid and (len(entries) != 2 or not summary or summary["entryCount"] != 2):
            failures.append("IoStore valid chunk CSV inventory differed")

    for identity, expected_valid in final_identity_cases:
        count += 1
        candidate_csv = valid_iostore_csv.replace(
            "../../../DiscGolfTour/Content/A.uasset", f"../../../{identity}"
        )
        entries, problems, _ = parse_iostore_chunk_csv(
            candidate_csv, "DiscGolfTour/Content/Paks/global.utoc", "global", policy
        )
        if (
                (not problems) is not expected_valid
                or (expected_valid and any(
                    item["distributionAuthoritative"] is not True for item in entries
                ))):
            failures.append(f"final IoStore development disposition differed: {identity}")

    count += 1
    landmass_csv = valid_iostore_csv.replace(
        "../../../DiscGolfTour/Content/A.uasset", f"../../../{landmass_identity}"
    )
    _, landmass_iostore_problems, _ = parse_iostore_chunk_csv(
        landmass_csv, "DiscGolfTour/Content/Paks/global.utoc", "global", policy
    )
    if landmass_iostore_problems:
        failures.append("required final IoStore Landmass descriptor was rejected")
    count += 1
    landmass_content_csv = valid_iostore_csv.replace(
        "../../../DiscGolfTour/Content/A.uasset",
        "../../../Engine/Plugins/Experimental/Landmass/Content/Leak.uasset",
    )
    _, landmass_content_iostore_problems, _ = parse_iostore_chunk_csv(
        landmass_content_csv, "DiscGolfTour/Content/Paks/global.utoc", "global", policy
    )
    if "FORBIDDEN_PATH" not in {
            item["code"] for item in landmass_content_iostore_problems}:
        failures.append("final IoStore Landmass content was not rejected")

    mutated_policies: list[Callable[[dict[str, Any]], None]] = [
        lambda value: value.__setitem__("releaseReady", True),
        lambda value: value.__setitem__("schemaVersion", True),
        lambda value: value["containerPolicy"].__setitem__("requireEveryContainerInventorySuccess", False),
        lambda value: value["classificationManifest"].__setitem__("allowUnclassified", True),
        lambda value: value["receipt"].__setitem__("recordHostPaths", True),
        lambda value: value["negativeFixture"].__setitem__("expectedResult", "PASS"),
        lambda value: value["forbiddenDevelopmentPathTokens"].remove(
            REQUIRED_ENGINE_DEVELOPMENT_ROOTS[0]
        ),
        lambda value: value["forbiddenDevelopmentPathTokens"].append(
            "/Engine/Content/Slate/"
        ),
        lambda value: value["forbiddenPathTokens"].remove(
            LANDMASS_FINAL_PAYLOAD_TOKENS[0]
        ),
        lambda value: value["distributionIdentityAuthority"].__setitem__(
            "ufsDevelopmentPathRows", "AUTHORITATIVE"
        ),
        lambda value: value["distributionIdentityAuthority"][
            "exactDevelopmentPathExceptions"
        ].pop(),
        lambda value: value["distributionIdentityAuthority"][
            "exactDevelopmentPathExceptions"
        ].append("Engine/Plugins/Editor/"),
        lambda value: value["distributionIdentityAuthority"][
            "exactDevelopmentPathExceptions"
        ].__setitem__(0, EXACT_DEVELOPMENT_PATH_EXCEPTIONS[0].lower()),
        lambda value: value["pluginDependencyClosure"]["capabilityPolicy"].__setitem__(
            "sha256", "0" * 64
        ),
        lambda value: value["pluginDependencyClosure"][
            "requiredFinalPayloadDescriptors"
        ].pop(),
        lambda value: value["pluginDependencyClosure"][
            "reviewedMetadataOnlyDependencyDescriptors"
        ].pop(),
        lambda value: value["pluginDependencyClosure"][
            "descriptorOnlyForbiddenSubdirectories"
        ].remove("Content"),
        lambda value: value["pluginDependencyClosure"][
            "reviewedResidualMetadataExclusions"
        ].pop(),
        lambda value: value["pluginDependencyClosure"].__setitem__(
            "landmassDescriptor", LANDMASS_DESCRIPTOR.lower()
        ),
    ]
    for mutate in mutated_policies:
        count += 1
        candidate = copy.deepcopy(policy)
        mutate(candidate)
        if not validate_policy(candidate):
            failures.append("policy mutation survived")

    for root in REQUIRED_ENGINE_DEVELOPMENT_ROOTS:
        count += 1
        normalized = root + "Sentinel.uasset/"
        if not any(
            token.casefold() in normalized.casefold()
            for token in policy["forbiddenDevelopmentPathTokens"]
        ):
            failures.append(f"required engine development root was not rejected: {root}")
    for identity in PROTECTED_RUNTIME_SLATE_IDENTITIES:
        count += 1
        normalized = "/" + identity + "/"
        if any(
            token.casefold() in normalized.casefold()
            for token in policy["forbiddenDevelopmentPathTokens"]
        ):
            failures.append(f"required runtime Slate identity was rejected: {identity}")

    count += 1
    if required_dependency_descriptor_problems(
            REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS, policy):
        failures.append("complete dependency-closure descriptor set was rejected")
    count += 1
    missing_descriptor_problems = required_dependency_descriptor_problems(
        REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS[:-1], policy
    )
    if "REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTOR_MISSING" not in {
            item["code"] for item in missing_descriptor_problems}:
        failures.append("missing dependency-closure descriptor was not rejected")
    count += 1
    case_variant_descriptors = list(REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS)
    case_variant_descriptors[0] = case_variant_descriptors[0].lower()
    case_variant_problems = required_dependency_descriptor_problems(
        case_variant_descriptors, policy
    )
    if "DEPENDENCY_CLOSURE_DESCRIPTOR_CASE_MISMATCH" not in {
            item["code"] for item in case_variant_problems}:
        failures.append("case-variant dependency-closure descriptor was not rejected")

    discovered = {
        identity_key("ARCHIVE_FILE", None, "DiscGolfTour.exe"):
            ("ARCHIVE_FILE", None, "DiscGolfTour.exe"),
    }
    valid_record = {
        "scope": "ARCHIVE_FILE",
        "container": None,
        "identity": "DiscGolfTour.exe",
        "classification": "PROJECT_GENERATED_BUILD_OUTPUT",
        "authorityId": "DG_PROJECT_BUILD",
        "licenseId": "PROJECT_OWNED",
        "shippingApproved": True,
        "evidencePaths": ["Config/DG_Session19StagedProvenancePolicy.json"],
    }
    valid_document = {
        "schema": policy["classificationManifest"]["schema"],
        "schemaVersion": 1,
        "candidateId": "SELFTEST",
        "state": "REVIEWED_FOR_WINDOWS_SHIPPING",
        "records": [valid_record],
    }
    classification_cases: list[tuple[Callable[[dict[str, Any]], None], bool]] = [
        (lambda value: None, True),
        (lambda value: value["records"].append(copy.deepcopy(valid_record)), False),
        (lambda value: value["records"][0].__setitem__("shippingApproved", False), False),
        (lambda value: value["records"][0].__setitem__("scope", "UNKNOWN"), False),
        (lambda value: value["records"][0].__setitem__("identity", "../escape"), False),
        (lambda value: value["records"][0].__setitem__("licenseId", "C:/host/path"), False),
        (lambda value: value.__setitem__("candidateId", "OTHER"), False),
        (lambda value: value.__setitem__("unexpected", True), False),
        (lambda value: value.__setitem__("records", []), False),
    ]
    for mutate, expected_valid in classification_cases:
        count += 1
        candidate = copy.deepcopy(valid_document)
        mutate(candidate)
        _, problems, _ = validate_classification_manifest(
            candidate, "SELFTEST", policy, project_root, discovered, check_evidence=True
        )
        if (not problems) is not expected_valid:
            failures.append("classification mutation result differed")

    with tempfile.TemporaryDirectory(prefix="dg-s19-provenance-output-") as folder:
        output = Path(folder) / "receipt.json"
        first_value = {"candidateId": "SELFTEST", "status": "FIRST"}
        write_json_explicit(output, first_value)
        original = output.read_bytes()
        count += 1
        if json.loads(original.decode("utf-8")) != first_value:
            failures.append("explicit output initial write differed")
        count += 1
        try:
            write_json_explicit(output, {"candidateId": "SELFTEST", "status": "SECOND"})
        except FileExistsError:
            pass
        else:
            failures.append("existing explicit output was overwritten")
        if output.read_bytes() != original:
            failures.append("rejected explicit output write changed existing bytes")
    return count, failures


def run_negative_fixture(
        policy: dict[str, Any], project_root: Path, unrealpak: Path | None,
        explicit_archive: Path | None,
) -> tuple[bool, set[str], str]:
    negative = policy["negativeFixture"]
    archive = explicit_archive
    if archive is None:
        package_roots = [
            Path(project_root.anchor) / "DGTour_Packages",
            project_root.parent / "DGTour_Packages",
        ]
        archive = next(
            (
                package_root.joinpath(*PurePosixPath(negative["archiveLeaf"]).parts)
                for package_root in package_roots
                if package_root.joinpath(*PurePosixPath(negative["archiveLeaf"]).parts).is_dir()
            ),
            package_roots[0].joinpath(*PurePosixPath(negative["archiveLeaf"]).parts),
        )
    uat_log = project_root.joinpath(*PurePosixPath(negative["uatLog"]).parts)
    if not archive.is_dir():
        return False, set(), "negative fixture archive is unavailable"
    receipt, _ = audit_candidate(
        archive_root=archive,
        uat_log=uat_log,
        classification_path=None,
        candidate_id=negative["candidateId"],
        policy=policy,
        project_root=project_root,
        unrealpak=unrealpak,
    )
    codes = set(receipt["reasonCodes"])
    required = set(negative["requiredReasonCodes"])
    passed = receipt["status"] == "FAIL_CLOSED" and required <= codes
    return passed, codes, "" if passed else f"required negative codes missing: {sorted(required - codes)}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--uat-log", type=Path)
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--emit-classification-template", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--negative-fixture-archive", type=Path)
    args = parser.parse_args()

    project_root = args.project_root.resolve()
    policy_path = args.policy
    if not policy_path.is_absolute():
        policy_path = project_root / policy_path
    try:
        policy = load_json_strict(policy_path)
    except StrictJsonError as exc:
        print(f"SESSION 19 STAGED PROVENANCE POLICY INVALID: {exc}")
        return 1
    policy_problems = validate_policy(policy)
    if policy_problems:
        print("SESSION 19 STAGED PROVENANCE POLICY INVALID")
        for problem in policy_problems:
            print(f" - {problem['code']}: {problem['message']}")
        return 1

    unrealpak = find_unrealpak(args.unrealpak)
    if args.self_test:
        count, failures = run_unit_self_tests(policy, project_root)
        negative_passed, negative_codes, negative_failure = run_negative_fixture(
            policy, project_root, unrealpak, args.negative_fixture_archive
        )
        count += 1
        if not negative_passed:
            failures.append(negative_failure)
        if failures:
            print(f"SESSION 19 STAGED PROVENANCE SELF-TEST FAILED: {count} cases")
            for failure in failures:
                print(" -", failure)
            return 1
        print(
            f"SESSION 19 STAGED PROVENANCE SELF-TEST PASS: {count}/{count} "
            f"negative_codes={','.join(sorted(negative_codes))}"
        )
        return 0

    missing_arguments = [
        name for name, value in (
            ("--archive", args.archive), ("--uat-log", args.uat_log),
            ("--candidate-id", args.candidate_id),
        ) if value is None
    ]
    if missing_arguments:
        parser.error("candidate audit requires " + ", ".join(missing_arguments))

    receipt, template = audit_candidate(
        archive_root=args.archive.resolve(),
        uat_log=args.uat_log.resolve(),
        classification_path=args.classification.resolve() if args.classification else None,
        candidate_id=args.candidate_id,
        policy=policy,
        project_root=project_root,
        unrealpak=unrealpak,
    )
    if args.output is not None:
        write_json_explicit(args.output.resolve(), receipt)
    if args.emit_classification_template is not None:
        write_json_explicit(args.emit_classification_template.resolve(), template)

    status = receipt["status"]
    print(
        f"SESSION 19 STAGED PROVENANCE {status}: "
        f"files={receipt['archive']['fileCount']} "
        f"ufs={receipt['stageManifests']['ufs']['entryCount']} "
        f"nonufs={receipt['stageManifests']['nonUfs']['entryCount']} "
        f"containers={len(receipt['containers'])} "
        f"identities={receipt['classification']['discoveredIdentityCount']} "
        f"unclassified={receipt['classification']['unclassifiedIdentityCount']} "
        f"reasons={','.join(receipt['reasonCodes']) or 'none'}"
    )
    return 0 if status == "PASS_EXHAUSTIVE_TECHNICAL_INVENTORY" else 1


if __name__ == "__main__":
    sys.exit(main())
