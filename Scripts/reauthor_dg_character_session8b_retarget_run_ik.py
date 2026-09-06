"""Disable only the canonical Session 8B Run IK Rig op transactionally."""

from __future__ import annotations

import hashlib
import json
import os
import re
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import unreal


SCHEMA = "DiscGolfTour.Session8BMetaHumanRetargetRunIKCorrection.v1"
CORRECT_SWITCH = "-DGSession8BMetaHumanRetargetRunIKCorrection"
NO_PORTAL_SWITCH = "-NoMetaHumanAccountPortalLoginFallback"
REPORT_RELATIVE = Path(
    "Saved/CharacterFramework/Session8BMetaHumanRetargetRunIKCorrection.json"
)
RETARGET_PACKAGE = "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman"
RETARGET_OBJECT = f"{RETARGET_PACKAGE}.RTG_DGMaster_To_MetaHuman"
RETARGET_RELATIVE = Path(
    "Content/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
)
EXPECTED_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"
EXPECTED_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_OP_ENABLED_MASK = [True, True, False, True, True]
CONTENT_BEFORE = {
    "file_count": 953,
    "bytes": 2920025165,
    "manifest_sha256": (
        "4306208A7347B8924DF8D1F3590B82FA605AC582529F4B2BC2F8ADED6E6194F2"
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
    str(RETARGET_RELATIVE).replace("\\", "/"): {
        "exists": True,
        "bytes": 23463,
        "sha256": (
            "51208403C91B72F490746AD2EDAA182F74B345CAD4B54D426A1C2C1441D74792"
        ),
    },
    "Content/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default.uasset": {
        "exists": True,
        "bytes": 2274,
        "sha256": (
            "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D"
        ),
    },
}
FRESH_VALIDATION_REPORT = Path(
    r"C:\DGTour_TestRuns\Session8B\3707a439-9178-44f2-b258-ccaefd3a0f0b"
    r"\Saved\CharacterFramework\Session8BMetaHumanRuntimeAssetValidation.json"
)
FRESH_VALIDATION_REPORT_RECORD = {
    "exists": True,
    "bytes": 8741,
    "sha256": (
        "A1B1BD67ABE48E0A5A109D5B6CA7863DE9264C14751918506E09F819DF959E7B"
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
        for path in sorted(
            candidate for candidate in root.rglob("*") if candidate.is_file()
        )
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
    temp = path.with_name(f"{path.name}.tmp")
    try:
        temp.write_text(
            json.dumps(state, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        temp.replace(path)
    finally:
        if temp.exists():
            temp.unlink()


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
    return bool(
        re.search(rf"(?i)(?:^|\s|\"){re.escape(switch)}(?=$|\s|\")", command_line)
    )


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
    run_tokens = re.findall(r"(?i)(?:^|\s)-run=pythonscript(?=$|\s)", command_line)
    if len(run_tokens) != 1 or re.search(
        r"(?i)(?:^|\s)-executepythonscript(?:=|\s|$)", command_line
    ):
        raise CorrectionError("correction requires exactly one PythonScript commandlet")
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


def resolve_method(names: tuple[str, ...], label: str) -> Callable[[], str]:
    utility = unreal.DiscGolfSession8BMetaHumanUtility
    matches = [name for name in names if hasattr(utility, name)]
    if len(matches) != 1:
        raise CorrectionError(f"expected exactly one {label} spelling, got {matches}")
    return getattr(utility, matches[0])


def resolve_prepare_helper() -> Callable[[], str]:
    return resolve_method(
        (
            "prepare_session8_b_meta_human_run_ik_correction",
            "prepare_session8b_meta_human_run_ik_correction",
        ),
        "prepare helper",
    )


def resolve_validate_helper() -> Callable[[], str]:
    return resolve_method(
        (
            "validate_session8_b_meta_human_assets",
            "validate_session8b_meta_human_assets",
        ),
        "validation helper",
    )


def require_exact(payload: dict[str, Any], expected: dict[str, Any], label: str) -> None:
    for key, value in expected.items():
        if payload.get(key) != value:
            raise CorrectionError(
                f"{label} field {key}={payload.get(key)!r}, expected {value!r}"
            )


def validate_prepare(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "PREPARE_RETARGET_RUN_IK_CORRECTION",
            "status": "PASS",
            "disk_mutation": "NONE_IN_MEMORY_ONLY",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "retarget_op_count": 5,
            "retarget_op_types": EXPECTED_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled_before_correction": True,
            "run_ik_rig_enabled_after_correction": False,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "retarget_runtime_policy": EXPECTED_POLICY,
            "retarget_correction_prepared": True,
            "retargeter_dirty_for_exact_save": True,
        },
        "prepare helper",
    )
    validate_pose_evidence(payload, "prepare helper")


def validate_helper(payload: dict[str, Any]) -> None:
    require_exact(
        payload,
        {
            "schema_version": 2,
            "session": "8B",
            "operation": "VALIDATE",
            "status": "PASS",
            "disk_mutation": "NONE",
            "writes": [],
            "attempted_writes": [],
            "errors": [],
            "retarget_op_count": 5,
            "retarget_op_types": EXPECTED_OP_TYPES,
            "retarget_op_enabled_mask": EXPECTED_OP_ENABLED_MASK,
            "run_ik_rig_op_count": 1,
            "run_ik_rig_enabled": False,
            "run_ik_rig_disabled_for_fixed_presentation": True,
            "retarget_runtime_policy": EXPECTED_POLICY,
            "retarget_processor_initialized": True,
            "retarget_output_bone_count": 342,
            "reload_from_disk_verified": True,
            "source_mhc_editor_only": True,
            "preferred_quality_profile_id": "GameplayPerformance",
        },
        "validation helper",
    )
    validate_pose_evidence(payload, "validation helper")


def validate_pose_evidence(payload: dict[str, Any], label: str) -> None:
    if payload.get("retarget_output_pose_plausible") is not True:
        raise CorrectionError(f"{label} did not prove a plausible retarget output pose")
    if payload.get("retarget_output_translation_outlier_count") != 0:
        raise CorrectionError(f"{label} reported retarget translation outliers")
    max_abs = payload.get("retarget_output_max_abs_translation_cm")
    if not isinstance(max_abs, (int, float)) or not 0 <= float(max_abs) <= 1000:
        raise CorrectionError(f"{label} max retarget translation is invalid: {max_abs!r}")
    anchors = payload.get("retarget_output_anchor_transforms")
    if not isinstance(anchors, dict) or set(anchors) != {
        "root",
        "pelvis",
        "spine_01",
        "head",
    }:
        raise CorrectionError(f"{label} retarget anchor set differs: {anchors!r}")


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    project_root = Path(unreal.Paths.project_dir()).resolve()
    content_root = Path(unreal.Paths.project_content_dir()).resolve()
    invalid_report = Path(tempfile.gettempdir()) / "Session8BRetargetRunIK.invalid.json"
    report_path = invalid_report
    state: dict[str, Any] = {
        "schema": SCHEMA,
        "generated_utc": utc_now(),
        "status": "FAIL_NOT_STARTED",
        "phase": "COMMAND_LINE",
        "errors": [],
        "writes": [],
        "attempted_writes": [],
        "asset_save_call_count": 0,
        "retarget_object": RETARGET_OBJECT,
        "expected_policy": EXPECTED_POLICY,
    }
    content_before: dict[str, dict[str, Any]] | None = None
    directories_before: set[str] | None = None
    savegames_before: dict[str, dict[str, Any]] | None = None
    savegame_directories_before: set[str] | None = None
    retarget_bytes_before: bytes | None = None
    retarget_mtime_ns_before: int | None = None
    retargeter: Any | None = None
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
            raise CorrectionError("accepted Fresh2 validation report identity changed")
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
        retarget_path = project_root / RETARGET_RELATIVE
        retarget_base = retarget_path.with_suffix("")
        for suffix in PACKAGE_SUFFIXES[1:]:
            if Path(f"{retarget_base}{suffix}").exists():
                raise CorrectionError(f"unexpected retargeter sidecar exists: {suffix}")
        retarget_bytes_before = retarget_path.read_bytes()
        retarget_mtime_ns_before = retarget_path.stat().st_mtime_ns

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
            "retargeter": record(retarget_path),
            "canonical_assets": CANONICAL_BEFORE,
            "project_savegames": savegames_before,
            "accepted_fresh_validation": record(FRESH_VALIDATION_REPORT),
        }

        state["phase"] = "PREPARE_EXACT_RUN_IK_CHANGE"
        mutation_started = True
        prepare_payload = strict_json_text(
            resolve_prepare_helper()(), "C++ prepare helper"
        )
        state["prepare_result"] = prepare_payload
        validate_prepare(prepare_payload)
        dirty_content, dirty_maps = dirty_names()
        if dirty_content != [RETARGET_PACKAGE] or dirty_maps:
            raise CorrectionError(
                f"prepare dirtied unexpected packages: content={dirty_content}, "
                f"maps={dirty_maps}"
            )

        state["phase"] = "EXACT_RETARGETER_SAVE"
        retargeter = unreal.EditorAssetLibrary.load_asset(RETARGET_OBJECT)
        if retargeter is None or str(retargeter.get_path_name()) != RETARGET_OBJECT:
            raise CorrectionError("canonical retargeter did not load at the exact path")
        state["attempted_writes"] = [RETARGET_OBJECT]
        state["asset_save_call_count"] = 1
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            retargeter, only_if_is_dirty=True
        ):
            raise CorrectionError("UE refused the exact canonical retargeter save")
        state["writes"] = [RETARGET_OBJECT]
        retargeter = None
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
        expected_changed = [RETARGET_RELATIVE.relative_to("Content").as_posix()]
        if changed != expected_changed or added or removed:
            raise CorrectionError(
                f"Content delta differs: changed={changed}, added={added}, removed={removed}"
            )
        if directories_after != directories_before:
            raise CorrectionError("Content directory topology changed")
        if record(retarget_path) == CANONICAL_BEFORE[
            str(RETARGET_RELATIVE).replace("\\", "/")
        ]:
            raise CorrectionError("retargeter bytes did not change")
        for relative, expected in CANONICAL_BEFORE.items():
            if relative != str(RETARGET_RELATIVE).replace("\\", "/"):
                if record(project_root / relative) != expected:
                    raise CorrectionError(f"non-retarget canonical asset changed: {relative}")
        for suffix in PACKAGE_SUFFIXES[1:]:
            if Path(f"{retarget_base}{suffix}").exists():
                raise CorrectionError(f"unexpected retargeter sidecar was created: {suffix}")

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
                "status": "PASS_RETARGET_RUN_IK_DISABLED_AND_RELOAD_VALIDATED",
                "phase": "COMPLETE",
                "disk_mutation": "EXACT_ONE_RETARGETER_WRITE",
                "content_before": CONTENT_BEFORE,
                "content_after": snapshot_summary(content_after),
                "content_changed": changed,
                "content_added": added,
                "content_removed": removed,
                "content_directories_unchanged": True,
                "retargeter_before": CANONICAL_BEFORE[
                    str(RETARGET_RELATIVE).replace("\\", "/")
                ],
                "retargeter_after": record(retarget_path),
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
            and retarget_bytes_before is not None
            and retarget_mtime_ns_before is not None
            and production is not None
            and savegames_root is not None
            and savegames_before is not None
            and savegame_directories_before is not None
            and user_dir is not None
        ):
            retarget_path = project_root / RETARGET_RELATIVE
            try:
                retargeter = None
                unreal.SystemLibrary.collect_garbage()
            except Exception as garbage_error:
                rollback_errors.append(
                    f"rollback garbage collection failed: {garbage_error}"
                )
            restore_temp = retarget_path.with_name(
                f"{retarget_path.name}.session8b-run-ik-rollback.tmp"
            )
            try:
                disk_bytes_changed = (
                    not retarget_path.is_file()
                    or retarget_path.read_bytes() != retarget_bytes_before
                )
                if disk_bytes_changed:
                    restore_temp.write_bytes(retarget_bytes_before)
                    restore_temp.replace(retarget_path)
                if (
                    disk_bytes_changed
                    or retarget_path.stat().st_mtime_ns
                    != retarget_mtime_ns_before
                ):
                    os.utime(
                        retarget_path,
                        ns=(retarget_mtime_ns_before, retarget_mtime_ns_before),
                    )
                for suffix in PACKAGE_SUFFIXES[1:]:
                    sidecar = Path(f"{retarget_path.with_suffix('')}{suffix}")
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
                "DG_SESSION8B_RETARGET_RUN_IK_CORRECTION: report write failed: "
                f"{report_error}"
            )
        unreal.log_error(
            "DG_SESSION8B_RETARGET_RUN_IK_CORRECTION: "
            f"{state['status']} phase={state['phase']} report={report_path}: {exc}"
        )
        raise

    unreal.log(
        "DG_SESSION8B_RETARGET_RUN_IK_CORRECTION: "
        f"{state['status']} report={report_path}"
    )


if __name__ == "__main__":
    main()
