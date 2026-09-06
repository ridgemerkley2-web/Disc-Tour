#!/usr/bin/env python3
"""Validate additive candidate-bound authored-JSON PCG runtime proof.

This gate reconciles the retired runtime-PCG closure with the active
authored-JSON/Editor-only strategy.  It may prove only a bounded operational
inference: a clean, source-closure-bound Shipping candidate that fails closed
without authored data, contains the exact authored JSON, excludes project PCG
runtime content, and completed all three holes in one game-emitted journal.

It deliberately does not claim cryptographic code-to-binary attestation,
visual/collision approval, performance or soak acceptance, human judgment,
provenance/legal clearance, blocker closure, or release readiness.
"""

from __future__ import annotations

import argparse
import copy
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_ID = "S19_WindowsShipping_20260827T023919Z_51a1c132f837"
POLICY_RELATIVE = (
    "Config/DG_Session19PcgOperationalRuntimeProofPolicy-"
    f"{CANDIDATE_ID}.json"
)
POLICY_SHA256 = "E6CEEE0EE777966C5D3133B8BEA4BD0204F5E8CD38F3BC588D11FF2621975D4E"
PASS_STATE = "PASS_CANDIDATE_BOUND_AUTHORED_JSON_OPERATIONAL_RUNTIME_TECHNICAL_PROOF"
FAIL_STATE = "FAIL_CANDIDATE_BOUND_AUTHORED_JSON_OPERATIONAL_RUNTIME_TECHNICAL_PROOF"
RECEIPT_SCHEMA = "DiscGolfTour.Session19PcgOperationalRuntimeProofReceipt.v1"
EXPECTED_EVENTS = (
    "ROUND_STARTED",
    "HOLE_1_TEE",
    "HOLE_1_LIE",
    "HOLE_1_COMPLETED",
    "HOLE_2_TEE",
    "HOLE_2_LIE",
    "HOLE_2_COMPLETED",
    "HOLE_3_TEE",
    "HOLE_3_LIE",
    "HOLE_3_COMPLETED",
    "ROUND_COMPLETED",
    "FINAL_SCORECARD",
)
DIGEST_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]+$")
UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-"
    r"[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
)


class ContractError(ValueError):
    """An input differs from the candidate-bound technical contract."""


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _canonical_json(value: Any) -> bytes:
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def _no_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise ContractError(f"non-finite JSON constant is forbidden: {value}")


def _strict_json(data: bytes, label: str) -> dict[str, Any]:
    try:
        value = json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=_no_duplicate_keys,
            parse_constant=_reject_constant,
        )
    except (UnicodeDecodeError, json.JSONDecodeError, ContractError) as exc:
        raise ContractError(f"invalid JSON object {label}: {exc}") from exc
    if type(value) is not dict:
        raise ContractError(f"JSON root must be an object: {label}")
    return value


def _expect(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def _digest(value: Any, label: str) -> str:
    _expect(type(value) is str and DIGEST_RE.fullmatch(value) is not None,
            f"{label} is not an uppercase SHA-256")
    return value


def _positive_int(value: Any, label: str) -> int:
    _expect(type(value) is int and value > 0, f"{label} must be a positive integer")
    return value


def _canonical_relative(value: Any, label: str) -> str:
    _expect(type(value) is str, f"{label} must be a string")
    normalized = value.replace("\\", "/")
    _expect(
        bool(normalized)
        and not normalized.startswith("/")
        and re.match(r"^[A-Za-z]:", normalized) is None
        and all(part not in {"", ".", ".."} for part in normalized.split("/")),
        f"{label} is not a canonical relative path",
    )
    return normalized


def _identity(data: bytes) -> dict[str, Any]:
    return {"bytes": len(data), "sha256": _sha256(data)}


def _require_identity(data: bytes, expected: dict[str, Any], label: str) -> None:
    actual = _identity(data)
    wanted = {"bytes": expected.get("bytes"), "sha256": expected.get("sha256")}
    _expect(actual == wanted, f"{label} identity differs")


def _is_reparse(path: Path) -> bool:
    attributes = getattr(path.lstat(), "st_file_attributes", 0)
    return bool(attributes & getattr(os.stat_result, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def _require_regular_within(path: Path, boundary: Path, label: str) -> None:
    _expect(boundary.is_dir() and not boundary.is_symlink() and not _is_reparse(boundary),
            f"{label} boundary is missing or irregular")
    _expect(path.is_file() and not path.is_symlink() and not _is_reparse(path),
            f"{label} is missing or irregular")
    resolved_boundary = boundary.resolve(strict=True)
    resolved = path.resolve(strict=True)
    _expect(resolved_boundary in resolved.parents, f"{label} resolves outside its boundary")


def _read_stable(path: Path, boundary: Path, label: str) -> bytes:
    _require_regular_within(path, boundary, label)
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    _expect(
        before.st_size == after.st_size
        and before.st_mtime_ns == after.st_mtime_ns
        and len(data) == after.st_size,
        f"{label} changed while read",
    )
    return data


def _hash_stable(path: Path, boundary: Path, label: str) -> dict[str, Any]:
    _require_regular_within(path, boundary, label)
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    _expect(
        before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns,
        f"{label} changed while hashed",
    )
    return {"bytes": after.st_size, "sha256": digest.hexdigest().upper()}


def _nested(document: dict[str, Any], *keys: str) -> Any:
    current: Any = document
    for key in keys:
        if type(current) is not dict or key not in current:
            raise ContractError("missing JSON field: " + ".".join(keys))
        current = current[key]
    return current


@dataclass
class ArchiveSnapshot:
    file_count: int
    total_bytes: int
    canonical_manifest_sha256: str
    records: list[dict[str, Any]]
    payloads: dict[str, bytes]
    complete_inventory: bool


@dataclass
class InputBundle:
    project: dict[str, bytes]
    archive: ArchiveSnapshot
    external: dict[str, bytes]
    user: dict[str, bytes]


def _policy_project_specs(policy: dict[str, Any]) -> list[dict[str, Any]]:
    specs: list[dict[str, Any]] = [
        policy["strategyReconciliation"]["retirementPolicy"],
        policy["strategyReconciliation"]["shippingSeparationPolicy"],
        policy["sourceClosure"]["preBuildReceipt"],
        policy["sourceClosure"]["candidateVerification"],
        *policy["sourceClosure"]["loaderSourceFiles"],
        policy["shippingLoader"]["automation"],
        policy["cleanShippingBuild"]["buildReceipt"],
        policy["cleanShippingBuild"]["verificationReceipt"],
        policy["cleanShippingBuild"]["buildLog"],
        policy["shippingPcgAbsence"]["receipt"],
        policy["operationalRuntime"]["externalTechnicalReceipt"],
        policy["operationalRuntime"]["freshUserDirReceipt"],
        policy["operationalRuntime"]["threeHoleReceipt"],
        *(
            {
                "path": item["sourcePath"],
                "bytes": item["bytes"],
                "sha256": item["sha256"],
            }
            for item in policy["archiveAuthoredJson"]["files"]
        ),
    ]
    seen: set[str] = set()
    for spec in specs:
        path = _canonical_relative(spec.get("path"), "project evidence path")
        _positive_int(spec.get("bytes"), f"{path} bytes")
        _digest(spec.get("sha256"), f"{path} hash")
        _expect(path.casefold() not in seen, f"duplicate project evidence path: {path}")
        seen.add(path.casefold())
    return specs


def _validate_policy_semantics(policy: dict[str, Any]) -> None:
    _expect(
        policy.get("schema") == "DiscGolfTour.Session19PcgOperationalRuntimeProofPolicy.v1"
        and policy.get("schemaVersion") == 1
        and policy.get("session") == 19
        and policy.get("policyId") == "s19_candidate_authored_json_operational_runtime_proof_v1"
        and policy.get("authority")
        == "CANDIDATE_BOUND_TECHNICAL_INFERENCE_NOT_CODE_TO_BINARY_ATTESTATION_OR_RELEASE_APPROVAL"
        and policy.get("state") == "CANDIDATE_BOUND_POLICY_PENDING_VALIDATION",
        "policy identity differs",
    )
    candidate = policy.get("candidate")
    _expect(
        type(candidate) is dict
        and candidate.get("candidateId") == CANDIDATE_ID
        and candidate.get("platform") == "Windows"
        and candidate.get("configuration") == "Shipping"
        and candidate.get("archiveFileCount") == 31
        and candidate.get("archiveBytes") == 1246026263
        and candidate.get("archiveCanonicalManifestSha256")
        == "5F8BB2E5885358CA3CBAD310AC0A4EC024196B4E80811AE8E748685E4DB50231",
        "policy candidate identity differs",
    )
    disposition = _nested(
        policy, "strategyReconciliation", "deterministicUnrealBakeSaveReopen"
    )
    _expect(
        disposition == {
            "disposition": "NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY",
            "reason": (
                "Shipping consumes exact authored JSON tree arrays and excludes project PCG "
                "authoring code and graph content; no generated Unreal PCG output is a Shipping "
                "runtime authority to bake, save, or reopen."
            ),
            "mayBeReportedAsPending": False,
            "mayBeReportedAsProven": False,
        },
        "bake/save/reopen disposition differs from strategy-inapplicable",
    )
    _expect(
        _nested(policy, "shippingLoader", "shippingSourceFallbackAllowed") is False,
        "policy allows a Shipping source fallback",
    )
    claim = policy.get("claimBoundary")
    _expect(type(claim) is dict, "policy claim boundary is missing")
    _expect(
        claim.get("boundedOperationalRuntimeAuthoredJsonSelectionMayBeProven") is True,
        "policy does not permit the bounded technical proof",
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
        _expect(claim.get(key) is False, f"policy overclaims {key}")
    _expect(policy.get("session13BlockerClosed") is False, "policy closes Session 13")
    _expect(policy.get("releaseReady") is False, "policy claims release readiness")
    _expect(tuple(_nested(policy, "operationalRuntime", "runtimeJournal", "requiredEvents"))
            == EXPECTED_EVENTS, "policy runtime event sequence differs")
    _expect(
        _nested(policy, "evidence", "passState") == PASS_STATE
        and _nested(policy, "evidence", "immutableCreateNewOutput") is True,
        "policy evidence contract differs",
    )
    _policy_project_specs(policy)


def _load_policy(path: Path, *, enforce_hash: bool = True) -> tuple[dict[str, Any], bytes]:
    data = _read_stable(path, ROOT, "candidate PCG operational policy")
    if enforce_hash:
        _expect(_sha256(data) == POLICY_SHA256, "candidate PCG operational policy hash differs")
    policy = _strict_json(data, path.as_posix())
    _validate_policy_semantics(policy)
    return policy, data


def _collect_archive(archive: Path, payload_paths: set[str]) -> ArchiveSnapshot:
    _expect(archive.is_dir() and not archive.is_symlink() and not _is_reparse(archive),
            "archive must be a regular non-reparse directory")
    resolved_root = archive.resolve(strict=True)
    records: list[dict[str, Any]] = []
    payloads: dict[str, bytes] = {}
    seen: set[str] = set()
    for current, directories, filenames in os.walk(archive, followlinks=False):
        current_path = Path(current)
        for directory in directories:
            path = current_path / directory
            _expect(not path.is_symlink() and not _is_reparse(path),
                    f"archive contains a reparse directory: {path}")
        for filename in filenames:
            path = current_path / filename
            relative = _canonical_relative(path.relative_to(archive).as_posix(), "archive path")
            _expect(relative.casefold() not in seen,
                    f"archive contains a case-insensitive duplicate: {relative}")
            seen.add(relative.casefold())
            _expect(path.resolve(strict=True).parent == resolved_root
                    or resolved_root in path.resolve(strict=True).parents,
                    f"archive entry resolves outside archive: {relative}")
            identity = _hash_stable(path, archive, f"archive entry {relative}")
            records.append({"path": relative, **identity})
            if relative in payload_paths:
                payloads[relative] = _read_stable(path, archive, f"archive payload {relative}")
                _expect(_identity(payloads[relative]) == identity,
                        f"archive payload changed after inventory: {relative}")
    records.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    manifest = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return ArchiveSnapshot(
        file_count=len(records),
        total_bytes=sum(item["bytes"] for item in records),
        canonical_manifest_sha256=_sha256(manifest),
        records=records,
        payloads=payloads,
        complete_inventory=True,
    )


def _load_bundle(
    policy: dict[str, Any], archive: Path, external_run: Path, user_dir: Path,
    *, full_archive: bool,
) -> InputBundle:
    project: dict[str, bytes] = {}
    for spec in _policy_project_specs(policy):
        relative = spec["path"]
        project[relative] = _read_stable(ROOT / relative, ROOT, f"project input {relative}")

    payload_paths = {
        policy["archiveAuthoredJson"]["nonUfsManifest"]["path"],
        policy["archiveAuthoredJson"]["ufsManifest"]["path"],
        *(item["archivePath"] for item in policy["archiveAuthoredJson"]["files"]),
    }
    if full_archive:
        archive_snapshot = _collect_archive(archive, payload_paths)
    else:
        records: list[dict[str, Any]] = []
        payloads: dict[str, bytes] = {}
        record_specs = [
            policy["candidate"]["shippingExecutable"],
            policy["archiveAuthoredJson"]["nonUfsManifest"],
            policy["archiveAuthoredJson"]["ufsManifest"],
            *(
                {"path": item["archivePath"], "bytes": item["bytes"], "sha256": item["sha256"]}
                for item in policy["archiveAuthoredJson"]["files"]
            ),
        ]
        for spec in record_specs:
            records.append({"path": spec["path"], "bytes": spec["bytes"], "sha256": spec["sha256"]})
            if spec["path"] in payload_paths:
                payloads[spec["path"]] = _read_stable(
                    archive / spec["path"], archive, f"self-test archive payload {spec['path']}"
                )
        candidate = policy["candidate"]
        archive_snapshot = ArchiveSnapshot(
            file_count=candidate["archiveFileCount"],
            total_bytes=candidate["archiveBytes"],
            canonical_manifest_sha256=candidate["archiveCanonicalManifestSha256"],
            records=records,
            payloads=payloads,
            complete_inventory=False,
        )

    external = {
        spec["path"]: _read_stable(
            external_run / spec["path"], external_run, f"external input {spec['path']}"
        )
        for spec in (
            policy["operationalRuntime"]["launchRecord"],
            policy["operationalRuntime"]["technicalEvidenceManifest"],
        )
    }
    journal = policy["operationalRuntime"]["runtimeJournal"]
    user = {
        journal["userDirRelativePath"]: _read_stable(
            user_dir / journal["userDirRelativePath"], user_dir, "runtime journal"
        )
    }
    return InputBundle(project=project, archive=archive_snapshot, external=external, user=user)


def _project_doc(bundle: InputBundle, spec: dict[str, Any], label: str) -> dict[str, Any]:
    path = spec["path"]
    _expect(path in bundle.project, f"missing project input: {path}")
    data = bundle.project[path]
    _require_identity(data, spec, label)
    return _strict_json(data, label)


def _extract_function(text: str, signature: str) -> str:
    match = re.search(signature + r"\s*\{", text)
    _expect(match is not None, "Shipping authored-data policy function is missing")
    assert match is not None
    opening = text.find("{", match.start())
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():index + 1]
    raise ContractError("Shipping authored-data policy function is unterminated")


def _audit_loader_source(cpp: bytes, header: bytes, tests: bytes) -> None:
    try:
        cpp_text = cpp.decode("utf-8")
        header_text = header.decode("utf-8")
        test_text = tests.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ContractError("loader source is not UTF-8") from exc
    function = _extract_function(
        cpp_text,
        r"EDiscGolfAuthoredCourseLoadAction\s+DiscGolfCourseDefinition::"
        r"ResolveAuthoredCourseLoadAction\s*\([^)]*\)",
    )
    compact = re.sub(r"\s+", " ", function)
    _expect(
        re.search(
            r"if \(DataState == EDiscGolfAuthoredCourseDataState::Valid\) \{ "
            r"return EDiscGolfAuthoredCourseLoadAction::UseAuthoredData; \}", compact
        ) is not None,
        "valid authored data is not selected by the loader policy",
    )
    _expect(
        re.search(
            r"return bIsShippingBuild \? EDiscGolfAuthoredCourseLoadAction::FailClosed "
            r": EDiscGolfAuthoredCourseLoadAction::UseSourceFallback;", compact
        ) is not None,
        "Shipping loader does not fail closed ahead of the development fallback",
    )
    _expect(
        re.search(
            r"bIsShippingBuild\s*\?\s*EDiscGolfAuthoredCourseLoadAction::UseSourceFallback",
            function,
        ) is None,
        "Shipping loader permits a source fallback",
    )
    _expect(cpp_text.count("UE_BUILD_SHIPPING != 0") >= 3,
            "runtime loader calls are not guarded by the Shipping compile identity")
    _expect(cpp_text.count('OutSource = TEXT("AUTHORED JSON")') >= 2,
            "runtime loader does not retain the authored JSON source identity")
    for marker in (
        "UseAuthoredData,",
        "UseSourceFallback,",
        "FailClosed",
        "Shipping must never mask missing or invalid authored course data with source fallbacks.",
        "ResolveAuthoredCourseLoadAction",
    ):
        _expect(marker in header_text, f"loader header marker is missing: {marker}")
    required_test_markers = (
        "DiscGolfTour.CourseDefinition.AuthoredDataLoadPolicy",
        "Shipping uses valid authored course data",
        "EDiscGolfAuthoredCourseDataState::Valid, true",
        "EDiscGolfAuthoredCourseLoadAction::UseAuthoredData",
        "Shipping fails closed when authored course data is missing",
        "EDiscGolfAuthoredCourseDataState::Missing, true",
        "Shipping fails closed when authored course data is invalid",
        "EDiscGolfAuthoredCourseDataState::Invalid, true",
        "EDiscGolfAuthoredCourseLoadAction::FailClosed",
    )
    for marker in required_test_markers:
        _expect(marker in test_text, f"loader policy test marker is missing: {marker}")


def _validate_strategy(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    reconciliation = policy["strategyReconciliation"]
    retirement_spec = reconciliation["retirementPolicy"]
    retirement = _project_doc(bundle, retirement_spec, "runtime closure retirement policy")
    for key in ("schema", "schemaVersion", "state", "strategy"):
        _expect(retirement.get(key) == retirement_spec[key],
                f"runtime closure retirement policy {key} differs")
    _expect(
        _nested(retirement, "retiredCandidateEvidence", "newCandidateReceiptEmissionAllowed") is False
        and _nested(retirement, "retiredCandidateEvidence", "candidateCookProofDelegatedToShippingSeparation") is True
        and _nested(retirement, "claimBoundary", "legacyCandidateRuntimeClosureRetired") is True,
        "runtime closure retirement delegation differs",
    )
    separation_spec = reconciliation["shippingSeparationPolicy"]
    separation = _project_doc(bundle, separation_spec, "Shipping PCG separation policy")
    for key in ("schema", "schemaVersion", "state", "strategy"):
        _expect(separation.get(key) == separation_spec[key],
                f"Shipping PCG separation policy {key} differs")
    _expect(
        _nested(separation, "runtimeAuthority", "shippingSourceFallbackAllowed") is False
        and _nested(separation, "runtimeAuthority", "runtimeRandomPlacementAllowed") is False
        and _nested(separation, "assetBoundary", "shippingNeverCookDirectory")
        == "/Game/Environment/Forest/PCG",
        "Shipping PCG separation authority differs",
    )
    return {
        "runtimeClosureRetired": True,
        "shippingStrategy": retirement_spec["strategy"],
        "shippingSeparationStrategy": separation_spec["strategy"],
        "bakeSaveReopenDisposition": reconciliation["deterministicUnrealBakeSaveReopen"]["disposition"],
        "bakeSaveReopenApplicable": False,
        "bakeSaveReopenPending": False,
        "bakeSaveReopenProven": False,
    }


def _validate_source_closure(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    closure_contract = policy["sourceClosure"]
    pre_spec = closure_contract["preBuildReceipt"]
    pre = _project_doc(bundle, pre_spec, "pre-build PCG source closure")
    _expect(
        pre.get("schema") == "DiscGolfTour.Session19BakedPcgSourceClosurePreBuild.v1"
        and pre.get("schemaVersion") == 1
        and pre.get("session") == 19
        and pre.get("candidateId") == CANDIDATE_ID
        and pre.get("state") == "PASS_PREBUILD_BAKED_PCG_SOURCE_CLOSURE"
        and pre.get("postBuildRevalidationRequired") is True
        and pre.get("releaseReady") is False,
        "pre-build PCG source closure contract differs",
    )
    closure = pre.get("closure")
    _expect(type(closure) is dict, "pre-build source closure payload is missing")
    _expect(
        closure.get("algorithm") == "SHA256_CANONICAL_JSON"
        and closure.get("sha256") == pre_spec["closureSha256"],
        "pre-build source closure identity differs",
    )
    files = closure.get("files")
    _expect(type(files) is list and len(files) == closure.get("fileCount"),
            "pre-build source closure file inventory differs")
    by_path: dict[str, dict[str, Any]] = {}
    for item in files:
        _expect(type(item) is dict and type(item.get("path")) is str,
                "pre-build source closure file entry is invalid")
        path = item["path"]
        _expect(path.casefold() not in (key.casefold() for key in by_path),
                f"duplicate source closure file: {path}")
        by_path[path] = item
    loader_bytes: list[bytes] = []
    for spec in closure_contract["loaderSourceFiles"]:
        path = spec["path"]
        _expect(by_path.get(path) == spec, f"source closure loader identity differs: {path}")
        data = bundle.project.get(path)
        _expect(data is not None, f"current loader source is missing: {path}")
        assert data is not None
        _require_identity(data, spec, f"current loader source {path}")
        loader_bytes.append(data)
    _audit_loader_source(*loader_bytes)

    candidate_spec = closure_contract["candidateVerification"]
    candidate = _project_doc(bundle, candidate_spec, "baked PCG candidate verification")
    _expect(
        candidate.get("schema") == "DiscGolfTour.Session19BakedPcgCandidateVerification.v1"
        and candidate.get("schemaVersion") == 1
        and candidate.get("candidateId") == CANDIDATE_ID
        and candidate.get("state") == candidate_spec["state"],
        "baked PCG candidate verification identity differs",
    )
    cross = candidate.get("sourceClosure")
    _expect(type(cross) is dict, "candidate source-closure cross-binding is missing")
    embedded = cross.get("preBuildReceipt")
    _expect(
        type(embedded) is dict
        and embedded.get("path") == pre_spec["path"]
        and embedded.get("bytes") == pre_spec["bytes"]
        and embedded.get("sha256") == pre_spec["sha256"]
        and embedded.get("preBuildSha256") == pre_spec["closureSha256"]
        and cross.get("postBuildSha256") == pre_spec["closureSha256"]
        and cross.get("prePostMatch") is True,
        "pre-build/post-build source closure does not cross-bind exactly",
    )
    _expect(
        _nested(candidate, "technicalBoundary", "candidateBoundAuthoredDataPass") is True
        and _nested(candidate, "technicalBoundary", "runtimeAuthoredSelectionProven") is False,
        "historical baked-PCG receipt was rewritten or its authored-data gate differs",
    )

    automation = policy["shippingLoader"]["automation"]
    log = bundle.project.get(automation["path"])
    _expect(log is not None, "candidate automation log is missing")
    assert log is not None
    _require_identity(log, automation, "candidate automation log")
    text = log.decode("utf-8")
    test = re.escape(automation["requiredTest"])
    success = re.findall(
        rf"Test Completed\. Result=\{{Success\}}[^\r\n]*Path=\{{{test}\}}", text
    )
    failures = re.findall(
        rf"Test Completed\. Result=\{{(?!Success\}})[^}}]+\}}[^\r\n]*Path=\{{{test}\}}", text
    )
    _expect(len(success) == 1 and not failures,
            "candidate automation does not contain exactly one successful authored-data policy test")
    return {
        "preBuildClosureSha256": pre_spec["closureSha256"],
        "postBuildClosureSha256": cross["postBuildSha256"],
        "prePostMatch": True,
        "failClosedShippingLoaderSourceIdentity": True,
        "shippingFallbackAllowed": False,
        "authoredDataLoadPolicyAutomationPass": True,
    }


def _validate_build(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    contract = policy["cleanShippingBuild"]
    build_spec = contract["buildReceipt"]
    build = _project_doc(bundle, build_spec, "Shipping build receipt")
    _expect(
        build.get("schema") == "DiscGolfTour.Session19ShippingCandidateBuildReceipt.v1"
        and build.get("schemaVersion") == 1
        and build.get("runId") == CANDIDATE_ID
        and build.get("state") == build_spec["state"]
        and build.get("uatExitCode") == contract["requiredUatExitCode"],
        "Shipping build receipt contract differs",
    )
    _expect(
        _nested(build, "target", "configuration") == "Shipping"
        and _nested(build, "target", "platform") == "Win64",
        "Shipping build target differs",
    )
    invocation = build.get("invocation")
    _expect(type(invocation) is dict, "Shipping build invocation is missing")
    for key, value in contract["requiredInvocation"].items():
        _expect(invocation.get(key) is value,
                f"Shipping build invocation differs: {key}")
    _expect(
        _nested(build, "archive", "windowsArchivePresent") is True
        and _nested(build, "archive", "requiredFilesPresent") is True,
        "Shipping build archive result differs",
    )

    verify_spec = contract["verificationReceipt"]
    verify = _project_doc(bundle, verify_spec, "Shipping verification receipt")
    _expect(
        verify.get("schema") == "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
        and verify.get("schemaVersion") == 2
        and verify.get("runId") == CANDIDATE_ID
        and verify.get("state") == verify_spec["state"]
        and verify.get("verificationMode") == contract["requiredVerificationMode"],
        "Shipping verification receipt contract differs",
    )
    _expect(
        _nested(verify, "buildReceipt", "path") == build_spec["path"]
        and _nested(verify, "buildReceipt", "bytes") == build_spec["bytes"]
        and _nested(verify, "buildReceipt", "sha256") == build_spec["sha256"]
        and _nested(verify, "buildReceipt", "uatExitCode") == 0,
        "Shipping verification/build receipt cross-binding differs",
    )
    candidate = policy["candidate"]
    _expect(
        _nested(verify, "shippingPluginCapabilities", "finalArchiveCanonicalManifestSha256")
        == candidate["archiveCanonicalManifestSha256"]
        and _nested(verify, "innerShippingExecutable", "relativePath")
        == candidate["shippingExecutable"]["path"]
        and _nested(verify, "innerShippingExecutable", "bytes")
        == candidate["shippingExecutable"]["bytes"]
        and _nested(verify, "innerShippingExecutable", "sha256")
        == candidate["shippingExecutable"]["sha256"],
        "Shipping verification/archive executable binding differs",
    )
    log_spec = contract["buildLog"]
    log = bundle.project.get(log_spec["path"])
    _expect(log is not None, "Shipping build log is missing")
    assert log is not None
    _require_identity(log, log_spec, "Shipping build log")
    _expect(
        _nested(verify, "buildLog", "path") == log_spec["path"]
        and _nested(verify, "buildLog", "bytes") == log_spec["bytes"]
        and _nested(verify, "buildLog", "sha256") == log_spec["sha256"]
        and _nested(verify, "buildLog", "requiredMarkersPresent") is True
        and _nested(verify, "buildLog", "forbiddenMarkersAbsent") is True,
        "Shipping verification/build-log binding differs",
    )
    return {
        "clean": True,
        "iterativeCook": False,
        "uatExitCode": 0,
        "verificationMode": contract["requiredVerificationMode"],
        "archiveVerified": True,
    }


def _parse_stage_manifest(data: bytes, label: str) -> list[str]:
    try:
        lines = data.decode("utf-8-sig").splitlines()
    except UnicodeDecodeError as exc:
        raise ContractError(f"{label} is not UTF-8") from exc
    identities: list[str] = []
    for line_number, line in enumerate(lines, 1):
        parts = line.split("\t")
        _expect(len(parts) == 2 and parts[1].endswith("Z"),
                f"{label} line {line_number} is malformed")
        identities.append(_canonical_relative(parts[0], f"{label} line {line_number}"))
    folded = [value.casefold() for value in identities]
    _expect(bool(identities) and len(folded) == len(set(folded)),
            f"{label} is empty or contains duplicates")
    return identities


def _validate_hole_json(data: bytes, expected: dict[str, Any]) -> dict[str, Any]:
    hole = _strict_json(data, expected["archivePath"])
    _expect(
        hole.get("schema") == "disc_golf_hole_blockout"
        and hole.get("schemaVersion") == 1
        and hole.get("courseId") == "PineRidgeChampionship"
        and hole.get("layoutId") == "Championship"
        and hole.get("holeNumber") == expected["holeNumber"],
        f"authored hole JSON identity differs: hole {expected['holeNumber']}",
    )
    trees = hole.get("trees")
    _expect(type(trees) is list and len(trees) == expected["treeCount"],
            f"authored tree array differs: hole {expected['holeNumber']}")
    for index, tree in enumerate(trees):
        _expect(type(tree) is dict and set(tree) == {"locationCm", "heightScale"},
                f"hole {expected['holeNumber']} tree {index} shape differs")
        location = tree.get("locationCm")
        _expect(type(location) is dict and set(location) == {"x", "y", "z"},
                f"hole {expected['holeNumber']} tree {index} location differs")
        for value in (*location.values(), tree.get("heightScale")):
            _expect(type(value) in {int, float} and not isinstance(value, bool),
                    f"hole {expected['holeNumber']} tree {index} is non-numeric")
    return {
        "courseId": hole["courseId"],
        "layoutId": hole["layoutId"],
        "holeNumber": hole["holeNumber"],
        "trees": trees,
    }


def _validate_archive(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    candidate = policy["candidate"]
    archive = bundle.archive
    _expect(
        archive.file_count == candidate["archiveFileCount"]
        and archive.total_bytes == candidate["archiveBytes"]
        and archive.canonical_manifest_sha256 == candidate["archiveCanonicalManifestSha256"],
        "live archive canonical identity differs",
    )
    by_path = {item["path"]: item for item in archive.records}
    _expect(len(by_path) == len(archive.records), "archive record paths are duplicated")
    if archive.complete_inventory:
        _expect(len(archive.records) == candidate["archiveFileCount"],
                "archive inventory is incomplete")
    executable = candidate["shippingExecutable"]
    _expect(by_path.get(executable["path"]) == executable,
            "archive Shipping executable identity differs")

    authored = policy["archiveAuthoredJson"]
    for key in ("nonUfsManifest", "ufsManifest"):
        spec = authored[key]
        _expect(by_path.get(spec["path"]) == spec, f"archive {key} identity differs")
        data = archive.payloads.get(spec["path"])
        _expect(data is not None, f"archive {key} payload is missing")
        assert data is not None
        _require_identity(data, spec, f"archive {key}")
    nonufs = _parse_stage_manifest(
        archive.payloads[authored["nonUfsManifest"]["path"]], "NonUFS manifest"
    )
    ufs = _parse_stage_manifest(
        archive.payloads[authored["ufsManifest"]["path"]], "UFS manifest"
    )
    nonufs_folded = [value.casefold() for value in nonufs]
    ufs_folded = [value.casefold() for value in ufs]

    calculated_holes: list[dict[str, Any]] = []
    files = authored["files"]
    _expect(len(files) == 4, "archive authored JSON set must contain one manifest and three holes")
    for spec in files:
        archive_path = spec["archivePath"]
        record = {"path": archive_path, "bytes": spec["bytes"], "sha256": spec["sha256"]}
        _expect(by_path.get(archive_path) == record,
                f"archived authored JSON identity differs: {archive_path}")
        data = archive.payloads.get(archive_path)
        _expect(data is not None, f"archived authored JSON is missing: {archive_path}")
        assert data is not None
        _require_identity(data, spec, f"archived authored JSON {archive_path}")
        source = bundle.project.get(spec["sourcePath"])
        _expect(source is not None, f"source authored JSON is missing: {spec['sourcePath']}")
        assert source is not None
        _expect(source == data, f"archive/source authored JSON bytes differ: {archive_path}")
        _expect(nonufs_folded.count(archive_path.casefold())
                == authored["requiredNonUfsOccurrenceCount"],
                f"NonUFS membership differs: {archive_path}")
        _expect(ufs_folded.count(archive_path.casefold())
                == authored["requiredUfsOccurrenceCount"],
                f"UFS membership differs: {archive_path}")
        if "holeNumber" in spec:
            calculated_holes.append(_validate_hole_json(data, spec))
        else:
            manifest = _strict_json(data, archive_path)
            actual_holes = [
                (item.get("holeNumber"), item.get("definitionFile"))
                for item in manifest.get("holes", []) if type(item) is dict
            ]
            _expect(
                manifest.get("schema") == "disc_golf_course_manifest"
                and manifest.get("schemaVersion") == 1
                and manifest.get("courseId") == "PineRidgeChampionship"
                and manifest.get("layoutId") == "Championship"
                and actual_holes == [
                    (1, "Data/PineRidgeHole1.json"),
                    (2, "Data/PineRidgeHole2.json"),
                    (3, "Data/PineRidgeHole3.json"),
                ],
                "archived Pine Ridge course manifest does not select the exact three holes",
            )
    combined_bytes = _canonical_json({"holes": calculated_holes})
    combined = {
        "treeCount": sum(len(item["trees"]) for item in calculated_holes),
        "bytes": len(combined_bytes),
        "sha256": _sha256(combined_bytes),
    }
    _expect(combined == authored["combinedTreePayload"],
            "combined authored tree payload differs")
    return {
        "fileCount": archive.file_count,
        "bytes": archive.total_bytes,
        "canonicalManifestSha256": archive.canonical_manifest_sha256,
        "shippingExecutable": copy.deepcopy(executable),
        "nonUfsManifest": {
            "path": authored["nonUfsManifest"]["path"],
            "bytes": authored["nonUfsManifest"]["bytes"],
            "sha256": authored["nonUfsManifest"]["sha256"],
        },
        "authoredJsonFileCount": 4,
        "allExactSourceMatches": True,
        "allNonUfsExactlyOnce": True,
        "allUfsAbsent": True,
        "combinedTreePayload": combined,
    }


def _validate_candidate_authored_receipt(policy: dict[str, Any], bundle: InputBundle) -> None:
    spec = policy["sourceClosure"]["candidateVerification"]
    receipt = _strict_json(bundle.project[spec["path"]], spec["path"])
    archive = receipt.get("archive")
    candidate = policy["candidate"]
    _expect(
        type(archive) is dict
        and archive.get("fileCount") == candidate["archiveFileCount"]
        and archive.get("bytes") == candidate["archiveBytes"]
        and archive.get("canonicalManifestSha256") == candidate["archiveCanonicalManifestSha256"]
        and archive.get("liveHashRevalidationPass") is True,
        "baked-PCG receipt archive binding differs",
    )
    authored = receipt.get("authoredRuntimeData")
    _expect(
        type(authored) is dict
        and authored.get("stagingClass") == "NONUFS"
        and authored.get("allThreeExactSourceAuthorityMatches") is True
        and authored.get("allThreeNonUfsManifestBindingsExact") is True
        and authored.get("allThreeUfsManifestOccurrencesAbsent") is True,
        "baked-PCG receipt authored-data binding differs",
    )
    expected_files = [item for item in policy["archiveAuthoredJson"]["files"] if "holeNumber" in item]
    rows = authored.get("files")
    _expect(type(rows) is list and len(rows) == 3,
            "baked-PCG receipt must bind exactly three authored hole JSON files")
    for expected, row in zip(expected_files, rows):
        _expect(
            row.get("sourcePath") == expected["sourcePath"]
            and row.get("archivePath") == expected["archivePath"]
            and row.get("bytes") == expected["bytes"]
            and row.get("sha256") == expected["sha256"]
            and row.get("sourceAuthorityMatch") is True
            and row.get("nonUfsManifestOccurrenceCount") == 1
            and row.get("ufsManifestOccurrenceCount") == 0,
            f"baked-PCG receipt authored hole binding differs: {expected['archivePath']}",
        )


def _validate_pcg_absence(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    contract = policy["shippingPcgAbsence"]
    spec = contract["receipt"]
    receipt = _project_doc(bundle, spec, "Shipping PCG separation receipt")
    _expect(
        receipt.get("schema") == "DiscGolfTour.Session19ShippingPcgSeparationEvidence.v1"
        and receipt.get("schemaVersion") == 1
        and receipt.get("candidateId") == CANDIDATE_ID
        and receipt.get("state") == spec["state"],
        "Shipping PCG separation receipt contract differs",
    )
    cook = receipt.get("cook")
    _expect(type(cook) is dict, "Shipping PCG cook evidence is missing")
    for field in contract["requiredZeroFields"]:
        _expect(cook.get(field) == 0, f"Shipping PCG absence counter is nonzero: {field}")
    _expect(
        cook.get("freshShippingCookAbsenceProven") is True
        and _nested(receipt, "claimBoundary", "freshShippingCookAbsenceProven") is True
        and _nested(receipt, "claimBoundary", "releaseReady") is False,
        "Shipping PCG absence claim boundary differs",
    )
    candidate = policy["candidate"]
    _expect(
        _nested(cook, "containerIdentity", "shippingBinary", "path")
        == candidate["shippingExecutable"]["path"]
        and _nested(cook, "containerIdentity", "shippingBinary", "bytes")
        == candidate["shippingExecutable"]["bytes"]
        and _nested(cook, "containerIdentity", "shippingBinary", "sha256")
        == candidate["shippingExecutable"]["sha256"],
        "Shipping PCG absence executable binding differs",
    )
    _expect(
        _nested(receipt, "source", "authoredTreePayload")
        == policy["archiveAuthoredJson"]["combinedTreePayload"],
        "Shipping PCG absence/source authored payload binding differs",
    )
    return {
        "freshShippingCookAbsenceProven": True,
        **{field: 0 for field in contract["requiredZeroFields"]},
    }


def _canonical_journal_records(data: bytes) -> list[dict[str, Any]]:
    _expect(bool(data) and len(data) <= 65536, "runtime journal byte count is invalid")
    _expect(not data.startswith(b"\xef\xbb\xbf") and b"\r" not in data and data.endswith(b"\n"),
            "runtime journal must be BOM-free LF-terminated JSONL")
    records: list[dict[str, Any]] = []
    for index, raw in enumerate(data.splitlines(keepends=True), 1):
        _expect(raw.endswith(b"\n") and raw != b"\n",
                f"runtime journal line {index} is empty or unterminated")
        try:
            raw[:-1].decode("ascii")
        except UnicodeDecodeError as exc:
            raise ContractError(f"runtime journal line {index} is not ASCII") from exc
        value = _strict_json(raw[:-1], f"runtime journal line {index}")
        canonical = json.dumps(
            value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
        ).encode("ascii") + b"\n"
        _expect(raw == canonical, f"runtime journal line {index} is not canonical JSON")
        records.append(value)
    return records


def _validate_journal(data: bytes, policy: dict[str, Any]) -> dict[str, Any]:
    contract = policy["operationalRuntime"]["runtimeJournal"]
    _require_identity(data, contract, "runtime journal")
    records = _canonical_journal_records(data)
    _expect(len(records) == 13, "runtime journal must contain one header and twelve events")
    header = records[0]
    expected_header_keys = {
        "archiveManifestSha256", "candidateId", "captureNonce", "executableSha256",
        "recordType", "roundId", "schema", "schemaVersion", "session", "startedUtc",
        "userDirToken",
    }
    _expect(set(header) == expected_header_keys, "runtime journal header fields differ")
    candidate = policy["candidate"]
    _expect(
        header.get("schema") == contract["schema"]
        and header.get("schemaVersion") == 1
        and header.get("session") == 19
        and header.get("recordType") == "HEADER"
        and header.get("candidateId") == candidate["candidateId"]
        and header.get("userDirToken") == candidate["userDirToken"]
        and header.get("executableSha256") == candidate["shippingExecutable"]["sha256"]
        and header.get("archiveManifestSha256") == candidate["archiveCanonicalManifestSha256"]
        and header.get("captureNonce") == contract["captureNonce"]
        and header.get("roundId") == contract["roundId"]
        and UUID_RE.fullmatch(header["captureNonce"]) is not None
        and UUID_RE.fullmatch(header["roundId"]) is not None,
        "runtime journal header candidate/hash binding differs",
    )
    events = records[1:]
    names = tuple(item.get("event") for item in events)
    _expect(names == EXPECTED_EVENTS, "runtime journal event sequence is incomplete or reordered")
    expected_completed = (0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3)
    expected_holes = (None, 1, 1, 1, 2, 2, 2, 3, 3, 3, None, None)
    previous_ms = 0
    previous_strokes = 0
    previous_penalties = 0
    for index, event in enumerate(events, 1):
        keys = {
            "completedHoles", "event", "holeNumber", "monotonicMs", "recordType",
            "roundId", "sequence", "totalPenalties", "totalStrokes",
        } | ({"finalScore"} if index == 12 else set())
        _expect(set(event) == keys, f"runtime journal event {index} fields differ")
        _expect(
            event.get("recordType") == "EVENT"
            and event.get("roundId") == contract["roundId"]
            and event.get("sequence") == index
            and event.get("completedHoles") == expected_completed[index - 1]
            and event.get("holeNumber") == expected_holes[index - 1]
            and type(event.get("monotonicMs")) is int
            and event["monotonicMs"] > previous_ms
            and type(event.get("totalStrokes")) is int
            and event["totalStrokes"] >= previous_strokes
            and type(event.get("totalPenalties")) is int
            and event["totalPenalties"] >= previous_penalties,
            f"runtime journal event {index} continuity differs",
        )
        previous_ms = event["monotonicMs"]
        previous_strokes = event["totalStrokes"]
        previous_penalties = event["totalPenalties"]
    score = events[-1]["finalScore"]
    _expect(
        type(score) is dict
        and score.get("completedHoles") == contract["finalCompletedHoles"]
        and score.get("totalHoles") == contract["finalTotalHoles"]
        and score.get("totalStrokes") == events[-1]["totalStrokes"]
        and score.get("totalPenalties") == events[-1]["totalPenalties"],
        "runtime journal final score differs",
    )
    rows = score.get("holeRows")
    _expect(
        type(rows) is list
        and [row.get("holeNumber") for row in rows if type(row) is dict] == [1, 2, 3]
        and sum(row.get("strokes", -1) for row in rows) == score["totalStrokes"]
        and sum(row.get("penalties", -1) for row in rows) == score["totalPenalties"],
        "runtime journal final score rows differ",
    )
    return {
        "schema": contract["schema"],
        "bytes": len(data),
        "sha256": _sha256(data),
        "captureNonce": contract["captureNonce"],
        "roundId": contract["roundId"],
        "eventCount": 12,
        "firstSequence": 1,
        "lastSequence": 12,
        "completedHoles": 3,
        "totalHoles": 3,
        "runtimeOperationalBindingValidated": True,
    }


def _validate_operational(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    contract = policy["operationalRuntime"]
    external_receipt = _project_doc(
        bundle, contract["externalTechnicalReceipt"], "external technical evidence receipt"
    )
    fresh = _project_doc(bundle, contract["freshUserDirReceipt"], "fresh UserDir receipt")
    three = _project_doc(bundle, contract["threeHoleReceipt"], "three-hole technical receipt")
    for label, document, spec in (
        ("external technical", external_receipt, contract["externalTechnicalReceipt"]),
        ("fresh UserDir", fresh, contract["freshUserDirReceipt"]),
        ("three-hole", three, contract["threeHoleReceipt"]),
    ):
        _expect(document.get("candidateId") == CANDIDATE_ID,
                f"{label} receipt is bound to another candidate")
        _expect(document.get("state") == spec["state"], f"{label} receipt state differs")

    launch_spec = contract["launchRecord"]
    manifest_spec = contract["technicalEvidenceManifest"]
    launch_data = bundle.external.get(launch_spec["path"])
    manifest_data = bundle.external.get(manifest_spec["path"])
    _expect(launch_data is not None and manifest_data is not None,
            "external launch or technical manifest is missing")
    assert launch_data is not None and manifest_data is not None
    _require_identity(launch_data, launch_spec, "launch record")
    _require_identity(manifest_data, manifest_spec, "technical evidence manifest")
    launch = _strict_json(launch_data, launch_spec["path"])
    manifest = _strict_json(manifest_data, manifest_spec["path"])
    candidate = policy["candidate"]
    journal_contract = contract["runtimeJournal"]
    _expect(
        launch.get("candidateId") == candidate["candidateId"]
        and launch.get("userDirToken") == candidate["userDirToken"]
        and launch.get("captureNonce") == journal_contract["captureNonce"]
        and launch.get("expectedExeSha256") == candidate["shippingExecutable"]["sha256"]
        and launch.get("observedExeSha256") == candidate["shippingExecutable"]["sha256"]
        and launch.get("expectedArchiveManifestSha256") == candidate["archiveCanonicalManifestSha256"]
        and launch.get("observedArchiveManifestSha256") == candidate["archiveCanonicalManifestSha256"]
        and launch.get("runtimeCheckpointJournalUserDirRelativePath")
        == journal_contract["userDirRelativePath"],
        "launch record candidate/hash/journal binding differs",
    )
    _expect(
        manifest.get("schema") == "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
        and manifest.get("schemaVersion") == 2
        and manifest.get("session") == 19
        and _nested(manifest, "candidate", "candidateId") == candidate["candidateId"]
        and _nested(manifest, "candidate", "archivePreserved") is True
        and _nested(manifest, "candidate", "archiveBefore", "canonicalManifestSha256")
        == candidate["archiveCanonicalManifestSha256"]
        and _nested(manifest, "candidate", "archiveAfter", "canonicalManifestSha256")
        == candidate["archiveCanonicalManifestSha256"]
        and _nested(manifest, "process", "exitCode") == 0
        and _nested(manifest, "process", "timedOut") is False
        and _nested(manifest, "userDir", "emptyBeforeLaunch") is True
        and _nested(manifest, "userDir", "token") == candidate["userDirToken"],
        "technical evidence manifest candidate/process binding differs",
    )
    journal_data = bundle.user.get(journal_contract["userDirRelativePath"])
    _expect(journal_data is not None, "game-emitted runtime journal is missing")
    assert journal_data is not None
    journal = _validate_journal(journal_data, policy)
    manifest_journal = _nested(manifest, "evidence", "runtimeCheckpointJournal")
    _expect(
        manifest_journal.get("bytes") == journal["bytes"]
        and manifest_journal.get("sha256") == journal["sha256"]
        and manifest_journal.get("captureNonce") == journal["captureNonce"]
        and manifest_journal.get("roundId") == journal["roundId"]
        and manifest_journal.get("eventCount") == 12
        and manifest_journal.get("firstSequence") == 1
        and manifest_journal.get("lastSequence") == 12
        and manifest_journal.get("validationFailures") == []
        and manifest_journal.get("validationState")
        == "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL",
        "technical manifest runtime-journal binding differs",
    )
    _expect(
        external_receipt.get("failures") == []
        and _nested(external_receipt, "manifest", "sha256") == manifest_spec["sha256"],
        "external technical receipt/manifest binding differs",
    )
    fresh_binding = fresh.get("runtimeJournalBinding")
    _expect(
        type(fresh_binding) is dict
        and fresh.get("userDirToken") == candidate["userDirToken"]
        and fresh.get("failures") == []
        and fresh_binding.get("userDirRelativePath") == journal_contract["userDirRelativePath"]
        and fresh_binding.get("bytes") == journal["bytes"]
        and fresh_binding.get("sha256") == journal["sha256"]
        and fresh_binding.get("captureNonce") == journal["captureNonce"]
        and fresh_binding.get("roundId") == journal["roundId"]
        and fresh_binding.get("launchRecordSha256") == launch_spec["sha256"]
        and fresh_binding.get("technicalEvidenceManifestSha256") == manifest_spec["sha256"]
        and fresh_binding.get("executableSha256") == candidate["shippingExecutable"]["sha256"]
        and fresh_binding.get("archiveManifestSha256") == candidate["archiveCanonicalManifestSha256"],
        "fresh UserDir runtime-journal binding differs",
    )
    three_journal = three.get("runtimeJournal")
    _expect(
        three.get("schema") == "DiscGolfTour.Session19ThreeHoleTechnicalAcceptanceReceipt.v2"
        and three.get("schemaVersion") == 2
        and three.get("technicalAcceptance") is True
        and type(three_journal) is dict
        and three_journal.get("bytes") == journal["bytes"]
        and three_journal.get("sha256") == journal["sha256"]
        and three_journal.get("candidateId") == candidate["candidateId"]
        and three_journal.get("executableSha256") == candidate["shippingExecutable"]["sha256"]
        and three_journal.get("archiveManifestSha256") == candidate["archiveCanonicalManifestSha256"]
        and three_journal.get("runtimeOperationalBindingValidated") is True
        and _nested(three, "continuityClaims", "singleRoundProven") is True
        and _nested(three, "continuityClaims", "threeHoleCompletionObserved") is True,
        "three-hole receipt runtime-journal binding differs",
    )
    for key in (
        "visualQualityApproval", "accessibilityApproval", "humanPlayFeelApproval",
        "manualGameplayAcceptance", "productOwnerApproval", "performanceAcceptance",
        "soakAcceptance", "provenanceApproval", "legalApproval", "distributionClearance",
        "releaseApproval", "releaseReady", "cryptographicProcessAttestation",
    ):
        _expect(_nested(three, "claimBoundary", key) is False,
                f"three-hole receipt unexpectedly claims {key}")
    return {
        "externalRunToken": candidate["externalRunToken"],
        "userDirToken": candidate["userDirToken"],
        "launchRecordSha256": launch_spec["sha256"],
        "technicalEvidenceManifestSha256": manifest_spec["sha256"],
        "runtimeJournal": journal,
        "singleRoundProven": True,
        "threeHoleCompletionObserved": True,
    }


def _evaluate(policy: dict[str, Any], bundle: InputBundle) -> dict[str, Any]:
    _validate_policy_semantics(policy)
    for spec in _policy_project_specs(policy):
        data = bundle.project.get(spec["path"])
        _expect(data is not None, f"project input is missing: {spec['path']}")
        assert data is not None
        _require_identity(data, spec, f"project input {spec['path']}")
    strategy = _validate_strategy(policy, bundle)
    source = _validate_source_closure(policy, bundle)
    build = _validate_build(policy, bundle)
    archive = _validate_archive(policy, bundle)
    _validate_candidate_authored_receipt(policy, bundle)
    pcg = _validate_pcg_absence(policy, bundle)
    runtime = _validate_operational(policy, bundle)
    return {
        "schema": RECEIPT_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "candidateId": CANDIDATE_ID,
        "verifiedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": PASS_STATE,
        "authority": policy["authority"],
        "strategyReconciliation": strategy,
        "sourceClosureAndLoader": source,
        "cleanShippingBuild": build,
        "archiveAuthoredJson": archive,
        "shippingPcgAbsence": pcg,
        "operationalRuntime": runtime,
        "technicalConclusion": {
            "boundedOperationalRuntimeAuthoredJsonSelectionProven": True,
            "reasoningBoundary": (
                "Exact source closure and fail-closed Shipping loader policy, clean direct build, "
                "archived authored JSON, Shipping PCG absence, and one completed game-emitted "
                "three-hole journal are mutually candidate/hash-bound."
            ),
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
        "remainingEvidence": [
            "THREE_HOLE_COLLISION_AND_VISUAL_PRODUCT_APPROVAL",
            "QUANTITATIVE_PERFORMANCE_PROFILE_AND_THERMAL_SOAK_ACCEPTANCE",
            "HUMAN_GAMEPLAY_AND_PLAY_FEEL_APPROVAL",
            "PROVENANCE_LEGAL_DISTRIBUTION_AND_OWNER_APPROVAL",
            "RELEASE_APPROVAL",
        ],
        "session13BlockerClosed": False,
        "releaseReady": False,
        "receiptPath": policy["evidence"]["receiptPattern"].format(candidateId=CANDIDATE_ID),
    }


def _json_mutation(data: bytes, mutate: Callable[[dict[str, Any]], None]) -> bytes:
    value = _strict_json(data, "self-test mutation")
    mutate(value)
    return json.dumps(value, indent=2, ensure_ascii=False).encode("utf-8") + b"\n"


def _run_self_test(policy: dict[str, Any], bundle: InputBundle) -> int:
    baseline = _evaluate(policy, bundle)
    _expect(baseline["state"] == PASS_STATE, "self-test baseline did not pass")
    caught: list[str] = []
    failures: list[str] = []

    def expect_failure(name: str, action: Callable[[], None]) -> None:
        try:
            action()
        except (ContractError, KeyError, UnicodeError):
            caught.append(name)
        else:
            failures.append(f"adversarial mutation escaped: {name}")

    loader_path = policy["sourceClosure"]["loaderSourceFiles"][0]["path"]
    fallback_source = bundle.project[loader_path].replace(
        b"? EDiscGolfAuthoredCourseLoadAction::FailClosed\r\n        : EDiscGolfAuthoredCourseLoadAction::UseSourceFallback",
        b"? EDiscGolfAuthoredCourseLoadAction::UseSourceFallback\r\n        : EDiscGolfAuthoredCourseLoadAction::FailClosed",
    )
    if fallback_source == bundle.project[loader_path]:
        fallback_source = bundle.project[loader_path].replace(
            b"? EDiscGolfAuthoredCourseLoadAction::FailClosed\n        : EDiscGolfAuthoredCourseLoadAction::UseSourceFallback",
            b"? EDiscGolfAuthoredCourseLoadAction::UseSourceFallback\n        : EDiscGolfAuthoredCourseLoadAction::FailClosed",
        )
    expect_failure(
        "FALLBACK_ALLOWED_SOURCE",
        lambda: _audit_loader_source(
            fallback_source,
            bundle.project[policy["sourceClosure"]["loaderSourceFiles"][1]["path"]],
            bundle.project[policy["sourceClosure"]["loaderSourceFiles"][2]["path"]],
        ),
    )

    cross_candidate = copy.deepcopy(bundle)
    separation_path = policy["shippingPcgAbsence"]["receipt"]["path"]
    cross_candidate.project[separation_path] = _json_mutation(
        cross_candidate.project[separation_path],
        lambda value: value.__setitem__("candidateId", "S19_WindowsShipping_OTHER"),
    )
    expect_failure("CROSS_CANDIDATE_DISAGREEMENT", lambda: _evaluate(policy, cross_candidate))

    cross_hash = copy.deepcopy(bundle)
    fresh_path = policy["operationalRuntime"]["freshUserDirReceipt"]["path"]
    cross_hash.project[fresh_path] = _json_mutation(
        cross_hash.project[fresh_path],
        lambda value: value["runtimeJournalBinding"].__setitem__("executableSha256", "0" * 64),
    )
    expect_failure("CROSS_HASH_DISAGREEMENT", lambda: _evaluate(policy, cross_hash))

    missing_json = copy.deepcopy(bundle)
    missing_path = policy["archiveAuthoredJson"]["files"][2]["archivePath"]
    missing_json.archive.payloads.pop(missing_path, None)
    missing_json.archive.records = [
        item for item in missing_json.archive.records if item["path"] != missing_path
    ]
    expect_failure("MISSING_ARCHIVED_JSON", lambda: _evaluate(policy, missing_json))

    altered_closure = copy.deepcopy(bundle)
    pre_path = policy["sourceClosure"]["preBuildReceipt"]["path"]
    altered_closure.project[pre_path] = _json_mutation(
        altered_closure.project[pre_path],
        lambda value: value["closure"].__setitem__("sha256", "A" * 64),
    )
    expect_failure("ALTERED_SOURCE_CLOSURE", lambda: _evaluate(policy, altered_closure))

    journal_path = policy["operationalRuntime"]["runtimeJournal"]["userDirRelativePath"]
    journal_lines = bundle.user[journal_path].splitlines(keepends=True)
    expect_failure(
        "INCOMPLETE_RUNTIME_JOURNAL",
        lambda: _validate_journal(b"".join(journal_lines[:-1]), policy),
    )

    bake_pending = copy.deepcopy(policy)
    bake_pending["strategyReconciliation"]["deterministicUnrealBakeSaveReopen"][
        "mayBeReportedAsPending"
    ] = True
    expect_failure("BAKE_SAVE_REOPEN_PENDING_OVERCLAIM", lambda: _validate_policy_semantics(bake_pending))

    dirty_build = copy.deepcopy(bundle)
    build_path = policy["cleanShippingBuild"]["buildReceipt"]["path"]
    dirty_build.project[build_path] = _json_mutation(
        dirty_build.project[build_path],
        lambda value: value["invocation"].__setitem__("clean", False),
    )
    expect_failure("DIRTY_BUILD", lambda: _evaluate(policy, dirty_build))

    pcg_present = copy.deepcopy(bundle)
    pcg_present.project[separation_path] = _json_mutation(
        pcg_present.project[separation_path],
        lambda value: value["cook"].__setitem__("projectPcgGraphEntryCount", 1),
    )
    expect_failure("SHIPPING_PCG_PRESENT", lambda: _evaluate(policy, pcg_present))

    archive_hash = copy.deepcopy(bundle)
    archive_hash.archive.canonical_manifest_sha256 = "B" * 64
    expect_failure("ARCHIVE_HASH_DISAGREEMENT", lambda: _evaluate(policy, archive_hash))

    required = set(policy["evidence"]["selfTestRequiredMutations"])
    if failures or not required.issubset(caught):
        print(
            f"Session 19 PCG operational runtime proof self-test: FAIL "
            f"({len(caught)} adversarial mutations caught)"
        )
        for failure in failures:
            print(f"- {failure}")
        for missing in sorted(required - set(caught)):
            print(f"- required mutation did not run: {missing}")
        return 1
    print(
        f"Session 19 PCG operational runtime proof self-test: PASS "
        f"({len(caught)} adversarial mutations caught)"
    )
    return 0


def _write_immutable(path: Path, result: dict[str, Any]) -> None:
    expected = ROOT / "Evidence" / "Session19" / f"PcgOperationalRuntimeProof-{CANDIDATE_ID}.json"
    _expect(path.resolve() == expected.resolve(), "output must use the exact canonical receipt path")
    _expect(path.parent.is_dir() and not path.parent.is_symlink() and not _is_reparse(path.parent),
            "output directory is missing or irregular")
    payload = json.dumps(result, indent=2, ensure_ascii=False).encode("utf-8") + b"\n"
    try:
        with path.open("xb") as handle:
            handle.write(payload)
    except FileExistsError as exc:
        raise ContractError(f"canonical receipt already exists: {path}") from exc


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--policy", type=Path, default=Path(POLICY_RELATIVE))
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--external-run", type=Path)
    parser.add_argument("--user-dir", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--emit-json", action="store_true")
    parser.add_argument("--require-release-ready", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        root = args.root.resolve(strict=True)
        _expect(root == ROOT.resolve(strict=True), "--root must resolve to this repository")
        policy_path = args.policy if args.policy.is_absolute() else root / args.policy
        policy, _ = _load_policy(policy_path)

        candidate = policy["candidate"]
        default_archive = Path(
            "C:/DGTour_Packages" "/" + CANDIDATE_ID + "/Windows"
        )
        default_external = Path("C:/DGTourExternal/Runs") / candidate["externalRunToken"]
        default_user = Path("C:/DGTourExternal/UserDirs") / candidate["userDirToken"]

        if args.self_test:
            _expect(
                not any((args.candidate_id, args.archive, args.external_run, args.user_dir,
                         args.output, args.emit_json, args.require_release_ready)),
                "--self-test does not accept validation or output arguments",
            )
            bundle = _load_bundle(
                policy, default_archive, default_external, default_user, full_archive=False
            )
            return _run_self_test(policy, bundle)

        _expect(
            type(args.candidate_id) is str
            and CANDIDATE_RE.fullmatch(args.candidate_id) is not None
            and args.candidate_id == CANDIDATE_ID,
            f"--candidate-id must be the exact candidate {CANDIDATE_ID}",
        )
        _expect(args.archive is not None and args.external_run is not None and args.user_dir is not None,
                "--archive, --external-run, and --user-dir are required")
        bundle = _load_bundle(
            policy,
            args.archive.resolve(strict=True),
            args.external_run.resolve(strict=True),
            args.user_dir.resolve(strict=True),
            full_archive=True,
        )
        result = _evaluate(policy, bundle)
        if args.output is not None:
            output = args.output if args.output.is_absolute() else root / args.output
            _write_immutable(output, result)
        if args.emit_json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"Session 19 PCG operational runtime proof: {PASS_STATE}")
            print(f"candidate={CANDIDATE_ID}")
            print("bakeSaveReopen=NOT_APPLICABLE_TO_AUTHORED_JSON_EDITOR_ONLY_PCG_SHIPPING_STRATEGY")
            print("boundedOperationalRuntimeAuthoredJsonSelectionProven=true")
            print("cryptographicCodeToBinaryAttestation=false")
            print("visualCollisionPerformanceSoakHumanLegalReleaseApprovals=false")
        if args.require_release_ready:
            return 2
        return 0
    except (ContractError, OSError, UnicodeError, KeyError) as exc:
        print(f"Session 19 PCG operational runtime proof: {FAIL_STATE}", file=sys.stderr)
        print(f"- {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
