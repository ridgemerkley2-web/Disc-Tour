#!/usr/bin/env python3
"""Build and validate candidate-bound Session 19 technical-origin attribution.

This lane closes no legal or release gate.  It converts only exact, hashed
technical mappings into attribution records.  Namespace inference and unknown
paths remain fail-closed. Anonymous IoStore chunks are attributed only at the
packaged-chunk technical-identity layer when an exact container/type rule and
the live offset, size, and SHA-1 identity all agree; underlying content rights
remain explicitly outside this technical classification.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any, Callable


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
SCHEMA = "DiscGolfTour.Session19TechnicalOriginAttributionAudit.v1"
STATE = "PASS_EXACT_TECHNICAL_ATTRIBUTION_LEGAL_REVIEW_PENDING"
ATTRIBUTED = "AUTHORITATIVELY_ATTRIBUTED_TECHNICAL_ORIGIN_ONLY"
UNRESOLVED_ANON = "UNRESOLVED_ANONYMOUS_IDENTITY"
UNRESOLVED_PATH = "UNRESOLVED_NO_AUTHORITATIVE_TECHNICAL_ORIGIN"
APPROVAL_KEYS = {
    "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
    "licenseApproved", "distributionApproved", "releaseReady",
    "releaseUseAllowed", "blockerClosed", "independentReviewComplete",
    "legalReviewComplete", "distributionClearanceComplete",
    "shippingApprovalGranted", "technicalAttributionIsLegalClassification",
}
HOST_PATH_RE = re.compile(
    r"(?:[A-Za-z]:[\\/]|\\\\[^\\/]+[\\/]|/(?:Users|home|mnt)/)", re.IGNORECASE
)
ANON_RE = re.compile(
    r"^<(?P<chunk_type>[^<>]+)>#(?P<chunk_id>[0-9A-F]{24})@"
    r"(?P<offset>[0-9]{16}):(?P<size>[0-9]{16}):(?P<sha1>[0-9A-F]{40})$"
)
CANDIDATE_RE = re.compile(r"^S19_WindowsShipping_[0-9]{8}T[0-9]{6}Z_[0-9a-f]{12}$")


class AttributionError(RuntimeError):
    pass


def strict_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AttributionError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8-sig", errors="strict"),
            object_pairs_hook=strict_pairs,
            parse_constant=lambda value: (_ for _ in ()).throw(
                AttributionError(f"non-finite JSON number: {value}")
            ),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise AttributionError(f"cannot strictly load {path.name}: {exc}") from exc


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def file_binding(path: Path, display_path: str) -> dict[str, Any]:
    try:
        before = path.stat()
        data = path.read_bytes()
        after = path.stat()
    except OSError as exc:
        raise AttributionError(f"cannot bind required file {display_path}: {exc}") from exc
    if not path.is_file() or path.is_symlink():
        raise AttributionError(f"required binding is not a regular non-symlink file: {display_path}")
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise AttributionError(f"required binding changed during hashing: {display_path}")
    return {"path": display_path, "bytes": len(data), "sha256": sha256_bytes(data)}


def import_script(name: str, path: Path) -> Any:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AttributionError(f"cannot import required helper: {path.name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def canonical_relative(value: Any) -> str | None:
    if type(value) is not str or not value or "\\" in value or "\x00" in value:
        return None
    path = PurePosixPath(value)
    if path.is_absolute() or str(path) != value or any(part in ("", ".", "..") for part in path.parts):
        return None
    return value


def source_for_identity(
    identity: str, project_root: Path, engine_root: Path, policy: dict[str, Any],
) -> tuple[str, Path, bool] | None:
    roots: list[tuple[str, str, Path]] = [
        ("DiscGolfTour/", "PROJECT", project_root),
        ("Engine/", "ENGINE", engine_root),
    ]
    for prefix, owner, root in roots:
        if identity.startswith(prefix):
            relative = identity[len(prefix):]
            exact = root.joinpath(*PurePosixPath(relative).parts)
            if exact.is_file() and not exact.is_symlink():
                return owner, exact, False
            lowered = identity.casefold()
            for companion in policy["classificationRules"]["sourceCompanionExtensions"]:
                if lowered.endswith(companion):
                    stem = identity[:-len(companion)]
                    for asset_ext in policy["classificationRules"]["sourceAssetExtensions"]:
                        source_identity = stem + asset_ext
                        relative_source = source_identity[len(prefix):]
                        source = root.joinpath(*PurePosixPath(relative_source).parts)
                        if source.is_file() and not source.is_symlink():
                            return owner, source, True
    return None


def generated_kind(identity: str, policy: dict[str, Any]) -> str | None:
    rules = policy["classificationRules"]
    if identity in rules["exactGeneratedIdentities"]:
        if identity in {"Manifest_NonUFSFiles_Win64.txt", "Manifest_UFSFiles_Win64.txt", "NOTICES.txt"}:
            return "PROJECT_RELEASE_METADATA_EXACT_CANDIDATE"
        if identity.startswith("Engine/"):
            return "EPIC_ENGINE_GENERATED_RUNTIME_OUTPUT"
        return "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE"
    if identity in rules["exactGeneratedEngineFontFaces"]:
        return "EPIC_ENGINE_GENERATED_RUNTIME_OUTPUT"
    for expression in rules["generatedIdentityPatterns"]:
        if re.fullmatch(expression, identity):
            return (
                "EPIC_ENGINE_GENERATED_RUNTIME_OUTPUT"
                if identity.startswith("Engine/")
                else "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE"
            )
    return None


def anonymous_chunk_origin(
    container: Any, identity: Any, policy: dict[str, Any],
) -> dict[str, Any] | None:
    """Resolve only an exact live IoStore packaged-chunk identity.

    The result classifies the generated build artifact, never ownership or
    licensing of source content that contributed bytes to that artifact.
    """
    if type(container) is not str or canonical_relative(container) is None:
        return None
    if type(identity) is not str:
        return None
    match = ANON_RE.fullmatch(identity)
    if match is None:
        return None
    if generated_kind(container, policy) != "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE":
        return None
    chunk_type = match.group("chunk_type")
    for rule in policy["classificationRules"]["anonymousChunkTechnicalOriginRules"]:
        if rule["container"] == container and chunk_type in rule["chunkTypes"]:
            return rule
    return None


def validate_policy_document(policy: dict[str, Any]) -> None:
    expected_root_keys = {
        "schema", "schemaVersion", "session", "policyId", "state", "reviewed",
        "shippingApproved", "legalReviewed", "legalApproved", "licenseApproved",
        "distributionApproved", "releaseReady", "inputs", "classificationRules",
        "originCategories", "licenseSourceCategories", "releaseBoundary",
    }
    if type(policy) is not dict or set(policy) != expected_root_keys:
        raise AttributionError("technical-origin attribution policy root keys differ")
    expected_header = {
        "schema": "DiscGolfTour.Session19TechnicalOriginAttributionPolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "candidate_bound_exact_technical_origin_attribution_v1",
        "state": "IMPLEMENTED_TECHNICAL_ATTRIBUTION_LEGAL_REVIEW_PENDING",
    }
    if any(type(policy.get(key)) is not type(value) or policy.get(key) != value for key, value in expected_header.items()):
        raise AttributionError("technical-origin attribution policy header differs")
    for key in (
        "reviewed", "shippingApproved", "legalReviewed", "legalApproved",
        "licenseApproved", "distributionApproved", "releaseReady",
    ):
        if policy.get(key) is not False:
            raise AttributionError(f"technical-origin policy overclaims {key}")
    expected_inputs = {
        "sourceDraftSchema": "DiscGolfTour.Session19StagedProvenanceClassification.v1",
        "technicalProposalSchema": "DiscGolfTour.Session19TechnicalOriginProposal.v1",
        "candidateContentSchema": "DiscGolfTour.Session19CandidateContentAuditReceipt.v1",
        "thirdPartyAuthoritySchema": "DiscGolfTour.Session19RuntimeThirdPartyLicenseAuthority.v1",
        "buildReceiptTargetName": "DiscGolfTour",
        "buildReceiptPlatform": "Win64",
        "buildReceiptConfiguration": "Shipping",
        "engineMajor": 5,
        "engineMinor": 8,
        "expectedAuditedIdentityCount": 7158,
    }
    if policy.get("inputs") != expected_inputs:
        raise AttributionError("technical-origin attribution input policy differs")
    expected_rules = {
        "anonymousChunkDisposition": UNRESOLVED_ANON,
        "resolvedAnonymousChunkDisposition": ATTRIBUTED,
        "anonymousChunkTechnicalOriginRules": [
            {
                "container": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
                "chunkTypes": ["ContainerHeader", "ShaderCode"],
                "originCategory": "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE",
                "licenseSourceCategory": "MIXED_BUILD_OUTPUT_RIGHTS_REVIEW_PENDING",
                "originRuleId": "A09_EXACT_CANDIDATE_IOSTORE_GENERATED_CHUNK_IDENTITY",
            },
            {
                "container": "DiscGolfTour/Content/Paks/global.utoc",
                "chunkTypes": ["ScriptObjects"],
                "originCategory": "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE",
                "licenseSourceCategory": "MIXED_BUILD_OUTPUT_RIGHTS_REVIEW_PENDING",
                "originRuleId": "A09_EXACT_CANDIDATE_IOSTORE_GENERATED_CHUNK_IDENTITY",
            },
        ],
        "requireAnonymousChunkExactTypeContainerRule": True,
        "requireAnonymousChunkOffsetSizeSha1Binding": True,
        "anonymousChunkAttributionScope": (
            "PACKAGED_CHUNK_TECHNICAL_IDENTITY_ONLY_UNDERLYING_CONTENT_RIGHTS_NOT_CLASSIFIED"
        ),
        "unknownPathDisposition": UNRESOLVED_PATH,
        "authoritativelyAttributedDisposition": ATTRIBUTED,
        "sourceCompanionExtensions": [".ubulk", ".uexp", ".uptnl"],
        "sourceAssetExtensions": [".uasset", ".umap"],
        "exactGeneratedIdentities": [
            "DiscGolfTour.exe", "DiscGolfTour/AssetRegistry.bin",
            "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe",
            "DiscGolfTour/Plugins/DiscGolfTour.upluginmanifest",
            "Engine/Config/StagedBuild_DiscGolfTour.ini",
            "Manifest_NonUFSFiles_Win64.txt", "Manifest_UFSFiles_Win64.txt", "NOTICES.txt",
        ],
        "exactGeneratedEngineFontFaces": [
            "Engine/Content/EngineFonts/Faces/DroidSansFallback.ufont",
            "Engine/Content/EngineFonts/Faces/RobotoBold.ufont",
            "Engine/Content/EngineFonts/Faces/RobotoBoldItalic.ufont",
            "Engine/Content/EngineFonts/Faces/RobotoItalic.ufont",
            "Engine/Content/EngineFonts/Faces/RobotoLight.ufont",
            "Engine/Content/EngineFonts/Faces/RobotoRegular.ufont",
        ],
        "generatedIdentityPatterns": [
            r"^DiscGolfTour/Content/Paks/(?:DiscGolfTour-Windows|global)\.(?:pak|ucas|utoc)$",
            r"^DiscGolfTour/Content/ShaderArchive-(?:DiscGolfTour|Global)-PCD3D_SM[56]-PCD3D_SM[56]\.ushaderbytecode$",
            r"^DiscGolfTour/Content/ShaderTypeInfo-(?:DiscGolfTour|Global)-PCD3D_SM[56]-PCD3D_SM[56]\.stinfo$",
            r"^Engine/GlobalShaderCache-PCD3D_SM[56]\.bin$",
        ],
        "forbidNamespaceOnlyAttribution": True,
        "requireExactLocalFileOrExactGeneratedRule": True,
        "requireLiveContainerReinventory": True,
        "recordHostPaths": False,
    }
    if policy.get("classificationRules") != expected_rules:
        raise AttributionError("technical-origin attribution classification rules differ")
    expected_origins = [
        "EPIC_ENGINE_COOKED_COMPANION_EXACT_SOURCE",
        "EPIC_ENGINE_GENERATED_RUNTIME_OUTPUT",
        "EPIC_ENGINE_INSTALL_FILE_EXACT",
        "EPIC_ENGINE_PLUGIN_COOKED_COMPANION_EXACT_SOURCE",
        "EPIC_ENGINE_PLUGIN_INSTALL_FILE_EXACT",
        "EPIC_METAHUMAN_COOKED_COMPANION_EXACT_SOURCE",
        "EPIC_METAHUMAN_PROJECT_FILE_EXACT",
        "POLY_HAVEN_DERIVED_COOKED_COMPANION_EXACT_RECEIPT",
        "POLY_HAVEN_DERIVED_FILE_EXACT_RECEIPT",
        "PROJECT_COOKED_COMPANION_EXACT_SOURCE",
        "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE",
        "PROJECT_RELEASE_METADATA_EXACT_CANDIDATE",
        "PROJECT_SOURCE_FILE_EXACT",
        "THIRD_PARTY_RUNTIME_EXACT_LICENSE_AUTHORITY",
    ]
    expected_license_sources = [
        "EPIC_ENGINE_TERMS_REVIEW_PENDING",
        "EPIC_METAHUMAN_TERMS_REVIEW_PENDING",
        "MIXED_BUILD_OUTPUT_RIGHTS_REVIEW_PENDING",
        "POLY_HAVEN_CC0_DERIVED_RECEIPT_REVIEW_PENDING",
        "PROJECT_RIGHTS_AUTHORITY_REVIEW_PENDING",
        "THIRD_PARTY_LOCAL_LICENSE_TEXT_LEGAL_REVIEW_PENDING",
        "THIRD_PARTY_LOCAL_REFERENCE_LEGAL_REVIEW_PENDING",
        "THIRD_PARTY_SPECIAL_REVIEW_PENDING",
        "UNRESOLVED_NO_LICENSE_SOURCE",
    ]
    if policy.get("originCategories") != expected_origins:
        raise AttributionError("technical-origin attribution origin categories differ")
    if policy.get("licenseSourceCategories") != expected_license_sources:
        raise AttributionError("technical-origin attribution license-source categories differ")
    expected_boundary = {
        "technicalAttributionIsLegalClassification": False,
        "independentReviewComplete": False,
        "legalReviewComplete": False,
        "distributionClearanceComplete": False,
        "shippingApprovalGranted": False,
        "releaseReady": False,
        "releaseUseAllowed": False,
        "blockerClosed": False,
    }
    if policy.get("releaseBoundary") != expected_boundary:
        raise AttributionError("technical-origin attribution release boundary differs")


def manifest_binding(entries: dict[str, tuple[int, str]], kind: str) -> dict[str, Any]:
    rows = "".join(
        f"{identity}\t{size}\t{digest}\n"
        for identity, (size, digest) in sorted(entries.items(), key=lambda item: (item[0].casefold(), item[0]))
    ).encode("utf-8")
    return {
        "kind": kind,
        "fileCount": len(entries),
        "bytes": sum(size for size, _ in entries.values()),
        "canonicalManifestSha256": sha256_bytes(rows),
        "manifestFormat": "UTF8_SORTED_TECHNICAL_IDENTITY_TAB_BYTES_TAB_SHA256_LF",
        "hostPathsRecorded": False,
    }


def verify_poly_receipt(path: Path, project_root: Path) -> tuple[set[str], set[str], dict[str, Any]]:
    document = load_json(path)
    if type(document) is not dict or document.get("schema") != "DiscGolfTour.PineRidgeDerivedAssetReceipt.v1":
        raise AttributionError("Poly Haven derived receipt schema is invalid")
    if document.get("status") != "COMPLETE_DURABLE_SOURCE_TO_UASSET_RECEIPT":
        raise AttributionError("Poly Haven derived receipt is not complete")
    license_value = document.get("license")
    if type(license_value) is not dict or license_value.get("spdxExpression") != "CC0-1.0":
        raise AttributionError("Poly Haven derived receipt lacks its CC0 technical license source")
    derived: set[str] = set()
    original: set[str] = set()
    for key, target in (("derivedRuntimeArtifacts", derived), ("excludedProjectOriginalRuntimeArtifacts", original)):
        records = document.get(key)
        if type(records) is not list:
            raise AttributionError(f"Poly Haven receipt {key} is malformed")
        for index, record in enumerate(records):
            if type(record) is not dict or canonical_relative(record.get("relativePath")) is None:
                raise AttributionError(f"Poly Haven receipt {key}[{index}] is malformed")
            relative = record["relativePath"]
            source = project_root.joinpath(*PurePosixPath(relative).parts)
            binding = file_binding(source, relative)
            if binding["bytes"] != record.get("byteSize") or binding["sha256"] != record.get("sha256"):
                raise AttributionError(f"Poly Haven receipt file binding drifted: {relative}")
            target.add("DiscGolfTour/" + relative)
    return derived, original, file_binding(
        path, "SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"
    )


def verify_third_party_authority(
    path: Path, engine_root: Path, project_root: Path,
) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    document = load_json(path)
    if type(document) is not dict or document.get("schema") != "DiscGolfTour.Session19RuntimeThirdPartyLicenseAuthority.v1":
        raise AttributionError("third-party authority schema is invalid")
    if document.get("coverage", {}).get("technicalMappingComplete") is not True:
        raise AttributionError("third-party authority technical mapping is incomplete")
    if document.get("legalApproval") is True or document.get("distributionClearance") is True:
        raise AttributionError("third-party authority unexpectedly claims approval")
    mapping: dict[str, dict[str, Any]] = {}
    components = document.get("components")
    if type(components) is not list:
        raise AttributionError("third-party authority components are malformed")
    for component in components:
        if type(component) is not dict:
            raise AttributionError("third-party authority component is malformed")
        status = component.get("licenseEvidenceStatus")
        if status == "LOCAL_LICENSE_TEXT_PRESENT":
            category = "THIRD_PARTY_LOCAL_LICENSE_TEXT_LEGAL_REVIEW_PENDING"
        elif type(status) is str and "REFERENCE" in status:
            category = "THIRD_PARTY_LOCAL_REFERENCE_LEGAL_REVIEW_PENDING"
        else:
            category = "THIRD_PARTY_SPECIAL_REVIEW_PENDING"
        for authority_file in component.get("authorityFiles", []):
            if type(authority_file) is not dict or canonical_relative(authority_file.get("path")) is None:
                raise AttributionError("third-party authority file record is malformed")
            relative = authority_file["path"]
            if relative.startswith("Engine/"):
                absolute = engine_root.joinpath(*PurePosixPath(relative[len("Engine/"):]).parts)
            else:
                absolute = project_root.joinpath(*PurePosixPath(relative).parts)
            binding = file_binding(absolute, relative)
            if binding["bytes"] != authority_file.get("bytes") or binding["sha256"] != authority_file.get("sha256"):
                raise AttributionError(f"third-party authority file drifted: {relative}")
        for identity in component.get("stagedRuntimeDlls", []):
            if canonical_relative(identity) is None or identity in mapping:
                raise AttributionError("third-party staged identity is malformed or duplicated")
            mapping[identity] = {
                "componentId": component.get("componentId"),
                "technicalAuthorityId": component.get("classificationAuthorityId"),
                "technicalLicenseSourceId": component.get("classificationLicenseId"),
                "licenseSourceCategory": category,
            }
    expected = document.get("coverage", {}).get("supportedStagedDllIdentityCount")
    if type(expected) is not int or expected != len(mapping):
        raise AttributionError("third-party authority staged identity count drifted")
    return mapping, file_binding(path, "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json")


def verify_build_receipt(path: Path) -> dict[str, Any]:
    document = load_json(path)
    required = {
        "TargetName": "DiscGolfTour", "Platform": "Win64", "Configuration": "Shipping"
    }
    if type(document) is not dict or any(document.get(key) != value for key, value in required.items()):
        raise AttributionError("build receipt target binding is not DiscGolfTour Win64 Shipping")
    version = document.get("Version")
    if type(version) is not dict or version.get("MajorVersion") != 5 or version.get("MinorVersion") != 8:
        raise AttributionError("build receipt engine version is not UE 5.8")
    if type(document.get("BuildProducts")) is not list or type(document.get("RuntimeDependencies")) is not list:
        raise AttributionError("build receipt product inventories are malformed")
    if path.name != "DiscGolfTour-Win64-Shipping.target":
        raise AttributionError("build receipt filename is not the explicit Win64 Shipping receipt")
    binding = file_binding(path, "Binaries/Win64/DiscGolfTour-Win64-Shipping.target")
    binding.update({
        "targetName": document["TargetName"], "platform": document["Platform"],
        "configuration": document["Configuration"],
        "buildProductCount": len(document["BuildProducts"]),
        "runtimeDependencyCount": len(document["RuntimeDependencies"]),
        "hostPathsRecorded": False,
    })
    return binding


def exact_record_key(record: dict[str, Any]) -> tuple[str, str, str]:
    return (
        str(record.get("scope")), str(record.get("container") or "").casefold(),
        str(record.get("identity")).casefold(),
    )


def validate_candidate_content(
    document: dict[str, Any], candidate_id: str, live_audit: dict[str, Any],
) -> None:
    if document.get("schema") != "DiscGolfTour.Session19CandidateContentAuditReceipt.v1":
        raise AttributionError("candidate-content receipt schema is invalid")
    if document.get("runId") != candidate_id or document.get("state") != "PASS_BOUNDED_STAGED_CONTENT_AUDIT":
        raise AttributionError("candidate-content receipt does not bind the requested candidate")
    archive = document.get("archive", {})
    live_archive = live_audit["archive"]
    for key in ("fileCount", "bytes", "canonicalManifestSha256"):
        if archive.get(key) != live_archive.get(key):
            raise AttributionError(f"candidate-content archive binding differs: {key}")
    inventory = document.get("inventory", {})
    expected_counts = {
        "ufsEntryCount": len(live_audit["stageManifests"]["ufs"]["entries"]),
        "nonUfsEntryCount": len(live_audit["stageManifests"]["nonUfs"]["entries"]),
        "containerCount": len(live_audit["containers"]),
        "containerNamedEntryCount": sum(
            entry["scope"] == "CONTAINER_NAMED_ENTRY"
            for container in live_audit["containers"] for entry in container["entries"]
        ),
    }
    for key, expected in expected_counts.items():
        if inventory.get(key) != expected:
            raise AttributionError(f"candidate-content inventory binding differs: {key}")


def live_inventory(
    provenance: Any, archive: Path, uat_log: Path, candidate_id: str,
    staged_policy_path: Path, project_root: Path, unrealpak: Path,
    expected_identity_count: int,
) -> tuple[dict[str, Any], dict[str, Any]]:
    policy = provenance.load_json_strict(staged_policy_path)
    receipt, template = provenance.audit_candidate(
        archive_root=archive, uat_log=uat_log, classification_path=None,
        candidate_id=candidate_id, policy=policy, project_root=project_root,
        unrealpak=unrealpak,
    )
    issues = receipt.get("issues", [])
    allowed = {"CLASSIFICATION_MANIFEST_MISSING", "UNCLASSIFIED_IDENTITY"}
    unexpected = [item for item in issues if item.get("code") not in allowed]
    observed = {item.get("code") for item in issues}
    if unexpected or observed != allowed:
        detail = ",".join(sorted({str(item.get("code")) for item in unexpected})) or "classification-boundary-drift"
        raise AttributionError(f"live staged inventory failed outside the expected classification blocker: {detail}")
    if receipt.get("classification", {}).get("discoveredIdentityCount") != expected_identity_count:
        raise AttributionError(
            "live candidate no longer contains exactly "
            f"{expected_identity_count:,} audited identities"
        )
    return receipt, template


def proposal_is_exact(origin: Any, proposal_path: Path, draft: dict[str, Any], binding: dict[str, Any]) -> dict[str, Any]:
    expected = origin.build_proposal(draft, binding)
    actual = load_json(proposal_path)
    expected_bytes = origin.proposal_bytes(expected)
    try:
        actual_bytes = proposal_path.read_bytes()
    except OSError as exc:
        raise AttributionError(f"cannot read technical-origin proposal: {exc}") from exc
    if actual_bytes != expected_bytes or actual != expected:
        raise AttributionError("technical-origin proposal is not the deterministic regeneration of the source draft")
    return actual


def classify_record(
    record: dict[str, Any], proposal_record: dict[str, Any], project_root: Path,
    engine_root: Path, policy: dict[str, Any], third_party: dict[str, dict[str, Any]],
    poly_derived: set[str], poly_original: set[str], source_manifest: dict[str, tuple[int, str]],
) -> dict[str, Any]:
    scope = record["scope"]
    identity = record["identity"]
    base = {"scope": scope, "container": record.get("container"), "identity": identity}
    if scope == "CONTAINER_ANONYMOUS_CHUNK":
        match = ANON_RE.fullmatch(identity)
        if match is None:
            raise AttributionError("anonymous source identity is malformed")
        anonymous_rule = anonymous_chunk_origin(record.get("container"), identity, policy)
        if anonymous_rule is not None:
            category = anonymous_rule["originCategory"]
            license_category = anonymous_rule["licenseSourceCategory"]
            if category not in policy["originCategories"]:
                raise AttributionError("anonymous chunk origin category is outside policy")
            if license_category not in policy["licenseSourceCategories"]:
                raise AttributionError("anonymous chunk license-source category is outside policy")
            return {
                **base, "technicalDisposition": ATTRIBUTED,
                "originCategory": category,
                "originRuleId": anonymous_rule["originRuleId"],
                "licenseSourceCategory": license_category,
                "technicalAuthorityIds": [
                    "LIVE_CANDIDATE_IOSTORE_REINVENTORY",
                    "EXACT_CONTAINER_TYPE_CHUNKID_OFFSET_SIZE_SHA1_IDENTITY",
                    "ARCHIVE_CANONICAL_MANIFEST",
                    "WIN64_SHIPPING_BUILD_RECEIPT",
                    "UAT_LOG",
                    policy["classificationRules"]["anonymousChunkAttributionScope"],
                ],
                "sourceProposalRuleId": proposal_record.get("ruleId"),
                "sourceProposalConfidence": proposal_record.get("confidence"),
                "reviewed": False, "shippingApproved": False,
                "legalReviewed": False, "legalApproved": False,
                "licenseApproved": False, "distributionApproved": False,
            }
        return {
            **base, "technicalDisposition": UNRESOLVED_ANON,
            "originCategory": None, "originRuleId": None,
            "licenseSourceCategory": "UNRESOLVED_NO_LICENSE_SOURCE",
            "technicalAuthorityIds": [], "reviewed": False,
            "shippingApproved": False, "legalReviewed": False,
            "legalApproved": False, "licenseApproved": False,
            "distributionApproved": False,
        }
    if canonical_relative(identity) is None:
        raise AttributionError("path-bearing source identity is non-canonical")

    source = source_for_identity(identity, project_root, engine_root, policy)
    third = third_party.get(identity)
    companion_uasset = None
    for extension in policy["classificationRules"]["sourceCompanionExtensions"]:
        if identity.casefold().endswith(extension):
            companion_uasset = identity[:-len(extension)] + ".uasset"
            break
    if third is not None:
        category = "THIRD_PARTY_RUNTIME_EXACT_LICENSE_AUTHORITY"
        license_category = third["licenseSourceCategory"]
        rule = "A01_EXACT_THIRD_PARTY_LICENSE_AUTHORITY_IDENTITY"
        authorities = ["THIRD_PARTY_LICENSE_AUTHORITY", str(third["technicalAuthorityId"])]
    elif identity in poly_derived:
        category = "POLY_HAVEN_DERIVED_FILE_EXACT_RECEIPT"
        license_category = "POLY_HAVEN_CC0_DERIVED_RECEIPT_REVIEW_PENDING"
        rule = "A02_EXACT_POLY_HAVEN_DERIVED_RECEIPT_IDENTITY"
        authorities = ["POLY_HAVEN_DERIVED_RUNTIME_RECEIPT"]
    elif companion_uasset in poly_derived:
        category = "POLY_HAVEN_DERIVED_COOKED_COMPANION_EXACT_RECEIPT"
        license_category = "POLY_HAVEN_CC0_DERIVED_RECEIPT_REVIEW_PENDING"
        rule = "A03_EXACT_COMPANION_OF_POLY_HAVEN_RECEIPT_IDENTITY"
        authorities = ["POLY_HAVEN_DERIVED_RUNTIME_RECEIPT"]
    elif source is not None:
        owner, source_path, companion = source
        source_identity = identity
        if companion:
            for extension in policy["classificationRules"]["sourceCompanionExtensions"]:
                if source_identity.casefold().endswith(extension):
                    source_identity = source_identity[:-len(extension)] + source_path.suffix.lower()
                    break
        binding = file_binding(source_path, source_identity)
        source_manifest[source_identity] = (binding["bytes"], binding["sha256"])
        is_meta = identity.startswith("DiscGolfTour/Content/DiscGolf/Characters/MetaHuman/")
        is_plugin = identity.startswith("Engine/Plugins/")
        if is_meta:
            category = (
                "EPIC_METAHUMAN_COOKED_COMPANION_EXACT_SOURCE" if companion
                else "EPIC_METAHUMAN_PROJECT_FILE_EXACT"
            )
            license_category = "EPIC_METAHUMAN_TERMS_REVIEW_PENDING"
            rule = "A04_EXACT_METAHUMAN_PROJECT_SOURCE_OR_COMPANION"
            authorities = ["PROJECT_SOURCE_HASH_MANIFEST", "METAHUMAN_TECHNICAL_NAMESPACE"]
        elif owner == "ENGINE" and is_plugin:
            category = (
                "EPIC_ENGINE_PLUGIN_COOKED_COMPANION_EXACT_SOURCE" if companion
                else "EPIC_ENGINE_PLUGIN_INSTALL_FILE_EXACT"
            )
            license_category = "EPIC_ENGINE_TERMS_REVIEW_PENDING"
            rule = "A05_EXACT_ENGINE_PLUGIN_INSTALL_SOURCE_OR_COMPANION"
            authorities = ["ENGINE_INSTALL_HASH_MANIFEST", "ENGINE_BUILD_VERSION"]
        elif owner == "ENGINE":
            category = (
                "EPIC_ENGINE_COOKED_COMPANION_EXACT_SOURCE" if companion
                else "EPIC_ENGINE_INSTALL_FILE_EXACT"
            )
            license_category = "EPIC_ENGINE_TERMS_REVIEW_PENDING"
            rule = "A06_EXACT_ENGINE_INSTALL_SOURCE_OR_COMPANION"
            authorities = ["ENGINE_INSTALL_HASH_MANIFEST", "ENGINE_BUILD_VERSION"]
        else:
            category = (
                "PROJECT_COOKED_COMPANION_EXACT_SOURCE" if companion
                else "PROJECT_SOURCE_FILE_EXACT"
            )
            license_category = "PROJECT_RIGHTS_AUTHORITY_REVIEW_PENDING"
            rule = "A07_EXACT_PROJECT_SOURCE_OR_COMPANION"
            authorities = ["PROJECT_SOURCE_HASH_MANIFEST"]
            if identity in poly_original or companion_uasset in poly_original:
                authorities.append("PINE_RIDGE_PROJECT_ORIGINAL_RECEIPT_PARTITION")
    else:
        category = generated_kind(identity, policy)
        if category is None:
            return {
                **base, "technicalDisposition": UNRESOLVED_PATH,
                "originCategory": None, "originRuleId": None,
                "licenseSourceCategory": "UNRESOLVED_NO_LICENSE_SOURCE",
                "technicalAuthorityIds": ["SOURCE_DRAFT_EXACT_IDENTITY_INVENTORY"],
                "reviewed": False, "shippingApproved": False,
                "legalReviewed": False, "legalApproved": False,
                "licenseApproved": False, "distributionApproved": False,
            }
        rule = "A08_EXACT_ALLOWLISTED_UE_GENERATED_CANDIDATE_IDENTITY"
        authorities = ["LIVE_CANDIDATE_CONTAINER_INVENTORY", "WIN64_SHIPPING_BUILD_RECEIPT", "UAT_LOG"]
        if category == "EPIC_ENGINE_GENERATED_RUNTIME_OUTPUT":
            license_category = "EPIC_ENGINE_TERMS_REVIEW_PENDING"
        elif category == "PROJECT_RELEASE_METADATA_EXACT_CANDIDATE":
            license_category = "PROJECT_RIGHTS_AUTHORITY_REVIEW_PENDING"
        else:
            license_category = "MIXED_BUILD_OUTPUT_RIGHTS_REVIEW_PENDING"

    if category not in policy["originCategories"]:
        raise AttributionError(f"origin category is outside policy: {category}")
    if license_category not in policy["licenseSourceCategories"]:
        raise AttributionError(f"license-source category is outside policy: {license_category}")
    return {
        **base, "technicalDisposition": ATTRIBUTED,
        "originCategory": category, "originRuleId": rule,
        "licenseSourceCategory": license_category,
        "technicalAuthorityIds": authorities,
        "sourceProposalRuleId": proposal_record.get("ruleId"),
        "sourceProposalConfidence": proposal_record.get("confidence"),
        "reviewed": False, "shippingApproved": False,
        "legalReviewed": False, "legalApproved": False,
        "licenseApproved": False, "distributionApproved": False,
    }


def recursive_strings(value: Any) -> list[str]:
    result: list[str] = []
    if type(value) is str:
        result.append(value)
    elif type(value) is list:
        for item in value:
            result.extend(recursive_strings(item))
    elif type(value) is dict:
        for key, item in value.items():
            result.append(str(key))
            result.extend(recursive_strings(item))
    return result


def security_problems(document: dict[str, Any]) -> list[str]:
    problems: list[str] = []
    def walk(value: Any, trail: str) -> None:
        if type(value) is dict:
            for key, item in value.items():
                if key in APPROVAL_KEYS and item is not False:
                    problems.append(f"approval boundary changed at {trail}.{key}")
                walk(item, f"{trail}.{key}")
        elif type(value) is list:
            for index, item in enumerate(value):
                walk(item, f"{trail}[{index}]")
    walk(document, "root")
    for value in recursive_strings(document):
        if HOST_PATH_RE.search(value):
            problems.append("host path leakage detected")
            break
    if document.get("schema") != SCHEMA:
        problems.append("audit schema changed")
    if document.get("authoritativeProvenanceClassificationCompatible") is not False:
        problems.append("authoritative provenance compatibility must remain false")
    records = document.get("records")
    if type(records) is not list:
        problems.append("records are missing")
        return problems
    for index, record in enumerate(records):
        if type(record) is not dict:
            problems.append(f"record {index} is malformed")
            continue
        disposition = record.get("technicalDisposition")
        if disposition == ATTRIBUTED:
            if record.get("originCategory") is None or record.get("licenseSourceCategory") == "UNRESOLVED_NO_LICENSE_SOURCE":
                problems.append(f"record {index} attributed without explicit origin/license source")
        elif disposition in (UNRESOLVED_ANON, UNRESOLVED_PATH):
            if record.get("originCategory") is not None:
                problems.append(f"record {index} unresolved but has an origin category")
        else:
            problems.append(f"record {index} has an unknown disposition")
    return problems


def build_document(args: argparse.Namespace) -> dict[str, Any]:
    if not CANDIDATE_RE.fullmatch(args.candidate_id):
        raise AttributionError("candidate ID is not a canonical Session 19 Shipping ID")
    project_root = args.project_root.resolve()
    engine_root = args.engine_root.resolve()
    archive = args.archive.resolve()
    policy = load_json(args.policy.resolve())
    validate_policy_document(policy)

    provenance = import_script("dg_staged_provenance_attribution", SCRIPT_DIR / "generate_dg_session19_staged_provenance.py")
    origin = import_script("dg_technical_origin_proposal_attribution", SCRIPT_DIR / "generate_dg_session19_technical_origin_proposal.py")
    live, template = live_inventory(
        provenance, archive, args.uat_log.resolve(), args.candidate_id,
        args.staged_policy.resolve(), project_root, args.unrealpak.resolve(),
        policy["inputs"]["expectedAuditedIdentityCount"],
    )
    draft, draft_binding = origin.load_source_draft(
        args.source_draft.resolve(), args.candidate_id, args.source_draft_sha256
    )
    live_records = template.get("records")
    if type(live_records) is not list or [exact_record_key(value) for value in live_records] != [exact_record_key(value) for value in draft["records"]]:
        raise AttributionError("live UFS/NonUFS/Pak/IoStore identities differ from the source draft")
    proposal = proposal_is_exact(origin, args.technical_proposal.resolve(), draft, draft_binding)
    if len(proposal.get("records", [])) != len(draft["records"]):
        raise AttributionError("technical-origin proposal record count differs")

    candidate_content = load_json(args.candidate_content.resolve())
    validate_candidate_content(candidate_content, args.candidate_id, live)
    third_party, third_binding = verify_third_party_authority(
        args.third_party_authority.resolve(), engine_root, project_root
    )
    poly_derived, poly_original, poly_binding = verify_poly_receipt(
        args.poly_receipt.resolve(), project_root
    )
    build_binding = verify_build_receipt(args.build_receipt.resolve())
    engine_build_binding = file_binding(engine_root / "Build/Build.version", "Engine/Build/Build.version")
    engine_build = load_json(engine_root / "Build/Build.version")
    if engine_build.get("MajorVersion") != 5 or engine_build.get("MinorVersion") != 8:
        raise AttributionError("engine Build.version is not UE 5.8")

    source_manifest: dict[str, tuple[int, str]] = {}
    records = [
        classify_record(
            record, proposal["records"][index], project_root, engine_root, policy,
            third_party, poly_derived, poly_original, source_manifest,
        )
        for index, record in enumerate(draft["records"])
    ]
    attributed = sum(record["technicalDisposition"] == ATTRIBUTED for record in records)
    unresolved_anon = sum(record["technicalDisposition"] == UNRESOLVED_ANON for record in records)
    unresolved_path = sum(record["technicalDisposition"] == UNRESOLVED_PATH for record in records)
    resolved_anon = sum(
        record["scope"] == "CONTAINER_ANONYMOUS_CHUNK"
        and record["technicalDisposition"] == ATTRIBUTED
        for record in records
    )
    expected_identity_count = policy["inputs"]["expectedAuditedIdentityCount"]
    if (
        len(records) != expected_identity_count
        or attributed + unresolved_anon + unresolved_path != expected_identity_count
    ):
        raise AttributionError("technical attribution partition is not exhaustive")

    project_entries = {k: v for k, v in source_manifest.items() if k.startswith("DiscGolfTour/")}
    engine_entries = {k: v for k, v in source_manifest.items() if k.startswith("Engine/")}
    scope_counts = dict(sorted(Counter(record["scope"] for record in records).items()))
    category_counts = dict(sorted(Counter(
        record["originCategory"] for record in records if record["originCategory"] is not None
    ).items()))
    license_counts = dict(sorted(Counter(record["licenseSourceCategory"] for record in records).items()))
    disposition_counts = dict(sorted(Counter(record["technicalDisposition"] for record in records).items()))
    live_bind = {
        "archiveFileCount": live["archive"]["fileCount"],
        "archiveBytes": live["archive"]["bytes"],
        "archiveCanonicalManifestSha256": live["archive"]["canonicalManifestSha256"],
        "ufsEntryCount": len(live["stageManifests"]["ufs"]["entries"]),
        "nonUfsEntryCount": len(live["stageManifests"]["nonUfs"]["entries"]),
        "containerCount": len(live["containers"]),
        "containerNamedEntryCount": sum(
            entry["scope"] == "CONTAINER_NAMED_ENTRY"
            for container in live["containers"] for entry in container["entries"]
        ),
        "containerAnonymousChunkCount": sum(
            entry["scope"] == "CONTAINER_ANONYMOUS_CHUNK"
            for container in live["containers"] for entry in container["entries"]
        ),
        "unrealPak": copy.deepcopy(live["unrealPak"]),
        "uatLog": copy.deepcopy(live["uatLog"]),
        "hostPathsRecorded": False,
    }
    document = {
        "schema": SCHEMA, "schemaVersion": 1, "session": 19,
        "candidateId": args.candidate_id, "state": STATE,
        "purpose": "Exact technical-origin and license-source triage; not legal provenance classification",
        "reviewed": False, "shippingApproved": False, "legalReviewed": False,
        "legalApproved": False, "licenseApproved": False,
        "distributionApproved": False, "releaseReady": False,
        "authoritativeProvenanceClassificationCompatible": False,
        "inputBindings": {
            "validator": file_binding(
                SCRIPT_DIR / "validate_dg_session19_technical_origin_attribution.py",
                "Scripts/validate_dg_session19_technical_origin_attribution.py",
            ),
            "policy": file_binding(args.policy.resolve(), "Config/DG_Session19TechnicalOriginAttributionPolicy.json"),
            "stagedProvenancePolicy": file_binding(args.staged_policy.resolve(), "Config/DG_Session19StagedProvenancePolicy.json"),
            "sourceDraft": file_binding(args.source_draft.resolve(), f"Evidence/Session19/{args.source_draft.name}"),
            "technicalOriginProposal": file_binding(args.technical_proposal.resolve(), f"Evidence/Session19/{args.technical_proposal.name}"),
            "candidateContent": file_binding(args.candidate_content.resolve(), f"Evidence/Session19/{args.candidate_content.name}"),
            "thirdPartyAuthority": third_binding,
            "polyHavenDerivedReceipt": poly_binding,
            "shippingBuildReceipt": build_binding,
            "engineBuildVersion": engine_build_binding,
        },
        "liveCandidateInventory": live_bind,
        "technicalSourceManifests": {
            "project": manifest_binding(project_entries, "EXACT_PROJECT_FILES_REFERENCED_BY_CANDIDATE"),
            "engine": manifest_binding(engine_entries, "EXACT_ENGINE_INSTALL_FILES_REFERENCED_BY_CANDIDATE"),
        },
        "records": records,
        "summary": {
            "auditedIdentityCount": len(records),
            "authoritativelyAttributedTechnicalOriginCount": attributed,
            "unresolvedIdentityCount": unresolved_anon + unresolved_path,
            "resolvedAnonymousChunkTechnicalIdentityCount": resolved_anon,
            "unresolvedAnonymousChunkCount": unresolved_anon,
            "unresolvedPathIdentityCount": unresolved_path,
            "scopeCounts": scope_counts,
            "technicalDispositionCounts": disposition_counts,
            "originCategoryCounts": category_counts,
            "licenseSourceCategoryCounts": license_counts,
            "legalReviewedRecordCount": 0,
            "shippingApprovedRecordCount": 0,
            "distributionApprovedRecordCount": 0,
        },
        "releaseBoundary": {
            "technicalAttributionIsLegalClassification": False,
            "independentReviewComplete": False,
            "legalReviewComplete": False,
            "distributionClearanceComplete": False,
            "shippingApprovalGranted": False,
            "releaseReady": False, "releaseUseAllowed": False,
            "blockerClosed": False,
            "remainingBlockers": (
                [
                    "INDEPENDENT_REVIEWED_EXACT_IDENTITY_CLASSIFICATION",
                    "LEGAL_AND_LICENSE_APPROVAL",
                ]
                + (["ANONYMOUS_IOSTORE_IDENTITY_RESOLUTION_OR_ACCEPTED_AUTHORITY"]
                   if unresolved_anon else [])
                + ["DISTRIBUTION_CLEARANCE"]
            ),
        },
    }
    problems = security_problems(document)
    if problems:
        raise AttributionError("; ".join(problems[:8]))
    return document


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def write_exclusive(path: Path, document: dict[str, Any]) -> bytes:
    data = json_bytes(document)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise AttributionError(
            "technical-origin attribution output already exists; refusing overwrite"
        ) from exc
    return data


def run_self_test(policy_path: Path) -> int:
    try:
        policy = load_json(policy_path.resolve())
        validate_policy_document(policy)
    except AttributionError as exc:
        print(f"FAIL technical-origin attribution self-test: {exc}", file=sys.stderr)
        return 1
    generated_cases = [
        ("DiscGolfTour.exe", True),
        ("DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak", True),
        ("DiscGolfTour/Content/Paks/global.utoc", True),
        ("DiscGolfTour/Content/ShaderArchive-Global-PCD3D_SM6-PCD3D_SM6.ushaderbytecode", True),
        ("Engine/GlobalShaderCache-PCD3D_SM5.bin", True),
        ("Engine/Content/EngineFonts/Faces/RobotoRegular.ufont", True),
        ("DiscGolfTour/Evil.bin", False),
        ("Engine/Evil.bin", False),
        ("DiscGolfTour/Content/Paks/Other.pak", False),
        ("DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pdb", False),
        ("../escape", False),
        ("C:/host/path", False),
    ]
    anonymous_cases = [
        (
            "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
            "<ShaderCode>#001007BC7F87EF3800000009@0000000090570752:0000000000002102:34A7EA23AC2A3AA9E7CFA89CEEB55F63F5E16EFD",
            True,
        ),
        (
            "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
            "<ContainerHeader>#E733E7D6C5AF438200000006@0000001658519552:0000000000029736:C8BE387615242EE4A0E271BAD4C5921463688EEB",
            True,
        ),
        (
            "DiscGolfTour/Content/Paks/global.utoc",
            "<ScriptObjects>#E733E7D6C5AF438200000005@0000000000000000:0000000000000001:C8BE387615242EE4A0E271BAD4C5921463688EEB",
            True,
        ),
        (
            "DiscGolfTour/Content/Paks/global.utoc",
            "<ShaderCode>#001007BC7F87EF3800000009@0000000090570752:0000000000002102:34A7EA23AC2A3AA9E7CFA89CEEB55F63F5E16EFD",
            False,
        ),
        (
            "DiscGolfTour/Content/Paks/Other.utoc",
            "<ShaderCode>#001007BC7F87EF3800000009@0000000090570752:0000000000002102:34A7EA23AC2A3AA9E7CFA89CEEB55F63F5E16EFD",
            False,
        ),
        (
            "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
            "<BulkData>#001007BC7F87EF3800000009@0000000090570752:0000000000002102:34A7EA23AC2A3AA9E7CFA89CEEB55F63F5E16EFD",
            False,
        ),
        (
            "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
            "<ShaderCode>#01007BC7F87EF3800000009@0000000090570752:0000000000002102:34A7EA23AC2A3AA9E7CFA89CEEB55F63F5E16EFD",
            False,
        ),
        (
            "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
            "<ShaderCode>#001007BC7F87EF3800000009@0000000090570752:0000000000002102:34a7ea23ac2a3aa9e7cfa89ceeb55f63f5e16efd",
            False,
        ),
    ]
    passed = 0
    for identity, expected in generated_cases:
        if (generated_kind(identity, policy) is not None) == expected:
            passed += 1
    for container, identity, expected in anonymous_cases:
        if (anonymous_chunk_origin(container, identity, policy) is not None) == expected:
            passed += 1
    policy_mutations: list[Callable[[dict[str, Any]], None]] = [
        lambda d: d["classificationRules"]["generatedIdentityPatterns"].append(r"^.*$"),
        lambda d: d["classificationRules"].__setitem__("forbidNamespaceOnlyAttribution", False),
        lambda d: d["classificationRules"].__setitem__("recordHostPaths", True),
        lambda d: d["classificationRules"]["anonymousChunkTechnicalOriginRules"][0]["chunkTypes"].append("BulkData"),
        lambda d: d["classificationRules"].__setitem__("requireAnonymousChunkOffsetSizeSha1Binding", False),
        lambda d: d.__setitem__("shippingApproved", True),
        lambda d: d["releaseBoundary"].__setitem__("blockerClosed", True),
        lambda d: d["originCategories"].append("GUESSED_ORIGIN"),
        lambda d: d["licenseSourceCategories"].remove("UNRESOLVED_NO_LICENSE_SOURCE"),
        lambda d: d["inputs"].__setitem__("buildReceiptConfiguration", "Development"),
        lambda d: d["inputs"].__setitem__("expectedAuditedIdentityCount", 7110),
    ]
    for mutate in policy_mutations:
        changed_policy = copy.deepcopy(policy)
        mutate(changed_policy)
        try:
            validate_policy_document(changed_policy)
        except AttributionError:
            passed += 1
    base = {
        "schema": SCHEMA, "authoritativeProvenanceClassificationCompatible": False,
        "reviewed": False, "shippingApproved": False, "legalReviewed": False,
        "legalApproved": False, "licenseApproved": False,
        "distributionApproved": False, "releaseReady": False,
        "releaseBoundary": {
            "technicalAttributionIsLegalClassification": False,
            "independentReviewComplete": False, "legalReviewComplete": False,
            "distributionClearanceComplete": False, "shippingApprovalGranted": False,
            "releaseReady": False, "releaseUseAllowed": False, "blockerClosed": False,
        },
        "records": [{
            "scope": "ARCHIVE_FILE", "container": None, "identity": "DiscGolfTour.exe",
            "technicalDisposition": ATTRIBUTED,
            "originCategory": "PROJECT_GENERATED_BUILD_OUTPUT_EXACT_CANDIDATE",
            "licenseSourceCategory": "MIXED_BUILD_OUTPUT_RIGHTS_REVIEW_PENDING",
            "reviewed": False, "shippingApproved": False, "legalReviewed": False,
            "legalApproved": False, "licenseApproved": False, "distributionApproved": False,
        }],
    }
    mutations: list[Callable[[dict[str, Any]], None]] = [
        lambda d: d.__setitem__("schema", "DiscGolfTour.Session19StagedProvenanceClassification.v1"),
        lambda d: d.__setitem__("authoritativeProvenanceClassificationCompatible", True),
        lambda d: d.__setitem__("reviewed", True),
        lambda d: d.__setitem__("shippingApproved", True),
        lambda d: d.__setitem__("legalApproved", True),
        lambda d: d.__setitem__("licenseApproved", True),
        lambda d: d.__setitem__("distributionApproved", True),
        lambda d: d.__setitem__("releaseReady", True),
        lambda d: d["releaseBoundary"].__setitem__("releaseUseAllowed", True),
        lambda d: d["releaseBoundary"].__setitem__("blockerClosed", True),
        lambda d: d["releaseBoundary"].__setitem__("technicalAttributionIsLegalClassification", True),
        lambda d: d["records"][0].__setitem__("reviewed", True),
        lambda d: d["records"][0].__setitem__("shippingApproved", True),
        lambda d: d["records"][0].__setitem__("legalApproved", True),
        lambda d: d["records"][0].__setitem__("licenseApproved", True),
        lambda d: d["records"][0].__setitem__("distributionApproved", True),
        lambda d: d["records"][0].__setitem__("originCategory", None),
        lambda d: d["records"][0].__setitem__("licenseSourceCategory", "UNRESOLVED_NO_LICENSE_SOURCE"),
        lambda d: d["records"][0].__setitem__("technicalDisposition", "APPROVED"),
        lambda d: d["records"][0].__setitem__("identity", "C:/Users/example/secret.bin"),
        lambda d: d.__setitem__("records", "not-a-list"),
    ]
    if not security_problems(base):
        passed += 1
    for mutate in mutations:
        changed = copy.deepcopy(base)
        mutate(changed)
        if security_problems(changed):
            passed += 1
    with tempfile.TemporaryDirectory(prefix="dgt-origin-attribution-output-") as folder:
        output = Path(folder) / "audit.json"
        written = write_exclusive(output, base)
        original = output.read_bytes()
        if written == original == json_bytes(base):
            passed += 1
        try:
            write_exclusive(output, base)
        except AttributionError:
            if output.read_bytes() == original:
                passed += 1
    total = (
        len(generated_cases) + len(anonymous_cases)
        + len(policy_mutations) + 1 + len(mutations) + 2
    )
    if passed != total:
        print(f"FAIL technical-origin attribution self-test {passed}/{total}", file=sys.stderr)
        return 1
    print(f"PASS technical-origin attribution self-test {passed}/{total}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--uat-log", type=Path)
    parser.add_argument("--source-draft", type=Path)
    parser.add_argument("--source-draft-sha256")
    parser.add_argument("--technical-proposal", type=Path)
    parser.add_argument("--candidate-content", type=Path)
    parser.add_argument("--third-party-authority", type=Path)
    parser.add_argument("--poly-receipt", type=Path)
    parser.add_argument("--build-receipt", type=Path)
    parser.add_argument("--engine-root", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--project-root", type=Path, default=PROJECT_ROOT)
    parser.add_argument(
        "--policy", type=Path,
        default=PROJECT_ROOT / "Config/DG_Session19TechnicalOriginAttributionPolicy.json",
    )
    parser.add_argument(
        "--staged-policy", type=Path,
        default=PROJECT_ROOT / "Config/DG_Session19StagedProvenancePolicy.json",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test(args.policy)
    required = [
        "candidate_id", "archive", "uat_log", "source_draft", "source_draft_sha256",
        "technical_proposal", "candidate_content", "third_party_authority",
        "poly_receipt", "build_receipt", "engine_root", "unrealpak", "output",
    ]
    missing = ["--" + name.replace("_", "-") for name in required if getattr(args, name) is None]
    if missing:
        parser.error("required arguments: " + ", ".join(missing))
    try:
        document = build_document(args)
        data = write_exclusive(args.output.resolve(), document)
    except AttributionError as exc:
        print(f"FAIL_TECHNICAL_ORIGIN_ATTRIBUTION: {exc}", file=sys.stderr)
        return 1
    summary = document["summary"]
    line = (
        "PASS_TECHNICAL_ORIGIN_ATTRIBUTION "
        f"candidate={document['candidateId']} identities={summary['auditedIdentityCount']} "
        f"authoritatively_attributed={summary['authoritativelyAttributedTechnicalOriginCount']} "
        f"unresolved={summary['unresolvedIdentityCount']} "
        f"anonymous_unresolved={summary['unresolvedAnonymousChunkCount']} "
        f"path_unresolved={summary['unresolvedPathIdentityCount']} "
        f"receipt_bytes={len(data)} receipt_sha256={sha256_bytes(data)} "
        "reviewed=false legalApproved=false distributionApproved=false releaseReady=false"
    )
    print(line)
    if args.release_required:
        unresolved_note = (
            f"; {summary['unresolvedIdentityCount']} identities remain unresolved"
            if summary["unresolvedIdentityCount"] else ""
        )
        print(
            "BLOCKED_RELEASE_REQUIRED: technical attribution is not reviewed legal provenance; "
            "independent review, legal/license approval, and distribution clearance remain required"
            f"{unresolved_note}",
            file=sys.stderr,
        )
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
