#!/usr/bin/env python3
"""Fail-closed staged-content audit for a Session 19 Windows Shipping candidate.

This is deliberately narrower than provenance review and gameplay acceptance. It
binds the corrected build verification and executable-marker receipts, inventories
every archive file, UFS/NonUFS manifest identity, and pak/IoStore entry, rejects
the frozen Session 19 forbidden content tokens, and proves the required three-hole
data and retained runtime packages are represented in the staged candidate.
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
from fnmatch import fnmatchcase
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
CONTENT_POLICY = ROOT / "Config/DG_Session19ShippingContentPolicy.json"
PROVENANCE_POLICY = ROOT / "Config/DG_Session19StagedProvenancePolicy.json"
BINARY_POLICY = ROOT / "Config/DG_Session19ShippingBinaryPolicy.json"
PLUGIN_CAPABILITY_POLICY = ROOT / "Config/DG_Session19ShippingPluginCapabilityPolicy.json"
PAK_RULES = ROOT / "Config/DefaultPakFileRules.ini"
PACKAGING_CONFIG = ROOT / "Config/DefaultGame.ini"
PROJECT_DESCRIPTOR = ROOT / "DiscGolfTour.uproject"
SHIPPING_GAME_TARGET = ROOT / "Source/DiscGolfTour.Target.cs"
SERIALIZED_RECEIPT = ROOT / "Evidence/Session19/SerializedAssetMigrationReceipt.json"
RUN_ID_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]+$")
PAK_ENTRY_RE = re.compile(r'^LogPakFile: Display: "(?P<path>.+)" offset: ')
PAK_SUMMARY_RE = re.compile(r"^LogPakFile: Display: (?P<count>\d+) files \(")
PAK_RULE_RE = re.compile(r'^\+Files="(?P<pattern>[^"]+)"$')

REQUIRED_ENGINE_DEVELOPMENT_ROOTS = (
    "/Engine/Content/VREditor/",
    "/Engine/Content/EditorMeshes/",
    "/Engine/Content/Slate/Testing/",
    "/Engine/Plugins/Developer/Concert/",
    "/Engine/Plugins/ChaosVD/",
)
REQUIRED_ENGINE_CONTENT_PAK_EXCLUSIONS = tuple(
    f"...{root.rstrip('/')}/*" for root in REQUIRED_ENGINE_DEVELOPMENT_ROOTS[:3]
)
PROTECTED_RUNTIME_SLATE_IDENTITIES = (
    "Engine/Content/Slate/Fonts/Roboto-Regular.ttf",
    "Engine/Content/Slate/Common/Selection.png",
    "Engine/Content/Slate/Starship/Common/Window/WindowButton_Close.png",
)
PROTECTED_PLUGIN_DESCRIPTOR_IDENTITIES = (
    "Engine/Plugins/EnhancedInput/EnhancedInput.uplugin",
    "Engine/Plugins/PCG/PCG.uplugin",
    "Engine/Plugins/Enterprise/VariantManagerContent/VariantManagerContent.uplugin",
)
REQUIRED_NEVER_COOK_VIRTUAL_ROOTS = (
    "/Engine/VREditor",
    "/Engine/EditorMaterials",
    "/Engine/EditorMeshes",
    "/Engine/EditorResources",
    "/SpeedTreeImporter",
    "/Landmass",
)
REQUIRED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES = (
    "ConcertMain",
    "ConcertSyncClient",
    "ConcertSyncCore",
    "ConcertSharedSlate",
    "AssetManagerEditor",
)
REQUIRED_PROJECT_SHIPPING_DENIED_PLUGINS = (
    "ChaosVD",
    "ConcertMain",
    "ConcertSyncClient",
    "ConcertSyncCore",
    "ConcertSharedSlate",
    "AssetManagerEditor",
    "ConsoleVariables",
    "FacialAnimation",
    "SpeedTreeImporter",
)
REQUIRED_PROJECT_EDITOR_ONLY_PLUGINS: tuple[str, ...] = ()
REQUIRED_PROJECT_SHIPPING_DEPENDENCY_ENABLED_PLUGINS = (
    "BlueprintMaterialTextureNodes", "EditorScriptingUtilities", "Landmass",
)
REQUIRED_RESIDUAL_PAK_EXCLUSIONS = (
    ".../Engine/Plugins/ChaosClothAssetEditorCore/Config/DefaultChaosClothAssetEditorCore.ini",
    ".../Engine/Plugins/Editor/EditorScriptingUtilities/Config/DefaultEditorScriptingUtilities.ini",
)
DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES = (
    "Binaries", "Config", "Content", "Resources", "Source",
)
REQUIRED_SHIPPING_FORBIDDEN_PLUGIN_SUBTREES = tuple(
    f"/Engine/Plugins/Experimental/Landmass/{subdirectory}/"
    for subdirectory in DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES
)
REQUIRED_PRESENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS = (
    "Engine/Plugins/Animation/ControlRigModules/ControlRigModules.uplugin",
    "Engine/Plugins/BaseMaterial/BaseMaterial.uplugin",
)
REQUIRED_ABSENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS = (
    "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
    "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin",
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
    ("ChaosClothAssetDataflowNodes", "Engine/Plugins/ChaosClothAssetDataflowNodes/ChaosClothAssetDataflowNodes.uplugin"),
    ("ChaosClothAssetEditorCore", "Engine/Plugins/ChaosClothAssetEditorCore/ChaosClothAssetEditorCore.uplugin"),
    ("ChaosSolverPlugin", "Engine/Plugins/Experimental/ChaosSolverPlugin/ChaosSolverPlugin.uplugin"),
    ("ContentBrowserAssetDataSource", "Engine/Plugins/Editor/ContentBrowser/ContentBrowserAssetDataSource/ContentBrowserAssetDataSource.uplugin"),
    ("DataValidation", "Engine/Plugins/Editor/DataValidation/DataValidation.uplugin"),
    ("EditorDataStorageFeatures", "Engine/Plugins/Experimental/EditorDataStorageFeatures/EditorDataStorageFeatures.uplugin"),
    ("EditorScriptingUtilities", "Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin"),
    ("EngineAssetDefinitions", "Engine/Plugins/Editor/EngineAssetDefinitions/EngineAssetDefinitions.uplugin"),
    ("LevelSequenceEditor", "Engine/Plugins/MovieScene/LevelSequenceEditor/LevelSequenceEditor.uplugin"),
    ("LiveLinkDevice", "Engine/Plugins/Animation/LiveLinkDevice/LiveLinkDevice.uplugin"),
    ("PluginUtils", "Engine/Plugins/Developer/PluginUtils/PluginUtils.uplugin"),
    ("ProxyLODPlugin", "Engine/Plugins/Editor/ProxyLODPlugin/ProxyLODPlugin.uplugin"),
    ("VariantManager", "Engine/Plugins/Enterprise/VariantManager/VariantManager.uplugin"),
)
EXPECTED_AUTHORITATIVE_IDENTITY_POLICY = {
    "authoritativeScopes": [
        "ARCHIVE_FILE", "NONUFS_ENTRY", "CONTAINER_NAMED_ENTRY",
    ],
    "manifestUfsDevelopmentRows": "NON_AUTHORITATIVE_DIAGNOSTIC_ONLY",
    "requiredDependencyClosureDescriptorExceptions": list(
        REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS
    ),
    "forbiddenIdentityExceptions": [],
}


class DuplicateKeyError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> Any:
    return load_json_text(path.read_text(encoding="utf-8-sig"))


def load_json_text(text: str) -> Any:
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def evidence_binding(path: Path) -> dict[str, Any]:
    return {
        "path": path.relative_to(ROOT).as_posix(),
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def canonical_relative(value: str) -> str:
    candidate = value.replace("\\", "/")
    while candidate.startswith("../../../"):
        candidate = candidate[9:]
    candidate = candidate.lstrip("/")
    pure = PurePosixPath(candidate)
    if (
        not candidate
        or pure.is_absolute()
        or any(part in ("", ".", "..") for part in pure.parts)
        or pure.as_posix() != candidate
        or ":" in candidate
        or "\x00" in candidate
    ):
        raise ValueError(f"non-canonical staged identity: {value!r}")
    return candidate


def normalized_for_token_scan(identity: str) -> str:
    return "/" + identity.strip("/") + "/"


def matches_any_token(identity: str, tokens: Iterable[str]) -> bool:
    normalized = normalized_for_token_scan(identity).casefold()
    return any(token.casefold() in normalized for token in tokens)


def pak_pattern_matches(identity: str, pattern: str) -> bool:
    normalized_pattern = pattern.replace("\\", "/")
    if normalized_pattern.startswith(".../"):
        normalized_pattern = "*/" + normalized_pattern[4:]
    return fnmatchcase(("/" + identity.lstrip("/")).casefold(), normalized_pattern.casefold())


def ini_section_lines(text: str, section: str) -> list[str]:
    result: list[str] = []
    active = False
    expected_header = f"[{section}]".casefold()
    found = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line.startswith("[") and line.endswith("]"):
            active = line.casefold() == expected_header
            found = found or active
            continue
        if active and line and not line.startswith((";", "#")):
            result.append(line)
    if not found:
        raise ValueError(f"required ini section is missing: {section}")
    return result


def csharp_initializer_string_set(text: str, name: str) -> list[str] | None:
    match = re.search(
        rf"\b{re.escape(name)}\s*=\s*new\s*\(\s*\)\s*\{{(?P<body>.*?)\}}\s*;",
        text,
        flags=re.DOTALL,
    )
    if match is None:
        return None
    return re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', match.group("body"))


def validate_project_shipping_denials(project_text: str) -> list[str]:
    problems: list[str] = []
    try:
        project = load_json_text(project_text)
    except (ValueError, TypeError) as exc:
        return [f"project descriptor is not strict JSON: {type(exc).__name__}"]
    plugins = project.get("Plugins") if isinstance(project, dict) else None
    if not isinstance(plugins, list):
        return ["project descriptor Plugins is not an array"]

    by_name: dict[str, dict[str, Any]] = {}
    for plugin in plugins:
        if not isinstance(plugin, dict) or not isinstance(plugin.get("Name"), str):
            problems.append("project descriptor contains an invalid plugin reference")
            continue
        folded = plugin["Name"].casefold()
        if folded in by_name:
            problems.append(f"project descriptor duplicates plugin reference: {plugin['Name']}")
            continue
        by_name[folded] = plugin

    for name in REQUIRED_PROJECT_SHIPPING_DENIED_PLUGINS:
        plugin = by_name.get(name.casefold())
        if plugin is None:
            problems.append(f"project Shipping-denied plugin is missing: {name}")
            continue
        if plugin.get("Name") != name or plugin.get("Enabled") is not True:
            problems.append(f"project Shipping-denied plugin identity is invalid: {name}")
        if plugin.get("TargetConfigurationDenyList") != ["Shipping"]:
            problems.append(f"project plugin is not denied exactly for Shipping: {name}")

    for name in REQUIRED_PROJECT_EDITOR_ONLY_PLUGINS:
        plugin = by_name.get(name.casefold())
        if plugin is None:
            problems.append(f"project editor-only plugin is missing: {name}")
            continue
        if plugin.get("Name") != name or plugin.get("Enabled") is not True:
            problems.append(f"project editor-only plugin identity is invalid: {name}")
        if plugin.get("TargetAllowList") != ["Editor"]:
            problems.append(f"project plugin is not restricted exactly to Editor: {name}")
    for name in REQUIRED_PROJECT_SHIPPING_DEPENDENCY_ENABLED_PLUGINS:
        plugin = by_name.get(name.casefold())
        if plugin != {"Name": name, "Enabled": True}:
            problems.append(
                f"project dependency-closure plugin is not enabled without target filters: {name}"
            )
    return problems


def validate_shipping_target_dependency_filter(target_text: str) -> list[str]:
    problems: list[str] = []
    configured = csharp_initializer_string_set(
        target_text, "ShippingIgnoredPluginDependencies"
    )
    if configured is None:
        return ["Shipping target dependency-ignore set is missing"]
    if len(configured) != len({value.casefold() for value in configured}):
        problems.append("Shipping target dependency-ignore set contains duplicates")
    if configured != list(REQUIRED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES):
        problems.append("Shipping target dependency-ignore set differs from authority")
    for required_fragment in (
        "public override bool ShouldIgnorePluginDependency(",
        "Configuration == UnrealTargetConfiguration.Shipping",
        "ShippingIgnoredPluginDependencies.Contains(descriptor.Name)",
        "return true;",
        "return base.ShouldIgnorePluginDependency(parentInfo, descriptor);",
    ):
        if target_text.count(required_fragment) != 1:
            problems.append(
                f"Shipping target dependency filter must contain exactly one {required_fragment}"
            )
    return problems


def validate_package_hardening_config(
    content_policy: dict[str, Any],
    provenance_policy: dict[str, Any],
    plugin_policy: dict[str, Any],
    pak_rules_text: str,
    packaging_config_text: str,
    project_descriptor_text: str,
    shipping_target_text: str,
) -> list[str]:
    problems: list[str] = []
    configured_tokens = provenance_policy.get("forbiddenDevelopmentPathTokens")
    if not isinstance(configured_tokens, list) or not all(
        isinstance(token, str) and token for token in configured_tokens
    ):
        return ["forbiddenDevelopmentPathTokens is not a non-empty string array"]

    configured_folded = {token.casefold() for token in configured_tokens}
    for root in REQUIRED_ENGINE_DEVELOPMENT_ROOTS:
        if root.casefold() not in configured_folded:
            problems.append(f"required engine development root is not classified: {root}")
    for identity in PROTECTED_RUNTIME_SLATE_IDENTITIES:
        if matches_any_token(identity, configured_tokens):
            problems.append(f"required runtime Slate identity is over-classified: {identity}")

    try:
        rule_lines = ini_section_lines(
            pak_rules_text, "ExcludeDevelopmentOnlyContentForWindowsShipping"
        )
        packaging_lines = ini_section_lines(
            packaging_config_text, "/Script/UnrealEd.ProjectPackagingSettings"
        )
    except ValueError as exc:
        return [str(exc)]
    patterns = [
        match.group("pattern")
        for line in rule_lines
        if (match := PAK_RULE_RE.fullmatch(line)) is not None
    ]
    pattern_folded = {pattern.casefold() for pattern in patterns}
    for pattern in REQUIRED_ENGINE_CONTENT_PAK_EXCLUSIONS:
        if pattern.casefold() not in pattern_folded:
            problems.append(f"required Windows Shipping Pak exclusion is missing: {pattern}")
    for identity in PROTECTED_RUNTIME_SLATE_IDENTITIES:
        if any(pak_pattern_matches(identity, pattern) for pattern in patterns):
            problems.append(f"required runtime Slate identity is over-excluded: {identity}")

    if (
        plugin_policy.get("schema")
        != "DiscGolfTour.Session19ShippingPluginCapabilityPolicy.v3"
        or plugin_policy.get("schemaVersion") != 3
    ):
        problems.append("shipping plugin capability policy must be exact schema v3")

    reviewed = plugin_policy.get("reviewedDisabledPlugins")
    if not isinstance(reviewed, list) or len(reviewed) != 31:
        problems.append("shipping plugin capability policy must review exactly 31 disabled descriptors")
        reviewed = []
    exact_plugin_patterns = {
        f".../{item.get('descriptor', '')}".casefold()
        for item in reviewed if isinstance(item, dict)
    }
    configured_plugin_patterns = {
        pattern.casefold() for pattern in patterns
        if "/engine/plugins/" in pattern.casefold()
        and pattern.casefold().endswith(".uplugin")
    }
    if configured_plugin_patterns != exact_plugin_patterns:
        problems.append(
            "Windows Shipping plugin exclusions must equal the 31 reviewed exact disabled descriptors"
        )
    if plugin_policy.get("requiredPreservedFinalPayloadDescriptors") != list(
        REQUIRED_PRESENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS
    ):
        problems.append("required-present final-payload plugin descriptors differ from authority")
    if plugin_policy.get("requiredAbsentFinalPayloadDescriptors") != list(
        REQUIRED_ABSENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS
    ):
        problems.append("required-absent final-payload plugin descriptors differ from authority")
    if plugin_policy.get("requiredDependencyClosureFinalPayloadDescriptors") != list(
        REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS
    ):
        problems.append("required dependency-closure descriptors differ from authority")
    if plugin_policy.get("requireMandatoryFinalDescriptorDependencyClosure") is not True:
        problems.append("mandatory final descriptor dependency closure is not required")
    expected_metadata_only = [
        {"name": name, "descriptor": descriptor}
        for name, descriptor in REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS
    ]
    if plugin_policy.get("reviewedMetadataOnlyDependencyDescriptors") != expected_metadata_only:
        problems.append("reviewed metadata-only dependency descriptors differ from authority")
    if (
        plugin_policy.get(
            "allowOnlyReviewedMetadataDependencyEditorDeveloperDescriptorsInFinalPayload"
        )
        is not True
    ):
        problems.append("metadata-only dependency descriptor exception is not fail-closed")
    if plugin_policy.get("reviewedResidualMetadataExclusions") != [
        value.removeprefix(".../") for value in REQUIRED_RESIDUAL_PAK_EXCLUSIONS
    ]:
        problems.append("reviewed residual metadata exclusions differ from authority")
    metadata_descriptor_list = [
        descriptor for _, descriptor in REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS
    ]
    metadata_descriptors = set(metadata_descriptor_list)
    closure_descriptors = set(REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS)
    if not metadata_descriptors < closure_descriptors:
        problems.append("metadata-only descriptors are not a strict dependency-closure subset")
    if metadata_descriptors & {
        item.get("descriptor") for item in reviewed if isinstance(item, dict)
    }:
        problems.append("metadata-only descriptors are also classified as disabled")
    for identity in PROTECTED_PLUGIN_DESCRIPTOR_IDENTITIES:
        if any(pak_pattern_matches(identity, pattern) for pattern in patterns):
            problems.append(f"runtime/content plugin descriptor is over-excluded: {identity}")
    for identity in REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS:
        if any(pak_pattern_matches(identity, pattern) for pattern in patterns):
            problems.append(f"dependency-closure plugin descriptor is over-excluded: {identity}")

    for residual in REQUIRED_RESIDUAL_PAK_EXCLUSIONS:
        if patterns.count(residual) != 1:
            problems.append(f"exact residual Pak exclusion is missing or duplicated: {residual}")
    residual_patterns = {
        pattern.casefold() for pattern in patterns
        if any(
            pattern.casefold().startswith(
                (".../" + descriptor.rsplit("/", 1)[0] + "/config/").casefold()
            )
            for descriptor in metadata_descriptors
        )
    }
    if residual_patterns != {
        pattern.casefold() for pattern in REQUIRED_RESIDUAL_PAK_EXCLUSIONS
    }:
        problems.append("residual metadata-only plugin Pak exclusions differ from authority")

    policy_never_cook = content_policy.get("neverCookRoots")
    if (
        not isinstance(policy_never_cook, list)
        or not all(isinstance(value, str) and value for value in policy_never_cook)
        or len(policy_never_cook) != len({value.casefold() for value in policy_never_cook})
    ):
        problems.append("shipping content policy neverCookRoots is invalid")
        policy_never_cook = []
    policy_never_cook_folded = {value.casefold() for value in policy_never_cook}
    for root in REQUIRED_NEVER_COOK_VIRTUAL_ROOTS:
        expected_line = f'+DirectoriesToNeverCook=(Path="{root}")'
        if packaging_lines.count(expected_line) != 1:
            problems.append(f"required virtual NeverCook root is missing or duplicated: {root}")
        if root.casefold() not in policy_never_cook_folded:
            problems.append(f"content policy does not bind virtual NeverCook root: {root}")

    problems.extend(validate_project_shipping_denials(project_descriptor_text))
    problems.extend(validate_shipping_target_dependency_filter(shipping_target_text))

    forbidden_shipping_tokens = content_policy.get("forbiddenShippingPathTokens")
    if not isinstance(forbidden_shipping_tokens, list) or not all(
        isinstance(token, str) and token for token in forbidden_shipping_tokens
    ):
        problems.append("forbiddenShippingPathTokens is not a non-empty string array")
        forbidden_shipping_tokens = []
    for root in REQUIRED_SHIPPING_FORBIDDEN_PLUGIN_SUBTREES:
        if forbidden_shipping_tokens.count(root) != 1:
            problems.append(
                f"required final-payload forbidden plugin subtree is missing or duplicated: {root}"
            )

    if content_policy.get("authoritativeIdentityPolicy") != EXPECTED_AUTHORITATIVE_IDENTITY_POLICY:
        problems.append("authoritative identity scope policy differs from authority")

    expected_hardening = {
        "pakRules": evidence_binding(PAK_RULES),
        "packagingConfig": evidence_binding(PACKAGING_CONFIG),
        "projectDescriptor": evidence_binding(PROJECT_DESCRIPTOR),
        "shippingGameTarget": evidence_binding(SHIPPING_GAME_TARGET),
        "shippingPluginCapabilityPolicy": evidence_binding(PLUGIN_CAPABILITY_POLICY),
        "reviewedExactPluginDescriptorExclusionCount": 31,
        "broadPluginDirectoryExclusionsAllowed": False,
        "preserveRuntimeCapablePlugins": True,
        "preserveContentOnlyPlugins": True,
        "requiredNeverCookVirtualRoots": list(REQUIRED_NEVER_COOK_VIRTUAL_ROOTS),
        "shippingIgnoredPluginDependencies": list(
            REQUIRED_SHIPPING_IGNORED_PLUGIN_DEPENDENCIES
        ),
        "projectTargetConfigurationShippingDeniedPlugins": list(
            REQUIRED_PROJECT_SHIPPING_DENIED_PLUGINS
        ),
        "projectEditorOnlyPlugins": list(REQUIRED_PROJECT_EDITOR_ONLY_PLUGINS),
        "projectShippingDependencyEnabledPlugins": list(
            REQUIRED_PROJECT_SHIPPING_DEPENDENCY_ENABLED_PLUGINS
        ),
        "residualExactPakExclusions": list(REQUIRED_RESIDUAL_PAK_EXCLUSIONS),
        "shippingForbiddenPluginContentRoots": list(
            REQUIRED_SHIPPING_FORBIDDEN_PLUGIN_SUBTREES
        ),
        "requiredPresentFinalPayloadPluginDescriptors": list(
            REQUIRED_PRESENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS
        ),
        "requiredAbsentFinalPayloadPluginDescriptors": list(
            REQUIRED_ABSENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS
        ),
        "requiredDependencyClosureFinalPayloadDescriptors": list(
            REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS
        ),
        "reviewedMetadataOnlyDependencyDescriptors": metadata_descriptor_list,
        "metadataOnlyDependencyForbiddenSubdirectories": list(
            DEPENDENCY_PLUGIN_FORBIDDEN_SUBDIRECTORIES
        ),
    }
    if content_policy.get("packageHardening") != expected_hardening:
        problems.append("shipping content policy package-hardening binding differs from disk")

    for required_line in (
        'Platforms="Windows"', 'Targets="Shipping"',
        "bExcludeFromPaks=true", "bOverrideChunkManifest=true",
    ):
        if rule_lines.count(required_line) != 1:
            problems.append(f"Windows Shipping Pak rule must contain exactly one {required_line}")
    if packaging_lines.count("bSkipEditorContent=True") != 1:
        problems.append("DefaultGame.ini must enable bSkipEditorContent exactly once")
    return problems


def collect_archive(archive: Path) -> list[dict[str, Any]]:
    files: list[dict[str, Any]] = []
    for path in sorted(archive.rglob("*"), key=lambda item: item.as_posix().casefold()):
        if path.is_symlink():
            raise ValueError(f"archive contains a symlink: {path}")
        if not path.is_file():
            continue
        identity = canonical_relative(path.relative_to(archive).as_posix())
        files.append({
            "path": identity,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        })
    lowered = [item["path"].casefold() for item in files]
    if len(lowered) != len(set(lowered)):
        raise ValueError("archive contains duplicate case-insensitive paths")
    return files


def parse_manifest(path: Path) -> list[str]:
    identities: list[str] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        parts = raw_line.split("\t")
        if len(parts) != 2 or not parts[1].endswith("Z"):
            raise ValueError(f"malformed stage manifest line {line_number}: {path.name}")
        identities.append(canonical_relative(parts[0]))
    lowered = [identity.casefold() for identity in identities]
    if not identities or len(lowered) != len(set(lowered)):
        raise ValueError(f"empty or duplicate stage manifest identities: {path.name}")
    return identities


def run_tool(command: list[str], timeout: int = 300) -> str:
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"container inventory failed with exit {completed.returncode}: "
            f"{completed.stdout[-2000:]}"
        )
    return completed.stdout


def inventory_pak(unrealpak: Path, path: Path) -> tuple[list[str], dict[str, Any]]:
    output = run_tool([str(unrealpak), str(path), "-List"])
    identities: list[str] = []
    summary_count: int | None = None
    for line in output.splitlines():
        match = PAK_ENTRY_RE.match(line)
        if match:
            identities.append(canonical_relative(match.group("path")))
        summary = PAK_SUMMARY_RE.match(line)
        if summary:
            summary_count = int(summary.group("count"))
    if summary_count is None or summary_count != len(identities):
        raise ValueError(
            f"pak list summary differs: summary={summary_count} parsed={len(identities)}"
        )
    if len({item.casefold() for item in identities}) != len(identities):
        raise ValueError("pak inventory contains duplicate identities")
    canonical_hash = hashlib.sha256(
        "".join(f"{item}\n" for item in identities).encode("utf-8")
    ).hexdigest().upper()
    return identities, {
        "method": "UnrealPak positional -List",
        "entryCount": len(identities),
        "canonicalIdentitySha256": canonical_hash,
    }


def inventory_utoc(unrealpak: Path, path: Path) -> tuple[list[str], dict[str, Any]]:
    with tempfile.TemporaryDirectory(prefix="dgt-s19-content-audit-") as temp_name:
        csv_path = Path(temp_name) / "inventory.csv"
        run_tool([str(unrealpak), f"-ListContainer={path}", f"-CSV={csv_path}"])
        if not csv_path.is_file():
            raise ValueError(f"UnrealPak did not emit IoStore CSV for {path.name}")
        with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream, skipinitialspace=True))
    if not rows or "OrderInContainer" not in rows[0] or "Filename" not in rows[0]:
        raise ValueError(f"IoStore CSV is empty or malformed for {path.name}")
    named: list[str] = []
    anonymous: list[str] = []
    for expected_order, row in enumerate(rows):
        if int(row["OrderInContainer"]) != expected_order:
            raise ValueError(f"IoStore order is not contiguous for {path.name}")
        filename = (row.get("Filename") or "").strip()
        if filename and not (filename.startswith("<") and filename.endswith(">")):
            named.append(canonical_relative(filename))
        else:
            chunk_type = (row.get("ChunkType") or "Unknown").strip() or "Unknown"
            chunk_id = (row.get("ChunkId") or "UNKNOWN").strip() or "UNKNOWN"
            offset = (row.get("Offset") or "0").strip() or "0"
            size = (row.get("Size") or "0").strip() or "0"
            digest = (row.get("Hash") or "UNKNOWN").strip().removeprefix("0x") or "UNKNOWN"
            anonymous.append(f"<{chunk_type}>#{chunk_id}@{offset}:{size}:{digest}")
    if len({item.casefold() for item in named}) != len(named):
        raise ValueError(f"IoStore named inventory contains duplicates for {path.name}")
    canonical_hash = hashlib.sha256(
        "".join(f"{item}\n" for item in named + anonymous).encode("utf-8")
    ).hexdigest().upper()
    return named, {
        "method": "UnrealPak -ListContainer CSV",
        "chunkCount": len(rows),
        "namedEntryCount": len(named),
        "anonymousChunkCount": len(anonymous),
        "canonicalIdentitySha256": canonical_hash,
    }


def find_unrealpak(explicit: Path | None) -> Path:
    candidates = [
        explicit,
        Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealPak.exe"),
        Path(r"C:\Program Files\Epic Games\UE_5.8.1\Engine\Binaries\Win64\UnrealPak.exe"),
    ]
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError("UnrealPak.exe was not found")


def marker_group(binary_policy: dict[str, Any], group_id: str) -> list[str]:
    for group in binary_policy["forbiddenMarkerGroups"]:
        if group["id"] == group_id:
            return list(group["markers"])
    raise ValueError(f"binary marker group is missing: {group_id}")


def unique_tokens(values: Iterable[str]) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for value in values:
        key = value.casefold()
        if key not in seen:
            seen.add(key)
            result.append(value)
    return result


def token_categories(
    content_policy: dict[str, Any],
    provenance_policy: dict[str, Any],
    binary_policy: dict[str, Any],
) -> dict[str, list[str]]:
    return {
        "LEGACY_CHARACTER_FRAMEWORK": unique_tokens([
            "DiscGolfCharacterFramework",
            *marker_group(binary_policy, "legacy_character_framework"),
        ]),
        "QUARANTINED_FAB_ROOTS": [
            "PN_interactiveSpruceForest", "Stump_Scanned", "WaterMaterials",
        ],
        "THROW_LAB": unique_tokens(marker_group(binary_policy, "throw_lab") + ["DGT_ThrowLab"]),
        "CAREER_AI": marker_group(binary_policy, "career_ai_runtime_runner_save"),
        "DEVELOPMENT_TEST_EDITOR": unique_tokens([
            *provenance_policy["forbiddenDevelopmentPathTokens"],
            "/Source/", "/Intermediate/", "/Saved/",
            "DiscGolfTourDeveloper", "/Script/DiscGolfTourDeveloper",
        ]),
        "PROTOTYPE_CUSTOMIZATION_OUTFIT": unique_tokens([
            *marker_group(binary_policy, "character_prototype_proxy_assets"),
            "/Animation/Mocap/", "/Animation/Throws/",
            "/Characters/Customization/", "/Characters/MetaHuman/Source/",
            "/Materials/CharacterCustomization/", "/Materials/Outfits/", "/Outfits/",
        ]),
        "COOK_MANIFEST_REGRESSION": marker_group(binary_policy, "regression_and_cook_manifest_json"),
        "ALL_FROZEN_PATH_TOKENS": unique_tokens([
            *content_policy["forbiddenShippingPathTokens"],
            *provenance_policy["forbiddenPathTokens"],
        ]),
    }


def classify_forbidden_identities(
    categories: dict[str, list[str]],
    authoritative_identities: Iterable[tuple[str, str | None, str]],
    ufs_manifest_identities: Iterable[str],
    exact_descriptor_exceptions: Iterable[str] = (),
) -> dict[str, Any]:
    authoritative = list(authoritative_identities)
    ufs_diagnostics = list(ufs_manifest_identities)
    exact_exceptions = set(exact_descriptor_exceptions)
    results: dict[str, Any] = {}
    for category, tokens in categories.items():
        matches: list[dict[str, Any]] = []
        diagnostics: list[dict[str, Any]] = []
        for scope, container, identity in authoritative:
            if identity in exact_exceptions:
                continue
            folded = normalized_for_token_scan(identity).casefold()
            for token in tokens:
                if token.casefold() in folded:
                    matches.append({
                        "token": token,
                        "scope": scope,
                        "container": container,
                        "identity": identity,
                    })
        for identity in ufs_diagnostics:
            folded = normalized_for_token_scan(identity).casefold()
            for token in tokens:
                if token.casefold() in folded:
                    diagnostics.append({
                        "token": token,
                        "scope": "UFS_ENTRY_DIAGNOSTIC",
                        "container": None,
                        "identity": identity,
                    })
        results[category] = {
            "passed": not matches,
            "matchCount": len(matches),
            "matches": matches,
            "manifestUfsDiagnosticMatchCount": len(diagnostics),
            "manifestUfsDiagnosticMatches": diagnostics,
        }
    return results


def dependency_descriptor_identity_issues(
    authoritative_identities: Iterable[tuple[str, str | None, str]],
) -> list[dict[str, Any]]:
    """Reject every companion beneath a reviewed metadata-only plugin root."""
    problems: list[dict[str, Any]] = []
    reviewed = [descriptor for _, descriptor in REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS]
    for scope, container, identity in authoritative_identities:
        folded = identity.casefold()
        for descriptor in reviewed:
            root = descriptor.rsplit("/", 1)[0] + "/"
            if folded.startswith(root.casefold()) and identity != descriptor:
                problems.append({
                    "code": "METADATA_ONLY_DEPENDENCY_PLUGIN_COMPANION_PRESENT",
                    "scope": scope,
                    "container": container,
                    "identity": identity,
                    "allowedDescriptor": descriptor,
                })
                break
    return problems


def audit(
    archive: Path,
    run_id: str,
    unrealpak: Path,
    verification_path: Path,
    binary_receipt_path: Path,
) -> tuple[dict[str, Any], bool]:
    content_policy = load_json(CONTENT_POLICY)
    provenance_policy = load_json(PROVENANCE_POLICY)
    binary_policy = load_json(BINARY_POLICY)
    plugin_policy = load_json(PLUGIN_CAPABILITY_POLICY)
    serialized_receipt = load_json(SERIALIZED_RECEIPT)
    verification = load_json(verification_path)
    binary_receipt = load_json(binary_receipt_path)

    issues: list[dict[str, Any]] = []
    try:
        pak_rules_text = PAK_RULES.read_text(encoding="utf-8-sig")
        packaging_config_text = PACKAGING_CONFIG.read_text(encoding="utf-8-sig")
        project_descriptor_text = PROJECT_DESCRIPTOR.read_text(encoding="utf-8-sig")
        shipping_target_text = SHIPPING_GAME_TARGET.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as exc:
        pak_rules_text = ""
        packaging_config_text = ""
        project_descriptor_text = ""
        shipping_target_text = ""
        issues.append({
            "code": "PACKAGE_HARDENING_CONFIG_INVALID",
            "detail": f"package-hardening authority cannot be read: {type(exc).__name__}",
        })
    else:
        for detail in validate_package_hardening_config(
            content_policy, provenance_policy, plugin_policy,
            pak_rules_text, packaging_config_text,
            project_descriptor_text, shipping_target_text,
        ):
            issues.append({"code": "PACKAGE_HARDENING_CONFIG_INVALID", "detail": detail})
    if verification.get("runId") != run_id or verification.get("state") != (
        "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
    ):
        issues.append({"code": "CORRECTED_BUILD_VERIFICATION_INVALID"})
    if not binary_receipt.get("passed") or binary_receipt.get("state") != "PASS_BINARY_MARKER_POLICY_ONLY":
        issues.append({"code": "SHIPPING_BINARY_POLICY_NOT_PASSING"})

    archive_files = collect_archive(archive)
    archive_by_path = {item["path"].casefold(): item for item in archive_files}
    ufs_path = archive / provenance_policy["manifestFiles"]["ufs"]
    nonufs_path = archive / provenance_policy["manifestFiles"]["nonUfs"]
    ufs = parse_manifest(ufs_path)
    nonufs = parse_manifest(nonufs_path)

    containers: list[dict[str, Any]] = []
    container_named: list[tuple[str, str]] = []
    for file_record in archive_files:
        suffix = PurePosixPath(file_record["path"]).suffix.casefold()
        if suffix not in (".pak", ".utoc"):
            continue
        physical_path = archive.joinpath(*PurePosixPath(file_record["path"]).parts)
        if suffix == ".pak":
            names, summary = inventory_pak(unrealpak, physical_path)
        else:
            companion = physical_path.with_suffix(".ucas")
            if not companion.is_file():
                issues.append({
                    "code": "IOSTORE_COMPANION_MISSING",
                    "identity": file_record["path"],
                })
            names, summary = inventory_utoc(unrealpak, physical_path)
        containers.append({
            "path": file_record["path"],
            "bytes": file_record["bytes"],
            "sha256": file_record["sha256"],
            **summary,
        })
        container_named.extend((file_record["path"], name) for name in names)

    authoritative_identities: list[tuple[str, str | None, str]] = []
    authoritative_identities.extend(
        ("ARCHIVE_FILE", None, item["path"]) for item in archive_files
    )
    authoritative_identities.extend(("NONUFS_ENTRY", None, item) for item in nonufs)
    authoritative_identities.extend(
        ("CONTAINER_NAMED_ENTRY", container, item)
        for container, item in container_named
    )
    identities = list(authoritative_identities)
    identities.extend(("UFS_ENTRY_DIAGNOSTIC", None, item) for item in ufs)

    categories = token_categories(content_policy, provenance_policy, binary_policy)
    category_results = classify_forbidden_identities(
        categories,
        authoritative_identities,
        ufs,
        content_policy["authoritativeIdentityPolicy"][
            "requiredDependencyClosureDescriptorExceptions"
        ],
    )
    for category, result in category_results.items():
        if result["matches"]:
            issues.append({
                "code": f"FORBIDDEN_{category}_IDENTITY",
                "matchCount": result["matchCount"],
            })
    issues.extend(dependency_descriptor_identity_issues(authoritative_identities))

    final_named_exact = {identity for _, identity in container_named}
    final_named_folded = {identity.casefold(): identity for _, identity in container_named}
    dependency_closure_descriptors: list[dict[str, Any]] = []
    for required in REQUIRED_DEPENDENCY_CLOSURE_FINAL_PAYLOAD_DESCRIPTORS:
        observed = final_named_folded.get(required.casefold())
        present_exact = required in final_named_exact
        dependency_closure_descriptors.append({
            "identity": required,
            "presentExact": present_exact,
            "observedIdentity": observed,
        })
        if not present_exact:
            issues.append({
                "code": "REQUIRED_DEPENDENCY_CLOSURE_DESCRIPTOR_MISSING",
                "identity": required,
                "observedCaseVariant": observed,
            })

    required_data: list[dict[str, Any]] = []
    nonufs_set = {item.casefold() for item in nonufs}
    course_id: str | None = None
    hole_numbers: list[int] = []
    for required in content_policy["requiredLooseDataFiles"]:
        item = archive_by_path.get(required.casefold())
        present_nonufs = required.casefold() in nonufs_set
        record: dict[str, Any] = {
            "path": required,
            "presentInArchive": item is not None,
            "presentInNonUfsManifest": present_nonufs,
        }
        if item is not None:
            record.update({"bytes": item["bytes"], "sha256": item["sha256"]})
            document = load_json(archive.joinpath(*PurePosixPath(required).parts))
            record["schema"] = document.get("schema")
            record["schemaVersion"] = document.get("schemaVersion")
            record["courseId"] = document.get("courseId")
            if required.endswith("PineRidgeCourse.json"):
                course_id = document.get("courseId")
                record["holeNumbers"] = [hole.get("holeNumber") for hole in document.get("holes", [])]
            elif "PineRidgeHole" in required:
                hole_numbers.append(document.get("holeNumber"))
                record["holeNumber"] = document.get("holeNumber")
            elif required.endswith("PineRidgePresentation.json"):
                record["holeNumbers"] = [hole.get("holeNumber") for hole in document.get("holes", [])]
                record["assetsReady"] = document.get("assetsReady")
        required_data.append(record)
        if item is None or not present_nonufs:
            issues.append({"code": "REQUIRED_THREE_HOLE_DATA_MISSING", "identity": required})
    expected_holes = list(content_policy["target"]["holes"])
    if course_id != "PineRidgeChampionship" or sorted(hole_numbers) != expected_holes:
        issues.append({"code": "THREE_HOLE_DATA_SEMANTICS_INVALID"})

    ufs_set = {item.casefold() for item in ufs}
    container_set = {item.casefold() for _, item in container_named}
    retained_assets: list[dict[str, Any]] = []
    for asset in serialized_receipt["retainedAssets"]:
        staged = canonical_relative("DiscGolfTour/" + asset["path"])
        in_ufs = staged.casefold() in ufs_set
        in_container = staged.casefold() in container_set
        retained_assets.append({
            "package": asset["package"],
            "stagedIdentity": staged,
            "presentInUfsManifest": in_ufs,
            "presentInContainerInventory": in_container,
        })
        if not in_ufs or not in_container:
            issues.append({"code": "RETAINED_RUNTIME_ASSET_MISSING", "identity": staged})

    inner = verification.get("innerShippingExecutable", {})
    inner_path = inner.get("relativePath", "")
    inner_file = archive_by_path.get(str(inner_path).casefold())
    binary_sha = str(binary_receipt.get("input", {}).get("sha256", "")).upper()
    if (
        inner_file is None
        or inner_file["sha256"] != str(inner.get("sha256", "")).upper()
        or inner_file["sha256"] != binary_sha
    ):
        issues.append({"code": "BOUND_SHIPPING_EXECUTABLE_HASH_MISMATCH"})

    inventory_lines = sorted(
        f"{scope}\t{container or ''}\t{identity}\n"
        for scope, container, identity in identities
    )
    inventory_hash = hashlib.sha256("".join(inventory_lines).encode("utf-8")).hexdigest().upper()
    authoritative_inventory_hash = hashlib.sha256(
        "".join(sorted(
            f"{scope}\t{container or ''}\t{identity}\n"
            for scope, container, identity in authoritative_identities
        )).encode("utf-8")
    ).hexdigest().upper()
    archive_manifest_hash = hashlib.sha256(
        "".join(
            f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n"
            for item in archive_files
        ).encode("utf-8")
    ).hexdigest().upper()

    passed = not issues
    receipt = {
        "schema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
        "schemaVersion": 1,
        "session": 19,
        "runId": run_id,
        "generatedUtc": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "state": "PASS_BOUNDED_STAGED_CONTENT_AUDIT" if passed else "FAIL_CLOSED",
        "bindings": {
            "shippingContentPolicy": evidence_binding(CONTENT_POLICY),
            "stagedProvenancePolicy": evidence_binding(PROVENANCE_POLICY),
            "shippingBinaryPolicy": evidence_binding(BINARY_POLICY),
            "windowsShippingPakRules": evidence_binding(PAK_RULES),
            "windowsShippingPackagingConfig": evidence_binding(PACKAGING_CONFIG),
            "projectDescriptor": evidence_binding(PROJECT_DESCRIPTOR),
            "shippingGameTarget": evidence_binding(SHIPPING_GAME_TARGET),
            "correctedCandidateVerification": evidence_binding(verification_path),
            "shippingBinaryReceipt": evidence_binding(binary_receipt_path),
            "serializedAssetMigrationReceipt": evidence_binding(SERIALIZED_RECEIPT),
        },
        "archive": {
            "hostPathRecorded": False,
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{run_id}/Windows",
            "fileCount": len(archive_files),
            "bytes": sum(item["bytes"] for item in archive_files),
            "canonicalManifestSha256": archive_manifest_hash,
        },
        "inventory": {
            "ufsEntryCount": len(ufs),
            "nonUfsEntryCount": len(nonufs),
            "containerCount": len(containers),
            "containerNamedEntryCount": len(container_named),
            "auditedIdentityCount": len(identities),
            "canonicalIdentitySha256": inventory_hash,
            "authoritativeIdentityCount": len(authoritative_identities),
            "authoritativeCanonicalIdentitySha256": authoritative_inventory_hash,
            "ufsManifestDiagnosticEntryCount": len(ufs),
            "authoritativeScopePolicy": EXPECTED_AUTHORITATIVE_IDENTITY_POLICY,
            "containers": containers,
        },
        "forbiddenIdentityCategories": category_results,
        "dependencyClosureFinalPayloadDescriptors": dependency_closure_descriptors,
        "requiredThreeHoleData": {
            "expectedHoleNumbers": expected_holes,
            "files": required_data,
        },
        "requiredRetainedRuntimeAssets": retained_assets,
        "issues": issues,
        "releaseBoundary": {
            "boundedStagedContentAuditPassed": passed,
            "freshInstallGameplayAcceptancePerformed": False,
            "provenanceClassificationPerformed": False,
            "legalOrVisualApprovalPerformed": False,
            "blockerClosed": False,
            "releaseReady": False,
        },
    }
    return receipt, passed


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(value, indent=2) + "\n").encode("utf-8")
    try:
        with path.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise FileExistsError(
            f"evidence is immutable and already exists: {path}"
        ) from exc


def self_test() -> int:
    failures: list[str] = []
    count = 0
    cases = {
        "DiscGolfTour/Content/DiscGolf/Tests/A.uasset": "/Tests/",
        "Engine/Content/Slate/Automation/Fail.png": "/Slate/Automation/",
        "DiscGolfTour/Content/DiscGolf/Rigs/CR_DG_Master.uasset": "ThrowLab",
    }
    for identity, token in cases.items():
        count += 1
        expected = token != "ThrowLab"
        if (token.casefold() in normalized_for_token_scan(identity).casefold()) != expected:
            failures.append(f"literal marker case differed: {identity}")
    invalid = ["../escape", "C:/host", "dot/../escape", ""]
    for value in invalid:
        count += 1
        try:
            canonical_relative(value)
        except ValueError:
            pass
        else:
            failures.append(f"invalid identity survived: {value!r}")
    try:
        count += 1
        json.loads('{"a":1,"a":2}', object_pairs_hook=reject_duplicate_keys)
    except DuplicateKeyError:
        pass
    else:
        failures.append("duplicate JSON key survived")

    content_policy = load_json(CONTENT_POLICY)
    provenance_policy = load_json(PROVENANCE_POLICY)
    binary_policy = load_json(BINARY_POLICY)
    plugin_policy = load_json(PLUGIN_CAPABILITY_POLICY)
    pak_rules_text = PAK_RULES.read_text(encoding="utf-8-sig")
    packaging_config_text = PACKAGING_CONFIG.read_text(encoding="utf-8-sig")
    project_descriptor_text = PROJECT_DESCRIPTOR.read_text(encoding="utf-8-sig")
    shipping_target_text = SHIPPING_GAME_TARGET.read_text(encoding="utf-8-sig")
    count += 1
    checked_in_hardening_problems = validate_package_hardening_config(
        content_policy, provenance_policy, plugin_policy,
        pak_rules_text, packaging_config_text,
        project_descriptor_text, shipping_target_text,
    )
    if checked_in_hardening_problems:
        failures.extend(
            f"checked-in package-hardening policy is invalid: {problem}"
            for problem in checked_in_hardening_problems
        )
    development_tokens = provenance_policy["forbiddenDevelopmentPathTokens"]
    for root in REQUIRED_ENGINE_DEVELOPMENT_ROOTS:
        count += 1
        if not matches_any_token(root.lstrip("/") + "Sentinel.uasset", development_tokens):
            failures.append(f"required engine development root was not rejected: {root}")
    for identity in PROTECTED_RUNTIME_SLATE_IDENTITIES:
        count += 1
        if matches_any_token(identity, development_tokens):
            failures.append(f"required runtime Slate identity was rejected: {identity}")

    hardening_mutations: list[
        tuple[
            str, dict[str, Any], dict[str, Any], dict[str, Any],
            str, str, str, str,
        ]
    ] = []

    def add_mutation(
        name: str,
        *,
        content: dict[str, Any] = content_policy,
        provenance: dict[str, Any] = provenance_policy,
        plugin: dict[str, Any] = plugin_policy,
        rules: str = pak_rules_text,
        packaging: str = packaging_config_text,
        project: str = project_descriptor_text,
        target: str = shipping_target_text,
    ) -> None:
        hardening_mutations.append(
            (name, content, provenance, plugin, rules, packaging, project, target)
        )

    missing_root_policy = json.loads(json.dumps(provenance_policy))
    missing_root_policy["forbiddenDevelopmentPathTokens"].remove(
        REQUIRED_ENGINE_DEVELOPMENT_ROOTS[0]
    )
    add_mutation("missing development classification", provenance=missing_root_policy)
    broad_slate_policy = json.loads(json.dumps(provenance_policy))
    broad_slate_policy["forbiddenDevelopmentPathTokens"].append("/Engine/Content/Slate/")
    add_mutation("overbroad Slate classification", provenance=broad_slate_policy)
    add_mutation(
        "missing content Pak exclusion",
        rules=pak_rules_text.replace(
            f'+Files="{REQUIRED_ENGINE_CONTENT_PAK_EXCLUSIONS[0]}"\n', "", 1
        ),
    )
    add_mutation(
        "overbroad Slate Pak exclusion",
        rules=pak_rules_text + '\n+Files=".../Engine/Content/Slate/*"\n',
    )
    first_plugin_pattern = f'.../{plugin_policy["reviewedDisabledPlugins"][0]["descriptor"]}'
    add_mutation(
        "missing exact plugin descriptor exclusion",
        rules=pak_rules_text.replace(f'+Files="{first_plugin_pattern}"\n', "", 1),
    )
    add_mutation(
        "broad plugin Pak exclusion",
        rules=pak_rules_text + '\n+Files=".../Engine/Plugins/*"\n',
    )
    add_mutation(
        "missing skip-editor-content",
        packaging=packaging_config_text.replace("bSkipEditorContent=True\n", "", 1),
    )
    for root in REQUIRED_NEVER_COOK_VIRTUAL_ROOTS:
        add_mutation(
            f"missing NeverCook root {root}",
            packaging=packaging_config_text.replace(
                f'+DirectoriesToNeverCook=(Path="{root}")\n', "", 1
            ),
        )
    for residual in REQUIRED_RESIDUAL_PAK_EXCLUSIONS:
        add_mutation(
            f"missing residual config exclusion {residual}",
            rules=pak_rules_text.replace(f'+Files="{residual}"\n', "", 1),
        )
    absent_descriptor_reclassified = json.loads(json.dumps(plugin_policy))
    absent_descriptor_reclassified["requiredAbsentFinalPayloadDescriptors"].remove(
        REQUIRED_ABSENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS[0]
    )
    absent_descriptor_reclassified["requiredPreservedFinalPayloadDescriptors"].append(
        REQUIRED_ABSENT_FINAL_PAYLOAD_PLUGIN_DESCRIPTORS[0]
    )
    add_mutation(
        "Shipping-denied plugin descriptor reclassified as required-present",
        plugin=absent_descriptor_reclassified,
    )
    missing_closure_descriptor = json.loads(json.dumps(plugin_policy))
    missing_closure_descriptor["requiredDependencyClosureFinalPayloadDescriptors"].pop()
    add_mutation(
        "required dependency-closure descriptor removed",
        plugin=missing_closure_descriptor,
    )
    missing_metadata_review = json.loads(json.dumps(plugin_policy))
    missing_metadata_review["reviewedMetadataOnlyDependencyDescriptors"].pop()
    add_mutation(
        "metadata-only dependency review removed",
        plugin=missing_metadata_review,
    )
    missing_plugin_residual = json.loads(json.dumps(plugin_policy))
    missing_plugin_residual["reviewedResidualMetadataExclusions"].pop()
    add_mutation(
        "plugin residual metadata authority removed",
        plugin=missing_plugin_residual,
    )

    denied_project = load_json_text(project_descriptor_text)
    for plugin in denied_project["Plugins"]:
        if plugin.get("Name") == "ConsoleVariables":
            plugin["TargetConfigurationDenyList"] = ["Development"]
    add_mutation(
        "ConsoleVariables not denied in Shipping",
        project=json.dumps(denied_project),
    )
    landmass_project = load_json_text(project_descriptor_text)
    for plugin in landmass_project["Plugins"]:
        if plugin.get("Name") == "Landmass":
            plugin["TargetConfigurationDenyList"] = ["Shipping"]
    add_mutation(
        "Landmass dependency descriptor denied in Shipping",
        project=json.dumps(landmass_project),
    )
    editor_only_project = load_json_text(project_descriptor_text)
    for plugin in editor_only_project["Plugins"]:
        if plugin.get("Name") == "EditorScriptingUtilities":
            plugin["TargetConfigurationDenyList"] = ["Shipping"]
    add_mutation(
        "EditorScriptingUtilities dependency descriptor denied in Shipping",
        project=json.dumps(editor_only_project),
    )
    add_mutation(
        "missing dependency-ignore identity",
        target=shipping_target_text.replace('        "ConcertMain",\n', "", 1),
    )
    add_mutation(
        "Landmass incorrectly added to dependency-ignore set",
        target=shipping_target_text.replace(
            '        "AssetManagerEditor",\n',
            '        "AssetManagerEditor",\n        "Landmass",\n',
            1,
        ),
    )
    add_mutation(
        "dependency-ignore not Shipping-only",
        target=shipping_target_text.replace(
            "Configuration == UnrealTargetConfiguration.Shipping",
            "Configuration == UnrealTargetConfiguration.Development",
            1,
        ),
    )
    missing_bound_root = json.loads(json.dumps(content_policy))
    missing_bound_root["neverCookRoots"].remove(REQUIRED_NEVER_COOK_VIRTUAL_ROOTS[0])
    add_mutation("policy omits NeverCook root", content=missing_bound_root)
    missing_forbidden_plugin_root = json.loads(json.dumps(content_policy))
    missing_forbidden_plugin_root["forbiddenShippingPathTokens"].remove(
        REQUIRED_SHIPPING_FORBIDDEN_PLUGIN_SUBTREES[0]
    )
    add_mutation(
        "policy omits Landmass final-payload forbidden subtree",
        content=missing_forbidden_plugin_root,
    )
    missing_dependency_exception = json.loads(json.dumps(content_policy))
    missing_dependency_exception["authoritativeIdentityPolicy"][
        "requiredDependencyClosureDescriptorExceptions"
    ].pop()
    add_mutation(
        "dependency-closure descriptor exception removed",
        content=missing_dependency_exception,
    )
    broad_exception = json.loads(json.dumps(content_policy))
    broad_exception["authoritativeIdentityPolicy"]["forbiddenIdentityExceptions"] = [
        "Engine/Plugins/Editor/*"
    ]
    add_mutation("forbidden identity allowlist added", content=broad_exception)
    stale_binding = json.loads(json.dumps(content_policy))
    stale_binding["packageHardening"]["shippingGameTarget"]["sha256"] = "0" * 64
    add_mutation("stale Shipping target binding", content=stale_binding)

    for (
        mutation_name,
        content_candidate,
        policy_candidate,
        plugin_candidate,
        rules_candidate,
        packaging_candidate,
        project_candidate,
        target_candidate,
    ) in hardening_mutations:
        count += 1
        if not validate_package_hardening_config(
            content_candidate, policy_candidate, plugin_candidate,
            rules_candidate, packaging_candidate,
            project_candidate, target_candidate,
        ):
            failures.append(f"package-hardening mutation survived: {mutation_name}")

    categories = token_categories(content_policy, provenance_policy, binary_policy)
    ufs_only = classify_forbidden_identities(
        categories, [], ["Engine/Content/VREditor/Tool.uasset"]
    )["DEVELOPMENT_TEST_EDITOR"]
    count += 1
    if not ufs_only["passed"] or ufs_only["matchCount"] != 0 or (
        ufs_only["manifestUfsDiagnosticMatchCount"] == 0
    ):
        failures.append("UFS-only development row was not diagnostic-only")

    authoritative = classify_forbidden_identities(
        categories,
        [("CONTAINER_NAMED_ENTRY", "Fixture.pak", "Engine/Content/VREditor/Tool.uasset")],
        [],
    )["DEVELOPMENT_TEST_EDITOR"]
    count += 1
    if authoritative["passed"] or authoritative["matchCount"] == 0:
        failures.append("authoritative development identity was not rejected")

    landmass_authoritative = classify_forbidden_identities(
        categories,
        [(
            "CONTAINER_NAMED_ENTRY",
            "Fixture.pak",
            "Engine/Plugins/Experimental/Landmass/Landmass.uplugin",
        )],
        [],
    )["ALL_FROZEN_PATH_TOKENS"]
    count += 1
    if not landmass_authoritative["passed"] or landmass_authoritative["matchCount"] != 0:
        failures.append("required Landmass descriptor was rejected")
    landmass_content = classify_forbidden_identities(
        categories,
        [(
            "CONTAINER_NAMED_ENTRY",
            "Fixture.pak",
            "Engine/Plugins/Experimental/Landmass/Content/Brush.uasset",
        )],
        [],
    )["ALL_FROZEN_PATH_TOKENS"]
    count += 1
    if landmass_content["passed"] or landmass_content["matchCount"] == 0:
        failures.append("authoritative Landmass content was not rejected")

    dependency_exceptions = content_policy["authoritativeIdentityPolicy"][
        "requiredDependencyClosureDescriptorExceptions"
    ]
    metadata_descriptor = REVIEWED_METADATA_ONLY_DEPENDENCY_DESCRIPTORS[3][1]
    metadata_allowed = classify_forbidden_identities(
        categories,
        [("CONTAINER_NAMED_ENTRY", "Fixture.pak", metadata_descriptor)],
        [],
        dependency_exceptions,
    )["DEVELOPMENT_TEST_EDITOR"]
    count += 1
    if not metadata_allowed["passed"] or metadata_allowed["matchCount"] != 0:
        failures.append("exact metadata-only dependency descriptor was rejected")
    metadata_case_variant = classify_forbidden_identities(
        categories,
        [("CONTAINER_NAMED_ENTRY", "Fixture.pak", metadata_descriptor.lower())],
        [],
        dependency_exceptions,
    )["DEVELOPMENT_TEST_EDITOR"]
    count += 1
    if metadata_case_variant["passed"] or metadata_case_variant["matchCount"] == 0:
        failures.append("case-variant metadata-only descriptor received an exception")
    metadata_companion = metadata_descriptor.rsplit("/", 1)[0] + "/Content/Leak.uasset"
    count += 1
    if not dependency_descriptor_identity_issues([
        ("CONTAINER_NAMED_ENTRY", "Fixture.pak", metadata_companion)
    ]):
        failures.append("metadata-only dependency plugin companion was not rejected")
    count += 1
    if dependency_descriptor_identity_issues([
        ("CONTAINER_NAMED_ENTRY", "Fixture.pak", metadata_descriptor)
    ]):
        failures.append("exact metadata-only dependency descriptor was treated as a companion")

    for mixed_runtime_identity in (
        "Engine/Plugins/Editor/ConsoleVariablesEditor/ConsoleVariables.uplugin",
        "Engine/Plugins/Editor/FacialAnimation/FacialAnimation.uplugin",
    ):
        result = classify_forbidden_identities(
            categories,
            [("CONTAINER_NAMED_ENTRY", "Fixture.pak", mixed_runtime_identity)],
            [],
        )["DEVELOPMENT_TEST_EDITOR"]
        count += 1
        if result["passed"] or result["matchCount"] == 0:
            failures.append(
                f"Shipping-disabled mixed runtime plugin received an allowlist: {mixed_runtime_identity}"
            )

    with tempfile.TemporaryDirectory(prefix="dgt-s19-manifest-selftest-") as temp_name:
        manifest = Path(temp_name) / "Manifest_UFSFiles_Win64.txt"
        valid_row = "Engine/Content/VREditor/Tool.uasset\t2026-08-25T00:00:00.000Z\n"
        manifest.write_text(valid_row, encoding="utf-8")
        count += 1
        if parse_manifest(manifest) != ["Engine/Content/VREditor/Tool.uasset"]:
            failures.append("valid UFS diagnostic manifest row did not parse")
        for invalid_payload in (valid_row + valid_row, "malformed-row\n"):
            manifest.write_text(invalid_payload, encoding="utf-8")
            count += 1
            try:
                parse_manifest(manifest)
            except ValueError:
                pass
            else:
                failures.append("invalid UFS diagnostic manifest survived parsing")
    with tempfile.TemporaryDirectory(prefix="dgt-s19-content-output-") as temp_name:
        collision = Path(temp_name) / "receipt.json"
        collision.write_bytes(b"sentinel")
        count += 1
        try:
            write_json(collision, {"state": "must-not-overwrite"})
        except FileExistsError:
            if collision.read_bytes() != b"sentinel":
                failures.append("exclusive output collision changed existing bytes")
        else:
            failures.append("exclusive output collision was accepted")
    if failures:
        print("SESSION 19 CANDIDATE CONTENT SELF-TEST FAILED")
        for failure in failures:
            print(" -", failure)
        return 1
    print(f"SESSION 19 CANDIDATE CONTENT SELF-TEST PASS: {count}/{count}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--run-id")
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--verification", type=Path)
    parser.add_argument("--binary-receipt", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    required = [args.archive, args.run_id, args.output]
    if any(value is None for value in required):
        parser.error("candidate audit requires --archive, --run-id, and --output")
    if not RUN_ID_RE.fullmatch(args.run_id):
        parser.error("--run-id is not a sanitized Session 19 Shipping identifier")
    verification = args.verification or (
        ROOT / f"Evidence/Session19/ShippingCandidateVerification-{args.run_id}.json"
    )
    binary_receipt = args.binary_receipt or (
        ROOT / f"Evidence/Session19/ShippingBinary-{args.run_id}.json"
    )
    receipt, passed = audit(
        args.archive.resolve(),
        args.run_id,
        find_unrealpak(args.unrealpak),
        verification.resolve(),
        binary_receipt.resolve(),
    )
    write_json(args.output.resolve(), receipt)
    print(
        f"SESSION 19 CANDIDATE CONTENT {receipt['state']}: "
        f"identities={receipt['inventory']['auditedIdentityCount']} "
        f"forbidden={sum(value['matchCount'] for value in receipt['forbiddenIdentityCategories'].values())} "
        f"issues={len(receipt['issues'])}"
    )
    return 0 if passed else 2


if __name__ == "__main__":
    sys.exit(main())
