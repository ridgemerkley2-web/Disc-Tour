#!/usr/bin/env python3
"""Deterministic filesystem-only Session 8 availability and cook-source audit.

The audit launches no Unreal process, build tool, downloader, browser, or vendor
utility. It reports local UE 5.8 MetaHuman plugin/Core Data evidence, project
asset entry points, acquisition registries, brand gates, and the exact frozen
69-in / 12-out cook-source inventory. Missing production content is BLOCKED,
never inferred or fabricated.

Exit codes: 0 READY, 1 audit integrity ERROR, 2 expected fail-closed BLOCKED.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SPEC_PATH = PROJECT_ROOT / "Config/DG_RuntimeCookManifest.json"
PROJECT_PATH = PROJECT_ROOT / "DiscGolfTour.uproject"
DEFAULT_GAME_PATH = PROJECT_ROOT / "Config/DefaultGame.ini"
BUILDKIT_ROOT = PROJECT_ROOT / "_BuildKit/DiscGolfCorePlayabilityKit_v1.5"
ACQUIRED_LOCK_PATH = BUILDKIT_ROOT / "Config/DG_AcquiredAssetLock.json"
THIRD_PARTY_PATH = BUILDKIT_ROOT / "Config/DG_ThirdPartyAssetRegistry.json"
BRAND_PATH = BUILDKIT_ROOT / "Config/DG_BrandSafetyRegistry.json"
PURCHASE_REGISTER_PATH = BUILDKIT_ROOT / "Assets/purchase_and_license_register.csv"
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session8AvailabilityAudit.json"
FROZEN_SOURCE_SPEC_SHA256 = (
    "E3F6FEAB2AFAFFE414D0B7F859FA3107490B3AC19EC6FF78DAA7F9EA90888AF7")

EXPECTED_MANIFEST_PACKAGE = "/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest"
EXPECTED_DGMASTER_PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster")
EXPECTED_METAHUMAN_PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default")
EXPECTED_METAHUMAN_ACTOR_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default")
EXPECTED_METAHUMAN_RETARGET_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman")

PLUGIN_DESCRIPTORS = {
    "MetaHumanSDK": "Engine/Plugins/MetaHuman/MetaHumanSDK/MetaHumanSDK.uplugin",
    "MetaHumanCharacter": (
        "Engine/Plugins/MetaHuman/MetaHumanCharacter/MetaHumanCharacter.uplugin"),
    "MetaHumanAnimator": (
        "Engine/Plugins/MetaHuman/MetaHumanAnimator/MetaHuman.uplugin"),
    "RigLogic": "Engine/Plugins/Animation/RigLogic/RigLogic.uplugin",
    "HairStrands": "Engine/Plugins/Runtime/HairStrands/HairStrands.uplugin",
    "MetaHumanRuntimeDeprecated": (
        "Engine/Plugins/Experimental/MetaHuman/MetaHumanRuntime/MetaHumanRuntime.uplugin"),
}
CORE_DATA_ROOT = (
    "Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Optional")
CORE_DATA_ENGINE_CHECK = (
    "Engine/Plugins/MetaHuman/MetaHumanCharacter/Source/"
    "MetaHumanCharacterEditor/Private/MetaHumanCharacterEditorModule.cpp")
CORE_DATA_REQUIRED_DIRECTORIES = ("TextureSynthesis", "BodyTextures")
REQUIRED_CONFIG_TOKENS = (
    'PrimaryAssetType="DGRuntimeCookManifest"',
    'AssetBaseClass="/Script/DiscGolfTour.DiscGolfRuntimeCookManifest"',
    'Path="/Game/DiscGolf/Cook"',
    'PrimaryAssetType="DiscGolfAvatarBackendProfile"',
    'AssetBaseClass="/Script/DiscGolfCharacterFramework.DiscGolfAvatarBackendProfile"',
    'Path="/Game/DiscGolf/Characters/Avatar/Data"',
)
REQUIRED_SOURCE_TOKENS = {
    "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.h": (
        "UDiscGolfRuntimeCookManifest",
        'meta=(AssetBundles="Runtime")',
        "ExplicitlyExcludedPackages",
    ),
    "Source/DiscGolfTour/DiscGolfRuntimeCookManifest.cpp": (
        'TEXT("DGRuntimeCookManifest")',
        "ValidateRuntimeContract",
        "ExpectedRuntimePackageCount != 69",
        "ExpectedExcludedPackageCount != 12",
    ),
    "Scripts/create_dg_character_session8_cook_assets.py": (
        "PASS_CREATED_AND_SAVED",
        "METAHUMAN_ASSETS_CREATED = 0",
    ),
    "Scripts/validate_dg_character_session8_cook_assets.py": (
        "PASS_NO_DISK_MUTATION",
        "EXPECTED_METAHUMAN_PROFILE_OBJECT",
    ),
    "Scripts/validate_dg_character_session8_cook_no_write.py": (
        "PASS_NO_WRITE",
        "asset_registry_mutation",
    ),
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _digest_lines(values: list[str]) -> str:
    payload = "".join(f"{value}\n" for value in sorted(values)).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def _load_json(path: Path, label: str, errors: list[str]) -> dict:
    if not path.is_file():
        errors.append(f"{label} missing: {path}")
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # pragma: no cover - diagnostic path
        errors.append(f"{label} invalid JSON: {exc}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{label} root is not an object")
        return {}
    return value


def _flatten_groups(spec: dict, field: str, errors: list[str]) -> tuple[list[str], list[dict]]:
    values: list[str] = []
    provenance: list[dict] = []
    groups = spec.get(field)
    if not isinstance(groups, list):
        errors.append(f"Cook spec {field} is not an array")
        return values, provenance
    for index, group in enumerate(groups):
        if not isinstance(group, dict):
            errors.append(f"Cook spec {field}[{index}] is not an object")
            continue
        source = group.get("provenance")
        packages = group.get("packages")
        if not isinstance(source, str) or not source:
            errors.append(f"Cook spec {field}[{index}] has no provenance")
        if not isinstance(packages, list) or not all(
                isinstance(value, str) for value in packages):
            errors.append(f"Cook spec {field}[{index}] packages are invalid")
            continue
        values.extend(packages)
        provenance.append({
            "provenance": source,
            "package_count": len(packages),
            "package_list_sha256": _digest_lines(packages),
            "reason": group.get("reason"),
            "shipping_status": group.get("shipping_status"),
        })
    return values, provenance


def _package_to_file(package: str) -> Path:
    if not package.startswith("/Game/"):
        raise ValueError(f"Package escaped /Game: {package}")
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _project_packages() -> list[str]:
    content = PROJECT_ROOT / "Content"
    if not content.is_dir():
        return []
    return sorted(
        "/Game/" + path.relative_to(content).with_suffix("").as_posix()
        for path in content.rglob("*.uasset") if path.is_file())


def _package_snapshot(packages: list[str]) -> dict[str, dict]:
    result = {}
    for package in sorted(packages):
        path = _package_to_file(package)
        if path.is_file():
            result[package] = {"bytes": path.stat().st_size, "sha256": _sha256(path)}
        else:
            result[package] = {"missing": True}
    return result


def _resolve_engine_root(explicit: str | Path | None) -> tuple[Path, str]:
    if explicit:
        return Path(explicit).resolve(), "COMMAND_LINE_OR_CALLER"
    environment = os.environ.get("DGT_UE_ENGINE_ROOT")
    if environment:
        return Path(environment).resolve(), "DGT_UE_ENGINE_ROOT"
    association = "5.8"
    try:
        association = str(json.loads(PROJECT_PATH.read_text(
            encoding="utf-8")).get("EngineAssociation", association))
    except Exception:
        pass
    candidates = (
        Path(f"C:/Program Files/Epic Games/UE_{association}"),
        Path(f"D:/Epic Games/UE_{association}"),
    )
    for candidate in candidates:
        if (candidate / "Engine/Build/Build.version").is_file():
            return candidate.resolve(), "PROJECT_ASSOCIATION_WELL_KNOWN_PATH"
    return candidates[0], "UNRESOLVED_PROJECT_ASSOCIATION_WELL_KNOWN_PATH"


def _plugin_record(engine_root: Path, relative: str, project_plugins: dict) -> dict:
    path = engine_root / relative
    if not path.is_file():
        return {"path": relative, "status": "MISSING"}
    descriptor = json.loads(path.read_text(encoding="utf-8"))
    plugin_name = path.stem
    explicit = project_plugins.get(plugin_name)
    if explicit is not None:
        effective = bool(explicit)
        basis = "PROJECT_EXPLICIT"
    elif "EnabledByDefault" in descriptor:
        effective = bool(descriptor.get("EnabledByDefault"))
        basis = "DESCRIPTOR_ENABLED_BY_DEFAULT"
    else:
        effective = None
        basis = "NOT_INFERRED"
    return {
        "path": relative,
        "status": "PRESENT",
        "sha256": _sha256(path),
        "friendly_name": descriptor.get("FriendlyName"),
        "version": descriptor.get("Version"),
        "version_name": descriptor.get("VersionName"),
        "enabled_by_default": descriptor.get("EnabledByDefault"),
        "project_explicit_enabled": explicit,
        "effective_enabled": effective,
        "effective_enabled_basis": basis,
        "beta": bool(descriptor.get("IsBetaVersion", False)),
        "experimental": bool(descriptor.get("IsExperimentalVersion", False)),
        "deprecated_description": descriptor.get("Description"),
    }


def _find_entry(entries: object, key: str, value: str) -> dict | None:
    if not isinstance(entries, list):
        return None
    return next((entry for entry in entries
                 if isinstance(entry, dict) and entry.get(key) == value), None)


def _purchase_rows(errors: list[str]) -> list[dict]:
    if not PURCHASE_REGISTER_PATH.is_file():
        errors.append(f"Purchase/license register missing: {PURCHASE_REGISTER_PATH}")
        return []
    try:
        with PURCHASE_REGISTER_PATH.open(
                "r", encoding="utf-8-sig", newline="") as stream:
            return list(csv.DictReader(stream))
    except Exception as exc:  # pragma: no cover - diagnostic path
        errors.append(f"Purchase/license register invalid: {exc}")
        return []


def _cook_source_audit(spec: dict, errors: list[str]) -> dict:
    spec_sha = _sha256(SPEC_PATH) if SPEC_PATH.is_file() else None
    if spec_sha != FROZEN_SOURCE_SPEC_SHA256:
        errors.append(
            f"Frozen cook source spec hash differs: {spec_sha} != "
            f"{FROZEN_SOURCE_SPEC_SHA256}")
    runtime, runtime_provenance = _flatten_groups(
        spec, "runtime_package_groups", errors)
    excluded, excluded_provenance = _flatten_groups(
        spec, "excluded_package_groups", errors)
    if spec.get("schema") != "DiscGolfTour.RuntimeCookManifestSource.v1" \
            or spec.get("schema_version") != 1:
        errors.append("Cook spec schema identity/version differs")
    if len(runtime) != 69 or spec.get("expected_runtime_package_count") != 69:
        errors.append(f"Cook spec runtime count is {len(runtime)}, expected 69")
    if len(excluded) != 12 or spec.get("expected_excluded_package_count") != 12:
        errors.append(f"Cook spec excluded count is {len(excluded)}, expected 12")
    if len(runtime) != len(set(runtime)):
        errors.append("Cook spec runtime package paths are not unique")
    if len(excluded) != len(set(excluded)):
        errors.append("Cook spec excluded package paths are not unique")
    overlap = sorted(set(runtime) & set(excluded))
    if overlap:
        errors.append(f"Cook spec runtime/excluded overlap: {overlap}")
    for package in runtime + excluded:
        if not package.startswith("/Game/DiscGolf/") or "." in package.rsplit("/", 1)[-1]:
            errors.append(f"Cook spec path is not a package-only /Game/DiscGolf path: {package}")

    outputs = spec.get("asset_outputs", {})
    manifest_package = outputs.get("runtime_cook_manifest", {}).get("package")
    dgmaster_package = outputs.get("dg_master_backend_profile", {}).get("package")
    if manifest_package != EXPECTED_MANIFEST_PACKAGE:
        errors.append("Cook spec manifest package entry point differs")
    if dgmaster_package != EXPECTED_DGMASTER_PROFILE_PACKAGE:
        errors.append("Cook spec DGMaster backend profile entry point differs")
    reserved = spec.get("reserved_optional_metahuman_entry_points", {})
    if reserved.get("created_by_session8_cook_author") is not False:
        errors.append("Cook spec must forbid MetaHuman fabrication by the cook author")

    actual_discgolf = sorted(
        package for package in _project_packages()
        if package.startswith("/Game/DiscGolf/"))
    authored_outputs = [EXPECTED_MANIFEST_PACKAGE, EXPECTED_DGMASTER_PROFILE_PACKAGE]
    present_outputs = sorted(package for package in authored_outputs
                             if _package_to_file(package).is_file())
    expected_present = set(runtime) | set(excluded) | set(present_outputs)
    missing = sorted((set(runtime) | set(excluded)) - set(actual_discgolf))
    unexpected = sorted(set(actual_discgolf) - expected_present)
    if missing:
        errors.append(f"Frozen cook-source packages missing: {missing}")
    if unexpected:
        errors.append(f"Unclassified /Game/DiscGolf packages: {unexpected}")
    return {
        "status": "PASS" if not missing and not unexpected else "FAIL",
        "spec_path": SPEC_PATH.relative_to(PROJECT_ROOT).as_posix(),
        "spec_sha256": spec_sha,
        "frozen_spec_sha256": FROZEN_SOURCE_SPEC_SHA256,
        "runtime_package_count": len(runtime),
        "runtime_package_list_sha256": _digest_lines(runtime),
        "runtime_provenance": runtime_provenance,
        "excluded_package_count": len(excluded),
        "excluded_package_list_sha256": _digest_lines(excluded),
        "excluded_provenance": excluded_provenance,
        "runtime_packages": sorted(runtime),
        "excluded_packages": sorted(excluded),
        "actual_discgolf_package_count": len(actual_discgolf),
        "present_author_outputs": present_outputs,
        "missing_frozen_packages": missing,
        "unexpected_discgolf_packages": unexpected,
        "package_file_snapshot": _package_snapshot(actual_discgolf),
    }


def audit(
        engine_root: str | Path | None = None,
        *,
        write_report: bool = True,
        report_path: str | Path | None = None) -> dict:
    errors: list[str] = []
    blockers: list[str] = []
    spec = _load_json(SPEC_PATH, "Runtime cook source spec", errors)
    project = _load_json(PROJECT_PATH, "Unreal project descriptor", errors)
    acquired = _load_json(ACQUIRED_LOCK_PATH, "Acquired asset lock", errors)
    third_party = _load_json(THIRD_PARTY_PATH, "Third-party registry", errors)
    brands = _load_json(BRAND_PATH, "Brand safety registry", errors)
    purchases = _purchase_rows(errors)

    cook = _cook_source_audit(spec, errors) if spec else {"status": "FAIL"}

    config_text = DEFAULT_GAME_PATH.read_text(
        encoding="utf-8") if DEFAULT_GAME_PATH.is_file() else ""
    missing_config_tokens = [token for token in REQUIRED_CONFIG_TOKENS
                             if token not in config_text]
    if missing_config_tokens:
        errors.append(f"Asset Manager cook config tokens missing: {missing_config_tokens}")
    missing_source_tokens: dict[str, list[str]] = {}
    for relative, tokens in REQUIRED_SOURCE_TOKENS.items():
        path = PROJECT_ROOT / relative
        source = path.read_text(encoding="utf-8") if path.is_file() else ""
        missing = [token for token in tokens if token not in source]
        if missing:
            missing_source_tokens[relative] = missing
            errors.append(f"Cook groundwork source contract missing in {relative}: {missing}")

    resolved_engine, engine_resolution = _resolve_engine_root(engine_root)
    build_version = _load_json(
        resolved_engine / "Engine/Build/Build.version", "UE build version", errors)
    if (build_version.get("MajorVersion"), build_version.get("MinorVersion")) != (5, 8):
        blockers.append("UE_5_8_BUILD_NOT_VERIFIED")

    project_plugins = {
        entry.get("Name"): bool(entry.get("Enabled"))
        for entry in project.get("Plugins", []) if isinstance(entry, dict)
    }
    plugin_records = {
        name: _plugin_record(resolved_engine, relative, project_plugins)
        for name, relative in PLUGIN_DESCRIPTORS.items()
    }
    creator = plugin_records["MetaHumanCharacter"]
    if creator.get("status") != "PRESENT":
        blockers.append("METAHUMAN_CREATOR_PLUGIN_MISSING")
    elif creator.get("project_explicit_enabled") is not True:
        blockers.append("METAHUMAN_CREATOR_NOT_EXPLICITLY_ENABLED_IN_PROJECT")

    core_root = resolved_engine / CORE_DATA_ROOT
    required_directories = {
        relative: (core_root / relative).is_dir()
        for relative in CORE_DATA_REQUIRED_DIRECTORIES
    }
    texture_archives = sorted(
        path.relative_to(core_root).as_posix()
        for path in (core_root / "TextureSynthesis").rglob("*.ar")
        if path.is_file()) if required_directories["TextureSynthesis"] else []
    body_texture_assets = sorted(
        path.relative_to(core_root).as_posix()
        for path in (core_root / "BodyTextures").glob("*.uasset")
        if path.is_file()) if required_directories["BodyTextures"] else []
    core_files = sorted(path for path in core_root.rglob("*")
                        if path.is_file()) if core_root.is_dir() else []
    core_present = (
        core_root.is_dir()
        and all(required_directories.values())
        and bool(texture_archives)
        and bool(body_texture_assets))
    if not core_present:
        blockers.append("METAHUMAN_CREATOR_CORE_DATA_OPTIONAL_PAYLOAD_MISSING")
    core_data = {
        "status": (
            "PRESENT_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT"
            if core_present
            else "MISSING_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT"),
        "evidence_method": (
            "MIRRORS_FMetaHumanCharacterEditorModule_"
            "IsOptionalMetaHumanContentInstalled_FILESYSTEM_CHECKS"),
        "official_requirement_reference": (
            "https://dev.epicgames.com/documentation/metahuman/creating-a-character"),
        "engine_contract_source": CORE_DATA_ENGINE_CHECK,
        "engine_contract_source_sha256": (
            _sha256(resolved_engine / CORE_DATA_ENGINE_CHECK)
            if (resolved_engine / CORE_DATA_ENGINE_CHECK).is_file() else None),
        "root": CORE_DATA_ROOT,
        "root_present": core_root.is_dir(),
        "required_directories": required_directories,
        "texture_synthesis_archive_count": len(texture_archives),
        "texture_synthesis_archives": texture_archives,
        "body_texture_asset_count": len(body_texture_assets),
        "body_texture_assets": body_texture_assets,
        "file_count": len(core_files),
        "bytes": sum(path.stat().st_size for path in core_files),
    }

    project_packages = _project_packages()
    metahuman_named = sorted(package for package in project_packages
                             if "metahuman" in package.casefold())
    groom_named = sorted(package for package in project_packages
                         if "groom" in package.casefold())
    backend_profiles = sorted(package for package in project_packages
                              if "/DA_DG_AvatarBackend_".casefold()
                              in package.casefold())
    retarget_named = sorted(package for package in project_packages
                            if package.rsplit("/", 1)[-1].casefold().startswith("rtg_")
                            and "metahuman" in package.casefold())
    actor_named = sorted(package for package in project_packages
                         if "BP_DG_MetaHuman".casefold() in package.casefold())
    premium_named = sorted(package for package in project_packages
                           if "premium" in package.casefold())

    canonical_presence = {
        "cook_manifest": EXPECTED_MANIFEST_PACKAGE in project_packages,
        "dg_master_backend_profile": EXPECTED_DGMASTER_PROFILE_PACKAGE in project_packages,
        "metahuman_backend_profile": EXPECTED_METAHUMAN_PROFILE_PACKAGE in project_packages,
        "metahuman_visual_actor": EXPECTED_METAHUMAN_ACTOR_PACKAGE in project_packages,
        "metahuman_retarget": EXPECTED_METAHUMAN_RETARGET_PACKAGE in project_packages,
    }
    if not canonical_presence["cook_manifest"] \
            or not canonical_presence["dg_master_backend_profile"]:
        blockers.append("AUTHORIZED_UE_COOK_ASSET_AUTHOR_RUN_REQUIRED")
    if not metahuman_named:
        blockers.append("NO_PROJECT_METAHUMAN_NAMED_PACKAGES")
    if not canonical_presence["metahuman_backend_profile"]:
        blockers.append("NO_CANONICAL_METAHUMAN_BACKEND_PROFILE")
    if not canonical_presence["metahuman_visual_actor"]:
        blockers.append("NO_CANONICAL_ASSEMBLED_METAHUMAN_VISUAL_ACTOR")
    if not canonical_presence["metahuman_retarget"]:
        blockers.append("NO_CANONICAL_DGMASTER_TO_METAHUMAN_RETARGET")

    acquired_entries = acquired.get("acquired")
    if not isinstance(acquired_entries, list):
        errors.append("Acquired asset lock 'acquired' field is not an array")
        acquired_entries = []
    meta_registry = _find_entry(third_party.get("assets"), "integration_id", "metahuman")
    paid_registry = sorted(
        ({"integration_id": entry.get("integration_id"), "status": entry.get("status")}
         for entry in third_party.get("assets", [])
         if isinstance(entry, dict) and entry.get("purchase_required") is True),
        key=lambda entry: str(entry["integration_id"]))
    paid_purchases = sorted(
        ({"integration_id": row.get("integration_id"), "status": row.get("status")}
         for row in purchases if row.get("integration_id")),
        key=lambda entry: str(entry["integration_id"]))

    premium = _find_entry(brands.get("brands"), "brand_id", "premium_disc_golf")
    generic = _find_entry(brands.get("brands"), "brand_id", "dg_generic")
    premium_logo_status = premium.get("logo_asset_status") if premium else None
    if brands.get("default_policy") != "BLOCK_UNLESS_EXPLICITLY_APPROVED":
        errors.append("Brand registry lost fail-closed default policy")

    unique_blockers = sorted(set(blockers))
    status = "ERROR" if errors else ("BLOCKED" if unique_blockers else "READY")
    result = {
        "schema": "DiscGolfTour.Session8AvailabilityAudit.v1",
        "status": status,
        "determinism": "FILESYSTEM_ONLY_NO_NETWORK_NO_ENGINE_PROCESS",
        "project_root": str(PROJECT_ROOT),
        "engine_root": str(resolved_engine),
        "engine_root_resolution": engine_resolution,
        "engine_build": build_version,
        "project_plugin_enablement": project_plugins,
        "engine_plugins": plugin_records,
        "metahuman_creator_core_data": core_data,
        "project_asset_truth": {
            "package_count": len(project_packages),
            "metahuman_named_packages": metahuman_named,
            "groom_named_packages": groom_named,
            "avatar_backend_profiles": backend_profiles,
            "metahuman_retarget_candidates": retarget_named,
            "metahuman_actor_candidates": actor_named,
            "premium_named_packages": premium_named,
            "canonical_entry_point_presence": canonical_presence,
            "reserved_optional_metahuman_entry_points": spec.get(
                "reserved_optional_metahuman_entry_points"),
            "scope_note": (
                "Package-name filesystem evidence only; strict class/dependency proof "
                "requires the authorized Unreal validation run."),
        },
        "acquisition_truth": {
            "acquired_lock_path": ACQUIRED_LOCK_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "acquired_entries": acquired_entries,
            "acquired_count": len(acquired_entries),
            "metahuman_registry_entry": meta_registry,
            "paid_candidate_registry_statuses": paid_registry,
            "purchase_register_statuses": paid_purchases,
            "vendor_assets_integrated_by_this_work": [],
        },
        "branding_truth": {
            "default_policy": brands.get("default_policy"),
            "premium_disc_golf": premium,
            "generic_unbranded": generic,
            "premium_logo_status": premium_logo_status,
            "project_premium_named_packages": premium_named,
            "other_real_brands": "BLOCKED_UNLESS_REGISTRY_APPROVED",
        },
        "cook_source_inventory": cook,
        "cook_groundwork_status": (
            "SOURCE_READY_AUTHOR_RUN_REQUIRED"
            if not canonical_presence["cook_manifest"]
            and not canonical_presence["dg_master_backend_profile"]
            else (
                "AUTHORED_OUTPUTS_PRESENT_REQUIRES_UE_STRICT_VALIDATION"
                if canonical_presence["cook_manifest"]
                and canonical_presence["dg_master_backend_profile"]
                else "PARTIAL_AUTHOR_OUTPUT_BLOCKED")),
        "author_run_required": (
            not canonical_presence["cook_manifest"]
            or not canonical_presence["dg_master_backend_profile"]),
        "metahuman_integration_status": "BLOCKED" if any(
            "METAHUMAN" in blocker for blocker in unique_blockers) else "READY",
        "premium_brand_art_status": (
            "OUT_OF_SESSION8_SCOPE_NOT_BUNDLED"
            if premium_logo_status != "BUNDLED_AUTHORIZED" else "AVAILABLE_AUTHORIZED"),
        "qualifications": ([
            "PREMIUM_DISC_GOLF_ART_OUT_OF_SESSION8_SCOPE_AND_NOT_BUNDLED"
        ] if premium_logo_status != "BUNDLED_AUTHORIZED" else []),
        "missing_config_tokens": missing_config_tokens,
        "missing_source_tokens": missing_source_tokens,
        "blockers": unique_blockers,
        "integrity_errors": errors,
        "network_access": "NONE",
        "asset_acquisition": "NONE",
        "unreal_launched": False,
        "ubt_launched": False,
        "cook_launched": False,
        "metahuman_assets_created": 0,
    }
    if write_report:
        destination = Path(report_path) if report_path else REPORT_PATH
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine-root")
    parser.add_argument("--report")
    parser.add_argument("--no-report", action="store_true")
    args = parser.parse_args()
    result = audit(
        args.engine_root,
        write_report=not args.no_report,
        report_path=args.report)
    print(
        "SESSION 8 AVAILABILITY " + result["status"] + ": "
        f"runtime={result.get('cook_source_inventory', {}).get('runtime_package_count')} "
        f"excluded={result.get('cook_source_inventory', {}).get('excluded_package_count')} "
        f"core_data={result.get('metahuman_creator_core_data', {}).get('status')} "
        f"blockers={len(result.get('blockers', []))}")
    for error in result.get("integrity_errors", []):
        print(f"  ERROR: {error}")
    for blocker in result.get("blockers", []):
        print(f"  BLOCKED: {blocker}")
    if result["status"] == "ERROR":
        return 1
    return 2 if result["status"] == "BLOCKED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
