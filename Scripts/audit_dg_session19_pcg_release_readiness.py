#!/usr/bin/env python3
"""Read-only audit of the Session 19 PCG production-readiness blocker.

This tool intentionally does not emit or update evidence.  It reconciles the
active authored-JSON/Editor-only-PCG Shipping strategy with the older release
closure contract, then inventories candidate-bound technical, collision,
visual, performance, and soak evidence without promoting human approvals.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]+$")
PCG_BLOCKER = "SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING"
SEPARATION_STRATEGY = (
    "SHIP_EXACT_AUTHORED_JSON_TREE_OUTPUT_AND_KEEP_PCG_AUTHORING_CODE_AND_GRAPH_OUT_OF_SHIPPING"
)
RELEASE_BAKE_STRATEGY = (
    "DETERMINISTICALLY_BAKE_VALIDATE_HASH_AND_COOK_PCG_OUTPUT_WITHOUT_RUNTIME_GENERATION"
)
REQUIRED_COLLISION_TESTS = (
    "DiscGolfTour.CourseDefinition.PresentationCollisionInvariant",
    "DiscGolfTour.Environment.AssetBinder.TreeCollisionProxyClassification",
    "DiscGolfTour.Fixtures.CollisionDynamics",
    "DiscGolfTour.Session13.CourseAuthoring.PcgAdapter.DeterminismAndAuthority",
    "DiscGolfTour.Session19.PcgShippingSeparation.EditorAuthoringOnly",
)
SUCCESS_RE = re.compile(
    r"Test Completed\. Result=\{Success\}.*?Path=\{([^}]+)\}"
)
OPERATIONAL_PASS_STATE = (
    "PASS_CANDIDATE_BOUND_AUTHORED_JSON_OPERATIONAL_RUNTIME_TECHNICAL_PROOF"
)
OPERATIONAL_PROOF_AUTHORITIES: dict[str, dict[str, dict[str, Any]]] = {
    "S19_WindowsShipping_20260827T023919Z_51a1c132f837": {
        "policy": {
            "path": (
                "Config/DG_Session19PcgOperationalRuntimeProofPolicy-"
                "S19_WindowsShipping_20260827T023919Z_51a1c132f837.json"
            ),
            "bytes": 12673,
            "sha256": "E6CEEE0EE777966C5D3133B8BEA4BD0204F5E8CD38F3BC588D11FF2621975D4E",
        },
        "validator": {
            "path": "Scripts/validate_dg_session19_pcg_operational_runtime_proof.py",
            "bytes": 68154,
            "sha256": "EC35ED3B5AA6D7D00C20006F4CF3418E28B0FDCAE29B81825641F09157D20025",
        },
        "receipt": {
            "path": (
                "Evidence/Session19/PcgOperationalRuntimeProof-"
                "S19_WindowsShipping_20260827T023919Z_51a1c132f837.json"
            ),
            "bytes": 5191,
            "sha256": "8B439C6C3E92E39DE86EC5F5C67903404A638F9215DB2779B3E2C2582AFC9ED0",
        },
    },
}


class AuditInputError(ValueError):
    """Raised when an audit input is absent or structurally unsafe."""


def _reject_constant(value: str) -> None:
    raise AuditInputError(f"non-finite JSON constant is forbidden: {value}")


def _object_pairs_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AuditInputError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _read_file(path: Path) -> bytes:
    try:
        before = path.stat()
        raw = path.read_bytes()
        after = path.stat()
    except OSError as exc:
        raise AuditInputError(f"cannot read {path}: {exc}") from exc
    if (
        not path.is_file()
        or path.is_symlink()
        or before.st_size != after.st_size
        or before.st_mtime_ns != after.st_mtime_ns
        or len(raw) != after.st_size
    ):
        raise AuditInputError(f"file is irregular or changed while read: {path}")
    return raw


def _parse_json_object(raw: bytes, label: str) -> dict[str, Any]:
    try:
        document = json.loads(
            raw.decode("utf-8-sig"),
            parse_constant=_reject_constant,
            object_pairs_hook=_object_pairs_no_duplicates,
        )
    except (UnicodeDecodeError, json.JSONDecodeError, AuditInputError) as exc:
        raise AuditInputError(f"invalid JSON object {label}: {exc}") from exc
    if type(document) is not dict:
        raise AuditInputError(f"JSON root must be an object: {label}")
    return document


def load_json_object(path: Path) -> dict[str, Any]:
    return _parse_json_object(_read_file(path), str(path))


def load_json_object_with_identity(
    path: Path, root: Path
) -> tuple[dict[str, Any], dict[str, Any]]:
    raw = _read_file(path)
    try:
        relative = path.resolve(strict=True).relative_to(root.resolve(strict=True)).as_posix()
    except (OSError, ValueError) as exc:
        raise AuditInputError(f"input is outside the project root: {path}") from exc
    return _parse_json_object(raw, relative), {
        "path": relative,
        "bytes": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
    }


def load_file_identity(path: Path, root: Path) -> dict[str, Any]:
    raw = _read_file(path)
    try:
        relative = path.resolve(strict=True).relative_to(root.resolve(strict=True)).as_posix()
    except (OSError, ValueError) as exc:
        raise AuditInputError(f"input is outside the project root: {path}") from exc
    return {
        "path": relative,
        "bytes": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
    }


def _nested(document: dict[str, Any], *keys: str, default: Any = None) -> Any:
    current: Any = document
    for key in keys:
        if type(current) is not dict or key not in current:
            return default
        current = current[key]
    return current


def _bool(document: dict[str, Any], *keys: str) -> bool:
    return _nested(document, *keys) is True


def _same_payload(left: Any, right: Any) -> bool:
    if type(left) is not dict or type(right) is not dict:
        return False
    def identity(document: dict[str, Any]) -> tuple[int, int, str] | None:
        tree_count = document.get("treeCount")
        byte_count = document.get("bytes", document.get("canonicalPayloadBytes"))
        sha256 = document.get("sha256", document.get("canonicalPayloadSha256"))
        if (
            type(tree_count) is not int or tree_count <= 0
            or type(byte_count) is not int or byte_count <= 0
            or type(sha256) is not str
            or re.fullmatch(r"[0-9A-F]{64}", sha256) is None
        ):
            return None
        return tree_count, byte_count, sha256

    left_identity = identity(left)
    return left_identity is not None and left_identity == identity(right)


def _same_file_identity(left: Any, right: Any) -> bool:
    if type(left) is not dict or type(right) is not dict:
        return False
    keys = ("path", "bytes", "sha256")
    return all(left.get(key) == right.get(key) for key in keys)


def _release_strategy(release_contract: dict[str, Any]) -> dict[str, Any] | None:
    strategies = release_contract.get("closureStrategies")
    if type(strategies) is not list:
        return None
    matches = [
        item for item in strategies
        if type(item) is dict and item.get("blockerId") == PCG_BLOCKER
    ]
    return matches[0] if len(matches) == 1 else None


def _successful_tests(log_text: str) -> set[str]:
    return set(SUCCESS_RE.findall(log_text))


def _candidate_matches(document: dict[str, Any], candidate_id: str) -> bool:
    return document.get("candidateId") == candidate_id


def _validate_operational_proof(
    *,
    candidate_id: str,
    operational_policy: dict[str, Any] | None,
    operational_receipt: dict[str, Any] | None,
    operational_artifacts: dict[str, dict[str, Any]] | None,
    runtime_closure_policy: dict[str, Any],
    separation_policy: dict[str, Any],
    baked_policy: dict[str, Any],
    candidate_receipt: dict[str, Any],
    separation_receipt: dict[str, Any],
    three_hole_receipt: dict[str, Any],
) -> tuple[bool, list[str], dict[str, Any]]:
    """Validate the exact additive proof without promoting its denied claims."""
    if (
        operational_policy is None
        and operational_receipt is None
        and operational_artifacts is None
    ):
        return False, [], {
            "supplied": False,
            "exactAuthorityIdentityPass": False,
            "candidateArchiveRunCrossBindingPass": False,
        }

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    require(
        operational_policy is not None
        and operational_receipt is not None
        and operational_artifacts is not None,
        "operational proof policy, validator, receipt, and artifact identities must be supplied together",
    )
    if operational_policy is None or operational_receipt is None or operational_artifacts is None:
        return False, failures, {
            "supplied": True,
            "exactAuthorityIdentityPass": False,
            "candidateArchiveRunCrossBindingPass": False,
        }

    expected_artifacts = OPERATIONAL_PROOF_AUTHORITIES.get(candidate_id)
    require(expected_artifacts is not None, "candidate has no exact operational-proof authority")
    require(
        expected_artifacts is not None and operational_artifacts == expected_artifacts,
        "operational proof policy, validator, or receipt file identity differs",
    )

    require(
        operational_policy.get("schema")
        == "DiscGolfTour.Session19PcgOperationalRuntimeProofPolicy.v1"
        and operational_policy.get("schemaVersion") == 1
        and operational_policy.get("session") == 19
        and operational_policy.get("policyId")
        == "s19_candidate_authored_json_operational_runtime_proof_v1"
        and operational_policy.get("authority")
        == "CANDIDATE_BOUND_TECHNICAL_INFERENCE_NOT_CODE_TO_BINARY_ATTESTATION_OR_RELEASE_APPROVAL"
        and operational_policy.get("state") == "CANDIDATE_BOUND_POLICY_PENDING_VALIDATION",
        "operational proof policy identity differs",
    )
    policy_candidate = operational_policy.get("candidate")
    require(type(policy_candidate) is dict, "operational proof policy candidate binding is missing")
    if type(policy_candidate) is not dict:
        policy_candidate = {}
    candidate_archive = candidate_receipt.get("archive")
    if type(candidate_archive) is not dict:
        candidate_archive = {}
    candidate_executable = candidate_receipt.get("shippingExecutable")
    if type(candidate_executable) is not dict:
        candidate_executable = {}
    require(
        policy_candidate.get("candidateId") == candidate_id
        and policy_candidate.get("platform") == "Windows"
        and policy_candidate.get("configuration") == "Shipping"
        and policy_candidate.get("archiveFileCount") == candidate_archive.get("fileCount")
        and policy_candidate.get("archiveBytes") == candidate_archive.get("bytes")
        and policy_candidate.get("archiveCanonicalManifestSha256")
        == candidate_archive.get("canonicalManifestSha256")
        and _same_file_identity(
            policy_candidate.get("shippingExecutable"), candidate_executable
        ),
        "operational proof policy candidate/archive/executable binding differs",
    )
    retirement_binding = _nested(
        operational_policy, "strategyReconciliation", "retirementPolicy"
    )
    separation_binding = _nested(
        operational_policy, "strategyReconciliation", "shippingSeparationPolicy"
    )
    require(
        type(retirement_binding) is dict
        and retirement_binding.get("schema") == runtime_closure_policy.get("schema")
        and retirement_binding.get("schemaVersion")
        == runtime_closure_policy.get("schemaVersion")
        and retirement_binding.get("state") == runtime_closure_policy.get("state")
        and retirement_binding.get("strategy") == runtime_closure_policy.get("strategy"),
        "operational proof policy does not bind the active runtime-closure retirement v2 strategy",
    )
    require(
        type(separation_binding) is dict
        and separation_binding.get("schema") == separation_policy.get("schema")
        and separation_binding.get("schemaVersion") == separation_policy.get("schemaVersion")
        and separation_binding.get("state") == separation_policy.get("state")
        and separation_binding.get("strategy") == separation_policy.get("strategy"),
        "operational proof policy does not bind the active Shipping-PCG separation v1 strategy",
    )
    disposition = _nested(
        operational_policy,
        "strategyReconciliation",
        "deterministicUnrealBakeSaveReopen",
    )
    require(
        type(disposition) is dict
        and disposition.get("disposition")
        == "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY"
        and disposition.get("mayBeReportedAsPending") is False
        and disposition.get("mayBeReportedAsProven") is False,
        "operational proof policy bake/save/reopen disposition differs",
    )
    require(
        _nested(operational_policy, "shippingLoader", "shippingSourceFallbackAllowed")
        is False,
        "operational proof policy permits a Shipping source fallback",
    )
    require(
        _same_payload(
            _nested(operational_policy, "archiveAuthoredJson", "combinedTreePayload"),
            baked_policy.get("combinedCanonicalPayload"),
        ),
        "operational proof policy authored-tree payload differs from source authority",
    )
    evidence = operational_policy.get("evidence")
    expected_receipt_path = (
        f"Evidence/Session19/PcgOperationalRuntimeProof-{candidate_id}.json"
    )
    require(
        type(evidence) is dict
        and evidence.get("validator")
        == "Scripts/validate_dg_session19_pcg_operational_runtime_proof.py"
        and evidence.get("receiptPattern", "").format(candidateId=candidate_id)
        == expected_receipt_path
        and evidence.get("passState") == OPERATIONAL_PASS_STATE
        and evidence.get("immutableCreateNewOutput") is True,
        "operational proof policy validator/receipt binding differs",
    )

    require(
        operational_receipt.get("schema")
        == "DiscGolfTour.Session19PcgOperationalRuntimeProofReceipt.v1"
        and operational_receipt.get("schemaVersion") == 1
        and operational_receipt.get("session") == 19
        and operational_receipt.get("candidateId") == candidate_id
        and operational_receipt.get("state") == OPERATIONAL_PASS_STATE
        and operational_receipt.get("authority") == operational_policy.get("authority")
        and operational_receipt.get("receiptPath") == expected_receipt_path,
        "operational proof receipt identity or candidate binding differs",
    )
    receipt_strategy = operational_receipt.get("strategyReconciliation")
    require(
        type(receipt_strategy) is dict
        and receipt_strategy.get("runtimeClosureRetired") is True
        and receipt_strategy.get("shippingStrategy")
        == runtime_closure_policy.get("strategy")
        and receipt_strategy.get("shippingSeparationStrategy")
        == separation_policy.get("strategy")
        and receipt_strategy.get("bakeSaveReopenDisposition")
        == "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY"
        and receipt_strategy.get("bakeSaveReopenApplicable") is False
        and receipt_strategy.get("bakeSaveReopenPending") is False
        and receipt_strategy.get("bakeSaveReopenProven") is False,
        "operational proof receipt strategy reconciliation differs",
    )
    source = operational_receipt.get("sourceClosureAndLoader")
    candidate_source = candidate_receipt.get("sourceClosure")
    if type(candidate_source) is not dict:
        candidate_source = {}
    prebuild = candidate_source.get("preBuildReceipt")
    if type(prebuild) is not dict:
        prebuild = {}
    require(
        type(source) is dict
        and source.get("preBuildClosureSha256") == prebuild.get("preBuildSha256")
        and source.get("postBuildClosureSha256")
        == candidate_source.get("postBuildSha256")
        and source.get("preBuildClosureSha256")
        == source.get("postBuildClosureSha256")
        and source.get("prePostMatch") is True
        and source.get("failClosedShippingLoaderSourceIdentity") is True
        and source.get("shippingFallbackAllowed") is False
        and source.get("authoredDataLoadPolicyAutomationPass") is True,
        "operational proof source-closure/fail-closed loader binding differs",
    )
    require(
        operational_receipt.get("cleanShippingBuild") == {
            "clean": True,
            "iterativeCook": False,
            "uatExitCode": 0,
            "verificationMode": "DIRECT_BUILD_PASS",
            "archiveVerified": True,
        },
        "operational proof clean Shipping build binding differs",
    )
    archive = operational_receipt.get("archiveAuthoredJson")
    three_archive = _nested(three_hole_receipt, "candidate", "archive")
    if type(three_archive) is not dict:
        three_archive = {}
    require(
        type(archive) is dict
        and archive.get("fileCount") == policy_candidate.get("archiveFileCount")
        and archive.get("bytes") == policy_candidate.get("archiveBytes")
        and archive.get("canonicalManifestSha256")
        == policy_candidate.get("archiveCanonicalManifestSha256")
        == three_archive.get("canonicalManifestSha256")
        and _same_file_identity(archive.get("shippingExecutable"), candidate_executable)
        and archive.get("authoredJsonFileCount") == 4
        and archive.get("allExactSourceMatches") is True
        and archive.get("allNonUfsExactlyOnce") is True
        and archive.get("allUfsAbsent") is True
        and _same_payload(
            archive.get("combinedTreePayload"), baked_policy.get("combinedCanonicalPayload")
        ),
        "operational proof receipt archive/authored-JSON binding differs",
    )
    pcg_absence = operational_receipt.get("shippingPcgAbsence")
    zero_fields = (
        "projectPcgGraphEntryCount",
        "pcgModuleEntryCount",
        "archivePcgModuleFileCount",
        "assetRegistryProjectPcgGraphCount",
        "serializedProjectPcgMarkerFileCount",
        "shippingBinaryProjectPcgMarkerFileCount",
    )
    require(
        type(pcg_absence) is dict
        and pcg_absence.get("freshShippingCookAbsenceProven") is True
        and all(pcg_absence.get(field) == 0 for field in zero_fields)
        and _nested(separation_receipt, "cook", "freshShippingCookAbsenceProven")
        is True,
        "operational proof receipt Shipping-PCG absence binding differs",
    )
    runtime = operational_receipt.get("operationalRuntime")
    policy_runtime = operational_policy.get("operationalRuntime")
    three_runtime = three_hole_receipt.get("runtimeJournal")
    if type(policy_runtime) is not dict:
        policy_runtime = {}
    if type(three_runtime) is not dict:
        three_runtime = {}
    receipt_journal = runtime.get("runtimeJournal") if type(runtime) is dict else None
    if type(receipt_journal) is not dict:
        receipt_journal = {}
    expected_run = policy_candidate.get("externalRunToken")
    expected_user_dir = policy_candidate.get("userDirToken")
    require(
        type(runtime) is dict
        and runtime.get("externalRunToken") == expected_run
        and runtime.get("userDirToken") == expected_user_dir
        and _nested(three_hole_receipt, "externalEvidence", "recoveryLocationToken", default="")
        .endswith("/" + str(expected_run))
        and _nested(three_hole_receipt, "freshUserDir", "token") == expected_user_dir
        and runtime.get("launchRecordSha256") == three_runtime.get("launchRecordSha256")
        and runtime.get("technicalEvidenceManifestSha256")
        == three_runtime.get("technicalEvidenceManifestSha256")
        and receipt_journal.get("schema") == three_runtime.get("schema")
        and receipt_journal.get("bytes") == three_runtime.get("bytes")
        and receipt_journal.get("sha256") == three_runtime.get("sha256")
        and receipt_journal.get("captureNonce") == three_runtime.get("captureNonce")
        and receipt_journal.get("roundId") == three_runtime.get("roundId")
        and receipt_journal.get("eventCount") == 12
        and receipt_journal.get("firstSequence") == 1
        and receipt_journal.get("lastSequence") == 12
        and receipt_journal.get("completedHoles") == 3
        and receipt_journal.get("totalHoles") == 3
        and receipt_journal.get("runtimeOperationalBindingValidated") is True
        and runtime.get("singleRoundProven") is True
        and runtime.get("threeHoleCompletionObserved") is True,
        "operational proof receipt external-run/UserDir/journal binding differs",
    )
    conclusion = operational_receipt.get("technicalConclusion")
    require(
        type(conclusion) is dict
        and conclusion.get("boundedOperationalRuntimeAuthoredJsonSelectionProven")
        is True
        and conclusion.get("cryptographicCodeToBinaryAttestation") is False,
        "operational proof technical conclusion differs",
    )
    claim = operational_receipt.get("claimBoundary")
    require(
        type(claim) is dict
        and claim.get("boundedOperationalRuntimeAuthoredJsonSelectionProven") is True,
        "operational proof bounded runtime-selection claim is missing",
    )
    for key in (
        "cryptographicCodeToBinaryAttestation",
        "unrealPcgBakeSaveReopenApplicable",
        "unrealPcgBakeSaveReopenProven",
        "visualApproval",
        "collisionApproval",
        "performanceAcceptance",
        "soakAcceptance",
        "manualGameplayAcceptance",
        "humanPlayFeelApproval",
        "provenanceApproval",
        "legalApproval",
        "distributionClearance",
        "ownerApproval",
        "releaseApproval",
        "session13BlockerClosed",
        "releaseReady",
    ):
        require(type(claim) is dict and claim.get(key) is False,
                f"operational proof receipt overclaims {key}")
    require(
        operational_receipt.get("session13BlockerClosed") is False
        and operational_receipt.get("releaseReady") is False,
        "operational proof receipt closes Session 13 or release readiness",
    )
    valid = not failures
    return valid, failures, {
        "supplied": True,
        "exactAuthorityIdentityPass": (
            expected_artifacts is not None and operational_artifacts == expected_artifacts
        ),
        "candidateArchiveRunCrossBindingPass": valid,
        "policyPath": operational_artifacts.get("policy", {}).get("path"),
        "validatorPath": operational_artifacts.get("validator", {}).get("path"),
        "receiptPath": operational_artifacts.get("receipt", {}).get("path"),
        "receiptState": operational_receipt.get("state"),
        "bakeSaveReopenDisposition": _nested(
            operational_receipt,
            "strategyReconciliation",
            "bakeSaveReopenDisposition",
        ),
        "cryptographicCodeToBinaryAttestation": False,
    }


def evaluate_documents(
    *,
    candidate_id: str,
    baked_policy: dict[str, Any],
    runtime_closure_policy: dict[str, Any],
    separation_policy: dict[str, Any],
    candidate_receipt: dict[str, Any],
    separation_receipt: dict[str, Any],
    three_hole_receipt: dict[str, Any],
    performance_receipt: dict[str, Any] | None,
    release_contract: dict[str, Any],
    automation_log_text: str,
    operational_policy: dict[str, Any] | None = None,
    operational_receipt: dict[str, Any] | None = None,
    operational_artifacts: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    issues: list[str] = []
    expected_schemas = (
        ("baked PCG policy", baked_policy, "DiscGolfTour.Session19BakedPcgPolicy.v3", 3),
        (
            "runtime closure policy",
            runtime_closure_policy,
            "DiscGolfTour.Session19BakedPcgRuntimeClosureRetirementPolicy.v2",
            2,
        ),
        (
            "Shipping separation policy",
            separation_policy,
            "DiscGolfTour.Session19ShippingPcgSeparationPolicy.v1",
            1,
        ),
        (
            "baked PCG candidate receipt",
            candidate_receipt,
            "DiscGolfTour.Session19BakedPcgCandidateVerification.v1",
            1,
        ),
        (
            "Shipping PCG separation receipt",
            separation_receipt,
            "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1",
            1,
        ),
        (
            "three-hole receipt",
            three_hole_receipt,
            "DiscGolfTour.Session19ThreeHoleTechnicalAcceptanceReceipt.v2",
            2,
        ),
        (
            "release contract",
            release_contract,
            "DiscGolfTour.Session19ReleaseScopeContract.v3",
            3,
        ),
    )
    for label, document, schema, version in expected_schemas:
        if document.get("schema") != schema or document.get("schemaVersion") != version:
            issues.append(f"{label} schema identity is invalid")
    if performance_receipt is not None and (
        performance_receipt.get("schema")
        != "DiscGolfTour.Session19ShippingPerformanceEvidence.v1"
        or performance_receipt.get("schemaVersion") != 1
    ):
        issues.append("performance receipt schema identity is invalid")

    for label, document in (
        ("baked PCG candidate receipt", candidate_receipt),
        ("Shipping PCG separation receipt", separation_receipt),
        ("three-hole receipt", three_hole_receipt),
    ):
        if not _candidate_matches(document, candidate_id):
            issues.append(f"{label} is not bound to {candidate_id}")
    if performance_receipt is not None and not _candidate_matches(
        performance_receipt, candidate_id
    ):
        issues.append("performance receipt is bound to a different candidate")
    if candidate_id not in automation_log_text:
        issues.append("automation log does not contain its exact candidate-scoped path")

    executable_identities = (
        _nested(candidate_receipt, "shippingExecutable", "sha256"),
        _nested(separation_receipt, "cook", "containerIdentity", "shippingBinary", "sha256"),
        _nested(three_hole_receipt, "candidate", "shippingExecutable", "sha256"),
        _nested(performance_receipt, "archive", "shippingExecutable", "sha256")
        if performance_receipt is not None else None,
    )
    present_executable_identities = [value for value in executable_identities if value is not None]
    if (
        len(present_executable_identities) < 3
        or any(
            type(value) is not str or re.fullmatch(r"[0-9A-F]{64}", value) is None
            for value in present_executable_identities
        )
        or len(set(present_executable_identities)) != 1
    ):
        issues.append("candidate evidence does not share one exact Shipping executable identity")

    authoritative_payload = baked_policy.get("combinedCanonicalPayload")
    candidate_payload = _nested(
        separation_receipt, "source", "authoredTreePayload"
    )
    policy_payload = _nested(separation_policy, "runtimeAuthority")
    if not _same_payload(authoritative_payload, candidate_payload):
        issues.append("Shipping separation receipt authored-tree payload differs from source authority")
    if not _same_payload(authoritative_payload, policy_payload):
        issues.append("Shipping separation policy authored-tree payload differs from source authority")

    source_authority_bound = (
        _bool(candidate_receipt, "technicalBoundary", "candidateBoundAuthoredDataPass")
        and _bool(candidate_receipt, "sourceClosure", "prePostMatch")
        and _bool(separation_receipt, "claimBoundary", "sourceConfigSeparationProven")
        and _same_payload(authoritative_payload, candidate_payload)
        and _same_payload(authoritative_payload, policy_payload)
    )
    shipping_pcg_absence = (
        _bool(separation_receipt, "cook", "freshShippingCookAbsenceProven")
        and _bool(separation_receipt, "claimBoundary", "freshShippingCookAbsenceProven")
        and _nested(separation_receipt, "cook", "projectPcgGraphEntryCount") == 0
        and _nested(separation_receipt, "cook", "assetRegistryProjectPcgGraphCount") == 0
        and _nested(separation_receipt, "cook", "serializedProjectPcgMarkerFileCount") == 0
    )
    operational_proof_valid, operational_failures, operational_binding = (
        _validate_operational_proof(
            candidate_id=candidate_id,
            operational_policy=operational_policy,
            operational_receipt=operational_receipt,
            operational_artifacts=operational_artifacts,
            runtime_closure_policy=runtime_closure_policy,
            separation_policy=separation_policy,
            baked_policy=baked_policy,
            candidate_receipt=candidate_receipt,
            separation_receipt=separation_receipt,
            three_hole_receipt=three_hole_receipt,
        )
    )
    issues.extend(f"operational authored-JSON proof: {item}" for item in operational_failures)
    # Historical candidate receipts remain immutable and intentionally retain
    # runtimeAuthoredSelectionProven=false.  Only the additive exact receipt may
    # advance this bounded technical gate.
    runtime_authored_selection = operational_proof_valid
    three_hole_technical = (
        _bool(three_hole_receipt, "technicalAcceptance")
        and _bool(three_hole_receipt, "continuityClaims", "singleRoundProven")
        and _bool(three_hole_receipt, "continuityClaims", "threeHoleCompletionObserved")
        and _bool(three_hole_receipt, "runtimeJournal", "runtimeOperationalBindingValidated")
    )

    successes = _successful_tests(automation_log_text)
    missing_collision_tests = sorted(set(REQUIRED_COLLISION_TESTS) - successes)
    automated_collision_contract = not missing_collision_tests

    performance_capture_pass = False
    performance_profile_ready = False
    performance_metadata_gaps: list[str] = []
    performance_runs: list[Any] = []
    if performance_receipt is not None:
        runs = performance_receipt.get("runs")
        if type(runs) is list:
            performance_runs = runs
        holes = sorted(
            run.get("holeNumber") for run in performance_runs
            if type(run) is dict and type(run.get("holeNumber")) is int
        )
        summaries_pass = all(
            type(run) is dict
            and type(run.get("summary")) is dict
            and type(run["summary"].get("sample_count")) is int
            and run["summary"]["sample_count"] >= 600
            and run["summary"].get("hitch_count") == 0
            for run in performance_runs
        )
        performance_capture_pass = (
            performance_receipt.get("passed") is True
            and performance_receipt.get("state")
            == "PASS_CANDIDATE_BOUND_THREE_HOLE_RENDERED_PERFORMANCE"
            and holes == [1, 2, 3]
            and summaries_pass
        )
        boundary = performance_receipt.get("claimBoundary")
        if type(boundary) is not dict:
            boundary = {}
        required_metadata = (
            "gpuDriverRecorded",
            "gpuThreadProfiling",
            "powerModeRecorded",
            "wallPowerRecorded",
        )
        performance_metadata_gaps = [
            key for key in required_metadata if boundary.get(key) is not True
        ]
        performance_profile_ready = performance_capture_pass and not performance_metadata_gaps
    else:
        performance_metadata_gaps = ["candidatePerformanceReceipt"]

    three_claim = three_hole_receipt.get("claimBoundary")
    if type(three_claim) is not dict:
        three_claim = {}
    separation_claim = separation_receipt.get("claimBoundary")
    if type(separation_claim) is not dict:
        separation_claim = {}

    visual_approval = (
        three_claim.get("visualQualityApproval") is True
        and three_claim.get("productOwnerApproval") is True
    )
    collision_visual_approval = separation_claim.get("collisionVisualApproval") is True
    production_performance_acceptance = (
        performance_profile_ready
        and three_claim.get("performanceAcceptance") is True
        and separation_claim.get("performanceAcceptance") is True
    )
    soak_acceptance = (
        three_claim.get("soakAcceptance") is True
        and separation_claim.get("soakAcceptance") is True
        and performance_receipt is not None
        and _bool(performance_receipt, "claimBoundary", "thermalSoak")
    )

    active_strategy = runtime_closure_policy.get("strategy")
    separation_strategy = separation_policy.get("strategy")
    graph_excluded = (
        active_strategy == SEPARATION_STRATEGY
        and separation_strategy
        == "SHIP_EXACT_AUTHORED_JSON_44_TREE_RUNTIME_OUTPUT_AND_EXCLUDE_ALL_PCG_AUTHORING_CODE_AND_GRAPH_CONTENT"
        and _bool(runtime_closure_policy, "retiredCandidateEvidence", "candidateCookProofDelegatedToShippingSeparation")
        and _bool(runtime_closure_policy, "claimBoundary", "legacyCandidateRuntimeClosureRetired")
    )
    contract_strategy = _release_strategy(release_contract)
    contract_approved_strategy = (
        contract_strategy.get("approvedStrategy")
        if contract_strategy is not None else None
    )
    contract_requires_bake = contract_approved_strategy == RELEASE_BAKE_STRATEGY
    strategy_aligned = graph_excluded and operational_proof_valid
    if contract_strategy is None:
        issues.append("release contract has no unique Session 13 PCG closure strategy")

    blockers: list[str] = []
    if graph_excluded and contract_requires_bake and not operational_proof_valid:
        blockers.append("PCG_SHIPPING_STRATEGY_AND_RELEASE_CONTRACT_CONFLICT")
    if not source_authority_bound:
        blockers.append("AUTHORED_TREE_SOURCE_AUTHORITY_NOT_CANDIDATE_BOUND")
    if not shipping_pcg_absence:
        blockers.append("FRESH_SHIPPING_PCG_ABSENCE_NOT_PROVEN")
    if not runtime_authored_selection:
        blockers.append("SHIPPING_RUNTIME_AUTHORED_JSON_SELECTION_NOT_PROVEN")
    if not three_hole_technical:
        blockers.append("THREE_HOLE_RUNTIME_TECHNICAL_ACCEPTANCE_MISSING")
    if not automated_collision_contract:
        blockers.append("AUTOMATED_COLLISION_AUTHORITY_CONTRACT_INCOMPLETE")
    if not collision_visual_approval:
        blockers.append("THREE_HOLE_COLLISION_VISUAL_PRODUCT_APPROVAL_MISSING")
    if not visual_approval:
        blockers.append("THREE_HOLE_VISUAL_QUALITY_OWNER_APPROVAL_MISSING")
    if not performance_capture_pass:
        blockers.append("CANDIDATE_BOUND_RENDERED_PERFORMANCE_CAPTURE_MISSING")
    if not production_performance_acceptance:
        blockers.append("PRODUCTION_PERFORMANCE_PROFILE_AND_ACCEPTANCE_MISSING")
    if not soak_acceptance:
        blockers.append("THERMAL_SOAK_ACCEPTANCE_MISSING")

    # This read-only audit cannot close Session 13 or promote a release.  Even a
    # technically complete future input set requires human/legal/owner authority.
    release_ready = False
    if issues:
        state = "FAIL_INVALID_PCG_READINESS_AUDIT_INPUT"
    elif release_ready:
        state = "PASS_PCG_PRODUCTION_READINESS"
    else:
        state = "PASS_READ_ONLY_PCG_READINESS_AUDIT_BLOCKERS_REMAIN"

    return {
        "schema": "DiscGolfTour.Session19PcgReleaseReadinessAudit.v1",
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "state": state,
        "readOnly": True,
        "strategy": {
            "activeShippingStrategy": active_strategy,
            "shippingGraphExcluded": graph_excluded,
            "releaseContractApprovedStrategy": contract_approved_strategy,
            "releaseContractRequiresBakeSaveReopen": contract_requires_bake,
            "legacyReleaseContractReconciledByOperationalProof": (
                contract_requires_bake and operational_proof_valid
            ),
            "bakeSaveReopenDisposition": (
                "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY"
                if operational_proof_valid
                else "NOT_APPLICABLE_TO_ACTIVE_SHIPPING_STRATEGY_BUT_STILL_REQUIRED_BY_RELEASE_CONTRACT"
                if graph_excluded and contract_requires_bake
                else "ACTIVE_STRATEGY_AND_RELEASE_CONTRACT_ALIGNED"
                if strategy_aligned
                else "UNRESOLVED"
            ),
            "aligned": strategy_aligned,
        },
        "technicalGates": {
            "authoredTreeSourceAuthorityCandidateBound": source_authority_bound,
            "freshShippingPcgAbsence": shipping_pcg_absence,
            "shippingRuntimeAuthoredJsonSelection": runtime_authored_selection,
            "operationalAuthoredJsonProof": operational_proof_valid,
            "operationalAuthoredJsonProofBinding": operational_binding,
            "threeHoleRuntimeTechnicalAcceptance": three_hole_technical,
            "automatedCollisionAuthorityContract": automated_collision_contract,
            "missingCollisionAutomationTests": missing_collision_tests,
            "candidateBoundRenderedPerformanceCapture": performance_capture_pass,
        },
        "productionGates": {
            "collisionVisualProductApproval": collision_visual_approval,
            "visualQualityOwnerApproval": visual_approval,
            "performanceProfileReady": performance_profile_ready,
            "performanceMetadataGaps": performance_metadata_gaps,
            "productionPerformanceAcceptance": production_performance_acceptance,
            "thermalSoakAcceptance": soak_acceptance,
            "humanGameplayApproval": False,
            "legalApproval": False,
            "session13BlockerClosed": False,
        },
        "issues": issues,
        "remainingBlockers": blockers,
        "session13BlockerClosed": False,
        "releaseReady": release_ready,
        "nextAutomatableStep": (
            "Add a candidate-bound thermal-soak capture that records GPU driver, power mode, "
            "wall-power state, and GPU-thread profiling. Human collision/visual, gameplay, legal, "
            "owner, Session 13 closure, and release approvals remain manual."
        ),
    }


def _fixture_documents() -> dict[str, Any]:
    payload = {
        "treeCount": 44,
        "bytes": 2982,
        "sha256": "98D9B1F10A683D1C90EC25F766A3224B00AC550F9C379F2051535F9B82638BF6",
    }
    candidate = "S19_WindowsShipping_20260827T023919Z_51a1c132f837"
    executable = {
        "path": "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
        "bytes": 162632192,
        "sha256": "9A7AB59B8B055335D249F4C7537EF047F6AADB687DEE20BD6D427C33CECADEF9",
    }
    archive_sha256 = "5F8BB2E5885358CA3CBAD310AC0A4EC024196B4E80811AE8E748685E4DB50231"
    closure_sha256 = "6A98727318AEE21E455ADD57AC2FAA45B5071A32C6FE9FB6EC66310FC224F24C"
    external_run = (
        "S19_WindowsShipping_20260827T023919Z_51a1c132f837_"
        "de633085-9a16-4b8d-ae18-e226c48735d9"
    )
    user_dir = "de633085-9a16-4b8d-ae18-e226c48735d9"
    launch_sha256 = "B49509A4A6EA40ABF5D324340AC480FACD6E8038CE900EECC8011FEDBCA01721"
    manifest_sha256 = "F059B6A860C85BE80172D2ADA808D867FB0F3BF4685C3D780F46217082D1D35E"
    journal = {
        "schema": "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1",
        "bytes": 3231,
        "sha256": "3A88BC7D343BCD21535E2336BC1AF53467AACBAFA357EA57ECB120AE0F3876D6",
        "captureNonce": "1bfc3241-8290-4976-b234-4f13fd0087b0",
        "roundId": "d90adf9a-4e65-467a-ae4d-f884a6960fe6",
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
        "completedHoles": 3,
        "totalHoles": 3,
        "runtimeOperationalBindingValidated": True,
    }
    retirement_state = (
        "RETIRED_RUNTIME_CLOSURE_DELEGATED_TO_EDITOR_AUTHORING_AND_SHIPPING_SEPARATION"
    )
    separation_state = "SOURCE_CONFIG_SEPARATION_PASS_FRESH_SHIPPING_COOK_PROOF_PENDING"
    return {
        "candidate_id": candidate,
        "baked_policy": {
            "schema": "DiscGolfTour.Session19BakedPcgPolicy.v3",
            "schemaVersion": 3,
            "combinedCanonicalPayload": copy.deepcopy(payload),
        },
        "runtime_closure_policy": {
            "schema": "DiscGolfTour.Session19BakedPcgRuntimeClosureRetirementPolicy.v2",
            "schemaVersion": 2,
            "state": retirement_state,
            "strategy": SEPARATION_STRATEGY,
            "retiredCandidateEvidence": {"candidateCookProofDelegatedToShippingSeparation": True},
            "claimBoundary": {"legacyCandidateRuntimeClosureRetired": True},
        },
        "separation_policy": {
            "schema": "DiscGolfTour.Session19ShippingPcgSeparationPolicy.v1",
            "schemaVersion": 1,
            "state": separation_state,
            "strategy": "SHIP_EXACT_AUTHORED_JSON_44_TREE_RUNTIME_OUTPUT_AND_EXCLUDE_ALL_PCG_AUTHORING_CODE_AND_GRAPH_CONTENT",
            "runtimeAuthority": copy.deepcopy(payload),
        },
        "candidate_receipt": {
            "schema": "DiscGolfTour.Session19BakedPcgCandidateVerification.v1",
            "schemaVersion": 1,
            "candidateId": candidate,
            "archive": {
                "fileCount": 31,
                "bytes": 1246026263,
                "canonicalManifestSha256": archive_sha256,
            },
            "shippingExecutable": copy.deepcopy(executable),
            "sourceClosure": {
                "preBuildReceipt": {"preBuildSha256": closure_sha256},
                "postBuildSha256": closure_sha256,
                "prePostMatch": True,
            },
            "technicalBoundary": {
                "candidateBoundAuthoredDataPass": True,
                "runtimeAuthoredSelectionProven": False,
            },
        },
        "separation_receipt": {
            "schema": "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1",
            "schemaVersion": 1,
            "candidateId": candidate,
            "source": {"authoredTreePayload": copy.deepcopy(payload)},
            "cook": {
                "freshShippingCookAbsenceProven": True,
                "projectPcgGraphEntryCount": 0,
                "assetRegistryProjectPcgGraphCount": 0,
                "serializedProjectPcgMarkerFileCount": 0,
                "containerIdentity": {
                    "shippingBinary": copy.deepcopy(executable)
                },
            },
            "claimBoundary": {
                "sourceConfigSeparationProven": True,
                "freshShippingCookAbsenceProven": True,
                "collisionVisualApproval": False,
                "performanceAcceptance": False,
                "soakAcceptance": False,
            },
        },
        "three_hole_receipt": {
            "schema": "DiscGolfTour.Session19ThreeHoleTechnicalAcceptanceReceipt.v2",
            "schemaVersion": 2,
            "candidateId": candidate,
            "candidate": {
                "archive": {
                    "fileCount": 31,
                    "bytes": 1246026263,
                    "canonicalManifestSha256": archive_sha256,
                },
                "shippingExecutable": copy.deepcopy(executable),
            },
            "technicalAcceptance": True,
            "externalEvidence": {
                "recoveryLocationToken": (
                    "DGTOUR_ACCEPTANCE/Session19ExternalEvidence/" + external_run
                )
            },
            "freshUserDir": {"token": user_dir},
            "runtimeJournal": {
                **copy.deepcopy(journal),
                "launchRecordSha256": launch_sha256,
                "technicalEvidenceManifestSha256": manifest_sha256,
            },
            "continuityClaims": {
                "singleRoundProven": True,
                "threeHoleCompletionObserved": True,
            },
            "claimBoundary": {
                "visualQualityApproval": False,
                "productOwnerApproval": False,
                "performanceAcceptance": False,
                "soakAcceptance": False,
            },
        },
        "performance_receipt": {
            "schema": "DiscGolfTour.Session19ShippingPerformanceEvidence.v1",
            "schemaVersion": 1,
            "candidateId": candidate,
            "archive": {"shippingExecutable": copy.deepcopy(executable)},
            "passed": True,
            "state": "PASS_CANDIDATE_BOUND_THREE_HOLE_RENDERED_PERFORMANCE",
            "claimBoundary": {
                "gpuDriverRecorded": False,
                "gpuThreadProfiling": False,
                "powerModeRecorded": False,
                "wallPowerRecorded": False,
                "thermalSoak": False,
            },
            "runs": [
                {"holeNumber": hole, "summary": {"sample_count": 600, "hitch_count": 0}}
                for hole in (1, 2, 3)
            ],
        },
        "release_contract": {
            "schema": "DiscGolfTour.Session19ReleaseScopeContract.v3",
            "schemaVersion": 3,
            "closureStrategies": [{
                "blockerId": PCG_BLOCKER,
                "approvedStrategy": RELEASE_BAKE_STRATEGY,
            }]
        },
        "automation_log_text": f"-abslog=Automation_AllDiscGolfTour-{candidate}.log\n" + "\n".join(
            f"Test Completed. Result={{Success}} Path={{{path}}}"
            for path in REQUIRED_COLLISION_TESTS
        ),
        "operational_policy": {
            "schema": "DiscGolfTour.Session19PcgOperationalRuntimeProofPolicy.v1",
            "schemaVersion": 1,
            "session": 19,
            "policyId": "s19_candidate_authored_json_operational_runtime_proof_v1",
            "authority": "CANDIDATE_BOUND_TECHNICAL_INFERENCE_NOT_CODE_TO_BINARY_ATTESTATION_OR_RELEASE_APPROVAL",
            "state": "CANDIDATE_BOUND_POLICY_PENDING_VALIDATION",
            "candidate": {
                "candidateId": candidate,
                "platform": "Windows",
                "configuration": "Shipping",
                "archiveFileCount": 31,
                "archiveBytes": 1246026263,
                "archiveCanonicalManifestSha256": archive_sha256,
                "shippingExecutable": copy.deepcopy(executable),
                "externalRunToken": external_run,
                "userDirToken": user_dir,
            },
            "strategyReconciliation": {
                "retirementPolicy": {
                    "schema": "DiscGolfTour.Session19BakedPcgRuntimeClosureRetirementPolicy.v2",
                    "schemaVersion": 2,
                    "state": retirement_state,
                    "strategy": SEPARATION_STRATEGY,
                },
                "shippingSeparationPolicy": {
                    "schema": "DiscGolfTour.Session19ShippingPcgSeparationPolicy.v1",
                    "schemaVersion": 1,
                    "state": separation_state,
                    "strategy": "SHIP_EXACT_AUTHORED_JSON_44_TREE_RUNTIME_OUTPUT_AND_EXCLUDE_ALL_PCG_AUTHORING_CODE_AND_GRAPH_CONTENT",
                },
                "deterministicUnrealBakeSaveReopen": {
                    "disposition": "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY",
                    "mayBeReportedAsPending": False,
                    "mayBeReportedAsProven": False,
                },
            },
            "shippingLoader": {"shippingSourceFallbackAllowed": False},
            "archiveAuthoredJson": {"combinedTreePayload": copy.deepcopy(payload)},
            "operationalRuntime": {},
            "evidence": {
                "validator": "Scripts/validate_dg_session19_pcg_operational_runtime_proof.py",
                "receiptPattern": "Evidence/Session19/PcgOperationalRuntimeProof-{candidateId}.json",
                "passState": OPERATIONAL_PASS_STATE,
                "immutableCreateNewOutput": True,
            },
        },
        "operational_receipt": {
            "schema": "DiscGolfTour.Session19PcgOperationalRuntimeProofReceipt.v1",
            "schemaVersion": 1,
            "session": 19,
            "candidateId": candidate,
            "state": OPERATIONAL_PASS_STATE,
            "authority": "CANDIDATE_BOUND_TECHNICAL_INFERENCE_NOT_CODE_TO_BINARY_ATTESTATION_OR_RELEASE_APPROVAL",
            "strategyReconciliation": {
                "runtimeClosureRetired": True,
                "shippingStrategy": SEPARATION_STRATEGY,
                "shippingSeparationStrategy": "SHIP_EXACT_AUTHORED_JSON_44_TREE_RUNTIME_OUTPUT_AND_EXCLUDE_ALL_PCG_AUTHORING_CODE_AND_GRAPH_CONTENT",
                "bakeSaveReopenDisposition": "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY",
                "bakeSaveReopenApplicable": False,
                "bakeSaveReopenPending": False,
                "bakeSaveReopenProven": False,
            },
            "sourceClosureAndLoader": {
                "preBuildClosureSha256": closure_sha256,
                "postBuildClosureSha256": closure_sha256,
                "prePostMatch": True,
                "failClosedShippingLoaderSourceIdentity": True,
                "shippingFallbackAllowed": False,
                "authoredDataLoadPolicyAutomationPass": True,
            },
            "cleanShippingBuild": {
                "clean": True,
                "iterativeCook": False,
                "uatExitCode": 0,
                "verificationMode": "DIRECT_BUILD_PASS",
                "archiveVerified": True,
            },
            "archiveAuthoredJson": {
                "fileCount": 31,
                "bytes": 1246026263,
                "canonicalManifestSha256": archive_sha256,
                "shippingExecutable": copy.deepcopy(executable),
                "authoredJsonFileCount": 4,
                "allExactSourceMatches": True,
                "allNonUfsExactlyOnce": True,
                "allUfsAbsent": True,
                "combinedTreePayload": copy.deepcopy(payload),
            },
            "shippingPcgAbsence": {
                "freshShippingCookAbsenceProven": True,
                "projectPcgGraphEntryCount": 0,
                "pcgModuleEntryCount": 0,
                "archivePcgModuleFileCount": 0,
                "assetRegistryProjectPcgGraphCount": 0,
                "serializedProjectPcgMarkerFileCount": 0,
                "shippingBinaryProjectPcgMarkerFileCount": 0,
            },
            "operationalRuntime": {
                "externalRunToken": external_run,
                "userDirToken": user_dir,
                "launchRecordSha256": launch_sha256,
                "technicalEvidenceManifestSha256": manifest_sha256,
                "runtimeJournal": copy.deepcopy(journal),
                "singleRoundProven": True,
                "threeHoleCompletionObserved": True,
            },
            "technicalConclusion": {
                "boundedOperationalRuntimeAuthoredJsonSelectionProven": True,
                "cryptographicCodeToBinaryAttestation": False,
            },
            "claimBoundary": {
                "boundedOperationalRuntimeAuthoredJsonSelectionProven": True,
                "cryptographicCodeToBinaryAttestation": False,
                "unrealPcgBakeSaveReopenApplicable": False,
                "unrealPcgBakeSaveReopenProven": False,
                "visualApproval": False,
                "collisionApproval": False,
                "performanceAcceptance": False,
                "soakAcceptance": False,
                "manualGameplayAcceptance": False,
                "humanPlayFeelApproval": False,
                "provenanceApproval": False,
                "legalApproval": False,
                "distributionClearance": False,
                "ownerApproval": False,
                "releaseApproval": False,
                "session13BlockerClosed": False,
                "releaseReady": False,
            },
            "session13BlockerClosed": False,
            "releaseReady": False,
            "receiptPath": f"Evidence/Session19/PcgOperationalRuntimeProof-{candidate}.json",
        },
        "operational_artifacts": copy.deepcopy(OPERATIONAL_PROOF_AUTHORITIES[candidate]),
    }


def run_self_test() -> int:
    fixture = _fixture_documents()
    result = evaluate_documents(**fixture)
    assertions = 0

    def check(condition: bool, label: str) -> None:
        nonlocal assertions
        assertions += 1
        if not condition:
            raise AssertionError(label)

    expected_manual_blockers = [
        "THREE_HOLE_COLLISION_VISUAL_PRODUCT_APPROVAL_MISSING",
        "THREE_HOLE_VISUAL_QUALITY_OWNER_APPROVAL_MISSING",
        "PRODUCTION_PERFORMANCE_PROFILE_AND_ACCEPTANCE_MISSING",
        "THERMAL_SOAK_ACCEPTANCE_MISSING",
    ]
    check(result["issues"] == [], "valid operational proof must not create audit issues")
    check(
        result["remainingBlockers"] == expected_manual_blockers,
        "valid proof must remove only strategy conflict and runtime selection blockers",
    )
    check(
        "PCG_SHIPPING_STRATEGY_AND_RELEASE_CONTRACT_CONFLICT"
        not in result["remainingBlockers"],
        "valid proof must reconcile the legacy strategy conflict",
    )
    check(
        "SHIPPING_RUNTIME_AUTHORED_JSON_SELECTION_NOT_PROVEN"
        not in result["remainingBlockers"],
        "valid proof must advance bounded runtime authored-JSON selection",
    )
    check(
        result["technicalGates"]["operationalAuthoredJsonProof"] is True
        and result["technicalGates"]["shippingRuntimeAuthoredJsonSelection"] is True,
        "valid proof must advance both bounded technical gates",
    )
    check(result["productionGates"]["collisionVisualProductApproval"] is False,
          "collision/visual approval must remain false")
    check(result["productionGates"]["visualQualityOwnerApproval"] is False,
          "visual/owner approval must remain false")
    check(result["productionGates"]["productionPerformanceAcceptance"] is False,
          "performance approval must remain false")
    check(result["productionGates"]["thermalSoakAcceptance"] is False,
          "soak approval must remain false")
    check(result["productionGates"]["humanGameplayApproval"] is False,
          "human approval must remain false")
    check(result["productionGates"]["legalApproval"] is False,
          "legal approval must remain false")
    check(result["session13BlockerClosed"] is False,
          "Session 13 blocker closure must remain false")
    check(result["releaseReady"] is False, "read-only audit must not promote release readiness")

    mutations: tuple[tuple[str, Any, str], ...] = (
        (
            "proof-missing",
            lambda f: (
                f.__setitem__("operational_policy", None),
                f.__setitem__("operational_receipt", None),
                f.__setitem__("operational_artifacts", None),
            ),
            "PCG_SHIPPING_STRATEGY_AND_RELEASE_CONTRACT_CONFLICT",
        ),
        (
            "proof-candidate-mismatch",
            lambda f: f["operational_receipt"].__setitem__("candidateId", "wrong"),
            "operational proof receipt identity or candidate binding differs",
        ),
        (
            "policy-file-hash",
            lambda f: f["operational_artifacts"]["policy"].__setitem__("sha256", "0" * 64),
            "policy, validator, or receipt file identity differs",
        ),
        (
            "validator-file-hash",
            lambda f: f["operational_artifacts"]["validator"].__setitem__("sha256", "0" * 64),
            "policy, validator, or receipt file identity differs",
        ),
        (
            "receipt-file-hash",
            lambda f: f["operational_artifacts"]["receipt"].__setitem__("sha256", "0" * 64),
            "policy, validator, or receipt file identity differs",
        ),
        (
            "proof-archive-hash",
            lambda f: f["operational_receipt"]["archiveAuthoredJson"].__setitem__(
                "canonicalManifestSha256", "0" * 64
            ),
            "archive/authored-JSON binding differs",
        ),
        (
            "proof-missing-archived-json",
            lambda f: f["operational_receipt"]["archiveAuthoredJson"].__setitem__(
                "authoredJsonFileCount", 3
            ),
            "archive/authored-JSON binding differs",
        ),
        (
            "proof-altered-source-closure",
            lambda f: f["operational_receipt"]["sourceClosureAndLoader"].__setitem__(
                "postBuildClosureSha256", "0" * 64
            ),
            "source-closure/fail-closed loader binding differs",
        ),
        (
            "proof-run-token",
            lambda f: f["operational_receipt"]["operationalRuntime"].__setitem__(
                "externalRunToken", "wrong"
            ),
            "external-run/UserDir/journal binding differs",
        ),
        (
            "proof-incomplete-runtime-journal",
            lambda f: f["operational_receipt"]["operationalRuntime"][
                "runtimeJournal"
            ].__setitem__("eventCount", 11),
            "external-run/UserDir/journal binding differs",
        ),
        (
            "proof-fallback-allowed",
            lambda f: f["operational_receipt"]["sourceClosureAndLoader"].__setitem__(
                "shippingFallbackAllowed", True
            ),
            "source-closure/fail-closed loader binding differs",
        ),
        (
            "proof-bake-applicable",
            lambda f: f["operational_receipt"]["strategyReconciliation"].__setitem__(
                "bakeSaveReopenApplicable", True
            ),
            "receipt strategy reconciliation differs",
        ),
        (
            "proof-crypto-overclaim",
            lambda f: f["operational_receipt"]["claimBoundary"].__setitem__(
                "cryptographicCodeToBinaryAttestation", True
            ),
            "overclaims cryptographicCodeToBinaryAttestation",
        ),
        (
            "proof-legal-overclaim",
            lambda f: f["operational_receipt"]["claimBoundary"].__setitem__(
                "legalApproval", True
            ),
            "overclaims legalApproval",
        ),
        (
            "proof-release-overclaim",
            lambda f: f["operational_receipt"]["claimBoundary"].__setitem__(
                "releaseReady", True
            ),
            "overclaims releaseReady",
        ),
        (
            "source-payload-mismatch",
            lambda f: f["separation_receipt"]["source"]["authoredTreePayload"].__setitem__(
                "treeCount", 43
            ),
            "authored-tree payload differs",
        ),
        (
            "collision-test",
            lambda f: f.__setitem__("automation_log_text", ""),
            "AUTOMATED_COLLISION_AUTHORITY_CONTRACT_INCOMPLETE",
        ),
        (
            "shipping-pcg-present",
            lambda f: f["separation_receipt"]["cook"].__setitem__(
                "projectPcgGraphEntryCount", 1
            ),
            "FRESH_SHIPPING_PCG_ABSENCE_NOT_PROVEN",
        ),
    )
    for name, mutate, expected in mutations:
        mutated = copy.deepcopy(fixture)
        mutate(mutated)
        observed = evaluate_documents(**mutated)
        haystack = json.dumps(observed, sort_keys=True)
        check(expected in haystack, f"mutation {name} was not detected")

    print(f"PASS self-test assertions={assertions} mutations={len(mutations)}")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--candidate-id")
    parser.add_argument("--performance-receipt", type=Path)
    parser.add_argument("--automation-log", type=Path)
    parser.add_argument("--operational-runtime-receipt", type=Path)
    parser.add_argument("--operational-runtime-policy", type=Path)
    parser.add_argument("--operational-runtime-validator", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--require-ready", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.self_test:
        return run_self_test()
    if not args.candidate_id or not CANDIDATE_RE.fullmatch(args.candidate_id):
        print("ERROR --candidate-id must match an S19 Windows Shipping candidate", file=sys.stderr)
        return 1

    root = args.root.resolve()
    candidate_id = args.candidate_id
    candidate_contract = root / "Config" / f"DG_Session19ReleaseScopeContract-{candidate_id}.json"
    release_contract_path = (
        candidate_contract
        if candidate_contract.is_file()
        else root / "Config" / "DG_Session19ReleaseScopeContract.json"
    )
    performance_path = args.performance_receipt
    if performance_path is None:
        performance_path = Path(
            "C:/DGTourExternal/ShippingPerformance/Receipts/"
            f"ShippingPerformanceEvidence-{candidate_id}.json"
        )
    automation_path = args.automation_log
    if automation_path is None:
        automation_path = root / "Saved" / "Logs" / f"Automation_AllDiscGolfTour-{candidate_id}.log"
    operational_receipt_path = args.operational_runtime_receipt
    if operational_receipt_path is None:
        operational_receipt_path = (
            root / "Evidence" / "Session19"
            / f"PcgOperationalRuntimeProof-{candidate_id}.json"
        )
    operational_policy_path = args.operational_runtime_policy
    if operational_policy_path is None:
        operational_policy_path = (
            root / "Config"
            / f"DG_Session19PcgOperationalRuntimeProofPolicy-{candidate_id}.json"
        )
    operational_validator_path = args.operational_runtime_validator
    if operational_validator_path is None:
        operational_validator_path = (
            root / "Scripts" / "validate_dg_session19_pcg_operational_runtime_proof.py"
        )

    paths = {
        "baked_policy": root / "Config" / "DG_Session19BakedPcgPolicy.json",
        "runtime_closure_policy": root / "Config" / "DG_Session19BakedPcgRuntimeClosurePolicy.json",
        "separation_policy": root / "Config" / "DG_Session19ShippingPcgSeparationPolicy.json",
        "candidate_receipt": root / "Evidence" / "Session19" / f"BakedPcgCandidateVerification-{candidate_id}.json",
        "separation_receipt": root / "Evidence" / "Session19" / f"ShippingPcgSeparation-{candidate_id}.json",
        "three_hole_receipt": root / "Evidence" / "Session19" / f"ThreeHoleTechnicalAcceptance-{candidate_id}.json",
        "release_contract": release_contract_path,
    }
    try:
        documents = {key: load_json_object(path) for key, path in paths.items()}
        performance = load_json_object(performance_path) if performance_path.is_file() else None
        automation_log_text = automation_path.read_text(encoding="utf-8", errors="strict")
        operational_policy: dict[str, Any] | None = None
        operational_receipt: dict[str, Any] | None = None
        operational_artifacts: dict[str, dict[str, Any]] | None = None
        if operational_receipt_path.is_file():
            operational_policy, policy_identity = load_json_object_with_identity(
                operational_policy_path, root
            )
            operational_receipt, receipt_identity = load_json_object_with_identity(
                operational_receipt_path, root
            )
            validator_identity = load_file_identity(operational_validator_path, root)
            operational_artifacts = {
                "policy": policy_identity,
                "validator": validator_identity,
                "receipt": receipt_identity,
            }
        elif args.operational_runtime_receipt is not None:
            raise AuditInputError(
                f"explicit operational runtime receipt is missing: {operational_receipt_path}"
            )
        result = evaluate_documents(
            candidate_id=candidate_id,
            performance_receipt=performance,
            automation_log_text=automation_log_text,
            operational_policy=operational_policy,
            operational_receipt=operational_receipt,
            operational_artifacts=operational_artifacts,
            **documents,
        )
    except (AuditInputError, OSError, UnicodeError) as exc:
        print(f"ERROR {exc}", file=sys.stderr)
        return 1

    result["inputs"] = {
        key: str(path) for key, path in paths.items()
    } | {
        "performance_receipt": str(performance_path),
        "automation_log": str(automation_path),
        "operational_runtime_policy": str(operational_policy_path),
        "operational_runtime_validator": str(operational_validator_path),
        "operational_runtime_receipt": str(operational_receipt_path),
    }
    if args.as_json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(result["state"])
        print(f"candidate={candidate_id}")
        print(f"strategy={result['strategy']['bakeSaveReopenDisposition']}")
        for gate, value in result["technicalGates"].items():
            if gate != "missingCollisionAutomationTests":
                print(f"technical.{gate}={str(value).lower()}")
        for gate, value in result["productionGates"].items():
            if gate != "performanceMetadataGaps":
                print(f"production.{gate}={str(value).lower()}")
        for blocker in result["remainingBlockers"]:
            print(f"BLOCKER {blocker}")
        for issue in result["issues"]:
            print(f"ERROR {issue}")

    if result["issues"]:
        return 1
    if args.require_ready and not result["releaseReady"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
