#!/usr/bin/env python3
"""Bind UE 5.8 runtime-DLL authorities and generate deterministic notices.

This is a technical inventory control.  It intentionally keeps legal approval
and distribution clearance false.  When an archive is supplied, every staged
DLL must have an exact policy mapping and the staged NOTICES file must match the
candidate-independent template byte for byte.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json"
DEFAULT_ENGINE_ROOT = Path(r"C:\Program Files\Epic Games\UE_5.8")
DEFAULT_AUTHORITY_OUTPUT = (
    ROOT / "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json"
)
DEFAULT_NOTICES_OUTPUT = ROOT / "Evidence/Session19/UE58RuntimeThirdPartyNOTICES.txt"
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
AUTHORITY_RE = re.compile(r"^[A-Z0-9][A-Z0-9._-]{1,127}$")
LICENSE_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9.+_-]{0,127}$")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
UTC_MILLI_RE = re.compile(
    r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$"
)

POLICY_KEYS = {
    "schema", "schemaVersion", "session", "policyId", "engineAssociation",
    "state", "legalApproval", "distributionClearance", "archiveDllPolicy",
    "noticeTemplate", "components",
}
ARCHIVE_POLICY_KEYS = {
    "extension", "caseInsensitive", "requireEveryStagedDllMapped",
    "allowMappedIdentityToMatchMultipleComponents",
}
NOTICE_TEMPLATE_KEYS = {
    "stagedFilename", "deterministicNewline", "manifestTimestampUtc",
    "headerLines", "baselineNoticeFiles",
}
COMPONENT_KEYS = {
    "componentId", "displayName", "stagedRuntimeDlls",
    "classificationAuthorityId", "classificationLicenseId",
    "licenseEvidenceStatus", "licenseReferenceUrl", "authorityFiles",
    "noticeTextFiles",
}


class ContractError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value: str) -> Any:
    raise ContractError(f"non-finite JSON number: {value}")


def load_json_strict(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8-sig"),
            object_pairs_hook=reject_duplicate_keys,
            parse_constant=reject_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ContractError) as exc:
        raise ContractError(f"strict JSON load failed for {path.name}: {exc}") from exc


def canonical_relative(value: Any) -> str | None:
    if (
        type(value) is not str or not value or value != value.strip()
        or "\\" in value or "\x00" in value or ":" in value
    ):
        return None
    pure = PurePosixPath(value)
    if (
        pure.is_absolute() or value.startswith("/")
        or any(part in ("", ".", "..") for part in pure.parts)
        or pure.as_posix() != value
    ):
        return None
    return value


def unique_strings(value: Any, *, allow_empty: bool = False) -> bool:
    return (
        type(value) is list
        and (allow_empty or bool(value))
        and all(type(item) is str and item for item in value)
        and len({item.casefold() for item in value}) == len(value)
    )


def validate_policy(policy: Any) -> None:
    if type(policy) is not dict or set(policy) != POLICY_KEYS:
        raise ContractError("policy root keys or type differ")
    expected = {
        "schema": "DiscGolfTour.Session19RuntimeThirdPartyLicensePolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "ue58_windows_shipping_runtime_dll_notice_inventory_v1",
        "engineAssociation": "5.8",
        "state": "TECHNICAL_AUTHORITY_IMPLEMENTED_LEGAL_APPROVAL_PENDING",
        "legalApproval": False,
        "distributionClearance": False,
    }
    for key, value in expected.items():
        if type(policy.get(key)) is not type(value) or policy.get(key) != value:
            raise ContractError(f"policy {key} differs")

    archive = policy["archiveDllPolicy"]
    expected_archive = {
        "extension": ".dll",
        "caseInsensitive": True,
        "requireEveryStagedDllMapped": True,
        "allowMappedIdentityToMatchMultipleComponents": False,
    }
    if type(archive) is not dict or set(archive) != ARCHIVE_POLICY_KEYS or archive != expected_archive:
        raise ContractError("archive DLL policy differs")

    notice = policy["noticeTemplate"]
    if type(notice) is not dict or set(notice) != NOTICE_TEMPLATE_KEYS:
        raise ContractError("notice template keys or type differ")
    if notice["stagedFilename"] != "NOTICES.txt" or notice["deterministicNewline"] != "LF":
        raise ContractError("notice output identity or newline differs")
    if type(notice["manifestTimestampUtc"]) is not str or not UTC_MILLI_RE.fullmatch(
        notice["manifestTimestampUtc"]
    ):
        raise ContractError("notice manifest timestamp is malformed")
    if not unique_strings(notice["headerLines"]):
        # A single intentional blank separator is permitted in the header.
        header = notice["headerLines"]
        if (
            type(header) is not list or header.count("") != 1
            or not all(type(item) is str for item in header)
            or len({item.casefold() for item in header if item}) != len(header) - 1
        ):
            raise ContractError("notice header lines are malformed")
    if not unique_strings(notice["baselineNoticeFiles"]):
        raise ContractError("baseline notice files are malformed")

    components = policy["components"]
    if type(components) is not list or not components:
        raise ContractError("components must be a nonempty array")
    component_ids: set[str] = set()
    staged_identities: set[str] = set()
    for index, component in enumerate(components):
        if type(component) is not dict or set(component) != COMPONENT_KEYS:
            raise ContractError(f"component {index} keys or type differ")
        component_id = component["componentId"]
        if (
            type(component_id) is not str
            or not re.fullmatch(r"[a-z0-9][a-z0-9_]{1,63}", component_id)
            or component_id in component_ids
        ):
            raise ContractError(f"component {index} ID is malformed or duplicated")
        component_ids.add(component_id)
        if type(component["displayName"]) is not str or not component["displayName"].strip():
            raise ContractError(f"component {index} display name is malformed")
        if not AUTHORITY_RE.fullmatch(component["classificationAuthorityId"]):
            raise ContractError(f"component {index} authority ID is malformed")
        if not LICENSE_RE.fullmatch(component["classificationLicenseId"]):
            raise ContractError(f"component {index} license ID is malformed")
        if (
            type(component["licenseEvidenceStatus"]) is not str
            or not component["licenseEvidenceStatus"]
            or type(component["licenseReferenceUrl"]) is not str
            or not component["licenseReferenceUrl"].startswith("https://")
        ):
            raise ContractError(f"component {index} license evidence metadata is malformed")
        if not unique_strings(component["stagedRuntimeDlls"]):
            raise ContractError(f"component {index} staged DLL list is malformed")
        for identity in component["stagedRuntimeDlls"]:
            if canonical_relative(identity) is None or not identity.casefold().endswith(".dll"):
                raise ContractError(f"component {index} staged DLL identity is noncanonical")
            collision = identity.casefold()
            if collision in staged_identities:
                raise ContractError("a staged DLL identity maps to multiple components")
            staged_identities.add(collision)
        if not unique_strings(component["authorityFiles"]):
            raise ContractError(f"component {index} authority file list is malformed")
        if not unique_strings(component["noticeTextFiles"], allow_empty=True):
            raise ContractError(f"component {index} notice text file list is malformed")
        authority_set = {item.casefold() for item in component["authorityFiles"]}
        for relative in component["authorityFiles"] + component["noticeTextFiles"]:
            if canonical_relative(relative) is None or not relative.startswith("Engine/"):
                raise ContractError(f"component {index} engine evidence path is noncanonical")
        if any(item.casefold() not in authority_set for item in component["noticeTextFiles"]):
            raise ContractError(f"component {index} notice text is not an authority file")

    for relative in notice["baselineNoticeFiles"]:
        if canonical_relative(relative) is None or not relative.startswith("Engine/"):
            raise ContractError("baseline notice path is noncanonical")


def hash_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def read_stable(path: Path) -> bytes:
    try:
        before = path.stat()
        if not path.is_file() or path.is_symlink():
            raise ContractError(f"authority source is not a regular file: {path.name}")
        data = path.read_bytes()
        after = path.stat()
    except OSError as exc:
        raise ContractError(f"authority source cannot be read: {path.name}") from exc
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ContractError(f"authority source changed while reading: {path.name}")
    return data


def engine_path(engine_root: Path, relative: str) -> Path:
    parts = PurePosixPath(relative).parts
    candidate = engine_root.joinpath(*parts)
    try:
        root_resolved = engine_root.resolve(strict=True)
        resolved = candidate.resolve(strict=True)
    except OSError as exc:
        raise ContractError(f"engine authority source is missing: {relative}") from exc
    if root_resolved not in resolved.parents or candidate.is_symlink() or not candidate.is_file():
        raise ContractError(f"engine authority source escapes or is irregular: {relative}")
    return candidate


def file_binding(engine_root: Path, relative: str) -> tuple[dict[str, Any], bytes]:
    data = read_stable(engine_path(engine_root, relative))
    return {
        "path": relative,
        "bytes": len(data),
        "sha256": hash_bytes(data),
    }, data


def load_engine_version(engine_root: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    relative = "Engine/Build/Build.version"
    binding, _ = file_binding(engine_root, relative)
    document = load_json_strict(engine_path(engine_root, relative))
    if type(document) is not dict:
        raise ContractError("UE Build.version is not an object")
    major = document.get("MajorVersion")
    minor = document.get("MinorVersion")
    patch = document.get("PatchVersion")
    changelist = document.get("Changelist")
    if not all(type(item) is int and item >= 0 for item in (major, minor, patch, changelist)):
        raise ContractError("UE Build.version version fields are malformed")
    if major != 5 or minor != 8:
        raise ContractError(f"engine authority must be UE 5.8, observed {major}.{minor}")
    return binding, {
        "major": major,
        "minor": minor,
        "patch": patch,
        "changelist": changelist,
    }


def generate_authority(
    policy: dict[str, Any], policy_bytes: bytes, engine_root: Path
) -> tuple[dict[str, Any], dict[str, bytes]]:
    build_binding, version = load_engine_version(engine_root)
    source_data: dict[str, bytes] = {}

    def bind(relative: str) -> dict[str, Any]:
        if relative not in source_data:
            binding, data = file_binding(engine_root, relative)
            source_data[relative] = data
            bindings[relative] = binding
        return copy.deepcopy(bindings[relative])

    bindings: dict[str, dict[str, Any]] = {}
    baseline = [bind(item) for item in policy["noticeTemplate"]["baselineNoticeFiles"]]
    component_records: list[dict[str, Any]] = []
    local_text_count = 0
    reference_only_count = 0
    for component in sorted(policy["components"], key=lambda item: item["componentId"]):
        if component["licenseEvidenceStatus"] == "LOCAL_LICENSE_TEXT_PRESENT":
            local_text_count += 1
        else:
            reference_only_count += 1
        component_records.append({
            "componentId": component["componentId"],
            "displayName": component["displayName"],
            "stagedRuntimeDlls": sorted(component["stagedRuntimeDlls"], key=str.casefold),
            "classificationAuthorityId": component["classificationAuthorityId"],
            "classificationLicenseId": component["classificationLicenseId"],
            "licenseEvidenceStatus": component["licenseEvidenceStatus"],
            "licenseReferenceUrl": component["licenseReferenceUrl"],
            "authorityFiles": [bind(item) for item in component["authorityFiles"]],
            "noticeTextFiles": [bind(item) for item in component["noticeTextFiles"]],
            "legalApproval": False,
            "distributionClearance": False,
        })
    supported_count = sum(len(item["stagedRuntimeDlls"]) for item in component_records)
    authority = {
        "schema": "DiscGolfTour.Session19RuntimeThirdPartyLicenseAuthority.v1",
        "schemaVersion": 1,
        "session": 19,
        "state": "PASS_TECHNICAL_AUTHORITY_INVENTORY_LEGAL_APPROVAL_PENDING",
        "candidateIndependent": True,
        "deterministic": True,
        "policy": {
            "path": "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json",
            "bytes": len(policy_bytes),
            "sha256": hash_bytes(policy_bytes),
        },
        "engine": {
            "association": policy["engineAssociation"],
            "buildVersion": build_binding,
            "version": version,
        },
        "baselineNoticeFiles": baseline,
        "coverage": {
            "componentCount": len(component_records),
            "supportedStagedDllIdentityCount": supported_count,
            "allAuthorityFilesPresentAndHashed": True,
            "localLicenseTextPresentComponentCount": local_text_count,
            "referenceOrSpecialReviewComponentCount": reference_only_count,
            "technicalMappingComplete": True,
            "legalApproval": False,
            "distributionClearance": False,
        },
        "components": component_records,
        "releaseBoundary": {
            "releaseReady": False,
            "legalApproval": False,
            "distributionClearance": False,
            "remainingDecision": "INDEPENDENT_LICENSE_REVIEW_AND_DISTRIBUTION_APPROVAL",
        },
    }
    return authority, source_data


def decode_authority_text(relative: str, data: bytes) -> str:
    try:
        if data.startswith(b"\xff\xfe"):
            text = data.decode("utf-16")
        elif data.startswith(b"\xfe\xff"):
            text = data.decode("utf-16")
        else:
            text = data.decode("utf-8-sig")
    except UnicodeError as exc:
        raise ContractError(f"notice authority text is not UTF-8: {relative}") from exc
    return text.replace("\r\n", "\n").replace("\r", "\n").rstrip("\n")


def generate_notices(
    policy: dict[str, Any], authority: dict[str, Any], source_data: dict[str, bytes]
) -> bytes:
    lines = list(policy["noticeTemplate"]["headerLines"])
    lines.extend(["", "TECHNICAL AUTHORITY SUMMARY"])
    lines.append(
        f"Supported staged runtime DLL identities: "
        f"{authority['coverage']['supportedStagedDllIdentityCount']}"
    )
    lines.append("Legal approval: false")
    lines.append("Distribution clearance: false")
    lines.extend(["", "UE BASELINE NOTICES"])
    for relative in policy["noticeTemplate"]["baselineNoticeFiles"]:
        binding = next(
            item for item in authority["baselineNoticeFiles"] if item["path"] == relative
        )
        lines.append(f"Source: {relative}")
        lines.append(f"SHA-256: {binding['sha256']}")
        lines.append(decode_authority_text(relative, source_data[relative]))

    by_id = {item["componentId"]: item for item in authority["components"]}
    policy_by_id = {item["componentId"]: item for item in policy["components"]}
    for component_id in sorted(by_id):
        record = by_id[component_id]
        source = policy_by_id[component_id]
        lines.extend(["", "=" * 79, f"COMPONENT: {record['displayName']}"])
        lines.append(f"Component ID: {component_id}")
        lines.append(f"Classification authority: {record['classificationAuthorityId']}")
        lines.append(f"Classification license: {record['classificationLicenseId']}")
        lines.append(f"Evidence status: {record['licenseEvidenceStatus']}")
        lines.append(f"License reference: {record['licenseReferenceUrl']}")
        lines.append("Staged runtime DLL identities:")
        lines.extend(f"- {item}" for item in record["stagedRuntimeDlls"])
        lines.append("Hashed local authority sources:")
        lines.extend(
            f"- {item['path']} | {item['bytes']} bytes | SHA-256 {item['sha256']}"
            for item in record["authorityFiles"]
        )
        if not source["noticeTextFiles"]:
            lines.append("Local license text: ABSENT; the reference above requires independent review.")
        else:
            for relative in source["noticeTextFiles"]:
                binding = next(item for item in record["noticeTextFiles"] if item["path"] == relative)
                lines.extend([
                    "", f"--- BEGIN LOCAL AUTHORITY TEXT: {relative} ---",
                    f"SHA-256: {binding['sha256']}",
                    decode_authority_text(relative, source_data[relative]),
                    f"--- END LOCAL AUTHORITY TEXT: {relative} ---",
                ])
    lines.extend([
        "", "=" * 79,
        "END OF TECHNICAL NOTICE INVENTORY",
        "Legal approval: false",
        "Distribution clearance: false",
        "",
    ])
    return "\n".join(lines).encode("utf-8")


def write_shared_bytes(path: Path, data: bytes) -> None:
    """Create deterministic shared authority, or verify the existing bytes.

    These outputs are referenced by historical candidates.  Re-generating the
    same deterministic authority must therefore be a true filesystem no-op,
    while any byte drift fails closed instead of replacing historical evidence.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() or path.is_symlink():
        if read_stable(path) != data:
            raise ContractError(
                f"shared authority output differs from existing bytes: {path.name}"
            )
        return
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError:
        # Another process may have created the deterministic output after the
        # existence check.  Accept only the exact same completed authority.
        if read_stable(path) != data:
            raise ContractError(
                f"shared authority output differs from concurrently created bytes: {path.name}"
            )


def write_exclusive_bytes(path: Path, data: bytes) -> None:
    """Create one candidate-bound evidence file and never reuse its identity."""
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise ContractError(
            f"candidate verification output already exists: {path.name}"
        ) from exc


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def verify_archive(
    *,
    archive_root: Path,
    candidate_id: str,
    verification_output: Path,
    policy: dict[str, Any],
    policy_bytes: bytes,
    authority: dict[str, Any],
    authority_bytes: bytes,
    notices_bytes: bytes,
) -> tuple[dict[str, Any], bool]:
    if not CANDIDATE_ID_RE.fullmatch(candidate_id):
        raise ContractError("candidate ID is malformed")
    if not archive_root.is_dir() or archive_root.is_symlink():
        raise ContractError("archive root is not a regular directory")
    mapping: dict[str, dict[str, Any]] = {}
    for component in policy["components"]:
        for identity in component["stagedRuntimeDlls"]:
            mapping[identity.casefold()] = component
    dlls: list[dict[str, Any]] = []
    unmapped: list[str] = []
    seen: set[str] = set()
    for path in sorted(archive_root.rglob("*"), key=lambda item: item.as_posix().casefold()):
        if path.is_symlink() or not path.is_file() or path.suffix.casefold() != ".dll":
            continue
        identity = path.relative_to(archive_root).as_posix()
        canonical = canonical_relative(identity)
        if canonical is None:
            raise ContractError("archive DLL identity is noncanonical")
        collision = canonical.casefold()
        if collision in seen:
            raise ContractError("archive DLL identity duplicates or case-collides")
        seen.add(collision)
        data = read_stable(path)
        component = mapping.get(collision)
        if component is None:
            unmapped.append(canonical)
            component_id = "UNMAPPED"
        else:
            component_id = component["componentId"]
        dlls.append({
            "identity": canonical,
            "bytes": len(data),
            "sha256": hash_bytes(data),
            "componentId": component_id,
        })

    notice_relative = policy["noticeTemplate"]["stagedFilename"]
    notice_path = archive_root / notice_relative
    try:
        actual_notice = read_stable(notice_path)
    except ContractError:
        actual_notice = b""
    notice_matches = actual_notice == notices_bytes
    manifest_path = archive_root / "Manifest_NonUFSFiles_Win64.txt"
    expected_row = (
        f"{notice_relative}\t{policy['noticeTemplate']['manifestTimestampUtc']}"
    )
    try:
        manifest_text = read_stable(manifest_path).decode("utf-8-sig")
    except (ContractError, UnicodeError):
        manifest_text = ""
    manifest_rows = manifest_text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    notice_rows = [row for row in manifest_rows if row.startswith(notice_relative + "\t")]
    manifest_binding_matches = notice_rows == [expected_row]
    staged_set = {item["identity"].casefold() for item in dlls}
    supported_but_absent = sorted(
        [
            identity
            for component in policy["components"]
            for identity in component["stagedRuntimeDlls"]
            if identity.casefold() not in staged_set
        ],
        key=str.casefold,
    )
    passed = not unmapped and bool(dlls) and notice_matches and manifest_binding_matches
    receipt = {
        "schema": "DiscGolfTour.Session19RuntimeThirdPartyNoticeVerification.v1",
        "schemaVersion": 1,
        "session": 19,
        "candidateId": candidate_id,
        "state": (
            "PASS_TECHNICAL_NOTICE_COVERAGE_LEGAL_APPROVAL_PENDING"
            if passed else "FAIL_CLOSED"
        ),
        "policy": {
            "path": "Config/DG_Session19RuntimeThirdPartyLicensePolicy.json",
            "bytes": len(policy_bytes),
            "sha256": hash_bytes(policy_bytes),
        },
        "authority": {
            "path": "Evidence/Session19/UE58RuntimeThirdPartyLicenseAuthority.json",
            "bytes": len(authority_bytes),
            "sha256": hash_bytes(authority_bytes),
            "engineBuildVersionSha256": authority["engine"]["buildVersion"]["sha256"],
        },
        "notices": {
            "archiveIdentity": notice_relative,
            "expectedBytes": len(notices_bytes),
            "expectedSha256": hash_bytes(notices_bytes),
            "actualBytes": len(actual_notice),
            "actualSha256": hash_bytes(actual_notice),
            "exactMatch": notice_matches,
            "nonUfsManifestExactBinding": manifest_binding_matches,
        },
        "coverage": {
            "supportedDllIdentityCount": len(mapping),
            "stagedDllIdentityCount": len(dlls),
            "mappedStagedDllIdentityCount": len(dlls) - len(unmapped),
            "unmappedStagedDllIdentityCount": len(unmapped),
            "unmappedStagedDlls": unmapped,
            "supportedButAbsentDlls": supported_but_absent,
            "stagedDlls": dlls,
        },
        "releaseBoundary": {
            "releaseReady": False,
            "legalApproval": False,
            "distributionClearance": False,
            "technicalCoverageOnly": passed,
            "remainingDecision": "INDEPENDENT_LICENSE_REVIEW_AND_DISTRIBUTION_APPROVAL",
        },
    }
    write_exclusive_bytes(verification_output, json_bytes(receipt))
    return receipt, passed


def run_self_test(policy: dict[str, Any], policy_bytes: bytes, engine_root: Path) -> int:
    count = 0
    authority, source_data = generate_authority(policy, policy_bytes, engine_root)
    count += 1
    first = generate_notices(policy, authority, source_data)
    second = generate_notices(policy, authority, source_data)
    if first != second or not first.endswith(b"\n") or b"\r" in first:
        raise ContractError("deterministic LF-only notice generation failed")
    count += 1
    if authority["releaseBoundary"]["legalApproval"] is not False:
        raise ContractError("authority receipt crossed the legal boundary")
    count += 1
    if authority["coverage"]["supportedStagedDllIdentityCount"] != 13:
        raise ContractError("supported runtime DLL identity count differs")
    count += 1

    for mutate in (
        lambda value: value.__setitem__("legalApproval", True),
        lambda value: value["components"][1]["stagedRuntimeDlls"].append(
            value["components"][0]["stagedRuntimeDlls"][0]
        ),
        lambda value: value["components"][0]["noticeTextFiles"].append(
            "Engine/NotAnAuthority.txt"
        ),
        lambda value: value["archiveDllPolicy"].__setitem__(
            "requireEveryStagedDllMapped", False
        ),
    ):
        candidate = copy.deepcopy(policy)
        mutate(candidate)
        count += 1
        try:
            validate_policy(candidate)
        except ContractError:
            pass
        else:
            raise ContractError("adversarial policy mutation survived")
    with tempfile.TemporaryDirectory(prefix="dg_s19_notices_") as temporary:
        shared_output = Path(temporary) / "shared-authority.json"
        shared_bytes = b"deterministic shared authority\n"
        write_shared_bytes(shared_output, shared_bytes)
        count += 1
        if shared_output.read_bytes() != shared_bytes:
            raise ContractError("shared authority create-if-absent failed")

        fixed_timestamp_ns = 946684800_000_000_000
        os.utime(
            shared_output,
            ns=(fixed_timestamp_ns, fixed_timestamp_ns),
        )
        unchanged_timestamp_ns = shared_output.stat().st_mtime_ns
        write_shared_bytes(shared_output, shared_bytes)
        count += 1
        if (
            shared_output.read_bytes() != shared_bytes
            or shared_output.stat().st_mtime_ns != unchanged_timestamp_ns
        ):
            raise ContractError("identical shared authority generation was not a no-op")

        try:
            write_shared_bytes(shared_output, b"drifted shared authority\n")
        except ContractError:
            pass
        else:
            raise ContractError("mismatched shared authority did not fail closed")
        count += 1
        if (
            shared_output.read_bytes() != shared_bytes
            or shared_output.stat().st_mtime_ns != unchanged_timestamp_ns
        ):
            raise ContractError("mismatched shared authority changed historical evidence")

        archive = Path(temporary) / "Windows"
        archive.mkdir()
        for component in policy["components"]:
            for identity in component["stagedRuntimeDlls"]:
                path = archive.joinpath(*PurePosixPath(identity).parts)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(component["componentId"].encode("ascii"))
        (archive / "NOTICES.txt").write_bytes(first)
        (archive / "Manifest_NonUFSFiles_Win64.txt").write_text(
            "NOTICES.txt\t" + policy["noticeTemplate"]["manifestTimestampUtc"] + "\n",
            encoding="utf-8",
        )
        authority_bytes = json_bytes(authority)
        pass_output = Path(temporary) / "pass.json"
        _, passed = verify_archive(
            archive_root=archive,
            candidate_id="SELFTEST",
            verification_output=pass_output,
            policy=policy,
            policy_bytes=policy_bytes,
            authority=authority,
            authority_bytes=authority_bytes,
            notices_bytes=first,
        )
        count += 1
        if not passed:
            raise ContractError("complete synthetic archive did not pass")
        pass_bytes = pass_output.read_bytes()
        try:
            verify_archive(
                archive_root=archive,
                candidate_id="SELFTEST",
                verification_output=pass_output,
                policy=policy,
                policy_bytes=policy_bytes,
                authority=authority,
                authority_bytes=authority_bytes,
                notices_bytes=first,
            )
        except ContractError:
            pass
        else:
            raise ContractError("existing candidate verification output was reused")
        count += 1
        if pass_output.read_bytes() != pass_bytes:
            raise ContractError("exclusive output refusal changed candidate evidence")
        unknown = archive / "Unknown.dll"
        unknown.write_bytes(b"unknown")
        receipt, passed = verify_archive(
            archive_root=archive,
            candidate_id="SELFTEST_UNKNOWN",
            verification_output=Path(temporary) / "fail.json",
            policy=policy,
            policy_bytes=policy_bytes,
            authority=authority,
            authority_bytes=authority_bytes,
            notices_bytes=first,
        )
        count += 1
        if passed or receipt["coverage"]["unmappedStagedDllIdentityCount"] != 1:
            raise ContractError("unknown synthetic DLL did not fail closed")
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--authority-output", type=Path, default=DEFAULT_AUTHORITY_OUTPUT)
    parser.add_argument("--notices-output", type=Path, default=DEFAULT_NOTICES_OUTPUT)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--verification-output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    policy_bytes = read_stable(POLICY_PATH)
    policy = load_json_strict(POLICY_PATH)
    validate_policy(policy)
    engine_root = args.engine_root.resolve()
    if args.self_test:
        count = run_self_test(policy, policy_bytes, engine_root)
        print(f"SESSION 19 THIRD-PARTY NOTICE SELFTEST PASS: {count}/{count}")
        return 0

    authority, source_data = generate_authority(policy, policy_bytes, engine_root)
    authority_bytes = json_bytes(authority)
    notices_bytes = generate_notices(policy, authority, source_data)
    write_shared_bytes(args.authority_output.resolve(), authority_bytes)
    write_shared_bytes(args.notices_output.resolve(), notices_bytes)

    if args.archive is not None:
        if not args.candidate_id or args.verification_output is None:
            raise ContractError(
                "--archive requires --candidate-id and --verification-output"
            )
        receipt, passed = verify_archive(
            archive_root=args.archive.resolve(),
            candidate_id=args.candidate_id,
            verification_output=args.verification_output.resolve(),
            policy=policy,
            policy_bytes=policy_bytes,
            authority=authority,
            authority_bytes=authority_bytes,
            notices_bytes=notices_bytes,
        )
        print(
            f"SESSION 19 THIRD-PARTY NOTICE {receipt['state']}: "
            f"dlls={receipt['coverage']['stagedDllIdentityCount']} "
            f"unmapped={receipt['coverage']['unmappedStagedDllIdentityCount']} "
            f"notices_sha256={receipt['notices']['expectedSha256']}"
        )
        return 0 if passed else 2

    print(
        "SESSION 19 THIRD-PARTY NOTICE AUTHORITY GENERATED: "
        f"components={authority['coverage']['componentCount']} "
        f"dll_identities={authority['coverage']['supportedStagedDllIdentityCount']} "
        f"notices_sha256={hash_bytes(notices_bytes)} "
        "legal_approval=false distribution_clearance=false"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ContractError as exc:
        print(f"SESSION 19 THIRD-PARTY NOTICE FAIL_CLOSED: {exc}")
        raise SystemExit(2)
