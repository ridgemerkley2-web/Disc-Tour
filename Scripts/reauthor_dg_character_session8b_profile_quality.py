"""Correct the canonical Session 8B profile quality with an exact rollback boundary."""

from __future__ import annotations

import hashlib
import json
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanProfileQualityCorrection.v1"
CORRECT_SWITCH = "-DGSession8BMetaHumanProfileQualityCorrection"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanProfileQualityCorrection.json"
)
PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default"
)
PROFILE_OBJECT = f"{PROFILE_PACKAGE}.DA_DG_AvatarBackend_MetaHuman_Default"
PROFILE_RELATIVE = Path(
    "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
)
OLD_QUALITY = "GameplayHigh"
EXPECTED_QUALITY = "GameplayPerformance"
EXPECTED_PIPELINE = "UE_OPTIMIZED"
EXPECTED_OPTIMIZATION = "MEDIUM"
CONTENT_BEFORE = {
    "file_count": 953,
    "bytes": 2919929355,
    "manifest_sha256": (
        "78BA11D1A5B18310FF719CEBD9AE84EF2AA58B5CDC0B73873EA8013A5CD908EF"
    ),
}
CONTENT_DIRECTORY_COUNT = 230
CANONICAL_BEFORE = {
    "Content/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.uasset": {
        "exists": True,
        "bytes": 225470,
        "sha256": (
            "72704ECC288B7D0917C6062198B3B5161B966244A5A249632021A847BE253634"
        ),
    },
    "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset": {
        "exists": True,
        "bytes": 23463,
        "sha256": (
            "51208403C91B72F490746AD2EDAA182F74B345CAD4B54D426A1C2C1441D74792"
        ),
    },
    str(PROFILE_RELATIVE).replace("\\", "/"): {
        "exists": True,
        "bytes": 2179,
        "sha256": (
            "D825DB875B079F9AAA0D99F5FFA7596A13EACBBB4424079829E3D473F706071F"
        ),
    },
}
FRESH_VALIDATION_REPORT = Path(
    r"C:\DGTour_TestRuns\Session8B\f8f9244c-4523-49e4-9465-bdb81449f9c8"
    r"\Saved\CharacterFramework\Session8BMetaHumanRuntimeAssetValidation.json"
)
FRESH_VALIDATION_REPORT_RECORD = {
    "exists": True,
    "bytes": 8596,
    "sha256": (
        "F000604C51662D3EDECF15BC83ED7C9279F782AAA9453A77953650FEC2234AC3"
    ),
}
PRODUCTION_SAVE_RELATIVE = Path("Saved/SaveGames/DiscGolfTour_Profile_0.sav")
ACCEPTED_BACKUP = Path(
    r"C:\DGTour_Backups\Session7_Accepted"
    r"\DiscGolfTour_Profile_0_Session7_Accepted.sav"
)
ACCEPTED_SAVE = {
    "exists": True,
    "bytes": 5212,
    "sha256": (
        "A9996D3A3368C929437A913FB8ADF3E1BCF947711D9F0867976C3BB30A491A14"
    ),
}
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")


class CorrectionError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {"exists": True, "bytes": path.stat().st_size, "sha256": sha256(path)}


def snapshot(root: Path) -> dict[str, dict[str, Any]]:
    if not root.is_dir():
        return {}
    return {
        path.relative_to(root).as_posix(): record(path)
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file())
    }


def snapshot_summary(files: dict[str, dict[str, Any]]) -> dict[str, Any]:
    digest = hashlib.sha256()
    total_bytes = 0
    for relative, identity in sorted(files.items()):
        size = int(identity["bytes"])
        total_bytes += size
        digest.update(
            f"{relative}\t{size}\t{identity['sha256']}\n".encode("utf-8")
        )
    return {
        "file_count": len(files),
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest().upper(),
    }


def directory_snapshot(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_dir()
    }


def write_report(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(state, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def strict_json_text(text: str, label: str) -> dict[str, Any]:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise CorrectionError(f"duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    try:
        value = json.loads(text, object_pairs_hook=no_duplicates)
    except json.JSONDecodeError as exc:
        raise CorrectionError(f"invalid JSON returned by {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise CorrectionError(f"{label} JSON root is not an object")
    return value


def exact_switch(command_line: str, switch: str) -> bool:
    return bool(re.search(rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line))


def command_value(command_line: str, name: str) -> str:
    match = re.search(
        rf'(?i)(?:^|\s)-{re.escape(name)}=(?:"([^"]+)"|(\S+))',
        command_line,
    )
    return (match.group(1) or match.group(2)) if match else ""


def validate_command_line(command_line: str, project_root: Path) -> Path:
    required = (CORRECT_SWITCH, NO_PORTAL_SWITCH, "-unattended", "-nop4")
    missing = [switch for switch in required if not exact_switch(command_line, switch)]
    if missing:
        raise CorrectionError(f"required command-line switches absent: {missing}")
    if "-run=pythonscript" not in command_line.casefold():
        raise CorrectionError("quality correction must use PythonScript commandlet")
    user_dir_text = command_value(command_line, "UserDir")
    if not user_dir_text:
        raise CorrectionError("absolute external -UserDir is required")
    user_dir = Path(user_dir_text).resolve()
    parent = Path(r"C:\DGTour_TestRuns\Session8B").resolve()
    try:
        relative = user_dir.relative_to(parent)
    except ValueError as exc:
        raise CorrectionError(f"UserDir is outside Session8B test root: {user_dir}") from exc
    if len(relative.parts) != 1:
        raise CorrectionError("UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise CorrectionError("UserDir child is not a UUID") from exc
    if str(parsed) != relative.name.casefold():
        raise CorrectionError("UserDir UUID is not canonical")
    try:
        user_dir.relative_to(project_root)
    except ValueError:
        pass
    else:
        raise CorrectionError("UserDir must be external to the project")
    return user_dir


def dirty_names() -> tuple[list[str], list[str]]:
    content = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    maps = sorted(
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return content, maps


def resolve_validate_helper() -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    names = (
        "validate_session8_b_meta_human_assets",
        "validate_session8b_meta_human_assets",
    )
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise CorrectionError(f"expected exactly one validation helper spelling, got {matches}")
    return getattr(utility, matches[0])


def validate_helper(payload: dict[str, Any]) -> None:
    exact = {
        "schema_version": 1,
        "session": "8B",
        "operation": "VALIDATE",
        "status": "PASS",
        "disk_mutation": "NONE",
        "writes": [],
        "attempted_writes": [],
        "errors": [],
        "profile_contract_verified": True,
        "preferred_quality_profile_id": EXPECTED_QUALITY,
        "assembly_pipeline": EXPECTED_PIPELINE,
        "assembly_optimization_level": EXPECTED_OPTIMIZATION,
        "reload_from_disk_verified": True,
        "source_mhc_editor_only": True,
    }
    for key, expected in exact.items():
        if payload.get(key) != expected:
            raise CorrectionError(
                f"validation helper field {key}={payload.get(key)!r}, "
                f"expected {expected!r}"
            )


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    invalid_report = Path(tempfile.gettempdir()) / "Session8BProfileQuality.invalid.json"
    report_path = invalid_report
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "errors": [],
        "profile_object": PROFILE_OBJECT,
        "old_quality": OLD_QUALITY,
        "expected_quality": EXPECTED_QUALITY,
    }
    content_before: dict[str, dict[str, Any]] | None = None
    directories_before: set[str] | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    profile_bytes_before: bytes | None = None
    profile: Any | None = None
    mutation_started = False
    user_dir: Path | None = None
    production: Path | None = None
    savegames_root: Path | None = None
    try:
        user_dir = validate_command_line(command_line, project_root)
        report_path = user_dir / REPORT_RELATIVE
        if report_path.exists():
            raise CorrectionError(f"fresh UUID already contains report: {report_path}")
        if list(user_dir.rglob("*.sav")):
            raise CorrectionError("fresh UUID already contains a save file")
        state["external_user_dir"] = str(user_dir)
        state["report_path"] = str(report_path)

        state["phase"] = "IMMUTABLE_BASELINES"
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise CorrectionError(
                f"commandlet began dirty: content={dirty_content}, maps={dirty_maps}"
            )
        if record(FRESH_VALIDATION_REPORT) != FRESH_VALIDATION_REPORT_RECORD:
            raise CorrectionError("accepted Fresh1 validation report identity changed")
        content_before = snapshot(content_root)
        if snapshot_summary(content_before) != CONTENT_BEFORE:
            raise CorrectionError(
                f"canonical Content baseline changed: {snapshot_summary(content_before)}"
            )
        directories_before = directory_snapshot(content_root)
        if len(directories_before) != CONTENT_DIRECTORY_COUNT:
            raise CorrectionError(
                f"Content directory count is {len(directories_before)}, "
                f"expected {CONTENT_DIRECTORY_COUNT}"
            )
        for relative, expected in CANONICAL_BEFORE.items():
            if record(project_root / relative) != expected:
                raise CorrectionError(f"canonical baseline changed: {relative}")
        profile_path = project_root / PROFILE_RELATIVE
        profile_base = profile_path.with_suffix("")
        for suffix in PACKAGE_SUFFIXES[1:]:
            if Path(f"{profile_base}{suffix}").exists():
                raise CorrectionError(f"unexpected profile sidecar exists: {suffix}")
        profile_bytes_before = profile_path.read_bytes()

        production = project_root / PRODUCTION_SAVE_RELATIVE
        savegames_root = production.parent
        savegames_before = snapshot(savegames_root)
        savegame_directories_before = directory_snapshot(savegames_root)
        if savegames_before != {production.name: ACCEPTED_SAVE}:
            raise CorrectionError("project SaveGames baseline differs")
        if savegame_directories_before:
            raise CorrectionError("project SaveGames unexpectedly contains directories")
        if record(ACCEPTED_BACKUP) != ACCEPTED_SAVE:
            raise CorrectionError("accepted backup identity changed")
        if production.read_bytes() != ACCEPTED_BACKUP.read_bytes():
            raise CorrectionError("production save and accepted backup differ")
        state["before"] = {
            "content": CONTENT_BEFORE,
            "content_directory_count": len(directories_before),
            "profile": record(profile_path),
            "project_savegames": savegames_before,
            "accepted_fresh_validation": record(FRESH_VALIDATION_REPORT),
        }

        state["phase"] = "EXACT_PROFILE_QUALITY_WRITE"
        profile = unreal.EditorAssetLibrary.load_asset(PROFILE_OBJECT)
        if profile is None:
            raise CorrectionError("canonical backend profile did not load")
        current_quality = str(profile.get_editor_property("preferred_quality_profile_id"))
        if current_quality != OLD_QUALITY:
            raise CorrectionError(
                f"profile quality is {current_quality!r}, expected stale {OLD_QUALITY!r}"
            )
        mutation_started = True
        profile.set_editor_property(
            "preferred_quality_profile_id", unreal.Name(EXPECTED_QUALITY)
        )
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            profile, only_if_is_dirty=False
        ):
            raise CorrectionError("UE refused the exact backend-profile save")
        # The validation helper unloads all three canonical packages before its
        # disk reload. Release the Python wrapper so it cannot keep this package
        # alive and weaken that evidence boundary.
        profile = None
        unreal.SystemLibrary.collect_garbage()

        state["phase"] = "HELPER_RELOAD_VALIDATE"
        helper_payload = strict_json_text(
            resolve_validate_helper()(), "C++ validation helper"
        )
        state["helper_result"] = helper_payload
        validate_helper(helper_payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content or dirty_maps:
            raise CorrectionError(
                f"dirty packages remain: content={dirty_content}, maps={dirty_maps}"
            )

        content_after = snapshot(content_root)
        directories_after = directory_snapshot(content_root)
        changed = sorted(
            key
            for key in set(content_before) & set(content_after)
            if content_before[key] != content_after[key]
        )
        added = sorted(set(content_after) - set(content_before))
        removed = sorted(set(content_before) - set(content_after))
        expected_changed = [
            PROFILE_RELATIVE.relative_to("Content").as_posix()
        ]
        if changed != expected_changed or added or removed:
            raise CorrectionError(
                f"Content delta differs: changed={changed}, added={added}, removed={removed}"
            )
        if directories_after != directories_before:
            raise CorrectionError("Content directory topology changed")
        savegames_after = snapshot(savegames_root)
        savegame_directories_after = directory_snapshot(savegames_root)
        external_saves = sorted(
            str(path.relative_to(user_dir)) for path in user_dir.rglob("*.sav")
        )
        if (
            savegames_after != savegames_before
            or savegame_directories_after != savegame_directories_before
            or external_saves
            or record(ACCEPTED_BACKUP) != ACCEPTED_SAVE
            or production.read_bytes() != ACCEPTED_BACKUP.read_bytes()
        ):
            raise CorrectionError("protected save boundary changed")

        state.update(
            {
                "status": "PASS_PROFILE_QUALITY_CORRECTED_AND_RELOAD_VALIDATED",
                "phase": "COMPLETE",
                "disk_mutation": "EXACT_ONE_PROFILE_QUALITY_WRITE",
                "content_before": CONTENT_BEFORE,
                "content_after": snapshot_summary(content_after),
                "content_changed": changed,
                "content_added": added,
                "content_removed": removed,
                "content_directories_unchanged": True,
                "profile_before": CANONICAL_BEFORE[
                    str(PROFILE_RELATIVE).replace("\\", "/")
                ],
                "profile_after": record(profile_path),
                "project_savegames_unchanged": True,
                "production_save_unchanged": True,
                "accepted_backup_unchanged": True,
                "external_save_files": external_saves,
                "completed_utc": utc_now(),
            }
        )
        write_report(report_path, state)
    except Exception as exc:
        state["errors"].append(f"{type(exc).__name__}: {exc}")
        rollback_verified = False
        rollback_errors: list[str] = []
        if (
            mutation_started
            and content_before is not None
            and directories_before is not None
            and profile_bytes_before is not None
            and production is not None
            and savegames_root is not None
            and savegames_before is not None
            and savegame_directories_before is not None
            and user_dir is not None
        ):
            profile_path = project_root / PROFILE_RELATIVE
            try:
                profile = None
                unreal.SystemLibrary.collect_garbage()
            except Exception as garbage_error:
                rollback_errors.append(
                    f"rollback garbage collection failed: {garbage_error}"
                )
            restore_temp = profile_path.with_name(
                f"{profile_path.name}.session8b-quality-rollback.tmp"
            )
            try:
                restore_temp.write_bytes(profile_bytes_before)
                restore_temp.replace(profile_path)
                for suffix in PACKAGE_SUFFIXES[1:]:
                    sidecar = Path(f"{profile_path.with_suffix('')}{suffix}")
                    if sidecar.exists():
                        sidecar.unlink()
            except OSError as rollback_error:
                rollback_errors.append(f"rollback file restore failed: {rollback_error}")
            finally:
                if restore_temp.exists():
                    try:
                        restore_temp.unlink()
                    except OSError as cleanup_error:
                        rollback_errors.append(
                            f"rollback temp cleanup failed: {cleanup_error}"
                        )
            try:
                rollback_verified = (
                    not rollback_errors
                    and snapshot(content_root) == content_before
                    and directory_snapshot(content_root) == directories_before
                    and snapshot(savegames_root) == savegames_before
                    and directory_snapshot(savegames_root)
                    == savegame_directories_before
                    and record(ACCEPTED_BACKUP) == ACCEPTED_SAVE
                    and production.read_bytes() == ACCEPTED_BACKUP.read_bytes()
                    and not list(user_dir.rglob("*.sav"))
                )
            except Exception as verification_error:
                rollback_errors.append(
                    f"rollback verification failed: {verification_error}"
                )
                rollback_verified = False
        state["rollback_errors"] = rollback_errors
        state["errors"].extend(rollback_errors)
        state["rollback_verified"] = rollback_verified
        state["status"] = (
            "FAIL_ROLLED_BACK_NO_PERSISTENT_ASSET_WRITES"
            if rollback_verified
            else (
                "FAIL_ROLLBACK_INCOMPLETE"
                if mutation_started
                else "FAIL_BEFORE_MUTATION_NO_PERSISTENT_ASSET_WRITES"
            )
        )
        state["failed_utc"] = utc_now()
        try:
            write_report(report_path, state)
        except Exception as report_error:
            unreal.log_error(
                "DG_SESSION8B_PROFILE_QUALITY_CORRECTION: report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            "DG_SESSION8B_PROFILE_QUALITY_CORRECTION: "
            f"{state['status']} phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_PROFILE_QUALITY_CORRECTION: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
