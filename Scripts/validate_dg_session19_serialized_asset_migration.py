#!/usr/bin/env python3
"""Validate Session 19 retained serialized-asset migration evidence.

The historical receipt remains a bounded source/Editor record.  Candidate mode
adds one explicit Windows Shipping archive and executable, re-runs both of the
candidate audits against those live inputs, and accepts only the narrower
technical migration boundary: the retained packages ship and the superseded
framework does not.  It never grants source-distribution, legal, fresh-install,
human-review, release, or distribution approval.
"""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import sys
from typing import Any, Callable
import uuid


ROOT = Path(__file__).resolve().parents[1]
RECEIPT_PATH = ROOT / "Evidence/Session19/SerializedAssetMigrationReceipt.json"
REPAIR_RECEIPT_PATH = (
    ROOT / "Evidence/Session19/RetainedAvatarProfileRepairReceipt.json"
)
REPAIRED_PROFILE_PACKAGE = (
    "/Game/DiscGolf/Characters/Avatar/Data/"
    "DA_DG_AvatarBackend_MetaHuman_Default"
)
OLD_SCRIPT = b"/Script/DiscGolfCharacterFramework"
NEW_SCRIPT = b"/Script/DiscGolfRuntimeFoundation"
SHA_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_ID_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]+$")
USER_DIR_TOKEN_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)
CANDIDATE_POLICY_SCHEMA = (
    "DiscGolfTour.Session19SerializedAssetMigrationCandidatePolicy.v1"
)
CANDIDATE_POLICY_AUTHORITY = (
    "CANDIDATE_SCOPED_TECHNICAL_VALIDATION_NOT_RELEASE_OR_LEGAL_APPROVAL"
)
EXTERNAL_MANIFEST_SCHEMA_V1 = (
    "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
)
EXTERNAL_MANIFEST_SCHEMA_V2 = (
    "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
)
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_RELATIVE = (
    "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
)
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_JOURNAL_BINDING_KEYS = {
    "bytes", "captureNonce", "eventCount", "firstSequence", "lastSequence",
    "modifiedUtc", "modifiedWithinRunWindow", "roundId", "schema", "sha256",
    "userDirRelativePath", "validationFailures", "validationState",
}
EXTERNAL_EVIDENCE_V1_KEYS = {
    "userDirToken", "runDirectoryLeaf", "manifestSha256",
    "externalTechnicalValidationState", "freshUserDirValidationState",
}
EXTERNAL_EVIDENCE_V2_KEYS = EXTERNAL_EVIDENCE_V1_KEYS | {
    "manifestSchema", "manifestSchemaVersion",
    "candidateEvidenceBindingSha256", "runtimeCheckpointJournal",
}
RETAINED = [
    "/Game/DiscGolf/Animation/ABP_DG_Player",
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster",
    "/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default",
    "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default",
    "/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter",
    "/Game/DiscGolf/Rigs/CR_DG_Master",
]
EXCLUDED_ROOTS = [
    "/Game/DiscGolf/Animation/Mocap",
    "/Game/DiscGolf/Animation/Throws",
    "/Game/DiscGolf/Characters/Customization",
    "/Game/DiscGolf/Outfits",
    "/Game/DiscGolf/Tests",
]
SHIPPING_CANDIDATE_ID = "S19_WindowsShipping_20260826T120643Z_afbe7694ccf5"
EXTERNAL_USER_DIR_TOKEN = "792a3edb-9e6a-41df-8715-46c8a1608825"
EXTERNAL_MANIFEST_SHA256 = (
    "D3348808AF3915B99F2676790A78CE81465AE41F22CB25873EDBD0A541F8330C"
)
SHIPPING_RECEIPT_PATH = (
    ROOT / f"Evidence/Session19/SerializedAssetMigrationShipping-{SHIPPING_CANDIDATE_ID}.json"
)
SHIPPING_EXE_RELATIVE = Path(
    "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
)
SHIPPING_EXE_BYTES = 162598400
SHIPPING_EXE_SHA256 = (
    "29A8E1681AC09749AFD08FA691706E81BED0AB1ABC8AFF7F113A9E59B08B3BF2"
)
SHIPPING_BINARY_POLICY_ID = "windows_shipping_v05_executable_marker_policy_v1"
SHIPPING_BINARY_FOUNDATION_MARKER = {
    "id": "project_owned_runtime_foundation",
    "value": "DiscGolfRuntimeFoundation",
    "requireAnyEncoding": True,
}
# The frozen historical supplement predates guarded release-performance capture
# markers.  Its one-row binary audit remains valid only under this exact policy
# identity; new/current audits bind to the independently validated live policy.
LEGACY_FOUNDATION_ONLY_BINARY_POLICY_SHA256 = (
    "3e750646ad0c9009c2a144b69334212187159833628a4def60f14889da80e70d"
)
SHIPPING_ARCHIVE = {
    "hostPathRecorded": False,
    "recoveryLocationToken": f"DGTOUR_PACKAGES/{SHIPPING_CANDIDATE_ID}/Windows",
    "fileCount": 31,
    "bytes": 1245992471,
    "canonicalManifestSha256": (
        "1FBFC3D2AAB6E90F17D104CA9DCED6B4753EC8666E520DDACBF5AB5581CF9822"
    ),
}
SHIPPING_BINDING_PATHS = {
    "sourceMigrationReceipt": "Evidence/Session19/SerializedAssetMigrationReceipt.json",
    "retainedProfileRepairReceipt": (
        "Evidence/Session19/RetainedAvatarProfileRepairReceipt.json"
    ),
    "candidateVerification": (
        f"Evidence/Session19/ShippingCandidateVerification-{SHIPPING_CANDIDATE_ID}.json"
    ),
    "candidateContentAudit": (
        f"Evidence/Session19/CandidateContent-{SHIPPING_CANDIDATE_ID}.json"
    ),
    "shippingBinaryAudit": (
        f"Evidence/Session19/ShippingBinary-{SHIPPING_CANDIDATE_ID}.json"
    ),
    "externalTechnicalValidation": (
        f"Evidence/Session19/ExternalTechnicalEvidence-{SHIPPING_CANDIDATE_ID}.json"
    ),
    "freshUserDirValidation": (
        f"Evidence/Session19/FreshUserDir-{SHIPPING_CANDIDATE_ID}.json"
    ),
}
SHIPPING_RETAINED = [
    {
        "package": package,
        "stagedIdentity": "DiscGolfTour/Content" + package.removeprefix("/Game") + ".uasset",
        "presentInUfsManifest": True,
        "presentInContainerInventory": True,
    }
    for package in RETAINED
]
SHIPPING_SCREENSHOTS = [
    {
        "milestone": "HOLE_2_COMPLETE_CHARACTER_RENDERED",
        "path": "Screenshots/007_hole2_complete.png",
        "bytes": 6070063,
        "sha256": "DF2930183C22DC6C458FC120D799F06D1C755B2877EC4689F9063EB5BC0ED205",
        "width": 2560,
        "height": 1600,
        "technicalOnly": True,
        "freshInstallEvidence": False,
        "humanReviewed": False,
    },
    {
        "milestone": "HOLE_3_TEE_CHARACTER_RENDERED",
        "path": "Screenshots/008_hole3_tee.png",
        "bytes": 4032236,
        "sha256": "E9DD6EB22883631F9047A80E87B9CD30BBEE0EB4C69154D607A273E993A8DBEC",
        "width": 2560,
        "height": 1600,
        "technicalOnly": True,
        "freshInstallEvidence": False,
        "humanReviewed": False,
    },
    {
        "milestone": "HOLE_3_RECOVERED_LIE_CHARACTER_RENDERED",
        "path": "Screenshots/009_hole3_recovered_lie.png",
        "bytes": 3659073,
        "sha256": "A8E44BF98389F1FEBFF326D4557576F0077A852794623B935AB9DC6E4AB6B223",
        "width": 2560,
        "height": 1600,
        "technicalOnly": True,
        "freshInstallEvidence": False,
        "humanReviewed": False,
    },
]


def historical_shipping_authority() -> dict[str, Any]:
    """Return the frozen pre-selectable supplement authority unchanged."""
    return {
        "mode": "historical",
        "candidateId": SHIPPING_CANDIDATE_ID,
        "shippingReceiptPath": (
            "Evidence/Session19/SerializedAssetMigrationShipping-"
            f"{SHIPPING_CANDIDATE_ID}.json"
        ),
        "receiptSchema": (
            "DiscGolfTour.Session19SerializedAssetMigrationShippingEvidence.v1"
        ),
        "receiptSchemaVersion": 1,
        "candidatePolicyBinding": None,
        "evidenceId": (
            "session19_retained_serialized_asset_migration_shipping_"
            "non_distribution_20260825"
        ),
        "bindingPaths": copy.deepcopy(SHIPPING_BINDING_PATHS),
        "archive": copy.deepcopy(SHIPPING_ARCHIVE),
        "executable": {
            "relativePath": SHIPPING_EXE_RELATIVE.as_posix(),
            "bytes": SHIPPING_EXE_BYTES,
            "sha256": SHIPPING_EXE_SHA256,
        },
        "externalEvidence": {
            "userDirToken": EXTERNAL_USER_DIR_TOKEN,
            "runDirectoryLeaf": (
                f"{SHIPPING_CANDIDATE_ID}_{EXTERNAL_USER_DIR_TOKEN}"
            ),
            "manifestSha256": EXTERNAL_MANIFEST_SHA256,
            "externalTechnicalValidationState": (
                "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
            ),
            "freshUserDirValidationState": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        },
        "runtimeScreenshots": copy.deepcopy(SHIPPING_SCREENSHOTS),
    }


class StrictJsonError(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StrictJsonError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise StrictJsonError(f"non-finite JSON constant: {value}")


def load_json(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_pairs,
            parse_constant=_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, StrictJsonError) as exc:
        raise StrictJsonError(f"{path}: {exc}") from exc


def require_bound_receipt_object(
    value: Any, label: str, issues: list[str],
) -> dict[str, Any] | None:
    """Fail closed before any receipt-specific or live re-audit branch."""
    if type(value) is not dict:
        issues.append(f"bound {label} root must be an object")
        return None
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def identity(path: Path) -> dict[str, Any]:
    return {"path": path.relative_to(ROOT).as_posix(),
            "bytes": path.stat().st_size, "sha256": sha256(path)}


def _is_reparse_point(path: Path) -> bool:
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
    except OSError:
        return False
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(attributes & reparse_flag)


def _host_path_contains_reparse(path: Path) -> bool:
    absolute = path.absolute()
    parts = absolute.parts
    if not parts:
        return False
    cursor = Path(parts[0])
    for part in parts[1:]:
        cursor /= part
        if cursor.exists() and _is_reparse_point(cursor):
            return True
    return False


def _safe_relative_token(value: Any, label: str, issues: list[str]) -> PurePosixPath | None:
    if type(value) is not str or not value:
        issues.append(f"{label} must be a non-empty relative path")
        return None
    pure = PurePosixPath(value)
    if (
        pure.is_absolute()
        or ".." in pure.parts
        or "." in pure.parts
        or "\\" in value
        or ":" in value
        or value != pure.as_posix()
    ):
        issues.append(f"{label} is not a canonical safe relative path: {value!r}")
        return None
    return pure


def _resolve_project_path(
    root: Path,
    value: Any,
    label: str,
    issues: list[str],
    *,
    must_exist: bool,
) -> Path | None:
    pure = _safe_relative_token(value, label, issues)
    if pure is None:
        return None
    root = root.resolve()
    lexical = root.joinpath(*pure.parts)
    cursor = root
    for part in pure.parts:
        cursor /= part
        if cursor.exists() and _is_reparse_point(cursor):
            issues.append(f"{label} contains a reparse point: {value}")
            return None
    try:
        resolved = lexical.resolve(strict=must_exist)
        resolved.relative_to(root)
    except (OSError, ValueError):
        issues.append(f"{label} escapes the project root or is missing")
        return None
    if must_exist and (not resolved.is_file() or _is_reparse_point(resolved)):
        issues.append(f"{label} must be an existing non-reparse file")
        return None
    return resolved


def _candidate_binding_paths(candidate_id: str) -> dict[str, str]:
    return {
        "sourceMigrationReceipt": (
            "Evidence/Session19/SerializedAssetMigrationReceipt.json"
        ),
        "retainedProfileRepairReceipt": (
            "Evidence/Session19/RetainedAvatarProfileRepairReceipt.json"
        ),
        "candidateVerification": (
            f"Evidence/Session19/ShippingCandidateVerification-{candidate_id}.json"
        ),
        "candidateContentAudit": (
            f"Evidence/Session19/CandidateContent-{candidate_id}.json"
        ),
        "shippingBinaryAudit": (
            f"Evidence/Session19/ShippingBinary-{candidate_id}.json"
        ),
        "externalTechnicalValidation": (
            f"Evidence/Session19/ExternalTechnicalEvidence-{candidate_id}.json"
        ),
        "freshUserDirValidation": (
            f"Evidence/Session19/FreshUserDir-{candidate_id}.json"
        ),
    }


def _canonical_uuid_v4(value: Any) -> bool:
    if type(value) is not str:
        return False
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError):
        return False
    return str(parsed) == value and parsed.version == 4


def _validate_runtime_checkpoint_journal_binding(
    value: Any,
    label: str,
    issues: list[str],
) -> None:
    """Validate the exact journal inventory carried by a v2 manifest."""
    if not exact_keys(value, RUNTIME_JOURNAL_BINDING_KEYS, label, issues):
        return
    if type(value["bytes"]) is not int or type(value["bytes"]) is bool or value["bytes"] < 1:
        issues.append(f"{label}.bytes must be a positive integer")
    if type(value["sha256"]) is not str or not SHA_RE.fullmatch(value["sha256"]):
        issues.append(f"{label}.sha256 must be uppercase SHA-256")
    for key in ("captureNonce", "roundId"):
        if not _canonical_uuid_v4(value[key]):
            issues.append(f"{label}.{key} must be a canonical UUIDv4")
    for key, expected in {
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
    }.items():
        if type(value[key]) is not int or type(value[key]) is bool or value[key] != expected:
            issues.append(f"{label}.{key} must be integer {expected}")
    if (
        type(value["modifiedUtc"]) is not str
        or re.fullmatch(
            r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{3})?Z",
            value["modifiedUtc"],
        ) is None
    ):
        issues.append(f"{label}.modifiedUtc must be canonical UTC text")
    for key, expected in {
        "modifiedWithinRunWindow": True,
        "schema": RUNTIME_JOURNAL_SCHEMA,
        "userDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
        "validationFailures": [],
        "validationState": RUNTIME_JOURNAL_PASS,
    }.items():
        expect(value[key], expected, f"{label}.{key}", issues)


def _external_manifest_version(external: Any) -> int | None:
    if type(external) is not dict:
        return None
    keys = set(external)
    if keys == EXTERNAL_EVIDENCE_V1_KEYS:
        return 1
    if keys == EXTERNAL_EVIDENCE_V2_KEYS:
        return 2
    return None


def validate_external_manifest_authority(
    external: Any,
    manifest: Any,
    label: str,
    issues: list[str],
) -> None:
    """Require a live manifest to match its candidate-policy authority variant."""
    version = _external_manifest_version(external)
    if version is None or type(manifest) is not dict:
        issues.append(f"{label} cannot be matched to candidate policy authority")
        return
    expected_schema = (
        EXTERNAL_MANIFEST_SCHEMA_V2 if version == 2 else EXTERNAL_MANIFEST_SCHEMA_V1
    )
    expect(manifest.get("schema"), expected_schema, f"{label}.schema", issues)
    expect(manifest.get("schemaVersion"), version, f"{label}.schemaVersion", issues)
    evidence = manifest.get("evidence")
    if type(evidence) is not dict:
        issues.append(f"{label}.evidence must be an object")
        return
    if version == 1:
        if "runtimeCheckpointJournal" in evidence:
            issues.append(f"{label} v1 must not carry runtimeCheckpointJournal")
        return
    expect(
        manifest.get("candidateEvidenceBindingSha256"),
        external["candidateEvidenceBindingSha256"],
        f"{label}.candidateEvidenceBindingSha256", issues,
    )
    expect(
        evidence.get("runtimeCheckpointJournal"),
        external["runtimeCheckpointJournal"],
        f"{label}.evidence.runtimeCheckpointJournal", issues,
    )


def validate_candidate_policy(
    policy: Any,
    root: Path,
    *,
    policy_binding: dict[str, Any],
    check_files: bool,
    require_shipping_receipt: bool = True,
) -> tuple[list[str], dict[str, Any] | None]:
    """Validate and normalize one append-only candidate supplement authority."""
    issues: list[str] = []
    root_keys = {
        "schema", "schemaVersion", "session", "candidateId", "policyId",
        "authority", "state", "releaseReady", "shippingEvidenceId",
        "shippingReceiptPath", "bindings", "archive", "executable",
        "externalEvidence", "runtimeScreenshots",
    }
    if not exact_keys(policy, root_keys, "candidate policy", issues):
        return issues, None
    for key, expected_value in {
        "schema": CANDIDATE_POLICY_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "authority": CANDIDATE_POLICY_AUTHORITY,
        "state": "CANDIDATE_INPUTS_BOUND_SUPPLEMENT_RECEIPT_REQUIRED",
        "releaseReady": False,
    }.items():
        expect(policy[key], expected_value, f"candidate policy.{key}", issues)

    candidate_id = policy["candidateId"]
    if type(candidate_id) is not str or not CANDIDATE_ID_RE.fullmatch(candidate_id):
        issues.append("candidate policy.candidateId is malformed")
        candidate_id = "INVALID_CANDIDATE"
    expect(
        policy["policyId"],
        f"session19_serialized_asset_migration_candidate_{candidate_id}",
        "candidate policy.policyId", issues,
    )
    expect(
        policy["shippingEvidenceId"],
        (
            "session19_retained_serialized_asset_migration_shipping_"
            f"non_distribution_{candidate_id}"
        ),
        "candidate policy.shippingEvidenceId", issues,
    )

    expected_receipt_path = (
        "Evidence/Session19/SerializedAssetMigrationShipping-"
        f"{candidate_id}.json"
    )
    expect(
        policy["shippingReceiptPath"], expected_receipt_path,
        "candidate policy.shippingReceiptPath", issues,
    )
    _resolve_project_path(
        root, policy["shippingReceiptPath"],
        "candidate policy.shippingReceiptPath", issues,
        must_exist=check_files and require_shipping_receipt,
    )

    expected_bindings = _candidate_binding_paths(candidate_id)
    expect(policy["bindings"], expected_bindings, "candidate policy.bindings", issues)
    if type(policy["bindings"]) is dict:
        for key, value in policy["bindings"].items():
            _resolve_project_path(
                root, value, f"candidate policy.bindings.{key}", issues,
                must_exist=check_files,
            )

    archive = policy["archive"]
    archive_keys = {
        "hostPathRecorded", "recoveryLocationToken", "fileCount", "bytes",
        "canonicalManifestSha256",
    }
    if exact_keys(archive, archive_keys, "candidate policy.archive", issues):
        expect(archive["hostPathRecorded"], False,
               "candidate policy.archive.hostPathRecorded", issues)
        expect(
            archive["recoveryLocationToken"],
            f"DGTOUR_PACKAGES/{candidate_id}/Windows",
            "candidate policy.archive.recoveryLocationToken", issues,
        )
        for key in ("fileCount", "bytes"):
            value = archive[key]
            if type(value) is not int or type(value) is bool or value < 1:
                issues.append(f"candidate policy.archive.{key} must be positive integer")
        if (
            type(archive["canonicalManifestSha256"]) is not str
            or not SHA_RE.fullmatch(archive["canonicalManifestSha256"])
        ):
            issues.append(
                "candidate policy.archive.canonicalManifestSha256 must be uppercase SHA-256"
            )

    executable = policy["executable"]
    executable_keys = {"relativePath", "bytes", "sha256"}
    if exact_keys(executable, executable_keys, "candidate policy.executable", issues):
        expect(
            executable["relativePath"], SHIPPING_EXE_RELATIVE.as_posix(),
            "candidate policy.executable.relativePath", issues,
        )
        if (
            type(executable["bytes"]) is not int
            or type(executable["bytes"]) is bool
            or executable["bytes"] < 1
        ):
            issues.append("candidate policy.executable.bytes must be positive integer")
        if type(executable["sha256"]) is not str or not SHA_RE.fullmatch(
                executable["sha256"]):
            issues.append("candidate policy.executable.sha256 must be uppercase SHA-256")

    external = policy["externalEvidence"]
    external_manifest_version = _external_manifest_version(external)
    if external_manifest_version is None:
        if type(external) is not dict:
            issues.append("candidate policy.externalEvidence must be an object")
        else:
            issues.append(
                "candidate policy.externalEvidence keys must match the exact "
                "legacy-v1 or journal-bound-v2 authority layout"
            )
    else:
        token = external["userDirToken"]
        if type(token) is not str or not USER_DIR_TOKEN_RE.fullmatch(token):
            issues.append("candidate policy.externalEvidence.userDirToken is malformed")
            token = "INVALID_TOKEN"
        expect(
            external["runDirectoryLeaf"], f"{candidate_id}_{token}",
            "candidate policy.externalEvidence.runDirectoryLeaf", issues,
        )
        if type(external["manifestSha256"]) is not str or not SHA_RE.fullmatch(
                external["manifestSha256"]):
            issues.append(
                "candidate policy.externalEvidence.manifestSha256 must be uppercase SHA-256"
            )
        expect(
            external["externalTechnicalValidationState"],
            "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
            "candidate policy.externalEvidence.externalTechnicalValidationState",
            issues,
        )
        expect(
            external["freshUserDirValidationState"],
            "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
            "candidate policy.externalEvidence.freshUserDirValidationState", issues,
        )
        if external_manifest_version == 2:
            expect(
                external["manifestSchema"], EXTERNAL_MANIFEST_SCHEMA_V2,
                "candidate policy.externalEvidence.manifestSchema", issues,
            )
            expect(
                external["manifestSchemaVersion"], 2,
                "candidate policy.externalEvidence.manifestSchemaVersion", issues,
            )
            if (
                type(external["candidateEvidenceBindingSha256"]) is not str
                or not SHA_RE.fullmatch(external["candidateEvidenceBindingSha256"])
            ):
                issues.append(
                    "candidate policy.externalEvidence."
                    "candidateEvidenceBindingSha256 must be uppercase SHA-256"
                )
            _validate_runtime_checkpoint_journal_binding(
                external["runtimeCheckpointJournal"],
                "candidate policy.externalEvidence.runtimeCheckpointJournal",
                issues,
            )

    screenshots = policy["runtimeScreenshots"]
    expected_milestones = [
        "HOLE_2_COMPLETE_CHARACTER_RENDERED",
        "HOLE_3_TEE_CHARACTER_RENDERED",
        "HOLE_3_RECOVERED_LIE_CHARACTER_RENDERED",
    ]
    if type(screenshots) is not list:
        issues.append("candidate policy.runtimeScreenshots must be an array")
        screenshots = []
    if [item.get("milestone") if type(item) is dict else None for item in screenshots] != expected_milestones:
        issues.append("candidate policy.runtimeScreenshots milestone order differs")
    screenshot_keys = {
        "milestone", "path", "bytes", "sha256", "width", "height",
        "technicalOnly", "freshInstallEvidence", "humanReviewed",
    }
    for index, item in enumerate(screenshots):
        label = f"candidate policy.runtimeScreenshots[{index}]"
        if not exact_keys(item, screenshot_keys, label, issues):
            continue
        _safe_relative_token(item["path"], f"{label}.path", issues)
        for key in ("bytes", "width", "height"):
            value = item[key]
            if type(value) is not int or type(value) is bool or value < 1:
                issues.append(f"{label}.{key} must be positive integer")
        if type(item["sha256"]) is not str or not SHA_RE.fullmatch(item["sha256"]):
            issues.append(f"{label}.sha256 must be uppercase SHA-256")
        for key, expected_value in {
            "technicalOnly": True,
            "freshInstallEvidence": False,
            "humanReviewed": False,
        }.items():
            expect(item[key], expected_value, f"{label}.{key}", issues)

    policy_path: Any = None
    if exact_keys(
        policy_binding, {"path", "bytes", "sha256"},
        "candidate policy binding", issues,
    ):
        policy_path = policy_binding["path"]
        resolved_policy = _resolve_project_path(
            root, policy_path, "candidate policy binding.path", issues,
            must_exist=check_files,
        )
        if (
            type(policy_binding["bytes"]) is not int
            or type(policy_binding["bytes"]) is bool
            or policy_binding["bytes"] < 1
        ):
            issues.append("candidate policy binding.bytes must be positive integer")
        if (
            type(policy_binding["sha256"]) is not str
            or not SHA_RE.fullmatch(policy_binding["sha256"])
        ):
            issues.append("candidate policy binding.sha256 must be uppercase SHA-256")
        if check_files and resolved_policy is not None:
            observed = {
                "path": policy_path,
                "bytes": resolved_policy.stat().st_size,
                "sha256": sha256(resolved_policy),
            }
            expect(observed, policy_binding, "candidate policy binding identity", issues)
    if type(policy_path) is not str or candidate_id not in Path(policy_path).name:
        issues.append("candidate policy filename must contain its candidateId")
    if issues:
        return issues, None
    return issues, {
        "mode": "selected-policy",
        "candidateId": candidate_id,
        "shippingReceiptPath": expected_receipt_path,
        "receiptSchema": (
            "DiscGolfTour.Session19SerializedAssetMigrationShippingEvidence.v2"
        ),
        "receiptSchemaVersion": 2,
        "candidatePolicyBinding": copy.deepcopy(policy_binding),
        "evidenceId": policy["shippingEvidenceId"],
        "bindingPaths": copy.deepcopy(expected_bindings),
        "archive": copy.deepcopy(archive),
        "executable": copy.deepcopy(executable),
        "externalEvidence": copy.deepcopy(external),
        "externalManifestSchemaVersion": external_manifest_version,
        "runtimeScreenshots": copy.deepcopy(screenshots),
    }


def load_candidate_policy(
    path: Path,
    root: Path,
    *,
    check_files: bool,
    require_shipping_receipt: bool = True,
) -> tuple[list[str], dict[str, Any] | None, dict[str, Any] | None]:
    issues: list[str] = []
    root = root.resolve()
    if path.is_absolute():
        return ["candidate policy must be a project-relative Config path"], None, None
    relative = path
    token = PurePosixPath(*relative.parts).as_posix()
    resolved = _resolve_project_path(
        root, token, "candidate policy path", issues, must_exist=True,
    )
    if resolved is None:
        return issues, None, None
    if not PurePosixPath(token).parts or PurePosixPath(token).parts[0] != "Config":
        issues.append("candidate policy must be selected from project Config")
        return issues, None, None
    try:
        policy = load_json(resolved)
    except StrictJsonError as exc:
        return [f"candidate policy is invalid: {exc}"], None, None
    binding = {
        "path": token,
        "bytes": resolved.stat().st_size,
        "sha256": sha256(resolved),
    }
    policy_issues, authority = validate_candidate_policy(
        policy,
        root,
        policy_binding=binding,
        check_files=check_files,
        require_shipping_receipt=require_shipping_receipt,
    )
    return policy_issues, authority, binding


def resolve_cli_project_file(
    root: Path, value: Path, label: str, *, must_exist: bool,
) -> tuple[Path | None, list[str]]:
    issues: list[str] = []
    root = root.resolve()
    if value.is_absolute():
        try:
            relative = value.resolve(strict=must_exist).relative_to(root)
        except (OSError, ValueError):
            return None, [f"{label} must be inside the project root"]
    else:
        relative = value
    token = PurePosixPath(*relative.parts).as_posix()
    resolved = _resolve_project_path(
        root, token, label, issues, must_exist=must_exist,
    )
    return resolved, issues


def exact_keys(value: Any, expected: set[str], label: str, issues: list[str]) -> bool:
    if type(value) is not dict:
        issues.append(f"{label} must be an object")
        return False
    actual = set(value)
    if actual != expected:
        issues.append(
            f"{label} keys differ: missing={sorted(expected - actual)} "
            f"extra={sorted(actual - expected)}")
        return False
    return True


def expect(actual: Any, expected: Any, label: str, issues: list[str]) -> None:
    if type(actual) is not type(expected) or actual != expected:
        issues.append(f"{label} differs: expected {expected!r}, got {actual!r}")


def validate_shipping_binary_required_markers(
    binary: Any,
    approved_policy: Any,
    approved_policy_sha256: Any,
    issues: list[str],
) -> None:
    """Bind required-marker evidence to the independent binary policy.

    The frozen historical audit contains only the runtime-foundation marker.
    Current audits may contain additional required rows, but only when the
    receipt binds the validator-authenticated current policy and reproduces its
    exact required-marker ID order.  Receipt rows deliberately omit marker
    values, so the ID-to-value authority is re-established here from the
    independently loaded policy before any extra row is accepted.
    """
    label = "Shipping binary audit required markers"
    if type(approved_policy_sha256) is not str or re.fullmatch(
        r"[0-9a-f]{64}", approved_policy_sha256
    ) is None:
        issues.append(f"{label} approved policy SHA-256 is malformed")

    if type(approved_policy) is not dict:
        issues.append(f"{label} approved policy must be an object")
        return
    expect(
        approved_policy.get("policyId"), SHIPPING_BINARY_POLICY_ID,
        f"{label} approved policy ID", issues,
    )
    policy_markers = approved_policy.get("requiredMarkers")
    if type(policy_markers) is not list:
        issues.append(f"{label} approved policy rows must be an array")
        return

    approved_by_id: dict[str, dict[str, Any]] = {}
    approved_ids: list[str] = []
    for index, marker in enumerate(policy_markers):
        marker_label = f"{label} approved policy row {index}"
        if not exact_keys(
            marker, {"id", "value", "requireAnyEncoding"},
            marker_label, issues,
        ):
            continue
        marker_id = marker["id"]
        if type(marker_id) is not str or not marker_id:
            issues.append(f"{marker_label}.id must be a non-empty string")
            continue
        if marker_id in approved_by_id:
            issues.append(f"{label} approved policy contains duplicate ID {marker_id!r}")
            continue
        if type(marker["value"]) is not str or not marker["value"]:
            issues.append(f"{marker_label}.value must be a non-empty string")
        if type(marker["requireAnyEncoding"]) is not bool:
            issues.append(f"{marker_label}.requireAnyEncoding must be a boolean")
        approved_by_id[marker_id] = marker
        approved_ids.append(marker_id)

    expect(
        approved_by_id.get(SHIPPING_BINARY_FOUNDATION_MARKER["id"]),
        SHIPPING_BINARY_FOUNDATION_MARKER,
        f"{label} approved runtime-foundation ID/value", issues,
    )

    if type(binary) is not dict:
        issues.append(f"{label} receipt must be an object")
        return
    expect(
        binary.get("policyId"), SHIPPING_BINARY_POLICY_ID,
        f"{label} receipt policy ID", issues,
    )
    receipt_policy_sha256 = binary.get("policySha256")
    if type(receipt_policy_sha256) is not str or re.fullmatch(
        r"[0-9a-f]{64}", receipt_policy_sha256
    ) is None:
        issues.append(f"{label} receipt policy SHA-256 must be lowercase hex")

    scan = binary.get("scan")
    if type(scan) is not dict:
        issues.append(f"{label} scan must be an object")
        return
    required = scan.get("required")
    if type(required) is not list:
        issues.append(f"{label} receipt rows must be an array")
        return

    receipt_by_id: dict[str, dict[str, Any]] = {}
    receipt_ids: list[str] = []
    for index, row in enumerate(required):
        row_label = f"{label} receipt row {index}"
        if not exact_keys(
            row, {"id", "foundAscii", "foundUtf16Le"}, row_label, issues,
        ):
            continue
        marker_id = row["id"]
        if type(marker_id) is not str or not marker_id:
            issues.append(f"{row_label}.id must be a non-empty string")
            continue
        if marker_id in receipt_by_id:
            issues.append(f"{label} receipt contains duplicate ID {marker_id!r}")
            continue
        if type(row["foundAscii"]) is not bool:
            issues.append(f"{row_label}.foundAscii must be a boolean")
        if type(row["foundUtf16Le"]) is not bool:
            issues.append(f"{row_label}.foundUtf16Le must be a boolean")
        approved = approved_by_id.get(marker_id)
        if approved is None:
            issues.append(f"{label} receipt ID {marker_id!r} is not policy-approved")
        elif (
            approved.get("requireAnyEncoding") is True
            and row["foundAscii"] is not True
            and row["foundUtf16Le"] is not True
        ):
            issues.append(f"{row_label} does not satisfy requireAnyEncoding")
        receipt_by_id[marker_id] = row
        receipt_ids.append(marker_id)

    foundation_id = SHIPPING_BINARY_FOUNDATION_MARKER["id"]
    # The independent policy's requireAnyEncoding contract is an OR.  The
    # generic row check above therefore accepts ASCII-only or UTF-16LE-only;
    # the frozen one-row policy has no separate both-encodings requirement.
    if foundation_id not in receipt_by_id:
        issues.append(f"{label} runtime-foundation row is missing")

    current_policy_bound = receipt_policy_sha256 == approved_policy_sha256
    extra_ids = [marker_id for marker_id in receipt_ids if marker_id != foundation_id]
    if current_policy_bound:
        expect(
            receipt_ids, approved_ids,
            f"{label} current-policy row IDs/order", issues,
        )
    elif extra_ids:
        issues.append(
            f"{label} extra rows require the independently approved current policy"
        )
    elif receipt_policy_sha256 != LEGACY_FOUNDATION_ONLY_BINARY_POLICY_SHA256:
        issues.append(f"{label} foundation-only receipt policy is not frozen/approved")


def safe_path(value: Any, label: str, issues: list[str]) -> Path | None:
    return _resolve_project_path(
        ROOT, value, label, issues, must_exist=False,
    )


def verify_bound_file(
        binding: Any, label: str, issues: list[str], *, check_files: bool) -> Path | None:
    keys = {"path", "bytes", "sha256"}
    if not exact_keys(binding, keys, label, issues):
        return None
    path = safe_path(binding["path"], f"{label}.path", issues)
    if type(binding["bytes"]) is not int or type(binding["bytes"]) is bool or binding["bytes"] < 1:
        issues.append(f"{label}.bytes must be a positive integer")
    if type(binding["sha256"]) is not str or not SHA_RE.fullmatch(binding["sha256"]):
        issues.append(f"{label}.sha256 must be uppercase SHA-256")
    if check_files and path is not None:
        if not path.is_file() or _is_reparse_point(path):
            issues.append(f"{label} is missing: {binding['path']}")
        else:
            actual = identity(path)
            if actual != binding:
                issues.append(f"{label} identity differs: {actual} != {binding}")
    return path


def validate_repair_receipt(
    repair: Any, issues: list[str], *, check_files: bool
) -> dict[str, Any] | None:
    root_keys = {
        "schema", "schemaVersion", "session", "authority", "state",
        "sourceMigrationReceipt", "repairScript", "target",
        "repairedProperties", "unchangedContract", "protectedSave",
        "acceptance",
    }
    if not exact_keys(repair, root_keys, "repair receipt", issues):
        return None
    for key, expected in {
        "schema": "DiscGolfTour.Session19RetainedAvatarProfileRepairReceipt.v1",
        "schemaVersion": 1,
        "session": 19,
        "authority": "ADDITIVE_ENGINEERING_EVIDENCE_NOT_RELEASE_OR_LEGAL_APPROVAL",
        "state": (
            "EXACT_RETAINED_PROFILE_LEGACY_DEFAULTS_REPAIRED_"
            "FRESH_SHIPPING_PACKAGE_PROOF_PENDING"
        ),
    }.items():
        expect(repair[key], expected, f"repair receipt.{key}", issues)

    verify_bound_file(
        repair["sourceMigrationReceipt"],
        "repair receipt.sourceMigrationReceipt",
        issues,
        check_files=check_files,
    )
    verify_bound_file(
        repair["repairScript"],
        "repair receipt.repairScript",
        issues,
        check_files=check_files,
    )

    target = repair["target"]
    if not exact_keys(
        target, {"package", "path", "before", "after"},
        "repair receipt.target", issues,
    ):
        return None
    expect(target["package"], REPAIRED_PROFILE_PACKAGE,
           "repair receipt.target.package", issues)
    target_path = safe_path(
        target["path"], "repair receipt.target.path", issues)
    for phase, expected in {
        "before": {
            "bytes": 2176,
            "sha256": "12E2A6BC0E01EEB9D69C4459B5FA3A963DC756F644EAA001D14CE15486E68465",
        },
        "after": {
            "bytes": 2608,
            "sha256": "F8B03386EF8C40307D49C8642EE167F486B88EAC70FDD0556D5605AB0317C8AD",
        },
    }.items():
        value = target[phase]
        if exact_keys(value, {"bytes", "sha256"},
                      f"repair receipt.target.{phase}", issues):
            expect(value, expected, f"repair receipt.target.{phase}", issues)

    expected_properties = [
        {"name": "MetaHumanRuntimeMode", "previousValue": "Disabled",
         "acceptedValue": "ShippingSafeAssembled"},
        {"name": "bUseRuntimeRetargeting", "previousValue": False,
         "acceptedValue": True},
        {"name": "VisualBodyComponentTag", "previousValue": "Body",
         "acceptedValue": "DGVisualBody"},
        {"name": "VisualHeadComponentTag", "previousValue": "Face",
         "acceptedValue": "DGVisualHead"},
    ]
    expect(repair["repairedProperties"], expected_properties,
           "repair receipt.repairedProperties", issues)
    expect(repair["unchangedContract"], {
        "backendId": "metahuman_assembled",
        "backend": "MetaHumanPreset",
        "visualActorClass": (
            "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default."
            "BP_DG_MetaHuman_Default_C"
        ),
        "retargetAsset": (
            "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
            "RTG_DGMaster_To_MetaHuman"
        ),
        "preferredQualityProfileId": "GameplayPerformance",
        "allowRuntimeFaceSculpting": False,
    }, "repair receipt.unchangedContract", issues)
    verify_bound_file(
        repair["protectedSave"], "repair receipt.protectedSave", issues,
        check_files=check_files,
    )
    expect(repair["acceptance"], {
        "onlyExactTargetAssetSaved": True,
        "legacySerializedEnumValuesRestored": True,
        "legacyUnserializedDefaultsMadeExplicit": True,
        "protectedProductionSaveUnchanged": True,
        "freshShippingPackageEvidenceAccepted": False,
        "releaseReady": False,
    }, "repair receipt.acceptance", issues)

    if check_files and target_path is not None:
        expected_live = {
            "path": target["path"],
            "bytes": target["after"]["bytes"],
            "sha256": target["after"]["sha256"],
        }
        if not target_path.is_file() or identity(target_path) != expected_live:
            issues.append(
                f"repair receipt target live identity differs: "
                f"{identity(target_path) if target_path.is_file() else 'missing'}"
            )
    return {
        "path": target.get("path"),
        "before": target.get("before"),
        "after": target.get("after"),
    }


def _read_inventory(path: Path, issues: list[str]) -> list[dict[str, str]]:
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
    except OSError as exc:
        issues.append(f"inventory cannot be read: {exc}")
        return []
    if len(rows) != 45:
        issues.append(f"inventory must contain 45 rows, got {len(rows)}")
    if len({row.get("package") for row in rows}) != len(rows):
        issues.append("inventory package names are not unique")
    return rows


def validate(receipt: Any, root: Path = ROOT, *, check_files: bool = True) -> tuple[list[str], dict[str, int]]:
    global ROOT
    old_root = ROOT
    ROOT = root.resolve()
    issues: list[str] = []
    summary = {"retained": 0, "excluded": 0, "legacyMarkers": 0}
    try:
        repair_state: dict[str, Any] | None = None
        if check_files:
            repair_path = ROOT / REPAIR_RECEIPT_PATH.relative_to(old_root)
            try:
                repair = load_json(repair_path)
            except StrictJsonError as exc:
                issues.append(f"retained profile repair receipt invalid: {exc}")
            else:
                repair_state = validate_repair_receipt(
                    repair, issues, check_files=True
                )
        root_keys = {
            "schema", "schemaVersion", "session", "evidenceId", "authority", "state",
            "policy", "inventory", "workflow", "boundary", "retainedAssets",
            "excludedEvidence", "acceptance", "closure",
        }
        if not exact_keys(receipt, root_keys, "receipt", issues):
            return issues, summary
        for key, value in {
            "schema": "DiscGolfTour.Session19SerializedAssetMigrationReceipt.v1",
            "schemaVersion": 1,
            "session": 19,
            "evidenceId": "session19_retained_v05_serialized_asset_migration_20260824",
            "authority": "ADDITIVE_ENGINEERING_EVIDENCE_NOT_RELEASE_OR_LEGAL_APPROVAL",
            "state": "SOURCE_AND_RETAINED_ASSET_MIGRATION_VERIFIED_FRESH_SHIPPING_PACKAGE_PROOF_PENDING",
        }.items():
            expect(receipt[key], value, key, issues)

        policy_path = verify_bound_file(receipt["policy"], "policy", issues, check_files=check_files)
        inventory_path = verify_bound_file(receipt["inventory"], "inventory", issues, check_files=check_files)

        boundary_expected = {
            "legacyScriptPackage": "/Script/DiscGolfCharacterFramework",
            "replacementScriptPackage": "/Script/DiscGolfRuntimeFoundation",
            "retainedAssetCount": 6,
            "excludedAssetCount": 39,
            "protectedContentFileCount": 414,
            "protectedContentMutationCount": 0,
            "legacyPluginProjectEntryPresent": False,
            "legacyPluginEnabledByDefault": False,
            "legacySourceTreeRetainedPendingVerifiedQuarantine": True,
        }
        expect(receipt["boundary"], boundary_expected, "boundary", issues)

        workflow_keys = {
            "command", "migrationScript", "controlRigFoundationScript",
            "recreationRun", "normalizationAcceptanceRun", "finalCleanLoadRun",
            "editorBuildRun", "finalRunReport",
        }
        workflow = receipt["workflow"]
        if exact_keys(workflow, workflow_keys, "workflow", issues):
            expect(workflow["command"], (
                "UnrealEditor-Cmd.exe C:/DGTour/DiscGolfTour.uproject -run=PythonScript "
                "-script=C:/DGTour/Scripts/migrate_dg_session19_serialized_assets.py "
                "-unattended -nop4 -nosplash -nullrhi"), "workflow.command", issues)
            migration_script = verify_bound_file(
                workflow["migrationScript"], "workflow.migrationScript", issues,
                check_files=check_files)
            foundation_script = verify_bound_file(
                workflow["controlRigFoundationScript"],
                "workflow.controlRigFoundationScript", issues, check_files=check_files)
            recreate_log = verify_bound_file(
                workflow["recreationRun"], "workflow.recreationRun", issues,
                check_files=check_files)
            normalize_log = verify_bound_file(
                workflow["normalizationAcceptanceRun"],
                "workflow.normalizationAcceptanceRun", issues, check_files=check_files)
            clean_log = verify_bound_file(
                workflow["finalCleanLoadRun"], "workflow.finalCleanLoadRun", issues,
                check_files=check_files)
            editor_log = verify_bound_file(
                workflow["editorBuildRun"], "workflow.editorBuildRun", issues,
                check_files=check_files)
            report_path = verify_bound_file(
                workflow["finalRunReport"], "workflow.finalRunReport", issues,
                check_files=check_files)
            if check_files:
                if migration_script and "runpy.run_path" not in migration_script.read_text(encoding="utf-8"):
                    issues.append("migration script does not invoke the project-owned rig workflow")
                if foundation_script:
                    source = foundation_script.read_text(encoding="utf-8")
                    if "DG_SESSION2_RIG:" not in source or 'f"PASS ik=' not in source:
                        issues.append("foundation script lacks its Session 2 pass marker")
                if recreate_log:
                    text = recreate_log.read_text(encoding="utf-8", errors="replace")
                    if "DG_SESSION2_RIG: PASS" not in text:
                        issues.append("recreation run does not prove independent Session 2 rig creation")
                    if "Session 4 authoring utility did not accept recreated CR_DG_Master" not in text:
                        issues.append("recreation run does not preserve its fail-closed handoff")
                if normalize_log:
                    text = normalize_log.read_text(encoding="utf-8", errors="replace")
                    for marker in [
                        "Compiling Blueprint '/Game/DiscGolf/Rigs/CR_DG_Master.CR_DG_Master'",
                        "DG_SESSION19_SERIALIZED_MIGRATION: PASS retained=6 excluded_unchanged=39 protected_mutations=0",
                        "Success - 0 error(s), 0 warning(s)",
                    ]:
                        if marker not in text:
                            issues.append(f"normalization/acceptance log lacks marker: {marker}")
                if clean_log:
                    text = clean_log.read_text(encoding="utf-8", errors="replace")
                    for forbidden in [
                        "Mounting Project plugin DiscGolfCharacterFramework",
                        "InternalLoadLibrary: 'DiscGolfCharacterFramework'",
                        "/Script/DiscGolfCharacterFramework",
                    ]:
                        if forbidden in text:
                            issues.append(f"final clean-load log contains legacy load marker: {forbidden}")
                    for marker in [
                        "DG_SESSION19_SERIALIZED_MIGRATION: PASS retained=6 excluded_unchanged=39 protected_mutations=0",
                        "Python script executed successfully",
                        "Success - 0 error(s), 0 warning(s)",
                    ]:
                        if marker not in text:
                            issues.append(f"final clean-load log lacks marker: {marker}")
                if editor_log and "Result: Succeeded" not in editor_log.read_text(encoding="utf-8", errors="replace"):
                    issues.append("Editor build log does not report success")
                if report_path:
                    report = load_json(report_path)
                    for key, expected in {
                        "status": "PASS_RETAINED_ASSETS_RESAVED_EXCLUDED_ASSETS_UNCHANGED",
                        "retainedAssetCount": 6,
                        "excludedAssetCount": 39,
                        "loadedAndSavedCount": 6,
                        "protectedContentMutationCount": 0,
                        "legacyPluginProjectEntryPresent": False,
                        "legacyPluginEnabledByDefault": False,
                        # This final verification run correctly did not recreate an already
                        # recreated rig; the preceding two bound logs prove that provenance.
                        "controlRigRecreatedFromProjectOwnedWorkflow": False,
                        "releaseReady": False,
                        "shippingPackageEvidenceAccepted": False,
                    }.items():
                        expect(report.get(key), expected, f"final report.{key}", issues)

        rows = _read_inventory(inventory_path, issues) if check_files and inventory_path else []
        retained_rows = [row for row in rows if row.get("disposition") == "RETAINED"]
        excluded_rows = [row for row in rows if row.get("disposition") == "EXCLUDED"]
        if rows:
            expect([row["package"] for row in retained_rows], RETAINED,
                   "inventory retained packages", issues)
            if len(excluded_rows) != 39:
                issues.append(f"inventory excluded count differs: {len(excluded_rows)}")
            for row in excluded_rows:
                if not any(row["package"].startswith(root + "/") for root in EXCLUDED_ROOTS):
                    issues.append(f"excluded package is outside NeverCook roots: {row['package']}")

        retained_assets = receipt["retainedAssets"]
        if type(retained_assets) is not list:
            issues.append("retainedAssets must be an array")
            retained_assets = []
        summary["retained"] = len(retained_assets)
        expect([item.get("package") for item in retained_assets if type(item) is dict],
               RETAINED, "retained asset order", issues)
        for index, item in enumerate(retained_assets):
            label = f"retainedAssets[{index}]"
            if not exact_keys(item, {"package", "path", "bytes", "sha256"}, label, issues):
                continue
            path = safe_path(item["path"], f"{label}.path", issues)
            if type(item["sha256"]) is not str or not SHA_RE.fullmatch(item["sha256"]):
                issues.append(f"{label}.sha256 must be uppercase SHA-256")
            if check_files and path:
                expected_identity = {key: item[key] for key in ("path", "bytes", "sha256")}
                if item["package"] == REPAIRED_PROFILE_PACKAGE:
                    if repair_state is None:
                        issues.append(f"{label} lacks accepted Session 19 repair evidence")
                    else:
                        expect(
                            repair_state["path"], item["path"],
                            f"{label} repair path", issues,
                        )
                        expect(
                            repair_state["before"],
                            {"bytes": item["bytes"], "sha256": item["sha256"]},
                            f"{label} repair before identity", issues,
                        )
                        expected_identity = {
                            "path": item["path"],
                            "bytes": repair_state["after"]["bytes"],
                            "sha256": repair_state["after"]["sha256"],
                        }
                if not path.is_file() or identity(path) != expected_identity:
                    issues.append(f"{label} identity differs")
                else:
                    data = path.read_bytes()
                    if OLD_SCRIPT in data:
                        issues.append(f"{label} retains the legacy script import")
                    if NEW_SCRIPT not in data:
                        issues.append(f"{label} lacks the replacement script import")

        excluded_expected = {
            "assetCount": 39,
            "allPreMigrationIdentitiesPreserved": True,
            "legacyMarkerAssetCount": 39,
            "allLegacyMarkerAssetsWithinNeverCookRoots": True,
        }
        expect(receipt["excludedEvidence"], excluded_expected, "excludedEvidence", issues)
        summary["excluded"] = 39
        if check_files and excluded_rows:
            for row in excluded_rows:
                path = ROOT / row["file"]
                if not path.is_file():
                    issues.append(f"excluded asset is missing: {row['file']}")
                    continue
                actual = {"bytes": path.stat().st_size, "sha256": sha256(path)}
                expected = {"bytes": int(row["pre_bytes"]), "sha256": row["pre_sha256"]}
                if actual != expected:
                    issues.append(f"excluded asset identity changed: {row['file']}")
                if OLD_SCRIPT not in path.read_bytes():
                    issues.append(f"excluded asset lost its inventoried legacy marker: {row['file']}")
            marker_paths = {
                path.relative_to(ROOT).as_posix()
                for path in (ROOT / "Content").rglob("*.uasset")
                if OLD_SCRIPT in path.read_bytes()
            }
            expected_paths = {row["file"] for row in excluded_rows}
            summary["legacyMarkers"] = len(marker_paths)
            if marker_paths != expected_paths:
                issues.append(
                    f"legacy-marker asset set differs: missing={sorted(expected_paths-marker_paths)} "
                    f"extra={sorted(marker_paths-expected_paths)}")

        acceptance_expected = {
            "editorDevelopmentBuildSucceeded": True,
            "retainedPackagesLoadedAndSaved": 6,
            "blueprintsAndControlRigCompiled": [
                "/Game/DiscGolf/Animation/ABP_DG_Player",
                "/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default",
                "/Game/DiscGolf/Rigs/CR_DG_Master",
            ],
            "legacyModuleNotLoadedInFinalRun": True,
            "gitLfsTracksUassetAndUmap": True,
        }
        expect(receipt["acceptance"], acceptance_expected, "acceptance", issues)
        closure_expected = {
            "sourceAndRetainedAssetMigrationAccepted": True,
            "freshShippingPackageEvidenceAccepted": False,
            "blockerId": "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
            "blockerClosed": False,
            "remainingReleaseBlockerCount": 13,
            "releaseReady": False,
        }
        expect(receipt["closure"], closure_expected, "closure", issues)

        if check_files:
            project = load_json(ROOT / "DiscGolfTour.uproject")
            modules = {item.get("Name") for item in project.get("Modules", [])}
            plugins = {item.get("Name") for item in project.get("Plugins", [])}
            if "DiscGolfRuntimeFoundation" not in modules:
                issues.append("DiscGolfRuntimeFoundation is absent from .uproject")
            if "DiscGolfCharacterFramework" in plugins:
                issues.append("legacy plugin entry remains in .uproject")
            plugin_path = (
                ROOT / "Plugins/DiscGolfCharacterFramework/"
                "DiscGolfCharacterFramework.uplugin"
            )
            if plugin_path.is_file():
                plugin = load_json(plugin_path)
                if plugin.get("EnabledByDefault") is not False:
                    issues.append("retained legacy plugin source is not disabled by default")
            else:
                # The migration receipt records the source-retained state at the
                # time of the successful Editor resave. A later additive Session
                # 19 receipt may supersede only this live-path expectation after
                # it verifies the exact before-move inventory, sanitized recovery
                # receipt, and absence of both framework roots from the project.
                try:
                    from validate_dg_session19_release_scope import (
                        validate_framework_quarantine_evidence,
                    )
                except ImportError as exc:
                    issues.append(
                        f"character framework quarantine verifier import failed: {exc}")
                else:
                    quarantine_issues = validate_framework_quarantine_evidence(ROOT)
                    issues.extend(
                        f"post-migration framework quarantine: {issue}"
                        for issue in quarantine_issues
                    )
            attrs = (ROOT / ".gitattributes").read_text(encoding="utf-8")
            for pattern in ["*.uasset", "*.umap"]:
                if pattern not in attrs or "filter=lfs" not in attrs:
                    issues.append(f"Git LFS rule is absent for {pattern}")
            if policy_path:
                policy = load_json(policy_path)
                expect(policy.get("sourceAndRetainedAssetMigrationAccepted"), True,
                       "policy source acceptance", issues)
                expect(policy.get("blockerClosed"), False, "policy blockerClosed", issues)
                expect(policy.get("shippingPackageEvidenceAccepted"), False,
                       "policy shippingPackageEvidenceAccepted", issues)
            for relative in [
                "Config/DefaultGame.ini",
                "Config/DG_RuntimeCookManifest.json",
                "Source/DiscGolfTourEditor/DiscGolfSession3AssetUtility.cpp",
                "Source/DiscGolfTourEditor/DiscGolfSession4AssetUtility.cpp",
                "Source/DiscGolfTourEditor/DiscGolfSession5MocapUtility.cpp",
            ]:
                data = (ROOT / relative).read_bytes()
                if OLD_SCRIPT in data or NEW_SCRIPT not in data:
                    issues.append(f"live binding is not Foundation-only: {relative}")
    finally:
        ROOT = old_root
    return issues, summary


def _jpeg_dimensions(path: Path) -> tuple[int, int]:
    """Return JPEG dimensions without adding a runtime image dependency."""
    data = path.read_bytes()
    if len(data) < 4 or data[:2] != b"\xff\xd8":
        raise ValueError("not a JPEG stream")
    offset = 2
    start_of_frame = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
        0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF,
    }
    while offset + 4 <= len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        while offset < len(data) and data[offset] == 0xFF:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker in {0x01, *range(0xD0, 0xD9)}:
            continue
        if offset + 2 > len(data):
            break
        length = int.from_bytes(data[offset:offset + 2], "big")
        if length < 2 or offset + length > len(data):
            break
        if marker in start_of_frame:
            if length < 7:
                break
            height = int.from_bytes(data[offset + 3:offset + 5], "big")
            width = int.from_bytes(data[offset + 5:offset + 7], "big")
            if width < 1 or height < 1:
                break
            return width, height
        offset += length
    raise ValueError("JPEG dimensions are unavailable")


def _png_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError("not a PNG stream")
    width = int.from_bytes(data[16:20], "big")
    height = int.from_bytes(data[20:24], "big")
    if width < 1 or height < 1:
        raise ValueError("PNG dimensions are invalid")
    return width, height


def _safe_external_path(
    external_root: Path, value: Any, label: str, issues: list[str]
) -> Path | None:
    pure = _safe_relative_token(value, label, issues)
    if pure is None:
        return None
    external_root = external_root.resolve()
    lexical = external_root.joinpath(*pure.parts)
    cursor = external_root
    for part in pure.parts:
        cursor /= part
        if cursor.exists() and _is_reparse_point(cursor):
            issues.append(f"{label} contains a reparse point")
            return None
    path = lexical.resolve()
    try:
        path.relative_to(external_root)
    except ValueError:
        issues.append(f"{label} escapes the external-evidence root")
        return None
    return path


def _without_volatile(value: Any, *keys: str) -> Any:
    result = copy.deepcopy(value)
    if type(result) is dict:
        for key in keys:
            result.pop(key, None)
    return result


def _validate_shipping_inputs(
    authority: dict[str, Any],
    candidate_id: str | None,
    archive: Path | None,
    executable: Path | None,
    unrealpak: Path | None,
    external_run: Path | None,
    issues: list[str],
) -> tuple[Path | None, Path | None, Path | None, Path | None]:
    expected_candidate_id = authority["candidateId"]
    executable_authority = authority["executable"]
    external_authority = authority["externalEvidence"]
    expect(candidate_id, expected_candidate_id, "candidate input id", issues)
    for label, value in (
        ("archive", archive),
        ("executable", executable),
        ("UnrealPak", unrealpak),
        ("external run", external_run),
    ):
        if value is not None and _host_path_contains_reparse(value):
            issues.append(f"candidate input {label} contains a reparse point")
    resolved_archive = archive.resolve() if archive is not None else None
    resolved_executable = executable.resolve() if executable is not None else None
    resolved_unrealpak = unrealpak.resolve() if unrealpak is not None else None
    resolved_external_run = external_run.resolve() if external_run is not None else None
    if resolved_archive is None or not resolved_archive.is_dir():
        issues.append("candidate input archive must be an existing directory")
    elif (
        resolved_archive.name != "Windows"
        or resolved_archive.parent.name != expected_candidate_id
    ):
        issues.append("candidate input archive is not the exact candidate Windows archive")
    if resolved_executable is None or not resolved_executable.is_file():
        issues.append("candidate input executable must be an existing file")
    elif resolved_archive is not None:
        expected_executable = (
            resolved_archive / Path(executable_authority["relativePath"])
        ).resolve()
        if resolved_executable != expected_executable:
            issues.append("candidate input executable is not the archive's exact inner Shipping PE")
    if resolved_unrealpak is None or not resolved_unrealpak.is_file():
        issues.append("candidate input UnrealPak must be an existing file")
    elif resolved_unrealpak.name.casefold() != "unrealpak.exe":
        issues.append("candidate input UnrealPak filename differs")
    expected_external_leaf = external_authority["runDirectoryLeaf"]
    if resolved_external_run is None or not resolved_external_run.is_dir():
        issues.append("candidate input external run must be an existing directory")
    elif resolved_external_run.name != expected_external_leaf:
        issues.append("candidate input external run is not the exact candidate/token directory")
    return resolved_archive, resolved_executable, resolved_unrealpak, resolved_external_run


def validate_shipping_evidence(
    receipt: Any,
    root: Path = ROOT,
    *,
    candidate_authority: dict[str, Any] | None = None,
    candidate_id: str | None = None,
    archive: Path | None = None,
    executable: Path | None = None,
    unrealpak: Path | None = None,
    external_run: Path | None = None,
    check_files: bool = True,
    live_reaudit: bool = True,
) -> tuple[list[str], dict[str, int]]:
    """Validate the candidate supplement without widening its claim boundary."""
    global ROOT
    old_root = ROOT
    ROOT = root.resolve()
    issues: list[str] = []
    summary = {"retained": 0, "screenshots": 0, "legacyArchiveMatches": 0}
    authority = (
        historical_shipping_authority()
        if candidate_authority is None
        else candidate_authority
    )
    expected_candidate_id = authority["candidateId"]
    executable_authority = authority["executable"]
    external_authority = authority["externalEvidence"]
    try:
        root_keys = {
            "schema", "schemaVersion", "session", "candidateId", "evidenceId",
            "authority", "state", "bindings", "archive", "retainedRuntimeAssets",
            "runtimeScreenshots", "nonDistributionBoundary", "claimBoundary",
        }
        if authority["candidatePolicyBinding"] is not None:
            root_keys.add("candidatePolicy")
        if not exact_keys(receipt, root_keys, "shipping receipt", issues):
            return issues, summary
        for key, expected in {
            "schema": authority["receiptSchema"],
            "schemaVersion": authority["receiptSchemaVersion"],
            "session": 19,
            "candidateId": expected_candidate_id,
            "evidenceId": authority["evidenceId"],
            "authority": "ADDITIVE_ENGINEERING_EVIDENCE_NOT_RELEASE_OR_LEGAL_APPROVAL",
            "state": "PASS_CANDIDATE_BOUND_TECHNICAL_MIGRATION_SHIPPING_NON_DISTRIBUTION",
        }.items():
            expect(receipt[key], expected, f"shipping receipt.{key}", issues)
        if authority["candidatePolicyBinding"] is not None:
            expect(
                receipt["candidatePolicy"], authority["candidatePolicyBinding"],
                "shipping receipt.candidatePolicy", issues,
            )

        bindings = receipt["bindings"]
        expected_binding_paths = authority["bindingPaths"]
        binding_paths: dict[str, Path | None] = {}
        if exact_keys(
            bindings, set(expected_binding_paths), "shipping receipt.bindings", issues
        ):
            for key, expected_path in expected_binding_paths.items():
                binding = bindings[key]
                path = verify_bound_file(
                    binding, f"shipping receipt.bindings.{key}", issues,
                    check_files=check_files,
                )
                binding_paths[key] = path
                if type(binding) is dict:
                    expect(
                        binding.get("path"), expected_path,
                        f"shipping receipt.bindings.{key}.path", issues,
                    )

        expect(receipt["archive"], authority["archive"],
               "shipping receipt.archive", issues)
        expect(
            receipt["retainedRuntimeAssets"], SHIPPING_RETAINED,
            "shipping receipt.retainedRuntimeAssets", issues,
        )
        if type(receipt["retainedRuntimeAssets"]) is list:
            summary["retained"] = len(receipt["retainedRuntimeAssets"])
        expect(
            receipt["runtimeScreenshots"], authority["runtimeScreenshots"],
            "shipping receipt.runtimeScreenshots", issues,
        )
        if type(receipt["runtimeScreenshots"]) is list:
            summary["screenshots"] = len(receipt["runtimeScreenshots"])

        expected_non_distribution = {
            "legacyScriptPackage": "/Script/DiscGolfCharacterFramework",
            "replacementScriptPackage": "/Script/DiscGolfRuntimeFoundation",
            "candidateContentCategory": "LEGACY_CHARACTER_FRAMEWORK",
            "archiveAuthoritativeMatchCount": 0,
            "archiveManifestDiagnosticMatchCount": 0,
            "shippingExecutableForbiddenMatchCount": 0,
            "shippingExecutableReplacementMarkerFound": True,
            "legacyFrameworkNonDistributionVerified": True,
            "technicalMigrationShippingBoundaryClosed": True,
        }
        expect(
            receipt["nonDistributionBoundary"], expected_non_distribution,
            "shipping receipt.nonDistributionBoundary", issues,
        )
        if type(receipt["nonDistributionBoundary"]) is dict:
            value = receipt["nonDistributionBoundary"].get(
                "archiveAuthoritativeMatchCount"
            )
            if type(value) is int and type(value) is not bool:
                summary["legacyArchiveMatches"] = value

        expected_claims = {
            "sourceAndRetainedAssetMigrationAccepted": True,
            "cleanShippingBuildReceiptAccepted": True,
            "freshShippingPackageEvidenceAccepted": True,
            "candidateBoundTechnicalEvidenceAccepted": True,
            "freshInstallGameplayAcceptance": False,
            "manualGameplayAcceptance": False,
            "humanPlayFeelApproval": False,
            "visualProductApproval": False,
            "accessibilityApproval": False,
            "sourceDistributionRightsResolved": False,
            "distributionClearance": False,
            "legalApproval": False,
            "blockerId": "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED",
            "blockerClosed": False,
            "releaseApproval": False,
            "releaseReady": False,
        }
        expect(
            receipt["claimBoundary"], expected_claims,
            "shipping receipt.claimBoundary", issues,
        )

        if not check_files:
            return issues, summary

        resolved_archive, resolved_executable, resolved_unrealpak, resolved_external_run = (
            _validate_shipping_inputs(
                authority, candidate_id, archive, executable, unrealpak,
                external_run, issues
            )
        )

        source_path = binding_paths.get("sourceMigrationReceipt")
        if source_path is not None and source_path.is_file():
            try:
                source_receipt = load_json(source_path)
            except StrictJsonError as exc:
                issues.append(f"bound source migration receipt invalid: {exc}")
            else:
                source_issues, _ = validate(source_receipt, ROOT, check_files=True)
                issues.extend(
                    f"bound source migration receipt: {issue}"
                    for issue in source_issues
                )

        external_validation_path = binding_paths.get("externalTechnicalValidation")
        if external_validation_path is not None and external_validation_path.is_file():
            try:
                external_validation = load_json(external_validation_path)
            except StrictJsonError as exc:
                issues.append(f"bound external technical validation is invalid: {exc}")
            else:
                expect(
                    external_validation.get("candidateId"), expected_candidate_id,
                    "external technical validation candidate", issues,
                )
                expect(
                    external_validation.get("state"),
                    external_authority["externalTechnicalValidationState"],
                    "external technical validation state", issues,
                )
                expect(
                    external_validation.get("manifest", {}).get("sha256"),
                    external_authority["manifestSha256"],
                    "external technical validation manifest hash", issues,
                )
                for claim in (
                    "humanPlayFeelApproval", "legalApproval",
                    "manualGameplayAcceptance", "releaseApproval", "releaseReady",
                    "visualProductApproval",
                ):
                    expect(
                        external_validation.get("claimBoundary", {}).get(claim),
                        False, f"external technical validation claim {claim}", issues,
                    )

        fresh_validation: Any = None
        fresh_validation_path = binding_paths.get("freshUserDirValidation")
        if fresh_validation_path is not None and fresh_validation_path.is_file():
            try:
                fresh_validation = load_json(fresh_validation_path)
            except StrictJsonError as exc:
                issues.append(f"bound fresh user-dir validation is invalid: {exc}")
            else:
                expect(
                    fresh_validation.get("candidateId"), expected_candidate_id,
                    "fresh user-dir validation candidate", issues,
                )
                expect(
                    fresh_validation.get("state"),
                    external_authority["freshUserDirValidationState"],
                    "fresh user-dir validation state", issues,
                )
                expect(
                    fresh_validation.get("userDirToken"),
                    external_authority["userDirToken"],
                    "fresh user-dir validation token", issues,
                )
                expect(
                    fresh_validation.get("counts", {}).get("forbiddenArtifacts"), 0,
                    "fresh user-dir forbidden artifacts", issues,
                )
                expect(
                    fresh_validation.get("releaseBoundary", {}).get("releaseReadinessClaimed"),
                    False, "fresh user-dir release claim", issues,
                )

        if resolved_external_run is not None:
            manifest_path = resolved_external_run / "technical-evidence-manifest.json"
            if not manifest_path.is_file() or manifest_path.is_symlink():
                issues.append("external technical-evidence manifest is missing or linked")
            elif sha256(manifest_path) != external_authority["manifestSha256"]:
                issues.append("external technical-evidence manifest SHA-256 differs")
            else:
                try:
                    manifest = load_json(manifest_path)
                except StrictJsonError as exc:
                    issues.append(f"external technical-evidence manifest is invalid: {exc}")
                else:
                    validate_external_manifest_authority(
                        external_authority,
                        manifest,
                        "external manifest",
                        issues,
                    )
                    expect(
                        manifest.get("candidate", {}).get("candidateId"),
                        expected_candidate_id,
                        "external manifest candidate id", issues,
                    )
                    expect(
                        manifest.get("userDir", {}).get("token"),
                        external_authority["userDirToken"],
                        "external manifest user-dir token", issues,
                    )
                    expect(
                        manifest.get("claimBoundary", {}).get("releaseReady"),
                        False, "external manifest release claim", issues,
                    )
                    if authority.get("externalManifestSchemaVersion", 1) == 2:
                        launch_record = manifest.get("evidence", {}).get(
                            "launchRecord", {}
                        )
                        expected_runtime = external_authority[
                            "runtimeCheckpointJournal"
                        ]
                        expected_fresh_binding = {
                            "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
                            "userDirRelativePath": expected_runtime[
                                "userDirRelativePath"
                            ],
                            "bytes": expected_runtime["bytes"],
                            "sha256": expected_runtime["sha256"],
                            "captureNonce": expected_runtime["captureNonce"],
                            "roundId": expected_runtime["roundId"],
                            "launchRecordSha256": launch_record.get("sha256"),
                            "technicalEvidenceManifestSha256": (
                                external_authority["manifestSha256"]
                            ),
                            "executableSha256": executable_authority["sha256"],
                            "archiveManifestSha256": authority["archive"][
                                "canonicalManifestSha256"
                            ],
                            "validationState": RUNTIME_JOURNAL_PASS,
                        }
                        expect(
                            fresh_validation.get("runtimeJournalBinding")
                            if type(fresh_validation) is dict else None,
                            expected_fresh_binding,
                            "fresh user-dir runtime journal binding",
                            issues,
                        )

        for item in authority["runtimeScreenshots"]:
            path = None if resolved_external_run is None else _safe_external_path(
                resolved_external_run, item["path"],
                f"runtime screenshot {item['milestone']}", issues,
            )
            if path is None:
                continue
            if not path.is_file() or path.is_symlink():
                issues.append(f"runtime screenshot is missing: {item['path']}")
                continue
            if path.stat().st_size != item["bytes"] or sha256(path) != item["sha256"]:
                issues.append(f"runtime screenshot identity differs: {item['path']}")
            try:
                dimensions = _png_dimensions(path)
            except (OSError, ValueError) as exc:
                issues.append(f"runtime screenshot PNG is invalid: {item['path']}: {exc}")
            else:
                if dimensions != (item["width"], item["height"]):
                    issues.append(
                        f"runtime screenshot dimensions differ: {item['path']} "
                        f"{dimensions}"
                    )

        verification_path = binding_paths.get("candidateVerification")
        content_path = binding_paths.get("candidateContentAudit")
        binary_path = binding_paths.get("shippingBinaryAudit")
        verification: Any = None
        content: Any = None
        binary: Any = None
        for label, path in [
            ("candidate verification", verification_path),
            ("candidate content audit", content_path),
            ("Shipping binary audit", binary_path),
        ]:
            if path is None or not path.is_file():
                continue
            try:
                value = load_json(path)
            except StrictJsonError as exc:
                issues.append(f"bound {label} is invalid: {exc}")
                continue
            value = require_bound_receipt_object(value, label, issues)
            if value is None:
                continue
            if label == "candidate verification":
                verification = value
            elif label == "candidate content audit":
                content = value
            else:
                binary = value

        if type(verification) is dict:
            for key, expected in {
                "schema": "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2",
                "schemaVersion": 2,
                "session": 19,
                "runId": expected_candidate_id,
                "state": "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING",
                "releaseReady": False,
            }.items():
                expect(verification.get(key), expected, f"candidate verification.{key}", issues)
            expect(
                verification.get("innerShippingExecutable"),
                {
                    "relativePath": executable_authority["relativePath"],
                    "bytes": executable_authority["bytes"],
                    "sha256": executable_authority["sha256"],
                },
                "candidate verification.innerShippingExecutable", issues,
            )

        if type(content) is dict:
            for key, expected in {
                "schema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
                "schemaVersion": 1,
                "session": 19,
                "runId": expected_candidate_id,
                "state": "PASS_BOUNDED_STAGED_CONTENT_AUDIT",
                "issues": [],
            }.items():
                expect(content.get(key), expected, f"candidate content audit.{key}", issues)
            expect(content.get("archive"), authority["archive"],
                   "candidate content audit.archive", issues)
            expect(
                content.get("requiredRetainedRuntimeAssets"), SHIPPING_RETAINED,
                "candidate content audit.requiredRetainedRuntimeAssets", issues,
            )
            legacy = content.get("forbiddenIdentityCategories", {}).get(
                "LEGACY_CHARACTER_FRAMEWORK", {}
            )
            expect(legacy, {
                "passed": True,
                "matchCount": 0,
                "matches": [],
                "manifestUfsDiagnosticMatchCount": 0,
                "manifestUfsDiagnosticMatches": [],
            }, "candidate content audit legacy framework category", issues)
            boundary = content.get("releaseBoundary", {})
            expect(boundary.get("boundedStagedContentAuditPassed"), True,
                   "candidate content audit technical pass", issues)
            expect(boundary.get("freshInstallGameplayAcceptancePerformed"), False,
                   "candidate content audit fresh-install claim", issues)
            expect(boundary.get("legalOrVisualApprovalPerformed"), False,
                   "candidate content audit legal/visual claim", issues)
            expect(boundary.get("releaseReady"), False,
                   "candidate content audit release claim", issues)

        if type(binary) is dict:
            for key, expected in {
                "schema": "DiscGolfTour.Session19ShippingBinaryEvidence.v1",
                "schemaVersion": 1,
                "session": 19,
                "state": "PASS_BINARY_MARKER_POLICY_ONLY",
                "passed": True,
                "errors": [],
                "resolvedBlockers": [],
                "releaseReady": False,
            }.items():
                expect(binary.get(key), expected, f"Shipping binary audit.{key}", issues)
            expect(binary.get("input", {}).get("bytes"),
                   executable_authority["bytes"],
                   "Shipping binary audit executable bytes", issues)
            expect(str(binary.get("input", {}).get("sha256", "")).upper(),
                   executable_authority["sha256"],
                   "Shipping binary audit executable sha256", issues)
            expect(binary.get("scan", {}).get("forbiddenMatchCount"), 0,
                   "Shipping binary audit forbidden count", issues)
            try:
                from validate_dg_session19_shipping_binary import (
                    POLICY_PATH as SHIPPING_BINARY_POLICY_PATH,
                    _load_policy_at,
                )
                approved_binary_policy, approved_binary_policy_sha = _load_policy_at(
                    SHIPPING_BINARY_POLICY_PATH
                )
            except Exception as exc:
                issues.append(
                    "independent Shipping binary policy load failed closed: "
                    f"{exc.__class__.__name__}: {exc}"
                )
            else:
                validate_shipping_binary_required_markers(
                    binary,
                    approved_binary_policy,
                    approved_binary_policy_sha,
                    issues,
                )

        if (
            live_reaudit
            and resolved_archive is not None
            and resolved_executable is not None
            and resolved_unrealpak is not None
            and verification_path is not None
            and binary_path is not None
            and type(content) is dict
            and type(binary) is dict
        ):
            try:
                from validate_dg_session19_candidate_content import (
                    audit as audit_candidate_content,
                    find_unrealpak,
                )
                from validate_dg_session19_shipping_binary import (
                    POLICY_PATH as SHIPPING_BINARY_POLICY_PATH,
                    _load_policy_at,
                    validate_executable,
                )
                live_content, content_passed = audit_candidate_content(
                    resolved_archive,
                    expected_candidate_id,
                    find_unrealpak(resolved_unrealpak),
                    verification_path,
                    binary_path,
                )
                binary_policy, binary_policy_sha = _load_policy_at(
                    SHIPPING_BINARY_POLICY_PATH
                )
                live_binary = validate_executable(
                    resolved_executable, binary_policy, binary_policy_sha
                )
            except Exception as exc:
                issues.append(
                    "live candidate re-audit failed closed: "
                    f"{exc.__class__.__name__}: {exc}"
                )
            else:
                if not content_passed:
                    issues.append("live candidate content re-audit did not pass")
                if _without_volatile(live_content, "generatedUtc") != _without_volatile(
                    content, "generatedUtc"
                ):
                    issues.append("live candidate content re-audit differs from bound receipt")
                if not live_binary.get("passed"):
                    issues.append("live Shipping binary re-audit did not pass")
                if _without_volatile(live_binary, "checkedUtc") != _without_volatile(
                    binary, "checkedUtc"
                ):
                    issues.append("live Shipping binary re-audit differs from bound receipt")
    finally:
        ROOT = old_root
    return issues, summary


Mutation = tuple[str, Callable[[dict[str, Any]], None]]


def run_self_test(receipt: dict[str, Any], root: Path) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    base, _ = validate(receipt, root, check_files=True)
    if base:
        failures.append("baseline failed: " + "; ".join(base[:5]))
    mutations: list[Mutation] = [
        ("extra", lambda d: d.__setitem__("releaseApproved", True)),
        ("schema", lambda d: d.__setitem__("schema", "wrong")),
        ("version-bool", lambda d: d.__setitem__("schemaVersion", True)),
        ("authority", lambda d: d.__setitem__("authority", "LEGAL_APPROVAL")),
        ("policy-hash", lambda d: d["policy"].__setitem__("sha256", "g" * 64)),
        ("inventory-count", lambda d: d["boundary"].__setitem__("excludedAssetCount", 38)),
        ("legacy-enabled", lambda d: d["boundary"].__setitem__("legacyPluginEnabledByDefault", True)),
        ("retained-missing", lambda d: d["retainedAssets"].pop()),
        ("retained-hash", lambda d: d["retainedAssets"][0].__setitem__("sha256", "g" * 64)),
        ("excluded-mutated", lambda d: d["excludedEvidence"].__setitem__("allPreMigrationIdentitiesPreserved", False)),
        ("recreation-log", lambda d: d["workflow"]["recreationRun"].__setitem__("path", "../escape.log")),
        ("clean-log", lambda d: d["workflow"]["finalCleanLoadRun"].__setitem__("sha256", "g" * 64)),
        ("lfs", lambda d: d["acceptance"].__setitem__("gitLfsTracksUassetAndUmap", False)),
        ("shipping-accepted", lambda d: d["closure"].__setitem__("freshShippingPackageEvidenceAccepted", True)),
        ("blocker-closed", lambda d: d["closure"].__setitem__("blockerClosed", True)),
        ("count-decrement", lambda d: d["closure"].__setitem__("remainingReleaseBlockerCount", 12)),
        ("release-ready", lambda d: d["closure"].__setitem__("releaseReady", True)),
    ]
    for name, mutation in mutations:
        candidate = copy.deepcopy(receipt)
        mutation(candidate)
        issues, _ = validate(candidate, root, check_files=False)
        if not issues:
            failures.append(f"mutation survived: {name}")
    return not failures, len(mutations), failures


def run_shipping_binary_marker_self_test() -> tuple[bool, int, list[str]]:
    """Exercise policy reconciliation, including adversarial row drift."""
    failures: list[str] = []
    checks = 0
    try:
        from validate_dg_session19_shipping_binary import (
            POLICY_PATH as SHIPPING_BINARY_POLICY_PATH,
            _load_policy_at,
        )
        policy, policy_sha = _load_policy_at(SHIPPING_BINARY_POLICY_PATH)
    except Exception as exc:
        return False, 1, [
            "binary-marker policy fixture load failed: "
            f"{exc.__class__.__name__}: {exc}"
        ]

    def current_receipt() -> dict[str, Any]:
        return {
            "policyId": policy["policyId"],
            "policySha256": policy_sha,
            "scan": {
                "required": [
                    {
                        "id": marker["id"],
                        "foundAscii": (
                            marker["id"]
                            == SHIPPING_BINARY_FOUNDATION_MARKER["id"]
                        ),
                        "foundUtf16Le": True,
                    }
                    for marker in policy["requiredMarkers"]
                ]
            },
        }

    def validate_case(
        name: str,
        *,
        receipt_mutation: Callable[[dict[str, Any]], None] | None = None,
        policy_mutation: Callable[[dict[str, Any]], None] | None = None,
        should_pass: bool = False,
        legacy: bool = False,
    ) -> None:
        nonlocal checks
        checks += 1
        candidate_receipt = current_receipt()
        candidate_policy = copy.deepcopy(policy)
        if legacy:
            candidate_receipt["policySha256"] = (
                LEGACY_FOUNDATION_ONLY_BINARY_POLICY_SHA256
            )
            candidate_receipt["scan"]["required"] = [
                {
                    "id": SHIPPING_BINARY_FOUNDATION_MARKER["id"],
                    "foundAscii": True,
                    "foundUtf16Le": True,
                }
            ]
        if receipt_mutation is not None:
            receipt_mutation(candidate_receipt)
        if policy_mutation is not None:
            policy_mutation(candidate_policy)
        case_issues: list[str] = []
        validate_shipping_binary_required_markers(
            candidate_receipt, candidate_policy, policy_sha, case_issues,
        )
        if should_pass and case_issues:
            failures.append(f"binary-marker {name} failed: {'; '.join(case_issues)}")
        if not should_pass and not case_issues:
            failures.append(f"binary-marker mutation survived: {name}")

    validate_case("current-policy-baseline", should_pass=True)
    validate_case("frozen-foundation-only-baseline", should_pass=True, legacy=True)
    validate_case(
        "unapproved-extra",
        receipt_mutation=lambda value: value["scan"]["required"].append({
            "id": "unapproved_marker",
            "foundAscii": True,
            "foundUtf16Le": False,
        }),
    )
    validate_case(
        "duplicate-foundation",
        receipt_mutation=lambda value: value["scan"]["required"].append(
            copy.deepcopy(value["scan"]["required"][0])
        ),
    )
    validate_case(
        "duplicate-approved-extra",
        receipt_mutation=lambda value: value["scan"]["required"].append(
            copy.deepcopy(value["scan"]["required"][1])
        ),
    )
    validate_case(
        "missing-foundation",
        receipt_mutation=lambda value: value["scan"]["required"].pop(0),
    )
    validate_case(
        "missing-current-policy-extra",
        receipt_mutation=lambda value: value["scan"]["required"].pop(),
    )
    validate_case(
        "foundation-current-ascii-only",
        receipt_mutation=lambda value: value["scan"]["required"][0].__setitem__(
            "foundUtf16Le", False
        ),
        should_pass=True,
    )
    validate_case(
        "foundation-current-utf16-only",
        receipt_mutation=lambda value: value["scan"]["required"][0].__setitem__(
            "foundAscii", False
        ),
        should_pass=True,
    )
    validate_case(
        "foundation-current-no-encoding",
        receipt_mutation=lambda value: value["scan"]["required"][0].update({
            "foundAscii": False,
            "foundUtf16Le": False,
        }),
    )
    validate_case(
        "foundation-frozen-ascii-only",
        legacy=True,
        receipt_mutation=lambda value: value["scan"]["required"][0].__setitem__(
            "foundUtf16Le", False
        ),
        should_pass=True,
    )
    validate_case(
        "foundation-frozen-utf16-only",
        legacy=True,
        receipt_mutation=lambda value: value["scan"]["required"][0].__setitem__(
            "foundAscii", False
        ),
        should_pass=True,
    )
    validate_case(
        "foundation-frozen-no-encoding",
        legacy=True,
        receipt_mutation=lambda value: value["scan"]["required"][0].update({
            "foundAscii": False,
            "foundUtf16Le": False,
        }),
    )
    validate_case(
        "extra-missing-all-encodings",
        receipt_mutation=lambda value: value["scan"]["required"][1].update({
            "foundAscii": False,
            "foundUtf16Le": False,
        }),
    )
    validate_case(
        "receipt-id-type-drift",
        receipt_mutation=lambda value: value["scan"]["required"][1].__setitem__(
            "id", 7
        ),
    )
    validate_case(
        "receipt-flag-type-drift",
        receipt_mutation=lambda value: value["scan"]["required"][1].__setitem__(
            "foundAscii", 0
        ),
    )
    validate_case(
        "receipt-row-extra-key",
        receipt_mutation=lambda value: value["scan"]["required"][1].__setitem__(
            "value", "forged"
        ),
    )
    validate_case(
        "receipt-required-not-array",
        receipt_mutation=lambda value: value["scan"].__setitem__("required", {}),
    )
    validate_case(
        "extra-policy-hash-drift",
        receipt_mutation=lambda value: value.__setitem__("policySha256", "0" * 64),
    )
    validate_case(
        "receipt-policy-id-drift",
        receipt_mutation=lambda value: value.__setitem__("policyId", "other"),
    )
    validate_case(
        "foundation-only-unapproved-policy",
        legacy=True,
        receipt_mutation=lambda value: value.__setitem__("policySha256", "0" * 64),
    )
    validate_case(
        "approved-foundation-value-drift",
        policy_mutation=lambda value: value["requiredMarkers"][0].__setitem__(
            "value", "OtherRuntime"
        ),
    )
    validate_case(
        "approved-policy-duplicate-id",
        policy_mutation=lambda value: value["requiredMarkers"].append(
            copy.deepcopy(value["requiredMarkers"][1])
        ),
    )
    validate_case(
        "approved-policy-type-drift",
        policy_mutation=lambda value: value["requiredMarkers"][1].__setitem__(
            "requireAnyEncoding", 1
        ),
    )
    validate_case(
        "approved-policy-row-extra-key",
        policy_mutation=lambda value: value["requiredMarkers"][1].__setitem__(
            "unexpected", True
        ),
    )
    for name, value in [
        ("bound-root-array", []),
        ("bound-root-null", None),
        ("bound-root-string", "PASS_BINARY_MARKER_POLICY_ONLY"),
        ("bound-root-integer", 1),
        ("bound-root-boolean", True),
    ]:
        checks += 1
        root_issues: list[str] = []
        accepted = require_bound_receipt_object(
            value, "Shipping binary audit", root_issues,
        )
        if accepted is not None or root_issues != [
            "bound Shipping binary audit root must be an object"
        ]:
            failures.append(f"binary-marker mutation survived: {name}")
    checks += 1
    object_issues: list[str] = []
    object_value = {"schema": "synthetic"}
    if require_bound_receipt_object(
        object_value, "Shipping binary audit", object_issues,
    ) is not object_value or object_issues:
        failures.append("binary-marker valid object root was rejected")
    return not failures, checks, failures


def run_shipping_self_test(
    receipt: dict[str, Any],
    root: Path,
    *,
    candidate_authority: dict[str, Any] | None = None,
    candidate_id: str,
    archive: Path,
    executable: Path,
    unrealpak: Path,
    external_run: Path,
) -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    baseline, _ = validate_shipping_evidence(
        receipt,
        root,
        candidate_authority=candidate_authority,
        candidate_id=candidate_id,
        archive=archive,
        executable=executable,
        unrealpak=unrealpak,
        external_run=external_run,
        check_files=True,
        live_reaudit=True,
    )
    if baseline:
        failures.append("Shipping baseline failed: " + "; ".join(baseline[:5]))
    mutations: list[Mutation] = [
        ("shipping-extra", lambda d: d.__setitem__("releaseApproved", True)),
        ("shipping-schema", lambda d: d.__setitem__("schema", "wrong")),
        ("shipping-version-bool", lambda d: d.__setitem__("schemaVersion", True)),
        ("shipping-candidate", lambda d: d.__setitem__("candidateId", "S19_WindowsShipping_bad")),
        ("shipping-authority", lambda d: d.__setitem__("authority", "LEGAL_APPROVAL")),
        ("shipping-state", lambda d: d.__setitem__("state", "RELEASE_READY")),
        ("shipping-source-path", lambda d: d["bindings"]["sourceMigrationReceipt"].__setitem__("path", "Evidence/Session19/ShippingBinaryPolicyReceipt.json")),
        ("shipping-content-hash", lambda d: d["bindings"]["candidateContentAudit"].__setitem__("sha256", "g" * 64)),
        ("shipping-external-hash", lambda d: d["bindings"]["externalTechnicalValidation"].__setitem__("sha256", "g" * 64)),
        ("shipping-fresh-userdir-hash", lambda d: d["bindings"]["freshUserDirValidation"].__setitem__("sha256", "h" * 64)),
        ("shipping-host-path", lambda d: d["archive"].__setitem__("hostPathRecorded", True)),
        ("shipping-archive-token", lambda d: d["archive"].__setitem__("recoveryLocationToken", "C:/host/path")),
        ("shipping-archive-bytes", lambda d: d["archive"].__setitem__("bytes", d["archive"]["bytes"] - 1)),
        ("shipping-archive-hash", lambda d: d["archive"].__setitem__("canonicalManifestSha256", "0" * 64)),
        ("shipping-retained-missing", lambda d: d["retainedRuntimeAssets"].pop()),
        ("shipping-retained-package", lambda d: d["retainedRuntimeAssets"][0].__setitem__("package", "/Game/Wrong")),
        ("shipping-retained-ufs", lambda d: d["retainedRuntimeAssets"][0].__setitem__("presentInUfsManifest", False)),
        ("shipping-retained-container", lambda d: d["retainedRuntimeAssets"][0].__setitem__("presentInContainerInventory", False)),
        ("shipping-screenshot-missing", lambda d: d["runtimeScreenshots"].pop()),
        ("shipping-screenshot-path", lambda d: d["runtimeScreenshots"][0].__setitem__("path", "Evidence/Session19/Diagnostics/other.jpg")),
        ("shipping-screenshot-hash", lambda d: d["runtimeScreenshots"][0].__setitem__("sha256", "0" * 64)),
        ("shipping-screenshot-width", lambda d: d["runtimeScreenshots"][0].__setitem__("width", 2559)),
        ("shipping-screenshot-technical", lambda d: d["runtimeScreenshots"][0].__setitem__("technicalOnly", False)),
        ("shipping-screenshot-fresh-install", lambda d: d["runtimeScreenshots"][0].__setitem__("freshInstallEvidence", True)),
        ("shipping-screenshot-human", lambda d: d["runtimeScreenshots"][0].__setitem__("humanReviewed", True)),
        ("shipping-legacy-archive-match", lambda d: d["nonDistributionBoundary"].__setitem__("archiveAuthoritativeMatchCount", 1)),
        ("shipping-legacy-diagnostic-match", lambda d: d["nonDistributionBoundary"].__setitem__("archiveManifestDiagnosticMatchCount", 1)),
        ("shipping-legacy-binary-match", lambda d: d["nonDistributionBoundary"].__setitem__("shippingExecutableForbiddenMatchCount", 1)),
        ("shipping-replacement-marker", lambda d: d["nonDistributionBoundary"].__setitem__("shippingExecutableReplacementMarkerFound", False)),
        ("shipping-non-distribution", lambda d: d["nonDistributionBoundary"].__setitem__("legacyFrameworkNonDistributionVerified", False)),
        ("shipping-technical-closure", lambda d: d["nonDistributionBoundary"].__setitem__("technicalMigrationShippingBoundaryClosed", False)),
        ("shipping-package-accepted", lambda d: d["claimBoundary"].__setitem__("freshShippingPackageEvidenceAccepted", False)),
        ("shipping-fresh-install-overclaim", lambda d: d["claimBoundary"].__setitem__("freshInstallGameplayAcceptance", True)),
        ("shipping-manual-overclaim", lambda d: d["claimBoundary"].__setitem__("manualGameplayAcceptance", True)),
        ("shipping-human-overclaim", lambda d: d["claimBoundary"].__setitem__("humanPlayFeelApproval", True)),
        ("shipping-visual-overclaim", lambda d: d["claimBoundary"].__setitem__("visualProductApproval", True)),
        ("shipping-accessibility-overclaim", lambda d: d["claimBoundary"].__setitem__("accessibilityApproval", True)),
        ("shipping-rights-overclaim", lambda d: d["claimBoundary"].__setitem__("sourceDistributionRightsResolved", True)),
        ("shipping-clearance-overclaim", lambda d: d["claimBoundary"].__setitem__("distributionClearance", True)),
        ("shipping-legal-overclaim", lambda d: d["claimBoundary"].__setitem__("legalApproval", True)),
        ("shipping-blocker-closed", lambda d: d["claimBoundary"].__setitem__("blockerClosed", True)),
        ("shipping-release-approval", lambda d: d["claimBoundary"].__setitem__("releaseApproval", True)),
        ("shipping-release-ready", lambda d: d["claimBoundary"].__setitem__("releaseReady", True)),
    ]
    for name, mutation in mutations:
        candidate = copy.deepcopy(receipt)
        mutation(candidate)
        issues, _ = validate_shipping_evidence(
            candidate, root, candidate_authority=candidate_authority,
            check_files=False, live_reaudit=False
        )
        if not issues:
            failures.append(f"Shipping mutation survived: {name}")
    return not failures, len(mutations), failures


def run_candidate_policy_self_test(
    historical_receipt: dict[str, Any], root: Path,
) -> tuple[bool, int, list[str]]:
    """Prove an alternate candidate can be selected without historical constants."""
    failures: list[str] = []
    checks = 0
    candidate_id = "S19_WindowsShipping_20990101T000000Z_0123456789ab"
    token = "01234567-89ab-4cde-8f01-23456789abcd"
    policy_path = (
        "Config/DG_Session19SerializedAssetMigrationCandidatePolicy-"
        f"{candidate_id}.json"
    )
    policy_binding = {
        "path": policy_path,
        "bytes": 4096,
        "sha256": "A" * 64,
    }
    policy = {
        "schema": CANDIDATE_POLICY_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "policyId": f"session19_serialized_asset_migration_candidate_{candidate_id}",
        "authority": CANDIDATE_POLICY_AUTHORITY,
        "state": "CANDIDATE_INPUTS_BOUND_SUPPLEMENT_RECEIPT_REQUIRED",
        "releaseReady": False,
        "shippingEvidenceId": (
            "session19_retained_serialized_asset_migration_shipping_"
            f"non_distribution_{candidate_id}"
        ),
        "shippingReceiptPath": (
            "Evidence/Session19/SerializedAssetMigrationShipping-"
            f"{candidate_id}.json"
        ),
        "bindings": _candidate_binding_paths(candidate_id),
        "archive": {
            **copy.deepcopy(SHIPPING_ARCHIVE),
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
        },
        "executable": {
            "relativePath": SHIPPING_EXE_RELATIVE.as_posix(),
            "bytes": SHIPPING_EXE_BYTES,
            "sha256": SHIPPING_EXE_SHA256,
        },
        "externalEvidence": {
            "userDirToken": token,
            "runDirectoryLeaf": f"{candidate_id}_{token}",
            "manifestSha256": "B" * 64,
            "externalTechnicalValidationState": (
                "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
            ),
            "freshUserDirValidationState": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        },
        "runtimeScreenshots": copy.deepcopy(SHIPPING_SCREENSHOTS),
    }

    policy_issues, authority = validate_candidate_policy(
        policy, root, policy_binding=policy_binding, check_files=False,
    )
    checks += 1
    if policy_issues or authority is None:
        failures.append("alternate candidate policy baseline failed: " + "; ".join(policy_issues))
        return False, checks, failures
    if (
        authority["candidateId"] != candidate_id
        or authority["candidateId"] == SHIPPING_CANDIDATE_ID
        or authority["externalEvidence"]["userDirToken"] != token
        or authority["externalManifestSchemaVersion"] != 1
        or candidate_id not in authority["shippingReceiptPath"]
    ):
        failures.append("alternate candidate policy did not control normalized authority")
    checks += 1

    runtime_journal = {
        "bytes": 3228,
        "captureNonce": "12345678-1234-4abc-8def-1234567890ab",
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
        "modifiedUtc": "2099-01-01T00:00:01.000Z",
        "modifiedWithinRunWindow": True,
        "roundId": "87654321-4321-4abc-8def-ba0987654321",
        "schema": RUNTIME_JOURNAL_SCHEMA,
        "sha256": "D" * 64,
        "userDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
        "validationFailures": [],
        "validationState": RUNTIME_JOURNAL_PASS,
    }
    v2_policy = copy.deepcopy(policy)
    v2_policy["externalEvidence"].update({
        "manifestSchema": EXTERNAL_MANIFEST_SCHEMA_V2,
        "manifestSchemaVersion": 2,
        "candidateEvidenceBindingSha256": "C" * 64,
        "runtimeCheckpointJournal": copy.deepcopy(runtime_journal),
    })
    v2_issues, v2_authority = validate_candidate_policy(
        v2_policy, root, policy_binding=policy_binding, check_files=False,
    )
    checks += 1
    if (
        v2_issues
        or v2_authority is None
        or v2_authority["externalManifestSchemaVersion"] != 2
    ):
        failures.append(
            "journal-bound v2 policy baseline failed: " + "; ".join(v2_issues)
        )

    def v2_policy_mutation_rejected(
        mutation: Callable[[dict[str, Any]], None],
    ) -> bool:
        candidate = copy.deepcopy(v2_policy)
        mutation(candidate)
        mutation_issues, _ = validate_candidate_policy(
            candidate, root, policy_binding=policy_binding, check_files=False,
        )
        return bool(mutation_issues)

    for name, mutation in [
        (
            "v2-journal-missing",
            lambda d: d["externalEvidence"].pop("runtimeCheckpointJournal"),
        ),
        (
            "v2-schema-downgrade",
            lambda d: d["externalEvidence"].__setitem__("manifestSchemaVersion", 1),
        ),
        (
            "v2-journal-state",
            lambda d: d["externalEvidence"]["runtimeCheckpointJournal"].__setitem__(
                "validationState", "PASS"
            ),
        ),
        (
            "v2-journal-count",
            lambda d: d["externalEvidence"]["runtimeCheckpointJournal"].__setitem__(
                "eventCount", 11
            ),
        ),
    ]:
        checks += 1
        if not v2_policy_mutation_rejected(mutation):
            failures.append(f"journal-bound v2 policy mutation survived: {name}")

    live_manifest = {
        "schema": EXTERNAL_MANIFEST_SCHEMA_V2,
        "schemaVersion": 2,
        "candidateEvidenceBindingSha256": "C" * 64,
        "evidence": {"runtimeCheckpointJournal": copy.deepcopy(runtime_journal)},
    }

    def v2_manifest_mutation_rejected(
        mutation: Callable[[dict[str, Any]], None],
    ) -> bool:
        candidate = copy.deepcopy(live_manifest)
        mutation(candidate)
        mutation_issues: list[str] = []
        validate_external_manifest_authority(
            v2_policy["externalEvidence"],
            candidate,
            "synthetic external manifest",
            mutation_issues,
        )
        return bool(mutation_issues)

    baseline_manifest_issues: list[str] = []
    validate_external_manifest_authority(
        v2_policy["externalEvidence"],
        live_manifest,
        "synthetic external manifest",
        baseline_manifest_issues,
    )
    checks += 1
    if baseline_manifest_issues:
        failures.append(
            "journal-bound v2 live-manifest baseline failed: "
            + "; ".join(baseline_manifest_issues)
        )
    for name, mutation in [
        (
            "v2-live-journal",
            lambda d: d["evidence"]["runtimeCheckpointJournal"].__setitem__(
                "sha256", "0" * 64
            ),
        ),
        (
            "v2-live-downgrade",
            lambda d: (
                d.__setitem__("schema", EXTERNAL_MANIFEST_SCHEMA_V1),
                d.__setitem__("schemaVersion", 1),
                d["evidence"].pop("runtimeCheckpointJournal"),
            ),
        ),
        (
            "v2-live-candidate-binding",
            lambda d: d.__setitem__("candidateEvidenceBindingSha256", "0" * 64),
        ),
    ]:
        checks += 1
        if not v2_manifest_mutation_rejected(mutation):
            failures.append(f"journal-bound v2 manifest mutation survived: {name}")

    supplement = copy.deepcopy(historical_receipt)
    supplement["schema"] = authority["receiptSchema"]
    supplement["schemaVersion"] = authority["receiptSchemaVersion"]
    supplement["candidateId"] = candidate_id
    supplement["candidatePolicy"] = copy.deepcopy(policy_binding)
    supplement["evidenceId"] = authority["evidenceId"]
    supplement["archive"] = copy.deepcopy(authority["archive"])
    supplement["runtimeScreenshots"] = copy.deepcopy(authority["runtimeScreenshots"])
    for key, path in authority["bindingPaths"].items():
        supplement["bindings"][key]["path"] = path
    supplement_issues, _ = validate_shipping_evidence(
        supplement, root, candidate_authority=authority,
        check_files=False, live_reaudit=False,
    )
    checks += 1
    if supplement_issues:
        failures.append(
            "alternate candidate supplement baseline failed: "
            + "; ".join(supplement_issues[:5])
        )

    mutations: list[Mutation] = [
        ("policy-candidate", lambda d: d.__setitem__("candidateId", SHIPPING_CANDIDATE_ID)),
        ("policy-id", lambda d: d.__setitem__("policyId", "historical")),
        ("policy-receipt-traversal", lambda d: d.__setitem__("shippingReceiptPath", "../receipt.json")),
        ("policy-binding-old-candidate", lambda d: d["bindings"].__setitem__("candidateVerification", SHIPPING_BINDING_PATHS["candidateVerification"])),
        ("policy-archive-old-candidate", lambda d: d["archive"].__setitem__("recoveryLocationToken", SHIPPING_ARCHIVE["recoveryLocationToken"])),
        ("policy-exe-traversal", lambda d: d["executable"].__setitem__("relativePath", "../Shipping.exe")),
        ("policy-token", lambda d: d["externalEvidence"].__setitem__("userDirToken", "not-a-token")),
        ("policy-run-leaf", lambda d: d["externalEvidence"].__setitem__("runDirectoryLeaf", "other")),
        ("policy-screenshot-traversal", lambda d: d["runtimeScreenshots"][0].__setitem__("path", "../shot.png")),
        ("policy-release-ready", lambda d: d.__setitem__("releaseReady", True)),
    ]
    for name, mutation in mutations:
        candidate = copy.deepcopy(policy)
        mutation(candidate)
        mutation_issues, _ = validate_candidate_policy(
            candidate, root, policy_binding=policy_binding, check_files=False,
        )
        checks += 1
        if not mutation_issues:
            failures.append(f"alternate policy mutation survived: {name}")

    wrong_binding = copy.deepcopy(policy_binding)
    wrong_binding["path"] = "Config/DG_Session19SerializedAssetMigrationCandidatePolicy-other.json"
    binding_issues, _ = validate_candidate_policy(
        policy, root, policy_binding=wrong_binding, check_files=False,
    )
    checks += 1
    if not binding_issues:
        failures.append("alternate policy filename mismatch survived")

    mutated_supplement = copy.deepcopy(supplement)
    mutated_supplement["candidatePolicy"]["sha256"] = "C" * 64
    supplement_issues, _ = validate_shipping_evidence(
        mutated_supplement, root, candidate_authority=authority,
        check_files=False, live_reaudit=False,
    )
    checks += 1
    if not supplement_issues:
        failures.append("alternate supplement policy-binding mutation survived")
    return not failures, checks, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--receipt", type=Path, default=RECEIPT_PATH)
    parser.add_argument(
        "--candidate-policy", type=Path,
        help=(
            "Explicit candidate-scoped Config policy. New candidates require "
            "this; omission retains only the frozen historical supplement mode."
        ),
    )
    parser.add_argument(
        "--shipping-receipt", type=Path,
        help=(
            "Existing immutable supplement receipt input. The validator never "
            "creates or overwrites this file."
        ),
    )
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--external-run", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-release-ready", "--require-release", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    receipt_path, receipt_path_issues = resolve_cli_project_file(
        root, args.receipt, "source migration receipt", must_exist=True,
    )
    if receipt_path_issues or receipt_path is None:
        print("SESSION 19 SERIALIZED ASSET MIGRATION INVALID")
        for issue in receipt_path_issues:
            print(f" - {issue}")
        return 1
    try:
        receipt = load_json(receipt_path)
    except StrictJsonError as exc:
        print(f"SESSION 19 SERIALIZED ASSET MIGRATION INVALID: {exc}")
        return 1
    if type(receipt) is not dict:
        print("SESSION 19 SERIALIZED ASSET MIGRATION INVALID: receipt root must be an object")
        return 1
    candidate_mode = any(value is not None for value in [
        args.candidate_policy, args.shipping_receipt, args.candidate_id,
        args.archive, args.exe, args.unrealpak, args.external_run,
    ])
    if candidate_mode:
        selected_policy_binding: dict[str, Any] | None = None
        if args.candidate_policy is not None:
            policy_issues, candidate_authority, selected_policy_binding = (
                load_candidate_policy(
                    args.candidate_policy, root, check_files=True,
                )
            )
            if policy_issues or candidate_authority is None:
                print("SESSION 19 SERIALIZED ASSET MIGRATION CANDIDATE POLICY INVALID")
                for issue in policy_issues:
                    print(f" - {issue}")
                return 1
            selected_candidate_id = candidate_authority["candidateId"]
            if (
                args.candidate_id is not None
                and args.candidate_id != selected_candidate_id
            ):
                parser.error("--candidate-id differs from --candidate-policy")
            missing = [
                name for name, value in [
                    ("--archive", args.archive),
                    ("--unrealpak", args.unrealpak),
                    ("--external-run", args.external_run),
                ] if value is None
            ]
        else:
            candidate_authority = historical_shipping_authority()
            selected_candidate_id = args.candidate_id
            if (
                selected_candidate_id is not None
                and selected_candidate_id != candidate_authority["candidateId"]
            ):
                parser.error(
                    "new candidates require --candidate-policy; the policy-less "
                    "supplement mode is frozen to the historical candidate"
                )
            missing = [
                name for name, value in [
                    ("--candidate-id", args.candidate_id),
                    ("--archive", args.archive),
                    ("--exe", args.exe),
                    ("--unrealpak", args.unrealpak),
                    ("--external-run", args.external_run),
                ] if value is None
            ]
        if missing:
            parser.error(
                "candidate Shipping validation requires explicit " + ", ".join(missing)
            )
        for name, value in [
            ("--archive", args.archive), ("--exe", args.exe),
            ("--unrealpak", args.unrealpak), ("--external-run", args.external_run),
        ]:
            if value is not None and not value.is_absolute():
                parser.error(f"{name} must be an absolute path")
        selected_executable = args.exe
        if selected_executable is None and args.archive is not None:
            selected_executable = (
                args.archive / Path(candidate_authority["executable"]["relativePath"])
            )

        expected_shipping_path, expected_shipping_issues = resolve_cli_project_file(
            root,
            Path(candidate_authority["shippingReceiptPath"]),
            "candidate Shipping supplement receipt",
            must_exist=True,
        )
        if expected_shipping_issues or expected_shipping_path is None:
            print("SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID")
            for issue in expected_shipping_issues:
                print(f" - {issue}")
            return 1
        if args.shipping_receipt is None:
            shipping_path = expected_shipping_path
        else:
            shipping_path, shipping_path_issues = resolve_cli_project_file(
                root, args.shipping_receipt,
                "--shipping-receipt", must_exist=True,
            )
            if shipping_path_issues or shipping_path is None:
                print("SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID")
                for issue in shipping_path_issues:
                    print(f" - {issue}")
                return 1
            if shipping_path != expected_shipping_path:
                print(
                    "SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID: "
                    "--shipping-receipt differs from the selected candidate authority"
                )
                return 1
        try:
            shipping_receipt = load_json(shipping_path)
        except StrictJsonError as exc:
            print(f"SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID: {exc}")
            return 1
        if type(shipping_receipt) is not dict:
            print(
                "SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID: "
                "receipt root must be an object"
            )
            return 1
        if args.self_test:
            source_passed, source_count, source_failures = run_self_test(receipt, root)
            shipping_passed, shipping_count, shipping_failures = run_shipping_self_test(
                shipping_receipt,
                root,
                candidate_authority=candidate_authority,
                candidate_id=selected_candidate_id,
                archive=args.archive,
                executable=selected_executable,
                unrealpak=args.unrealpak,
                external_run=args.external_run,
            )
            policy_passed, policy_count, policy_failures = (
                run_candidate_policy_self_test(shipping_receipt, root)
            )
            marker_passed, marker_count, marker_failures = (
                run_shipping_binary_marker_self_test()
            )
            failures = (
                source_failures + shipping_failures + policy_failures
                + marker_failures
            )
            if (
                not source_passed or not shipping_passed or not policy_passed
                or not marker_passed
            ):
                print("SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING SELF-TEST FAILED")
                for failure in failures:
                    print(f" - {failure}")
                return 1
            count = source_count + shipping_count + policy_count + marker_count
            print(
                "SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING SELF-TEST PASS "
                f"checks={count} source={source_count} shipping={shipping_count} "
                f"selectable_policy={policy_count} binary_markers={marker_count}"
            )
            return 0
        source_issues, source_summary = validate(receipt, root, check_files=True)
        shipping_issues, shipping_summary = validate_shipping_evidence(
            shipping_receipt,
            root,
            candidate_authority=candidate_authority,
            candidate_id=selected_candidate_id,
            archive=args.archive,
            executable=selected_executable,
            unrealpak=args.unrealpak,
            external_run=args.external_run,
            check_files=True,
            live_reaudit=True,
        )
        issues = [
            *(f"source migration: {issue}" for issue in source_issues),
            *(f"Shipping supplement: {issue}" for issue in shipping_issues),
        ]
        if issues:
            print("SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING INVALID")
            for issue in issues:
                print(f" - {issue}")
            return 1
        if args.require_release_ready:
            print(
                "SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING RELEASE BLOCKED: "
                "technical non-distribution is verified, but source-distribution rights, "
                "legal/distribution approval, fresh-install/manual acceptance, and release "
                "approval remain false"
            )
            return 2
        policy_suffix = (
            "" if selected_policy_binding is None else
            f"policy={selected_policy_binding['path']} "
            f"policy_sha256={selected_policy_binding['sha256']} "
        )
        print(
            "SESSION 19 SERIALIZED ASSET MIGRATION SHIPPING PASS "
            f"candidate={selected_candidate_id} "
            f"authority_mode={candidate_authority['mode']} "
            f"{policy_suffix}"
            f"retained={shipping_summary['retained']} "
            f"screenshots={shipping_summary['screenshots']} "
            f"legacy_archive_matches={shipping_summary['legacyArchiveMatches']} "
            f"excluded={source_summary['excluded']} "
            "technical_non_distribution=verified fresh_install=false "
            "legal=false distribution_clearance=false release_ready=false"
        )
        return 0
    if args.self_test:
        passed, count, failures = run_self_test(receipt, root)
        try:
            historical_shipping_receipt = load_json(
                root / historical_shipping_authority()["shippingReceiptPath"]
            )
        except StrictJsonError as exc:
            policy_passed, policy_count, policy_failures = (
                False, 0, [f"historical supplement fixture invalid: {exc}"]
            )
        else:
            policy_passed, policy_count, policy_failures = (
                run_candidate_policy_self_test(historical_shipping_receipt, root)
            )
        marker_passed, marker_count, marker_failures = (
            run_shipping_binary_marker_self_test()
        )
        failures.extend(policy_failures)
        failures.extend(marker_failures)
        if not passed or not policy_passed or not marker_passed:
            print("SESSION 19 SERIALIZED ASSET MIGRATION SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(
            "SESSION 19 SERIALIZED ASSET MIGRATION SELF-TEST PASS "
            f"checks={count + policy_count + marker_count} source={count} "
            f"selectable_policy={policy_count} binary_markers={marker_count}"
        )
        return 0
    issues, summary = validate(receipt, root, check_files=True)
    if issues:
        print("SESSION 19 SERIALIZED ASSET MIGRATION INVALID")
        for issue in issues:
            print(f" - {issue}")
        return 1
    if args.require_release_ready:
        print(
            "SESSION 19 SERIALIZED ASSET MIGRATION RELEASE BLOCKED: "
            "fresh Windows Shipping package absence/runtime evidence is pending")
        return 2
    print(
        "SESSION 19 SERIALIZED ASSET MIGRATION PASS "
        f"retained={summary['retained']} excluded={summary['excluded']} "
        f"legacy_markers_excluded_only={summary['legacyMarkers']} "
        "shipping_package_evidence=pending blockers=13")
    return 0


if __name__ == "__main__":
    sys.exit(main())
