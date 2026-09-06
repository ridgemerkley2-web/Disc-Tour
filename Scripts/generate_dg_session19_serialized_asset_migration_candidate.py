#!/usr/bin/env python3
"""Append-only generator for Session 19 candidate migration evidence.

This command has two deliberately separate publication stages.  ``policy``
binds one fresh candidate to its already-created technical inputs.  Later,
``supplement`` performs the live archive/binary re-audits and publishes the
candidate's immutable ShippingEvidence.v2 receipt.  The companion validator is
read-only; neither command widens the evidence's non-distribution claim.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import tempfile
from typing import Any, Callable

import validate_dg_session19_serialized_asset_migration as migration
import validate_dg_session19_external_technical_evidence as external_evidence


ROOT = Path(__file__).resolve().parents[1]
SCREENSHOT_MILESTONES = [
    ("HOLE_2_COMPLETE_CHARACTER_RENDERED", "Screenshots/007_hole2_complete.png"),
    ("HOLE_3_TEE_CHARACTER_RENDERED", "Screenshots/008_hole3_tee.png"),
    (
        "HOLE_3_RECOVERED_LIE_CHARACTER_RENDERED",
        "Screenshots/009_hole3_recovered_lie.png",
    ),
]


class GenerationError(RuntimeError):
    """A fail-closed generation error that must not publish an output."""


def _json_bytes(value: Any) -> bytes:
    try:
        text = json.dumps(
            value,
            ensure_ascii=False,
            indent=2,
            allow_nan=False,
        ) + "\n"
    except (TypeError, ValueError) as exc:
        raise GenerationError(f"receipt is not strict JSON: {exc}") from exc
    return text.encode("utf-8")


def _bytes_binding(relative_path: str, payload: bytes) -> dict[str, Any]:
    return {
        "path": relative_path,
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
    }


def _exclusive_publish(path: Path, payload: bytes) -> None:
    """Publish complete bytes atomically without an overwrite-capable open."""
    if migration._host_path_contains_reparse(path.parent):
        raise GenerationError(f"output parent contains a reparse point: {path.parent}")
    if not path.parent.is_dir():
        raise GenerationError(f"output parent is missing: {path.parent}")
    temporary_name: str | None = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=".dgtour-serialized-migration-",
            suffix=".tmp",
            dir=path.parent,
        )
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        temporary_path = Path(temporary_name)
        expected_sha256 = hashlib.sha256(payload).hexdigest().upper()
        if (
            temporary_path.stat().st_size != len(payload)
            or migration.sha256(temporary_path) != expected_sha256
        ):
            raise GenerationError("staged output identity verification failed")
        # Hard-link creation is exclusive and exposes only the fully written
        # inode.  It cannot truncate or replace an existing immutable receipt.
        os.link(temporary_name, path)
    except FileExistsError as exc:
        raise GenerationError(f"immutable output already exists: {path}") from exc
    except OSError as exc:
        raise GenerationError(f"exclusive publication failed for {path}: {exc}") from exc
    finally:
        if temporary_name is not None:
            try:
                Path(temporary_name).unlink(missing_ok=True)
            except OSError:
                pass


def _publish_validated(
    path: Path,
    document: Any,
    issues: list[str],
) -> dict[str, Any]:
    if issues:
        raise GenerationError("validation failed: " + "; ".join(issues))
    payload = _json_bytes(document)
    _exclusive_publish(path, payload)
    return {
        "path": path,
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
    }


def _project_output(root: Path, token: str, label: str) -> Path:
    issues: list[str] = []
    path = migration._resolve_project_path(
        root, token, label, issues, must_exist=False,
    )
    if issues or path is None:
        raise GenerationError("; ".join(issues or [f"invalid {label}"]))
    if os.path.lexists(path):
        raise GenerationError(f"immutable output already exists: {token}")
    return path


def _project_binding(root: Path, token: str, label: str) -> dict[str, Any]:
    issues: list[str] = []
    path = migration._resolve_project_path(
        root, token, label, issues, must_exist=True,
    )
    if issues or path is None:
        raise GenerationError("; ".join(issues or [f"invalid {label}"]))
    return {
        "path": token,
        "bytes": path.stat().st_size,
        "sha256": migration.sha256(path),
    }


def _load_project_json(root: Path, token: str, label: str) -> dict[str, Any]:
    binding = _project_binding(root, token, label)
    path = root.joinpath(*PurePosixPath(binding["path"]).parts)
    try:
        value = migration.load_json(path)
    except migration.StrictJsonError as exc:
        raise GenerationError(f"{label} is invalid: {exc}") from exc
    if type(value) is not dict:
        raise GenerationError(f"{label} root must be an object")
    return value


def _absolute_host_path(
    value: Path,
    label: str,
    *,
    directory: bool,
) -> Path:
    if not value.is_absolute():
        raise GenerationError(f"{label} must be an absolute path")
    if ".." in value.parts:
        raise GenerationError(f"{label} must not contain parent traversal")
    if migration._host_path_contains_reparse(value):
        raise GenerationError(f"{label} contains a reparse point")
    try:
        path = value.resolve(strict=True)
    except OSError as exc:
        raise GenerationError(f"{label} is missing: {exc}") from exc
    if directory and not path.is_dir():
        raise GenerationError(f"{label} must be an existing directory")
    if not directory and not path.is_file():
        raise GenerationError(f"{label} must be an existing file")
    return path


def _policy_path(candidate_id: str) -> str:
    return (
        "Config/DG_Session19SerializedAssetMigrationCandidatePolicy-"
        f"{candidate_id}.json"
    )


def _shipping_path(candidate_id: str) -> str:
    return (
        "Evidence/Session19/SerializedAssetMigrationShipping-"
        f"{candidate_id}.json"
    )


def _external_manifest_policy_binding(
    manifest_path: Path,
    manifest: dict[str, Any],
) -> dict[str, Any]:
    """Normalize only an exact v1 or journal-bound v2 external manifest."""
    manifest_sha256 = migration.sha256(manifest_path)
    schema = manifest.get("schema")
    version = manifest.get("schemaVersion")
    if (
        schema == migration.EXTERNAL_MANIFEST_SCHEMA_V1
        and type(version) is int
        and version == 1
    ):
        evidence = manifest.get("evidence")
        if type(evidence) is not dict or "runtimeCheckpointJournal" in evidence:
            raise GenerationError(
                "external manifest v1 has an invalid runtime-journal layout"
            )
        return {"manifestSha256": manifest_sha256}
    if not (
        schema == migration.EXTERNAL_MANIFEST_SCHEMA_V2
        and type(version) is int
        and version == 2
    ):
        raise GenerationError(
            "external manifest schema/version is neither exact v1 nor exact v2"
        )
    evidence = manifest.get("evidence")
    if type(evidence) is not dict:
        raise GenerationError("external manifest v2 evidence must be an object")
    runtime_journal = evidence.get("runtimeCheckpointJournal")
    journal_issues: list[str] = []
    migration._validate_runtime_checkpoint_journal_binding(
        runtime_journal,
        "external manifest evidence.runtimeCheckpointJournal",
        journal_issues,
    )
    candidate_binding = manifest.get("candidateEvidenceBindingSha256")
    if (
        type(candidate_binding) is not str
        or not migration.SHA_RE.fullmatch(candidate_binding)
    ):
        journal_issues.append(
            "external manifest candidateEvidenceBindingSha256 must be uppercase SHA-256"
        )
    else:
        try:
            observed_binding = external_evidence.candidate_binding(manifest)
        except (KeyError, TypeError, ValueError) as exc:
            raise GenerationError(
                "external manifest v2 candidate binding cannot be recomputed"
            ) from exc
        if candidate_binding != observed_binding:
            journal_issues.append(
                "external manifest candidateEvidenceBindingSha256 differs"
            )
    if journal_issues:
        raise GenerationError("; ".join(journal_issues))
    binding = {
        "manifestSha256": manifest_sha256,
        "manifestSchema": migration.EXTERNAL_MANIFEST_SCHEMA_V2,
        "manifestSchemaVersion": 2,
        "candidateEvidenceBindingSha256": candidate_binding,
        "runtimeCheckpointJournal": copy.deepcopy(runtime_journal),
    }
    authority_issues: list[str] = []
    migration.validate_external_manifest_authority(
        {
            "userDirToken": "00000000-0000-4000-8000-000000000000",
            "runDirectoryLeaf": "synthetic",
            **binding,
            "externalTechnicalValidationState": (
                "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
            ),
            "freshUserDirValidationState": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        },
        manifest,
        "external manifest",
        authority_issues,
    )
    if authority_issues:
        raise GenerationError("; ".join(authority_issues))
    return binding


def _policy_document(
    candidate_id: str,
    archive: dict[str, Any],
    executable: dict[str, Any],
    user_dir_token: str,
    external_manifest: dict[str, Any],
    runtime_screenshots: list[dict[str, Any]],
) -> dict[str, Any]:
    return {
        "schema": migration.CANDIDATE_POLICY_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "policyId": f"session19_serialized_asset_migration_candidate_{candidate_id}",
        "authority": migration.CANDIDATE_POLICY_AUTHORITY,
        "state": "CANDIDATE_INPUTS_BOUND_SUPPLEMENT_RECEIPT_REQUIRED",
        "releaseReady": False,
        "shippingEvidenceId": (
            "session19_retained_serialized_asset_migration_shipping_"
            f"non_distribution_{candidate_id}"
        ),
        "shippingReceiptPath": _shipping_path(candidate_id),
        "bindings": migration._candidate_binding_paths(candidate_id),
        "archive": copy.deepcopy(archive),
        "executable": copy.deepcopy(executable),
        "externalEvidence": {
            "userDirToken": user_dir_token,
            "runDirectoryLeaf": f"{candidate_id}_{user_dir_token}",
            **copy.deepcopy(external_manifest),
            "externalTechnicalValidationState": (
                "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
            ),
            "freshUserDirValidationState": (
                "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
            ),
        },
        "runtimeScreenshots": copy.deepcopy(runtime_screenshots),
    }


def _supplement_document(
    authority: dict[str, Any],
    bindings: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    receipt = {
        "schema": authority["receiptSchema"],
        "schemaVersion": authority["receiptSchemaVersion"],
        "session": 19,
        "candidateId": authority["candidateId"],
        "evidenceId": authority["evidenceId"],
        "authority": "ADDITIVE_ENGINEERING_EVIDENCE_NOT_RELEASE_OR_LEGAL_APPROVAL",
        "state": (
            "PASS_CANDIDATE_BOUND_TECHNICAL_MIGRATION_SHIPPING_NON_DISTRIBUTION"
        ),
        "bindings": copy.deepcopy(bindings),
        "archive": copy.deepcopy(authority["archive"]),
        "retainedRuntimeAssets": copy.deepcopy(migration.SHIPPING_RETAINED),
        "runtimeScreenshots": copy.deepcopy(authority["runtimeScreenshots"]),
        "nonDistributionBoundary": {
            "legacyScriptPackage": "/Script/DiscGolfCharacterFramework",
            "replacementScriptPackage": "/Script/DiscGolfRuntimeFoundation",
            "candidateContentCategory": "LEGACY_CHARACTER_FRAMEWORK",
            "archiveAuthoritativeMatchCount": 0,
            "archiveManifestDiagnosticMatchCount": 0,
            "shippingExecutableForbiddenMatchCount": 0,
            "shippingExecutableReplacementMarkerFound": True,
            "legacyFrameworkNonDistributionVerified": True,
            "technicalMigrationShippingBoundaryClosed": True,
        },
        "claimBoundary": {
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
            "blockerId": (
                "CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED"
            ),
            "blockerClosed": False,
            "releaseApproval": False,
            "releaseReady": False,
        },
    }
    policy_binding = authority.get("candidatePolicyBinding")
    if policy_binding is not None:
        # Insert at a stable location without making ordering authoritative.
        receipt = {
            **{key: receipt[key] for key in (
                "schema", "schemaVersion", "session", "candidateId", "evidenceId",
                "authority", "state",
            )},
            "candidatePolicy": copy.deepcopy(policy_binding),
            **{key: value for key, value in receipt.items() if key not in {
                "schema", "schemaVersion", "session", "candidateId", "evidenceId",
                "authority", "state",
            }},
        }
    return receipt


def _bound_documents(root: Path, candidate_id: str) -> dict[str, dict[str, Any]]:
    return {
        key: _load_project_json(root, token, f"candidate binding {key}")
        for key, token in migration._candidate_binding_paths(candidate_id).items()
    }


def _manifest_screenshots(
    external_run: Path,
    manifest: dict[str, Any],
) -> list[dict[str, Any]]:
    evidence = manifest.get("evidence")
    if type(evidence) is not dict:
        raise GenerationError("external manifest evidence must be an object")
    values = evidence.get("screenshots")
    if type(values) is not list:
        raise GenerationError("external manifest screenshots must be an array")
    by_path: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(values):
        if type(value) is not dict or type(value.get("relativePath")) is not str:
            raise GenerationError(f"external manifest screenshot[{index}] is malformed")
        token = value["relativePath"]
        if token in by_path:
            raise GenerationError(f"external manifest duplicates screenshot {token}")
        by_path[token] = value
    result: list[dict[str, Any]] = []
    for milestone, token in SCREENSHOT_MILESTONES:
        manifest_item = by_path.get(token)
        if manifest_item is None:
            raise GenerationError(f"external manifest lacks required screenshot {token}")
        issues: list[str] = []
        path = migration._safe_external_path(
            external_run, token, f"runtime screenshot {milestone}", issues,
        )
        if issues or path is None or not path.is_file() or path.is_symlink():
            raise GenerationError("; ".join(issues or [f"missing screenshot {token}"]))
        try:
            width, height = migration._png_dimensions(path)
        except (OSError, ValueError) as exc:
            raise GenerationError(f"runtime screenshot is invalid: {token}: {exc}") from exc
        observed = {
            "bytes": path.stat().st_size,
            "sha256": migration.sha256(path),
            "width": width,
            "height": height,
        }
        expected = {
            key: manifest_item.get(key)
            for key in ("bytes", "sha256", "width", "height")
        }
        if observed != expected or manifest_item.get("modifiedWithinRunWindow") is not True:
            raise GenerationError(f"runtime screenshot differs from manifest: {token}")
        result.append({
            "milestone": milestone,
            "path": token,
            **observed,
            "technicalOnly": True,
            "freshInstallEvidence": False,
            "humanReviewed": False,
        })
    return result


def _resolve_live_inputs(
    candidate_id: str,
    archive_value: Path,
    unrealpak_value: Path,
    external_run_value: Path,
    user_dir_token: str,
) -> tuple[Path, Path, Path, Path]:
    if not migration.CANDIDATE_ID_RE.fullmatch(candidate_id):
        raise GenerationError("candidate ID is malformed")
    if not migration.USER_DIR_TOKEN_RE.fullmatch(user_dir_token):
        raise GenerationError("external user-dir token is malformed")
    archive = _absolute_host_path(archive_value, "archive", directory=True)
    unrealpak = _absolute_host_path(unrealpak_value, "UnrealPak", directory=False)
    external_run = _absolute_host_path(
        external_run_value, "external run", directory=True,
    )
    if archive.name != "Windows" or archive.parent.name != candidate_id:
        raise GenerationError("archive must be the exact <candidateId>/Windows directory")
    if unrealpak.name.casefold() != "unrealpak.exe":
        raise GenerationError("UnrealPak filename must be UnrealPak.exe")
    if external_run.name != f"{candidate_id}_{user_dir_token}":
        raise GenerationError(
            "external run leaf must be the exact <candidateId>_<userDirToken>"
        )
    executable = archive / migration.SHIPPING_EXE_RELATIVE
    executable = _absolute_host_path(executable, "Shipping executable", directory=False)
    return archive, executable, unrealpak, external_run


def _build_policy_from_live_inputs(
    root: Path,
    candidate_id: str,
    archive: Path,
    executable: Path,
    external_run: Path,
    user_dir_token: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    documents = _bound_documents(root, candidate_id)
    content = documents["candidateContentAudit"]
    manifest_path = external_run / "technical-evidence-manifest.json"
    if migration._host_path_contains_reparse(manifest_path):
        raise GenerationError("external technical-evidence manifest contains a reparse point")
    try:
        manifest = migration.load_json(manifest_path)
    except migration.StrictJsonError as exc:
        raise GenerationError(f"external technical-evidence manifest is invalid: {exc}") from exc
    if type(manifest) is not dict:
        raise GenerationError("external technical-evidence manifest root must be an object")
    archive_identity = content.get("archive")
    if type(archive_identity) is not dict:
        raise GenerationError("candidate content audit archive identity is missing")
    executable_identity = {
        "relativePath": migration.SHIPPING_EXE_RELATIVE.as_posix(),
        "bytes": executable.stat().st_size,
        "sha256": migration.sha256(executable),
    }
    screenshots = _manifest_screenshots(external_run, manifest)
    external_manifest_binding = _external_manifest_policy_binding(
        manifest_path, manifest,
    )
    policy = _policy_document(
        candidate_id,
        archive_identity,
        executable_identity,
        user_dir_token,
        external_manifest_binding,
        screenshots,
    )
    payload = _json_bytes(policy)
    policy_binding = _bytes_binding(_policy_path(candidate_id), payload)
    policy_issues, authority = migration.validate_candidate_policy(
        policy,
        root,
        policy_binding=policy_binding,
        check_files=False,
        require_shipping_receipt=False,
    )
    if policy_issues or authority is None:
        raise GenerationError(
            "candidate policy construction failed: " + "; ".join(policy_issues)
        )
    candidate = manifest.get("candidate")
    user_dir = manifest.get("userDir")
    claim_boundary = manifest.get("claimBoundary")
    process = manifest.get("process")
    if type(candidate) is not dict:
        raise GenerationError("external manifest candidate must be an object")
    if type(user_dir) is not dict:
        raise GenerationError("external manifest userDir must be an object")
    if type(claim_boundary) is not dict:
        raise GenerationError("external manifest claimBoundary must be an object")
    if type(process) is not dict:
        raise GenerationError("external manifest process must be an object")
    manifest_archive_identity = {
        key: archive_identity.get(key)
        for key in ("fileCount", "bytes", "canonicalManifestSha256")
    }
    manifest_version = authority["externalManifestSchemaVersion"]
    manifest_checks = {
        "schema": manifest.get("schema") == (
            migration.EXTERNAL_MANIFEST_SCHEMA_V2
            if manifest_version == 2
            else migration.EXTERNAL_MANIFEST_SCHEMA_V1
        ),
        "schemaVersion": (
            type(manifest.get("schemaVersion")) is int
            and manifest.get("schemaVersion") == manifest_version
        ),
        "session": manifest.get("session") == 19,
        "state": manifest.get("state") == (
            "CAPTURED_TECHNICAL_EVIDENCE_PENDING_INDEPENDENT_VALIDATION"
        ),
        "candidateId": candidate.get("candidateId") == candidate_id,
        "archivePreserved": candidate.get("archivePreserved") is True,
        "archiveBeforeIdentity": (
            candidate.get("archiveBefore") == manifest_archive_identity
        ),
        "archiveAfterIdentity": (
            candidate.get("archiveAfter") == manifest_archive_identity
        ),
        "archiveManifest": candidate.get("expectedArchiveManifestSha256") == (
            archive_identity.get("canonicalManifestSha256")
        ),
        "executablePath": candidate.get("exeRelativePath") == (
            executable_identity["relativePath"]
        ),
        "executableIdentity": candidate.get("observedExeSha256") == (
            executable_identity["sha256"]
        ),
        "expectedExecutableIdentity": candidate.get("expectedExeSha256") == (
            executable_identity["sha256"]
        ),
        "userDirToken": user_dir.get("token") == user_dir_token,
        "userDirEmpty": user_dir.get("emptyBeforeLaunch") is True,
        "userDirExternal": user_dir.get("externalBoundaryValidated") is True,
        "userDirPathRedacted": user_dir.get("hostPathRecorded") is False,
        "userDirReparseRejected": user_dir.get("reparsePointsRejected") is True,
        "processExit": process.get("exitCode") == 0,
        "processTimeout": process.get("timedOut") is False,
        "releaseBoundary": claim_boundary.get("releaseReady") is False,
    }
    if manifest_version == 2:
        manifest_checks.update({
            "candidateEvidenceBinding": (
                manifest.get("candidateEvidenceBindingSha256")
                == authority["externalEvidence"]["candidateEvidenceBindingSha256"]
            ),
            "runtimeCheckpointJournal": (
                manifest.get("evidence", {}).get("runtimeCheckpointJournal")
                == authority["externalEvidence"]["runtimeCheckpointJournal"]
            ),
        })
    failed = [key for key, passed in manifest_checks.items() if not passed]
    if failed:
        raise GenerationError(
            "external manifest candidate binding differs: " + ", ".join(failed)
        )
    return policy, authority


def _supplement_bindings(
    root: Path,
    authority: dict[str, Any],
) -> dict[str, dict[str, Any]]:
    return {
        key: _project_binding(root, token, f"supplement binding {key}")
        for key, token in authority["bindingPaths"].items()
    }


def _live_validate_supplement(
    root: Path,
    authority: dict[str, Any],
    archive: Path,
    executable: Path,
    unrealpak: Path,
    external_run: Path,
) -> tuple[dict[str, Any], list[str]]:
    supplement = _supplement_document(
        authority,
        _supplement_bindings(root, authority),
    )
    issues, _ = migration.validate_shipping_evidence(
        supplement,
        root,
        candidate_authority=authority,
        candidate_id=authority["candidateId"],
        archive=archive,
        executable=executable,
        unrealpak=unrealpak,
        external_run=external_run,
        check_files=True,
        live_reaudit=True,
    )
    return supplement, issues


def generate_policy(args: argparse.Namespace) -> dict[str, Any]:
    root = args.root.resolve(strict=True)
    archive, executable, unrealpak, external_run = _resolve_live_inputs(
        args.candidate_id,
        args.archive,
        args.unrealpak,
        args.external_run,
        args.external_user_dir_token,
    )
    output_token = _policy_path(args.candidate_id)
    output = _project_output(root, output_token, "candidate policy output")
    # Refuse a stale/foreign supplement before committing the authority that
    # requires this exact append-only receipt path.
    _project_output(root, _shipping_path(args.candidate_id), "Shipping supplement output")
    policy, authority = _build_policy_from_live_inputs(
        root,
        args.candidate_id,
        archive,
        executable,
        external_run,
        args.external_user_dir_token,
    )
    _, issues = _live_validate_supplement(
        root, authority, archive, executable, unrealpak, external_run,
    )
    result = _publish_validated(output, policy, issues)
    return {**result, "candidateId": args.candidate_id}


def generate_supplement(args: argparse.Namespace) -> dict[str, Any]:
    root = args.root.resolve(strict=True)
    if args.candidate_policy.is_absolute():
        raise GenerationError("candidate policy must be a project-relative Config path")
    policy_issues, authority, policy_binding = migration.load_candidate_policy(
        args.candidate_policy,
        root,
        check_files=True,
        require_shipping_receipt=False,
    )
    if policy_issues or authority is None or policy_binding is None:
        raise GenerationError(
            "candidate policy is invalid: " + "; ".join(policy_issues)
        )
    token = authority["externalEvidence"]["userDirToken"]
    archive, executable, unrealpak, external_run = _resolve_live_inputs(
        authority["candidateId"],
        args.archive,
        args.unrealpak,
        args.external_run,
        token,
    )
    output = _project_output(
        root, authority["shippingReceiptPath"], "Shipping supplement output",
    )
    supplement, issues = _live_validate_supplement(
        root, authority, archive, executable, unrealpak, external_run,
    )
    result = _publish_validated(output, supplement, issues)
    return {**result, "candidateId": authority["candidateId"]}


def _synthetic_runtime_journal() -> dict[str, Any]:
    return {
        "bytes": 3228,
        "captureNonce": "12345678-1234-4abc-8def-1234567890ab",
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
        "modifiedUtc": "2099-01-01T00:00:01.000Z",
        "modifiedWithinRunWindow": True,
        "roundId": "87654321-4321-4abc-8def-ba0987654321",
        "schema": migration.RUNTIME_JOURNAL_SCHEMA,
        "sha256": "E" * 64,
        "userDirRelativePath": migration.RUNTIME_JOURNAL_RELATIVE,
        "validationFailures": [],
        "validationState": migration.RUNTIME_JOURNAL_PASS,
    }


def _synthetic_external_manifest_v2(
    candidate_id: str,
    screenshots: list[dict[str, Any]],
) -> dict[str, Any]:
    manifest = {
        "schema": migration.EXTERNAL_MANIFEST_SCHEMA_V2,
        "schemaVersion": 2,
        "policy": {"sha256": "F" * 64},
        "candidate": {"candidateId": candidate_id},
        "userDir": {"token": "12345678-1234-4abc-8def-1234567890ab"},
        "process": {
            "pid": 1234,
            "startedUtc": "2099-01-01T00:00:00.000Z",
            "finishedUtc": "2099-01-01T00:00:02.000Z",
            "exitCode": 0,
            "timedOut": False,
        },
        "evidence": {
            "launchRecord": {"sha256": "A" * 64},
            "screenshots": [
                {
                    "relativePath": item["path"],
                    "bytes": item["bytes"],
                    "sha256": item["sha256"],
                    "width": item["width"],
                    "height": item["height"],
                }
                for item in screenshots
            ],
            "presentMon": {
                "requested": False,
                "relativePath": None,
                "bytes": 0,
                "sha256": None,
                "rowCount": 0,
            },
            "runtimeCheckpointJournal": _synthetic_runtime_journal(),
        },
        "candidateEvidenceBindingSha256": "",
    }
    manifest["candidateEvidenceBindingSha256"] = external_evidence.candidate_binding(
        manifest
    )
    return manifest


def _synthetic_policy(
    candidate_id: str,
    *,
    external_manifest: dict[str, Any] | None = None,
) -> tuple[dict[str, Any], dict[str, Any]]:
    token = "12345678-1234-4abc-8def-1234567890ab"
    screenshots = [
        {
            "milestone": milestone,
            "path": path,
            "bytes": index + 10,
            "sha256": f"{index + 1:064X}",
            "width": 1920,
            "height": 1080,
            "technicalOnly": True,
            "freshInstallEvidence": False,
            "humanReviewed": False,
        }
        for index, (milestone, path) in enumerate(SCREENSHOT_MILESTONES)
    ]
    policy = _policy_document(
        candidate_id,
        {
            "hostPathRecorded": False,
            "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
            "fileCount": 31,
            "bytes": 123456,
            "canonicalManifestSha256": "A" * 64,
        },
        {
            "relativePath": migration.SHIPPING_EXE_RELATIVE.as_posix(),
            "bytes": 98765,
            "sha256": "B" * 64,
        },
        token,
        (
            {"manifestSha256": "C" * 64}
            if external_manifest is None
            else external_manifest
        ),
        screenshots,
    )
    payload = _json_bytes(policy)
    binding = _bytes_binding(_policy_path(candidate_id), payload)
    issues, authority = migration.validate_candidate_policy(
        policy,
        ROOT,
        policy_binding=binding,
        check_files=False,
        require_shipping_receipt=False,
    )
    if issues or authority is None:
        raise GenerationError("synthetic policy failed: " + "; ".join(issues))
    return policy, authority


def run_self_test() -> tuple[bool, int, list[str]]:
    failures: list[str] = []
    checks = 0

    def check(name: str, action: Callable[[], bool]) -> None:
        nonlocal checks
        checks += 1
        try:
            passed = action()
        except Exception as exc:
            failures.append(f"{name}: {exc.__class__.__name__}: {exc}")
            return
        if not passed:
            failures.append(f"{name}: condition was false")

    candidate_a = "S19_WindowsShipping_Synthetic_20990101T000000Z_deadbeefcafe"
    candidate_b = "S19_WindowsShipping_Synthetic_20990101T000001Z_feedfacecafe"
    policy_a, authority_a = _synthetic_policy(candidate_a)
    policy_b, authority_b = _synthetic_policy(candidate_b)
    check("alternate-candidate-policy", lambda: policy_a["candidateId"] != policy_b["candidateId"])
    check(
        "alternate-candidate-paths",
        lambda: authority_a["shippingReceiptPath"] != authority_b["shippingReceiptPath"],
    )
    fake_bindings = {
        key: {"path": token, "bytes": 1, "sha256": "D" * 64}
        for key, token in authority_a["bindingPaths"].items()
    }
    supplement = _supplement_document(authority_a, fake_bindings)
    supplement_issues, _ = migration.validate_shipping_evidence(
        supplement,
        ROOT,
        candidate_authority=authority_a,
        candidate_id=candidate_a,
        check_files=False,
        live_reaudit=False,
    )
    check("synthetic-supplement", lambda: not supplement_issues)
    tampered = copy.deepcopy(supplement)
    tampered["candidatePolicy"]["sha256"] = "0" * 64
    tamper_issues, _ = migration.validate_shipping_evidence(
        tampered,
        ROOT,
        candidate_authority=authority_a,
        candidate_id=candidate_a,
        check_files=False,
        live_reaudit=False,
    )
    check("tampered-policy-binding-rejected", lambda: bool(tamper_issues))
    traversal = copy.deepcopy(policy_a)
    traversal["runtimeScreenshots"][0]["path"] = "../escape.png"
    traversal_issues, _ = migration.validate_candidate_policy(
        traversal,
        ROOT,
        policy_binding=authority_a["candidatePolicyBinding"],
        check_files=False,
        require_shipping_receipt=False,
    )
    check("traversal-rejected", lambda: bool(traversal_issues))
    missing_receipt_issues, _ = migration.validate_candidate_policy(
        policy_a,
        ROOT,
        policy_binding=authority_a["candidatePolicyBinding"],
        check_files=True,
    )
    check(
        "read-only-validation-requires-supplement",
        lambda: any("shippingReceiptPath" in issue for issue in missing_receipt_issues),
    )

    with tempfile.TemporaryDirectory(prefix="dgtour-serialized-generator-test-") as temp:
        directory = Path(temp)
        synthetic_manifest = _synthetic_external_manifest_v2(
            candidate_b, policy_b["runtimeScreenshots"],
        )
        synthetic_manifest_path = directory / "technical-evidence-manifest.json"
        synthetic_manifest_path.write_bytes(_json_bytes(synthetic_manifest))
        v2_binding = _external_manifest_policy_binding(
            synthetic_manifest_path, synthetic_manifest,
        )
        policy_v2, authority_v2 = _synthetic_policy(
            candidate_b, external_manifest=v2_binding,
        )
        check(
            "journal-bound-v2-policy",
            lambda: (
                authority_v2["externalManifestSchemaVersion"] == 2
                and authority_v2["externalEvidence"]["runtimeCheckpointJournal"]
                == synthetic_manifest["evidence"]["runtimeCheckpointJournal"]
            ),
        )

        def policy_mutation_rejected(mutation: Callable[[dict[str, Any]], None]) -> bool:
            candidate = copy.deepcopy(policy_v2)
            mutation(candidate)
            mutation_issues, _ = migration.validate_candidate_policy(
                candidate,
                ROOT,
                policy_binding=authority_v2["candidatePolicyBinding"],
                check_files=False,
                require_shipping_receipt=False,
            )
            return bool(mutation_issues)

        check(
            "v2-journal-binding-required",
            lambda: policy_mutation_rejected(
                lambda d: d["externalEvidence"].pop("runtimeCheckpointJournal")
            ),
        )
        check(
            "v2-schema-downgrade-rejected",
            lambda: policy_mutation_rejected(
                lambda d: d["externalEvidence"].__setitem__(
                    "manifestSchemaVersion", 1
                )
            ),
        )
        check(
            "v2-journal-state-tamper-rejected",
            lambda: policy_mutation_rejected(
                lambda d: d["externalEvidence"]["runtimeCheckpointJournal"].__setitem__(
                    "validationState", "PASS"
                )
            ),
        )
        check(
            "v2-journal-count-tamper-rejected",
            lambda: policy_mutation_rejected(
                lambda d: d["externalEvidence"]["runtimeCheckpointJournal"].__setitem__(
                    "eventCount", 11
                )
            ),
        )

        def live_manifest_mutation_rejected(
            mutation: Callable[[dict[str, Any]], None],
        ) -> bool:
            candidate_manifest = copy.deepcopy(synthetic_manifest)
            mutation(candidate_manifest)
            live_issues: list[str] = []
            migration.validate_external_manifest_authority(
                policy_v2["externalEvidence"],
                candidate_manifest,
                "synthetic external manifest",
                live_issues,
            )
            return bool(live_issues)

        check(
            "v2-live-journal-tamper-rejected",
            lambda: live_manifest_mutation_rejected(
                lambda d: d["evidence"]["runtimeCheckpointJournal"].__setitem__(
                    "sha256", "0" * 64
                )
            ),
        )
        check(
            "v2-live-manifest-downgrade-rejected",
            lambda: live_manifest_mutation_rejected(
                lambda d: (
                    d.__setitem__("schema", migration.EXTERNAL_MANIFEST_SCHEMA_V1),
                    d.__setitem__("schemaVersion", 1),
                    d["evidence"].pop("runtimeCheckpointJournal"),
                )
            ),
        )
        check(
            "v2-live-candidate-binding-tamper-rejected",
            lambda: live_manifest_mutation_rejected(
                lambda d: d.__setitem__(
                    "candidateEvidenceBindingSha256", "0" * 64
                )
            ),
        )

        def generator_v2_missing_journal_rejected() -> bool:
            candidate_manifest = copy.deepcopy(synthetic_manifest)
            candidate_manifest["evidence"].pop("runtimeCheckpointJournal")
            try:
                _external_manifest_policy_binding(
                    synthetic_manifest_path, candidate_manifest,
                )
            except GenerationError:
                return True
            return False

        check(
            "generator-v2-missing-journal-rejected",
            generator_v2_missing_journal_rejected,
        )

        def host_traversal_refused() -> bool:
            try:
                _absolute_host_path(
                    directory / "untrusted" / ".." / "input.json",
                    "synthetic host input",
                    directory=False,
                )
            except GenerationError as exc:
                return "parent traversal" in str(exc)
            return False

        check("host-traversal-rejected", host_traversal_refused)
        created = directory / "created.json"
        created_result = _publish_validated(created, policy_a, [])
        original = created.read_bytes()
        check(
            "exclusive-create",
            lambda: (
                created_result["bytes"] == len(original)
                and created_result["sha256"] == hashlib.sha256(original).hexdigest().upper()
            ),
        )

        def overwrite_refused() -> bool:
            try:
                _publish_validated(created, {"state": "MUTATED"}, [])
            except GenerationError:
                return created.read_bytes() == original
            return False

        check("overwrite-refused", overwrite_refused)
        tamper_output = directory / "tamper.json"

        def validation_failure_absent() -> bool:
            try:
                _publish_validated(tamper_output, tampered, tamper_issues)
            except GenerationError:
                return not os.path.lexists(tamper_output)
            return False

        check("tamper-no-partial-output", validation_failure_absent)
        strict_output = directory / "strict.json"

        def serialization_failure_absent() -> bool:
            try:
                _publish_validated(strict_output, {"value": float("nan")}, [])
            except GenerationError:
                return not os.path.lexists(strict_output)
            return False

        check("serialization-no-partial-output", serialization_failure_absent)
        check(
            "temporary-files-cleaned",
            lambda: not list(directory.glob(".dgtour-serialized-migration-*.tmp")),
        )
    return not failures, checks, failures


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    commands = parser.add_subparsers(dest="command")
    policy = commands.add_parser("policy", help="create one immutable candidate policy v1")
    policy.add_argument("--candidate-id", required=True)
    policy.add_argument("--archive", type=Path, required=True)
    policy.add_argument("--external-user-dir-token", required=True)
    policy.add_argument("--external-run", type=Path, required=True)
    policy.add_argument("--unrealpak", type=Path, required=True)
    supplement = commands.add_parser(
        "supplement", help="create one immutable candidate ShippingEvidence v2 receipt",
    )
    supplement.add_argument("--candidate-policy", type=Path, required=True)
    supplement.add_argument("--archive", type=Path, required=True)
    supplement.add_argument("--external-run", type=Path, required=True)
    supplement.add_argument("--unrealpak", type=Path, required=True)
    return parser


def main() -> int:
    parser = _parser()
    args = parser.parse_args()
    if args.self_test:
        if args.command is not None:
            parser.error("--self-test cannot be combined with a generation command")
        passed, checks, failures = run_self_test()
        if not passed:
            print("SESSION 19 SERIALIZED MIGRATION GENERATOR SELF-TEST FAILED")
            for failure in failures:
                print(f" - {failure}")
            return 1
        print(f"SESSION 19 SERIALIZED MIGRATION GENERATOR SELF-TEST PASS checks={checks}")
        return 0
    if args.command is None:
        parser.error("choose policy or supplement (or use --self-test)")
    try:
        result = generate_policy(args) if args.command == "policy" else generate_supplement(args)
    except (GenerationError, OSError, migration.StrictJsonError) as exc:
        print(f"SESSION 19 SERIALIZED MIGRATION GENERATION REFUSED: {exc}")
        return 1
    print(
        "SESSION 19 SERIALIZED MIGRATION APPEND-ONLY CREATE PASS "
        f"stage={args.command} candidate={result['candidateId']} "
        f"path={result['path'].relative_to(args.root.resolve()).as_posix()} "
        f"bytes={result['bytes']} sha256={result['sha256']} "
        "release_ready=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
