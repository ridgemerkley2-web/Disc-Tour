"""Strict read-only UE validation for the Session 8B cook contract.

The validator preserves the exact legacy 69/12 contract, then independently
proves the exact 264-package accepted MetaHuman inventory, two backend profiles,
and the editor-only source-MHC exclusion. It never creates, edits,
saves, imports, renames, deletes, checks out, compiles, builds, or cooks content.
"""

from __future__ import annotations

import hashlib
import json
import re
import uuid
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_PATH = PROJECT_ROOT / "Config/DG_RuntimeCookManifest.json"
DEFAULT_GAME_PATH = PROJECT_ROOT / "Config/DefaultGame.ini"
SCHEMA = "DiscGolfTour.Session8BCookAssetValidation.v1"
VALIDATE_SWITCH = "-DGSession8BCookAssetValidate"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BCookAssetValidation.json")
PRODUCTION_SAVE = PROJECT_ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
ACCEPTED_EXTERNAL_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav")
EXPECTED_ACCEPTED_SAVE_BYTES = 5212
EXPECTED_ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14")
SOURCE_SPEC_RELATIVE = "Config/DG_RuntimeCookManifest.json"
FROZEN_SOURCE_SPEC_SHA256 = (
    "1B37EDF5A13EFDEA9243A97E2E733477AA4B6D6394AC82CAB6FA4FA6651BCB09")
LEGACY_SOURCE_SPEC_SHA256 = (
    "E3F6FEAB2AFAFFE414D0B7F859FA3107490B3AC19EC6FF78DAA7F9EA90888AF7")
EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256 = (
    "003A7683069A2353819421CD71ECA82A2A0F8AE2E7786F56E04EA18DB11AB3B9")

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
EXPECTED_METAHUMAN_TARGET_IK_OBJECT = (
    "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig")
EXPECTED_SOURCE_MHC_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default")
EXPECTED_S8B_METADATA_BASE = {
    "DG_Session": "8B",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_CLOSURE",
    "DG_SourceSpec": SOURCE_SPEC_RELATIVE,
    "DG_Authority": "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    "DG_ProductionArtApproved": "false",
    "DG_VendorAsset": "false",
}
EXPECTED_LEGACY_DGMASTER_METADATA_BASE = {
    **EXPECTED_S8B_METADATA_BASE,
    "DG_Session": "8",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_GROUNDWORK",
}
EXPECTED_INVENTORY_SEMANTICS = (
    "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH")
EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT = 340
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


def _record(path: Path) -> dict:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": _sha256(path),
    }


def _tree_snapshot(root: Path) -> dict[str, dict]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): _record(path)
        for path in sorted(root.rglob("*")) if path.is_file()
    }


def _directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*") if path.is_dir()
    }


def _exact_switch(command_line: str, switch: str) -> bool:
    return bool(re.search(
        rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line))


def _command_values(command_line: str, name: str) -> list[str]:
    pattern = re.compile(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))')
    return [quoted or plain for quoted, plain in pattern.findall(command_line)]


def _validate_command_line(command_line: str) -> Path:
    required = (VALIDATE_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required
               if not _exact_switch(command_line, switch)]
    if missing:
        raise RuntimeError(f"Required strict-validation switches absent: {missing}")
    run_values = _command_values(command_line, "run")
    if len(run_values) != 1 or run_values[0].casefold() != "pythonscript":
        raise RuntimeError(
            "Strict cook validator requires exactly one -run=PythonScript")
    values = _command_values(command_line, "UserDir")
    if len(values) != 1:
        raise RuntimeError(
            "Command line must contain exactly one absolute -UserDir=<UUID path>")
    user_dir = Path(values[0]).resolve()
    expected_parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(expected_parent)
    except ValueError as exc:
        raise RuntimeError(
            f"UserDir is outside the Session8B external test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise RuntimeError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise RuntimeError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold() or not user_dir.is_dir():
        raise RuntimeError("UserDir UUID is not canonical and active")
    try:
        user_dir.relative_to(PROJECT_ROOT)
    except ValueError:
        pass
    else:
        raise RuntimeError("UserDir must remain external to the project")
    return user_dir


def _load_spec() -> dict:
    return json.loads(SPEC_PATH.read_text(encoding="utf-8"))


def _flatten(spec: dict, field: str) -> list[str]:
    return [package for group in spec[field] for package in group["packages"]]


def _metahuman_runtime_paths(spec: dict) -> list[str]:
    errors = []
    inventory = spec["metahuman_runtime_contract"]["runtime_inventory"]
    if inventory.get("inventory_semantics") != EXPECTED_INVENTORY_SEMANTICS \
            or inventory.get("fresh_validation_transitive_runtime_dependency_count") \
                != EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT \
            or inventory.get("transitive_dependency_evidence_status") != \
                "FRESH2_SOURCE_VALIDATION_ONLY_REQUIRES_PACKAGED_IOSTORE_REPROOF":
        errors.append("MetaHuman explicit-root/transitive-dependency semantics differ")
    paths: list[str] = []
    content_root = (PROJECT_ROOT / "Content").resolve()
    for group in inventory["directory_groups"]:
        root = (PROJECT_ROOT / group["disk_relative_root"]).resolve()
        if content_root not in root.parents or not root.is_dir():
            errors.append(f"MetaHuman inventory root is unavailable/unsafe: {root}")
            continue
        group_paths = sorted(
            "/Game/" + path.relative_to(content_root).as_posix()[:-len(".uasset")]
            for path in root.rglob("*.uasset") if path.is_file())
        if len(group_paths) != group["expected_package_count"] \
                or len(set(group_paths)) != len(group_paths) \
                or _digest_lines(group_paths) != group["package_list_sha256"]:
            errors.append(f"MetaHuman directory inventory differs: {group['name']}")
        paths.extend(group_paths)
    explicit = list(inventory["explicit_packages"])
    if _digest_lines(explicit) != inventory["explicit_package_list_sha256"]:
        errors.append("MetaHuman explicit package inventory hash differs")
    paths = sorted(paths + explicit)
    if len(paths) != 264 or len(set(paths)) != 264 \
            or inventory["expected_package_count"] != 264 \
            or inventory["package_list_sha256"] \
                != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256 \
            or _digest_lines(paths) != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256:
        errors.append("MetaHuman runtime inventory is not the frozen 264-package set")
    if errors:
        raise RuntimeError("; ".join(errors))
    return paths


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
    match = re.search(r"(/[A-Za-z0-9_]+/[^'\" ]+)", text)
    return match.group(1) if match else text


def _enum_token(value) -> str:
    name = getattr(value, "name", None)
    if callable(name):
        name = name()
    text = str(name) if name else str(value).rsplit(".", 1)[-1].split(":", 1)[0]
    return re.sub(r"[^A-Za-z0-9]", "", text).casefold()


def _content_snapshot() -> dict[str, dict]:
    return _tree_snapshot(PROJECT_ROOT / "Content")


def _metadata_errors(
        asset, role: str, spec_sha: str,
        base: dict[str, str] = EXPECTED_S8B_METADATA_BASE) -> list[str]:
    errors = []
    expected = {
        **base,
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
        profile, "DGMASTER_FALLBACK_BACKEND_PROFILE", spec_sha,
        EXPECTED_LEGACY_DGMASTER_METADATA_BASE))
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


def _validate_metahuman_profile(
        profile, spec: dict, errors: list[str]) -> None:
    output = spec["asset_outputs"]["metahuman_default_backend_profile"]
    profile_disk = _uasset_path(output["object"])
    if not profile_disk.is_file():
        errors.append("MetaHuman profile package file is absent")
    else:
        if profile_disk.stat().st_size != output.get("accepted_uasset_bytes"):
            errors.append("MetaHuman profile package byte count differs")
        if _sha256(profile_disk) != output.get("accepted_uasset_sha256"):
            errors.append("MetaHuman profile package SHA256 differs")
    if profile.get_path_name() != EXPECTED_METAHUMAN_PROFILE_OBJECT:
        errors.append("MetaHuman profile object path differs")
    if str(profile.get_editor_property("backend_id")) != "metahuman_assembled":
        errors.append("MetaHuman BackendId differs")
    if _enum_token(profile.get_editor_property("backend")) != "metahumanpreset":
        errors.append("MetaHuman backend enum differs")
    if _enum_token(profile.get_editor_property("meta_human_runtime_mode")) \
            != "shippingsafeassembled":
        errors.append("MetaHuman runtime mode differs")
    if _soft_path(profile.get_editor_property("visual_actor_class")) \
            != EXPECTED_METAHUMAN_ACTOR_CLASS:
        errors.append("MetaHuman wrapper class differs")
    if _soft_path(profile.get_editor_property("retarget_asset")) \
            != EXPECTED_METAHUMAN_RETARGET_OBJECT:
        errors.append("MetaHuman retarget asset differs")
    if not bool(profile.get_editor_property("use_runtime_retargeting")):
        errors.append("MetaHuman runtime retargeting is disabled")
    if str(profile.get_editor_property("visual_body_component_tag")) \
            != "DGVisualBody" \
            or str(profile.get_editor_property("visual_head_component_tag")) \
            != "DGVisualHead":
        errors.append("MetaHuman body/head tags differ")
    if str(profile.get_editor_property("preferred_quality_profile_id")) \
            != "GameplayPerformance":
        errors.append(
            "MetaHuman quality ID is not GameplayPerformance for Optimized Medium")
    if bool(profile.get_editor_property("allow_runtime_face_sculpting")):
        errors.append("MetaHuman profile enables unsupported runtime sculpting")
    expected_fields = {
        "object": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "visual_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "retarget_asset": EXPECTED_METAHUMAN_RETARGET_OBJECT,
        "preferred_quality_profile_id": "GameplayPerformance",
    }
    for field, expected in expected_fields.items():
        if output.get(field) != expected:
            errors.append(f"Cook spec MetaHuman profile {field} differs")


def _validate_manifest(
        manifest, dgmaster_profile, metahuman_profile, spec: dict, spec_sha: str,
        runtime: list[str], metahuman_runtime: list[str], excluded: list[str],
        excluded_metahuman: list[str], errors: list[str]) -> None:
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
    if int(manifest.get_editor_property(
            "expected_meta_human_runtime_package_count")) != 264:
        errors.append("Manifest expected MetaHuman package count is not 264")
    if int(manifest.get_editor_property(
            "expected_avatar_backend_profile_count")) != 2:
        errors.append("Manifest expected backend profile count is not 2")
    if int(manifest.get_editor_property("expected_excluded_package_count")) != 12:
        errors.append("Manifest expected excluded package count is not 12")
    if int(manifest.get_editor_property(
            "expected_excluded_meta_human_package_count")) != 1:
        errors.append("Manifest expected MetaHuman exclusion count is not 1")
    if str(manifest.get_editor_property(
            "meta_human_runtime_package_list_sha256")) \
            != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256:
        errors.append("Manifest MetaHuman package-list hash differs")

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

    metahuman_values = list(
        manifest.get_editor_property("meta_human_runtime_assets"))
    actual_metahuman_objects = [_soft_path(value) for value in metahuman_values]
    actual_metahuman_packages = [
        _package_path(path) for path in actual_metahuman_objects]
    if actual_metahuman_packages != metahuman_runtime:
        errors.append("Manifest MetaHuman list/order differs from frozen 264")
    if len(actual_metahuman_packages) != 264 \
            or len(set(actual_metahuman_packages)) != 264 \
            or _digest_lines(actual_metahuman_packages) \
                != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256:
        errors.append("Manifest MetaHuman package count/set/hash differs")

    actual_profiles = [_soft_path(value) for value in
                       manifest.get_editor_property("avatar_backend_profiles")]
    expected_profiles = [
        EXPECTED_DGMASTER_PROFILE_OBJECT, EXPECTED_METAHUMAN_PROFILE_OBJECT]
    if actual_profiles != expected_profiles:
        errors.append("Manifest backend profiles are not exact DGMaster/MetaHuman entries")
    elif dgmaster_profile.get_path_name() != EXPECTED_DGMASTER_PROFILE_OBJECT \
            or metahuman_profile.get_path_name() != EXPECTED_METAHUMAN_PROFILE_OBJECT:
        errors.append("Loaded backend profile object paths differ")
    actual_excluded = [str(value) for value in
                       manifest.get_editor_property("explicitly_excluded_packages")]
    if actual_excluded != excluded:
        errors.append("Manifest explicit exclusion list/order differs from frozen 12")

    actual_excluded_metahuman = [str(value) for value in manifest.get_editor_property(
        "explicitly_excluded_meta_human_packages")]
    if actual_excluded_metahuman != excluded_metahuman:
        errors.append("Manifest source-MHC exclusion differs")

    for package in runtime + excluded + metahuman_runtime + excluded_metahuman:
        if not unreal.EditorAssetLibrary.does_asset_exist(package):
            errors.append(f"Frozen package is absent from Asset Registry: {package}")
    if spec["asset_outputs"]["runtime_cook_manifest"].get("object") \
            != EXPECTED_MANIFEST_OBJECT:
        errors.append("Cook spec manifest object path differs from frozen entry point")


def validate() -> dict:
    errors: list[str] = []
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir = _validate_command_line(command_line)
    report_path = user_dir / REPORT_RELATIVE
    if report_path.exists():
        raise RuntimeError(f"Fresh UUID already contains strict report: {report_path}")
    external_saves_before = sorted(
        str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
    if external_saves_before:
        raise RuntimeError(
            f"Fresh external UserDir already contains saves: {external_saves_before}")
    content_root = PROJECT_ROOT / "Content"
    savegames_root = PROJECT_ROOT / "Saved/SaveGames"
    spec = _load_spec()
    spec_sha = _sha256(SPEC_PATH)
    if spec_sha != FROZEN_SOURCE_SPEC_SHA256:
        errors.append(
            f"Frozen source spec hash differs: {spec_sha} != "
            f"{FROZEN_SOURCE_SPEC_SHA256}")
    runtime = _flatten(spec, "runtime_package_groups")
    try:
        metahuman_runtime = _metahuman_runtime_paths(spec)
    except RuntimeError as exc:
        errors.append(str(exc))
        metahuman_runtime = []
    excluded = _flatten(spec, "excluded_package_groups")
    metahuman_contract = spec.get("metahuman_runtime_contract", {})
    excluded_metahuman = list(
        metahuman_contract.get("explicitly_excluded_packages", []))
    before = _content_snapshot()
    content_directories_before = _directory_snapshot(content_root)
    savegames_before = _tree_snapshot(savegames_root)
    savegame_directories_before = _directory_snapshot(savegames_root)
    production_before = _record(PRODUCTION_SAVE)
    backup_before = _record(ACCEPTED_EXTERNAL_BACKUP)
    expected_save = {
        "exists": True,
        "bytes": EXPECTED_ACCEPTED_SAVE_BYTES,
        "sha256": EXPECTED_ACCEPTED_SAVE_SHA256,
    }
    if production_before != expected_save or backup_before != expected_save:
        errors.append(
            "Production save or accepted backup is not the frozen A999 baseline")
    elif PRODUCTION_SAVE.read_bytes() != ACCEPTED_EXTERNAL_BACKUP.read_bytes():
        errors.append("Production save and accepted backup are not byte-identical")

    availability_gate = "SEPARATE_S8B_AUDIT_NOT_INVOKED"
    core_status = "SEPARATE_S8B_AUDIT_NOT_INVOKED"
    if len(runtime) != 69 or len(set(runtime)) != 69:
        errors.append("Source spec does not contain exactly 69 unique runtime packages")
    if len(excluded) != 12 or len(set(excluded)) != 12:
        errors.append("Source spec does not contain exactly 12 unique exclusions")
    if len(metahuman_runtime) != 264 or len(set(metahuman_runtime)) != 264:
        errors.append("Source spec does not identify exactly 264 MetaHuman packages")
    if excluded_metahuman != [EXPECTED_SOURCE_MHC_PACKAGE]:
        errors.append("Source spec does not identify the exact source MHC exclusion")
    if set(runtime) & set(excluded):
        errors.append("Source spec runtime and exclusion sets overlap")

    config_text = DEFAULT_GAME_PATH.read_text(encoding="utf-8")
    for token in CONFIG_TOKENS:
        if token not in config_text:
            errors.append(f"DefaultGame.ini missing AssetManager token: {token}")

    manifest = _load_asset(
        EXPECTED_MANIFEST_OBJECT, unreal.DiscGolfRuntimeCookManifest, errors)
    dgmaster_profile = _load_asset(
        EXPECTED_DGMASTER_PROFILE_OBJECT, unreal.DiscGolfAvatarBackendProfile, errors)
    metahuman_profile = _load_asset(
        EXPECTED_METAHUMAN_PROFILE_OBJECT,
        unreal.DiscGolfAvatarBackendProfile, errors)
    if dgmaster_profile:
        _validate_profile(
            dgmaster_profile, spec, LEGACY_SOURCE_SPEC_SHA256, errors)
    if metahuman_profile:
        _validate_metahuman_profile(metahuman_profile, spec, errors)
    if manifest and dgmaster_profile and metahuman_profile:
        _validate_manifest(
            manifest, dgmaster_profile, metahuman_profile, spec, spec_sha,
            runtime, metahuman_runtime, excluded, excluded_metahuman, errors)

    expected_metahuman = {
        "backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "backend_profile": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "assembled_visual_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "retarget_asset": EXPECTED_METAHUMAN_RETARGET_OBJECT,
        "target_ik_rig": EXPECTED_METAHUMAN_TARGET_IK_OBJECT,
    }
    for field, expected in expected_metahuman.items():
        if metahuman_contract.get(field) != expected:
            errors.append(f"Accepted MetaHuman {field} differs")
    metahuman_presence = {
        name: unreal.EditorAssetLibrary.does_asset_exist(_package_path(path))
        for name, path in expected_metahuman.items() if name != "backend_id"
    }
    if not all(metahuman_presence.values()):
        errors.append(
            "Accepted MetaHuman entry point is absent: " + str(metahuman_presence))

    actual_project_metahuman_packages = sorted(set(
        _package_path(_soft_path(path))
        for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/DiscGolf/Characters/MetaHuman",
            recursive=True, include_folder=False)))
    expected_project_metahuman_packages = sorted(
        [path for path in metahuman_runtime if path.startswith("/Game/")]
        + excluded_metahuman)
    if actual_project_metahuman_packages != expected_project_metahuman_packages:
        errors.append(
            "Project MetaHuman Asset Registry set contains missing/unrelated packages")

    after = _content_snapshot()
    content_directories_after = _directory_snapshot(content_root)
    savegames_after = _tree_snapshot(savegames_root)
    savegame_directories_after = _directory_snapshot(savegames_root)
    production_after = _record(PRODUCTION_SAVE)
    backup_after = _record(ACCEPTED_EXTERNAL_BACKUP)
    external_saves_after = sorted(
        str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
    disk_mutation = before != after \
        or content_directories_before != content_directories_after
    if disk_mutation:
        errors.append(
            "Strict cook validation observed a full Content file/topology mutation")
    if savegames_before != savegames_after \
            or savegame_directories_before != savegame_directories_after:
        errors.append("Strict cook validation observed a SaveGames file/topology mutation")
    if production_before != production_after or backup_before != backup_after:
        errors.append("Production save or accepted external backup changed")
    if external_saves_after:
        errors.append(
            f"Strict cook validation observed external saves: {external_saves_after}")
    output_hashes = {}
    for path in (EXPECTED_MANIFEST_OBJECT, EXPECTED_DGMASTER_PROFILE_OBJECT,
                 EXPECTED_METAHUMAN_PROFILE_OBJECT):
        disk_path = _uasset_path(path)
        if disk_path.is_file():
            output_hashes[_package_path(path)] = {
                "bytes": disk_path.stat().st_size,
                "sha256": _sha256(disk_path),
            }

    result = {
        "schema": SCHEMA,
        "status": "PASS_NO_DISK_MUTATION" if not errors else "FAIL",
        "availability_gate": availability_gate,
        "validation_switch": VALIDATE_SWITCH,
        "no_portal_switch": NO_PORTAL_SWITCH,
        "external_user_dir": str(user_dir),
        "report_path": str(report_path),
        "core_data_status": core_status,
        "source_spec_sha256": spec_sha,
        "runtime_package_count": len(runtime),
        "runtime_package_list_sha256": _digest_lines(runtime),
        "metahuman_runtime_package_count": len(metahuman_runtime),
        "metahuman_explicit_cook_root_package_count": len(
            metahuman_runtime),
        "metahuman_inventory_semantics": EXPECTED_INVENTORY_SEMANTICS,
        "fresh_validation_transitive_runtime_dependency_count":
            EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT,
        "packaged_transitive_dependency_reproof": "REQUIRED",
        "metahuman_runtime_package_list_sha256": _digest_lines(
            metahuman_runtime),
        "backend_profile_count": 2,
        "excluded_package_count": len(excluded),
        "excluded_package_list_sha256": _digest_lines(excluded),
        "excluded_metahuman_package_count": len(excluded_metahuman),
        "manifest_object": EXPECTED_MANIFEST_OBJECT,
        "dg_master_backend_profile_object": EXPECTED_DGMASTER_PROFILE_OBJECT,
        "metahuman_backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
        "metahuman_profile_object": EXPECTED_METAHUMAN_PROFILE_OBJECT,
        "metahuman_preferred_quality_profile_id": "GameplayPerformance",
        "metahuman_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
        "metahuman_retarget_object": EXPECTED_METAHUMAN_RETARGET_OBJECT,
        "metahuman_target_ik_object": EXPECTED_METAHUMAN_TARGET_IK_OBJECT,
        "metahuman_entry_point_presence": metahuman_presence,
        "source_mhc_package": EXPECTED_SOURCE_MHC_PACKAGE,
        "manifest_metadata_session": "8B",
        "dg_master_profile_metadata_session": "8_LEGACY_PRESERVED",
        "project_metahuman_package_count": len(
            actual_project_metahuman_packages),
        "project_metahuman_package_list_sha256": _digest_lines(
            actual_project_metahuman_packages),
        "metahuman_assets_created": 0,
        "output_package_hashes": output_hashes,
        "content_snapshot_before": before,
        "content_snapshot_after": after,
        "content_directories_before": sorted(content_directories_before),
        "content_directories_after": sorted(content_directories_after),
        "savegames_before": savegames_before,
        "savegames_after": savegames_after,
        "savegame_directories_before": sorted(savegame_directories_before),
        "savegame_directories_after": sorted(savegame_directories_after),
        "production_save_before": production_before,
        "production_save_after": production_after,
        "accepted_external_backup_before": backup_before,
        "accepted_external_backup_after": backup_after,
        "external_save_files_before": external_saves_before,
        "external_save_files_after": external_saves_after,
        "content_files_unchanged": before == after,
        "content_directories_unchanged": (
            content_directories_before == content_directories_after),
        "savegames_files_unchanged": savegames_before == savegames_after,
        "savegame_directories_unchanged": (
            savegame_directories_before == savegame_directories_after),
        "disk_mutation": "DETECTED" if disk_mutation else "NONE",
        "asset_save_calls": 0,
        "asset_import_calls": 0,
        "asset_factory_calls": 0,
        "asset_delete_calls": 0,
        "ubt_launched": False,
        "cook_launched": False,
        "errors": errors,
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8", newline="\n")
    return result


def main() -> None:
    result = validate()
    if result["status"] != "PASS_NO_DISK_MUTATION":
        raise RuntimeError(
            "Session 8B strict cook validation failed: "
            + "; ".join(result["errors"]))
    unreal.log(
        "DG_SESSION8B_COOK_ASSET_VALIDATION: PASS_NO_DISK_MUTATION "
        f"runtime={result['runtime_package_count']} "
        f"metahuman_runtime={result['metahuman_runtime_package_count']} "
        f"profiles={result['backend_profile_count']} "
        f"excluded={result['excluded_package_count']} "
        f"core_data={result['core_data_status']}")


if __name__ == "__main__":
    main()
