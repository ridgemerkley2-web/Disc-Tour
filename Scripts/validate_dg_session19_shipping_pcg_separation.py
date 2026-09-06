#!/usr/bin/env python3
"""Validate the v0.5 Shipping/editor PCG compile and cook separation.

Shipping consumes the exact authored JSON tree arrays. PCG remains available
only to the Editor target and its source graph is explicitly never cooked.
This is technical evidence, not visual, performance, legal, or release approval.
"""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = "Config/DG_Session19ShippingPcgSeparationPolicy.json"
SOURCE_STATE = "PASS_SOURCE_CONFIG_SHIPPING_PCG_SEPARATION_COOK_PROOF_PENDING"
COOK_STATE = "PASS_FRESH_SHIPPING_COOK_PCG_ABSENCE_HUMAN_GATES_PENDING"
RELEASE_STATE = "BLOCKED_SESSION13_HUMAN_VISUAL_PERFORMANCE_AND_RELEASE_GATES"
GRAPH_PACKAGE = "/Game/Environment/Forest/PCG/PCG_TemperateMountainForest"
EXPECTED_TREE_PAYLOAD = {
    "treeCount": 44,
    "bytes": 2982,
    "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
}
EXPECTED_CLAIMS = {
    "sourceConfigSeparationProven": True,
    "freshShippingCookAbsenceProven": False,
    "unrealPcgBakeSaveReopenProven": False,
    "collisionVisualApproval": False,
    "performanceAcceptance": False,
    "soakAcceptance": False,
    "manualGameplayAcceptance": False,
    "humanPlayFeelApproval": False,
    "provenanceApproval": False,
    "legalApproval": False,
    "distributionClearance": False,
    "session13BlockerClosed": False,
    "releaseReady": False,
}
REQUIRED_SOURCE_PROOF = [
    "RUNTIME_MODULE_HAS_NO_PCG_DEPENDENCY",
    "RUNTIME_SOURCE_HAS_NO_PCG_GENERATION_TYPE_OR_API",
    "PCG_PLUGIN_IS_EDITOR_TARGET_ONLY",
    "FOREST_GRAPH_PROPERTY_IS_EDITOR_ONLY_AND_GENERIC",
    "EDITOR_MODULE_OWNS_GRAPH_CONTROLLER_AND_CUSTOM_NODE",
    "SHIPPING_COOK_CONFIGURATION_EXCLUDES_PCG_GRAPH_DIRECTORY",
    "AUTHORED_JSON_44_TREE_AUTHORITY_UNCHANGED",
]
FRESH_COOK_PROOF = [
    "FINAL_WINDOWS_ARCHIVE_IDENTITY",
    "IOSTORE_HAS_NO_PROJECT_PCG_GRAPH_ENTRY",
    "COOKED_ASSET_REGISTRY_HAS_NO_PROJECT_PCG_GRAPH_OBJECT",
    "COOKED_PROJECT_ASSETS_HAVE_NO_PCG_COMPONENT_GRAPH_OR_GENERATION_MARKERS",
    "SHIPPING_BINARY_AND_MODULE_INVENTORY_HAVE_NO_PROJECT_PCG_RUNTIME_DEPENDENCY",
]


class ValidationError(ValueError):
    pass


def strict_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValidationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_bytes(data: bytes, context: str) -> Any:
    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=strict_pairs,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValidationError(f"non-finite JSON value in {context}: {value}")
            ),
        )
    except (UnicodeError, json.JSONDecodeError, ValidationError) as exc:
        raise ValidationError(f"invalid JSON in {context}: {exc}") from exc


def load_json(path: Path) -> Any:
    try:
        return load_json_bytes(path.read_bytes(), str(path))
    except OSError as exc:
        raise ValidationError(f"unreadable file {path}: {exc}") from exc


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def identity(path: Path) -> dict[str, Any]:
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(block)
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ValidationError(f"file changed while hashing: {path}")
    return {"bytes": after.st_size, "sha256": digest.hexdigest().upper()}


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def policy_errors(policy: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(policy, dict):
        return ["policy root must be an object"]
    expected = {
        "schema": "DiscGolfTour.Session19ShippingPcgSeparationPolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "v05_shipping_authored_json_editor_only_pcg_v1",
        "authority": "ADDITIVE_SOURCE_COOK_TECHNICAL_POLICY_NOT_RELEASE_APPROVAL",
        "state": "SOURCE_CONFIG_SEPARATION_PASS_FRESH_SHIPPING_COOK_PROOF_PENDING",
        "strategy": "SHIP_EXACT_AUTHORED_JSON_44_TREE_RUNTIME_OUTPUT_AND_EXCLUDE_ALL_PCG_AUTHORING_CODE_AND_GRAPH_CONTENT",
        "session13BlockerClosed": False,
        "releaseReady": False,
    }
    for key, value in expected.items():
        if policy.get(key) != value:
            errors.append(f"policy.{key} differs")
    if policy.get("target") != {
        "platform": "Windows",
        "configuration": "Shipping",
        "milestone": "v0.5",
        "courseId": "PineRidgeChampionship",
        "layoutId": "Championship",
        "holeNumbers": [1, 2, 3],
    }:
        errors.append("policy.target differs")
    if policy.get("runtimeAuthority") != {
        "source": "ORDERED_EXPLICIT_TREES_ARRAYS_IN_AUTHORED_HOLE_JSON",
        "treeCount": 44,
        "canonicalPayloadBytes": 2982,
        "canonicalPayloadSha256": EXPECTED_TREE_PAYLOAD["sha256"],
        "shippingSourceFallbackAllowed": False,
        "runtimeRandomPlacementAllowed": False,
    }:
        errors.append("policy.runtimeAuthority differs")
    compile_boundary = policy.get("compileBoundary", {})
    if compile_boundary != {
        "runtimeModuleRules": "Source/DiscGolfTour/DiscGolfTour.Build.cs",
        "runtimeForbiddenModuleDependencies": ["PCG"],
        "runtimeControllerHeader": "Source/DiscGolfTour/DiscGolfEnvironmentController.h",
        "runtimeControllerSource": "Source/DiscGolfTour/DiscGolfEnvironmentController.cpp",
        "runtimeForbiddenSymbols": ["UPCG", "PCGComponent", "GenerateForest", "GenerateLocal", "SetGraph"],
        "editorModuleRules": "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs",
        "editorRequiredModuleDependency": "PCG",
        "editorAuthoringController": "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp",
        "editorNode": "Source/DiscGolfTourEditor/PCGDiscGolfZoneDensity.cpp",
        "pluginTargetAllowList": ["Editor"],
    }:
        errors.append("policy.compileBoundary differs")
    asset = policy.get("assetBoundary", {})
    if asset != {
        "presetHeader": "Source/DiscGolfTour/DiscGolfEnvironmentDataAssets.h",
        "forestGraphProperty": "ForestGraph",
        "forestGraphPropertyEditorOnly": True,
        "forestGraphPropertyGenericSoftObject": True,
        "graphDiskPath": "Content/Environment/Forest/PCG/PCG_TemperateMountainForest.uasset",
        "graphPackagePath": GRAPH_PACKAGE,
        "graphBytes": 705957,
        "graphSha256": "86DF264918903F0F18832D6B078F4AFE032B3DFC31D64134F2B2CD2F3B17D159",
        "shippingNeverCookDirectory": "/Game/Environment/Forest/PCG",
        "legacyClassPath": "/Script/DiscGolfTour.PCGDiscGolfZoneDensitySettings",
        "editorClassPath": "/Script/DiscGolfTourEditor.PCGDiscGolfZoneDensitySettings",
    }:
        errors.append("policy.assetBoundary differs")
    if policy.get("claimBoundary") != EXPECTED_CLAIMS:
        errors.append("policy.claimBoundary differs")
    if policy.get("evidence") != {
        "validator": "Scripts/validate_dg_session19_shipping_pcg_separation.py",
        "selfTestMinimumMutationCount": 28,
    }:
        errors.append("policy.evidence differs")
    if policy.get("requiredSourceProof") != REQUIRED_SOURCE_PROOF:
        errors.append("policy.requiredSourceProof differs")
    if policy.get("freshCookProofRequired") != FRESH_COOK_PROOF:
        errors.append("policy.freshCookProofRequired differs")
    return errors


def source_text(relative: str, overrides: dict[str, str]) -> str:
    if relative in overrides:
        return overrides[relative]
    try:
        return (ROOT / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        raise ValidationError(f"unreadable source {relative}: {exc}") from exc


def authored_tree_payload() -> dict[str, Any]:
    holes: list[dict[str, Any]] = []
    count = 0
    for number in (1, 2, 3):
        value = load_json(ROOT / f"Data/PineRidgeHole{number}.json")
        trees = value.get("trees") if isinstance(value, dict) else None
        if not isinstance(trees, list):
            raise ValidationError(f"Hole {number} trees are not an explicit array")
        count += len(trees)
        holes.append({
            "courseId": value.get("courseId"),
            "layoutId": value.get("layoutId"),
            "holeNumber": value.get("holeNumber"),
            "trees": trees,
        })
    raw = canonical_bytes({"holes": holes})
    result = {"treeCount": count, "bytes": len(raw), "sha256": sha256(raw)}
    if result != EXPECTED_TREE_PAYLOAD:
        raise ValidationError("authored 44-tree authority identity differs")
    return result


def source_audit(policy: dict[str, Any], overrides: dict[str, str] | None = None) -> dict[str, Any]:
    overrides = overrides or {}
    errors = policy_errors(policy)
    runtime_rules = source_text("Source/DiscGolfTour/DiscGolfTour.Build.cs", overrides)
    editor_rules = source_text("Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs", overrides)
    if re.search(r'"PCG"', runtime_rules):
        errors.append("runtime module still depends on PCG")
    if len(re.findall(r'"PCG"', editor_rules)) != 1:
        errors.append("editor module must own exactly one PCG dependency")

    forbidden = policy["compileBoundary"]["runtimeForbiddenSymbols"]
    runtime_hits: list[str] = []
    for path in (ROOT / "Source/DiscGolfTour").rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".h", ".cpp", ".cs"}:
            continue
        relative = path.relative_to(ROOT).as_posix()
        if "/Tests/" in f"/{relative}/":
            continue
        text = source_text(relative, overrides)
        for token in forbidden:
            if token in text:
                runtime_hits.append(f"{relative}:{token}")
    if runtime_hits:
        errors.append("runtime PCG symbol(s) remain: " + ", ".join(runtime_hits))

    descriptor = load_json_bytes(
        source_text("DiscGolfTour.uproject", overrides).encode("utf-8"),
        "DiscGolfTour.uproject",
    )
    pcg_entries = [item for item in descriptor.get("Plugins", [])
                   if isinstance(item, dict) and item.get("Name") == "PCG"]
    if pcg_entries != [{"Name": "PCG", "Enabled": True, "TargetAllowList": ["Editor"]}]:
        errors.append("PCG plugin is not restricted to the Editor target")

    game_ini = source_text("Config/DefaultGame.ini", overrides)
    never_line = '+DirectoriesToNeverCook=(Path="/Game/Environment/Forest/PCG")'
    if game_ini.count(never_line) != 1:
        errors.append("PCG graph directory is not exactly once in DirectoriesToNeverCook")
    engine_ini = source_text("Config/DefaultEngine.ini", overrides)
    redirect = '+ClassRedirects=(OldName="/Script/DiscGolfTour.PCGDiscGolfZoneDensitySettings",NewName="/Script/DiscGolfTourEditor.PCGDiscGolfZoneDensitySettings")'
    if engine_ini.count(redirect) != 1:
        errors.append("legacy custom-node class redirect differs")

    preset = source_text("Source/DiscGolfTour/DiscGolfEnvironmentDataAssets.h", overrides)
    editor_property = re.search(
        r"#if WITH_EDITORONLY_DATA(?:(?!#endif).)*TSoftObjectPtr<UObject>\s+ForestGraph;(?:(?!#endif).)*#endif",
        preset,
        re.DOTALL,
    )
    if not editor_property:
        errors.append("ForestGraph is not an editor-only generic soft-object property")

    authoring = source_text("Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp", overrides)
    node = source_text("Source/DiscGolfTourEditor/PCGDiscGolfZoneDensity.cpp", overrides)
    for marker in (
        "CreateDefaultSubobject<UPCGComponent>",
        "EPCGComponentGenerationTrigger::GenerateOnDemand",
        "bGenerateOnDropWhenTriggerOnDemand = false",
        "PCGComponent->SetGraph(Graph)",
        "PCGComponent->GenerateLocal(",
    ):
        if marker not in authoring:
            errors.append(f"editor authoring controller marker missing: {marker}")
    if "UPCGDiscGolfZoneDensitySettings::CreateElement" not in node:
        errors.append("editor custom density node implementation is missing")

    graph = ROOT / policy["assetBoundary"]["graphDiskPath"]
    if not graph.is_file() or identity(graph) != {
        "bytes": policy["assetBoundary"]["graphBytes"],
        "sha256": policy["assetBoundary"]["graphSha256"],
    }:
        errors.append("preserved editor graph identity differs")
    try:
        tree_payload = authored_tree_payload()
    except ValidationError as exc:
        errors.append(str(exc))
        tree_payload = {}
    if errors:
        raise ValidationError("; ".join(errors))
    return {
        "runtimePcgModuleDependencyCount": 0,
        "runtimePcgGenerationSymbolCount": 0,
        "pcgPluginTargets": ["Editor"],
        "forestGraphPropertyEditorOnly": True,
        "shippingNeverCookGraphDirectory": True,
        "editorAuthoringControllerPresent": True,
        "editorCustomNodePresent": True,
        "authoredTreePayload": tree_payload,
    }


def run_tool(arguments: list[str], context: str) -> str:
    completed = subprocess.run(arguments, capture_output=True, check=False)
    output = (completed.stdout + b"\n" + completed.stderr).decode("utf-8", errors="replace")
    if completed.returncode != 0:
        raise ValidationError(f"{context} failed ({completed.returncode}): {output[-2000:]}")
    return output


def file_contains_any(path: Path, markers: list[bytes]) -> bool:
    overlap = max((len(marker) for marker in markers), default=1) - 1
    tail = b""
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            data = tail + block
            if any(marker in data for marker in markers):
                return True
            tail = data[-overlap:] if overlap else b""
    return False


def archive_audit(archive: Path, unrealpak: Path, editor: Path) -> dict[str, Any]:
    archive = archive.resolve(strict=True)
    if not archive.is_dir() or archive.name.casefold() != "windows":
        raise ValidationError("--archive must be a final Windows archive root")
    unrealpak = unrealpak.resolve(strict=True)
    editor = editor.resolve(strict=True)
    paks = archive / "DiscGolfTour/Content/Paks"
    pak = paks / "DiscGolfTour-Windows.pak"
    utoc = paks / "DiscGolfTour-Windows.utoc"
    ucas = paks / "DiscGolfTour-Windows.ucas"
    shipping_binary = archive / "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
    for path in (pak, utoc, ucas, shipping_binary):
        if not path.is_file():
            raise ValidationError(f"candidate container is missing: {path.name}")

    scratch = Path(tempfile.mkdtemp(prefix="dgpcgsep_", dir="C:\\"))
    try:
        inventory_csv = scratch / "container.csv"
        run_tool([str(unrealpak), f"-ListContainer={utoc}", f"-CSV={inventory_csv}"], "IoStore inventory")
        with inventory_csv.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle, skipinitialspace=True))
        required_columns = {"OrderInContainer", "Filename", "ChunkType", "Size", "Hash"}
        if not rows or not required_columns.issubset(rows[0]):
            raise ValidationError("IoStore inventory is empty or missing required columns")
        for expected_order, row in enumerate(rows):
            try:
                observed_order = int(row.get("OrderInContainer", "-1"))
            except ValueError as exc:
                raise ValidationError("IoStore inventory order is not numeric") from exc
            if observed_order != expected_order:
                raise ValidationError("IoStore inventory order is not contiguous")
        names = [(row.get("Filename") or "").strip().replace("\\", "/") for row in rows]
        graph_entries = [name for name in names if "/Environment/Forest/PCG/" in name]
        if graph_entries:
            raise ValidationError("Shipping IoStore still contains project PCG graph entries")
        pcg_module_entries = [
            name for name in names
            if "/Plugins/PCG/" in name
            or re.search(r"(?:^|/)PCG[^/]*\.(?:dll|pdb|modules)$", name, re.IGNORECASE)
        ]
        if pcg_module_entries:
            raise ValidationError("Shipping IoStore still contains PCG module entries")

        registry_root = scratch / "registry"
        run_tool([str(unrealpak), str(pak), "-Extract", str(registry_root), "-Filter=*AssetRegistry.bin"], "AssetRegistry extraction")
        registry = registry_root / "DiscGolfTour/AssetRegistry.bin"
        if not registry.is_file():
            raise ValidationError("Shipping AssetRegistry was not extracted")
        dump_root = scratch / "dump"
        run_tool([
            str(editor), str(ROOT / "DiscGolfTour.uproject"), "-run=DumpAssetRegistry",
            f"-Path={registry}", f"-OutDir={dump_root}", "-All", "-unattended",
            "-nop4", "-nosplash", "-nullrhi",
        ], "DumpAssetRegistry")
        pages = sorted(dump_root.glob("Page_*.txt"))
        if not pages:
            raise ValidationError("AssetRegistry commandlet emitted no pages")
        registry_text = ""
        for page in pages:
            raw = page.read_bytes()
            registry_text += raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
        if GRAPH_PACKAGE in registry_text or "/Script/PCG.PCGGraph" in registry_text:
            raise ValidationError("Shipping AssetRegistry still exposes a project PCG graph")

        project_root = scratch / "project"
        run_tool([
            str(unrealpak), str(utoc), "-Extract", str(project_root),
            "-Filter=*DiscGolfTour/Content/*.uasset",
        ], "project cooked-asset extraction")
        markers = [
            b"/Game/Environment/Forest/PCG/PCG_TemperateMountainForest",
            b"/Script/PCG.PCGComponent",
            b"GenerateForest",
            b"GenerateLocal",
            b"DiscGolfPcgAuthoringController",
            b"PCGDiscGolfZoneDensitySettings",
        ]
        marker_files: list[str] = []
        file_count = 0
        for path in project_root.rglob("*"):
            if not path.is_file():
                continue
            file_count += 1
            raw = path.read_bytes()
            if any(marker in raw for marker in markers):
                marker_files.append(path.relative_to(project_root).as_posix())
        if marker_files:
            raise ValidationError("cooked project PCG marker(s) remain: " + ", ".join(marker_files))

        binary_markers = [
            b"/Script/PCG",
            b"PCGDiscGolfZoneDensitySettings",
            b"DiscGolfPcgAuthoringController",
            b"GenerateForest",
            b"GenerateLocal",
        ]
        archive_files = [path for path in archive.rglob("*") if path.is_file()]
        archive_pcg_module_files = [
            path.relative_to(archive).as_posix() for path in archive_files
            if "/plugins/pcg/" in f"/{path.relative_to(archive).as_posix().casefold()}"
            or (
                path.suffix.casefold() in {".dll", ".pdb", ".modules"}
                and re.fullmatch(r"(?:UnrealEditor-)?PCG[^.]*", path.stem, re.IGNORECASE)
            )
        ]
        if archive_pcg_module_files:
            raise ValidationError(
                "Shipping archive still contains PCG module file(s): "
                + ", ".join(archive_pcg_module_files)
            )
        binary_marker_files: list[str] = []
        for binary in archive_files:
            if binary.suffix.casefold() in {".exe", ".dll"} \
                    and file_contains_any(binary, binary_markers):
                binary_marker_files.append(binary.relative_to(archive).as_posix())
        if binary_marker_files:
            raise ValidationError(
                "Shipping binary project-PCG marker(s) remain: " + ", ".join(binary_marker_files)
            )
        return {
            "archiveHostPathRecorded": False,
            "archiveFileCount": len(archive_files),
            "containerIdentity": {
                "pak": {"path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak", **identity(pak)},
                "utoc": {"path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc", **identity(utoc)},
                "ucas": {"path": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas", **identity(ucas)},
                "shippingBinary": {
                    "path": "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
                    **identity(shipping_binary),
                },
            },
            "ioStoreChunkCount": len(rows),
            "projectPcgGraphEntryCount": 0,
            "pcgModuleEntryCount": 0,
            "archivePcgModuleFileCount": 0,
            "assetRegistryProjectPcgGraphCount": 0,
            "extractedProjectCookedFileCount": file_count,
            "serializedProjectPcgMarkerFileCount": 0,
            "shippingBinaryProjectPcgMarkerFileCount": 0,
            "freshShippingCookAbsenceProven": True,
        }
    finally:
        resolved = scratch.resolve()
        if resolved.parent == Path("C:\\") and resolved.name.startswith("dgpcgsep_"):
            shutil.rmtree(resolved, ignore_errors=True)


def self_test(policy: dict[str, Any]) -> tuple[int, list[str]]:
    failures: list[str] = []
    caught = 0
    mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("schema", lambda v: v.__setitem__("schema", "bad")),
        ("version", lambda v: v.__setitem__("schemaVersion", 2)),
        ("session", lambda v: v.__setitem__("session", 18)),
        ("policy id", lambda v: v.__setitem__("policyId", "bad")),
        ("authority", lambda v: v.__setitem__("authority", "release")),
        ("state", lambda v: v.__setitem__("state", "PASS")),
        ("strategy", lambda v: v.__setitem__("strategy", "runtime graph")),
        ("target", lambda v: v["target"].__setitem__("configuration", "Development")),
        ("hole scope", lambda v: v["target"]["holeNumbers"].pop()),
        ("tree count", lambda v: v["runtimeAuthority"].__setitem__("treeCount", 43)),
        ("tree hash", lambda v: v["runtimeAuthority"].__setitem__("canonicalPayloadSha256", "0" * 64)),
        ("fallback", lambda v: v["runtimeAuthority"].__setitem__("shippingSourceFallbackAllowed", True)),
        ("random", lambda v: v["runtimeAuthority"].__setitem__("runtimeRandomPlacementAllowed", True)),
        ("runtime dependency", lambda v: v["compileBoundary"]["runtimeForbiddenModuleDependencies"].clear()),
        ("runtime symbol", lambda v: v["compileBoundary"]["runtimeForbiddenSymbols"].pop()),
        ("editor module", lambda v: v["compileBoundary"].__setitem__("editorRequiredModuleDependency", "Engine")),
        ("plugin target", lambda v: v["compileBoundary"]["pluginTargetAllowList"].append("Game")),
        ("graph path", lambda v: v["assetBoundary"].__setitem__("graphPackagePath", "/Game/Other")),
        ("graph hash", lambda v: v["assetBoundary"].__setitem__("graphSha256", "0" * 64)),
        ("never cook", lambda v: v["assetBoundary"].__setitem__("shippingNeverCookDirectory", "/Game")),
        ("legacy class", lambda v: v["assetBoundary"].__setitem__("legacyClassPath", "bad")),
        ("source proof", lambda v: v["requiredSourceProof"].pop()),
        ("source proof value", lambda v: v["requiredSourceProof"].__setitem__(0, "WRONG")),
        ("cook proof", lambda v: v["freshCookProofRequired"].pop()),
        ("cook proof value", lambda v: v["freshCookProofRequired"].__setitem__(0, "WRONG")),
        ("cook overclaim", lambda v: v["claimBoundary"].__setitem__("freshShippingCookAbsenceProven", True)),
        ("bake overclaim", lambda v: v["claimBoundary"].__setitem__("unrealPcgBakeSaveReopenProven", True)),
        ("performance", lambda v: v["claimBoundary"].__setitem__("performanceAcceptance", True)),
        ("blocker", lambda v: v.__setitem__("session13BlockerClosed", True)),
        ("release", lambda v: v.__setitem__("releaseReady", True)),
        ("minimum", lambda v: v["evidence"].__setitem__("selfTestMinimumMutationCount", 1)),
    ]
    for name, mutation in mutations:
        value = copy.deepcopy(policy)
        mutation(value)
        if policy_errors(value):
            caught += 1
        else:
            failures.append(f"policy mutation escaped: {name}")

    source_mutations = [
        ("runtime dependency", "Source/DiscGolfTour/DiscGolfTour.Build.cs", lambda t: t + '\n// "PCG"\n'),
        ("missing editor dependency", "Source/DiscGolfTourEditor/DiscGolfTourEditor.Build.cs", lambda t: t.replace('"PCG",', '')),
        ("runtime API", "Source/DiscGolfTour/DiscGolfEnvironmentController.cpp", lambda t: t + "\nvoid Mutation(){ GenerateLocal(); }\n"),
        ("plugin game target", "DiscGolfTour.uproject", lambda t: t.replace('"Editor"\n      ]', '"Editor", "Game"\n      ]', 1)),
        ("cook exclusion", "Config/DefaultGame.ini", lambda t: t.replace('+DirectoriesToNeverCook=(Path="/Game/Environment/Forest/PCG")', '')),
        ("redirect", "Config/DefaultEngine.ini", lambda t: t.replace('/Script/DiscGolfTourEditor.PCGDiscGolfZoneDensitySettings', '/Script/DiscGolfTour.PCGDiscGolfZoneDensitySettings')),
        ("editor property", "Source/DiscGolfTour/DiscGolfEnvironmentDataAssets.h", lambda t: t.replace('#if WITH_EDITORONLY_DATA', '#if 1', 1)),
        ("authoring graph", "Source/DiscGolfTourEditor/DiscGolfPcgAuthoringController.cpp", lambda t: t.replace('PCGComponent->SetGraph(Graph);', '')),
    ]
    for name, relative, mutation in source_mutations:
        original = source_text(relative, {})
        try:
            source_audit(policy, {relative: mutation(original)})
        except ValidationError:
            caught += 1
        else:
            failures.append(f"source mutation escaped: {name}")
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--unreal-editor-cmd", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-receipt", type=Path)
    parser.add_argument("--emit-json", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    args = parser.parse_args()
    try:
        policy = load_json(ROOT / POLICY_PATH)
        if args.self_test:
            if any((args.archive, args.unrealpak, args.unreal_editor_cmd, args.candidate_id,
                    args.output, args.expected_receipt, args.emit_json, args.release_required)):
                parser.error("--self-test does not accept live arguments")
            caught, failures = self_test(policy)
            minimum = policy.get("evidence", {}).get("selfTestMinimumMutationCount", 28)
            if failures or caught < minimum:
                print(f"Shipping PCG separation self-test: FAIL ({caught} mutations caught)")
                for failure in failures:
                    print(f"- {failure}")
                return 1
            print(f"Shipping PCG separation self-test: PASS ({caught} mutations caught)")
            return 0

        source_facts = source_audit(policy)
        cook_facts = None
        if args.archive is not None:
            if args.unrealpak is None or args.unreal_editor_cmd is None:
                parser.error("--archive requires --unrealpak and --unreal-editor-cmd")
            if not args.candidate_id or not re.fullmatch(
                    r"S19_WindowsShipping_[A-Za-z0-9_-]+", args.candidate_id):
                parser.error("--archive requires a valid --candidate-id")
            cook_facts = archive_audit(args.archive, args.unrealpak, args.unreal_editor_cmd)
        elif args.unrealpak is not None or args.unreal_editor_cmd is not None or args.candidate_id is not None:
            parser.error("cook tools and --candidate-id require --archive")
        if args.output is not None and args.archive is None:
            parser.error("--output requires --archive")
        if args.expected_receipt is not None and args.archive is None:
            parser.error("--expected-receipt requires --archive")
        if args.output is not None and args.expected_receipt is not None:
            parser.error("--output and --expected-receipt are mutually exclusive")
    except (OSError, ValueError, ValidationError) as exc:
        print(f"Shipping PCG separation: FAIL\n- {exc}")
        return 1

    state = COOK_STATE if cook_facts else SOURCE_STATE
    result = {
        "schema": "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1",
        "schemaVersion": 1,
        "session": 19,
        "candidateId": args.candidate_id,
        "state": state,
        "source": source_facts,
        "cook": cook_facts,
        "claimBoundary": {
            **EXPECTED_CLAIMS,
            "freshShippingCookAbsenceProven": bool(cook_facts),
        },
        "session13BlockerClosed": False,
        "releaseReady": False,
    }
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    expected_name = f"ShippingPcgSeparation-{args.candidate_id}.json"
    evidence_root = (ROOT / "Evidence/Session19").resolve()
    if args.output is not None:
        output = args.output.resolve()
        if output.parent != evidence_root or output.name != expected_name:
            print(f"Shipping PCG separation: FAIL\n- --output must be Evidence/Session19/{expected_name}")
            return 1
        try:
            with output.open("x", encoding="utf-8", newline="\n") as handle:
                handle.write(rendered)
        except OSError as exc:
            print(f"Shipping PCG separation: FAIL\n- could not create fresh receipt: {exc}")
            return 1
    if args.expected_receipt is not None:
        expected_receipt = args.expected_receipt.resolve()
        if expected_receipt.parent != evidence_root or expected_receipt.name != expected_name:
            print(
                "Shipping PCG separation: FAIL\n"
                f"- --expected-receipt must be Evidence/Session19/{expected_name}"
            )
            return 1
        try:
            expected_value = load_json(expected_receipt)
        except ValidationError as exc:
            print(f"Shipping PCG separation: FAIL\n- {exc}")
            return 1
        if expected_value != result:
            print("Shipping PCG separation: FAIL\n- live archive audit differs from expected receipt")
            return 1
    if args.emit_json:
        print(rendered, end="")
    else:
        print(f"Shipping PCG separation: {state}")
        print("Shipping runtime module/generation APIs are absent; PCG authoring is Editor-only.")
        print("Fresh Shipping cook absence and human/performance gates remain pending." if not cook_facts
              else "Fresh Shipping cook absence is proven; human/performance gates remain pending.")
    if args.release_required:
        if not args.emit_json:
            print(f"Shipping PCG separation release state: {RELEASE_STATE}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
