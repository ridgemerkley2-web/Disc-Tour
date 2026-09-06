#!/usr/bin/env python3
"""Fail closed on unsafe or dependency-incomplete UE plugin metadata in Shipping.

The reviewed exact Pak descriptor exclusions are re-derived from the installed
UE 5.8 descriptors. Exact reviewed editor-only descriptors required by the
enabled-plugin graph remain metadata-only exceptions; every other editor or
developer descriptor fails closed. Candidate authority comes from strict final
loose, Pak, and IoStore inventory; the UFS staging manifest is diagnostic-only.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
from typing import Any

import generate_dg_session19_staged_provenance as staged_provenance


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ShippingPluginCapabilityPolicy.json"
STAGED_PROVENANCE_POLICY_PATH = ROOT / "Config/DG_Session19StagedProvenancePolicy.json"
STAGED_PROVENANCE_GENERATOR_PATH = ROOT / "Scripts/generate_dg_session19_staged_provenance.py"
DEFAULT_ENGINE_ROOT = Path(r"C:\Program Files\Epic Games\UE_5.8")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{3})?Z$")

POLICY_KEYS = {
    "schema", "schemaVersion", "session", "policyId", "engineAssociation",
    "state", "exclusionAuthority", "shippingConfigurationOnly",
    "finalPayloadAuthority",
    "restrictedModuleTypes", "preserveRuntimeCapablePlugins",
    "preserveContentOnlyPlugins", "requiredPreservedFinalPayloadDescriptors",
    "requiredAbsentFinalPayloadDescriptors",
    "requiredDependencyClosureFinalPayloadDescriptors",
    "requireMandatoryFinalDescriptorDependencyClosure",
    "allowOnlyReviewedMetadataDependencyEditorDeveloperDescriptorsInFinalPayload",
    "reviewedMetadataOnlyDependencyDescriptors",
    "reviewedDisabledPlugins", "reviewedResidualMetadataExclusions",
    "negativeFixture", "releaseBoundary",
}
RECORD_KEYS = {"name", "descriptor"}
EXCLUSION_AUTHORITY_KEYS = {"path", "section", "requiredMountPrefix"}
FINAL_PAYLOAD_AUTHORITY_KEYS = {
    "scope", "containerExtensions", "ioStoreCompanionExtension",
    "stagingManifestAuthoritativeForDistribution",
    "requireEveryContainerInventorySuccess", "requireArchiveStableAcrossInventory",
}
NON_STRUCTURAL_PROVENANCE_CODES = {
    "DEVELOPMENT_ARTIFACT", "FORBIDDEN_ARCHIVE_EXTENSION",
    "FORBIDDEN_FRAMEWORK_ARTIFACT", "FORBIDDEN_PATH", "FORBIDDEN_PDB",
}
REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTORS = {
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
}
REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS = (
    REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTORS - {
        "Engine/Plugins/Editor/BlueprintMaterialTextureNodes/BlueprintMaterialTextureNodes.uplugin",
        "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
    }
)


class ContractError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value: str) -> Any:
    raise ContractError(f"non-finite JSON number: {value}")


def load_json_strict(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8-sig"),
            object_pairs_hook=reject_duplicate_keys,
            parse_constant=reject_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ContractError) as exc:
        raise ContractError(f"strict JSON load failed for {path.name}: {exc}") from exc


def sanitize_unreal_json(text: str) -> str:
    """Remove Unreal descriptor comments/trailing commas without touching strings."""
    without_comments: list[str] = []
    index = 0
    in_string = False
    escaped = False
    while index < len(text):
        character = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if in_string:
            without_comments.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
            index += 1
            continue
        if character == '"':
            in_string = True
            without_comments.append(character)
            index += 1
            continue
        if character == "/" and following == "/":
            index += 2
            while index < len(text) and text[index] not in "\r\n":
                index += 1
            continue
        if character == "/" and following == "*":
            index += 2
            while index + 1 < len(text) and text[index:index + 2] != "*/":
                index += 1
            if index + 1 >= len(text):
                raise ContractError("Unreal descriptor has an unterminated block comment")
            index += 2
            continue
        without_comments.append(character)
        index += 1
    if in_string:
        raise ContractError("Unreal descriptor has an unterminated string")

    source = "".join(without_comments)
    cleaned: list[str] = []
    index = 0
    in_string = False
    escaped = False
    while index < len(source):
        character = source[index]
        if in_string:
            cleaned.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
            index += 1
            continue
        if character == '"':
            in_string = True
            cleaned.append(character)
            index += 1
            continue
        if character == ",":
            lookahead = index + 1
            while lookahead < len(source) and source[lookahead].isspace():
                lookahead += 1
            if lookahead < len(source) and source[lookahead] in "]}":
                index += 1
                continue
        cleaned.append(character)
        index += 1
    return "".join(cleaned)


def load_unreal_descriptor(path: Path) -> Any:
    try:
        text = read_stable(path).decode("utf-8-sig")
        return json.loads(
            sanitize_unreal_json(text),
            object_pairs_hook=reject_duplicate_keys,
            parse_constant=reject_constant,
        )
    except (UnicodeError, json.JSONDecodeError, ContractError) as exc:
        raise ContractError(f"Unreal descriptor load failed for {path.name}: {exc}") from exc


def canonical_relative(value: Any) -> str | None:
    if (
        type(value) is not str or not value or value != value.strip()
        or "\\" in value or "\x00" in value or ":" in value
    ):
        return None
    pure = PurePosixPath(value)
    if (
        pure.is_absolute() or value.startswith("/")
        or any(part in ("", ".", "..") for part in pure.parts)
        or pure.as_posix() != value
    ):
        return None
    return value


def unique_strings(value: Any) -> bool:
    return (
        type(value) is list and bool(value)
        and all(type(item) is str and item for item in value)
        and len({item.casefold() for item in value}) == len(value)
    )


def validate_policy(policy: Any) -> None:
    if type(policy) is not dict or set(policy) != POLICY_KEYS:
        raise ContractError("policy root keys or type differ")
    expected = {
        "schema": "DiscGolfTour.Session19ShippingPluginCapabilityPolicy.v3",
        "schemaVersion": 3,
        "session": 19,
        "policyId": "ue58_shipping_plugin_dependency_closed_metadata_exclusion_v3",
        "engineAssociation": "5.8",
        "state": "IMPLEMENTED_PENDING_FRESH_WINDOWS_SHIPPING_CANDIDATE",
        "exclusionAuthority": {
            "path": "Config/DefaultPakFileRules.ini",
            "section": "ExcludeDevelopmentOnlyContentForWindowsShipping",
            "requiredMountPrefix": ".../",
        },
        "shippingConfigurationOnly": True,
        "finalPayloadAuthority": {
            "scope": "FINAL_WINDOWS_ARCHIVE_PAK_IOSTORE",
            "containerExtensions": [".pak", ".utoc"],
            "ioStoreCompanionExtension": ".ucas",
            "stagingManifestAuthoritativeForDistribution": False,
            "requireEveryContainerInventorySuccess": True,
            "requireArchiveStableAcrossInventory": True,
        },
        "restrictedModuleTypes": [
            "Editor", "EditorNoCommandlet", "Developer", "DeveloperTool", "Program"
        ],
        "preserveRuntimeCapablePlugins": True,
        "preserveContentOnlyPlugins": True,
        "requiredPreservedFinalPayloadDescriptors": [
            "Engine/Plugins/Animation/ControlRigModules/ControlRigModules.uplugin",
            "Engine/Plugins/BaseMaterial/BaseMaterial.uplugin",
        ],
        "requiredAbsentFinalPayloadDescriptors": [
            "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
            "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin",
        ],
        "requireMandatoryFinalDescriptorDependencyClosure": True,
        "allowOnlyReviewedMetadataDependencyEditorDeveloperDescriptorsInFinalPayload": True,
        "reviewedResidualMetadataExclusions": [
            "Engine/Plugins/ChaosClothAssetEditorCore/Config/"
            "DefaultChaosClothAssetEditorCore.ini",
            "Engine/Plugins/Editor/EditorScriptingUtilities/Config/"
            "DefaultEditorScriptingUtilities.ini"
        ],
        "negativeFixture": {
            "candidateId": "S19_WindowsShipping_20260825T032238Z_b969c38b32f5",
            "expectedFinalPayloadEditorDeveloperOnlyDescriptorCount": 44,
            "expectedReviewedMetadataOnlyDependencyDescriptorCount": 13,
            "expectedUnreviewedEditorDeveloperOnlyDescriptorCount": 31,
            "expectedResult": "REJECT",
            "mayNeverSatisfyPositiveEvidence": True,
        },
        "releaseBoundary": {
            "releaseReady": False,
            "blockerClosedWithoutFreshDependencyClosedReceipt": False,
        },
    }
    for key, value in expected.items():
        if type(policy.get(key)) is not type(value) or policy.get(key) != value:
            raise ContractError(f"policy {key} differs")
    if set(policy["exclusionAuthority"]) != EXCLUSION_AUTHORITY_KEYS:
        raise ContractError("policy exclusionAuthority keys differ")
    if set(policy["finalPayloadAuthority"]) != FINAL_PAYLOAD_AUTHORITY_KEYS:
        raise ContractError("policy finalPayloadAuthority keys differ")
    required_preserved = policy["requiredPreservedFinalPayloadDescriptors"]
    if (
        type(required_preserved) is not list or len(required_preserved) != 2
        or len({value.casefold() for value in required_preserved}) != 2
        or any(
            canonical_relative(value) is None
            or not value.startswith("Engine/Plugins/")
            or not value.endswith(".uplugin")
            for value in required_preserved
        )
    ):
        raise ContractError("required preserved final descriptor authority differs")
    required_absent = policy["requiredAbsentFinalPayloadDescriptors"]
    if (
        type(required_absent) is not list or len(required_absent) != 2
        or len({value.casefold() for value in required_absent}) != 2
        or any(
            canonical_relative(value) is None
            or not value.startswith("Engine/Plugins/")
            or not value.endswith(".uplugin")
            for value in required_absent
        )
        or {
            value.casefold() for value in required_absent
        } & {
            value.casefold() for value in required_preserved
        }
    ):
        raise ContractError("required absent final descriptor authority differs")
    required_dependency_closure = policy[
        "requiredDependencyClosureFinalPayloadDescriptors"
    ]
    if (
        type(required_dependency_closure) is not list
        or len(required_dependency_closure) != 15
        or set(required_dependency_closure) != REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTORS
        or len({value.casefold() for value in required_dependency_closure}) != 15
        or any(canonical_relative(value) is None for value in required_dependency_closure)
        or set(required_dependency_closure) & set(required_absent)
    ):
        raise ContractError("required dependency-closure descriptor authority differs")
    metadata_records = policy["reviewedMetadataOnlyDependencyDescriptors"]
    if type(metadata_records) is not list or len(metadata_records) != 13:
        raise ContractError(
            "reviewed metadata-only dependency list must contain exactly 13 records"
        )
    metadata_names: set[str] = set()
    metadata_descriptors: set[str] = set()
    metadata_order: list[str] = []
    for index, record in enumerate(metadata_records):
        if type(record) is not dict or set(record) != RECORD_KEYS:
            raise ContractError(
                f"reviewed metadata-only dependency record {index} keys or type differ"
            )
        name = record["name"]
        descriptor = record["descriptor"]
        if type(name) is not str or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{1,127}", name):
            raise ContractError(
                f"reviewed metadata-only dependency record {index} name is malformed"
            )
        if (
            canonical_relative(descriptor) is None
            or not descriptor.startswith("Engine/Plugins/")
            or not descriptor.endswith("/" + name + ".uplugin")
            or descriptor not in REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS
        ):
            raise ContractError(
                f"reviewed metadata-only dependency record {index} descriptor is malformed"
            )
        if name.casefold() in metadata_names or descriptor.casefold() in metadata_descriptors:
            raise ContractError(
                "reviewed metadata-only dependency records duplicate or case-collide"
            )
        metadata_names.add(name.casefold())
        metadata_descriptors.add(descriptor.casefold())
        metadata_order.append(name)
    if metadata_order != sorted(metadata_order, key=str.casefold):
        raise ContractError("reviewed metadata-only dependency records must be sorted by name")
    records = policy["reviewedDisabledPlugins"]
    if type(records) is not list or len(records) != 31:
        raise ContractError("reviewed disabled plugin list must contain exactly 31 records")
    names: set[str] = set()
    descriptors: set[str] = set()
    observed_order: list[str] = []
    for index, record in enumerate(records):
        if type(record) is not dict or set(record) != RECORD_KEYS:
            raise ContractError(f"reviewed plugin record {index} keys or type differ")
        name = record["name"]
        descriptor = record["descriptor"]
        if type(name) is not str or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{1,127}", name):
            raise ContractError(f"reviewed plugin record {index} name is malformed")
        if (
            canonical_relative(descriptor) is None
            or not descriptor.startswith("Engine/Plugins/")
            or not descriptor.endswith("/" + name + ".uplugin")
        ):
            raise ContractError(f"reviewed plugin record {index} descriptor is malformed")
        if name.casefold() in names or descriptor.casefold() in descriptors:
            raise ContractError("reviewed plugin records duplicate or case-collide")
        names.add(name.casefold())
        descriptors.add(descriptor.casefold())
        observed_order.append(name)
    if observed_order != sorted(observed_order, key=str.casefold):
        raise ContractError("reviewed plugin records must be sorted by name")
    if descriptors & metadata_descriptors:
        raise ContractError("disabled and metadata-only descriptor authorities overlap")
    if descriptors & {
        value.casefold() for value in required_dependency_closure
    }:
        raise ContractError("disabled and required dependency-closure authorities overlap")
    residual = policy["reviewedResidualMetadataExclusions"]
    if (
        type(residual) is not list or len(residual) != 2
        or len({value.casefold() for value in residual}) != 2
        or any(
            canonical_relative(value) is None
            or not value.startswith("Engine/Plugins/")
            or value.casefold().endswith(".uplugin")
            for value in residual
        )
    ):
        raise ContractError("reviewed residual metadata exclusion differs or is malformed")


def hash_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def read_stable(path: Path) -> bytes:
    try:
        before = path.stat()
        if not path.is_file() or path.is_symlink():
            raise ContractError(f"input is not a regular file: {path.name}")
        data = path.read_bytes()
        after = path.stat()
    except OSError as exc:
        raise ContractError(f"input cannot be read: {path.name}") from exc
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ContractError(f"input changed while reading: {path.name}")
    return data


def binding(path: Path, public_path: str) -> dict[str, Any]:
    data = read_stable(path)
    return {"path": public_path, "bytes": len(data), "sha256": hash_bytes(data)}


def engine_file(engine_root: Path, relative: str) -> Path:
    candidate = engine_root.joinpath(*PurePosixPath(relative).parts)
    try:
        root_resolved = engine_root.resolve(strict=True)
        resolved = candidate.resolve(strict=True)
    except OSError as exc:
        raise ContractError(f"installed engine descriptor is missing: {relative}") from exc
    if root_resolved not in resolved.parents or candidate.is_symlink() or not candidate.is_file():
        raise ContractError(f"installed engine descriptor escapes or is irregular: {relative}")
    return candidate


def engine_version(engine_root: Path) -> tuple[dict[str, Any], dict[str, int]]:
    relative = "Engine/Build/Build.version"
    path = engine_file(engine_root, relative)
    document = load_json_strict(path)
    fields = ("MajorVersion", "MinorVersion", "PatchVersion", "Changelist")
    if type(document) is not dict or not all(
        type(document.get(key)) is int and document[key] >= 0 for key in fields
    ):
        raise ContractError("UE Build.version is malformed")
    if document["MajorVersion"] != 5 or document["MinorVersion"] != 8:
        raise ContractError("plugin authority requires installed UE 5.8")
    return binding(path, relative), {
        "major": document["MajorVersion"],
        "minor": document["MinorVersion"],
        "patch": document["PatchVersion"],
        "changelist": document["Changelist"],
    }


def classify_descriptor(
    document: Any, restricted_types: set[str]
) -> tuple[str, list[str]]:
    if type(document) is not dict:
        raise ContractError("plugin descriptor root is not an object")
    modules = document.get("Modules", [])
    if modules is None:
        modules = []
    if type(modules) is not list:
        raise ContractError("plugin descriptor Modules is not an array")
    if not modules:
        return "CONTENT_ONLY", []
    types: list[str] = []
    for module in modules:
        if type(module) is not dict or type(module.get("Type")) is not str or not module["Type"]:
            raise ContractError("plugin descriptor module type is absent or malformed")
        types.append(module["Type"])
    unique_types = sorted(set(types), key=str.casefold)
    if all(value in restricted_types for value in unique_types):
        return "EDITOR_DEVELOPER_ONLY", unique_types
    return "RUNTIME_CAPABLE", unique_types


def validate_exclusion_authority(
    authority_path: Path, policy: dict[str, Any]
) -> dict[str, Any]:
    try:
        text = read_stable(authority_path).decode("utf-8-sig")
    except UnicodeError as exc:
        raise ContractError("Shipping Pak exclusion authority is not UTF-8") from exc

    section_name = policy["exclusionAuthority"]["section"]
    sections: dict[str, list[tuple[int, str]]] = {}
    current: str | None = None
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith((";", "#")):
            continue
        section_match = re.fullmatch(r"\[([^\[\]]+)\]", line)
        if section_match:
            current = section_match.group(1)
            if current in sections:
                raise ContractError(f"duplicate Pak rule section: {current}")
            sections[current] = []
            continue
        if current is None:
            raise ContractError(f"Pak rule line {line_number} is outside a section")
        sections[current].append((line_number, line))

    if section_name not in sections:
        raise ContractError("Shipping plugin exclusion section is absent")
    rows = sections[section_name]
    settings: dict[str, list[str]] = {}
    for line_number, line in rows:
        match = re.fullmatch(r'([+A-Za-z][A-Za-z0-9+]*)=(?:"([^"]*)"|(true|false))', line)
        if not match:
            raise ContractError(f"Pak rule line {line_number} is malformed")
        key = match.group(1)
        value = match.group(2) if match.group(2) is not None else match.group(3)
        settings.setdefault(key, []).append(value)

    exact_settings = {
        "Platforms": ["Windows"],
        "Targets": ["Shipping"],
        "bExcludeFromPaks": ["true"],
        "bOverrideChunkManifest": ["true"],
    }
    for key, expected in exact_settings.items():
        if settings.get(key) != expected:
            raise ContractError(f"Pak exclusion section {key} differs")
    unexpected_keys = set(settings) - set(exact_settings) - {"+Files"}
    if unexpected_keys:
        raise ContractError(
            "Pak exclusion section contains unsupported keys: "
            + ", ".join(sorted(unexpected_keys))
        )

    mount_prefix = policy["exclusionAuthority"]["requiredMountPrefix"]
    expected_rules = [
        mount_prefix + item["descriptor"]
        for item in policy["reviewedDisabledPlugins"]
    ] + [
        mount_prefix + identity
        for identity in policy["reviewedResidualMetadataExclusions"]
    ]
    all_rules = settings.get("+Files", [])
    if len({rule.casefold() for rule in all_rules}) != len(all_rules):
        raise ContractError("Pak exclusion rules duplicate or case-collide")
    plugin_rules = [
        rule for rule in all_rules
        if rule.casefold().startswith((mount_prefix + "Engine/Plugins/").casefold())
    ]
    if set(plugin_rules) != set(expected_rules) or len(plugin_rules) != len(expected_rules):
        missing = sorted(set(expected_rules) - set(plugin_rules), key=str.casefold)
        extra = sorted(set(plugin_rules) - set(expected_rules), key=str.casefold)
        raise ContractError(
            "exact Shipping plugin descriptor exclusions differ "
            f"(missing={len(missing)}, extra_or_broad={len(extra)})"
        )
    if any(any(token in rule for token in ("*", "?", "[", "]")) for rule in plugin_rules):
        raise ContractError("Shipping plugin exclusions must be exact files")
    non_plugin_rules = [rule for rule in all_rules if rule not in plugin_rules]
    content_prefix = mount_prefix + "Engine/Content/"
    if any(
        not rule.casefold().startswith(content_prefix.casefold())
        for rule in non_plugin_rules
    ):
        raise ContractError(
            "non-plugin Pak exclusions must remain under Engine/Content and may not "
            "blanket-match Engine plugin descriptors"
        )

    authority_public_path = policy["exclusionAuthority"]["path"]
    return {
        **binding(authority_path, authority_public_path),
        "section": section_name,
        "platforms": ["Windows"],
        "targets": ["Shipping"],
        "exactPluginRuleCount": len(plugin_rules),
        "exactPluginRules": plugin_rules,
        "reviewedDescriptorRuleCount": len(policy["reviewedDisabledPlugins"]),
        "reviewedMetadataOnlyDependencyExceptionCount": len(
            policy["reviewedMetadataOnlyDependencyDescriptors"]
        ),
        "reviewedResidualMetadataRuleCount": len(
            policy["reviewedResidualMetadataExclusions"]
        ),
        "runtimeCapableOrContentOnlyPluginRuleCount": 0,
        "broadPluginRuleCount": 0,
    }


def inspect_reviewed_descriptors(
    policy: dict[str, Any], engine_root: Path
) -> list[dict[str, Any]]:
    restricted = set(policy["restrictedModuleTypes"])
    results: list[dict[str, Any]] = []
    for record in policy["reviewedDisabledPlugins"]:
        path = engine_file(engine_root, record["descriptor"])
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        if capability != "EDITOR_DEVELOPER_ONLY":
            raise ContractError(
                f"reviewed disabled plugin is no longer editor/developer-only: {record['name']}"
            )
        results.append({
            "name": record["name"],
            "descriptor": binding(path, record["descriptor"]),
            "moduleTypes": module_types,
            "capability": capability,
        })
    return results


def inspect_reviewed_metadata_only_dependencies(
    policy: dict[str, Any], engine_root: Path
) -> list[dict[str, Any]]:
    restricted = set(policy["restrictedModuleTypes"])
    results: list[dict[str, Any]] = []
    for record in policy["reviewedMetadataOnlyDependencyDescriptors"]:
        path = engine_file(engine_root, record["descriptor"])
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        if capability != "EDITOR_DEVELOPER_ONLY":
            raise ContractError(
                "reviewed metadata-only dependency is no longer editor/developer-only: "
                + record["name"]
            )
        results.append({
            "name": record["name"],
            "descriptor": binding(path, record["descriptor"]),
            "moduleTypes": module_types,
            "capability": capability,
            "metadataOnlyShippingException": True,
        })
    return results


def inspect_required_dependency_closure_descriptors(
    policy: dict[str, Any], engine_root: Path
) -> list[dict[str, Any]]:
    restricted = set(policy["restrictedModuleTypes"])
    metadata_only = {
        item["descriptor"].casefold()
        for item in policy["reviewedMetadataOnlyDependencyDescriptors"]
    }
    results: list[dict[str, Any]] = []
    for identity in policy["requiredDependencyClosureFinalPayloadDescriptors"]:
        path = engine_file(engine_root, identity)
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        is_metadata_exception = identity.casefold() in metadata_only
        if is_metadata_exception != (capability == "EDITOR_DEVELOPER_ONLY"):
            raise ContractError(
                "dependency-closure descriptor capability/exception authority differs: "
                + identity
            )
        results.append({
            "identity": identity,
            "descriptor": binding(path, identity),
            "moduleTypes": module_types,
            "capability": capability,
            "metadataOnlyShippingException": is_metadata_exception,
        })
    return results


def inspect_required_preserved_descriptors(
    policy: dict[str, Any], engine_root: Path
) -> list[dict[str, Any]]:
    restricted = set(policy["restrictedModuleTypes"])
    results: list[dict[str, Any]] = []
    for identity in policy["requiredPreservedFinalPayloadDescriptors"]:
        path = engine_file(engine_root, identity)
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        if capability not in {"RUNTIME_CAPABLE", "CONTENT_ONLY"}:
            raise ContractError(
                f"required preserved plugin is not runtime-capable/content-only: {identity}"
            )
        results.append({
            "identity": identity,
            "descriptor": binding(path, identity),
            "moduleTypes": module_types,
            "capability": capability,
        })
    return results


def inspect_required_absent_descriptors(
    policy: dict[str, Any], engine_root: Path
) -> list[dict[str, Any]]:
    restricted = set(policy["restrictedModuleTypes"])
    results: list[dict[str, Any]] = []
    for identity in policy["requiredAbsentFinalPayloadDescriptors"]:
        path = engine_file(engine_root, identity)
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        results.append({
            "identity": identity,
            "descriptor": binding(path, identity),
            "moduleTypes": module_types,
            "capability": capability,
        })
    return results


def missing_required_preserved_descriptors(
    policy: dict[str, Any], records: list[dict[str, Any]],
) -> list[str]:
    final_descriptor_identities = {
        item["identity"].casefold() for item in records
    }
    return [
        identity for identity in policy["requiredPreservedFinalPayloadDescriptors"]
        if identity.casefold() not in final_descriptor_identities
    ]


def required_absent_descriptor_survivors(
    policy: dict[str, Any], records: list[dict[str, Any]],
) -> list[str]:
    final_descriptor_identities = {
        item["identity"].casefold() for item in records
    }
    return [
        identity for identity in policy["requiredAbsentFinalPayloadDescriptors"]
        if identity.casefold() in final_descriptor_identities
    ]


def missing_required_dependency_closure_descriptors(
    policy: dict[str, Any], records: list[dict[str, Any]],
) -> list[str]:
    final_descriptor_identities = {
        item["identity"].casefold() for item in records
    }
    return [
        identity
        for identity in policy["requiredDependencyClosureFinalPayloadDescriptors"]
        if identity.casefold() not in final_descriptor_identities
    ]


def evaluate_required_descriptor_authority(
    policy: dict[str, Any], records: list[dict[str, Any]],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    issues: list[dict[str, Any]] = []
    missing_preserved = missing_required_preserved_descriptors(policy, records)
    if missing_preserved:
        issues.append(payload_issue(
            "REQUIRED_RUNTIME_OR_CONTENT_PLUGIN_DESCRIPTOR_MISSING",
            f"{len(missing_preserved)} required runtime/content plugin descriptors are absent",
        ))
    required_absent_survivors = required_absent_descriptor_survivors(policy, records)
    if required_absent_survivors:
        issues.append(payload_issue(
            "PROJECT_SHIPPING_DENIED_PLUGIN_DESCRIPTOR_IN_FINAL_PAYLOAD",
            f"{len(required_absent_survivors)} project Shipping-denied plugin "
            "descriptors survived the final payload",
        ))
    missing_dependency_closure = missing_required_dependency_closure_descriptors(
        policy, records
    )
    if missing_dependency_closure:
        issues.append(payload_issue(
            "REQUIRED_PLUGIN_DEPENDENCY_CLOSURE_DESCRIPTOR_MISSING",
            f"{len(missing_dependency_closure)} required dependency-closure "
            "descriptors are absent",
        ))
    return {
        "requiredPreservedDescriptorCount": len(
            policy["requiredPreservedFinalPayloadDescriptors"]
        ),
        "missingRequiredPreservedDescriptorCount": len(missing_preserved),
        "missingRequiredPreservedDescriptors": missing_preserved,
        "requiredAbsentDescriptorCount": len(
            policy["requiredAbsentFinalPayloadDescriptors"]
        ),
        "requiredAbsentSurvivorCount": len(required_absent_survivors),
        "requiredAbsentSurvivors": required_absent_survivors,
        "requiredDependencyClosureDescriptorCount": len(
            policy["requiredDependencyClosureFinalPayloadDescriptors"]
        ),
        "missingRequiredDependencyClosureDescriptorCount": len(
            missing_dependency_closure
        ),
        "missingRequiredDependencyClosureDescriptors": missing_dependency_closure,
    }, issues


def parse_manifest(path: Path) -> list[str]:
    try:
        text = read_stable(path).decode("utf-8-sig")
    except UnicodeError as exc:
        raise ContractError("UFS manifest is not UTF-8") from exc
    identities: list[str] = []
    seen: set[str] = set()
    for index, line in enumerate(text.splitlines(), 1):
        fields = line.split("\t")
        if len(fields) != 2:
            raise ContractError(f"UFS manifest row {index} does not have two fields")
        identity, timestamp = fields
        if canonical_relative(identity) is None or not UTC_RE.fullmatch(timestamp):
            raise ContractError(f"UFS manifest row {index} is malformed")
        collision = identity.casefold()
        if collision in seen:
            raise ContractError("UFS manifest duplicates or case-collides")
        seen.add(collision)
        if (
            identity.startswith("Engine/Plugins/")
            and identity.casefold().endswith(".uplugin")
        ):
            identities.append(identity)
    return sorted(identities, key=str.casefold)


def inspect_manifest(
    manifest: Path, policy: dict[str, Any], engine_root: Path
) -> dict[str, Any]:
    restricted = set(policy["restrictedModuleTypes"])
    metadata_exceptions = {
        item["descriptor"].casefold(): item["name"]
        for item in policy["reviewedMetadataOnlyDependencyDescriptors"]
    }
    records: list[dict[str, Any]] = []
    editor_only: list[str] = []
    unreviewed_editor_only: list[str] = []
    for identity in parse_manifest(manifest):
        path = engine_file(engine_root, identity)
        document = load_unreal_descriptor(path)
        capability, module_types = classify_descriptor(document, restricted)
        if capability == "EDITOR_DEVELOPER_ONLY":
            editor_only.append(identity)
            if identity.casefold() not in metadata_exceptions:
                unreviewed_editor_only.append(identity)
        records.append({
            "identity": identity,
            "name": PurePosixPath(identity).stem,
            "capability": capability,
            "moduleTypes": module_types,
            "reviewedMetadataOnlyDependency": (
                identity.casefold() in metadata_exceptions
            ),
            "descriptorSha256": hash_bytes(read_stable(path)),
        })
    counts = {
        capability: sum(1 for item in records if item["capability"] == capability)
        for capability in ("RUNTIME_CAPABLE", "CONTENT_ONLY", "EDITOR_DEVELOPER_ONLY")
    }
    return {
        "binding": binding(manifest, "Manifest_UFSFiles_Win64.txt"),
        "descriptorCount": len(records),
        "capabilityCounts": counts,
        "editorDeveloperOnlySurvivorCount": len(editor_only),
        "editorDeveloperOnlySurvivors": editor_only,
        "unreviewedEditorDeveloperOnlySurvivorCount": len(unreviewed_editor_only),
        "unreviewedEditorDeveloperOnlySurvivors": unreviewed_editor_only,
        "records": records,
    }


def payload_issue(
    code: str, message: str, identity: str | None = None
) -> dict[str, Any]:
    value: dict[str, Any] = {"code": code, "message": message}
    if identity is not None:
        value["identity"] = identity
    return value


def load_staged_provenance_inventory_policy() -> dict[str, Any]:
    try:
        document = staged_provenance.load_json_strict(STAGED_PROVENANCE_POLICY_PATH)
    except Exception as exc:
        raise ContractError("staged provenance inventory policy cannot be loaded") from exc
    problems = staged_provenance.validate_policy(document)
    if problems:
        raise ContractError("staged provenance inventory policy is invalid")
    return document


def resolve_unrealpak(explicit: Path | None) -> Path:
    if explicit is not None:
        try:
            if explicit.is_symlink() or not explicit.is_file():
                raise ContractError("explicit UnrealPak executable is irregular")
            resolved = explicit.resolve(strict=True)
        except OSError as exc:
            raise ContractError("explicit UnrealPak executable is missing") from exc
        if not resolved.is_file():
            raise ContractError("explicit UnrealPak executable is irregular")
        return resolved
    resolved = staged_provenance.find_unrealpak(None)
    if resolved is None:
        raise ContractError("UnrealPak executable was not found")
    return resolved


def archive_public(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [
        {"path": item["path"], "bytes": item["bytes"], "sha256": item["sha256"]}
        for item in records
    ]


def archive_manifest_sha(records: list[dict[str, Any]]) -> str:
    return staged_provenance.canonical_archive_manifest_hash(records)


def descriptor_occurrence(
    *, location: str, identity: str, container: dict[str, Any] | None,
    entry: dict[str, Any],
) -> dict[str, Any]:
    value: dict[str, Any] = {"location": location, "identity": identity}
    if container is None:
        value.update({"bytes": entry["bytes"], "sha256": entry["sha256"]})
        return value
    value["container"] = container["path"]
    value["bytes"] = entry["bytes"]
    if "sha1" in entry:
        value["payloadSha1"] = entry["sha1"]
    if "ioHash" in entry:
        value["payloadIoHash"] = entry["ioHash"]
    if "chunkId" in entry:
        value["chunkId"] = entry["chunkId"]
    return value


def evaluate_final_descriptor_records(
    records: list[dict[str, Any]], metadata_only_exceptions: set[str],
    residual_occurrences: list[dict[str, Any]],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    issues: list[dict[str, Any]] = []
    editor_only = [
        item["identity"] for item in records
        if item["capability"] == "EDITOR_DEVELOPER_ONLY"
    ]
    unclassified = [
        item["identity"] for item in records if item["capability"] == "UNCLASSIFIED"
    ]
    metadata_only_survivors = [
        value for value in editor_only if value.casefold() in metadata_only_exceptions
    ]
    unreviewed_survivors = [
        value for value in editor_only if value.casefold() not in metadata_only_exceptions
    ]
    if unreviewed_survivors:
        issues.append(payload_issue(
            "UNREVIEWED_EDITOR_DEVELOPER_ONLY_PLUGIN_DESCRIPTOR_IN_FINAL_PAYLOAD",
            f"{len(unreviewed_survivors)} unreviewed editor/developer-only descriptors survived",
        ))
    if unclassified:
        issues.append(payload_issue(
            "FINAL_PLUGIN_DESCRIPTOR_AUTHORITY_MISSING",
            f"{len(unclassified)} final plugin descriptors lack installed-engine authority",
        ))
    if residual_occurrences:
        issues.append(payload_issue(
            "REVIEWED_RESIDUAL_PLUGIN_METADATA_IN_FINAL_PAYLOAD",
            f"{len(residual_occurrences)} reviewed residual plugin metadata occurrences survived",
        ))
    capability_counts = {
        capability: sum(1 for item in records if item["capability"] == capability)
        for capability in (
            "RUNTIME_CAPABLE", "CONTENT_ONLY", "EDITOR_DEVELOPER_ONLY", "UNCLASSIFIED"
        )
    }
    if capability_counts["RUNTIME_CAPABLE"] == 0 or capability_counts["CONTENT_ONLY"] == 0:
        issues.append(payload_issue(
            "FINAL_PAYLOAD_RUNTIME_OR_CONTENT_PLUGIN_CLASS_MISSING",
            "final payload must preserve both runtime-capable and content-only plugin descriptors",
        ))
    return {
        "pluginDescriptorCount": len(records),
        "capabilityCounts": capability_counts,
        "editorDeveloperOnlySurvivorCount": len(editor_only),
        "editorDeveloperOnlySurvivors": editor_only,
        "reviewedMetadataOnlyDependencySurvivorCount": len(
            metadata_only_survivors
        ),
        "reviewedMetadataOnlyDependencySurvivors": metadata_only_survivors,
        "unreviewedEditorDeveloperOnlySurvivorCount": len(unreviewed_survivors),
        "unreviewedEditorDeveloperOnlySurvivors": unreviewed_survivors,
        "unclassifiedDescriptorCount": len(unclassified),
        "unclassifiedDescriptors": unclassified,
        "reviewedResidualMetadataSurvivorCount": len(residual_occurrences),
        "reviewedResidualMetadataSurvivors": residual_occurrences,
    }, issues


def _descriptor_identity_from_extracted_path(path: Path, extraction_root: Path) -> str | None:
    try:
        relative = path.relative_to(extraction_root).as_posix()
    except ValueError:
        return None
    folded = relative.casefold()
    for marker in ("engine/plugins/", "discgolftour/plugins/"):
        index = folded.find(marker)
        if index >= 0:
            identity = relative[index:]
            return identity if canonical_relative(identity) is not None else None
    return None


def extract_final_descriptor_documents(
    *, archive_root: Path, containers: list[dict[str, Any]],
    records: list[dict[str, Any]], unrealpak: Path,
) -> tuple[
    dict[str, dict[str, Any]], dict[str, dict[str, Any]], list[dict[str, Any]]
]:
    """Extract the exact final descriptor JSON used by runtime PluginManager."""
    issues: list[dict[str, Any]] = []
    expected = {item["identity"].casefold(): item for item in records}
    documents: dict[str, dict[str, Any]] = {}
    payload_bindings: dict[str, dict[str, Any]] = {}
    with tempfile.TemporaryDirectory(prefix="dgtour-s19-plugin-closure-") as temporary:
        temporary_root = Path(temporary)
        for index, container in enumerate(containers):
            if container["extension"] != ".pak":
                continue
            source = archive_root.joinpath(*PurePosixPath(container["path"]).parts)
            destination = temporary_root / f"pak-{index:03d}"
            destination.mkdir()
            try:
                completed = subprocess.run(
                    [
                        str(unrealpak), str(source), "-Extract", str(destination),
                        "-Filter=*.uplugin",
                    ],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    timeout=300,
                    check=False,
                )
            except (OSError, subprocess.TimeoutExpired):
                issues.append(payload_issue(
                    "FINAL_PLUGIN_DESCRIPTOR_EXTRACTION_FAILED",
                    "UnrealPak could not extract final plugin descriptors",
                    container["path"],
                ))
                continue
            if completed.returncode != 0:
                issues.append(payload_issue(
                    "FINAL_PLUGIN_DESCRIPTOR_EXTRACTION_FAILED",
                    f"UnrealPak descriptor extraction returned {completed.returncode}",
                    container["path"],
                ))
                continue
            for extracted in sorted(destination.rglob("*.uplugin")):
                identity = _descriptor_identity_from_extracted_path(extracted, destination)
                if identity is None or identity.casefold() not in expected:
                    continue
                collision = identity.casefold()
                if collision in documents:
                    issues.append(payload_issue(
                        "DUPLICATE_EXTRACTED_FINAL_PLUGIN_DESCRIPTOR",
                        "a final plugin descriptor extracted more than once",
                        identity,
                    ))
                    continue
                try:
                    data = read_stable(extracted)
                    document = load_unreal_descriptor(extracted)
                except ContractError:
                    issues.append(payload_issue(
                        "FINAL_PLUGIN_DESCRIPTOR_JSON_INVALID",
                        "the exact final plugin descriptor is not valid Unreal JSON",
                        identity,
                    ))
                    continue
                if type(document) is not dict:
                    issues.append(payload_issue(
                        "FINAL_PLUGIN_DESCRIPTOR_JSON_INVALID",
                        "the exact final plugin descriptor root is not an object",
                        identity,
                    ))
                    continue
                documents[collision] = document
                payload_bindings[collision] = {
                    "bytes": len(data), "sha256": hash_bytes(data)
                }

        for record in records:
            collision = record["identity"].casefold()
            if collision in documents:
                continue
            loose = archive_root.joinpath(*PurePosixPath(record["identity"]).parts)
            if loose.is_file() and not loose.is_symlink():
                try:
                    data = read_stable(loose)
                    document = load_unreal_descriptor(loose)
                except ContractError:
                    document = None
                if type(document) is dict:
                    documents[collision] = document
                    payload_bindings[collision] = {
                        "bytes": len(data), "sha256": hash_bytes(data)
                    }
                    continue
            issues.append(payload_issue(
                "FINAL_PLUGIN_DESCRIPTOR_CONTENT_UNAVAILABLE",
                "the exact final descriptor content could not be loaded for dependency closure",
                record["identity"],
            ))
    return documents, payload_bindings, issues


def _reference_applies_to_windows_shipping_game(reference: dict[str, Any]) -> bool:
    dimensions = (
        ("PlatformAllowList", "PlatformDenyList", "Win64"),
        ("TargetConfigurationAllowList", "TargetConfigurationDenyList", "Shipping"),
        ("TargetAllowList", "TargetDenyList", "Game"),
    )
    for allow_key, deny_key, value in dimensions:
        allow = reference.get(allow_key)
        deny = reference.get(deny_key)
        if allow is not None and (
            type(allow) is not list or any(type(item) is not str for item in allow)
        ):
            raise ContractError(f"plugin dependency {allow_key} is malformed")
        if deny is not None and (
            type(deny) is not list or any(type(item) is not str for item in deny)
        ):
            raise ContractError(f"plugin dependency {deny_key} is malformed")
        if allow and value.casefold() not in {item.casefold() for item in allow}:
            return False
        if deny and value.casefold() in {item.casefold() for item in deny}:
            return False
    return True


def evaluate_final_descriptor_dependency_closure(
    records: list[dict[str, Any]], documents: dict[str, dict[str, Any]],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    issues: list[dict[str, Any]] = []
    by_name: dict[str, str] = {}
    for record in records:
        collision = record["name"].casefold()
        if collision in by_name and by_name[collision] != record["identity"]:
            issues.append(payload_issue(
                "DUPLICATE_FINAL_PLUGIN_NAME",
                "multiple final descriptors have the same plugin name",
                record["identity"],
            ))
        else:
            by_name[collision] = record["identity"]

    edges: list[dict[str, str]] = []
    gaps: list[dict[str, str]] = []
    for record in records:
        document = documents.get(record["identity"].casefold())
        if document is None:
            continue
        references = document.get("Plugins", [])
        if references is None:
            references = []
        if type(references) is not list:
            raise ContractError(
                f"final plugin descriptor Plugins is not an array: {record['identity']}"
            )
        for reference in references:
            if type(reference) is not dict:
                raise ContractError(
                    f"final plugin descriptor dependency is not an object: {record['identity']}"
                )
            enabled = reference.get("Enabled")
            optional = reference.get("Optional", False)
            name = reference.get("Name")
            if enabled is not True or optional is True:
                continue
            if type(optional) is not bool or type(name) is not str or not name:
                raise ContractError(
                    f"final plugin descriptor mandatory dependency is malformed: {record['identity']}"
                )
            if not _reference_applies_to_windows_shipping_game(reference):
                continue
            edge = {
                "parentIdentity": record["identity"],
                "parentName": record["name"],
                "dependencyName": name,
            }
            edges.append(edge)
            if name.casefold() not in by_name:
                gaps.append(edge)
    if gaps:
        issues.append(payload_issue(
            "MANDATORY_FINAL_PLUGIN_DESCRIPTOR_DEPENDENCY_MISSING",
            f"{len(gaps)} mandatory final descriptor dependency edges are unresolved",
        ))
    return {
        "mandatoryDependencyEdgeCount": len(edges),
        "mandatoryDependencyGapCount": len(gaps),
        "mandatoryDependencyGaps": gaps,
        "dependencyClosureComplete": not gaps and len(documents) == len(records),
    }, issues


def evaluate_required_dependency_payload_bindings(
    *, policy: dict[str, Any], engine_root: Path,
    payload_bindings: dict[str, dict[str, Any]],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    issues: list[dict[str, Any]] = []
    bindings: list[dict[str, Any]] = []
    mismatches: list[dict[str, Any]] = []
    for identity in policy["requiredDependencyClosureFinalPayloadDescriptors"]:
        source_path = engine_file(engine_root, identity)
        source = binding(source_path, identity)
        final_payload = payload_bindings.get(identity.casefold())
        matches = (
            final_payload is not None
            and final_payload["bytes"] == source["bytes"]
            and final_payload["sha256"] == source["sha256"]
        )
        item = {
            "identity": identity,
            "installedSource": source,
            "finalPayload": copy.deepcopy(final_payload),
            "matchesInstalledSource": matches,
        }
        bindings.append(item)
        if not matches:
            mismatches.append(copy.deepcopy(item))
    if mismatches:
        issues.append(payload_issue(
            "REQUIRED_PLUGIN_DEPENDENCY_DESCRIPTOR_PAYLOAD_MISMATCH",
            f"{len(mismatches)} required dependency descriptors do not exactly "
            "match installed UE source authority",
        ))
    return {
        "requiredDependencyClosurePayloadBindingCount": len(bindings),
        "requiredDependencyClosurePayloadBindings": bindings,
        "requiredDependencyClosurePayloadMismatchCount": len(mismatches),
        "requiredDependencyClosurePayloadMismatches": mismatches,
    }, issues


def inspect_final_payload(
    *, archive_root: Path, policy: dict[str, Any], engine_root: Path,
    unrealpak: Path,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    inventory_policy = load_staged_provenance_inventory_policy()
    first_archive, first_problems = staged_provenance.collect_archive(
        archive_root, inventory_policy
    )
    issues = [
        copy.deepcopy(item) for item in first_problems
        if item.get("code") not in NON_STRUCTURAL_PROVENANCE_CODES
    ]
    containers, container_problems, tool_binding = staged_provenance.inventory_containers(
        archive_root, first_archive, unrealpak, inventory_policy
    )
    issues.extend(
        copy.deepcopy(item) for item in container_problems
        if item.get("code") not in NON_STRUCTURAL_PROVENANCE_CODES
    )
    second_archive, second_problems = staged_provenance.collect_archive(
        archive_root, inventory_policy
    )
    issues.extend(
        copy.deepcopy(item) for item in second_problems
        if item.get("code") not in NON_STRUCTURAL_PROVENANCE_CODES
    )
    first_manifest = archive_manifest_sha(first_archive)
    second_manifest = archive_manifest_sha(second_archive)
    archive_stable = (
        first_manifest == second_manifest
        and archive_public(first_archive) == archive_public(second_archive)
    )
    if not archive_stable:
        issues.append(payload_issue(
            "FINAL_ARCHIVE_CHANGED_DURING_INVENTORY",
            "final archive bindings changed while containers were inventoried",
        ))

    final_authority = policy["finalPayloadAuthority"]
    payload_extensions = {
        value.casefold() for value in final_authority["containerExtensions"]
    } | {final_authority["ioStoreCompanionExtension"].casefold()}
    payload_files = [
        item for item in archive_public(second_archive)
        if PurePosixPath(item["path"]).suffix.casefold() in payload_extensions
    ]

    occurrences: dict[str, list[dict[str, Any]]] = {}
    identity_case: dict[str, str] = {}
    residual_occurrences: list[dict[str, Any]] = []
    residual = {
        value.casefold() for value in policy["reviewedResidualMetadataExclusions"]
    }

    def add_occurrence(identity: str, occurrence: dict[str, Any]) -> None:
        collision = identity.casefold()
        identity_case.setdefault(collision, identity)
        occurrences.setdefault(collision, []).append(occurrence)
        if collision in residual:
            residual_occurrences.append(occurrence)

    for item in second_archive:
        identity = item["path"]
        if identity.casefold() in residual:
            residual_occurrences.append(descriptor_occurrence(
                location="ARCHIVE_LOOSE_FILE", identity=identity,
                container=None, entry=item,
            ))
        if (
            identity.startswith("Engine/Plugins/")
            and identity.casefold().endswith(".uplugin")
        ):
            add_occurrence(identity, descriptor_occurrence(
                location="ARCHIVE_LOOSE_FILE", identity=identity,
                container=None, entry=item,
            ))
    for container in containers:
        location = (
            "PAK_NAMED_ENTRY"
            if container["extension"] == ".pak"
            else "IOSTORE_NAMED_ENTRY"
        )
        for entry in container["entries"]:
            if entry["scope"] != "CONTAINER_NAMED_ENTRY":
                continue
            identity = entry["identity"]
            occurrence = descriptor_occurrence(
                location=location, identity=identity, container=container, entry=entry,
            )
            if identity.casefold() in residual:
                residual_occurrences.append(occurrence)
            if (
                identity.startswith("Engine/Plugins/")
                and identity.casefold().endswith(".uplugin")
            ):
                add_occurrence(identity, occurrence)

    records: list[dict[str, Any]] = []
    restricted = set(policy["restrictedModuleTypes"])
    for collision in sorted(occurrences, key=lambda value: (identity_case[value].casefold(), identity_case[value])):
        identity = identity_case[collision]
        item_occurrences = occurrences[collision]
        if len(item_occurrences) != 1:
            issues.append(payload_issue(
                "DUPLICATE_FINAL_PLUGIN_DESCRIPTOR_IDENTITY",
                "a final plugin descriptor identity occurs more than once",
                identity,
            ))
        try:
            source_path = engine_file(engine_root, identity)
            document = load_unreal_descriptor(source_path)
            capability, module_types = classify_descriptor(document, restricted)
            source_binding = binding(source_path, identity)
        except ContractError:
            capability, module_types, source_binding = "UNCLASSIFIED", [], None
        records.append({
            "identity": identity,
            "name": PurePosixPath(identity).stem,
            "capability": capability,
            "moduleTypes": module_types,
            "sourceDescriptor": source_binding,
            "occurrences": item_occurrences,
        })

    metadata_only_exceptions = {
        item["descriptor"].casefold()
        for item in policy["reviewedMetadataOnlyDependencyDescriptors"]
    }
    evaluation, evaluation_issues = evaluate_final_descriptor_records(
        records, metadata_only_exceptions, residual_occurrences
    )
    issues.extend(evaluation_issues)
    required_evaluation, required_issues = evaluate_required_descriptor_authority(
        policy, records
    )
    evaluation.update(required_evaluation)
    issues.extend(required_issues)
    descriptor_documents, descriptor_payload_bindings, extraction_issues = (
        extract_final_descriptor_documents(
            archive_root=archive_root, containers=containers, records=records,
            unrealpak=unrealpak,
        )
    )
    issues.extend(extraction_issues)
    dependency_evaluation, dependency_issues = (
        evaluate_final_descriptor_dependency_closure(records, descriptor_documents)
    )
    evaluation.update(dependency_evaluation)
    issues.extend(dependency_issues)
    payload_binding_evaluation, payload_binding_issues = (
        evaluate_required_dependency_payload_bindings(
            policy=policy, engine_root=engine_root,
            payload_bindings=descriptor_payload_bindings,
        )
    )
    evaluation.update(payload_binding_evaluation)
    issues.extend(payload_binding_issues)
    container_bindings = []
    for container in containers:
        named_count = sum(
            entry["scope"] == "CONTAINER_NAMED_ENTRY" for entry in container["entries"]
        )
        anonymous_count = sum(
            entry["scope"] == "CONTAINER_ANONYMOUS_CHUNK" for entry in container["entries"]
        )
        container_bindings.append({
            "path": container["path"],
            "extension": container["extension"],
            "bytes": container["bytes"],
            "sha256": container["sha256"],
            "inventoryMethod": container["inventoryMethod"],
            "inventoryExitCode": container["inventoryExitCode"],
            "entryCount": len(container["entries"]),
            "namedEntryCount": named_count,
            "anonymousChunkCount": anonymous_count,
        })
    return {
        "authority": final_authority["scope"],
        "archive": {
            "fileCount": len(second_archive),
            "bytes": sum(item["bytes"] for item in second_archive),
            "canonicalManifestSha256": second_manifest,
            "files": archive_public(second_archive),
            "stableAcrossInventory": archive_stable,
        },
        "inventoryAuthority": {
            "policy": binding(
                STAGED_PROVENANCE_POLICY_PATH,
                "Config/DG_Session19StagedProvenancePolicy.json",
            ),
            "generator": binding(
                STAGED_PROVENANCE_GENERATOR_PATH,
                "Scripts/generate_dg_session19_staged_provenance.py",
            ),
            "unrealPak": tool_binding,
        },
        "containerPayloadFiles": payload_files,
        "containerCount": len(containers),
        "containers": container_bindings,
        **evaluation,
        "records": records,
    }, issues


def write_json_explicit(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    os.replace(temporary, path)


def build_receipt(
    *, policy: dict[str, Any], policy_bytes: bytes, engine_root: Path,
    archive: Path | None, manifest: Path | None, candidate_id: str | None,
    unrealpak: Path | None,
) -> tuple[dict[str, Any], bool]:
    reviewed = inspect_reviewed_descriptors(policy, engine_root)
    reviewed_metadata_only = inspect_reviewed_metadata_only_dependencies(
        policy, engine_root
    )
    required_dependency_closure = inspect_required_dependency_closure_descriptors(
        policy, engine_root
    )
    required_preserved = inspect_required_preserved_descriptors(policy, engine_root)
    required_absent = inspect_required_absent_descriptors(policy, engine_root)
    exclusion_binding = validate_exclusion_authority(
        ROOT / policy["exclusionAuthority"]["path"], policy,
    )
    build_binding, version = engine_version(engine_root)
    manifest_result: dict[str, Any] | None = None
    final_payload: dict[str, Any] | None = None
    issues: list[dict[str, Any]] = []
    if archive is None and manifest is not None:
        raise ContractError("staging manifest diagnostics require a concrete final archive")
    if archive is not None:
        if candidate_id is None or not CANDIDATE_ID_RE.fullmatch(candidate_id):
            raise ContractError("final archive validation requires a canonical candidate ID")
        if unrealpak is None:
            raise ContractError("final archive validation requires UnrealPak")
        final_payload, final_issues = inspect_final_payload(
            archive_root=archive, policy=policy, engine_root=engine_root,
            unrealpak=unrealpak,
        )
        issues.extend(final_issues)
        if manifest is not None:
            manifest_result = inspect_manifest(manifest, policy, engine_root)
            manifest_result["authoritativeForDistribution"] = False
            manifest_result["diagnosticOnly"] = True
    passed = not issues
    receipt = {
        "schema": "DiscGolfTour.Session19ShippingPluginCapabilityReceipt.v3",
        "schemaVersion": 3,
        "session": 19,
        "candidateId": candidate_id,
        "state": (
            "PASS_REVIEWED_METADATA_ONLY_AND_DEPENDENCY_CLOSED_PLUGIN_DESCRIPTORS_IN_FINAL_PAYLOAD"
            if archive is not None and passed
            else "PASS_EXACT_DESCRIPTOR_EXCLUSION_AND_DEPENDENCY_AUTHORITY"
            if passed else "FAIL_CLOSED"
        ),
        "policy": {
            "path": "Config/DG_Session19ShippingPluginCapabilityPolicy.json",
            "bytes": len(policy_bytes),
            "sha256": hash_bytes(policy_bytes),
        },
        "engine": {
            "association": policy["engineAssociation"],
            "buildVersion": build_binding,
            "version": version,
        },
        "exclusionAuthority": exclusion_binding,
        "reviewedDisabledPluginCount": len(reviewed),
        "reviewedDisabledPlugins": reviewed,
        "reviewedMetadataOnlyDependencyPluginCount": len(reviewed_metadata_only),
        "reviewedMetadataOnlyDependencyPlugins": reviewed_metadata_only,
        "requiredDependencyClosurePluginCount": len(required_dependency_closure),
        "requiredDependencyClosurePlugins": required_dependency_closure,
        "requiredPreservedPluginCount": len(required_preserved),
        "requiredPreservedPlugins": required_preserved,
        "requiredAbsentPluginCount": len(required_absent),
        "requiredAbsentPlugins": required_absent,
        "reviewedResidualMetadataExclusionCount": len(
            policy["reviewedResidualMetadataExclusions"]
        ),
        "reviewedResidualMetadataExclusions": copy.deepcopy(
            policy["reviewedResidualMetadataExclusions"]
        ),
        "finalPayload": final_payload,
        "stagingManifestDiagnostics": manifest_result,
        "reasonCodes": sorted({item["code"] for item in issues}),
        "issues": issues,
        "releaseBoundary": {
            "technicalPluginDescriptorGatePass": passed,
            "releaseReady": False,
            "blockerClosed": False,
            "remainingEvidence": (
                [] if archive is not None and passed
                else ["FRESH_WINDOWS_SHIPPING_FINAL_PAYLOAD_DEPENDENCY_CLOSED_AUDIT"]
            ),
        },
    }
    return receipt, passed


def run_self_test(policy: dict[str, Any], policy_bytes: bytes, engine_root: Path) -> int:
    count = 0
    receipt, passed = build_receipt(
        policy=policy, policy_bytes=policy_bytes, engine_root=engine_root,
        archive=None, manifest=None, candidate_id=None, unrealpak=None,
    )
    count += 1
    if (
        not passed
        or receipt["reviewedDisabledPluginCount"] != 31
        or receipt["reviewedMetadataOnlyDependencyPluginCount"] != 13
        or receipt["requiredDependencyClosurePluginCount"] != 15
    ):
        raise ContractError("live exclusion/descriptor authority did not pass")
    count += 1
    if any(
        item["capability"] != "EDITOR_DEVELOPER_ONLY"
        for item in receipt["reviewedDisabledPlugins"]
    ):
        raise ContractError("reviewed disable authority contains a runtime-capable plugin")
    count += 1
    if any(
        item["capability"] != "EDITOR_DEVELOPER_ONLY"
        or not item["metadataOnlyShippingException"]
        for item in receipt["reviewedMetadataOnlyDependencyPlugins"]
    ):
        raise ContractError("metadata-only dependency exception authority differs")
    count += 1
    if sum(
        item["metadataOnlyShippingException"]
        for item in receipt["requiredDependencyClosurePlugins"]
    ) != 13:
        raise ContractError("required dependency-closure capability split differs")
    count += 1
    if (
        receipt["exclusionAuthority"]["exactPluginRuleCount"] != 33
        or receipt["exclusionAuthority"]["reviewedDescriptorRuleCount"] != 31
        or receipt["exclusionAuthority"]["reviewedResidualMetadataRuleCount"] != 2
        or receipt["exclusionAuthority"][
            "reviewedMetadataOnlyDependencyExceptionCount"
        ] != 13
        or receipt["exclusionAuthority"]["runtimeCapableOrContentOnlyPluginRuleCount"] != 0
        or receipt["exclusionAuthority"]["broadPluginRuleCount"] != 0
    ):
        raise ContractError("live exact exclusion authority did not preserve plugin capabilities")
    for mutate in (
        lambda value: value.__setitem__("preserveRuntimeCapablePlugins", False),
        lambda value: value["reviewedDisabledPlugins"].append(
            copy.deepcopy(value["reviewedDisabledPlugins"][0])
        ),
        lambda value: value["reviewedMetadataOnlyDependencyDescriptors"].append(
            copy.deepcopy(value["reviewedMetadataOnlyDependencyDescriptors"][0])
        ),
        lambda value: value["requiredDependencyClosureFinalPayloadDescriptors"].pop(),
        lambda value: value.__setitem__(
            "requireMandatoryFinalDescriptorDependencyClosure", False
        ),
        lambda value: value["restrictedModuleTypes"].remove("Editor"),
        lambda value: value["reviewedDisabledPlugins"][0].__setitem__(
            "descriptor", "../escape.uplugin"
        ),
        lambda value: value["finalPayloadAuthority"].__setitem__(
            "stagingManifestAuthoritativeForDistribution", True
        ),
    ):
        candidate = copy.deepcopy(policy)
        mutate(candidate)
        count += 1
        try:
            validate_policy(candidate)
        except ContractError:
            pass
        else:
            raise ContractError("adversarial policy mutation survived")
    count += 1
    runtime_fixture = {"Modules": [{"Name": "Runtime", "Type": "Runtime"}]}
    if classify_descriptor(runtime_fixture, set(policy["restrictedModuleTypes"]))[0] != "RUNTIME_CAPABLE":
        raise ContractError("runtime-capable fixture was not preserved")
    count += 1
    if classify_descriptor({}, set(policy["restrictedModuleTypes"]))[0] != "CONTENT_ONLY":
        raise ContractError("content-only fixture was not preserved")
    project_denied_required_absent_descriptors = {
        "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
        "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin",
    }
    count += 1
    if (
            project_denied_required_absent_descriptors
            != set(policy["requiredAbsentFinalPayloadDescriptors"])
            or project_denied_required_absent_descriptors & set(
                policy["requiredPreservedFinalPayloadDescriptors"]
            )
            or project_denied_required_absent_descriptors & set(
                policy["requiredDependencyClosureFinalPayloadDescriptors"]
            )):
        raise ContractError("project-denied descriptor authority is not exact/disjoint")
    required_records = [
        {"identity": identity}
        for identity in [
            *policy["requiredPreservedFinalPayloadDescriptors"],
            *policy["requiredDependencyClosureFinalPayloadDescriptors"],
        ]
    ]
    count += 1
    required_evaluation, required_issues = evaluate_required_descriptor_authority(
        policy, required_records
    )
    if required_issues or any(
            required_evaluation[key] != expected
            for key, expected in (
                ("missingRequiredPreservedDescriptorCount", 0),
                ("requiredAbsentSurvivorCount", 0),
                ("missingRequiredDependencyClosureDescriptorCount", 0),
            )):
        raise ContractError(
            "absence of project-denied optional descriptors failed preserved authority"
        )
    for required_identity in policy["requiredPreservedFinalPayloadDescriptors"]:
        count += 1
        without_required = [
            record for record in required_records
            if record["identity"] != required_identity
        ]
        mutation_evaluation, mutation_issues = evaluate_required_descriptor_authority(
            policy, without_required
        )
        if (
                mutation_evaluation["missingRequiredPreservedDescriptors"]
                != [required_identity]
                or "REQUIRED_RUNTIME_OR_CONTENT_PLUGIN_DESCRIPTOR_MISSING"
                not in {item["code"] for item in mutation_issues}):
            raise ContractError(
                f"required preserved descriptor mutation survived: {required_identity}"
            )
    for absent_identity in policy["requiredAbsentFinalPayloadDescriptors"]:
        count += 1
        with_forbidden_survivor = [
            *required_records, {"identity": absent_identity},
        ]
        mutation_evaluation, mutation_issues = evaluate_required_descriptor_authority(
            policy, with_forbidden_survivor
        )
        if (
                mutation_evaluation["requiredAbsentSurvivors"] != [absent_identity]
                or "PROJECT_SHIPPING_DENIED_PLUGIN_DESCRIPTOR_IN_FINAL_PAYLOAD"
                not in {item["code"] for item in mutation_issues}):
            raise ContractError(
                f"required absent descriptor survivor escaped: {absent_identity}"
            )
    for dependency_identity in policy[
        "requiredDependencyClosureFinalPayloadDescriptors"
    ]:
        count += 1
        without_dependency = [
            record for record in required_records
            if record["identity"] != dependency_identity
        ]
        mutation_evaluation, mutation_issues = evaluate_required_descriptor_authority(
            policy, without_dependency
        )
        if (
            mutation_evaluation["missingRequiredDependencyClosureDescriptors"]
            != [dependency_identity]
            or "REQUIRED_PLUGIN_DEPENDENCY_CLOSURE_DESCRIPTOR_MISSING"
            not in {item["code"] for item in mutation_issues}
        ):
            raise ContractError(
                "required dependency-closure descriptor mutation survived: "
                + dependency_identity
            )
    matching_payload_bindings = {}
    for identity in policy["requiredDependencyClosureFinalPayloadDescriptors"]:
        source = binding(engine_file(engine_root, identity), identity)
        matching_payload_bindings[identity.casefold()] = {
            "bytes": source["bytes"], "sha256": source["sha256"]
        }
    count += 1
    binding_evaluation, binding_issues = evaluate_required_dependency_payload_bindings(
        policy=policy, engine_root=engine_root,
        payload_bindings=matching_payload_bindings,
    )
    if (
        binding_issues
        or binding_evaluation["requiredDependencyClosurePayloadBindingCount"] != 15
        or binding_evaluation["requiredDependencyClosurePayloadMismatchCount"] != 0
    ):
        raise ContractError("required dependency payload bindings did not pass")
    count += 1
    mismatched_payload_bindings = copy.deepcopy(matching_payload_bindings)
    first_dependency = policy["requiredDependencyClosureFinalPayloadDescriptors"][0]
    mismatched_payload_bindings[first_dependency.casefold()]["sha256"] = "0" * 64
    mismatch_evaluation, mismatch_issues = evaluate_required_dependency_payload_bindings(
        policy=policy, engine_root=engine_root,
        payload_bindings=mismatched_payload_bindings,
    )
    if (
        mismatch_evaluation["requiredDependencyClosurePayloadMismatchCount"] != 1
        or mismatch_evaluation["requiredDependencyClosurePayloadMismatches"][0][
            "identity"
        ] != first_dependency
        or "REQUIRED_PLUGIN_DEPENDENCY_DESCRIPTOR_PAYLOAD_MISMATCH"
        not in {item["code"] for item in mismatch_issues}
    ):
        raise ContractError("dependency payload mismatch survived")
    authority_path = ROOT / policy["exclusionAuthority"]["path"]
    authority_text = read_stable(authority_path).decode("utf-8-sig")
    first_rule = (
        policy["exclusionAuthority"]["requiredMountPrefix"]
        + policy["reviewedDisabledPlugins"][0]["descriptor"]
    )
    mutations = (
        authority_text.replace(f'+Files="{first_rule}"', "", 1),
        authority_text.replace(
            f'+Files="{first_rule}"',
            f'+Files="{first_rule.rsplit("/", 1)[0]}/*"',
            1,
        ),
        authority_text + '+Files=".../Engine/*"\n',
    )
    for mutated in mutations:
        count += 1
        with tempfile.TemporaryDirectory() as temporary_directory:
            fixture = Path(temporary_directory) / "DefaultPakFileRules.ini"
            fixture.write_text(mutated, encoding="utf-8")
            try:
                validate_exclusion_authority(fixture, policy)
            except ContractError:
                pass
            else:
                raise ContractError("adversarial Pak exclusion mutation survived")

    reviewed_fixture = {"Engine/Plugins/Editor/Reviewed/Reviewed.uplugin".casefold()}
    runtime_record = {
        "identity": "Engine/Plugins/Runtime/Runtime.uplugin",
        "capability": "RUNTIME_CAPABLE",
        "occurrences": [{"location": "PAK_NAMED_ENTRY"}],
    }
    content_record = {
        "identity": "Engine/Plugins/Content/Content.uplugin",
        "capability": "CONTENT_ONLY",
        "occurrences": [{"location": "IOSTORE_NAMED_ENTRY"}],
    }
    count += 1
    evaluation, evaluation_issues = evaluate_final_descriptor_records(
        [runtime_record, content_record], reviewed_fixture, []
    )
    staging_manifest_editor_only_diagnostic_count = 44
    if (
        evaluation_issues
        or evaluation["editorDeveloperOnlySurvivorCount"] != 0
        or staging_manifest_editor_only_diagnostic_count != 44
    ):
        raise ContractError("staging-manifest false positive affected final payload authority")

    for location in ("PAK_NAMED_ENTRY", "IOSTORE_NAMED_ENTRY", "ARCHIVE_LOOSE_FILE"):
        count += 1
        editor_record = {
            "identity": "Engine/Plugins/Editor/Reviewed/Reviewed.uplugin",
            "capability": "EDITOR_DEVELOPER_ONLY",
            "occurrences": [{"location": location}],
        }
        evaluation, evaluation_issues = evaluate_final_descriptor_records(
            [runtime_record, content_record, editor_record], reviewed_fixture, []
        )
        if (
            evaluation["editorDeveloperOnlySurvivorCount"] != 1
            or evaluation["reviewedMetadataOnlyDependencySurvivorCount"] != 1
            or evaluation["unreviewedEditorDeveloperOnlySurvivorCount"] != 0
            or evaluation_issues
        ):
            raise ContractError(
                f"{location} reviewed metadata-only dependency did not pass"
            )

    count += 1
    unreviewed_record = {
        "identity": "Engine/Plugins/Editor/Unknown/Unknown.uplugin",
        "capability": "EDITOR_DEVELOPER_ONLY",
        "occurrences": [{"location": "PAK_NAMED_ENTRY"}],
    }
    _, evaluation_issues = evaluate_final_descriptor_records(
        [unreviewed_record], reviewed_fixture, []
    )
    if "UNREVIEWED_EDITOR_DEVELOPER_ONLY_PLUGIN_DESCRIPTOR_IN_FINAL_PAYLOAD" not in {
        item["code"] for item in evaluation_issues
    }:
        raise ContractError("unreviewed final editor-only descriptor did not fail closed")

    count += 1
    _, evaluation_issues = evaluate_final_descriptor_records(
        [runtime_record], reviewed_fixture,
        [{"location": "PAK_NAMED_ENTRY", "identity": "residual.ini"}],
    )
    if "REVIEWED_RESIDUAL_PLUGIN_METADATA_IN_FINAL_PAYLOAD" not in {
        item["code"] for item in evaluation_issues
    }:
        raise ContractError("reviewed residual metadata survivor did not fail closed")

    closure_records = [
        {"identity": "Engine/Plugins/A/A.uplugin", "name": "A"},
        {"identity": "Engine/Plugins/B/B.uplugin", "name": "B"},
    ]
    closure_documents = {
        "engine/plugins/a/a.uplugin": {
            "Plugins": [{"Name": "B", "Enabled": True}]
        },
        "engine/plugins/b/b.uplugin": {},
    }
    count += 1
    closure, closure_issues = evaluate_final_descriptor_dependency_closure(
        closure_records, closure_documents
    )
    if (
        closure_issues or closure["mandatoryDependencyEdgeCount"] != 1
        or closure["mandatoryDependencyGapCount"] != 0
        or not closure["dependencyClosureComplete"]
    ):
        raise ContractError("complete mandatory dependency fixture failed")
    count += 1
    missing_closure, missing_issues = evaluate_final_descriptor_dependency_closure(
        closure_records[:1], {"engine/plugins/a/a.uplugin": closure_documents[
            "engine/plugins/a/a.uplugin"
        ]}
    )
    if (
        missing_closure["mandatoryDependencyGapCount"] != 1
        or missing_closure["mandatoryDependencyGaps"] != [{
            "parentIdentity": "Engine/Plugins/A/A.uplugin",
            "parentName": "A",
            "dependencyName": "B",
        }]
        or "MANDATORY_FINAL_PLUGIN_DESCRIPTOR_DEPENDENCY_MISSING"
        not in {item["code"] for item in missing_issues}
    ):
        raise ContractError("missing mandatory dependency fixture survived")
    count += 1
    optional_documents = {
        "engine/plugins/a/a.uplugin": {
            "Plugins": [{"Name": "B", "Enabled": True, "Optional": True}]
        }
    }
    optional_closure, optional_issues = evaluate_final_descriptor_dependency_closure(
        closure_records[:1], optional_documents
    )
    if optional_issues or not optional_closure["dependencyClosureComplete"]:
        raise ContractError("optional missing dependency failed closure")
    count += 1
    disabled_documents = {
        "engine/plugins/a/a.uplugin": {
            "Plugins": [{"Name": "B", "Enabled": False}]
        }
    }
    disabled_closure, disabled_issues = evaluate_final_descriptor_dependency_closure(
        closure_records[:1], disabled_documents
    )
    if disabled_issues or not disabled_closure["dependencyClosureComplete"]:
        raise ContractError("disabled missing dependency failed closure")
    count += 1
    filtered_documents = {
        "engine/plugins/a/a.uplugin": {
            "Plugins": [{
                "Name": "B", "Enabled": True, "TargetAllowList": ["Editor"]
            }]
        }
    }
    filtered_closure, filtered_issues = evaluate_final_descriptor_dependency_closure(
        closure_records[:1], filtered_documents
    )
    if filtered_issues or not filtered_closure["dependencyClosureComplete"]:
        raise ContractError("non-Game dependency failed Shipping closure")
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-receipt", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    policy_bytes = read_stable(POLICY_PATH)
    policy = load_json_strict(POLICY_PATH)
    validate_policy(policy)
    engine_root = args.engine_root.resolve()
    if args.self_test:
        count = run_self_test(policy, policy_bytes, engine_root)
        print(f"SESSION 19 SHIPPING PLUGIN CAPABILITY SELFTEST PASS: {count}/{count}")
        return 0
    if args.output is not None and args.expected_receipt is not None:
        raise ContractError("--output and --expected-receipt are mutually exclusive")
    if (args.output is not None or args.expected_receipt is not None) and args.archive is None:
        raise ContractError("candidate receipt operations require --archive")
    if args.manifest is not None and args.archive is None:
        raise ContractError("--manifest is diagnostic-only and requires --archive")
    unrealpak = resolve_unrealpak(args.unrealpak) if args.archive is not None else None
    receipt, passed = build_receipt(
        policy=policy,
        policy_bytes=policy_bytes,
        engine_root=engine_root,
        archive=args.archive.resolve() if args.archive else None,
        manifest=args.manifest.resolve() if args.manifest else None,
        candidate_id=args.candidate_id,
        unrealpak=unrealpak,
    )
    if args.output is not None:
        write_json_explicit(args.output.resolve(), receipt)
    if args.expected_receipt is not None:
        expected = load_json_strict(args.expected_receipt.resolve())
        if not staged_provenance.typed_equal(expected, receipt):
            raise ContractError("expected receipt differs from live final archive re-audit")
    manifest_result = receipt["stagingManifestDiagnostics"]
    final_payload = receipt["finalPayload"]
    suffix = ""
    if final_payload is not None:
        suffix += (
            f" final_descriptors={final_payload['pluginDescriptorCount']} "
            f"reviewed_metadata_only="
            f"{final_payload['reviewedMetadataOnlyDependencySurvivorCount']} "
            f"unreviewed_editor_only="
            f"{final_payload['unreviewedEditorDeveloperOnlySurvivorCount']} "
            f"dependency_gaps={final_payload['mandatoryDependencyGapCount']}"
        )
    if manifest_result is not None:
        suffix += (
            f" staging_diagnostic_descriptors={manifest_result['descriptorCount']} "
            f"staging_diagnostic_editor_only="
            f"{manifest_result['editorDeveloperOnlySurvivorCount']}"
        )
    print(
        f"SESSION 19 SHIPPING PLUGIN CAPABILITY {receipt['state']}: "
        f"reviewed_disabled={receipt['reviewedDisabledPluginCount']}{suffix}"
    )
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ContractError as exc:
        print(f"SESSION 19 SHIPPING PLUGIN CAPABILITY FAIL_CLOSED: {exc}")
        raise SystemExit(2)
