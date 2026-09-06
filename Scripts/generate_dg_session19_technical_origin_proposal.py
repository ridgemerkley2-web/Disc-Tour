#!/usr/bin/env python3
"""Generate a non-authoritative Session 19 technical-origin proposal.

This tool is deliberately separate from the reviewed staged-provenance
classification path.  It proposes only deterministic technical origin from exact
candidate paths and local technical receipts.  It cannot review, approve, clear,
or license any identity, and its schema is intentionally incompatible with
``DiscGolfTour.Session19StagedProvenanceClassification.v1``.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
POLY_HAVEN_RECEIPT = ROOT / "SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"
THIRD_PARTY_POLICY = ROOT / "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json"

PROPOSAL_SCHEMA = "DiscGolfTour.Session19TechnicalOriginProposal.v1"
PROPOSAL_STATE = (
    "DRAFT_TECHNICAL_ORIGIN_PROPOSAL_UNREVIEWED_NOT_SHIPPING_APPROVED"
)
PROPOSAL_PURPOSE = "NON_AUTHORITATIVE_TECHNICAL_ORIGIN_TRIAGE_ONLY"
SOURCE_SCHEMA = "DiscGolfTour.Session19StagedProvenanceClassification.v1"
SOURCE_STATE = "DRAFT_UNREVIEWED_NOT_SHIPPING_APPROVED"

CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
SHA256_RE = re.compile(r"^[0-9A-Fa-f]{64}$")
ANONYMOUS_ID_RE = re.compile(
    r"^<[^<>]+>#[0-9A-F]{24}@\d{16}:\d{16}:[0-9A-F]{40}$"
)

SOURCE_ROOT_KEYS = {"schema", "schemaVersion", "candidateId", "state", "records"}
SOURCE_RECORD_KEYS = {
    "scope", "container", "identity", "classification", "authorityId",
    "licenseId", "shippingApproved", "evidencePaths",
}
PROPOSAL_ROOT_KEYS = {
    "schema", "schemaVersion", "candidateId", "state", "purpose", "reviewed",
    "shippingApproved", "legalReviewed", "legalApproved", "licenseApproved",
    "distributionApproved", "authoritativeClassificationCompatible", "inputBinding",
    "scopePolicy", "technicalEvidence", "ruleCatalog", "records", "summary",
    "releaseBoundary",
}
PROPOSAL_RECORD_KEYS = {
    "scope", "container", "identity", "proposalDisposition",
    "technicalOriginCategory", "ruleId", "confidence", "technicalBasisIds",
    "licenseStatus", "reviewed", "shippingApproved", "legalReviewed",
    "legalApproved", "licenseApproved", "distributionApproved",
}

SCOPE_ORDER = (
    "ARCHIVE_FILE",
    "UFS_ENTRY",
    "NONUFS_ENTRY",
    "CONTAINER_NAMED_ENTRY",
    "CONTAINER_ANONYMOUS_CHUNK",
)
AUTHORITATIVE_SCOPES = (
    "ARCHIVE_FILE",
    "NONUFS_ENTRY",
    "CONTAINER_NAMED_ENTRY",
    "CONTAINER_ANONYMOUS_CHUNK",
)
CLASSIFICATION_ELIGIBLE_SCOPES = (
    "ARCHIVE_FILE",
    "NONUFS_ENTRY",
    "CONTAINER_NAMED_ENTRY",
)
DIAGNOSTIC_SCOPES = ("UFS_ENTRY",)
UNRESOLVED_SCOPES = ("CONTAINER_ANONYMOUS_CHUNK",)

TECHNICAL_DISPOSITION = "TECHNICAL_ORIGIN_PROPOSED_UNREVIEWED"
DIAGNOSTIC_DISPOSITION = "DIAGNOSTIC_NON_AUTHORITATIVE_NOT_CLASSIFIED"
ANONYMOUS_DISPOSITION = "UNRESOLVED_ANONYMOUS_CHUNK"
PATH_DISPOSITION = "UNRESOLVED_PATH_IDENTITY"
LICENSE_STATUS = "UNASSESSED_NOT_A_LEGAL_CLASSIFICATION"

RULE_CATALOG = (
    {
        "ruleId": "R01_THIRD_PARTY_POLICY_EXACT_STAGED_PATH",
        "technicalOriginCategory": "THIRD_PARTY_TECHNICAL_MAPPING",
        "matchAuthority": "EXACT_IDENTITY_FROM_LOCAL_TECHNICAL_POLICY",
        "confidence": "EXACT_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R02_METAHUMAN_EXACT_PROJECT_SOURCE_PATH",
        "technicalOriginCategory": "METAHUMAN_EXACT_PROJECT_SOURCE_PATH",
        "matchAuthority": "EXACT_PROJECT_SOURCE_PATH",
        "confidence": "EXACT_SOURCE_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R03_METAHUMAN_COOKED_COMPANION",
        "technicalOriginCategory": "METAHUMAN_COOKED_COMPANION_INFERENCE",
        "matchAuthority": "EXACT_STEM_OF_PROJECT_SOURCE_UASSET",
        "confidence": "COOKED_COMPANION_INFERENCE",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R04_POLY_HAVEN_DERIVED_EXACT_RECEIPT_PATH",
        "technicalOriginCategory": "POLY_HAVEN_DERIVED_RECEIPT_EXACT_PATH",
        "matchAuthority": "EXACT_IDENTITY_FROM_LOCAL_DERIVED_RUNTIME_RECEIPT",
        "confidence": "EXACT_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R05_POLY_HAVEN_DERIVED_COOKED_COMPANION",
        "technicalOriginCategory": "POLY_HAVEN_COOKED_COMPANION_INFERENCE",
        "matchAuthority": "EXACT_STEM_OF_RECEIPT_UASSET",
        "confidence": "COOKED_COMPANION_INFERENCE",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R06_PINE_RIDGE_PROJECT_ORIGINAL_EXACT_RECEIPT_PATH",
        "technicalOriginCategory": "PINE_PROJECT_ORIGINAL_RECEIPT",
        "matchAuthority": "EXACT_IDENTITY_FROM_LOCAL_PROJECT_ORIGINAL_RECEIPT_SET",
        "confidence": "EXACT_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R07_ENGINE_PLUGIN_DESCRIPTOR_PATH",
        "technicalOriginCategory": "UE_PLUGIN_DESCRIPTOR_TECHNICAL_INVENTORY",
        "matchAuthority": "ENGINE_PLUGIN_NAMESPACE_AND_DESCRIPTOR_SUFFIX",
        "confidence": "PATH_NAMESPACE_INFERENCE",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R08_ENGINE_PATH_NAMESPACE",
        "technicalOriginCategory": "UE_ENGINE_PATH_TECHNICAL_INFERENCE",
        "matchAuthority": "ENGINE_PATH_NAMESPACE",
        "confidence": "PATH_NAMESPACE_INFERENCE",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R09_PROJECT_SOURCE_EXACT_PATH",
        "technicalOriginCategory": "PROJECT_TREE_RUNTIME_IDENTITY",
        "matchAuthority": "EXACT_PROJECT_SOURCE_PATH",
        "confidence": "EXACT_SOURCE_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R10_PROJECT_RELEASE_METADATA_EXACT_PATH",
        "technicalOriginCategory": "PROJECT_RELEASE_METADATA",
        "matchAuthority": "EXACT_RELEASE_METADATA_IDENTITY",
        "confidence": "EXACT_PATH",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
    {
        "ruleId": "R11_PROJECT_BUILD_NAMESPACE",
        "technicalOriginCategory": "PROJECT_GENERATED_BUILD_OUTPUT",
        "matchAuthority": "PROJECT_BUILD_PATH_NAMESPACE",
        "confidence": "PATH_NAMESPACE_INFERENCE",
        "legalConclusionAllowed": False,
        "distributionConclusionAllowed": False,
    },
)
RULE_BY_ID = {rule["ruleId"]: rule for rule in RULE_CATALOG}

RELEASE_METADATA_IDENTITIES = {
    "DiscGolfTour/DiscGolfTour.uproject",
    "Engine/Config/StagedBuild_DiscGolfTour.ini",
    "Manifest_NonUFSFiles_Win64.txt",
    "Manifest_UFSFiles_Win64.txt",
    "NOTICES.txt",
}
PROJECT_ROOT_BUILD_IDENTITIES = {
    "DiscGolfTour.exe",
}
COOKED_COMPANION_SUFFIXES = (".ubulk", ".uptnl")
RESERVED_AUTHORITATIVE_OUTPUT_PREFIXES = (
    "stagedprovenanceclassification",
    "stagedprovenancedraftaudit",
    "stagedprovenancereceipt",
)


class ProposalError(ValueError):
    """A fail-closed proposal input or output error."""


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ProposalError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def reject_constant(value: str) -> Any:
    raise ProposalError(f"non-finite JSON number: {value}")


def load_json_strict(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8-sig"),
            object_pairs_hook=reject_duplicate_keys,
            parse_constant=reject_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ProposalError) as exc:
        raise ProposalError(f"strict JSON load failed for {safe_display_path(path)}: {exc}") from exc


def safe_display_path(path: Path) -> str:
    """Return a repository-relative label, never an absolute host path."""
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except (OSError, ValueError):
        return path.name


def canonical_relative(value: Any) -> str | None:
    if type(value) is not str or not value or value != value.strip():
        return None
    if "\\" in value or "\x00" in value or ":" in value or value.startswith("/"):
        return None
    pure = PurePosixPath(value)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        return None
    if pure.as_posix() != value:
        return None
    return value


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def file_binding(path: Path) -> dict[str, Any]:
    if not path.is_file() or path.is_symlink():
        raise ProposalError(f"required regular file is missing: {safe_display_path(path)}")
    data = path.read_bytes()
    return {
        "path": safe_display_path(path),
        "bytes": len(data),
        "sha256": sha256_bytes(data),
    }


def validate_candidate_id(value: str) -> str:
    if not CANDIDATE_ID_RE.fullmatch(value):
        raise ProposalError("candidate ID is malformed")
    return value


def validate_explicit_sha256(value: str) -> str:
    if not SHA256_RE.fullmatch(value):
        raise ProposalError("source draft SHA-256 must contain exactly 64 hexadecimal characters")
    return value.upper()


def validate_source_record(record: Any, index: int) -> tuple[str, str | None, str]:
    if type(record) is not dict or set(record) != SOURCE_RECORD_KEYS:
        raise ProposalError(f"source record {index} keys or type differ")
    scope = record["scope"]
    if scope not in SCOPE_ORDER:
        raise ProposalError(f"source record {index} scope is invalid")
    identity = record["identity"]
    if type(identity) is not str or not identity:
        raise ProposalError(f"source record {index} identity is invalid")
    if scope == "CONTAINER_ANONYMOUS_CHUNK":
        if not ANONYMOUS_ID_RE.fullmatch(identity):
            raise ProposalError(f"source record {index} anonymous identity is malformed")
    elif canonical_relative(identity) is None:
        raise ProposalError(f"source record {index} path identity is non-canonical")
    container = record["container"]
    if scope.startswith("CONTAINER_"):
        if canonical_relative(container) is None:
            raise ProposalError(f"source record {index} container is non-canonical")
    elif container is not None:
        raise ProposalError(f"source record {index} unexpectedly names a container")
    if (
        record["classification"] != "UNCLASSIFIED"
        or record["authorityId"] != "PENDING"
        or record["licenseId"] != "PENDING"
        or record["shippingApproved"] is not False
    ):
        raise ProposalError(
            f"source record {index} is not an unreviewed/unapproved draft record"
        )
    evidence_paths = record["evidencePaths"]
    if (
        type(evidence_paths) is not list
        or not evidence_paths
        or any(canonical_relative(path) is None for path in evidence_paths)
        or len({path.casefold() for path in evidence_paths}) != len(evidence_paths)
    ):
        raise ProposalError(f"source record {index} evidence paths are malformed")
    return scope, container, identity


def load_source_draft(
    path: Path, candidate_id: str, explicit_sha256: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    candidate_id = validate_candidate_id(candidate_id)
    expected_hash = validate_explicit_sha256(explicit_sha256)
    binding = file_binding(path)
    if binding["sha256"] != expected_hash:
        raise ProposalError(
            f"source draft SHA-256 mismatch: expected {expected_hash}, observed {binding['sha256']}"
        )
    document = load_json_strict(path)
    if type(document) is not dict or set(document) != SOURCE_ROOT_KEYS:
        raise ProposalError("source draft root keys or type differ")
    if document["schema"] != SOURCE_SCHEMA or document["schemaVersion"] != 1:
        raise ProposalError("source draft schema differs")
    if document["candidateId"] != candidate_id:
        raise ProposalError("source draft candidate ID differs from the CLI binding")
    if document["state"] != SOURCE_STATE:
        raise ProposalError("source draft is not in the required unreviewed state")
    records = document["records"]
    if type(records) is not list or not records:
        raise ProposalError("source draft records must be a non-empty array")
    seen: set[tuple[str, str, str]] = set()
    for index, record in enumerate(records):
        scope, container, identity = validate_source_record(record, index)
        key = (scope, (container or "").casefold(), identity.casefold())
        if key in seen:
            raise ProposalError(f"source record {index} duplicates or case-collides")
        seen.add(key)
    binding.update(
        {
            "schema": document["schema"],
            "schemaVersion": document["schemaVersion"],
            "candidateId": candidate_id,
            "state": document["state"],
            "recordCount": len(records),
        }
    )
    return document, binding


def receipt_identity(relative_path: Any) -> str:
    canonical = canonical_relative(relative_path)
    if canonical is None or not canonical.startswith("Content/"):
        raise ProposalError("runtime receipt contains a non-canonical Content identity")
    return "DiscGolfTour/" + canonical


def load_poly_haven_sets() -> tuple[set[str], set[str], dict[str, Any]]:
    receipt = load_json_strict(POLY_HAVEN_RECEIPT)
    if type(receipt) is not dict:
        raise ProposalError("Poly Haven runtime receipt root is malformed")
    derived = receipt.get("derivedRuntimeArtifacts")
    originals = receipt.get("excludedProjectOriginalRuntimeArtifacts")
    if type(derived) is not list or type(originals) is not list:
        raise ProposalError("Poly Haven runtime receipt identity arrays are malformed")
    derived_set = {
        receipt_identity(record.get("relativePath"))
        for record in derived if type(record) is dict
    }
    original_set = {
        receipt_identity(record.get("relativePath"))
        for record in originals if type(record) is dict
    }
    if len(derived_set) != len(derived) or len(original_set) != len(originals):
        raise ProposalError("Poly Haven runtime receipt identities duplicate or are malformed")
    if derived_set & original_set:
        raise ProposalError("Poly Haven derived and project-original sets overlap")
    return derived_set, original_set, file_binding(POLY_HAVEN_RECEIPT)


def load_third_party_paths() -> tuple[set[str], dict[str, Any]]:
    policy = load_json_strict(THIRD_PARTY_POLICY)
    if type(policy) is not dict or type(policy.get("components")) is not list:
        raise ProposalError("third-party technical policy is malformed")
    paths: list[str] = []
    for component in policy["components"]:
        if type(component) is not dict or type(component.get("stagedRuntimeDlls")) is not list:
            raise ProposalError("third-party component staged identities are malformed")
        for identity in component["stagedRuntimeDlls"]:
            canonical = canonical_relative(identity)
            if canonical is None:
                raise ProposalError("third-party policy contains a non-canonical staged identity")
            paths.append(canonical)
    if len({path.casefold() for path in paths}) != len(paths):
        raise ProposalError("third-party policy staged identities duplicate or case-collide")
    return set(paths), file_binding(THIRD_PARTY_POLICY)


def project_source_path(identity: str) -> Path | None:
    prefixes = (
        ("DiscGolfTour/Content/", ROOT / "Content"),
        ("DiscGolfTour/Data/", ROOT / "Data"),
    )
    for prefix, source_root in prefixes:
        if not identity.startswith(prefix):
            continue
        relative = identity[len(prefix):]
        if canonical_relative(relative) is None:
            return None
        path = source_root.joinpath(*PurePosixPath(relative).parts)
        try:
            resolved = path.resolve(strict=True)
            resolved_root = source_root.resolve(strict=True)
        except OSError:
            return None
        if resolved_root not in resolved.parents or not path.is_file() or path.is_symlink():
            return None
        return path
    return None


def uasset_identity_for_companion(identity: str) -> str | None:
    lowered = identity.casefold()
    for suffix in COOKED_COMPANION_SUFFIXES:
        if lowered.endswith(suffix):
            return identity[:-len(suffix)] + ".uasset"
    return None


def project_source_manifest(records: list[dict[str, Any]]) -> dict[str, Any]:
    paths: dict[str, Path] = {}
    for record in records:
        if record["scope"] not in CLASSIFICATION_ELIGIBLE_SCOPES:
            continue
        identity = record["identity"]
        source = project_source_path(identity)
        if source is None:
            companion = uasset_identity_for_companion(identity)
            source = project_source_path(companion) if companion is not None else None
        if source is not None:
            paths[safe_display_path(source)] = source
    lines: list[str] = []
    aggregate_bytes = 0
    for relative in sorted(paths, key=str.casefold):
        data = paths[relative].read_bytes()
        aggregate_bytes += len(data)
        lines.append(f"{relative}\t{len(data)}\t{sha256_bytes(data)}\n")
    manifest = "".join(lines).encode("utf-8")
    return {
        "fileCount": len(paths),
        "aggregateBytes": aggregate_bytes,
        "canonicalManifestSha256": sha256_bytes(manifest),
        "manifestFormat": "UTF8_SORTED_RELATIVE_PATH_TAB_BYTES_TAB_SHA256_LF",
        "hostPathsRecorded": False,
    }


def technical_context(records: list[dict[str, Any]]) -> dict[str, Any]:
    derived, originals, poly_binding = load_poly_haven_sets()
    third_party, third_party_binding = load_third_party_paths()
    return {
        "polyDerived": derived,
        "polyOriginal": originals,
        "thirdParty": third_party,
        "polyBinding": poly_binding,
        "thirdPartyBinding": third_party_binding,
        "projectSourceManifest": project_source_manifest(records),
    }


def rule_result(rule_id: str, basis_ids: list[str]) -> dict[str, Any]:
    rule = RULE_BY_ID[rule_id]
    return {
        "proposalDisposition": TECHNICAL_DISPOSITION,
        "technicalOriginCategory": rule["technicalOriginCategory"],
        "ruleId": rule_id,
        "confidence": rule["confidence"],
        "technicalBasisIds": basis_ids,
    }


def classify_path(identity: str, context: dict[str, Any]) -> dict[str, Any]:
    if identity in context["thirdParty"]:
        return rule_result(
            "R01_THIRD_PARTY_POLICY_EXACT_STAGED_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "THIRD_PARTY_TECHNICAL_PATH_POLICY"],
        )

    meta_prefix = "DiscGolfTour/Content/DiscGolf/Characters/MetaHuman/"
    if identity.startswith(meta_prefix):
        if project_source_path(identity) is not None:
            return rule_result(
                "R02_METAHUMAN_EXACT_PROJECT_SOURCE_PATH",
                ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "PROJECT_SOURCE_PATH_MANIFEST"],
            )
        companion = uasset_identity_for_companion(identity)
        if companion is not None and project_source_path(companion) is not None:
            return rule_result(
                "R03_METAHUMAN_COOKED_COMPANION",
                ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "PROJECT_SOURCE_PATH_MANIFEST"],
            )

    if identity in context["polyDerived"]:
        return rule_result(
            "R04_POLY_HAVEN_DERIVED_EXACT_RECEIPT_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "POLY_HAVEN_DERIVED_RUNTIME_RECEIPT"],
        )
    companion = uasset_identity_for_companion(identity)
    if companion is not None and companion in context["polyDerived"]:
        return rule_result(
            "R05_POLY_HAVEN_DERIVED_COOKED_COMPANION",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "POLY_HAVEN_DERIVED_RUNTIME_RECEIPT"],
        )
    if identity in context["polyOriginal"]:
        return rule_result(
            "R06_PINE_RIDGE_PROJECT_ORIGINAL_EXACT_RECEIPT_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "POLY_HAVEN_DERIVED_RUNTIME_RECEIPT"],
        )
    if identity.startswith("Engine/Plugins/") and identity.casefold().endswith(".uplugin"):
        return rule_result(
            "R07_ENGINE_PLUGIN_DESCRIPTOR_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        )
    if identity in RELEASE_METADATA_IDENTITIES:
        return rule_result(
            "R10_PROJECT_RELEASE_METADATA_EXACT_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        )
    if identity.startswith("Engine/"):
        return rule_result(
            "R08_ENGINE_PATH_NAMESPACE",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        )
    if project_source_path(identity) is not None:
        return rule_result(
            "R09_PROJECT_SOURCE_EXACT_PATH",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY", "PROJECT_SOURCE_PATH_MANIFEST"],
        )
    if identity.startswith("DiscGolfTour/") or identity in PROJECT_ROOT_BUILD_IDENTITIES:
        return rule_result(
            "R11_PROJECT_BUILD_NAMESPACE",
            ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        )
    return {
        "proposalDisposition": PATH_DISPOSITION,
        "technicalOriginCategory": None,
        "ruleId": None,
        "confidence": None,
        "technicalBasisIds": ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
    }


def proposal_record(record: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    scope = record["scope"]
    if scope in DIAGNOSTIC_SCOPES:
        result = {
            "proposalDisposition": DIAGNOSTIC_DISPOSITION,
            "technicalOriginCategory": None,
            "ruleId": None,
            "confidence": None,
            "technicalBasisIds": ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        }
    elif scope == "CONTAINER_ANONYMOUS_CHUNK":
        result = {
            "proposalDisposition": ANONYMOUS_DISPOSITION,
            "technicalOriginCategory": None,
            "ruleId": None,
            "confidence": None,
            "technicalBasisIds": ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
        }
    else:
        result = classify_path(record["identity"], context)
    return {
        "scope": scope,
        "container": record["container"],
        "identity": record["identity"],
        **result,
        "licenseStatus": LICENSE_STATUS,
        "reviewed": False,
        "shippingApproved": False,
        "legalReviewed": False,
        "legalApproved": False,
        "licenseApproved": False,
        "distributionApproved": False,
    }


def build_proposal(
    source_document: dict[str, Any], source_binding: dict[str, Any],
) -> dict[str, Any]:
    records = source_document["records"]
    context = technical_context(records)
    proposed_records = [proposal_record(record, context) for record in records]
    scope_counts = {
        scope: sum(record["scope"] == scope for record in proposed_records)
        for scope in SCOPE_ORDER
    }
    category_counts: dict[str, int] = {}
    for record in proposed_records:
        category = record["technicalOriginCategory"]
        if category is not None:
            category_counts[category] = category_counts.get(category, 0) + 1
    category_counts = dict(sorted(category_counts.items(), key=lambda item: item[0]))
    proposed_count = sum(
        record["proposalDisposition"] == TECHNICAL_DISPOSITION
        for record in proposed_records
    )
    unresolved_anonymous = sum(
        record["proposalDisposition"] == ANONYMOUS_DISPOSITION
        for record in proposed_records
    )
    unresolved_path = sum(
        record["proposalDisposition"] == PATH_DISPOSITION
        for record in proposed_records
    )
    technical_evidence = [
        {
            "technicalBasisId": "SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY",
            "kind": "CLI_BOUND_EXACT_IDENTITY_INVENTORY",
            "binding": copy.deepcopy(source_binding),
            "legalAuthority": False,
            "distributionAuthority": False,
        },
        {
            "technicalBasisId": "PROJECT_SOURCE_PATH_MANIFEST",
            "kind": "CANDIDATE_REFERENCED_LOCAL_SOURCE_FILES",
            "binding": context["projectSourceManifest"],
            "legalAuthority": False,
            "distributionAuthority": False,
        },
        {
            "technicalBasisId": "POLY_HAVEN_DERIVED_RUNTIME_RECEIPT",
            "kind": "LOCAL_TECHNICAL_SOURCE_TO_RUNTIME_RECEIPT",
            "binding": context["polyBinding"],
            "legalAuthority": False,
            "distributionAuthority": False,
        },
        {
            "technicalBasisId": "THIRD_PARTY_TECHNICAL_PATH_POLICY",
            "kind": "LOCAL_TECHNICAL_STAGED_PATH_MAPPING",
            "binding": context["thirdPartyBinding"],
            "legalAuthority": False,
            "distributionAuthority": False,
        },
    ]
    return {
        "schema": PROPOSAL_SCHEMA,
        "schemaVersion": 1,
        "candidateId": source_document["candidateId"],
        "state": PROPOSAL_STATE,
        "purpose": PROPOSAL_PURPOSE,
        "reviewed": False,
        "shippingApproved": False,
        "legalReviewed": False,
        "legalApproved": False,
        "licenseApproved": False,
        "distributionApproved": False,
        "authoritativeClassificationCompatible": False,
        "inputBinding": copy.deepcopy(source_binding),
        "scopePolicy": {
            "authoritativeCandidateScopes": list(AUTHORITATIVE_SCOPES),
            "technicalProposalEligibleScopes": list(CLASSIFICATION_ELIGIBLE_SCOPES),
            "diagnosticNonAuthoritativeScopes": list(DIAGNOSTIC_SCOPES),
            "alwaysUnresolvedScopes": list(UNRESOLVED_SCOPES),
            "ufsCanSupplyDistributionClassification": False,
            "anonymousChunksCanBePathClassified": False,
        },
        "technicalEvidence": technical_evidence,
        "ruleCatalog": copy.deepcopy(list(RULE_CATALOG)),
        "records": proposed_records,
        "summary": {
            "sourceRecordCount": len(proposed_records),
            "scopeCounts": scope_counts,
            "authoritativeCandidateRecordCount": sum(
                scope_counts[scope] for scope in AUTHORITATIVE_SCOPES
            ),
            "technicalProposalEligibleRecordCount": sum(
                scope_counts[scope] for scope in CLASSIFICATION_ELIGIBLE_SCOPES
            ),
            "technicalOriginProposedCount": proposed_count,
            "diagnosticNonAuthoritativeRecordCount": sum(
                scope_counts[scope] for scope in DIAGNOSTIC_SCOPES
            ),
            "unresolvedRecordCount": unresolved_anonymous + unresolved_path,
            "unresolvedAnonymousChunkCount": unresolved_anonymous,
            "unresolvedPathIdentityCount": unresolved_path,
            "reviewedRecordCount": 0,
            "shippingApprovedRecordCount": 0,
            "legalApprovedRecordCount": 0,
            "distributionApprovedRecordCount": 0,
            "technicalOriginCategoryCounts": category_counts,
        },
        "releaseBoundary": {
            "nonAuthoritativeTechnicalProposalOnly": True,
            "mayBeUsedAsAuthoritativeClassification": False,
            "independentReviewComplete": False,
            "legalReviewComplete": False,
            "distributionClearanceComplete": False,
            "shippingApprovalGranted": False,
            "releaseReady": False,
            "releaseUseAllowed": False,
            "blockerClosed": False,
        },
    }


def proposal_bytes(document: dict[str, Any]) -> bytes:
    return (json.dumps(document, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def validate_output_target(path: Path, source_path: Path) -> None:
    try:
        if path.resolve() == source_path.resolve():
            raise ProposalError("proposal output cannot overwrite its source draft")
    except OSError as exc:
        raise ProposalError(f"output path cannot be resolved: {exc}") from exc
    lowered = path.name.casefold()
    if any(lowered.startswith(prefix) for prefix in RESERVED_AUTHORITATIVE_OUTPUT_PREFIXES):
        raise ProposalError("proposal output name is reserved for authoritative provenance evidence")
    if "technicaloriginproposal" not in lowered:
        raise ProposalError("proposal output filename must identify the technical-origin proposal lane")
    if path.exists() or path.is_symlink():
        raise ProposalError("proposal output already exists; refusing overwrite")


def write_proposal(path: Path, document: dict[str, Any], source_path: Path) -> None:
    validate_output_target(path, source_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = proposal_bytes(document)
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise ProposalError("proposal output already exists; refusing overwrite") from exc


def test_source_document(candidate_id: str) -> dict[str, Any]:
    evidence = ["Config/DG_Session19StagedProvenancePolicy.json"]
    return {
        "schema": SOURCE_SCHEMA,
        "schemaVersion": 1,
        "candidateId": candidate_id,
        "state": SOURCE_STATE,
        "records": [
            {
                "scope": "ARCHIVE_FILE", "container": None,
                "identity": "DiscGolfTour.exe", "classification": "UNCLASSIFIED",
                "authorityId": "PENDING", "licenseId": "PENDING",
                "shippingApproved": False, "evidencePaths": evidence,
            },
            {
                "scope": "CONTAINER_NAMED_ENTRY", "container": "Game/Test.pak",
                "identity": "Engine/Content/Test.bin", "classification": "UNCLASSIFIED",
                "authorityId": "PENDING", "licenseId": "PENDING",
                "shippingApproved": False, "evidencePaths": evidence,
            },
            {
                "scope": "CONTAINER_ANONYMOUS_CHUNK", "container": "Game/Test.utoc",
                "identity": "<ShaderCode>#000000000000000000000009@0000000000000000:0000000000000001:0000000000000000000000000000000000000000",
                "classification": "UNCLASSIFIED", "authorityId": "PENDING",
                "licenseId": "PENDING", "shippingApproved": False,
                "evidencePaths": evidence,
            },
            {
                "scope": "UFS_ENTRY", "container": None,
                "identity": "Engine/Content/Diagnostic.bin", "classification": "UNCLASSIFIED",
                "authorityId": "PENDING", "licenseId": "PENDING",
                "shippingApproved": False, "evidencePaths": evidence,
            },
        ],
    }


def run_self_test() -> int:
    passed = 0
    expected = 15
    candidate_id = "S19_TestCandidate"
    with tempfile.TemporaryDirectory(prefix="dgt_origin_proposal_") as folder:
        root = Path(folder)
        draft_path = root / "draft.json"
        draft = test_source_document(candidate_id)
        draft_path.write_bytes(proposal_bytes(draft))
        digest = sha256_bytes(draft_path.read_bytes())

        loaded, binding = load_source_draft(draft_path, candidate_id, digest)
        proposal = build_proposal(loaded, binding)
        if set(proposal) == PROPOSAL_ROOT_KEYS:
            passed += 1
        if proposal["schema"] == PROPOSAL_SCHEMA:
            passed += 1
        if all(proposal[key] is False for key in (
            "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
            "licenseApproved", "distributionApproved",
        )):
            passed += 1
        if proposal["records"][0]["proposalDisposition"] == TECHNICAL_DISPOSITION:
            passed += 1
        if proposal["records"][2]["proposalDisposition"] == ANONYMOUS_DISPOSITION:
            passed += 1
        if proposal["records"][3]["proposalDisposition"] == DIAGNOSTIC_DISPOSITION:
            passed += 1
        if build_proposal(loaded, binding) == proposal:
            passed += 1

        try:
            load_source_draft(draft_path, candidate_id, "0" * 64)
        except ProposalError:
            passed += 1
        try:
            load_source_draft(draft_path, "S19_Other", digest)
        except ProposalError:
            passed += 1

        duplicate = copy.deepcopy(draft)
        duplicate["records"].append(copy.deepcopy(duplicate["records"][0]))
        duplicate_path = root / "duplicate.json"
        duplicate_path.write_bytes(proposal_bytes(duplicate))
        try:
            load_source_draft(
                duplicate_path, candidate_id, sha256_bytes(duplicate_path.read_bytes())
            )
        except ProposalError:
            passed += 1

        reviewed = copy.deepcopy(draft)
        reviewed["records"][0]["shippingApproved"] = True
        reviewed_path = root / "reviewed.json"
        reviewed_path.write_bytes(proposal_bytes(reviewed))
        try:
            load_source_draft(
                reviewed_path, candidate_id, sha256_bytes(reviewed_path.read_bytes())
            )
        except ProposalError:
            passed += 1

        duplicate_key_path = root / "duplicate-key.json"
        duplicate_key_path.write_text('{"schema":1,"schema":2}', encoding="utf-8")
        try:
            load_json_strict(duplicate_key_path)
        except ProposalError:
            passed += 1

        reserved = root / "StagedProvenanceClassificationDraft-test.json"
        try:
            validate_output_target(reserved, draft_path)
        except ProposalError:
            passed += 1

        output = root / "S19_TestCandidate_TechnicalOriginProposal.json"
        write_proposal(output, proposal, draft_path)
        original = output.read_bytes()
        if original == proposal_bytes(proposal):
            passed += 1
        try:
            write_proposal(output, proposal, draft_path)
        except ProposalError:
            if output.read_bytes() == original:
                passed += 1

    if passed != expected:
        print(f"FAIL technical-origin proposal generator self-test {passed}/{expected}")
        return 1
    print(f"PASS technical-origin proposal generator self-test {passed}/{expected}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--source-draft", type=Path)
    parser.add_argument("--source-draft-sha256")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    missing = [
        name for name, value in (
            ("--candidate-id", args.candidate_id),
            ("--source-draft", args.source_draft),
            ("--source-draft-sha256", args.source_draft_sha256),
        ) if value is None
    ]
    if missing:
        parser.error("required arguments: " + ", ".join(missing))
    if args.dry_run and args.output is not None:
        parser.error("--dry-run and --output are mutually exclusive")
    if not args.dry_run and args.output is None:
        parser.error("--output is required unless --dry-run is used")
    try:
        source_path = args.source_draft.resolve()
        source, binding = load_source_draft(
            source_path, args.candidate_id, args.source_draft_sha256
        )
        proposal = build_proposal(source, binding)
        data = proposal_bytes(proposal)
        if args.output is not None:
            write_proposal(args.output.resolve(), proposal, source_path)
    except ProposalError as exc:
        print(f"FAIL_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL: {exc}", file=sys.stderr)
        return 1

    summary = proposal["summary"]
    verb = "DRY_RUN" if args.dry_run else "WROTE"
    print(
        f"PASS_NON_AUTHORITATIVE_TECHNICAL_ORIGIN_PROPOSAL_{verb} "
        f"candidate={proposal['candidateId']} records={summary['sourceRecordCount']} "
        f"proposed={summary['technicalOriginProposedCount']} "
        f"anonymous_unresolved={summary['unresolvedAnonymousChunkCount']} "
        f"ufs_diagnostic={summary['diagnosticNonAuthoritativeRecordCount']} "
        f"proposal_bytes={len(data)} proposal_sha256={sha256_bytes(data)} "
        "reviewed=false shippingApproved=false legalApproved=false releaseReady=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
