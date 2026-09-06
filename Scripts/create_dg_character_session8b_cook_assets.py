"""Transactionally refresh the existing Session 8B runtime cook manifest.

This utility requires a compiled UE 5.8 editor and a separately authorized run.
It refuses missing or partial state and mutates only the already-owned manifest.
It never creates or edits either backend profile, the assembled actor,
retargeter, assembly output, vendor asset, gameplay authority, save object, or
pawn wiring.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_PATH = PROJECT_ROOT / "Config/DG_RuntimeCookManifest.json"
SCHEMA = "DiscGolfTour.Session8BCookAssetCreation.v1"
AUTHOR_SWITCH = "-DGSession8BCookAssetAuthor"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BCookAssetCreation.json")
PRODUCTION_SAVE = PROJECT_ROOT / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
ACCEPTED_EXTERNAL_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted\DiscGolfTour_Profile_0_Session7_Accepted.sav")
EXPECTED_ACCEPTED_SAVE_BYTES = 5212
EXPECTED_ACCEPTED_SAVE_SHA256 = (
    "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14")
EXPECTED_RUNTIME_COUNT = 69
EXPECTED_METAHUMAN_RUNTIME_COUNT = 264
EXPECTED_METAHUMAN_COMMON_COUNT = 205
EXPECTED_METAHUMAN_GENERATED_COUNT = 56
EXPECTED_BACKEND_PROFILE_COUNT = 2
EXPECTED_EXCLUDED_COUNT = 12
EXPECTED_EXCLUDED_METAHUMAN_COUNT = 1
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
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
S8B_METADATA_BASE = {
    "DG_Session": "8B",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_CLOSURE",
    "DG_SourceSpec": SOURCE_SPEC_RELATIVE,
    "DG_Authority": "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY",
    "DG_ProductionArtApproved": "false",
    "DG_VendorAsset": "false",
}
LEGACY_DGMASTER_METADATA_BASE = {
    **S8B_METADATA_BASE,
    "DG_Session": "8",
    "DG_SourceKind": "PROJECT_LOCAL_COOK_GROUNDWORK",
}
EXPECTED_INVENTORY_SEMANTICS = (
    "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH")
EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT = 340


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _digest_lines(values: list[str]) -> str:
    payload = "".join(f"{value}\n" for value in sorted(values)).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": _sha256(path),
    }


def _tree_snapshot(
        root: Path, excluded: set[Path] | None = None) -> dict[str, dict[str, Any]]:
    excluded_resolved = {path.resolve() for path in (excluded or set())}
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): _record(path)
        for path in sorted(root.rglob("*"))
        if path.is_file() and path.resolve() not in excluded_resolved
    }


def _directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*") if path.is_dir()
    }


def _snapshot_summary(snapshot: dict[str, dict[str, Any]]) -> dict[str, Any]:
    payload = json.dumps(
        snapshot, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return {
        "file_count": len(snapshot),
        "bytes": sum(int(value.get("bytes", 0)) for value in snapshot.values()),
        "manifest_sha256": hashlib.sha256(payload).hexdigest().upper(),
    }


def _directory_summary(directories: set[str]) -> dict[str, Any]:
    payload = "".join(f"{value}\n" for value in sorted(directories)).encode(
        "utf-8")
    return {
        "directory_count": len(directories),
        "manifest_sha256": hashlib.sha256(payload).hexdigest().upper(),
    }


def _exact_switch(command_line: str, switch: str) -> bool:
    return bool(re.search(
        rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line))


def _command_values(command_line: str, name: str) -> list[str]:
    pattern = re.compile(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))')
    return [quoted or plain for quoted, plain in pattern.findall(command_line)]


def _validate_command_line(command_line: str) -> Path:
    required = (AUTHOR_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required
               if not _exact_switch(command_line, switch)]
    if missing:
        raise RuntimeError(f"Required author switches are absent: {missing}")
    run_values = _command_values(command_line, "run")
    if len(run_values) != 1 or run_values[0].casefold() != "pythonscript":
        raise RuntimeError(
            "Cook-manifest author requires exactly one -run=PythonScript")
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
    if str(parsed) != relative.name.casefold():
        raise RuntimeError("UserDir UUID is not canonical")
    try:
        user_dir.relative_to(PROJECT_ROOT)
    except ValueError:
        pass
    else:
        raise RuntimeError("UserDir must remain external to the project")
    if not user_dir.is_dir():
        raise RuntimeError("Active external UserDir does not exist")
    return user_dir


def _dirty_names() -> tuple[list[str], list[str]]:
    content = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    maps = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    return content, maps


def _write_report(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8", newline="\n")


def _load_spec() -> dict:
    return json.loads(SPEC_PATH.read_text(encoding="utf-8"))


def _flatten(spec: dict, field: str) -> list[str]:
    return [package for group in spec[field] for package in group["packages"]]


def _metahuman_runtime_paths(spec: dict) -> list[str]:
    inventory = spec["metahuman_runtime_contract"]["runtime_inventory"]
    if inventory.get("inventory_semantics") != EXPECTED_INVENTORY_SEMANTICS \
            or inventory.get("fresh_validation_transitive_runtime_dependency_count") \
                != EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT \
            or inventory.get("transitive_dependency_evidence_status") != \
                "FRESH2_SOURCE_VALIDATION_ONLY_REQUIRES_PACKAGED_IOSTORE_REPROOF":
        raise RuntimeError("MetaHuman explicit-root/transitive-dependency semantics differ")
    paths: list[str] = []
    for group in inventory["directory_groups"]:
        root = (PROJECT_ROOT / group["disk_relative_root"]).resolve()
        content_root = (PROJECT_ROOT / "Content").resolve()
        if content_root not in root.parents or not root.is_dir():
            raise RuntimeError(f"MetaHuman inventory root is unavailable/unsafe: {root}")
        group_paths = sorted(
            "/Game/" + path.relative_to(content_root).as_posix()[:-len(".uasset")]
            for path in root.rglob("*.uasset") if path.is_file())
        if len(group_paths) != group["expected_package_count"] \
                or len(set(group_paths)) != len(group_paths) \
                or _digest_lines(group_paths) != group["package_list_sha256"]:
            raise RuntimeError(
                f"MetaHuman directory inventory differs: {group['name']}")
        paths.extend(group_paths)
    explicit = list(inventory["explicit_packages"])
    if _digest_lines(explicit) != inventory["explicit_package_list_sha256"]:
        raise RuntimeError("MetaHuman explicit package inventory hash differs")
    paths.extend(explicit)
    paths = sorted(paths)
    if len(paths) != EXPECTED_METAHUMAN_RUNTIME_COUNT \
            or len(set(paths)) != EXPECTED_METAHUMAN_RUNTIME_COUNT \
            or inventory["expected_package_count"] != EXPECTED_METAHUMAN_RUNTIME_COUNT \
            or inventory["package_list_sha256"] \
                != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256 \
            or _digest_lines(paths) != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256:
        raise RuntimeError("MetaHuman runtime inventory is not the frozen 264-package set")
    return paths


def _package_path(path: str) -> str:
    leaf = path.rsplit("/", 1)[-1]
    return path.rsplit(".", 1)[0] if "." in leaf else path


def _object_path(path: str) -> str:
    package = _package_path(path)
    return f"{package}.{package.rsplit('/', 1)[-1]}"


def _uasset_path(path: str) -> Path:
    package = _package_path(path)
    if not package.startswith("/Game/"):
        raise RuntimeError(f"Session 8B output escaped /Game: {package}")
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


def _metadata(
        role: str, spec_sha: str,
        base: dict[str, str] = S8B_METADATA_BASE) -> dict[str, str]:
    return {
        **base,
        "DG_AssetRole": role,
        "DG_SourceSpecSHA256": spec_sha,
    }


def _mark_owned(
        asset, role: str, spec_sha: str,
        base: dict[str, str] = S8B_METADATA_BASE) -> None:
    for key, value in _metadata(role, spec_sha, base).items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)


def _require_owned(
        asset, role: str, spec_sha: str,
        base: dict[str, str] = S8B_METADATA_BASE) -> list[str]:
    errors = []
    for key, expected in _metadata(role, spec_sha, base).items():
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


def _accepted_output_hash_errors(output: dict) -> list[str]:
    path = _uasset_path(output["package"])
    if not path.is_file():
        return [f"Accepted profile file is absent: {path}"]
    errors = []
    if path.stat().st_size != output["accepted_uasset_bytes"]:
        errors.append(f"Accepted profile byte size differs: {path}")
    if _sha256(path) != output["accepted_uasset_sha256"]:
        errors.append(f"Accepted profile SHA-256 differs: {path}")
    return errors


def _validate_profile(profile, output: dict, spec_sha: str) -> list[str]:
    errors = _require_owned(
        profile, "DGMASTER_FALLBACK_BACKEND_PROFILE", spec_sha,
        LEGACY_DGMASTER_METADATA_BASE)
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


def _validate_metahuman_profile(profile, output: dict) -> list[str]:
    errors = _accepted_output_hash_errors(output)
    if not isinstance(profile, unreal.DiscGolfAvatarBackendProfile):
        return ["MetaHuman backend profile type differs"]
    if profile.get_path_name() != output["object"]:
        errors.append("MetaHuman backend profile object path differs")
    if str(profile.get_editor_property("backend_id")) != output["backend_id"]:
        errors.append("MetaHuman BackendId differs")
    if _enum_token(profile.get_editor_property("backend")) != "metahumanpreset":
        errors.append("MetaHuman backend enum differs")
    if _enum_token(profile.get_editor_property("meta_human_runtime_mode")) \
            != "shippingsafeassembled":
        errors.append("MetaHuman runtime mode is not ShippingSafeAssembled")
    if _soft_path(profile.get_editor_property("visual_actor_class")) \
            != output["visual_actor_class"]:
        errors.append("MetaHuman visual actor class differs")
    if _soft_path(profile.get_editor_property("retarget_asset")) \
            != output["retarget_asset"]:
        errors.append("MetaHuman retarget asset differs")
    if not bool(profile.get_editor_property("use_runtime_retargeting")):
        errors.append("MetaHuman runtime retargeting is disabled")
    if str(profile.get_editor_property("visual_body_component_tag")) \
            != output["visual_body_component_tag"]:
        errors.append("MetaHuman body tag differs")
    if str(profile.get_editor_property("visual_head_component_tag")) \
            != output["visual_head_component_tag"]:
        errors.append("MetaHuman head tag differs")
    if str(profile.get_editor_property("preferred_quality_profile_id")) \
            != output["preferred_quality_profile_id"]:
        errors.append("MetaHuman quality ID is not the accepted Optimized Medium mapping")
    if bool(profile.get_editor_property("allow_runtime_face_sculpting")):
        errors.append("MetaHuman runtime face sculpting is enabled")
    return errors


def _validate_manifest(
        manifest, spec: dict, runtime_paths: list[str],
        metahuman_runtime_paths: list[str], excluded: list[str],
        excluded_metahuman: list[str], profile_objects: list[str],
        spec_sha: str) -> list[str]:
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
    if int(manifest.get_editor_property(
            "expected_meta_human_runtime_package_count")) != 264:
        errors.append("Manifest expected MetaHuman runtime count differs")
    if int(manifest.get_editor_property(
            "expected_avatar_backend_profile_count")) != 2:
        errors.append("Manifest expected backend profile count differs")
    if int(manifest.get_editor_property("expected_excluded_package_count")) != 12:
        errors.append("Manifest expected excluded count differs")
    if int(manifest.get_editor_property(
            "expected_excluded_meta_human_package_count")) != 1:
        errors.append("Manifest expected excluded MetaHuman count differs")
    if str(manifest.get_editor_property(
            "meta_human_runtime_package_list_sha256")) \
            != EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256:
        errors.append("Manifest MetaHuman package-list hash differs")
    actual_runtime = [_package_path(_soft_path(value)) for value in
                      manifest.get_editor_property("runtime_assets")]
    if actual_runtime != runtime_paths:
        errors.append("Manifest runtime asset paths/order differ from frozen source spec")
    actual_metahuman = [_package_path(_soft_path(value)) for value in
                        manifest.get_editor_property("meta_human_runtime_assets")]
    if actual_metahuman != metahuman_runtime_paths:
        errors.append("Manifest MetaHuman asset paths/order differ from frozen inventory")
    actual_profiles = [_soft_path(value) for value in
                       manifest.get_editor_property("avatar_backend_profiles")]
    if actual_profiles != profile_objects:
        errors.append("Manifest backend profile entry points differ")
    actual_excluded = [str(value) for value in
                       manifest.get_editor_property("explicitly_excluded_packages")]
    if actual_excluded != excluded:
        errors.append("Manifest excluded package paths/order differ from frozen source spec")
    actual_excluded_metahuman = [str(value) for value in manifest.get_editor_property(
        "explicitly_excluded_meta_human_packages")]
    if actual_excluded_metahuman != excluded_metahuman:
        errors.append("Manifest MetaHuman exclusions differ from frozen source spec")
    return errors


def _snapshot_output_file(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    stat = path.stat()
    return {
        "bytes": path.read_bytes(),
        "mtime_ns": stat.st_mtime_ns,
    }


def _output_file_snapshot(
        packages: list[str]) -> dict[str, dict[str, dict[str, Any] | None]]:
    return {
        package: {
            suffix: _snapshot_output_file(
                _uasset_path(package).with_suffix(suffix))
            for suffix in PACKAGE_SUFFIXES
        }
        for package in packages
    }


def _restore_output_files(
        snapshot: dict[str, dict[str, dict[str, Any] | None]]) -> list[str]:
    errors: list[str] = []
    for package, records in snapshot.items():
        for suffix, record in records.items():
            path = _uasset_path(package).with_suffix(suffix)
            temporary = path.with_name(path.name + ".session8b-cook-rollback.tmp")
            try:
                if record is None:
                    if path.is_file():
                        path.unlink()
                else:
                    payload = record["bytes"]
                    if not path.is_file() or path.read_bytes() != payload:
                        temporary.write_bytes(payload)
                        temporary.replace(path)
                    current_stat = path.stat()
                    if current_stat.st_mtime_ns != record["mtime_ns"]:
                        os.utime(
                            path,
                            ns=(current_stat.st_atime_ns, record["mtime_ns"]))
            except OSError as exc:
                errors.append(f"{path}: {exc}")
            finally:
                if temporary.exists():
                    try:
                        temporary.unlink()
                    except OSError as exc:
                        errors.append(f"{temporary}: {exc}")
    return errors


def _set_manifest(
        manifest, spec: dict, spec_sha: str, runtime_assets: list,
        metahuman_assets: list, profiles: list, excluded: list[str],
        excluded_metahuman: list[str]) -> None:
    manifest.set_editor_property("manifest_id", spec["manifest_id"])
    manifest.set_editor_property("source_spec_relative_path", SOURCE_SPEC_RELATIVE)
    manifest.set_editor_property("source_spec_sha256", spec_sha)
    manifest.set_editor_property("content_status", spec["content_status"])
    manifest.set_editor_property("shipping_status", spec["shipping_status"])
    manifest.set_editor_property("authority", spec["authority"])
    manifest.set_editor_property("expected_runtime_package_count", 69)
    manifest.set_editor_property("expected_meta_human_runtime_package_count", 264)
    manifest.set_editor_property("expected_avatar_backend_profile_count", 2)
    manifest.set_editor_property("expected_excluded_package_count", 12)
    manifest.set_editor_property("expected_excluded_meta_human_package_count", 1)
    manifest.set_editor_property(
        "meta_human_runtime_package_list_sha256",
        EXPECTED_METAHUMAN_PACKAGE_LIST_SHA256)
    manifest.set_editor_property("runtime_assets", runtime_assets)
    manifest.set_editor_property("meta_human_runtime_assets", metahuman_assets)
    manifest.set_editor_property("avatar_backend_profiles", profiles)
    manifest.set_editor_property("explicitly_excluded_packages", excluded)
    manifest.set_editor_property(
        "explicitly_excluded_meta_human_packages", excluded_metahuman)
    _mark_owned(manifest, "RUNTIME_COOK_MANIFEST", spec_sha)


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    user_dir: Path | None = None
    report_path: Path | None = None
    content_root = (PROJECT_ROOT / "Content").resolve()
    savegames_root = (PROJECT_ROOT / "Saved/SaveGames").resolve()
    manifest_package = ""
    manifest_sidecars: set[Path] = set()
    manifest_snapshot: dict[
        str, dict[str, dict[str, Any] | None]] | None = None
    content_before: dict[str, dict[str, Any]] | None = None
    content_directories_before: set[str] | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    production_before: dict[str, Any] | None = None
    backup_before: dict[str, Any] | None = None
    baselines_captured = False
    transaction_started = False
    manifest = None
    runtime_assets: list[Any] = []
    metahuman_runtime_assets: list[Any] = []
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": _utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "errors": [],
        "author_switch": AUTHOR_SWITCH,
        "no_portal_switch": NO_PORTAL_SWITCH,
    }
    try:
        user_dir = _validate_command_line(command_line)
        report_path = user_dir / REPORT_RELATIVE
        if report_path.exists():
            raise RuntimeError(f"Fresh UUID already contains report: {report_path}")
        external_saves_before = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
        if external_saves_before:
            raise RuntimeError(
                f"Fresh external UserDir already contains saves: {external_saves_before}")
        state.update({
            "external_user_dir": str(user_dir),
            "report_path": str(report_path),
            "external_save_files_before": external_saves_before,
        })

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = _dirty_names()
        if dirty_content or dirty_maps:
            raise RuntimeError(
                f"Commandlet began dirty: content={dirty_content}, maps={dirty_maps}")
        spec = _load_spec()
        spec_sha = _sha256(SPEC_PATH)
        if spec_sha != FROZEN_SOURCE_SPEC_SHA256:
            raise RuntimeError(
                f"Frozen cook source spec hash differs: {spec_sha} != "
                f"{FROZEN_SOURCE_SPEC_SHA256}")
        runtime_paths = _flatten(spec, "runtime_package_groups")
        metahuman_runtime_paths = _metahuman_runtime_paths(spec)
        excluded = _flatten(spec, "excluded_package_groups")
        if len(runtime_paths) != EXPECTED_RUNTIME_COUNT \
                or len(set(runtime_paths)) != EXPECTED_RUNTIME_COUNT:
            raise RuntimeError(
                "Cook source does not contain exactly 69 unique legacy runtime packages")
        if len(excluded) != EXPECTED_EXCLUDED_COUNT \
                or len(set(excluded)) != EXPECTED_EXCLUDED_COUNT:
            raise RuntimeError(
                "Cook source does not contain exactly 12 unique legacy exclusions")
        metahuman_contract = spec["metahuman_runtime_contract"]
        excluded_metahuman = list(
            metahuman_contract["explicitly_excluded_packages"])
        if excluded_metahuman != [EXPECTED_SOURCE_MHC_PACKAGE]:
            raise RuntimeError(
                "Cook source does not contain the exact source MHC exclusion")

        outputs = spec["asset_outputs"]
        manifest_output = outputs["runtime_cook_manifest"]
        profile_output = outputs["dg_master_backend_profile"]
        metahuman_profile_output = outputs["metahuman_default_backend_profile"]
        manifest_package = manifest_output["package"]
        profile_package = profile_output["package"]
        manifest_object = manifest_output["object"]
        profile_object = profile_output["object"]
        metahuman_profile_object = metahuman_profile_output["object"]
        if manifest_object != EXPECTED_MANIFEST_OBJECT \
                or profile_object != EXPECTED_DGMASTER_PROFILE_OBJECT \
                or metahuman_profile_object != EXPECTED_METAHUMAN_PROFILE_OBJECT:
            raise RuntimeError(
                "Cook spec output object paths differ from frozen entry points")
        expected_metahuman = {
            "backend_id": EXPECTED_METAHUMAN_BACKEND_ID,
            "backend_profile": EXPECTED_METAHUMAN_PROFILE_OBJECT,
            "assembled_visual_actor_class": EXPECTED_METAHUMAN_ACTOR_CLASS,
            "retarget_asset": EXPECTED_METAHUMAN_RETARGET_OBJECT,
            "target_ik_rig": EXPECTED_METAHUMAN_TARGET_IK_OBJECT,
        }
        if any(metahuman_contract.get(field) != value
               for field, value in expected_metahuman.items()):
            raise RuntimeError(
                "Accepted MetaHuman entry points differ from frozen constants")

        required_existing = {
            manifest_package: unreal.EditorAssetLibrary.does_asset_exist(
                manifest_package),
            profile_package: unreal.EditorAssetLibrary.does_asset_exist(
                profile_package),
            metahuman_profile_output["package"]:
                unreal.EditorAssetLibrary.does_asset_exist(
                    metahuman_profile_output["package"]),
        }
        required_disks = {
            package: _uasset_path(package).is_file()
            for package in required_existing
        }
        if not all(required_existing.values()) or not all(required_disks.values()):
            raise RuntimeError(
                "S8B author is refresh-only; every existing manifest/profile "
                f"asset and file is required: registry={required_existing}, "
                f"disk={required_disks}")
        required_exclusion_packages = excluded + excluded_metahuman
        exclusion_registry = {
            package: unreal.EditorAssetLibrary.does_asset_exist(package)
            for package in required_exclusion_packages
        }
        exclusion_disk = {
            package: _uasset_path(package).is_file()
            for package in required_exclusion_packages
        }
        if not all(exclusion_registry.values()) or not all(exclusion_disk.values()):
            raise RuntimeError(
                "S8B author requires all 12 legacy fixtures and the exact "
                "editor-only source MHC to remain present before mutation: "
                f"registry={exclusion_registry}, disk={exclusion_disk}")

        manifest_sidecars = {
            _uasset_path(manifest_package).with_suffix(suffix).resolve()
            for suffix in PACKAGE_SUFFIXES
        }
        manifest_snapshot = _output_file_snapshot([manifest_package])
        if manifest_snapshot[manifest_package][".uasset"] is None:
            raise RuntimeError("Existing runtime manifest uasset is absent")
        content_before = _tree_snapshot(content_root, manifest_sidecars)
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
            raise RuntimeError(
                "Production save or accepted external backup is not the frozen A999 baseline")
        if PRODUCTION_SAVE.read_bytes() != ACCEPTED_EXTERNAL_BACKUP.read_bytes():
            raise RuntimeError(
                "Production save and accepted external backup are not byte-identical")
        state["before"] = {
            "content_excluding_manifest": _snapshot_summary(content_before),
            "content_directories": _directory_summary(
                content_directories_before),
            "savegames": _snapshot_summary(savegames_before),
            "savegame_directories": _directory_summary(
                savegame_directories_before),
            "production_save": production_before,
            "accepted_external_backup": backup_before,
            "manifest_files": {
                suffix: _record(
                    _uasset_path(manifest_package).with_suffix(suffix))
                for suffix in PACKAGE_SUFFIXES
            },
        }
        baselines_captured = True

        state["phase"] = "LOAD_AND_VALIDATE"
        runtime_assets = _load_required_runtime_assets(runtime_paths)
        metahuman_runtime_assets = _load_required_runtime_assets(
            metahuman_runtime_paths)
        metahuman_profile = unreal.EditorAssetLibrary.load_asset(
            metahuman_profile_output["package"])
        metahuman_errors = _validate_metahuman_profile(
            metahuman_profile, metahuman_profile_output)
        if metahuman_errors:
            raise RuntimeError(
                "Accepted MetaHuman profile is not ready for manifest refresh: "
                + "; ".join(metahuman_errors))
        manifest = unreal.EditorAssetLibrary.load_asset(manifest_package)
        profile = unreal.EditorAssetLibrary.load_asset(profile_package)
        if not isinstance(manifest, unreal.DiscGolfRuntimeCookManifest):
            raise RuntimeError(f"Unexpected manifest type: {manifest}")
        if not isinstance(profile, unreal.DiscGolfAvatarBackendProfile):
            raise RuntimeError(f"Unexpected DGMaster profile type: {profile}")
        profile_errors = _validate_profile(
            profile, profile_output, LEGACY_SOURCE_SPEC_SHA256)
        if profile_errors:
            raise RuntimeError(
                "DGMaster fallback profile lost its accepted legacy contract: "
                + "; ".join(profile_errors))
        profile_objects = [profile_object, metahuman_profile_object]
        current_errors = _validate_manifest(
            manifest, spec, runtime_paths, metahuman_runtime_paths,
            excluded, excluded_metahuman, profile_objects, spec_sha)

        asset_save_calls = 0
        if not current_errors:
            status = "PASS_ALREADY_CURRENT_NO_ASSET_WRITES"
            disk_mutation = "NONE"
        else:
            owned_current = not _require_owned(
                manifest, "RUNTIME_COOK_MANIFEST", spec_sha)
            owned_legacy = not _require_owned(
                manifest, "RUNTIME_COOK_MANIFEST", LEGACY_SOURCE_SPEC_SHA256,
                LEGACY_DGMASTER_METADATA_BASE)
            if not owned_current and not owned_legacy:
                raise RuntimeError(
                    "Refusing to refresh a non-owned cook manifest: "
                    + "; ".join(current_errors))
            state["phase"] = "EXACT_MANIFEST_MUTATION"
            transaction_started = True
            _set_manifest(
                manifest, spec, spec_sha, runtime_assets,
                metahuman_runtime_assets, [profile, metahuman_profile],
                excluded, excluded_metahuman)
            refresh_errors = _validate_manifest(
                manifest, spec, runtime_paths, metahuman_runtime_paths,
                excluded, excluded_metahuman, profile_objects, spec_sha)
            if refresh_errors:
                raise RuntimeError(
                    "Refreshed manifest failed pre-save validation: "
                    + "; ".join(refresh_errors))
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                    manifest, only_if_is_dirty=False):
                raise RuntimeError(
                    "Failed to save the existing runtime cook manifest")
            asset_save_calls = 1
            status = "PASS_REFRESHED_EXISTING_MANIFEST_AND_SAVED"
            disk_mutation = "EXACT_EXISTING_MANIFEST_WRITE"

        state["phase"] = "BOUNDARY_VERIFICATION"
        dirty_content, dirty_maps = _dirty_names()
        if dirty_content or dirty_maps:
            raise RuntimeError(
                f"Dirty packages remain: content={dirty_content}, maps={dirty_maps}")
        content_after = _tree_snapshot(content_root, manifest_sidecars)
        content_directories_after = _directory_snapshot(content_root)
        savegames_after = _tree_snapshot(savegames_root)
        savegame_directories_after = _directory_snapshot(savegames_root)
        production_after = _record(PRODUCTION_SAVE)
        backup_after = _record(ACCEPTED_EXTERNAL_BACKUP)
        external_saves_after = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav"))
        if content_after != content_before:
            raise RuntimeError(
                "A non-manifest file anywhere under Content changed")
        if content_directories_after != content_directories_before:
            raise RuntimeError("Content directory topology changed")
        if savegames_after != savegames_before \
                or savegame_directories_after != savegame_directories_before:
            raise RuntimeError("Project SaveGames files or directory topology changed")
        if production_after != production_before or backup_after != backup_before \
                or PRODUCTION_SAVE.read_bytes() \
                    != ACCEPTED_EXTERNAL_BACKUP.read_bytes():
            raise RuntimeError("Production save or accepted external backup changed")
        if external_saves_after:
            raise RuntimeError(
                f"External UserDir contains save files: {external_saves_after}")

        output_hashes = {
            manifest_package: {
                "files": {
                    suffix: _record(
                        _uasset_path(manifest_package).with_suffix(suffix))
                    for suffix in PACKAGE_SUFFIXES
                }
            },
            profile_package: {
                "uasset": _record(_uasset_path(profile_package))
            },
            metahuman_profile_output["package"]: {
                "uasset": _record(
                    _uasset_path(metahuman_profile_output["package"]))
            },
        }
        result = {
            "schema": SCHEMA,
            "generated_utc": state["generated_utc"],
            "completed_utc": _utc_now(),
            "status": status,
            "phase": "COMPLETE",
            "source_spec": SOURCE_SPEC_RELATIVE,
            "source_spec_sha256": spec_sha,
            "runtime_package_count": len(runtime_paths),
            "metahuman_runtime_package_count": len(
                metahuman_runtime_paths),
            "metahuman_explicit_cook_root_package_count": len(
                metahuman_runtime_paths),
            "metahuman_inventory_semantics": EXPECTED_INVENTORY_SEMANTICS,
            "fresh_validation_transitive_runtime_dependency_count":
                EXPECTED_FRESH_TRANSITIVE_DEPENDENCY_COUNT,
            "packaged_transitive_dependency_reproof": "REQUIRED",
            "metahuman_runtime_package_list_sha256": _digest_lines(
                metahuman_runtime_paths),
            "backend_profile_count": EXPECTED_BACKEND_PROFILE_COUNT,
            "excluded_package_count": len(excluded),
            "excluded_metahuman_package_count": len(excluded_metahuman),
            "manifest_object": manifest_object,
            "dg_master_backend_profile_object": profile_object,
            "metahuman_backend_profile_object": metahuman_profile_object,
            "metahuman_preferred_quality_profile_id":
                metahuman_profile_output["preferred_quality_profile_id"],
            "availability_gate": "SEPARATE_S8B_AUDIT_NOT_INVOKED",
            "manifest_metadata_session": "8B",
            "dg_master_profile_metadata_session": "8_LEGACY_PRESERVED",
            "disk_mutation": disk_mutation,
            "mutated_packages": (
                [manifest_package] if asset_save_calls == 1 else []),
            "protected_content_files_unchanged": True,
            "protected_content_directories_unchanged": True,
            "project_savegames_files_unchanged": True,
            "project_savegames_directories_unchanged": True,
            "production_save_unchanged": True,
            "accepted_external_backup_unchanged": True,
            "external_save_files_before": external_saves_before,
            "external_save_files_after": external_saves_after,
            "external_user_dir": str(user_dir),
            "report_path": str(report_path),
            "before": state["before"],
            "after": {
                "content_excluding_manifest": _snapshot_summary(
                    content_after),
                "content_directories": _directory_summary(
                    content_directories_after),
                "savegames": _snapshot_summary(savegames_after),
                "savegame_directories": _directory_summary(
                    savegame_directories_after),
                "production_save": production_after,
                "accepted_external_backup": backup_after,
            },
            "output_package_hashes": output_hashes,
            "transactional_manifest_snapshot": True,
            "rollback_status": "NOT_REQUIRED",
            "rollback_verified": False,
            "rollback_performed": False,
            "asset_factory_calls": 0,
            "asset_save_calls": asset_save_calls,
            "asset_import_calls": 0,
            "asset_delete_calls": 0,
            "metahuman_assets_created": 0,
            "vendor_assets_created": 0,
            "brand_art_created": 0,
            "unreal_process_spawned_by_script": False,
            "ubt_launched": False,
            "cook_launched": False,
            "errors": [],
        }
        state["phase"] = "REPORT_WRITE"
        _write_report(report_path, result)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback_errors: list[str] = []
        rollback_verified = False
        boundaries_verified = False
        if transaction_started and manifest_snapshot is not None \
                and manifest_package:
            manifest = None
            runtime_assets = []
            metahuman_runtime_assets = []
            try:
                unreal.SystemLibrary.collect_garbage()
            except Exception as garbage_error:
                rollback_errors.append(
                    f"Rollback garbage collection failed: {garbage_error}")
            rollback_errors.extend(_restore_output_files(manifest_snapshot))
        if baselines_captured and content_before is not None \
                and content_directories_before is not None \
                and savegames_before is not None \
                and savegame_directories_before is not None \
                and production_before is not None and backup_before is not None \
                and user_dir is not None and manifest_package \
                and manifest_snapshot is not None:
            for save_path in sorted(user_dir.rglob("*.sav")):
                try:
                    save_path.unlink()
                except OSError as save_error:
                    rollback_errors.append(
                        f"Could not remove external save {save_path}: {save_error}")
            try:
                boundaries_verified = (
                    not rollback_errors
                    and _output_file_snapshot([manifest_package])
                        == manifest_snapshot
                    and _tree_snapshot(content_root, manifest_sidecars)
                        == content_before
                    and _directory_snapshot(content_root)
                        == content_directories_before
                    and _tree_snapshot(savegames_root) == savegames_before
                    and _directory_snapshot(savegames_root)
                        == savegame_directories_before
                    and _record(PRODUCTION_SAVE) == production_before
                    and _record(ACCEPTED_EXTERNAL_BACKUP) == backup_before
                    and PRODUCTION_SAVE.read_bytes()
                        == ACCEPTED_EXTERNAL_BACKUP.read_bytes()
                    and not list(user_dir.rglob("*.sav"))
                    and not list(
                        _uasset_path(manifest_package).parent.glob(
                            "*.session8b-cook-rollback.tmp"))
                )
            except Exception as verify_error:
                rollback_errors.append(
                    f"Rollback verification failed: {verify_error}")
                boundaries_verified = False
            rollback_verified = transaction_started and boundaries_verified
            if not boundaries_verified:
                rollback_errors.append(
                    "Failure boundary verification did not restore every "
                    "manifest/content/save/external-save boundary")
        if baselines_captured:
            if not boundaries_verified:
                rollback_status = "INCOMPLETE"
                status = "FAIL_ROLLBACK_INCOMPLETE"
            elif transaction_started:
                rollback_status = "ROLLED_BACK"
                status = "FAIL_ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
            else:
                rollback_status = "NOT_REQUIRED_BOUNDARIES_VERIFIED"
                status = "FAIL_BEFORE_MUTATION_NO_PERSISTENT_ASSET_WRITES"
        else:
            rollback_status = "NOT_REQUIRED"
            status = "FAIL_BEFORE_MUTATION_NO_PERSISTENT_ASSET_WRITES"
        state.update({
            "status": status,
            "phase": "FAILED",
            "failed_utc": _utc_now(),
            "rollback_status": rollback_status,
            "rollback_verified": rollback_verified,
            "failure_boundaries_verified": boundaries_verified,
            "rollback_performed": transaction_started,
            "rollback_errors": rollback_errors,
            "asset_factory_calls": 0,
            "asset_save_calls": 0,
            "asset_import_calls": 0,
            "asset_delete_calls": 0,
            "errors": state["errors"] + rollback_errors,
        })
        if report_path is not None:
            try:
                _write_report(report_path, state)
            except Exception as report_error:
                unreal.log_error(
                    "DG_SESSION8B_COOK_ASSET_AUTHOR: failure report write "
                    f"failed: {report_error}")
        unreal.log_error(
            "DG_SESSION8B_COOK_ASSET_AUTHOR: "
            f"{status} phase={state['phase']} report={report_path}: {exc}")
        raise

    unreal.log(
        "DG_SESSION8B_COOK_ASSET_AUTHOR: "
        f"{result['status']} runtime={result['runtime_package_count']} "
        f"metahuman_explicit_roots="
        f"{result['metahuman_explicit_cook_root_package_count']} "
        f"profiles={result['backend_profile_count']} "
        f"excluded={result['excluded_package_count']}/"
        f"{result['excluded_metahuman_package_count']} "
        f"saves={result['asset_save_calls']} report={report_path}")


if __name__ == "__main__":
    main()
