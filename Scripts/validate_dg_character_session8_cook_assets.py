"""Strict read-only UE validation for Session 8 cook-groundwork assets.

The validator proves exact 69-package inclusion, exact 12-package exclusion,
the DGMaster fallback entry point, provenance metadata, and AssetManager config.
It also proves that the reserved MetaHuman profile/actor/retarget entry points
remain absent while local availability is BLOCKED. It never creates, edits,
saves, imports, renames, deletes, checks out, compiles, builds, or cooks content.
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
DEFAULT_GAME_PATH = PROJECT_ROOT / "Config/DefaultGame.ini"
AVAILABILITY_AUDIT = (
    PROJECT_ROOT / "Scripts/audit_dg_character_session8_availability.py")
REPORT_PATH = (
    PROJECT_ROOT / "Saved/CharacterFramework/Session8CookAssetValidation.json")
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
EXPECTED_CORE_DATA_STATUS = (
    "MISSING_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT")
PACKAGE_SUFFIXES = {".uasset", ".uexp", ".ubulk", ".uptnl"}
EXPECTED_METADATA_BASE = {
    "DG_Session": "8",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_GROUNDWORK",
    "DG_SourceSpec": SOURCE_SPEC_RELATIVE,
    "DG_Authority": "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    "DG_ProductionArtApproved": "false",
    "DG_VendorAsset": "false",
}
CONFIG_TOKENS = (
    'PrimaryAssetType="DGRuntimeCookManifest"',
    'AssetBaseClass="/Script/DiscGolfTour.DiscGolfRuntimeCookManifest"',
    'Path="/Game/DiscGolf/Cook"',
    'PrimaryAssetType="DiscGolfAvatarBackendProfile"',
    'AssetBaseClass="/Script/DiscGolfCharacterFramework.DiscGolfAvatarBackendProfile"',
    'Path="/Game/DiscGolf/Characters/Avatar/Data"',
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _digest_lines(values: list[str]) -> str:
    payload = "".join(f"{value}\n" for value in sorted(values)).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


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


def _uasset_path(path: str) -> Path:
    package = _package_path(path)
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


def _content_snapshot() -> dict[str, dict]:
    root = PROJECT_ROOT / "Content/DiscGolf"
    result = {}
    if not root.is_dir():
        return result
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.suffix.casefold() in PACKAGE_SUFFIXES:
            relative = path.relative_to(PROJECT_ROOT).as_posix()
            result[relative] = {
                "bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
    return result


def _metadata_errors(asset, role: str, spec_sha: str) -> list[str]:
    errors = []
    expected = {
        **EXPECTED_METADATA_BASE,
        "DG_AssetRole": role,
        "DG_SourceSpecSHA256": spec_sha,
    }
    for key, value in expected.items():
        actual = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key))
        if actual != value:
            errors.append(
                f"{asset.get_path_name()} metadata {key}={actual!r}, expected {value!r}")
    return errors


def _load_asset(path: str, expected_type, errors: list[str]):
    asset = unreal.EditorAssetLibrary.load_asset(_package_path(path))
    if not isinstance(asset, expected_type):
        errors.append(
            f"Expected {expected_type.__name__} at {_object_path(path)}, found {asset}")
        return None
    return asset


def _validate_profile(profile, spec: dict, spec_sha: str, errors: list[str]) -> None:
    output = spec["asset_outputs"]["dg_master_backend_profile"]
    errors.extend(_metadata_errors(
        profile, "DGMASTER_FALLBACK_BACKEND_PROFILE", spec_sha))
    if str(profile.get_editor_property("backend_id")) != "dg_master":
        errors.append("DGMaster BackendId differs")
    if _enum_token(profile.get_editor_property("backend")) != "dgmaster":
        errors.append("DGMaster backend enum differs")
    if _soft_path(profile.get_editor_property("visual_actor_class")):
        errors.append("DGMaster fallback fabricated a VisualActorClass")
    if _soft_path(profile.get_editor_property("retarget_asset")):
        errors.append("DGMaster fallback fabricated a RetargetAsset")
    if bool(profile.get_editor_property("use_runtime_retargeting")):
        errors.append("DGMaster fallback requires runtime retargeting")
    if str(profile.get_editor_property("visual_body_component_tag")) != "DGVisualBody":
        errors.append("DGMaster visual body tag differs")
    if str(profile.get_editor_property("visual_head_component_tag")) != "DGVisualHead":
        errors.append("DGMaster visual head tag differs")
    if str(profile.get_editor_property("preferred_quality_profile_id")) != "Prototype":
        errors.append("DGMaster preferred quality profile differs")
    if bool(profile.get_editor_property("allow_runtime_face_sculpting")):
        errors.append("DGMaster fallback enables unsupported face sculpting")
    if output.get("object") != EXPECTED_DGMASTER_PROFILE_OBJECT:
        errors.append("Cook spec DGMaster object path differs from frozen entry point")


def _validate_manifest(
        manifest, profile, spec: dict, spec_sha: str,
        runtime: list[str], excluded: list[str], errors: list[str]) -> None:
    errors.extend(_metadata_errors(manifest, "RUNTIME_COOK_MANIFEST", spec_sha))
    expected_strings = {
        "manifest_id": "dg_runtime_v1",
        "source_spec_relative_path": SOURCE_SPEC_RELATIVE,
        "source_spec_sha256": spec_sha,
        "content_status": (
            "ACCEPTED_TECHNICAL_PIPELINE_WITH_NON_PRODUCTION_PROXY_VISUALS"),
        "shipping_status": "DO_NOT_CLAIM_SHIPPING_ART_APPROVAL",
        "authority": "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    }
    for field, expected in expected_strings.items():
        actual = str(manifest.get_editor_property(field))
        if actual != expected:
            errors.append(f"Manifest {field}={actual!r}, expected {expected!r}")
    if int(manifest.get_editor_property("expected_runtime_package_count")) != 69:
        errors.append("Manifest expected runtime package count is not 69")
    if int(manifest.get_editor_property("expected_excluded_package_count")) != 12:
        errors.append("Manifest expected excluded package count is not 12")

    runtime_values = list(manifest.get_editor_property("runtime_assets"))
    actual_runtime_objects = [_soft_path(value) for value in runtime_values]
    expected_runtime_objects = [_object_path(package) for package in runtime]
    if actual_runtime_objects != expected_runtime_objects:
        errors.append("Manifest runtime soft-reference list/order differs from frozen 69")
    actual_runtime_packages = [_package_path(path) for path in actual_runtime_objects]
    if len(actual_runtime_packages) != 69 or len(set(actual_runtime_packages)) != 69:
        errors.append("Manifest does not contain exactly 69 unique runtime packages")
    if set(actual_runtime_packages) & set(excluded):
        errors.append("Excluded test/synthetic package leaked into runtime references")

    actual_profiles = [_soft_path(value) for value in
                       manifest.get_editor_property("avatar_backend_profiles")]
    if actual_profiles != [EXPECTED_DGMASTER_PROFILE_OBJECT]:
        errors.append("Manifest backend profile list is not exact DGMaster-only entry")
    elif profile.get_path_name() != EXPECTED_DGMASTER_PROFILE_OBJECT:
        errors.append("Loaded DGMaster profile object path differs")
    actual_excluded = [str(value) for value in
                       manifest.get_editor_property("explicitly_excluded_packages")]
    if actual_excluded != excluded:
        errors.append("Manifest explicit exclusion list/order differs from frozen 12")

    for package in runtime + excluded:
        if not unreal.EditorAssetLibrary.does_asset_exist(package):
            errors.append(f"Frozen package is absent from Asset Registry: {package}")
    if spec["asset_outputs"]["runtime_cook_manifest"].get("object") \
            != EXPECTED_MANIFEST_OBJECT:
        errors.append("Cook spec manifest object path differs from frozen entry point")


def validate() -> dict:
    errors: list[str] = []
    spec = _load_spec()
    spec_sha = _sha256(SPEC_PATH)
    if spec_sha != FROZEN_SOURCE_SPEC_SHA256:
        errors.append(
            f"Frozen source spec hash differs: {spec_sha} != "
            f"{FROZEN_SOURCE_SPEC_SHA256}")
    runtime = _flatten(spec, "runtime_package_groups")
    excluded = _flatten(spec, "excluded_package_groups")
    before = _content_snapshot()

    availability_namespace = runpy.run_path(
        str(AVAILABILITY_AUDIT), run_name="dg_session8_strict_availability")
    availability = availability_namespace["audit"](write_report=True)
    if availability.get("integrity_errors"):
        errors.extend(
            f"Availability integrity: {value}"
            for value in availability["integrity_errors"])
    if availability.get("status") != "BLOCKED":
        errors.append(
            f"Availability status changed from frozen BLOCKED: {availability.get('status')}")
    core_status = availability.get(
        "metahuman_creator_core_data", {}).get("status")
    if core_status != EXPECTED_CORE_DATA_STATUS:
        errors.append(
            f"MetaHuman Core Data status changed: {core_status} != {EXPECTED_CORE_DATA_STATUS}")
    if availability.get("cook_source_inventory", {}).get("status") != "PASS":
        errors.append("Filesystem cook-source inventory did not pass")
    if len(runtime) != 69 or len(set(runtime)) != 69:
        errors.append("Source spec does not contain exactly 69 unique runtime packages")
    if len(excluded) != 12 or len(set(excluded)) != 12:
        errors.append("Source spec does not contain exactly 12 unique exclusions")
    if set(runtime) & set(excluded):
        errors.append("Source spec runtime and exclusion sets overlap")

    config_text = DEFAULT_GAME_PATH.read_text(encoding="utf-8")
    for token in CONFIG_TOKENS:
        if token not in config_text:
            errors.append(f"DefaultGame.ini missing AssetManager token: {token}")

    manifest = _load_asset(
        EXPECTED_MANIFEST_OBJECT, unreal.DiscGolfRuntimeCookManifest, errors)
    profile = _load_asset(
        EXPECTED_DGMASTER_PROFILE_OBJECT, unreal.DiscGolfAvatarBackendProfile, errors)
    if profile:
        _validate_profile(profile, spec, spec_sha, errors)
    if manifest and profile:
        _validate_manifest(
            manifest, profile, spec, spec_sha, runtime, excluded, errors)

    reserved = spec.get("reserved_optional_metahuman_entry_points", {})
    if reserved.get("backend_id") != EXPECTED_METAHUMAN_BACKEND_ID:
        errors.append("Reserved MetaHuman backend stable ID differs")
    expected_reserved = {
        "backend_profile": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "assembled_visual_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "retarget_asset": EXPECTED_METAHUMAN_RETARGET_OBJECT,
    }
    for field, expected in expected_reserved.items():
        if reserved.get(field) != expected:
            errors.append(f"Reserved MetaHuman {field} differs")
    reserved_presence = {
        name: unreal.EditorAssetLibrary.does_asset_exist(_package_path(path))
        for name, path in expected_reserved.items()
    }
    if any(reserved_presence.values()):
        errors.append(
            "Reserved MetaHuman entry point exists despite BLOCKED/no-fabrication state: "
            + str(reserved_presence))

    after = _content_snapshot()
    disk_mutation = before != after
    if disk_mutation:
        errors.append("Strict cook asset validation observed a content package mutation")
    output_hashes = {}
    for path in (EXPECTED_MANIFEST_OBJECT, EXPECTED_DGMASTER_PROFILE_OBJECT):
        disk_path = _uasset_path(path)
        if disk_path.is_file():
            output_hashes[_package_path(path)] = {
                "bytes": disk_path.stat().st_size,
                "sha256": _sha256(disk_path),
            }

    result = {
        "schema": "DiscGolfTour.Session8CookAssetValidation.v1",
        "status": "PASS_NO_DISK_MUTATION" if not errors else "FAIL",
        "availability_status": availability.get("status"),
        "core_data_status": core_status,
        "source_spec_sha256": spec_sha,
        "runtime_package_count": len(runtime),
        "runtime_package_list_sha256": _digest_lines(runtime),
        "excluded_package_count": len(excluded),
        "excluded_package_list_sha256": _digest_lines(excluded),
        "manifest_object": EXPECTED_MANIFEST_OBJECT,
        "dg_master_backend_profile_object": EXPECTED_DGMASTER_PROFILE_OBJECT,
        "metahuman_backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "reserved_metahuman_profile_object": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "reserved_metahuman_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "reserved_metahuman_retarget_object": EXPECTED_METAHUMAN_RETARGET_OBJECT,
        "reserved_metahuman_entry_point_presence": reserved_presence,
        "metahuman_assets_created": 0,
        "output_package_hashes": output_hashes,
        "content_snapshot_before": before,
        "content_snapshot_after": after,
        "disk_mutation": "DETECTED" if disk_mutation else "NONE",
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "asset_factory_calls": 0,
        "asset_delete_calls": 0,
        "ubt_launched": False,
        "cook_launched": False,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    result = validate()
    if result["status"] != "PASS_NO_DISK_MUTATION":
        raise RuntimeError(
            "Session 8 strict cook validation failed: "
            + "; ".join(result["errors"]))
    unreal.log(
        "DG_SESSION8_COOK_ASSET_VALIDATION: PASS_NO_DISK_MUTATION "
        f"runtime={result['runtime_package_count']} "
        f"excluded={result['excluded_package_count']} "
        f"core_data={result['core_data_status']}")


if __name__ == "__main__":
    main()
