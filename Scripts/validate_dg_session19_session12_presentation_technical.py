#!/usr/bin/env python3
"""Validate candidate-bound Session 12 presentation/input/audio technical evidence.

This is intentionally not a release or human-review gate. It accepts only the
implemented routing/focus/camera/replay/audio-event technical surface supported
by immutable source, automation, archive, and fresh-install artifacts.
"""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import sys
import tempfile
from typing import Any, Callable
import wave


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19Session12PresentationTechnicalPolicy.json"
STATE = "PASS_CANDIDATE_BOUND_SESSION12_PRESENTATION_TECHNICAL_WITH_SHIPPING_AUDIO_COOK"
RELEASE_STATE = "BLOCKED_PENDING_DEVICE_AUDIO_HUMAN_PROVENANCE_LEGAL_AND_RELEASE_APPROVALS"
RECEIPT_SCHEMA = "DiscGolfTour.Session19Session12PresentationTechnicalReceipt.v2"
TRUSTED_RECEIPT_SCHEMA = "DiscGolfTour.Session19Session12PresentationTechnicalReceipt.v3"
LEGACY_POLICY_SCHEMA = "DiscGolfTour.Session19Session12PresentationTechnicalPolicy.v2"
TRUSTED_POLICY_SCHEMA = "DiscGolfTour.Session19Session12PresentationTechnicalPolicy.v3"
LEGACY_MANIFEST_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1"
LEGACY_LAUNCH_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1"
TRUSTED_MANIFEST_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
TRUSTED_LAUNCH_SCHEMA = "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_RELATIVE = "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_TRUST_BOUNDARY = {
    "evidenceTrustModel": (
        "BOUNDED_OPERATIONAL_PROCESS_EVIDENCE_NOT_CRYPTOGRAPHIC_ATTESTATION"
    ),
    "cryptographicProcessAttestation": False,
    "tamperProofEvidence": False,
    "hostileSameUserForgeryResistance": False,
}
TRUSTED_RUNTIME_JOURNAL_CONTRACT = {
    "manifestSchema": TRUSTED_MANIFEST_SCHEMA,
    "manifestSchemaVersion": 2,
    "launchRecordSchema": TRUSTED_LAUNCH_SCHEMA,
    "launchRecordSchemaVersion": 2,
    "runtimeJournalSchema": RUNTIME_JOURNAL_SCHEMA,
    "runtimeJournalUserDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
    "runtimeJournalValidationState": RUNTIME_JOURNAL_PASS,
    "requireIndependentRawJournalValidation": True,
    "trustBoundary": RUNTIME_TRUST_BOUNDARY,
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
UUID_RE = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
CANDIDATE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
EXPLICIT_CANDIDATE_ID_RE = re.compile(
    r"^S19_WindowsShipping_(?P<utc>\d{8}T\d{6}Z)_(?P<nonce>[0-9a-f]{12})$"
)
UNREAL_LOG_UTC_RE = re.compile(
    r"\[(?P<utc>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}):\d{3}\]"
)
ABSLOG_PREFIX_RE = re.compile(r"(?i)(?<![A-Za-z0-9_-])-abslog=")
AUDIO_CATEGORIES = [
    ("ThrowRelease", "ResolveThrowRelease", "ThrowRelease"),
    ("AirborneFlight", "ResolveAirborneFlight", "DiscFlightLoop"),
    ("GroundContact", "ResolveCourseSurfaceContact", "DiscImpact"),
    ("GroundState", "ResolveGroundState", "DiscImpact"),
    ("BasketOutcome", "ResolveBasketOutcome", "BasketChainsOrCage"),
    ("Penalty", "ResolvePenalty", "CrowdReaction"),
    ("HoleStart", "ResolveHoleStart", "EnvironmentAmbience"),
    ("HoleCompletion", "ResolveHoleCompletion", "CrowdReaction"),
    ("HoleTransition", "ResolveHoleTransition", "EnvironmentAmbience"),
    ("RoundCompletion", "ResolveRoundCompletion", "CrowdReaction"),
    ("Replay", "ResolveReplay", "EnvironmentAmbience"),
    ("Flyover", "ResolveFlyover", "EnvironmentAmbience"),
]
HISTORICAL_CURRENT_LOG_TESTS = [
    "DiscGolfTour.Session12.Audio.SemanticProviderMapping",
    "DiscGolfTour.Session12.CameraReplay.ReplayActorControls",
    "DiscGolfTour.Session12.Input.RouteIsolation",
    "DiscGolfTour.Session17.RoundFlow.ActionWhitelist.Exact",
    "DiscGolfTour.Session17.RoundFlow.Widget.FocusAndStructure",
    "DiscGolfTour.Session19.Input.HoleIntroConfirmIsolation",
]
EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS = [
    "DiscGolfTour.Session19.Input.HoleIntroConfirmIsolation",
    "DiscGolfTour.Session19.Input.HoleIntroThrowTimingLifecycle",
]
EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS = [
    *HISTORICAL_CURRENT_LOG_TESTS,
    "DiscGolfTour.Session19.Input.HoleIntroThrowTimingLifecycle",
    "DiscGolfTour.Presentation.PersistentSemanticProxyCollisionContract",
]
HISTORICAL_TEMPLATE_AUTOMATION_LOGS = {
    "FOCUS_INTRO_RELEASE_LOG": (
        "Saved/Logs/Automation_S19_IntroReleaseBarrier_PostRoundFlowPatch.log"
    ),
    "CURRENT_FULL_AUTOMATION_LOG": (
        "Saved/Logs/Automation_DiscGolfTour_Full_PostRoundFlowPatch.log"
    ),
}


class ContractError(ValueError):
    pass


def _reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _strict_json(data: bytes, label: str) -> Any:
    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=_reject_duplicates,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ContractError(f"non-finite JSON value in {label}: {value}")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ContractError(f"invalid JSON {label}: {exc}") from exc


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def _stat_fingerprint(value: os.stat_result) -> tuple[int, int, int, int, int, int]:
    return (
        value.st_mode,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
        value.st_dev,
        value.st_ino,
    )


def _read_snapshot(
    path: Path, overrides: dict[Path, bytes]
) -> tuple[bytes, tuple[int, int, int, int, int, int] | None]:
    resolved = path.resolve()
    if resolved in overrides:
        return overrides[resolved], None
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if _stat_fingerprint(before) != _stat_fingerprint(after) or len(data) != after.st_size:
        raise ContractError(f"file changed while read: {path}")
    return data, _stat_fingerprint(after)


def _read(path: Path, overrides: dict[Path, bytes]) -> bytes:
    return _read_snapshot(path, overrides)[0]


def _embedded_abslog_values(text: str, label: str) -> list[str]:
    """Parse every Unreal ``-abslog=`` assignment without substring matching."""
    prefixes = list(ABSLOG_PREFIX_RE.finditer(text))
    if not prefixes:
        raise ContractError(f"{label} contains no embedded -abslog assignment")
    values: list[str] = []
    for prefix in prefixes:
        start = prefix.end()
        if start >= len(text):
            raise ContractError(f"{label} contains an empty -abslog assignment")
        if text[start] == '"':
            end = text.find('"', start + 1)
            if end < 0:
                raise ContractError(f"{label} contains an unterminated quoted -abslog")
            value = text[start + 1:end]
            trailer = end + 1
        else:
            end = start
            while end < len(text) and not text[end].isspace() and text[end] != '"':
                end += 1
            value = text[start:end]
            trailer = end
        while trailer < len(text) and text[trailer] == '"':
            trailer += 1
        if trailer < len(text) and not text[trailer].isspace():
            raise ContractError(f"{label} contains a malformed -abslog delimiter")
        if not value or "\x00" in value or "'" in value:
            raise ContractError(f"{label} contains an invalid -abslog value")
        values.append(value)
    return values


def _validate_embedded_abslog_identity(
    text: str,
    selected: Path,
    observed_fingerprint: tuple[int, int, int, int, int, int] | None,
    label: str,
    *,
    virtual: bool = False,
) -> None:
    """Bind all embedded aliases to one stable, selected regular file."""
    try:
        selected_resolved = selected.resolve(strict=not virtual)
    except OSError as exc:
        raise ContractError(f"{label} selected log is missing: {exc}") from exc
    if not virtual:
        if observed_fingerprint is None:
            raise ContractError(f"{label} lacks a selected-log byte snapshot")
        if (
            selected_resolved != selected
            or not selected.is_file()
            or selected.is_symlink()
            or _is_reparse(selected)
        ):
            raise ContractError(f"{label} selected log is not an exact regular file")
        try:
            before = selected.stat()
        except OSError as exc:
            raise ContractError(f"{label} selected log cannot be inspected: {exc}") from exc
        if _stat_fingerprint(before) != observed_fingerprint:
            raise ContractError(f"{label} selected log changed after its byte snapshot")

    embedded_paths: list[Path] = []
    for value in _embedded_abslog_values(text, label):
        embedded = Path(value)
        if not embedded.is_absolute() or ".." in embedded.parts:
            raise ContractError(f"{label} embedded -abslog is not an absolute direct path")
        try:
            resolved = embedded.resolve(strict=not virtual)
        except OSError as exc:
            raise ContractError(f"{label} embedded -abslog target is missing: {exc}") from exc
        if resolved != selected_resolved:
            raise ContractError(
                f"{label} embedded -abslog is foreign or ambiguous"
            )
        if not virtual and (
            not embedded.is_file() or embedded.is_symlink() or _is_reparse(embedded)
        ):
            raise ContractError(f"{label} embedded -abslog target is irregular")
        embedded_paths.append(embedded)

    if virtual:
        return
    try:
        after = selected.stat()
        aliases_still_exact = all(
            path.resolve(strict=True) == selected_resolved for path in embedded_paths
        )
    except OSError as exc:
        raise ContractError(f"{label} log path changed during validation: {exc}") from exc
    if _stat_fingerprint(after) != observed_fingerprint or not aliases_still_exact:
        raise ContractError(f"{label} log identity changed during provenance validation")


def _load(path: Path, overrides: dict[Path, bytes]) -> Any:
    return _strict_json(_read(path, overrides), str(path))


def _binding(path: Path, overrides: dict[Path, bytes], relative: str) -> dict[str, Any]:
    data = _read(path, overrides)
    return {"path": relative, "bytes": len(data), "sha256": _sha256(data)}


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _receipt_without_timestamp(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ContractError(f"{label} is not a receipt object")
    comparable = copy.deepcopy(value)
    comparable.pop("generatedUtc", None)
    return comparable


def _write_receipt_once_or_verify(path: Path, receipt: dict[str, Any]) -> None:
    """Publish a complete receipt exclusively, or verify an identical existing one."""
    payload = _json_bytes(receipt)
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = tempfile.NamedTemporaryFile(
        mode="wb", prefix=".session12-presentation-", suffix=".tmp",
        dir=path.parent, delete=False,
    )
    temporary = Path(handle.name)
    try:
        with handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        try:
            # A same-directory hard link publishes the already-flushed inode
            # atomically and fails if another process won the target name.
            os.link(temporary, path)
        except FileExistsError:
            if path.is_symlink() or _is_reparse(path) or not path.is_file():
                raise ContractError("existing receipt path is irregular")
            existing = _strict_json(_read(path, {}), str(path))
            if _receipt_without_timestamp(existing, "existing receipt") != _receipt_without_timestamp(
                receipt, "candidate receipt"
            ):
                raise ContractError("existing receipt differs; refusing overwrite")
    finally:
        try:
            temporary.unlink()
        except OSError:
            pass


def _canonical_relative(value: Any) -> str:
    if not isinstance(value, str):
        raise ContractError(f"relative path must be a string: {value!r}")
    normalized = value.replace("\\", "/")
    if (
        not normalized
        or normalized.startswith("/")
        or re.match(r"^[A-Za-z]:", normalized)
        or any(part in {"", ".", ".."} for part in normalized.split("/"))
    ):
        raise ContractError(f"non-canonical relative path: {value!r}")
    return normalized


def _candidate_relative(pattern: Any, candidate_id: str) -> str:
    if not isinstance(candidate_id, str) or CANDIDATE_ID_RE.fullmatch(candidate_id) is None:
        raise ContractError("candidate identifier is not safe for path substitution")
    if (
        not isinstance(pattern, str)
        or pattern.count("{candidateId}") != 1
        or "{" in pattern.replace("{candidateId}", "")
        or "}" in pattern.replace("{candidateId}", "")
    ):
        raise ContractError("candidate path pattern must contain exactly one {candidateId} placeholder")
    return _canonical_relative(pattern.replace("{candidateId}", candidate_id))


def _project_path(value: Any) -> tuple[Path, str]:
    relative = _canonical_relative(value)
    path = (ROOT / relative).resolve()
    try:
        path.relative_to(ROOT.resolve())
    except ValueError as exc:
        raise ContractError(f"project path resolves outside the project root: {relative}") from exc
    return path, relative


def _selected_policy_location(path: Path) -> tuple[Path, str]:
    resolved = path.resolve()
    try:
        relative = resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError as exc:
        raise ContractError("selected policy must be inside the project root") from exc
    return resolved, _canonical_relative(relative)


def _trusted_runtime_journal_required(policy: Any) -> bool:
    return (
        isinstance(policy, dict)
        and policy.get("schema") == TRUSTED_POLICY_SCHEMA
        and type(policy.get("schemaVersion")) is int
        and policy.get("schemaVersion") == 3
    )


def _external_schema_contract_errors(
    policy: dict[str, Any], manifest: Any, launch: Any
) -> list[str]:
    trusted = _trusted_runtime_journal_required(policy)
    expected_manifest_schema = (
        TRUSTED_MANIFEST_SCHEMA if trusted else LEGACY_MANIFEST_SCHEMA
    )
    expected_manifest_version = 2 if trusted else 1
    expected_launch_schema = TRUSTED_LAUNCH_SCHEMA if trusted else LEGACY_LAUNCH_SCHEMA
    expected_launch_version = 2 if trusted else 1
    errors: list[str] = []
    if not isinstance(manifest, dict):
        errors.append("external manifest must be an object")
    else:
        if manifest.get("schema") != expected_manifest_schema:
            errors.append("external manifest schema differs from selected policy lane")
        if (
            type(manifest.get("schemaVersion")) is not int
            or manifest.get("schemaVersion") != expected_manifest_version
        ):
            errors.append("external manifest schemaVersion differs from selected policy lane")
    if not isinstance(launch, dict):
        errors.append("external launch record must be an object")
    else:
        if launch.get("schema") != expected_launch_schema:
            errors.append("external launch schema differs from selected policy lane")
        if (
            type(launch.get("schemaVersion")) is not int
            or launch.get("schemaVersion") != expected_launch_version
        ):
            errors.append("external launch schemaVersion differs from selected policy lane")
    return errors


def _trusted_runtime_journal_binding(
    *,
    archive: Path,
    user_dir: Path,
    manifest_path: Path,
    launch_path: Path,
    manifest: dict[str, Any],
    launch: dict[str, Any],
    policy: dict[str, Any],
    manifest_sha: str,
) -> dict[str, Any]:
    errors = _external_schema_contract_errors(policy, manifest, launch)
    if errors:
        raise ContractError("; ".join(errors))
    candidate = policy["candidate"]
    executable = archive / candidate["shippingExeRelativePath"]
    try:
        from validate_dg_session19_external_technical_evidence import validate_evidence

        validation = validate_evidence(
            archive_root=archive,
            shipping_exe=executable,
            user_dir=user_dir,
            manifest_path=manifest_path,
        )
    except Exception as exc:
        raise ContractError(
            "trusted runtime journal validation failed closed: "
            f"{exc.__class__.__name__}"
        ) from exc
    if (
        validation.get("state")
        != "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
        or validation.get("failures") != []
    ):
        codes = sorted({
            str(item.get("code"))
            for item in validation.get("failures", [])
            if isinstance(item, dict)
        })
        raise ContractError(
            "trusted runtime journal independent validation failed: "
            + (",".join(codes) if codes else "UNKNOWN")
        )
    if validation.get("manifest", {}).get("sha256") != manifest_sha:
        raise ContractError("trusted runtime journal manifest changed across validation")

    contract = policy.get("trustedRuntimeJournalContract")
    runtime = manifest.get("evidence", {}).get("runtimeCheckpointJournal")
    launch_binding = manifest.get("evidence", {}).get("launchRecord")
    if (
        contract != TRUSTED_RUNTIME_JOURNAL_CONTRACT
        or not isinstance(runtime, dict)
        or not isinstance(launch_binding, dict)
    ):
        raise ContractError("trusted runtime journal contract or manifest binding differs")
    launch_data = _read(launch_path, {})
    if (
        set(launch_binding) != {"relativePath", "bytes", "sha256"}
        or launch_binding.get("relativePath") != "launch-record.json"
        or type(launch_binding.get("bytes")) is not int
        or launch_binding.get("bytes") != len(launch_data)
        or not isinstance(launch_binding.get("sha256"), str)
        or SHA256_RE.fullmatch(launch_binding.get("sha256", "")) is None
        or launch_binding.get("sha256") != _sha256(launch_data)
    ):
        raise ContractError("trusted runtime journal launch-record binding differs")
    expected_keys = {
        "bytes", "captureNonce", "eventCount", "firstSequence", "lastSequence",
        "modifiedUtc", "modifiedWithinRunWindow", "roundId", "schema", "sha256",
        "userDirRelativePath", "validationFailures", "validationState",
    }
    if set(runtime) != expected_keys:
        raise ContractError("trusted runtime journal manifest fields differ")
    if (
        runtime.get("schema") != contract["runtimeJournalSchema"]
        or runtime.get("userDirRelativePath")
        != contract["runtimeJournalUserDirRelativePath"]
        or runtime.get("validationState") != contract["runtimeJournalValidationState"]
        or runtime.get("validationFailures") != []
        or runtime.get("modifiedWithinRunWindow") is not True
        or runtime.get("captureNonce") != launch.get("captureNonce")
        or launch.get("runtimeCheckpointJournalUserDirRelativePath")
        != contract["runtimeJournalUserDirRelativePath"]
        or manifest.get("candidate", {}).get("candidateId") != candidate["candidateId"]
        or type(runtime.get("bytes")) is not int
        or runtime.get("bytes", 0) <= 0
        or type(runtime.get("eventCount")) is not int
        or runtime.get("eventCount") != 12
        or type(runtime.get("firstSequence")) is not int
        or runtime.get("firstSequence") != 1
        or type(runtime.get("lastSequence")) is not int
        or runtime.get("lastSequence") != 12
        or not isinstance(runtime.get("sha256"), str)
        or SHA256_RE.fullmatch(runtime.get("sha256", "")) is None
        or not isinstance(runtime.get("captureNonce"), str)
        or UUID_RE.fullmatch(runtime.get("captureNonce", "")) is None
        or not isinstance(runtime.get("roundId"), str)
        or UUID_RE.fullmatch(runtime.get("roundId", "")) is None
    ):
        raise ContractError("trusted runtime journal manifest/launch binding differs")
    return {
        "schema": runtime["schema"],
        "userDirRelativePath": runtime["userDirRelativePath"],
        "bytes": runtime["bytes"],
        "sha256": runtime["sha256"],
        "captureNonce": runtime["captureNonce"],
        "roundId": runtime["roundId"],
        "eventCount": runtime["eventCount"],
        "firstSequence": runtime["firstSequence"],
        "lastSequence": runtime["lastSequence"],
        "modifiedWithinRunWindow": True,
        "validationState": runtime["validationState"],
        "independentValidationState": validation["state"],
        "launchRecordSha256": launch_binding["sha256"],
        "technicalEvidenceManifestSha256": manifest_sha,
        "executableSha256": candidate["shippingExeSha256"],
        "archiveManifestSha256": candidate["archiveCanonicalManifestSha256"],
        "trustBoundary": copy.deepcopy(RUNTIME_TRUST_BOUNDARY),
    }


def _is_reparse(path: Path) -> bool:
    return bool(getattr(path.lstat(), "st_file_attributes", 0) & 0x400)


def _hash_file(path: Path) -> tuple[int, str]:
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ContractError(f"file changed while hashed: {path}")
    return after.st_size, digest.hexdigest().upper()


def _collect_archive(archive: Path) -> dict[str, Any]:
    if not archive.is_dir() or archive.is_symlink() or _is_reparse(archive):
        raise ContractError("archive root must be a regular non-reparse directory")
    root = archive.resolve(strict=True)
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for current, directories, files in os.walk(archive, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if child.is_symlink() or _is_reparse(child):
                raise ContractError("archive contains a reparse directory")
        for name in files:
            child = current_path / name
            relative = _canonical_relative(child.relative_to(archive).as_posix())
            if relative.casefold() in seen:
                raise ContractError("archive contains a case-insensitive duplicate")
            seen.add(relative.casefold())
            if child.is_symlink() or _is_reparse(child) or not child.is_file():
                raise ContractError("archive contains an irregular file")
            if root not in child.resolve(strict=True).parents:
                raise ContractError("archive entry resolves outside archive")
            byte_count, digest = _hash_file(child)
            records.append({"path": relative, "bytes": byte_count, "sha256": digest})
    records.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    payload = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return {
        "fileCount": len(records),
        "bytes": sum(item["bytes"] for item in records),
        "canonicalManifestSha256": _sha256(payload),
        "records": records,
    }


def _png_dimensions(data: bytes, label: str) -> tuple[int, int]:
    if len(data) < 33 or data[:8] != PNG_SIGNATURE:
        raise ContractError(f"not a PNG milestone: {label}")
    if struct.unpack(">I", data[8:12])[0] != 13 or data[12:16] != b"IHDR":
        raise ContractError(f"PNG has no canonical IHDR: {label}")
    width, height = struct.unpack(">II", data[16:24])
    if width < 1280 or height < 720:
        raise ContractError(f"milestone resolution below 1280x720: {label}")
    return width, height


def _exact_identity(data: bytes, expected: dict[str, Any], label: str) -> None:
    if len(data) != expected.get("bytes") or _sha256(data) != expected.get("sha256"):
        raise ContractError(f"immutable identity differs: {label}")


def _validate_policy(policy: Any) -> None:
    if not isinstance(policy, dict):
        raise ContractError("policy root must be an object")
    trusted_policy = _trusted_runtime_journal_required(policy)
    head = {
        "schema": TRUSTED_POLICY_SCHEMA if trusted_policy else LEGACY_POLICY_SCHEMA,
        "schemaVersion": 3 if trusted_policy else 2,
        "session": 19,
        "policyId": "windows_shipping_session12_presentation_input_audio_candidate_bound_technical_v2",
        "receiptPattern": "Evidence/Session19/Session12PresentationTechnicalAudioCoverage-{candidateId}.json",
        "selfTestMinimumMutationCount": 30,
        "releaseRequiredExitCode": 2,
    }
    for key, expected in head.items():
        if policy.get(key) != expected:
            raise ContractError(f"policy.{key} differs")
    expected_policy_keys = set(head) | {
        "candidate", "projectBindings", "dynamicCandidateEvidence", "sourceBindings",
        "requiredFocusedTests", "requiredCameraTests", "requiredCurrentLogTests",
        "audioCoverage", "originalAudioCandidate", "externalBindings",
        "requiredMilestones", "technicalClaims", "unsupportedClaims",
    }
    if trusted_policy:
        expected_policy_keys.add("trustedRuntimeJournalContract")
    if set(policy) != expected_policy_keys:
        raise ContractError("policy root keys differ")
    if (
        trusted_policy
        and policy.get("trustedRuntimeJournalContract")
        != TRUSTED_RUNTIME_JOURNAL_CONTRACT
    ):
        raise ContractError("policy trusted runtime journal contract differs")
    candidate = policy.get("candidate", {})
    if not isinstance(candidate, dict) or set(candidate) != {
        "candidateId", "archiveLeaf", "externalRunTokenPattern", "archiveFileCount",
        "archiveBytes", "archiveCanonicalManifestSha256", "shippingExeRelativePath",
        "shippingExeBytes", "shippingExeSha256",
    }:
        raise ContractError("policy candidate shape differs")
    if (
        not isinstance(candidate.get("candidateId"), str)
        or CANDIDATE_ID_RE.fullmatch(candidate.get("candidateId", "")) is None
        or candidate.get("archiveLeaf") != "Windows"
        or candidate.get("externalRunTokenPattern") != "{candidateId}_{uuid}"
        or candidate.get("shippingExeRelativePath")
        != "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
    ):
        raise ContractError("policy candidate binding differs")
    for key in ("archiveFileCount", "archiveBytes", "shippingExeBytes"):
        if not isinstance(candidate[key], int) or isinstance(candidate[key], bool) or candidate[key] <= 0:
            raise ContractError(f"policy candidate {key} differs")
    for key in ("archiveCanonicalManifestSha256", "shippingExeSha256"):
        if not isinstance(candidate[key], str) or SHA256_RE.fullmatch(candidate[key]) is None:
            raise ContractError(f"policy candidate {key} differs")
    project_bindings = policy.get("projectBindings")
    if not isinstance(project_bindings, list) or any(not isinstance(item, dict) for item in project_bindings):
        raise ContractError("policy project bindings are not objects")
    project_roles = [item.get("role") for item in project_bindings]
    expected_roles = [
        "SESSION12_CONTRACT", "SESSION12_VALIDATOR", "AUDIO_EVENT_COVERAGE_MANIFEST",
        "ORIGINAL_AUDIO_CANDIDATE_PACK", "ORIGINAL_AUDIO_IMPORT_RECEIPT",
        "SHIPPING_AUDIO_COOK_POLICY", "SHIPPING_AUDIO_COOK_RECEIPT",
        "ORIGINAL_AUDIO_IMPORT_LOG", "ORIGINAL_AUDIO_AUTOMATION_LOG",
        "FOCUSED_AUTOMATION_INDEX",
        "CAMERA_AUTOMATION_INDEX", "FOCUSED_AUTOMATION_LOG", "CAMERA_AUTOMATION_LOG",
        "FOCUS_ACTION_WHITELIST_LOG", "FOCUS_INTRO_RELEASE_LOG",
        "CURRENT_FULL_AUTOMATION_LOG", "SHIPPING_CANDIDATE_VERIFICATION",
        "CANDIDATE_CONTENT", "SHIPPING_BINARY",
    ]
    if project_roles != expected_roles:
        raise ContractError("policy project roles differ")
    for item in project_bindings:
        if set(item) != {"role", "path", "bytes", "sha256"}:
            raise ContractError(f"policy project binding shape differs: {item.get('role')}")
        _project_path(item["path"])
        if (
            not isinstance(item["bytes"], int) or isinstance(item["bytes"], bool)
            or item["bytes"] <= 0
            or not isinstance(item["sha256"], str)
            or SHA256_RE.fullmatch(item["sha256"]) is None
        ):
            raise ContractError(f"policy project binding identity differs: {item['role']}")
    required_current_log_tests = policy.get("requiredCurrentLogTests")
    if required_current_log_tests == HISTORICAL_CURRENT_LOG_TESTS:
        explicit_candidate_logs = False
    elif required_current_log_tests == EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS:
        explicit_candidate_logs = True
    else:
        raise ContractError("policy current full automation test set differs")
    if explicit_candidate_logs:
        project_by_role = {item["role"]: item for item in project_bindings}
        focused_binding = project_by_role["FOCUS_INTRO_RELEASE_LOG"]
        full_binding = project_by_role["CURRENT_FULL_AUTOMATION_LOG"]
        explicit_match = EXPLICIT_CANDIDATE_ID_RE.fullmatch(candidate["candidateId"])
        if explicit_match is None:
            raise ContractError("policy explicit automation candidate ID is not timestamped")
        try:
            datetime.strptime(explicit_match.group("utc"), "%Y%m%dT%H%M%SZ")
        except ValueError as exc:
            raise ContractError("policy explicit automation candidate UTC is invalid") from exc
        expected_suffix = f"-{candidate['candidateId']}.log"
        for role in ("FOCUS_INTRO_RELEASE_LOG", "CURRENT_FULL_AUTOMATION_LOG"):
            if not PurePosixPath(project_by_role[role]["path"]).name.endswith(
                expected_suffix
            ):
                raise ContractError(
                    f"policy explicit automation filename is not candidate-scoped: {role}"
                )
        for role, historical_path in HISTORICAL_TEMPLATE_AUTOMATION_LOGS.items():
            if project_by_role[role]["path"].casefold() == historical_path.casefold():
                raise ContractError(
                    f"policy explicit candidate automation reuses historical template path: {role}"
                )
        if (
            focused_binding["path"].casefold() == full_binding["path"].casefold()
            or focused_binding["sha256"] == full_binding["sha256"]
        ):
            raise ContractError("policy explicit candidate automation identities are not distinct")
    dynamic_evidence = policy.get("dynamicCandidateEvidence")
    if not isinstance(dynamic_evidence, list) or any(not isinstance(item, dict) for item in dynamic_evidence):
        raise ContractError("policy dynamic evidence entries are not objects")
    dynamic_roles = [item.get("role") for item in dynamic_evidence]
    if dynamic_roles != ["EXTERNAL_TECHNICAL_VALIDATION", "FRESH_USERDIR_VALIDATION"]:
        raise ContractError("policy dynamic evidence roles differ")
    for item in dynamic_evidence:
        if set(item) != {"role", "pattern", "requiredSchema", "requiredState"}:
            raise ContractError(f"policy dynamic evidence shape differs: {item.get('role')}")
        relative = _candidate_relative(item["pattern"], candidate["candidateId"])
        _project_path(relative)
        if not isinstance(item["requiredSchema"], str) or not isinstance(item["requiredState"], str):
            raise ContractError(f"policy dynamic evidence contract differs: {item['role']}")
    source_bindings = policy.get("sourceBindings")
    if not isinstance(source_bindings, list) or any(not isinstance(item, dict) for item in source_bindings):
        raise ContractError("policy source bindings are not objects")
    if len(source_bindings) < 19:
        raise ContractError("policy source closure is incomplete")
    paths = [item.get("path") for item in source_bindings]
    if len(paths) != len({str(path).casefold() for path in paths}):
        raise ContractError("policy source bindings contain duplicates")
    for item in source_bindings:
        if set(item) != {"path", "bytes", "sha256", "markers"}:
            raise ContractError("policy source binding shape differs")
        _project_path(item["path"])
        if (
            not isinstance(item["bytes"], int) or isinstance(item["bytes"], bool)
            or item["bytes"] <= 0
            or not isinstance(item["sha256"], str)
            or SHA256_RE.fullmatch(item["sha256"]) is None
            or not isinstance(item["markers"], list) or not item["markers"]
            or any(not isinstance(marker, str) or not marker for marker in item["markers"])
        ):
            raise ContractError(f"policy source binding identity differs: {item.get('path')}")
    required_audio_sources = {
        "Source/DiscGolfTour/DiscGolfPresentationAudio.h",
        "Source/DiscGolfTour/DiscGolfPresentationAudio.cpp",
        "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.h",
        "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp",
        "Source/DiscGolfTour/DiscGolfTourGameMode.cpp",
        "Source/DiscGolfTour/Tests/DiscGolfSession19AudioCandidateTests.cpp",
        "Scripts/generate_dg_session19_audio_candidate_pack.py",
        "Scripts/author_dg_session19_audio_candidate_pack.py",
    }
    if not required_audio_sources.issubset(set(paths)):
        raise ContractError("policy audio source closure is incomplete")
    if len(policy.get("requiredFocusedTests", [])) != 16:
        raise ContractError("policy focused test set differs")
    if len(policy.get("requiredCameraTests", [])) != 6:
        raise ContractError("policy camera test set differs")
    audio = policy.get("audioCoverage", {})
    if audio != {
        "manifestRole": "AUDIO_EVENT_COVERAGE_MANIFEST",
        "requiredCategoryCount": 12,
        "orderedCategories": [item[0] for item in AUDIO_CATEGORIES],
        "runtimeHookCoverageRequired": True,
        "productionAssetCoverageRequired": False,
        "intentionalSilenceCategoryCount": 0,
        "unboundProductionAssetCategoryCount": 12,
        "rawProductionAudioFileCount": 0,
    }:
        raise ContractError("policy audio coverage boundary differs")
    original = policy.get("originalAudioCandidate", {})
    if original != {
        "manifestRole": "ORIGINAL_AUDIO_CANDIDATE_PACK",
        "categoryCount": 12,
        "sourceMasterCount": 12,
        "runtimeSoftBindingCount": 12,
        "cookRoot": "/Game/Presentation/Audio/Generated",
        "projectOwnedOriginalSynthesis": True,
        "externalSamplesUsed": False,
        "serializedUnrealAssetsPresent": True,
        "shippingCookReceiptRole": "SHIPPING_AUDIO_COOK_RECEIPT",
        "shippingCookProven": True,
        "audibleRuntimePlaybackProven": False,
        "selectedShippingCandidateContainsAudioCandidate": True,
        "shippingCandidateInvalidatedBySourceDelta": False,
        "freshShippingRebuildRequired": False,
    }:
        raise ContractError("policy original audio candidate boundary differs")
    milestones = policy.get("requiredMilestones", [])
    if not isinstance(milestones, list):
        raise ContractError("policy milestones are not an array")
    milestone_paths = [
        _canonical_relative(item.get("path")) if isinstance(item, dict) else ""
        for item in milestones
    ]
    if (
        len(milestones) != 10
        or any(set(item) != {"path", "semantic"} for item in milestones if isinstance(item, dict))
        or any(not isinstance(item, dict) for item in milestones)
        or len({path.casefold() for path in milestone_paths}) != 10
        or any(not path.startswith("Screenshots/") or not path.casefold().endswith(".png") for path in milestone_paths)
        or any(not isinstance(item.get("semantic"), str) or not item["semantic"] for item in milestones)
    ):
        raise ContractError("policy milestone set differs")
    external_bindings = policy.get("externalBindings")
    if not isinstance(external_bindings, list) or any(not isinstance(item, dict) for item in external_bindings):
        raise ContractError("policy external bindings are not objects")
    external_roles = [item.get("role") for item in external_bindings]
    if external_roles != ["LAUNCH_RECORD", "TECHNICAL_MANIFEST", "TECHNICAL_MANIFEST_SIDECAR"]:
        raise ContractError("policy external binding roles differ")
    external_paths: list[str] = []
    for item in external_bindings:
        if set(item) != {"role", "path"}:
            raise ContractError(f"policy external binding shape differs: {item.get('role')}")
        external_paths.append(_canonical_relative(item["path"]))
    if len(external_paths) != len({path.casefold() for path in external_paths}):
        raise ContractError("policy external bindings contain duplicate paths")
    _candidate_relative(policy.get("receiptPattern"), candidate["candidateId"])
    technical = policy.get("technicalClaims", {})
    if not technical or any(value is not True for value in technical.values()):
        raise ContractError("policy technical claim boundary differs")
    unsupported = policy.get("unsupportedClaims", {})
    required_false = {
        "unsupportedDeviceCoverage", "productionAudioAssetsPresent",
        "audibleProductionCategoryCoverage", "intentionalSilenceDesignApproval",
        "audibleRuntimePlaybackProven",
        "audioLicensingApproved", "manualAudioMixReview", "manualCameraReview",
        "manualReplayReview", "accessibilityApproval", "visualQualityApproval",
        "productOwnerApproval", "humanPlayFeelApproval", "manualGameplayAcceptance",
        "provenanceApproval", "legalApproval", "distributionClearance", "blockerClosed",
        "releaseApproval", "releaseReady",
    }
    if set(unsupported) != required_false or any(value is not False for value in unsupported.values()):
        raise ContractError("policy unsupported claim boundary differs")


def _validate_contract(contract: Any) -> None:
    if not isinstance(contract, dict):
        raise ContractError("Session 12 contract is not an object")
    expected = {
        "schema": "dg.session12.presentation.contract",
        "contract_id": "DG_SESSION12_CAMERA_REPLAY_INPUT_UI_AUDIO_V1",
        "normal_status": "PASS_SESSION12_PRESENTATION_CONTRACT_RELEASE_BLOCKED",
        "implementation_status": "TECHNICAL_RUNTIME_IMPLEMENTATION_ACCEPTED_PRODUCTION_READINESS_PENDING",
        "feature_complete": True,
        "releaseBlocked": True,
        "release_ready": False,
        "release_use_allowed": False,
    }
    for key, value in expected.items():
        if contract.get(key) != value:
            raise ContractError(f"Session 12 contract boundary differs: {key}")
    router = contract.get("audio_router_contract", {})
    if (
        router.get("owner") != "UDiscGolfPresentationAudioRouterComponent"
        or router.get("semantic_trigger_owner") != "DiscGolfPresentationAudio"
        or router.get("production_audio_assets_present") is not False
        or router.get("gameplay_mutation_allowed") is not False
        or router.get("synchronous_asset_load_during_routing") is not False
        or router.get("missing_asset_result")
        != "SUCCESSFUL_SILENT_ROUTE_WITH_EXPLICIT_STATUS"
    ):
        raise ContractError("Session 12 audio router truth boundary differs")
    gate = contract.get("exit_gate", {})
    if (
        gate.get("technical_audio_router_complete") is not True
        or gate.get("all_capabilities_technical_pass") is not True
        or gate.get("production_readiness_approved") is not False
    ):
        raise ContractError("Session 12 exit gate differs")
    if len(contract.get("release_blockers", [])) != 11:
        raise ContractError("Session 12 blocker count differs")


def _run_session12_validator() -> dict[str, Any]:
    result = subprocess.run(
        [sys.executable, str(ROOT / "Scripts/validate_dg_session12_presentation.py")],
        cwd=ROOT, capture_output=True, text=True, timeout=30, check=False,
    )
    stdout = result.stdout.replace("\r\n", "\n").strip()
    required = (
        "Session 12 presentation contract: PASS_SESSION12_PRESENTATION_CONTRACT_RELEASE_BLOCKED",
        "Pending capabilities: 0",
        "Release blocked: yes (11 blockers)",
    )
    if result.returncode != 0 or any(marker not in stdout for marker in required):
        raise ContractError("Session 12 contract validator did not return its passing blocked state")
    return {"exitCode": result.returncode, "stdoutSha256": _sha256(stdout.encode("utf-8")), "status": required[0].split(": ", 1)[1]}


def _validate_index(index: Any, expected_tests: list[str], label: str) -> dict[str, Any]:
    if not isinstance(index, dict):
        raise ContractError(f"{label} index is not an object")
    if (
        index.get("succeeded") != len(expected_tests)
        or index.get("succeededWithWarnings") != 0
        or index.get("failed") != 0
        or index.get("notRun") != 0
        or index.get("inProcess") != 0
    ):
        raise ContractError(f"{label} summary is not clean")
    tests = index.get("tests")
    if not isinstance(tests, list) or len(tests) != len(expected_tests):
        raise ContractError(f"{label} test count differs")
    paths = [item.get("fullTestPath") for item in tests]
    if paths != expected_tests:
        raise ContractError(f"{label} exact ordered test list differs")
    if any(
        item.get("state") != "Success"
        or item.get("warnings") != 0
        or item.get("errors") != 0
        for item in tests
    ):
        raise ContractError(f"{label} contains a non-clean test")
    devices = index.get("devices", [])
    if len(devices) != 1 or devices[0].get("platform") != "WindowsEditor":
        raise ContractError(f"{label} device scope differs")
    return {
        "succeeded": len(tests),
        "failed": 0,
        "warnings": 0,
        "deviceScope": "ONE_WINDOWS_EDITOR_DEVICE_ONLY",
        "unsupportedDeviceCoverage": False,
    }


def _success_line(path: str) -> str:
    return f"Test Completed. Result={{Success}} Name="


def _validate_log(text: str, tests: list[str], label: str, found_count: int | None = None) -> None:
    if "Test Completed. Result={Fail}" in text or "Test Completed. Result={Error}" in text:
        raise ContractError(f"{label} contains a failed/error test")
    if found_count is not None and f"Found {found_count} automation tests" not in text:
        raise ContractError(f"{label} discovery count differs")
    for test in tests:
        pattern = re.compile(
            r"Test Completed\. Result=\{Success\} Name=\{[^}]+\} Path=\{" + re.escape(test) + r"\}"
        )
        if not pattern.search(text):
            raise ContractError(f"{label} lacks success for {test}")


def _validate_full_automation_log(text: str, tests: list[str]) -> int:
    _validate_log(text, tests, "current full automation")
    discoveries = re.findall(r"Found (\d+) automation tests", text)
    if len(discoveries) != 1:
        raise ContractError("current full automation discovery summary is not unique")
    discovered = int(discoveries[0])
    if discovered < 255:
        raise ContractError("current full automation coverage regressed below the historical floor")
    if text.count("Test Completed. Result={Success}") != discovered:
        raise ContractError("current full automation success count differs from discovery")
    if text.count("**** TEST COMPLETE. EXIT CODE: 0 ****") != 1:
        raise ContractError("current full automation did not exit cleanly")
    return discovered


def _validate_explicit_candidate_log_provenance(
    text: str,
    relative: str,
    candidate_id: str,
    label: str,
    observed_fingerprint: tuple[int, int, int, int, int, int] | None,
    *,
    virtual: bool = False,
) -> None:
    match = EXPLICIT_CANDIDATE_ID_RE.fullmatch(candidate_id)
    if match is None:
        raise ContractError(f"{label} candidate ID is not timestamped")
    try:
        build_utc = datetime.strptime(
            match.group("utc"), "%Y%m%dT%H%M%SZ"
        ).replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ContractError(f"{label} candidate UTC is invalid") from exc
    pure = PurePosixPath(relative)
    lexical = ROOT.joinpath(*pure.parts)
    if not virtual:
        cursor = ROOT
        for part in pure.parts:
            cursor /= part
            if os.path.lexists(cursor) and (cursor.is_symlink() or _is_reparse(cursor)):
                raise ContractError(f"{label} selected project path contains a reparse point")
    path = lexical.resolve(strict=not virtual)
    _validate_embedded_abslog_identity(
        text,
        path,
        observed_fingerprint,
        label,
        virtual=virtual,
    )
    observed_times: list[datetime] = []
    for timestamp in UNREAL_LOG_UTC_RE.finditer(text):
        try:
            observed_times.append(
                datetime.strptime(
                    timestamp.group("utc"), "%Y.%m.%d-%H.%M.%S"
                ).replace(tzinfo=timezone.utc)
            )
        except ValueError as exc:
            raise ContractError(f"{label} Unreal UTC timestamp is invalid") from exc
    if not observed_times:
        raise ContractError(f"{label} lacks Unreal UTC timestamps")
    if min(observed_times) < build_utc:
        raise ContractError(f"{label} contains pre-candidate Unreal log content")


def _validate_audio_coverage(
    policy: dict[str, Any], manifest: Any, overrides: dict[Path, bytes]
) -> dict[str, Any]:
    if not isinstance(manifest, dict):
        raise ContractError("audio coverage manifest is not an object")
    expected_head = {
        "schema": "dg.session12.audio.event_coverage",
        "schemaVersion": 1,
        "manifestId": "DG_SESSION12_SEMANTIC_AUDIO_RUNTIME_HOOK_COVERAGE_V1",
        "coverageState":
            "SOURCE_CONTROLLED_RUNTIME_HOOK_COVERAGE_COMPLETE_ASSET_COVERAGE_ABSENT",
        "categoryCount": len(AUDIO_CATEGORIES),
        "runtimeHookCoverageComplete": True,
        "productionAssetCoverageComplete": False,
        "intentionalSilenceCategoryCount": 0,
        "unboundProductionAssetCategoryCount": len(AUDIO_CATEGORIES),
    }
    for key, expected in expected_head.items():
        if manifest.get(key) != expected:
            raise ContractError(f"audio coverage manifest differs: {key}")
    if manifest.get("authority") != {
        "semanticResolver": "DiscGolfPresentationAudio",
        "lifecycleOwner": "ADiscGolfTourGameMode",
        "providerRouter": "UDiscGolfPresentationAudioRouterComponent",
        "gameplayMutationAllowed": False,
    }:
        raise ContractError("audio coverage authority differs")

    categories = manifest.get("categories")
    if not isinstance(categories, list) or len(categories) != len(AUDIO_CATEGORIES):
        raise ContractError("audio coverage category inventory differs")
    names = [item.get("category") for item in categories if isinstance(item, dict)]
    if names != [item[0] for item in AUDIO_CATEGORIES] or len(names) != len(set(names)):
        raise ContractError("audio coverage category order or uniqueness differs")

    vocabulary = _read(
        ROOT / "Source/DiscGolfTour/DiscGolfPresentationAudio.h", overrides
    ).decode("utf-8", errors="strict")
    router = _read(
        ROOT / "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp", overrides
    ).decode("utf-8", errors="strict")
    hook_cache: dict[str, str] = {}
    dispositions: list[dict[str, Any]] = []
    for item, expected in zip(categories, AUDIO_CATEGORIES):
        if not isinstance(item, dict):
            raise ContractError("audio coverage category entry is not an object")
        category, resolver, provider = expected
        if (
            item.get("category") != category
            or item.get("resolver") != resolver
            or item.get("providerType") != provider
            or item.get("hookFile") != "Source/DiscGolfTour/DiscGolfTourGameMode.cpp"
            or item.get("runtimeDisposition")
            != "ROUTE_TO_PROVIDER_OR_EXPLICIT_SILENT_FALLBACK"
            or item.get("assetDisposition") != "UNBOUND_PRODUCTION_ASSET"
            or item.get("productionAssetPath") is not None
            or item.get("intentionalSilence") is not False
            or item.get("technicalHookValidated") is not True
            or item.get("audibleAssetValidated") is not False
            or not isinstance(item.get("lifecycle"), str)
            or not item.get("lifecycle")
        ):
            raise ContractError(f"audio coverage disposition differs: {category}")
        markers = item.get("hookMarkers")
        if (
            not isinstance(markers, list)
            or not markers
            or not all(isinstance(marker, str) and marker for marker in markers)
        ):
            raise ContractError(f"audio coverage hook markers differ: {category}")
        hook_path = _canonical_relative(item["hookFile"])
        if hook_path not in hook_cache:
            hook_cache[hook_path] = _read(ROOT / hook_path, overrides).decode(
                "utf-8", errors="strict"
            )
        if f"    {category}" not in vocabulary or f"{resolver}(" not in vocabulary:
            raise ContractError(f"semantic audio vocabulary differs: {category}")
        if any(marker not in hook_cache[hook_path] for marker in markers):
            raise ContractError(f"runtime audio hook differs: {category}")
        if f"case EDiscGolfPresentationAudioCategory::{category}:" not in router:
            raise ContractError(f"audio router category mapping differs: {category}")
        dispositions.append({
            "category": category,
            "resolver": resolver,
            "providerType": provider,
            "lifecycle": item["lifecycle"],
            "runtimeHookValidated": True,
            "assetDisposition": "UNBOUND_PRODUCTION_ASSET",
            "intentionalSilence": False,
            "audibleAssetValidated": False,
        })

    router_evidence = manifest.get("routerEvidence", {})
    if (
        router_evidence.get("sourceFile")
        != "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp"
        or router_evidence.get("semanticIdPreserved") is not True
        or router_evidence.get("duplicateSuppression") is not True
        or router_evidence.get("boundedRouteTrace") != 64
        or router_evidence.get("synchronousLoadDuringRouting") is not False
        or router_evidence.get("missingAssetIsExplicitSilentFallback") is not True
    ):
        raise ContractError("audio router evidence boundary differs")
    router_markers = router_evidence.get("requiredMarkers")
    if (
        not isinstance(router_markers, list)
        or len(router_markers) < 6
        or any(not isinstance(marker, str) or marker not in router for marker in router_markers)
        or "LoadSynchronous" in router
    ):
        raise ContractError("audio router implementation evidence differs")

    asset_truth = manifest.get("assetTruth", {})
    if asset_truth != {
        "rawProductionAudioFileCount": 0,
        "authoredFallbackBindingCount": 0,
        "candidateAudibleAssetCoverageProven": False,
        "licensedAudioInventoryPresent": False,
        "intentionalSilenceDesignApproved": False,
        "manualMixApproved": False,
        "productOwnerApproved": False,
    }:
        raise ContractError("audio asset truth boundary differs")
    if policy["audioCoverage"].get("rawProductionAudioFileCount") != 0:
        raise ContractError("production-audio inventory boundary differs")
    if manifest.get("claimBoundary") != {
        "sourceControlledSemanticVocabularyValidated": True,
        "runtimeLifecycleHooksValidated": True,
        "providerRoutingSurfaceValidated": True,
        "audibleProductionAssetsClaimed": False,
        "audioLicensingClaimed": False,
        "manualMixApprovalClaimed": False,
        "intentionalSilenceApprovalClaimed": False,
        "blockerClosed": False,
        "releaseReady": False,
    }:
        raise ContractError("audio coverage claim boundary differs")
    return {
        "categoryCount": len(dispositions),
        "runtimeHookCoverageComplete": True,
        "productionAssetCoverageComplete": False,
        "intentionalSilenceCategoryCount": 0,
        "unboundProductionAssetCategoryCount": len(dispositions),
        "rawProductionAudioFileCount": 0,
        "categories": dispositions,
        "blockerClosed": False,
        "releaseReady": False,
    }


def _validate_original_audio_candidate(
    policy: dict[str, Any], manifest: Any, import_receipt: Any,
    overrides: dict[Path, bytes],
) -> dict[str, Any]:
    if not isinstance(manifest, dict):
        raise ContractError("original audio candidate manifest is not an object")
    if (
        manifest.get("schema") != "DiscGolfTour.Session19OriginalAudioCandidatePack.v1"
        or manifest.get("schemaVersion") != 1
        or manifest.get("packId") != "dg_session19_original_audio_candidate_v1"
        or manifest.get("categoryCount") != len(AUDIO_CATEGORIES)
        or manifest.get("audioFormat")
        != {"container": "RIFF_WAVE", "encoding": "PCM_S16LE"}
    ):
        raise ContractError("original audio candidate manifest header differs")
    generator = manifest.get("generator", {})
    generator_path = "Scripts/generate_dg_session19_audio_candidate_pack.py"
    generator_data = _read(ROOT / generator_path, overrides)
    if generator != {
        "path": generator_path,
        "bytes": len(generator_data),
        "sha256": _sha256(generator_data),
        "deterministic": True,
        "externalDependencies": [],
    }:
        raise ContractError("original audio generator identity differs")
    if manifest.get("provenance") != {
        "author": "Disc Golf Tour project",
        "method": "Original mathematical oscillators, deterministic seeded noise, and envelopes",
        "externalDownloads": False,
        "externalRecordingsOrSamples": False,
        "thirdPartyGenerativeService": False,
        "projectOwnedCandidateClaim": True,
    }:
        raise ContractError("original audio provenance boundary differs")

    if not isinstance(import_receipt, dict):
        raise ContractError("original audio import receipt is not an object")
    manifest_path = "Config/DG_Session19OriginalAudioCandidatePack.json"
    manifest_data = _read(ROOT / manifest_path, overrides)
    if (
        import_receipt.get("schema")
        != "DiscGolfTour.Session19OriginalAudioCandidateImportReceipt.v1"
        or import_receipt.get("schemaVersion") != 1
        or import_receipt.get("state")
        != "PASS_ORIGINAL_AUDIO_CANDIDATE_IMPORTED_PENDING_BUILD_COOK_AND_HUMAN_REVIEW"
        or import_receipt.get("manifest") != {
            "path": manifest_path,
            "bytes": len(manifest_data),
            "sha256": _sha256(manifest_data),
        }
        or import_receipt.get("assetCount") != len(AUDIO_CATEGORIES)
        or import_receipt.get("cookRoot") != "/Game/Presentation/Audio/Generated"
    ):
        raise ContractError("original audio import receipt header differs")
    expected_import_claims = {
        "sourceAndSerializedAssetIdentityBound": True,
        "shippingCookProven": False,
        "audibleRuntimePlaybackProven": False,
        "humanMixApproved": False,
        "finalQualityApproved": False,
        "accessibilityApproved": False,
        "legalApproved": False,
        "productOwnerApproved": False,
        "blockerClosed": False,
        "releaseReady": False,
    }
    if import_receipt.get("claimBoundary") != expected_import_claims:
        raise ContractError("original audio import receipt claim boundary differs")

    records = manifest.get("categories")
    if not isinstance(records, list) or len(records) != len(AUDIO_CATEGORIES):
        raise ContractError("original audio source-master inventory differs")
    imported_records = import_receipt.get("assets")
    if not isinstance(imported_records, list) or len(imported_records) != len(AUDIO_CATEGORIES):
        raise ContractError("original audio imported asset inventory differs")
    runtime_source = _read(
        ROOT / "Source/DiscGolfTour/DiscGolfPresentationAudioRouterComponent.cpp", overrides
    ).decode("utf-8", errors="strict")
    output: list[dict[str, Any]] = []
    declared_paths: list[str] = []
    declared_uassets: list[str] = []
    for record, imported, expected in zip(records, imported_records, AUDIO_CATEGORIES):
        if not isinstance(record, dict):
            raise ContractError("original audio source-master record is not an object")
        if not isinstance(imported, dict):
            raise ContractError("original audio imported asset record is not an object")
        category = expected[0]
        source_path = f"OriginalAudio/Session19Generated/DG_{category}.wav"
        package = f"/Game/Presentation/Audio/Generated/SW_{category}"
        object_path = f"{package}.SW_{category}"
        uasset_path = f"Content/Presentation/Audio/Generated/SW_{category}.uasset"
        if (
            record.get("category") != category
            or record.get("sourcePath") != source_path
            or record.get("unrealPackage") != package
            or record.get("unrealObject") != object_path
            or record.get("sampleRateHz") != 48000
            or record.get("channels") != 1
            or record.get("sampleWidthBits") != 16
            or record.get("rights") != "PROJECT_OWNED_ORIGINAL_PROCEDURAL_SYNTHESIS"
            or record.get("externalSamplesUsed") is not False
            or record.get("humanMixApproved") is not False
            or record.get("finalQualityApproved") is not False
        ):
            raise ContractError(f"original audio record differs: {category}")
        data = _read(ROOT / source_path, overrides)
        if len(data) != record.get("bytes") or _sha256(data) != record.get("sha256"):
            raise ContractError(f"original audio WAV identity differs: {category}")
        try:
            with wave.open(io.BytesIO(data), "rb") as wav:
                wave_shape = (
                    wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()
                )
        except wave.Error as exc:
            raise ContractError(f"invalid original audio WAV: {category}") from exc
        if wave_shape != (1, 2, 48000, record.get("frameCount")):
            raise ContractError(f"original audio WAV format differs: {category}")
        if object_path not in runtime_source:
            raise ContractError(f"runtime soft binding missing: {category}")
        uasset_data = _read(ROOT / uasset_path, overrides)
        if (
            imported.get("category") != category
            or imported.get("object") != object_path
            or imported.get("sourceSha256") != record.get("sha256")
            or imported.get("uassetPath") != uasset_path
            or imported.get("uassetBytes") != len(uasset_data)
            or imported.get("uassetSha256") != _sha256(uasset_data)
            or imported.get("metadataBound") is not True
        ):
            raise ContractError(f"serialized SoundWave identity differs: {category}")
        declared_paths.append(source_path)
        declared_uassets.append(uasset_path)
        output.append({
            "category": category,
            "sourcePath": source_path,
            "bytes": len(data),
            "sha256": _sha256(data),
            "unrealObject": object_path,
            "runtimeSoftBindingValidated": True,
            "serializedUnrealAssetPresent": True,
            "uassetPath": uasset_path,
            "uassetBytes": len(uasset_data),
            "uassetSha256": _sha256(uasset_data),
        })

    raw_extensions = {".wav", ".ogg", ".mp3", ".flac", ".aif", ".aiff", ".wem", ".bnk"}
    observed_raw = sorted(
        path.relative_to(ROOT).as_posix()
        for folder in (ROOT / "Content", ROOT / "SourceArt", ROOT / "OriginalAudio")
        if folder.is_dir()
        for path in folder.rglob("*")
        if path.is_file() and path.suffix.lower() in raw_extensions
    )
    if observed_raw != sorted(declared_paths):
        raise ContractError("raw audio inventory is not exactly manifest-bound")
    observed_uassets = sorted(
        path.relative_to(ROOT).as_posix()
        for path in (ROOT / "Content/Presentation/Audio/Generated").glob("SW_*.uasset")
    )
    if observed_uassets != sorted(declared_uassets):
        raise ContractError("serialized SoundWave inventory is not exactly import-receipt-bound")
    default_game = _read(ROOT / "Config/DefaultGame.ini", overrides).decode("utf-8", errors="strict")
    if '+DirectoriesToAlwaysCook=(Path="/Game/Presentation/Audio/Generated")' not in default_game:
        raise ContractError("original audio cook root is not configured")
    claims = manifest.get("claimBoundary", {})
    expected_claims = {
        "sourceMastersGeneratedAndHashBound": True,
        "runtimeHooksCovered": True,
        "unrealAssetsImported": False,
        "shippingCookProven": False,
        "humanMixApproved": False,
        "finalQualityApproved": False,
        "accessibilityApproved": False,
        "legalApproved": False,
        "productOwnerApproved": False,
        "blockerClosed": False,
        "releaseReady": False,
    }
    if claims != expected_claims:
        raise ContractError("original audio candidate claim boundary differs")
    return {
        "state": "PASS_PROJECT_OWNED_SOURCE_MASTERS_RUNTIME_BINDINGS_AND_SERIALIZED_SOUNDWAVES_PRECOOK",
        "categoryCount": len(output),
        "sourceMasterCount": len(output),
        "runtimeSoftBindingCount": len(output),
        "serializedUnrealAssetCount": len(output),
        "cookRootConfigured": True,
        "shippingCookProven": False,
        "audibleRuntimePlaybackProven": False,
        "selectedShippingCandidateContainsAudioCandidate": False,
        "shippingCandidateInvalidatedBySourceDelta": True,
        "freshShippingRebuildRequired": True,
        "records": output,
        "blockerClosed": False,
        "releaseReady": False,
    }


def _validate_shipping_audio_cook(
    policy: dict[str, Any], shipping_policy: Any, receipt: Any,
    original_audio: dict[str, Any], overrides: dict[Path, bytes],
) -> dict[str, Any]:
    """Accept cook presence only through the independently-authored Shipping receipt."""
    if not isinstance(shipping_policy, dict) or not isinstance(receipt, dict):
        raise ContractError("Shipping audio policy or receipt is not an object")
    expected_boundary = {
        "allTwelveSerializedSoundWavesSourceBound": True,
        "allTwelveSoundWavePackagesInIoStore": True,
        "allTwelveSoundWaveObjectsInPackagedAssetRegistry": True,
        "allTwelveCookedObjectsExtractedAndInspected": True,
        "shippingCookProven": True,
        "audibleRuntimePlaybackProven": False,
        "humanMixApproved": False,
        "finalQualityApproved": False,
        "accessibilityApproved": False,
        "legalApproved": False,
        "productOwnerApproved": False,
        "blockerClosed": False,
        "releaseReady": False,
    }
    if (
        shipping_policy.get("schema") != "DiscGolfTour.Session19ShippingAudioCookPolicy.v1"
        or shipping_policy.get("schemaVersion") != 1
        or shipping_policy.get("session") != 19
        or shipping_policy.get("receiptPattern")
        != "Evidence/Session19/ShippingAudioCook-{candidateId}.json"
        or shipping_policy.get("orderedCategories") != [item[0] for item in AUDIO_CATEGORIES]
        or shipping_policy.get("cookRoot") != "/Game/Presentation/Audio/Generated"
        or shipping_policy.get("resultBoundary") != expected_boundary
    ):
        raise ContractError("Shipping audio policy boundary differs")

    candidate = policy["candidate"]
    shipping_policy_path = "Config/DG_Session19ShippingAudioPolicy.json"
    shipping_policy_data = _read(ROOT / shipping_policy_path, overrides)
    if (
        set(receipt) != {
            "schema", "schemaVersion", "session", "candidateId", "generatedUtc", "state",
            "policy", "source", "archive", "tools", "containers", "ioStore",
            "packagedAssetRegistry", "categoryCount", "categories", "claimBoundary",
        }
        or receipt.get("schema") != "DiscGolfTour.Session19ShippingAudioCookReceipt.v1"
        or receipt.get("schemaVersion") != 1
        or receipt.get("session") != 19
        or receipt.get("candidateId") != candidate["candidateId"]
        or receipt.get("state")
        != "PASS_CANDIDATE_BOUND_ALL_TWELVE_GENERATED_SOUNDWAVES_COOKED_AUDIBLE_PLAYBACK_AND_HUMAN_APPROVALS_PENDING"
        or receipt.get("policy") != {
            "path": shipping_policy_path,
            "bytes": len(shipping_policy_data),
            "sha256": _sha256(shipping_policy_data),
        }
        or receipt.get("categoryCount") != len(AUDIO_CATEGORIES)
        or receipt.get("claimBoundary") != expected_boundary
    ):
        raise ContractError("Shipping audio receipt header or claim boundary differs")

    archive = receipt.get("archive", {})
    if (
        archive.get("hostPathRecorded") is not False
        or archive.get("recoveryLocationToken")
        != f"DGTOUR_PACKAGES/{candidate['candidateId']}/Windows"
        or archive.get("fileCount") != candidate["archiveFileCount"]
        or archive.get("bytes") != candidate["archiveBytes"]
        or archive.get("canonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
    ):
        raise ContractError("Shipping audio receipt archive binding differs")
    source = receipt.get("source", {})
    source_roles = [item.get("role") for item in source.get("bindings", [])]
    if (
        source_roles != ["ORIGINAL_AUDIO_CANDIDATE_PACK", "ORIGINAL_AUDIO_IMPORT_RECEIPT"]
        or source.get("cookRootConfigured") is not True
    ):
        raise ContractError("Shipping audio receipt source binding differs")
    project_by_role = {item["role"]: item for item in policy["projectBindings"]}
    if source.get("bindings") != [
        project_by_role["ORIGINAL_AUDIO_CANDIDATE_PACK"],
        project_by_role["ORIGINAL_AUDIO_IMPORT_RECEIPT"],
    ]:
        raise ContractError("Shipping audio receipt source identities differ")

    records = receipt.get("categories")
    source_records = source.get("records")
    if (
        not isinstance(records, list) or len(records) != len(AUDIO_CATEGORIES)
        or not isinstance(source_records, list) or len(source_records) != len(AUDIO_CATEGORIES)
    ):
        raise ContractError("Shipping audio receipt category inventory differs")
    original_by_category = {item["category"]: item for item in original_audio["records"]}
    output: list[dict[str, Any]] = []
    for expected, source_record, record in zip(AUDIO_CATEGORIES, source_records, records):
        category = expected[0]
        original = original_by_category[category]
        object_path = f"/Game/Presentation/Audio/Generated/SW_{category}.SW_{category}"
        source_path = f"Content/Presentation/Audio/Generated/SW_{category}.uasset"
        expected_source = {
            "category": category,
            "object": object_path,
            "sourceSha256": original["sha256"],
            "serializedSource": {
                "path": source_path,
                "bytes": original["uassetBytes"],
                "sha256": original["uassetSha256"],
            },
        }
        if source_record != expected_source:
            raise ContractError(f"Shipping audio source record differs: {category}")
        io_store = record.get("ioStore", {})
        cooked = record.get("cookedObject", {})
        parts = cooked.get("parts")
        if (
            record.get("category") != category
            or record.get("object") != object_path
            or record.get("sourceSha256") != original["sha256"]
            or record.get("serializedSource") != expected_source["serializedSource"]
            or io_store.get("filename")
            != f"../../../DiscGolfTour/Content/Presentation/Audio/Generated/SW_{category}.uasset"
            or io_store.get("chunkType") != "ExportBundleData"
            or not isinstance(io_store.get("size"), int) or io_store["size"] <= 0
            or not isinstance(io_store.get("compressedSize"), int) or io_store["compressedSize"] <= 0
            or record.get("packagedAssetRegistrySoundWave") is not True
            or cooked.get("exportBundleBytes") != io_store["size"]
            or not isinstance(cooked.get("bulkPayloadBytes"), int) or cooked["bulkPayloadBytes"] <= 0
            or cooked.get("partCount") != 3
            or not isinstance(parts, list) or len(parts) != 3
            or any(
                not isinstance(part.get("bytes"), int) or part["bytes"] <= 0
                or not SHA256_RE.fullmatch(str(part.get("sha256", "")))
                for part in parts
            )
            or cooked.get("objectNameMarkerPresent") is not True
            or cooked.get("bulkPayloadPresent") is not True
            or record.get("shippingCookProven") is not True
            or record.get("audibleRuntimePlaybackProven") is not False
        ):
            raise ContractError(f"Shipping audio cooked proof differs: {category}")
        output.append({
            "category": category,
            "object": object_path,
            "ioStoreExportBundleBytes": io_store["size"],
            "packagedAssetRegistrySoundWave": True,
            "extractedCookedObjectInspected": True,
            "shippingCookProven": True,
            "audibleRuntimePlaybackProven": False,
        })
    io_store_summary = receipt.get("ioStore", {})
    registry = receipt.get("packagedAssetRegistry", {})
    if (
        io_store_summary.get("generatedSoundWaveExportBundleCount") != 12
        or registry.get("soundWaveClassObjectCount") != 12
        or registry.get("generatedSoundWaveObjectCount") != 12
    ):
        raise ContractError("Shipping audio aggregate proof differs")
    return {
        "state": receipt["state"],
        "receipt": _binding(
            _project_path(project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"])[0],
            overrides, project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"],
        ),
        "categoryCount": len(output),
        "categories": output,
        **copy.deepcopy(expected_boundary),
    }


def _validate_project_static(policy: dict[str, Any], overrides: dict[Path, bytes]) -> dict[str, Any]:
    loaded: dict[str, Any] = {}
    snapshots: dict[str, tuple[int, int, int, int, int, int] | None] = {}
    bindings: list[dict[str, Any]] = []
    for expected in policy["projectBindings"]:
        path, relative = _project_path(expected["path"])
        data, snapshot = _read_snapshot(path, overrides)
        _exact_identity(data, expected, expected["role"])
        bindings.append({
            "role": expected["role"],
            "path": relative,
            "bytes": len(data),
            "sha256": _sha256(data),
        })
        snapshots[expected["role"]] = snapshot
        if relative.endswith(".json"):
            loaded[expected["role"]] = _strict_json(data, expected["role"])
        elif relative.endswith(".log"):
            loaded[expected["role"]] = data.decode("utf-8", errors="replace")
    contract = loaded["SESSION12_CONTRACT"]
    _validate_contract(contract)
    audio_coverage = _validate_audio_coverage(
        policy, loaded["AUDIO_EVENT_COVERAGE_MANIFEST"], overrides
    )
    original_audio_candidate = _validate_original_audio_candidate(
        policy, loaded["ORIGINAL_AUDIO_CANDIDATE_PACK"],
        loaded["ORIGINAL_AUDIO_IMPORT_RECEIPT"], overrides,
    )
    shipping_audio_cook = _validate_shipping_audio_cook(
        policy, loaded["SHIPPING_AUDIO_COOK_POLICY"],
        loaded["SHIPPING_AUDIO_COOK_RECEIPT"], original_audio_candidate, overrides,
    )
    original_audio_candidate.update({
        "state": "PASS_PROJECT_OWNED_AUDIO_SOURCE_IMPORT_AND_SHIPPING_COOK_BOUND",
        "shippingCookProven": True,
        "selectedShippingCandidateContainsAudioCandidate": True,
        "shippingCandidateInvalidatedBySourceDelta": False,
        "freshShippingRebuildRequired": False,
    })
    focused = _validate_index(
        loaded["FOCUSED_AUTOMATION_INDEX"], policy["requiredFocusedTests"], "focused automation"
    )
    camera = _validate_index(
        loaded["CAMERA_AUTOMATION_INDEX"], policy["requiredCameraTests"], "camera automation"
    )
    _validate_log(loaded["FOCUS_ACTION_WHITELIST_LOG"],
                  ["DiscGolfTour.Session17.RoundFlow.ActionWhitelist.Exact"],
                  "action whitelist focus log", 1)
    intro_focus_tests = (
        EXPLICIT_CANDIDATE_FOCUSED_LOG_TESTS
        if policy["requiredCurrentLogTests"] == EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS
        else ["DiscGolfTour.Session19.Input.HoleIntroConfirmIsolation"]
    )
    _validate_log(
        loaded["FOCUS_INTRO_RELEASE_LOG"], intro_focus_tests,
        "intro release focus log", len(intro_focus_tests),
    )
    if policy["requiredCurrentLogTests"] == EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS:
        if loaded["FOCUS_INTRO_RELEASE_LOG"].count(
            "**** TEST COMPLETE. EXIT CODE: 0 ****"
        ) != 1:
            raise ContractError("explicit candidate intro focus automation did not exit cleanly")
    import_log = loaded["ORIGINAL_AUDIO_IMPORT_LOG"]
    if (
        import_log.count("PASS_ORIGINAL_AUDIO_CANDIDATE_IMPORT (12 SoundWave assets)") != 1
        or import_log.count("Python script executed successfully") != 1
        or import_log.count("Success - 0 error(s), 0 warning(s)") != 1
        or re.search(r"(?i)(failed to find.*montage|missing.*montage|montage.*missing|FObjectFinder)", import_log)
    ):
        raise ContractError("original audio import log is not clean and exact")
    _validate_log(
        loaded["ORIGINAL_AUDIO_AUTOMATION_LOG"],
        ["DiscGolfTour.Session19.AudioCandidate.CategoryBindings"],
        "original audio candidate automation log", 1,
    )
    if "**** TEST COMPLETE. EXIT CODE: 0 ****" not in loaded["ORIGINAL_AUDIO_AUTOMATION_LOG"]:
        raise ContractError("original audio candidate automation did not exit cleanly")
    full_log = loaded["CURRENT_FULL_AUTOMATION_LOG"]
    full_automation_count = _validate_full_automation_log(
        full_log, policy["requiredCurrentLogTests"]
    )
    if policy["requiredCurrentLogTests"] == EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS:
        project_by_role = {
            item["role"]: item for item in policy["projectBindings"]
        }
        _validate_explicit_candidate_log_provenance(
            loaded["FOCUS_INTRO_RELEASE_LOG"],
            project_by_role["FOCUS_INTRO_RELEASE_LOG"]["path"],
            policy["candidate"]["candidateId"],
            "explicit candidate intro focus automation",
            snapshots["FOCUS_INTRO_RELEASE_LOG"],
            virtual=snapshots["FOCUS_INTRO_RELEASE_LOG"] is None,
        )
        _validate_explicit_candidate_log_provenance(
            full_log,
            project_by_role["CURRENT_FULL_AUTOMATION_LOG"]["path"],
            policy["candidate"]["candidateId"],
            "explicit candidate full automation",
            snapshots["CURRENT_FULL_AUTOMATION_LOG"],
            virtual=snapshots["CURRENT_FULL_AUTOMATION_LOG"] is None,
        )

    candidate = policy["candidate"]
    verify = loaded["SHIPPING_CANDIDATE_VERIFICATION"]
    if (
        verify.get("runId") != candidate["candidateId"]
        or verify.get("state") != "PASS_BUILD_ARCHIVE_VERIFIED_PROVENANCE_AND_ACCEPTANCE_PENDING"
        or verify.get("releaseReady") is not False
        or verify.get("innerShippingExecutable", {}).get("sha256") != candidate["shippingExeSha256"]
        or verify.get("shippingPluginCapabilities", {}).get("finalArchiveCanonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
    ):
        raise ContractError("candidate verification receipt differs")
    content = loaded["CANDIDATE_CONTENT"]
    if (
        content.get("runId") != candidate["candidateId"]
        or content.get("state") != "PASS_BOUNDED_STAGED_CONTENT_AUDIT"
        or content.get("archive", {}).get("canonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
    ):
        raise ContractError("candidate content receipt differs")
    binary = loaded["SHIPPING_BINARY"]
    if (
        binary.get("state") != "PASS_BINARY_MARKER_POLICY_ONLY"
        or binary.get("passed") is not True
        or binary.get("releaseReady") is not False
        or str(binary.get("input", {}).get("sha256", "")).upper() != candidate["shippingExeSha256"]
    ):
        raise ContractError("Shipping binary receipt differs")

    source_bindings: list[dict[str, Any]] = []
    for expected in policy["sourceBindings"]:
        path, relative = _project_path(expected["path"])
        data = _read(path, overrides)
        _exact_identity(data, expected, expected["path"])
        text = data.decode("utf-8", errors="strict")
        missing = [marker for marker in expected["markers"] if marker not in text]
        if missing:
            raise ContractError(f"authoritative source markers missing: {expected['path']}")
        source_bindings.append({
            **_binding(path, overrides, relative),
            "reviewedMarkerCount": len(expected["markers"]),
        })
    return {
        "bindings": bindings,
        "sourceBindings": source_bindings,
        "focusedAutomation": focused,
        "cameraAutomation": camera,
        "currentFullAutomation": {
            "discovered": full_automation_count,
            "succeeded": full_automation_count,
            "failed": 0,
        },
        "audioCoverage": audio_coverage,
        "originalAudioCandidate": original_audio_candidate,
        "shippingAudioCook": shipping_audio_cook,
        "contractReleaseBlocked": True,
        "productionAudioAssetsPresent": False,
    }


def _validate_archive_snapshot(snapshot: dict[str, Any], archive: Path, policy: dict[str, Any]) -> dict[str, Any]:
    candidate = policy["candidate"]
    if archive.name != candidate["archiveLeaf"] or archive.parent.name != candidate["candidateId"]:
        raise ContractError("archive path is not the selected candidate's Windows leaf")
    for key, expected_key in (
        ("fileCount", "archiveFileCount"),
        ("bytes", "archiveBytes"),
        ("canonicalManifestSha256", "archiveCanonicalManifestSha256"),
    ):
        if snapshot.get(key) != candidate[expected_key]:
            raise ContractError(f"live archive {key} differs")
    exe_record = next(
        (item for item in snapshot["records"] if item["path"] == candidate["shippingExeRelativePath"]), None
    )
    if (
        exe_record is None
        or exe_record["bytes"] != candidate["shippingExeBytes"]
        or exe_record["sha256"] != candidate["shippingExeSha256"]
    ):
        raise ContractError("live Shipping executable differs")
    return {key: snapshot[key] for key in ("fileCount", "bytes", "canonicalManifestSha256")}


def _validate_dynamic_receipts(
    policy: dict[str, Any], candidate_id: str, manifest_sha: str, token: str,
    overrides: dict[Path, bytes],
) -> list[dict[str, Any]]:
    output: list[dict[str, Any]] = []
    for expected in policy["dynamicCandidateEvidence"]:
        relative = _candidate_relative(expected["pattern"], candidate_id)
        path, relative = _project_path(relative)
        data = _read(path, overrides)
        value = _strict_json(data, expected["role"])
        if (
            value.get("schema") != expected["requiredSchema"]
            or value.get("candidateId") != candidate_id
            or value.get("state") != expected["requiredState"]
            or value.get("failures") != []
        ):
            raise ContractError(f"dynamic candidate receipt differs: {expected['role']}")
        if expected["role"] == "EXTERNAL_TECHNICAL_VALIDATION":
            if (
                value.get("counts", {}).get("screenshots") != 10
                or str(value.get("manifest", {}).get("sha256", "")).upper() != manifest_sha
                or value.get("claimBoundary", {}).get("technicalArtifactValidationOnly") is not True
            ):
                raise ContractError("external technical validation receipt is not manifest-bound")
        else:
            if (
                value.get("userDirToken") != token
                or value.get("counts", {}).get("forbiddenArtifacts") != 0
                or value.get("counts", {}).get("unknownFiles") != 0
                or value.get("releaseBoundary", {}).get("releaseReadinessClaimed") is not False
            ):
                raise ContractError("fresh UserDir validation receipt differs")
        output.append({"role": expected["role"], **_binding(path, overrides, relative)})
    return output


def _false_collection_boundary(value: Any, technical_key: str) -> None:
    if not isinstance(value, dict) or value.get(technical_key) is not True:
        raise ContractError(f"external claim boundary lacks {technical_key}=true")
    for key in (
        "accessibilityApproval", "humanPlayFeelApproval", "legalApproval",
        "manualGameplayAcceptance", "releaseApproval", "releaseReady",
        "visualProductApproval",
    ):
        if value.get(key) is not False:
            raise ContractError(f"external evidence overclaims {key}")
    if value.get("manualReviewedUtc") is not None or value.get("manualReviewerId") is not None:
        raise ContractError("external evidence invents a manual reviewer")


def _external_file(external_run: Path, relative_value: Any) -> tuple[Path, str]:
    relative = _canonical_relative(relative_value)
    root = external_run.resolve(strict=True)
    path = external_run / relative
    if not path.is_file() or path.is_symlink() or _is_reparse(path):
        raise ContractError(f"external evidence file is missing or irregular: {relative}")
    resolved = path.resolve(strict=True)
    if root not in resolved.parents:
        raise ContractError(f"external evidence path resolves outside the run: {relative}")
    cursor = path.parent
    while cursor != external_run:
        if cursor.is_symlink() or _is_reparse(cursor):
            raise ContractError(f"external evidence contains a reparse directory: {relative}")
        cursor = cursor.parent
    return resolved, relative


def _validate_external(
    external_run: Path, policy: dict[str, Any], overrides: dict[Path, bytes],
    *, archive: Path | None = None, user_dir: Path | None = None,
) -> dict[str, Any]:
    candidate = policy["candidate"]
    candidate_id = candidate["candidateId"]
    prefix = candidate_id + "_"
    if not external_run.name.startswith(prefix):
        raise ContractError("external run is not bound to the selected candidate")
    token = external_run.name[len(prefix):]
    if not UUID_RE.fullmatch(token):
        raise ContractError("external run token is not a canonical lowercase UUID")
    if not external_run.is_dir() or external_run.is_symlink() or _is_reparse(external_run):
        raise ContractError("external run directory is missing or irregular")
    bound: dict[str, dict[str, Any]] = {}
    for expected in policy["externalBindings"]:
        path, relative = _external_file(external_run, expected["path"])
        bound[expected["role"]] = _binding(path, overrides, relative)
    manifest_path, _ = _external_file(external_run, "technical-evidence-manifest.json")
    manifest_data = _read(manifest_path, overrides)
    manifest_sha = _sha256(manifest_data)
    manifest = _strict_json(manifest_data, "technical evidence manifest")
    sidecar_path, _ = _external_file(external_run, "technical-evidence-manifest.sha256")
    sidecar = _read(sidecar_path, overrides).decode("ascii").strip()
    if not sidecar.upper().startswith(manifest_sha):
        raise ContractError("technical manifest sidecar differs")
    launch_path, _ = _external_file(external_run, "launch-record.json")
    launch = _load(launch_path, overrides)
    schema_errors = _external_schema_contract_errors(policy, manifest, launch)
    if schema_errors:
        raise ContractError("; ".join(schema_errors))
    trusted_runtime_required = _trusted_runtime_journal_required(policy)
    if trusted_runtime_required and (archive is None or user_dir is None):
        raise ContractError(
            "selected trusted-runtime-journal policy requires archive and UserDir"
        )
    if not trusted_runtime_required and user_dir is not None:
        raise ContractError("historical v1 policy does not accept a trusted-journal UserDir")
    if manifest.get("candidate", {}).get("candidateId") != candidate_id:
        raise ContractError("manifest candidate differs")
    if launch.get("candidateId") != candidate_id:
        raise ContractError("launch record candidate differs")
    if (
        manifest.get("candidate", {}).get("archivePreserved") is not True
        or manifest.get("candidate", {}).get("archiveBefore", {}).get("canonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
        or manifest.get("candidate", {}).get("archiveAfter", {}).get("canonicalManifestSha256")
        != candidate["archiveCanonicalManifestSha256"]
        or manifest.get("candidate", {}).get("observedExeSha256") != candidate["shippingExeSha256"]
    ):
        raise ContractError("technical manifest candidate binding differs")
    if (
        manifest.get("process", {}).get("exitCode") != 0
        or manifest.get("process", {}).get("timedOut") is not False
    ):
        raise ContractError("fresh-install process did not exit cleanly")
    user = manifest.get("userDir", {})
    if (
        user.get("token") != token
        or user.get("emptyBeforeLaunch") is not True
        or user.get("externalBoundaryValidated") is not True
        or user.get("reparsePointsRejected") is not True
    ):
        raise ContractError("fresh-install UserDir boundary differs")
    _false_collection_boundary(manifest.get("claimBoundary"), "technicalArtifactCollectionOnly")
    _false_collection_boundary(launch.get("claimBoundary"), "technicalArtifactCollectionOnly")
    if (
        launch.get("userDirToken") != token
        or launch.get("exeRelativePath") != candidate["shippingExeRelativePath"]
        or launch.get("expectedArchiveManifestSha256") != candidate["archiveCanonicalManifestSha256"]
        or launch.get("observedArchiveManifestSha256") != candidate["archiveCanonicalManifestSha256"]
        or launch.get("expectedExeSha256") != candidate["shippingExeSha256"]
        or launch.get("observedExeSha256") != candidate["shippingExeSha256"]
        or launch.get("hostPathsRecorded") is not False
    ):
        raise ContractError("launch record candidate identity differs")
    runtime_journal_binding: dict[str, Any] | None = None
    if trusted_runtime_required:
        assert archive is not None
        assert user_dir is not None
        runtime_journal_binding = _trusted_runtime_journal_binding(
            archive=archive,
            user_dir=user_dir,
            manifest_path=manifest_path,
            launch_path=launch_path,
            manifest=manifest,
            launch=launch,
            policy=policy,
            manifest_sha=manifest_sha,
        )
    manifest_shots = manifest.get("evidence", {}).get("screenshots")
    if not isinstance(manifest_shots, list) or len(manifest_shots) != 10:
        raise ContractError("technical manifest must contain exactly ten screenshots")
    expected_paths = [item["path"] for item in policy["requiredMilestones"]]
    if [item.get("relativePath") for item in manifest_shots] != expected_paths:
        raise ContractError("technical manifest milestone order differs")
    screenshot_directory = external_run / "Screenshots"
    if (
        not screenshot_directory.is_dir()
        or screenshot_directory.is_symlink()
        or _is_reparse(screenshot_directory)
    ):
        raise ContractError("external screenshot directory is missing or irregular")
    live_paths = sorted(
        path.relative_to(external_run).as_posix()
        for path in screenshot_directory.iterdir() if path.is_file()
    )
    if live_paths != sorted(expected_paths):
        raise ContractError("external screenshot directory differs from exact milestone set")
    milestones: list[dict[str, Any]] = []
    seen_hashes: set[str] = set()
    for expected, recorded in zip(policy["requiredMilestones"], manifest_shots):
        path, _ = _external_file(external_run, expected["path"])
        data = _read(path, overrides)
        width, height = _png_dimensions(data, expected["path"])
        digest = _sha256(data)
        if (
            recorded.get("bytes") != len(data)
            or str(recorded.get("sha256", "")).upper() != digest
            or recorded.get("width") != width
            or recorded.get("height") != height
            or recorded.get("modifiedWithinRunWindow") is not True
        ):
            raise ContractError(f"manifest/live milestone identity differs: {expected['path']}")
        if digest in seen_hashes:
            raise ContractError("fresh-install milestone images are not distinct")
        seen_hashes.add(digest)
        milestones.append({
            "path": expected["path"], "semantic": expected["semantic"],
            "bytes": len(data), "sha256": digest, "width": width, "height": height,
        })
    dynamic = _validate_dynamic_receipts(policy, candidate_id, manifest_sha, token, overrides)
    if runtime_journal_binding is not None:
        fresh_contract = next(
            item for item in policy["dynamicCandidateEvidence"]
            if item["role"] == "FRESH_USERDIR_VALIDATION"
        )
        fresh_relative = _candidate_relative(
            fresh_contract["pattern"], candidate_id
        )
        fresh_path, _ = _project_path(fresh_relative)
        fresh = _load(fresh_path, overrides)
        expected_fresh_binding = {
            "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
            "userDirRelativePath": runtime_journal_binding[
                "userDirRelativePath"
            ],
            "bytes": runtime_journal_binding["bytes"],
            "sha256": runtime_journal_binding["sha256"],
            "captureNonce": runtime_journal_binding["captureNonce"],
            "roundId": runtime_journal_binding["roundId"],
            "launchRecordSha256": runtime_journal_binding[
                "launchRecordSha256"
            ],
            "technicalEvidenceManifestSha256": runtime_journal_binding[
                "technicalEvidenceManifestSha256"
            ],
            "executableSha256": runtime_journal_binding["executableSha256"],
            "archiveManifestSha256": runtime_journal_binding[
                "archiveManifestSha256"
            ],
            "validationState": runtime_journal_binding["validationState"],
        }
        if fresh.get("runtimeJournalBinding") != expected_fresh_binding:
            raise ContractError(
                "fresh UserDir trusted runtime journal binding differs"
            )
    result = {
        "externalRunToken": external_run.name,
        "userDirToken": token,
        "bindings": list(bound.values()),
        "dynamicCandidateReceipts": dynamic,
        "manifestSha256": manifest_sha,
        "process": {"exitCode": 0, "timedOut": False, "cleanExit": True},
        "userDir": {"emptyBeforeLaunch": True, "externalBoundaryValidated": True},
        "milestones": milestones,
    }
    if runtime_journal_binding is not None:
        result["runtimeCheckpointJournal"] = runtime_journal_binding
    return result


def _candidate_receipt(
    candidate_id: str, archive: Path, external_run: Path,
    validator_result: dict[str, Any], archive_snapshot: dict[str, Any],
    overrides: dict[Path, bytes] | None = None,
    policy_path: Path = POLICY_PATH,
    user_dir: Path | None = None,
) -> dict[str, Any]:
    overrides = overrides or {}
    selected_policy_path, selected_policy_relative = _selected_policy_location(policy_path)
    policy = _load(selected_policy_path, overrides)
    _validate_policy(policy)
    if candidate_id != policy["candidate"]["candidateId"]:
        raise ContractError("candidate-id differs from the selected policy candidate")
    static = _validate_project_static(policy, overrides)
    archive_result = _validate_archive_snapshot(archive_snapshot, archive, policy)
    external = _validate_external(
        external_run, policy, overrides, archive=archive, user_dir=user_dir
    )
    trusted = _trusted_runtime_journal_required(policy)
    return {
        "schema": TRUSTED_RECEIPT_SCHEMA if trusted else RECEIPT_SCHEMA,
        "schemaVersion": 3 if trusted else 2,
        "session": 19,
        "candidateId": candidate_id,
        "generatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": STATE,
        "releaseState": RELEASE_STATE,
        "policy": _binding(
            selected_policy_path, overrides, selected_policy_relative,
        ),
        "candidateArchive": archive_result,
        "session12ContractValidator": validator_result,
        "projectEvidence": static,
        "freshInstallEvidence": external,
        "technicalClaims": copy.deepcopy(policy["technicalClaims"]),
        "unsupportedClaims": copy.deepcopy(policy["unsupportedClaims"]),
        "truthBoundary": {
            "technicalArtifactAcceptanceOnly": True,
            "focusedAutomationDeviceScope": "ONE_WINDOWS_EDITOR_DEVICE_ONLY",
            "screenshotsProveMilestonePresenceNotVisualQuality": True,
            "audioEvidenceProvesSemanticEventRouterAndSerializedCandidateCoverageNotAudiblePlaybackOrMix": True,
            "audioRuntimeHookCategoryCount": 12,
            "audioProductionAssetCoverageProven": False,
            "intentionalSilenceDesignApproved": False,
            "originalProjectOwnedAudioSourceMasterCount": 12,
            "originalAudioRuntimeSoftBindingCount": 12,
            "serializedUnrealSoundAssetCount": 12,
            "shippingAudioCookProven": True,
            "audibleRuntimePlaybackProven": False,
            "selectedShippingCandidateContainsAudioCandidate": True,
            "shippingCandidateInvalidatedBySourceDelta": False,
            "freshShippingRebuildRequired": False,
            "cameraReplayEvidenceProvesImplementationWiringNotManualPresentationApproval": True,
            "blockerClosed": False,
            "releaseReady": False,
        },
    }


def _mutated_json(path: Path, overrides: dict[Path, bytes], mutate: Callable[[Any], None]) -> bytes:
    value = copy.deepcopy(_load(path, overrides))
    mutate(value)
    return _json_bytes(value)


def _fixture_png(index: int) -> bytes:
    # Sufficient canonical PNG header for the validator's artifact-header check.
    return PNG_SIGNATURE + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 1922, 1128) + bytes([8, 2, 0, 0, 0, index & 0xFF, 0, 0, 0])


def _build_self_test_fixture(
    root: Path, policy: dict[str, Any], overrides: dict[Path, bytes]
) -> Path:
    candidate = policy["candidate"]
    token = "11111111-2222-3333-8444-555555555555"
    run = root / f"{candidate['candidateId']}_{token}"
    (run / "Screenshots").mkdir(parents=True)
    shots: list[dict[str, Any]] = []
    for index, expected in enumerate(policy["requiredMilestones"], 1):
        data = _fixture_png(index)
        path = run / expected["path"]
        path.write_bytes(data)
        shots.append({
            "relativePath": expected["path"], "bytes": len(data), "sha256": _sha256(data),
            "width": 1922, "height": 1128, "modifiedWithinRunWindow": True,
        })
    false_boundary = {
        "accessibilityApproval": False, "humanPlayFeelApproval": False,
        "legalApproval": False, "manualGameplayAcceptance": False,
        "manualReviewedUtc": None, "manualReviewerId": None,
        "releaseApproval": False, "releaseReady": False,
        "technicalArtifactCollectionOnly": True, "visualProductApproval": False,
    }
    manifest = {
        "schema": "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v1",
        "schemaVersion": 1, "session": 19, "candidateId": candidate["candidateId"],
        "candidate": {
            "candidateId": candidate["candidateId"],
            "archiveBefore": {"canonicalManifestSha256": candidate["archiveCanonicalManifestSha256"]},
            "archiveAfter": {"canonicalManifestSha256": candidate["archiveCanonicalManifestSha256"]},
            "archivePreserved": True, "observedExeSha256": candidate["shippingExeSha256"],
        },
        "claimBoundary": false_boundary,
        "process": {"exitCode": 0, "timedOut": False},
        "userDir": {"token": token, "emptyBeforeLaunch": True, "externalBoundaryValidated": True, "reparsePointsRejected": True},
        "evidence": {"screenshots": shots},
    }
    manifest_data = _json_bytes(manifest)
    (run / "technical-evidence-manifest.json").write_bytes(manifest_data)
    manifest_sha = _sha256(manifest_data)
    (run / "technical-evidence-manifest.sha256").write_text(
        manifest_sha + "  technical-evidence-manifest.json\n", encoding="ascii"
    )
    launch = {
        "schema": "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v1",
        "schemaVersion": 1, "session": 19, "candidateId": candidate["candidateId"],
        "claimBoundary": false_boundary, "userDirToken": token,
        "exeRelativePath": candidate["shippingExeRelativePath"],
        "expectedArchiveManifestSha256": candidate["archiveCanonicalManifestSha256"],
        "observedArchiveManifestSha256": candidate["archiveCanonicalManifestSha256"],
        "expectedExeSha256": candidate["shippingExeSha256"],
        "observedExeSha256": candidate["shippingExeSha256"], "hostPathsRecorded": False,
    }
    (run / "launch-record.json").write_bytes(_json_bytes(launch))
    external_relative = _candidate_relative(
        policy["dynamicCandidateEvidence"][0]["pattern"], candidate["candidateId"]
    )
    fresh_relative = _candidate_relative(
        policy["dynamicCandidateEvidence"][1]["pattern"], candidate["candidateId"]
    )
    external_receipt = {
        "schema": "DiscGolfTour.Session19ExternalTechnicalEvidenceValidation.v1",
        "candidateId": candidate["candidateId"], "state": "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE",
        "failures": [], "counts": {"screenshots": 10, "presentMonRows": 0},
        "manifest": {"sha256": manifest_sha},
        "claimBoundary": {"technicalArtifactValidationOnly": True},
    }
    fresh_receipt = {
        "schema": "DiscGolfTour.Session19FreshUserDirValidation.v1",
        "candidateId": candidate["candidateId"], "state": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        "failures": [], "userDirToken": token,
        "counts": {"forbiddenArtifacts": 0, "unknownFiles": 0},
        "releaseBoundary": {"releaseReadinessClaimed": False},
    }
    overrides[_project_path(external_relative)[0]] = _json_bytes(external_receipt)
    overrides[_project_path(fresh_relative)[0]] = _json_bytes(fresh_receipt)
    return run


def _replace_candidate_strings(value: Any, old: str, new: str) -> Any:
    if isinstance(value, dict):
        return {key: _replace_candidate_strings(item, old, new) for key, item in value.items()}
    if isinstance(value, list):
        return [_replace_candidate_strings(item, old, new) for item in value]
    if isinstance(value, str):
        return value.replace(old, new)
    return value


def _validate_alternate_candidate_policy(
    temporary_root: Path, policy: dict[str, Any], candidate_id: str,
    validator_result: dict[str, Any], archive_snapshot: dict[str, Any],
) -> str | None:
    alternate_id = "S19_WindowsShipping_20990101T000000Z_0123456789ab"
    alternate_policy = copy.deepcopy(policy)
    alternate_policy["candidate"]["candidateId"] = alternate_id
    alternate_overrides: dict[Path, bytes] = {}
    candidate_json_roles = {
        "SHIPPING_AUDIO_COOK_RECEIPT", "SHIPPING_CANDIDATE_VERIFICATION",
        "CANDIDATE_CONTENT", "SHIPPING_BINARY",
    }
    candidate_log_roles = (
        {"FOCUS_INTRO_RELEASE_LOG", "CURRENT_FULL_AUTOMATION_LOG"}
        if policy["requiredCurrentLogTests"] == EXPLICIT_CANDIDATE_CURRENT_LOG_TESTS
        else set()
    )
    candidate_roles = candidate_json_roles | candidate_log_roles
    for binding in alternate_policy["projectBindings"]:
        role = binding["role"]
        if role not in candidate_roles:
            continue
        original_path = ROOT / _canonical_relative(binding["path"])
        relative = binding["path"].replace(candidate_id, alternate_id)
        if relative == binding["path"]:
            source = Path(relative)
            relative = (source.parent / f"{source.stem}-{alternate_id}{source.suffix}").as_posix()
        relative = _canonical_relative(relative)
        if role in candidate_json_roles:
            value = _replace_candidate_strings(
                _load(original_path, {}), candidate_id, alternate_id
            )
            data = _json_bytes(value)
        else:
            alternate_path = (ROOT / relative).resolve()
            text = _read(original_path, {}).decode("utf-8-sig", errors="replace")
            text = re.sub(
                re.escape(str(original_path.resolve())),
                lambda _match: str(alternate_path),
                text,
                flags=re.IGNORECASE,
            )
            text = text.replace(candidate_id, alternate_id)
            text = re.sub(
                r"\[\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}(?=:\d{3}\])",
                "[2099.01.01-00.00.01",
                text,
            )
            data = text.encode("utf-8")
        binding.update({"path": relative, "bytes": len(data), "sha256": _sha256(data)})
        alternate_overrides[(ROOT / relative).resolve()] = data

    # Make the source review result observably policy-derived without changing
    # or re-signing any source-controlled file.
    alternate_source = alternate_policy["sourceBindings"][0]
    alternate_source["markers"].append(alternate_source["markers"][0])
    alternate_policy_path = (
        ROOT / "Config/DG_Session19Session12PresentationTechnicalPolicy.selftest-alternate.json"
    ).resolve()
    alternate_policy_data = _json_bytes(alternate_policy)
    alternate_overrides[alternate_policy_path] = alternate_policy_data

    alternate_run = _build_self_test_fixture(
        temporary_root / "alternate-run", alternate_policy, alternate_overrides,
    )
    alternate_archive = temporary_root / "alternate-archive" / alternate_id / "Windows"
    alternate_archive.mkdir(parents=True)
    try:
        receipt = _candidate_receipt(
            alternate_id, alternate_archive, alternate_run, validator_result,
            archive_snapshot, alternate_overrides, alternate_policy_path,
        )
    except (ContractError, FileNotFoundError, OSError, UnicodeError) as exc:
        return f"alternate candidate-scoped policy failed: {exc}"

    policy_relative = alternate_policy_path.relative_to(ROOT.resolve()).as_posix()
    expected_policy_binding = {
        "path": policy_relative,
        "bytes": len(alternate_policy_data),
        "sha256": _sha256(alternate_policy_data),
    }
    if receipt["policy"] != expected_policy_binding:
        return "alternate selected policy path/hash was not bound into the receipt"
    observed_projects = {
        item["role"]: item for item in receipt["projectEvidence"]["bindings"]
    }
    for expected in alternate_policy["projectBindings"]:
        if expected["role"] in candidate_roles and observed_projects[expected["role"]] != expected:
            return f"alternate candidate receipt binding was not selected: {expected['role']}"
    expected_marker_count = len(alternate_source["markers"])
    if receipt["projectEvidence"]["sourceBindings"][0]["reviewedMarkerCount"] != expected_marker_count:
        return "alternate policy source binding was not selected"
    return None


def _runtime_contract_self_tests() -> tuple[int, list[str]]:
    failures: list[str] = []
    count = 0
    legacy_policy = {"schema": LEGACY_POLICY_SCHEMA, "schemaVersion": 2}
    trusted_policy = {
        "schema": TRUSTED_POLICY_SCHEMA,
        "schemaVersion": 3,
        "trustedRuntimeJournalContract": copy.deepcopy(
            TRUSTED_RUNTIME_JOURNAL_CONTRACT
        ),
    }
    legacy_manifest = {"schema": LEGACY_MANIFEST_SCHEMA, "schemaVersion": 1}
    legacy_launch = {"schema": LEGACY_LAUNCH_SCHEMA, "schemaVersion": 1}
    trusted_manifest = {"schema": TRUSTED_MANIFEST_SCHEMA, "schemaVersion": 2}
    trusted_launch = {"schema": TRUSTED_LAUNCH_SCHEMA, "schemaVersion": 2}
    for name, selected, manifest, launch, should_pass in [
        ("v1", legacy_policy, legacy_manifest, legacy_launch, True),
        ("v2", trusted_policy, trusted_manifest, trusted_launch, True),
        ("v1-policy-v2-evidence", legacy_policy, trusted_manifest, trusted_launch, False),
        ("v2-policy-v1-evidence", trusted_policy, legacy_manifest, legacy_launch, False),
        ("v1-manifest-v2-launch", legacy_policy, legacy_manifest, trusted_launch, False),
        ("v2-manifest-v1-launch", trusted_policy, trusted_manifest, legacy_launch, False),
        ("v1-policy-mixed-evidence", legacy_policy, trusted_manifest, legacy_launch, False),
        ("v2-policy-mixed-evidence", trusted_policy, legacy_manifest, trusted_launch, False),
    ]:
        count += 1
        actual_pass = not _external_schema_contract_errors(selected, manifest, launch)
        if actual_pass is not should_pass:
            failures.append(f"runtime schema contract case differed: {name}")
    count += 1
    try:
        from validate_dg_session19_external_technical_evidence import _self_test

        authoritative = _self_test()
        if (
            authoritative.get("state") != "PASS"
            or type(authoritative.get("testsPassed")) is not int
            or authoritative.get("testsPassed", 0) < 25
        ):
            failures.append(
                "authoritative v1/v2/raw-journal forgery self-test did not pass"
            )
    except Exception as exc:
        failures.append(
            "authoritative raw-journal self-test failed closed: "
            f"{exc.__class__.__name__}"
        )

    selected_script = Path(__file__).resolve(strict=True)
    lexical_script = Path(__file__).absolute()
    selected_fingerprint = _stat_fingerprint(selected_script.stat())
    foreign_script = lexical_script.with_name(
        "generate_dg_session19_candidate_technical_policies.py"
    )
    stale_fingerprint = list(selected_fingerprint)
    stale_fingerprint[2] -= 1
    abslog_cases = [
        (
            "junction alias duplicates",
            f'-abslog={lexical_script}""\n-abslog="{lexical_script}"\n',
            selected_fingerprint,
            True,
        ),
        (
            "resolved selected path",
            f"-abslog={selected_script}\n",
            selected_fingerprint,
            True,
        ),
        (
            "foreign reparse-root path",
            f"-abslog={foreign_script}\n",
            selected_fingerprint,
            False,
        ),
        (
            "mixed selected and foreign paths",
            f"-abslog={lexical_script}\n-abslog={foreign_script}\n",
            selected_fingerprint,
            False,
        ),
        (
            "relative path",
            "-abslog=Saved/Logs/selected.log\n",
            selected_fingerprint,
            False,
        ),
        (
            "parent traversal",
            f"-abslog={lexical_script.parent / '..' / lexical_script.parent.name / lexical_script.name}\n",
            selected_fingerprint,
            False,
        ),
        (
            "unterminated quote",
            f'-abslog="{lexical_script}\n',
            selected_fingerprint,
            False,
        ),
        (
            "missing assignment",
            "LogInit: no absolute log switch\n",
            selected_fingerprint,
            False,
        ),
        (
            "selected-file mutation",
            f"-abslog={lexical_script}\n",
            tuple(stale_fingerprint),
            False,
        ),
    ]
    for name, text, fingerprint, should_pass in abslog_cases:
        count += 1
        try:
            _validate_embedded_abslog_identity(
                text,
                selected_script,
                fingerprint,
                f"abslog self-test {name}",
            )
        except (ContractError, OSError):
            actual_pass = False
        else:
            actual_pass = True
        if actual_pass is not should_pass:
            failures.append(f"abslog provenance case differed: {name}")
    return count, failures


def _self_test(candidate_id: str, archive: Path, policy_path: Path) -> tuple[int, list[str]]:
    selected_policy_path, _ = _selected_policy_location(policy_path)
    selected_policy = _load(selected_policy_path, {})
    _validate_policy(selected_policy)
    trusted_policy = copy.deepcopy(selected_policy)
    trusted_policy["schema"] = TRUSTED_POLICY_SCHEMA
    trusted_policy["schemaVersion"] = 3
    trusted_policy["trustedRuntimeJournalContract"] = copy.deepcopy(
        TRUSTED_RUNTIME_JOURNAL_CONTRACT
    )
    policy_contract_passes = 0
    try:
        _validate_policy(trusted_policy)
        policy_contract_passes += 1
    except ContractError as exc:
        policy_contract_failure = f"trusted selected policy failed: {exc}"
    else:
        policy_contract_failure = None
    forged_policy = copy.deepcopy(trusted_policy)
    forged_policy["trustedRuntimeJournalContract"][
        "runtimeJournalValidationState"
    ] = "PASS_UNTRUSTED_INVENTORY_ONLY"
    try:
        _validate_policy(forged_policy)
    except ContractError:
        policy_contract_passes += 1
    else:
        policy_contract_failure = "forged trusted-runtime policy contract survived"

    policy = copy.deepcopy(selected_policy)
    selected_was_trusted = _trusted_runtime_journal_required(policy)
    if selected_was_trusted:
        policy["schema"] = LEGACY_POLICY_SCHEMA
        policy["schemaVersion"] = 2
        policy.pop("trustedRuntimeJournalContract", None)
        _validate_policy(policy)
    project_by_role = {item["role"]: item for item in policy["projectBindings"]}
    validator_result = _run_session12_validator()
    snapshot = _collect_archive(archive)
    caught = 0
    failures: list[str] = []
    if policy_contract_failure is not None:
        failures.append(policy_contract_failure)
    caught += policy_contract_passes
    runtime_contract_count, runtime_contract_failures = _runtime_contract_self_tests()
    failures.extend(runtime_contract_failures)
    if not runtime_contract_failures:
        caught += runtime_contract_count
    with tempfile.TemporaryDirectory(prefix="dg_s12_present_selftest_") as tmp:
        base_overrides: dict[Path, bytes] = {}
        if selected_was_trusted:
            base_overrides[selected_policy_path] = _json_bytes(policy)
        run = _build_self_test_fixture(Path(tmp), policy, base_overrides)
        _candidate_receipt(
            candidate_id, archive, run, validator_result, snapshot,
            base_overrides, selected_policy_path,
        )

        mutations: list[tuple[str, Path, Callable[[Any], None]]] = [
            ("policy schema", selected_policy_path, lambda v: v.__setitem__("schema", "wrong")),
            ("policy candidate", selected_policy_path, lambda v: v["candidate"].__setitem__("candidateId", "wrong")),
            ("policy candidate unsafe path token", selected_policy_path, lambda v: v["candidate"].__setitem__("candidateId", "../wrong")),
            ("policy archive hash", selected_policy_path, lambda v: v["candidate"].__setitem__("archiveCanonicalManifestSha256", "0" * 64)),
            ("policy exe hash", selected_policy_path, lambda v: v["candidate"].__setitem__("shippingExeSha256", "0" * 64)),
            ("policy project role", selected_policy_path, lambda v: v["projectBindings"][0].__setitem__("role", "wrong")),
            ("policy project path escape", selected_policy_path, lambda v: v["projectBindings"][0].__setitem__("path", "../wrong.json")),
            ("policy dynamic path escape", selected_policy_path, lambda v: v["dynamicCandidateEvidence"][0].__setitem__("pattern", "../wrong-{candidateId}.json")),
            ("policy receipt path escape", selected_policy_path, lambda v: v.__setitem__("receiptPattern", "../wrong-{candidateId}.json")),
            ("policy source removal", selected_policy_path, lambda v: v["sourceBindings"].pop()),
            ("policy milestone removal", selected_policy_path, lambda v: v["requiredMilestones"].pop()),
            ("policy overclaim", selected_policy_path, lambda v: v["unsupportedClaims"].__setitem__("releaseReady", True)),
            ("contract release ready", ROOT / "Config/DG_Session12PresentationContract.json", lambda v: v.__setitem__("release_ready", True)),
            ("contract audio assets", ROOT / "Config/DG_Session12PresentationContract.json", lambda v: v["audio_router_contract"].__setitem__("production_audio_assets_present", True)),
            ("audio category removal", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["categories"].pop()),
            ("audio duplicate category", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["categories"][1].__setitem__("category", v["categories"][0]["category"])),
            ("audio runtime hook overclaim", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["categories"][0].__setitem__("technicalHookValidated", False)),
            ("audio asset overclaim", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["categories"][0].__setitem__("audibleAssetValidated", True)),
            ("audio invented silence", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["categories"][0].__setitem__("intentionalSilence", True)),
            ("audio mix approval overclaim", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["assetTruth"].__setitem__("manualMixApproved", True)),
            ("audio blocker overclaim", ROOT / project_by_role["AUDIO_EVENT_COVERAGE_MANIFEST"]["path"], lambda v: v["claimBoundary"].__setitem__("blockerClosed", True)),
            ("original audio category removal", ROOT / project_by_role["ORIGINAL_AUDIO_CANDIDATE_PACK"]["path"], lambda v: v["categories"].pop()),
            ("original audio source hash", ROOT / project_by_role["ORIGINAL_AUDIO_CANDIDATE_PACK"]["path"], lambda v: v["categories"][0].__setitem__("sha256", "0" * 64)),
            ("original audio external sample claim", ROOT / project_by_role["ORIGINAL_AUDIO_CANDIDATE_PACK"]["path"], lambda v: v["categories"][0].__setitem__("externalSamplesUsed", True)),
            ("original audio import overclaim", ROOT / project_by_role["ORIGINAL_AUDIO_CANDIDATE_PACK"]["path"], lambda v: v["claimBoundary"].__setitem__("unrealAssetsImported", True)),
            ("original audio import asset count", ROOT / project_by_role["ORIGINAL_AUDIO_IMPORT_RECEIPT"]["path"], lambda v: v.__setitem__("assetCount", 11)),
            ("original audio import source hash", ROOT / project_by_role["ORIGINAL_AUDIO_IMPORT_RECEIPT"]["path"], lambda v: v["assets"][0].__setitem__("sourceSha256", "0" * 64)),
            ("original audio import blocker overclaim", ROOT / project_by_role["ORIGINAL_AUDIO_IMPORT_RECEIPT"]["path"], lambda v: v["claimBoundary"].__setitem__("blockerClosed", True)),
            ("shipping audio policy cook regression", ROOT / project_by_role["SHIPPING_AUDIO_COOK_POLICY"]["path"], lambda v: v["resultBoundary"].__setitem__("shippingCookProven", False)),
            ("shipping audio category removal", ROOT / project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"], lambda v: v["categories"].pop()),
            ("shipping audio cook regression", ROOT / project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"], lambda v: v["claimBoundary"].__setitem__("shippingCookProven", False)),
            ("shipping audio audible overclaim", ROOT / project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"], lambda v: v["claimBoundary"].__setitem__("audibleRuntimePlaybackProven", True)),
            ("shipping audio human overclaim", ROOT / project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"], lambda v: v["claimBoundary"].__setitem__("humanMixApproved", True)),
            ("shipping audio release overclaim", ROOT / project_by_role["SHIPPING_AUDIO_COOK_RECEIPT"]["path"], lambda v: v["claimBoundary"].__setitem__("releaseReady", True)),
            ("focused count", ROOT / project_by_role["FOCUSED_AUTOMATION_INDEX"]["path"], lambda v: v.__setitem__("succeeded", 15)),
            ("focused warning", ROOT / project_by_role["FOCUSED_AUTOMATION_INDEX"]["path"], lambda v: v["tests"][0].__setitem__("warnings", 1)),
            ("focused test path", ROOT / project_by_role["FOCUSED_AUTOMATION_INDEX"]["path"], lambda v: v["tests"][0].__setitem__("fullTestPath", "wrong")),
            ("camera count", ROOT / project_by_role["CAMERA_AUTOMATION_INDEX"]["path"], lambda v: v.__setitem__("succeeded", 5)),
            ("candidate verification", ROOT / project_by_role["SHIPPING_CANDIDATE_VERIFICATION"]["path"], lambda v: v.__setitem__("runId", "wrong")),
            ("candidate verification release", ROOT / project_by_role["SHIPPING_CANDIDATE_VERIFICATION"]["path"], lambda v: v.__setitem__("releaseReady", True)),
            ("content archive", ROOT / project_by_role["CANDIDATE_CONTENT"]["path"], lambda v: v["archive"].__setitem__("canonicalManifestSha256", "0" * 64)),
            ("binary pass", ROOT / project_by_role["SHIPPING_BINARY"]["path"], lambda v: v.__setitem__("passed", False)),
            ("manifest candidate", run / "technical-evidence-manifest.json", lambda v: v["candidate"].__setitem__("candidateId", "wrong")),
            ("manifest empty before", run / "technical-evidence-manifest.json", lambda v: v["userDir"].__setitem__("emptyBeforeLaunch", False)),
            ("manifest process exit", run / "technical-evidence-manifest.json", lambda v: v["process"].__setitem__("exitCode", 1)),
            ("manifest timeout", run / "technical-evidence-manifest.json", lambda v: v["process"].__setitem__("timedOut", True)),
            ("manifest archive preserved", run / "technical-evidence-manifest.json", lambda v: v["candidate"].__setitem__("archivePreserved", False)),
            ("manifest screenshot path", run / "technical-evidence-manifest.json", lambda v: v["evidence"]["screenshots"][0].__setitem__("relativePath", "wrong.png")),
            ("manifest screenshot hash", run / "technical-evidence-manifest.json", lambda v: v["evidence"]["screenshots"][0].__setitem__("sha256", "0" * 64)),
            ("manifest visual approval", run / "technical-evidence-manifest.json", lambda v: v["claimBoundary"].__setitem__("visualProductApproval", True)),
            ("launch candidate", run / "launch-record.json", lambda v: v.__setitem__("candidateId", "wrong")),
            ("launch exe", run / "launch-record.json", lambda v: v.__setitem__("observedExeSha256", "0" * 64)),
            ("launch release", run / "launch-record.json", lambda v: v["claimBoundary"].__setitem__("releaseReady", True)),
        ]
        for name, path, mutate in mutations:
            override = dict(base_overrides)
            mutated_data = _mutated_json(path, base_overrides, mutate)
            override[path.resolve()] = mutated_data
            resigned_role = next(
                (role for role in (
                    "AUDIO_EVENT_COVERAGE_MANIFEST", "ORIGINAL_AUDIO_CANDIDATE_PACK",
                    "ORIGINAL_AUDIO_IMPORT_RECEIPT", "SHIPPING_AUDIO_COOK_POLICY",
                    "SHIPPING_AUDIO_COOK_RECEIPT",
                )
                 if path == ROOT / project_by_role[role]["path"]),
                None,
            )
            if resigned_role:
                resigned_policy = copy.deepcopy(policy)
                binding = next(
                    item for item in resigned_policy["projectBindings"]
                    if item["role"] == resigned_role
                )
                binding["bytes"] = len(mutated_data)
                binding["sha256"] = _sha256(mutated_data)
                override[selected_policy_path] = _json_bytes(resigned_policy)
            # Re-sign mutated manifests so their semantic checks, rather than
            # merely the detached-hash check, must reject the adversarial case.
            if path == run / "technical-evidence-manifest.json":
                mutated_sha = _sha256(mutated_data)
                override[(run / "technical-evidence-manifest.sha256").resolve()] = (
                    mutated_sha + "  technical-evidence-manifest.json\n"
                ).encode("ascii")
                external_relative = _candidate_relative(
                    policy["dynamicCandidateEvidence"][0]["pattern"], candidate_id
                )
                external_path = _project_path(external_relative)[0]
                external_receipt = copy.deepcopy(_load(external_path, base_overrides))
                external_receipt["manifest"]["sha256"] = mutated_sha
                override[external_path.resolve()] = _json_bytes(external_receipt)
            try:
                _candidate_receipt(
                    candidate_id, archive, run, validator_result, snapshot,
                    override, selected_policy_path,
                )
            except (ContractError, FileNotFoundError, OSError, UnicodeError):
                caught += 1
            else:
                failures.append(name)

        raw_mutations: list[tuple[str, Path, bytes]] = [
            ("manifest sidecar", run / "technical-evidence-manifest.sha256", b"0" * 64),
            ("screenshot PNG", run / policy["requiredMilestones"][0]["path"], b"not-a-png"),
            ("source identity", ROOT / policy["sourceBindings"][0]["path"], b"mutated"),
            ("focus log identity", ROOT / project_by_role["FOCUS_ACTION_WHITELIST_LOG"]["path"], b"mutated"),
            ("serialized SoundWave identity", ROOT / "Content/Presentation/Audio/Generated/SW_ThrowRelease.uasset", b"mutated"),
        ]
        for name, path, data in raw_mutations:
            override = dict(base_overrides)
            override[path.resolve()] = data
            try:
                _candidate_receipt(
                    candidate_id, archive, run, validator_result, snapshot,
                    override, selected_policy_path,
                )
            except (ContractError, FileNotFoundError, OSError, UnicodeError):
                caught += 1
            else:
                failures.append(name)

        bad_snapshot = copy.deepcopy(snapshot)
        bad_snapshot["canonicalManifestSha256"] = "0" * 64
        try:
            _candidate_receipt(
                candidate_id, archive, run, validator_result, bad_snapshot,
                base_overrides, selected_policy_path,
            )
        except ContractError:
            caught += 1
        else:
            failures.append("live archive snapshot")

        alternate_failure = _validate_alternate_candidate_policy(
            Path(tmp), policy, candidate_id, validator_result, snapshot,
        )
        if alternate_failure is not None:
            failures.append(alternate_failure)

        immutable_output = Path(tmp) / "immutable-receipt.json"
        immutable_receipt = {"schema": "selftest", "generatedUtc": "first", "value": 1}
        _write_receipt_once_or_verify(immutable_output, immutable_receipt)
        immutable_before = (immutable_output.read_bytes(), immutable_output.stat().st_mtime_ns)
        _write_receipt_once_or_verify(
            immutable_output,
            {"schema": "selftest", "generatedUtc": "second", "value": 1},
        )
        if (immutable_output.read_bytes(), immutable_output.stat().st_mtime_ns) != immutable_before:
            failures.append("identical existing receipt was modified")
        try:
            _write_receipt_once_or_verify(
                immutable_output,
                {"schema": "selftest", "generatedUtc": "third", "value": 2},
            )
        except ContractError:
            caught += 1
        else:
            failures.append("different existing receipt was overwritten")

    expected = (
        len(mutations) + len(raw_mutations) + 2
        + 2 + runtime_contract_count
    )
    if expected < policy["selfTestMinimumMutationCount"]:
        failures.append("mutation inventory below policy minimum")
    if caught != expected:
        failures.append(f"caught {caught} of {expected}")
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", type=Path, default=POLICY_PATH)
    parser.add_argument("--candidate-id", required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--external-run", type=Path)
    parser.add_argument(
        "--user-dir", type=Path,
        help="Required only by a selected v3 trusted-runtime-journal policy.",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-receipt", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--release-required", action="store_true")
    args = parser.parse_args()

    try:
        policy_input = args.policy if args.policy.is_absolute() else ROOT / args.policy
        policy_path, _ = _selected_policy_location(policy_input)
        policy = _load(policy_path, {})
        _validate_policy(policy)
        if args.candidate_id != policy["candidate"]["candidateId"]:
            raise ContractError("candidate-id differs from selected policy candidate")
        trusted_runtime_required = _trusted_runtime_journal_required(policy)
        if args.self_test:
            caught, failures = _self_test(args.candidate_id, args.archive, policy_path)
            if failures:
                print("FAIL_SESSION12_PRESENTATION_TECHNICAL_SELF_TEST: " + "; ".join(failures), file=sys.stderr)
                return 1
            print(f"PASS_SESSION12_PRESENTATION_TECHNICAL_SELF_TEST ({caught}/{caught} mutations caught)")
            return 0
        if trusted_runtime_required and args.user_dir is None:
            raise ContractError("selected v3 policy requires --user-dir")
        if not trusted_runtime_required and args.user_dir is not None:
            raise ContractError("historical v2 policy does not accept --user-dir")
        if args.external_run is None:
            raise ContractError(
                "selected candidate has no supplied fresh external evidence; --external-run is required and must be candidate-bound"
            )
        validator_result = _run_session12_validator()
        snapshot = _collect_archive(args.archive)
        receipt = _candidate_receipt(
            args.candidate_id, args.archive, args.external_run,
            validator_result, snapshot,
            policy_path=policy_path,
            user_dir=args.user_dir,
        )
        output = args.output or _project_path(
            _candidate_relative(policy["receiptPattern"], args.candidate_id)
        )[0]
        _write_receipt_once_or_verify(output, receipt)
        if args.expected_receipt is not None:
            expected = _strict_json(args.expected_receipt.read_bytes(), str(args.expected_receipt))
            if _receipt_without_timestamp(expected, "expected receipt") != _receipt_without_timestamp(
                receipt, "live receipt"
            ):
                raise ContractError("expected receipt differs from live revalidation")
        print(STATE)
        print(f"Receipt: {output}")
        print(f"Release ready: no ({RELEASE_STATE})")
        return policy["releaseRequiredExitCode"] if args.release_required else 0
    except (ContractError, FileNotFoundError, OSError, subprocess.SubprocessError) as exc:
        print(f"FAIL_SESSION12_PRESENTATION_TECHNICAL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
