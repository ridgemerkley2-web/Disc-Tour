#!/usr/bin/env python3
"""Source-only validation for the MetaHuman-to-DGMaster retarget scaffold."""

from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import importlib.util
import json
import math
import stat
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).absolute().parents[1]
AUTHOR = PROJECT_ROOT / "Scripts/author_dg_metahuman_to_dgmaster_retargeter.py"
INSPECTOR = PROJECT_ROOT / "Scripts/inspect_dg_metahuman_to_master_inputs.py"
RETARGETER_INSPECTOR = (
    PROJECT_ROOT / "Scripts/inspect_dg_metahuman_to_dgmaster_retargeter.py"
)
HOST_RUNNER = (
    PROJECT_ROOT
    / "Scripts/run_dg_metahuman_to_dgmaster_retargeter_inspection.py"
)
RETAINED_INSPECTION_RECEIPT = (
    PROJECT_ROOT
    / "Evidence/Session19/MetaHumanToDGMasterRetargeterInspectionReceipt.json"
)
DOC = PROJECT_ROOT / "Docs/DG_MOCAP_PIPELINE.md"
RETARGETER = (
    PROJECT_ROOT
    / "Content/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster.uasset"
)
EXPECTED_RETARGETER_BYTES = 14068
EXPECTED_RETARGETER_SHA256 = (
    "B66227838C0857416119BB8A17534A106704081DAA63833F02D99A9AF4F21CD8"
)
EXPECTED_INSPECTION_RECEIPT_BYTES = 31178
EXPECTED_INSPECTION_RECEIPT_SHA256 = (
    "75ED86A657F1634C594A17CC65456324E750F19AB0176DE0CA0AA68FA7C565C3"
)
EXPECTED_INSPECTION_RECEIPT_SCHEMA = (
    "DiscGolfTour.MetaHumanToDGMasterInspectionHostReceipt.v1"
)
EXPECTED_INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"
EXPECTED_INSPECTION_USER_ROOT = Path(
    r"C:\DGTour_TestRuns\MotionRetargetInspection"
).resolve()
EXPECTED_ENGINE_VERSION = {
    "MajorVersion": 5,
    "MinorVersion": 8,
    "PatchVersion": 2,
    "Changelist": 56702186,
    "BranchName": "++UE5+Release-5.8",
}
EXPECTED_ENGINE_BUILD_SHA256 = (
    "FF99FC3DD98E7C7FD2F5700334BC792DFB7BACED3828CDEE32940BB69581A6A4"
)
EXPECTED_PROTECTED_LABELS = (
    "inspected_retargeter",
    "runtime_reverse_retargeter",
    "source_ik_rig",
    "source_preview_mesh",
    "source_skeleton",
    "target_ik_rig",
    "target_preview_mesh",
    "target_skeleton",
)
EXPECTED_PACKAGE_SUFFIXES = (".uasset", ".ubulk", ".uexp", ".uptnl")
EXPECTED_PROTECTED_KEYS = {
    f"{label}{suffix}"
    for label in EXPECTED_PROTECTED_LABELS
    for suffix in EXPECTED_PACKAGE_SUFFIXES
}
EXPECTED_RECEIPT_KEYS = {
    "changed_protected_paths",
    "command",
    "completed_utc",
    "errors",
    "external_user_dir",
    "fatal_log_markers",
    "inspection_switch",
    "inspector_payload",
    "production_approval",
    "project_root",
    "protected_file_mutation",
    "protected_files_after",
    "protected_files_before",
    "protected_files_unchanged",
    "receipt_path",
    "schema",
    "started_utc",
    "status",
    "stdout_log",
    "unreal_engine",
    "unreal_exit_code",
    "unreal_log",
}
EXPECTED_UNPROVED_BOUNDARIES = [
    "SOURCE_HEAD_CHAIN_IS_NOT_MAPPED_TO_AN_INDEPENDENT_TARGET_HEAD_CHAIN",
    "SOURCE_LEG_ENDS_AT_BALL_WHILE_TARGET_LEG_ENDS_AT_FOOT",
    "TARGET_FOOT_CHAINS_ARE_UNMAPPED",
    "ROOT_AND_PELVIS_BEHAVIOR_REQUIRES_A_SOLVED_CLIP_EXPORT_TEST",
    "NO_ANIMATION_OR_CONTACT_QUALITY_APPROVAL",
]
REMAINING_RELEASE_BLOCKERS = [
    "SOURCE_HEAD_CHAIN_UNMAPPED",
    "SOURCE_TARGET_LEG_ENDPOINT_MISMATCH_REQUIRES_CLIP_VALIDATION",
    "TARGET_FOOT_CHAINS_UNMAPPED",
    "ROOT_AND_PELVIS_MOTION_REQUIRE_SOLVED_CLIP_EXPORT_VALIDATION",
]

EXPECTED_MAPPINGS = (
    ("Root", "Root"),
    ("Spine", "Spine"),
    ("Neck", "Neck"),
    ("LeftArm", "Arm_L"),
    ("RightArm", "Arm_R"),
    ("LeftLeg", "Leg_L"),
    ("RightLeg", "Leg_R"),
)
EXPECTED_UNMAPPED = ("Foot_L", "Foot_R")
EXPECTED_UNMAPPED_SOURCE = ("Head",)
EXPECTED_SOURCE_CHAINS = {
    "Root": ("root", "root", "None"),
    "Spine": ("spine_01", "spine_05", "None"),
    "Neck": ("neck_01", "neck_02", "None"),
    "Head": ("head", "head", "None"),
    "LeftArm": ("upperarm_l", "hand_l", "LeftHandIK"),
    "RightArm": ("upperarm_r", "hand_r", "RightHandIK"),
    "LeftLeg": ("thigh_l", "ball_l", "LeftFootIK"),
    "RightLeg": ("thigh_r", "ball_r", "RightFootIK"),
}
EXPECTED_TARGET_CHAINS = {
    "Root": ("root", "root", "None"),
    "Spine": ("spine_01", "spine_04", "None"),
    "Neck": ("neck_01", "head", "None"),
    "Arm_L": ("upperarm_l", "hand_l", "hand_l_Goal"),
    "Arm_R": ("upperarm_r", "hand_r", "hand_r_Goal"),
    "Leg_L": ("thigh_l", "foot_l", "foot_l_Goal"),
    "Leg_R": ("thigh_r", "foot_r", "foot_r_Goal"),
    "Foot_L": ("foot_l", "ball_l", "None"),
    "Foot_R": ("foot_r", "ball_r", "None"),
}


class ValidationError(RuntimeError):
    pass


_HOST_RUNNER_CONTRACT = None


def strict_json_text(text: str, label: str) -> Any:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValidationError(f"{label} contains duplicate key {key!r}")
            result[key] = value
        return result

    def strict_float(raw: str) -> float:
        value = float(raw)
        if not math.isfinite(value):
            raise ValidationError(f"{label} contains non-finite number {raw}")
        return value

    def no_constant(raw: str) -> None:
        raise ValidationError(f"{label} contains non-finite constant {raw}")

    try:
        return json.loads(
            text,
            object_pairs_hook=no_duplicates,
            parse_float=strict_float,
            parse_constant=no_constant,
        )
    except (json.JSONDecodeError, TypeError, ValueError) as exc:
        raise ValidationError(f"{label} is not strict JSON: {exc}") from exc


def is_sha256(value: Any) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and value == value.upper()
        and all(character in "0123456789ABCDEF" for character in value)
    )


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def require_regular_file_mode(mode: int, label: str) -> None:
    if stat.S_ISLNK(mode):
        raise ValidationError(f"{label} is a symbolic link")
    if not stat.S_ISREG(mode):
        raise ValidationError(f"{label} is not a regular file")


def current_protected_record(path_text: str, label: str) -> dict[str, Any]:
    path = Path(path_text)
    try:
        before = path.lstat()
    except FileNotFoundError:
        return {"path": path_text, "present": False}
    except OSError as exc:
        raise ValidationError(f"cannot inspect current {label}: {exc}") from exc
    require_regular_file_mode(before.st_mode, f"current {label}")
    try:
        digest = sha256_path(path)
        after = path.lstat()
    except OSError as exc:
        raise ValidationError(f"cannot hash current {label}: {exc}") from exc
    require_regular_file_mode(after.st_mode, f"current {label}")
    stable_fields_before = (
        before.st_dev,
        before.st_ino,
        before.st_size,
        before.st_mtime_ns,
    )
    stable_fields_after = (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
    )
    if stable_fields_before != stable_fields_after:
        raise ValidationError(f"current {label} changed while it was hashed")
    return {
        "bytes": after.st_size,
        "path": path_text,
        "present": True,
        "sha256": digest,
    }


def validate_current_protected_match(
    key: str,
    retained: dict[str, Any],
    current: dict[str, Any],
) -> None:
    if current != retained:
        raise ValidationError(
            f"current protected path {key} differs from the retained before/after row"
        )


def host_runner_contract():
    global _HOST_RUNNER_CONTRACT
    if _HOST_RUNNER_CONTRACT is not None:
        return _HOST_RUNNER_CONTRACT
    spec = importlib.util.spec_from_file_location(
        "dg_metahuman_to_dgmaster_inspection_host_contract",
        HOST_RUNNER,
    )
    if spec is None or spec.loader is None:
        raise ValidationError("cannot load the retargeter inspection host contract")
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:
        raise ValidationError(f"cannot import the host runner contract: {exc}") from exc
    _HOST_RUNNER_CONTRACT = module
    return module


def validate_protected_record(key: str, row: Any) -> str:
    if not isinstance(row, dict) or type(row.get("present")) is not bool:
        raise ValidationError(f"protected row {key} is malformed")
    present = row["present"]
    expected_keys = (
        {"bytes", "path", "present", "sha256"}
        if present
        else {"path", "present"}
    )
    if set(row) != expected_keys:
        raise ValidationError(f"protected row {key} has an unexpected schema")
    path_text = row.get("path")
    if not isinstance(path_text, str) or not path_text or not Path(path_text).is_absolute():
        raise ValidationError(f"protected row {key} path is not absolute")
    suffix = next(
        (candidate for candidate in EXPECTED_PACKAGE_SUFFIXES if key.endswith(candidate)),
        None,
    )
    if suffix is None or Path(path_text).suffix.casefold() != suffix:
        raise ValidationError(f"protected row {key} path suffix differs")
    if key.endswith(".uasset") and not present:
        raise ValidationError(f"protected main package {key} is absent")
    if present:
        if type(row.get("bytes")) is not int or row["bytes"] <= 0:
            raise ValidationError(f"protected row {key} has invalid byte count")
        if not is_sha256(row.get("sha256")):
            raise ValidationError(f"protected row {key} has invalid SHA-256")
    current = current_protected_record(path_text, f"protected path {key}")
    validate_current_protected_match(key, row, current)
    return str(Path(path_text).resolve()).casefold()


def validate_log_record(label: str, row: Any, user_dir: Path) -> dict[str, Any]:
    if not isinstance(row, dict) or set(row) != {"bytes", "path", "present", "sha256"}:
        raise ValidationError(f"{label} record schema differs")
    if row.get("present") is not True:
        raise ValidationError(f"{label} was not retained by the host run")
    if type(row.get("bytes")) is not int or row["bytes"] <= 0:
        raise ValidationError(f"{label} has invalid byte count")
    if not is_sha256(row.get("sha256")):
        raise ValidationError(f"{label} has invalid SHA-256")
    path_text = row.get("path")
    if not isinstance(path_text, str) or not Path(path_text).is_absolute():
        raise ValidationError(f"{label} path is not absolute")
    path = Path(path_text).resolve()
    if path.parent != user_dir:
        raise ValidationError(f"{label} is outside the receipt UserDir")
    expected_name = (
        "UnrealEditor-Cmd.stdout.log" if label == "stdout log" else "UnrealEditor-Cmd.log"
    )
    if path.name != expected_name:
        raise ValidationError(f"{label} filename differs")
    return {"bytes": row["bytes"], "sha256": row["sha256"], "path": str(path)}


def validate_inspection_receipt_payload(receipt: Any) -> dict[str, Any]:
    if not isinstance(receipt, dict) or set(receipt) != EXPECTED_RECEIPT_KEYS:
        raise ValidationError("retained inspection receipt top-level schema differs")
    if receipt.get("schema") != EXPECTED_INSPECTION_RECEIPT_SCHEMA:
        raise ValidationError("retained inspection receipt schema identifier differs")
    if receipt.get("status") != "PASS_NO_PROTECTED_FILE_WRITES":
        raise ValidationError("retained inspection receipt status is not PASS")
    if receipt.get("errors") != []:
        raise ValidationError("retained inspection receipt has host-run errors")
    if receipt.get("fatal_log_markers") != []:
        raise ValidationError("retained inspection receipt contains fatal log markers")
    if type(receipt.get("unreal_exit_code")) is not int or receipt["unreal_exit_code"] != 0:
        raise ValidationError("retained inspection Unreal exit code is not exact zero")
    if receipt.get("production_approval") is not False:
        raise ValidationError("retained inspection receipt claims production approval")
    if receipt.get("inspection_switch") != EXPECTED_INSPECTION_SWITCH:
        raise ValidationError("retained inspection switch differs")

    project_root_text = receipt.get("project_root")
    if (
        not isinstance(project_root_text, str)
        or not Path(project_root_text).is_absolute()
        or Path(project_root_text).resolve() != PROJECT_ROOT.resolve()
    ):
        raise ValidationError("retained inspection project-root binding differs")
    user_dir_text = receipt.get("external_user_dir")
    if not isinstance(user_dir_text, str) or not Path(user_dir_text).is_absolute():
        raise ValidationError("retained inspection UserDir is not absolute")
    user_dir = Path(user_dir_text).resolve()
    try:
        relative_user_dir = user_dir.relative_to(EXPECTED_INSPECTION_USER_ROOT)
    except ValueError as exc:
        raise ValidationError("retained inspection UserDir is outside its external root") from exc
    if len(relative_user_dir.parts) != 1:
        raise ValidationError("retained inspection UserDir is not one direct UUID child")
    try:
        parsed_user_id = uuid.UUID(relative_user_dir.name)
    except ValueError as exc:
        raise ValidationError("retained inspection UserDir child is not a UUID") from exc
    if str(parsed_user_id) != relative_user_dir.name:
        raise ValidationError("retained inspection UserDir UUID is not canonical lowercase")
    try:
        user_dir.relative_to(PROJECT_ROOT.resolve())
    except ValueError:
        pass
    else:
        raise ValidationError("retained inspection UserDir is inside the project")
    receipt_path_text = receipt.get("receipt_path")
    if (
        not isinstance(receipt_path_text, str)
        or Path(receipt_path_text).resolve()
        != user_dir / "RetargeterInspectionHostReceipt.json"
    ):
        raise ValidationError("retained receipt path/UserDir binding differs")

    timestamps = []
    for field in ("started_utc", "completed_utc"):
        raw = receipt.get(field)
        if not isinstance(raw, str) or not raw.endswith("Z"):
            raise ValidationError(f"retained inspection {field} is not UTC RFC3339")
        try:
            parsed = datetime.fromisoformat(raw[:-1] + "+00:00")
        except ValueError as exc:
            raise ValidationError(f"retained inspection {field} is invalid") from exc
        if parsed.tzinfo != timezone.utc:
            raise ValidationError(f"retained inspection {field} is not UTC")
        timestamps.append(parsed)
    if timestamps[1] < timestamps[0]:
        raise ValidationError("retained inspection completion precedes its start")

    editor = Path(
        r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    ).resolve()
    expected_command = [
        str(editor),
        str(Path(project_root_text).resolve() / "DiscGolfTour.uproject"),
        "-run=PythonScript",
        "-script="
        + str(
            Path(project_root_text).resolve()
            / "Scripts/inspect_dg_metahuman_to_dgmaster_retargeter.py"
        ),
        EXPECTED_INSPECTION_SWITCH,
        f"-UserDir={user_dir}",
        "-unattended",
        "-nop4",
        "-NullRHI",
        "-NoSplash",
        "-NoSound",
        "-NoAssetRegistryCache",
        "-stdout",
        "-FullStdOutLogOutput",
        "-UTF8Output",
        f"-abslog={user_dir / 'UnrealEditor-Cmd.log'}",
    ]
    if receipt.get("command") != expected_command:
        raise ValidationError("retained inspection command-line contract differs")

    engine = receipt.get("unreal_engine")
    expected_engine_keys = set(EXPECTED_ENGINE_VERSION) | {"path", "sha256"}
    if not isinstance(engine, dict) or set(engine) != expected_engine_keys:
        raise ValidationError("retained inspection Unreal version schema differs")
    for key, expected in EXPECTED_ENGINE_VERSION.items():
        if engine.get(key) != expected or (
            isinstance(expected, int) and type(engine.get(key)) is not int
        ):
            raise ValidationError(f"retained inspection Unreal version {key} differs")
    engine_path = engine.get("path")
    expected_engine_path = Path(
        r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
    ).resolve()
    if (
        not isinstance(engine_path, str)
        or Path(engine_path).resolve() != expected_engine_path
        or engine.get("sha256") != EXPECTED_ENGINE_BUILD_SHA256
    ):
        raise ValidationError("retained inspection Unreal Build.version identity differs")

    before = receipt.get("protected_files_before")
    after = receipt.get("protected_files_after")
    if not isinstance(before, dict) or not isinstance(after, dict):
        raise ValidationError("retained inspection protected snapshots are malformed")
    if set(before) != EXPECTED_PROTECTED_KEYS or set(after) != EXPECTED_PROTECTED_KEYS:
        raise ValidationError("retained inspection does not contain exactly 32 protected rows")
    if before != after:
        raise ValidationError("retained inspection protected before/after rows differ")
    if receipt.get("protected_files_unchanged") is not True:
        raise ValidationError("retained inspection lacks the protected-files-unchanged result")
    if receipt.get("changed_protected_paths") != []:
        raise ValidationError("retained inspection reports changed protected paths")
    if receipt.get("protected_file_mutation") != "NONE":
        raise ValidationError("retained inspection reports protected-file mutation")
    resolved_paths = [validate_protected_record(key, before[key]) for key in sorted(before)]
    if len(resolved_paths) != 32 or len(set(resolved_paths)) != 32:
        raise ValidationError("retained inspection protected paths are not 32 unique files")

    stdout_log = validate_log_record("stdout log", receipt.get("stdout_log"), user_dir)
    unreal_log = validate_log_record("Unreal log", receipt.get("unreal_log"), user_dir)
    inspector_payload = receipt.get("inspector_payload")
    contract = host_runner_contract()
    try:
        contract.validate_inspector_payload(inspector_payload, user_dir)
    except Exception as exc:
        raise ValidationError(f"retained inspector payload violates host contract: {exc}") from exc
    if inspector_payload.get("known_unproved_boundaries") != EXPECTED_UNPROVED_BOUNDARIES:
        raise ValidationError("retained inspector payload unproved-boundary list differs")

    return {
        "schema": receipt["schema"],
        "status": receipt["status"],
        "unreal_exit_code": receipt["unreal_exit_code"],
        "unreal_version": dict(EXPECTED_ENGINE_VERSION),
        "external_user_dir": str(user_dir),
        "protected_row_count": len(before),
        "protected_present_count": sum(
            int(row.get("present") is True) for row in before.values()
        ),
        "protected_absent_sidecar_count": sum(
            int(row.get("present") is False) for row in before.values()
        ),
        "protected_files_unchanged": True,
        "protected_current_filesystem_match": True,
        "fatal_log_marker_count": 0,
        "stdout_log": stdout_log,
        "unreal_log": unreal_log,
        "per_operation_mapping_contract_validated": True,
        "operation_count": len(inspector_payload["operations"]),
        "production_approval": False,
    }


def validate_retained_inspection_receipt(
    path: Path = RETAINED_INSPECTION_RECEIPT,
    expected_bytes: int = EXPECTED_INSPECTION_RECEIPT_BYTES,
    expected_sha256: str = EXPECTED_INSPECTION_RECEIPT_SHA256,
) -> dict[str, Any]:
    if not path.is_file():
        raise ValidationError(f"retained inspection receipt is missing: {path}")
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest().upper()
    if len(data) != expected_bytes or digest != expected_sha256:
        raise ValidationError("retained inspection receipt byte identity differs")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValidationError("retained inspection receipt is not strict UTF-8") from exc
    receipt = strict_json_text(text, "retained inspection receipt")
    result = validate_inspection_receipt_payload(receipt)
    result["identity"] = {"bytes": len(data), "sha256": digest}
    return result


def validate_remaining_release_blockers(blockers: Any) -> dict[str, Any]:
    if blockers != REMAINING_RELEASE_BLOCKERS:
        raise ValidationError("remaining retargeter release-blocker set differs")
    if "RETARGETER_PER_OP_SETTINGS_INSPECTION_NOT_YET_EXECUTED" in blockers:
        raise ValidationError("executed retargeter-inspection blocker remains open")
    return {
        "closed_blocker": "RETARGETER_PER_OP_SETTINGS_INSPECTION_NOT_YET_EXECUTED",
        "remaining_blocker_count": len(blockers),
        "remaining_blockers": list(blockers),
    }


def validate_retargeter_file(
    path: Path = RETARGETER,
    expected_bytes: int = EXPECTED_RETARGETER_BYTES,
    expected_sha256: str = EXPECTED_RETARGETER_SHA256,
) -> dict | None:
    if not path.is_file():
        return None
    size = path.stat().st_size
    digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    if size != expected_bytes or digest != expected_sha256:
        raise ValidationError(
            "retargeter package identity differs from the author/cold-reload receipt"
        )
    return {"bytes": size, "sha256": digest}


def literal_assignment(tree: ast.Module, name: str):
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if any(isinstance(target, ast.Name) and target.id == name for target in node.targets):
                return ast.literal_eval(node.value)
    raise ValidationError(f"missing literal assignment: {name}")


def validate_author_source(text: str) -> dict:
    tree = ast.parse(text)
    mappings = tuple(tuple(row) for row in literal_assignment(tree, "CHAIN_MAPPINGS"))
    unmapped = tuple(literal_assignment(tree, "UNMAPPED_TARGET_CHAINS"))
    if mappings != EXPECTED_MAPPINGS:
        raise ValidationError("explicit chain mapping contract differs")
    if unmapped != EXPECTED_UNMAPPED:
        raise ValidationError("explicit unmapped-foot contract differs")
    source_chains = literal_assignment(tree, "EXPECTED_METAHUMAN_CHAINS")
    target_chains = literal_assignment(tree, "EXPECTED_DG_CHAINS")
    if source_chains != EXPECTED_SOURCE_CHAINS:
        raise ValidationError("exact MetaHuman source-chain contract differs")
    if target_chains != EXPECTED_TARGET_CHAINS:
        raise ValidationError("exact DGMaster target-chain contract differs")

    required = (
        'AUTHOR_ENV = "DG_METAHUMAN_TO_DGMASTER_AUTHORING"',
        "RTG_MetaHuman_To_DGMaster",
        "IK_MH_IKRig.IK_MH_IKRig",
        "IK_DG_Master.IK_DG_Master",
        "AutoMapChainType.CLEAR",
        'get_editor_property("chain_name")',
        'get_editor_property("ik_goal_name")',
        "PASS_ALREADY_CURRENT_NO_ASSET_WRITES",
        "PASS_AUTHORED_METAHUMAN_TO_DGMASTER_RETARGETER",
        "EXACTLY_ONE_NEW_RETARGETER_PACKAGE",
        '"production_approval": False',
        "protected_snapshot() != before",
        "rollback_created_target()",
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise ValidationError(f"author source is missing tokens: {missing}")
    forbidden = (
        "AutoMapChainType.FUZZY",
        "RTG_DGMaster_To_MetaHuman",
        '"production_approval": True',
        "save_directory(",
        "duplicate_asset(",
        ".chain_name",
        ".ik_goal_name",
    )
    present = [token for token in forbidden if token in text]
    if present:
        raise ValidationError(f"author source contains forbidden tokens: {present}")
    if text.count("create_asset(") != 1:
        raise ValidationError("author must have exactly one asset creation call")
    if text.count("save_loaded_asset(") != 1:
        raise ValidationError("author must have exactly one asset save call")
    return {
        "mapping_count": len(mappings),
        "source_chain_contract_count": len(source_chains),
        "target_chain_contract_count": len(target_chains),
        "unmapped_target_chains": list(unmapped),
        "unmapped_source_chains": list(EXPECTED_UNMAPPED_SOURCE),
        "source_target_endpoint_mismatches": [
            {"source": "LeftLeg:ball_l", "target": "Leg_L:foot_l"},
            {"source": "RightLeg:ball_r", "target": "Leg_R:foot_r"},
            {"source": "Neck:neck_02", "target": "Neck:head"},
        ],
        "op_settings_explicitly_authored": "set_settings(" in text,
        "per_op_mapping_validation": False,
        "asset_create_call_count": 1,
        "asset_save_call_count": 1,
    }


def validate_inspector_source(text: str) -> dict:
    ast.parse(text)
    required = (
        "PASS_READ_ONLY_INPUT_INSPECTION",
        "NO_CREATE_NO_MODIFY_NO_SAVE_NO_RENAME_NO_DELETE",
        "IK_MH_IKRig.IK_MH_IKRig",
        "IK_DG_Master.IK_DG_Master",
        "SKM_MHC_DG_Golfer_Default_BodyMesh",
        "SK_DG_Master.SK_DG_Master",
        'get_editor_property("chain_name")',
        'get_editor_property("ik_goal_name")',
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise ValidationError(f"inspector source is missing tokens: {missing}")
    forbidden = (
        "create_asset(",
        "save_loaded_asset(",
        "delete_asset(",
        ".chain_name",
        ".ik_goal_name",
    )
    present = [token for token in forbidden if token in text]
    if present:
        raise ValidationError(f"inspector source contains mutation calls: {present}")
    return {"mutation_calls": 0}


def validate_retargeter_inspector_source(text: str) -> dict:
    ast.parse(text)
    required = (
        "INSPECTION_COMPLETE_RELEASE_BLOCKED",
        "NO_CREATE_NO_MODIFY_NO_SAVE_NO_RENAME_NO_DELETE",
        "IKRetargetPelvisMotionController",
        "IKRetargetFKChainsController",
        "IKRetargetRunIKRigController",
        "IKRetargetRootMotionController",
        "IKRetargetCurveRemapController",
        'get_editor_property("floor_constraint_weight")',
        'get_editor_property("root_motion_source")',
        'get_editor_property("root_height_source")',
        '"propagate_to_non_retargeted_children"',
        'controller.get_source_chain(target, operation["name"])',
        'INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"',
        'EXPECTED_USER_ROOT = Path(',
        'unreal.SystemLibrary.get_command_line()',
        'validate_invocation(command_line)',
        'required_once = (INSPECTION_SWITCH, "-unattended", "-nop4", "-NullRHI")',
        'Path(script_values[0]).resolve() != SCRIPT_PATH',
        'user_dir.relative_to(EXPECTED_USER_ROOT)',
        'controller.get_ik_rig(source_side)',
        'controller.get_ik_rig(target_side)',
        'controller.get_preview_mesh(source_side)',
        'controller.get_preview_mesh(target_side)',
        'controller.get_all_target_ik_rigs()',
        'EXPECTED_MAPPING_BY_TARGET',
        'if mapping != EXPECTED_MAPPING_BY_TARGET:',
        'operation_names = set()',
        'if not op_name or normalized_name == "none" or op_name != op_name.strip():',
        'if normalized_name in operation_names:',
        'operation_names.add(normalized_name)',
        'if not enabled:',
        'operation_names_unique_nonempty',
        'per_operation_mapping_contract_validated',
        'math.isfinite(float(value))',
        'json.dumps(payload, sort_keys=True, allow_nan=False)',
        "SOURCE_HEAD_CHAIN_IS_NOT_MAPPED_TO_AN_INDEPENDENT_TARGET_HEAD_CHAIN",
        "SOURCE_LEG_ENDS_AT_BALL_WHILE_TARGET_LEG_ENDS_AT_FOOT",
        "TARGET_FOOT_CHAINS_ARE_UNMAPPED",
        "ROOT_AND_PELVIS_BEHAVIOR_REQUIRES_A_SOLVED_CLIP_EXPORT_TEST",
        '"production_approval": False',
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise ValidationError(
            f"retargeter inspector source is missing tokens: {missing}"
        )
    forbidden = (
        "create_asset(",
        "save_loaded_asset(",
        "save_asset(",
        "delete_asset(",
        "rename_asset(",
        "duplicate_asset(",
        "set_settings(",
        "set_source_chain(",
        "set_retarget_op_enabled(",
        ".modify(",
    )
    present = [token for token in forbidden if token in text]
    if present:
        raise ValidationError(
            f"retargeter inspector source contains mutation calls: {present}"
        )
    return {
        "mutation_calls": 0,
        "source_supports_per_op_mapping_inspection": True,
        "per_op_mapping_fail_closed": True,
        "exact_rig_and_preview_bindings": True,
        "strict_external_invocation": True,
        "operation_names_unique_nonempty": True,
        "numeric_values_finite": True,
        "source_supports_pelvis_and_root_motion_settings_inspection": True,
        "quality_approval": False,
    }


def validate_host_runner_source(text: str) -> dict:
    ast.parse(text)
    required = (
        'INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"',
        'USER_ROOT = Path(r"C:\\DGTour_TestRuns\\MotionRetargetInspection")',
        'mode.add_argument(',
        '"--execute"',
        '"--self-test"',
        '"PatchVersion": 2',
        '"Changelist": 56702186',
        'Unreal build does not match the reflected 5.8.2 contract',
        '"-run=PythonScript"',
        'f"-script={INSPECTOR}"',
        'f"-UserDir={user_dir}"',
        '"-unattended"',
        '"-nop4"',
        '"-NullRHI"',
        'protected_snapshot(editor, require_main=True)',
        'protected_snapshot(editor, require_main=False)',
        'changed_paths(before, after)',
        'PASS_NO_PROTECTED_FILE_WRITES',
        'RetargeterInspectionHostReceipt.json',
        'FATAL_LOG_MARKERS = (',
        'fatal_markers = fatal_log_markers(stdout_text, unreal_text)',
        '"fatal_log_markers": fatal_markers',
        'detect_fatal_log_marker',
        'json.dumps(payload, indent=2, sort_keys=True, allow_nan=False)',
        'validate_inspector_payload(inspector_payload, user_dir)',
        'reject_wrong_per_op_mapping',
        'detect_protected_hash_drift',
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise ValidationError(f"host runner source is missing tokens: {missing}")
    forbidden = (
        "unreal.EditorAssetLibrary",
        "save_loaded_asset(",
        "save_asset(",
        "delete_asset(",
        "rename_asset(",
        "duplicate_asset(",
    )
    present = [token for token in forbidden if token in text]
    if present:
        raise ValidationError(f"host runner source contains asset mutation calls: {present}")
    return {
        "requires_explicit_execute": True,
        "external_uuid_user_dir": True,
        "protected_package_sidecar_hash_guard": True,
        "external_receipt": True,
        "unreal_launched_by_self_test": False,
    }


def validate_doc(text: str) -> dict:
    required = (
        "## MetaHuman-to-DGMaster retarget contract",
        "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster",
        "DG_METAHUMAN_TO_DGMASTER_AUTHORING=1",
        "Foot_L",
        "Foot_R",
        "explicitly unmapped",
        "source `Head` chain",
        "source legs end at `ball_l` and `ball_r`",
        "MetaHumanToDGMasterRetargeterInspectionReceipt.json",
        "31,178 bytes",
        "zero fatal markers and zero drift",
        "does not approve animation quality or production use",
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise ValidationError(f"pipeline documentation is missing tokens: {missing}")
    return {"retarget_contract_documented": True}


def validate() -> dict:
    for path in (AUTHOR, INSPECTOR, RETARGETER_INSPECTOR, HOST_RUNNER, DOC):
        if not path.is_file():
            raise ValidationError(f"required source file is missing: {path}")
    retargeter_identity = validate_retargeter_file()
    retained_inspection = validate_retained_inspection_receipt()
    return {
        "status": (
            "PASS_IDENTITY_AND_LIMITED_CONTRACT_RELEASE_BLOCKED"
            if retargeter_identity is not None
            else "PASS_SOURCE_SCAFFOLD_READY_RETARGETER_AUTHORING_PENDING"
        ),
        "disk_mutation": "NONE",
        "retargeter_asset_present": retargeter_identity is not None,
        "retargeter_asset_identity": retargeter_identity,
        "planned_retargeter": (
            "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster."
            "RTG_MetaHuman_To_DGMaster"
        ),
        "author": validate_author_source(AUTHOR.read_text(encoding="utf-8")),
        "inspector": validate_inspector_source(INSPECTOR.read_text(encoding="utf-8")),
        "retargeter_inspector": validate_retargeter_inspector_source(
            RETARGETER_INSPECTOR.read_text(encoding="utf-8")
        ),
        "retargeter_inspection_host_runner": validate_host_runner_source(
            HOST_RUNNER.read_text(encoding="utf-8")
        ),
        "retained_retargeter_inspection": retained_inspection,
        "documentation": validate_doc(DOC.read_text(encoding="utf-8")),
        "release_blockers": validate_remaining_release_blockers(
            list(REMAINING_RELEASE_BLOCKERS)
        )["remaining_blockers"],
        "production_approval": False,
    }


def self_test() -> dict:
    author = AUTHOR.read_text(encoding="utf-8")
    inspector = INSPECTOR.read_text(encoding="utf-8")
    retargeter_inspector = RETARGETER_INSPECTOR.read_text(encoding="utf-8")
    host_runner = HOST_RUNNER.read_text(encoding="utf-8")
    doc = DOC.read_text(encoding="utf-8")
    receipt = strict_json_text(
        RETAINED_INSPECTION_RECEIPT.read_text(encoding="utf-8"),
        "self-test retained inspection receipt",
    )

    def mutated_receipt(mutator) -> dict[str, Any]:
        value = copy.deepcopy(receipt)
        mutator(value)
        return validate_inspection_receipt_payload(value)

    synthetic_present = {
        "bytes": 3,
        "path": r"C:\synthetic\asset.uasset",
        "present": True,
        "sha256": "A" * 64,
    }
    synthetic_absent = {
        "path": r"C:\synthetic\asset.uexp",
        "present": False,
    }
    cases = [
        ("author_current", lambda: validate_author_source(author), True),
        ("inspector_current", lambda: validate_inspector_source(inspector), True),
        (
            "retargeter_inspector_current",
            lambda: validate_retargeter_inspector_source(retargeter_inspector),
            True,
        ),
        (
            "retargeter_inspection_host_runner_current",
            lambda: validate_host_runner_source(host_runner),
            True,
        ),
        ("doc_current", lambda: validate_doc(doc), True),
        ("retargeter_identity_current", lambda: validate_retargeter_file(), True),
        (
            "retained_inspection_receipt_current",
            lambda: validate_retained_inspection_receipt(),
            True,
        ),
        (
            "retained_inspection_receipt_semantics_current",
            lambda: validate_inspection_receipt_payload(receipt),
            True,
        ),
        (
            "remaining_release_blockers_exact",
            lambda: validate_remaining_release_blockers(
                list(REMAINING_RELEASE_BLOCKERS)
            ),
            True,
        ),
        (
            "accept_current_protected_exact_match",
            lambda: validate_current_protected_match(
                "synthetic.uasset",
                synthetic_present,
                copy.deepcopy(synthetic_present),
            ),
            True,
        ),
        (
            "reject_current_protected_missing_main",
            lambda: validate_current_protected_match(
                "synthetic.uasset",
                synthetic_present,
                {"path": synthetic_present["path"], "present": False},
            ),
            False,
        ),
        (
            "reject_current_absent_sidecar_materialized",
            lambda: validate_current_protected_match(
                "synthetic.uexp",
                synthetic_absent,
                {
                    "bytes": 1,
                    "path": synthetic_absent["path"],
                    "present": True,
                    "sha256": "B" * 64,
                },
            ),
            False,
        ),
        (
            "reject_current_protected_hash_drift",
            lambda: validate_current_protected_match(
                "synthetic.uasset",
                synthetic_present,
                {**synthetic_present, "sha256": "B" * 64},
            ),
            False,
        ),
        (
            "reject_current_protected_symlink_mode",
            lambda: require_regular_file_mode(
                stat.S_IFLNK | 0o777,
                "synthetic protected path",
            ),
            False,
        ),
        (
            "reject_current_protected_directory_mode",
            lambda: require_regular_file_mode(
                stat.S_IFDIR | 0o755,
                "synthetic protected path",
            ),
            False,
        ),
        (
            "reject_retained_receipt_byte_identity_mismatch",
            lambda: validate_retained_inspection_receipt(
                expected_bytes=EXPECTED_INSPECTION_RECEIPT_BYTES + 1
            ),
            False,
        ),
        (
            "reject_retained_receipt_hash_identity_mismatch",
            lambda: validate_retained_inspection_receipt(expected_sha256="0" * 64),
            False,
        ),
        (
            "reject_retained_receipt_schema",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("schema", "Wrong.v1")
            ),
            False,
        ),
        (
            "reject_retained_receipt_status",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("status", "FAIL")
            ),
            False,
        ),
        (
            "reject_retained_receipt_errors",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("errors", ["synthetic"])
            ),
            False,
        ),
        (
            "reject_retained_receipt_fatal_log_marker",
            lambda: mutated_receipt(
                lambda value: value.__setitem__(
                    "fatal_log_markers",
                    [{"source": "stdout", "line": "LogPython: Error: synthetic"}],
                )
            ),
            False,
        ),
        (
            "reject_retained_receipt_exit_code",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("unreal_exit_code", 1)
            ),
            False,
        ),
        (
            "reject_retained_receipt_ue_version",
            lambda: mutated_receipt(
                lambda value: value["unreal_engine"].__setitem__("PatchVersion", 3)
            ),
            False,
        ),
        (
            "reject_retained_receipt_missing_protected_row",
            lambda: mutated_receipt(
                lambda value: value["protected_files_before"].pop(
                    "target_skeleton.uptnl"
                )
            ),
            False,
        ),
        (
            "reject_retained_receipt_protected_hash_drift",
            lambda: mutated_receipt(
                lambda value: value["protected_files_after"][
                    "target_ik_rig.uasset"
                ].__setitem__("sha256", "0" * 64)
            ),
            False,
        ),
        (
            "reject_retained_receipt_changed_path_claim",
            lambda: mutated_receipt(
                lambda value: value.__setitem__(
                    "changed_protected_paths", ["target_ik_rig.uasset"]
                )
            ),
            False,
        ),
        (
            "reject_retained_receipt_unchanged_false",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("protected_files_unchanged", False)
            ),
            False,
        ),
        (
            "reject_retained_receipt_production_claim",
            lambda: mutated_receipt(
                lambda value: value.__setitem__("production_approval", True)
            ),
            False,
        ),
        (
            "reject_retained_receipt_wrong_per_op_mapping",
            lambda: mutated_receipt(
                lambda value: value["inspector_payload"]["mapping_by_operation"][
                    "FK Chains"
                ].__setitem__("Arm_L", "RightArm")
            ),
            False,
        ),
        (
            "reject_retained_receipt_removed_unproved_boundary",
            lambda: mutated_receipt(
                lambda value: value["inspector_payload"][
                    "known_unproved_boundaries"
                ].pop()
            ),
            False,
        ),
        (
            "reject_reopened_executed_inspection_blocker",
            lambda: validate_remaining_release_blockers(
                [
                    "RETARGETER_PER_OP_SETTINGS_INSPECTION_NOT_YET_EXECUTED",
                    *REMAINING_RELEASE_BLOCKERS,
                ]
            ),
            False,
        ),
        (
            "reject_strict_receipt_duplicate_key",
            lambda: strict_json_text('{"schema":1,"schema":2}', "duplicate"),
            False,
        ),
        (
            "reject_strict_receipt_nonfinite_constant",
            lambda: strict_json_text('{"value":NaN}', "nonfinite"),
            False,
        ),
        (
            "reject_strict_receipt_float_overflow",
            lambda: strict_json_text('{"value":1e9999}', "overflow"),
            False,
        ),
        (
            "reject_retargeter_byte_identity_mismatch",
            lambda: validate_retargeter_file(
                expected_bytes=EXPECTED_RETARGETER_BYTES + 1
            ),
            False,
        ),
        (
            "reject_retargeter_hash_identity_mismatch",
            lambda: validate_retargeter_file(expected_sha256="0" * 64),
            False,
        ),
        (
            "reject_fuzzy",
            lambda: validate_author_source(
                author.replace("AutoMapChainType.CLEAR", "AutoMapChainType.FUZZY")
            ),
            False,
        ),
        (
            "reject_runtime_retarget_mutation",
            lambda: validate_author_source(author + "\nRTG_DGMaster_To_MetaHuman\n"),
            False,
        ),
        (
            "reject_production_claim",
            lambda: validate_author_source(
                author.replace('"production_approval": False', '"production_approval": True')
            ),
            False,
        ),
        (
            "reject_missing_author_gate",
            lambda: validate_author_source(
                author.replace(
                    'AUTHOR_ENV = "DG_METAHUMAN_TO_DGMASTER_AUTHORING"',
                    'AUTHOR_ENV = ""',
                )
            ),
            False,
        ),
        (
            "reject_changed_mapping",
            lambda: validate_author_source(
                author.replace('(\"LeftArm\", \"Arm_L\")', '(\"LeftArm\", \"Arm_R\")')
            ),
            False,
        ),
        (
            "reject_changed_source_chain_endpoint",
            lambda: validate_author_source(
                author.replace(
                    '"Spine": ("spine_01", "spine_05", "None")',
                    '"Spine": ("spine_01", "spine_04", "None")',
                )
            ),
            False,
        ),
        (
            "reject_changed_target_chain_goal",
            lambda: validate_author_source(
                author.replace(
                    '"Arm_L": ("upperarm_l", "hand_l", "hand_l_Goal")',
                    '"Arm_L": ("upperarm_l", "hand_l", "None")',
                )
            ),
            False,
        ),
        (
            "reject_foot_mapping",
            lambda: validate_author_source(
                author.replace(
                    'UNMAPPED_TARGET_CHAINS = ("Foot_L", "Foot_R")',
                    'UNMAPPED_TARGET_CHAINS = ("Foot_L",)',
                )
            ),
            False,
        ),
        (
            "reject_extra_create",
            lambda: validate_author_source(author + "\n# create_asset(\n"),
            False,
        ),
        (
            "reject_inspector_save",
            lambda: validate_inspector_source(inspector + "\n# save_loaded_asset(\n"),
            False,
        ),
        (
            "reject_retargeter_inspector_save",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector + "\n# save_loaded_asset(\n"
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_missing_root_policy",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    'get_editor_property("root_motion_source")',
                    'get_editor_property("unused_motion_source")',
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_missing_head_boundary",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    "SOURCE_HEAD_CHAIN_IS_NOT_MAPPED_TO_AN_INDEPENDENT_TARGET_HEAD_CHAIN",
                    "HEAD_BOUNDARY_REMOVED",
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_mapping_report_only",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    "if mapping != EXPECTED_MAPPING_BY_TARGET:",
                    "if False:",
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_without_finite_guard",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    "math.isfinite(float(value))",
                    "True",
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_without_strict_switch",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    'INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"',
                    'INSPECTION_SWITCH = ""',
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_without_unique_name_guard",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    "if normalized_name in operation_names:",
                    "if False:",
                )
            ),
            False,
        ),
        (
            "reject_retargeter_inspector_without_source_rig_binding",
            lambda: validate_retargeter_inspector_source(
                retargeter_inspector.replace(
                    "controller.get_ik_rig(source_side)",
                    "None",
                )
            ),
            False,
        ),
        (
            "reject_host_runner_without_execute_gate",
            lambda: validate_host_runner_source(
                host_runner.replace('"--execute"', '"--run-now"')
            ),
            False,
        ),
        (
            "reject_host_runner_without_post_snapshot",
            lambda: validate_host_runner_source(
                host_runner.replace(
                    "protected_snapshot(editor, require_main=False)",
                    "{}",
                )
            ),
            False,
        ),
        (
            "reject_host_runner_without_fatal_log_scan",
            lambda: validate_host_runner_source(
                host_runner.replace(
                    "fatal_markers = fatal_log_markers(stdout_text, unreal_text)",
                    "fatal_markers = []",
                )
            ),
            False,
        ),
        (
            "reject_host_runner_asset_mutation",
            lambda: validate_host_runner_source(
                host_runner + "\n# save_loaded_asset(\n"
            ),
            False,
        ),
        (
            "reject_inspector_direct_struct_attribute",
            lambda: validate_inspector_source(
                inspector.replace(
                    'chain.get_editor_property("chain_name")',
                    "chain.chain_name",
                )
            ),
            False,
        ),
        (
            "reject_author_direct_struct_attribute",
            lambda: validate_author_source(
                author.replace(
                    'chain.get_editor_property("chain_name")',
                    "chain.chain_name",
                )
            ),
            False,
        ),
        (
            "reject_doc_without_author_gate",
            lambda: validate_doc(doc.replace("DG_METAHUMAN_TO_DGMASTER_AUTHORING=1", "")),
            False,
        ),
    ]
    passed = 0
    rows = []
    for name, action, should_pass in cases:
        accepted = True
        try:
            action()
        except (ValidationError, SyntaxError, ValueError):
            accepted = False
        ok = accepted == should_pass
        passed += int(ok)
        rows.append({"name": name, "pass": ok})
    if passed != len(cases):
        failed = [row["name"] for row in rows if not row["pass"]]
        raise ValidationError(
            f"self-test passed {passed}/{len(cases)}; failed={failed}"
        )
    return {"status": "PASS_SELF_TEST", "passed": passed, "total": len(cases), "cases": rows}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-asset", action="store_true")
    args = parser.parse_args()
    try:
        payload = self_test() if args.self_test else validate()
        if args.require_asset and not payload.get("retargeter_asset_present", False):
            raise ValidationError("retargeter asset is required but has not been authored")
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0
    except (ValidationError, SyntaxError, ValueError) as exc:
        print(
            json.dumps(
                {"status": "FAIL", "error": str(exc), "disk_mutation": "NONE"},
                sort_keys=True,
            )
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
