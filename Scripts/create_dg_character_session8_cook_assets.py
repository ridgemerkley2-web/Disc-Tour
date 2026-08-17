"""Author only the Session 8 cook manifest and DGMaster fallback profile.

This utility requires a compiled UE 5.8 editor and a separately authorized run.
It creates exactly two project-owned data assets from the frozen source spec.
It never creates or guesses a MetaHuman, assembled actor, retargeter, groom,
vendor asset, licensed logo, gameplay authority, save object, or pawn wiring.
Existing partial or stale outputs fail closed and are never reconciled in place.
"""

from __future__ import annotations

import hashlib
import json
import re
import runpy
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_PATH = PROJECT_ROOT / "Config/DG_RuntimeCookManifest.json"
AVAILABILITY_AUDIT = (
    PROJECT_ROOT / "Scripts/audit_dg_character_session8_availability.py")
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session8CookAssetCreation.json")
METAHUMAN_ASSETS_CREATED = 0
EXPECTED_RUNTIME_COUNT = 69
EXPECTED_EXCLUDED_COUNT = 12
SOURCE_SPEC_RELATIVE = "Config/DG_RuntimeCookManifest.json"
FROZEN_SOURCE_SPEC_SHA256 = (
    "E3F6FEAB2AFAFFE414D0B7F859FA3107490B3AC19EC6FF78DAA7F9EA90888AF7")
EXPECTED_MANIFEST_OBJECT = (
    "/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest.DA_DG_RuntimeCookManifest")
EXPECTED_DGMASTER_PROFILE_OBJECT = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster."
    "DA_DG_AvatarBackend_DGMaster")
EXPECTED_METAHUMAN_BACKEND_ID = "metahuman_assembled"
EXPECTED_METAHUMAN_PROFILE_OBJECT = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default")
EXPECTED_METAHUMAN_ACTOR_CLASS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default_C")
EXPECTED_METAHUMAN_RETARGET_OBJECT = (
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman")
COMMON_METADATA = {
    "DG_Session": "8",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_GROUNDWORK",
    "DG_SourceSpec": SOURCE_SPEC_RELATIVE,
    "DG_Authority": "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    "DG_ProductionArtApproved": "false",
    "DG_VendorAsset": "false",
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load_spec() -> dict:
    return json.loads(SPEC_PATH.read_text(encoding="utf-8"))


def _flatten(spec: dict, field: str) -> list[str]:
    return [package for group in spec[field] for package in group["packages"]]


def _package_path(path: str) -> str:
    leaf = path.rsplit("/", 1)[-1]
    return path.rsplit(".", 1)[0] if "." in leaf else path


def _object_path(path: str) -> str:
    package = _package_path(path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _asset_name(path: str) -> str:
    return _package_path(path).rsplit("/", 1)[-1]


def _asset_folder(path: str) -> str:
    return _package_path(path).rsplit("/", 1)[0]


def _uasset_path(path: str) -> Path:
    package = _package_path(path)
    if not package.startswith("/Game/"):
        raise RuntimeError(f"Session 8 output escaped /Game: {package}")
    return PROJECT_ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def _soft_path(value) -> str:
    if value is None:
        return ""
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "to_soft_object_path"):
        value = value.to_soft_object_path()
    text = str(value)
    match = re.search(r"(/Game/[^'\" ]+)", text)
    return match.group(1) if match else text


def _enum_token(value) -> str:
    name = getattr(value, "name", None)
    if callable(name):
        name = name()
    text = str(name) if name else str(value).rsplit(".", 1)[-1].split(":", 1)[0]
    return re.sub(r"[^A-Za-z0-9]", "", text).casefold()


def _enum_member(enum_type, name: str):
    token = name if "_" in name else re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()
    try:
        return getattr(enum_type, token)
    except AttributeError as exc:
        raise RuntimeError(f"Could not resolve {enum_type.__name__}.{token}") from exc


def _ensure_folder(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create owned Session 8 folder: {path}")


def _content_snapshot(excluded_outputs: set[Path]) -> dict[str, dict]:
    root = PROJECT_ROOT / "Content/DiscGolf"
    result = {}
    if not root.is_dir():
        return result
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix.casefold() not in {
                ".uasset", ".uexp", ".ubulk", ".uptnl"}:
            continue
        if path in excluded_outputs:
            continue
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        result[relative] = {"bytes": path.stat().st_size, "sha256": _sha256(path)}
    return result


def _metadata(role: str, spec_sha: str) -> dict[str, str]:
    return {
        **COMMON_METADATA,
        "DG_AssetRole": role,
        "DG_SourceSpecSHA256": spec_sha,
    }


def _mark_owned(asset, role: str, spec_sha: str) -> None:
    for key, value in _metadata(role, spec_sha).items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)


def _require_owned(asset, role: str, spec_sha: str) -> list[str]:
    errors = []
    for key, expected in _metadata(role, spec_sha).items():
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key))
        if actual != expected:
            errors.append(
                f"{asset.get_path_name()} metadata {key}={actual!r}, expected {expected!r}")
    return errors


def _load_required_runtime_assets(paths: list[str]) -> list:
    assets = []
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            raise RuntimeError(f"Frozen runtime package did not load: {_object_path(path)}")
        assets.append(asset)
    return assets


def _create_data_asset(path: str, data_asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", data_asset_class.static_class())
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(path), _asset_folder(path), data_asset_class, factory)
    if asset is None:
        raise RuntimeError(f"DataAssetFactory failed for {_object_path(path)}")
    return asset


def _validate_profile(profile, output: dict, spec_sha: str) -> list[str]:
    errors = _require_owned(profile, "DGMASTER_FALLBACK_BACKEND_PROFILE", spec_sha)
    if str(profile.get_editor_property("backend_id")) != output["backend_id"]:
        errors.append("DGMaster BackendId differs")
    if _enum_token(profile.get_editor_property("backend")) != "dgmaster":
        errors.append("DGMaster backend enum differs")
    if _soft_path(profile.get_editor_property("visual_actor_class")):
        errors.append("DGMaster fallback must not invent a visual actor class")
    if _soft_path(profile.get_editor_property("retarget_asset")):
        errors.append("DGMaster fallback must not invent a retarget asset")
    if bool(profile.get_editor_property("use_runtime_retargeting")):
        errors.append("DGMaster fallback must not require runtime retargeting")
    if str(profile.get_editor_property("preferred_quality_profile_id")) != "Prototype":
        errors.append("DGMaster fallback quality profile differs")
    if bool(profile.get_editor_property("allow_runtime_face_sculpting")):
        errors.append("DGMaster fallback unexpectedly enables MetaHuman face sculpting")
    return errors


def _validate_manifest(
        manifest, spec: dict, runtime_paths: list[str], excluded: list[str],
        profile_object: str, spec_sha: str) -> list[str]:
    errors = _require_owned(manifest, "RUNTIME_COOK_MANIFEST", spec_sha)
    checks = {
        "manifest_id": spec["manifest_id"],
        "source_spec_relative_path": SOURCE_SPEC_RELATIVE,
        "source_spec_sha256": spec_sha,
        "content_status": spec["content_status"],
        "shipping_status": spec["shipping_status"],
        "authority": spec["authority"],
    }
    for field, expected in checks.items():
        actual = str(manifest.get_editor_property(field))
        if actual != expected:
            errors.append(f"Manifest {field}={actual!r}, expected {expected!r}")
    if int(manifest.get_editor_property("expected_runtime_package_count")) != 69:
        errors.append("Manifest expected runtime count differs")
    if int(manifest.get_editor_property("expected_excluded_package_count")) != 12:
        errors.append("Manifest expected excluded count differs")
    actual_runtime = [_package_path(_soft_path(value)) for value in
                      manifest.get_editor_property("runtime_assets")]
    if actual_runtime != runtime_paths:
        errors.append("Manifest runtime asset paths/order differ from frozen source spec")
    actual_profiles = [_soft_path(value) for value in
                       manifest.get_editor_property("avatar_backend_profiles")]
    if actual_profiles != [profile_object]:
        errors.append("Manifest backend profile entry point differs")
    actual_excluded = [str(value) for value in
                       manifest.get_editor_property("explicitly_excluded_packages")]
    if actual_excluded != excluded:
        errors.append("Manifest excluded package paths/order differ from frozen source spec")
    return errors


def _availability_ready_for_author() -> dict:
    namespace = runpy.run_path(
        str(AVAILABILITY_AUDIT), run_name="dg_session8_cook_author_availability")
    result = namespace["audit"](write_report=True)
    if result.get("integrity_errors"):
        raise RuntimeError(
            "Session 8 filesystem audit integrity failed: "
            + "; ".join(result["integrity_errors"]))
    if result.get("cook_source_inventory", {}).get("status") != "PASS":
        raise RuntimeError("Session 8 frozen cook-source inventory did not pass")
    return result


def main() -> None:
    availability = _availability_ready_for_author()
    spec = _load_spec()
    spec_sha = _sha256(SPEC_PATH)
    if spec_sha != FROZEN_SOURCE_SPEC_SHA256:
        raise RuntimeError(
            f"Frozen cook source spec hash differs: {spec_sha} != "
            f"{FROZEN_SOURCE_SPEC_SHA256}")
    runtime_paths = _flatten(spec, "runtime_package_groups")
    excluded = _flatten(spec, "excluded_package_groups")
    if len(runtime_paths) != EXPECTED_RUNTIME_COUNT or len(set(runtime_paths)) != 69:
        raise RuntimeError("Cook source does not contain exactly 69 unique runtime packages")
    if len(excluded) != EXPECTED_EXCLUDED_COUNT or len(set(excluded)) != 12:
        raise RuntimeError("Cook source does not contain exactly 12 unique exclusions")

    outputs = spec["asset_outputs"]
    manifest_output = outputs["runtime_cook_manifest"]
    profile_output = outputs["dg_master_backend_profile"]
    manifest_package = manifest_output["package"]
    profile_package = profile_output["package"]
    manifest_object = manifest_output["object"]
    profile_object = profile_output["object"]
    if manifest_object != EXPECTED_MANIFEST_OBJECT \
            or profile_object != EXPECTED_DGMASTER_PROFILE_OBJECT:
        raise RuntimeError("Cook spec output object paths differ from frozen entry points")
    reserved = spec["reserved_optional_metahuman_entry_points"]
    expected_reserved = {
        "backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "backend_profile": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "assembled_visual_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "retarget_asset": EXPECTED_METAHUMAN_RETARGET_OBJECT,
    }
    if any(reserved.get(field) != value
           for field, value in expected_reserved.items()):
        raise RuntimeError("Reserved MetaHuman entry points differ from frozen constants")
    output_disk = {_uasset_path(manifest_package), _uasset_path(profile_package)}
    protected_before = _content_snapshot(output_disk)

    presence = {
        manifest_package: unreal.EditorAssetLibrary.does_asset_exist(manifest_package),
        profile_package: unreal.EditorAssetLibrary.does_asset_exist(profile_package),
    }
    if any(presence.values()) and not all(presence.values()):
        raise RuntimeError(
            "Refusing partial Session 8 cook asset state: " + str(presence))

    runtime_assets = _load_required_runtime_assets(runtime_paths)
    asset_save_calls = 0
    asset_factory_calls = 0
    if all(presence.values()):
        manifest = unreal.EditorAssetLibrary.load_asset(manifest_package)
        profile = unreal.EditorAssetLibrary.load_asset(profile_package)
        if not isinstance(manifest, unreal.DiscGolfRuntimeCookManifest):
            raise RuntimeError(f"Unexpected manifest type: {manifest}")
        if not isinstance(profile, unreal.DiscGolfAvatarBackendProfile):
            raise RuntimeError(f"Unexpected DGMaster profile type: {profile}")
        errors = _validate_profile(profile, profile_output, spec_sha)
        errors.extend(_validate_manifest(
            manifest, spec, runtime_paths, excluded, profile_object, spec_sha))
        if errors:
            raise RuntimeError(
                "Refusing to reconcile stale/non-owned Session 8 cook assets: "
                + "; ".join(errors))
        status = "PASS_ALREADY_CURRENT_NO_ASSET_WRITES"
    else:
        if not hasattr(unreal, "DiscGolfRuntimeCookManifest"):
            raise RuntimeError(
                "UDiscGolfRuntimeCookManifest is unavailable; build the editor module "
                "before the separately authorized author run")
        _ensure_folder(_asset_folder(profile_package))
        _ensure_folder(_asset_folder(manifest_package))

        profile = _create_data_asset(profile_package, unreal.DiscGolfAvatarBackendProfile)
        asset_factory_calls += 1
        profile.set_editor_property("backend_id", profile_output["backend_id"])
        profile.set_editor_property(
            "backend", _enum_member(unreal.DGAvatarBackend, "DG_MASTER"))
        profile.set_editor_property("visual_actor_class", None)
        profile.set_editor_property("retarget_asset", None)
        profile.set_editor_property("use_runtime_retargeting", False)
        profile.set_editor_property("visual_body_component_tag", "DGVisualBody")
        profile.set_editor_property("visual_head_component_tag", "DGVisualHead")
        profile.set_editor_property("preferred_quality_profile_id", "Prototype")
        profile.set_editor_property("allow_runtime_face_sculpting", False)
        _mark_owned(profile, "DGMASTER_FALLBACK_BACKEND_PROFILE", spec_sha)

        manifest = _create_data_asset(
            manifest_package, unreal.DiscGolfRuntimeCookManifest)
        asset_factory_calls += 1
        manifest.set_editor_property("manifest_id", spec["manifest_id"])
        manifest.set_editor_property("source_spec_relative_path", SOURCE_SPEC_RELATIVE)
        manifest.set_editor_property("source_spec_sha256", spec_sha)
        manifest.set_editor_property("content_status", spec["content_status"])
        manifest.set_editor_property("shipping_status", spec["shipping_status"])
        manifest.set_editor_property("authority", spec["authority"])
        manifest.set_editor_property("expected_runtime_package_count", 69)
        manifest.set_editor_property("expected_excluded_package_count", 12)
        manifest.set_editor_property("runtime_assets", runtime_assets)
        manifest.set_editor_property("avatar_backend_profiles", [profile])
        manifest.set_editor_property("explicitly_excluded_packages", excluded)
        _mark_owned(manifest, "RUNTIME_COOK_MANIFEST", spec_sha)

        errors = _validate_profile(profile, profile_output, spec_sha)
        errors.extend(_validate_manifest(
            manifest, spec, runtime_paths, excluded, profile_object, spec_sha))
        if errors:
            raise RuntimeError(
                "New Session 8 cook assets failed pre-save validation: "
                + "; ".join(errors))
        for asset in (profile, manifest):
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                    asset, only_if_is_dirty=False):
                raise RuntimeError(f"Failed to save Session 8 asset: {asset.get_path_name()}")
            asset_save_calls += 1
        status = "PASS_CREATED_AND_SAVED"

    protected_after = _content_snapshot(output_disk)
    if protected_before != protected_after:
        raise RuntimeError("Existing /Game/DiscGolf package bytes changed during cook authoring")
    output_hashes = {
        package: {
            "bytes": _uasset_path(package).stat().st_size,
            "sha256": _sha256(_uasset_path(package)),
        }
        for package in (manifest_package, profile_package)
    }
    result = {
        "schema": "DiscGolfTour.Session8CookAssetCreation.v1",
        "status": status,
        "source_spec": SOURCE_SPEC_RELATIVE,
        "source_spec_sha256": spec_sha,
        "runtime_package_count": len(runtime_paths),
        "excluded_package_count": len(excluded),
        "manifest_object": manifest_object,
        "dg_master_backend_profile_object": profile_object,
        "reserved_metahuman_entry_points": reserved,
        "availability_status": availability.get("status"),
        "metahuman_assets_created": METAHUMAN_ASSETS_CREATED,
        "vendor_assets_created": 0,
        "brand_art_created": 0,
        "protected_content_mutation": "NONE",
        "output_package_hashes": output_hashes,
        "asset_factory_calls": asset_factory_calls,
        "asset_save_calls": asset_save_calls,
        "asset_import_calls": 0,
        "asset_delete_calls": 0,
        "unreal_process_spawned_by_script": False,
        "ubt_launched": False,
        "cook_launched": False,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    unreal.log(
        "DG_SESSION8_COOK_ASSET_AUTHOR: " + status
        + f" runtime={len(runtime_paths)} excluded={len(excluded)} "
        + f"saves={asset_save_calls} metahuman_created={METAHUMAN_ASSETS_CREATED}")


if __name__ == "__main__":
    main()
