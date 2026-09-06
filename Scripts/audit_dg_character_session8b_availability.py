#!/usr/bin/env python3
"""Read-only Session 8B availability checkpoint after canonical authoring.

The accepted Session 8A audit remains untouched. This successor reuses all of
its engine/plugin/cook/brand checks while classifying the exact Session 8B
editor source, 261 Optimized-Medium Generated/Common packages, and three
canonical runtime assets. It additionally binds the one-retargeter Run IK
policy correction and its newer fresh-process validation. Asset presence is
necessary but not a ready claim: corrected-asset recook/package closure,
runtime visuals, and packaged Omen performance remain explicit blockers.

The audit reads the frozen successful author transaction plus a caller-supplied
fresh-process validation report. It never starts Unreal, UBT, a browser, or a
network request. Exit code 2 is the expected honest BLOCKED checkpoint; exit
code 1 means evidence or filesystem integrity failed.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
from typing import Any
import uuid


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SESSION8A_AUDIT = PROJECT_ROOT / "Scripts/audit_dg_character_session8_availability.py"
DEFAULT_REPORT = PROJECT_ROOT / "Saved/CharacterFramework/Session8BAvailabilityAudit.json"

SCHEMA = "DiscGolfTour.Session8BAvailabilityAudit.v3"
SOURCE_PACKAGE = "/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default"
SOURCE_OBJECT_PATH = SOURCE_PACKAGE + ".MHC_DG_Golfer_Default"
SOURCE_FILE = (
    PROJECT_ROOT
    / "Content/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default.uasset"
)
SOURCE_IDENTITY = {
    "bytes": 112_982_127,
    "sha256": "1AA1EFBFDD882308959D1429D562DD2D92F64317219660003063D0AEF17BCFAB",
}

METAHUMAN_ROOT = PROJECT_ROOT / "Content/DiscGolf/Characters/MetaHuman"
GENERATED_ROOT = METAHUMAN_ROOT / "Generated"
COMMON_ROOT = METAHUMAN_ROOT / "Common"
ASSEMBLY_PARTITION = {
    "common_files": 205,
    "generated_files": 56,
    "file_count": 261,
    "bytes": 376_200_391,
    "manifest_sha256": "42BD0396180C5BF3021FBB9B618BEE61F07CDF2CF084BA026B408D081076AA23",
}
ACCEPTED_PRE_CANONICAL_CONTENT = {
    "file_count": 950,
    "bytes": 2_919_678_243,
    "manifest_sha256": "79EA7D1AC3B0A97ADD2C6D3AFD0A588C71A669DAB000DCEAE426A156497B6A34",
}

GENERATED_BLUEPRINT_PACKAGE = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/"
    "MHC_DG_Golfer_Default/BP_MHC_DG_Golfer_Default"
)
RUNTIME_OBJECTS = (
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
    "BP_DG_MetaHuman_Default",
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman",
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default."
    "DA_DG_AvatarBackend_MetaHuman_Default",
)
RUNTIME_PACKAGES = tuple(path.split(".", 1)[0] for path in RUNTIME_OBJECTS)
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")
CHAIN_MAPPING = {
    "Head": "Neck",
    "LeftArm": "Arm_L",
    "LeftLeg": "Leg_L",
    "Neck": "Neck",
    "RightArm": "Arm_R",
    "RightLeg": "Leg_R",
    "Root": "Root",
    "Spine": "Spine",
}
EXPECTED_QUALITY_PROFILE = "GameplayPerformance"

HISTORICAL_AUTHOR_RUN_ID = "e19140e1-2182-4e8c-b2a3-dfd8b608c8b1"
HISTORICAL_AUTHOR_REPORT = {
    "bytes": 17_195,
    "sha256": "527919070C79FD26157B0F45501EEBEEFF3B620FE2B7EF9B727A0ACDD1879BFD",
}
HISTORICAL_AUTHOR_LOG = {
    "bytes": 262_099,
    "sha256": "5FFF8295840239BFAE8F9CFC34CC50C07B2520AF61D2C7689D4891621C288847",
}
QUALITY_CORRECTION_RUN_ID = "ce98855c-5fd3-4b08-94d3-5953d58017f6"
QUALITY_CORRECTION_REPORT = {
    "bytes": 7_997,
    "sha256": "D6F88283457B9DDEE4578D9F5F4E1DA957FA4581EDF00AB7E3E62895CB798344",
}
QUALITY_CORRECTION_LOG = {
    "bytes": 261_277,
    "sha256": "56283CCBD08E94A8D043BE94445542924F8FCF196D9F7C401FE9969F00193DE6",
}
STALE_PROFILE_IDENTITY = {
    "bytes": 2_179,
    "sha256": "D825DB875B079F9AAA0D99F5FFA7596A13EACBBB4424079829E3D473F706071F",
}
CORRECTED_PROFILE_IDENTITY = {
    "bytes": 2_274,
    "sha256": "8D2B6F25EE629CA04FFF630BC8A3CD9030CB34D161A5955866C1BB208217DC2D",
}
CORRECTED_CONTENT = {
    "file_count": 953,
    "bytes": 2_919_929_450,
    "manifest_sha256": "AA94DC98207FFFD78E51A7418E3367CB1DD33455D702B06DC6A67735B79728AE",
}
PRE_RETARGET_CORRECTION_CONTENT = {
    "file_count": 953,
    "bytes": 2_920_025_165,
    "manifest_sha256": "4306208A7347B8924DF8D1F3590B82FA605AC582529F4B2BC2F8ADED6E6194F2",
}
CURRENT_CONTENT = {
    "file_count": 953,
    "bytes": 2_920_024_342,
    "manifest_sha256": "56DF1ED51ED32858141B6312BCD43E81E260C215137946E1F5395CED61762C0B",
}
CURRENT_ASSEMBLY_PARTITION = {
    "file_count": 950,
    "bytes": 2_919_773_958,
    "manifest_sha256": "CFA6EE7443BA141C770E02052460BE69989941C3F924F4FF54FA26B27D802E22",
}
CURRENT_ASSEMBLY_WITHOUT_COOK_MANIFEST = {
    "file_count": 949,
    "bytes": 2_919_652_760,
    "manifest_sha256": "658627B46C343E83308D1407FC33F9F6518D9F5149E832AD754FB9183F4DDBFE",
}
CURRENT_COOK_MANIFEST_IDENTITY = {
    "bytes": 121_198,
    "sha256": "438CA66DEC96AB7F7A19AC551C5FCFE9AEE6DDCDCC8E8432B475F0FD85AF30BE",
}
STALE_RETARGET_IDENTITY = {
    "bytes": 23_463,
    "sha256": "51208403C91B72F490746AD2EDAA182F74B345CAD4B54D426A1C2C1441D74792",
}
CURRENT_RETARGET_IDENTITY = {
    "bytes": 22_640,
    "sha256": "6316D573F271EE40BD7D3C42258723F9C8669A5606A0FB0373016DF3EBFBEAE9",
}
RETARGET_CORRECTION_RUN_ID = "5fcde026-faa0-4c6b-aeba-bfbb27f978e6"
RETARGET_CORRECTION_REPORT = {
    "bytes": 18_573,
    "sha256": "3B09DC2CDB0A0831F534CF3B21DAADF29237B2B84D8CA62149A913DF58074ACB",
}
RETARGET_CORRECTION_LOG = {
    "bytes": 261_088,
    "sha256": "7076641FB000BA32A2E386077B11CD7EE753637BD1BCBC9132A1857CBA4CD900",
}
EXPECTED_RETARGET_OP_TYPES = [
    "IKRetargetPelvisMotionOp",
    "IKRetargetFKChainsOp",
    "IKRetargetRunIKRigOp",
    "IKRetargetRootMotionOp",
    "IKRetargetCurveRemapOp",
]
EXPECTED_RETARGET_OP_ENABLED_MASK = [True, True, False, True, True]
EXPECTED_RETARGET_POLICY = "PELVIS_FK_ROOT_CURVES_ENABLED_RUN_IK_DISABLED"
EXPECTED_RETARGET_OUTPUT_MAX_ABS_TRANSLATION_CM = 164.71923439615676
EXPECTED_RETARGET_ANCHORS = {"root", "pelvis", "spine_01", "head"}
CURRENT_VALIDATION_RUN_ID = "dc8962f0-bc36-409b-853e-54dfee62f75c"
CURRENT_VALIDATION_REPORT = {
    "bytes": 11_215,
    "sha256": "CD3BD3D09C748D6E907287129939D91033299E89FEE4D186B458839F7AC82FA8",
}
CURRENT_VALIDATION_LOG = {
    "bytes": 259_666,
    "sha256": "3D7322AF9E7963566B170F3E69B22BEC1F21224A98DE59D88C5673B77286050D",
}
COOK_SPEC = PROJECT_ROOT / "Config/DG_RuntimeCookManifest.json"
COOK_SPEC_IDENTITY = {
    "bytes": 12_977,
    "sha256": "1B37EDF5A13EFDEA9243A97E2E733477AA4B6D6394AC82CAB6FA4FA6651BCB09",
}
HISTORICAL_COOK_DIAGNOSTIC_PREFIXES = (
    "Frozen cook source spec hash differs:",
    "Cook spec schema identity/version differs",
    "Cook spec must forbid MetaHuman fabrication by the cook author",
)

BASE_EXPECTED_BLOCKERS = {
    "NO_PROJECT_METAHUMAN_NAMED_PACKAGES",
    "NO_CANONICAL_METAHUMAN_BACKEND_PROFILE",
    "NO_CANONICAL_ASSEMBLED_METAHUMAN_VISUAL_ACTOR",
    "NO_CANONICAL_DGMASTER_TO_METAHUMAN_RETARGET",
}
EXPECTED_BLOCKERS = {
    "SESSION8B_POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE_NOT_ACCEPTED",
    "SESSION8B_RUNTIME_VISUAL_EVIDENCE_NOT_ACCEPTED",
    "SESSION8B_PACKAGED_OMEN_PERFORMANCE_NOT_MEASURED",
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _file_record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"exists": False, "bytes": 0, "sha256": ""}
    return {
        "exists": True,
        "bytes": path.stat().st_size,
        "sha256": _sha256(path),
        "modified_utc": datetime.fromtimestamp(
            path.stat().st_mtime, timezone.utc
        ).isoformat(),
    }


def _identity(record: dict[str, Any]) -> dict[str, Any]:
    return {
        "exists": bool(record.get("exists")),
        "bytes": int(record.get("bytes", 0)),
        "sha256": str(record.get("sha256", "")).upper(),
    }


def _expected_identity(expected: dict[str, Any]) -> dict[str, Any]:
    return {
        "exists": True,
        "bytes": expected["bytes"],
        "sha256": expected["sha256"],
    }


def _retarget_policy_and_pose_ok(payload: dict[str, Any]) -> bool:
    anchors = payload.get("retarget_output_anchor_transforms")
    return (
        payload.get("schema_version") == 2
        and payload.get("retarget_op_count") == 5
        and payload.get("retarget_op_types") == EXPECTED_RETARGET_OP_TYPES
        and payload.get("retarget_op_enabled_mask")
        == EXPECTED_RETARGET_OP_ENABLED_MASK
        and payload.get("run_ik_rig_op_count") == 1
        and payload.get("run_ik_rig_enabled") is False
        and payload.get("run_ik_rig_disabled_for_fixed_presentation") is True
        and payload.get("retarget_runtime_policy") == EXPECTED_RETARGET_POLICY
        and payload.get("retarget_processor_initialized") is True
        and payload.get("retarget_output_bone_count") == 342
        and payload.get("retarget_output_pose_plausible") is True
        and payload.get("retarget_output_translation_outlier_count") == 0
        and payload.get("retarget_output_max_abs_translation_cm")
        == EXPECTED_RETARGET_OUTPUT_MAX_ABS_TRANSLATION_CM
        and isinstance(anchors, dict)
        and set(anchors) == EXPECTED_RETARGET_ANCHORS
    )


def _package_to_file(package: str) -> Path:
    if not package.startswith("/Game/"):
        raise ValueError(f"package escaped /Game: {package}")
    return PROJECT_ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def _load_json(path: Path, label: str, errors: list[str]) -> dict[str, Any]:
    if not path.is_file():
        errors.append(f"{label} missing: {path}")
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{label} invalid JSON: {exc}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{label} root is not an object")
        return {}
    return value


def _snapshot_summary(roots: tuple[Path, ...], relative_to: Path) -> dict[str, Any]:
    files = sorted(
        (
            path
            for root in roots
            if root.is_dir()
            for path in root.rglob("*")
            if path.is_file()
        ),
        key=lambda path: path.relative_to(relative_to).as_posix(),
    )
    digest = hashlib.sha256()
    total_bytes = 0
    for path in files:
        relative = path.relative_to(relative_to).as_posix()
        size = path.stat().st_size
        total_bytes += size
        digest.update(f"{relative}\t{size}\t{_sha256(path)}\n".encode("utf-8"))
    return {
        "file_count": len(files),
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest().upper(),
    }


def _load_session8a_module(errors: list[str]) -> Any | None:
    if not SESSION8A_AUDIT.is_file():
        errors.append(f"accepted Session 8A availability audit missing: {SESSION8A_AUDIT}")
        return None
    spec = importlib.util.spec_from_file_location(
        "dg_session8a_availability_accepted", SESSION8A_AUDIT
    )
    if spec is None or spec.loader is None:
        errors.append("accepted Session 8A availability audit could not be loaded")
        return None
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:  # pragma: no cover - diagnostic path
        errors.append(f"accepted Session 8A availability import failed: {exc}")
        return None
    return module


def _is_classified_session8b_package(package: str) -> bool:
    return (
        package == SOURCE_PACKAGE
        or package.startswith("/Game/DiscGolf/Characters/MetaHuman/Generated/")
        or package.startswith("/Game/DiscGolf/Characters/MetaHuman/Common/")
        or package in RUNTIME_PACKAGES
    )


def _run_session8a_with_exact_session8b_classification(
    engine_root: str | Path | None,
    errors: list[str],
) -> tuple[dict[str, Any], list[str]]:
    """Reuse every accepted 8A check while hiding only classified S8B assets."""

    module = _load_session8a_module(errors)
    if module is None:
        return {}, []
    original_project_packages = module._project_packages
    all_packages = original_project_packages()
    module._project_packages = lambda: [
        package
        for package in original_project_packages()
        if not _is_classified_session8b_package(package)
    ]
    try:
        result = module.audit(engine_root, write_report=False)
    except Exception as exc:  # pragma: no cover - diagnostic path
        errors.append(f"accepted Session 8A successor execution failed: {exc}")
        result = {}
    finally:
        module._project_packages = original_project_packages
    return result, all_packages


def _external_uuid_report(
    report_path: Path,
    expected_name: str,
    label: str,
    errors: list[str],
) -> tuple[str, Path]:
    try:
        report_path.relative_to(PROJECT_ROOT)
        errors.append(f"{label} must remain external to the project tree")
    except ValueError:
        pass
    if (
        report_path.name != expected_name
        or report_path.parent.name != "CharacterFramework"
        or report_path.parent.parent.name != "Saved"
    ):
        errors.append(f"{label} is not <UUID>/Saved/CharacterFramework/{expected_name}")
        return "", report_path.parent
    run_root = report_path.parents[2]
    try:
        run_id = str(uuid.UUID(run_root.name))
    except ValueError:
        run_id = ""
        errors.append(f"{label} run directory is not a canonical UUID")
    return run_id, run_root


def _log_has_severity_failure(text: str) -> bool:
    return bool(re.search(r"(?:^|\n).*\b(?:Warning|Error|Fatal):", text)) or any(
        token in text
        for token in (
            "Unhandled Exception",
            "EXCEPTION_ACCESS_VIOLATION",
            "Assertion failed",
            "Ensure condition failed",
            "Critical error",
        )
    )


def _validate_historical_author_transaction(
    report_path: Path,
    log_path: Path,
    errors: list[str],
) -> dict[str, Any]:
    report_path = report_path.expanduser().resolve()
    log_path = log_path.expanduser().resolve()
    run_id, run_root = _external_uuid_report(
        report_path,
        "Session8BMetaHumanRuntimeAssetAuthor.json",
        "historical canonical author report",
        errors,
    )
    report_record = _file_record(report_path)
    log_record = _file_record(log_path)
    if run_id != HISTORICAL_AUTHOR_RUN_ID:
        errors.append(f"historical author UUID differs: {run_id!r}")
    if _identity(report_record) != _expected_identity(HISTORICAL_AUTHOR_REPORT):
        errors.append(f"historical author report identity differs: {report_record}")
    if _identity(log_record) != _expected_identity(HISTORICAL_AUTHOR_LOG):
        errors.append(f"historical author log identity differs: {log_record}")
    if log_path != PROJECT_ROOT / "Saved/Logs/Session8B_MetaHumanRuntimeAssetsAuthor_Retry7.log":
        errors.append("historical author log path differs")

    payload = _load_json(report_path, "historical canonical author report", errors)
    author = payload.get("author_result", {})
    validate = payload.get("validation_result", {})
    delta = payload.get("disk_delta", {})
    expected_added = sorted(
        package.removeprefix("/Game/") + ".uasset" for package in RUNTIME_PACKAGES
    )
    contract_ok = (
        payload.get("schema") == "DiscGolfTour.Session8BMetaHumanRuntimeAssetAuthor.v1"
        and payload.get("status") == "PASS_AUTHORED_AND_HELPER_RELOAD_VALIDATED"
        and payload.get("phase") == "COMPLETE"
        and payload.get("errors") == []
        and payload.get("canonical_objects") == list(RUNTIME_OBJECTS)
        and author.get("status") == "PASS"
        and author.get("operation") == "AUTHOR"
        and author.get("disk_mutation") == "EXACT_THREE_ASSET_WRITES"
        and author.get("writes") == list(RUNTIME_OBJECTS)
        and author.get("attempted_writes") == list(RUNTIME_OBJECTS)
        and author.get("reload_from_disk_verified") is True
        and author.get("reloaded_canonical_package_count") == 3
        and author.get("on_disk_registry_asset_count") == 3
        and author.get("wrapper_contract_verified") is True
        and author.get("profile_contract_verified") is True
        and author.get("retarget_processor_initialized") is True
        and author.get("retarget_output_bone_count") == 342
        and author.get("chain_mapping") == CHAIN_MAPPING
        and author.get("clean_shaven") is True
        and author.get("hair_s_clean_present") is True
        and author.get("outfit_present") is True
        and author.get("source_mhc_editor_only") is True
        and validate.get("status") == "PASS"
        and validate.get("operation") == "VALIDATE"
        and validate.get("disk_mutation") == "NONE"
        and validate.get("writes") == []
        and validate.get("attempted_writes") == []
        and sorted(delta.get("content_added", [])) == expected_added
        and delta.get("content_changed") == []
        and delta.get("content_removed") == []
        and delta.get("external_save_files") == []
        and payload.get("production_save_unchanged") is True
        and payload.get("accepted_backup_unchanged") is True
    )
    if not contract_ok:
        errors.append("historical canonical author transaction contract differs")

    log_text = (
        log_path.read_text(encoding="utf-8", errors="replace")
        if log_path.is_file()
        else ""
    )
    log_ok = (
        "DG_SESSION8B_METAHUMAN_RUNTIME_ASSETS: "
        "PASS_AUTHORED_AND_HELPER_RELOAD_VALIDATED" in log_text
        and "Success - 0 error(s), 0 warning(s)" in log_text
        and log_text.count("LogSavePackage") == 6
        and not _log_has_severity_failure(log_text)
    )
    if not log_ok:
        errors.append("historical canonical author log success/save contract differs")
    if (
        report_path.is_file()
        and log_path.is_file()
        and log_path.stat().st_mtime < report_path.stat().st_mtime
    ):
        errors.append("historical author log is older than its report")

    return {
        "status": "VERIFIED_EXACT_THREE_ASSET_AUTHOR_TRANSACTION",
        "run_id": run_id,
        "run_root": str(run_root),
        "report_path": str(report_path),
        "report_file": report_record,
        "log_path": str(log_path),
        "log_file": log_record,
        "writes": author.get("writes"),
        "content_added": delta.get("content_added"),
        "historical_profile_quality_note": (
            "RETRY7_PRECEDED_OPTIMIZED_MEDIUM_QUALITY_METADATA_CORRECTION;"
            "CURRENT_FRESH_VALIDATION_IS_AUTHORITATIVE"
        ),
        "success_markers_verified": log_ok,
    }


def _validate_quality_correction(
    report_path: Path,
    log_path: Path,
    errors: list[str],
) -> dict[str, Any]:
    report_path = report_path.expanduser().resolve()
    log_path = log_path.expanduser().resolve()
    run_id, run_root = _external_uuid_report(
        report_path,
        "Session8BMetaHumanProfileQualityCorrection.json",
        "profile-quality correction report",
        errors,
    )
    report_record = _file_record(report_path)
    log_record = _file_record(log_path)
    if run_id != QUALITY_CORRECTION_RUN_ID:
        errors.append(f"profile-quality correction UUID differs: {run_id!r}")
    if _identity(report_record) != _expected_identity(QUALITY_CORRECTION_REPORT):
        errors.append(f"profile-quality correction report identity differs: {report_record}")
    if _identity(log_record) != _expected_identity(QUALITY_CORRECTION_LOG):
        errors.append(f"profile-quality correction log identity differs: {log_record}")
    if log_path != PROJECT_ROOT / "Saved/Logs/Session8B_MetaHumanProfileQualityCorrection.log":
        errors.append("profile-quality correction log path differs")

    payload = _load_json(report_path, "profile-quality correction report", errors)
    helper = payload.get("helper_result", {})
    profile_relative = (
        "DiscGolf/Characters/Avatar/Data/"
        "DA_DG_AvatarBackend_MetaHuman_Default.uasset"
    )
    contract_ok = (
        payload.get("schema")
        == "DiscGolfTour.Session8BMetaHumanProfileQualityCorrection.v1"
        and payload.get("status")
        == "PASS_PROFILE_QUALITY_CORRECTED_AND_RELOAD_VALIDATED"
        and payload.get("phase") == "COMPLETE"
        and payload.get("errors") == []
        and payload.get("disk_mutation") == "EXACT_ONE_PROFILE_QUALITY_WRITE"
        and payload.get("old_quality") == "GameplayHigh"
        and payload.get("expected_quality") == EXPECTED_QUALITY_PROFILE
        and payload.get("content_before", {}).get("manifest_sha256")
        == "78BA11D1A5B18310FF719CEBD9AE84EF2AA58B5CDC0B73873EA8013A5CD908EF"
        and payload.get("content_after") == CORRECTED_CONTENT
        and payload.get("content_changed") == [profile_relative]
        and payload.get("content_added") == []
        and payload.get("content_removed") == []
        and payload.get("content_directories_unchanged") is True
        and payload.get("profile_before")
        == _expected_identity(STALE_PROFILE_IDENTITY)
        and payload.get("profile_after")
        == _expected_identity(CORRECTED_PROFILE_IDENTITY)
        and payload.get("project_savegames_unchanged") is True
        and payload.get("production_save_unchanged") is True
        and payload.get("accepted_backup_unchanged") is True
        and payload.get("external_save_files") == []
        and helper.get("status") == "PASS"
        and helper.get("operation") == "VALIDATE"
        and helper.get("disk_mutation") == "NONE"
        and helper.get("writes") == []
        and helper.get("attempted_writes") == []
        and helper.get("canonical_packages_absent_before_reload") is True
        and helper.get("reload_from_disk_verified") is True
        and helper.get("preferred_quality_profile_id") == EXPECTED_QUALITY_PROFILE
        and helper.get("assembly_pipeline") == "UE_OPTIMIZED"
        and helper.get("assembly_optimization_level") == "MEDIUM"
        and helper.get("character_instance_storage_contract")
        == "NO_REFLECTED_PROPERTY_FIXED_ASSEMBLED_ACTOR"
        and helper.get("character_instance_contract_verified") is True
        and helper.get("orientation_requires_runtime_visual_validation") is True
    )
    if not contract_ok:
        errors.append("profile-quality correction contract differs")

    log_text = (
        log_path.read_text(encoding="utf-8", errors="replace")
        if log_path.is_file()
        else ""
    )
    log_ok = (
        "DG_SESSION8B_PROFILE_QUALITY_CORRECTION: "
        "PASS_PROFILE_QUALITY_CORRECTED_AND_RELOAD_VALIDATED" in log_text
        and str(report_path) in log_text
        and "Success - 0 error(s), 0 warning(s)" in log_text
        and log_text.count("LogSavePackage") == 2
        and not _log_has_severity_failure(log_text)
    )
    if not log_ok:
        errors.append("profile-quality correction log/write contract differs")
    profile_path = _package_to_file(RUNTIME_PACKAGES[-1])
    if _identity(_file_record(profile_path)) != _expected_identity(
        CORRECTED_PROFILE_IDENTITY
    ):
        errors.append("corrected profile disk identity differs")

    return {
        "status": "VERIFIED_EXACT_ONE_PROFILE_QUALITY_WRITE",
        "run_id": run_id,
        "run_root": str(run_root),
        "report_path": str(report_path),
        "report_file": report_record,
        "log_path": str(log_path),
        "log_file": log_record,
        "profile_before": payload.get("profile_before"),
        "profile_after": payload.get("profile_after"),
        "content_after": payload.get("content_after"),
        "preferred_quality_profile_id": helper.get("preferred_quality_profile_id"),
        "assembly_pipeline": helper.get("assembly_pipeline"),
        "assembly_optimization_level": helper.get("assembly_optimization_level"),
        "success_markers_verified": log_ok,
    }


def _validate_retarget_run_ik_correction(
    report_path: Path,
    log_path: Path,
    errors: list[str],
) -> dict[str, Any]:
    report_path = report_path.expanduser().resolve()
    log_path = log_path.expanduser().resolve()
    run_id, run_root = _external_uuid_report(
        report_path,
        "Session8BMetaHumanRetargetRunIKCorrection.json",
        "retarget Run IK correction report",
        errors,
    )
    report_record = _file_record(report_path)
    log_record = _file_record(log_path)
    if run_id != RETARGET_CORRECTION_RUN_ID:
        errors.append(f"retarget Run IK correction UUID differs: {run_id!r}")
    if _identity(report_record) != _expected_identity(RETARGET_CORRECTION_REPORT):
        errors.append(f"retarget Run IK correction report identity differs: {report_record}")
    if _identity(log_record) != _expected_identity(RETARGET_CORRECTION_LOG):
        errors.append(f"retarget Run IK correction log identity differs: {log_record}")
    if log_path != (
        PROJECT_ROOT
        / "Saved/Logs/Session8B_MetaHumanRetargetRunIKCorrection_Retry1.log"
    ):
        errors.append("retarget Run IK correction log path differs")

    payload = _load_json(report_path, "retarget Run IK correction report", errors)
    prepare = payload.get("prepare_result", {})
    helper = payload.get("helper_result", {})
    exact_object = RUNTIME_OBJECTS[1]
    exact_relative = (
        "DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.uasset"
    )
    contract_ok = (
        payload.get("schema")
        == "DiscGolfTour.Session8BMetaHumanRetargetRunIKCorrection.v1"
        and payload.get("status")
        == "PASS_RETARGET_RUN_IK_DISABLED_AND_RELOAD_VALIDATED"
        and payload.get("phase") == "COMPLETE"
        and payload.get("errors") == []
        and payload.get("disk_mutation") == "EXACT_ONE_RETARGETER_WRITE"
        and payload.get("asset_save_call_count") == 1
        and payload.get("attempted_writes") == [exact_object]
        and payload.get("writes") == [exact_object]
        and payload.get("expected_policy") == EXPECTED_RETARGET_POLICY
        and payload.get("content_before") == PRE_RETARGET_CORRECTION_CONTENT
        and payload.get("content_after") == CURRENT_CONTENT
        and payload.get("content_changed") == [exact_relative]
        and payload.get("content_added") == []
        and payload.get("content_removed") == []
        and payload.get("content_directories_unchanged") is True
        and payload.get("retargeter_before")
        == _expected_identity(STALE_RETARGET_IDENTITY)
        and payload.get("retargeter_after")
        == _expected_identity(CURRENT_RETARGET_IDENTITY)
        and payload.get("project_savegames_unchanged") is True
        and payload.get("production_save_unchanged") is True
        and payload.get("accepted_backup_unchanged") is True
        and payload.get("external_save_files") == []
        and prepare.get("status") == "PASS"
        and prepare.get("operation") == "PREPARE_RETARGET_RUN_IK_CORRECTION"
        and prepare.get("disk_mutation") == "NONE_IN_MEMORY_ONLY"
        and prepare.get("writes") == []
        and prepare.get("attempted_writes") == []
        and prepare.get("preexisting_nonsavable_dirty_package_count") == 2
        and prepare.get("run_ik_rig_enabled_before_correction") is True
        and prepare.get("run_ik_rig_enabled_after_correction") is False
        and prepare.get("retarget_correction_prepared") is True
        and prepare.get("retargeter_dirty_for_exact_save") is True
        and _retarget_policy_and_pose_ok(prepare)
        and helper.get("status") == "PASS"
        and helper.get("operation") == "VALIDATE"
        and helper.get("disk_mutation") == "NONE"
        and helper.get("writes") == []
        and helper.get("attempted_writes") == []
        and helper.get("canonical_packages_absent_before_reload") is True
        and helper.get("reload_from_disk_verified") is True
        and helper.get("source_mhc_editor_only") is True
        and _retarget_policy_and_pose_ok(helper)
    )
    if not contract_ok:
        errors.append("retarget Run IK correction transaction contract differs")

    retarget_path = _package_to_file(RUNTIME_PACKAGES[1])
    retarget_record = _file_record(retarget_path)
    retarget_base = retarget_path.with_suffix("")
    sidecars = [
        str(Path(f"{retarget_base}{suffix}"))
        for suffix in PACKAGE_SUFFIXES[1:]
        if Path(f"{retarget_base}{suffix}").is_file()
    ]
    if (
        _identity(retarget_record) != _expected_identity(CURRENT_RETARGET_IDENTITY)
        or sidecars
    ):
        errors.append(
            "corrected retargeter disk identity/sidecar contract differs: "
            f"record={retarget_record}, sidecars={sidecars}"
        )

    log_text = (
        log_path.read_text(encoding="utf-8", errors="replace")
        if log_path.is_file()
        else ""
    )
    exact_save_marker = "LogFileHelpers: Saving Package: " + RUNTIME_PACKAGES[1]
    log_ok = (
        "DG_SESSION8B_RETARGET_RUN_IK_CORRECTION: "
        "PASS_RETARGET_RUN_IK_DISABLED_AND_RELOAD_VALIDATED" in log_text
        and str(report_path) in log_text
        and "Success - 0 error(s), 0 warning(s)" in log_text
        and log_text.count(exact_save_marker) == 1
        and log_text.count("LogSavePackage") == 2
        and not _log_has_severity_failure(log_text)
    )
    if not log_ok:
        errors.append("retarget Run IK correction log/write contract differs")
    if (
        report_path.is_file()
        and log_path.is_file()
        and log_path.stat().st_mtime < report_path.stat().st_mtime
    ):
        errors.append("retarget Run IK correction log is older than its report")

    return {
        "status": "VERIFIED_EXACT_ONE_RETARGET_RUN_IK_POLICY_WRITE",
        "run_id": run_id,
        "run_root": str(run_root),
        "report_path": str(report_path),
        "report_file": report_record,
        "report_schema": payload.get("schema"),
        "log_path": str(log_path),
        "log_file": log_record,
        "disk_mutation": payload.get("disk_mutation"),
        "attempted_writes": payload.get("attempted_writes"),
        "writes": payload.get("writes"),
        "asset_save_call_count": payload.get("asset_save_call_count"),
        "retargeter_before": payload.get("retargeter_before"),
        "retargeter_after": payload.get("retargeter_after"),
        "content_before": payload.get("content_before"),
        "content_after": payload.get("content_after"),
        "content_changed": payload.get("content_changed"),
        "content_added": payload.get("content_added"),
        "content_removed": payload.get("content_removed"),
        "content_directories_unchanged": payload.get(
            "content_directories_unchanged"
        ),
        "project_savegames_unchanged": payload.get("project_savegames_unchanged"),
        "production_save_unchanged": payload.get("production_save_unchanged"),
        "accepted_backup_unchanged": payload.get("accepted_backup_unchanged"),
        "external_save_files": payload.get("external_save_files"),
        "prepare_schema_version": prepare.get("schema_version"),
        "helper_schema_version": helper.get("schema_version"),
        "retarget_op_types": helper.get("retarget_op_types"),
        "retarget_op_enabled_mask": helper.get("retarget_op_enabled_mask"),
        "retarget_runtime_policy": helper.get("retarget_runtime_policy"),
        "retarget_output_pose_plausible": helper.get(
            "retarget_output_pose_plausible"
        ),
        "retarget_output_max_abs_translation_cm": helper.get(
            "retarget_output_max_abs_translation_cm"
        ),
        "retarget_output_translation_outlier_count": helper.get(
            "retarget_output_translation_outlier_count"
        ),
        "retarget_output_anchor_transforms": helper.get(
            "retarget_output_anchor_transforms"
        ),
        "success_markers_verified": log_ok,
        "disk_record": retarget_record,
        "sidecars": sidecars,
    }


def _validate_current_runtime_assets(
    report_path: Path,
    log_path: Path,
    errors: list[str],
) -> dict[str, Any]:
    report_path = report_path.expanduser().resolve()
    log_path = log_path.expanduser().resolve()
    run_id, run_root = _external_uuid_report(
        report_path,
        "Session8BMetaHumanRuntimeAssetValidation.json",
        "current runtime-asset validation report",
        errors,
    )
    report_record = _file_record(report_path)
    log_record = _file_record(log_path)
    if run_id != CURRENT_VALIDATION_RUN_ID:
        errors.append(f"current runtime-asset validation UUID differs: {run_id!r}")
    if _identity(report_record) != _expected_identity(CURRENT_VALIDATION_REPORT):
        errors.append(
            f"current runtime-asset validation report identity differs: {report_record}"
        )
    if _identity(log_record) != _expected_identity(CURRENT_VALIDATION_LOG):
        errors.append(
            f"current runtime-asset validation log identity differs: {log_record}"
        )
    if log_path != (
        PROJECT_ROOT
        / "Saved/Logs/Session8B_MetaHumanRuntimeAssetValidation_Fresh3.log"
    ):
        errors.append("current runtime-asset validation log path differs")
    payload = _load_json(report_path, "current runtime-asset validation report", errors)
    helper = payload.get("helper_result", {})
    contract_ok = (
        payload.get("schema") == "DiscGolfTour.Session8BMetaHumanRuntimeAssetValidation.v2"
        and payload.get("status") == "PASS_NO_DISK_MUTATION"
        and payload.get("phase") == "COMPLETE"
        and payload.get("errors") == []
        and payload.get("content") == CURRENT_CONTENT
        and payload.get("content_directory_count_before") == 230
        and payload.get("content_directory_count_after") == 230
        and payload.get("accepted_assembly_partition")
        == CURRENT_ASSEMBLY_PARTITION
        and payload.get("accepted_assembly_without_cook_manifest")
        == CURRENT_ASSEMBLY_WITHOUT_COOK_MANIFEST
        and payload.get("accepted_cook_manifest")
        == _expected_identity(CURRENT_COOK_MANIFEST_IDENTITY)
        and payload.get("production_save_unchanged") is True
        and payload.get("accepted_backup_unchanged") is True
        and payload.get("external_save_files") == []
        and payload.get("project_savegame_directories_before") == []
        and payload.get("project_savegame_directories_after") == []
        and helper.get("status") == "PASS"
        and helper.get("operation") == "VALIDATE"
        and helper.get("disk_mutation") == "NONE"
        and helper.get("writes") == []
        and helper.get("attempted_writes") == []
        and helper.get("canonical_packages_absent_before_reload") is True
        and helper.get("reload_from_disk_verified") is True
        and helper.get("reloaded_canonical_package_count") == 3
        and helper.get("on_disk_registry_asset_count") == 3
        and helper.get("wrapper_contract_verified") is True
        and helper.get("profile_contract_verified") is True
        and helper.get("retarget_processor_initialized") is True
        and helper.get("retarget_output_bone_count") == 342
        and _retarget_policy_and_pose_ok(helper)
        and helper.get("runtime_dependency_package_count") == 340
        and helper.get("explicit_chain_mapping_count") == 8
        and helper.get("chain_mapping") == CHAIN_MAPPING
        and helper.get("character_instance_storage_contract")
        == "NO_REFLECTED_PROPERTY_FIXED_ASSEMBLED_ACTOR"
        and helper.get("character_instance_contract_verified") is True
        and helper.get("preferred_quality_profile_id") == EXPECTED_QUALITY_PROFILE
        and helper.get("assembly_pipeline") == "UE_OPTIMIZED"
        and helper.get("assembly_optimization_level") == "MEDIUM"
        and helper.get("clean_shaven") is True
        and helper.get("beard_groom_asset_count") == 0
        and helper.get("mustache_groom_asset_count") == 0
        and helper.get("hair_s_clean_present") is True
        and helper.get("outfit_present") is True
        and helper.get("source_mhc_editor_only") is True
        and helper.get("orientation_requires_runtime_visual_validation") is True
        and helper.get("rollback_verified") is False
        and helper.get("rollback_performed") is False
    )
    if not contract_ok:
        errors.append("current runtime-asset validation contract differs")

    canonical_disk = payload.get("canonical_disk", {})
    expected_disk_keys: set[str] = set()
    disk_mismatches: dict[str, Any] = {}
    for package in RUNTIME_PACKAGES:
        base = PROJECT_ROOT / "Content" / package.removeprefix("/Game/")
        for suffix in PACKAGE_SUFFIXES:
            key = package + suffix
            expected_disk_keys.add(key)
            actual = _identity(_file_record(Path(f"{base}{suffix}")))
            reported = _identity(canonical_disk.get(key, {}))
            if actual != reported:
                disk_mismatches[key] = {"reported": reported, "actual": actual}
            if suffix == ".uasset" and not actual["exists"]:
                disk_mismatches[key] = {"expected": "PRESENT", "actual": actual}
            if suffix != ".uasset" and actual["exists"]:
                disk_mismatches[key] = {
                    "expected": "ABSENT_SIDECAR",
                    "actual": actual,
                }
    if set(canonical_disk) != expected_disk_keys or disk_mismatches:
        errors.append(
            "current canonical disk records differ: "
            f"keys={sorted(set(canonical_disk) ^ expected_disk_keys)}, "
            f"mismatches={disk_mismatches}"
        )

    log_text = (
        log_path.read_text(encoding="utf-8", errors="replace")
        if log_path.is_file()
        else ""
    )
    log_ok = (
        "DG_SESSION8B_METAHUMAN_RUNTIME_ASSET_VALIDATION: PASS_NO_DISK_MUTATION"
        in log_text
        and str(report_path) in log_text
        and "Success - 0 error(s), 0 warning(s)" in log_text
        and "LogSavePackage" not in log_text
        and not _log_has_severity_failure(log_text)
    )
    if not log_ok:
        errors.append("current runtime-asset validation log contract differs")
    if (
        report_path.is_file()
        and log_path.is_file()
        and log_path.stat().st_mtime < report_path.stat().st_mtime
    ):
        errors.append("current validation log is older than its report")
    if report_path.is_file():
        asset_mtimes = [
            _package_to_file(package).stat().st_mtime
            for package in RUNTIME_PACKAGES
            if _package_to_file(package).is_file()
        ]
        if asset_mtimes and report_path.stat().st_mtime < max(asset_mtimes):
            errors.append("current validation report predates a canonical runtime asset")

    return {
        "status": "PRESENT_VERIFIED_FRESH_PROCESS_NO_DISK_MUTATION",
        "run_id": run_id,
        "run_root": str(run_root),
        "report_path": str(report_path),
        "report_file": report_record,
        "report_schema": payload.get("schema"),
        "log_path": str(log_path),
        "log_file": log_record,
        "helper_schema_version": helper.get("schema_version"),
        "content": payload.get("content"),
        "content_directory_count_before": payload.get(
            "content_directory_count_before"
        ),
        "content_directory_count_after": payload.get(
            "content_directory_count_after"
        ),
        "accepted_assembly_partition": payload.get("accepted_assembly_partition"),
        "accepted_assembly_without_cook_manifest": payload.get(
            "accepted_assembly_without_cook_manifest"
        ),
        "accepted_cook_manifest": payload.get("accepted_cook_manifest"),
        "helper_writes": helper.get("writes"),
        "helper_attempted_writes": helper.get("attempted_writes"),
        "production_save_unchanged": payload.get("production_save_unchanged"),
        "accepted_backup_unchanged": payload.get("accepted_backup_unchanged"),
        "external_save_files": payload.get("external_save_files"),
        "project_savegame_directories_before": payload.get(
            "project_savegame_directories_before"
        ),
        "project_savegame_directories_after": payload.get(
            "project_savegame_directories_after"
        ),
        "canonical_disk": canonical_disk,
        "preferred_quality_profile_id": helper.get("preferred_quality_profile_id"),
        "assembly_pipeline": helper.get("assembly_pipeline"),
        "assembly_optimization_level": helper.get("assembly_optimization_level"),
        "orientation_requires_runtime_visual_validation": helper.get(
            "orientation_requires_runtime_visual_validation"
        ),
        "retarget_op_types": helper.get("retarget_op_types"),
        "retarget_op_enabled_mask": helper.get("retarget_op_enabled_mask"),
        "retarget_runtime_policy": helper.get("retarget_runtime_policy"),
        "retarget_output_pose_plausible": helper.get(
            "retarget_output_pose_plausible"
        ),
        "retarget_output_max_abs_translation_cm": helper.get(
            "retarget_output_max_abs_translation_cm"
        ),
        "retarget_output_translation_outlier_count": helper.get(
            "retarget_output_translation_outlier_count"
        ),
        "retarget_output_anchor_transforms": helper.get(
            "retarget_output_anchor_transforms"
        ),
        "log_success_markers_verified": log_ok,
        "disk_mismatches": disk_mismatches,
    }


def _validate_cook_prep_spec(errors: list[str]) -> dict[str, Any]:
    record = _file_record(COOK_SPEC)
    if _identity(record) != _expected_identity(COOK_SPEC_IDENTITY):
        errors.append(f"Session 8B cook-prep source identity differs: {record}")
    spec = _load_json(COOK_SPEC, "Session 8B cook-prep source", errors)
    contract = spec.get("metahuman_runtime_contract", {})
    inventory = contract.get("runtime_inventory", {})
    groups = inventory.get("directory_groups", [])
    expected_groups = [
        {
            "name": "ACCEPTED_METAHUMAN_COMMON",
            "package_root": "/Game/DiscGolf/Characters/MetaHuman/Common",
            "disk_relative_root": "Content/DiscGolf/Characters/MetaHuman/Common",
            "expected_package_count": 205,
            "package_list_sha256": (
                "90C3299AB7182FE1A6D60133936CDD1D6387B3509FF753F46C772579C20CCD1F"
            ),
        },
        {
            "name": "ACCEPTED_METAHUMAN_GENERATED",
            "package_root": "/Game/DiscGolf/Characters/MetaHuman/Generated",
            "disk_relative_root": "Content/DiscGolf/Characters/MetaHuman/Generated",
            "expected_package_count": 56,
            "package_list_sha256": (
                "0D545C7185BD7794E88E5AFECCAAEFA0BC9E0959B8F26004A560BF53A5AE1238"
            ),
        },
    ]
    explicit = [
        "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default",
        "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman",
        "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig",
    ]
    outputs = spec.get("asset_outputs", {})
    profile = outputs.get("metahuman_default_backend_profile", {})
    contract_ok = (
        spec.get("schema") == "DiscGolfTour.RuntimeCookManifestSource.v2"
        and spec.get("schema_version") == 2
        and spec.get("manifest_id") == "dg_runtime_v1"
        and spec.get("expected_runtime_package_count") == 69
        and spec.get("expected_metahuman_runtime_package_count") == 264
        and spec.get("expected_avatar_backend_profile_count") == 2
        and spec.get("expected_excluded_package_count") == 12
        and spec.get("expected_excluded_metahuman_package_count") == 1
        and spec.get("authority") == "COOK_DISCOVERY_ONLY_NO_GAMEPLAY_OR_RELEASE_AUTHORITY"
        and spec.get("shipping_status") == "DO_NOT_CLAIM_SHIPPING_ART_APPROVAL"
        and contract.get("backend_id") == "metahuman_assembled"
        and contract.get("source_mhc_runtime_status")
        == "EDITOR_ONLY_EXCLUDED_FROM_MANIFEST_AND_COOK_CLOSURE"
        and contract.get("authoring_status")
        == "AUTHORED_AND_FRESH_PROCESS_VALIDATED_PENDING_PACKAGED_COOK_CLOSURE"
        and contract.get("created_by_session8_cook_author") is False
        and contract.get("explicitly_excluded_packages") == [SOURCE_PACKAGE]
        and inventory.get("expected_package_count") == 264
        and inventory.get("inventory_semantics")
        == "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH"
        and inventory.get("fresh_validation_transitive_runtime_dependency_count")
        == 340
        and inventory.get("transitive_dependency_evidence_status")
        == "FRESH2_SOURCE_VALIDATION_ONLY_REQUIRES_PACKAGED_IOSTORE_REPROOF"
        and inventory.get("package_list_sha256")
        == "003A7683069A2353819421CD71ECA82A2A0F8AE2E7786F56E04EA18DB11AB3B9"
        and groups == expected_groups
        and inventory.get("explicit_packages") == explicit
        and inventory.get("explicit_package_list_sha256")
        == "21DB66FA98C4CBC7B12EE0518F4A1927116898E035411373113AABF3973AD931"
        and profile.get("package") == RUNTIME_PACKAGES[-1]
        and profile.get("object") == RUNTIME_OBJECTS[-1]
        and profile.get("backend_id") == "metahuman_assembled"
        and profile.get("backend") == "MetaHumanPreset"
        and profile.get("runtime_mode") == "ShippingSafeAssembled"
        and profile.get("preferred_quality_profile_id") == EXPECTED_QUALITY_PROFILE
        and profile.get("visual_actor_class") == RUNTIME_OBJECTS[0] + "_C"
        and profile.get("retarget_asset") == RUNTIME_OBJECTS[1]
        and profile.get("use_runtime_retargeting") is True
        and profile.get("visual_body_component_tag") == "DGVisualBody"
        and profile.get("visual_head_component_tag") == "DGVisualHead"
        and profile.get("runtime_face_sculpting") is False
    )
    if not contract_ok:
        errors.append("Session 8B cook-prep source contract differs")
    return {
        "status": "SOURCE_PREP_VERIFIED_PENDING_AUTHORED_ASSET_AND_PACKAGE_CLOSURE",
        "file": str(COOK_SPEC),
        "file_record": record,
        "schema": spec.get("schema"),
        "manifest_id": spec.get("manifest_id"),
        "metahuman_runtime_package_count": inventory.get("expected_package_count"),
        "metahuman_runtime_package_list_sha256": inventory.get("package_list_sha256"),
        "inventory_semantics": inventory.get("inventory_semantics"),
        "fresh_validation_transitive_runtime_dependency_count": inventory.get(
            "fresh_validation_transitive_runtime_dependency_count"
        ),
        "transitive_dependency_evidence_status": inventory.get(
            "transitive_dependency_evidence_status"
        ),
        "source_mhc_runtime_status": contract.get("source_mhc_runtime_status"),
        "authoring_status": contract.get("authoring_status"),
        "profile_quality": profile.get("preferred_quality_profile_id"),
    }


def audit(
    historical_author_report: str | Path,
    quality_correction_report: str | Path,
    retarget_correction_report: str | Path,
    current_validation_report: str | Path,
    *,
    historical_author_log: str | Path,
    quality_correction_log: str | Path,
    retarget_correction_log: str | Path,
    current_validation_log: str | Path,
    engine_root: str | Path | None = None,
    write_report: bool = True,
    report_path: str | Path | None = None,
) -> dict[str, Any]:
    errors: list[str] = []
    base, all_packages = _run_session8a_with_exact_session8b_classification(
        engine_root, errors
    )
    base_errors = base.get("integrity_errors", []) if base else []
    historical_diagnostics_ok = (
        len(base_errors) == len(HISTORICAL_COOK_DIAGNOSTIC_PREFIXES)
        and all(
            sum(error.startswith(prefix) for error in base_errors) == 1
            for prefix in HISTORICAL_COOK_DIAGNOSTIC_PREFIXES
        )
    )
    if not historical_diagnostics_ok:
        errors.append(
            "accepted Session 8A diagnostics differ outside the exact successor "
            f"cook-spec boundary: {base_errors}"
        )
    base_blockers = set(base.get("blockers", [])) if base else set()
    if base and (
        base.get("status") != "ERROR" or base_blockers != BASE_EXPECTED_BLOCKERS
    ):
        errors.append(
            "accepted Session 8A classified baseline differs: "
            f"status={base.get('status')!r}, blockers={sorted(base_blockers)}"
        )

    core = base.get("metahuman_creator_core_data", {}) if base else {}
    engine_root_path = Path(base.get("engine_root", "")) if base else Path()
    core_root = engine_root_path / str(core.get("root", ""))
    core_files = (
        sorted(path for path in core_root.rglob("*") if path.is_file())
        if core_root.is_dir()
        else []
    )
    core_counts = {
        "files": len(core_files),
        "uassets": sum(path.suffix.casefold() == ".uasset" for path in core_files),
        "umaps": sum(path.suffix.casefold() == ".umap" for path in core_files),
        "other": sum(
            path.suffix.casefold() not in {".uasset", ".umap"}
            for path in core_files
        ),
    }
    expected_core_counts = {
        "files": 1816,
        "uassets": 1783,
        "umaps": 0,
        "other": 33,
    }
    if (
        core.get("status")
        != "PRESENT_BY_UE_IS_OPTIONAL_METAHUMAN_CONTENT_INSTALLED_CONTRACT"
        or core_counts != expected_core_counts
    ):
        errors.append(
            f"MetaHuman Creator Core Data contract differs: status={core.get('status')!r}, "
            f"counts={core_counts}"
        )

    project_plugins = base.get("project_plugin_enablement", {}) if base else {}
    creator = (base.get("engine_plugins", {}) or {}).get("MetaHumanCharacter", {})
    if (
        project_plugins.get("MetaHumanCharacter") is not True
        or creator.get("status") != "PRESENT"
        or creator.get("project_explicit_enabled") is not True
    ):
        errors.append("MetaHuman Creator is not present and explicitly project-enabled")

    source_record = _file_record(SOURCE_FILE)
    if _identity(source_record) != _expected_identity(SOURCE_IDENTITY):
        errors.append(f"canonical editor-only source package drifted: {source_record}")
    assembly_summary = _snapshot_summary((COMMON_ROOT, GENERATED_ROOT), METAHUMAN_ROOT)
    common_count = (
        sum(1 for path in COMMON_ROOT.rglob("*") if path.is_file())
        if COMMON_ROOT.is_dir()
        else 0
    )
    generated_count = (
        sum(1 for path in GENERATED_ROOT.rglob("*") if path.is_file())
        if GENERATED_ROOT.is_dir()
        else 0
    )
    expected_summary = {
        key: ASSEMBLY_PARTITION[key]
        for key in ("file_count", "bytes", "manifest_sha256")
    }
    if (
        common_count != ASSEMBLY_PARTITION["common_files"]
        or generated_count != ASSEMBLY_PARTITION["generated_files"]
        or assembly_summary != expected_summary
    ):
        errors.append(
            "Optimized-Medium Generated/Common partition differs: "
            f"common={common_count}, generated={generated_count}, summary={assembly_summary}"
        )

    metahuman_files = (
        sorted(path for path in METAHUMAN_ROOT.rglob("*") if path.is_file())
        if METAHUMAN_ROOT.is_dir()
        else []
    )
    if len(metahuman_files) != 264 or any(
        path.suffix.casefold() != ".uasset" for path in metahuman_files
    ):
        errors.append(
            "MetaHuman tree is not exact source+261 assembly+wrapper+retarget uassets: "
            f"count={len(metahuman_files)}"
        )
    if not _package_to_file(GENERATED_BLUEPRINT_PACKAGE).is_file():
        errors.append("assembled generated MetaHuman Blueprint is absent")

    unclassified = sorted(
        package
        for package in all_packages
        if "metahuman" in package.casefold()
        and not _is_classified_session8b_package(package)
    )
    classified = sorted(
        package for package in all_packages if _is_classified_session8b_package(package)
    )
    if unclassified or len(classified) != 265:
        errors.append(
            "Session 8B MetaHuman package classification differs: "
            f"classified={len(classified)}, unclassified={unclassified}"
        )

    historical_author = _validate_historical_author_transaction(
        Path(historical_author_report), Path(historical_author_log), errors
    )
    quality_correction = _validate_quality_correction(
        Path(quality_correction_report), Path(quality_correction_log), errors
    )
    retarget_correction = _validate_retarget_run_ik_correction(
        Path(retarget_correction_report), Path(retarget_correction_log), errors
    )
    current_validation = _validate_current_runtime_assets(
        Path(current_validation_report), Path(current_validation_log), errors
    )
    cook_prep = _validate_cook_prep_spec(errors)
    if (
        current_validation.get("content") != CURRENT_CONTENT
        or retarget_correction.get("content_after") != CURRENT_CONTENT
        or retarget_correction.get("retargeter_after")
        != _expected_identity(CURRENT_RETARGET_IDENTITY)
        or current_validation.get("retarget_op_types")
        != retarget_correction.get("retarget_op_types")
        or current_validation.get("retarget_op_enabled_mask")
        != retarget_correction.get("retarget_op_enabled_mask")
        or current_validation.get("retarget_runtime_policy")
        != retarget_correction.get("retarget_runtime_policy")
        or current_validation.get("retarget_output_pose_plausible") is not True
        or current_validation.get("preferred_quality_profile_id")
        != quality_correction.get("preferred_quality_profile_id")
    ):
        errors.append(
            "Fresh3 validation does not supersede the exact quality and Run IK correction state"
        )
    quality_report_path = Path(quality_correction_report).expanduser().resolve()
    correction_report_path = Path(retarget_correction_report).expanduser().resolve()
    validation_report_path = Path(current_validation_report).expanduser().resolve()
    if (
        quality_report_path.is_file()
        and correction_report_path.is_file()
        and correction_report_path.stat().st_mtime
        <= quality_report_path.stat().st_mtime
    ):
        errors.append("Run IK correction report is not newer than the quality correction")
    if (
        correction_report_path.is_file()
        and validation_report_path.is_file()
        and validation_report_path.stat().st_mtime
        <= correction_report_path.stat().st_mtime
    ):
        errors.append("Fresh3 validation report is not newer than the Run IK correction")

    result: dict[str, Any] = json.loads(json.dumps(base)) if base else {}
    # These Session 8A field names describe the historical cook-author phase and
    # would be ambiguous after real Session 8B assets exist. Replace them with
    # explicit current-checkpoint semantics below while retaining the accepted
    # 8A audit itself and its exact successor diagnostics.
    result.pop("metahuman_assets_created", None)
    result.update(
        {
            "schema": SCHEMA,
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "determinism": "FILESYSTEM_HASH_REPORT_LOG_ONLY_NO_NETWORK_NO_ENGINE_PROCESS",
            "metahuman_creator_core_data": {
                **core,
                "file_type_counts": core_counts,
                "expected_file_type_counts": expected_core_counts,
            },
            "project_asset_truth": {
                "editor_only_source": {
                    "package": SOURCE_PACKAGE,
                    "object_path": SOURCE_OBJECT_PATH,
                    "file": str(SOURCE_FILE),
                    "file_record": source_record,
                    "classification": "EDITOR_ONLY_AUTHORING_INPUT_NOT_RUNTIME_COOK_OUTPUT",
                },
                "optimized_medium_assembly": {
                    "classification": "PRESENT_VERIFIED",
                    "common_files": common_count,
                    "generated_files": generated_count,
                    **assembly_summary,
                    "generated_blueprint": GENERATED_BLUEPRINT_PACKAGE,
                },
                "canonical_runtime_assets": {
                    "classification": "PRESENT_VERIFIED",
                    "objects": list(RUNTIME_OBJECTS),
                    "disk": current_validation.get("canonical_disk"),
                },
                "retarget_runtime_policy": {
                    "classification": "RUN_IK_DISABLED_FIXED_PRESENTATION_VERIFIED",
                    "retargeter": RUNTIME_OBJECTS[1],
                    "op_types": current_validation.get("retarget_op_types"),
                    "op_enabled_mask": current_validation.get(
                        "retarget_op_enabled_mask"
                    ),
                    "policy": current_validation.get("retarget_runtime_policy"),
                    "output_pose_plausible": current_validation.get(
                        "retarget_output_pose_plausible"
                    ),
                    "output_max_abs_translation_cm": current_validation.get(
                        "retarget_output_max_abs_translation_cm"
                    ),
                    "output_translation_outlier_count": current_validation.get(
                        "retarget_output_translation_outlier_count"
                    ),
                    "output_anchor_transforms": current_validation.get(
                        "retarget_output_anchor_transforms"
                    ),
                },
                "current_content": CURRENT_CONTENT,
                "classified_metahuman_package_count": len(classified),
                "unclassified_metahuman_packages": unclassified,
            },
            "historical_author_evidence": historical_author,
            "quality_correction_evidence": quality_correction,
            "retarget_run_ik_correction_evidence": retarget_correction,
            "current_runtime_asset_validation": current_validation,
            "cook_prep_source": cook_prep,
            "cook_source_inventory": {
                "status": "SOURCE_AND_ASSET_PREVIOUSLY_VALIDATED_PENDING_POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE",
                "legacy_runtime_package_count": 69,
                "metahuman_explicit_cook_root_package_count": 264,
                "fresh_validation_transitive_runtime_dependency_count": 340,
                "inventory_semantics": (
                    "EXPLICIT_COOK_ROOT_SET_NOT_COMPLETE_TRANSITIVE_DEPENDENCY_GRAPH"
                ),
                "backend_profile_count": 2,
                "legacy_excluded_package_count": 12,
                "metahuman_excluded_package_count": 1,
                "metahuman_source_exclusion": SOURCE_PACKAGE,
                "packaged_iostore_reproof_required": True,
            },
            "cook_groundwork_status": (
                "SESSION8B_COOK_MANIFEST_SOURCE_AND_ASSET_PREVIOUSLY_VALIDATED_"
                "POST_RETARGET_RECOOK_AND_PACKAGED_CLOSURE_REQUIRED"
            ),
            "author_run_required": False,
            "post_retarget_recook_required": True,
            "historical_session8a_successor_diagnostics": {
                "classification": (
                    "EXACT_V1_COOK_SPEC_DIAGNOSTICS_SUPERSEDED_BY_VERIFIED_V2_SOURCE"
                ),
                "diagnostics": base_errors,
                "exact_boundary_verified": historical_diagnostics_ok,
            },
            "asset_authoring_status": "PRESENT_VERIFIED",
            "quality_metadata_contract": {
                "assembly_pipeline": "UE_OPTIMIZED",
                "assembly_optimization_level": "MEDIUM",
                "preferred_quality_profile_id": EXPECTED_QUALITY_PROFILE,
                "classification": "OPTIMIZED_MEDIUM_MATCHES_GAMEPLAY_PERFORMANCE",
            },
            "blockers": sorted(EXPECTED_BLOCKERS),
            "blocker_count": len(EXPECTED_BLOCKERS),
            "blocker_state": {
                "post_retarget_recook_and_packaged_closure": "NOT_YET_ACCEPTED",
                "runtime_visual_orientation_material_attachment_atomic_switching": (
                    "NOT_YET_ACCEPTED"
                ),
                "packaged_omen_performance": "NOT_YET_MEASURED",
            },
            "integrity_errors": errors,
            "status": "ERROR" if errors else "BLOCKED",
            "metahuman_integration_status": "BLOCKED",
            "ready_claimed": False,
            "proxy_gameplay_authority_retained": True,
            "session8a_audit_path": str(SESSION8A_AUDIT),
            "session8a_audit_reused_without_edit": True,
            "source_classification_exception": [SOURCE_PACKAGE],
            "network_access": "NONE",
            "unreal_launched": False,
            "ubt_launched": False,
            "metahuman_assets_created_by_audit": 0,
        }
    )

    if write_report:
        destination = Path(report_path) if report_path else DEFAULT_REPORT
        destination = destination.expanduser().resolve()
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--historical-author-report", required=True)
    parser.add_argument("--historical-author-log", required=True)
    parser.add_argument("--quality-correction-report", required=True)
    parser.add_argument("--quality-correction-log", required=True)
    parser.add_argument("--retarget-correction-report", required=True)
    parser.add_argument("--retarget-correction-log", required=True)
    parser.add_argument("--current-validation-report", required=True)
    parser.add_argument("--current-validation-log", required=True)
    parser.add_argument("--engine-root")
    parser.add_argument("--report")
    parser.add_argument("--no-report", action="store_true")
    args = parser.parse_args()

    result = audit(
        args.historical_author_report,
        args.quality_correction_report,
        args.retarget_correction_report,
        args.current_validation_report,
        historical_author_log=args.historical_author_log,
        quality_correction_log=args.quality_correction_log,
        retarget_correction_log=args.retarget_correction_log,
        current_validation_log=args.current_validation_log,
        engine_root=args.engine_root,
        write_report=not args.no_report,
        report_path=args.report,
    )
    print(
        "SESSION 8B AVAILABILITY " + result["status"] + ": "
        f"assets={result.get('asset_authoring_status')} "
        f"blockers={len(result.get('blockers', []))} "
        f"errors={len(result.get('integrity_errors', []))}"
    )
    for error in result.get("integrity_errors", []):
        print(f"  ERROR: {error}")
    for blocker in result.get("blockers", []):
        print(f"  BLOCKED: {blocker}")
    return 1 if result["status"] == "ERROR" else 2


if __name__ == "__main__":
    raise SystemExit(main())
