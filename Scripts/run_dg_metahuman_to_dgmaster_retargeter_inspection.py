#!/usr/bin/env python3
"""Run the retargeter inspector with an external UserDir and no-write guard.

This host-side launcher is deliberately inert unless ``--execute`` is passed.
It creates one external UUID UserDir, snapshots every protected package and
possible package sidecar before and after Unreal, and writes its receipt only
inside that external directory.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).absolute().parents[1].resolve()
EXPECTED_PROJECT_ROOT = Path(r"C:\DGTour").resolve()
PROJECT_FILE = PROJECT_ROOT / "DiscGolfTour.uproject"
INSPECTOR = (
    PROJECT_ROOT / "Scripts/inspect_dg_metahuman_to_dgmaster_retargeter.py"
)
DEFAULT_UNREAL_EDITOR_CMD = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
USER_ROOT = Path(r"C:\DGTour_TestRuns\MotionRetargetInspection")
INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"
PAYLOAD_MARKER = "DG_METAHUMAN_TO_DGMASTER_INSPECTION="
RECEIPT_SCHEMA = "DiscGolfTour.MetaHumanToDGMasterInspectionHostReceipt.v1"
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
FATAL_LOG_MARKERS = (
    "fatal error",
    "unhandled exception",
    "traceback (most recent call last)",
    "assertion failed",
    "ensure condition failed",
    "logpython: error",
    "logwindows: error",
    "logcore: error",
)
EXPECTED_ENGINE_VERSION = {
    "MajorVersion": 5,
    "MinorVersion": 8,
    "PatchVersion": 2,
    "Changelist": 56702186,
    "BranchName": "++UE5+Release-5.8",
}

EXPECTED_CONTROLLERS = (
    "IKRetargetPelvisMotionController",
    "IKRetargetFKChainsController",
    "IKRetargetRunIKRigController",
    "IKRetargetRootMotionController",
    "IKRetargetCurveRemapController",
)
EXPECTED_MAPPING_BY_TARGET = {
    "Root": "Root",
    "Spine": "Spine",
    "Neck": "Neck",
    "Arm_L": "LeftArm",
    "Arm_R": "RightArm",
    "Leg_L": "LeftLeg",
    "Leg_R": "RightLeg",
    "Foot_L": "None",
    "Foot_R": "None",
}
EXPECTED_BINDINGS = {
    "retargeter": (
        "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster."
        "RTG_MetaHuman_To_DGMaster"
    ),
    "source_ik_rig": (
        "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig."
        "IK_MH_IKRig"
    ),
    "target_ik_rig": "/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master",
    "source_preview_mesh": (
        "/Game/DiscGolf/Characters/MetaHuman/Generated/"
        "MHC_DG_Golfer_Default/Body/SKM_MHC_DG_Golfer_Default_BodyMesh."
        "SKM_MHC_DG_Golfer_Default_BodyMesh"
    ),
    "target_preview_mesh": (
        "/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master"
    ),
}
PROJECT_PROTECTED_PACKAGES = {
    "inspected_retargeter": (
        "Content/DiscGolf/Animation/Mocap/Rigs/"
        "RTG_MetaHuman_To_DGMaster.uasset"
    ),
    "source_preview_mesh": (
        "Content/DiscGolf/Characters/MetaHuman/Generated/"
        "MHC_DG_Golfer_Default/Body/"
        "SKM_MHC_DG_Golfer_Default_BodyMesh.uasset"
    ),
    "source_skeleton": (
        "Content/DiscGolf/Characters/MetaHuman/Common/Female/Medium/"
        "NormalWeight/Body/metahuman_base_skel.uasset"
    ),
    "target_preview_mesh": (
        "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset"
    ),
    "target_skeleton": (
        "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset"
    ),
    "target_ik_rig": "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    "runtime_reverse_retargeter": (
        "Content/DiscGolf/Characters/MetaHuman/"
        "RTG_DGMaster_To_MetaHuman.uasset"
    ),
}


class RunnerError(RuntimeError):
    """Raised for a fail-closed host inspection contract violation."""


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace(
        "+00:00", "Z"
    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict[str, Any]:
    record: dict[str, Any] = {"path": str(path), "present": path.exists()}
    if not record["present"]:
        return record
    if not path.is_file() or path.is_symlink():
        raise RunnerError(f"protected path is not a regular file: {path}")
    record.update({"bytes": path.stat().st_size, "sha256": sha256(path)})
    return record


def protected_main_paths(editor: Path) -> dict[str, Path]:
    engine_root = editor.parents[2]
    result = {
        label: PROJECT_ROOT / relative
        for label, relative in PROJECT_PROTECTED_PACKAGES.items()
    }
    result["source_ik_rig"] = (
        engine_root
        / "Plugins/MetaHuman/MetaHumanCharacter/Content/Animation/"
        "Retargeting/IK_MH_IKRig.uasset"
    )
    return result


def protected_snapshot(editor: Path, require_main: bool) -> dict[str, dict[str, Any]]:
    rows: dict[str, dict[str, Any]] = {}
    for label, main_path in protected_main_paths(editor).items():
        if require_main and not main_path.is_file():
            raise RunnerError(f"required protected package is missing: {main_path}")
        stem = main_path.with_suffix("")
        for suffix in PACKAGE_SUFFIXES:
            candidate = stem.with_suffix(suffix)
            rows[f"{label}{suffix}"] = file_record(candidate)
    return rows


def changed_paths(
    before: dict[str, dict[str, Any]], after: dict[str, dict[str, Any]]
) -> list[str]:
    return sorted(
        key for key in set(before) | set(after) if before.get(key) != after.get(key)
    )


def strict_json_text(text: str, label: str) -> Any:
    def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise RunnerError(f"{label} contains duplicate key {key!r}")
            result[key] = value
        return result

    def no_constant(value: str) -> None:
        raise RunnerError(f"{label} contains non-finite constant {value}")

    try:
        return json.loads(
            text,
            object_pairs_hook=no_duplicates,
            parse_constant=no_constant,
        )
    except (json.JSONDecodeError, TypeError) as exc:
        raise RunnerError(f"{label} is not strict JSON: {exc}") from exc


def require_finite_numbers(value: Any, label: str = "payload") -> None:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return
    if isinstance(value, (int, float)):
        if not math.isfinite(float(value)):
            raise RunnerError(f"{label} contains a non-finite number")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            require_finite_numbers(item, f"{label}.{key}")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            require_finite_numbers(item, f"{label}[{index}]")
        return
    raise RunnerError(f"{label} contains unsupported type {type(value).__name__}")


def extract_payload(log_text: str) -> dict[str, Any]:
    encoded = []
    for line in log_text.splitlines():
        if PAYLOAD_MARKER in line:
            encoded.append(line.split(PAYLOAD_MARKER, 1)[1].strip())
    if len(encoded) != 1:
        raise RunnerError(
            f"expected exactly one inspector payload, found {len(encoded)}"
        )
    payload = strict_json_text(encoded[0], "inspector payload")
    if not isinstance(payload, dict):
        raise RunnerError("inspector payload must be a JSON object")
    return payload


def fatal_log_markers(*log_texts: str) -> list[dict[str, Any]]:
    hits: list[dict[str, Any]] = []
    seen: set[tuple[str, tuple[str, ...]]] = set()
    for log_text in log_texts:
        for line in log_text.splitlines():
            folded = line.casefold()
            matched = tuple(marker for marker in FATAL_LOG_MARKERS if marker in folded)
            if not matched:
                continue
            normalized_line = line.strip()[:2000]
            key = (normalized_line, matched)
            if key in seen:
                continue
            seen.add(key)
            hits.append({"markers": list(matched), "line": normalized_line})
    return hits


def validate_inspector_payload(payload: dict[str, Any], user_dir: Path) -> None:
    if payload.get("status") != "INSPECTION_COMPLETE_RELEASE_BLOCKED":
        raise RunnerError("inspector status differs from the release-blocked contract")
    if payload.get("disk_mutation") != "NONE":
        raise RunnerError("inspector did not report zero disk mutation")
    if payload.get("invocation_switch") != INSPECTION_SWITCH:
        raise RunnerError("inspector invocation-switch receipt differs")
    reported_user_dir = payload.get("external_user_dir")
    if not isinstance(reported_user_dir, str) or Path(reported_user_dir).resolve() != user_dir:
        raise RunnerError("inspector external UserDir binding differs")
    for key, expected in EXPECTED_BINDINGS.items():
        if payload.get(key) != expected:
            raise RunnerError(f"inspector binding {key} differs")
    if payload.get("op_target_ik_rigs") != [EXPECTED_BINDINGS["target_ik_rig"]]:
        raise RunnerError("operation target IK Rig set differs")

    operations = payload.get("operations")
    if not isinstance(operations, list) or len(operations) != len(EXPECTED_CONTROLLERS):
        raise RunnerError("inspector operation list length differs")
    names: list[str] = []
    mapping_op_names: list[str] = []
    for index, (row, expected_class) in enumerate(zip(operations, EXPECTED_CONTROLLERS)):
        if not isinstance(row, dict):
            raise RunnerError(f"operation {index} is not an object")
        name = row.get("name")
        if (
            row.get("index") != index
            or row.get("controller_class") != expected_class
            or row.get("enabled") is not True
            or not isinstance(name, str)
            or not name
            or name.casefold() == "none"
            or name != name.strip()
        ):
            raise RunnerError(f"operation {index} differs from its frozen contract")
        names.append(name)
        if index in (1, 2):
            mapping_op_names.append(name)
    if len({name.casefold() for name in names}) != len(names):
        raise RunnerError("operation names are not case-insensitively unique")
    if payload.get("operation_names_unique_nonempty") is not True:
        raise RunnerError("operation-name integrity receipt is absent")

    mappings = payload.get("mapping_by_operation")
    if not isinstance(mappings, dict) or set(mappings) != set(mapping_op_names):
        raise RunnerError("per-operation mapping set differs")
    for op_name in mapping_op_names:
        if mappings.get(op_name) != EXPECTED_MAPPING_BY_TARGET:
            raise RunnerError(f"per-operation mapping differs for {op_name!r}")
    if payload.get("per_operation_mapping_contract_validated") is not True:
        raise RunnerError("per-operation mapping validation receipt is absent")
    settings = payload.get("settings")
    if not isinstance(settings, dict) or set(settings) != set(names):
        raise RunnerError("operation settings records are missing or collided")
    if payload.get("numeric_values_finite") is not True:
        raise RunnerError("finite-number receipt is absent")
    if payload.get("production_approval") is not False:
        raise RunnerError("inspector crossed the production-approval boundary")
    require_finite_numbers(payload)


def log_record(path: Path) -> dict[str, Any]:
    return file_record(path)


def write_receipt(path: Path, payload: dict[str, Any]) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(path)


def validate_host_environment(editor: Path) -> tuple[Path, dict[str, Any]]:
    if PROJECT_ROOT != EXPECTED_PROJECT_ROOT:
        raise RunnerError(
            f"project root {PROJECT_ROOT} is not {EXPECTED_PROJECT_ROOT}"
        )
    if not PROJECT_FILE.is_file() or not INSPECTOR.is_file():
        raise RunnerError("project file or inspector source is missing")
    editor = editor.resolve()
    if editor.name.casefold() != "unrealeditor-cmd.exe" or not editor.is_file():
        raise RunnerError("--unreal-editor-cmd must name an existing UnrealEditor-Cmd.exe")
    engine_root = editor.parents[2]
    if editor.parent != engine_root / "Binaries/Win64":
        raise RunnerError("UnrealEditor-Cmd.exe is outside the installed Engine layout")
    build_version_path = engine_root / "Build/Build.version"
    if not build_version_path.is_file():
        raise RunnerError(f"Unreal Build.version is missing: {build_version_path}")
    build_version = strict_json_text(
        build_version_path.read_text(encoding="utf-8-sig", errors="strict"),
        "Unreal Build.version",
    )
    if not isinstance(build_version, dict):
        raise RunnerError("Unreal Build.version must be a JSON object")
    differences = {
        key: {"actual": build_version.get(key), "expected": expected}
        for key, expected in EXPECTED_ENGINE_VERSION.items()
        if build_version.get(key) != expected
    }
    if differences:
        raise RunnerError(
            f"Unreal build does not match the reflected 5.8.2 contract: {differences}"
        )
    return editor, {
        "path": str(build_version_path),
        **{key: build_version[key] for key in EXPECTED_ENGINE_VERSION},
        "sha256": sha256(build_version_path),
    }


def launch(editor: Path, timeout_seconds: int) -> tuple[int, Path]:
    editor, engine_version = validate_host_environment(editor)
    if timeout_seconds < 30 or timeout_seconds > 3600:
        raise RunnerError("--timeout-seconds must be between 30 and 3600")
    user_root = USER_ROOT.resolve()
    try:
        user_root.relative_to(PROJECT_ROOT)
    except ValueError:
        pass
    else:
        raise RunnerError("host UserDir root must remain external to the project")
    user_root.mkdir(parents=True, exist_ok=True)
    user_dir = user_root / str(uuid.uuid4())
    user_dir.mkdir(exist_ok=False)

    receipt_path = user_dir / "RetargeterInspectionHostReceipt.json"
    stdout_path = user_dir / "UnrealEditor-Cmd.stdout.log"
    unreal_log_path = user_dir / "UnrealEditor-Cmd.log"
    started = utc_now()
    errors: list[str] = []
    return_code: int | None = None
    inspector_payload: dict[str, Any] | None = None
    fatal_markers: list[dict[str, Any]] = []
    before: dict[str, dict[str, Any]] = {}
    after: dict[str, dict[str, Any]] = {}
    command = [
        str(editor),
        str(PROJECT_FILE),
        "-run=PythonScript",
        f"-script={INSPECTOR}",
        INSPECTION_SWITCH,
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
        f"-abslog={unreal_log_path}",
    ]

    try:
        before = protected_snapshot(editor, require_main=True)
        with stdout_path.open("w", encoding="utf-8", newline="\n") as output:
            try:
                completed = subprocess.run(
                    command,
                    cwd=PROJECT_ROOT,
                    stdin=subprocess.DEVNULL,
                    stdout=output,
                    stderr=subprocess.STDOUT,
                    text=True,
                    timeout=timeout_seconds,
                    check=False,
                )
                return_code = completed.returncode
            except subprocess.TimeoutExpired:
                errors.append(f"Unreal timed out after {timeout_seconds} seconds")
        after = protected_snapshot(editor, require_main=False)
        if return_code != 0:
            errors.append(f"Unreal exit code was {return_code!r}, expected 0")
        drift = changed_paths(before, after)
        if drift:
            errors.append(f"protected package bytes/presence changed: {drift}")
        stdout_text = stdout_path.read_text(encoding="utf-8", errors="replace")
        unreal_text = (
            unreal_log_path.read_text(encoding="utf-8", errors="replace")
            if unreal_log_path.is_file()
            else ""
        )
        fatal_markers = fatal_log_markers(stdout_text, unreal_text)
        if fatal_markers:
            errors.append(
                f"Unreal logs contain fatal error markers: {fatal_markers}"
            )
        log_text = stdout_text if PAYLOAD_MARKER in stdout_text else unreal_text
        try:
            inspector_payload = extract_payload(log_text)
            validate_inspector_payload(inspector_payload, user_dir)
        except RunnerError as exc:
            errors.append(str(exc))
    except (OSError, RunnerError, subprocess.SubprocessError) as exc:
        errors.append(str(exc))
        try:
            after = protected_snapshot(editor, require_main=False)
        except (OSError, RunnerError) as snapshot_exc:
            errors.append(f"post-run protected snapshot failed: {snapshot_exc}")

    drift = changed_paths(before, after) if before and after else ["SNAPSHOT_INCOMPLETE"]
    receipt = {
        "schema": RECEIPT_SCHEMA,
        "status": "PASS_NO_PROTECTED_FILE_WRITES" if not errors else "FAIL",
        "started_utc": started,
        "completed_utc": utc_now(),
        "project_root": str(PROJECT_ROOT),
        "external_user_dir": str(user_dir),
        "receipt_path": str(receipt_path),
        "inspection_switch": INSPECTION_SWITCH,
        "unreal_engine": engine_version,
        "command": command,
        "unreal_exit_code": return_code,
        "protected_files_before": before,
        "protected_files_after": after,
        "protected_files_unchanged": not drift,
        "changed_protected_paths": drift,
        "protected_file_mutation": "NONE" if not drift else "DRIFT_DETECTED",
        "stdout_log": log_record(stdout_path),
        "unreal_log": log_record(unreal_log_path),
        "fatal_log_markers": fatal_markers,
        "inspector_payload": inspector_payload,
        "errors": errors,
        "production_approval": False,
    }
    write_receipt(receipt_path, receipt)
    print(
        json.dumps(
            {
                "status": receipt["status"],
                "receipt": str(receipt_path),
                "errors": errors,
            },
            sort_keys=True,
            allow_nan=False,
        )
    )
    return (0 if not errors else 1), receipt_path


def sample_payload(user_dir: Path) -> dict[str, Any]:
    names = ["Pelvis", "FK", "IK", "RootMotion", "Curves"]
    operations = [
        {
            "index": index,
            "name": names[index],
            "controller_class": controller,
            "enabled": True,
        }
        for index, controller in enumerate(EXPECTED_CONTROLLERS)
    ]
    return {
        "status": "INSPECTION_COMPLETE_RELEASE_BLOCKED",
        "disk_mutation": "NONE",
        "invocation_switch": INSPECTION_SWITCH,
        "external_user_dir": str(user_dir),
        **EXPECTED_BINDINGS,
        "op_target_ik_rigs": [EXPECTED_BINDINGS["target_ik_rig"]],
        "operations": operations,
        "operation_names_unique_nonempty": True,
        "settings": {name: {"value": 0.0} for name in names},
        "mapping_by_operation": {
            names[1]: dict(EXPECTED_MAPPING_BY_TARGET),
            names[2]: dict(EXPECTED_MAPPING_BY_TARGET),
        },
        "per_operation_mapping_contract_validated": True,
        "numeric_values_finite": True,
        "production_approval": False,
    }


def self_test() -> dict[str, Any]:
    user_dir = USER_ROOT.resolve() / "11111111-1111-4111-8111-111111111111"
    baseline = sample_payload(user_dir)
    cases: list[tuple[str, Any, bool]] = [
        ("accept_current_payload", baseline, True),
        (
            "reject_wrong_binding",
            {**baseline, "target_ik_rig": "/Game/Wrong.Wrong"},
            False,
        ),
        (
            "reject_nonfinite",
            {**baseline, "settings": {**baseline["settings"], "FK": {"x": math.nan}}},
            False,
        ),
    ]
    duplicate = copy.deepcopy(baseline)
    duplicate["operations"][2]["name"] = duplicate["operations"][1]["name"]
    cases.append(("reject_duplicate_op_name", duplicate, False))
    wrong_mapping = copy.deepcopy(baseline)
    wrong_mapping["mapping_by_operation"]["FK"]["Arm_L"] = "RightArm"
    cases.append(("reject_wrong_per_op_mapping", wrong_mapping, False))

    rows = []
    for name, payload, should_pass in cases:
        accepted = True
        try:
            validate_inspector_payload(payload, user_dir)
        except RunnerError:
            accepted = False
        rows.append({"name": name, "pass": accepted == should_pass})
    strict_cases = (
        ("reject_duplicate_json_key", '{"x":1,"x":2}'),
        ("reject_nonfinite_json", '{"x":NaN}'),
    )
    for name, text in strict_cases:
        rejected = False
        try:
            strict_json_text(text, name)
        except RunnerError:
            rejected = True
        rows.append({"name": name, "pass": rejected})
    drift = changed_paths(
        {"asset.uasset": {"present": True, "sha256": "A"}},
        {"asset.uasset": {"present": True, "sha256": "B"}},
    )
    rows.append({"name": "detect_protected_hash_drift", "pass": bool(drift)})
    rows.append(
        {
            "name": "detect_fatal_log_marker",
            "pass": fatal_log_markers(
                "LogPython: Error: synthetic inspection failure"
            )
            == [
                {
                    "markers": ["logpython: error"],
                    "line": "LogPython: Error: synthetic inspection failure",
                }
            ],
        }
    )
    passed = sum(int(row["pass"]) for row in rows)
    if passed != len(rows):
        raise RunnerError(f"host runner self-test passed {passed}/{len(rows)}")
    return {"status": "PASS_SELF_TEST", "passed": passed, "total": len(rows), "cases": rows}


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--execute",
        action="store_true",
        help="Launch the guarded Unreal inspection.",
    )
    mode.add_argument(
        "--self-test",
        action="store_true",
        help="Run host-only adversarial tests; never launches Unreal.",
    )
    parser.add_argument(
        "--unreal-editor-cmd",
        type=Path,
        default=DEFAULT_UNREAL_EDITOR_CMD,
    )
    parser.add_argument("--timeout-seconds", type=int, default=900)
    args = parser.parse_args()
    try:
        if args.self_test:
            print(json.dumps(self_test(), indent=2, sort_keys=True, allow_nan=False))
            return 0
        return launch(args.unreal_editor_cmd, args.timeout_seconds)[0]
    except (OSError, RunnerError, ValueError) as exc:
        print(
            json.dumps(
                {"status": "FAIL", "error": str(exc), "unreal_launched": False},
                sort_keys=True,
            )
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
